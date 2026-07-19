#include "gwy_launcher/ext_entry_null_contract.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define GWY_ENC_CALL_MAX 16
#define GWY_ENC_PATH_INSNS 64
#define GWY_ENC_R0_WALK 24
#define GWY_ENC_SEQ_CAP 192

typedef struct {
    uint32_t callsite_id;
    uint32_t caller_offset;
    uint64_t to_mid;
    char module[96];
    uint32_t target;
    uint32_t target_offset;
    uint32_t r0, r1, r2, r3;
    char r0_source[24];
    char stage[16];
    int return_seen;
    uint32_t return_r0;
    int logged;
} EncEntryCall;

typedef struct {
    void *uc;
    uint32_t ord;
    char seq[GWY_ENC_SEQ_CAP];
    uint32_t load_return_ord;
    uint32_t helper_register_ord;
    uint32_t mr_helper_first_ord;
    uint32_t module_entry_call_ord;
    uint32_t code_image_ord;
    int lifecycle_logged;
    int type_evidence_logged;
    int p_link_logged;
    int family_logged;
    int data_init_logged;
    int path_logged;
    int root_logged;
    int mr_helper_seen;
    int entry_fault;
    int xt_crossed; /* return after first CODE_IMAGE entry without fault on that hop */
    uint32_t p_guest;
    uint32_t p_len;
    uint32_t helper;
    uint64_t entry_mid;
    uint32_t entry_pc;
    uint32_t entry_r0;
    uint32_t entry_r9;
    uint32_t entry_er_rw;
    uint32_t entry_return_lr; /* DSM LR at second-hop; true return lands here */
    uint32_t sampling_pc;
    int sample_count;
    int saw_r0_plus_28;
    int saw_r9_load;
    uint32_t r9_load_imm;
    EncEntryCall calls[GWY_ENC_CALL_MAX];
    int call_count;
    ExtNullContractClass last_class;
} EntryNullContractState;

static EntryNullContractState g_enc;

void ext_entry_null_contract_reset(void) { memset(&g_enc, 0, sizeof(g_enc)); }

int ext_entry_null_contract_enabled(void) {
    const char *env = getenv("GWY_ENTRY_NULL_CONTRACT");
    return env && env[0] == '1';
}

void ext_entry_null_contract_bind_uc(void *uc) { g_enc.uc = uc; }

const char *ext_null_contract_class_name(ExtNullContractClass c) {
    switch (c) {
        case NULL_CONTRACT_ENTRY_NULL_DOCUMENTED: return "ENTRY_NULL_DOCUMENTED";
        case NULL_CONTRACT_ENTRY_NULL_CROSS_TARGET: return "ENTRY_NULL_CROSS_TARGET";
        case NULL_CONTRACT_MISSING_MODULE_ENTRY_CONTEXT: return "MISSING_MODULE_ENTRY_CONTEXT";
        case NULL_CONTRACT_MISSING_PRE_ENTRY_INITIALIZATION:
            return "MISSING_PRE_ENTRY_INITIALIZATION";
        case NULL_CONTRACT_MISSING_MODULE_DATA_INITIALIZATION:
            return "MISSING_MODULE_DATA_INITIALIZATION";
        case NULL_CONTRACT_WRONG_MODULE_ENTRY_LIFECYCLE: return "WRONG_MODULE_ENTRY_LIFECYCLE";
        case NULL_CONTRACT_LOADCODE_RETURN_MISINTERPRETED: return "LOADCODE_RETURN_MISINTERPRETED";
        default: return "UNKNOWN";
    }
}

ExtNullContractClass ext_entry_null_contract_last_class(void) { return g_enc.last_class; }

int ext_entry_null_contract_xt_crossed(void) { return g_enc.xt_crossed; }

static uint32_t next_ord(void) { return ++g_enc.ord; }

static void seq_append(const char *tag) {
    size_t n, m;
    if (!tag || !tag[0]) return;
    n = strlen(g_enc.seq);
    m = strlen(tag);
    if (n + m + 2 >= GWY_ENC_SEQ_CAP) return;
    if (n) {
        g_enc.seq[n++] = '>';
        g_enc.seq[n] = '\0';
    }
    strncat(g_enc.seq, tag, GWY_ENC_SEQ_CAP - strlen(g_enc.seq) - 1);
}

