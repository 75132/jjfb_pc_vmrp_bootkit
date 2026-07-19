#include "gwy_launcher/ext_dsm_record_observe.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/ext_object_observe.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define GWY_DSM_REC_MAX 8
#define GWY_DSM_WRITE_RING 64
#define GWY_DSM_REC_HEADER 0x44u

typedef struct {
    uint64_t record_id;
    uint32_t field_offset;
    uint32_t old_value;
    uint32_t new_value;
    char writer_module[96];
    uint32_t writer_offset;
    char stage[24];
} DsmRecordWriteEvt;

typedef struct {
    DsmModuleRecord records[GWY_DSM_REC_MAX];
    int record_count;
    uint64_t next_record_id;
    DsmRecordWriteEvt writes[GWY_DSM_WRITE_RING];
    int write_count;
    uint32_t helper_raw;
    uint32_t helper_norm;
    uint64_t helper_module_id;
    int registered_seen;
    int finalize_seen;
    int finalize_wrote_dispatch;
    int helper_in_any_record_field;
    int post_register_window;
    ExtContractCase last_case;
    int case_emitted;
    int documented_side_logged;
    ExtEvidenceLevel best_evidence;
    int xt_helper_is_dispatch;
    int phase6b_b_open;
    void *uc;
} DsmRecordObserveState;

static DsmRecordObserveState g_dr;

void ext_dsm_record_observe_reset(void) { memset(&g_dr, 0, sizeof(g_dr)); }
void ext_dsm_record_observe_bind_uc(void *uc) { g_dr.uc = uc; }

int ext_dsm_record_contract_enabled(void) {
    const char *env = getenv("GWY_DSM_RECORD_CONTRACT");
    return env && env[0] == '1';
}

const char *ext_contract_case_name(ExtContractCase c) {
    switch (c) {
        case CONTRACT_CASE_REGISTER_FINALIZE_MISSING: return "REGISTER_FINALIZE_MISSING";
        case CONTRACT_CASE_WRONG_RECORD_ASSOCIATION: return "WRONG_RECORD_ASSOCIATION";
        case CONTRACT_CASE_WRONG_FIELD_READ: return "WRONG_FIELD_READ";
        case CONTRACT_CASE_HELPER_ADDRESS_ENCODING: return "HELPER_ADDRESS_ENCODING";
        case CONTRACT_CASE_HELPER_NOT_DISPATCH_TARGET: return "HELPER_NOT_DISPATCH_TARGET";
        case CONTRACT_CASE_GENERIC_HANDOFF_MISSING: return "GENERIC_HANDOFF_MISSING";
        default: return "UNKNOWN";
    }
}

const char *ext_evidence_level_name(ExtEvidenceLevel e) {
    switch (e) {
        case EV_DOCUMENTED: return "DOCUMENTED";
        case EV_CROSS_TARGET: return "CROSS_TARGET";
        case EV_OBSERVED: return "OBSERVED";
        case EV_INFERRED: return "INFERRED";
        default: return "UNKNOWN";
    }
}

ExtContractCase ext_dsm_record_last_case(void) { return g_dr.last_case; }
int ext_dsm_record_phase6b_b_open(void) { return g_dr.phase6b_b_open; }

static int value_is_helper(uint32_t v) {
    uint32_t n = v & ~1u;
    if (!g_dr.helper_norm) return 0;
    return v == g_dr.helper_raw || n == g_dr.helper_norm || v == (g_dr.helper_raw | 1u);
}

static DsmModuleRecord *find_record_by_base(uint32_t base) {
    int i;
    uint32_t b = base & ~3u;
    for (i = 0; i < g_dr.record_count; i++) {
        if (g_dr.records[i].valid && (g_dr.records[i].guest_base & ~3u) == b)
            return &g_dr.records[i];
    }
    return NULL;
}

static DsmModuleRecord *find_record_covering(uint32_t addr) {
    int i;
    for (i = 0; i < g_dr.record_count; i++) {
        DsmModuleRecord *r = &g_dr.records[i];
        if (!r->valid || !r->guest_base) continue;
        if (addr >= r->guest_base && addr < r->guest_base + r->size) return r;
    }
    return NULL;
}

