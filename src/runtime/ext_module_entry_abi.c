#include "gwy_launcher/ext_module_entry_abi.h"
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

#define GWY_MEA_PROTO_INSNS 48
#define GWY_MEA_R0_WALK 24

typedef struct {
    uint32_t image_base;
    uint32_t image_size;
    int valid;
} MeaCodeImage;

typedef struct {
    uint32_t helper;
    uint32_t p_guest;
    uint32_t p_len;
    int alloc_logged;
    int init_seen;
    int helper_call_seen;
    int p28_store_seen;
} MeaCFunctionP;

typedef struct {
    int entry_seen;
    uint32_t entry_pc;
    uint32_t helper;
    uint64_t module_id;
    char module_name[96];
    uint32_t r0, r1, r2, r3;
    char r0_role[32];
    char r1_role[32];
    char r2_role[32];
    char r3_role[32];
    int accesses_r0_plus_28;
    int command_dispatch;
    ExtR0AssignKind r0_assign;
    char r0_assign_detail[64];
    ExtLoadReturnClass ret_class;
    ExtLoadReturnTransform transform;
    uint32_t return_raw;
    uint32_t return_norm;
    uint32_t return_offset;
    int loadcode_logged;
    int proto_logged;
    int compare_logged;
    int chain_logged;
} MeaEntrySession;

typedef struct {
    void *uc;
    MeaCodeImage last_image;
    MeaCFunctionP p;
    MeaEntrySession entry;
    int mr_helper_seen;
    int mr_helper_not_reached_logged;
    int fault_seen;
    ExtModuleEntryRootCause root;
    int root_emitted;
    uint32_t sampling_entry_pc;
    int sample_count;
} ModuleEntryAbiState;

static ModuleEntryAbiState g_mea;

void ext_module_entry_abi_reset(void) { memset(&g_mea, 0, sizeof(g_mea)); }

int ext_module_entry_abi_enabled(void) {
    const char *env = getenv("GWY_MODULE_ENTRY_ABI");
    return env && env[0] == '1';
}

void ext_module_entry_abi_bind_uc(void *uc) { g_mea.uc = uc; }

const char *ext_load_return_class_name(ExtLoadReturnClass c) {
    switch (c) {
        case LOAD_RET_CODE_POINTER: return "CODE_POINTER";
        case LOAD_RET_DESCRIPTOR: return "DESCRIPTOR";
        case LOAD_RET_HANDLE: return "HANDLE";
        case LOAD_RET_TRAMPOLINE: return "TRAMPOLINE";
        default: return "UNKNOWN";
    }
}

const char *ext_load_return_transform_name(ExtLoadReturnTransform t) {
    switch (t) {
        case LOAD_XFORM_NONE: return "NONE";
        case LOAD_XFORM_THUMB_BIT: return "THUMB_BIT";
        case LOAD_XFORM_ADD_OFFSET: return "ADD_OFFSET";
        case LOAD_XFORM_DEREF: return "DEREF";
        case LOAD_XFORM_DESCRIPTOR_FIELD: return "DESCRIPTOR_FIELD";
        default: return "UNKNOWN";
    }
}

const char *ext_module_entry_root_cause_name(ExtModuleEntryRootCause r) {
    switch (r) {
        case ME_ROOT_MISSING_MODULE_ENTRY_CONTEXT: return "MISSING_MODULE_ENTRY_CONTEXT";
        case ME_ROOT_MISSING_PRE_ENTRY_INITIALIZATION: return "MISSING_PRE_ENTRY_INITIALIZATION";
        case ME_ROOT_LOADCODE_RETURN_MISINTERPRETED: return "LOADCODE_RETURN_MISINTERPRETED";
        case ME_ROOT_WRONG_MODULE_ENTRY_LIFECYCLE: return "WRONG_MODULE_ENTRY_LIFECYCLE";
        case ME_ROOT_MRC_FUNCTION_P_ENTRY_ARGUMENT: return "MRC_FUNCTION_P_ENTRY_ARGUMENT";
        default: return "UNKNOWN";
    }
}

const char *ext_r0_assign_kind_name(ExtR0AssignKind k) {
    switch (k) {
        case R0_ASSIGN_EXPLICIT_ZERO: return "explicit_zero";
        case R0_ASSIGN_MOV_FROM_REG: return "mov_from_reg";
        case R0_ASSIGN_LOADED_FIELD: return "loaded_field";
        case R0_ASSIGN_LOADCODE_OUT: return "loadcode_out";
        default: return "unknown";
    }
}

