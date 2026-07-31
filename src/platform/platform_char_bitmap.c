#include "gwy_launcher/platform_char_bitmap.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* Strong in gwy_ext_obs; weak stub in plain vmrp. */
extern uint32_t gwy_ext_obs_guest_malloc0_ex(uint32_t size, void **host_out);

#define P13_GLYPH_BYTES 32u
#define P13_CHAR_H 16
#define P13_EN_W 8
#define P13_CN_W 16
#define P13_CACHE_CAP 48u
#define P13_LOG_FIRST 32u
#define P13_DUMP_MAX 8u

typedef struct GlyphCacheSlot {
    int used;
    uint16_t ch;
    uint16_t font;
    uint32_t guest_va;
    void *host_ptr; /* MRP heap host view of guest_va */
    uint8_t bits[P13_GLYPH_BYTES];
    int width;
    int height;
    int row_bytes;
    int fallback;
    uint32_t last_use;
} GlyphCacheSlot;

typedef struct CharBitmapState {
    int font_ready;
    int font_failed;
    FILE *font_fp;
    char font_path[520];
    uint8_t host_scratch[P13_GLYPH_BYTES];
    GlyphCacheSlot cache[P13_CACHE_CAP];
    uint32_t use_tick;
    uint32_t call_n;
    int last_fallback;
    char dump_dir[260];
    uint16_t dumped_ch[P13_DUMP_MAX];
    uint16_t dumped_font[P13_DUMP_MAX];
    unsigned dump_n;
} CharBitmapState;

static CharBitmapState g_cb;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

void platform_char_bitmap_reset(void) {
    if (g_cb.font_fp) {
        fclose(g_cb.font_fp);
        g_cb.font_fp = NULL;
    }
    memset(&g_cb, 0, sizeof(g_cb));
}

void platform_char_bitmap_set_font_path(const char *host_path) {
    if (!host_path || !host_path[0]) return;
    snprintf(g_cb.font_path, sizeof(g_cb.font_path), "%s", host_path);
    if (g_cb.font_fp) {
        fclose(g_cb.font_fp);
        g_cb.font_fp = NULL;
    }
    g_cb.font_ready = 0;
    g_cb.font_failed = 0;
}

void platform_char_bitmap_set_dump_dir(const char *dir) {
    snprintf(g_cb.dump_dir, sizeof(g_cb.dump_dir), "%s", dir && dir[0] ? dir : "out/p13");
}

uint32_t platform_char_bitmap_call_count(void) { return g_cb.call_n; }

uint32_t platform_char_bitmap_cache_live(void) {
    unsigned i, n = 0;
    for (i = 0; i < P13_CACHE_CAP; i++)
        if (g_cb.cache[i].used) n++;
    return n;
}

int platform_char_bitmap_last_fallback(void) { return g_cb.last_fallback; }

static int open_font(void) {
    const char *candidates[6];
    int i;
    if (g_cb.font_ready && g_cb.font_fp) return 1;
    if (g_cb.font_failed) return 0;

    i = 0;
    if (g_cb.font_path[0]) candidates[i++] = g_cb.font_path;
    candidates[i++] = "mythroad/system/gb16.uc2";
    candidates[i++] = "system/gb16.uc2";
    candidates[i++] = "game_files/mythroad/320x480/system/gb16.uc2";
    {
        const char *root = getenv("GWY_RESOURCE_ROOT");
        static char rooted[560];
        if (root && root[0]) {
            snprintf(rooted, sizeof(rooted), "%s/system/gb16.uc2", root);
            candidates[i++] = rooted;
        }
    }
    candidates[i] = NULL;

    for (i = 0; candidates[i]; i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (!f) continue;
        g_cb.font_fp = f;
        g_cb.font_ready = 1;
        if (!g_cb.font_path[0])
            snprintf(g_cb.font_path, sizeof(g_cb.font_path), "%s", candidates[i]);
        printf("[JJFB_P13_FONT] open=%s evidence=OBSERVED\n", g_cb.font_path);
        fflush(stdout);
        return 1;
    }
    g_cb.font_failed = 1;
    printf("[JJFB_P13_FONT] open=FAIL note=gb16.uc2_missing evidence=OBSERVED\n");
    fflush(stdout);
    return 0;
}

