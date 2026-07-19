#include "gwy_launcher/ext_post_cfn_r9_audit.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_er_rw_bind_restore.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PCFN_RING 64
#define PCFN_GATE_LINE                                                                            \
    "post_cfn_r9_gate=%s post_continuation_gate=open graphics_gate=blocked "                      \
    "event_scheduler_gate=blocked nested_r9_scope_gate=open module_r9_switch_gate=open "          \
    "guest_callback_frame_gate=blocked bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked "   \
    "er_rw_metadata_timing_gate=blocked"
#define PCFN_GATE_ARGS (g_pcfn.gate_open ? "open" : "blocked")

typedef struct R0Ring {
    uint32_t pc;
    uint32_t insn;
    uint32_t r0_before;
    uint32_t r0_after;
    uint32_t r9;
    uint32_t r3;
    int thumb;
    int wrote_r0;
} R0Ring;

typedef struct PSnap {
    int valid;
    uint32_t p;
    uint32_t f00, f04, f08, f0c, f10;
} PSnap;

static struct {
    int enabled;
    int enabled_known;
    void *uc;
    int armed;
    int finalized;
    int gate_open;
    PostCfnR9Class last_class;
    char next_fix[72];
    char stop_reason[48];

    uint64_t module_id;
    char module[72];
    uint32_t cont_pc;
    uint32_t call_pc;
    uint32_t cont_r9;
    uint32_t helper;
    uint32_t p_guest;
    uint32_t p_len;

    PSnap before_cont;
    PSnap after_cont;
    PSnap at_fault;
    int er_rw_ready;
    int er_rw_ready_before_cont;
    uint32_t er_rw_exact;
    uint32_t er_rw_size;
    uint32_t event_seq;
    PostCfnRegistryPending reg_pending;

    R0Ring ring[PCFN_RING];
    int ring_count;
    int ring_head;
    uint32_t last_r0;
    int have_last_r0;

    PostCfnR0Source r0_source;
    uint32_t r0_last_writer_pc;
    uint32_t r0_last_insn;
    uint32_t r0_source_address;
    uint32_t r0_source_value;

    uint32_t fault_pc;
    uint32_t fault_addr;
    uint32_t fault_insn;
    uint32_t fault_r9;
    int fault_thumb;
    int fault_rt, fault_rn;
    uint32_t fault_imm;
    int fault_is_load;
    uint32_t function_start;
    char address_expr[48];
    int fault_seen;

    int saw_guest_r9_to_er_rw;
    int promotion_needed;
} g_pcfn;

const char *ext_post_cfn_r9_class_name(PostCfnR9Class c) {
    switch (c) {
    case PCFN_POST_CFN_R9_PROMOTION_REQUIRED: return "POST_CFN_R9_PROMOTION_REQUIRED";
    case PCFN_ER_RW_REGISTRY_PUBLICATION_LATE: return "ER_RW_REGISTRY_PUBLICATION_LATE";
    case PCFN_GUEST_FIXR9_PATH_MISSING: return "GUEST_FIXR9_PATH_MISSING";
    case PCFN_CONTINUATION_SCOPE_NOT_PROMOTED: return "CONTINUATION_SCOPE_NOT_PROMOTED";
    case PCFN_R0_SOURCE_INDEPENDENT_OF_R9: return "R0_SOURCE_INDEPENDENT_OF_R9";
    case PCFN_P_MODULE_ASSOCIATION_MISSING: return "P_MODULE_ASSOCIATION_MISSING";
    default: return "UNKNOWN";
    }
}

const char *ext_post_cfn_r0_source_name(PostCfnR0Source s) {
    switch (s) {
    case PCFN_R0_R9_REL_GLOBAL: return "R9_REL_GLOBAL";
    case PCFN_R0_P_FIELD: return "P_FIELD";
    case PCFN_R0_RETURN_VALUE: return "RETURN_VALUE";
    case PCFN_R0_ARGUMENT: return "ARGUMENT";
    case PCFN_R0_STACK: return "STACK";
    default: return "UNKNOWN";
    }
}

const char *ext_post_cfn_registry_pending_name(PostCfnRegistryPending r) {
    switch (r) {
    case PCFN_REG_PUBLICATION_LATE: return "ER_RW_REGISTRY_PUBLICATION_LATE";
    case PCFN_REG_MODULE_ASSOCIATION_MISSING: return "P_MODULE_ASSOCIATION_MISSING";
    case PCFN_REG_OBSERVER_LAG: return "OBSERVER_LAG";
    case PCFN_REG_PUBLISHED: return "PUBLISHED";
    default: return "UNKNOWN";
    }
}

PostCfnR9Class ext_post_cfn_r9_audit_last_class(void) { return g_pcfn.last_class; }
int ext_post_cfn_r9_gate_open(void) { return g_pcfn.gate_open; }
int ext_post_cfn_r9_audit_armed(void) { return g_pcfn.armed; }

