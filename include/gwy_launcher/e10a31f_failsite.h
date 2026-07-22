#ifndef GWY_LAUNCHER_E10A31F_FAILSITE_H
#define GWY_LAUNCHER_E10A31F_FAILSITE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1f: method0 fail-site refinement.
 * Env: JJFB_E10A31F_MODE=1
 *      JJFB_E10A31F_CONTINUE_PAST_SENTINEL=1  (ignore 0x2E1C24 MVNS as first-fail)
 *      JJFB_E10A31F_DENSE_WINDOW=1
 *      JJFB_E10A31F_ABI_FILEBUF=1             (method0 input=guest EXT base)
 */

int e10a31f_enabled(void);
int e10a31f_continue_past_sentinel(void);
int e10a31f_abi_filebuf(void);
void e10a31f_reset(void);
void e10a31f_mark_milestone(const char *name, const char *note);

/* True if (pc,bytes) is the known unconditional MVNS r0,r4 sentinel at 0x2E1C24. */
int e10a31f_is_neg1_sentinel_store(uint32_t pc, const uint8_t *bytes, uint32_t size);

/* Dense branch/window logging during method0. */
void e10a31f_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr,
                             const uint8_t *bytes, uint32_t size);

void e10a31f_on_method0_return(void *uc, uint32_t helper, int32_t ret);

/* Optional: guest pointer to use as method0/6 filebuf input (0 = keep current). */
uint32_t e10a31f_method0_input_override(uint32_t default_input);

/*
 * When CONTINUE_PAST_SENTINEL: return 1 if this r0=-1 event is the known
 * unconditional MVNS store at 0x2E1C24 (not a real helper failure).
 */
int e10a31f_should_ignore_neg1_at(uint32_t fail_pc, uint32_t next_pc, const uint8_t *fail_bytes,
                                  uint32_t fail_size);

#ifdef __cplusplus
}
#endif

#endif
