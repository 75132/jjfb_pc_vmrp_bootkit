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

#ifdef __cplusplus
}
#endif

#endif