int ext_post_cfn_r9_audit_enabled(void) {
    const char *e;
    if (g_pcfn.enabled_known) return g_pcfn.enabled;
    e = getenv("GWY_POST_CFN_R9_AUDIT");
    if (e && e[0] == '1' && e[1] == '\0') {
        g_pcfn.enabled = 1;
    } else {
        const char *pc = getenv("GWY_POST_CONT_AUDIT");
        const char *sw = getenv("GWY_MODULE_R9_SWITCH");
        g_pcfn.enabled = ((pc && pc[0] == '1') || (sw && sw[0] == '1')) ? 1 : 0;
    }
    g_pcfn.enabled_known = 1;
    return g_pcfn.enabled;
}

void ext_post_cfn_r9_audit_reset(void) {
    memset(&g_pcfn, 0, sizeof(g_pcfn));
    snprintf(g_pcfn.next_fix, sizeof(g_pcfn.next_fix), "%s", "NONE");
    snprintf(g_pcfn.stop_reason, sizeof(g_pcfn.stop_reason), "%s", "UNKNOWN");
}

void ext_post_cfn_r9_audit_bind_uc(void *uc) { g_pcfn.uc = uc; }

static int name_is_robotol(const char *nm) {
    return nm && strstr(nm, "robotol.ext") != NULL;
}

static void emit_fixr9_contracts(void) {
    printf("[FIXR9_STAGE_CONTRACT] stage=bootstrap_start r9_contract=CALLER_OR_UNKNOWN "
           "evidence=DOCUMENTED source=fixR9.c " PCFN_GATE_LINE "\n",
           PCFN_GATE_ARGS);
    printf("[FIXR9_STAGE_CONTRACT] stage=mr_c_function_load_internal "
           "r9_contract=CALLER_OR_TEMP_NO_PERMANENT_SWITCH evidence=DOCUMENTED source=fixR9.c "
           PCFN_GATE_LINE "\n",
           PCFN_GATE_ARGS);
    printf("[FIXR9_STAGE_CONTRACT] stage=post_cfn_continuation "
           "r9_contract=NOT_DOCUMENTED_AS_AUTO_PROMOTION evidence=DOCUMENTED "
           "note=helper_entry_sets_r9_not_cfn_return " PCFN_GATE_LINE "\n",
           PCFN_GATE_ARGS);
    printf("[FIXR9_STAGE_CONTRACT] stage=registered_helper "
           "r9_contract=CALLEE_ER_RW_P_START_OF_ER_RW evidence=DOCUMENTED source=fixR9.c:L20-21 "
           PCFN_GATE_LINE "\n",
           PCFN_GATE_ARGS);
    printf("[FIXR9_STAGE_CONTRACT] stage=callback r9_contract=PENDING "
           "evidence=CROSS_TARGET source=dsm.c:network_cb " PCFN_GATE_LINE "\n",
           PCFN_GATE_ARGS);
    fflush(stdout);
}

static int peek_p_snap(void *uc, uint32_t p, PSnap *out) {
    uint32_t v0 = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0;
    if (!out || !p || !uc) return 0;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, p + 0x00u, &v0)) return 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, p + 0x04u, &v1);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, p + 0x08u, &v2);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, p + 0x0Cu, &v3);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, p + 0x10u, &v4);
    out->valid = 1;
    out->p = p;
    out->f00 = v0;
    out->f04 = v1;
    out->f08 = v2;
    out->f0c = v3;
    out->f10 = v4;
    return 1;
}

static void emit_p_snap(const char *stage, const PSnap *s) {
    if (!s || !s->valid) {
        printf("[CFUNCTION_P_SNAPSHOT] stage=%s p=0x0 valid=no " PCFN_GATE_LINE
               " evidence=OBSERVED\n",
               stage ? stage : "?", PCFN_GATE_ARGS);
        fflush(stdout);
        return;
    }
    printf("[CFUNCTION_P_SNAPSHOT] stage=%s p=0x%X field_00=0x%X field_04=0x%X field_08=0x%X "
           "field_0C=0x%X field_10=0x%X "
           "names=start_of_ER_RW,ER_RW_Length,ext_type,mrc_extChunk,stack "
           "evidence=DOCUMENTED " PCFN_GATE_LINE "\n",
           stage ? stage : "?", s->p, s->f00, s->f04, s->f08, s->f0c, s->f10, PCFN_GATE_ARGS);
    fflush(stdout);
}

