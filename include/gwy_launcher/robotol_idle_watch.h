#ifndef GWY_LAUNCHER_ROBOTOL_IDLE_WATCH_H
#define GWY_LAUNCHER_ROBOTOL_IDLE_WATCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stage E8C/E8D: observe-only idle flag watch.
 * Env: JJFB_E8C_IDLE_WATCH=1, JJFB_E8C_WATCH_OFFSETS / WATCH_ADDRS
 * E8D: JJFB_E8D_EARLY_WATCH=1, JJFB_E8D_ERW_DIFF=1, JJFB_E8D_10165_PROBE=1
 * Never mutates guest flag bytes or plat ret. */

int robotol_idle_watch_enabled(void);
void robotol_idle_watch_reset(void);
void robotol_idle_watch_bind_uc(void *uc);

void robotol_idle_watch_try_arm(void *uc);
void robotol_idle_watch_set_tick(uint32_t tick);
void robotol_idle_watch_snap(void *uc, const char *reason);

/* E8D: milestone dump/diff of ER_RW[0xC00..0x1200) + early arm. */
void robotol_idle_watch_note_stage(void *uc, const char *stage);

/* E8D: observe-only one-shot fire of registered 0x10165 enqueue handler. */
void robotol_idle_watch_try_10165_probe(void *uc);

void robotol_idle_watch_helper_fx_begin(uint32_t r0, uint32_t r1);
void robotol_idle_watch_helper_fx_end(uint32_t r0, uint32_t r1, uint32_t ret);

/* Called when plat registers a handler (map log). */
void robotol_idle_watch_on_handler_register(uint32_t plat_code, uint32_t family, uint32_t handler);

#ifdef __cplusplus
}
#endif

#endif