ExtModuleEntryRootCause ext_module_entry_abi_last_root_cause(void) { return g_mea.root; }

int ext_module_entry_abi_mr_helper_seen(void) { return g_mea.mr_helper_seen; }

static void classify_roles_at(void *uc, uint32_t addr, char roles[4][32], int *out_cmp,
                              int *out_ldr_r0, int *out_r0_28) {
    int i;
    uint32_t pc;
    int saw_cmp = 0, saw_ldr = 0, saw_28 = 0;
    int thumb;
    for (i = 0; i < 4; i++) snprintf(roles[i], 32, "%s", "UNKNOWN");
    if (!uc || !addr) return;
    thumb = (addr & 1u) != 0;
    pc = addr & ~1u;
    for (i = 0; i < GWY_MEA_PROTO_INSNS; i++) {
        uint32_t word = 0;
        int rt = -1, rn = -1, is_load = 0;
        uint32_t imm = 0;
        if (thumb) {
            uint16_t half;
            uint32_t a = pc + (uint32_t)(i * 2);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, a & ~3u, &word)) continue;
            half = (uint16_t)((a & 2u) ? (word >> 16) : (word & 0xFFFFu));
            if ((half & 0xFF00u) == 0x2800u) {
                saw_cmp = 1;
                if (strcmp(roles[0], "UNKNOWN") == 0) snprintf(roles[0], 32, "%s", "COMMAND");
            }
            if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load && rn == 0) {
                saw_ldr = 1;
                if (imm == 0x28u) saw_28 = 1;
                if (strcmp(roles[0], "UNKNOWN") == 0 || strcmp(roles[0], "COMMAND") == 0)
                    snprintf(roles[0], 32, "%s", "SELF_OR_OBJECT");
            }
        } else {
            uint32_t a = pc + (uint32_t)(i * 4);
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, a, &word)) continue;
            if ((word & 0x0FF0F000u) == 0x03500000u) {
                saw_cmp = 1;
                if (strcmp(roles[0], "UNKNOWN") == 0) snprintf(roles[0], 32, "%s", "COMMAND");
            }
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rn == 0) {
                saw_ldr = 1;
                if (imm == 0x28u) saw_28 = 1;
                if (strcmp(roles[0], "UNKNOWN") == 0 || strcmp(roles[0], "COMMAND") == 0)
                    snprintf(roles[0], 32, "%s", "SELF_OR_OBJECT");
            }
        }
    }
    if (out_cmp) *out_cmp = saw_cmp;
    if (out_ldr_r0) *out_ldr_r0 = saw_ldr;
    if (out_r0_28) *out_r0_28 = saw_28;
    if (saw_cmp && !saw_ldr) snprintf(roles[0], 32, "%s", "COMMAND");
    else if (saw_ldr && !saw_cmp) snprintf(roles[0], 32, "%s", "SELF_OR_OBJECT");
    else if (saw_cmp && saw_ldr) snprintf(roles[0], 32, "%s", "COMMAND_OR_OBJECT");
}

static ExtR0AssignKind recover_r0_assign(void *uc, uint32_t caller_pc, char *detail, size_t dcap) {
    int i;
    uint32_t pc;
    if (detail && dcap) detail[0] = '\0';
    if (!uc || !caller_pc) return R0_ASSIGN_UNKNOWN;
    pc = caller_pc & ~1u;
    for (i = 1; i <= GWY_MEA_R0_WALK; i++) {
        uint32_t a = pc - (uint32_t)(i * 2);
        uint32_t word = 0;
        uint16_t half;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, a & ~3u, &word)) continue;
        half = (uint16_t)((a & 2u) ? (word >> 16) : (word & 0xFFFFu));
        if ((half & 0xFF00u) == 0x2000u) {
            uint32_t imm = half & 0xFFu;
            if (imm == 0) {
                if (detail && dcap)
                    snprintf(detail, dcap, "movs_r0_#0_at_-0x%X", (unsigned)(i * 2));
                return R0_ASSIGN_EXPLICIT_ZERO;
            }
            if (detail && dcap)
                snprintf(detail, dcap, "movs_r0_#0x%X_at_-0x%X", imm, (unsigned)(i * 2));
            return R0_ASSIGN_MOV_FROM_REG;
        }
        if ((half & 0xFFC7u) == 0x4600u) {
            int rm = (half >> 3) & 0xF;
            if (detail && dcap)
                snprintf(detail, dcap, "mov_r0_r%d_at_-0x%X", rm, (unsigned)(i * 2));
            return R0_ASSIGN_MOV_FROM_REG;
        }
        {
            int rt = -1, rn = -1, is_load = 0;
            uint32_t imm = 0;
            if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load && rt == 0) {
                if (detail && dcap)
                    snprintf(detail, dcap, "ldr_r0_[r%d,#0x%X]_at_-0x%X", rn, imm,
                             (unsigned)(i * 2));
                return R0_ASSIGN_LOADED_FIELD;
            }
        }
    }
    if (detail && dcap) snprintf(detail, dcap, "no_assign_in_window");
    return R0_ASSIGN_UNKNOWN;
}

