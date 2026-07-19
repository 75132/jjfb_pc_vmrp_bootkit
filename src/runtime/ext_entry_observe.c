#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_chunk_observe.h"
#include "gwy_launcher/ext_helper_handoff.h"
#include "gwy_launcher/ext_dsm_record_observe.h"
#include "gwy_launcher/ext_module_entry_abi.h"
#include "gwy_launcher/ext_entry_null_contract.h"
#include "gwy_launcher/ext_module_data_init.h"
#include "gwy_launcher/ext_er_rw_producer.h"
#include "gwy_launcher/ext_bootstrap_abi.h"
#include "gwy_launcher/ext_callback_frame.h"
#include "gwy_launcher/ext_post_cont_audit.h"
#include "gwy_launcher/ext_post_cfn_r9_audit.h"
#include "gwy_launcher/ext_p_extchunk_audit.h"
#include "gwy_launcher/ext_shell_publication_audit.h"
#include "gwy_launcher/ext_cfunction_publication_audit.h"
#include "gwy_launcher/ext_gwy_startgame_audit.h"
#include "gwy_launcher/ext_r9_scope_audit.h"
#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/ext_object_observe.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

typedef struct {
    GuestInstructionTrace ring[GWY_ENTRY_TRACE_RING];
    size_t ring_count;
    size_t ring_next;
    int tracing;
    uint64_t trace_module_id;
    ExtEntryCallContext entry;
    GuestFaultReport fault;
    int entry_marked;
    HelperAbiBoundary call_site;
    HelperAbiBoundary thunk;
    HelperAbiBoundary enter;
    int call_site_window_dumped;
    int dsm_select_logged;
    uint32_t p_candidate;
    uint32_t p_candidate_len;
    uint32_t chunk_candidate;
    int field28_write_seen;
    int chunk_field_04_write_seen;
    int context_watch_armed;
    int watch_base_is_chunk;
    int candidates_disasm_done;
    char bootstrap_seq[256];
    size_t bootstrap_seq_len;
#ifdef GWY_HAVE_UNICORN
    uc_hook write_hook;
    void *write_uc;
#endif
} ExtEntryObserveState;

static ExtEntryObserveState g_obs;
static void *g_obs_uc;

void ext_entry_observe_reset(void) {
    memset(&g_obs, 0, sizeof(g_obs));
    g_obs_uc = NULL;
    ext_chunk_observe_reset();
    ext_object_observe_reset();
    ext_helper_handoff_reset();
    ext_dsm_record_observe_reset();
}

void ext_entry_observe_bind_uc(void *uc) {
    g_obs_uc = uc;
    ext_chunk_observe_bind_uc(uc);
}

const ExtEntryCallContext *ext_entry_observe_last_entry(void) {
    return g_obs.entry.valid ? &g_obs.entry : NULL;
}

const GuestFaultReport *ext_entry_observe_last_fault(void) {
    return g_obs.fault.valid ? &g_obs.fault : NULL;
}

const HelperAbiBoundary *ext_entry_observe_last_helper_abi(HelperAbiStage stage) {
    switch (stage) {
        case HELPER_ABI_STAGE_CALL_SITE_BEFORE:
            return g_obs.call_site.valid ? &g_obs.call_site : NULL;
        case HELPER_ABI_STAGE_THUNK_ENTER:
            return g_obs.thunk.valid ? &g_obs.thunk : NULL;
        case HELPER_ABI_STAGE_ROBOTOL_ENTER:
            return g_obs.enter.valid ? &g_obs.enter : NULL;
        default:
            return NULL;
    }
}

const char *ext_fault_root_cause_name(ExtFaultRootCause c) {
    switch (c) {
        case EXT_FAULT_CLASS_ENTRY_ARGUMENT: return "ENTRY_ARGUMENT";
        case EXT_FAULT_CLASS_ER_RW_INIT: return "ER_RW_INIT";
        case EXT_FAULT_CLASS_RELOCATION: return "RELOCATION";
        case EXT_FAULT_CLASS_PLATFORM_CONTEXT: return "PLATFORM_CONTEXT";
        case EXT_FAULT_CLASS_WRONG_ENTRY: return "WRONG_ENTRY";
        case EXT_FAULT_CLASS_OTHER: return "OTHER";
        default: return "UNKNOWN";
    }
}

const char *helper_abi_stage_name(HelperAbiStage s) {
    switch (s) {
        case HELPER_ABI_STAGE_CALL_SITE_BEFORE: return "CALL_SITE_BEFORE";
        case HELPER_ABI_STAGE_THUNK_ENTER: return "THUNK_ENTER";
        case HELPER_ABI_STAGE_ROBOTOL_ENTER: return "ROBOTOL_ENTER";
        default: return "UNKNOWN";
    }
}

int ext_entry_decode_ldr_imm(uint32_t insn,
                             int thumb,
                             int *out_rt,
                             int *out_rn,
                             uint32_t *out_imm,
                             int *out_is_load) {
    if (out_rt) *out_rt = -1;
    if (out_rn) *out_rn = -1;
    if (out_imm) *out_imm = 0;
    if (out_is_load) *out_is_load = 0;

    if (!thumb) {
        if ((insn & 0x0C000000u) != 0x04000000u) return 0;
        if ((insn & (1u << 25)) != 0) return 0;
        {
            int L = (insn >> 20) & 1;
            int U = (insn >> 23) & 1;
            int Rn = (insn >> 16) & 0xF;
            int Rd = (insn >> 12) & 0xF;
            uint32_t imm = insn & 0xFFFu;
            if (out_rt) *out_rt = Rd;
            if (out_rn) *out_rn = Rn;
            if (out_imm) *out_imm = imm;
            if (out_is_load) *out_is_load = L;
            (void)U;
            return 1;
        }
    }

    if ((insn & 0xF800u) == 0x6800u) {
        int Rt = insn & 0x7;
        int Rn = (insn >> 3) & 0x7;
        uint32_t imm = ((insn >> 6) & 0x1Fu) * 4u;
        if (out_rt) *out_rt = Rt;
        if (out_rn) *out_rn = Rn;
        if (out_imm) *out_imm = imm;
        if (out_is_load) *out_is_load = 1;
        return 1;
    }
    if ((insn & 0xF800u) == 0x9800u) {
        int Rt = (insn >> 8) & 0x7;
        uint32_t imm = (insn & 0xFFu) * 4u;
        if (out_rt) *out_rt = Rt;
        if (out_rn) *out_rn = 13;
        if (out_imm) *out_imm = imm;
        if (out_is_load) *out_is_load = 1;
        return 1;
    }
    return 0;
}

int ext_entry_decode_thumb_blx_rm(uint16_t half, int *out_rm) {
    /* 010001 11 1 Rm 000 */
    if ((half & 0xFF87u) != 0x4780u) return 0;
    if (out_rm) *out_rm = (int)((half >> 3) & 0xFu);
    return 1;
}

int ext_entry_decode_arm_blx_rm(uint32_t insn, int *out_rm) {
    if ((insn & 0x0FFFFFF0u) != 0x012FFF30u) return 0;
    if (out_rm) *out_rm = (int)(insn & 0xFu);
    return 1;
}

static const char *module_display_name(const GwyLoadedModule *m) {
    if (!m) return "unknown";
    if (m->origin == MODULE_ORIGIN_DSM) return "dsm:cfunction.ext";
    if (m->resolved_name[0]) return m->resolved_name;
    if (m->requested_name[0]) return m->requested_name;
    return "unknown";
}

/* Stage E: guest path for logs (prefer logical package, else stem containing .mrp). */
static const char *module_package_guest(const ModuleRegistry *reg, const GwyLoadedModule *m) {
    const char *p;
    if (reg && reg->logical_package[0]) return reg->logical_package;
    if (!m || !m->package_path[0]) return "?";
    p = m->package_path;
    if (strstr(p, "gwy/jjfb.mrp") || strstr(p, "gwy\\jjfb.mrp")) return "gwy/jjfb.mrp";
    if (strstr(p, "gwy/wxjwq.mrp") || strstr(p, "gwy\\wxjwq.mrp")) return "gwy/wxjwq.mrp";
    if (strstr(p, "jjfb.mrp")) return "gwy/jjfb.mrp";
    if (strstr(p, "wxjwq.mrp")) return "gwy/wxjwq.mrp";
    return p;
}

static int pc_in_module_image(const GwyLoadedModule *m, uint32_t pc) {
    uint32_t p, base, size;
    if (!m) return 0;
    p = pc & ~1u;
    base = m->map.guest_code_base;
    size = m->map.guest_code_size;
    if (!base || !size) return 0;
    return p >= base && p < (base + size);
}

/*
 * True robotol.ext MODULE ENTRY only (Stage E).
 * Rejects mrc_loader / DSM / wrong package / PC outside mapped image.
 */
static int is_true_robotol_enter(const ModuleRegistry *reg, const GwyLoadedModule *m,
                                 uint32_t entry_pc, const char **reject_reason) {
    const char *mod;
    const char *pkg;
    if (reject_reason) *reject_reason = "not_robotol_module";
    if (!m) {
        if (reject_reason) *reject_reason = "not_robotol_module";
        return 0;
    }
    if (m->origin == MODULE_ORIGIN_DSM) {
        if (reject_reason) *reject_reason = "dsm_context";
        return 0;
    }
    if (m->origin != MODULE_ORIGIN_MRP_MEMBER) {
        if (reject_reason) *reject_reason = "loader_context";
        return 0;
    }
    mod = module_display_name(m);
    if (!mod || strcmp(mod, "robotol.ext") != 0) {
        if (reject_reason) {
            if (strstr(mod, "mrc_loader"))
                *reject_reason = "loader_context";
            else
                *reject_reason = "not_robotol_module";
        }
        return 0;
    }
    pkg = module_package_guest(reg, m);
    if (!pkg || (!strstr(pkg, "jjfb.mrp") && !strstr(m->package_path, "jjfb.mrp"))) {
        if (reject_reason) *reject_reason = "wrong_package";
        return 0;
    }
    if (!pc_in_module_image(m, entry_pc)) {
        if (reject_reason) *reject_reason = "pc_out_of_range";
        return 0;
    }
    return 1;
}

