#include "gwy_launcher/ext_helper_handoff.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/ext_module_entry_abi.h"
#include "gwy_launcher/ext_object_observe.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define GWY_HANDOFF_RECORD_MAX 8
#define GWY_HANDOFF_NATURE_INSNS 40

typedef struct {
    int active;
    int exit_emitted;
    uint32_t helper;
    uint32_t p_len;
    uint32_t p_guest;
    uint32_t return_lr;
    char caller_module[96];
    uint32_t caller_offset;
    char module_name[96];
    char origin[24];
    int writes_count;
} CFunctionNewSession;

typedef struct {
    uint32_t helper_raw;
    uint32_t helper_norm;
    uint64_t module_id;
    char module_name[96];
    int registered;
    int handoff_none_logged;
    int nature_emitted;
    int case_emitted;
    ExtHandoffCase last_case;
    int saw_return_eq_helper;
    int saw_helper_write;
    int saw_helper_read;
    int saw_field_eq_helper;
    int saw_field_eq_direct;
    int saw_wrong_field;
    uint32_t last_direct;
    uint32_t last_record_base;
    uint32_t last_field_off;
    uint32_t last_field_val;
    char helper_nature[32];
    char direct_nature[32];
    UnknownDsmModuleRecord records[GWY_HANDOFF_RECORD_MAX];
    int record_count;
    CFunctionNewSession session;
    void *uc;
} HelperHandoffState;

static HelperHandoffState g_hh;

void ext_helper_handoff_reset(void) { memset(&g_hh, 0, sizeof(g_hh)); }

int ext_helper_handoff_enabled(void) {
    const char *env = getenv("GWY_HELPER_HANDOFF");
    return env && env[0] == '1';
}

void ext_helper_handoff_bind_uc(void *uc) { g_hh.uc = uc; }

const char *ext_handoff_case_name(ExtHandoffCase c) {
    switch (c) {
        case HANDOFF_CASE_RETURN_LOST: return "RETURN_LOST";
        case HANDOFF_CASE_FIELD_NOT_UPDATED: return "FIELD_NOT_UPDATED";
        case HANDOFF_CASE_WRONG_FIELD_READ: return "WRONG_FIELD_READ";
        case HANDOFF_CASE_WRONG_HELPER_IDENTITY: return "WRONG_HELPER_IDENTITY";
        case HANDOFF_CASE_MISSING_TRAMPOLINE: return "MISSING_TRAMPOLINE";
        case HANDOFF_CASE_CROSS_TARGET_DISPROVES: return "CROSS_TARGET_DISPROVES";
        default: return "UNKNOWN";
    }
}

ExtHandoffCase ext_helper_handoff_last_case(void) { return g_hh.last_case; }

static int value_is_helper(uint32_t v) {
    uint32_t n = v & ~1u;
    if (!g_hh.helper_norm && !g_hh.helper_raw) return 0;
    if (v == g_hh.helper_raw || n == g_hh.helper_norm) return 1;
    if (g_hh.helper_raw && (v == (g_hh.helper_raw | 1u) || n == (g_hh.helper_raw & ~1u))) return 1;
    return 0;
}

static const char *addr_class_short(uint32_t va) {
    ExtAddrClassResult r;
    ext_object_classify_address(va, 0, &r);
    switch (r.addr_class) {
        case ADDR_DSM_RW: return "DSM_RW";
        case ADDR_DSM_CODE: return "DSM_CODE";
        case ADDR_MODULE_RW: return "MODULE_TABLE";
        case ADDR_MODULE_CODE: return "MODULE_CODE";
        case ADDR_HEAP: return "HEAP";
        case ADDR_STACK: return "STACK";
        default: return "UNKNOWN";
    }
}