static void maybe_emit_root(void) {
    MeaEntrySession *e = &g_mea.entry;
    ExtModuleEntryRootCause r = ME_ROOT_UNKNOWN;
    if (g_mea.root_emitted || !e->entry_seen) return;
    if (e->ret_class == LOAD_RET_DESCRIPTOR || e->ret_class == LOAD_RET_HANDLE ||
        e->transform == LOAD_XFORM_DEREF || e->transform == LOAD_XFORM_DESCRIPTOR_FIELD)
        r = ME_ROOT_LOADCODE_RETURN_MISINTERPRETED;
    else if (e->r0 == 0 && e->accesses_r0_plus_28 && e->r0_assign != R0_ASSIGN_EXPLICIT_ZERO)
        r = ME_ROOT_MISSING_MODULE_ENTRY_CONTEXT;
    g_mea.root = r;
    g_mea.root_emitted = 1;
    printf("[MODULE_ENTRY_ROOT] root=%s r0=0x%X r0_assign=%s return_class=%s "
           "accesses_r0_plus_28=%d mr_helper_seen=%d evidence=OBSERVED "
           "note=no_guest_mutation phase6b_b_gate=blocked\n",
           ext_module_entry_root_cause_name(r), e->r0, ext_r0_assign_kind_name(e->r0_assign),
           ext_load_return_class_name(e->ret_class), e->accesses_r0_plus_28, g_mea.mr_helper_seen);
    fflush(stdout);
}

void ext_module_entry_abi_on_code_image(uint32_t guest_addr, uint32_t size) {
    if (!ext_module_entry_abi_enabled() || !guest_addr) return;
    g_mea.last_image.image_base = guest_addr;
    g_mea.last_image.image_size = size;
    g_mea.last_image.valid = 1;
    printf("[LOADCODE_CONTRACT] stage=CODE_IMAGE image_base=0x%X image_size=%u "
           "evidence=OBSERVED\n",
           guest_addr, size);
    fflush(stdout);
}

void ext_module_entry_abi_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                         const char *origin) {
    if (!ext_module_entry_abi_enabled() || !helper) return;
    g_mea.p.helper = helper;
    g_mea.p.p_guest = p_guest;
    g_mea.p.p_len = p_len;
    if (!g_mea.p.alloc_logged) {
        g_mea.p.alloc_logged = 1;
        printf("[CFUNCTION_P] stage=ALLOC address=0x%X size=%u helper=0x%X caller=%s "
               "note=mr_c_function_st_doc_size_20 evidence=DOCUMENTED "
               "source=mythroad.c:_mr_c_function_new\n",
               p_guest, p_len, helper, origin ? origin : "?");
        fflush(stdout);
    }
    if (p_guest && p_len > 0 && !g_mea.p.init_seen) {
        g_mea.p.init_seen = 1;
        printf("[CFUNCTION_P] stage=INIT address=0x%X size=%u helper=0x%X "
               "note=zeroed_by_new_then_guest_fills_ER_RW evidence=DOCUMENTED\n",
               p_guest, p_len, helper);
        fflush(stdout);
    }
}