static void emit_module_identity(const ModuleRegistry *reg, const GwyLoadedModule *m,
                                 uint32_t entry_pc, int entry_in_range) {
    const char *src = getenv("JJFB_LAUNCH_SOURCE");
    const char *pkg;
    const char *mod;
    if (!m) return;
    if (!src || !src[0]) {
        if (getenv("JJFB_PRODUCT_DESCRIPTOR_DIRECT") &&
            getenv("JJFB_PRODUCT_DESCRIPTOR_DIRECT")[0] == '1')
            src = "descriptor_launcher";
        else
            src = "?";
    }
    pkg = module_package_guest(reg, m);
    mod = module_display_name(m);
    printf("[JJFB_MODULE_IDENTITY] package=%s module=%s module_id=%llu image_base=0x%X "
           "image_size=0x%X entry_pc=0x%X entry_in_range=%s sha256=%s source=%s\n",
           pkg ? pkg : "?", mod, (unsigned long long)m->module_id, m->map.guest_code_base,
           m->map.guest_code_size, entry_pc, entry_in_range ? "yes" : "no",
           (reg && reg->package_sha256_hex[0]) ? reg->package_sha256_hex : "?", src);
    fflush(stdout);
}

void ext_entry_observe_format_caller(uint32_t addr,
                                     char *name_out,
                                     size_t name_cap,
                                     uint32_t *offset_out,
                                     uint64_t *module_id_out) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = NULL;
    uint32_t a = addr & ~1u;

    if (name_out && name_cap) name_out[0] = '\0';
    if (offset_out) *offset_out = 0;
    if (module_id_out) *module_id_out = 0;
    if (!reg || a == 0) {
        if (name_out && name_cap) snprintf(name_out, name_cap, "unknown");
        return;
    }
    m = module_registry_find_by_code_addr(reg, a);
    if (!m) {
        if (name_out && name_cap) snprintf(name_out, name_cap, "unknown");
        return;
    }
    if (module_id_out) *module_id_out = m->module_id;
    if (name_out && name_cap) snprintf(name_out, name_cap, "%s", module_display_name(m));
    if (offset_out && m->map.guest_code_base && a >= m->map.guest_code_base)
        *offset_out = a - m->map.guest_code_base;
}

void ext_entry_observe_bootstrap_event(const char *tag) {
    size_t n;
    if (!tag || !tag[0]) return;
    n = strlen(tag);
    if (g_obs.bootstrap_seq_len + n + 2 >= sizeof(g_obs.bootstrap_seq)) return;
    if (g_obs.bootstrap_seq_len > 0) {
        g_obs.bootstrap_seq[g_obs.bootstrap_seq_len++] = '>';
    }
    memcpy(g_obs.bootstrap_seq + g_obs.bootstrap_seq_len, tag, n);
    g_obs.bootstrap_seq_len += n;
    g_obs.bootstrap_seq[g_obs.bootstrap_seq_len] = '\0';
    printf("[BOOTSTRAP_SEQ] event=%s seq=%s\n", tag, g_obs.bootstrap_seq);
    fflush(stdout);
}

const char *ext_entry_observe_entry_class(uint64_t module_id, uint32_t observed_pc) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    uint32_t pc = gwy_guest_pc_norm(observed_pc);
    if (!m) return "unknown";
    if (m->entries.header_entry_candidate &&
        pc == gwy_guest_pc_norm(m->entries.header_entry_candidate))
        return "LEGAL_HEADER_ENTRY_NULL_R0";
    if (m->entries.chunk_field_04 && pc == gwy_guest_pc_norm(m->entries.chunk_field_04))
        return "CHUNK_INIT_ENTRY";
    if (m->entries.registered_helper && pc == gwy_guest_pc_norm(m->entries.registered_helper)) {
        /*
         * DOCUMENTED: bridge_mr_extHelper enters registered helper with r0=P.
         * WRONG_HELPER applies when helper is used as module entry without P
         * (guest nested / LR_PROXY with r0==0 or r0==method code only).
         */
        if (g_obs.entry.valid && g_obs.entry.r0 != 0 && g_obs.entry.r0 != g_obs.entry.method &&
            (g_obs.entry.method != 0 || g_obs.entry.r0 > 0x1000u))
            return "LEGAL_HELPER_EVENT_WITH_P";
        return "WRONG_HELPER_CALL_MISSING_P";
    }
    return "WRONG_ENTRY_SELECTION";
}

static void emit_entry_reconcile(GwyLoadedModule *mut,
                                 const char *dsm_select_correct,
                                 GwyEntryRelation rel) {
    const char *mod;
    uint32_t header_raw, chunk_raw, helper_raw, obs_raw;
    if (!mut) return;
    if (rel != GWY_ENTRY_REL_UNKNOWN) mut->entries.relation = rel;
    mod = module_display_name(mut);
    header_raw = mut->entries.header_entry_candidate;
    chunk_raw = mut->entries.chunk_field_04;
    helper_raw = mut->entries.registered_helper;
    obs_raw = mut->entries.observed_first_pc;
    printf("[ENTRY_RECONCILE] module=%s module_id=%llu image_base=0x%X "
           "header_raw=0x%X header_norm=0x%X "
           "chunk_field_04_raw=0x%X chunk_field_04_norm=0x%X "
           "helper_raw=0x%X helper_norm=0x%X "
           "observed_first_pc_raw=0x%X observed_norm=0x%X "
           "relation=%s dsm_select_correct=%s "
           "chunk_field_04_writer=%s\n",
           mod, (unsigned long long)mut->module_id, mut->entries.image_base, header_raw,
           gwy_guest_pc_norm(header_raw), chunk_raw, gwy_guest_pc_norm(chunk_raw), helper_raw,
           gwy_guest_pc_norm(helper_raw), obs_raw, gwy_guest_pc_norm(obs_raw),
           gwy_entry_relation_name(mut->entries.relation),
           dsm_select_correct ? dsm_select_correct : "unknown",
           g_obs.chunk_field_04_write_seen ? "SEEN" : "NONE_BEFORE_SELECT");
    fflush(stdout);
    ext_cfunction_publication_audit_on_entry_reconcile(
        mod, chunk_raw, g_obs.chunk_field_04_write_seen ? "SEEN" : "NONE_BEFORE_SELECT");
}

static GwyEntryRelation classify_entry_relation(const GwyLoadedModule *m, uint32_t dsm_target) {
    uint32_t t = gwy_guest_pc_norm(dsm_target);
    uint32_t h, help;
    if (!m) return GWY_ENTRY_REL_UNKNOWN;
    h = gwy_guest_pc_norm(m->entries.header_entry_candidate);
    help = gwy_guest_pc_norm(m->entries.registered_helper);
    /* Case A: DSM/observed target is header or helper (Thumb-normalized). */
    if (t && h && t == h) {
        if (dsm_target != m->entries.header_entry_candidate)
            return GWY_ENTRY_REL_THUMB_NORMALIZED_MATCH;
        return GWY_ENTRY_REL_DIRECT_MATCH;
    }
    if (t && help && t == help) {
        if (dsm_target != m->entries.registered_helper)
            return GWY_ENTRY_REL_THUMB_NORMALIZED_MATCH;
        return GWY_ENTRY_REL_DIRECT_MATCH;
    }
    /* Do NOT treat chunk_field_04==observed as match — field is often filled from the
     * same DSM target (circular). If target is neither header nor helper → Case C. */
    if (t && h && t != h && (!help || t != help))
        return GWY_ENTRY_REL_HEADER_ENTRY_WRONG;
    return GWY_ENTRY_REL_UNKNOWN;
}

#ifdef GWY_HAVE_UNICORN
static void arm_context_write_watch(uc_engine *uc, uint32_t watch_base, uint32_t watch_len,
                                    int is_chunk);

static void on_context_mem_write(uc_engine *uc,
                                 uc_mem_type type,
                                 uint64_t address,
                                 int size,
                                 int64_t value,
                                 void *user_data) {
    uint32_t addr = (uint32_t)address;
    uint32_t base;
    uint32_t off;
    uint32_t pc = 0;
    char writer_mod[96];
    uint32_t writer_off = 0;
    uint64_t writer_mid = 0;
    const char *tclass = "unknown";
    ModuleRegistry *reg;
    LauncherError err;
    (void)type;
    (void)size;
    (void)user_data;

    /* Prefer chunk base when known; else P. */
    base = g_obs.chunk_candidate ? g_obs.chunk_candidate : g_obs.p_candidate;
    if (!base || addr < base) return;
    off = addr - base;

    /* P+0x0C may install chunk pointer after first P_CANDIDATE. */
    if (!g_obs.watch_base_is_chunk && g_obs.p_candidate && addr == g_obs.p_candidate + 0x0Cu &&
        (uint32_t)value != 0) {
        const char *env = getenv("GWY_CONTEXT_WRITE_WATCH");
        g_obs.chunk_candidate = (uint32_t)value;
        printf("[CONTEXT_CANDIDATE] kind=mrc_extChunk_ptr chunk=0x%X from=P+0x0C_write\n",
               g_obs.chunk_candidate);
        fflush(stdout);
        ext_chunk_observe_on_p_plus_0c(g_obs.chunk_candidate, g_obs.p_candidate);
        if (env && env[0] == '1') {
            arm_context_write_watch(uc, g_obs.chunk_candidate, 0x40u, 1);
        }
        printf("[CONTEXT_FIELD_WRITE] allocation_id=p base=0x%X offset=0x0C value=0x%X "
               "writer_module=pending target_class=chunk_ptr\n",
               g_obs.p_candidate, (uint32_t)value);
        fflush(stdout);
        return;
    }

    if (off != 0x04u && off != 0x00u && off != 0x08u && off != 0x28u && off != 0x0Cu &&
        off != 0x2Cu)
        return;

    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    ext_entry_observe_format_caller(pc, writer_mod, sizeof(writer_mod), &writer_off, &writer_mid);

    if (off == 0x28u) {
        g_obs.field28_write_seen = 1;
        tclass = ((uint32_t)value > 0x10000u) ? "function_pointer" : "guest_data";
    } else if (off == 0x04u) {
        tclass = ((uint32_t)value > 0x10000u) ? "function_pointer" : "guest_data";
        /* Only treat as chunk_field_04 write when value lands in a mapped EXT image. */
        reg = gwy_ext_loader_bound_registry();
        if (reg && (uint32_t)value > 0x10000u) {
            const GwyLoadedModule *tgt =
                module_registry_find_by_code_addr(reg, gwy_guest_pc_norm((uint32_t)value));
            uint64_t mid = tgt ? tgt->module_id : 0;
            const char *vs_h = "ne";
            const char *vs_help = "ne";
            if (tgt) {
                g_obs.chunk_field_04_write_seen = 1;
                module_registry_set_chunk_field_04(reg, mid, (uint32_t)value, &err);
                if (gwy_guest_pc_norm((uint32_t)value) ==
                    gwy_guest_pc_norm(tgt->entries.header_entry_candidate))
                    vs_h = "eq";
                if (gwy_guest_pc_norm((uint32_t)value) ==
                    gwy_guest_pc_norm(tgt->entries.registered_helper))
                    vs_help = "eq";
                printf("[CHUNK_FIELD_04_WRITE] chunk_base=0x%X value=0x%X writer_module=%s "
                       "writer_offset=0x%X module=%s module_id=%llu vs_header_norm=%s "
                       "vs_helper_norm=%s\n",
                       base, (uint32_t)value, writer_mod, writer_off, module_display_name(tgt),
                       (unsigned long long)mid, vs_h, vs_help);
                fflush(stdout);
                {
                    GwyLoadedModule *mut = NULL;
                    size_t i;
                    for (i = 0; i < reg->count; i++) {
                        if (reg->modules[i].module_id == mid) {
                            mut = &reg->modules[i];
                            break;
                        }
                    }
                    if (mut) {
                        GwyEntryRelation rel =
                            classify_entry_relation(mut, (uint32_t)value);
                        emit_entry_reconcile(mut, "unknown", rel);
                    }
                }
            } else {
                printf("[CHUNK_FIELD_04_WRITE] chunk_base=0x%X value=0x%X writer_module=%s "
                       "writer_offset=0x%X module=unknown module_id=0 vs_header_norm=ne "
                       "vs_helper_norm=ne note=value_not_in_mapped_ext\n",
                       base, (uint32_t)value, writer_mod, writer_off);
                fflush(stdout);
            }
        }
    }

    printf("[CONTEXT_FIELD_WRITE] allocation_id=p_or_chunk base=0x%X offset=0x%X value=0x%X "
           "writer_module=%s writer_offset=0x%X target_class=%s\n",
           base, off, (uint32_t)value, writer_mod, writer_off, tclass);
    /* Phase 6J: mirror P field writes into publication chain (observe-only). */
    if (!g_obs.watch_base_is_chunk && g_obs.p_candidate && base == g_obs.p_candidate &&
        (off == 0x00u || off == 0x04u || off == 0x08u || off == 0x0Cu || off == 0x10u)) {
        ext_shell_publication_audit_on_p_candidate(base);
        ext_shell_publication_audit_on_p_write(base, off, 0, (uint32_t)value, pc, 0, writer_mod);
    }
    fflush(stdout);
}

