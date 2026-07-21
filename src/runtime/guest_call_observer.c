#include "gwy_launcher/guest_call_observer.h"
#include "gwy_launcher/e10a_shell_trace.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_helper_handoff.h"
#include "gwy_launcher/ext_dsm_record_observe.h"
#include "gwy_launcher/ext_module_entry_abi.h"
#include "gwy_launcher/ext_entry_null_contract.h"
#include "gwy_launcher/ext_module_data_init.h"
#include "gwy_launcher/ext_er_rw_producer.h"
#include "gwy_launcher/ext_bootstrap_abi.h"
#include "gwy_launcher/ext_callback_frame.h"
#include "gwy_launcher/ext_r9_scope_audit.h"
#include "gwy_launcher/ext_post_cont_audit.h"
#include "gwy_launcher/ext_post_cfn_r9_audit.h"
#include "gwy_launcher/ext_p_extchunk_audit.h"
#include "gwy_launcher/ext_gwy_startgame_audit.h"
#include "gwy_launcher/ext_gwy_shell_native_exec.h"
#include "gwy_launcher/ext_mrpgcmap_entry_order.h"
#include "gwy_launcher/ext_entry_abi_cluster_audit.h"
#include "gwy_launcher/ext_cfunction_publication_audit.h"
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

#define GWY_GCO_WATCH_MAX 8

typedef struct {
    int in_use;
    int entry_seen;
    int xfer_seen; /* A5: first PC cross-module note (may precede REGISTERED) */
    int thunk_seen;
    uint64_t module_id;
    uint32_t code_base;
    uint32_t code_size;
    uint32_t helper_address;
#ifdef GWY_HAVE_UNICORN
    uc_hook hook;
#endif
} GcoWatch;

struct GuestCallObserver {
    ExtLoader *loader;
    void *uc;
    GcoWatch watches[GWY_GCO_WATCH_MAX];
};

static GuestCallObserver g_gco;
static int g_gco_inited;

GuestCallObserver *guest_call_observer_create(void) {
    return (GuestCallObserver *)calloc(1, sizeof(GuestCallObserver));
}

void guest_call_observer_destroy(GuestCallObserver *obs) {
    size_t i;
    if (!obs) return;
#ifdef GWY_HAVE_UNICORN
    if (obs->uc) {
        for (i = 0; i < GWY_GCO_WATCH_MAX; i++) {
            if (obs->watches[i].in_use && obs->watches[i].hook) {
                uc_hook_del((uc_engine *)obs->uc, obs->watches[i].hook);
            }
        }
    }
#endif
    if (obs != &g_gco) free(obs);
    else memset(obs, 0, sizeof(*obs));
}

void guest_call_observer_bind(GuestCallObserver *obs, ExtLoader *loader, void *uc_engine) {
    if (!obs) return;
    obs->loader = loader;
    obs->uc = uc_engine;
}

GuestCallObserver *gwy_guest_call_observer_ensure(void) {
    if (!g_gco_inited) {
        memset(&g_gco, 0, sizeof(g_gco));
        g_gco_inited = 1;
    }
    return &g_gco;
}

void gwy_guest_call_observer_bind_uc(void *uc_engine) {
    GuestCallObserver *o = gwy_guest_call_observer_ensure();
    o->uc = uc_engine;
    o->loader = gwy_ext_loader_ensure();
}

#ifdef GWY_HAVE_UNICORN
static void gco_read_regs(uc_engine *uc, uint32_t regs[16], uint32_t *cpsr) {
    int i;
    for (i = 0; i < 13; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &regs[13]);
    uc_reg_read(uc, UC_ARM_REG_LR, &regs[14]);
    uc_reg_read(uc, UC_ARM_REG_PC, &regs[15]);
    if (cpsr) uc_reg_read(uc, UC_ARM_REG_CPSR, cpsr);
}

