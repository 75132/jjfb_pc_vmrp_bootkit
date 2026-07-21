#ifndef GWY_LAUNCHER_GWY_PACK_REGISTRY_H
#define GWY_LAUNCHER_GWY_PACK_REGISTRY_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/mrp_archive.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_PACK_MAX 64
#define GWY_PACK_PATH_MAX 1024
#define GWY_PACK_NAME_MAX 96

typedef struct GwyPackEntry {
    char pack_name[GWY_PACK_NAME_MAX]; /* stem without .mrp */
    char pack_path[GWY_PACK_PATH_MAX];
    char sha256_hex[65];
    uint32_t appid;
    uint32_t appver;
    size_t member_count;
    int is_downimage; /* pack_name contains "downimage" (casefold) */
} GwyPackEntry;

typedef struct GwyPackRegistry {
    char target_mrp[GWY_PACK_PATH_MAX];
    char gwy_root[GWY_PACK_PATH_MAX];
    char side_pack_dir[GWY_PACK_PATH_MAX];
    GwyPackEntry packs[GWY_PACK_MAX];
    size_t pack_count;
    int ready;
} GwyPackRegistry;

void gwy_pack_registry_reset(void);
GwyPackRegistry *gwy_pack_registry_get(void);

/*
 * Scan target MRP sibling side-pack dir: gwy/<stem>ol/ (fallback: gwy/*ol/).
 * Indexes all *.mrp in that dir — no hardcoded show/downimage/target names.
 */
LauncherStatus gwy_pack_registry_scan(const char *target_mrp_or_null, LauncherError *err);

int gwy_pack_registry_ready(void);
size_t gwy_pack_registry_pack_count(void);

/* Resolve pack stem → host path. Returns 1 if found. */
int gwy_pack_registry_find_pack_path(const char *stem, char *out_path, size_t out_sz);

/* Exact member across registered packs. */
int gwy_pack_registry_find_member(const char *member_name, char *out_pack_path, size_t pack_sz,
                                  char *out_member, size_t mem_sz);

/* Export CSV (default reports/e9z_gwy_pack_registry.csv). */
int gwy_pack_registry_export_csv(const char *csv_path_or_null);

#ifdef __cplusplus
}
#endif

#endif
