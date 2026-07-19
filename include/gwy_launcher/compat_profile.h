#ifndef GWY_LAUNCHER_COMPAT_PROFILE_H
#define GWY_LAUNCHER_COMPAT_PROFILE_H

#include "gwy_launcher/error.h"
#include <stddef.h>
#include <stdint.h>

#define GWY_PROFILE_ID_MAX 64
#define GWY_PROFILE_ALIAS_MAX 32
#define GWY_PROFILE_NAME_MAX 128
#define GWY_PROFILE_SCHEMA_VERSION 1

typedef struct CompatibilityProfile {
    int schema_version;
    char id[GWY_PROFILE_ID_MAX];
    char target_mrp[256];
    char expected_sha256_hex[65];
    uint32_t appid;
    uint32_t appver;
    int has_appid;
    int has_appver;
    int has_sha256;
    char alias_from[GWY_PROFILE_ALIAS_MAX][GWY_PROFILE_NAME_MAX];
    char alias_to[GWY_PROFILE_ALIAS_MAX][GWY_PROFILE_NAME_MAX];
    size_t alias_count;
} CompatibilityProfile;

LauncherStatus compatibility_profile_load_json_file(const char *path,
                                                    CompatibilityProfile *out,
                                                    LauncherError *err);

LauncherStatus compatibility_profile_load_json_text(const char *json_text,
                                                    CompatibilityProfile *out,
                                                    LauncherError *err);

/* Returns 1 if profile match constraints succeed (when declared). */
int compatibility_profile_matches_archive(const CompatibilityProfile *profile,
                                          uint32_t appid,
                                          uint32_t appver,
                                          const char *sha256_hex);

/* Strong match including normalized target path when profile declares target_mrp. */
int compatibility_profile_matches_request(const CompatibilityProfile *profile,
                                          const char *package_guest_path,
                                          uint32_t appid,
                                          uint32_t appver,
                                          const char *sha256_hex);

/* Exact alias lookup; returns 1 and writes alias_to if found. */
int compatibility_profile_find_member_alias(const CompatibilityProfile *profile,
                                            const char *requested_member,
                                            char *out_to,
                                            size_t out_cap);

/* True if profile has any mrp member aliases (requires full identity when loading). */
int compatibility_profile_has_member_aliases(const CompatibilityProfile *profile);

#endif