static void emit_handoff(const char *stage, uint32_t helper, uint32_t addr, const char *cls,
                         const char *mod, uint32_t off, int is_read) {
    if (strcmp(stage, "WRITE") == 0) g_hh.saw_helper_write = 1;
    if (strcmp(stage, "READ") == 0) g_hh.saw_helper_read = 1;
    if (strcmp(stage, "RETURN") == 0) g_hh.saw_return_eq_helper = 1;
    if (is_read)
        printf("[HELPER_HANDOFF] stage=%s helper=0x%X source=0x%X source_class=%s "
               "reader_module=%s offset=0x%X evidence=OBSERVED\n",
               stage, helper, addr, cls ? cls : "UNKNOWN", mod ? mod : "?", off);
    else if (strcmp(stage, "RETURN") == 0)
        printf("[HELPER_HANDOFF] stage=RETURN helper=0x%X consumer_module=%s evidence=OBSERVED\n",
               helper, mod ? mod : "?");
    else
        printf("[HELPER_HANDOFF] stage=%s helper=0x%X destination=0x%X destination_class=%s "
               "writer_module=%s writer_offset=0x%X evidence=OBSERVED\n",
               stage, helper, addr, cls ? cls : "UNKNOWN", mod ? mod : "?", off);
    fflush(stdout);
}

static void maybe_handoff_none(void) {
    if (g_hh.handoff_none_logged) return;
    if (g_hh.saw_helper_write || g_hh.saw_return_eq_helper || g_hh.saw_helper_read) return;
    if (!g_hh.registered && !g_hh.helper_raw) return;
    g_hh.handoff_none_logged = 1;
    printf("[HELPER_HANDOFF] NONE evidence=OBSERVED note=no_write_return_or_read_of_helper_seen\n");
    fflush(stdout);
}

static void analyze_entry_nature(void *uc, uint32_t addr, char *out, size_t out_cap) {
    int i, thumb, saw_cmp = 0, saw_ldr_r0 = 0;
    uint32_t pc;
    if (!out || !out_cap) return;
    snprintf(out, out_cap, "%s", "unknown");
    if (!uc || !addr) return;
    thumb = (addr & 1u) != 0;
    pc = addr & ~1u;
    for (i = 0; i < GWY_HANDOFF_NATURE_INSNS; i++) {
        uint32_t word = 0;
        int rt = -1, rn = -1, is_load = 0;
        uint32_t imm = 0;
        if (thumb) {
            uint16_t half;
            uint32_t a = pc + (uint32_t)(i * 2);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, a & ~3u, &word)) continue;
            half = (uint16_t)((a & 2u) ? (word >> 16) : (word & 0xFFFFu));
            if ((half & 0xFF00u) == 0x2800u) saw_cmp = 1;
            if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load && rn == 0)
                saw_ldr_r0 = 1;
        } else {
            uint32_t a = pc + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, a, &word)) continue;
            if ((word & 0x0FF0F000u) == 0x03500000u) saw_cmp = 1;
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rn == 0)
                saw_ldr_r0 = 1;
        }
    }
    if (saw_cmp) snprintf(out, out_cap, "%s", "command_dispatch");
    else if (saw_ldr_r0) snprintf(out, out_cap, "%s", "context_required");
}

static void emit_entry_natures(void *uc, uint32_t helper, uint32_t direct) {
    if (g_hh.nature_emitted || !helper || !direct) return;
    g_hh.nature_emitted = 1;
    analyze_entry_nature(uc, helper, g_hh.helper_nature, sizeof(g_hh.helper_nature));
    analyze_entry_nature(uc, direct, g_hh.direct_nature, sizeof(g_hh.direct_nature));
    printf("[ENTRY_NATURE] addr=0x%X role=registered_helper nature=%s evidence=OBSERVED\n",
           helper, g_hh.helper_nature);
    printf("[ENTRY_NATURE] addr=0x%X role=dsm_direct_target nature=%s evidence=OBSERVED\n",
           direct, g_hh.direct_nature);
    fflush(stdout);
}

static void emit_direct_provenance(uint32_t value, const char *first_seen, const char *producer,
                                   uint32_t producer_off, uint32_t dest, const char *source_kind) {
    printf("[DIRECT_TARGET_PROVENANCE] value=0x%X first_seen=%s producer_module=%s "
           "producer_offset=0x%X destination=0x%X source_kind=%s evidence=OBSERVED\n",
           value, first_seen ? first_seen : "UNKNOWN", producer ? producer : "?", producer_off,
           dest, source_kind ? source_kind : "UNKNOWN");
    fflush(stdout);
}

