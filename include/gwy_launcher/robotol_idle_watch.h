#ifndef GWY_LAUNCHER_ROBOTOL_IDLE_WATCH_H
#define GWY_LAUNCHER_ROBOTOL_IDLE_WATCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stage E8C/E8D/E8E: observe-only idle flag + enqueue ABI watch.
 * Env: JJFB_E8C_IDLE_WATCH=1, JJFB_E8C_WATCH_OFFSETS / WATCH_ADDRS
 * E8D: JJFB_E8D_EARLY_WATCH=1, JJFB_E8D_ERW_DIFF=1, JJFB_E8D_10165_PROBE=1
 * E8E: JJFB_E8E_FE8_WATCH=1, JJFB_E8E_EVENT_PROBE=1, JJFB_E8E_CANDIDATE=<name>
 *      JJFB_E8E_DRAIN_ORDER=A|B|C
 * Never mutates guest flag bytes or plat ret. */

int robotol_idle_watch_enabled(void);
void robotol_idle_watch_reset(void);
void robotol_idle_watch_bind_uc(void *uc);

void robotol_idle_watch_try_arm(void *uc);
void robotol_idle_watch_set_tick(uint32_t tick);
void robotol_idle_watch_snap(void *uc, const char *reason);

/* E8D: milestone dump/diff of ER_RW[0xC00..0x1200) + early arm. */
void robotol_idle_watch_note_stage(void *uc, const char *stage);

/* E8D/E8E: observe-only fire of registered 0x10165 enqueue handler. */
void robotol_idle_watch_try_10165_probe(void *uc);

/* E8E: 'A'|'B'|'C'|0 — drain-order experiment (0 = legacy B: 10140 then probe). */
int robotol_idle_watch_drain_order(void);

void robotol_idle_watch_helper_fx_begin(uint32_t r0, uint32_t r1);
void robotol_idle_watch_helper_fx_end(uint32_t r0, uint32_t r1, uint32_t ret);

/* Called when plat registers a handler (map log). */
void robotol_idle_watch_on_handler_register(uint32_t plat_code, uint32_t family, uint32_t handler);

#ifdef __cplusplus
}
#endif

#endif
