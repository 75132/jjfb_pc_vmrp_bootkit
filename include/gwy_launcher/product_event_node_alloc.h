#ifndef GWY_LAUNCHER_PRODUCT_EVENT_NODE_ALLOC_H
#define GWY_LAUNCHER_PRODUCT_EVENT_NODE_ALLOC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product List-Node Allocation Contract Closure (TraceNodeAlloc).
 * Observe Path-A → list push → heap helper → DSM mem primitive; classify
 * 0x94E40 and name the first causal zero. Enabled by
 * JJFB_PRODUCT_TRACE_NODE_ALLOC=1.
 */

int product_na_enabled(void);
void product_na_reset(void);
void product_na_set_run_id(const char *run_id);
const char *product_na_run_id(void);

void product_na_bind_uc(void *uc);
void product_na_note_er_rw(uint32_t er_rw);
void product_na_arm_code_hooks(void *uc);

void product_na_on_platform(uint32_t code, uint32_t app, uint32_t arg2, uint32_t ret);
void product_na_on_indirect_call(void *uc, uint32_t pc, uint32_t target, uint32_t r0, uint32_t r1);
void product_na_on_cross_module(void *uc, uint32_t from_pc, uint32_t target, uint32_t r0, uint32_t r1,
                                uint32_t lr);
void product_na_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr, uint32_t r0);

void product_na_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
