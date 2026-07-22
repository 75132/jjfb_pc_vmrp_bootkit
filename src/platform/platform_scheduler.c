#include "gwy_launcher/platform_scheduler.h"
#include <stdio.h>
#include <string.h>

#define GWY_SCHED_Q_MAX 32
#define GWY_SCHED_POST_CB_MAX 10

static char g_run_id[64];
static GwyScheduledWork g_q[GWY_SCHED_Q_MAX];
static int g_q_n;
static uint64_t g_next_event_id = 1;

static uint64_t g_live_module_id;
static uint64_t g_live_generation;
static char g_live_module[64];

static int g_enqueued;
static int g_dequeued;
static int g_natural_cb;
static int g_work_source_seen;
static int g_forced_attempt_seen;
static int g_stale_reject_seen;
static int g_post_cb_remaining;

void platform_scheduler_reset(void) {
    memset(g_q, 0, sizeof(g_q));
    g_q_n = 0;
    g_next_event_id = 1;
    g_live_module_id = 0;
    g_live_generation = 0;
    g_live_module[0] = 0;
    g_enqueued = g_dequeued = 0;
    g_natural_cb = 0;
    g_work_source_seen = 0;
    g_forced_attempt_seen = 0;
    g_stale_reject_seen = 0;
    g_post_cb_remaining = 0;
}

void platform_scheduler_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *platform_scheduler_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

const char *gwy_scheduler_source_name(GwySchedulerSource s) {
    switch (s) {
    case GWY_SCHED_SRC_PLATFORM_TIMER: return "PLATFORM_TIMER";
    case GWY_SCHED_SRC_PLATFORM_EVENT: return "PLATFORM_EVENT";
    case GWY_SCHED_SRC_PLATFORM_COMPLETION: return "PLATFORM_COMPLETION";
    default: return "NONE";
    }
}

const char *gwy_scheduler_gap_name(GwySchedulerGapVerdict v) {
    switch (v) {
    case GWY_SCHED_INIT_ZERO_NO_NATURAL_WORK_SOURCE: return "INIT_ZERO_NO_NATURAL_WORK_SOURCE";
    case GWY_SCHED_NATURAL_WORK_NOT_ENQUEUED: return "NATURAL_WORK_NOT_ENQUEUED";
    case GWY_SCHED_NATURAL_WORK_ENQUEUED_NOT_DELIVERED: return "NATURAL_WORK_ENQUEUED_NOT_DELIVERED";
    case GWY_SCHED_NATURAL_CALLBACK_CONTEXT_INVALID: return "NATURAL_CALLBACK_CONTEXT_INVALID";
    case GWY_SCHED_NATURAL_CALLBACK: return "SCHEDULER_NATURAL_CALLBACK";
    default: return "NONE";
    }
}

void platform_scheduler_set_live_generation(uint64_t module_id, uint64_t generation,
                                            const char *module_name) {
    if (g_live_generation && generation && g_live_generation != generation)
        platform_scheduler_cancel_stale(g_live_generation);
    g_live_module_id = module_id;
    g_live_generation = generation;
    if (module_name && module_name[0])
        snprintf(g_live_module, sizeof(g_live_module), "%s", module_name);
}

void platform_scheduler_cancel_stale(uint64_t dead_generation) {
    int i, w = 0;
    for (i = 0; i < g_q_n; i++) {
        if (g_q[i].owner_generation == dead_generation) continue;
        if (w != i) g_q[w] = g_q[i];
        w++;
    }
    g_q_n = w;
}

void platform_scheduler_note_work_source(GwySchedulerSource source) {
    if (source != GWY_SCHED_SRC_NONE) g_work_source_seen = 1;
}

int platform_scheduler_work_source_seen(void) { return g_work_source_seen; }

int platform_scheduler_enqueue(const GwyScheduledWork *work, uint32_t guest_depth) {
    GwyScheduledWork item;
    (void)guest_depth; /* depth>0: still enqueue only; delivery gated in try_dequeue */
    if (!work || work->source == GWY_SCHED_SRC_NONE) return 0;
    if (g_q_n >= GWY_SCHED_Q_MAX) return 0;

    item = *work;
    if (!item.event_id) item.event_id = g_next_event_id++;
    if (!item.owner_module[0] && g_live_module[0])
        snprintf(item.owner_module, sizeof(item.owner_module), "%s", g_live_module);
    if (!item.owner_module_id) item.owner_module_id = g_live_module_id;
    if (!item.owner_generation) item.owner_generation = g_live_generation;

    if (g_live_generation && item.owner_generation &&
        item.owner_generation != g_live_generation) {
        g_stale_reject_seen = 1;
        printf("[SCHEDULER_ENQUEUE_REJECT] reason=STALE_GENERATION owner_generation=%llu "
               "live=%llu run_id=%s evidence=OBSERVED\n",
               (unsigned long long)item.owner_generation, (unsigned long long)g_live_generation,
               platform_scheduler_run_id());
        fflush(stdout);
        return 0;
    }

    g_q[g_q_n++] = item;
    g_enqueued++;
    g_work_source_seen = 1;

    printf("[SCHEDULER_ENQUEUE] source=%s owner_module=%s owner_module_id=%llu "
           "owner_generation=%llu handler=0x%X forced=%s guest_depth=%u event_id=%llu "
           "run_id=%s evidence=OBSERVED\n",
           gwy_scheduler_source_name(item.source),
           item.owner_module[0] ? item.owner_module : "?",
           (unsigned long long)item.owner_module_id, (unsigned long long)item.owner_generation,
           item.handler_or_helper, item.forced ? "yes" : "no", guest_depth,
           (unsigned long long)item.event_id, platform_scheduler_run_id());
    fflush(stdout);
    return 1;
}

