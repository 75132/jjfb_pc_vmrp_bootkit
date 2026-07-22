#include "gwy_launcher/platform_timer_cadence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GWY_TIMER_SLOTS 8

typedef struct {
    int used;
    uint32_t timer_id;
    uint64_t owner_generation;
    uint32_t period_ms;
    int repeating;
    char rearm_source[48];
    uint64_t next_due;
    uint32_t outstanding;
    int inflight;
    uint64_t inflight_occ;
    uint32_t delivery_count;
    uint32_t dropped;
    uint32_t duplicate;
    uint64_t last_queued_occ;
} TimerCadenceSlot;

static TimerCadenceSlot g_slots[GWY_TIMER_SLOTS];
static char g_run_id[64];
static uint32_t g_total_delivery;
static uint32_t g_total_dup;
static uint32_t g_total_drop;
static uint32_t g_max_outstanding;

static TimerCadenceSlot *find_slot(uint32_t timer_id) {
    int i;
    for (i = 0; i < GWY_TIMER_SLOTS; i++) {
        if (g_slots[i].used && g_slots[i].timer_id == timer_id) return &g_slots[i];
    }
    return NULL;
}

static TimerCadenceSlot *ensure_slot(uint32_t timer_id) {
    TimerCadenceSlot *s = find_slot(timer_id);
    int i;
    if (s) return s;
    for (i = 0; i < GWY_TIMER_SLOTS; i++) {
        if (!g_slots[i].used) {
            memset(&g_slots[i], 0, sizeof(g_slots[i]));
            g_slots[i].used = 1;
            g_slots[i].timer_id = timer_id;
            return &g_slots[i];
        }
    }
    return NULL;
}

void platform_timer_cadence_reset(void) {
    memset(g_slots, 0, sizeof(g_slots));
    g_total_delivery = g_total_dup = g_total_drop = 0;
    g_max_outstanding = 0;
}

void platform_timer_cadence_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *gwy_timer_cadence_verdict_name(GwyTimerCadenceVerdict v) {
    switch (v) {
    case GWY_TIMER_CADENCE_VALID: return "TIMER_CADENCE_VALID";
    case GWY_TIMER_DUPLICATE_DELIVERY_FOUND: return "TIMER_DUPLICATE_DELIVERY_FOUND";
    case GWY_TIMER_BACKLOG_BURST_FOUND: return "TIMER_BACKLOG_BURST_FOUND";
    case GWY_TIMER_REARM_CONTRACT_BROKEN: return "TIMER_REARM_CONTRACT_BROKEN";
    default: return "UNKNOWN";
    }
}

void platform_timer_cadence_note_arm(uint32_t timer_id, uint64_t owner_generation, uint32_t period_ms,
                                     int repeating, const char *rearm_source) {
    TimerCadenceSlot *s = ensure_slot(timer_id ? timer_id : 1u);
    if (!s) return;
    s->owner_generation = owner_generation;
    s->period_ms = period_ms;
    s->repeating = repeating;
    if (rearm_source)
        snprintf(s->rearm_source, sizeof(s->rearm_source), "%s", rearm_source);
}

int platform_timer_cadence_try_queue_occurrence(uint32_t timer_id, uint64_t occurrence_id) {
    TimerCadenceSlot *s = ensure_slot(timer_id ? timer_id : 1u);
    if (!s) return 0;
    if (s->inflight && s->inflight_occ == occurrence_id) {
        s->duplicate++;
        g_total_dup++;
        printf("[TIMER_CADENCE] op=DUP_REJECT timer_id=%u occ=%llu run_id=%s evidence=OBSERVED\n",
               s->timer_id, (unsigned long long)occurrence_id, g_run_id[0] ? g_run_id : "unknown");
        fflush(stdout);
        return 0;
    }
    if (s->outstanding > 0 && s->last_queued_occ == occurrence_id) {
        s->dropped++;
        g_total_drop++;
        return 0;
    }
    s->outstanding++;
    s->last_queued_occ = occurrence_id;
    if (s->outstanding > g_max_outstanding) g_max_outstanding = s->outstanding;
    return 1;
}

