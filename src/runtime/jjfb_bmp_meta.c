#include "gwy_launcher/jjfb_bmp_meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JJFB_BMP_META_N 16
#define JJFB_BMP_META_MAX_BYTES (240u * 320u * 2u)

typedef struct {
    int used;
    uint32_t pixels_va;
    uint16_t w;
    uint16_t h;
    char member[96];
    uint8_t *pixels;
    size_t nbytes;
} JjfbBmpMetaSlot;

static JjfbBmpMetaSlot g_slots[JJFB_BMP_META_N];
static JjfbE9hBlitFn g_e9h_blit_fn;
static JjfbE9kHoldFn g_e9k_hold_fn;
static JjfbE9nTextDrawFn g_e9n_text_draw_fn;
static int g_e9k_hold_requested;

static void slot_clear_pixels(JjfbBmpMetaSlot *s) {
    if (!s) return;
    free(s->pixels);
    s->pixels = NULL;
    s->nbytes = 0;
}

void jjfb_bmp_meta_reset(void) {
    int i;
    for (i = 0; i < JJFB_BMP_META_N; i++)
        slot_clear_pixels(&g_slots[i]);
    memset(g_slots, 0, sizeof(g_slots));
    g_e9k_hold_requested = 0;
}

static JjfbBmpMetaSlot *slot_upsert(uint32_t pixels_va, uint16_t w, uint16_t h, const char *member) {
    int i, free_i = -1;
    if (!pixels_va || w == 0 || h == 0) return NULL;
    for (i = 0; i < JJFB_BMP_META_N; i++) {
        if (g_slots[i].used && g_slots[i].pixels_va == pixels_va) {
            g_slots[i].w = w;
            g_slots[i].h = h;
            memset(g_slots[i].member, 0, sizeof(g_slots[i].member));
            if (member && member[0])
                strncpy(g_slots[i].member, member, sizeof(g_slots[i].member) - 1);
            return &g_slots[i];
        }
        if (!g_slots[i].used && free_i < 0) free_i = i;
    }
    if (free_i < 0) {
        slot_clear_pixels(&g_slots[0]);
        memmove(&g_slots[0], &g_slots[1], sizeof(g_slots[0]) * (JJFB_BMP_META_N - 1));
        free_i = JJFB_BMP_META_N - 1;
        memset(&g_slots[free_i], 0, sizeof(g_slots[free_i]));
    }
    g_slots[free_i].used = 1;
    g_slots[free_i].pixels_va = pixels_va;
    g_slots[free_i].w = w;
    g_slots[free_i].h = h;
    if (member && member[0])
        strncpy(g_slots[free_i].member, member, sizeof(g_slots[free_i].member) - 1);
    return &g_slots[free_i];
}

void jjfb_bmp_meta_set(uint32_t pixels_va, uint16_t w, uint16_t h, const char *member) {
    (void)slot_upsert(pixels_va, w, h, member);
}

void jjfb_bmp_meta_set_pixels(uint32_t pixels_va, uint16_t w, uint16_t h, const char *member,
                              const void *rgb565, size_t nbytes) {
    JjfbBmpMetaSlot *s = slot_upsert(pixels_va, w, h, member);
    if (!s) return;
    slot_clear_pixels(s);
    if (!rgb565 || nbytes == 0 || nbytes > JJFB_BMP_META_MAX_BYTES) return;
    s->pixels = (uint8_t *)malloc(nbytes);
    if (!s->pixels) return;
    memcpy(s->pixels, rgb565, nbytes);
    s->nbytes = nbytes;
}

int jjfb_bmp_meta_get(uint32_t pixels_va, uint16_t *w_out, uint16_t *h_out, char *member_out,
                      size_t member_cap) {
    int i;
    if (!pixels_va) return 0;
    for (i = 0; i < JJFB_BMP_META_N; i++) {
        if (!g_slots[i].used || g_slots[i].pixels_va != pixels_va) continue;
        if (w_out) *w_out = g_slots[i].w;
        if (h_out) *h_out = g_slots[i].h;
        if (member_out && member_cap > 0) {
            memset(member_out, 0, member_cap);
            strncpy(member_out, g_slots[i].member, member_cap - 1);
        }
        return 1;
    }
    return 0;
}

size_t jjfb_bmp_meta_copy_pixels(uint32_t pixels_va, void *dst, size_t dst_cap) {
    int i;
    if (!pixels_va || !dst || dst_cap == 0) return 0;
    for (i = 0; i < JJFB_BMP_META_N; i++) {
        if (!g_slots[i].used || g_slots[i].pixels_va != pixels_va) continue;
        if (!g_slots[i].pixels || g_slots[i].nbytes == 0) return 0;
        if (g_slots[i].nbytes > dst_cap) return 0;
        memcpy(dst, g_slots[i].pixels, g_slots[i].nbytes);
        return g_slots[i].nbytes;
    }
    return 0;
}

void jjfb_e9h_set_blit_fn(JjfbE9hBlitFn fn) {
    g_e9h_blit_fn = fn;
}

int jjfb_e9h_blit_guest_pixels(void *uc, uint32_t pixels_va, int x, int y, int w, int h,
                               const char *member) {
    if (!g_e9h_blit_fn) return 0;
    return g_e9h_blit_fn(uc, pixels_va, x, y, w, h, member);
}

void jjfb_e9k_set_hold_fn(JjfbE9kHoldFn fn) {
    g_e9k_hold_fn = fn;
}

int jjfb_e9k_hold_requested(void) {
    return g_e9k_hold_requested;
}

void jjfb_e9k_request_hold(const char *reason) {
    if (g_e9k_hold_requested) return;
    g_e9k_hold_requested = 1;
    if (g_e9k_hold_fn) {
        g_e9k_hold_fn(reason ? reason : "e9k");
    } else {
        fprintf(stderr, "[JJFB_E9K_HOLD] fail=no_hold_fn reason=%s evidence=OBSERVED\n",
                reason ? reason : "?");
        fflush(stderr);
    }
}

void jjfb_e9n_set_text_draw_fn(JjfbE9nTextDrawFn fn) {
    g_e9n_text_draw_fn = fn;
}

int jjfb_e9n_host_draw_gbk(int x, int y, const uint8_t *bytes, int nbytes) {
    if (!g_e9n_text_draw_fn || !bytes || nbytes <= 0) return 0;
    return g_e9n_text_draw_fn(x, y, bytes, nbytes);
}
