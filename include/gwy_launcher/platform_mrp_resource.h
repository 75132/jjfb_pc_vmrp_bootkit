#ifndef GWY_LAUNCHER_PLATFORM_MRP_RESOURCE_H
#define GWY_LAUNCHER_PLATFORM_MRP_RESOURCE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product MRP member resource service (Task 12 Phase C).
 *
 * Replaces the E9E host postmatch shim with a formal platform module:
 *   Guest natural index scan + true strcmp (0xAC2D0)
 *   → on strcmp==0 and exact MrpArchive hit
 *   → host decode original member bytes into guest buffer
 *   → restore 0x304BF0 entry ABI and return r0=0
 *
 * Never force-equal strcmp. Never hardcode member names / entry #20.
 *
 * Env:
 *   JJFB_PLATFORM_MRP_RESOURCE=0  — disable (A/B baseline)
 *   JJFB_PLATFORM_MRP_RESOURCE=1  — enable (default when unset)
 *   JJFB_REAL_MRP_PATH=<path>     — override package host path
 *
 * Regression sample (Task 12 §14):
 *   wy_jiao1!11!11.bmp decoded sha256 =
 *   edfe428dfb2daa8deea599915b7c5d4db75b6bfbfe78671cecd33e4ca4662a13
 */

#define PLATFORM_MRP_LOOKUP_ENTRY_PC 0x304BF0u /* frame capture only, not behavior trigger */
#define PLATFORM_MRP_PIXEL_BASE 0x3920000u
#define PLATFORM_MRP_PIXEL_MAP_SIZE 0x40000u
#define PLATFORM_MRP_HANDLE_BASE 0x3910000u
#define PLATFORM_MRP_HANDLE_MAP_SIZE 0x1000u

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
    char sha256_hex[65];
} GwyMrpResourceResult;

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
 * Pending bitmap construct FIFO (Phase 4).
 * classify: reserve oldest size-matching entry (does not delete)
 * executor: commit after successful alloc+copy, or release on failure
 * Never write handle.pixels from host (guest owns store).
 */
uint64_t platform_mrp_resource_pending_reserve(uint32_t bytes, uint32_t *out_guest_pixels);
int platform_mrp_resource_pending_commit(uint64_t pending_id);
int platform_mrp_resource_pending_release(uint64_t pending_id);
uint32_t platform_mrp_resource_pending_depth(void);

/* Enqueue after MRP decode (also used by tests via note_pixels). */
void platform_mrp_resource_pending_enqueue(const char *package_name, const char *member_name,
                                          uint32_t decoded_pixels, uint32_t decoded_bytes,
                                          uint16_t w, uint16_t h, uint32_t guest_handle,
                                          uint32_t lookup_lr);

/*
 * Compatibility: peek source pixels by size without reserving (memcpy null-src guard).
 * Prefer pending_reserve for 0x10134 classify.
 */
uint32_t platform_mrp_resource_pixels_by_bytes(uint32_t bytes);

void platform_mrp_resource_note_pixels(uint32_t bytes, uint32_t guest_pixels, uint16_t w,
                                      uint16_t h);
void platform_mrp_resource_note_pixels_ex(uint32_t bytes, uint32_t guest_pixels,
                                         uint32_t handle_guest, uint16_t w, uint16_t h);

/*
 * FORBIDDEN on product path (Task 16): early handle.pixels bind causes mr_free invalid.
 * Always returns 0 and logs; kept only so accidental callers fail closed.
 */
int platform_mrp_resource_bind_10134_pixels(void *uc, uint32_t bytes, uint32_t user_pixels);

#ifdef __cplusplus
}
#endif

#endif