static void metrics_for_ch(uint16_t ch, int *w, int *h, int *row_bytes) {
    int width = (ch < 128u) ? P13_EN_W : P13_CN_W;
    if (w) *w = width;
    if (h) *h = P13_CHAR_H;
    if (row_bytes) *row_bytes = (width + 7) / 8;
}

static void make_blank_glyph(uint16_t ch, uint8_t *bits, int *w, int *h, int *row_bytes) {
    metrics_for_ch(ch, w, h, row_bytes);
    memset(bits, 0, P13_GLYPH_BYTES);
}

static int load_sky16_record(uint16_t ch, uint8_t *bits) {
    long off;
    size_t n;
    if (!open_font()) return 0;
    off = (long)ch * (long)P13_GLYPH_BYTES;
    if (fseek(g_cb.font_fp, off, SEEK_SET) != 0) return 0;
    n = fread(bits, 1, P13_GLYPH_BYTES, g_cb.font_fp);
    return n == P13_GLYPH_BYTES;
}

/* Ensure unused LSBs in last byte of each row are zero for reported width. */
static void sanitize_row_padding(uint8_t *bits, int width, int height) {
    int row_bytes = (width + 7) / 8;
    int pad_bits = (row_bytes * 8) - width;
    int y;
    uint8_t mask;
    if (pad_bits <= 0 || row_bytes <= 0) return;
    mask = (uint8_t)(0xFFu << pad_bits); /* keep MSBs; clear LSBs */
    /* sky16 stores 2 bytes/row; only touch first row_bytes of each logical row in the 2-byte cell */
    for (y = 0; y < height && y < P13_CHAR_H; y++) {
        uint8_t *row = bits + (y * 2); /* sky16 physical stride */
        if (row_bytes >= 1) {
            /* For width 8: second byte must be 0; first byte fully used */
            if (row_bytes == 1) {
                row[1] = 0;
            } else if (row_bytes == 2) {
                row[1] &= mask;
            }
        }
    }
}

int platform_font_get_glyph(uint16_t ch, uint16_t font_size, PlatformGlyphBitmap *out) {
    int w = 0, h = 0, rb = 0;
    int fallback = 0;
    (void)font_size; /* sky16 ignores size; all MR_FONT_* map to same face */
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    metrics_for_ch(ch, &w, &h, &rb);
    if (!load_sky16_record(ch, g_cb.host_scratch)) {
        make_blank_glyph(ch, g_cb.host_scratch, &w, &h, &rb);
        fallback = 1;
    } else {
        sanitize_row_padding(g_cb.host_scratch, w, h);
        /* Heuristic: all-zero CJK may be missing codepoint → keep bits but mark fallback if empty ink */
        if (ch >= 128u) {
            unsigned i, ink = 0;
            for (i = 0; i < P13_GLYPH_BYTES; i++)
                if (g_cb.host_scratch[i]) ink = 1;
            if (!ink) fallback = 1;
        }
    }
    out->host_bits = g_cb.host_scratch;
    out->width = w;
    out->height = h;
    out->row_bytes = rb;
    out->fallback = fallback;
    out->source = fallback && g_cb.font_failed ? "blank" : "gb16.uc2";
    return 1;
}

static GlyphCacheSlot *cache_find(uint16_t ch, uint16_t font) {
    unsigned i;
    for (i = 0; i < P13_CACHE_CAP; i++) {
        if (g_cb.cache[i].used && g_cb.cache[i].ch == ch && g_cb.cache[i].font == font)
            return &g_cb.cache[i];
    }
    return NULL;
}

