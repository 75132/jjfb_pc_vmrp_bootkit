#include "gwy_launcher/ext_object_observe.h"
#include "gwy_launcher/ext_chunk_observe.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_helper_handoff.h"
#include "gwy_launcher/ext_dsm_record_observe.h"
#include "gwy_launcher/ext_module_entry_abi.h"
#include "gwy_launcher/ext_entry_null_contract.h"
#include "gwy_launcher/ext_module_data_init.h"
#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/ext_r9_scope_audit.h"
#include "gwy_launcher/ext_callback_frame.h"
#include "gwy_launcher/ext_cfunction_publication_audit.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define GWY_PTR_WRITE_RING 32
#define GWY_DISPATCH_PROTO_INSNS 40

typedef struct {
    uint32_t value;
    uint32_t dest;
    uint64_t writer_mid;
    uint32_t writer_pc;
    uint64_t target_mid;
    char target_name[96];
    char relation[24];
} ExtModulePtrWrite;

typedef struct {
    int active;
    uint32_t transfer_id;
    uint64_t loader_mid;
    uint32_t enter_pc;
    uint32_t return_lr;
    uint32_t enter_r0;
    uint32_t last_r0;
    int writes_to_robotol_code;
    int registered_helper_seen;
    int proto_emitted;
    int exit_emitted;
} ExtDispatchSession;

typedef struct {
    ExtCallTransfer last_xfer;
    ExtEntryObjectTrace last_trace;
    int identity_case_emitted;
    int cross_logged_mask; /* bit0 DSM->loader, bit1 loader->robotol approx */
    uint32_t next_transfer_id;
    ExtDispatchSession session;
    int dispatch_exit_seen; /* EXIT for transfer_id=1 logged */
    int hop_case_emitted;
    ExtModulePtrWrite ptr_writes[GWY_PTR_WRITE_RING];
    int ptr_write_count;
    int ptr_write_none_logged;
} ExtObjectObserveState;

static ExtObjectObserveState g_obj;

void ext_object_observe_reset(void) {
    memset(&g_obj, 0, sizeof(g_obj));
}

int ext_object_identity_enabled(void) {
    const char *env = getenv("GWY_OBJECT_IDENTITY");
    return env && env[0] == '1';
}

int ext_object_dispatch_trace_enabled(void) {
    const char *env = getenv("GWY_DISPATCH_TRACE");
    return env && env[0] == '1';
}

const char *ext_object_kind_name(ExtObjectKind k) {
    switch (k) {
        case EXT_OBJECT_C_FUNCTION_ST: return "C_FUNCTION_ST";
        case EXT_OBJECT_MRC_EXT_CHUNK: return "MRC_EXT_CHUNK";
        case EXT_OBJECT_MODULE_DESCRIPTOR: return "MODULE_DESCRIPTOR";
        case EXT_OBJECT_LOADER_RECORD: return "LOADER_RECORD";
        case EXT_OBJECT_FUNCTION_TABLE: return "FUNCTION_TABLE";
        case EXT_OBJECT_OTHER: return "OTHER";
        default: return "UNKNOWN";
    }
}

const char *guest_address_class_name(GuestAddressClass c) {
    switch (c) {
        case ADDR_DSM_CODE: return "DSM_CODE";
        case ADDR_DSM_RW: return "DSM_RW";
        case ADDR_MODULE_CODE: return "MODULE_CODE";
        case ADDR_MODULE_RW: return "MODULE_RW";
        case ADDR_HEAP: return "HEAP";
        case ADDR_STACK: return "STACK";
        case ADDR_PLATFORM_TABLE: return "PLATFORM_TABLE";
        default: return "UNKNOWN";
    }
}

const char *ext_identity_confidence_name(ExtIdentityConfidence c) {
    switch (c) {
        case EXT_IDENTITY_CANDIDATE: return "CANDIDATE";
        case EXT_IDENTITY_PROBABLE: return "PROBABLE";
        case EXT_IDENTITY_CONFIRMED: return "CONFIRMED";
        default: return "UNKNOWN";
    }
}

const char *ext_transfer_hop_kind_name(ExtTransferHopKind k) {
    switch (k) {
        case EXT_HOP_DSM_TO_LOADER: return "DSM_TO_LOADER";
        case EXT_HOP_DSM_TO_ROBOTOL: return "DSM_TO_ROBOTOL";
        default: return "OTHER";
    }
}

const char *ext_arg_role_name(ExtArgRole r) {
    switch (r) {
        case EXT_ARG_COMMAND: return "COMMAND";
        case EXT_ARG_CONTEXT: return "CONTEXT";
        case EXT_ARG_BUFFER: return "BUFFER";
        case EXT_ARG_LENGTH: return "LENGTH";
        default: return "UNKNOWN";
    }
}

static const char *module_display(const GwyLoadedModule *m) {
    if (!m) return "unknown";
    if (m->origin == MODULE_ORIGIN_DSM) return "dsm:cfunction.ext";
    if (m->resolved_name[0]) return m->resolved_name;
    if (m->requested_name[0]) return m->requested_name;
    return "unknown";
}

void ext_object_classify_address(uint32_t va, uint32_t sp_hint, ExtAddrClassResult *out) {
    ModuleRegistry *reg;
    size_t i;
    uint32_t a;

    if (out) memset(out, 0, sizeof(*out));
    if (!out) return;
    out->addr_class = ADDR_UNKNOWN;
    a = va & ~1u;
    if (!a) return;

    if (sp_hint && a >= (sp_hint & ~0xFFFu) && a < sp_hint + 0x10000u) {
        out->addr_class = ADDR_STACK;
        snprintf(out->module_name, sizeof(out->module_name), "%s", "stack");
        return;
    }

    reg = gwy_ext_loader_bound_registry();
    if (!reg) return;

    for (i = 0; i < reg->count; i++) {
        const GwyLoadedModule *m = &reg->modules[i];
        uint32_t cb = m->map.guest_code_base;
        uint32_t cs = m->map.guest_code_size;
        uint32_t rb = m->map.guest_rw_base;
        uint32_t rs = m->map.guest_rw_size;
        if (cb && cs && a >= cb && a < cb + cs) {
            out->module_id = m->module_id;
            out->module_offset = a - cb;
            snprintf(out->module_name, sizeof(out->module_name), "%s", module_display(m));
            out->addr_class = (m->origin == MODULE_ORIGIN_DSM) ? ADDR_DSM_CODE : ADDR_MODULE_CODE;
            return;
        }
        if (rb && rs && a >= rb && a < rb + rs) {
            out->module_id = m->module_id;
            out->module_offset = a - rb;
            snprintf(out->module_name, sizeof(out->module_name), "%s", module_display(m));
            out->addr_class = (m->origin == MODULE_ORIGIN_DSM) ? ADDR_DSM_RW : ADDR_MODULE_RW;
            return;
        }
    }

    /* Heuristic heap: high guest VA outside mapped modules. */
    if (a >= 0x10000u && a < 0x400000u) {
        out->addr_class = ADDR_HEAP;
        snprintf(out->module_name, sizeof(out->module_name), "%s", "heap");
    }
}