void ext_module_entry_abi_on_mr_helper(uint32_t helper, uint32_t code, uint32_t p_guest, uint32_t r0,
                                       uint32_t r1, uint32_t r2, uint32_t r3) {
    if (!ext_module_entry_abi_enabled()) return;
    g_mea.mr_helper_seen = 1;
    printf("[MR_HELPER] stage=ENTER event=%u mr_c_function=0x%X mr_c_function_P=0x%X "
           "evidence=DOCUMENTED source=mythroad.c:case_801\n",
           code, helper, p_guest);
    printf("[MR_HELPER] stage=CALL_HELPER r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
           "note=r0_is_P_r1_is_code evidence=DOCUMENTED\n",
           r0 ? r0 : p_guest, r1 ? r1 : code, r2, r3);
    fflush(stdout);
    if (p_guest) {
        printf("[CFUNCTION_P] stage=HELPER_CALL address=0x%X size=%u helper=0x%X "
               "evidence=DOCUMENTED\n",
               p_guest, g_mea.p.p_len, helper);
        fflush(stdout);
        g_mea.p.helper_call_seen = 1;
    }
}

void ext_module_entry_abi_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr) {
    if (!ext_module_entry_abi_enabled()) return;
    g_mea.fault_seen = 1;
    if (!g_mea.mr_helper_seen && !g_mea.mr_helper_not_reached_logged) {
        g_mea.mr_helper_not_reached_logged = 1;
        printf("[MR_HELPER] stage=NOT_REACHED "
               "tag=MR_HELPER_NOT_REACHED_BEFORE_MODULE_ENTRY_FAULT "
               "fault_pc=0x%X fault_addr=0x%X evidence=OBSERVED "
               "note=event_helper_path_unrelated_to_current_fault\n",
               fault_pc, fault_addr);
        fflush(stdout);
    }
    if (!g_mea.root_emitted) {
        if (g_mea.entry.entry_seen) {
            maybe_emit_root();
        } else {
            g_mea.root = ME_ROOT_UNKNOWN;
            g_mea.root_emitted = 1;
            printf("[MODULE_ENTRY_ROOT] root=UNKNOWN r0=0x0 r0_assign=unknown "
                   "return_class=UNKNOWN accesses_r0_plus_28=0 mr_helper_seen=%d "
                   "evidence=OBSERVED note=entry_session_incomplete_no_guest_mutation "
                   "phase6b_b_gate=blocked\n",
                   g_mea.mr_helper_seen);
            fflush(stdout);
        }
    }
}

void ext_module_entry_abi_on_store(uint32_t value, uint32_t dest, uint64_t writer_mid,
                                   uint32_t writer_pc) {
    (void)writer_mid;
    (void)writer_pc;
    if (!ext_module_entry_abi_enabled()) return;
    if (!g_mea.p.p_guest || !g_mea.p.p_len) return;
    if (dest == g_mea.p.p_guest + 0x28u) {
        g_mea.p.p28_store_seen = 1;
        printf("[CFUNCTION_P] stage=FIELD_STORE address=0x%X field_offset=0x28 value=0x%X "
               "note=unexpected_for_size_20 evidence=OBSERVED\n",
               g_mea.p.p_guest, value);
        fflush(stdout);
    }
}