void platform_timer_cadence_note_dequeue(uint32_t timer_id, uint64_t occurrence_id) {
    TimerCadenceSlot *s = find_slot(timer_id ? timer_id : 1u);
    (void)occurrence_id;
    if (!s || s->outstanding == 0) return;
    s->outstanding--;
}

void platform_timer_cadence_note_inflight_begin(uint32_t timer_id, uint64_t occurrence_id) {
    TimerCadenceSlot *s = ensure_slot(timer_id ? timer_id : 1u);
    if (!s) return;
    if (s->inflight) {
        s->duplicate++;
        g_total_dup++;
        return;
    }
    s->inflight = 1;
    s->inflight_occ = occurrence_id;
}

void platform_timer_cadence_note_inflight_end(uint32_t timer_id, uint64_t occurrence_id, int ok) {
    TimerCadenceSlot *s = find_slot(timer_id ? timer_id : 1u);
    (void)occurrence_id;
    (void)ok;
    if (!s) return;
    s->inflight = 0;
    s->inflight_occ = 0;
    s->delivery_count++;
    g_total_delivery++;
}

void platform_timer_cadence_note_drop_coalesce(uint32_t timer_id) {
    TimerCadenceSlot *s = ensure_slot(timer_id ? timer_id : 1u);
    if (!s) return;
    s->dropped++;
    g_total_drop++;
}

int platform_timer_cadence_inflight(uint32_t timer_id) {
    TimerCadenceSlot *s = find_slot(timer_id ? timer_id : 1u);
    return s ? s->inflight : 0;
}

uint32_t platform_timer_cadence_outstanding(void) {
    int i;
    uint32_t n = 0;
    for (i = 0; i < GWY_TIMER_SLOTS; i++)
        if (g_slots[i].used) n += g_slots[i].outstanding;
    return n;
}

uint32_t platform_timer_cadence_delivery_count(void) { return g_total_delivery; }
uint32_t platform_timer_cadence_duplicate_count(void) { return g_total_dup; }

GwyTimerCadenceVerdict platform_timer_cadence_classify(void) {
    if (g_total_dup) return GWY_TIMER_DUPLICATE_DELIVERY_FOUND;
    if (g_max_outstanding > 4u) return GWY_TIMER_BACKLOG_BURST_FOUND;
    return GWY_TIMER_CADENCE_VALID;
}

void platform_timer_cadence_flush_csv(void) {
    char path[512];
    FILE *f;
    int i;
    const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
    GwyTimerCadenceVerdict v = platform_timer_cadence_classify();
    if (root && root[0])
        snprintf(path, sizeof(path), "%s/product_p3_timer_cadence.csv", root);
    else
        snprintf(path, sizeof(path), "reports/product_p3_timer_cadence.csv");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "run_id,timer_id,owner_generation,period_ms,repeating,rearm_source,outstanding,"
               "inflight,delivery_count,dropped,duplicate,verdict\n");
    for (i = 0; i < GWY_TIMER_SLOTS; i++) {
        TimerCadenceSlot *s = &g_slots[i];
        if (!s->used) continue;
        fprintf(f, "%s,%u,%llu,%u,%d,%s,%u,%d,%u,%u,%u,%s\n", g_run_id[0] ? g_run_id : "unknown",
                s->timer_id, (unsigned long long)s->owner_generation, s->period_ms, s->repeating,
                s->rearm_source[0] ? s->rearm_source : "?", s->outstanding, s->inflight,
                s->delivery_count, s->dropped, s->duplicate, gwy_timer_cadence_verdict_name(v));
    }
    if (g_total_delivery == 0) {
        fprintf(f, "%s,0,0,0,0,none,0,0,0,0,0,%s\n", g_run_id[0] ? g_run_id : "unknown",
                gwy_timer_cadence_verdict_name(v));
    }
    fclose(f);
    printf("[TIMER_CADENCE_VERDICT] verdict=%s deliveries=%u duplicates=%u max_outstanding=%u "
           "run_id=%s evidence=OBSERVED\n",
           gwy_timer_cadence_verdict_name(v), g_total_delivery, g_total_dup, g_max_outstanding,
           g_run_id[0] ? g_run_id : "unknown");
    fflush(stdout);
}