static const GwyLoadedModule *gco_find_call_target(ModuleRegistry *reg, uint32_t target) {
    const GwyLoadedModule *m;
    uint32_t t = target & ~1u;
    if (!reg || t == 0) return NULL;
    m = module_registry_find_by_helper(reg, target);
    if (!m) m = module_registry_find_by_helper(reg, t);
    if (!m) m = module_registry_find_by_helper(reg, t | 1u);
    if (m && m->state >= GWY_MODULE_REGISTERED) return m;
    m = module_registry_find_by_code_addr(reg, t);
    if (m && m->origin == MODULE_ORIGIN_MRP_MEMBER && m->state >= GWY_MODULE_REGISTERED) return m;
    return NULL;
}

static int gco_try_decode_branch_target(uc_engine *uc,
                                        uint32_t pc,
                                        uint32_t cpsr,
                                        const uint32_t regs[16],
                                        uint32_t *out_target,
                                        const char **out_kind,
                                        int *out_rm) {
    int thumb = (cpsr & (1u << 5)) != 0;
    uint32_t word = 0;
    int rm = -1;

    if (out_target) *out_target = 0;
    if (out_kind) *out_kind = NULL;
    if (out_rm) *out_rm = -1;

    if (thumb) {
        uint16_t half;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) return 0;
        half = (uint16_t)((pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
        if (ext_entry_decode_thumb_blx_rm(half, &rm) && rm >= 0 && rm < 16) {
            if (out_target) *out_target = regs[rm];
            if (out_kind) *out_kind = "blx_rm";
            if (out_rm) *out_rm = rm;
            return 1;
        }
        if (ext_entry_decode_thumb_bx_rm(half, &rm) && rm >= 0 && rm < 16) {
            if (out_target) *out_target = regs[rm];
            if (out_kind) *out_kind = "bx_rm";
            if (out_rm) *out_rm = rm;
            return 1;
        }
        /* Thumb BL/BLX immediate: two halfwords starting with 0xF000 / 0xF8xx */
        if ((half & 0xF800u) == 0xF000u) {
            uint32_t word2 = 0;
            uint16_t hi;
            uint32_t next = (pc + 2u) & ~1u;
            if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, next & ~3u, &word2)) return 0;
            hi = (uint16_t)((next & 2u) ? (word2 >> 16) : (word2 & 0xFFFFu));
            if ((hi & 0xD000u) == 0xD000u) {
                /* BL */
                int32_t imm32;
                uint32_t S = (half >> 10) & 1u;
                uint32_t imm10 = half & 0x3FFu;
                uint32_t J1 = (hi >> 13) & 1u;
                uint32_t J2 = (hi >> 11) & 1u;
                uint32_t imm11 = hi & 0x7FFu;
                uint32_t I1 = (J1 ^ S) ^ 1u;
                uint32_t I2 = (J2 ^ S) ^ 1u;
                imm32 = (int32_t)((S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) |
                                  (imm11 << 1));
                if (S) imm32 |= (int32_t)0xFE000000;
                if (out_target) *out_target = (pc + 4u + (uint32_t)imm32) | 1u;
                if (out_kind) *out_kind = "bl_imm";
                return 1;
            }
            if ((hi & 0xD000u) == 0xC000u) {
                /* BLX */
                int32_t imm32;
                uint32_t S = (half >> 10) & 1u;
                uint32_t imm10 = half & 0x3FFu;
                uint32_t J1 = (hi >> 13) & 1u;
                uint32_t J2 = (hi >> 11) & 1u;
                uint32_t imm11 = (hi & 0x7FFu) & ~1u;
                uint32_t I1 = (J1 ^ S) ^ 1u;
                uint32_t I2 = (J2 ^ S) ^ 1u;
                imm32 = (int32_t)((S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | imm11);
                if (S) imm32 |= (int32_t)0xFE000000;
                if (out_target) *out_target = ((pc + 4u + (uint32_t)imm32) & ~3u);
                if (out_kind) *out_kind = "blx_imm";
                return 1;
            }
        }
        return 0;
    }

    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &word)) return 0;
    if (ext_entry_decode_arm_blx_rm(word, &rm) && rm >= 0 && rm < 16) {
        if (out_target) *out_target = regs[rm];
        if (out_kind) *out_kind = "arm_blx_rm";
        if (out_rm) *out_rm = rm;
        return 1;
    }
    if (ext_entry_decode_arm_bx_rm(word, &rm) && rm >= 0 && rm < 16) {
        if (out_target) *out_target = regs[rm];
        if (out_kind) *out_kind = "arm_bx_rm";
        if (out_rm) *out_rm = rm;
        return 1;
    }
    /* ARM BL / BLX imm */
    if ((word & 0x0F000000u) == 0x0B000000u) {
        int32_t imm24 = (int32_t)(word & 0x00FFFFFFu);
        if (imm24 & 0x00800000) imm24 |= (int32_t)0xFF000000;
        if (out_target) *out_target = pc + 8u + (uint32_t)(imm24 << 2);
        if (out_kind) *out_kind = "arm_bl_imm";
        return 1;
    }
    if ((word & 0xFE000000u) == 0xFA000000u) {
        int32_t imm24 = (int32_t)(word & 0x00FFFFFFu);
        uint32_t H = (word >> 24) & 1u;
        if (imm24 & 0x00800000) imm24 |= (int32_t)0xFF000000;
        if (out_target) *out_target = (pc + 8u + (uint32_t)(imm24 << 2) + (H << 1)) | 1u;
        if (out_kind) *out_kind = "arm_blx_imm";
        return 1;
    }
    return 0;
}

