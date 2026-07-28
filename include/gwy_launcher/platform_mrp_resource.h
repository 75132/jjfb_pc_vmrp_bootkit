#ifndef GWY_LAUNCHER_PLATFORM_MRP_RESOURCE_H
#define GWY_LAUNCHER_PLATFORM_MRP_RESOURCE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product MRP member resource service (Task 12 Phase C + P3 Visual Completeness).
 *
 * Env:
 *   JJFB_PLATFORM_MRP_RESOURCE=0  — disable (A/B baseline)
 *   JJFB_PLATFORM_MRP_RESOURCE=1  — enable (default when unset)
 *   JJFB_REAL_MRP_PATH=<path>     — override package host path
 */

#define PLATFORM_MRP_LOOKUP_ENTRY_PC 0x304BF0u /* frame capture only, not behavior trigger */
#define PLATFORM_MRP_PIXEL_BASE 0x3920000u
#define PLATFORM_MRP_PIXEL_MAP_SIZE 0x40000u
#define PLATFORM_MRP_HANDLE_BASE 0x3910000u
#define PLATFORM_MRP_HANDLE_MAP_SIZE 0x1000u

typedef enum {
    GWY_MRP_RESOURCE_BITMAP_RGB565 = 0,
    GWY_MRP_RESOURCE_RAW_BLOB = 1,
    GWY_MRP_RESOURCE_MODULE = 2,
    GWY_MRP_RESOURCE_UNKNOWN = 3
} GwyMrpResourceKind;

typedef struct GwyMrpResourceRequest {
    uint32_t guest_name_ptr;
    char name[256];
    uint32_t caller_pc;
    uint32_t caller_lr;
} GwyMrpResourceRequest;

typedef struct GwyMrpResourceResult {
    int status; /* 0=ok, -1=fail */
    uint32_t guest_data;
    uint32_t decoded_size;
    uint32_t stored_size;
    uint32_t member_offset;
    uint32_t handle_guest;
    uint16_t width;
    uint16_t height;
    GwyMrpResourceKind kind;
    char sha256_hex[65];
} GwyMrpResourceResult;

GwyMrpResourceKind platform_mrp_resource_classify(const char *member_name);

int platform_mrp_resource_enabled(void);
void platform_mrp_resource_reset(void);
void platform_mrp_resource_bind_uc(void *uc);
void platform_mrp_resource_arm(void *uc);

/* Host-only decode (unit tests / diagnostics). No guest mutation. */
int platform_mrp_resource_load_host(const char *package_host_path, const char *member_name,
                                    GwyMrpResourceResult *out, uint8_t **decoded_out,
                                    size_t *decoded_len_out);

/* Live guest path: exact lookup + poke decoded bytes; fills result. */
int platform_mrp_resource_load(void *uc, const GwyMrpResourceRequest *request,
                               GwyMrpResourceResult *result);

uint32_t platform_mrp_resource_postmatch_count(void);

/*
 * Pending bitmap construct FIFO.
 * Pending owns host_pixels until commit/release/reset.
 * classify: reserve oldest size-matching entry
 * executor: copy_pixels then commit, or release on failure
 * Never write handle.pixels from host (guest owns store).
 */
uint64_t platform_mrp_resource_pending_reserve(uint32_t bytes, uint32_t *out_guest_pixels);
int platform_mrp_resource_pending_commit(uint64_t pending_id);
int platform_mrp_resource_pending_release(uint64_t pending_id);
uint32_t platform_mrp_resource_pending_depth(void);
int platform_mrp_resource_pending_copy_pixels(uint64_t pending_id, void *dst_host,
                                             uint32_t dst_bytes);

/* Enqueue after MRP decode; copies rgb565 into pending-owned host_pixels. */
void platform_mrp_resource_pending_enqueue(const char *package_name, const char *member_name,
                                          const uint8_t *rgb565, uint32_t decoded_bytes,
                                          uint16_t w, uint16_t h, uint32_t guest_handle,
                                          uint32_t lookup_lr);

/*
 * Compatibility: non-zero if a READY/RESERVED pending matches size.
 * Prefer pending_reserve for 0x10134 classify.
 */
uint32_t platform_mrp_resource_pixels_by_bytes(uint32_t bytes);

void platform_mrp_resource_note_pixels(uint32_t bytes, uint32_t guest_pixels, uint16_t w,
                                      uint16_t h);
void platform_mrp_resource_note_pixels_ex(uint32_t bytes, uint32_t guest_pixels,
                                         uint32_t handle_guest, uint16_t w, uint16_t h);

/*
 * FORBIDDEN on product path (Task 16): early handle.pixels bind causes mr_free invalid.
 */
int platform_mrp_resource_bind_10134_pixels(void *uc, uint32_t bytes, uint32_t user_pixels);

/* Atlas alias for ANI-referenced names only (P3-5). Returns 1 if aliased. */
int platform_mrp_resource_apply_ani_atlas_alias(const char *ani_package, const char *requested,
                                               char *out_name, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif
