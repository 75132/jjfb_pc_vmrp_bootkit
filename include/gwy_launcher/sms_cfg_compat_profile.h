#ifndef GWY_LAUNCHER_SMS_CFG_COMPAT_PROFILE_H
#define GWY_LAUNCHER_SMS_CFG_COMPAT_PROFILE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mythroad / cfunction platform compatibility database (not a jjfb/gamelist patch).
 * Profiles are selected by cfunction SHA256 + MR_VERSION + cfg buffer length.
 */

#define GWY_SMS_CFG_LEN 0x10E0u
#define GWY_SMS_CFG_GPT_OFF 0x349u
#define GWY_SMS_CFG_MR_TABLE_OFF 0x1C0u
#define GWY_SMS_CFG_ERW_OFF 0x8D4u
#define GWY_SMS_CFG_MR_VERSION 2011u
#define GWY_SMS_CFG_TAG_DATA_MAX 16u
#define GWY_SMS_CFG_REQUIRED_TAGS_MAX 8u

typedef struct SmsCfgCompatTag {
    uint32_t offset;
    uint32_t length;
    uint8_t data[GWY_SMS_CFG_TAG_DATA_MAX];
} SmsCfgCompatTag;

typedef struct SmsCfgCompatProfile {
    const char *profile_id;
    uint8_t cfunction_sha256[32]; /* all-zero = match any hash */
    uint32_t mr_version;
    uint32_t cfg_length;
    SmsCfgCompatTag required_tags[GWY_SMS_CFG_REQUIRED_TAGS_MAX];
    uint32_t required_tag_count;
} SmsCfgCompatProfile;

const SmsCfgCompatProfile *sms_cfg_compat_profiles(uint32_t *out_count);
const SmsCfgCompatProfile *sms_cfg_compat_select(const uint8_t cfunction_sha256[32],
                                                 uint32_t mr_version, uint32_t cfg_length);

#ifdef __cplusplus
}
#endif

#endif
