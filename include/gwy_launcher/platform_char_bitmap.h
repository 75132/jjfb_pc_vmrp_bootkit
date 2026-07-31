#ifndef GWY_LAUNCHER_PLATFORM_CHAR_BITMAP_H
#define GWY_LAUNCHER_PLATFORM_CHAR_BITMAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product glyph service (sky16 / gb16.uc2).
 * Bit contract (mrporting.h + mythroad dsm.c xl_font_sky16_*):
 *   - 1 = ink, 0 = transparent
 *   - MSB-first within each byte (0x80 = leftmost pixel of that byte)
 *   - row_bytes = (width + 7) / 8; unused LSBs of last byte are 0
 *   - sky16 records are always 16 rows × 2 bytes (= 32) even when width=8
 */

typedef struct PlatformGlyphBitmap {
    const uint8_t *host_bits;
    int width;
    int height;
    int row_bytes;
    int fallback; /* 1 = replacement / blank glyph */
    const char *source; /* "gb16.uc2" / "blank" / ... */
} PlatformGlyphBitmap;

void platform_char_bitmap_reset(void);
void platform_char_bitmap_set_font_path(const char *host_path);

/* Host-side glyph fetch (no Unicorn). host_bits valid until next call or reset. */
int platform_font_get_glyph(uint16_t ch, uint16_t font_size, PlatformGlyphBitmap *out);

/*
 * Guest-visible path: copies glyph into guest-mapped scratch/cache, writes
 * width/height via Guest-safe poke when pointers are non-NULL and writable.
 * Returns 1 on success (guest_addr_out != 0), 0 on hard failure.
 */
int platform_char_bitmap_get_guest(void *uc, uint16_t ch, uint16_t font_size,
                                   uint32_t *guest_addr_out, int *width_out, int *height_out,
                                   int *row_bytes_out);

/* Optional: export PBM self-check for first unique glyphs (out/p13/). */
void platform_char_bitmap_set_dump_dir(const char *dir);

/* Observability */
uint32_t platform_char_bitmap_call_count(void);
uint32_t platform_char_bitmap_cache_live(void);
int platform_char_bitmap_last_fallback(void);

#ifdef __cplusplus
}
#endif

#endif