void ext_object_log_addr_class(const char *role, uint32_t va, uint32_t sp_hint) {
    ExtAddrClassResult r;
    ext_object_classify_address(va, sp_hint, &r);
    printf("[ADDR_CLASS] role=%s va=0x%X class=%s module=%s offset=0x%X module_id=%llu\n",
           role ? role : "?", va, guest_address_class_name(r.addr_class), r.module_name,
           r.module_offset, (unsigned long long)r.module_id);
    fflush(stdout);
}

static ExtTransferHopKind classify_hop(const GwyLoadedModule *from_m, const GwyLoadedModule *to_m,
                                       const char *to_name) {
    if (from_m && from_m->origin == MODULE_ORIGIN_DSM && to_m) {
        if (strstr(to_name, "mrc_loader")) return EXT_HOP_DSM_TO_LOADER;
        if (strstr(to_name, "robotol") || strstr(to_name, "cfunction"))
            return EXT_HOP_DSM_TO_ROBOTOL;
    }
    return EXT_HOP_OTHER;
}

static void record_ptr_write(uint32_t value,
                             uint32_t dest,
                             uint64_t writer_mid,
                             uint32_t writer_pc,
                             uint64_t target_mid,
                             const char *target_name,
                             const char *relation) {
    ExtModulePtrWrite *w;
    int idx = g_obj.ptr_write_count % GWY_PTR_WRITE_RING;
    w = &g_obj.ptr_writes[idx];
    memset(w, 0, sizeof(*w));
    w->value = value;
    w->dest = dest;
    w->writer_mid = writer_mid;
    w->writer_pc = writer_pc;
    w->target_mid = target_mid;
    snprintf(w->target_name, sizeof(w->target_name), "%s", target_name ? target_name : "?");
    snprintf(w->relation, sizeof(w->relation), "%s", relation ? relation : "unknown");
    g_obj.ptr_write_count++;
}

static const char *match_target_source(uint32_t target, uint32_t return_r0, uint32_t helper) {
    uint32_t t = target & ~1u;
    int i;
    int n = g_obj.ptr_write_count;
    if (n > GWY_PTR_WRITE_RING) n = GWY_PTR_WRITE_RING;
    for (i = 0; i < n; i++) {
        const ExtModulePtrWrite *w = &g_obj.ptr_writes[i];
        if ((w->value & ~1u) == t) return "global_slot";
        if ((w->dest & ~1u) == t) return "global_slot";
    }
    if (return_r0 && (return_r0 & ~1u) == t) return "dispatcher_return";
    if (helper && (helper & ~1u) == t) return "registration_table";
    return "unknown";
}

static const char *match_target_relation(const GwyLoadedModule *to_m, uint32_t target) {
    uint32_t t = target & ~1u;
    if (!to_m) return "unknown";
    if (to_m->entries.registered_helper &&
        (to_m->entries.registered_helper & ~1u) == t)
        return "helper";
    if (ext_callback_frame_is_cfn_continuation(t, NULL, NULL))
        return "guest_callback_continuation";
    if (to_m->entries.direct_target_relation ==
        GWY_ENTRY_REL_CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW)
        return "guest_callback_continuation";
    if (to_m->entries.observed_first_pc && (to_m->entries.observed_first_pc & ~1u) == t)
        return "internal_entry";
    if (to_m->map.guest_code_base && t >= to_m->map.guest_code_base &&
        t < to_m->map.guest_code_base + to_m->map.guest_code_size)
        return "trampoline";
    return "unknown";
}

static void emit_dispatch_exit(uint32_t return_r0, const char *return_to_module) {
    ExtDispatchSession *s = &g_obj.session;
    if (!s->active || s->exit_emitted) return;
    s->exit_emitted = 1;
    s->active = 0;
    if (s->transfer_id == 1) g_obj.dispatch_exit_seen = 1;
    printf("[DISPATCH_CALL] stage=EXIT transfer_id=%u return_r0=0x%X return_to_module=%s "
           "writes_to_robotol_code=%d registered_helper_seen=%d\n",
           s->transfer_id, return_r0, return_to_module ? return_to_module : "dsm:?",
           s->writes_to_robotol_code, s->registered_helper_seen);
    fflush(stdout);
    if (!g_obj.ptr_write_count && !g_obj.ptr_write_none_logged) {
        g_obj.ptr_write_none_logged = 1;
        printf("[MODULE_POINTER_WRITE] NONE evidence=OBSERVED note=no_code_ptr_store_seen\n");
        fflush(stdout);
    }
}