static void reverse_dsm_record_field(void *uc, uint32_t caller_pc, uint32_t target,
                                     uint32_t helper, const char *reader_mod) {
    int i, found = 0;
    uint32_t word = 0, imm = 0, record_base = 0, field_off = 0, field_val = 0, r9 = 0;
    int rt = -1, rn = -1, is_load = 0;
    ModuleRegistry *reg;
    const GwyLoadedModule *dsm;

    if (!uc || !caller_pc) return;
#ifdef GWY_HAVE_UNICORN
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &r9);
#endif
    for (i = -12; i <= 0; i++) {
        uint32_t addr = (caller_pc & ~1u) + (uint32_t)(i * 4);
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
        if (!ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) || !is_load || imm > 0x40u)
            continue;
        {
            uint32_t base = 0, slot = 0;
#ifdef GWY_HAVE_UNICORN
            if (rn >= 0 && rn < 13)
                uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0 + rn, &base);
            else if (rn == 13)
                uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &base);
            else if (rn == 15)
                base = addr + 8u;
#endif
            if (rn == 15 && r9) {
                if (guest_memory_uc_peek_u32((struct uc_struct *)uc, addr + 8u + imm, &slot)) {
                    record_base = slot + r9;
                    guest_memory_uc_peek_u32((struct uc_struct *)uc, record_base + 4u, &field_val);
                    if ((field_val & ~1u) == (target & ~1u) || field_val == target) {
                        field_off = 4u;
                        found = 1;
                        break;
                    }
                    guest_memory_uc_peek_u32((struct uc_struct *)uc, record_base, &field_val);
                    if ((field_val & ~1u) == (target & ~1u)) {
                        field_off = 0;
                        found = 1;
                        break;
                    }
                }
            } else if (base) {
                record_base = base;
                field_off = imm;
                guest_memory_uc_peek_u32((struct uc_struct *)uc, base + imm, &field_val);
                found = 1;
                if ((field_val & ~1u) == (target & ~1u)) break;
            }
        }
    }
    if (!found) {
        printf("[DSM_RECORD_FIELD] record_base=0x0 field_offset=0x0 current_value=0x%X "
               "equals_helper=%d equals_direct=1 creator_module=unknown evidence=OBSERVED "
               "note=reverse_miss\n",
               target, helper && ((helper & ~1u) == (target & ~1u)) ? 1 : 0);
        fflush(stdout);
        return;
    }
    g_hh.last_record_base = record_base;
    g_hh.last_field_off = field_off;
    g_hh.last_field_val = field_val;
    g_hh.saw_field_eq_direct =
        ((field_val & ~1u) == (target & ~1u) || field_val == target) ? 1 : 0;
    g_hh.saw_field_eq_helper =
        (helper && ((field_val & ~1u) == (helper & ~1u) || field_val == helper)) ? 1 : 0;
    if (helper && record_base) {
        uint32_t off;
        for (off = 0; off <= 0x40u; off += 4u) {
            uint32_t v = 0;
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, record_base + off, &v)) continue;
            if (value_is_helper(v) && off != field_off) {
                g_hh.saw_wrong_field = 1;
                emit_handoff("READ", helper, record_base + off, "DSM_RW", reader_mod, off, 1);
            }
        }
    }
    if (g_hh.record_count < GWY_HANDOFF_RECORD_MAX) {
        UnknownDsmModuleRecord *rec = &g_hh.records[g_hh.record_count++];
        memset(rec, 0, sizeof(*rec));
        rec->guest_base = record_base;
        rec->size = 0x44u;
        rec->dispatch_target = field_val;
        rec->registered_helper_candidate = helper;
    }
    reg = gwy_ext_loader_bound_registry();
    dsm = reg ? module_registry_find(reg, "dsm:cfunction.ext") : NULL;
    printf("[DSM_RECORD_FIELD] record_base=0x%X field_offset=0x%X current_value=0x%X "
           "equals_helper=%d equals_direct=%d creator_module=%s evidence=OBSERVED\n",
           record_base, field_off, field_val, g_hh.saw_field_eq_helper, g_hh.saw_field_eq_direct,
           dsm ? "dsm:cfunction.ext" : (reader_mod ? reader_mod : "unknown"));
    fflush(stdout);
    if (value_is_helper(field_val))
        emit_handoff("READ", helper ? helper : field_val, record_base + field_off, "DSM_RW",
                     reader_mod, field_off, 1);
}

