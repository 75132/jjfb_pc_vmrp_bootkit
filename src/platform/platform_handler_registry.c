#include "gwy_launcher/platform_handler_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GWY_PLAT_HANDLER_SLOTS 8

static GwyPlatformHandlerRecord g_slots[GWY_PLAT_HANDLER_SLOTS];
static char g_run_id[64];
static int g_robotol_owned;

void platform_handler_registry_reset(void) {
    memset(g_slots, 0, sizeof(g_slots));
    g_robotol_owned = 0;
}

void platform_handler_registry_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

static const char *run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

static int handler_ok(uint32_t h) { return h && (h & ~1u) > 0x1000u; }

static int owner_rejected(const char *owner_module, uint64_t owner_module_id) {
    if (!owner_module_id || !owner_module || !owner_module[0]) return 1;
    if (strstr(owner_module, "mrc_loader") != NULL) return 1;
    if (strstr(owner_module, "dsm:") != NULL) return 1;
    if (strcmp(owner_module, "cfunction.ext") == 0) return 1; /* DSM cfunction */
    return 0;
}

static int is_robotol_owner(const char *owner_module) {
    return owner_module && strstr(owner_module, "robotol") != NULL;
}

static void append_csv(const GwyPlatformHandlerRecord *r, const char *verdict) {
    char path[512];
    FILE *f;
    const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (root && root[0])
        snprintf(path, sizeof(path), "%s/product_handler_registration.csv", root);
    else
        snprintf(path, sizeof(path), "reports/product_handler_registration.csv");
    f = fopen(path, "a");
    if (!f) {
        f = fopen(path, "w");
        if (!f) return;
        fprintf(f, "run_id,plat_code,family,handler,owner_module,owner_module_id,"
                   "owner_generation,isa,source_api,verdict\n");
    } else {
        /* append mode; header may already exist */
    }
    fprintf(f, "%s,0x%X,0x%X,0x%X,%s,%llu,%llu,%s,%s,%s\n", run_id(), r->plat_code, r->family,
            r->handler, r->owner_module[0] ? r->owner_module : "?",
            (unsigned long long)r->owner_module_id, (unsigned long long)r->owner_generation,
            r->isa == GWY_HANDLER_ISA_THUMB ? "THUMB"
            : r->isa == GWY_HANDLER_ISA_ARM ? "ARM"
                                           : "UNKNOWN",
            r->source_api[0] ? r->source_api : "?", verdict ? verdict : "?");
    fclose(f);
}

int platform_handler_registry_register(uint32_t plat_code, uint32_t family, uint32_t handler) {
    int i;
    int free_i = -1;
    if (!plat_code || !handler_ok(handler)) return 0;
    for (i = 0; i < GWY_PLAT_HANDLER_SLOTS; i++) {
        if (g_slots[i].used && g_slots[i].plat_code == plat_code) {
            g_slots[i].family = family;
            g_slots[i].handler = handler;
            /* Legacy path does not set product_accepted. */
            return 1;
        }
        if (!g_slots[i].used && free_i < 0) free_i = i;
    }
    if (free_i < 0) return 0;
    g_slots[free_i].used = 1;
    g_slots[free_i].plat_code = plat_code;
    g_slots[free_i].family = family;
    g_slots[free_i].handler = handler;
    g_slots[free_i].product_accepted = 0;
    return 1;
}

