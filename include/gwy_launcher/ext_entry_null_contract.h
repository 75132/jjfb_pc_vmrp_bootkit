#ifndef GWY_LAUNCHER_EXT_ENTRY_NULL_CONTRACT_H
#define GWY_LAUNCHER_EXT_ENTRY_NULL_CONTRACT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6B-A9: Module Entry NULL-Argument Contract Discrimination (observe-only). */

typedef enum ExtNullContractClass {
    NULL_CONTRACT_UNKNOWN = 0,
    NULL_CONTRACT_ENTRY_NULL_DOCUMENTED = 1,
    NULL_CONTRACT_ENTRY_NULL_CROSS_TARGET = 2,
    NULL_CONTRACT_MISSING_MODULE_ENTRY_CONTEXT = 3,
    NULL_CONTRACT_MISSING_PRE_ENTRY_INITIALIZATION = 4,
    NULL_CONTRACT_MISSING_MODULE_DATA_INITIALIZATION = 5,
    NULL_CONTRACT_WRONG_MODULE_ENTRY_LIFECYCLE = 6,
    NULL_CONTRACT_LOADCODE_RETURN_MISINTERPRETED = 7
} ExtNullContractClass;

void ext_entry_null_contract_reset(void);
int ext_entry_null_contract_enabled(void);
void ext_entry_null_contract_bind_uc(void *uc);

const char *ext_null_contract_class_name(ExtNullContractClass c);

void ext_entry_null_contract_on_code_image(uint32_t guest_addr, uint32_t size);
void ext_entry_null_contract_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len);
void ext_entry_null_contract_on_registered(uint64_t module_id, uint32_t helper);
void ext_entry_null_contract_on_mr_helper(uint32_t helper, uint32_t code, uint32_t p_guest);

void ext_entry_null_contract_on_second_hop(void *uc,
                                           uint32_t caller_pc,
                                           uint32_t target,
                                           uint32_t r0,
                                           uint32_t r1,
                                           uint32_t r2,
                                           uint32_t r3,
                                           uint64_t from_mid,
                                           uint64_t to_mid,
                                           const char *target_relation);

void ext_entry_null_contract_on_code(void *uc,
                                     uint64_t module_id,
                                     uint32_t pc,
                                     const uint32_t regs[16],
                                     uint32_t cpsr);

void ext_entry_null_contract_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr, uint32_t r0,
                                          uint32_t r9);

void ext_entry_null_contract_on_return_to_dsm(uint64_t from_mid, uint32_t return_r0);

ExtNullContractClass ext_entry_null_contract_last_class(void);
int ext_entry_null_contract_xt_crossed(void);

#ifdef __cplusplus
}
#endif

#endif
