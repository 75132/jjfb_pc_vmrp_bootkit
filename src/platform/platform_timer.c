#include "gwy_launcher/platform_timer.h"

static PlatformTimerClockFn g_clock;
static int g_running;
static uint32_t g_period_ms;
static uint32_t g_deadline_ms;

void platform_timer_reset(void) {
    g_running = 0;
    g_period_ms = 0;
    g_deadline_ms = 0;
}

void platform_timer_set_clock(PlatformTimerClockFn clock_ms) { g_clock = clock_ms; }

void platform_timer_stop(void) {
    g_running = 0;
    g_period_ms = 0;
    g_deadline_ms = 0;
}

void platform_timer_start(uint32_t period_ms) {
    if (!g_clock || period_ms == 0 || period_ms > 60000u) {
        platform_timer_stop();
        return;
    }
    g_period_ms = period_ms;
    g_deadline_ms = g_clock() + period_ms;
    g_running = 1;
}

void platform_timer_signal_due(void) {
    if (!g_running || !g_clock) return;
    g_deadline_ms = g_clock();
}

/* 1 if running and now >= deadline; does not clear (for defer-while-busy). */
int platform_timer_is_due(void) {
    uint32_t now;
    if (!g_running || !g_clock) return 0;
    now = g_clock();
    return ((int32_t)(now - g_deadline_ms) >= 0) ? 1 : 0;
}

int platform_timer_running(void) { return g_running; }
uint32_t platform_timer_period_ms(void) { return g_period_ms; }
uint32_t platform_timer_deadline_ms(void) { return g_deadline_ms; }

int platform_timer_take_due(void) {
    uint32_t now;
    if (!g_running || !g_clock) return 0;
    now = g_clock();
    /* Unsigned wrap-safe: due when (now - deadline) < 2^31 for short periods. */
    if ((int32_t)(now - g_deadline_ms) < 0) return 0;
    g_running = 0;
    return 1;
}