static void classify_and_emit_case(void) {
    ExtHandoffCase c = HANDOFF_CASE_UNKNOWN;
    const char *status = "ACTIVE";
    const char *causal = "YES";
    if (g_hh.case_emitted) return;
    if (g_hh.saw_wrong_field)
        c = HANDOFF_CASE_WRONG_FIELD_READ;
    else if (g_hh.saw_return_eq_helper && !g_hh.saw_helper_write && !g_hh.saw_field_eq_helper)
        c = HANDOFF_CASE_RETURN_LOST;
    else if (strcmp(g_hh.helper_nature, "context_required") == 0 &&
             strcmp(g_hh.direct_nature, "command_dispatch") == 0)
        c = HANDOFF_CASE_WRONG_HELPER_IDENTITY;
    else if (g_hh.registered && g_hh.last_direct &&
             (g_hh.last_direct & ~1u) != g_hh.helper_norm && !g_hh.saw_field_eq_helper)
        c = HANDOFF_CASE_FIELD_NOT_UPDATED;
    /* A8: FIELD_NOT_UPDATED is expected by _mr_c_function_new contract — not causal. */
    if (c == HANDOFF_CASE_FIELD_NOT_UPDATED) {
        status = "EXPECTED_BY_CONTRACT";
        causal = "NO";
    }
    g_hh.last_case = c;
    g_hh.case_emitted = 1;
    maybe_handoff_none();
    printf("[HANDOFF_CASE] case=%s status=%s causal=%s "
           "legend=RETURN_LOST|FIELD_NOT_UPDATED|WRONG_FIELD_READ|WRONG_HELPER_IDENTITY|"
           "MISSING_TRAMPOLINE|CROSS_TARGET_DISPROVES|UNKNOWN "
           "helper=0x%X direct=0x%X helper_nature=%s direct_nature=%s "
           "saw_write=%d saw_return=%d field_eq_helper=%d phase6b_b_gate=blocked\n",
           ext_handoff_case_name(c), status, causal, g_hh.helper_raw, g_hh.last_direct,
           g_hh.helper_nature, g_hh.direct_nature, g_hh.saw_helper_write,
           g_hh.saw_return_eq_helper, g_hh.saw_field_eq_helper);
    fflush(stdout);
    if (c == HANDOFF_CASE_FIELD_NOT_UPDATED && g_hh.helper_raw && g_hh.last_direct &&
        (g_hh.last_direct & ~1u) != g_hh.helper_norm) {
        printf("[CHAIN_SEPARATION] registered_helper_ne_module_entry=EXPECTED "
               "helper=0x%X module_entry=0x%X evidence=DOCUMENTED "
               "note=helper_is_mr_helper_path_entry_is_dsm_code_image\n",
               g_hh.helper_raw, g_hh.last_direct);
        printf("[MODULE_ENTRY_BLOCKER] blocker=MODULE_ENTRY_ABI_UNKNOWN "
               "prior_case=FIELD_NOT_UPDATED status=EXPECTED_BY_CONTRACT causal=NO "
               "phase6b_b_gate=blocked evidence=OBSERVED\n");
        fflush(stdout);
    }
}

