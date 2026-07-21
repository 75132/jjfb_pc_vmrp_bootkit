#ifndef GWY_LAUNCHER_PLATFORM_TIMER_H
#define GWY_LAUNCHER_PLATFORM_TIMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Monotonic deadline timer for sendAppEvent / EXT timer ABI.
 * DOCUMENTED: mrc_extTimerStart/Stop; host must advance while guest holds emu.
 * SDL_AddTimer alone does not fire during nested runCode (no event pump) —
 * TARGET_OBSERVED Full Boot: START logged, FIRE_CB never within period+margin.
 */

typedef uint32_t (*PlatformTimerClockFn)(void);

void platform_timer_reset(void);
void platform_timer_set_clock(PlatformTimerClockFn clock_ms);

void platform_timer_start(uint32_t period_ms);
void platform_timer_stop(void);
/* Mark running timer immediately due (SDL callback while emu holds mutex). */
void platform_timer_signal_due(void);

int platform_timer_running(void);
/* 1 if running and now >= deadline; does not clear running. */
int platform_timer_is_due(void);
uint32_t platform_timer_period_ms(void);
uint32_t platform_timer_deadline_ms(void);

/* 1 if running and now >= deadline. Clears running (one-shot). */
int platform_timer_take_due(void);

#ifdef __cplusplus
}
#endif

#endif