static void analyze_dispatch_proto(void *uc, uint32_t entry_pc) {
    ExtArgRole roles[4] = {EXT_ARG_UNKNOWN, EXT_ARG_UNKNOWN, EXT_ARG_UNKNOWN, EXT_ARG_UNKNOWN};
    int i;
    int thumb = (entry_pc & 1u) != 0;
    uint32_t pc = entry_pc & ~1u;
    int saw_cmp_r0 = 0, saw_ldr_r0 = 0;

    if (!uc || !entry_pc) return;

    for (i = 0; i < GWY_DISPATCH_PROTO_INSNS; i++) {
        uint32_t word = 0;
        uint32_t addr;
        int rt = -1, rn = -1, is_load = 0;
        uint32_t imm = 0;
        if (thumb) {
            uint16_t half;
            addr = pc + (uint32_t)(i * 2);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr & ~3u, &word)) continue;
            half = (uint16_t)((addr & 2u) ? (word >> 16) : (word & 0xFFFFu));
            /* CMP Rn, #imm0: 0x2800 | (Rn<<8) | imm8 — CMP r0,#imm → 0x28xx */
            if ((half & 0xFF00u) == 0x2800u) {
                saw_cmp_r0 = 1;
                if (roles[0] == EXT_ARG_UNKNOWN) roles[0] = EXT_ARG_COMMAND;
            }
            /* TST r0,r0 style: 0x4200 | ... approximate ANDS/TST low regs */
            if ((half & 0xFFC0u) == 0x4000u && ((half >> 3) & 7) == 0) {
                saw_cmp_r0 = 1;
                if (roles[0] == EXT_ARG_UNKNOWN) roles[0] = EXT_ARG_COMMAND;
            }
            if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load &&
                rn == 0) {
                saw_ldr_r0 = 1;
                if (roles[0] == EXT_ARG_UNKNOWN) roles[0] = EXT_ARG_CONTEXT;
            }
            /* CMP r1,#imm → length-ish; LDR from r1 → buffer */
            if ((half & 0xFF00u) == 0x2900u && roles[1] == EXT_ARG_UNKNOWN)
                roles[1] = EXT_ARG_LENGTH;
            if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load &&
                rn == 1 && roles[1] == EXT_ARG_UNKNOWN)
                roles[1] = EXT_ARG_BUFFER;
        } else {
            addr = pc + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
            /* CMP r0, #imm: cond|001|1010|0|0000|0000|imm12 → 0xE3500xxx */
            if ((word & 0x0FF0F000u) == 0x03500000u) {
                saw_cmp_r0 = 1;
                if (roles[0] == EXT_ARG_UNKNOWN) roles[0] = EXT_ARG_COMMAND;
            }
            /* TST r0, ... */
            if ((word & 0x0FF0F000u) == 0x03100000u) {
                saw_cmp_r0 = 1;
                if (roles[0] == EXT_ARG_UNKNOWN) roles[0] = EXT_ARG_COMMAND;
            }
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load &&
                rn == 0) {
                saw_ldr_r0 = 1;
                if (roles[0] == EXT_ARG_UNKNOWN) roles[0] = EXT_ARG_CONTEXT;
            }
            if ((word & 0x0FF0F000u) == 0x03510000u && roles[1] == EXT_ARG_UNKNOWN)
                roles[1] = EXT_ARG_LENGTH;
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load &&
                rn == 1 && roles[1] == EXT_ARG_UNKNOWN)
                roles[1] = EXT_ARG_BUFFER;
        }
        (void)saw_cmp_r0;
        (void)saw_ldr_r0;
    }

    /* Prefer COMMAND over CONTEXT if both seen early (command dispatch first). */
    if (saw_cmp_r0 && saw_ldr_r0 && roles[0] == EXT_ARG_CONTEXT) roles[0] = EXT_ARG_COMMAND;

    printf("[DISPATCH_PROTO] r0_role=%s r1_role=%s r2_role=%s r3_role=%s evidence=OBSERVED\n",
           ext_arg_role_name(roles[0]), ext_arg_role_name(roles[1]), ext_arg_role_name(roles[2]),
           ext_arg_role_name(roles[3]));
    fflush(stdout);
    /* Comment-only prototype — not an executable API. */
    printf("[DISPATCH_PROTO] comment_proto=/* OBSERVED */ int dispatch(r0=%s, r1=%s, r2=%s, "
           "r3=%s);\n",
           ext_arg_role_name(roles[0]), ext_arg_role_name(roles[1]), ext_arg_role_name(roles[2]),
           ext_arg_role_name(roles[3]));
    fflush(stdout);
}

static void emit_r0_semantics(ExtTransferHopKind hop,
                              uint32_t r0,
                              ExtArgRole role,
                              const GwyLoadedModule *to_m) {
    const char *vs = "na";
    if (hop == EXT_HOP_DSM_TO_ROBOTOL && to_m) {
        uint32_t h = to_m->entries.registered_helper;
        if (!h)
            vs = "ne";
        else if ((r0 & ~1u) == (h & ~1u))
            vs = "eq";
        else
            vs = "ne";
    }
    printf("[R0_SEMANTICS] hop=%s r0=0x%X role=%s vs_registered_helper=%s evidence=OBSERVED\n",
           ext_transfer_hop_kind_name(hop), r0, ext_arg_role_name(role), vs);
    fflush(stdout);
}

static ExtArgRole infer_r0_role_quick(uint32_t r0, ExtTransferHopKind hop) {
    /*
     * A8: DSM→CODE_IMAGE entry hop must not assume COMMAND (that role is for
     * DSM→mrc_loader dispatcher). Keep UNKNOWN until MODULE_ENTRY_PROTO proves.
     */
    if (hop == EXT_HOP_DSM_TO_ROBOTOL) return EXT_ARG_UNKNOWN;
    if (r0 <= 0xFFu) return EXT_ARG_COMMAND;
    if (r0 >= 0x10000u) return EXT_ARG_CONTEXT;
    return EXT_ARG_UNKNOWN;
}

static void emit_dispatch_hop_case(void) {
    const char *case_id = "F";
    const ExtCallTransfer *xfer = g_obj.last_xfer.valid ? &g_obj.last_xfer : NULL;
    ExtDispatchSession *s = &g_obj.session;
    uint32_t helper = 0;
    uint32_t direct = 0;
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *rob = NULL;

    if (g_obj.hop_case_emitted) return;
    if (reg) {
        rob = module_registry_find(reg, "robotol.ext");
        if (!rob) rob = module_registry_find(reg, "cfunction.ext");
        if (rob) {
            helper = rob->entries.registered_helper;
            direct = rob->entries.dsm_direct_target;
        }
    }

    /*
     * A: dispatcher r0=command valid; second hop wrong robotol entry
     * B: dispatcher returns/writes helper; second hop is that helper
     * C: registration handoff mismatch
     * D: missing documented descriptor bootstrap
     * E: second hop is normal method dispatch
     * F: unknown
     */
    if (xfer && xfer->hop_kind == EXT_HOP_DSM_TO_ROBOTOL) {
        uint32_t t = xfer->caller_target & ~1u;
        if (xfer->r0 > 0x10000u && xfer->r0 != 0)
            case_id = "E";
        else if (helper && (t == (helper & ~1u)))
            case_id = "B";
        else if (direct && helper && (direct & ~1u) != (helper & ~1u))
            case_id = "C";
        else if (direct && helper && (direct & ~1u) != (helper & ~1u) &&
                 !g_obj.dispatch_exit_seen)
            case_id = "C";
        else if (xfer->r0 == 0 && helper && (t != (helper & ~1u)))
            case_id = "A";
        else if (!helper && !direct)
            case_id = "D";
        else
            case_id = "A";
    } else if (g_obj.cross_logged_mask & 1) {
        case_id = "F";
    }

    (void)s;
    if (!g_obj.ptr_write_count && !g_obj.ptr_write_none_logged) {
        g_obj.ptr_write_none_logged = 1;
        printf("[MODULE_POINTER_WRITE] NONE evidence=OBSERVED note=no_code_ptr_store_seen\n");
        fflush(stdout);
    }
    g_obj.hop_case_emitted = 1;
    printf("[DISPATCH_HOP_CASE] case=%s "
           "legend=A_cmd_ok_wrong_entry|B_helper_match|C_reg_mismatch|D_missing_desc|"
           "E_method_dispatch|F_unknown "
           "dispatch_exit_seen=%d dsm_direct=0x%X helper=0x%X phase6b_b_gate=blocked\n",
           case_id, g_obj.dispatch_exit_seen, direct, helper);
    fflush(stdout);
}