static GlyphCacheSlot *cache_alloc_slot(void) {
    unsigned i;
    GlyphCacheSlot *lru = NULL;
    for (i = 0; i < P13_CACHE_CAP; i++) {
        if (!g_cb.cache[i].used) return &g_cb.cache[i];
        if (!lru || g_cb.cache[i].last_use < lru->last_use) lru = &g_cb.cache[i];
    }
    return lru;
}

static int ensure_guest_va(GlyphCacheSlot *slot) {
    void *host = NULL;
    if (slot->guest_va && slot->host_ptr) return 1;
    slot->guest_va = gwy_ext_obs_guest_malloc0_ex(P13_GLYPH_BYTES, &host);
    slot->host_ptr = host;
    return slot->guest_va != 0 && slot->host_ptr != NULL;
}

static void sha256_hex16(const uint8_t *bits, char out[65]) {
    /* Lightweight fingerprint: FNV-1a 64 + length — enough for dump index; full SHA via tool optional */
    uint64_t h = 14695981039346656037ull;
    unsigned i;
    for (i = 0; i < P13_GLYPH_BYTES; i++) {
        h ^= bits[i];
        h *= 1099511628211ull;
    }
    snprintf(out, 65, "%016llx%016llx", (unsigned long long)h, (unsigned long long)~h);
}

static void ensure_dump_dir(const char *dir) {
#ifdef _WIN32
    char cmd[360];
    snprintf(cmd, sizeof(cmd), "mkdir \"%s\" 2>nul", dir);
    (void)system(cmd);
#else
    char cmd[360];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dir);
    (void)system(cmd);
#endif
}

static void maybe_dump_pbm(uint16_t ch, uint16_t font, const GlyphCacheSlot *slot) {
    char path[320];
    char fp[65];
    FILE *f;
    int x, y;
    unsigned i;
    const char *dir;
    if (!env1("JJFB_P13_GLYPH_DUMP") && !g_cb.dump_dir[0]) return;
    if (!g_cb.dump_dir[0]) {
        const char *envd = getenv("JJFB_P13_DUMP_DIR");
        if (envd && envd[0])
            platform_char_bitmap_set_dump_dir(envd);
        else
            platform_char_bitmap_set_dump_dir("out/p13");
    }
    dir = g_cb.dump_dir;
    for (i = 0; i < g_cb.dump_n; i++) {
        if (g_cb.dumped_ch[i] == ch && g_cb.dumped_font[i] == font) return;
    }
    if (g_cb.dump_n >= P13_DUMP_MAX) return;
    g_cb.dumped_ch[g_cb.dump_n] = ch;
    g_cb.dumped_font[g_cb.dump_n] = font;
    g_cb.dump_n++;
    ensure_dump_dir(dir);

    snprintf(path, sizeof(path), "%s/glyph_U%04X_font%u.pbm", dir, (unsigned)ch, (unsigned)font);
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P1\n# ch=U+%04X font=%u w=%d h=%d rb=%d guest=0x%X fallback=%d src=gb16\n%d %d\n",
            (unsigned)ch, (unsigned)font, slot->width, slot->height, slot->row_bytes, slot->guest_va,
            slot->fallback, slot->width, slot->height);
    for (y = 0; y < slot->height; y++) {
        for (x = 0; x < slot->width; x++) {
            int byte_i = x / 8;
            int bit = 7 - (x % 8);
            /* sky16 physical: 2 bytes/row */
            uint8_t b = slot->bits[y * 2 + byte_i];
            int ink = (b >> bit) & 1;
            fputc(ink ? '1' : '0', f);
            fputc(x + 1 < slot->width ? ' ' : '\n', f);
        }
    }
    fclose(f);
    sha256_hex16(slot->bits, fp);
    printf("[JJFB_P13_GLYPH_DUMP] ch=U+%04X font=%u w=%d h=%d row_bytes=%d guest=0x%X "
           "fingerprint=%s fallback=%d path=%s evidence=OBSERVED\n",
           (unsigned)ch, (unsigned)font, slot->width, slot->height, slot->row_bytes, slot->guest_va,
           fp, slot->fallback, path);
    fflush(stdout);
}

