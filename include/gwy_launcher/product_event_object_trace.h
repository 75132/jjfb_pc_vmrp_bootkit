#ifndef GWY_LAUNCHER_PRODUCT_EVENT_OBJECT_TRACE_H
#define GWY_LAUNCHER_PRODUCT_EVENT_OBJECT_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Event object provenance (observe-only).
 * Env: JJFB_EVENT_OBJECT_TRACE=1
 *
 * Tracks Path-A payload/entry objects through queue node link, consumer
 * get/pop, and R0 at 0x2E2520. Watches stores to object +0/+4/+8/+C.
 * Never mutates guest memory or forces dispatch.
 */

int product_eot_enabled(void);
void product_eot_reset(void);
void product_eot_set_run_id(const char *run_id);
const char *product_eot_run_id(void);

void product_eot_bind_uc(void *uc);
void product_eot_arm_hooks(void *uc);

void product_eot_on_path_a_begin(void *uc, uint32_t list, uint32_t entry, uint32_t count);
void product_eot_on_path_a_linked(void *uc, uint32_t list, uint32_t head, uint32_t node,
                                  uint32_t entry, uint32_t count);
void product_eot_on_get_item(void *uc, uint32_t node, uint32_t item);
void product_eot_on_drain_item(void *uc, uint32_t item);
void product_eot_on_dispatch_enter(void *uc, uint32_t call_id, uint32_t r0, uint32_t lr,
                                   uint32_t sp, uint32_t r1, uint32_t r2, uint32_t r3,
                                   uint32_t r9);

void product_eot_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
