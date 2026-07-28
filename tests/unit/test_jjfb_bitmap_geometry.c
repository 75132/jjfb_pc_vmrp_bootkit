#include "gwy_launcher/jjfb_bitmap_geometry.h"
#include <stdio.h>
#include <string.h>

static int expect_wh(const char *tag, const char *pkg, const char *name, uint32_t bytes,
                     uint16_t want_w, uint16_t want_h, const char *want_ev_sub) {
    uint16_t w = 0, h = 0;
    const char *ev = NULL;
    if (!jjfb_resolve_bitmap_geometry(pkg, name, bytes, &w, &h, &ev)) {
        fprintf(stderr, "%s: resolve failed name=%s bytes=%u\n", tag, name, bytes);
        return 0;
    }
    if (w != want_w || h != want_h) {
        fprintf(stderr, "%s: got %ux%u want %ux%u\n", tag, w, h, want_w, want_h);
        return 0;
    }
    if (want_ev_sub && (!ev || !strstr(ev, want_ev_sub))) {
        fprintf(stderr, "%s: evidence=%s missing %s\n", tag, ev ? ev : "(null)", want_ev_sub);
        return 0;
    }
    return 1;
}

int main(void) {
    if (!expect_wh("listicon", "jjfb.mrp", "listicon.bmp", 800u, 20, 20, "listicon")) return 1;
    if (!expect_wh("shan", "attack.mrp", "shan!134!22@attack.bmp", 134u * 32u * 2u, 134, 32,
                   "mismatch"))
        return 1;
    if (!expect_wh("devil", "devil1.mrp", "devil5!59!69@devil1.bmp", 59u * 60u * 2u, 59, 60,
                   "mismatch"))
        return 1;
    if (!expect_wh("loadingbar", "jjfb.mrp", "loadingbar!201!29.bmp", 201u * 29u * 2u, 201, 29,
                   "filename_wh"))
        return 1;
    /* No 11x11 fallback: unknown geometry must reject. */
    {
        uint16_t w = 99, h = 99;
        const char *ev = NULL;
        if (jjfb_resolve_bitmap_geometry("jjfb.mrp", "mystery.bmp", 242u, &w, &h, &ev)) {
            fprintf(stderr, "mystery.bmp should reject, got %ux%u\n", w, h);
            return 1;
        }
    }
    printf("jjfb_bitmap_geometry ok\n");
    return 0;
}