int platform_char_bitmap_get_guest(void *uc, uint16_t ch, uint16_t font_size,
                                   uint32_t *guest_addr_out, int *width_out, int *height_out,
                                   int *row_bytes_out) {
    PlatformGlyphBitmap g;
    GlyphCacheSlot *slot;
    uint32_t lr = 0, pc = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0;
    g_cb.call_n++;
    g_cb.use_tick++;

#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &lr);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &sp);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0, &r0);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R1, &r1);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R2, &r2);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R3, &r3);
    }
#else
    (void)uc;
#endif

    if (g_cb.call_n <= P13_LOG_FIRST) {
        printf("[JJFB_P13_GETCHAR] call_index=%u caller_pc=0x%X caller_lr=0x%X ch=0x%X "
               "fontSize=0x%X width_ptr=0x%X height_ptr=0x%X entry_sp=0x%X evidence=OBSERVED\n",
               g_cb.call_n, pc, lr, (unsigned)ch, (unsigned)font_size, r2, r3, sp);
        fflush(stdout);
    }

    if (!platform_font_get_glyph(ch, font_size, &g) || !g.host_bits) {
        if (guest_addr_out) *guest_addr_out = 0;
        return 0;
    }
    g_cb.last_fallback = g.fallback;

    slot = cache_find(ch, font_size);
    if (!slot) {
        slot = cache_alloc_slot();
        if (!slot) {
            if (guest_addr_out) *guest_addr_out = 0;
            return 0;
        }
        slot->used = 1;
        slot->ch = ch;
        slot->font = font_size;
        slot->guest_va = 0; /* (re)alloc below */
    }
    memcpy(slot->bits, g.host_bits, P13_GLYPH_BYTES);
    slot->width = g.width;
    slot->height = g.height;
    slot->row_bytes = g.row_bytes;
    slot->fallback = g.fallback;
    slot->last_use = g_cb.use_tick;

    if (!ensure_guest_va(slot)) {
        printf("[JJFB_P13_GETCHAR] FAIL guest_alloc ch=0x%X evidence=OBSERVED\n", (unsigned)ch);
        fflush(stdout);
        if (guest_addr_out) *guest_addr_out = 0;
        return 0;
    }

    /* Fill host-mapped MRP buffer first (guest VA aliases this pointer). */
    memcpy(slot->host_ptr, slot->bits, P13_GLYPH_BYTES);
    if (uc) {
        /* Keep Unicorn view coherent if the engine tracks separate pages. */
        if (!guest_memory_uc_poke((struct uc_struct *)uc, slot->guest_va, slot->bits,
                                  P13_GLYPH_BYTES)) {
            printf("[JJFB_P13_GETCHAR] WARN poke_bits va=0x%X note=host_filled evidence=OBSERVED\n",
                   slot->guest_va);
            fflush(stdout);
        }
    }

    if (width_out) *width_out = slot->width;
    if (height_out) *height_out = slot->height;
    if (row_bytes_out) *row_bytes_out = slot->row_bytes;
    if (guest_addr_out) *guest_addr_out = slot->guest_va;

    if (g_cb.call_n <= P13_LOG_FIRST || g.fallback) {
        printf("[JJFB_P13_GETCHAR] ret guest=0x%X w=%d h=%d row_bytes=%d fallback=%d "
               "source=%s evidence=OBSERVED\n",
               slot->guest_va, slot->width, slot->height, slot->row_bytes, slot->fallback,
               g.source ? g.source : "?");
        fflush(stdout);
    }

    maybe_dump_pbm(ch, font_size, slot);
    return 1;
}
