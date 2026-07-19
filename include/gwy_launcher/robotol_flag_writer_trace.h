#ifndef GWY_LAUNCHER_ROBOTOL_FLAG_WRITER_TRACE_H
#define GWY_LAUNCHER_ROBOTOL_FLAG_WRITER_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stage E8F: observe-only idle-flag writer callpath + sibling/counterfactual diagnostics.
 *
 * Env:
 *   JJFB_E8F_WRITER_BP=1          — CODE hooks on writer PCs (csv or built-in top set)
 *   JJFB_E8F_WRITER_PCS=0x..,..   — optional override CSV
 *   JJFB_E8F_SIBLING_PROBE=1      — fire 0x10162 and/or 0x10102 once
 *   JJFB_E8F_SIBLING=10162|10102|BOTH
 *   JJFB_E8F_COUNTERFACTUAL=C44|C9D|CF5|ALL  — COUNTERFACTUAL_ONLY poke (not product)
 *   JJFB_E8F_LONGPATH_WATCH=1     — hook 0x30D28C + snap queue depth
 *
 * Never claims counterfactual as product success. Never mutates 0x1E209 ret / MRP.
 */

int robotol_flag_writer_trace_enabled(void);
void robotol_flag_writer_trace_reset(void);
void robotol_flag_writer_trace_bind_uc(void *uc);
void robotol_flag_writer_trace_set_tick(uint32_t tick);

/* Arm CODE breakpoints once robotol code mapping is known. */
void robotol_flag_writer_trace_try_arm(void *uc);

/* After lifecycle tick (guest depth==0): sibling probes / counterfactual / summary. */
void robotol_flag_writer_trace_on_lifecycle(void *uc, uint32_t tick);

/* Dump never-hit writers (call at tick 40 / shutdown). */
void robotol_flag_writer_trace_dump_summary(const char *reason);

#ifdef __cplusplus
}
#endif

#endif