static void emit_dispatch_enter(void *uc,
                                uint32_t transfer_id,
                                const GwyLoadedModule *to_m,
                                uint32_t target,
                                const uint32_t *regs,
                                uint32_t cpsr,
                                uint32_t lr) {
    ExtDispatchSession *s = &g_obj.session;
    uint32_t to_off = 0;
    uint32_t tnorm = target & ~1u;
    uint32_t r0 = regs ? regs[0] : 0;
    uint32_t r1 = regs ? regs[1] : 0;
    uint32_t r2 = regs ? regs[2] : 0;
    uint32_t r3 = regs ? regs[3] : 0;
    char r4_12[160];

    if (to_m && to_m->map.guest_code_base && tnorm >= to_m->map.guest_code_base)
        to_off = tnorm - to_m->map.guest_code_base;

    /* Only arm session for first loader hop (transfer_id==1) or +0x34. */
    if (s->active && !s->exit_emitted)
        emit_dispatch_exit(s->last_r0, "dsm:cfunction.ext");

    memset(s, 0, sizeof(*s));
    s->active = 1;
    s->transfer_id = transfer_id;
    s->loader_mid = to_m ? to_m->module_id : 0;
    s->enter_pc = tnorm;
    s->return_lr = lr;
    s->enter_r0 = r0;
    s->last_r0 = r0;

    if (regs) {
        snprintf(r4_12, sizeof(r4_12),
                 "r4=0x%X r5=0x%X r6=0x%X r7=0x%X r8=0x%X r9=0x%X r10=0x%X r11=0x%X r12=0x%X",
                 regs[4], regs[5], regs[6], regs[7], regs[8], regs[9], regs[10], regs[11],
                 regs[12]);
        printf("[DISPATCH_CALL] stage=ENTER transfer_id=%u module=%s offset=0x%X "
               "r0=0x%X r1=0x%X r2=0x%X r3=0x%X %s sp=0x%X lr=0x%X cpsr=0x%X\n",
               transfer_id, module_display(to_m), to_off, r0, r1, r2, r3, r4_12, regs[13], lr,
               cpsr);
    } else {
        printf("[DISPATCH_CALL] stage=ENTER transfer_id=%u module=%s offset=0x%X "
               "r0=0x%X r1=0x%X r2=0x0 r3=0x0 r4-r12=unknown sp=unknown lr=0x%X cpsr=unknown\n",
               transfer_id, module_display(to_m), to_off, r0, r1, lr);
    }
    fflush(stdout);

    if (!s->proto_emitted) {
        s->proto_emitted = 1;
        analyze_dispatch_proto(uc, target);
    }
}

static void maybe_note_module_pointer_store(uint32_t value,
                                            uint32_t dest,
                                            uint64_t writer_mid,
                                            uint32_t writer_pc,
                                            const char *writer_name,
                                            uint32_t writer_off) {
    ModuleRegistry *reg;
    const GwyLoadedModule *tgt;
    const GwyLoadedModule *writer;
    uint32_t v = value & ~1u;
    const char *relation = "unknown";

    if (!ext_object_dispatch_trace_enabled()) return;
    if (v < 0x10000u) return;

    reg = gwy_ext_loader_bound_registry();
    if (!reg) return;
    tgt = module_registry_find_by_code_addr(reg, v);
    if (!tgt || tgt->origin != MODULE_ORIGIN_MRP_MEMBER) return;

    writer = writer_mid ? module_registry_find_by_id(reg, writer_mid) : NULL;
    /* Prefer DSM / mrc_loader writers to limit noise. */
    if (writer) {
        const char *wn = module_display(writer);
        if (writer->origin != MODULE_ORIGIN_DSM && !strstr(wn, "mrc_loader")) return;
    }

    if (tgt->entries.registered_helper && (tgt->entries.registered_helper & ~1u) == v)
        relation = "helper";
    else if (tgt->entries.observed_first_pc && (tgt->entries.observed_first_pc & ~1u) == v)
        relation = "entry";
    else
        relation = "unknown";

    if (g_obj.session.active && strstr(module_display(tgt), "robotol"))
        g_obj.session.writes_to_robotol_code++;
    if (g_obj.session.active && tgt->entries.registered_helper &&
        (tgt->entries.registered_helper & ~1u) == v)
        g_obj.session.registered_helper_seen = 1;

    record_ptr_write(value, dest, writer_mid, writer_pc, tgt->module_id, module_display(tgt),
                     relation);
    printf("[MODULE_POINTER_WRITE] value=0x%X target_module=%s relation=%s dest=0x%X "
           "writer_module=%s writer_offset=0x%X evidence=OBSERVED\n",
           value, module_display(tgt), relation, dest, writer_name ? writer_name : "?",
           writer_off);
    fflush(stdout);
}

void ext_object_dispatch_on_code(void *uc,
                                 uint64_t module_id,
                                 uint32_t pc,
                                 const uint32_t regs[16],
                                 uint32_t cpsr) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    ExtDispatchSession *s;
    int thumb;
    uint32_t word = 0;
    int rt = -1, rn = -1, is_load = 0;
    uint32_t imm = 0;

    if (!ext_object_dispatch_trace_enabled() || !regs) return;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    if (!m) return;

    s = &g_obj.session;
    if (s->active) s->last_r0 = regs[0];

    /* EXIT: BX lr / MOV pc,lr while in loader session → return into DSM. */
    if (s->active && !s->exit_emitted && module_id == s->loader_mid) {
        thumb = (cpsr & (1u << 5)) != 0;
        if (thumb) {
            uint16_t half;
            if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) {
                half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
                /* BX lr = 0x4770 */
                if (half == 0x4770u) {
                    ExtAddrClassResult lrcls;
                    ext_object_classify_address(regs[14], regs[13], &lrcls);
                    if (lrcls.addr_class == ADDR_DSM_CODE ||
                        (s->return_lr && (regs[14] & ~1u) == (s->return_lr & ~1u)))
                        emit_dispatch_exit(regs[0], lrcls.module_name[0] ? lrcls.module_name
                                                                           : "dsm:cfunction.ext");
                }
            }
        } else if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &word)) {
            /* BX lr / MOV pc, lr */
            if (word == 0xE12FFF1Eu || word == 0xE1A0F00Eu) {
                ExtAddrClassResult lrcls;
                ext_object_classify_address(regs[14], regs[13], &lrcls);
                if (lrcls.addr_class == ADDR_DSM_CODE ||
                    (s->return_lr && (regs[14] & ~1u) == (s->return_lr & ~1u)))
                    emit_dispatch_exit(regs[0], lrcls.module_name[0] ? lrcls.module_name
                                                                       : "dsm:cfunction.ext");
            }
        }
    }

    /* Also: PC landed back on return_lr while session open (if DSM hooked). */
    if (s->active && !s->exit_emitted && s->return_lr &&
        (pc & ~1u) == (s->return_lr & ~1u) && m->origin == MODULE_ORIGIN_DSM) {
        emit_dispatch_exit(regs[0], module_display(m));
    }

    /* STR that stores a module code pointer. */
    thumb = (cpsr & (1u << 5)) != 0;
    if (thumb) {
        uint16_t half;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) return;
        half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
        if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && !is_load && rt >= 0 &&
            rt < 16 && rn >= 0 && rn < 16) {
            uint32_t dest = regs[rn] + imm;
            uint32_t off = 0;
            if (m->map.guest_code_base && (pc & ~1u) >= m->map.guest_code_base)
                off = (pc & ~1u) - m->map.guest_code_base;
            maybe_note_module_pointer_store(regs[rt], dest, module_id, pc, module_display(m),
                                            off);
        }
    } else if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &word)) {
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && !is_load && rt >= 0 &&
            rt < 16 && rn >= 0 && rn < 16) {
            uint32_t dest = regs[rn] + imm;
            uint32_t off = 0;
            if (m->map.guest_code_base && (pc & ~1u) >= m->map.guest_code_base)
                off = (pc & ~1u) - m->map.guest_code_base;
            maybe_note_module_pointer_store(regs[rt], dest, module_id, pc, module_display(m),
                                            off);
        }
    }
}

