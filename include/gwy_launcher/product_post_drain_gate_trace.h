#ifndef GWY_LAUNCHER_PRODUCT_POST_DRAIN_GATE_TRACE_H
#define GWY_LAUNCHER_PRODUCT_POST_DRAIN_GATE_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Post-Drain Gate Contract Closure — observe-only.
 * Env: JJFB_POST_DRAIN_GATE_TRACE=1
 * Hooks candidate writers (30CBBC / 2E2520 / 2DC4D8) and ER_RW+15D/B71 stores.
 * Never writes 15D/B71/UI_MODE. Not a cleanliness claim for product core.
 */

int product_pdgt_enabled(void);
void product_pdgt_reset(void);
void product_pdgt_set_run_id(const char *run_id);
const char *product_pdgt_run_id(void);

void product_pdgt_bind_uc(void *uc);
void product_pdgt_note_er_rw(uint32_t er_rw);
void product_pdgt_arm_hooks(void *uc);
void product_pdgt_note_anchor(const char *name, uint32_t pc, uint64_t insn);
void product_pdgt_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
