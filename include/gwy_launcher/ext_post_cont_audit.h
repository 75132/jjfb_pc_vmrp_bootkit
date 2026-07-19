#ifndef GWY_LAUNCHER_EXT_POST_CONT_AUDIT_H
#define GWY_LAUNCHER_EXT_POST_CONT_AUDIT_H

#include "gwy_launcher/module_r9_switch.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6D-A: Post-Continuation Runtime Progress Audit (observe-only). */

typedef enum PostContClass {
    PC_CLASS_UNKNOWN = 0,
    PC_CLASS_RUNTIME_PROGRESS_TO_PLATFORM_API = 1,
    PC_CLASS_MODULE_ER_RW_RUNTIME_GAP = 2,
    PC_CLASS_MISSING_EVENT_SCHEDULING = 3,
    PC_CLASS_MISSING_GRAPHICS_REFRESH = 4,
    PC_CLASS_GRAPHICS_PIPELINE_ACTIVE = 5,
    PC_CLASS_NORMAL_IDLE = 6,
    PC_CLASS_NEW_ABI_FAULT = 7,
    PC_CLASS_HARNESS_WINDOW_TOO_SHORT = 8
} PostContClass;

typedef enum PostContEventType {
    PC_EVT_NONE = 0,
    PC_EVT_ROBOTOL_CONTINUATION = 1,
    PC_EVT_NORMAL_RETURN = 2,
    PC_EVT_YIELD = 3,
    PC_EVT_EVENT_WAIT = 4,
    PC_EVT_PLATFORM_API = 5,
    PC_EVT_DRAW_CALL = 6,
    PC_EVT_REFRESH_CALL = 7,
    PC_EVT_FILE_REQUEST = 8,
    PC_EVT_TIMER_REQUEST = 9,
    PC_EVT_NETWORK_REQUEST = 10,
    PC_EVT_NEW_FAULT = 11,
    PC_EVT_PROGRESS = 12,
    PC_EVT_HELPER_CALL = 13,
    PC_EVT_RUNTIME_READY = 14
} PostContEventType;

void ext_post_cont_audit_reset(void);
int ext_post_cont_audit_enabled(void);
void ext_post_cont_audit_bind_uc(void *uc);

const char *ext_post_cont_class_name(PostContClass c);
const char *ext_post_cont_event_name(PostContEventType t);
PostContClass ext_post_cont_audit_last_class(void);
int ext_post_cont_audit_armed(void);
uint64_t ext_post_cont_audit_instruction_count(void);

/* Gates: all blocked until final classification opens post_continuation_gate. */
int ext_post_cont_gate_open(void);
int ext_post_cont_graphics_gate_open(void);
int ext_post_cont_event_scheduler_gate_open(void);

/* Arm on robotol nested CFN continuation resume (observe-only). */
void ext_post_cont_audit_on_continuation_resume(void *uc, uint64_t module_id, const char *module,
                                                uint32_t call_pc, uint32_t continuation_pc,
                                                const uint32_t regs[16], uint32_t sp, uint32_t lr,
                                                uint32_t cpsr);

void ext_post_cont_audit_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                 uint32_t cpsr);

void ext_post_cont_audit_on_host_api(void *uc, uint32_t slot_addr, const char *api_name,
                                     int is_enter, uint32_t result_r0);

void ext_post_cont_audit_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                        uint32_t rw_base, uint32_t rw_size);

void ext_post_cont_audit_on_helper_call(uint32_t helper, uint32_t method, uint32_t r0, uint32_t r9);

void ext_post_cont_audit_on_r9_leave(const char *requested_by, GwyR9LeaveAction action,
                                     GwyEmuExitReason emu_exit, uint64_t top_frame_id,
                                     GwyModuleCallKind top_kind, uint32_t restore_r9);

void ext_post_cont_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                      const uint32_t regs[16], uint32_t cpsr);

/* Host-driven lifecycle/timer/event observe (not guest injection). */
void ext_post_cont_audit_on_host_event(const char *event_name, int32_t code);

/* Mark unimplemented via a dedicated path so classification can open FIRST_MISSING. */
void ext_post_cont_audit_note_unimplemented(const char *api_name);

/* Emit summary if armed and not yet finalized (harness end / process teardown). */
void ext_post_cont_audit_finalize(const char *stop_reason);

#ifdef __cplusplus
}
#endif

#endif