void ext_object_note_cross_module_call(uint64_t from_mid,
                                       uint32_t from_pc,
                                       uint64_t to_mid,
                                       uint32_t target,
                                       uint32_t r0,
                                       uint32_t r1,
                                       uint32_t lr) {
    uint32_t regs[16];
    memset(regs, 0, sizeof(regs));
    regs[0] = r0;
    regs[1] = r1;
    regs[14] = lr;
    ext_object_note_cross_module_call_ex(from_mid, from_pc, to_mid, target, regs, 0, NULL);
}

void ext_object_note_cross_module_call_ex(uint64_t from_mid,
                                          uint32_t from_pc,
                                          uint64_t to_mid,
                                          uint32_t target,
                                          const uint32_t *regs,
                                          uint32_t cpsr,
                                          void *uc) {
    ModuleRegistry *reg;
    const GwyLoadedModule *from_m;
    const GwyLoadedModule *to_m;
    uint32_t tnorm = target & ~1u;
    uint32_t to_off = 0;
    uint32_t r0 = regs ? regs[0] : 0;
    uint32_t r1 = regs ? regs[1] : 0;
    uint32_t lr = regs ? regs[14] : 0;
    char from_name[96];
    char to_name[96];
    ExtTransferHopKind hop;
    uint32_t tid;

    if (!from_mid || !to_mid || from_mid == to_mid) return;

    reg = gwy_ext_loader_bound_registry();
    from_m = reg ? module_registry_find_by_id(reg, from_mid) : NULL;
    to_m = reg ? module_registry_find_by_id(reg, to_mid) : NULL;
    snprintf(from_name, sizeof(from_name), "%s", module_display(from_m));
    snprintf(to_name, sizeof(to_name), "%s", module_display(to_m));
    hop = classify_hop(from_m, to_m, to_name);

    /* Dedupe: DSM select + guest enter both observe the same hop. */
    if (g_obj.last_xfer.valid && g_obj.last_xfer.caller_module_id == from_mid &&
        g_obj.last_xfer.callee_module_id == to_mid &&
        (g_obj.last_xfer.dispatch_entry & ~1u) == tnorm) {
        if (regs && g_obj.session.active &&
            g_obj.session.transfer_id == g_obj.last_xfer.transfer_id)
            g_obj.session.last_r0 = regs[0];
        return;
    }

    if (to_m && to_m->map.guest_code_base && tnorm >= to_m->map.guest_code_base)
        to_off = tnorm - to_m->map.guest_code_base;

    tid = ++g_obj.next_transfer_id;

    g_obj.last_xfer.valid = 1;
    g_obj.last_xfer.transfer_id = tid;
    g_obj.last_xfer.hop_kind = hop;
    g_obj.last_xfer.caller_module_id = from_mid;
    g_obj.last_xfer.caller_pc = from_pc;
    g_obj.last_xfer.caller_target = target;
    g_obj.last_xfer.dispatch_module_id = to_mid;
    g_obj.last_xfer.dispatch_entry = tnorm;
    g_obj.last_xfer.callee_module_id = to_mid;
    g_obj.last_xfer.callee_first_pc = tnorm;
    g_obj.last_xfer.r0 = r0;
    g_obj.last_xfer.r1 = r1;
    g_obj.last_xfer.lr = lr;

    printf("[CROSS_MODULE_CALL] transfer_id=%u hop=%s from_module=%s from_module_id=%llu "
           "from_pc=0x%X to_module=%s to_module_id=%llu target=0x%X target_offset=0x%X "
           "r0=0x%X r1=0x%X lr=0x%X evidence=OBSERVED\n",
           tid, ext_transfer_hop_kind_name(hop), from_name, (unsigned long long)from_mid, from_pc,
           to_name, (unsigned long long)to_mid, tnorm, to_off, r0, r1, lr);
    fflush(stdout);
    ext_cfunction_publication_audit_on_cross_module(from_pc, tnorm, r0, r1, lr, from_name, to_name);

    /* A9: nested EXT returning into DSM after module-entry call. */
    if (from_m && to_m && from_m->origin == MODULE_ORIGIN_MRP_MEMBER &&
        (to_m->origin == MODULE_ORIGIN_DSM || strstr(to_name, "dsm") ||
         strstr(to_name, "cfunction"))) {
        ext_entry_null_contract_on_return_to_dsm(from_mid, r0);
        /* Phase 6C-A/D2: hop into DSM may not own an R9 frame (observe leave). */
        {
            ModuleR9Scope sc;
            uint32_t cur_r9 = 0;
            module_r9_switch_last_enter_scope(&sc);
            (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &cur_r9);
            /* Arm watch before leave so ZERO/FOREIGN attach to nested CFN. */
            ext_r9_scope_audit_on_nested_cfn(from_pc, tnorm, cur_r9, from_mid);
            module_r9_switch_set_guest_context(from_pc, from_name);
            /* D3: YIELD leave with unowned/already_switched token → NOOP (keep outer MR_HELPER). */
            module_r9_switch_leave_scope(uc, &sc, "CROSS_MODULE_MRP_TO_DSM",
                                        GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST);
        }
    }

    if (hop == EXT_HOP_DSM_TO_LOADER) {
        g_obj.cross_logged_mask |= 1;
        printf("[R0_LAYER] stage=DSM_TO_DISPATCH r0=0x%X dispatch_module=%s\n", r0, to_name);
        fflush(stdout);
        if (ext_object_dispatch_trace_enabled()) {
            emit_dispatch_enter(uc, tid, to_m, target, regs, cpsr, lr);
            emit_r0_semantics(hop, r0, infer_r0_role_quick(r0, hop), to_m);
        }
    }

    if (from_m && strstr(from_name, "mrc_loader") && to_m && strstr(to_name, "robotol")) {
        g_obj.cross_logged_mask |= 2;
        printf("[R0_LAYER] stage=LOADER_TO_ROBOTOL r0=0x%X callee_module=%s\n", r0, to_name);
        fflush(stdout);
    }

    if (hop == EXT_HOP_DSM_TO_ROBOTOL) {
        LauncherError err;
        const char *tsrc;
        const char *trel;
        uint32_t ret_r0 = g_obj.session.last_r0;
        uint32_t helper = to_m ? to_m->entries.registered_helper : 0;
        uint32_t caller_off = 0;

        if (g_obj.session.active && !g_obj.session.exit_emitted)
            emit_dispatch_exit(ret_r0, from_name);

        if (reg && to_m)
            module_registry_set_dsm_direct_target(reg, to_m->module_id, target, &err);

        if (from_m && from_m->map.guest_code_base &&
            (from_pc & ~1u) >= from_m->map.guest_code_base)
            caller_off = (from_pc & ~1u) - from_m->map.guest_code_base;

        tsrc = match_target_source(target, ret_r0, helper);
        trel = match_target_relation(to_m, target);

        if (ext_object_dispatch_trace_enabled()) {
            printf("[SECOND_HOP] transfer_id=%u source_module=%s caller_offset=0x%X "
                   "target_module=%s target=0x%X target_offset=0x%X "
                   "target_source=%s target_relation=%s r0=0x%X r1=0x%X after_dispatch_exit=%d\n",
                   tid, from_name, caller_off, to_name, tnorm, to_off, tsrc, trel, r0, r1,
                   g_obj.dispatch_exit_seen ? 1 : 0);
            fflush(stdout);
            emit_r0_semantics(hop, r0, infer_r0_role_quick(r0, hop), to_m);
            emit_dispatch_hop_case();
        }
        /* A6: helper handoff / record field / ENTRY_NATURE (independent of dispatch-trace). */
        ext_helper_handoff_on_second_hop(uc, from_pc, target, r0, r1, from_mid, to_mid);
        ext_dsm_record_on_second_hop(uc, from_pc, target, r0, r1, from_mid, to_mid);
        /* A8: CODE_IMAGE entry ABI vs mr_helper path separation. */
        {
            uint32_t r2 = regs ? regs[2] : 0;
            uint32_t r3 = regs ? regs[3] : 0;
            ext_module_entry_abi_on_second_hop(uc, from_pc, target, r0, r1, r2, r3, from_mid, to_mid,
                                               trel);
            ext_entry_null_contract_on_second_hop(uc, from_pc, target, r0, r1, r2, r3, from_mid,
                                                  to_mid, trel);
            {
                uint32_t r9 = regs ? regs[9] : 0;
                ext_module_data_init_on_second_hop(uc, from_pc, target, r0, r1, r2, r3, r9, from_mid,
                                                   to_mid, trel);
            }
        }
    }
}

