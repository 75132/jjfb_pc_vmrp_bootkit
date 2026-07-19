#ifndef GWY_LAUNCHER_ROBOTOL_FLAG_WRITER_TRACE_H
#define GWY_LAUNCHER_ROBOTOL_FLAG_WRITER_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stage E8F/E8G/E8H: observe-only writer/caller/dispatcher BP + SVC trap.
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
 * E8H Env:
 *   JJFB_E8H_DISPATCHER_BP=1      — 0x300714/0x30103C/0x3020C8/0x302340/... CODE hooks
 *   JJFB_E8H_DISPATCHER_PCS=...   — optional CSV override
 *   JJFB_E8H_SVC_TRAP=1           — observe-only trap at SVC #0xAB (0x2D92AE); no fake success
 *   JJFB_E8H_SVC_STOP=1           — uc_emu_stop on first SVC #0xAB (default when trap=1)
 *
 * E8I Env:
 *   JJFB_E8I_PARENT_BP=1          — 0x300158 + upstream BL sites + dispatcher chain
 *   JJFB_E8I_PARENT_PCS=0x..,..   — CSV (priority or all)
 *   JJFB_E8I_STATE_WATCH=1        — MEM_WRITE watch on R9+(0x800+0xD0); observe-only
 *
 * E8J Env:
 *   JJFB_E8J_CLUSTER_BP=1         — role-tagged cluster/upstream/queue CODE hooks
 *   JJFB_E8J_BP_SPEC=e:0x..,u:0x..,q:0x..,p:0x..,b:0x..
 *   JJFB_E8J_QUEUE_READ_WATCH=1   — MEM_READ watch on R9+FE8 / R9+B7D; observe-only
 *
 * E8K Env:
 *   JJFB_E8K_10102_CASE=<n>       — observe-only fire registered 0x10102 handler with R0=case
 *                                   (derived switch index; NOT product success)
 *
 * E8L Env:
 *   JJFB_E8L_10102_REGS=r0,r1,r2,r3 — structured ABI (overrides CASE when r0 present)
 *   JJFB_E8L_10102_R1=<n>         — R1 payload (event code or pointer); with E8K_CASE
 *   JJFB_E8L_10102_R2=<n>
 *   JJFB_E8L_10102_R3=<n>
 *
 * E8M Env:
 *   JJFB_E8M_PARENT_TRACE=1       — log first 200 insns in 0x300158..0x3004F6 (tick1)
 *   JJFB_E8M_SEQ=10165+310+156:18 — ordered observe-only fires (not product success)
 *
 * Never claims counterfactual as product success.
 * Never returns blind success from SVC #0xAB.
 * Never force-writes state word or idle flags as product success.
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
