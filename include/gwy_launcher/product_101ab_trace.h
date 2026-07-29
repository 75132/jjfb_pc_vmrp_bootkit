#ifndef GWY_LAUNCHER_PRODUCT_101AB_TRACE_H
#define GWY_LAUNCHER_PRODUCT_101AB_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P15 unified 0x101AB transport trace (observe-only).
 *
 * Env: JJFB_101AB_TRACE=1 (default ON when launcher product path sets it)
 * Dumps:
 *   reports/P15_101AB_TRACE.csv
 *   out/p15_101ab/frames/frame_<id>.bin
 *
 * Never enqueues Event15, never rewrites event_code, never stores E6C.
 */

int product_101ab_trace_enabled(void);
void product_101ab_trace_reset(void);
void product_101ab_trace_set_run_id(const char *run_id);
const char *product_101ab_trace_run_id(void);

void product_101ab_trace_bind_uc(void *uc);
void product_101ab_trace_note_er_rw(uint32_t er_rw);
void product_101ab_trace_arm_hooks(void *uc);

/* Called from platform BUFFER_FILL path after provider fill + guest poke. */
void product_101ab_trace_on_platform_fill(void *uc, uint32_t buf_va, uint32_t capacity,
                                          const uint8_t *host_bytes, uint32_t host_n,
                                          uint32_t guest_r0_cursor, int with_record,
                                          const char *provider_name);

void product_101ab_trace_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