static void refresh_registry_pending(void) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = NULL;
    size_t i;
    if (!reg) {
        g_pcfn.reg_pending = PCFN_REG_MODULE_ASSOCIATION_MISSING;
        return;
    }
    if (g_pcfn.helper) m = module_registry_find_by_helper(reg, g_pcfn.helper);
    if (!m && g_pcfn.module_id) m = module_registry_find_by_id(reg, g_pcfn.module_id);
    if (!m) {
        for (i = 0; i < reg->count; i++) {
            if (name_is_robotol(reg->modules[i].resolved_name)) {
                m = &reg->modules[i];
                break;
            }
        }
    }
    if (!m) {
        g_pcfn.reg_pending = PCFN_REG_MODULE_ASSOCIATION_MISSING;
        return;
    }
    if (!g_pcfn.module_id) g_pcfn.module_id = m->module_id;
    if (!g_pcfn.helper && m->entries.registered_helper)
        g_pcfn.helper = m->entries.registered_helper;
    if (m->data.start_of_er_rw) {
        g_pcfn.reg_pending = PCFN_REG_PUBLISHED;
        return;
    }
    if (g_pcfn.er_rw_exact || (g_pcfn.after_cont.valid && g_pcfn.after_cont.f00) ||
        (g_pcfn.at_fault.valid && g_pcfn.at_fault.f00)) {
        g_pcfn.reg_pending = PCFN_REG_PUBLICATION_LATE;
        return;
    }
    if (g_pcfn.p_guest && g_pcfn.helper)
        g_pcfn.reg_pending = PCFN_REG_PUBLICATION_LATE;
    else
        g_pcfn.reg_pending = PCFN_REG_MODULE_ASSOCIATION_MISSING;
}

static void maybe_mark_er_rw_ready(const PSnap *s, int before_cont) {
    if (!s || !s->valid || !s->f00) return;
    /* Reject caller/DSM SB masquerading as ER_RW when it equals continuation R9
     * and P was still empty at resume (field filled later). */
    if (before_cont && g_pcfn.cont_r9 && s->f00 == g_pcfn.cont_r9) return;
    if (!g_pcfn.er_rw_exact) {
        g_pcfn.er_rw_exact = s->f00;
        g_pcfn.er_rw_size = s->f04;
        g_pcfn.er_rw_ready = 1;
        g_pcfn.event_seq++;
        if (before_cont) g_pcfn.er_rw_ready_before_cont = 1;
        printf("[ROBOTOL_ER_RW_READY] event_seq=%u p=0x%X er_rw=0x%X size=%u "
               "available_before_continuation=%s registry_published=%s " PCFN_GATE_LINE
               " evidence=OBSERVED\n",
               g_pcfn.event_seq, s->p, s->f00, s->f04,
               g_pcfn.er_rw_ready_before_cont ? "yes" : "no",
               (g_pcfn.reg_pending == PCFN_REG_PUBLISHED) ? "yes" : "no", PCFN_GATE_ARGS);
        fflush(stdout);
        /* Stage E2: guest filled P+0/+4 after FixR9 slot publish — bind registry + R9. */
        if (s->p && s->f00 && s->f04)
            ext_er_rw_bind_restore_peek_and_bind(s->p, "mr_c_function_st_metadata_bind");
    } else if (s->f00 && s->f00 != g_pcfn.er_rw_exact) {
        /* Prefer later guest P fill over any earlier mistaken base. */
        if (g_pcfn.cont_r9 && g_pcfn.er_rw_exact == g_pcfn.cont_r9) {
            g_pcfn.er_rw_exact = s->f00;
            g_pcfn.er_rw_size = s->f04;
            g_pcfn.er_rw_ready_before_cont = 0;
        } else {
            g_pcfn.er_rw_exact = s->f00;
            g_pcfn.er_rw_size = s->f04;
        }
    }
}