static void emit_loadcode_and_proto(void *uc, const GwyLoadedModule *to_m, uint32_t target,
                                    uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                                    uint32_t caller_pc, const char *from_name,
                                    const char *target_relation) {
    MeaEntrySession *e = &g_mea.entry;
    char roles[4][32];
    int cmp = 0, ldr = 0, r028 = 0;
    uint32_t base = 0, tnorm = target & ~1u;
    ExtLoadReturnClass rc = LOAD_RET_UNKNOWN;
    ExtLoadReturnTransform xf = LOAD_XFORM_UNKNOWN;
    const char *mod = "unknown";

    if (!to_m) return;
    base = to_m->map.guest_code_base;
    mod = to_m->resolved_name[0] ? to_m->resolved_name : to_m->requested_name;

    e->entry_seen = 1;
    e->entry_pc = target;
    e->helper = to_m->entries.registered_helper;
    e->module_id = to_m->module_id;
    snprintf(e->module_name, sizeof(e->module_name), "%s", mod);
    e->r0 = r0;
    e->r1 = r1;
    e->r2 = r2;
    e->r3 = r3;
    e->return_raw = target;
    e->return_norm = tnorm;
    e->return_offset = (base && tnorm >= base) ? (tnorm - base) : 0;

    if (target_relation && strcmp(target_relation, "trampoline") == 0) {
        rc = LOAD_RET_TRAMPOLINE;
        xf = LOAD_XFORM_NONE;
    } else if (base && tnorm >= base && tnorm < base + to_m->map.guest_code_size) {
        rc = LOAD_RET_CODE_POINTER;
        if ((target & 1u) != 0)
            xf = LOAD_XFORM_THUMB_BIT;
        else
            xf = LOAD_XFORM_NONE;
    }
    e->ret_class = rc;
    e->transform = xf;

    e->r0_assign =
        recover_r0_assign(uc, caller_pc, e->r0_assign_detail, sizeof(e->r0_assign_detail));
    if (e->r0 == 0 && e->r0_assign == R0_ASSIGN_UNKNOWN)
        e->r0_assign = R0_ASSIGN_EXPLICIT_ZERO;

    if (!e->loadcode_logged) {
        e->loadcode_logged = 1;
        printf("[LOADCODE_CONTRACT] module=%s image_base=0x%X return_raw=0x%X return_norm=0x%X "
               "return_class=%s return_offset=0x%X consumer_module=%s transform=%s "
               "evidence=OBSERVED\n",
               mod, base, e->return_raw, e->return_norm, ext_load_return_class_name(rc),
               e->return_offset, from_name ? from_name : "dsm:cfunction.ext",
               ext_load_return_transform_name(xf));
        fflush(stdout);
    }

    classify_roles_at(uc, target, roles, &cmp, &ldr, &r028);
    snprintf(e->r0_role, sizeof(e->r0_role), "%s", roles[0]);
    snprintf(e->r1_role, sizeof(e->r1_role), "%s", roles[1]);
    snprintf(e->r2_role, sizeof(e->r2_role), "%s", roles[2]);
    snprintf(e->r3_role, sizeof(e->r3_role), "%s", roles[3]);
    e->command_dispatch = (cmp && !ldr) ? 1 : 0;
    e->accesses_r0_plus_28 = r028;

    if (!e->proto_logged) {
        e->proto_logged = 1;
        printf("[MODULE_ENTRY_PROTO] target=%s+0x%X r0_role=%s r1_role=%s r2_role=%s r3_role=%s "
               "r0=0x%X r1=0x%X r2=0x%X r3=0x%X accesses_r0_plus_28=%d "
               "r0_assign=%s r0_assign_detail=%s evidence=OBSERVED\n",
               mod, e->return_offset, e->r0_role, e->r1_role, e->r2_role, e->r3_role, r0, r1, r2,
               r3, r028, ext_r0_assign_kind_name(e->r0_assign), e->r0_assign_detail);
        fflush(stdout);
        if (r0 == 0 && r028) {
            printf("[MODULE_ENTRY_BLOCKER] blocker=CODE_IMAGE_ENTRY_NULL_ARGUMENT "
                   "r0=0x0 accesses_r0_plus_28=1 r0_assign=%s phase6b_b_gate=blocked "
                   "evidence=OBSERVED\n",
                   ext_r0_assign_kind_name(e->r0_assign));
            fflush(stdout);
        } else {
            printf("[MODULE_ENTRY_BLOCKER] blocker=MODULE_ENTRY_ABI_UNKNOWN "
                   "phase6b_b_gate=blocked evidence=OBSERVED\n");
            fflush(stdout);
        }
    }

    if (!e->chain_logged) {
        e->chain_logged = 1;
        printf("[CHAIN_SEPARATION] registered_helper_ne_module_entry=EXPECTED "
               "helper=0x%X module_entry=0x%X evidence=DOCUMENTED\n",
               e->helper, target);
        fflush(stdout);
    }

    if (!e->compare_logged && e->helper) {
        char hroles[4][32];
        int hcmp = 0, hldr = 0, h28 = 0;
        e->compare_logged = 1;
        classify_roles_at(uc, e->helper, hroles, &hcmp, &hldr, &h28);
        printf("[ENTRY_COMPARE] item=caller code_image=DSM helper=mr_helper_event\n");
        printf("[ENTRY_COMPARE] item=r0_role code_image=%s helper=%s\n", e->r0_role, hroles[0]);
        printf("[ENTRY_COMPARE] item=command_dispatch code_image=%d helper=%d\n",
               e->command_dispatch, hcmp ? 1 : 0);
        printf("[ENTRY_COMPARE] item=accesses_r0_plus_28 code_image=%d helper=%d\n", r028, h28);
        printf("[ENTRY_COMPARE] item=lifecycle code_image=load_second_hop helper=event_dispatch\n");
        printf("[ENTRY_COMPARE] item=null_r0_allowed code_image=UNKNOWN helper=by_event_abi\n");
        printf("[ENTRY_COMPARE] item=addr code_image=0x%X helper=0x%X evidence=OBSERVED\n", target,
               e->helper);
        fflush(stdout);
    }

    g_mea.sampling_entry_pc = tnorm;
    g_mea.sample_count = 0;
}

