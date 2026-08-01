#ifndef GWY_LAUNCHER_P22L_PARENT_RETURN_H
#define GWY_LAUNCHER_P22L_PARENT_RETURN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22L-CLEAN: fix E8BD8DF0 LDM reglist decode, capture real wrapper return PC,
 * then slice parent consumption of wrapper R0=2.
 *
 * Env: JJFB_P22L_CLEAN=1
 * NATURAL_ONLY — no Host helper invoke, no Guest mutation, do not chase 0xC.
 */

int p22l_enabled(void);
void p22l_reset(void);
void p22l_bind_uc(void *uc);
void p22l_finalize(const char *stop_reason);

void p22l_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw);

/* Arm sparse watch when dispatcher continuation (e.g. 0x89BF0) is known. */
void p22l_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       uint32_t sp);

/* Optional secondary path from shell native-exec CODE feed. */
void p22l_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