static DsmModuleRecord *ensure_record(uint32_t base, uint32_t size) {
    DsmModuleRecord *r;
    if (!base) return NULL;
    r = find_record_by_base(base);
    if (r) {
        if (size > r->size) r->size = size;
        return r;
    }
    if (g_dr.record_count >= GWY_DSM_REC_MAX) return NULL;
    r = &g_dr.records[g_dr.record_count++];
    memset(r, 0, sizeof(*r));
    r->valid = 1;
    r->record_id = ++g_dr.next_record_id;
    r->guest_base = base;
    r->size = size ? size : GWY_DSM_REC_HEADER;
    r->code_image_field_offset = 0xFFFFFFFFu;
    r->dispatch_field_offset = 0xFFFFFFFFu;
    r->helper_field_offset = 0xFFFFFFFFu;
    r->status_field_offset = 0xFFFFFFFFu;
    r->confidence = EXT_IDENTITY_CANDIDATE;
    printf("[DSM_MODULE_RECORD] record_id=%llu guest_base=0x%X size=%u stage=ALLOC "
           "confidence=CANDIDATE evidence=OBSERVED\n",
           (unsigned long long)r->record_id, base, r->size);
    fflush(stdout);
    return r;
}

static void snap_fields(DsmModuleRecord *r) {
    uint32_t off;
    if (!r || !g_dr.uc || !r->guest_base) return;
    for (off = 0; off < GWY_DSM_REC_HEADER && (off / 4u) < GWY_DSM_RECORD_FIELD_SLOTS; off += 4u) {
        uint32_t v = 0;
        guest_memory_uc_peek_u32((struct uc_struct *)g_dr.uc, r->guest_base + off, &v);
        r->field_snap[off / 4u] = v;
    }
}

static void log_record_write(DsmModuleRecord *r, uint32_t field_off, uint32_t old_v, uint32_t new_v,
                             const char *writer, uint32_t writer_off, const char *stage) {
    DsmRecordWriteEvt *e;
    int idx;
    if (!r) return;
    idx = g_dr.write_count % GWY_DSM_WRITE_RING;
    e = &g_dr.writes[idx];
    memset(e, 0, sizeof(*e));
    e->record_id = r->record_id;
    e->field_offset = field_off;
    e->old_value = old_v;
    e->new_value = new_v;
    snprintf(e->writer_module, sizeof(e->writer_module), "%s", writer ? writer : "?");
    e->writer_offset = writer_off;
    snprintf(e->stage, sizeof(e->stage), "%s", stage ? stage : "UNKNOWN");
    g_dr.write_count++;
    printf("[DSM_RECORD_WRITE] record_id=%llu field_offset=0x%X field_name=field_%02X "
           "old_value=0x%X new_value=0x%X writer_module=%s writer_offset=0x%X stage=%s "
           "evidence=OBSERVED\n",
           (unsigned long long)r->record_id, field_off, field_off & 0xFFu, old_v, new_v,
           e->writer_module, writer_off, e->stage);
    fflush(stdout);
    if (value_is_helper(new_v)) {
        g_dr.helper_in_any_record_field = 1;
        if (r->helper_field_offset == 0xFFFFFFFFu) r->helper_field_offset = field_off;
        if (g_dr.post_register_window && r->dispatch_field_offset != 0xFFFFFFFFu &&
            field_off == r->dispatch_field_offset) {
            g_dr.finalize_wrote_dispatch = 1;
            g_dr.finalize_seen = 1;
            printf("[REGISTER_FINALIZE] module_id=%llu helper=0x%X finalizer_module=%s "
                   "finalizer_offset=0x%X record_field_written=0x%X evidence=OBSERVED\n",
                   (unsigned long long)g_dr.helper_module_id, g_dr.helper_raw, e->writer_module,
                   writer_off, field_off);
            fflush(stdout);
        }
    }
}