static void gco_on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    GcoWatch *w = (GcoWatch *)user_data;
    GuestCallObserver *obs = gwy_guest_call_observer_ensure();
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    uint32_t pc = (uint32_t)address;
    uint32_t regs[16];
    uint32_t cpsr = 0;
    uint32_t target = 0;
    const char *branch_kind = NULL;
    int branch_rm = -1;
    (void)size;

    if (!w || !w->in_use || !obs->loader) return;
    if (pc < w->code_base || pc >= w->code_base + w->code_size) return;

    gco_read_regs(uc, regs, &cpsr);
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, w->module_id) : NULL;
    e10a_note_guest_pc_sample(pc, regs[0], regs[1], regs[14]);

    if (m && m->origin == MODULE_ORIGIN_DSM)
        module_r9_switch_note_dsm_code(m->module_id);

    /* MRP→DSM helper enter switches R9; YIELD leave is NOOP. Restore on return PC. */
    if (m && m->origin == MODULE_ORIGIN_MRP_MEMBER &&
        module_r9_switch_restore_after_dsm_helper(uc, m->module_id, pc) == 1) {
        gco_read_regs(uc, regs, &cpsr);
        /* Discriminate DSM heap helper returns (e.g. 0x80160→A17D0) that yield low r0. */
        if (regs[0] < 0x1000u) {
            printf("[JJFB_DSM_HELPER_RET] r0=0x%X r9=0x%X pc=0x%X evidence=OBSERVED "
                   "note=low_pointer_or_status\n",
                   regs[0], regs[9], pc);
            fflush(stdout);
        }
    }

    /* A5: DISPATCH_CALL EXIT + MODULE_POINTER_WRITE (observe-only). */
    ext_object_dispatch_on_code(uc, w->module_id, pc, regs, cpsr);
    /* A6: CFUNCTION_NEW EXIT + HELPER_HANDOFF STR watch. */
    ext_helper_handoff_on_code(uc, w->module_id, pc, regs, cpsr);
    /* A7: DSM module-record write provenance. */
    ext_dsm_record_on_code(uc, w->module_id, pc, regs, cpsr);
    /* A8: module-entry ABI / mr_helper path. */
    ext_module_entry_abi_on_code(uc, w->module_id, pc, regs, cpsr);
    /* A9: NULL-argument contract discrimination. */
    ext_entry_null_contract_on_code(uc, w->module_id, pc, regs, cpsr);
    /* A10: EXT module data initialization contract. */
    ext_module_data_init_on_code(uc, w->module_id, pc, regs, cpsr);
    /* 6C-B: ER_RW producer timing / first-entry ordering. */
    ext_er_rw_producer_on_code(uc, w->module_id, pc, regs, cpsr);
    /* 6C-C: bootstrap entry R9 ABI. */
    ext_bootstrap_abi_on_code(uc, w->module_id, pc, regs, cpsr);
    /* 6C-D1: callback continuation frame audit. */
    ext_callback_frame_on_code(uc, w->module_id, pc, regs, cpsr);
    /* 6C-D2: nested R9 scope prologue watch. */
    ext_r9_scope_audit_on_code(uc, pc, regs, cpsr);
    /* 6D-A: post-continuation progress audit. */
    ext_post_cont_audit_on_code(uc, w->module_id, pc, regs, cpsr);
    /* 6D-B: post-CFN R9 promotion audit. */
    ext_post_cfn_r9_audit_on_code(uc, w->module_id, pc, regs, cpsr);
    /* 6E: P.mrc_extChunk provider audit. */
    ext_p_extchunk_audit_on_code(uc, w->module_id, pc, regs, cpsr);
    /* 6H: shell native exec / export call watch. */
    ext_gwy_shell_native_exec_on_code(uc, w->module_id, m ? m->requested_name : NULL, pc, regs);
    /* 6K: MRPGCMAP entry-hit while running documented init. */
    ext_mrpgcmap_entry_order_on_code(uc, m ? m->requested_name : NULL, pc);
    ext_entry_abi_cluster_audit_on_code(uc, m ? m->requested_name : NULL, pc);
    ext_cfunction_publication_audit_on_code(uc, m ? m->requested_name : NULL, pc, regs, cpsr);
    module_r9_switch_set_guest_context(pc, NULL);

    /* A5: observe DSM→module transfer on first PC even while still MAPPED
     * (robotol often runs before REGISTERED / _mr_c_function_new). */
    if (m && m->origin == MODULE_ORIGIN_MRP_MEMBER && !w->xfer_seen &&
        m->state >= GWY_MODULE_MAPPED) {
        uint32_t lr = regs[14];
        w->xfer_seen = 1;
        if (lr && reg) {
            const GwyLoadedModule *caller = module_registry_find_by_code_addr(reg, lr & ~1u);
            if (caller && caller->module_id != m->module_id) {
                /* Phase 6C-C: BOOTSTRAP vs RUNTIME entry kind (observe). */
                GwyModuleCallKind ck = gwy_module_call_kind_for_entry(m);
                int sw = module_r9_switch_enter(uc, caller->module_id, m->module_id, ck);
                ext_er_rw_producer_on_r9_switch(uc, caller->module_id, m->module_id, ck, sw);
                ext_bootstrap_abi_on_r9_switch(uc, caller->module_id, m->module_id, ck, sw, pc,
                                               regs);
                ext_callback_frame_on_r9_switch(uc, caller->module_id, m->module_id, ck, sw, pc,
                                               regs);
                ext_object_note_cross_module_call_ex(caller->module_id, lr, m->module_id, pc, regs,
                                                    cpsr, uc);
            }
        }
    }

    /* THUNK_ENTER: first hit of registered helper address inside this module. */
    if (m && m->origin == MODULE_ORIGIN_MRP_MEMBER && w->helper_address && !w->thunk_seen) {
        uint32_t h = w->helper_address & ~1u;
        if ((pc & ~1u) == h) {
            uint32_t lr = regs[14];
            const GwyLoadedModule *caller =
                (lr && reg) ? module_registry_find_by_code_addr(reg, lr & ~1u) : NULL;
            w->thunk_seen = 1;
            {
                int sw = module_r9_switch_enter(uc, caller ? caller->module_id : 0, m->module_id,
                                               GWY_CALL_MR_HELPER);
                ext_er_rw_producer_on_r9_switch(uc, caller ? caller->module_id : 0, m->module_id,
                                                GWY_CALL_MR_HELPER, sw);
            }
            ext_entry_observe_helper_abi(HELPER_ABI_STAGE_THUNK_ENTER, w->module_id, pc,
                                         w->helper_address, pc, regs, cpsr, "GUEST_NESTED", uc);
            {
                const char *mt = getenv("JJFB_MRC_INIT_TRACE");
                const char *mn = m->resolved_name[0] ? m->resolved_name : m->requested_name;
                if (mt && mt[0] == '1' && regs[1] == 0u && mn &&
                    (strcmp(mn, "robotol.ext") == 0 || strcmp(mn, "mmochat.ext") == 0)) {
                    printf("[JJFB_MRC_INIT_ATTEMPT] module=%s pc=0x%X method=0 r0=0x%X r9=0x%X "
                           "route=guest_helper_thunk evidence=DOCUMENTED "
                           "source=mythroad.c:case_801\n",
                           mn, pc, regs[0], regs[9]);
                    fflush(stdout);
                }
            }
        }
    }

    /* First instruction after REGISTERED → ENTRY_CALLED (MRP only). */
    if (m && m->origin == MODULE_ORIGIN_MRP_MEMBER &&
        (m->state == GWY_MODULE_REGISTERED || m->state == GWY_MODULE_ENTRY_CALLED) &&
        !w->entry_seen) {
        uint32_t lr = regs[14];
        w->entry_seen = 1;
        ext_entry_observe_on_entry_begin(w->module_id, pc, 0, regs[0], regs[1], regs[2], regs[3],
                                         regs[9], regs[13], lr, cpsr);
        ext_mrpgcmap_entry_order_on_first_pc(m->requested_name, w->code_base, pc);
        /* Cross-module enter: LR module ≠ this module. */
        if (lr && reg) {
            const GwyLoadedModule *caller = module_registry_find_by_code_addr(reg, lr & ~1u);
            if (caller && caller->module_id != m->module_id) {
                ext_object_note_cross_module_call_ex(caller->module_id, lr, m->module_id, pc, regs,
                                                    cpsr, uc);
            }
        }
        /* Recover CALL_SITE from LR (DSM not fully hooked). Regs are ENTER snapshot —
         * label origin so Case A is not falsely claimed without a true pre-BL sample. */
        if (lr) {
            int thumb = (cpsr & (1u << 5)) != 0;
            uint32_t call_pc = thumb ? ((lr & ~1u) - 2u) : (lr - 4u);
            ext_entry_observe_helper_abi(HELPER_ABI_STAGE_CALL_SITE_BEFORE, w->module_id, call_pc,
                                         w->helper_address, pc, regs, cpsr, "LR_PROXY", uc);
        }
    }

    if (m && m->origin == MODULE_ORIGIN_MRP_MEMBER &&
        (m->state == GWY_MODULE_ENTRY_CALLED || m->state == GWY_MODULE_REGISTERED ||
         m->state == GWY_MODULE_FAILED)) {
        ext_entry_observe_push_insn(w->module_id, pc, regs, cpsr);
    }

    /* CALL_SITE_BEFORE: BL/BLX/BX toward nested EXT helper/region or mr_open stub. */
    if (gco_try_decode_branch_target(uc, pc, cpsr, regs, &target, &branch_kind, &branch_rm) &&
        target) {
        const GwyLoadedModule *tgt = gco_find_call_target(reg, target);
        const GwyLoadedModule *from = reg ? module_registry_find_by_id(reg, w->module_id) : NULL;
        const GwyLoadedModule *to_by_addr =
            reg ? module_registry_find_by_code_addr(reg, target & ~1u) : NULL;
        if (e10a_is_mr_open_stub(target)) {
            e10a_note_open_enter_branch(pc, target, branch_kind ? branch_kind : "?", branch_rm,
                                        regs[0], regs[1], regs[9], regs[14]);
        }
        if (to_by_addr && from && to_by_addr->module_id != from->module_id) {
            /* Pre-BLX: switch R9 for cross-module target. */
            GwyModuleCallKind ck;
            int sw;
            if (to_by_addr->entries.registered_helper &&
                (target & ~1u) == (to_by_addr->entries.registered_helper & ~1u))
                ck = GWY_CALL_MR_HELPER;
            else
                ck = gwy_module_call_kind_for_entry(to_by_addr);
            sw = module_r9_switch_enter(uc, from->module_id, to_by_addr->module_id, ck);
            ext_er_rw_producer_on_r9_switch(uc, from->module_id, to_by_addr->module_id, ck, sw);
            ext_bootstrap_abi_on_r9_switch(uc, from->module_id, to_by_addr->module_id, ck, sw, pc,
                                           regs);
            ext_callback_frame_on_r9_switch(uc, from->module_id, to_by_addr->module_id, ck, sw, pc,
                                           regs);
            /* Arm return PC after push or nested DSM balance frame. */
            if (sw >= 0 && from->origin == MODULE_ORIGIN_MRP_MEMBER &&
                to_by_addr->origin == MODULE_ORIGIN_DSM)
                module_r9_switch_arm_dsm_helper_return(uc, from->module_id, pc, cpsr);
            ext_object_note_cross_module_call_ex(from->module_id, pc, to_by_addr->module_id, target,
                                                regs, cpsr, uc);
        } else if (tgt && from && tgt->module_id != from->module_id) {
            int sw = module_r9_switch_enter(uc, from->module_id, tgt->module_id, GWY_CALL_MR_HELPER);
            ext_er_rw_producer_on_r9_switch(uc, from->module_id, tgt->module_id, GWY_CALL_MR_HELPER,
                                            sw);
            if (sw >= 0 && from->origin == MODULE_ORIGIN_MRP_MEMBER &&
                tgt->origin == MODULE_ORIGIN_DSM)
                module_r9_switch_arm_dsm_helper_return(uc, from->module_id, pc, cpsr);
            ext_object_note_cross_module_call_ex(from->module_id, pc, tgt->module_id, target, regs,
                                                cpsr, uc);
        }
        if (tgt && tgt->origin == MODULE_ORIGIN_MRP_MEMBER) {
            printf("[GUEST_INDIRECT_CALL] module_id=%llu pc=0x%X target=0x%X arg0=0x%X arg1=0x%X\n",
                   (unsigned long long)w->module_id, pc, target, regs[0], regs[1]);
            fflush(stdout);
            ext_entry_observe_helper_abi(HELPER_ABI_STAGE_CALL_SITE_BEFORE, w->module_id, pc,
                                         tgt->map.helper_address, target, regs, cpsr, "GUEST_CALL",
                                         uc);
        }

        /* Legacy: BLX Rm with r0 in own code + small r1 → c_function_new heuristic. */
        if (regs[0] >= w->code_base && regs[0] < w->code_base + w->code_size && regs[1] > 0 &&
            regs[1] <= 0x100) {
            int rm = -1;
            uint32_t insn = 0;
            int is_blx = 0;
            if ((cpsr & (1u << 5)) != 0) {
                uint16_t half;
                if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &insn)) {
                    half = (uint16_t)((pc & 2u) ? (insn >> 16) : (insn & 0xFFFFu));
                    is_blx = ext_entry_decode_thumb_blx_rm(half, &rm);
                }
            } else if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &insn)) {
                is_blx = ext_entry_decode_arm_blx_rm(insn, &rm);
            }
            if (is_blx) {
                const char *modn = "unknown";
                const char *calln = "unknown";
                uint32_t call_off = 0;
                if (from) {
                    calln = from->origin == MODULE_ORIGIN_DSM
                                ? "dsm:cfunction.ext"
                                : (from->resolved_name[0] ? from->resolved_name
                                                          : from->requested_name);
                    if (from->map.guest_code_base && (pc & ~1u) >= from->map.guest_code_base)
                        call_off = (pc & ~1u) - from->map.guest_code_base;
                }
                if (to_by_addr)
                    modn = to_by_addr->resolved_name[0] ? to_by_addr->resolved_name
                                                        : to_by_addr->requested_name;
                else if (m)
                    modn = m->resolved_name[0] ? m->resolved_name : m->requested_name;
                ext_helper_handoff_cfunction_enter("GUEST_NESTED", regs[0], regs[1], 0, regs, cpsr,
                                                   pc, calln, call_off, modn);
                ext_dsm_record_note_cfunction_side_effects(regs[0], 0, regs[1], "GUEST_NESTED");
                ext_loader_on_c_function_new(obs->loader, regs[0], regs[1], 0, 0, 0, 0);
                ext_callback_frame_on_guest_cfn_call(uc, pc, target ? target : regs[2], regs, cpsr,
                                                     from ? from->module_id : w->module_id);
                ext_r9_scope_audit_on_nested_cfn(pc, target ? target : regs[0], regs[9],
                                                 from ? from->module_id : w->module_id);
                printf("[GUEST_CFUNCTION_NEW] module_id=%llu helper=0x%X p_len=%u "
                       "reason=region_indirect\n",
                       (unsigned long long)w->module_id, regs[0], regs[1]);
                fflush(stdout);
            }
        }
    }
}
#endif

