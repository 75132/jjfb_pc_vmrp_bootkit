#include "gwy_launcher/jjfb_bmp_meta.h"
#include <string.h>

#define JJFB_BMP_META_N 16

typedef struct {
    int used;
    uint32_t pixels_va;
    uint16_t w;
    uint16_t h;
    char member[96];
} JjfbBmpMetaSlot;

static JjfbBmpMetaSlot g_slots[JJFB_BMP_META_N];

void jjfb_bmp_meta_reset(void) {
    memset(g_slots, 0, sizeof(g_slots));
}

void jjfb_bmp_meta_set(uint32_t pixels_va, uint16_t w, uint16_t h, const char *member) {
    int i, free_i = -1;
    if (!pixels_va || w == 0 || h == 0) return;
    for (i = 0; i < JJFB_BMP_META_N; i++) {
        if (g_slots[i].used && g_slots[i].pixels_va == pixels_va) {
            g_slots[i].w = w;
            g_slots[i].h = h;
            memset(g_slots[i].member, 0, sizeof(g_slots[i].member));
            if (member && member[0])
                strncpy(g_slots[i].member, member, sizeof(g_slots[i].member) - 1);
            return;
        }
        if (!g_slots[i].used && free_i < 0) free_i = i;
    }
    if (free_i < 0) {
        /* Full: replace oldest slot (index 0) by shifting left. */
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
