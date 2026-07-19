#ifndef GWY_LAUNCHER_PLATFORM_IDENTITY_H
#define GWY_LAUNCHER_PLATFORM_IDENTITY_H

#include "gwy_launcher/error.h"
#include <stddef.h>
#include <stdint.h>

#define GWY_SDK_KEY_SIZE 48
#define GWY_IMEI_DIGITS 15

typedef struct PlatformIdentity {
    char vmver[32];
    char imei[GWY_IMEI_DIGITS + 1];
    char manufacturer[64];
    char model[64];
} PlatformIdentity;

typedef struct SdkKey {
    uint8_t bytes[GWY_SDK_KEY_SIZE];
    char sha256_hex[65];
} SdkKey;

void platform_identity_set_defaults(PlatformIdentity *id);

LauncherStatus platform_identity_validate(const PlatformIdentity *id, LauncherError *err);

/*
 * Mythroad sdk_key.dat (TARGET_OBSERVED / CROSS_TARGET via recovered generator):
 *   MD5(custom_b64(part1)) || MD5(custom_b64(part2)) || MD5(custom_b64(part3))
 * Defaults match profile identity and legacy v51 vector.
 */
LauncherStatus platform_sdk_key_generate(const PlatformIdentity *id,
                                         SdkKey *out,
                                         LauncherError *err);

LauncherStatus platform_sdk_key_write_file(const char *path,
                                           const SdkKey *key,
                                           LauncherError *err);

#endif
