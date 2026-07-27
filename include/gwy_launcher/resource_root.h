#ifndef GWY_LAUNCHER_RESOURCE_ROOT_H
#define GWY_LAUNCHER_RESOURCE_ROOT_H

#include "gwy_launcher/error.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_RESOURCE_ROOT_PATH_MAX 1024
#define GWY_RESOURCE_ROOT_REASON_MAX 128

typedef struct GwyResourceRootRequest {
    /* Absolute or relative path from --root / -ResourceRoot. Empty = unset. */
    const char *explicit_root;
    /* Optional: getenv("GWY_RESOURCE_ROOT") already read by caller. Empty = unset. */
    const char *env_root;
    /* Repo root containing game_files/mythroad (required for default/scan). */
    const char *repo_root;
    /* Optional expected jjfb.mrp SHA-256 hex (64 chars). NULL/empty = skip hash check. */
    const char *expected_sha256_hex;
    /* cfg.bin index that must name gwy/jjfb.mrp (JJFB product = 36). */
    uint32_t cfg_index;
} GwyResourceRootRequest;

typedef struct GwyResourceRootResult {
    char path[GWY_RESOURCE_ROOT_PATH_MAX];
    char reason[GWY_RESOURCE_ROOT_REASON_MAX]; /* explicit|env|default_240x320|scan */
    char jjfb_mrp[GWY_RESOURCE_ROOT_PATH_MAX];
    char jjfb_sha256_hex[65];
} GwyResourceRootResult;

/*
 * Validate a candidate mythroad root:
 *   gwy/jjfb.mrp, gwy/jjfbol/, gwy/jjfbol/downVersion,
 *   cfg.bin[cfg_index] -> gwy/jjfb.mrp, optional SHA match.
 */
LauncherStatus gwy_resource_root_validate(const char *candidate_root,
                                          uint32_t cfg_index,
                                          const char *expected_sha256_hex,
                                          GwyResourceRootResult *out,
                                          LauncherError *err);

/*
 * Resolve with strict fail for explicit/env; default 240x320 then scan siblings.
 * Priority: explicit -> env -> game_files/mythroad/240x320 -> scan mythroad star dirs.
 */
LauncherStatus gwy_resource_root_resolve(const GwyResourceRootRequest *req,
                                         GwyResourceRootResult *out,
                                         LauncherError *err);

#ifdef __cplusplus
}
#endif

#endif
