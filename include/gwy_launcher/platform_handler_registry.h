#ifndef GWY_LAUNCHER_PLATFORM_HANDLER_REGISTRY_H
#define GWY_LAUNCHER_PLATFORM_HANDLER_REGISTRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic plat handler registry (docs/06).
 * Stores guest VAs from live sendAppEvent registrations only — no fixed addresses.
 * P2: owner_module_id + generation required for product ROBOTOL_HANDLER_REGISTERED gate.
 */

typedef enum GwyHandlerIsa {
    GWY_HANDLER_ISA_UNKNOWN = 0,
    GWY_HANDLER_ISA_ARM = 1,
    GWY_HANDLER_ISA_THUMB = 2
} GwyHandlerIsa;

typedef struct GwyPlatformHandlerRecord {
    uint32_t plat_code;
    uint32_t family;
    uint32_t handler;
    uint64_t owner_module_id;
    uint64_t owner_generation;
    char owner_module[64];
    GwyHandlerIsa isa;
    char source_api[48];
    int used;
    int product_accepted;
} GwyPlatformHandlerRecord;

void platform_handler_registry_reset(void);
void platform_handler_registry_set_run_id(const char *run_id);

/* Legacy: stores handler without owner proof (not enough for product gate). */
int platform_handler_registry_register(uint32_t plat_code, uint32_t family, uint32_t handler);

/*
 * Owner-scoped registration. Rejects DSM / mrc_loader / empty owner / unmapped handler.
 * Emits [PLATFORM_HANDLER_REGISTERED] only when accepted for product.
 */
int platform_handler_registry_register_owned(uint32_t plat_code, uint32_t family, uint32_t handler,
                                             uint64_t owner_module_id, uint64_t owner_generation,
                                             const char *owner_module, GwyHandlerIsa isa,
                                             const char *source_api);

uint32_t platform_handler_registry_get(uint32_t plat_code);
uint32_t platform_handler_registry_family(uint32_t plat_code);
int platform_handler_registry_has(uint32_t plat_code);

/*
 * Find product-accepted 0x10102 family registration whose family band covers
 * event_code (same top 24 bits). Returns NULL if none.
 */
const GwyPlatformHandlerRecord *platform_handler_registry_find_family_event(uint32_t event_code);

/* Enqueue-handler registration (0x10165), if product-accepted. */
const GwyPlatformHandlerRecord *platform_handler_registry_enqueue_handler(void);

/* Product gate: robotol-owned registration observed this run. */
int platform_handler_registry_robotol_owned_observed(void);
const GwyPlatformHandlerRecord *platform_handler_registry_robotol_record(void);

/* Loose text patterns must NOT satisfy product gate (unit-tested). */
int platform_handler_registry_text_mentions_handler(const char *line);

#ifdef __cplusplus
}
#endif

#endif