void ext_helper_handoff_cfunction_enter(const char *origin, uint32_t helper, uint32_t p_len,
                                        uint32_t p_guest, const uint32_t *regs, uint32_t cpsr,
                                        uint32_t caller_pc, const char *caller_module,
                                        uint32_t caller_offset, const char *module_name) {
    CFunctionNewSession *s;
    uint32_t r0, r1, r2, r3, sp, lr;
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    if (!ext_helper_handoff_enabled() || !helper) return;
    s = &g_hh.session;
    if (s->active && !s->exit_emitted) {
        printf("[CFUNCTION_NEW] stage=EXIT return_r0=0x0 return_r1=0x0 return_r2=0x0 "
               "return_r3=0x0 registered_helper=0x%X helper_eq_return_r0=0 writes_count=%d "
               "note=preempted\n",
               s->helper, s->writes_count);
        fflush(stdout);
        s->active = 0;
    }
    memset(s, 0, sizeof(*s));
    s->active = 1;
    s->helper = helper;
    s->p_len = p_len;
    s->p_guest = p_guest;
    snprintf(s->origin, sizeof(s->origin), "%s", origin ? origin : "UNKNOWN");
    snprintf(s->caller_module, sizeof(s->caller_module), "%s",
             caller_module ? caller_module : "unknown");
    s->caller_offset = caller_offset;
    snprintf(s->module_name, sizeof(s->module_name), "%s", module_name ? module_name : "unknown");
    r0 = regs ? regs[0] : helper;
    r1 = regs ? regs[1] : p_len;
    r2 = regs ? regs[2] : 0;
    r3 = regs ? regs[3] : 0;
    sp = regs ? regs[13] : 0;
    lr = regs ? regs[14] : 0;
    s->return_lr = lr;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_code_addr(reg, helper & ~1u) : NULL;
    if (m) {
        snprintf(s->module_name, sizeof(s->module_name), "%s",
                 m->resolved_name[0] ? m->resolved_name : m->requested_name);
        if (!caller_module || !caller_module[0]) {
            const GwyLoadedModule *cm =
                (lr && reg) ? module_registry_find_by_code_addr(reg, lr & ~1u) : NULL;
            if (cm) {
                if (cm->origin == MODULE_ORIGIN_DSM)
                    snprintf(s->caller_module, sizeof(s->caller_module), "%s", "dsm:cfunction.ext");
                else
                    snprintf(s->caller_module, sizeof(s->caller_module), "%s",
                             cm->resolved_name[0] ? cm->resolved_name : cm->requested_name);
                if (cm->map.guest_code_base)
                    s->caller_offset = (lr & ~1u) - cm->map.guest_code_base;
            }
        }
    }
    g_hh.helper_raw = helper;
    g_hh.helper_norm = helper & ~1u;
    printf("[CFUNCTION_NEW] stage=ENTER caller_module=%s caller_offset=0x%X module=%s "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X p_len=%u p_guest=0x%X sp=0x%X lr=0x%X "
           "cpsr=0x%X origin=%s evidence=OBSERVED\n",
           s->caller_module, s->caller_offset, s->module_name, r0, r1, r2, r3, p_len, p_guest, sp,
           lr, cpsr, s->origin);
    fflush(stdout);
    if (p_guest) {
        printf("[CFUNCTION_NEW] note=p_struct_guest=0x%X len=%u helper_is_input_r0=1\n", p_guest,
               p_len);
        fflush(stdout);
    }
    (void)caller_pc;
}

void ext_helper_handoff_note_registered(uint64_t module_id, uint32_t helper,
                                        const char *module_name) {
    CFunctionNewSession *s;
    if (!ext_helper_handoff_enabled() || !helper) return;
    g_hh.helper_raw = helper;
    g_hh.helper_norm = helper & ~1u;
    g_hh.module_id = module_id;
    g_hh.registered = 1;
    if (module_name) snprintf(g_hh.module_name, sizeof(g_hh.module_name), "%s", module_name);
    s = &g_hh.session;
    if (s->active && !s->exit_emitted &&
        (strcmp(s->origin, "HOST_BRIDGE") == 0 || strcmp(s->origin, "LOG_PARSE") == 0 ||
         strcmp(s->origin, "GUEST_NESTED") == 0)) {
        s->exit_emitted = 1;
        s->active = 0;
        printf("[CFUNCTION_NEW] stage=EXIT return_r0=0x0 return_r1=0x0 return_r2=0x0 "
               "return_r3=0x0 registered_helper=0x%X helper_eq_return_r0=0 writes_count=%d "
               "note=api_returns_status_helper_is_input evidence=OBSERVED\n",
               helper, s->writes_count);
        fflush(stdout);
        printf("[HELPER_HANDOFF] stage=RETURN helper=0x%X consumer_module=%s "
               "note=helper_is_register_input_not_return_r0 evidence=OBSERVED\n",
               helper, s->caller_module);
        fflush(stdout);
    }
}

