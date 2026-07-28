#include "gwy_launcher/jjfb_bitmap_geometry.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int jjfb_parse_name_wh(const char *member_name, int *out_w, int *out_h) {
    const char *p;
    int w = 0, h = 0;
    if (!member_name || !out_w || !out_h) return 0;
    *out_w = 0;
    *out_h = 0;
    p = strchr(member_name, '!');
    if (!p) return 0;
    w = 0;
    p++;
    while (*p >= '0' && *p <= '9') {
        w = w * 10 + (*p - '0');
        p++;
    }
    if (*p != '!') return 0;
    p++;
    h = 0;
    while (*p >= '0' && *p <= '9') {
        h = h * 10 + (*p - '0');
        p++;
    }
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048) return 0;
    *out_w = w;
    *out_h = h;
    return 1;
}

static int ascii_ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb)) return 0;
    }
    return *a == 0 && *b == 0;
}

static int package_is_jjfb(const char *package_name) {
    const char *base;
    if (!package_name || !package_name[0]) return 0;
    base = package_name;
    {
        const char *p = package_name;
        while (*p) {
            if (*p == '/' || *p == '\\') base = p + 1;
            p++;
        }
    }
    return ascii_ieq(base, "jjfb.mrp");
}

int jjfb_resolve_bitmap_geometry(const char *package_name, const char *member_name,
                                 uint32_t decoded_bytes, uint16_t *out_w, uint16_t *out_h,
                                 const char **out_evidence) {
    int name_w = 0, name_h = 0;
    uint32_t pixels;
    if (out_evidence) *out_evidence = "reject";
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!member_name || !member_name[0] || decoded_bytes < 2u || (decoded_bytes & 1u)) return 0;
    pixels = decoded_bytes / 2u;

    if (jjfb_parse_name_wh(member_name, &name_w, &name_h)) {
        uint32_t expect = (uint32_t)name_w * (uint32_t)name_h * 2u;
        if (decoded_bytes == expect) {
            if (out_w) *out_w = (uint16_t)name_w;
            if (out_h) *out_h = (uint16_t)name_h;
            if (out_evidence) *out_evidence = "filename_wh";
            return 1;
        }
        if (name_w > 0 && (pixels % (uint32_t)name_w) == 0u) {
            uint32_t actual_h = pixels / (uint32_t)name_w;
            if (actual_h > 0u && actual_h <= 2048u) {
                if (out_w) *out_w = (uint16_t)name_w;
                if (out_h) *out_h = (uint16_t)actual_h;
                if (out_evidence) *out_evidence = "filename_mismatch";
                printf("[JJFB_BMP_GEOM] filename_mismatch name=\"%s\" file=%dx%d actual=%ux%u "
                       "bytes=%u evidence=OBSERVED\n",
                       member_name, name_w, name_h, (unsigned)name_w, (unsigned)actual_h,
                       decoded_bytes);
                fflush(stdout);
                return 1;
            }
        }
    }

    if (strcmp(member_name, "listicon.bmp") == 0 && decoded_bytes == 800u &&
        (package_is_jjfb(package_name) || !package_name || !package_name[0])) {
        if (out_w) *out_w = 20;
        if (out_h) *out_h = 20;
        if (out_evidence) *out_evidence = "audited_listicon_20x20";
        return 1;
    }

    printf("[JJFB_BMP_GEOM] reject name=\"%s\" pkg=\"%s\" bytes=%u evidence=OBSERVED\n", member_name,
           package_name ? package_name : "", decoded_bytes);
    fflush(stdout);
    return 0;
}