static int classify_r0_writer(const R0Ring *e, PostCfnR0Source *src, uint32_t *src_addr) {
    int rt = 0, rn = 0, is_load = 0;
    uint32_t imm = 0;
    if (!e || !src) return 0;
    *src = PCFN_R0_UNKNOWN;
    if (src_addr) *src_addr = 0;

    if (ext_entry_decode_ldr_imm(e->insn, e->thumb, &rt, &rn, &imm, &is_load) && is_load &&
        rt == 0) {
        if (rn == 9) {
            *src = PCFN_R0_R9_REL_GLOBAL;
            if (src_addr) *src_addr = e->r9 + imm;
            return 1;
        }
        if (rn == 13) {
            *src = PCFN_R0_STACK;
            if (src_addr) *src_addr = imm;
            return 1;
        }
        if (g_pcfn.p_guest && (rn == 3 || e->r3 == g_pcfn.p_guest)) {
            *src = PCFN_R0_P_FIELD;
            if (src_addr) *src_addr = (e->r3 ? e->r3 : g_pcfn.p_guest) + imm;
            return 1;
        }
        /* Multi-hop: base was previously loaded from R9 — check recent ring for rn fill from r9. */
        {
            int i, idx;
            for (i = 1; i < g_pcfn.ring_count && i < 16; i++) {
                idx = (g_pcfn.ring_head - 1 - i + PCFN_RING * 2) % PCFN_RING;
                {
                    R0Ring *p = &g_pcfn.ring[idx];
                    int prt = 0, prn = 0, pload = 0;
                    uint32_t pimm = 0;
                    if (ext_entry_decode_ldr_imm(p->insn, p->thumb, &prt, &prn, &pimm, &pload) &&
                        pload && prt == rn && prn == 9) {
                        *src = PCFN_R0_R9_REL_GLOBAL;
                        if (src_addr) *src_addr = p->r9 + pimm;
                        return 1;
                    }
                }
            }
        }
        return 1;
    }

    /* Thumb: MOVS r0,#imm8 / MOV r0,rN / ADDS r0,rN,#imm */
    if (e->thumb) {
        uint16_t h = (uint16_t)(e->insn & 0xFFFFu);
        if ((h & 0xFF00u) == 0x2000u) { /* MOVS r0,#imm8 */
            *src = PCFN_R0_ARGUMENT;
            if (src_addr) *src_addr = h & 0xFFu;
            return 1;
        }
        /* ADDS Rd,Rn,#imm3: 0001110 iii nnn ddd */
        if ((h & 0xFE00u) == 0x1C00u) {
            int rd = (int)(h & 7u);
            int rn = (int)((h >> 3) & 7u);
            if (rd == 0) {
                if (rn == 9 || (g_pcfn.er_rw_exact && e->r9 == g_pcfn.er_rw_exact && rn == 9)) {
                    *src = PCFN_R0_R9_REL_GLOBAL;
                } else {
                    *src = PCFN_R0_ARGUMENT;
                }
                return 1;
            }
        }
        /* ADDS Rd,#imm8: 00110 ddd iiiiiiii — if Rd==0 and value stayed 0, likely imm0 */
        if ((h & 0xF800u) == 0x3000u && ((h >> 8) & 7u) == 0u) {
            *src = PCFN_R0_ARGUMENT;
            if (src_addr) *src_addr = h & 0xFFu;
            return 1;
        }
        if ((h & 0xFF00u) == 0x4600u) {
            int rd = (int)((h & 7u) | ((h >> 4) & 8u));
            int rm = (int)((h >> 3) & 0xFu);
            if (rd == 0) {
                *src = (rm == 9) ? PCFN_R0_R9_REL_GLOBAL : PCFN_R0_ARGUMENT;
                return 1;
            }
        }
    }

    /* After BL/BLX, r0 often return value — heuristic if pc near and r0 changed. */
    if (e->thumb) {
        uint16_t h = (uint16_t)(e->insn & 0xFFFFu);
        if ((h & 0xFF80u) == 0x4780u || (h & 0xF800u) == 0xF000u)
            *src = PCFN_R0_RETURN_VALUE;
    }
    return 1;
}

static void ring_push(uint32_t pc, uint32_t insn, uint32_t r0_before, uint32_t r0_after, uint32_t r9,
                      uint32_t r3, int thumb) {
    R0Ring *e = &g_pcfn.ring[g_pcfn.ring_head];
    e->pc = pc;
    e->insn = insn;
    e->r0_before = r0_before;
    e->r0_after = r0_after;
    e->r9 = r9;
    e->r3 = r3;
    e->thumb = thumb;
    e->wrote_r0 = (r0_before != r0_after) ? 1 : 0;
    g_pcfn.ring_head = (g_pcfn.ring_head + 1) % PCFN_RING;
    if (g_pcfn.ring_count < PCFN_RING) g_pcfn.ring_count++;

    if (e->wrote_r0 && r0_after == 0 && g_pcfn.ring_count >= 2) {
        /* r0 changed between prior and current hook — writer is the previous insn. */
        int pidx = (g_pcfn.ring_head - 2 + PCFN_RING) % PCFN_RING;
        R0Ring *w = &g_pcfn.ring[pidx];
        PostCfnR0Source src = PCFN_R0_UNKNOWN;
        uint32_t saddr = 0;
        classify_r0_writer(w, &src, &saddr);
        g_pcfn.r0_source = src;
        g_pcfn.r0_last_writer_pc = w->pc;
        g_pcfn.r0_last_insn = w->insn;
        g_pcfn.r0_source_address = saddr;
        g_pcfn.r0_source_value = 0;
    } else if (e->wrote_r0 && r0_after == 0) {
        PostCfnR0Source src = PCFN_R0_UNKNOWN;
        uint32_t saddr = 0;
        classify_r0_writer(e, &src, &saddr);
        g_pcfn.r0_source = src;
        g_pcfn.r0_last_writer_pc = pc;
        g_pcfn.r0_last_insn = insn;
        g_pcfn.r0_source_address = saddr;
        g_pcfn.r0_source_value = 0;
    }

    if (g_pcfn.er_rw_exact && r9 == g_pcfn.er_rw_exact && r9 != g_pcfn.cont_r9)
        g_pcfn.saw_guest_r9_to_er_rw = 1;
}

