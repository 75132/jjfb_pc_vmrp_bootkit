#include "gwy_launcher/ext_lifecycle.h"
#include <stdio.h>
#include <string.h>

#define GWY_EXT_LC_MAX 16

static GwyExtLifecycleRecord g_recs[GWY_EXT_LC_MAX];
static int g_n;
static char g_run_id[64];

void ext_lifecycle_reset(void) {
    memset(g_recs, 0, sizeof(g_recs));
    g_n = 0;
}

void ext_lifecycle_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *ext_lifecycle_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

const char *ext_lifecycle_state_name(GwyExtLifecycleState s) {
    switch (s) {
    case EXT_LIFECYCLE_DISCOVERED: return "DISCOVERED";
    case EXT_LIFECYCLE_MAPPED: return "MAPPED";
    case EXT_LIFECYCLE_BOOTSTRAP_ENTERED: return "BOOTSTRAP_ENTERED";
    case EXT_LIFECYCLE_BOOTSTRAP_RETURNED: return "BOOTSTRAP_RETURNED";
    case EXT_LIFECYCLE_ABI_READY: return "ABI_READY";
    case EXT_LIFECYCLE_VERSION_OK: return "VERSION_OK";
    case EXT_LIFECYCLE_APPINFO_OK: return "APPINFO_OK";
    case EXT_LIFECYCLE_INIT_OK: return "INIT_OK";
    case EXT_LIFECYCLE_RUNNING: return "RUNNING";
    case EXT_LIFECYCLE_FAILED: return "FAILED";
    default: return "UNKNOWN";
    }
}

static GwyExtLifecycleRecord *find_mut(uint64_t module_id) {
    int i;
    for (i = 0; i < g_n; i++) {
        if (g_recs[i].module_id == module_id) return &g_recs[i];
    }
    return NULL;
}

GwyExtLifecycleRecord *ext_lifecycle_ensure(uint64_t module_id, uint64_t generation,
                                            const char *module_name) {
    GwyExtLifecycleRecord *r = find_mut(module_id);
    if (!r) {
        if (g_n >= GWY_EXT_LC_MAX || !module_id) return NULL;
        r = &g_recs[g_n++];
        memset(r, 0, sizeof(*r));
        r->module_id = module_id;
        r->state = EXT_LIFECYCLE_DISCOVERED;
    }
    r->module_generation = generation;
    if (module_name && module_name[0])
        snprintf(r->module_name, sizeof(r->module_name), "%s", module_name);
    return r;
}

const GwyExtLifecycleRecord *ext_lifecycle_find(uint64_t module_id) { return find_mut(module_id); }

const GwyExtLifecycleRecord *ext_lifecycle_find_by_name(const char *module_name) {
    int i;
    if (!module_name || !module_name[0]) return NULL;
    for (i = 0; i < g_n; i++) {
        if (strcmp(g_recs[i].module_name, module_name) == 0) return &g_recs[i];
    }
    return NULL;
}

void ext_lifecycle_set_state(uint64_t module_id, GwyExtLifecycleState state) {
    GwyExtLifecycleRecord *r = find_mut(module_id);
    if (r) r->state = state;
}

void ext_lifecycle_note_bootstrap_enter(uint64_t module_id, uint64_t generation,
                                        const char *module_name, uint32_t pc) {
    GwyExtLifecycleRecord *r = ext_lifecycle_ensure(module_id, generation, module_name);
    if (!r) return;
    r->state = EXT_LIFECYCLE_BOOTSTRAP_ENTERED;
    printf("[ROBOTOL_BOOTSTRAP_ENTER] module=%s module_id=%llu generation=%llu pc=0x%X "
           "run_id=%s evidence=OBSERVED\n",
           r->module_name[0] ? r->module_name : "?", (unsigned long long)module_id,
           (unsigned long long)generation, pc, ext_lifecycle_run_id());
    fflush(stdout);
}

void ext_lifecycle_note_bootstrap_return(uint64_t module_id, int32_t bootstrap_return) {
    GwyExtLifecycleRecord *r = find_mut(module_id);
    if (!r) return;
    r->bootstrap_return = bootstrap_return;
    r->bootstrap_return_valid = 1;
    r->state = EXT_LIFECYCLE_BOOTSTRAP_RETURNED;
    printf("[ROBOTOL_BOOTSTRAP_RETURN] module=%s module_id=%llu generation=%llu "
           "bootstrap_return=%d run_id=%s evidence=OBSERVED\n",
           r->module_name[0] ? r->module_name : "?", (unsigned long long)module_id,
           (unsigned long long)r->module_generation, (int)bootstrap_return,
           ext_lifecycle_run_id());
    fflush(stdout);
}

void ext_lifecycle_note_abi_ready(uint64_t module_id) {
    GwyExtLifecycleRecord *r = find_mut(module_id);
    if (!r) return;
    r->state = EXT_LIFECYCLE_ABI_READY;
    printf("[ROBOTOL_ABI_READY] module=%s module_id=%llu generation=%llu run_id=%s "
           "evidence=OBSERVED\n",
           r->module_name[0] ? r->module_name : "?", (unsigned long long)module_id,
           (unsigned long long)r->module_generation, ext_lifecycle_run_id());
    fflush(stdout);
}

void ext_lifecycle_note_version(uint64_t module_id, int32_t ret) {
    GwyExtLifecycleRecord *r = find_mut(module_id);
    if (!r) return;
    r->version_return = ret;
    r->version_return_valid = 1;
    if (ret == 0)
        r->state = EXT_LIFECYCLE_VERSION_OK;
    else
        r->state = EXT_LIFECYCLE_FAILED;
}

void ext_lifecycle_note_appinfo(uint64_t module_id, int32_t ret) {
    GwyExtLifecycleRecord *r = find_mut(module_id);
    if (!r) return;
    r->appinfo_return = ret;
    r->appinfo_return_valid = 1;
    if (ret == 0)
        r->state = EXT_LIFECYCLE_APPINFO_OK;
    else
        r->state = EXT_LIFECYCLE_FAILED;
}

void ext_lifecycle_note_init(uint64_t module_id, int32_t ret) {
    GwyExtLifecycleRecord *r = find_mut(module_id);
    if (!r) return;
    r->init_return = ret;
    r->init_return_valid = 1;
    if (ret == 0) {
        r->state = EXT_LIFECYCLE_INIT_OK;
        printf("[ROBOTOL_INIT_RETURN_ZERO] module_id=%llu generation=%llu ret=0 "
               "run_id=%s evidence=OBSERVED\n",
               (unsigned long long)module_id, (unsigned long long)r->module_generation,
               ext_lifecycle_run_id());
        fflush(stdout);
    } else {
        r->state = EXT_LIFECYCLE_FAILED;
    }
}

void ext_lifecycle_note_failed(uint64_t module_id) {
    GwyExtLifecycleRecord *r = find_mut(module_id);
    if (r) r->state = EXT_LIFECYCLE_FAILED;
}
