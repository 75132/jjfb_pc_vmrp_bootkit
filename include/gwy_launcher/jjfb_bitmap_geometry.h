#ifndef GWY_LAUNCHER_JJFB_BITMAP_GEOMETRY_H
#define GWY_LAUNCHER_JJFB_BITMAP_GEOMETRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Resolve RGB565 bitmap width/height from member name + decoded byte count.
 * Returns 1 on success, 0 if geometry cannot be determined (never invents 11x11).
 */
int jjfb_resolve_bitmap_geometry(const char *package_name, const char *member_name,
                                 uint32_t decoded_bytes, uint16_t *out_w, uint16_t *out_h,
                                 const char **out_evidence);

/* Parse !W!H from member name; returns 1 if both positive ints found. */
int jjfb_parse_name_wh(const char *member_name, int *out_w, int *out_h);

#ifdef __cplusplus
}
#endif

#endif