static void arm_context_write_watch(uc_engine *uc, uint32_t watch_base, uint32_t watch_len,
                                    int is_chunk) {
    uc_err ue;
    if (!uc || !watch_base || !watch_len) return;
    if (g_obs.context_watch_armed && g_obs.write_uc == uc) {
        uc_hook_del(uc, g_obs.write_hook);
        g_obs.context_watch_armed = 0;
    }
    ue = uc_hook_add(uc, &g_obs.write_hook, UC_HOOK_MEM_WRITE, (void *)on_context_mem_write, NULL,
                     (uint64_t)watch_base, (uint64_t)watch_base + (uint64_t)watch_len - 1ull);
    if (ue == UC_ERR_OK) {
        g_obs.context_watch_armed = 1;
        g_obs.watch_base_is_chunk = is_chunk;
        g_obs.write_uc = uc;
        printf("[CONTEXT_FIELD_WRITE] watch_armed base=0x%X len=%u is_chunk=%d\n", watch_base,
               watch_len, is_chunk);
        fflush(stdout);
    }
}
#endif

void ext_entry_observe_on_p_candidate(uint32_t p_guest, uint32_t p_len, void *uc) {
    const char *env;
    if (!uc) uc = g_obs_uc;
    if (!p_guest) return;
    g_obs.p_candidate = p_guest;
    g_obs.p_candidate_len = p_len ? p_len : 20u;
    printf("[CONTEXT_CANDIDATE] kind=mr_c_function_st p=0x%X len=%u evidence=OBSERVED\n", p_guest,
           g_obs.p_candidate_len);
    fflush(stdout);
    ext_entry_observe_bootstrap_event("P_CANDIDATE");

#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uint32_t chunk = 0;
        if (guest_memory_uc_peek_u32((struct uc_struct *)uc, p_guest + 0x0Cu, &chunk) && chunk) {
            g_obs.chunk_candidate = chunk;
            printf("[CONTEXT_CANDIDATE] kind=mrc_extChunk_ptr chunk=0x%X from=P+0x0C\n", chunk);
            fflush(stdout);
            ext_chunk_observe_on_p_plus_0c(chunk, p_guest);
        }
    }

    env = getenv("GWY_CONTEXT_WRITE_WATCH");
    if (env && env[0] == '1' && uc) {
        if (g_obs.chunk_candidate) {
            arm_context_write_watch((uc_engine *)uc, g_obs.chunk_candidate, 0x40u, 1);
        } else {
            /* Watch P so P+0x0C write can retarget to chunk. */
            arm_context_write_watch((uc_engine *)uc, p_guest, 0x20u, 0);
        }
    }
#else
    (void)uc;
    (void)env;
#endif
}

static int dump_entry_candidate_disasm(void *uc, const char *role, uint32_t va,
                                       uint32_t lands_helper, uint32_t lands_obs) {
    int i;
    int thumb = (va & 1u) != 0;
    uint32_t pc = gwy_guest_pc_norm(va);
    const char *cls = "unknown";
    const char *lands = "none";
    int saw_bx = 0;
    uint32_t branch_target = 0;
    int trampoline_code = 0; /* 1=to_obs 2=to_helper */

    if (!uc || !pc) return 0;
    printf("[ENTRY_CANDIDATE_DISASM] role=%s va=0x%X thumb=%d window=±%d\n", role, va, thumb,
           GWY_HELPER_ABI_CALLER_WINDOW);
    for (i = -GWY_HELPER_ABI_CALLER_WINDOW; i <= GWY_HELPER_ABI_CALLER_WINDOW; i++) {
        uint32_t addr;
        uint32_t word = 0;
        char line[128];
        int rt = -1, rn = -1, is_load = 0, rm = -1;
        uint32_t imm = 0;

        if (thumb) {
            uint16_t half;
            addr = pc + (uint32_t)(i * 2);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr & ~3u, &word)) continue;
            half = (uint16_t)((addr & 2u) ? (word >> 16) : (word & 0xFFFFu));
            if (i == 0 && (half & 0xFF00u) == 0xB500u) cls = "prologue";
            if (ext_entry_decode_thumb_blx_rm(half, &rm)) {
                snprintf(line, sizeof(line), "BLX r%d", rm);
                saw_bx = 1;
            } else if ((half & 0xF800u) == 0xE000u) {
                int32_t off11 = (int32_t)(half & 0x7FFu);
                if (off11 & 0x400) off11 |= ~0x7FF;
                branch_target = addr + 4u + (uint32_t)(off11 * 2);
                snprintf(line, sizeof(line), "B 0x%X", branch_target);
                saw_bx = 1;
            } else if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load)) {
                snprintf(line, sizeof(line), "%s r%d, [r%d, #0x%X]", is_load ? "LDR" : "STR", rt, rn,
                         imm);
            } else {
                snprintf(line, sizeof(line), ".hword 0x%04X", half);
            }
            word = half;
        } else {
            addr = pc + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
            if (i == 0 && (word & 0xFFFF0000u) == 0xE92D0000u) cls = "prologue";
            if (ext_entry_decode_arm_blx_rm(word, &rm)) {
                snprintf(line, sizeof(line), "BLX r%d", rm);
                saw_bx = 1;
            } else if ((word & 0x0F000000u) == 0x0A000000u) {
                int32_t imm24 = (int32_t)(word & 0x00FFFFFFu);
                if (imm24 & 0x800000) imm24 |= ~0x00FFFFFF;
                branch_target = addr + 8u + (uint32_t)(imm24 * 4);
                snprintf(line, sizeof(line), "B 0x%X", branch_target);
                saw_bx = 1;
            } else if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load)) {
                snprintf(line, sizeof(line), "%s r%d, [r%d, #0x%X]", is_load ? "LDR" : "STR", rt, rn,
                         imm);
            } else {
                snprintf(line, sizeof(line), ".word 0x%08X", word);
            }
        }
        printf("[ENTRY_CANDIDATE_DISASM] %c off=%+d addr=0x%X raw=0x%08X %s\n", i == 0 ? '*' : ' ', i,
               addr, word, line);
    }
    if (saw_bx && branch_target) {
        if (lands_obs && gwy_guest_pc_norm(branch_target) == gwy_guest_pc_norm(lands_obs)) {
            lands = "observed";
            cls = "trampoline";
            trampoline_code = 1;
        } else if (lands_helper &&
                   gwy_guest_pc_norm(branch_target) == gwy_guest_pc_norm(lands_helper)) {
            lands = "helper";
            cls = "trampoline";
            trampoline_code = 2;
        } else {
            lands = "other";
            if (strcmp(cls, "prologue") != 0) cls = "dispatcher";
        }
    } else if (strcmp(cls, "unknown") == 0 && !saw_bx) {
        cls = "data_or_unknown";
    }
    printf("[ENTRY_CANDIDATE_CLASS] role=%s class=%s lands_on=%s\n", role, cls, lands);
    fflush(stdout);
    return trampoline_code;
}

static uint32_t reconstruct_chunk_base_from_select_window(void *uc, uint32_t call_pc,
                                                          uint32_t r9) {
    /* Observe-only: recover chunk object after DSM clears r0 before BLX.
     * Pattern (CROSS_TARGET / TARGET_OBSERVED): 
     *   LDR r0,[pc,#imm]; ADD r0,r0,r9; LDR r0,[r0]; LDR r1,[r0,#4]; …; BLX r1
     */
    int i;
    uint32_t word = 0;
    int rt = -1, rn = -1, is_load = 0;
    uint32_t imm = 0;
    int saw_ldr_plus4 = 0;
    int saw_ldr_indirect = 0;
    int saw_add_r9 = 0;
    uint32_t pc_rel_addr = 0;
    uint32_t pc_rel_imm = 0;
    uint32_t lit = 0;
    uint32_t sum = 0;
    uint32_t chunk = 0;

    if (!uc || !call_pc) return 0;
    for (i = -8; i <= 0; i++) {
        uint32_t addr = gwy_guest_pc_norm(call_pc) + (uint32_t)(i * 4);
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rt == 1 &&
            rn == 0 && imm == 4u)
            saw_ldr_plus4 = 1;
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rt == 0 &&
            rn == 0 && imm == 0)
            saw_ldr_indirect = 1;
        /* ADD r0, r0, r9 : cond=1110 opcode=0100 S=0 Rn=0 Rd=0 shifter=r9 */
        if ((word & 0x0FFFFFFFu) == 0x00800009u) saw_add_r9 = 1;
        /* LDR r0, [pc, #imm] */
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rt == 0 &&
            rn == 15) {
            pc_rel_addr = addr;
            pc_rel_imm = imm;
        }
    }
    if (!saw_ldr_plus4 || !saw_ldr_indirect || !saw_add_r9 || !pc_rel_addr) return 0;
    /* ARM PC-relative: literal at (instr_addr + 8 + imm) for U=1 LDR */
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, pc_rel_addr + 8u + pc_rel_imm, &lit))
        return 0;
    sum = lit + r9;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, sum, &chunk)) return 0;
    printf("[CHUNK_PTR_TRACE] reconstruct=pc_rel_add_r9 lit=0x%X r9=0x%X slot=0x%X chunk=0x%X "
           "evidence=OBSERVED\n",
           lit, r9, sum, chunk);
    fflush(stdout);
    return chunk;
}

