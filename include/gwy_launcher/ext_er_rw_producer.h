#ifndef GWY_LAUNCHER_EXT_ER_RW_PRODUCER_H
#define GWY_LAUNCHER_EXT_ER_RW_PRODUCER_H

#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/module_registry.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6C-B: ER_RW Producer Timing & First-Entry Ordering (observe-only). */

typedef enum ErRwTimingClass {
    ER_RW_TIMING_UNKNOWN = 0,
    ER_RW_METADATA_PUBLICATION_LATE = 1,
    ER_RW_BOOTSTRAP_ENTRY_PRECEDES = 2,
    ER_RW_MODULE_ENTRY_ORDERING_WRONG = 3,
    ER_RW_DESCRIPTOR_NOT_EXPOSED = 4,
    ER_RW_BOOTSTRAP_ENTRY_MISCLASSIFIED = 5
} ErRwTimingClass;

typedef enum ErRwEntryIdentity {
    ER_RW_ENTRY_UNKNOWN = 0,
    ER_RW_ENTRY_BOOTSTRAP = 1,
    ER_RW_ENTRY_MODULE_RUNTIME = 2
} ErRwEntryIdentity;

typedef enum ErRwFaultPath {
    ER_RW_FAULT_UNKNOWN = 0,
    ER_RW_FAULT_BOOTSTRAP_CODE = 1,
    ER_RW_FAULT_MODULE_RUNTIME_CODE = 2,
    ER_RW_FAULT_HELPER_CODE = 3
} ErRwFaultPath;

void ext_er_rw_producer_reset(void);
int ext_er_rw_producer_enabled(void);
void ext_er_rw_producer_bind_uc(void *uc);

const char *ext_er_rw_timing_class_name(ErRwTimingClass c);
ErRwTimingClass ext_er_rw_producer_last_class(void);

void ext_er_rw_producer_on_member_open(const char *guest_path);
void ext_er_rw_producer_on_code_image(uint32_t guest_addr, uint32_t size);
void ext_er_rw_producer_on_mapped(uint64_t module_id, uint32_t code_base, uint32_t code_size);
void ext_er_rw_producer_on_alloc(uint32_t guest_addr, uint32_t size);
void ext_er_rw_producer_on_block_copy(uint32_t dst, uint32_t src, uint32_t len);
void ext_er_rw_producer_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                       uint32_t rw_base, uint32_t rw_size);
void ext_er_rw_producer_on_helper_call(uint32_t helper, uint32_t method);
void ext_er_rw_producer_on_er_rw_publish(uint64_t module_id, uint32_t base, uint32_t size,
                                         GwyErRwSource source);

void ext_er_rw_producer_on_r9_switch(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                                     GwyModuleCallKind call_kind, int enter_rc);

void ext_er_rw_producer_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                uint32_t cpsr);

void ext_er_rw_producer_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr, uint32_t r0,
                                     uint32_t r9);

#ifdef __cplusplus
}
#endif

#endif