static const char *classify_write_stage(void) {
    if (g_dr.post_register_window) return "POST_REGISTER";
    if (g_dr.registered_seen) return "REGISTER";
    return "LOAD_CODE";
}
void ext_dsm_record_note_cfunction_side_effects(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                                const char *origin) {
    if (!ext_dsm_record_contract_enabled()) return;
    if (!helper) return;
    g_dr.helper_raw = helper;
    g_dr.helper_norm = helper & ~1u;
    /*
     * DOCUMENTED (mythroad.c _mr_c_function_new):
     *   mr_c_function = f;                 // host/platform global helper
     *   *((void**)(mr_load_c_function)-1) = mr_c_function_P;  // or fix_p+1
     * Does NOT write f into a guest DSM module-record dispatch field.
     */
    if (!g_dr.documented_side_logged) {
        g_dr.documented_side_logged = 1;
        g_dr.best_evidence = EV_DOCUMENTED;
    }
    printf("[CFUNCTION_NEW_SIDE_EFFECT] kind=HOST_GLOBAL_HELPER helper=0x%X origin=%s "
           "evidence=DOCUMENTED source=mythroad.c:_mr_c_function_new\n",
           helper, origin ? origin : "?");
    printf("[CFUNCTION_NEW_SIDE_EFFECT] kind=GUEST_P_STRUCT p_guest=0x%X p_len=%u "
           "note=P_not_helper evidence=DOCUMENTED source=mythroad.c:_mr_c_function_new\n",
           p_guest, p_len);
    printf("[CFUNCTION_NEW_SIDE_EFFECT] kind=NO_DSM_DISPATCH_WRITE "
           "note=documented_api_does_not_update_module_record_dispatch evidence=DOCUMENTED\n");
    fflush(stdout);
}

void ext_dsm_record_note_registered(uint64_t module_id, uint32_t helper) {
    if (!ext_dsm_record_contract_enabled() || !helper) return;
    g_dr.helper_raw = helper;
    g_dr.helper_norm = helper & ~1u;
    g_dr.helper_module_id = module_id;
    g_dr.registered_seen = 1;
    g_dr.post_register_window = 1;
    printf("[DSM_RECORD_CONTRACT] stage=REGISTERED module_id=%llu helper=0x%X "
           "post_register_window=1 evidence=OBSERVED\n",
           (unsigned long long)module_id, helper);
    fflush(stdout);
}

void ext_dsm_record_note_post_register(uint64_t module_id, uint32_t helper) {
    if (!ext_dsm_record_contract_enabled()) return;
    g_dr.post_register_window = 1;
    if (helper) {
        g_dr.helper_raw = helper;
        g_dr.helper_norm = helper & ~1u;
    }
    g_dr.helper_module_id = module_id;
    printf("[REGISTER_FINALIZE] module_id=%llu helper=0x%X finalizer_module=pending "
           "finalizer_offset=0x0 record_field_written=none note=window_armed evidence=OBSERVED\n",
           (unsigned long long)module_id, helper);
    fflush(stdout);
}

void ext_dsm_record_on_store(uint32_t value, uint32_t dest, uint64_t writer_mid, uint32_t writer_pc,
                             const char *writer_name, uint32_t writer_off) {
    DsmModuleRecord *r;
    uint32_t field_off;
    uint32_t old_v = 0;
    (void)writer_mid;
    (void)writer_pc;
    if (!ext_dsm_record_contract_enabled()) return;
    r = find_record_covering(dest);
    if (!r) {
        /* Late bind: if storing code pointer into heap, candidate record. */
        if (value > 0x10000u && (dest & 3u) == 0 && dest > 0x10000u) {
            ExtAddrClassResult cls;
            ext_object_classify_address(dest, 0, &cls);
            if (cls.addr_class == ADDR_HEAP || cls.addr_class == ADDR_DSM_RW) {
                r = ensure_record(dest & ~0x3Fu, GWY_DSM_REC_HEADER);
                if (r) snap_fields(r);
            }
        }
    }
    if (!r) return;
    field_off = dest - r->guest_base;
    if (field_off >= r->size) return;
    if (g_dr.uc)
        guest_memory_uc_peek_u32((struct uc_struct *)g_dr.uc, dest, &old_v);
    /* old_v peek races with the store; prefer snap when available */
    if ((field_off / 4u) < GWY_DSM_RECORD_FIELD_SLOTS)
        old_v = r->field_snap[field_off / 4u];
    log_record_write(r, field_off, old_v, value, writer_name, writer_off, classify_write_stage());
    if ((field_off / 4u) < GWY_DSM_RECORD_FIELD_SLOTS)
        r->field_snap[field_off / 4u] = value;
    if (r->code_image_field_offset == 0xFFFFFFFFu && value > 0x10000u &&
        !value_is_helper(value))
        r->code_image_field_offset = field_off;
}

