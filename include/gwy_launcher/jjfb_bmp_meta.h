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
/* Keep original MRP RGB565 bytes for blit when guest VA uc_mem_read fails. */
void jjfb_bmp_meta_set_pixels(uint32_t pixels_va, uint16_t w, uint16_t h, const char *member,
                              const void *rgb565, size_t nbytes);
int jjfb_bmp_meta_get(uint32_t pixels_va, uint16_t *w_out, uint16_t *h_out, char *member_out,
                      size_t member_cap);
/* Copy cached pixels; returns bytes copied, 0 if miss. */
size_t jjfb_bmp_meta_copy_pixels(uint32_t pixels_va, void *dst, size_t dst_cap);

/* E9H: blit original MRP pixels from guest VA via guiDrawBitmapSprite (main_gwy).
 * Returns 1 on blit, 0 if unavailable / invalid.
 * MinGW PE has no reliable ELF weak override — bridge registers the impl. */
typedef int (*JjfbE9hBlitFn)(void *uc, uint32_t pixels_va, int x, int y, int w, int h,
                             const char *member);
void jjfb_e9h_set_blit_fn(JjfbE9hBlitFn fn);
int jjfb_e9h_blit_guest_pixels(void *uc, uint32_t pixels_va, int x, int y, int w, int h,
                               const char *member);

/* E9K: post-r4 / text path may not blit again — robotol arms HWND hold via this hook.
 * bridge/main registers an impl that calls guiVisibleWindowFinalize(). */
typedef void (*JjfbE9kHoldFn)(const char *reason);
void jjfb_e9k_set_hold_fn(JjfbE9kHoldFn fn);
void jjfb_e9k_request_hold(const char *reason);
int jjfb_e9k_hold_requested(void);

/* E9N: platform 0x11F00 text draw compat — bridge registers guiDrawBitmapSprite impl. */
typedef int (*JjfbE9nTextDrawFn)(int x, int y, const uint8_t *bytes, int nbytes);
void jjfb_e9n_set_text_draw_fn(JjfbE9nTextDrawFn fn);
int jjfb_e9n_host_draw_gbk(int x, int y, const uint8_t *bytes, int nbytes);

#ifdef __cplusplus
}
#endif
#endif