static void emit_cfunction_exit_from_regs(const uint32_t regs[16]) {
    CFunctionNewSession *s = &g_hh.session;
    int eq;
    if (!s->active || s->exit_emitted || !regs) return;
    s->exit_emitted = 1;
    s->active = 0;
    eq = value_is_helper(regs[0]) ? 1 : 0;
    if (eq) g_hh.saw_return_eq_helper = 1;
    printf("[CFUNCTION_NEW] stage=EXIT return_r0=0x%X return_r1=0x%X return_r2=0x%X "
           "return_r3=0x%X registered_helper=0x%X helper_eq_return_r0=%d writes_count=%d "
           "evidence=OBSERVED\n",
           regs[0], regs[1], regs[2], regs[3], s->helper ? s->helper : g_hh.helper_raw, eq,
           s->writes_count);
    fflush(stdout);
    if (eq)
        emit_handoff("RETURN", s->helper ? s->helper : g_hh.helper_raw, 0, "UNKNOWN",
                     s->caller_module, 0, 0);
}

void ext_helper_handoff_on_store(uint32_t value, uint32_t dest, uint64_t writer_mid,
                                 uint32_t writer_pc, const char *writer_name, uint32_t writer_off) {
    (void)writer_name;
    (void)writer_off;
    ext_module_entry_abi_on_store(value, dest, writer_mid, writer_pc);
    if (!ext_helper_handoff_enabled() || !value_is_helper(value)) return;
    if (g_hh.session.active) g_hh.session.writes_count++;
    emit_handoff("WRITE", g_hh.helper_raw ? g_hh.helper_raw : value, dest, addr_class_short(dest),
                 writer_name, writer_off, 0);
}

void ext_helper_handoff_on_block_copy(uint32_t dst, uint32_t src, uint32_t len) {
    uint32_t off;
    if (!ext_helper_handoff_enabled() || !g_hh.helper_norm || !dst || !src || !len || len > 0x1000u)
        return;
    if (!g_hh.uc) return;
    for (off = 0; off + 4u <= len; off += 4u) {
        uint32_t v = 0;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)g_hh.uc, src + off, &v)) continue;
        if (value_is_helper(v)) {
            ext_helper_handoff_on_store(v, dst + off, 0, 0, "block_copy", off);
            emit_direct_provenance(v, "FIELD_WRITE", "block_copy", off, dst + off, "REGISTER_API");
        }
    }
}

void ext_helper_handoff_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                uint32_t cpsr) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    CFunctionNewSession *s;
    int thumb, rt = -1, rn = -1, is_load = 0;
    uint32_t word = 0, imm = 0;
    if (!ext_helper_handoff_enabled() || !regs) return;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    s = &g_hh.session;
    if (s->active && !s->exit_emitted && s->return_lr && (pc & ~1u) == (s->return_lr & ~1u))
        emit_cfunction_exit_from_regs(regs);
    thumb = (cpsr & (1u << 5)) != 0;
    if (thumb) {
        uint16_t half;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) return;
        half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
        if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && !is_load && rt >= 0 &&
            rt < 16 && rn >= 0 && rn < 16 && value_is_helper(regs[rt])) {
            uint32_t dest = regs[rn] + imm, off = 0;
            const char *wn = "unknown";
            if (m) {
                wn = m->origin == MODULE_ORIGIN_DSM
                         ? "dsm:cfunction.ext"
                         : (m->resolved_name[0] ? m->resolved_name : m->requested_name);
                if (m->map.guest_code_base && (pc & ~1u) >= m->map.guest_code_base)
                    off = (pc & ~1u) - m->map.guest_code_base;
            }
            ext_helper_handoff_on_store(regs[rt], dest, module_id, pc, wn, off);
        }
    } else if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &word)) {
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && !is_load && rt >= 0 &&
            rt < 16 && rn >= 0 && rn < 16 && value_is_helper(regs[rt])) {
            uint32_t dest = regs[rn] + imm, off = 0;
            const char *wn = "unknown";
            if (m) {
                wn = m->origin == MODULE_ORIGIN_DSM
                         ? "dsm:cfunction.ext"
                         : (m->resolved_name[0] ? m->resolved_name : m->requested_name);
                if (m->map.guest_code_base && (pc & ~1u) >= m->map.guest_code_base)
                    off = (pc & ~1u) - m->map.guest_code_base;
            }
            ext_helper_handoff_on_store(regs[rt], dest, module_id, pc, wn, off);
        }
    }
}