const ExtCallTransfer *ext_object_last_transfer(void) {
    return g_obj.last_xfer.valid ? &g_obj.last_xfer : NULL;
}

const ExtEntryObjectTrace *ext_object_last_entry_trace(void) {
    return g_obj.last_trace.valid ? &g_obj.last_trace : NULL;
}

static uint32_t resolve_entry_object_base(void *uc,
                                          uint32_t call_pc,
                                          uint32_t r9,
                                          int *out_base_reg,
                                          uint32_t *out_field_value,
                                          char *source_out,
                                          size_t source_cap,
                                          uint32_t *source_addr_out,
                                          char *producer_out,
                                          size_t producer_cap,
                                          uint32_t *producer_off_out) {
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
    uint32_t slot = 0;
    uint32_t base = 0;
    uint32_t field = 0;
    ModuleRegistry *reg;

    if (out_base_reg) *out_base_reg = -1;
    if (out_field_value) *out_field_value = 0;
    if (source_out && source_cap) snprintf(source_out, source_cap, "%s", "unknown");
    if (source_addr_out) *source_addr_out = 0;
    if (producer_out && producer_cap) snprintf(producer_out, producer_cap, "%s", "unknown");
    if (producer_off_out) *producer_off_out = 0;
    if (!uc || !call_pc) return 0;

    for (i = -8; i <= 0; i++) {
        uint32_t addr = (call_pc & ~1u) + (uint32_t)(i * 4);
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rt == 1 &&
            imm == 4u) {
            saw_ldr_plus4 = 1;
            if (out_base_reg) *out_base_reg = rn;
        }
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rt == 0 &&
            rn == 0 && imm == 0)
            saw_ldr_indirect = 1;
        if ((word & 0x0FFFFFFFu) == 0x00800009u) saw_add_r9 = 1;
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rt == 0 &&
            rn == 15) {
            pc_rel_addr = addr;
            pc_rel_imm = imm;
        }
    }

    if (saw_ldr_plus4 && saw_ldr_indirect && saw_add_r9 && pc_rel_addr) {
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, pc_rel_addr + 8u + pc_rel_imm, &lit))
            return 0;
        slot = lit + r9;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, slot, &base)) return 0;
        if (base)
            guest_memory_uc_peek_u32((struct uc_struct *)uc, base + 4u, &field);
        if (out_field_value) *out_field_value = field;
        if (source_out && source_cap) snprintf(source_out, source_cap, "%s", "pc_rel_add_r9");
        if (source_addr_out) *source_addr_out = slot;
        reg = gwy_ext_loader_bound_registry();
        {
            const GwyLoadedModule *pm =
                reg ? module_registry_find_by_code_addr(reg, call_pc & ~1u) : NULL;
            if (pm) {
                if (producer_out && producer_cap)
                    snprintf(producer_out, producer_cap, "%s", module_display(pm));
                if (producer_off_out && pm->map.guest_code_base)
                    *producer_off_out = (call_pc & ~1u) - pm->map.guest_code_base;
            }
        }
        return base;
    }
    return 0;
}