static void maybe_p_link(void) {
    if (g_enc.p_link_logged || !ext_entry_null_contract_enabled()) return;
    g_enc.p_link_logged = 1;
    printf("[P_ENTRY_LINK] linked=NO evidence=none "
           "note=helper_path_only_until_DOCUMENTED_or_CROSS_TARGET\n");
    fflush(stdout);
}

static void maybe_type_evidence(void) {
    if (g_enc.type_evidence_logged || !ext_entry_null_contract_enabled()) return;
    g_enc.type_evidence_logged = 1;
    printf("[ENTRY_TYPE_EVIDENCE] prototype_candidate=int32(int32) "
           "note=MR_LOAD_C_FUNCTION_is_image_plus8_not_code_image_second_hop "
           "source=DOCUMENTED confidence=med "
           "code_image_second_hop_proto=unknown evidence=DOCUMENTED\n");
    fflush(stdout);
}

static const char *recover_r0_source(void *uc, uint32_t caller_pc) {
    int i;
    uint32_t pc;
    if (!uc || !caller_pc) return "unknown";
    pc = caller_pc & ~1u;
    for (i = 1; i <= GWY_ENC_R0_WALK; i++) {
        uint32_t a = pc - (uint32_t)(i * 2);
        uint32_t word = 0;
        uint16_t half;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, a & ~3u, &word)) continue;
        half = (uint16_t)((a & 2u) ? (word >> 16) : (word & 0xFFFFu));
        if ((half & 0xFF00u) == 0x2000u) {
            if ((half & 0xFFu) == 0) return "explicit_zero";
            return "argument";
        }
        if ((half & 0xFFC7u) == 0x4600u) return "argument";
        {
            int rt = -1, rn = -1, is_load = 0;
            uint32_t imm = 0;
            if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load && rt == 0)
                return "record_field";
        }
    }
    return "unknown";
}

static void emit_family(void) {
    int i, all_zero = 1, nonzero = 0;
    char mods[256];
    mods[0] = '\0';
    if (g_enc.call_count <= 0) return;
    /* Allow re-emit once finalized on fault by resetting family_logged in on_mem_fault. */
    if (g_enc.family_logged) return;
    g_enc.family_logged = 1;
    for (i = 0; i < g_enc.call_count; i++) {
        EncEntryCall *c = &g_enc.calls[i];
        if (c->r0 != 0) {
            all_zero = 0;
            nonzero++;
        }
        if (c->module[0] && !strstr(mods, c->module)) {
            size_t n = strlen(mods);
            if (n && n + 1 < sizeof(mods)) {
                mods[n++] = ',';
                mods[n] = '\0';
            }
            strncat(mods, c->module, sizeof(mods) - strlen(mods) - 1);
        }
    }
    printf("[DSM_ENTRY_FAMILY] callsite_count=%d all_r0_zero=%d nonzero_r0_count=%d "
           "modules=%s evidence=OBSERVED\n",
           g_enc.call_count, all_zero, nonzero, mods[0] ? mods : "?");
    fflush(stdout);
}

static void emit_lifecycle(void) {
    if (g_enc.lifecycle_logged) return;
    if (!g_enc.module_entry_call_ord && !g_enc.mr_helper_first_ord) return;
    g_enc.lifecycle_logged = 1;
    printf("[MODULE_ENTRY_LIFECYCLE] load_id=%u seq=%s "
           "load_return_ord=%u helper_register_ord=%u mr_helper_first_ord=%u "
           "module_entry_call_ord=%u note=compare_to_clean_dsm_order evidence=OBSERVED\n",
           g_enc.load_return_ord ? 1u : 0u, g_enc.seq[0] ? g_enc.seq : "EMPTY",
           g_enc.load_return_ord, g_enc.helper_register_ord, g_enc.mr_helper_first_ord,
           g_enc.module_entry_call_ord);
    fflush(stdout);
}

