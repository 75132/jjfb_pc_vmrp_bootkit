#ifndef GWY_LAUNCHER_P22G_CALLBACK_PUBLICATION_H
#define GWY_LAUNCHER_P22G_CALLBACK_PUBLICATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22G-CLEAN: close gamelist callback publication + init lifecycle.
 * Env: JJFB_P22G_CLEAN=1
 * Must NOT enable JJFB_P22_MODE / HEADLESS / P25 / FAST init / Host callback invoke.
 *
 * Targets (runtime_base-relative): +0xF670, +0x8CDC, +0xD978 (and Thumb |1).
 */

int p22g_enabled(void);
void p22g_reset(void);
void p22g_bind_uc(void *uc);
void p22g_finalize(const char *stop_reason);

void p22g_note_member_open(const char *guest_path);
void p22g_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, uint64_t module_id,
                          const char *package_owner);
void p22g_note_gamelist_started(void);
void p22g_note_c_function_new(uint32_t helper, uint32_t p_len, uint32_t p_guest, uint32_t rw_base,
                              uint32_t rw_size, const char *origin);
void p22g_note_helper_call(uint32_t helper, uint32_t method, int32_t ret_value);
void p22g_note_entry_begin(uint32_t helper, uint32_t method, uint32_t p_guest, uint32_t input,
                           uint32_t input_len, uint32_t er_rw, uint32_t sp);
void p22g_note_timer_fire(uint32_t helper, uint32_t p_guest, uint32_t erw, int end);
void p22g_note_plat(uint32_t code, uint32_t app, uint32_t arg2, uint32_t arg3, uint32_t ret,
                    uint32_t caller_pc, uint32_t r9);
void p22g_note_memcpy(uint32_t dst, uint32_t src, uint32_t n, uint32_t caller_pc);
void p22g_note_mem_write(uint32_t pc, uint32_t addr, uint32_t size, uint32_t value,
                         const char *module);
void p22g_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
