#include "gwy_launcher/platform_path_a_response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GwyPathAResponseState g_state;
static GwyPathAInitialRecord g_rec;
static int g_rec_set;

static int env_override(void) {
    /* -1 = unset, 0 = force empty, 1 = force one-record once */
    const char *e = getenv("JJFB_101AB_WITH_RECORD");
    if (!e || !e[0]) return -1;
    if (e[0] == '0' && e[1] == '\0') return 0;
    if (e[0] == '1' && e[1] == '\0') return 1;
    return -1;
}

static const char *phase_name(GwyPathAResponsePhase p) {
    switch (p) {
    case GWY_PATH_A_RESPONSE_PRIMING_EMPTY:
        return "PRIMING_EMPTY";
    case GWY_PATH_A_RESPONSE_INITIAL_RECORD:
        return "INITIAL_RECORD";
    case GWY_PATH_A_RESPONSE_EMPTY:
        return "EMPTY";
    default:
        return "?";
    }
}

static void apply_builtin_defaults(GwyPathAInitialRecord *r) {
    memset(r, 0, sizeof(*r));
    r->enabled = 0;
    r->tag = 1u;
    snprintf(r->name, sizeof(r->name), "%s", "downVersion");
    r->secondary[0] = '\0';
    r->field_c = 0;
    r->field_d = 0x3EEu; /* 1006 */
    r->deliver_once_per_generation = 1;
}

void platform_path_a_response_reset(void) {
    memset(&g_state, 0, sizeof(g_state));
    /* Product default remains record-first (Task 11 B71). PRIMING_EMPTY is opt-in
     * via empty-first experiments; V75 empty→2FC26C currently hangs in drawFP DSM. */
    g_state.phase = GWY_PATH_A_RESPONSE_INITIAL_RECORD;
}

void platform_path_a_response_set_initial_record(const GwyPathAInitialRecord *rec) {
    if (!rec) {
        apply_builtin_defaults(&g_rec);
        g_rec_set = 0;
        return;
    }
    g_rec = *rec;
    if (!g_rec.name[0])
        snprintf(g_rec.name, sizeof(g_rec.name), "%s", "downVersion");
    if (g_rec.tag == 0) g_rec.tag = 1u;
    g_rec_set = 1;
    printf("[PATH_A_RESPONSE_CFG] enabled=%d tag=%u name=%s secondary=%s "
           "field_c=%u field_d=0x%X once_per_gen=%d evidence=PROFILE\n",
           g_rec.enabled, g_rec.tag, g_rec.name, g_rec.secondary[0] ? g_rec.secondary : "",
           g_rec.field_c, g_rec.field_d, g_rec.deliver_once_per_generation);
    fflush(stdout);
}

const GwyPathAInitialRecord *platform_path_a_response_initial_record(void) {
    if (!g_rec_set) apply_builtin_defaults(&g_rec);
    return &g_rec;
}

const GwyPathAResponseState *platform_path_a_response_state(void) { return &g_state; }

void platform_path_a_response_bind(uint64_t module_id, uint64_t module_generation,
                                   uint32_t er_rw) {
    int gen_changed =
        (g_state.module_id != 0 && g_state.module_id != module_id) ||
        (module_generation != 0 && g_state.module_generation != 0 &&
         g_state.module_generation != module_generation);
    /* Task 13 / V75: empty priming → leave_2FC26C → second record → B71 → C0.
     * Record-first (old default) writes B71 before E6C exists → 2FEC3C Case A fault.
     * Opt into record-first only: JJFB_PATH_A_RECORD_FIRST=1 (or EMPTY_FIRST=0 legacy). */
    GwyPathAResponsePhase start_phase = GWY_PATH_A_RESPONSE_PRIMING_EMPTY;
    {
        const char *rec1 = getenv("JJFB_PATH_A_RECORD_FIRST");
        const char *e = getenv("JJFB_PATH_A_EMPTY_FIRST");
        if (rec1 && rec1[0] == '1' && rec1[1] == '\0')
            start_phase = GWY_PATH_A_RESPONSE_INITIAL_RECORD;
        else if (e && e[0] == '0' && e[1] == '\0')
            start_phase = GWY_PATH_A_RESPONSE_INITIAL_RECORD;
        else if (e && e[0] == '1' && e[1] == '\0')
            start_phase = GWY_PATH_A_RESPONSE_PRIMING_EMPTY;
    }

    if (!g_state.module_id && !g_state.module_generation) {
        g_state.module_id = module_id;
        g_state.module_generation = module_generation ? module_generation : 1ull;
        g_state.er_rw = er_rw;
        g_state.phase = start_phase;
        g_state.initial_record_delivered = 0;
        printf("[PATH_A_RESPONSE_BIND] module_id=%llu generation=%llu er_rw=0x%X "
               "phase=%s reason=FIRST evidence=OBSERVED\n",
               (unsigned long long)g_state.module_id,
               (unsigned long long)g_state.module_generation, er_rw, phase_name(start_phase));
        fflush(stdout);
        return;
    }

    if (gen_changed) {
        g_state.module_id = module_id;
        g_state.module_generation = module_generation ? module_generation : 1ull;
        g_state.er_rw = er_rw;
        g_state.phase = start_phase;
        g_state.initial_record_delivered = 0;
        printf("[PATH_A_RESPONSE_BIND] module_id=%llu generation=%llu er_rw=0x%X "
               "phase=%s reason=GENERATION_CHANGE evidence=OBSERVED\n",
               (unsigned long long)g_state.module_id,
               (unsigned long long)g_state.module_generation, er_rw, phase_name(start_phase));
        fflush(stdout);
        return;
    }

    /* Same generation: keep phase; refresh er_rw if a sibling base appears. */
    if (er_rw && !g_state.er_rw) g_state.er_rw = er_rw;
}

