#ifndef GWY_LAUNCHER_P22_CFG_LOADER_PREDICATE_H
#define GWY_LAUNCHER_P22_CFG_LOADER_PREDICATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22-CLEAN: close cfg-loader entry predicate (observe-only).
 * Env: JJFB_P22_CLEAN=1
 *
 * MUST NOT enable JJFB_P22_MODE / JJFB_P22_HEADLESS_SELECT.
 * Never call base+0x7B6C / 0x10112 / forge cfg / select / startGame.
 */

int p22c_enabled(void);
void p22c_reset(void);
void p22c_bind_uc(void *uc);
void p22c_finalize(const char *stop_reason);

void p22c_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint32_t generation, const char *package_owner);
void p22c_note_gamelist_started(void);
void p22c_note_plat_10800(uint32_t caller_pc, uint32_t app, uint32_t arg2, uint32_t arg3,
                          uint32_t status_ret, uint32_t r9);
void p22c_note_plat_10112(const char *path, uint32_t caller_pc, int ret);
void p22c_note_param_byte_read(uint32_t pc, const char *module, uint32_t addr, uint32_t size,
                               const uint8_t *bytes, const uint32_t regs[13], uint32_t lr,
                               uint32_t r9);
void p22c_note_timer_fire(uint32_t helper, uint32_t method, int end);

void p22c_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
