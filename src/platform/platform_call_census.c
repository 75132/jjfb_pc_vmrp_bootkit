#include "gwy_launcher/platform_call_census.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CENSUS_MAX 96
#define MISS_MAX 48

typedef struct CensusSlot {
    int used;
    uint32_t code;
    uint32_t app;
    uint32_t caller_pc;
    uint32_t ret;
    uint32_t count;
    uint32_t first_tick;
    uint32_t last_tick;
} CensusSlot;

typedef struct MissSlot {
    int used;
    char path[160];
    uint32_t count;
} MissSlot;

static struct {
    int enabled_known;
    int enabled;
    int t1e209_known;
    int t1e209;
    uint32_t tick;
    CensusSlot slots[CENSUS_MAX];
    uint32_t draw_n;
    uint32_t refresh_n;
    uint32_t file_miss_n;
    uint32_t network_n;
    MissSlot misses[MISS_MAX];
    uint32_t t1e209_n;
} g_cen;

static int env1(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

int platform_call_census_enabled(void) {
    if (!g_cen.enabled_known) {
        g_cen.enabled = env1("JJFB_PLAT_CENSUS");
        g_cen.enabled_known = 1;
    }
    return g_cen.enabled;
}

int platform_1e209_trace_enabled(void) {
    if (!g_cen.t1e209_known) {
        g_cen.t1e209 = env1("JJFB_PLAT_1E209_TRACE");
        g_cen.t1e209_known = 1;
    }
    return g_cen.t1e209;
}

void platform_call_census_reset(void) {
    memset(&g_cen, 0, sizeof(g_cen));
}

void platform_call_census_set_tick(uint32_t tick) { g_cen.tick = tick; }

void platform_call_census_note(uint32_t code, uint32_t app_or_family, uint32_t caller_pc,
                               uint32_t ret) {
    int i, free_i = -1;
    uint32_t tick;
    if (!platform_call_census_enabled()) return;
    tick = g_cen.tick ? g_cen.tick : 1u;
    for (i = 0; i < CENSUS_MAX; i++) {
        CensusSlot *s = &g_cen.slots[i];
        if (!s->used) {
            if (free_i < 0) free_i = i;
            continue;
        }
        if (s->code == code && s->app == app_or_family && s->caller_pc == caller_pc &&
            s->ret == ret) {
            s->count++;
            s->last_tick = tick;
            return;
        }
    }
    if (free_i < 0) return;
    g_cen.slots[free_i].used = 1;
    g_cen.slots[free_i].code = code;
    g_cen.slots[free_i].app = app_or_family;
    g_cen.slots[free_i].caller_pc = caller_pc;
    g_cen.slots[free_i].ret = ret;
    g_cen.slots[free_i].count = 1;
    g_cen.slots[free_i].first_tick = tick;
    g_cen.slots[free_i].last_tick = tick;
}

void platform_call_census_note_draw(void) {
    if (!platform_call_census_enabled()) return;
    g_cen.draw_n++;
}

void platform_call_census_note_refresh(void) {
    if (!platform_call_census_enabled()) return;
    g_cen.refresh_n++;
}

void platform_call_census_note_file_miss(const char *guest_path) {
    int i, free_i = -1;
    if (!platform_call_census_enabled()) return;
    g_cen.file_miss_n++;
    if (!guest_path || !guest_path[0]) return;
    for (i = 0; i < MISS_MAX; i++) {
        if (!g_cen.misses[i].used) {
            if (free_i < 0) free_i = i;
            continue;
        }
        if (strncmp(g_cen.misses[i].path, guest_path, sizeof(g_cen.misses[i].path) - 1) == 0) {
            g_cen.misses[i].count++;
            return;
        }
    }
    if (free_i < 0) return;
    g_cen.misses[free_i].used = 1;
    g_cen.misses[free_i].count = 1;
    snprintf(g_cen.misses[free_i].path, sizeof(g_cen.misses[free_i].path), "%s", guest_path);
}

void platform_call_census_note_networkish(const char *tag) {
    if (!platform_call_census_enabled()) return;
    g_cen.network_n++;
    if (tag && tag[0]) {
        printf("[JJFB_PLAT_CENSUS_NET] tag=%s count=%u evidence=OBSERVED\n", tag, g_cen.network_n);
        fflush(stdout);
    }
}

void platform_call_census_dump(const char *reason) {
    int i;
    if (!platform_call_census_enabled()) return;
    printf("[JJFB_PLAT_CENSUS_SUMMARY] reason=%s draw=%u refresh=%u file_miss=%u networkish=%u "
           "1e209_trace_calls=%u tick=%u evidence=OBSERVED\n",
           reason ? reason : "?", g_cen.draw_n, g_cen.refresh_n, g_cen.file_miss_n, g_cen.network_n,
           g_cen.t1e209_n, g_cen.tick);
    for (i = 0; i < CENSUS_MAX; i++) {
        CensusSlot *s = &g_cen.slots[i];
        if (!s->used) continue;
        printf("[JJFB_PLAT_CENSUS] code=0x%X app=0x%X caller_pc=0x%X ret=0x%X count=%u "
               "first_tick=%u last_tick=%u evidence=OBSERVED\n",
               s->code, s->app, s->caller_pc, s->ret, s->count, s->first_tick, s->last_tick);
    }
    for (i = 0; i < MISS_MAX; i++) {
        if (!g_cen.misses[i].used) continue;
        printf("[JJFB_PLAT_CENSUS_MISS] path=\"%s\" count=%u evidence=OBSERVED\n",
               g_cen.misses[i].path, g_cen.misses[i].count);
    }
    fflush(stdout);
}

void platform_1e209_trace_call(uint32_t caller_pc, uint32_t r0, uint32_t r1, uint32_t r2,
                               uint32_t r3, uint32_t ret, uint32_t tick) {
    if (!platform_1e209_trace_enabled()) return;
    if (!(r0 == 0x1E209u && r1 == 0x9u)) return;
    g_cen.t1e209_n++;
    printf("[JJFB_PLAT_1E209] tick=%u caller_pc=0x%X R0=0x%X R1=0x%X R2=0x%X R3=0x%X ret=0x%X "
           "note=observe_only_no_ret_mutation evidence=OBSERVED\n",
           tick ? tick : g_cen.tick, caller_pc, r0, r1, r2, r3, ret);
    fflush(stdout);
}
