#ifndef GWY_LAUNCHER_JJFB_BMP_META_H
#define GWY_LAUNCHER_JJFB_BMP_META_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Per-bmp pixel VA → (w,h,member) table shared by launcher_core + bridge.o
 * in the same main_gwy process. Prefer over global JJFB_E9E_LAST_WH.
 * NOT product success — observe/assist only.
 */

void jjfb_bmp_meta_reset(void);
void jjfb_bmp_meta_set(uint32_t pixels_va, uint16_t w, uint16_t h, const char *member);
int jjfb_bmp_meta_get(uint32_t pixels_va, uint16_t *w_out, uint16_t *h_out, char *member_out,
                      size_t member_cap);

/* E9H: blit original MRP pixels from guest VA via guiDrawBitmapSprite (main_gwy).
 * Returns 1 on blit, 0 if unavailable / invalid.
 * MinGW PE has no reliable ELF weak override — bridge registers the impl. */
typedef int (*JjfbE9hBlitFn)(void *uc, uint32_t pixels_va, int x, int y, int w, int h,
                             const char *member);
void jjfb_e9h_set_blit_fn(JjfbE9hBlitFn fn);
int jjfb_e9h_blit_guest_pixels(void *uc, uint32_t pixels_va, int x, int y, int w, int h,
                               const char *member);

#ifdef __cplusplus
}
#endif

#endif
