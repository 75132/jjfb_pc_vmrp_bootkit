#include "gwy_launcher/platform_timer.h"
#include <stdio.h>

static uint32_t g_fake_now;

static uint32_t fake_clock(void) { return g_fake_now; }

int main(void) {
    platform_timer_reset();
    platform_timer_set_clock(fake_clock);

    g_fake_now = 1000;
    platform_timer_start(50);
    if (!platform_timer_running() || platform_timer_period_ms() != 50) {
        fprintf(stderr, "start failed\n");
        return 1;
    }
    if (platform_timer_take_due()) {
        fprintf(stderr, "not due yet at t=1000 deadline=1050\n");
        return 1;
    }
    g_fake_now = 1049;
    if (platform_timer_take_due()) {
        fprintf(stderr, "not due at t=1049\n");
        return 1;
    }
    g_fake_now = 1050;
    if (!platform_timer_take_due()) {
        fprintf(stderr, "should be due at t=1050\n");
        return 1;
    }
    if (platform_timer_running() || platform_timer_take_due()) {
        fprintf(stderr, "one-shot should clear\n");
        return 1;
    }

    platform_timer_start(5000);
    g_fake_now = 2000;
    platform_timer_signal_due();
    if (!platform_timer_take_due()) {
        fprintf(stderr, "signal_due should make take_due succeed\n");
        return 1;
    }

    platform_timer_start(10);
    platform_timer_stop();
    g_fake_now = 99999;
    if (platform_timer_take_due()) {
        fprintf(stderr, "stop should clear due\n");
        return 1;
    }

    printf("[OK] platform_timer\n");
    return 0;
}