int platform_handler_registry_register_owned(uint32_t plat_code, uint32_t family, uint32_t handler,
                                             uint64_t owner_module_id, uint64_t owner_generation,
                                             const char *owner_module, GwyHandlerIsa isa,
                                             const char *source_api) {
    int i;
    int free_i = -1;
    GwyPlatformHandlerRecord *slot = NULL;

    if (!plat_code || !handler_ok(handler)) return 0;
    if (owner_rejected(owner_module, owner_module_id)) {
        GwyPlatformHandlerRecord tmp;
        memset(&tmp, 0, sizeof(tmp));
        tmp.plat_code = plat_code;
        tmp.family = family;
        tmp.handler = handler;
        tmp.owner_module_id = owner_module_id;
        tmp.owner_generation = owner_generation;
        tmp.isa = isa;
        if (owner_module) snprintf(tmp.owner_module, sizeof(tmp.owner_module), "%s", owner_module);
        if (source_api) snprintf(tmp.source_api, sizeof(tmp.source_api), "%s", source_api);
        append_csv(&tmp, "REJECTED_OWNER");
        printf("[PLATFORM_HANDLER_REJECT] reason=OWNER_NOT_PRODUCT owner=%s module_id=%llu "
               "handler=0x%X run_id=%s evidence=OBSERVED\n",
               owner_module ? owner_module : "?", (unsigned long long)owner_module_id, handler,
               run_id());
        fflush(stdout);
        return 0;
    }

    for (i = 0; i < GWY_PLAT_HANDLER_SLOTS; i++) {
        if (g_slots[i].used && g_slots[i].plat_code == plat_code) {
            slot = &g_slots[i];
            break;
        }
        if (!g_slots[i].used && free_i < 0) free_i = i;
    }
    if (!slot) {
        if (free_i < 0) return 0;
        slot = &g_slots[free_i];
        memset(slot, 0, sizeof(*slot));
        slot->used = 1;
        slot->plat_code = plat_code;
    }

    slot->family = family;
    slot->handler = handler;
    slot->owner_module_id = owner_module_id;
    slot->owner_generation = owner_generation;
    slot->isa = isa;
    snprintf(slot->owner_module, sizeof(slot->owner_module), "%s", owner_module);
    snprintf(slot->source_api, sizeof(slot->source_api), "%s",
             source_api && source_api[0] ? source_api : "sendAppEvent");
    slot->product_accepted = is_robotol_owner(owner_module) ? 1 : 0;

    if (slot->product_accepted) {
        g_robotol_owned = 1;
        printf("[PLATFORM_HANDLER_REGISTERED] owner_module=%s owner_module_id=%llu "
               "owner_generation=%llu handler=0x%X handler_mapped=yes instruction_set=%s "
               "source_api=%s run_id=%s evidence=OBSERVED\n",
               slot->owner_module, (unsigned long long)slot->owner_module_id,
               (unsigned long long)slot->owner_generation, slot->handler,
               slot->isa == GWY_HANDLER_ISA_THUMB ? "THUMB" : "ARM", slot->source_api, run_id());
        printf("[ROBOTOL_HANDLER_REGISTERED] owner_module=%s handler=0x%X run_id=%s "
               "evidence=OBSERVED\n",
               slot->owner_module, slot->handler, run_id());
        fflush(stdout);
        append_csv(slot, "ACCEPTED");
    } else {
        append_csv(slot, "STORED_NOT_ROBOTOL");
    }
    return 1;
}

uint32_t platform_handler_registry_get(uint32_t plat_code) {
    int i;
    for (i = 0; i < GWY_PLAT_HANDLER_SLOTS; i++) {
        if (g_slots[i].used && g_slots[i].plat_code == plat_code) return g_slots[i].handler;
    }
    return 0;
}

uint32_t platform_handler_registry_family(uint32_t plat_code) {
    int i;
    for (i = 0; i < GWY_PLAT_HANDLER_SLOTS; i++) {
        if (g_slots[i].used && g_slots[i].plat_code == plat_code) return g_slots[i].family;
    }
    return 0;
}

int platform_handler_registry_has(uint32_t plat_code) {
    return platform_handler_registry_get(plat_code) != 0;
}

const GwyPlatformHandlerRecord *platform_handler_registry_find_family_event(uint32_t event_code) {
    int i;
    for (i = 0; i < GWY_PLAT_HANDLER_SLOTS; i++) {
        if (!g_slots[i].used || !g_slots[i].product_accepted) continue;
        if (g_slots[i].plat_code != 0x10102u) continue;
        if (!g_slots[i].family || !g_slots[i].handler) continue;
        /* Same top-24-bit family band (0x1E200 covers 0x1E209). */
        if ((event_code & 0xFFFFFF00u) == (g_slots[i].family & 0xFFFFFF00u)) return &g_slots[i];
    }
    return NULL;
}

const GwyPlatformHandlerRecord *platform_handler_registry_enqueue_handler(void) {
    int i;
    for (i = 0; i < GWY_PLAT_HANDLER_SLOTS; i++) {
        if (!g_slots[i].used || !g_slots[i].product_accepted) continue;
        if (g_slots[i].plat_code == 0x10165u && g_slots[i].handler) return &g_slots[i];
    }
    return NULL;
}

int platform_handler_registry_robotol_owned_observed(void) { return g_robotol_owned; }

const GwyPlatformHandlerRecord *platform_handler_registry_robotol_record(void) {
    int i;
    for (i = 0; i < GWY_PLAT_HANDLER_SLOTS; i++) {
        if (g_slots[i].used && g_slots[i].product_accepted) return &g_slots[i];
    }
    return NULL;
}

int platform_handler_registry_text_mentions_handler(const char *line) {
    if (!line) return 0;
    if (strstr(line, "platform_handler") != NULL) return 1;
    if (strstr(line, "HANDLER_REGISTER") != NULL) return 1;
    if (strstr(line, "PLAT") != NULL && strstr(line, "register") != NULL) return 1;
    if (strstr(line, "handler") != NULL) return 1;
    return 0;
}
