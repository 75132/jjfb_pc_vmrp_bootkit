#ifndef GWY_LAUNCHER_HANDLER_FORENSIC_H
#define GWY_LAUNCHER_HANDLER_FORENSIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stage E8A: observe-only lifecycle handler forensics (JJFB_HANDLER_FORENSIC=1).
 * No ABI/LR/SP/R0 mutation. Ring-traces at most 64 instructions. */

int handler_forensic_enabled(void);

/* Install temporary UC_HOOK_CODE over owner code range; print ENTRY snap. */
int handler_forensic_begin(void *uc, uint32_t handler_raw, uint32_t code_base, uint32_t code_size,
                           const char *owner_name, uint32_t er_rw);

/* After uc_emu_start: print FAULT/DONE with last 16 insn + last branch.
 * Prefer last ring snap — guest_memory_uc_run_entry_ex restores UC regs. */
void handler_forensic_end(void *uc, int run_ok, unsigned uc_err, const char *end_reason,
                          uint32_t pc_after, uint32_t r0_after, uint32_t r9_after,
                          uint32_t sp_after, uint32_t lr_after, uint32_t cpsr_after);

#ifdef __cplusplus
}
#endif

#endif