static ExtObjectKind classify_entry_object_kind(uint32_t base,
                                                uint32_t field_at_base_plus4,
                                                uint32_t dispatch_target,
                                                int field_matches_dispatch) {
    ExtAddrClassResult br, fr, dr;
    (void)field_at_base_plus4;
    ext_object_classify_address(base, 0, &br);
    ext_object_classify_address(dispatch_target, 0, &dr);
    ext_object_classify_address(field_at_base_plus4, 0, &fr);

    /* Consecutive self-ish pointer table → function table / other, not chunk. */
    if (base && field_at_base_plus4 == base + 4u) return EXT_OBJECT_FUNCTION_TABLE;

    if (field_matches_dispatch && dr.addr_class == ADDR_MODULE_CODE)
        return EXT_OBJECT_MODULE_DESCRIPTOR;

    if (br.addr_class == ADDR_HEAP && field_matches_dispatch &&
        (dr.addr_class == ADDR_MODULE_CODE || dr.addr_class == ADDR_DSM_CODE))
        return EXT_OBJECT_MRC_EXT_CHUNK; /* only if field matches — else not */

    if (!field_matches_dispatch && br.addr_class == ADDR_HEAP) return EXT_OBJECT_OTHER;

    if (br.addr_class == ADDR_DSM_RW || br.addr_class == ADDR_MODULE_RW)
        return EXT_OBJECT_LOADER_RECORD;

    return EXT_OBJECT_UNKNOWN;
}

static const char *classify_identity_case(const ExtAddrClassResult *dispatch_cls,
                                         ExtObjectKind kind,
                                         int field_matches,
                                         int trampoline) {
    if (dispatch_cls && dispatch_cls->addr_class == ADDR_MODULE_CODE &&
        strstr(dispatch_cls->module_name, "mrc_loader")) {
        if (trampoline) return "B";
        return "A";
    }
    if (dispatch_cls && dispatch_cls->addr_class == ADDR_UNKNOWN) return "E";
    if (kind == EXT_OBJECT_MODULE_DESCRIPTOR || kind == EXT_OBJECT_FUNCTION_TABLE ||
        kind == EXT_OBJECT_LOADER_RECORD)
        return "D";
    if (!field_matches) return "D";
    if (dispatch_cls && dispatch_cls->module_id == 0 &&
        dispatch_cls->addr_class == ADDR_MODULE_CODE)
        return "C";
    return "E";
}

void ext_object_disasm_dispatch(void *uc, uint32_t dispatch_target) {
    ExtAddrClassResult r;
    int tramp;
    if (!ext_object_identity_enabled() || !uc || !dispatch_target) return;
    ext_object_classify_address(dispatch_target, 0, &r);
    printf("[DISPATCH_DISASM] va=0x%X module=%s offset=0x%X class=%s\n", dispatch_target,
           r.module_name, r.module_offset, guest_address_class_name(r.addr_class));
    fflush(stdout);
    /* Reuse entry candidate disasm via public path: dump through helper by calling
     * a lightweight local loop (avoid exporting dump_entry_candidate_disasm). */
    {
        int i;
        uint32_t pc = dispatch_target & ~1u;
        int thumb = (dispatch_target & 1u) != 0;
        for (i = -30; i <= 30; i++) {
            uint32_t addr;
            uint32_t word = 0;
            char line[96];
            int rt = -1, rn = -1, is_load = 0, rm = -1;
            uint32_t imm = 0;
            if (thumb) {
                uint16_t half;
                addr = pc + (uint32_t)(i * 2);
                if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr & ~3u, &word)) continue;
                half = (uint16_t)((addr & 2u) ? (word >> 16) : (word & 0xFFFFu));
                if (ext_entry_decode_thumb_blx_rm(half, &rm))
                    snprintf(line, sizeof(line), "BLX r%d", rm);
                else if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load))
                    snprintf(line, sizeof(line), "%s r%d,[r%d,#0x%X]", is_load ? "LDR" : "STR", rt,
                             rn, imm);
                else
                    snprintf(line, sizeof(line), ".hword 0x%04X", half);
                word = half;
            } else {
                addr = pc + (uint32_t)(i * 4);
                if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &word)) continue;
                if (ext_entry_decode_arm_blx_rm(word, &rm))
                    snprintf(line, sizeof(line), "BLX r%d", rm);
                else if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load))
                    snprintf(line, sizeof(line), "%s r%d,[r%d,#0x%X]", is_load ? "LDR" : "STR", rt,
                             rn, imm);
                else if ((word & 0xFFFF0000u) == 0xE92D0000u)
                    snprintf(line, sizeof(line), "STMFD ...");
                else
                    snprintf(line, sizeof(line), ".word 0x%08X", word);
            }
            printf("[DISPATCH_DISASM] %c off=%+d addr=0x%X raw=0x%08X %s\n", i == 0 ? '*' : ' ', i,
                   addr, word, line);
        }
        tramp = 0;
        {
            const char *nature = tramp ? "trampoline" : "entry_or_dispatcher";
            if (ext_callback_frame_is_cfn_continuation(dispatch_target, NULL, NULL))
                nature = "guest_callback_continuation";
            printf("[DISPATCH_CLASS] module=%s offset=0x%X nature=%s equals_observed_first=%s "
                   "superseded_by_callback_continuation_identity=%s causal=%s\n",
                   r.module_name, r.module_offset, nature, "check_registry",
                   (nature[0] == 'g') ? "yes" : "no", (nature[0] == 'g') ? "false" : "unknown");
        }
        fflush(stdout);
        (void)tramp;
    }
}