int platform_scheduler_queue_depth(void) { return g_q_n; }

int platform_scheduler_peek(GwyScheduledWork *out) {
    if (!out || g_q_n <= 0) return 0;
    *out = g_q[0];
    return 1;
}

int platform_scheduler_try_dequeue(uint32_t guest_depth, GwyScheduledWork *out) {
    GwyScheduledWork item;
    int i;
    if (guest_depth > 0) return 0; /* enqueue only while nested */
    if (!out || g_q_n <= 0) return 0;

    item = g_q[0];
    for (i = 1; i < g_q_n; i++) g_q[i - 1] = g_q[i];
    g_q_n--;
    g_dequeued++;

    if (g_live_generation && item.owner_generation &&
        item.owner_generation != g_live_generation) {
        g_stale_reject_seen = 1;
        printf("[SCHEDULER_DEQUEUE] owner_module=%s generation_match=no "
               "owner_generation=%llu live=%llu run_id=%s evidence=OBSERVED\n",
               item.owner_module[0] ? item.owner_module : "?",
               (unsigned long long)item.owner_generation, (unsigned long long)g_live_generation,
               platform_scheduler_run_id());
        fflush(stdout);
        return 0;
    }

    printf("[SCHEDULER_DEQUEUE] owner_module=%s generation_match=yes "
           "owner_generation=%llu source=%s forced=%s run_id=%s evidence=OBSERVED\n",
           item.owner_module[0] ? item.owner_module : "?",
           (unsigned long long)item.owner_generation, gwy_scheduler_source_name(item.source),
           item.forced ? "yes" : "no", platform_scheduler_run_id());
    fflush(stdout);

    *out = item;
    return 1;
}

void platform_scheduler_note_natural_callback(const GwyScheduledWork *work, int32_t ret,
                                              int guest_entered) {
    if (!work) return;
    if (work->forced) {
        g_forced_attempt_seen = 1;
        printf("[SCHEDULER_CALLBACK_REJECT] reason=FORCED source=%s run_id=%s "
               "evidence=OBSERVED\n",
               gwy_scheduler_source_name(work->source), platform_scheduler_run_id());
        fflush(stdout);
        return;
    }
    if (!guest_entered) {
        printf("[SCHEDULER_CALLBACK_REJECT] reason=NO_GUEST_ENTRY run_id=%s evidence=OBSERVED\n",
               platform_scheduler_run_id());
        fflush(stdout);
        return;
    }
    g_natural_cb = 1;
    printf("[SCHEDULER_NATURAL_CALLBACK] owner_module=%s source=%s forced=no "
           "handler=0x%X ret=%d guest_entered=yes run_id=%s evidence=OBSERVED\n",
           work->owner_module[0] ? work->owner_module : "robotol.ext",
           gwy_scheduler_source_name(work->source), work->handler_or_helper, (int)ret,
           platform_scheduler_run_id());
    fflush(stdout);
    if (g_post_cb_remaining == 0) platform_scheduler_begin_post_callback_window();
}

int platform_scheduler_natural_callback_observed(void) { return g_natural_cb; }

GwySchedulerGapVerdict platform_scheduler_classify_gap(int init_was_zero, int handler_registered,
                                                       int work_source_seen) {
    (void)handler_registered;
    if (g_natural_cb) return GWY_SCHED_NATURAL_CALLBACK;
    if (!init_was_zero) return GWY_SCHED_GAP_NONE;
    if (!work_source_seen && !g_work_source_seen)
        return GWY_SCHED_INIT_ZERO_NO_NATURAL_WORK_SOURCE;
    if (g_enqueued == 0) return GWY_SCHED_NATURAL_WORK_NOT_ENQUEUED;
    if (g_dequeued == 0 || !g_natural_cb) {
        if (g_stale_reject_seen || g_forced_attempt_seen)
            return GWY_SCHED_NATURAL_CALLBACK_CONTEXT_INVALID;
        return GWY_SCHED_NATURAL_WORK_ENQUEUED_NOT_DELIVERED;
    }
    return GWY_SCHED_NATURAL_CALLBACK_CONTEXT_INVALID;
}

int platform_scheduler_enqueued_count(void) { return g_enqueued; }
int platform_scheduler_dequeued_count(void) { return g_dequeued; }

void platform_scheduler_begin_post_callback_window(void) {
    g_post_cb_remaining = GWY_SCHED_POST_CB_MAX;
}

int platform_scheduler_post_callback_remaining(void) { return g_post_cb_remaining; }

void platform_scheduler_consume_post_callback_slot(void) {
    if (g_post_cb_remaining > 0) g_post_cb_remaining--;
}
