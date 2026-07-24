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
    g_state.phase = GWY_PATH_A_RESPONSE_INITIAL_RECORD;
    /* Keep profile-declared record across queue resets within the same process. */
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

    if (!g_state.module_id && !g_state.module_generation) {
        g_state.module_id = module_id;
        g_state.module_generation = module_generation ? module_generation : 1ull;
        g_state.er_rw = er_rw;
        g_state.phase = GWY_PATH_A_RESPONSE_INITIAL_RECORD;
        g_state.initial_record_delivered = 0;
        printf("[PATH_A_RESPONSE_BIND] module_id=%llu generation=%llu er_rw=0x%X "
               "phase=INITIAL_RECORD reason=FIRST evidence=OBSERVED\n",
               (unsigned long long)g_state.module_id,
               (unsigned long long)g_state.module_generation, er_rw);
        fflush(stdout);
        return;
    }

    if (gen_changed) {
        g_state.module_id = module_id;
        g_state.module_generation = module_generation ? module_generation : 1ull;
        g_state.er_rw = er_rw;
        g_state.phase = GWY_PATH_A_RESPONSE_INITIAL_RECORD;
        g_state.initial_record_delivered = 0;
        printf("[PATH_A_RESPONSE_BIND] module_id=%llu generation=%llu er_rw=0x%X "
               "phase=INITIAL_RECORD reason=GENERATION_CHANGE evidence=OBSERVED\n",
               (unsigned long long)g_state.module_id,
               (unsigned long long)g_state.module_generation, er_rw);
        fflush(stdout);
        return;
    }

    /* Same generation: keep phase; refresh er_rw if a sibling base appears. */
    if (er_rw && !g_state.er_rw) g_state.er_rw = er_rw;
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
    if (g_state.phase == GWY_PATH_A_RESPONSE_EMPTY || g_state.initial_record_delivered) {
        printf("[PATH_A_RESPONSE_DECIDE] with_rec=0 reason=PHASE_EMPTY delivered=%d "
               "evidence=GENERATION\n",
               g_state.initial_record_delivered);
        fflush(stdout);
        return 0;
    }
    printf("[PATH_A_RESPONSE_DECIDE] with_rec=1 reason=INITIAL_RECORD phase=%d "
           "generation=%llu evidence=PROFILE\n",
           (int)g_state.phase, (unsigned long long)g_state.module_generation);
    fflush(stdout);
    return 1;
}

void platform_path_a_response_note_delivered(int with_record) {
    if (!with_record) return;
    g_state.initial_record_delivered = 1;
    g_state.phase = GWY_PATH_A_RESPONSE_EMPTY;
    printf("[PATH_A_RESPONSE_DELIVERED] phase=EMPTY generation=%llu er_rw=0x%X "
           "evidence=OBSERVED\n",
           (unsigned long long)g_state.module_generation, g_state.er_rw);
    fflush(stdout);
}