void ext_dsm_record_on_block_copy(uint32_t dst, uint32_t src, uint32_t len) {
    uint32_t off;
    if (!ext_dsm_record_contract_enabled() || !g_dr.uc || !dst || !src || !len || len > 0x200u)
        return;
    for (off = 0; off + 4u <= len; off += 4u) {
        uint32_t v = 0;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)g_dr.uc, src + off, &v)) continue;
        if (v > 0x10000u)
            ext_dsm_record_on_store(v, dst + off, 0, 0, "block_copy", off);
    }
}

void ext_dsm_record_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                            uint32_t cpsr) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    int thumb, rt = -1, rn = -1, is_load = 0;
    uint32_t word = 0, imm = 0;
    if (!ext_dsm_record_contract_enabled() || !regs) return;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    thumb = (cpsr & (1u << 5)) != 0;
    if (thumb) {
        uint16_t half;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) return;
        half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
        if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && !is_load && rt >= 0 &&
            rt < 16 && rn >= 0 && rn < 16) {
            uint32_t dest = regs[rn] + imm, off = 0;
            const char *wn = "unknown";
            if (m) {
                wn = m->origin == MODULE_ORIGIN_DSM
                         ? "dsm:cfunction.ext"
                         : (m->resolved_name[0] ? m->resolved_name : m->requested_name);
                if (m->map.guest_code_base && (pc & ~1u) >= m->map.guest_code_base)
                    off = (pc & ~1u) - m->map.guest_code_base;
            }
            ext_dsm_record_on_store(regs[rt], dest, module_id, pc, wn, off);
        }
    } else if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &word)) {
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && !is_load && rt >= 0 &&
            rt < 16 && rn >= 0 && rn < 16) {
            uint32_t dest = regs[rn] + imm, off = 0;
            const char *wn = "unknown";
            if (m) {
                wn = m->origin == MODULE_ORIGIN_DSM
                         ? "dsm:cfunction.ext"
                         : (m->resolved_name[0] ? m->resolved_name : m->requested_name);
                if (m->map.guest_code_base && (pc & ~1u) >= m->map.guest_code_base)
                    off = (pc & ~1u) - m->map.guest_code_base;
            }
            ext_dsm_record_on_store(regs[rt], dest, module_id, pc, wn, off);
        }
    }
}
static void reverse_bind_dispatch_field(void *uc, uint32_t caller_pc, uint32_t target,
                                        uint64_t to_mid, DsmModuleRecord **out_rec) {
    int i, found = 0;
    uint32_t word = 0, imm = 0, record_base = 0, field_off = 0, field_val = 0, r9 = 0;
    int rt = -1, rn = -1, is_load = 0;
    DsmModuleRecord *r;
    if (out_rec) *out_rec = NULL;
    if (!uc || !caller_pc) return;
#ifdef GWY_HAVE_UNICORN
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &r9);
#endif
    for (i = -12; i <= 0; i++) {
        uint32_t addr = (caller_pc & ~1u) + (uint32_t)(i * 4);
        uint32_t base = 0, slot = 0;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
        if (!ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) || !is_load || imm > 0x40u)
            continue;
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
                if ((field_val & ~1u) == (target & ~1u)) {
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
    if (!found) {
        printf("[DSM_MODULE_RECORD] stage=REVERSE_MISS target=0x%X evidence=OBSERVED\n", target);
        fflush(stdout);
        return;
    }
    r = ensure_record(record_base, GWY_DSM_REC_HEADER);
    if (!r) return;
    r->module_id = to_mid;
    r->dispatch_field_offset = field_off;
    r->confidence = EXT_IDENTITY_PROBABLE;
    snap_fields(r);
    /* Scan for helper elsewhere in record. */
    {
        uint32_t off;
        for (off = 0; off <= 0x40u; off += 4u) {
            uint32_t v = 0;
            guest_memory_uc_peek_u32((struct uc_struct *)uc, record_base + off, &v);
            if (value_is_helper(v)) {
                g_dr.helper_in_any_record_field = 1;
                r->helper_field_offset = off;
            }
            if (v && (v & ~1u) == (target & ~1u) && off != field_off)
                r->code_image_field_offset = off;
        }
    }
    if (r->code_image_field_offset == 0xFFFFFFFFu)
        r->code_image_field_offset = field_off; /* dispatch currently holds load-time entry */
    printf("[DSM_MODULE_RECORD] record_id=%llu guest_base=0x%X module_id=%llu "
           "dispatch_field_offset=0x%X code_image_field_offset=0x%X helper_field_offset=0x%X "
           "current_dispatch=0x%X equals_helper=%d confidence=PROBABLE evidence=OBSERVED\n",
           (unsigned long long)r->record_id, r->guest_base, (unsigned long long)r->module_id,
           r->dispatch_field_offset, r->code_image_field_offset,
           r->helper_field_offset == 0xFFFFFFFFu ? 0 : r->helper_field_offset, field_val,
           value_is_helper(field_val) ? 1 : 0);
    fflush(stdout);
    if (out_rec) *out_rec = r;
}

