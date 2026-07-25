#ifndef GWY_LAUNCHER_PRODUCT_LIFECYCLE_RECORD_TRACE_H
#define GWY_LAUNCHER_PRODUCT_LIFECYCLE_RECORD_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Observe-only: 0x2DADC4 lifecycle record branches + natural B71 path.
 *
 * Env: JJFB_LIFECYCLE_RECORD_TRACE=1
 *
 * Does not write B71/15D/B70/UI_MODE, does not call 0x30ED2C, and does not
 * host-enqueue a second code=5.
 */

int product_lrt_enabled(void);
void product_lrt_reset(void);
void product_lrt_bind_uc(void *uc);
void product_lrt_note_er_rw(uint32_t er_rw);
void product_lrt_arm_hooks(void *uc);
void product_lrt_finalize(void);

/*
 * Implemented in gwy_ext_obs.c (V75 family_C0_after_B71 contract).
 * On natural B71=1 with B70==0: arm deferred 0x2FEBBC and uc_emu_stop so the
 * call runs at top-level (not nested mid-Path-A / mid-drain).
 */
void gwy_ext_obs_on_b71_natural_for_b70(void *uc, uint32_t er_rw);

#ifdef __cplusplus
}
#endif

#endif
