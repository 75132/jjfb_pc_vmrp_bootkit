#ifndef GWY_LAUNCHER_LAUNCH_DESCRIPTOR_H
#define GWY_LAUNCHER_LAUNCH_DESCRIPTOR_H

#include <stdint.h>

#define GL_PATH_MAX 1024

typedef struct LaunchDescriptor {
    char profile_id[64];
    char resource_root[GL_PATH_MAX];
    char target_mrp[256];
    char entry_member[64];
    uint32_t cfg_index;
    int32_t napptype;
    int32_t nextid;
    int32_t ncode;
    int32_t narg;
    int32_t narg1;
    uint32_t appid;
    uint32_t appver;
    uint8_t target_sha256[32];
} LaunchDescriptor;

int launch_descriptor_validate_basic(const LaunchDescriptor *desc);

#endif