static void emit_data_init(const GwyLoadedModule *m, uint32_t r9) {
    uint32_t rw = 0, rws = 0, er = 0;
    int r9_eq = 0;
    if (g_enc.data_init_logged || !m) return;
    g_enc.data_init_logged = 1;
    rw = m->map.guest_rw_base;
    rws = m->map.guest_rw_size;
    er = rw ? rw : g_enc.entry_er_rw;
    if (!er && g_enc.p_guest && g_enc.uc) {
        uint32_t v = 0;
        if (guest_memory_uc_peek_u32((struct uc_struct *)g_enc.uc, g_enc.p_guest, &v)) er = v;
    }
    r9_eq = (r9 && er && r9 == er) ? 1 : 0;
    g_enc.entry_r9 = r9;
    g_enc.entry_er_rw = er;
    printf("[MODULE_DATA_INIT] module=%s rw_base=0x%X rw_size=%u bss_base=0x0 bss_size=0 "
           "template_copied=unknown bss_zeroed=unknown relocations_applied=unknown "
           "entry_r9=0x%X er_rw=0x%X r9_eq_er_rw=%d p_guest=0x%X p_len=%u evidence=OBSERVED\n",
           m->resolved_name[0] ? m->resolved_name : m->requested_name, rw, rws, r9, er, r9_eq,
           g_enc.p_guest, g_enc.p_len);
    fflush(stdout);
}

static void analyze_entry_path(void *uc, uint32_t entry_pc, uint32_t entry_off) {
    int i, thumb;
    uint32_t pc;
    int branch_id = 0;
    int leads = 0;
    char cond_src[24] = "UNKNOWN";
    uint32_t cond_reg = 0, cond_val = 0;
    if (g_enc.path_logged || !uc || !entry_pc) return;
    g_enc.path_logged = 1;
    thumb = (entry_pc & 1u) != 0;
    pc = entry_pc & ~1u;
    for (i = 0; i < GWY_ENC_PATH_INSNS; i++) {
        uint32_t word = 0;
        int rt = -1, rn = -1, is_load = 0;
        uint32_t imm = 0;
        if (thumb) {
            uint16_t half;
            uint32_t a = pc + (uint32_t)(i * 2);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, a & ~3u, &word)) continue;
            half = (uint16_t)((a & 2u) ? (word >> 16) : (word & 0xFFFFu));
            /* CMP Rn,#imm */
            if ((half & 0xF800u) == 0x2800u) {
                branch_id = i;
                cond_reg = (half >> 8) & 7u;
                cond_val = half & 0xFFu;
                snprintf(cond_src, sizeof(cond_src), "%s", cond_reg == 0 ? "ARGUMENT" : "UNKNOWN");
            }
            if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load) {
                if (rn == 0 && imm == 0x28u) leads = 1;
                if (rn == 9) {
                    snprintf(cond_src, sizeof(cond_src), "%s", "MODULE_RW");
                    cond_reg = 9;
                    cond_val = imm;
                    g_enc.saw_r9_load = 1;
                    g_enc.r9_load_imm = imm;
                }
            }
        } else {
            uint32_t a = pc + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, a, &word)) continue;
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load) {
                if (rn == 0 && imm == 0x28u) leads = 1;
                if (rn == 9) {
                    snprintf(cond_src, sizeof(cond_src), "%s", "MODULE_RW");
                    cond_reg = 9;
                    cond_val = imm;
                }
            }
        }
    }
    if (g_enc.saw_r0_plus_28) leads = 1;
    printf("[ENTRY_PATH] entry_offset=0x%X branch_id=%d condition_register=r%u "
           "condition_value=0x%X condition_source=%s leads_to_null_deref=%s evidence=OBSERVED\n",
           entry_off, branch_id, cond_reg, cond_val, cond_src, leads ? "yes" : "no");
    fflush(stdout);
}