static void emit_write_history_summary(DsmModuleRecord *r) {
    int i, n, matched = 0;
    if (!r) return;
    n = g_dr.write_count;
    if (n > GWY_DSM_WRITE_RING) n = GWY_DSM_WRITE_RING;
    for (i = 0; i < n; i++) {
        if (g_dr.writes[i].record_id == r->record_id &&
            g_dr.writes[i].field_offset == r->dispatch_field_offset)
            matched++;
    }
    printf("[DSM_RECORD_WRITE_SUMMARY] record_id=%llu dispatch_field=0x%X write_events=%d "
           "helper_ever_written_to_record=%d finalize_wrote_dispatch=%d evidence=OBSERVED\n",
           (unsigned long long)r->record_id, r->dispatch_field_offset, matched,
           g_dr.helper_in_any_record_field, g_dr.finalize_wrote_dispatch);
    fflush(stdout);
}

static void classify_contract(DsmModuleRecord *r, uint32_t target) {
    ExtContractCase c = CONTRACT_CASE_UNKNOWN;
    int open = 0;
    ExtEvidenceLevel ev = g_dr.best_evidence;
    if (g_dr.case_emitted) return;

    /* DOCUMENTED: _mr_c_function_new does not update DSM dispatch fields. */
    if (ev >= EV_DOCUMENTED && !g_dr.finalize_wrote_dispatch && !g_dr.helper_in_any_record_field &&
        g_dr.helper_norm && (target & ~1u) != g_dr.helper_norm) {
        /* Cross-target would need to prove otherwise; default: helper is NOT the
         * DSM second-hop dispatch target under documented mythroad ABI. */
        c = CONTRACT_CASE_HELPER_NOT_DISPATCH_TARGET;
        open = 0;
    } else if (g_dr.helper_in_any_record_field && r &&
               r->helper_field_offset != 0xFFFFFFFFu &&
               r->dispatch_field_offset != 0xFFFFFFFFu &&
               r->helper_field_offset != r->dispatch_field_offset)
        c = CONTRACT_CASE_WRONG_FIELD_READ;
    else if (g_dr.registered_seen && !g_dr.finalize_seen && !g_dr.finalize_wrote_dispatch)
        c = CONTRACT_CASE_REGISTER_FINALIZE_MISSING;
    else if (r && r->module_id && g_dr.helper_module_id && r->module_id != g_dr.helper_module_id)
        c = CONTRACT_CASE_WRONG_RECORD_ASSOCIATION;
    else if (g_dr.helper_raw && ((g_dr.helper_raw & 1u) != 0) &&
             g_dr.helper_norm == (target & ~1u))
        c = CONTRACT_CASE_HELPER_ADDRESS_ENCODING;
    else if (ev >= EV_DOCUMENTED && !g_dr.finalize_wrote_dispatch)
        c = CONTRACT_CASE_HELPER_NOT_DISPATCH_TARGET;
    else
        c = CONTRACT_CASE_GENERIC_HANDOFF_MISSING;

    /* Gate open only with DOCUMENTED|CROSS_TARGET proving helper MUST be written
     * to the dispatch field — we currently have DOCUMENTED that it is NOT. */
    if ((ev >= EV_CROSS_TARGET || ev >= EV_DOCUMENTED) &&
        c != CONTRACT_CASE_HELPER_NOT_DISPATCH_TARGET && g_dr.finalize_wrote_dispatch == 0 &&
        g_dr.xt_helper_is_dispatch)
        open = 1;
    else
        open = 0;

    g_dr.last_case = c;
    g_dr.phase6b_b_open = open;
    g_dr.case_emitted = 1;

    if (!g_dr.finalize_seen && g_dr.registered_seen) {
        printf("[REGISTER_FINALIZE] module_id=%llu helper=0x%X finalizer_module=NONE "
               "finalizer_offset=0x0 record_field_written=none note=no_post_register_dispatch_write "
               "evidence=OBSERVED\n",
               (unsigned long long)g_dr.helper_module_id, g_dr.helper_raw);
        fflush(stdout);
    }

    printf("[CONTRACT_CASE] case=%s evidence=%s "
           "legend=REGISTER_FINALIZE_MISSING|WRONG_RECORD_ASSOCIATION|WRONG_FIELD_READ|"
           "HELPER_ADDRESS_ENCODING|HELPER_NOT_DISPATCH_TARGET|GENERIC_HANDOFF_MISSING|UNKNOWN "
           "helper=0x%X direct=0x%X helper_in_record=%d finalize_wrote_dispatch=%d "
           "documented_no_dispatch_write=1 phase6b_b_gate=%s\n",
           ext_contract_case_name(c), ext_evidence_level_name(ev), g_dr.helper_raw, target,
           g_dr.helper_in_any_record_field, g_dr.finalize_wrote_dispatch,
           open ? "open" : "blocked");
    fflush(stdout);
}

