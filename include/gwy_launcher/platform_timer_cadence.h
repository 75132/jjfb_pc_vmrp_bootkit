#ifndef GWY_LAUNCHER_PLATFORM_TIMER_CADENCE_H
#define GWY_LAUNCHER_PLATFORM_TIMER_CADENCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GwyTimerCadenceVerdict {
    GWY_TIMER_CADENCE_VALID = 0,
    GWY_TIMER_DUPLICATE_DELIVERY_FOUND = 1,
    GWY_TIMER_BACKLOG_BURST_FOUND = 2,
    GWY_TIMER_REARM_CONTRACT_BROKEN = 3
} GwyTimerCadenceVerdict;

void platform_timer_cadence_reset(void);
void platform_timer_cadence_set_run_id(const char *run_id);

const char *gwy_timer_cadence_verdict_name(GwyTimerCadenceVerdict v);

/* Register / re-arm a timer occurrence stream. */
void platform_timer_cadence_note_arm(uint32_t timer_id, uint64_t owner_generation, uint32_t period_ms,
                                     int repeating, const char *rearm_source);

/* Attempt to queue one occurrence. Returns 0 if duplicate while in-flight. */
int platform_timer_cadence_try_queue_occurrence(uint32_t timer_id, uint64_t occurrence_id);

void platform_timer_cadence_note_dequeue(uint32_t timer_id, uint64_t occurrence_id);
void platform_timer_cadence_note_inflight_begin(uint32_t timer_id, uint64_t occurrence_id);
void platform_timer_cadence_note_inflight_end(uint32_t timer_id, uint64_t occurrence_id, int ok);
void platform_timer_cadence_note_drop_coalesce(uint32_t timer_id);

int platform_timer_cadence_inflight(uint32_t timer_id);
uint32_t platform_timer_cadence_outstanding(void);
uint32_t platform_timer_cadence_delivery_count(void);
uint32_t platform_timer_cadence_duplicate_count(void);

GwyTimerCadenceVerdict platform_timer_cadence_classify(void);
void platform_timer_cadence_flush_csv(void);

#ifdef __cplusplus
}
#endif

#endif