static void classify_and_emit(void) {
    ExtNullContractClass c = NULL_CONTRACT_UNKNOWN;
    const char *gate = "blocked";
    const char *gev = "none";
    int all_zero = 1, i;
    if (g_enc.root_logged) return;
    g_enc.root_logged = 1;
    for (i = 0; i < g_enc.call_count; i++) {
        if (g_enc.calls[i].r0 != 0) all_zero = 0;
    }
    if (g_enc.xt_crossed && g_enc.entry_r0 == 0)
        c = NULL_CONTRACT_ENTRY_NULL_CROSS_TARGET;
    else if (g_enc.xt_crossed && g_enc.entry_r0 != 0)
        c = NULL_CONTRACT_MISSING_MODULE_ENTRY_CONTEXT;
    else if (g_enc.module_entry_call_ord && g_enc.mr_helper_first_ord &&
             g_enc.module_entry_call_ord < g_enc.helper_register_ord)
        c = NULL_CONTRACT_WRONG_MODULE_ENTRY_LIFECYCLE;
    else if (g_enc.entry_fault && g_enc.entry_r0 == 0 && all_zero && g_enc.call_count >= 2)
        /* Same family always NULL but robotol faults — prefer data-init over context. */
        c = NULL_CONTRACT_MISSING_MODULE_DATA_INITIALIZATION;
    else if (g_enc.entry_fault && g_enc.entry_r0 == 0 &&
             (!g_enc.entry_er_rw || (g_enc.entry_r9 && g_enc.entry_er_rw &&
                                    g_enc.entry_r9 != g_enc.entry_er_rw)))
        c = NULL_CONTRACT_MISSING_PRE_ENTRY_INITIALIZATION;
    else if (g_enc.entry_fault && g_enc.entry_r0 == 0 && g_enc.saw_r0_plus_28 && !all_zero)
        c = NULL_CONTRACT_MISSING_MODULE_ENTRY_CONTEXT;
    g_enc.last_class = c;
    if (c == NULL_CONTRACT_ENTRY_NULL_DOCUMENTED) {
        gate = "blocked"; /* still blocked: opens data-init research, not Phase B */
        gev = "DOCUMENTED";
    } else if (c == NULL_CONTRACT_ENTRY_NULL_CROSS_TARGET) {
        gate = "blocked";
        gev = "CROSS_TARGET";
    }
    printf("[NULL_CONTRACT_CLASS] class=%s all_r0_zero=%d xt_crossed=%d entry_r0=0x%X "
           "saw_r0_plus_28=%d mr_helper_seen=%d phase6b_b_gate=%s gate_evidence=%s "
           "evidence=OBSERVED note=no_guest_mutation\n",
           ext_null_contract_class_name(c), all_zero, g_enc.xt_crossed, g_enc.entry_r0,
           g_enc.saw_r0_plus_28, g_enc.mr_helper_seen, gate, gev);
    printf("[DISCRIMINATION_MATRIX] H1_null_legal=%s H2_missing_ctx=%s "
           "dsm_all_r0_zero=%d xt_entry_null_ok=%d xt_entry_nonzero=%d "
           "rw_r9_mismatch=%d lifecycle_odd=%d\n",
           (c == NULL_CONTRACT_ENTRY_NULL_CROSS_TARGET ||
            c == NULL_CONTRACT_MISSING_MODULE_DATA_INITIALIZATION ||
            c == NULL_CONTRACT_MISSING_PRE_ENTRY_INITIALIZATION)
               ? "supported"
               : "unproven",
           (c == NULL_CONTRACT_MISSING_MODULE_ENTRY_CONTEXT) ? "supported" : "unproven", all_zero,
           (g_enc.xt_crossed && g_enc.entry_r0 == 0) ? 1 : 0,
           (g_enc.xt_crossed && g_enc.entry_r0 != 0) ? 1 : 0,
           (g_enc.entry_r9 && g_enc.entry_er_rw && g_enc.entry_r9 != g_enc.entry_er_rw) ? 1 : 0,
           (g_enc.module_entry_call_ord && g_enc.helper_register_ord &&
            g_enc.module_entry_call_ord < g_enc.helper_register_ord)
               ? 1
               : 0);
    fflush(stdout);
}

