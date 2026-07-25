#ifndef GWY_LAUNCHER_PLATFORM_DISPLAY_H
#define GWY_LAUNCHER_PLATFORM_DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product display / _DrawBitmap contract (Task 14).
 *
 * mr_table+0x1E0 = _DrawBitmap trampoline (Guest-callable host CB).
 * Robotol caches that FP at ER_RW+0x150C (wrapper 0x2EC6B8 → BLX).
 *
 * Default ON. A/B:
 *   JJFB_DRAWFP_BINDING=0  — skip ER_RW+0x150C publish (baseline hang)
 *   JJFB_DRAWFP_CONTRACT_TRACE=1 — dense DRAW_FP_* logs
 */

#define PLATFORM_DRAWBITMAP_MR_TABLE_OFF 0x1E0u
#define PLATFORM_ROBOTOL_ERW_DRAWFP_OFF 0x150Cu
#define PLATFORM_DISPLAY_WIDTH 240
#define PLATFORM_DISPLAY_HEIGHT 320

typedef struct GwyDisplaySurface {
    uint32_t width;
    uint32_t height;
    uint16_t *rgb565;
    int dirty;
    int dirty_x0;
    int dirty_y0;
    int dirty_x1;
    int dirty_y1;
    void *window;
    void *renderer;
    void *texture;
} GwyDisplaySurface;

typedef struct GwyDrawBitmapArgs {
    uint32_t guest_pixels;
    int32_t dst_x;
    int32_t dst_y;
    int32_t copy_w;
    int32_t copy_h;
    int32_t src_x;
    int32_t src_y;
    int32_t src_pitch;
    uint32_t rop;
    uint16_t transparent_key;
    int transparent_enabled;
    int wrapper_abi;
} GwyDrawBitmapArgs;

/* Optional host present: blit RGB565 sprite into the existing SDL game window. */
typedef void (*GwyDisplayPresentFn)(const uint16_t *rgb565, int32_t x, int32_t y, int32_t w,
                                    int32_t h, uint16_t key, int key_en);

void platform_display_reset(void);
void platform_display_bind_surface(GwyDisplaySurface *surface);
void platform_display_set_present_fn(GwyDisplayPresentFn fn);

/* Launcher/main bind helper — does not create a third window. */
void gwy_runtime_bind_display_surface(uint32_t width, uint32_t height, uint16_t *rgb565,
                                      void *window, void *renderer, void *texture);

int platform_display_binding_enabled(void);
int platform_display_trace_enabled(void);

int platform_display_decode_drawbitmap_args(void *uc, GwyDrawBitmapArgs *out);
int platform_display_draw_bitmap(void *uc, const GwyDrawBitmapArgs *args);
int platform_display_disp_up_ex(int32_t x, int32_t y, int32_t w, int32_t h);

/*
 * Guest-callable _DrawBitmap body (Robotol wrapper + classic ABI).
 * Always returns via normal host-callback frame; R0=1 (V67-proven continue).
 * Never UC_FAULT / never jump into cfunction on bad pixels.
 */
uint32_t platform_guest_draw_bitmap(void *uc);

/* Publish mr_table+0x1E0 into Robotol ER_RW+0x150C once per ER_RW (no PC heal). */
int platform_drawfp_cache_publish(void *uc, uint32_t er_rw, uint32_t mr_table);

uint32_t platform_drawfp_trampoline_guest(void *uc);
uint32_t platform_drawfp_call_count(void);
uint32_t platform_drawfp_reject_count(void);

#ifdef __cplusplus
}
#endif

#endif
