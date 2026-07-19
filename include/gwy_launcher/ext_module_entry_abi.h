#ifndef GWY_LAUNCHER_EXT_MODULE_ENTRY_ABI_H
#define GWY_LAUNCHER_EXT_MODULE_ENTRY_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6B-A8: CODE_IMAGE entry ABI vs mr_helper event path (observe-only). */

typedef enum ExtLoadReturnClass {
    LOAD_RET_UNKNOWN = 0,
    LOAD_RET_CODE_POINTER = 1,
    LOAD_RET_DESCRIPTOR = 2,
    LOAD_RET_HANDLE = 3,
    LOAD_RET_TRAMPOLINE = 4
} ExtLoadReturnClass;

typedef enum ExtLoadReturnTransform {
    LOAD_XFORM_UNKNOWN = 0,
    LOAD_XFORM_NONE = 1,
    LOAD_XFORM_THUMB_BIT = 2,
    LOAD_XFORM_ADD_OFFSET = 3,
    LOAD_XFORM_DEREF = 4,
    LOAD_XFORM_DESCRIPTOR_FIELD = 5
} ExtLoadReturnTransform;

typedef enum ExtModuleEntryRootCause {
    ME_ROOT_UNKNOWN = 0,
    ME_ROOT_MISSING_MODULE_ENTRY_CONTEXT = 1,
    ME_ROOT_MISSING_PRE_ENTRY_INITIALIZATION = 2,
    ME_ROOT_LOADCODE_RETURN_MISINTERPRETED = 3,
    ME_ROOT_WRONG_MODULE_ENTRY_LIFECYCLE = 4,
    ME_ROOT_MRC_FUNCTION_P_ENTRY_ARGUMENT = 5
} ExtModuleEntryRootCause;

typedef enum ExtR0AssignKind {
    R0_ASSIGN_UNKNOWN = 0,
    R0_ASSIGN_EXPLICIT_ZERO = 1,
    R0_ASSIGN_MOV_FROM_REG = 2,
    R0_ASSIGN_LOADED_FIELD = 3,
    R0_ASSIGN_LOADCODE_OUT = 4
} ExtR0AssignKind;

void ext_module_entry_abi_reset(void);
int ext_module_entry_abi_enabled(void);
void ext_module_entry_abi_bind_uc(void *uc);

const char *ext_load_return_class_name(ExtLoadReturnClass c);
const char *ext_load_return_transform_name(ExtLoadReturnTransform t);
const char *ext_module_entry_root_cause_name(ExtModuleEntryRootCause r);
const char *ext_r0_assign_kind_name(ExtR0AssignKind k);

/* CODE_IMAGE map notification (guest nested or host DSM firmware). */
void ext_module_entry_abi_on_code_image(uint32_t guest_addr, uint32_t size);

/* After _mr_c_function_new: track P alloc/lifecycle separately from module entry. */
void ext_module_entry_abi_on_cfunction_p(uint32_t helper,
                                         uint32_t p_guest,
                                         uint32_t p_len,
                                         const char *origin);

/* DSM second-hop into nested EXT (CODE_IMAGE / LOAD_RETURN path). */
void ext_module_entry_abi_on_second_hop(void *uc,
                                        uint32_t caller_pc,
                                        uint32_t target,
                                        uint32_t r0,
                                        uint32_t r1,
                                        uint32_t r2,
                                        uint32_t r3,
                                        uint64_t from_mid,
                                        uint64_t to_mid,
                                        const char *target_relation);

/* Per-instruction: P field stores, helper calls, entry proto sampling. */
void ext_module_entry_abi_on_code(void *uc,
                                  uint64_t module_id,
                                  uint32_t pc,
                                  const uint32_t regs[16],
                                  uint32_t cpsr);

void ext_module_entry_abi_on_store(uint32_t value,
                                   uint32_t dest,
                                   uint64_t writer_mid,
                                   uint32_t writer_pc);

/* Host mythroad case-801 / mr_extHelper path (if bridged). */
void ext_module_entry_abi_on_mr_helper(uint32_t helper,
                                       uint32_t code,
                                       uint32_t p_guest,
                                       uint32_t r0,
                                       uint32_t r1,
                                       uint32_t r2,
                                       uint32_t r3);

/* Mem-invalid before any MR_HELPER → NOT_REACHED tag. */
void ext_module_entry_abi_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr);

ExtModuleEntryRootCause ext_module_entry_abi_last_root_cause(void);
int ext_module_entry_abi_mr_helper_seen(void);

#ifdef __cplusplus
}
#endif

#endif
