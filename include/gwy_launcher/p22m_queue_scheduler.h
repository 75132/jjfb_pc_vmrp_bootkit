#ifndef GWY_LAUNCHER_P22M_QUEUE_SCHEDULER_H
#define GWY_LAUNCHER_P22M_QUEUE_SCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22M-CLEAN: close the post-object-consume scheduler gap after Class C.
 *
 * From cfunction +0x1D098 → BL +0x174C8, continue until the containing
 * state-machine function returns or the next helper/event/callback schedule
 * is observed. NATURAL_ONLY — no Guest mutation, no Host helper invoke, no FAST.
 *
 * Env: JJFB_P22M_CLEAN=1
 */

int p22m_enabled(void);
void p22m_reset(void);
void p22m_bind_uc(void *uc);
void p22m_finalize(const char *stop_reason);

void p22m_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, const char *package_owner);

void p22m_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       uint32_t sp);

/* Optional secondary path from shell native-exec CODE feed. */
void p22m_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
