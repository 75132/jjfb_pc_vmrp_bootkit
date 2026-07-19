#ifndef GWY_LAUNCHER_EXT_R9_SCOPE_AUDIT_H
#define GWY_LAUNCHER_EXT_R9_SCOPE_AUDIT_H

#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_r9_switch.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6C-D2: Nested CFN R9 Scope Ownership Audit (observe-only). */

typedef enum NestedR9ScopeClass {
    NRS_CLASS_UNKNOWN = 0,
    NRS_UNBALANCED_ALREADY_SWITCHED_SCOPE = 1,
    NRS_PREMATURE_OUTER_MR_HELPER_POP = 2,
    NRS_EMU_YIELD_MISCLASSIFIED_AS_RETURN = 3,
    NRS_GUEST_INSTRUCTION_WRITES_R9 = 4,
    NRS_HOST_CONTEXT_RESET_WRITES_R9 = 5,
    NRS_R9_WRITE_SOURCE_UNKNOWN = 6
} NestedR9ScopeClass;

void ext_r9_scope_audit_reset(void);
int ext_r9_scope_audit_enabled(void);
void ext_r9_scope_audit_bind_uc(void *uc);

const char *ext_r9_scope_class_name(NestedR9ScopeClass c);
NestedR9ScopeClass ext_r9_scope_audit_last_class(void);
int ext_r9_scope_audit_gate_open(void);

/* Notify from switch / R9 write / nested CFN / fault paths. */
void ext_r9_scope_audit_on_enter(const ModuleR9Scope *scope, GwyModuleCallKind kind,
                                 int already_switched, uint32_t current_r9, uint32_t target_r9);
void ext_r9_scope_audit_on_leave(const char *requested_by, uint64_t requesting_scope_id,
                                 int owns_frame, GwyR9LeaveAction action, uint64_t top_frame_id,
                                 GwyModuleCallKind top_kind, GwyEmuExitReason emu_exit,
                                 uint32_t restore_r9);
void ext_r9_scope_audit_on_r9_write(const GwyR9WriteRecord *rec);
void ext_r9_scope_audit_on_nested_cfn(uint32_t call_pc, uint32_t target, uint32_t pre_r9,
                                      uint64_t caller_module_id);
void ext_r9_scope_audit_on_code(void *uc, uint32_t pc, const uint32_t regs[16], uint32_t cpsr);
void ext_r9_scope_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                     const uint32_t regs[16], uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
