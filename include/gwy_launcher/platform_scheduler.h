#ifndef GWY_LAUNCHER_PLATFORM_SCHEDULER_H
#define GWY_LAUNCHER_PLATFORM_SCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GwySchedulerSource {
    GWY_SCHED_SRC_NONE = 0,
    GWY_SCHED_SRC_PLATFORM_TIMER = 1,
    GWY_SCHED_SRC_PLATFORM_EVENT = 2,
    GWY_SCHED_SRC_PLATFORM_COMPLETION = 3
} GwySchedulerSource;

typedef struct GwyScheduledWork {
    uint64_t event_id;
    uint64_t owner_module_id;
    uint64_t owner_generation;

    GwySchedulerSource source;
    uint32_t handler_or_helper;
    uint32_t method_or_event;
    uint64_t due_time;

    int forced;
    char owner_module[64];
} GwyScheduledWork;

typedef enum GwySchedulerGapVerdict {
    GWY_SCHED_GAP_NONE = 0,
    GWY_SCHED_INIT_ZERO_NO_NATURAL_WORK_SOURCE = 1,
    GWY_SCHED_NATURAL_WORK_NOT_ENQUEUED = 2,
    GWY_SCHED_NATURAL_WORK_ENQUEUED_NOT_DELIVERED = 3,
    GWY_SCHED_NATURAL_CALLBACK_CONTEXT_INVALID = 4,
    GWY_SCHED_NATURAL_CALLBACK = 5
} GwySchedulerGapVerdict;

void platform_scheduler_reset(void);
void platform_scheduler_set_run_id(const char *run_id);
const char *platform_scheduler_run_id(void);

const char *gwy_scheduler_source_name(GwySchedulerSource s);
const char *gwy_scheduler_gap_name(GwySchedulerGapVerdict v);

/* Current live owner generation for stale checks (0 = unset). */
void platform_scheduler_set_live_generation(uint64_t module_id, uint64_t generation,
                                            const char *module_name);
void platform_scheduler_cancel_stale(uint64_t dead_generation);

/*
 * Enqueue platform work. If guest_depth > 0, only enqueues (never delivers).
 * forced=1 items cannot satisfy the natural-callback gate.
 */
int platform_scheduler_enqueue(const GwyScheduledWork *work, uint32_t guest_depth);

int platform_scheduler_queue_depth(void);
int platform_scheduler_peek(GwyScheduledWork *out);

/*
 * Drain at most one item when guest_depth==0.
 * Returns 1 if an item was dequeued into *out (caller delivers guest callback).
 * Rejects stale generation / forced items for natural gate bookkeeping.
 */
int platform_scheduler_try_dequeue(uint32_t guest_depth, GwyScheduledWork *out);

/* After a successful natural guest callback return/fault. */
void platform_scheduler_note_natural_callback(const GwyScheduledWork *work, int32_t ret,
                                              int guest_entered);

int platform_scheduler_natural_callback_observed(void);
GwySchedulerGapVerdict platform_scheduler_classify_gap(int init_was_zero, int handler_registered,
                                                       int work_source_seen);

/* Test: mark that a work source (timer/event) was registered by guest. */
void platform_scheduler_note_work_source(GwySchedulerSource source);
int platform_scheduler_work_source_seen(void);

int platform_scheduler_enqueued_count(void);
int platform_scheduler_dequeued_count(void);

/* Post-callback drain budget (Lane I). */
void platform_scheduler_begin_post_callback_window(void);
int platform_scheduler_post_callback_remaining(void);
void platform_scheduler_consume_post_callback_slot(void);

#ifdef __cplusplus
}
#endif

#endif