static void emit_scope_stack(uint32_t active_r9) {
    uint32_t depth = module_r9_switch_depth();
    uint32_t i;
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    printf("[R9_STACK_SNAPSHOT] depth=%u active_r9=0x%X " PCFN_GATE_LINE " evidence=OBSERVED\n",
           depth, active_r9, PCFN_GATE_ARGS);
    for (i = 0; i < depth; i++) {
        ModuleR9Frame fr;
        const GwyLoadedModule *caller = NULL;
        const GwyLoadedModule *callee = NULL;
        if (!module_r9_switch_peek_at(i, &fr)) continue;
        if (reg) {
            caller = module_registry_find_by_id(reg, fr.caller_module_id);
            callee = module_registry_find_by_id(reg, fr.callee_module_id);
        }
        if (g_pcfn.er_rw_exact &&
            (fr.saved_r9 == g_pcfn.er_rw_exact || fr.callee_r9 == g_pcfn.er_rw_exact))
            g_pcfn.saw_guest_r9_to_er_rw = 1;
        printf("[R9_STACK_SNAPSHOT] frame[%u] frame_id=%llu owner_kind=%s caller=%s callee=%s "
               "saved_r9=0x%X callee_r9=0x%X pushed_by=%s " PCFN_GATE_LINE " evidence=OBSERVED\n",
               i, (unsigned long long)fr.frame_id, gwy_module_call_kind_name(fr.owner_kind),
               caller ? (caller->resolved_name[0] ? caller->resolved_name : caller->requested_name)
                      : "?",
               callee ? (callee->resolved_name[0] ? callee->resolved_name : callee->requested_name)
                      : "?",
               fr.saved_r9, fr.callee_r9, fr.pushed_by[0] ? fr.pushed_by : "?", PCFN_GATE_ARGS);
    }
    fflush(stdout);
}

static void emit_promotion_candidate(uint32_t old_r9) {
    uint32_t new_r9 = g_pcfn.er_rw_exact;
    ModuleR9Scope sc;
    module_r9_switch_last_enter_scope(&sc);
    printf("[R9_SCOPE_PROMOTION_CANDIDATE] scope_id=%llu module=%s old_r9=0x%X new_r9=0x%X "
           "er_rw_ready=%s continuation_pending=%s promotion_performed=no "
           "call_kind=%s " PCFN_GATE_LINE " evidence=OBSERVED\n",
           (unsigned long long)sc.scope_id, g_pcfn.module[0] ? g_pcfn.module : "robotol.ext",
           old_r9, new_r9, g_pcfn.er_rw_ready ? "yes" : "no", g_pcfn.armed ? "yes" : "no",
           gwy_module_call_kind_name(sc.call_kind), PCFN_GATE_ARGS);
    fflush(stdout);
}

static uint32_t find_function_start(void *uc, uint32_t fault_pc, int thumb) {
    uint32_t pc = fault_pc & ~1u;
    int steps;
    (void)thumb;
    if (!uc) return 0;
    /* Walk back up to 64 halfwords looking for PUSH {...,lr} Thumb pattern. */
    for (steps = 0; steps < 64; steps++) {
        uint32_t w = 0;
        uint32_t addr = pc - (uint32_t)(steps * 2);
        if (addr < 0x1000u) break;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr & ~1u, &w)) break;
        {
            uint16_t h = (uint16_t)(w & 0xFFFFu);
            /* PUSH {regs,lr}: 1011 0 10 R MMMMMMMM — 0xB5xx common */
            if ((h & 0xFF00u) == 0xB500u) return addr & ~1u;
        }
    }
    return 0;
}

