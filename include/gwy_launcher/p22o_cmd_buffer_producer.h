#ifndef GWY_LAUNCHER_P22O_CMD_BUFFER_PRODUCER_H
#define GWY_LAUNCHER_P22O_CMD_BUFFER_PRODUCER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22O-CLEAN: early command-buffer producer provenance.
 *
 * Arm MEM_WRITE at cfunction map (before +0x1C40C first fetch). NATURAL_ONLY —
 * no opcode forge, no [object+0x30]|=0x0C, no Host helper / FAST / +0x10740 call.
 *
 * Env: JJFB_P22O_CLEAN=1
 */

int p22o_enabled(void);
int p22o_observation_complete(void);
void p22o_reset(void);
void p22o_bind_uc(void *uc);
void p22o_finalize(const char *stop_reason);

void p22o_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, const char *package_owner);

void p22o_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       uint32_t sp);

#ifdef __cplusplus
}
#endif

#endif
