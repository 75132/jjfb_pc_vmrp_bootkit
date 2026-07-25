#ifndef GWY_LAUNCHER_PRODUCT_C0_START_OBJECT_TRACE_H
#define GWY_LAUNCHER_PRODUCT_C0_START_OBJECT_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int product_c0_sot_enabled(void);
void product_c0_sot_reset(void);
void product_c0_sot_bind_uc(void *uc);
void product_c0_sot_note_er_rw(uint32_t er_rw);
void product_c0_sot_arm_hooks(void *uc);
/* Dump R9+E6C / root snapshot at C0 entry (observe-only). */
void product_c0_sot_on_c0_enter(void *uc, uint32_t er_rw, const char *why);

#ifdef __cplusplus
}
#endif

#endif