static void classify_and_summary(const char *stop) {
    PostCfnR9Class c = PCFN_CLASS_UNKNOWN;
    const char *fix = "NONE";
    int open_gate = 0;
    uint32_t active_r9 = g_pcfn.cont_r9;

    if (g_pcfn.finalized) return;
    g_pcfn.finalized = 1;
    if (stop && stop[0]) snprintf(g_pcfn.stop_reason, sizeof(g_pcfn.stop_reason), "%s", stop);

    refresh_registry_pending();

    active_r9 = g_pcfn.cont_r9;
    if (g_pcfn.fault_seen && g_pcfn.fault_r9)
        active_r9 = g_pcfn.fault_r9;
    else if (g_pcfn.ring_count > 0) {
        int idx = (g_pcfn.ring_head - 1 + PCFN_RING) % PCFN_RING;
        active_r9 = g_pcfn.ring[idx].r9;
    }

    /* Scope needs promotion when guest ER_RW exists but active R9 is still foreign. */
    g_pcfn.promotion_needed =
        g_pcfn.er_rw_ready && g_pcfn.er_rw_exact != 0 && active_r9 != g_pcfn.er_rw_exact;

    if (g_pcfn.r0_source == PCFN_R0_R9_REL_GLOBAL && g_pcfn.promotion_needed) {
        c = PCFN_POST_CFN_R9_PROMOTION_REQUIRED;
        fix = "CURRENT_SCOPE_R9_PROMOTION_ONLY";
        open_gate = 1;
    } else if (g_pcfn.r0_source != PCFN_R0_UNKNOWN && g_pcfn.r0_source != PCFN_R0_R9_REL_GLOBAL &&
               g_pcfn.r0_source != PCFN_R0_ARGUMENT) {
        c = PCFN_R0_SOURCE_INDEPENDENT_OF_R9;
        fix = "NONE";
    } else if (g_pcfn.promotion_needed) {
        /* Primary: ER_RW ready but active R9 still DSM/caller — not a registry-only issue. */
        c = PCFN_CONTINUATION_SCOPE_NOT_PROMOTED;
        if (g_pcfn.r0_source == PCFN_R0_R9_REL_GLOBAL) {
            c = PCFN_POST_CFN_R9_PROMOTION_REQUIRED;
            fix = "CURRENT_SCOPE_R9_PROMOTION_ONLY";
            open_gate = 1;
        } else if (!g_pcfn.saw_guest_r9_to_er_rw) {
            c = PCFN_GUEST_FIXR9_PATH_MISSING;
            fix = "NONE";
        } else {
            /* Guest briefly held ER_RW then nested enter switched back to DSM. */
            c = PCFN_CONTINUATION_SCOPE_NOT_PROMOTED;
            fix = "NONE";
        }
    } else if (g_pcfn.reg_pending == PCFN_REG_MODULE_ASSOCIATION_MISSING) {
        c = PCFN_P_MODULE_ASSOCIATION_MISSING;
        fix = "NONE";
    } else if (g_pcfn.reg_pending == PCFN_REG_PUBLICATION_LATE) {
        c = PCFN_ER_RW_REGISTRY_PUBLICATION_LATE;
        fix = "ROBOTOL_ER_RW_REGISTRY_PUBLISH_ONLY";
    }

    g_pcfn.last_class = c;
    g_pcfn.gate_open = open_gate;
    snprintf(g_pcfn.next_fix, sizeof(g_pcfn.next_fix), "%s", fix);

    printf("[POST_CFN_SUMMARY] post_cfn_r9_class=%s fault_instruction=%s "
           "r0_last_writer=0x%X r0_source_class=%s robotol_er_rw_exact=0x%X "
           "er_rw_ready_before_continuation=%s registry_pending_reason=%s "
           "scope_depth=%u next_allowed_fix=%s stop_reason=%s "
           "function_start=0x%X memory_access_pc=0x%X " PCFN_GATE_LINE
           " evidence=OBSERVED note=observe_only\n",
           ext_post_cfn_r9_class_name(c), g_pcfn.address_expr[0] ? g_pcfn.address_expr : "none",
           g_pcfn.r0_last_writer_pc, ext_post_cfn_r0_source_name(g_pcfn.r0_source),
           g_pcfn.er_rw_exact, g_pcfn.er_rw_ready_before_cont ? "yes" : "no",
           ext_post_cfn_registry_pending_name(g_pcfn.reg_pending), module_r9_switch_depth(),
           g_pcfn.next_fix, g_pcfn.stop_reason, g_pcfn.function_start, g_pcfn.fault_pc,
           PCFN_GATE_ARGS);
    fflush(stdout);
}

void ext_post_cfn_r9_audit_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                          uint32_t rw_base, uint32_t rw_size) {
    if (!ext_post_cfn_r9_audit_enabled()) return;
    if (helper) g_pcfn.helper = helper;
    if (p_guest) {
        g_pcfn.p_guest = p_guest;
        g_pcfn.p_len = p_len ? p_len : 20u;
        if (!g_pcfn.armed && g_pcfn.uc) {
            if (peek_p_snap(g_pcfn.uc, p_guest, &g_pcfn.before_cont)) {
                emit_p_snap("BEFORE_CONTINUATION", &g_pcfn.before_cont);
                maybe_mark_er_rw_ready(&g_pcfn.before_cont, 1);
            }
        }
    }
    if (rw_base) {
        /* Do not treat host/DSM rw_base as robotol ER_RW; wait for P[0] snapshot. */
        (void)rw_base;
        (void)rw_size;
    }
}