static void log_dsm_entry_select(void *uc, const HelperAbiBoundary *cs, uint32_t target) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *tgt = NULL;
    GwyLoadedModule *mut = NULL;
    uint32_t word = 0;
    int i;
    int rt = -1, rn = -1, is_load = 0;
    uint32_t imm = 0;
    const char *source = "unknown";
    const char *match = "neither";
    const char *r0_source = "unknown";
    const char *dsm_ok = "unknown";
    uint32_t header = 0, helper = 0, chunk_f04 = 0;
    uint32_t entry_off = 0;
    uint32_t tnorm;
    char entry_mod[96];
    GwyEntryRelation rel;
    size_t mi;
    int chunk_rn = -1;
    uint32_t dsm_chunk_base = 0;

    if (!cs || g_obs.dsm_select_logged) return;
    if (strncmp(cs->module_name, "dsm:", 4) != 0) return;
    g_obs.dsm_select_logged = 1;
    tnorm = gwy_guest_pc_norm(target);

    if (reg) {
        tgt = module_registry_find_by_code_addr(reg, tnorm);
        if (!tgt) tgt = module_registry_find_by_helper(reg, target);
    }
    if (tgt) {
        header = tgt->entries.header_entry_candidate;
        helper = tgt->entries.registered_helper;
        chunk_f04 = tgt->entries.chunk_field_04;
        if (!chunk_f04) {
            /* Record DSM-selected value as observed chunk+0x4 when write missed. */
            LauncherError err;
            module_registry_set_chunk_field_04(reg, tgt->module_id, target, &err);
            chunk_f04 = target;
            tgt = module_registry_find_by_id(reg, tgt->module_id);
        }
        snprintf(entry_mod, sizeof(entry_mod), "%s",
                 tgt->resolved_name[0] ? tgt->resolved_name : tgt->requested_name);
        if (tgt->map.guest_code_base && tnorm >= tgt->map.guest_code_base)
            entry_off = tnorm - tgt->map.guest_code_base;
        if (header && tnorm == gwy_guest_pc_norm(header)) match = "header_entry_candidate";
        else if (helper && tnorm == gwy_guest_pc_norm(helper)) match = "helper";
        else if (chunk_f04 && tnorm == gwy_guest_pc_norm(chunk_f04)) match = "chunk_field_04";
        for (mi = 0; mi < reg->count; mi++) {
            if (reg->modules[mi].module_id == tgt->module_id) {
                mut = &reg->modules[mi];
                break;
            }
        }
    } else {
        snprintf(entry_mod, sizeof(entry_mod), "unknown");
    }

    if (uc) {
        for (i = -8; i <= 0; i++) {
            uint32_t addr = gwy_guest_pc_norm(cs->pc) + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rt == 1 &&
                imm == 4u) {
                source = "chunk_field_0x4";
                chunk_rn = rn;
            }
        }
        for (i = -8; i < 0; i++) {
            uint32_t addr = gwy_guest_pc_norm(cs->pc) + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
            if ((word & 0x0FFFFFFF) == 0x03A00000u) {
                r0_source = "explicit_zero";
                break;
            }
            if ((word & 0x0FFF0FF0u) == 0x01A00000u && ((word >> 12) & 0xF) == 0) {
                r0_source = "mov_from_reg";
                break;
            }
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rt == 0)
                r0_source = "loaded_field";
        }
#ifdef GWY_HAVE_UNICORN
        if (chunk_rn >= 0 && chunk_rn <= 15) {
            uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0 + chunk_rn, &dsm_chunk_base);
        }
#endif
    }
    /* At BLX, r0 is often already cleared after LDR r1,[chunk,#4]; recover via P+0x0C or
     * reconstruct pc-rel+r9 chain from the caller window. */
    if (!dsm_chunk_base) dsm_chunk_base = g_obs.chunk_candidate;
    if (!dsm_chunk_base && g_obs.p_candidate && uc) {
        uint32_t ch = 0;
        if (guest_memory_uc_peek_u32((struct uc_struct *)uc, g_obs.p_candidate + 0x0Cu, &ch) &&
            ch) {
            dsm_chunk_base = ch;
            g_obs.chunk_candidate = ch;
            printf("[CONTEXT_CANDIDATE] kind=mrc_extChunk_ptr chunk=0x%X from=P+0x0C_peek_at_select\n",
                   ch);
            fflush(stdout);
            ext_chunk_observe_on_p_plus_0c(ch, g_obs.p_candidate);
        }
    }
    if (!dsm_chunk_base && uc && cs) {
        uint32_t cand = reconstruct_chunk_base_from_select_window(uc, cs->pc, cs->r9);
        if (cand) {
            uint32_t at4 = 0;
            guest_memory_uc_peek_u32((struct uc_struct *)uc, cand + 4u, &at4);
            dsm_chunk_base = cand;
            g_obs.chunk_candidate = cand;
            if (!(at4 == target || gwy_guest_pc_norm(at4) == tnorm)) {
                printf("[CHUNK_PTR_TRACE] reconstruct_field04_mismatch cand=0x%X at4=0x%X "
                       "dsm_target=0x%X note=observe_anyway\n",
                       cand, at4, target);
                fflush(stdout);
            }
            ext_chunk_observe_on_p_plus_0c(cand, g_obs.p_candidate);
        }
    }
    if (cs->r0 == 0 && strcmp(r0_source, "unknown") == 0) r0_source = "explicit_zero";

    if (!g_obs.chunk_field_04_write_seen) {
        printf("[CHUNK_FIELD_04_WRITE] chunk_base=0x%X value=0x%X writer_module=NONE "
               "writer_offset=0x0 module=%s note=NONE_BEFORE_SELECT\n",
               dsm_chunk_base ? dsm_chunk_base : g_obs.chunk_candidate, target, entry_mod);
        fflush(stdout);
    }

    printf("[DSM_ENTRY_SELECT] source=%s entry_va=0x%X entry_module=%s entry_offset=0x%X "
           "registered_helper=0x%X header_entry_candidate=0x%X chunk_field_04=0x%X match=%s "
           "r0_source=%s r0_value=0x%X r1_target=0x%X "
           "caller_module=%s caller_offset=0x%X evidence=OBSERVED+DOCUMENTED_chunk_plus4 "
           "chunk_field_04_writer=%s chunk_base=0x%X chunk_reg=r%d\n",
           source, tnorm, entry_mod, entry_off, helper, header, chunk_f04, match, r0_source,
           cs->r0, target, cs->module_name, cs->module_offset,
           g_obs.chunk_field_04_write_seen ? "SEEN" : "NONE_BEFORE_SELECT", dsm_chunk_base,
           chunk_rn >= 0 ? chunk_rn : -1);
    fflush(stdout);
    ext_entry_observe_bootstrap_event("DSM_ENTRY_SELECT");

    {
        uint32_t first_pc = tgt ? tgt->entries.observed_first_pc : 0;
        ExtAddrClassResult dcls;
        ext_object_classify_address(tnorm, cs->sp, &dcls);
        printf("[ENTRY_TRANSFER] dispatch_target=0x%X dispatch_module=%s dispatch_offset=0x%X "
               "callee_module=%s first_executed_pc=0x%X r0=0x%X stage=CALL_SITE_BEFORE "
               "raw_target=0x%X\n",
               tnorm, dcls.module_name, dcls.module_offset, dcls.module_name, first_pc, cs->r0,
               target);
        fflush(stdout);
    }

    /* A4: reverse entry_object identity (independent of chunk provenance). */
    ext_object_observe_on_dsm_select(uc, cs->pc, cs->r9, cs->sp, cs->r0, chunk_rn, target,
                                     dsm_chunk_base, cs->module_name, cs->module_offset);

    if (ext_chunk_provenance_enabled()) {
        ext_chunk_observe_on_dsm_select(dsm_chunk_base, chunk_f04 ? chunk_f04 : target, target,
                                        cs->module_name, cs->module_offset);
    }

    rel = tgt ? classify_entry_relation(tgt, target) : GWY_ENTRY_REL_UNKNOWN;
    if (strcmp(match, "header_entry_candidate") == 0 || strcmp(match, "helper") == 0)
        dsm_ok = "yes";
    else if (rel == GWY_ENTRY_REL_THUMB_NORMALIZED_MATCH || rel == GWY_ENTRY_REL_DIRECT_MATCH)
        dsm_ok = "yes";
    else if (rel == GWY_ENTRY_REL_HEADER_ENTRY_WRONG)
        dsm_ok = "no"; /* DSM uses neither documented image+8 nor helper */
    else
        dsm_ok = "unknown";

    if (mut) emit_entry_reconcile(mut, dsm_ok, rel);

    if (uc && tgt && !g_obs.candidates_disasm_done) {
        const char *recon_env = getenv("GWY_ENTRY_RECONCILE");
        int tramp = 0;
        g_obs.candidates_disasm_done = 1;
        /* Heavy ±30 disasm only when reconcile live script opts in. */
        if (recon_env && recon_env[0] == '1') {
            if (chunk_f04 || target)
                tramp = dump_entry_candidate_disasm(uc, "chunk_field_04",
                                                    chunk_f04 ? chunk_f04 : target, helper,
                                                    tgt->entries.observed_first_pc);
            if (header)
                dump_entry_candidate_disasm(uc, "header", header, helper,
                                            tgt->entries.observed_first_pc);
            if (helper)
                dump_entry_candidate_disasm(uc, "helper", helper, helper,
                                            tgt->entries.observed_first_pc);
            if (mut && tramp == 1) {
                rel = GWY_ENTRY_REL_TRAMPOLINE_TO_ENTRY;
                dsm_ok = "yes";
                emit_entry_reconcile(mut, dsm_ok, rel);
            } else if (mut && tramp == 2) {
                rel = GWY_ENTRY_REL_TRAMPOLINE_TO_HELPER;
                dsm_ok = "yes";
                emit_entry_reconcile(mut, dsm_ok, rel);
            }
        }
    }

    /* Emit Case A–F at select time (fault may not occur in short live runs). */
    {
        const char *recon_case = "F";
        GwyEntryRelation r = mut ? mut->entries.relation : rel;
        if (r == GWY_ENTRY_REL_THUMB_NORMALIZED_MATCH || r == GWY_ENTRY_REL_DIRECT_MATCH)
            recon_case = "A";
        else if (r == GWY_ENTRY_REL_TRAMPOLINE_TO_ENTRY || r == GWY_ENTRY_REL_TRAMPOLINE_TO_HELPER)
            recon_case = "B";
        else if (r == GWY_ENTRY_REL_HEADER_ENTRY_WRONG)
            recon_case = "C";
        else if (r == GWY_ENTRY_REL_CHUNK_ENTRY_CORRUPT)
            recon_case = "D";
        else if (!g_obs.chunk_field_04_write_seen)
            recon_case = "E";
        printf("[ENTRY_RECONCILE_CASE] case=%s relation=%s chunk_field_04_writer=%s "
               "module=%s "
               "header_ev=DOCUMENTED_image_plus8_fixR9_TestCom800 "
               "chunk_plus4=DOCUMENTED_mrc_extChunk_field_04 "
               "dsm_match=%s dsm_select_correct=%s "
               "fix_applied=none phase6b_b_gate=blocked\n",
               recon_case, gwy_entry_relation_name(r),
               g_obs.chunk_field_04_write_seen ? "SEEN" : "NONE_BEFORE_SELECT", entry_mod, match,
               dsm_ok);
        fflush(stdout);
    }
}

