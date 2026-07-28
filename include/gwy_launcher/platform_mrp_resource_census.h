#ifndef GWY_LAUNCHER_PLATFORM_MRP_RESOURCE_CENSUS_H
#define GWY_LAUNCHER_PLATFORM_MRP_RESOURCE_CENSUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P3-0 observe-only resource request census at 0x304BF0.
 * Does not change completion logic.
 */

typedef enum {
    GWY_MRP_CENSUS_UNKNOWN = 0,
    GWY_MRP_CENSUS_BITMAP_CANDIDATE,
    GWY_MRP_CENSUS_ANI_CANDIDATE,
    GWY_MRP_CENSUS_TEXT_CANDIDATE,
    GWY_MRP_CENSUS_MODULE_CANDIDATE,
    GWY_MRP_CENSUS_EXTENSIONLESS_CANDIDATE
} GwyMrpCensusKind;

void platform_mrp_resource_census_reset(void);
void platform_mrp_resource_census_arm(void);

/* Record one 304BF0 name observation (before looks_like_member_name filter). */
void platform_mrp_resource_census_note(const char *member_name, uint32_t caller_pc,
                                      uint32_t caller_lr, int passes_legacy_filter,
                                      const char *host_action_override);

void platform_mrp_resource_census_note_complete(const char *member_name);
void platform_mrp_resource_census_flush(const char *reason);

const char *platform_mrp_resource_census_kind_name(GwyMrpCensusKind kind);
GwyMrpCensusKind platform_mrp_resource_census_classify(const char *member_name);

#ifdef __cplusplus
}
#endif

#endif