void ext_post_cfn_r9_audit_on_continuation_resume(void *uc, uint64_t module_id, const char *module,
                                                  uint32_t call_pc, uint32_t continuation_pc,
                                                  const uint32_t regs[16], uint32_t sp, uint32_t lr,
                                                  uint32_t cpsr) {
    if (!ext_post_cfn_r9_audit_enabled() || g_pcfn.armed) return;
    (void)sp;
    (void)lr;
    (void)cpsr;
    if (!name_is_robotol(module) && module_id == 0) return;
    if (module && !name_is_robotol(module)) {
        /* Allow if resolved robotol via id later. */
        char tmp[72];
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *m = reg && module_id ? module_registry_find_by_id(reg, module_id) : NULL;
        snprintf(tmp, sizeof(tmp), "%s", module);
        if (!name_is_robotol(tmp) && !(m && name_is_robotol(m->resolved_name))) return;
    }

    g_pcfn.armed = 1;
    g_pcfn.uc = uc ? uc : g_pcfn.uc;
    g_pcfn.module_id = module_id;
    snprintf(g_pcfn.module, sizeof(g_pcfn.module), "%s",
             (module && module[0]) ? module : "robotol.ext");
    g_pcfn.cont_pc = continuation_pc;
    g_pcfn.call_pc = call_pc;
    g_pcfn.cont_r9 = regs ? regs[9] : 0;
    g_pcfn.last_r0 = regs ? regs[0] : 0;
    g_pcfn.have_last_r0 = 1;

    emit_fixr9_contracts();

    if (g_pcfn.p_guest && g_pcfn.uc) {
        if (!g_pcfn.before_cont.valid) {
            peek_p_snap(g_pcfn.uc, g_pcfn.p_guest, &g_pcfn.before_cont);
            emit_p_snap("BEFORE_CONTINUATION", &g_pcfn.before_cont);
            maybe_mark_er_rw_ready(&g_pcfn.before_cont, 1);
        }
        peek_p_snap(g_pcfn.uc, g_pcfn.p_guest, &g_pcfn.after_cont);
        emit_p_snap("AFTER_CONTINUATION", &g_pcfn.after_cont);
        maybe_mark_er_rw_ready(&g_pcfn.after_cont, 0);
    }
    refresh_registry_pending();
    emit_promotion_candidate(g_pcfn.cont_r9);
    printf("[POST_CFN_ARM] module=%s continuation_pc=0x%X cont_r9=0x%X p=0x%X helper=0x%X "
           "er_rw=0x%X " PCFN_GATE_LINE " evidence=OBSERVED\n",
           g_pcfn.module, continuation_pc, g_pcfn.cont_r9, g_pcfn.p_guest, g_pcfn.helper,
           g_pcfn.er_rw_exact, PCFN_GATE_ARGS);
    fflush(stdout);
}

void ext_post_cfn_r9_audit_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                   uint32_t cpsr) {
    uint32_t insn = 0;
    int thumb;
    uint32_t r0_before;
    if (!ext_post_cfn_r9_audit_enabled() || !g_pcfn.armed || g_pcfn.finalized || !regs) return;
    (void)module_id;
    g_pcfn.uc = uc ? uc : g_pcfn.uc;
    thumb = (cpsr >> 5) & 1;
    r0_before = g_pcfn.have_last_r0 ? g_pcfn.last_r0 : regs[0];

    if (uc) {
        if (thumb)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~1u, &insn);
        else
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &insn);
    }

    ring_push(pc, insn, r0_before, regs[0], regs[9], regs[3], thumb);
    g_pcfn.last_r0 = regs[0];
    g_pcfn.have_last_r0 = 1;

    /* Periodic P refresh for ER_RW birth after continuation. */
    if (g_pcfn.p_guest && g_pcfn.uc && (g_pcfn.ring_count % 16) == 0) {
        PSnap s;
        if (peek_p_snap(g_pcfn.uc, g_pcfn.p_guest, &s)) {
            g_pcfn.after_cont = s;
            maybe_mark_er_rw_ready(&s, 0);
            refresh_registry_pending();
        }
    }
}