static void fill_regs_from_uc(void *uc, uint32_t regs_out[16], uint32_t *cpsr_out) {
#ifdef GWY_HAVE_UNICORN
    uc_engine *u = (uc_engine *)uc;
    int i;
    for (i = 0; i < 13; i++) {
        uc_reg_read(u, UC_ARM_REG_R0 + i, &regs_out[i]);
    }
    uc_reg_read(u, UC_ARM_REG_SP, &regs_out[13]);
    uc_reg_read(u, UC_ARM_REG_LR, &regs_out[14]);
    uc_reg_read(u, UC_ARM_REG_PC, &regs_out[15]);
    if (cpsr_out) uc_reg_read(u, UC_ARM_REG_CPSR, cpsr_out);
#else
    (void)uc;
    memset(regs_out, 0, sizeof(uint32_t) * 16);
    if (cpsr_out) *cpsr_out = 0;
#endif
}

static uint64_t caller_module_id_from_lr(ModuleRegistry *reg, uint32_t lr) {
    const GwyLoadedModule *m;
    if (!reg || lr == 0) return 0;
    m = module_registry_find_by_code_addr(reg, lr & ~1u);
    return m ? m->module_id : 0;
}

static void dump_caller_window(void *uc, uint32_t call_pc, int thumb) {
    int i;
    uint32_t pc = call_pc & ~1u;

    printf("[HELPER_ABI_CALLER_DISASM] call_pc=0x%X thumb=%d window=±%d\n", pc, thumb,
           GWY_HELPER_ABI_CALLER_WINDOW);
    for (i = -GWY_HELPER_ABI_CALLER_WINDOW; i <= GWY_HELPER_ABI_CALLER_WINDOW; i++) {
        uint32_t addr;
        uint32_t word = 0;
        int rt = -1, rn = -1, is_load = 0, rm = -1;
        uint32_t imm = 0;
        char line[128];

        if (thumb) {
            addr = pc + (uint32_t)(i * 2);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr & ~3u, &word)) continue;
            {
                uint16_t half = (uint16_t)((addr & 2u) ? (word >> 16) : (word & 0xFFFFu));
                if (ext_entry_decode_thumb_blx_rm(half, &rm)) {
                    snprintf(line, sizeof(line), "BLX r%d", rm);
                } else if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load)) {
                    snprintf(line, sizeof(line), "%s r%d, [r%d, #0x%X]", is_load ? "LDR" : "STR",
                             rt, rn, imm);
                } else if ((half & 0xFF00u) == 0x2000u) {
                    snprintf(line, sizeof(line), "MOVS r%d, #0x%X", (half >> 8) & 7, half & 0xFFu);
                } else if ((half & 0xFFC0u) == 0x0000u && ((half >> 6) & 0x1Fu) == 0) {
                    snprintf(line, sizeof(line), "MOVS r%d, r%d", half & 7, (half >> 3) & 7);
                } else {
                    snprintf(line, sizeof(line), ".hword 0x%04X", half);
                }
                word = half;
            }
        } else {
            addr = pc + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
            if (ext_entry_decode_arm_blx_rm(word, &rm)) {
                snprintf(line, sizeof(line), "BLX r%d", rm);
            } else if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load)) {
                int U = (word >> 23) & 1;
                snprintf(line, sizeof(line), "%s r%d, [r%d, #%s0x%X]", is_load ? "LDR" : "STR", rt,
                         rn, U ? "" : "-", imm);
            } else if ((word & 0x0FE00000u) == 0x03A00000u) {
                int rd = (word >> 12) & 0xF;
                uint32_t imm8 = word & 0xFFu;
                uint32_t rot = ((word >> 8) & 0xFu) * 2u;
                uint32_t imm = imm8;
                if (rot) imm = (imm8 >> rot) | (imm8 << (32u - rot));
                snprintf(line, sizeof(line), "MOV r%d, #0x%X", rd, imm);
            } else {
                snprintf(line, sizeof(line), ".word 0x%08X", word);
            }
        }
        printf("[HELPER_ABI_CALLER_DISASM] %c off=%+d addr=0x%X raw=0x%08X %s\n", i == 0 ? '*' : ' ',
               i, addr, word, line);
    }
    fflush(stdout);
}

static void analyze_field28_forward(void *uc, GuestFaultReport *f) {
    uint32_t pc = f->fault_pc & ~1u;
    int thumb = (f->cpsr & (1u << 5)) != 0 || (f->fault_pc & 1u) != 0;
    int i;
    int saw_00 = 0, saw_04 = 0, saw_0c = 0, saw_28 = 0, saw_2c = 0;
    int r7_blx = 0, r7_store = 0, r7_ldr = 0;
    int dest_rt = -1;

    /* Decode fault LDR dest. */
    {
        uint32_t word = 0;
        int rt = -1, rn = -1, is_load = 0;
        uint32_t imm = 0;
        if (thumb) {
            if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) {
                uint16_t half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
                if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load)
                    dest_rt = rt;
            }
        } else if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &word)) {
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load)
                dest_rt = rt;
        }
    }

    printf("[HELPER_ABI_FIELD28] fault_pc=0x%X dest_rt=%d scan_forward=24\n", pc, dest_rt);
    for (i = 0; i < 24; i++) {
        uint32_t addr;
        uint32_t word = 0;
        int rt = -1, rn = -1, is_load = 0, rm = -1;
        uint32_t imm = 0;

        if (thumb) {
            addr = pc + (uint32_t)(i * 2);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr & ~3u, &word)) continue;
            {
                uint16_t half = (uint16_t)((addr & 2u) ? (word >> 16) : (word & 0xFFFFu));
                if (ext_entry_decode_thumb_blx_rm(half, &rm) && rm == dest_rt) r7_blx = 1;
                if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load)) {
                    if (rn == 0) {
                        if (imm == 0x00) saw_00 = 1;
                        if (imm == 0x04) saw_04 = 1;
                        if (imm == 0x0C) saw_0c = 1;
                        if (imm == 0x28) saw_28 = 1;
                        if (imm == 0x2C) saw_2c = 1;
                    }
                    if (rt == dest_rt && is_load) r7_ldr = 1;
                    if (rn == dest_rt && !is_load) r7_store = 1;
                }
            }
        } else {
            addr = pc + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
            if (ext_entry_decode_arm_blx_rm(word, &rm) && rm == dest_rt) r7_blx = 1;
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load)) {
                if (rn == 0) {
                    if (imm == 0x00) saw_00 = 1;
                    if (imm == 0x04) saw_04 = 1;
                    if (imm == 0x0C) saw_0c = 1;
                    if (imm == 0x28) saw_28 = 1;
                    if (imm == 0x2C) saw_2c = 1;
                }
                if (rt == dest_rt && is_load) r7_ldr = 1;
                if (rn == dest_rt && !is_load) r7_store = 1;
            }
        }
    }

    printf("[HELPER_ABI_FIELD28] r0_offsets saw_00=%d saw_04=%d saw_0c=%d saw_28=%d saw_2c=%d\n",
           saw_00, saw_04, saw_0c, saw_28, saw_2c);
    printf("[HELPER_ABI_FIELD28] dest_use blx=%d store=%d further_ldr=%d\n", r7_blx, r7_store,
           r7_ldr);
    /* DOCUMENTED mrc_extChunk: +0x28 sendAppEvent, +0x2c extMrTable; mr_c_function_st len=20. */
    if (saw_28 && saw_2c) {
        printf("[HELPER_ABI_FIELD28] layout_class=probable_mrc_extChunk "
               "field_28=sendAppEvent_candidate field_2c=extMrTable_candidate "
               "evidence=DOCUMENTED_header_offsets\n");
    } else if (saw_28 && !saw_2c) {
        printf("[HELPER_ABI_FIELD28] layout_class=unknown_or_partial_chunk "
               "field_28=unconfirmed note=mr_c_function_st_len20_cannot_hold_0x28\n");
    } else {
        printf("[HELPER_ABI_FIELD28] layout_class=unknown field_28=unconfirmed\n");
    }
    printf("[HELPER_ABI_FIELD28] UnknownEntryContext candidate: "
           "field_00=%s field_04=%s field_0c=%s field_28=%s field_2c=%s\n",
           saw_00 ? "seen" : "?", saw_04 ? "seen" : "?", saw_0c ? "seen" : "?",
           saw_28 ? "seen" : "?", saw_2c ? "seen" : "?");
    fflush(stdout);
}