void ext_module_entry_abi_on_second_hop(void *uc, uint32_t caller_pc, uint32_t target, uint32_t r0,
                                        uint32_t r1, uint32_t r2, uint32_t r3, uint64_t from_mid,
                                        uint64_t to_mid, const char *target_relation) {
    ModuleRegistry *reg;
    const GwyLoadedModule *from_m, *to_m;
    const char *from_name = "dsm:cfunction.ext";
    const char *rel = target_relation ? target_relation : "unknown";
    if (!ext_module_entry_abi_enabled() || !target) return;
    reg = gwy_ext_loader_bound_registry();
    from_m = reg ? module_registry_find_by_id(reg, from_mid) : NULL;
    to_m = reg ? module_registry_find_by_id(reg, to_mid) : NULL;
    if (from_m) {
        if (from_m->origin == MODULE_ORIGIN_DSM)
            from_name = "dsm:cfunction.ext";
        else
            from_name = from_m->resolved_name[0] ? from_m->resolved_name : from_m->requested_name;
    }
    if (strcmp(rel, "trampoline") == 0 && to_m && !to_m->entries.registered_helper) {
        printf("[LOADCODE_CONTRACT] stage=TRAMPOLINE_HOP target=0x%X r0=0x%X "
               "consumer_module=%s evidence=OBSERVED note=pre_register\n",
               target, r0, from_name);
        fflush(stdout);
        return;
    }
    if (!to_m) return;
    if (strcmp(rel, "internal_entry") != 0 && !to_m->entries.registered_helper &&
        !to_m->entries.observed_first_pc)
        return;
    emit_loadcode_and_proto(uc ? uc : g_mea.uc, to_m, target, r0, r1, r2, r3, caller_pc, from_name,
                            rel);
}

void ext_module_entry_abi_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                  uint32_t cpsr) {
    MeaEntrySession *e = &g_mea.entry;
    int rt = -1, rn = -1, is_load = 0;
    uint32_t imm = 0, word = 0;
    int thumb = (cpsr & (1u << 5)) != 0;
    if (!ext_module_entry_abi_enabled() || !regs) return;

    /* Any STR that may touch mr_c_function_P. */
    if (thumb) {
        uint16_t half;
        if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) {
            half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
            if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && !is_load &&
                rt >= 0 && rt < 16 && rn >= 0 && rn < 16)
                ext_module_entry_abi_on_store(regs[rt], regs[rn] + imm, module_id, pc);
        }
    } else if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &word)) {
        if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && !is_load && rt >= 0 &&
            rt < 16 && rn >= 0 && rn < 16)
            ext_module_entry_abi_on_store(regs[rt], regs[rn] + imm, module_id, pc);
    }

    if (e->helper && (pc & ~1u) == (e->helper & ~1u) && !g_mea.mr_helper_seen) {
        uint32_t r0 = regs[0];
        if (g_mea.p.p_guest && (r0 == g_mea.p.p_guest || (r0 > 0x10000u && regs[1] < 0x10000u))) {
            ext_module_entry_abi_on_mr_helper(e->helper, regs[1], g_mea.p.p_guest, r0, regs[1],
                                              regs[2], regs[3]);
        }
    }
    if (g_mea.sampling_entry_pc && e->entry_seen && g_mea.sample_count < GWY_MEA_PROTO_INSNS) {
        g_mea.sample_count++;
        if (thumb) {
            uint16_t half;
            if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) {
                half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
                if (ext_entry_decode_ldr_imm(half, 1, &rt, &rn, &imm, &is_load) && is_load &&
                    rn == 0 && imm == 0x28u)
                    e->accesses_r0_plus_28 = 1;
            }
        } else if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &word)) {
            if (ext_entry_decode_ldr_imm(word, 0, &rt, &rn, &imm, &is_load) && is_load && rn == 0 &&
                imm == 0x28u)
                e->accesses_r0_plus_28 = 1;
        }
    }
}
