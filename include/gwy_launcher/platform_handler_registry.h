#ifndef GWY_LAUNCHER_PLATFORM_HANDLER_REGISTRY_H
#define GWY_LAUNCHER_PLATFORM_HANDLER_REGISTRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic plat handler registry (docs/06).
 * Stores guest VAs from live sendAppEvent registrations only — no fixed addresses.
 */

void platform_handler_registry_reset(void);

/* Returns 1 if stored/updated. handler must be a plausible guest code VA. */
int platform_handler_registry_register(uint32_t plat_code, uint32_t family, uint32_t handler);

uint32_t platform_handler_registry_get(uint32_t plat_code);
uint32_t platform_handler_registry_family(uint32_t plat_code);
int platform_handler_registry_has(uint32_t plat_code);

#ifdef __cplusplus
}
#endif

#endif
