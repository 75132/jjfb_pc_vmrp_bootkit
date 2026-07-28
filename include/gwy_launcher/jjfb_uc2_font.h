#ifndef GWY_LAUNCHER_JJFB_UC2_FONT_H
#define GWY_LAUNCHER_JJFB_UC2_FONT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JJFB_UC2_GLYPH_W 12
#define JJFB_UC2_GLYPH_H 12
#define JJFB_UC2_GLYPH_BYTES 18

typedef struct JjfbUc2Font {
    uint8_t *data;
    size_t size;
    uint32_t glyph_bytes;
    uint32_t glyph_w;
    uint32_t glyph_h;
    char path[512];
    int loaded;
} JjfbUc2Font;

int jjfb_uc2_font_load(JjfbUc2Font *font, const char *host_path);
void jjfb_uc2_font_reset(JjfbUc2Font *font);
int jjfb_uc2_font_ready(const JjfbUc2Font *font);

/* Load from GWY_RESOURCE_ROOT/system/gb12.uc2 (or explicit path). */
int jjfb_uc2_font_load_product_default(JjfbUc2Font *font);

/* GBK/ASCII → glyph id used as index into UC2. */
uint16_t jjfb_uc2_gbk_to_id(const uint8_t *bytes, int nbytes, int *consumed);

int jjfb_uc2_font_glyph(const JjfbUc2Font *font, uint16_t id, uint8_t *out18);

/* Measure GBK string using same metrics as draw (no fallback estimate). */
int jjfb_uc2_font_measure_gbk(const JjfbUc2Font *font, const uint8_t *bytes, int nbytes, int *out_w,
                              int *out_h);

/*
 * Draw GBK into RGB565 framebuffer. Returns 1 if any glyph plotted.
 * font_fallback stays 0 on success path (product forbids block glyphs).
 */
int jjfb_uc2_font_draw_gbk(const JjfbUc2Font *font, uint16_t *fb, uint32_t fb_w, uint32_t fb_h,
                           int x, int y, const uint8_t *bytes, int nbytes, uint16_t fg_rgb565,
                           int clip_x, int clip_y, int clip_w, int clip_h, int *out_bbox_w,
                           int *out_bbox_h);

#ifdef __cplusplus
}
#endif

#endif