void ext_entry_null_contract_on_code_image(uint32_t guest_addr, uint32_t size) {
    if (!ext_entry_null_contract_enabled() || !guest_addr) return;
    (void)size;
    if (!g_enc.code_image_ord) {
        g_enc.code_image_ord = next_ord();
        g_enc.load_return_ord = g_enc.code_image_ord;
        seq_append("CODE_IMAGE");
        seq_append("cacheSync");
    } else {
        next_ord();
        seq_append("CODE_IMAGE");
    }
    maybe_type_evidence();
    maybe_p_link();
}

void ext_entry_null_contract_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len) {
    if (!ext_entry_null_contract_enabled() || !helper) return;
    g_enc.helper = helper;
    g_enc.p_guest = p_guest;
    g_enc.p_len = p_len;
    maybe_p_link();
}

void ext_entry_null_contract_on_registered(uint64_t module_id, uint32_t helper) {
    if (!ext_entry_null_contract_enabled() || !helper) return;
    (void)module_id;
    g_enc.helper = helper;
    if (!g_enc.helper_register_ord) {
        g_enc.helper_register_ord = next_ord();
        seq_append("register");
    }
}

void ext_entry_null_contract_on_mr_helper(uint32_t helper, uint32_t code, uint32_t p_guest) {
    if (!ext_entry_null_contract_enabled()) return;
    (void)helper;
    (void)code;
    (void)p_guest;
    g_enc.mr_helper_seen = 1;
    if (!g_enc.mr_helper_first_ord) {
        g_enc.mr_helper_first_ord = next_ord();
        seq_append("mr_helper");
    }
}

