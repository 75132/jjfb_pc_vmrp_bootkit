#ifndef GWY_LAUNCHER_EXT_LOADER_H
#define GWY_LAUNCHER_EXT_LOADER_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/module_registry.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_EXT_LOAD_SESSION_MAX 16

typedef struct {
    uint64_t load_id;
    uint64_t module_id;
    ModuleOrigin origin;
    char package_path[260];
    char requested_name[128];
    char resolved_name[128];
} ExtLoadContext;

typedef struct ExtLoadSession ExtLoadSession;

typedef struct ExtLoader ExtLoader;

void ext_loader_init(ExtLoader *loader);
void ext_loader_destroy_all(ExtLoader *loader);

void ext_loader_bind_registry(ExtLoader *loader, ModuleRegistry *reg);

/* Seed one session per EXTRACTED module (idempotent). */
LauncherStatus ext_loader_seed_from_registry(ExtLoader *loader, LauncherError *err);

/*
 * Observe-only API: updates ModuleRegistry from upstream facts.
 * Does not decompress, relocate, or allocate guest code.
 */
LauncherStatus ext_loader_begin(ExtLoader *loader,
                                uint64_t module_id,
                                const uint8_t *image,
                                size_t image_size,
                                ExtLoadSession **out_session,
                                LauncherError *err);

LauncherStatus ext_loader_map(ExtLoadSession *session,
                              const GwyModuleMapping *mapping,
                              LauncherError *err);

LauncherStatus ext_loader_register(ExtLoadSession *session,
                                   uint32_t helper_address,
                                   uint32_t entry_address,
                                   LauncherError *err);

LauncherStatus ext_loader_call_entry(ExtLoadSession *session,
                                     uint32_t method,
                                     uint32_t input,
                                     uint32_t input_len,
                                     int32_t ret_value,
                                     LauncherError *err);

void ext_loader_destroy(ExtLoadSession *session);

const ExtLoadContext *ext_loader_session_context(const ExtLoadSession *session);
ExtLoadSession *ext_loader_find_session_by_module(ExtLoader *loader, uint64_t module_id);
ExtLoadSession *ext_loader_find_by_code_addr(ExtLoader *loader, uint32_t guest_addr);

/* --- Upstream observation hooks (Gwy runtime) --- */

void ext_loader_on_member_open(ExtLoader *loader, const char *guest_path);
void ext_loader_on_alloc(ExtLoader *loader, uint32_t guest_addr, uint32_t size);
/* Observe EXT image after guest cacheSync (size often = align(extracted+pad)). */
void ext_loader_on_code_image(ExtLoader *loader, uint32_t guest_addr, uint32_t size);
/*
 * DOCUMENTED (mythroad.c case 800): real EXT image is input1, while mr_cacheSync is called
 * with input1&~0x1F. Refine a previously mapped MRP module when --- ext: @raw arrives.
 */
void ext_loader_on_ext_image_raw(ExtLoader *loader, uint32_t raw_base);
void ext_loader_on_c_function_new(ExtLoader *loader,
                                  uint32_t helper,
                                  uint32_t p_len,
                                  uint32_t p_guest_addr,
                                  uint32_t rw_base,
                                  uint32_t rw_size,
                                  uint32_t stack_base);
void ext_loader_on_helper_call(ExtLoader *loader,
                               uint32_t helper,
                               uint32_t method,
                               int32_t ret_value);

/* Flush DSM _mr_c_function_new stashed before registry bind. */
void ext_loader_flush_early(ExtLoader *loader);

/* Process-local singleton (always available; bind registry after VFS prepare). */
ExtLoader *gwy_ext_loader_ensure(void);
ExtLoader *gwy_ext_loader_global(void);
void gwy_ext_loader_set_global(ExtLoader *loader);

/* Bound ModuleRegistry for observers (NULL if unbound). */
ModuleRegistry *gwy_ext_loader_bound_registry(void);

#ifdef __cplusplus
}
#endif

#endif
