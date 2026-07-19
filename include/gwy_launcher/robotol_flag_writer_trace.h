#ifndef GWY_LAUNCHER_ROBOTOL_FLAG_WRITER_TRACE_H
#define GWY_LAUNCHER_ROBOTOL_FLAG_WRITER_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stage E8F/E8G: observe-only writer/caller BP + counterfactual diagnostics.
 *
 * E8F Env:
 *   JJFB_E8F_WRITER_BP=1, JJFB_E8F_WRITER_PCS=...
 *   JJFB_E8F_SIBLING_PROBE=1, JJFB_E8F_SIBLING=...
 *   JJFB_E8F_COUNTERFACTUAL=C44|C9D|CF5|C44C9D|C44CF5|C9DCF5|ALL
 *   JJFB_E8F_LONGPATH_WATCH=1
 *
 * E8G Env:
 *   JJFB_E8G_CALLER_BP=1          — bootstrap caller CODE hooks
 *   JJFB_E8G_CALLER_PCS=0x..,..   — optional CSV (else built-in priority set)
 *   JJFB_E8G_FAULT_WATCH=1        — hook 0x2D92B0 + rich fault dump on lifecycle fail
 *
 * Never claims counterfactual as product success.
 */

int robotol_flag_writer_trace_enabled(void);
void robotol_flag_writer_trace_reset(void);
void robotol_flag_writer_trace_bind_uc(void *uc);
void robotol_flag_writer_trace_set_tick(uint32_t tick);

void robotol_flag_writer_trace_try_arm(void *uc);
void robotol_flag_writer_trace_on_lifecycle(void *uc, uint32_t tick);
void robotol_flag_writer_trace_dump_summary(const char *reason);

/* E8G: after 10140 fire fails — dump regs/context at fault PC (COUNTERFACTUAL path). */
void robotol_flag_writer_trace_on_lifecycle_fault(void *uc, uint32_t tick, int ok,
                                                  unsigned uc_err, uint32_t pc_after,
                                                  uint32_t r0_after, uint32_t r9_after,
                                                  uint32_t sp_after, uint32_t lr_after);

#ifdef __cplusplus
}
#endif

#endif
