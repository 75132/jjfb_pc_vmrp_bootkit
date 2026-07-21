#ifndef GWY_LAUNCHER_E10A31B_PUBLICATION_H
#define GWY_LAUNCHER_E10A31B_PUBLICATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1b: gamelist P / extChunk / ERW publication provenance.
 * Env: JJFB_E10A31B_MODE=1 (also implied by JJFB_E10A31_TIMER_CONTEXT)
 */

int e10a31b_enabled(void);
void e10a31b_reset(void);

void e10a31b_mark(const char *milestone, const char *module, uint32_t helper, uint32_t p_guest,
                  uint32_t chunk_guest, uint32_t erw, uint64_t module_id, const char *note);

void e10a31b_note_cfn(const char *module, uint32_t helper, uint32_t p_guest, uint32_t p_len,
                      int p_reused, uint32_t prior_p);
void e10a31b_note_chunk(const char *event, const char *module, uint32_t helper, uint32_t p_guest,
                        uint32_t chunk_guest, uint64_t old_mid, uint64_t new_mid, const char *note);
void e10a31b_note_erw(const char *event, const char *module, uint32_t p_guest, uint32_t erw,
                      uint32_t erw_len, uint64_t module_id, const char *note);
void e10a31b_note_timer_arm(const char *module, uint32_t helper, uint32_t p_guest,
                            uint32_t chunk_guest, uint32_t erw, uint32_t registry_erw,
                            int own_erw);

#ifdef __cplusplus
}
#endif

#endif