void ext_entry_null_contract_on_second_hop(void *uc, uint32_t caller_pc, uint32_t target,
                                           uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                                           uint64_t from_mid, uint64_t to_mid,
                                           const char *target_relation) {
    ModuleRegistry *reg;
    const GwyLoadedModule *from_m, *to_m;
    EncEntryCall *c;
    uint32_t caller_off = 0, target_off = 0;
    const char *rel = target_relation ? target_relation : "unknown";
    const char *r0src;
    char stage[16];
    if (!ext_entry_null_contract_enabled() || !target) return;
    reg = gwy_ext_loader_bound_registry();
    from_m = reg ? module_registry_find_by_id(reg, from_mid) : NULL;
    to_m = reg ? module_registry_find_by_id(reg, to_mid) : NULL;
    if (!to_m) return;
    /* Only DSM → nested EXT family. */
    if (from_m && from_m->origin != MODULE_ORIGIN_DSM &&
        !(from_m->requested_name[0] && strstr(from_m->requested_name, "dsm"))) {
        /* allow when from display is dsm */
        const char *fn = from_m->resolved_name[0] ? from_m->resolved_name : from_m->requested_name;
        if (!strstr(fn, "cfunction") && from_m->origin != MODULE_ORIGIN_DSM) return;
    }
    if (strcmp(rel, "helper") == 0) return;
    if (from_m && from_m->map.guest_code_base && (caller_pc & ~1u) >= from_m->map.guest_code_base)
        caller_off = (caller_pc & ~1u) - from_m->map.guest_code_base;
    if (to_m->map.guest_code_base && (target & ~1u) >= to_m->map.guest_code_base)
        target_off = (target & ~1u) - to_m->map.guest_code_base;

    if (strcmp(rel, "trampoline") == 0 && !to_m->entries.registered_helper)
        snprintf(stage, sizeof(stage), "%s", "FIRST_CALL");
    else if (strcmp(rel, "internal_entry") == 0)
        snprintf(stage, sizeof(stage), "%s",
                 g_enc.module_entry_call_ord ? "RECALL" : "FIRST_CALL");
    else
        snprintf(stage, sizeof(stage), "%s", "UNKNOWN");

    r0src = recover_r0_source(uc ? uc : g_enc.uc, caller_pc);
    if (r0 == 0 && strcmp(r0src, "unknown") == 0) r0src = "explicit_zero";

    if (g_enc.call_count < GWY_ENC_CALL_MAX) {
        c = &g_enc.calls[g_enc.call_count++];
        memset(c, 0, sizeof(*c));
        c->callsite_id = (uint32_t)g_enc.call_count;
        c->caller_offset = caller_off;
        c->to_mid = to_mid;
        snprintf(c->module, sizeof(c->module), "%s",
                 to_m->resolved_name[0] ? to_m->resolved_name : to_m->requested_name);
        c->target = target;
        c->target_offset = target_off;
        c->r0 = r0;
        c->r1 = r1;
        c->r2 = r2;
        c->r3 = r3;
        snprintf(c->r0_source, sizeof(c->r0_source), "%s", r0src);
        snprintf(c->stage, sizeof(c->stage), "%s", stage);
        c->logged = 1;
        printf("[DSM_MODULE_ENTRY_CALL] callsite_id=%u caller_offset=0x%X module=%s "
               "target_offset=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r0_source=%s stage=%s "
               "return_seen=0 return_r0=0x0 evidence=OBSERVED\n",
               c->callsite_id, caller_off, c->module, target_off, r0, r1, r2, r3, r0src, stage);
        fflush(stdout);
    }

    /* Primary CODE_IMAGE hop: trampoline init or internal_entry. */
    if (strcmp(rel, "internal_entry") == 0 || strcmp(rel, "trampoline") == 0 ||
        (to_m->entries.registered_helper &&
         (target & ~1u) != (to_m->entries.registered_helper & ~1u))) {
        int is_primary = (strcmp(rel, "internal_entry") == 0) ||
                         (strcmp(rel, "trampoline") == 0 && target_off <= 0x40u);
        if (is_primary && !g_enc.module_entry_call_ord) {
            g_enc.module_entry_call_ord = next_ord();
            seq_append("module_entry");
            g_enc.entry_mid = to_mid;
            g_enc.entry_pc = target;
            g_enc.entry_r0 = r0;
            g_enc.entry_return_lr = caller_pc;
            g_enc.sampling_pc = target & ~1u;
            g_enc.sample_count = 0;
            analyze_entry_path(uc ? uc : g_enc.uc, target, target_off);
            emit_data_init(to_m, 0);
            emit_lifecycle();
            /* Early family so live gate sees tags even if kill cuts before fault. */
            emit_family();
            /* Provisional class; mem_fault will re-emit with full evidence. */
            if (!g_enc.root_logged) classify_and_emit();
        } else if (strcmp(rel, "internal_entry") == 0 && g_enc.module_entry_call_ord) {
            /* Second hop after trampoline: refresh entry PC to CODE_POINTER. */
            g_enc.entry_pc = target;
            g_enc.entry_r0 = r0;
            g_enc.entry_return_lr = caller_pc;
            g_enc.sampling_pc = target & ~1u;
            g_enc.sample_count = 0;
            g_enc.path_logged = 0;
            analyze_entry_path(uc ? uc : g_enc.uc, target, target_off);
        }
    }
    maybe_type_evidence();
    /* Partial family snapshot after each call (finalized again on fault). */
    if (g_enc.call_count >= 1 && !g_enc.family_logged && g_enc.module_entry_call_ord) {
        /* defer family until ≥1 primary or fault; allow early peek */
    }
}

void ext_entry_null_contract_on_return_to_dsm(uint64_t from_mid, uint32_t return_r0) {
    (void)from_mid;
    (void)return_r0;
    /* Intentionally unused: any nested→DSM API call is not an entry return. */
}