int platform_path_a_response_ready_for_record(void) {
    const GwyPathAInitialRecord *rec = platform_path_a_response_initial_record();
    if (!rec->enabled) return 0;
    if (g_state.initial_record_delivered) return 0;
    return g_state.phase == GWY_PATH_A_RESPONSE_INITIAL_RECORD ||
           g_state.phase == GWY_PATH_A_RESPONSE_PRIMING_EMPTY;
}

int platform_path_a_response_decide_with_record(void) {
    int ov = env_override();
    const GwyPathAInitialRecord *rec = platform_path_a_response_initial_record();

    if (ov == 0) {
        printf("[PATH_A_RESPONSE_DECIDE] with_rec=0 reason=ENV_FORCE_EMPTY evidence=OVERRIDE\n");
        fflush(stdout);
        return 0;
    }

    if (ov == 1) {
        if (g_state.initial_record_delivered && rec->deliver_once_per_generation) {
            printf("[PATH_A_RESPONSE_DECIDE] with_rec=0 reason=ENV_ALREADY_DELIVERED "
                   "evidence=OVERRIDE\n");
            fflush(stdout);
            return 0;
        }
        printf("[PATH_A_RESPONSE_DECIDE] with_rec=1 reason=ENV_FORCE_RECORD evidence=OVERRIDE\n");
        fflush(stdout);
        return 1;
    }

    /* Product default: profile declaration only (no env). */
    if (!rec->enabled) {
        printf("[PATH_A_RESPONSE_DECIDE] with_rec=0 reason=PROFILE_DISABLED evidence=PROFILE\n");
        fflush(stdout);
        return 0;
    }
    /* Opt-in empty-first: only when explicitly in PRIMING_EMPTY (not product default). */
    if (g_state.phase == GWY_PATH_A_RESPONSE_PRIMING_EMPTY) {
        printf("[PATH_A_RESPONSE_DECIDE] with_rec=0 reason=PRIMING_EMPTY phase=%s "
               "generation=%llu evidence=OBSERVED+V75\n",
               phase_name(g_state.phase), (unsigned long long)g_state.module_generation);
        fflush(stdout);
        return 0;
    }
    if (g_state.phase == GWY_PATH_A_RESPONSE_EMPTY || g_state.initial_record_delivered) {
        printf("[PATH_A_RESPONSE_DECIDE] with_rec=0 reason=PHASE_EMPTY delivered=%d "
               "evidence=GENERATION\n",
               g_state.initial_record_delivered);
        fflush(stdout);
        return 0;
    }
    printf("[PATH_A_RESPONSE_DECIDE] with_rec=1 reason=INITIAL_RECORD phase=%s "
           "generation=%llu evidence=PROFILE\n",
           phase_name(g_state.phase), (unsigned long long)g_state.module_generation);
    fflush(stdout);
    return 1;
}

void platform_path_a_response_note_delivered(int with_record) {
    if (with_record) {
        g_state.initial_record_delivered = 1;
        g_state.phase = GWY_PATH_A_RESPONSE_EMPTY;
        printf("[PATH_A_RESPONSE_DELIVERED] phase=EMPTY generation=%llu er_rw=0x%X "
               "evidence=OBSERVED\n",
               (unsigned long long)g_state.module_generation, g_state.er_rw);
        fflush(stdout);
        return;
    }
    /* Empty priming fill (opt-in): advance so post-2FC26C second enqueue can embed record. */
    if (g_state.phase == GWY_PATH_A_RESPONSE_PRIMING_EMPTY) {
        g_state.phase = GWY_PATH_A_RESPONSE_INITIAL_RECORD;
        printf("[PATH_A_RESPONSE_DELIVERED] phase=INITIAL_RECORD generation=%llu er_rw=0x%X "
               "note=after_priming_empty evidence=OBSERVED+V75\n",
               (unsigned long long)g_state.module_generation, g_state.er_rw);
        fflush(stdout);
    }
}