void ext_object_observe_on_dsm_select(void *uc,
                                      uint32_t call_pc,
                                      uint32_t r9,
                                      uint32_t sp,
                                      uint32_t r0_at_blx,
                                      int base_reg,
                                      uint32_t dispatch_target,
                                      uint32_t candidate_chunk_base,
                                      const char *caller_module,
                                      uint32_t caller_offset) {
    ExtEntryObjectTrace *t = &g_obj.last_trace;
    uint32_t live_base = 0;
    uint32_t field_live = 0;
    char source[32];
    char producer[96];
    uint32_t source_addr = 0;
    uint32_t producer_off = 0;
    int resolved_reg = base_reg;
    ExtAddrClassResult dcls, bcls, fcls;
    int field_matches = 0;
    ExtObjectKind kind;
    ExtChunkRecord *cand;
    const char *case_id;
    uint32_t tnorm = dispatch_target & ~1u;
    ModuleRegistry *reg;
    const GwyLoadedModule *dm;
    int trampoline = 0;

    memset(t, 0, sizeof(*t));
    t->valid = 1;
    t->base_reg = base_reg;
    t->field_offset = 0x4u;
    t->dispatch_target = dispatch_target;

    live_base = resolve_entry_object_base(uc, call_pc, r9, &resolved_reg, &field_live, source,
                                         sizeof(source), &source_addr, producer, sizeof(producer),
                                         &producer_off);
    if (resolved_reg >= 0) t->base_reg = resolved_reg;
    t->base_value = live_base;
    /* Prefer DSM BLX target as field value when live peek mismatches (wrong object). */
    t->field_value = field_live;
    snprintf(t->base_source, sizeof(t->base_source), "%s", source);
    t->source_address = source_addr;
    snprintf(t->producer_module, sizeof(t->producer_module), "%s", producer);
    t->producer_offset = producer_off;

    field_matches =
        (field_live && (field_live == dispatch_target || (field_live & ~1u) == tnorm));
    if (!field_matches && dispatch_target)
        t->field_value = dispatch_target; /* dispatch target is authoritative for typing */

    printf("[ENTRY_OBJECT_TRACE] stage=FIELD_LOAD base_reg=r%d base_value=0x%X field_offset=0x4 "
           "field_value=0x%X field_live_peek=0x%X dispatch_target=0x%X r0_at_blx=0x%X\n",
           t->base_reg, t->base_value, t->field_value, field_live, dispatch_target, r0_at_blx);
    fflush(stdout);

    printf("[ENTRY_OBJECT_TRACE] stage=BASE_SOURCE source=%s source_address=0x%X "
           "producer_module=%s producer_offset=0x%X caller_module=%s caller_offset=0x%X\n",
           t->base_source, t->source_address, t->producer_module, t->producer_offset,
           caller_module ? caller_module : "unknown", caller_offset);
    fflush(stdout);

    ext_object_log_addr_class("entry_object_base", t->base_value, sp);
    ext_object_log_addr_class("entry_object_field_04", t->field_value, sp);
    ext_object_log_addr_class("dispatch_target", dispatch_target, sp);

    ext_object_classify_address(dispatch_target, sp, &dcls);
    ext_object_classify_address(t->base_value, sp, &bcls);
    ext_object_classify_address(t->field_value, sp, &fcls);

    kind = classify_entry_object_kind(t->base_value, field_live, dispatch_target, field_matches);
    t->object_kind = kind;
    t->confidence = field_matches ? EXT_IDENTITY_PROBABLE : EXT_IDENTITY_CANDIDATE;

    cand = candidate_chunk_base ? ext_chunk_observe_find(candidate_chunk_base) : NULL;
    if (cand) {
        int mismatch = 0;
        if (t->base_value && candidate_chunk_base && t->base_value != candidate_chunk_base)
            mismatch = 1;
        if (field_live && dispatch_target && !field_matches) mismatch = 1;
        printf("[CANDIDATE_CHUNK] chunk_id=%llu base=0x%X vs_entry_object_base=0x%X "
               "mismatch=%d confidence=%s note=candidate_only_until_confirmed\n",
               (unsigned long long)cand->chunk_id, candidate_chunk_base, t->base_value, mismatch,
               ext_identity_confidence_name(t->confidence));
        fflush(stdout);
        if (mismatch) {
            printf("[CANDIDATE_CHUNK] candidate_mismatch evidence=OBSERVED\n");
            fflush(stdout);
            t->confidence = EXT_IDENTITY_CANDIDATE;
        }
    }

    reg = gwy_ext_loader_bound_registry();
    dm = reg ? module_registry_find_by_code_addr(reg, tnorm) : NULL;

    /* Explicit DSM→dispatch transfer (DSM code region may lack BLX hook coverage). */
    if (dm && caller_module && strncmp(caller_module, "dsm:", 4) == 0) {
        const GwyLoadedModule *dsm_m = NULL;
        size_t i;
        for (i = 0; i < reg->count; i++) {
            if (reg->modules[i].origin == MODULE_ORIGIN_DSM) {
                dsm_m = &reg->modules[i];
                break;
            }
        }
        if (dsm_m) {
            uint32_t regs[16];
            uint32_t cpsr = 0;
            memset(regs, 0, sizeof(regs));
            regs[0] = r0_at_blx;
#ifdef GWY_HAVE_UNICORN
            if (uc) {
                int ri;
                for (ri = 0; ri < 13; ri++)
                    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0 + ri, &regs[ri]);
                uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &regs[13]);
                uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &regs[14]);
                uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &regs[15]);
                uc_reg_read((uc_engine *)uc, UC_ARM_REG_CPSR, &cpsr);
                regs[0] = r0_at_blx; /* authoritative pre-BLX sample from observer */
            }
#endif
            ext_object_note_cross_module_call_ex(dsm_m->module_id, call_pc, dm->module_id,
                                                dispatch_target, regs, cpsr, uc);
        }
    }

    {
        uint32_t first_pc = dm ? dm->entries.observed_first_pc : 0;
        printf("[ENTRY_TRANSFER] dispatch_target=0x%X dispatch_module=%s dispatch_offset=0x%X "
               "callee_module=%s first_executed_pc=0x%X r0=0x%X stage=CALL_SITE_BEFORE "
               "equals_observed_first=%d\n",
               tnorm, dcls.module_name, dcls.module_offset, dcls.module_name, first_pc, r0_at_blx,
               (first_pc && (first_pc & ~1u) == tnorm) ? 1 : 0);
        fflush(stdout);
    }

    if (ext_object_identity_enabled()) {
        ext_object_disasm_dispatch(uc, dispatch_target);
        /* Trampoline hint: if disasm later sets — for now if target module is loader and
         * robotol first_pc differs, treat as dispatcher (Case A) not trampoline unless
         * BRANCH lands in robotol (checked lightly). */
        if (dm && strstr(module_display(dm), "mrc_loader")) {
            const GwyLoadedModule *rob =
                reg ? module_registry_find(reg, "robotol.ext") : NULL;
            if (!rob && reg) rob = module_registry_find(reg, "cfunction.ext");
            if (rob && rob->entries.observed_first_pc &&
                (rob->entries.observed_first_pc & ~1u) != tnorm)
                trampoline = 0; /* separate later enter → dispatcher */
        }
    }

    printf("[ENTRY_OBJECT_KIND] kind=%s confidence=%s field_matches_dispatch=%d "
           "base_class=%s dispatch_class=%s\n",
           ext_object_kind_name(kind), ext_identity_confidence_name(t->confidence), field_matches,
           guest_address_class_name(bcls.addr_class), guest_address_class_name(dcls.addr_class));
    fflush(stdout);

    case_id = classify_identity_case(&dcls, kind, field_matches, trampoline);
    if (!g_obj.identity_case_emitted) {
        g_obj.identity_case_emitted = 1;
        printf("[OBJECT_IDENTITY_CASE] case=%s "
               "legend=A_loader_dispatcher|B_trampoline|C_base_wrong|D_descriptor_or_table|"
               "E_attrib_bug "
               "dispatch_module=%s dispatch_offset=0x%X object_kind=%s "
               "dsm_direct_robotol=%s r0_at_dsm_blx=0x%X "
               "candidate_wrong=%d phase6b_b_gate=blocked\n",
               case_id, dcls.module_name, dcls.module_offset, ext_object_kind_name(kind),
               (dcls.module_name[0] && strstr(dcls.module_name, "robotol")) ? "yes" : "no",
               r0_at_blx, (cand && (!field_matches || (t->base_value && candidate_chunk_base &&
                                                       t->base_value != candidate_chunk_base)))
                              ? 1
                              : 0);
        fflush(stdout);
    }

    (void)fcls;
}