void ext_entry_null_contract_on_code(void *uc, uint64_t module_id, uint32_t pc,
                                     const uint32_t regs[16], uint32_t cpsr) {
    int thumb;
    uint32_t word = 0;
    int rt = -1, rn = -1, is_load = 0;
    uint32_t imm = 0;
    if (!ext_entry_null_contract_enabled() || !regs) return;

    /* True entry return: PC lands back at DSM BLX site (LR from second-hop). */
    if (g_enc.entry_mid && g_enc.entry_return_lr && !g_enc.xt_crossed && !g_enc.entry_fault) {
        uint32_t want = g_enc.entry_return_lr & ~1u;
        uint32_t now = pc & ~1u;
        if (now == want || now == (want + 2u) || now == (want + 4u)) {
            int i;
            g_enc.xt_crossed = 1;
            for (i = 0; i < g_enc.call_count; i++) {
                EncEntryCall *c = &g_enc.calls[i];
                if (c->to_mid == g_enc.entry_mid && !c->return_seen) {
                    c->return_seen = 1;
                    c->return_r0 = regs[0];
                    printf("[DSM_MODULE_ENTRY_CALL] callsite_id=%u stage=RETURN module=%s "
                           "return_seen=1 return_r0=0x%X evidence=OBSERVED\n",
                           c->callsite_id, c->module, regs[0]);
                    fflush(stdout);
                    break;
                }
            }
            {
                ModuleRegistry *reg = gwy_ext_loader_bound_registry();
                const GwyLoadedModule *m =
                    reg ? module_registry_find_by_id(reg, g_enc.entry_mid) : NULL;
                printf("[XT_ENTRY_CROSS] target=%s first_r0=0x%X returned=1 "
                       "fault_on_first_entry=0 helper_reached=%d evidence=OBSERVED\n",
                       m ? (m->resolved_name[0] ? m->resolved_name : m->requested_name) : "unknown",
                       g_enc.entry_r0, g_enc.mr_helper_seen);
                fflush(stdout);
            }
        }
    }

    if (g_enc.helper && (pc & ~1u) == (g_enc.helper & ~1u) && !g_enc.mr_helper_seen) {
        if (g_enc.p_guest && regs[0] == g_enc.p_guest)
            ext_entry_null_contract_on_mr_helper(g_enc.helper, regs[1], g_enc.p_guest);
    }

    if (g_enc.sampling_pc && g_enc.entry_mid == module_id &&
        g_enc.sample_count < GWY_ENC_PATH_INSNS) {
        g_enc.sample_count++;
        thumb = (cpsr & (1u << 5)) != 0;
        if (!g_enc.entry_r9 && regs[9]) {
            ModuleRegistry *reg = gwy_ext_loader_bound_registry();
            const GwyLoadedModule *m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
            g_enc.entry_r9 = regs[9];
            if (m) {
                if (!g_enc.data_init_logged)
                    emit_data_init(m, regs[9]);
                else if (g_enc.entry_er_rw && regs[9] != g_enc.entry_er_rw) {
                    printf("[MODULE_DATA_INIT] stage=R9_UPDATE entry_r9=0x%X er_rw=0x%X "
                           "r9_eq_er_rw=0 evidence=OBSERVED\n",
                           regs[9], g_enc.entry_er_rw);
                    fflush(stdout);
                }
            }
        }
        if (thumb) {
            uint16_t half;
            if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) {
                half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
                if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load) {
                    if (rn == 0 && imm == 0x28u) g_enc.saw_r0_plus_28 = 1;
                    if (rn == 9) {
                        g_enc.saw_r9_load = 1;
                        g_enc.r9_load_imm = imm;
                    }
                }
            }
        }
    }
    (void)uc;
}

void ext_entry_null_contract_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr, uint32_t r0,
                                          uint32_t r9) {
    if (!ext_entry_null_contract_enabled()) return;
    (void)fault_addr;
    (void)r0;
    g_enc.entry_fault = 1;
    if (r9 && !g_enc.entry_r9) g_enc.entry_r9 = r9;
    if (g_enc.entry_mid && !g_enc.xt_crossed) {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *m =
            reg ? module_registry_find_by_id(reg, g_enc.entry_mid) : NULL;
        printf("[XT_ENTRY_CROSS] target=%s first_r0=0x%X returned=0 fault_on_first_entry=1 "
               "helper_reached=%d fault_pc=0x%X evidence=OBSERVED\n",
               m ? (m->resolved_name[0] ? m->resolved_name : m->requested_name) : "unknown",
               g_enc.entry_r0, g_enc.mr_helper_seen, fault_pc);
        fflush(stdout);
    }
    g_enc.family_logged = 0; /* refresh with full call set */
    g_enc.root_logged = 0;
    g_enc.lifecycle_logged = 0;
    emit_family();
    emit_lifecycle();
    classify_and_emit();
}
