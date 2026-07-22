#include "gwy_launcher/sms_cfg_compat_profile.h"

#include <string.h>

/*
 * Platform build validated by E10A-3.1h/j:
 *   cfunction.ext SHA256 =
 *     8F85E3CF8F0ED4A8E09EB658F9DABA566989FBC06C510E4E76CD474DD275CAD5
 *   MR_VERSION=2011, cfg_len=0x10E0
 *   method0 requires offset 0x349 bytes "GPT"
 *
 * original_default_recovered=false — compatibility reconstruction only.
 */

static const SmsCfgCompatProfile g_profiles[] = {
    {
        .profile_id = "mythroad_mini_2011_smscfg_0x10E0_gpt349",
        .cfunction_sha256 =
            {
                0x8F, 0x85, 0xE3, 0xCF, 0x8F, 0x0E, 0xD4, 0xA8, 0xE0, 0x9E, 0xB6, 0x58,
                0xF9, 0xDA, 0xBA, 0x56, 0x69, 0x89, 0xFB, 0xC0, 0x6C, 0x51, 0x0E, 0x4E,
                0x76, 0xCD, 0x47, 0x4D, 0xD2, 0x75, 0xCA, 0xD5,
            },
        .mr_version = GWY_SMS_CFG_MR_VERSION,
        .cfg_length = GWY_SMS_CFG_LEN,
        .required_tags =
            {
                {
                    .offset = GWY_SMS_CFG_GPT_OFF,
                    .length = 3u,
                    .data = {'G', 'P', 'T'},
                },
            },
        .required_tag_count = 1u,
    },
    {
        /* Wildcard fallback: same platform ABI when hash file is unavailable. */
        .profile_id = "mythroad_mini_2011_smscfg_0x10E0_gpt349_anyhash",
        .cfunction_sha256 = {0},
        .mr_version = GWY_SMS_CFG_MR_VERSION,
        .cfg_length = GWY_SMS_CFG_LEN,
        .required_tags =
            {
                {
                    .offset = GWY_SMS_CFG_GPT_OFF,
                    .length = 3u,
                    .data = {'G', 'P', 'T'},
                },
            },
        .required_tag_count = 1u,
    },
};

const SmsCfgCompatProfile *sms_cfg_compat_profiles(uint32_t *out_count) {
    if (out_count) *out_count = (uint32_t)(sizeof(g_profiles) / sizeof(g_profiles[0]));
    return g_profiles;
}

static int sha_is_zero(const uint8_t sha[32]) {
    uint32_t i;
    if (!sha) return 1;
    for (i = 0; i < 32u; i++) {
        if (sha[i]) return 0;
    }
    return 1;
}

const SmsCfgCompatProfile *sms_cfg_compat_select(const uint8_t cfunction_sha256[32],
                                                 uint32_t mr_version, uint32_t cfg_length) {
    uint32_t n = 0, i;
    const SmsCfgCompatProfile *p = sms_cfg_compat_profiles(&n);
    const SmsCfgCompatProfile *wildcard = NULL;
    for (i = 0; i < n; i++) {
        if (p[i].mr_version != mr_version || p[i].cfg_length != cfg_length) continue;
        if (sha_is_zero(p[i].cfunction_sha256)) {
            if (!wildcard) wildcard = &p[i];
            continue;
        }
        if (cfunction_sha256 && memcmp(p[i].cfunction_sha256, cfunction_sha256, 32) == 0)
            return &p[i];
    }
    return wildcard;
}
