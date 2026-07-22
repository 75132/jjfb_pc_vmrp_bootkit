#ifndef GWY_LAUNCHER_PACKAGE_METADATA_H
#define GWY_LAUNCHER_PACKAGE_METADATA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Package-scoped MRP metadata + appInfo binding (E10A-3.1e).
 * Product source: active package MRP header (appid @+68, appver @+72).
 * Env GWY_PACKAGE_APPID/APPVER is diagnostic-only (disabled by default).
 */

typedef enum GwyPackageMetadataSource {
    GWY_PKG_META_NONE = 0,
    GWY_PKG_META_MRP_HEADER = 1,
    GWY_PKG_META_PACKAGE_REGISTRY = 2,
    GWY_PKG_META_DIAGNOSTIC_ENV_OVERRIDE = 3
} GwyPackageMetadataSource;

typedef struct GwyPackageMetadata {
    uint32_t appid;
    uint32_t appver;

    uint32_t flags;
    uint32_t header_version;

    char archive_path[260];
    char internal_name[128];
    char display_name[128];
    char guest_path[160];

    uint64_t archive_id;
    uint64_t package_id;
    uint64_t package_generation;

    int valid;
    int source; /* GwyPackageMetadataSource */
} GwyPackageMetadata;

typedef struct GwyPackageAppInfoBinding {
    uint64_t package_id;
    uint64_t package_generation;

    uint32_t appinfo_guest;
    uint32_t appid;
    uint32_t appver;
    uint32_t sidname_guest;
    uint32_t ram;

    int valid;
    int source; /* GwyPackageMetadataSource used to fill id/ver */
} GwyPackageAppInfoBinding;

void gwy_package_metadata_reset(void);

/* Parse MRP header for guest package path; bump generation; invalidate appInfo binding. */
int gwy_package_metadata_activate(const char *package_guest_path);

void gwy_package_metadata_invalidate_active(void);

const GwyPackageMetadata *gwy_package_registry_active_metadata(void);

/* Resolve host filesystem path for a guest MRP path (uses GWY_RESOURCE_ROOT). */
int gwy_package_metadata_resolve_host_path(const char *package_guest_path, char *out,
                                           size_t out_sz);

const char *gwy_package_metadata_source_name(int source);

/*
 * Resolve id/ver for code8 publish.
 * Normal: MRP_HEADER (or PACKAGE_REGISTRY cache of same).
 * JJFB_E10A31E_FORCE_ENV_APPINFO=1 or JJFB_E10A31E_DIAGNOSTIC_ENV_OVERRIDE=1:
 *   use GWY_PACKAGE_APPID/APPVER and print DIAGNOSTIC_OVERRIDE.
 */
int gwy_package_appinfo_resolve_id_ver(uint32_t *appid, uint32_t *appver, int *source_out);

/* Binding keyed by package_id + package_generation (not appid alone). */
const GwyPackageAppInfoBinding *gwy_package_appinfo_binding_active(void);
void gwy_package_appinfo_binding_invalidate(void);
void gwy_package_appinfo_binding_set(uint32_t appinfo_guest, uint32_t appid, uint32_t appver,
                                     int source);

/* 1 if binding matches active metadata generation and guest ptr still mapped. */
int gwy_package_appinfo_binding_matches_active(uint32_t guest_ptr,
                                               int (*guest_mapped)(uint32_t guest_ptr));

#ifdef __cplusplus
}
#endif

#endif
