#ifndef GWY_LAUNCHER_LAUNCH_DESCRIPTOR_H
#define GWY_LAUNCHER_LAUNCH_DESCRIPTOR_H

#include "gwy_launcher/error.h"
#include <stddef.h>
#include <stdint.h>

#define GL_PATH_MAX 1024
#define GL_PARAM_MAX 512

typedef struct LaunchDescriptor {
    char profile_id[64];
    char resource_root[GL_PATH_MAX];
    char target_mrp[256];
    char entry_member[64];
    char nmrpname[256];
    char param[GL_PARAM_MAX];
    uint32_t cfg_index;
    int32_t napptype;
    int32_t nextid;
    int32_t ncode;
    int32_t narg;
    int32_t narg1;
    uint32_t appid;
    uint32_t appver;
    uint8_t target_sha256[32];
    char target_sha256_hex[65];
} LaunchDescriptor;

typedef struct LaunchExpectations {
    int has_target;
    char target_mrp[256];
    int has_sha256;
    char sha256_hex[65];
    int has_appid;
    uint32_t appid;
    int has_appver;
    uint32_t appver;
    int has_entry;
    char entry_member[64];
} LaunchExpectations;

int launch_descriptor_validate_basic(const LaunchDescriptor *desc);

LauncherStatus launch_param_serialize(const LaunchDescriptor *desc,
                                      char *out,
                                      size_t out_cap,
                                      LauncherError *err);

/* Build from resource root + cfg index. Optional expectations fail closed on mismatch. */
LauncherStatus launch_descriptor_build(const char *resource_root,
                                       uint32_t cfg_index,
                                       const char *profile_id,
                                       const LaunchExpectations *expect,
                                       LaunchDescriptor *out,
                                       LauncherError *err);

LauncherStatus launch_manifest_write(const char *path,
                                     const LaunchDescriptor *desc,
                                     LauncherError *err);

/* Reject dangerous profile keys without claiming full JSON schema support. */
LauncherStatus launch_profile_reject_dangerous(const char *json_text, LauncherError *err);

#endif