void ext_helper_handoff_on_second_hop(void *uc, uint32_t caller_pc, uint32_t target, uint32_t r0,
                                      uint32_t r1, uint64_t from_mid, uint64_t to_mid) {
    ModuleRegistry *reg;
    const GwyLoadedModule *from_m, *to_m;
    uint32_t helper = 0;
    const char *reader = "dsm:cfunction.ext";
    const char *first_seen = "UNKNOWN", *source_kind = "UNKNOWN";
    if (!ext_helper_handoff_enabled()) return;
    (void)r0;
    (void)r1;
    reg = gwy_ext_loader_bound_registry();
    from_m = reg ? module_registry_find_by_id(reg, from_mid) : NULL;
    to_m = reg ? module_registry_find_by_id(reg, to_mid) : NULL;
    if (to_m) helper = to_m->entries.registered_helper;
    if (!helper) helper = g_hh.helper_raw;
    if (helper) {
        g_hh.helper_raw = helper;
        g_hh.helper_norm = helper & ~1u;
        g_hh.registered = 1;
    }
    g_hh.last_direct = target;
    reverse_dsm_record_field(uc ? uc : g_hh.uc, caller_pc, target, helper, reader);
    if (to_m) {
        uint32_t hdr = to_m->entries.header_entry_candidate;
        uint32_t first = to_m->entries.observed_first_pc;
        if (hdr && (hdr & ~1u) == (target & ~1u)) {
            first_seen = "HEADER";
            source_kind = "HEADER";
        } else if (first && (first & ~1u) == (target & ~1u)) {
            first_seen = "CODE_IMAGE";
            source_kind = "LOAD_RETURN";
        } else if (helper && (helper & ~1u) == (target & ~1u)) {
            first_seen = "REGISTER_API";
            source_kind = "REGISTER_API";
        } else {
            first_seen = "FIELD_WRITE";
            source_kind = "UNKNOWN";
        }
    }
    emit_direct_provenance(target, first_seen, reader,
                           from_m && from_m->map.guest_code_base
                               ? ((caller_pc & ~1u) - from_m->map.guest_code_base)
                               : 0,
                           g_hh.last_record_base + g_hh.last_field_off, source_kind);

    /* Defer ENTRY_NATURE / HANDOFF_CASE until callee has its own registered_helper
     * (avoid pairing mrc_loader helper with an early robotol trampoline hop). */
    {
        int helper_in_callee = 0;
        if (to_m && helper && to_m->map.guest_code_base && to_m->map.guest_code_size) {
            uint32_t hn = helper & ~1u;
            if (hn >= to_m->map.guest_code_base &&
                hn < to_m->map.guest_code_base + to_m->map.guest_code_size)
                helper_in_callee = 1;
        }
        if (helper_in_callee) {
            if (g_hh.nature_emitted &&
                (g_hh.helper_norm != (helper & ~1u) || g_hh.last_direct != target)) {
                g_hh.nature_emitted = 0;
                g_hh.case_emitted = 0;
                g_hh.handoff_none_logged = 0;
            }
            g_hh.helper_raw = helper;
            g_hh.helper_norm = helper & ~1u;
            emit_entry_natures(uc ? uc : g_hh.uc, helper, target);
            classify_and_emit_case();
        }
    }
}
