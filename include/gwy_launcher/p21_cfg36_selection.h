#ifndef GWY_LAUNCHER_P21_CFG36_SELECTION_H
#define GWY_LAUNCHER_P21_CFG36_SELECTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P21: observe-only cfg36 list load / selection contract.
 * Env: JJFB_P21_MODE=1
 * NATURAL_ONLY — no headless select, no startGame call, no index forge.
 *
 * Five distinct gates (never collapse):
 *   CFG_FMT_MAPPED / CFG_FILE_OPENED / CFG_RECORD_READ /
 *   CFG36_RECORD_PRESENT / CFG36_SELECTED
 */

int p21_enabled(void);
void p21_reset(void);
void p21_bind_uc(void *uc);
void p21_finalize(const char *stop_reason);

void p21_note_gamelist_started(void);
void p21_note_cfg_fmt_mapped(uint32_t fmt_va, const char *note);

void p21_note_file_io(const char *api, const char *path, const char *mode, int32_t offset,
                      uint32_t requested_size, int32_t returned_size, int32_t return_value,
                      uint32_t buffer_guest, const void *host_buf, uint32_t host_buf_len);

void p21_note_plat_10112(const char *path, const char *ns, const char *host, uint32_t buf,
                         uint32_t len, int loaded, int ret, uint32_t caller_pc);

void p21_set_launch_param(uint32_t va, uint32_t len, const char *entry);
void p21_note_param_byte_read(uint32_t pc, const char *module, uint32_t addr, uint32_t size,
                              const uint8_t *bytes, const uint32_t regs[13], uint32_t lr,
                              uint32_t sp, uint32_t r9);

void p21_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                 uint32_t lr, uint32_t sp, uint32_t cpsr);

void p21_on_timer_fire_begin(void *uc, uint32_t helper, uint32_t p_guest, uint32_t erw,
                             uint32_t period_ms, uint64_t generation);
void p21_on_timer_fire_end(void *uc, uint32_t helper, uint32_t method, uint32_t p_guest,
                           uint32_t erw, int32_t ret);

int p21_gate_fmt_mapped(void);
int p21_gate_file_opened(void);
int p21_gate_record_read(void);
int p21_gate_cfg36_present(void);
int p21_gate_cfg36_selected(void);

uint32_t p21_cfg36_guest_va(void);
uint32_t p21_cfg36_source_offset(void);
int p21_cfg_list_record_count(void);
const char *p21_cfg_source_path(void);

#ifdef __cplusplus
}
#endif

#endif
