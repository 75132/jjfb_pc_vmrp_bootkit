#ifndef GWY_LAUNCHER_EXT_ENTRY_ABI_CLUSTER_AUDIT_H
#define GWY_LAUNCHER_EXT_ENTRY_ABI_CLUSTER_AUDIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Phase 6L: MRPGCMAP entry ABI / init-cluster reachability audit.
 * Observe + small ABI variants only. Never invents P+0xC / R9 / UI.
 *
 * Env:
 *   JJFB_ENTRY_ABI_AUDIT=1
 *   JJFB_ENTRY_COVERAGE_TRACE=1
 *   JJFB_ENTRY_ABI_VARIANT=baseline|r0_p|r0_1_r1_p|r0_p_r1_param|mirror_callback_regs
 *   JJFB_ENTRY_INSN_LIMIT=<n>   (optional; default 200000)
 */

typedef enum GwyEntryAbiVariant {
    GWY_ENTRY_ABI_BASELINE = 0,
    GWY_ENTRY_ABI_R0_P = 1,
    GWY_ENTRY_ABI_R0_1_R1_P = 2,
    GWY_ENTRY_ABI_R0_P_R1_PARAM = 3,
    GWY_ENTRY_ABI_MIRROR_CALLBACK = 4
} GwyEntryAbiVariant;

void ext_entry_abi_cluster_audit_reset(void);
void ext_entry_abi_cluster_audit_bind_uc(void *uc);
int ext_entry_abi_cluster_audit_enabled(void);
GwyEntryAbiVariant ext_entry_abi_cluster_audit_variant(void);
const char *ext_entry_abi_cluster_audit_variant_name(GwyEntryAbiVariant v);

int ext_entry_abi_cluster_audit_in_entry(void);
void ext_entry_abi_cluster_audit_set_in_entry(int on);

/* Capture nested CFN boundary regs for mirror_callback_regs (from before_continuation). */
void ext_entry_abi_cluster_audit_capture_callback_regs(void *uc);

/* Known nested/global P from CFN observe. */
void ext_entry_abi_cluster_audit_on_p_candidate(uint32_t p_guest, const char *kind);
void ext_entry_abi_cluster_audit_on_p_write(uint32_t p_guest, uint32_t off, uint32_t old_v,
                                            uint32_t new_v, uint32_t pc, const char *module);

/* Code-path coverage while in documented entry emu. */
void ext_entry_abi_cluster_audit_on_code(void *uc, const char *module_name, uint32_t pc);

/* Bridge mr_exit observe (process still exits). */
void ext_entry_abi_cluster_audit_on_mr_exit(void *uc);

/*
 * Run documented entry with ABI variant. Returns 1 on UC_ERR_OK path (incl. stop),
 * 0 on hard UC error / skip. Emits PRE/RET/COVERAGE/LASTPCS.
 */
int ext_entry_abi_cluster_audit_run_documented(void *uc, const char *module_name, uint32_t code_base,
                                               uint32_t code_size, uint32_t entry_pc,
                                               uint32_t stop_addr, uint32_t start_pc_with_thumb);

void ext_entry_abi_cluster_audit_finalize(const char *stop_reason);

int ext_entry_abi_cluster_audit_cluster_reached(void);
int ext_entry_abi_cluster_audit_any_pxc_nonzero(void);
const char *ext_entry_abi_cluster_audit_last_end_reason(void);

#ifdef __cplusplus
}
#endif

#endif
