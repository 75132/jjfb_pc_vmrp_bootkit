#ifndef GWY_LAUNCHER_PRODUCT_PLATFORM_10138_TRACE_H
#define GWY_LAUNCHER_PRODUCT_PLATFORM_10138_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform 0x10138 ABI trace — observe-only.
 *
 * Env: JJFB_PLATFORM_10138_TRACE=1  (Diagnostic may enable; default OFF)
 *
 * Prefer logging only while Path-A helper 0x2F68E4 is active (PAH in-handler
 * after BL 0x2F68E4). Without PAH, falls back to known robotol site LRs.
 */

int product_p10138_enabled(void);
void product_p10138_reset(void);
void product_p10138_set_run_id(const char *run_id);

/* Scope: call when PAH sees 0x2F68E4 / handler leave. */
void product_p10138_note_helper_enter(void);
void product_p10138_note_helper_leave(void);
int product_p10138_helper_active(void);

/*
 * Before/after a 0x10138 multi-out execution.
 * outs[6] = guest pointers (R1,R2,R3,SP0,SP4,SP8); vals[6] written values.
 */
void product_p10138_on_enter(void *uc, uint32_t api_call_id, uint32_t handler_call_id,
                             uint32_t caller_pc, uint32_t lr, uint32_t sp, uint32_t cpsr,
                             uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9,
                             uint32_t site_lr, uint32_t slot_fn, const uint32_t outs[6]);
void product_p10138_on_complete(void *uc, uint32_t api_call_id, uint32_t ret_r0, int mode_metrics,
                                uint32_t site_lr, const uint32_t outs[6], const uint32_t vals[6],
                                uint32_t er_rw, uint32_t ed8, uint8_t f7dc, uint8_t f7dd,
                                int wrote_gates);

void product_p10138_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
