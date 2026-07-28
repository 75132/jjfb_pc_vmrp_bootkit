#include "gwy_launcher/jjfb_uc2_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void jjfb_uc2_font_reset(JjfbUc2Font *font) {
    if (!font) return;
    if (font->data) free(font->data);
    memset(font, 0, sizeof(*font));
}

int jjfb_uc2_font_ready(const JjfbUc2Font *font) {
    return font && font->loaded && font->data && font->size >= JJFB_UC2_GLYPH_BYTES;
}

int jjfb_uc2_font_load(JjfbUc2Font *font, const char *host_path) {
    FILE *fp;
    long sz;
    uint8_t *buf;
    if (!font || !host_path || !host_path[0]) return 0;
    jjfb_uc2_font_reset(font);
    fp = fopen(host_path, "rb");
    if (!fp) {
        printf("[JJFB_UC2] open_fail path=%s evidence=OBSERVED\n", host_path);
        fflush(stdout);
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    sz = ftell(fp);
    if (sz < (long)JJFB_UC2_GLYPH_BYTES || sz > 8 * 1024 * 1024) {
        fclose(fp);
        return 0;
    }
    rewind(fp);
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(fp);
        return 0;
    }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    font->data = buf;
    font->size = (size_t)sz;
    font->glyph_bytes = JJFB_UC2_GLYPH_BYTES;
    font->glyph_w = JJFB_UC2_GLYPH_W;
    font->glyph_h = JJFB_UC2_GLYPH_H;
    font->loaded = 1;
    snprintf(font->path, sizeof(font->path), "%s", host_path);
    printf("[JJFB_UC2] loaded path=%s bytes=%zu glyph=%ux%u stride=%u evidence=OBSERVED\n",
           host_path, font->size, font->glyph_w, font->glyph_h, font->glyph_bytes);
    fflush(stdout);
    return 1;
}

int jjfb_uc2_font_load_product_default(JjfbUc2Font *font) {
    char path[512];
    const char *rr = getenv("GWY_RESOURCE_ROOT");
    if (rr && rr[0]) {
        snprintf(path, sizeof(path), "%s/system/gb12.uc2", rr);
        if (jjfb_uc2_font_load(font, path)) return 1;
    }
    snprintf(path, sizeof(path), "game_files/mythroad/240x320/system/gb12.uc2");
    return jjfb_uc2_font_load(font, path);
}

uint16_t jjfb_uc2_gbk_to_id(const uint8_t *bytes, int nbytes, int *consumed) {
    if (consumed) *consumed = 0;
    if (!bytes || nbytes <= 0) return 0;
    if (bytes[0] < 0x80u) {
        if (consumed) *consumed = 1;
        return (uint16_t)bytes[0];
    }
    if (nbytes >= 2 && bytes[0] >= 0x81u) {
        if (consumed) *consumed = 2;
        return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
    }
    if (consumed) *consumed = 1;
    return (uint16_t)bytes[0];
}

int jjfb_uc2_font_glyph(const JjfbUc2Font *font, uint16_t id, uint8_t *out18) {
    size_t off;
    if (!jjfb_uc2_font_ready(font) || !out18) return 0;
    off = (size_t)id * (size_t)font->glyph_bytes;
    if (off + font->glyph_bytes > font->size) return 0;
    memcpy(out18, font->data + off, font->glyph_bytes);
    return 1;
}

int jjfb_uc2_font_measure_gbk(const JjfbUc2Font *font, const uint8_t *bytes, int nbytes, int *out_w,
                              int *out_h) {
    int i = 0, w = 0;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!jjfb_uc2_font_ready(font) || !bytes || nbytes <= 0) return 0;
    while (i < nbytes) {
        int cons = 0;
        uint16_t id = jjfb_uc2_gbk_to_id(bytes + i, nbytes - i, &cons);
        uint8_t g[JJFB_UC2_GLYPH_BYTES];
        if (cons <= 0) break;
        if (!jjfb_uc2_font_glyph(font, id, g)) return 0; /* no fallback */
        w += (int)font->glyph_w;
        i += cons;
    }
    if (out_w) *out_w = w;
    if (out_h) *out_h = (int)font->glyph_h;
    return w > 0;
}

static int glyph_bit(const uint8_t *g18, int row, int col) {
    /* 12x12 packed: 12 bits/row × 12 rows = 144 bits = 18 bytes, MSB first. */
    int bit_index = row * 12 + col;
    int byte_i = bit_index / 8;
    int bit_i = 7 - (bit_index % 8);
    if (row < 0 || row >= 12 || col < 0 || col >= 12) return 0;
    if (byte_i < 0 || byte_i >= 18) return 0;
    return (g18[byte_i] >> bit_i) & 1;
}

int jjfb_uc2_font_draw_gbk(const JjfbUc2Font *font, uint16_t *fb, uint32_t fb_w, uint32_t fb_h,
                           int x, int y, const uint8_t *bytes, int nbytes, uint16_t fg_rgb565,
                           int clip_x, int clip_y, int clip_w, int clip_h, int *out_bbox_w,
                           int *out_bbox_h) {
    int i = 0, pen_x = x, plotted = 0;
    int clip_x1, clip_y1;
    if (out_bbox_w) *out_bbox_w = 0;
    if (out_bbox_h) *out_bbox_h = 0;
    if (!jjfb_uc2_font_ready(font) || !fb || !bytes || nbytes <= 0 || !fb_w || !fb_h) return 0;
    if (clip_w <= 0 || clip_h <= 0) {
        clip_x = 0;
        clip_y = 0;
        clip_w = (int)fb_w;
        clip_h = (int)fb_h;
    }
    clip_x1 = clip_x + clip_w;
    clip_y1 = clip_y + clip_h;
    while (i < nbytes) {
        int cons = 0, row, col;
        uint16_t id = jjfb_uc2_gbk_to_id(bytes + i, nbytes - i, &cons);
        uint8_t g[JJFB_UC2_GLYPH_BYTES];
        if (cons <= 0) break;
        if (!jjfb_uc2_font_glyph(font, id, g)) return 0;
        for (row = 0; row < (int)font->glyph_h; row++) {
            int py = y + row;
            if (py < clip_y || py >= clip_y1 || py < 0 || py >= (int)fb_h) continue;
            for (col = 0; col < (int)font->glyph_w; col++) {
                int px = pen_x + col;
                if (px < clip_x || px >= clip_x1 || px < 0 || px >= (int)fb_w) continue;
                if (!glyph_bit(g, row, col)) continue;
                fb[(uint32_t)py * fb_w + (uint32_t)px] = fg_rgb565;
                plotted = 1;
            }
        }
        pen_x += (int)font->glyph_w;
        i += cons;
    }
    if (out_bbox_w) *out_bbox_w = pen_x - x;
    if (out_bbox_h) *out_bbox_h = (int)font->glyph_h;
    return plotted;
}