static void fill_boundary(HelperAbiBoundary *b,
                          HelperAbiStage stage,
                          uint64_t module_id,
                          uint32_t pc,
                          uint32_t helper_address,
                          uint32_t target,
                          const uint32_t regs[16],
                          uint32_t cpsr,
                          const char *origin) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    const GwyLoadedModule *at_pc = reg ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
    uint32_t lr = regs ? regs[14] : 0;

    /* CALL_SITE module is the code that contains the call PC (often DSM), not the callee. */
    if (stage == HELPER_ABI_STAGE_CALL_SITE_BEFORE && at_pc) {
        m = at_pc;
        module_id = at_pc->module_id;
    }

    memset(b, 0, sizeof(*b));
    b->valid = 1;
    b->stage = stage;
    b->module_id = module_id;
    snprintf(b->module_name, sizeof(b->module_name), "%s", module_display_name(m));
    b->pc = pc;
    if (m && m->map.guest_code_base && (pc & ~1u) >= m->map.guest_code_base)
        b->module_offset = (pc & ~1u) - m->map.guest_code_base;
    b->helper_address = helper_address;
    b->target = target;
    if (regs) {
        b->r0 = regs[0];
        b->r1 = regs[1];
        b->r2 = regs[2];
        b->r3 = regs[3];
        b->r9 = regs[9];
        b->sp = regs[13];
        b->lr = regs[14];
    }
    b->cpsr = cpsr;
    snprintf(b->origin, sizeof(b->origin), "%s", origin ? origin : "?");
    ext_entry_observe_format_caller(lr, b->caller_module, sizeof(b->caller_module),
                                    &b->caller_offset, &b->caller_module_id);
}

void ext_entry_observe_helper_abi(HelperAbiStage stage,
                                  uint64_t module_id,
                                  uint32_t pc,
                                  uint32_t helper_address,
                                  uint32_t target,
                                  const uint32_t regs[16],
                                  uint32_t cpsr,
                                  const char *origin,
                                  void *uc_for_caller_window) {
    HelperAbiBoundary *slot = NULL;
    int thumb = (cpsr & (1u << 5)) != 0 || (pc & 1u) != 0;

    if (stage == HELPER_ABI_STAGE_CALL_SITE_BEFORE) slot = &g_obs.call_site;
    else if (stage == HELPER_ABI_STAGE_THUNK_ENTER) slot = &g_obs.thunk;
    else if (stage == HELPER_ABI_STAGE_ROBOTOL_ENTER) slot = &g_obs.enter;
    else return;

    /* Keep the first CALL_SITE for nested entry analysis; ignore later internal BLXs. */
    if (stage == HELPER_ABI_STAGE_CALL_SITE_BEFORE && slot->valid) return;

    fill_boundary(slot, stage, module_id, pc, helper_address, target, regs, cpsr, origin);

    {
        const char *stage_disp = helper_abi_stage_name(stage);
        /* Stage E: do not label non-robotol nested enters as ROBOTOL_ENTER. */
        if (stage == HELPER_ABI_STAGE_ROBOTOL_ENTER) {
            ModuleRegistry *r2 = gwy_ext_loader_bound_registry();
            const GwyLoadedModule *m2 = r2 ? module_registry_find_by_id(r2, module_id) : NULL;
            const char *rj = NULL;
            if (!is_true_robotol_enter(r2, m2, pc, &rj)) stage_disp = "MODULE_ENTER";
        }
        printf("[HELPER_ABI] stage=%s nested_enter_module=%s module=%s module_id=%llu "
               "module_offset=0x%X pc=0x%X "
               "helper=0x%X target=0x%X origin=%s "
               "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X sp=0x%X lr=0x%X cpsr=0x%X "
               "caller_module=%s caller_offset=0x%X\n",
               stage_disp, slot->module_name, slot->module_name, (unsigned long long)module_id,
               slot->module_offset, pc, helper_address, target, slot->origin, slot->r0, slot->r1,
               slot->r2, slot->r3, slot->r9, slot->sp, slot->lr, cpsr, slot->caller_module,
               slot->caller_offset);
        fflush(stdout);
    }

    if (stage == HELPER_ABI_STAGE_ROBOTOL_ENTER)
        ext_module_data_init_on_mr_helper(helper_address, slot->r1, slot->r0, slot->r9);

    if (stage == HELPER_ABI_STAGE_CALL_SITE_BEFORE && uc_for_caller_window &&
        !g_obs.call_site_window_dumped) {
        g_obs.call_site_window_dumped = 1;
        dump_caller_window(uc_for_caller_window, pc, thumb);
        log_dsm_entry_select(uc_for_caller_window, slot, target);
    }
}

static void print_helper_abi_summary(void) {
    const char *zero_at = "unknown";
    uint32_t cs = g_obs.call_site.valid ? g_obs.call_site.r0 : 0xFFFFFFFFu;
    uint32_t th = g_obs.thunk.valid ? g_obs.thunk.r0 : 0xFFFFFFFFu;
    uint32_t en = g_obs.enter.valid ? g_obs.enter.r0 : 0xFFFFFFFFu;

    if (g_obs.call_site.valid && g_obs.call_site.r0 == 0)
        zero_at = "CALL_SITE_BEFORE";
    else if (g_obs.thunk.valid && g_obs.thunk.r0 == 0 &&
             (!g_obs.call_site.valid || g_obs.call_site.r0 != 0))
        zero_at = "THUNK_ENTER";
    else if (g_obs.enter.valid && g_obs.enter.r0 == 0 &&
             (!g_obs.thunk.valid || g_obs.thunk.r0 != 0) &&
             (!g_obs.call_site.valid || g_obs.call_site.r0 != 0))
        zero_at = "ROBOTOL_ENTER";
    else if (g_obs.enter.valid && g_obs.enter.r0 == 0)
        zero_at = "ROBOTOL_ENTER_or_earlier";

    printf("[HELPER_ABI_SUMMARY] call_site_r0=0x%X thunk_r0=0x%X enter_r0=0x%X "
           "r0_became_zero_at=%s thunk_drop=%s call_site_origin=%s\n",
           g_obs.call_site.valid ? cs : 0, g_obs.thunk.valid ? th : 0,
           g_obs.enter.valid ? en : 0, zero_at,
           (g_obs.call_site.valid && g_obs.call_site.r0 != 0 && g_obs.enter.valid &&
            g_obs.enter.r0 == 0 && strcmp(g_obs.call_site.origin, "LR_PROXY") != 0)
               ? "true"
               : "false",
           g_obs.call_site.valid ? g_obs.call_site.origin : "none");
    {
        const char *e = getenv("JJFB_DSM_CFUNCTION_HELPER_ABI_AUDIT");
        if (e && e[0] == '1') {
            printf("[JJFB_HELPER_ARG_FLOW] call_site_r0=0x%X thunk_r0=0x%X enter_r0=0x%X "
                   "lr_proxy=%s r0_zero_at=%s evidence=TARGET_OBSERVED\n",
                   g_obs.call_site.valid ? cs : 0, g_obs.thunk.valid ? th : 0,
                   g_obs.enter.valid ? en : 0,
                   (g_obs.call_site.valid &&
                    strcmp(g_obs.call_site.origin, "LR_PROXY") == 0)
                       ? "yes"
                       : "no",
                   zero_at);
        }
    }
    fflush(stdout);
}

