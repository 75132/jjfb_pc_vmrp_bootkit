#ifndef GWY_LAUNCHER_EXT_BOOTSTRAP_ABI_H
#define GWY_LAUNCHER_EXT_BOOTSTRAP_ABI_H

#include "gwy_launcher/module_r9_switch.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6C-C: Bootstrap Entry R9 ABI Contract Recovery (observe-only). */

typedef enum BootstrapAbiClass {
    BS_ABI_UNKNOWN = 0,
    BS_ABI_PRESERVE_CALLER_R9_DOCUMENTED = 1,
    BS_ABI_PRESERVE_CALLER_R9_CROSS_TARGET = 2,
    BS_ABI_TEMP_R9_REQUIRED = 3,
    BS_ABI_R9_UNUSED = 4,
    BS_ABI_ENTRY_POINTER_MISINTERPRETED = 5,
    BS_ABI_RELOCATION_INCOMPLETE = 6,
    BS_ABI_ARGUMENT_ABI_MISSING = 7,
    BS_ABI_LIFECYCLE_MISSING = 8
} BootstrapAbiClass;

typedef enum BootstrapR9Causality {
    BS_R9_UNKNOWN = 0,
    BS_R9_DIRECTLY_CAUSAL = 1,
    BS_R9_INDIRECTLY_CAUSAL = 2,
    BS_R9_USED_BUT_NOT_CAUSAL = 3,
    BS_R9_NOT_USED_BEFORE_FAULT = 4
} BootstrapR9Causality;

void ext_bootstrap_abi_reset(void);
int ext_bootstrap_abi_enabled(void);
void ext_bootstrap_abi_bind_uc(void *uc);

const char *ext_bootstrap_abi_class_name(BootstrapAbiClass c);
BootstrapAbiClass ext_bootstrap_abi_last_class(void);

void ext_bootstrap_abi_on_code_image(uint32_t guest_addr, uint32_t size);
void ext_bootstrap_abi_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                      uint32_t rw_base, uint32_t rw_size);

void ext_bootstrap_abi_on_r9_switch(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                                    GwyModuleCallKind call_kind, int enter_rc, uint32_t pc,
                                    const uint32_t regs[16]);

void ext_bootstrap_abi_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                               uint32_t cpsr);

void ext_bootstrap_abi_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                    uint32_t access_size, const uint32_t regs[16], uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
