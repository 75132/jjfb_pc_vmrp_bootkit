#ifndef GWY_LAUNCHER_MODULE_R9_SWITCH_H
#define GWY_LAUNCHER_MODULE_R9_SWITCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GwyLoadedModule;

/* Phase 6C-A/C/D2/D3: Nested EXT R9 switch + tokenized scope balancing (D3). */

#define GWY_MAX_MODULE_CALL_DEPTH 16

typedef enum GwyModuleCallKind {
    GWY_CALL_BOOTSTRAP_ENTRY = 1, /* first CODE_IMAGE; callee ER_RW may be absent */
    GWY_CALL_RUNTIME_ENTRY = 2,   /* post-ER_RW module entry */
    GWY_CALL_MR_HELPER = 3,
    GWY_CALL_CALLBACK = 4,
    GWY_CALL_NESTED_EXT = 5
} GwyModuleCallKind;

typedef enum GwyEmuExitReason {
    GWY_EMU_EXIT_UNKNOWN = 0,
    GWY_EMU_EXIT_NORMAL_GUEST_RETURN = 1,
    GWY_EMU_EXIT_HOST_CALLBACK = 2,
    GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST = 3,
    GWY_EMU_EXIT_UC_FAULT = 4,
    GWY_EMU_EXIT_EXPLICIT_STOP = 5
} GwyEmuExitReason;

typedef enum GwyR9LeaveAction {
    GWY_R9_LEAVE_NOOP = 0,
    GWY_R9_LEAVE_POP_OWNED_FRAME = 1,
    GWY_R9_LEAVE_REJECT = 2,          /* owns_frame but token mismatch — fail closed */
    GWY_R9_LEAVE_POP_FOREIGN_FRAME = 3 /* legacy D2 observe only; D3 must not emit as pop */
} GwyR9LeaveAction;

typedef struct ModuleR9Frame {
    uint64_t frame_id;
    uint64_t generation;
    uint64_t owner_call_id;
    uint64_t owner_scope_id;
    uint64_t caller_module_id;
    uint64_t callee_module_id;
    GwyModuleCallKind call_kind;
    GwyModuleCallKind owner_kind;
    uint32_t saved_r9;
    uint32_t callee_r9;
    uint32_t depth;
    /* MRP→DSM: BL/BLX call site + return site in caller; 0 if not armed. */
    uint32_t dsm_call_pc;
    uint32_t dsm_return_pc;
    int dsm_helper_executed;
    int pushed;
    int active;
    char pushed_by[48];
    char expected_leave_by[48];
} ModuleR9Frame;

/* Scope token produced by every enter (even already_switched). */
typedef struct ModuleR9Scope {
    uint64_t scope_id;
    uint64_t frame_id;
    uint64_t generation;
    uint64_t caller_module_id;
    uint64_t callee_module_id;
    GwyModuleCallKind call_kind;
    int owns_frame;
    int enter_result; /* 1 switched+pushed, 0 no-op, -1 blocked/error */
    int frame_pushed;
} ModuleR9Scope;

void module_r9_switch_reset(void);
int module_r9_switch_enabled(void);
uint32_t module_r9_switch_depth(void);

/*
 * Returns 1 on switch applied, 0 on same-module/disabled/already_switched no-op,
 * -1 on blocked (CALLEE_ER_RW_NOT_AVAILABLE) or stack error.
 * Only mutates UC_ARM_REG_R9 when returning 1.
 * BOOTSTRAP_ENTRY never invents R9 when callee ER_RW is missing.
 */
int module_r9_switch_enter(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                           GwyModuleCallKind call_kind);

/* Same as enter; fills out_scope. pushed_by tags host callsite. */
int module_r9_switch_enter_ex(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                              GwyModuleCallKind call_kind, const char *pushed_by,
                              ModuleR9Scope *out_scope);

/* Legacy: leave using last enter scope (tokenized rules — no foreign pop). */
int module_r9_switch_leave(void *uc);

/*
 * Phase 6C-D3: tokenized leave.
 * owns_frame=false / YIELD_TO_NESTED_GUEST → NOOP.
 * owns_frame + matching top frame + NORMAL/HOST return → POP_OWNED.
 * owns_frame + mismatch → REJECT (no R9/stack change).
 */
int module_r9_switch_leave_scope(void *uc, const ModuleR9Scope *scope, const char *requested_by,
                                 GwyEmuExitReason emu_exit);

/* Compatibility wrapper → leave_scope with reconstructed token. */
int module_r9_switch_leave_ex(void *uc, const char *requested_by, uint64_t requesting_scope_id,
                              int owns_frame, GwyEmuExitReason emu_exit);

void module_r9_switch_abort(void *uc, const char *reason);

const char *gwy_module_call_kind_name(GwyModuleCallKind k);
const char *gwy_emu_exit_reason_name(GwyEmuExitReason r);
const char *gwy_r9_leave_action_name(GwyR9LeaveAction a);

GwyModuleCallKind gwy_module_call_kind_for_entry(const struct GwyLoadedModule *callee);

void module_r9_switch_last_enter_scope(ModuleR9Scope *out);
int module_r9_switch_peek_top(ModuleR9Frame *out);
/* Read-only: frame at stack index 0..depth-1 (0=oldest). Returns 1 if valid. */
int module_r9_switch_peek_at(uint32_t depth_index, ModuleR9Frame *out);
uint64_t module_r9_switch_event_seq(void);

void module_r9_switch_set_guest_context(uint32_t guest_pc, const char *guest_module);
void module_r9_switch_set_emu_exit_hint(GwyEmuExitReason reason);

/* Mark that guest executed at least one insn inside DSM after MRP→DSM enter. */
void module_r9_switch_note_dsm_code(uint64_t module_id);

/* If PC is in DSM but R9 is an MRP ER_RW, force DSM R9. Returns 1 if corrected. */
int module_r9_switch_ensure_dsm_r9(void *uc, uint32_t guest_pc);

/* Minimal UC_HOOK_CODE on DSM image: note executed + ensure DSM R9 (not full GCO). */
void module_r9_switch_attach_dsm_r9_guard(void *uc, uint64_t dsm_module_id, uint32_t code_base,
                                         uint32_t code_size);

/*
 * Arm / refresh expected return PC for MRP→DSM helper (BLX site).
 * Call on Pre-BLX enter and on already_switched re-enter. cpsr bit5 = Thumb.
 */
void module_r9_switch_arm_dsm_helper_return(void *uc, uint64_t caller_module_id,
                                           uint32_t call_pc, uint32_t cpsr);

/*
 * After MRP→DSM helper returns to the armed return PC, pop the DSM enter frame
 * (YIELD leave is NOOP). Requires note_dsm_code + PC match. Returns 1 if restored.
 */
int module_r9_switch_restore_after_dsm_helper(void *uc, uint64_t caller_module_id,
                                               uint32_t guest_pc);

/*
 * Host skipped an MRP→DSM BLX (e.g. E9D memcpy/strcmp shim). Disarm the matching
 * side-stack site and optionally restore caller R9 so later helpers can arm again.
 * Returns 1 if a site was cancelled.
 */
int module_r9_switch_cancel_dsm_helper_blx(void *uc, uint32_t call_pc, uint32_t restore_r9);

/* Drop all pending DSM return sites (after mid-function host complete). */
void module_r9_switch_clear_dsm_return_side_stack(void);

#ifdef __cplusplus
}
#endif

#endif
