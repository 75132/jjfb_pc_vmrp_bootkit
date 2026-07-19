#ifndef GWY_LAUNCHER_EXT_POST_CFN_R9_AUDIT_H
#define GWY_LAUNCHER_EXT_POST_CFN_R9_AUDIT_H

#include "gwy_launcher/module_r9_switch.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6D-B: Post-CFN Runtime R9 Promotion Audit (observe-only). */

typedef enum PostCfnR9Class {
    PCFN_CLASS_UNKNOWN = 0,
    PCFN_POST_CFN_R9_PROMOTION_REQUIRED = 1,
    PCFN_ER_RW_REGISTRY_PUBLICATION_LATE = 2,
    PCFN_GUEST_FIXR9_PATH_MISSING = 3,
    PCFN_CONTINUATION_SCOPE_NOT_PROMOTED = 4,
    PCFN_R0_SOURCE_INDEPENDENT_OF_R9 = 5,
    PCFN_P_MODULE_ASSOCIATION_MISSING = 6
} PostCfnR9Class;

typedef enum PostCfnR0Source {
    PCFN_R0_UNKNOWN = 0,
    PCFN_R0_R9_REL_GLOBAL = 1,
    PCFN_R0_P_FIELD = 2,
    PCFN_R0_RETURN_VALUE = 3,
    PCFN_R0_ARGUMENT = 4,
    PCFN_R0_STACK = 5
} PostCfnR0Source;

typedef enum PostCfnRegistryPending {
    PCFN_REG_UNKNOWN = 0,
    PCFN_REG_PUBLICATION_LATE = 1,
    PCFN_REG_MODULE_ASSOCIATION_MISSING = 2,
    PCFN_REG_OBSERVER_LAG = 3,
    PCFN_REG_PUBLISHED = 4
} PostCfnRegistryPending;

void ext_post_cfn_r9_audit_reset(void);
int ext_post_cfn_r9_audit_enabled(void);
void ext_post_cfn_r9_audit_bind_uc(void *uc);

const char *ext_post_cfn_r9_class_name(PostCfnR9Class c);
const char *ext_post_cfn_r0_source_name(PostCfnR0Source s);
const char *ext_post_cfn_registry_pending_name(PostCfnRegistryPending r);
PostCfnR9Class ext_post_cfn_r9_audit_last_class(void);
int ext_post_cfn_r9_gate_open(void);
int ext_post_cfn_r9_audit_armed(void);

void ext_post_cfn_r9_audit_on_continuation_resume(void *uc, uint64_t module_id, const char *module,
                                                  uint32_t call_pc, uint32_t continuation_pc,
                                                  const uint32_t regs[16], uint32_t sp, uint32_t lr,
                                                  uint32_t cpsr);

void ext_post_cfn_r9_audit_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                   uint32_t cpsr);

void ext_post_cfn_r9_audit_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                          uint32_t rw_base, uint32_t rw_size);

void ext_post_cfn_r9_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                        const uint32_t regs[16], uint32_t cpsr);

void ext_post_cfn_r9_audit_finalize(const char *stop_reason);

#ifdef __cplusplus
}
#endif

#endif
