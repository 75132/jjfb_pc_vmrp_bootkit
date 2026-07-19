#ifndef GWY_LAUNCHER_EXT_CALLBACK_FRAME_H
#define GWY_LAUNCHER_EXT_CALLBACK_FRAME_H

#include "gwy_launcher/module_r9_switch.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6C-D1: Guest Callback Continuation Frame Audit (observe-only). */

typedef enum CallbackFrameClass {
    CF_CLASS_UNKNOWN = 0,
    CF_CLASS_CONTINUATION_CONFIRMED_R9_LOST = 1,
    CF_CLASS_CONTINUATION_CONFIRMED_R9_INTACT = 2,
    CF_CLASS_GUEST_WROTE_R9 = 3,
    CF_CLASS_PRE_R9_ALREADY_ZERO = 4,
    CF_CLASS_NOT_CFN_OR_INCONCLUSIVE = 5
} CallbackFrameClass;

typedef struct GuestCallbackBoundarySnap {
    uint64_t callback_id;
    uint64_t guest_module_id;
    uint32_t callback_slot;
    char slot_name[48];
    uint32_t call_pc;
    uint32_t continuation_pc;
    uint32_t r[13];
    uint32_t sp;
    uint32_t lr;
    uint32_t cpsr;
} GuestCallbackBoundarySnap;

void ext_callback_frame_reset(void);
int ext_callback_frame_enabled(void);
void ext_callback_frame_bind_uc(void *uc);

const char *ext_callback_frame_class_name(CallbackFrameClass c);
CallbackFrameClass ext_callback_frame_last_class(void);
int ext_callback_frame_gate_open(void);

/* True if pc matches a captured CFN continuation (observe query). */
int ext_callback_frame_is_cfn_continuation(uint32_t pc, uint64_t *out_module_id,
                                           uint32_t *out_continuation);

void ext_callback_frame_on_host_enter(void *uc, uint32_t slot_addr, const char *name);
void ext_callback_frame_on_host_leave(void *uc, uint32_t slot_addr, const char *name);
void ext_callback_frame_on_host_resume(void *uc, uint32_t slot_addr, const char *name);

/* Nested EXT→DSM/host CFN (GCO) and LOG_PARSE observe paths. */
void ext_callback_frame_on_guest_cfn_call(void *uc, uint32_t call_pc, uint32_t target,
                                          const uint32_t regs[16], uint32_t cpsr,
                                          uint64_t caller_module_id);
void ext_callback_frame_on_cfunction_observe(void *uc, uint32_t helper, uint32_t p_len,
                                             const char *origin);

void ext_callback_frame_on_r9_switch(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                                     GwyModuleCallKind call_kind, int enter_rc, uint32_t pc,
                                     const uint32_t regs[16]);

/* D2: scope-level R9 leave interference (not continuation-PC-only). */
void ext_callback_frame_on_r9_scope_leave(const char *requested_by, GwyR9LeaveAction action,
                                          uint64_t top_frame_id, GwyModuleCallKind top_kind);

void ext_callback_frame_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                uint32_t cpsr);

void ext_callback_frame_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                     uint32_t access_size, const uint32_t regs[16], uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