void ext_entry_observe_on_entry_begin(uint64_t module_id,
                                      uint32_t entry_pc,
                                      uint32_t method,
                                      uint32_t r0,
                                      uint32_t r1,
                                      uint32_t r2,
                                      uint32_t r3,
                                      uint32_t r9,
                                      uint32_t sp,
                                      uint32_t lr,
                                      uint32_t cpsr) {
    ExtLoader *loader = gwy_ext_loader_ensure();
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m;
    ExtLoadSession *s;
    LauncherError err;
    uint32_t regs[16];
    const char *origin = "GUEST_NESTED";

    if (!reg || module_id == 0) return;
    m = module_registry_find_by_id(reg, module_id);
    if (!m) return;

    /* Guest GCO never calls this for DSM; host bridge still marks DSM ENTRY_CALLED. */

    if (m->state == GWY_MODULE_REGISTERED) {
        module_registry_set_entry_called(reg, module_id, method, 0, &err);
    } else if (m->state != GWY_MODULE_ENTRY_CALLED) {
        return;
    }

    memset(&g_obs.entry, 0, sizeof(g_obs.entry));
    g_obs.entry.valid = 1;
    g_obs.entry.module_id = module_id;
    s = ext_loader_find_session_by_module(loader, module_id);
    g_obs.entry.load_id = s ? ext_loader_session_context(s)->load_id : 0;
    g_obs.entry.helper_address = m->map.helper_address;
    g_obs.entry.entry_pc = entry_pc;
    g_obs.entry.method = method;
    g_obs.entry.input_address = r2;
    g_obs.entry.input_length = r3;
    g_obs.entry.er_rw = m->map.guest_rw_base ? m->map.guest_rw_base : r9;
    g_obs.entry.stack_pointer = sp;
    g_obs.entry.caller_pc = lr;
    g_obs.entry.caller_module_id = caller_module_id_from_lr(reg, lr);
    ext_entry_observe_format_caller(lr, g_obs.entry.caller_module, sizeof(g_obs.entry.caller_module),
                                    &g_obs.entry.caller_offset, &g_obs.entry.caller_module_id);
    g_obs.entry.r0 = r0;
    g_obs.entry.r1 = r1;
    g_obs.entry.r2 = r2;
    g_obs.entry.r3 = r3;
    g_obs.entry.r9 = r9;

    module_registry_set_observed_first_pc(reg, module_id, entry_pc, &err);
    m = module_registry_find_by_id(reg, module_id);

    g_obs.tracing = 1;
    g_obs.trace_module_id = module_id;
    g_obs.entry_marked = 1;
    g_obs.ring_count = 0;
    g_obs.ring_next = 0;

    memset(regs, 0, sizeof(regs));
    regs[0] = r0;
    regs[1] = r1;
    regs[2] = r2;
    regs[3] = r3;
    regs[9] = r9;
    regs[13] = sp;
    regs[14] = lr;
    regs[15] = entry_pc;

    if (method != 0 && lr == 0) origin = "HOST_BRIDGE";

    printf("[EXT_ENTRY_CTX] module_id=%llu load_id=%llu helper=0x%X entry_pc=0x%X "
           "module_offset=0x%X method=%u r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X "
           "er_rw=0x%X sp=0x%X caller_pc=0x%X caller_module=%s caller_offset=0x%X "
           "caller_module_id=%llu header_entry_candidate=0x%X chunk_field_04=0x%X "
           "registered_helper=0x%X entry_class=%s\n",
           (unsigned long long)module_id, (unsigned long long)g_obs.entry.load_id,
           g_obs.entry.helper_address, entry_pc,
           m->map.guest_code_base ? (entry_pc - m->map.guest_code_base) : 0, method, r0, r1, r2, r3,
           r9, g_obs.entry.er_rw, sp, lr, g_obs.entry.caller_module, g_obs.entry.caller_offset,
           (unsigned long long)g_obs.entry.caller_module_id, m->entries.header_entry_candidate,
           m->entries.chunk_field_04, m->entries.registered_helper,
           ext_entry_observe_entry_class(module_id, entry_pc));
    {
        const char *e = getenv("JJFB_DSM_CFUNCTION_HELPER_ABI_AUDIT");
        const char *ecl = ext_entry_observe_entry_class(module_id, entry_pc);
        const char *route = "module_entry";
        if (e && e[0] == '1') {
            if (origin && strcmp(origin, "HOST_BRIDGE") == 0)
                route = (strcmp(ecl, "LEGAL_HELPER_EVENT_WITH_P") == 0) ? "mr_table_helper"
                                                                       : "wrong_helper_entry";
            else if (strcmp(ecl, "WRONG_HELPER_CALL_MISSING_P") == 0)
                route = "wrong_helper_entry";
            else if (strcmp(ecl, "LEGAL_HEADER_ENTRY_NULL_R0") == 0)
                route = "module_entry";
            printf("[JJFB_HELPER_ABI_TRACE] module=%s helper=0x%X kind=%s header_entry=0x%X "
                   "chunk_field_04=0x%X origin=%s evidence=TARGET_OBSERVED\n",
                   module_display_name(m), g_obs.entry.helper_address, ecl,
                   m->entries.header_entry_candidate, m->entries.chunk_field_04,
                   origin ? origin : "?");
            printf("[JJFB_HELPER_CALL_ROUTE] route=%s module=%s entry_pc=0x%X method=%u "
                   "evidence=DOCUMENTED\n",
                   route, module_display_name(m), entry_pc, method);
            printf("[JJFB_HELPER_ARG_FLOW] call_site_r0=pending thunk_r0=pending enter_r0=0x%X "
                   "lr_proxy=%s method=%u evidence=TARGET_OBSERVED\n",
                   r0, (lr == 0 && method != 0) ? "yes" : "no", method);
            fflush(stdout);
        }
    }
    fflush(stdout);

    {
        ExtAddrClassResult dcls;
        uint32_t ep = gwy_guest_pc_norm(entry_pc);
        ext_object_classify_address(ep, sp, &dcls);
        printf("[ENTRY_TRANSFER] dispatch_target=0x%X dispatch_module=%s dispatch_offset=0x%X "
               "callee_module=%s first_executed_pc=0x%X r0=0x%X stage=FIRST_EXECUTED_PC "
               "nested_enter=1\n",
               ep, dcls.module_name, dcls.module_offset, dcls.module_name, ep, r0);
        fflush(stdout);
    }

    {
        GwyLoadedModule *mut = NULL;
        size_t mi;
        for (mi = 0; mi < reg->count; mi++) {
            if (reg->modules[mi].module_id == module_id) {
                mut = &reg->modules[mi];
                break;
            }
        }
        if (mut) {
            GwyEntryRelation rel = classify_entry_relation(mut, entry_pc);
            emit_entry_reconcile(mut, "unknown", rel);
        }
    }

    {
        const char *reject = NULL;
        int in_range = pc_in_module_image(m, entry_pc);
        int robotol_ok = is_true_robotol_enter(reg, m, entry_pc, &reject);

        emit_module_identity(reg, m, entry_pc, in_range);

        if (robotol_ok) {
            ext_entry_observe_bootstrap_event("ROBOTOL_ENTER");
            printf("[JJFB_ROBOTOL_ENTRY_CALLED] package=%s module=robotol.ext module_id=%llu "
                   "pc=0x%X image_base=0x%X evidence=OBSERVED\n",
                   module_package_guest(reg, m), (unsigned long long)module_id, entry_pc,
                   m->map.guest_code_base);
            {
                const char *mt = getenv("JJFB_MRC_INIT_TRACE");
                if (mt && mt[0] == '1') {
                    /* GCO first-PC method is always 0; real mrc_init is helper code=0 with P. */
                    printf("[JJFB_MRC_INIT_ATTEMPT] module=robotol.ext pc=0x%X method=%u r0=0x%X "
                           "r9=0x%X route=%s evidence=OBSERVED "
                           "note=first_pc_not_proof_of_mrc_init\n",
                           entry_pc, method, r0, r9,
                           (r0 && method == 0) ? "helper_code0_candidate" : "bootstrap_first_pc");
                    fflush(stdout);
                }
            }
            fflush(stdout);
        } else if (m->origin == MODULE_ORIGIN_MRP_MEMBER || m->origin == MODULE_ORIGIN_DSM) {
            if (m->origin == MODULE_ORIGIN_MRP_MEMBER)
                ext_entry_observe_bootstrap_event("MODULE_ENTER");
            printf("[JJFB_ROBOTOL_ENTER_REJECT] reason=%s module=%s package=%s pc=0x%X "
                   "module_id=%llu evidence=OBSERVED\n",
                   reject ? reject : "not_robotol_module", module_display_name(m),
                   module_package_guest(reg, m), entry_pc, (unsigned long long)module_id);
            fflush(stdout);
        }

        ext_entry_observe_helper_abi(HELPER_ABI_STAGE_ROBOTOL_ENTER, module_id, entry_pc,
                                     m->map.helper_address, entry_pc, regs, cpsr, origin, NULL);
    }
}

void ext_entry_observe_push_insn(uint64_t module_id,
                                 uint32_t pc,
                                 const uint32_t regs[16],
                                 uint32_t cpsr) {
    GuestInstructionTrace *t;
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    uint32_t base = 0;

    if (!g_obs.tracing || module_id != g_obs.trace_module_id || !regs) return;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    if (m) base = m->map.guest_code_base;

    t = &g_obs.ring[g_obs.ring_next];
    memset(t, 0, sizeof(*t));
    t->pc = pc;
    t->module_offset = (base && pc >= base) ? (pc - base) : 0;
    memcpy(t->regs, regs, sizeof(t->regs));
    t->cpsr = cpsr;
    g_obs.ring_next = (g_obs.ring_next + 1) % GWY_ENTRY_TRACE_RING;
    if (g_obs.ring_count < GWY_ENTRY_TRACE_RING) g_obs.ring_count++;
}

static void dump_ring(void) {
    size_t i, start, n;
    printf("[EXT_FAULT_RING] count=%zu\n", g_obs.ring_count);
    if (g_obs.ring_count == 0) {
        fflush(stdout);
        return;
    }
    n = g_obs.ring_count;
    start = (g_obs.ring_next + GWY_ENTRY_TRACE_RING - n) % GWY_ENTRY_TRACE_RING;
    for (i = 0; i < n; i++) {
        size_t idx = (start + i) % GWY_ENTRY_TRACE_RING;
        const GuestInstructionTrace *t = &g_obs.ring[idx];
        printf("[EXT_FAULT_RING] i=%zu pc=0x%X off=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
               "r9=0x%X sp=0x%X lr=0x%X cpsr=0x%X\n",
               i, t->pc, t->module_offset, t->regs[0], t->regs[1], t->regs[2], t->regs[3],
               t->regs[9], t->regs[13], t->regs[14], t->cpsr);
    }
    fflush(stdout);
}

static ExtFaultRootCause classify_fault(const GuestFaultReport *f, const ExtEntryCallContext *e) {
    if (!f || !f->valid) return EXT_FAULT_CLASS_UNKNOWN;
    if (e && e->valid && e->helper_address && (e->entry_pc & ~1u) != (e->helper_address & ~1u) &&
        f->base_reg_index >= 0 && f->base_reg_value == 0) {
        /* entry≠helper + null base — possible wrong-entry, keep ENTRY_ARGUMENT primary */
    }
    if (f->base_reg_index >= 0 && f->base_reg_index <= 3 && f->base_reg_value == 0) {
        return EXT_FAULT_CLASS_ENTRY_ARGUMENT;
    }
    if (f->base_reg_index == 9 && f->base_reg_value == 0) {
        return EXT_FAULT_CLASS_ER_RW_INIT;
    }
    if (e && e->valid && e->er_rw == 0) {
        return EXT_FAULT_CLASS_ER_RW_INIT;
    }
    if (f->base_reg_index >= 0 && f->base_reg_value == 0) {
        return EXT_FAULT_CLASS_PLATFORM_CONTEXT;
    }
    return EXT_FAULT_CLASS_OTHER;
}

static void disasm_window(void *uc, GuestFaultReport *f) {
    uint32_t pc = f->fault_pc & ~1u;
    int thumb = (f->cpsr & (1u << 5)) != 0 || (f->fault_pc & 1u) != 0;
    int i;
    int matched = 0;

    printf("[EXT_FAULT_DISASM] window_pc=0x%X thumb=%d\n", pc, thumb);
    for (i = -20; i <= 20; i++) {
        uint32_t addr;
        uint32_t word = 0;
        int rt = -1, rn = -1, is_load = 0;
        uint32_t imm = 0;
        char line[128];

        if (thumb) {
            addr = pc + (uint32_t)(i * 2);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr & ~3u, &word)) continue;
            {
                uint32_t half = (addr & 2u) ? (word >> 16) : (word & 0xFFFFu);
                if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load)) {
                    snprintf(line, sizeof(line), "%s r%d, [r%d, #0x%X]", is_load ? "LDR" : "STR",
                             rt, rn, imm);
                } else {
                    snprintf(line, sizeof(line), ".hword 0x%04X", half & 0xFFFF);
                }
                word = half;
            }
        } else {
            addr = pc + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load)) {
                int U = (word >> 23) & 1;
                snprintf(line, sizeof(line), "%s r%d, [r%d, #%s0x%X]", is_load ? "LDR" : "STR", rt,
                         rn, U ? "" : "-", imm);
            } else {
                snprintf(line, sizeof(line), ".word 0x%08X", word);
            }
        }

        printf("[EXT_FAULT_DISASM] %c off=%+d addr=0x%X raw=0x%08X %s\n", i == 0 ? '*' : ' ', i,
               addr, word, line);

        if (i == 0 && rn >= 0 && rn < 16) {
            uint32_t base_val = f->regs[rn];
            uint32_t eff = base_val + imm;
            matched = 1;
            f->base_reg_index = rn;
            f->base_reg_value = base_val;
            f->imm_offset = imm;
            snprintf(f->fault_instruction, sizeof(f->fault_instruction), "%s", line);
            printf("[EXT_FAULT_INSN] fault_instruction=%s base_register=r%d base_value=0x%X "
                   "imm=0x%X effective_address=0x%X dest_or_src=r%d\n",
                   line, rn, base_val, imm, eff, rt);
        }
    }
    if (!matched) {
        snprintf(f->fault_instruction, sizeof(f->fault_instruction), "unknown");
        f->base_reg_index = -1;
        printf("[EXT_FAULT_INSN] fault_instruction=unknown (could not decode LDR at PC)\n");
    }
    fflush(stdout);
}