void ext_post_cfn_r9_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                        const uint32_t regs[16], uint32_t cpsr) {
    int thumb;
    uint32_t insn = 0;
    int rt = 0, rn = 0, is_load = 0;
    uint32_t imm = 0;
    if (!ext_post_cfn_r9_audit_enabled() || !g_pcfn.armed || g_pcfn.finalized) return;
    g_pcfn.uc = uc ? uc : g_pcfn.uc;
    g_pcfn.fault_seen = 1;
    g_pcfn.fault_pc = fault_pc;
    g_pcfn.fault_addr = fault_addr;
    if (regs) g_pcfn.fault_r9 = regs[9];
    thumb = regs ? ((cpsr >> 5) & 1) : 1;
    g_pcfn.fault_thumb = thumb;

    if (uc) {
        if (thumb)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, fault_pc & ~1u, &insn);
        else
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, fault_pc & ~3u, &insn);
    }
    g_pcfn.fault_insn = insn;
    if (ext_entry_decode_ldr_imm(insn, thumb, &rt, &rn, &imm, &is_load)) {
        g_pcfn.fault_rt = rt;
        g_pcfn.fault_rn = rn;
        g_pcfn.fault_imm = imm;
        g_pcfn.fault_is_load = is_load;
        snprintf(g_pcfn.address_expr, sizeof(g_pcfn.address_expr), "r%d+0x%X", rn, imm);
    } else {
        snprintf(g_pcfn.address_expr, sizeof(g_pcfn.address_expr), "unknown");
    }

    g_pcfn.function_start = find_function_start(uc, fault_pc, thumb);

    if (g_pcfn.p_guest && uc) {
        peek_p_snap(uc, g_pcfn.p_guest, &g_pcfn.at_fault);
        emit_p_snap("AT_FAULT", &g_pcfn.at_fault);
        maybe_mark_er_rw_ready(&g_pcfn.at_fault, 0);
    }
    refresh_registry_pending();

    /* If r0 already 0 but last writer not recorded, scan ring backward. */
    if (regs && regs[0] == 0 && g_pcfn.r0_last_writer_pc == 0) {
        int i;
        int found = 0;
        for (i = 0; i < g_pcfn.ring_count; i++) {
            int idx = (g_pcfn.ring_head - 1 - i + PCFN_RING * 2) % PCFN_RING;
            R0Ring *e = &g_pcfn.ring[idx];
            if (e->wrote_r0 && e->r0_after == 0) {
                PostCfnR0Source src = PCFN_R0_UNKNOWN;
                uint32_t saddr = 0;
                R0Ring *w = e;
                if (i + 1 < g_pcfn.ring_count) {
                    int pidx = (g_pcfn.ring_head - 2 - i + PCFN_RING * 2) % PCFN_RING;
                    w = &g_pcfn.ring[pidx];
                }
                classify_r0_writer(w, &src, &saddr);
                g_pcfn.r0_source = src;
                g_pcfn.r0_last_writer_pc = w->pc;
                g_pcfn.r0_last_insn = w->insn;
                g_pcfn.r0_source_address = saddr;
                found = 1;
                break;
            }
        }
        if (!found) {
            /* r0 was already 0 at continuation and never rewritten. */
            g_pcfn.r0_source = PCFN_R0_ARGUMENT;
            g_pcfn.r0_last_writer_pc = g_pcfn.cont_pc;
            g_pcfn.r0_last_insn = 0;
            g_pcfn.r0_source_address = 0;
            g_pcfn.r0_source_value = 0;
        }
    }

    printf("[POST_CFN_FAULT] function_start=0x%X memory_access_pc=0x%X instruction=0x%X "
           "thumb=%s address_expr=%s rt=%d rn=%d imm=0x%X is_load=%d "
           "r0=0x%X r3=0x%X r9=0x%X fault_addr=0x%X " PCFN_GATE_LINE " evidence=OBSERVED\n",
           g_pcfn.function_start, fault_pc, insn, thumb ? "yes" : "no", g_pcfn.address_expr,
           g_pcfn.fault_rt, g_pcfn.fault_rn, g_pcfn.fault_imm, g_pcfn.fault_is_load,
           regs ? regs[0] : 0, regs ? regs[3] : 0, regs ? regs[9] : 0, fault_addr, PCFN_GATE_ARGS);

    printf("[POST_CFN_R0_SOURCE] last_writer_pc=0x%X instruction=0x%X source=%s "
           "source_address=0x%X source_value=0x%X " PCFN_GATE_LINE " evidence=OBSERVED\n",
           g_pcfn.r0_last_writer_pc, g_pcfn.r0_last_insn,
           ext_post_cfn_r0_source_name(g_pcfn.r0_source), g_pcfn.r0_source_address,
           g_pcfn.r0_source_value, PCFN_GATE_ARGS);

    emit_scope_stack(regs ? regs[9] : 0);
    emit_promotion_candidate(regs ? regs[9] : g_pcfn.cont_r9);

    printf("[POST_CFN_REGISTRY] pending_reason=%s helper=0x%X p=0x%X er_rw_guest=0x%X "
           "registry_er_rw=0x0 " PCFN_GATE_LINE " evidence=OBSERVED\n",
           ext_post_cfn_registry_pending_name(g_pcfn.reg_pending), g_pcfn.helper, g_pcfn.p_guest,
           g_pcfn.er_rw_exact, PCFN_GATE_ARGS);
    fflush(stdout);

    classify_and_summary("NEW_FAULT");
}

void ext_post_cfn_r9_audit_finalize(const char *stop_reason) {
    if (!ext_post_cfn_r9_audit_enabled() || !g_pcfn.armed) return;
    if (g_pcfn.finalized) return;
    refresh_registry_pending();
    if (g_pcfn.p_guest && g_pcfn.uc) {
        peek_p_snap(g_pcfn.uc, g_pcfn.p_guest, &g_pcfn.at_fault);
        emit_p_snap("FINALIZE", &g_pcfn.at_fault);
    }
    emit_scope_stack(g_pcfn.cont_r9);
    emit_promotion_candidate(g_pcfn.cont_r9);
    classify_and_summary(stop_reason ? stop_reason : "EXPLICIT_STOP");
}
