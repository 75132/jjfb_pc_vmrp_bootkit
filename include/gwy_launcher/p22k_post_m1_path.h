#ifndef GWY_LAUNCHER_P22K_POST_M1_PATH_H
#define GWY_LAUNCHER_P22K_POST_M1_PATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22K: observe-only post-method=1 path in cfunction (ARM continuation @ LR).
 * Env: JJFB_P22K_CLEAN=1 (typically with JJFB_P22I_CLEAN=1).
 * NATURAL_ONLY — no Host helper invoke, no FAST, no Guest mutation.
 *
 * Closes Class B next_fix: first blocking branch after natural 6→0→1 return.
 * Sparse CODE hook on [continuation, continuation+window) — not full DSM GCO.
 */

int p22k_enabled(void);
void p22k_reset(void);
void p22k_bind_uc(void *uc);
void p22k_finalize(const char *stop_reason);

/* Arm sparse watch when dispatcher continuation (e.g. 0x89BF0) is known. */
void p22k_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       int32_t return_r0, uint32_t sp);

/* Optional: note parent resume / emu leave after post-m1. */
void p22k_note_emu_leave(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r9, const char *phase);

#ifdef __cplusplus
}
#endif

#endif
