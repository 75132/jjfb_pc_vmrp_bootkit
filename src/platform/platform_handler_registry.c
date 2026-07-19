#include "gwy_launcher/platform_handler_registry.h"
#include <string.h>

#define GWY_PLAT_HANDLER_SLOTS 8

typedef struct {
    uint32_t plat_code;
    uint32_t family;
    uint32_t handler;
    int used;
} PlatHandlerSlot;

static PlatHandlerSlot g_slots[GWY_PLAT_HANDLER_SLOTS];

void platform_handler_registry_reset(void) { memset(g_slots, 0, sizeof(g_slots)); }

static int handler_ok(uint32_t h) { return h && (h & ~1u) > 0x1000u; }

int platform_handler_registry_register(uint32_t plat_code, uint32_t family, uint32_t handler) {
    int i;
    int free_i = -1;
    if (!plat_code || !handler_ok(handler)) return 0;
    for (i = 0; i < GWY_PLAT_HANDLER_SLOTS; i++) {
        if (g_slots[i].used && g_slots[i].plat_code == plat_code) {
            g_slots[i].family = family;
            g_slots[i].handler = handler;
            return 1;
        }
        if (!g_slots[i].used && free_i < 0) free_i = i;
    }
    if (free_i < 0) return 0;
    g_slots[free_i].used = 1;
    g_slots[free_i].plat_code = plat_code;
    g_slots[free_i].family = family;
    g_slots[free_i].handler = handler;
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