LauncherStatus guest_call_observer_attach_module(GuestCallObserver *obs,
                                                 uint64_t module_id,
                                                 LauncherError *err) {
    size_t i;
    GcoWatch *slot = NULL;
    const GwyLoadedModule *m;
    ModuleRegistry *reg;

    launcher_error_clear(err);
    if (!obs) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_call_observer", "null", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (!obs->loader) obs->loader = gwy_ext_loader_ensure();
    reg = gwy_ext_loader_bound_registry();
    if (!reg) {
        launcher_error_set(err, L_ERR_STATE, "guest_call_observer", "no registry", NULL);
        return L_ERR_STATE;
    }
    m = module_registry_find_by_id(reg, module_id);
    if (!m || m->map.guest_code_base == 0 || m->map.guest_code_size == 0) {
        launcher_error_set(err, L_ERR_STATE, "guest_call_observer", "module not mapped", NULL);
        return L_ERR_STATE;
    }

    for (i = 0; i < GWY_GCO_WATCH_MAX; i++) {
        if (obs->watches[i].in_use && obs->watches[i].module_id == module_id) {
            obs->watches[i].code_base = m->map.guest_code_base;
            obs->watches[i].code_size = m->map.guest_code_size;
            obs->watches[i].helper_address = m->map.helper_address;
            return L_OK;
        }
        if (!obs->watches[i].in_use && !slot) slot = &obs->watches[i];
    }
    if (!slot) {
        launcher_error_set(err, L_ERR_BOUNDS, "guest_call_observer", "watch table full", NULL);
        return L_ERR_BOUNDS;
    }
    memset(slot, 0, sizeof(*slot));
    slot->in_use = 1;
    slot->entry_seen = 0;
    slot->xfer_seen = 0;
    slot->thunk_seen = 0;
    slot->module_id = module_id;
    slot->code_base = m->map.guest_code_base;
    slot->code_size = m->map.guest_code_size;
    slot->helper_address = m->map.helper_address;

#ifdef GWY_HAVE_UNICORN
    if (obs->uc) {
        uc_err ue = uc_hook_add((uc_engine *)obs->uc, &slot->hook, UC_HOOK_CODE,
                                (void *)gco_on_code, slot, (uint64_t)slot->code_base,
                                (uint64_t)slot->code_base + (uint64_t)slot->code_size - 1ull);
        if (ue != UC_ERR_OK) {
            slot->in_use = 0;
            launcher_error_set(err, L_ERR_STATE, "guest_call_observer", "uc_hook_add failed", NULL);
            return L_ERR_STATE;
        }
    }
#endif
    printf("[GUEST_CALL_OBSERVER] attach module_id=%llu code=[0x%X,0x%X) helper=0x%X "
           "observe_only=1\n",
           (unsigned long long)module_id, slot->code_base, slot->code_base + slot->code_size,
           slot->helper_address);
    fflush(stdout);
    return L_OK;
}
