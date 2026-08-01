#ifndef GWY_LAUNCHER_P22F_10740_SCHEDULER_H
#define GWY_LAUNCHER_P22F_10740_SCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22F-CLEAN: natural scheduler provenance for gamelist +0x10740.
 * Env: JJFB_P22F_CLEAN=1
 * Must NOT enable JJFB_P22_MODE / HEADLESS / P25 forge paths.
 */

int p22f_enabled(void);
void p22f_reset(void);
void p22f_bind_uc(void *uc);
void p22f_finalize(const char *stop_reason);

void p22f_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, uint64_t module_id,
                          const char *package_owner);
void p22f_note_gamelist_started(void);
void p22f_note_timer_fire(uint32_t helper, uint32_t p_guest, uint32_t erw, int end);
void p22f_note_plat(uint32_t code, uint32_t app, uint32_t arg2, uint32_t arg3, uint32_t ret,
                    uint32_t caller_pc, uint32_t r9);
void p22f_note_mem_write(uint32_t pc, uint32_t addr, uint32_t size, uint32_t value,
                         const char *module);
void p22f_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
