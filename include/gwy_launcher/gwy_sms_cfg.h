#ifndef GWY_LAUNCHER_GWY_SMS_CFG_H
#define GWY_LAUNCHER_GWY_SMS_CFG_H

#include "gwy_launcher/sms_cfg_compat_profile.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DSM-level SMSCFG lifecycle (E10A-3.1k).
 *
 * Env:
 *   GWY_DIAG_SMSCFG_GPT_MINIMAL=1  — A/B: set offset 0x349 "GPT" after publish
 *   GWY_SMSCFG_BOOTSTRAP=1         — load overlay dsm.cfg or apply compat profile
 *   JJFB_E10A31K_MODE=1            — enables bootstrap path for stage runner
 *
 * Forbidden: fixed absolute buffer address, gamelist-name branch, method0 patch,
 * invented full dsm.cfg image, auto-persist of diagnostic-only writes.
 */

typedef enum GwySmsCfgSource {
    GWY_SMSCFG_SRC_NONE = 0,
    GWY_SMSCFG_SRC_ZEROED = 1,
    GWY_SMSCFG_SRC_FILE = 2,
    GWY_SMSCFG_SRC_COMPAT_PROFILE = 3,
    GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY = 4,
    GWY_SMSCFG_SRC_GUEST_SET = 5
} GwySmsCfgSource;

typedef struct GwySmsCfgState {
    uint32_t dsm_generation;
    uint32_t cfunction_generation;
    uint32_t cfg_guest;
    uint32_t cfg_len;
    uint32_t erw_base;
    uint32_t erw_size;
    uint32_t mr_table;
    int initialized;
    GwySmsCfgSource source;
    int dirty;
    char persistence_path[512];
    char profile_id[96];
    uint8_t cfunction_sha256[32];
    uint32_t mr_version;
} GwySmsCfgState;

void gwy_sms_cfg_reset(void);
const GwySmsCfgState *gwy_sms_cfg_state(void);

int gwy_sms_cfg_diag_minimal_enabled(void);
int gwy_sms_cfg_bootstrap_enabled(void);

/* Resolve live buffer via active DSM context (mr_table+0x1C0 or ERW+0x8D4). */
int gwy_sms_cfg_resolve(void *uc, uint32_t erw_base, uint32_t erw_size, uint32_t mr_table);

/* Called when cfunction ERW publishes. */
void gwy_sms_cfg_on_erw(void *uc, uint32_t helper, uint32_t erw_base, uint32_t erw_size);

/* Ensure bootstrap/diag applied before gamelist init sequence. */
int gwy_sms_cfg_ensure_ready(void *uc);

/* Generic get/set (pointer-relative). */
int32_t gwy_sms_cfg_get_bytes(void *uc, int32_t pos, void *dst, int32_t len);
int32_t gwy_sms_cfg_set_bytes(void *uc, int32_t pos, const void *src, int32_t len,
                              GwySmsCfgSource source_tag);

/* Mythroad-compatible load/save entry points (host bridge). */
int32_t gwy_sms_cfg_load(void *uc);
int32_t gwy_sms_cfg_save(void *uc);

#ifdef __cplusplus
}
#endif

#endif
