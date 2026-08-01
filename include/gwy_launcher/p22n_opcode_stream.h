#ifndef GWY_LAUNCHER_P22N_OPCODE_STREAM_H
#define GWY_LAUNCHER_P22N_OPCODE_STREAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22N-CLEAN: close cfunction opcode-stream / UI-init record producer gap.
 *
 * NATURAL_ONLY — observe opcode interpreter at +0x1C3A0/+0x1C40C; do not set
 * [object+0x30]|=0x0C, do not forge records, no Host helper / FAST / cfg inject.
 *
 * Env: JJFB_P22N_CLEAN=1
 *
 * Correction vs P22M: 0x9C41C BEQ → +0x1C56C is NORMAL_OPCODE_DISPATCH, not a lock.
 */

int p22n_enabled(void);
int p22n_observation_complete(void);
void p22n_reset(void);
void p22n_bind_uc(void *uc);
void p22n_finalize(const char *stop_reason);

void p22n_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, const char *package_owner);

void p22n_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       uint32_t sp);

void p22n_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