void ext_entry_observe_on_mem_fault(void *uc,
                                    uint32_t access_type,
                                    uint64_t address,
                                    uint32_t size,
                                    int64_t value) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    ExtLoader *loader = gwy_ext_loader_ensure();
    const GwyLoadedModule *m = NULL;
    ExtLoadSession *s;
    LauncherError err;
    GuestFaultReport *f = &g_obs.fault;
    uint32_t regs[16];
    uint32_t cpsr = 0;
    GwyModuleFailInfo fi;
    char caller_mod[96];
    uint32_t caller_off = 0;
    uint64_t caller_mid = 0;
    (void)value;

    memset(f, 0, sizeof(*f));
    f->valid = 1;
    f->access_type = access_type;
    f->invalid_address = (uint32_t)address;
    f->access_size = size;
    fill_regs_from_uc(uc, regs, &cpsr);
    memcpy(f->regs, regs, sizeof(regs));
    f->fault_pc = regs[15];
    f->fault_lr = regs[14];
    f->fault_sp = regs[13];
    f->cpsr = cpsr;
    snprintf(f->fail_stage, sizeof(f->fail_stage), "ENTRY_EXECUTION");
    /* D2: classify R9 scope before heavy bootstrap dumps (short live runs may be killed). */
    ext_r9_scope_audit_on_mem_fault(uc, f->fault_pc, (uint32_t)address, regs, cpsr);
    ext_callback_frame_on_mem_fault(uc, f->fault_pc, (uint32_t)address, size, regs, cpsr);
    ext_post_cont_audit_on_mem_fault(uc, f->fault_pc, (uint32_t)address, regs, cpsr);
    ext_post_cfn_r9_audit_on_mem_fault(uc, f->fault_pc, (uint32_t)address, regs, cpsr);
    ext_p_extchunk_audit_on_mem_fault(uc, f->fault_pc, (uint32_t)address, regs, cpsr);
    ext_gwy_startgame_audit_on_mem_fault(uc, f->fault_pc, (uint32_t)address, regs, cpsr);
    module_r9_switch_abort(uc, "UC_FAULT");
    ext_module_entry_abi_on_mem_fault(f->fault_pc, (uint32_t)address);
    ext_entry_null_contract_on_mem_fault(f->fault_pc, (uint32_t)address, regs[0], regs[9]);
    ext_module_data_init_on_mem_fault(f->fault_pc, (uint32_t)address, regs[0], regs[9]);
    ext_er_rw_producer_on_mem_fault(f->fault_pc, (uint32_t)address, regs[0], regs[9]);
    ext_bootstrap_abi_on_mem_fault(uc, f->fault_pc, (uint32_t)address, size, regs, cpsr);

    if (reg) {
        m = module_registry_find_by_code_addr(reg, f->fault_pc & ~1u);
    }
    if (m) {
        f->module_id = m->module_id;
        f->code_base = m->map.guest_code_base;
        f->code_size = m->map.guest_code_size;
        f->module_offset =
            (f->code_base && (f->fault_pc & ~1u) >= f->code_base)
                ? ((f->fault_pc & ~1u) - f->code_base)
                : 0;
        s = ext_loader_find_session_by_module(loader, m->module_id);
        f->load_id = s ? ext_loader_session_context(s)->load_id : 0;

        if (m->state == GWY_MODULE_REGISTERED) {
            module_registry_set_entry_called(reg, m->module_id,
                                             g_obs.entry.valid ? g_obs.entry.method : 0, 0, &err);
        }
    }

    ext_entry_observe_format_caller(f->fault_lr, caller_mod, sizeof(caller_mod), &caller_off,
                                    &caller_mid);

    printf("[EXT_FAULT] access_type=%u invalid_address=0x%X access_size=%u "
           "fault_pc=0x%X module_offset=0x%X module_id=%llu load_id=%llu "
           "code_base=0x%X code_size=%u lr=0x%X sp=0x%X cpsr=0x%X "
           "caller_module=%s caller_offset=0x%X\n",
           access_type, f->invalid_address, f->access_size, f->fault_pc, f->module_offset,
           (unsigned long long)f->module_id, (unsigned long long)f->load_id, f->code_base,
           f->code_size, f->fault_lr, f->fault_sp, f->cpsr, caller_mod, caller_off);
    printf("[EXT_FAULT_REGS] r0=0x%X r1=0x%X r2=0x%X r3=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X "
           "r8=0x%X r9=0x%X r10=0x%X r11=0x%X r12=0x%X sp=0x%X lr=0x%X pc=0x%X\n",
           regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7], regs[8],
           regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15]);
    fflush(stdout);

    /* Emit lifecycle-critical tags before heavy dumps (short live runs may be killed). */
    if (f->invalid_address == 0x28u && regs[0] == 0) {
        f->base_reg_index = 0;
        f->base_reg_value = 0;
        f->imm_offset = 0x28u;
        snprintf(f->fault_instruction, sizeof(f->fault_instruction), "LDR ?, [r0, #0x28]");
    }
    print_helper_abi_summary();
    f->root_cause = classify_fault(f, g_obs.entry.valid ? &g_obs.entry : NULL);
    printf("[EXT_FAULT_CLASS] root_cause=%s\n", ext_fault_root_cause_name(f->root_cause));
    if (m) {
        const char *ecl =
            ext_entry_observe_entry_class(m->module_id, g_obs.entry.valid ? g_obs.entry.entry_pc
                                                                          : f->fault_pc);
        const char *primary = "D";
        const char *recon_case = "F";
        GwyEntryRelation rel = m->entries.relation;
        if (strcmp(ecl, "WRONG_ENTRY_SELECTION") == 0) primary = "A";
        else if (strcmp(ecl, "WRONG_HELPER_CALL_MISSING_P") == 0) primary = "B";
        else if (strcmp(ecl, "LEGAL_HELPER_EVENT_WITH_P") == 0) primary = "C";
        else if (strcmp(ecl, "LEGAL_HEADER_ENTRY_NULL_R0") == 0 ||
                 strcmp(ecl, "CHUNK_INIT_ENTRY") == 0)
            primary = "C";
        if (g_obs.entry.valid && reg) {
            GwyLoadedModule *mut = NULL;
            size_t mi;
            for (mi = 0; mi < reg->count; mi++) {
                if (reg->modules[mi].module_id == m->module_id) {
                    mut = &reg->modules[mi];
                    break;
                }
            }
            if (mut) {
                rel = classify_entry_relation(mut, g_obs.entry.entry_pc);
                mut->entries.relation = rel;
            }
        }
        if (rel == GWY_ENTRY_REL_THUMB_NORMALIZED_MATCH || rel == GWY_ENTRY_REL_DIRECT_MATCH)
            recon_case = "A";
        else if (rel == GWY_ENTRY_REL_TRAMPOLINE_TO_ENTRY ||
                 rel == GWY_ENTRY_REL_TRAMPOLINE_TO_HELPER)
            recon_case = "B";
        else if (rel == GWY_ENTRY_REL_HEADER_ENTRY_WRONG)
            recon_case = "C";
        else if (rel == GWY_ENTRY_REL_CHUNK_ENTRY_CORRUPT)
            recon_case = "D";
        else if (!g_obs.chunk_field_04_write_seen)
            recon_case = "E";
        else
            recon_case = "F";
        printf("[BOOTSTRAP_CLASSIFY] entry_class=%s primary=%s "
               "field28_writer=%s p_candidate=0x%X chunk_candidate=0x%X "
               "bootstrap_seq=%s "
               "note_v49=HYPOTHESIS_only_not_copied "
               "phase6b_gate=no_context_create\n",
               ecl, primary, g_obs.field28_write_seen ? "SEEN" : "NONE_BEFORE_FAULT",
               g_obs.p_candidate, g_obs.chunk_candidate,
               g_obs.bootstrap_seq[0] ? g_obs.bootstrap_seq : "empty");
        printf("[ENTRY_RECONCILE_CASE] case=%s relation=%s chunk_field_04_writer=%s "
               "module=%s "
               "header_ev=DOCUMENTED_image_plus8_fixR9_TestCom800 "
               "chunk_plus4=DOCUMENTED_mrc_extChunk_field_04 "
               "fix_applied=none phase6b_b_gate=blocked\n",
               recon_case, gwy_entry_relation_name(rel),
               g_obs.chunk_field_04_write_seen ? "SEEN" : "NONE_BEFORE_SELECT",
               module_display_name(m));
    }
    fflush(stdout);
    ext_entry_observe_bootstrap_event("FAULT");

    if (m && reg &&
        (m->state == GWY_MODULE_ENTRY_CALLED || m->state == GWY_MODULE_REGISTERED)) {
        memset(&fi, 0, sizeof(fi));
        snprintf(fi.fail_stage, sizeof(fi.fail_stage), "ENTRY_EXECUTION");
        snprintf(fi.fail_access, sizeof(fi.fail_access), "UC_MEM_READ_UNMAPPED");
        fi.fault_pc = f->fault_pc;
        fi.module_offset = f->module_offset;
        fi.invalid_address = f->invalid_address;
        fi.access_size = f->access_size;
        fi.method = g_obs.entry.valid ? g_obs.entry.method : 0;
        fi.caller_pc = g_obs.entry.valid ? g_obs.entry.caller_pc : f->fault_lr;
        fi.base_reg_index = f->base_reg_index;
        fi.base_reg_value = f->base_reg_value;
        module_registry_set_entry_failed(reg, m->module_id, &fi, &err);
    }

    if (uc) {
        disasm_window(uc, f);
        if (f->imm_offset == 0x28 || f->invalid_address == 0x28) analyze_field28_forward(uc, f);
    }
    dump_ring();

    g_obs.tracing = 0;
}