void ext_dsm_record_on_second_hop(void *uc, uint32_t caller_pc, uint32_t target, uint32_t r0,
                                  uint32_t r1, uint64_t from_mid, uint64_t to_mid) {
    ModuleRegistry *reg;
    const GwyLoadedModule *to_m;
    DsmModuleRecord *r = NULL;
    uint32_t helper = 0;
    if (!ext_dsm_record_contract_enabled()) return;
    (void)r0;
    (void)r1;
    (void)from_mid;
    reg = gwy_ext_loader_bound_registry();
    to_m = reg ? module_registry_find_by_id(reg, to_mid) : NULL;
    if (to_m) helper = to_m->entries.registered_helper;
    if (helper) {
        g_dr.helper_raw = helper;
        g_dr.helper_norm = helper & ~1u;
        g_dr.helper_module_id = to_mid;
        g_dr.registered_seen = 1;
    }
    reverse_bind_dispatch_field(uc ? uc : g_dr.uc, caller_pc, target, to_mid, &r);
    emit_write_history_summary(r);
    /* Only classify when helper belongs to callee module (robotol). */
    if (to_m && helper && to_m->map.guest_code_base && to_m->map.guest_code_size) {
        uint32_t hn = helper & ~1u;
        if (hn >= to_m->map.guest_code_base &&
            hn < to_m->map.guest_code_base + to_m->map.guest_code_size)
            classify_contract(r, target);
    }
}