#ifndef GWY_LAUNCHER_PRODUCT_HELPER_2F68E4_TRACE_H
#define GWY_LAUNCHER_PRODUCT_HELPER_2F68E4_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sparse observe-only trace for Path-A stream helper 0x2F68E4.
 *
 * Env: JJFB_HELPER_2F68E4_TRACE=1
 *
 * Three layers: basic-block coverage, branch edges, periodic snapshots.
 * Does not mutate guest state, force returns, or fabricate platform results.
 */

int product_h2_enabled(void);
void product_h2_reset(void);
void product_h2_set_run_id(const char *run_id);
const char *product_h2_run_id(void);

void product_h2_bind_uc(void *uc);
void product_h2_note_er_rw(uint32_t er_rw);
void product_h2_note_module_range(uint32_t code_base, uint32_t code_size);
void product_h2_note_handler_call_id(uint32_t id);

/* 1 while 0x2F68E4 body is being sparse-traced. */
int product_h2_helper_active(void);

void product_h2_on_handler_enter(void *uc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                                 uint32_t r3, uint32_t r9);
void product_h2_on_handler_leave(void *uc, const char *reason);
void product_h2_on_outside_pc(void *uc, uint32_t pc, const char *site);

void product_h2_note_nested_publish(uint32_t parent_event_id, uint32_t queue_count);
void product_h2_note_nested_consume(uint32_t event_id, uint32_t queue_count);
void product_h2_note_platform_api(uint32_t api_id, uint32_t slot, uint32_t caller_pc, uint32_t lr,
                                  uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                                  uint32_t ret_r0, const char *kind);
void product_h2_note_consumer_enter(uint32_t queue_count);
void product_h2_note_callback_depth(uint32_t depth);

void product_h2_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
