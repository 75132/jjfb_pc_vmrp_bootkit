#ifndef GWY_LAUNCHER_JJFB_PLAT_11F00_H
#define GWY_LAUNCHER_JJFB_PLAT_11F00_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform sendAppEvent 0x11F00 — drawText / glyphDraw (GWY/robotol).
 * Runtime compatibility (JJFB_PLATFORM_TEXT_API_11F00=1), not E9N diagnostic shim.
 * NOT product success while upstream assists remain.
 *
 * Observed ABI (E9N live):
 *   R0 = 0x11F00
 *   R1 = app (often 7)
 *   R2 = rich-text / code object
 *   R3 = param0 pack: i16 y@+0, i16 x@+2, RGB888@+0x2C, …
 * Guest string originates from 305BFC R0 (C-string) in the same draw chain;
 * also scanned from code-object pointer fields.
 */

void jjfb_plat_11f00_note_guest_cstr(uint32_t str_va, int16_t x, int16_t y, uint32_t color);
uint32_t jjfb_plat_11f00_last_guest_cstr(void);

/* Returns 1 if handled (pixels drawn or consciously skipped with success ret). */
int jjfb_plat_11f00_handle(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0,
                           uint32_t caller_pc, uint32_t caller_lr);

/* Host draw: GBK bytes → RGB565 sprite via registered bridge/GDI impl. */
typedef int (*JjfbPlat11f00DrawFn)(int x, int y, const uint8_t *bytes, int nbytes,
                                   uint16_t fg_rgb565, int clip_x, int clip_y, int clip_w,
                                   int clip_h, const char **font_name_out, int *font_fallback_out);
void jjfb_plat_11f00_set_draw_fn(JjfbPlat11f00DrawFn fn);

/*
 * Platform sendAppEvent 0x12340 — text measure (GWY/robotol).
 * Runtime compatibility (JJFB_PLATFORM_TEXT_MEASURE_12340=1).
 *
 * Observed ABI (E9M/E9P live):
 *   R0 = 0x12340
 *   R1 = app (often 1)
 *   R2 = rich-text / code object (string resolved like 0x11F00)
 *   R3 = 0 at call
 * After return @ 0x305EA0: R4=&width_out, R7=&height_out (u32 locals).
 * Writes pending measure into those outs (MR_SUCCESS ret=0).
 */
typedef int (*JjfbPlat12340MeasureFn)(const uint8_t *bytes, int nbytes, int *w_out, int *h_out,
                                      const char **font_name_out, int *font_fallback_out);
void jjfb_plat_12340_set_measure_fn(JjfbPlat12340MeasureFn fn);

int jjfb_plat_12340_handle(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0,
                           uint32_t caller_pc, uint32_t caller_lr, uint32_t sp);
/* Apply pending measure to guest width/height pointers (after 0x304558 returns). */
int jjfb_plat_12340_flush_outs(void *uc, uint32_t width_ptr, uint32_t height_ptr);
int jjfb_plat_12340_pending(uint32_t *w_out, uint32_t *h_out, uint32_t *str_va_out);

/* Last GDI draw bounding box (for measure-vs-draw metrics). */
void jjfb_plat_11f00_note_draw_bbox(int w, int h);
void jjfb_plat_11f00_last_draw_bbox(int *w_out, int *h_out);

#ifdef __cplusplus
}
#endif

#endif
