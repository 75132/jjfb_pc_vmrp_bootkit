#ifndef GWY_LAUNCHER_PLATFORM_USERINFO_H
#define GWY_LAUNCHER_PLATFORM_USERINFO_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/platform_identity.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DOCUMENTED (mrporting.h mr_userinfo):
 *   +0x00 IMEI[16]
 *   +0x10 IMSI[16]
 *   +0x20 manufactory[8]
 *   +0x28 type[8]
 *   +0x30 ver (uint32)
 *   +0x34 spare[12]
 * Total 64 bytes.
 *
 * sendAppEvent(0x10180) returns a guest pointer to a host-owned blob
 * (TARGET_OBSERVED). Callers may read beyond sizeof(mr_userinfo); allocate
 * GWY_USERINFO_BLOB_BYTES and zero-pad unconfirmed tail fields.
 */
#define GWY_MR_USERINFO_BYTES 64u
#define GWY_USERINFO_BLOB_BYTES 0x100u

/* CROSS_TARGET default IMSI from mythroad dsm.c mr_getUserInfo. */
#define GWY_DEFAULT_IMSI "460019707327302"

/* Base SW ver used by dsm.c (101000000 + plat/fae additives). */
#define GWY_DEFAULT_USERINFO_VER 101000000u

typedef struct PlatformUserInfoBlob {
    uint8_t bytes[GWY_USERINFO_BLOB_BYTES];
    size_t filled; /* DOCUMENTED mr_userinfo span (64) */
    uint32_t ver;
} PlatformUserInfoBlob;

LauncherStatus platform_userinfo_fill(const PlatformIdentity *id,
                                      const char *imsi,
                                      PlatformUserInfoBlob *out,
                                      LauncherError *err);

#ifdef __cplusplus
}
#endif

#endif
