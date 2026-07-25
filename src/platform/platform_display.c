#include "gwy_launcher/platform_display.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/product_runtime_progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define MAX_BLIT_W 512
#define MAX_BLIT_H 512
#define MAX_PIXEL_BYTES (MAX_BLIT_W * MAX_BLIT_H * 2u)

static GwyDisplaySurface g_surf;
static int g_surf_bound;
static GwyDisplayPresentFn g_present;
static uint16_t g_local_fb[PLATFORM_DISPLAY_WIDTH * PLATFORM_DISPLAY_HEIGHT];
static int g_local_fb_init;

static int g_bind_known;
static int g_bind_en;
static int g_trace_known;
static int g_trace_en;
static uint32_t g_drawfp_erw;
static int g_drawfp_published;
static uint32_t g_call_n;
static uint32_t g_reject_n;
static uint32_t g_drawn_n;

static int env_zero(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '0' && e[1] == '\0';
}
static int env_one(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

int platform_display_binding_enabled(void) {
    if (!g_bind_known) {
        /* Default ON; explicit 0 disables ER_RW publish for Variant A. */
        g_bind_en = env_zero("JJFB_DRAWFP_BINDING") ? 0 : 1;
        g_bind_known = 1;
    }
    return g_bind_en;
}

int platform_display_trace_enabled(void) {
    if (!g_trace_known) {
        g_trace_en = env_one("JJFB_DRAWFP_CONTRACT_TRACE") ? 1 : 0;
        g_trace_known = 1;
    }
    return g_trace_en;
}

void platform_display_reset(void) {
    g_bind_known = 0;
    g_bind_en = 0;
    g_trace_known = 0;
    g_trace_en = 0;
    g_drawfp_erw = 0;
    g_drawfp_published = 0;
    g_call_n = 0;
    g_reject_n = 0;
    g_drawn_n = 0;
    /* Keep surface/present across resets within one process (window lifetime). */
}

void platform_display_bind_surface(GwyDisplaySurface *surface) {
    if (!surface || !surface->rgb565 || !surface->width || !surface->height) return;
    g_surf = *surface;
    g_surf_bound = 1;
    if (platform_display_trace_enabled()) {
        printf("[DRAW_FP_SURFACE] bind w=%u h=%u rgb565=%p window=%p evidence=OBSERVED\n",
               g_surf.width, g_surf.height, (void *)g_surf.rgb565, g_surf.window);
        fflush(stdout);
    }
}

void platform_display_set_present_fn(GwyDisplayPresentFn fn) { g_present = fn; }

void gwy_runtime_bind_display_surface(uint32_t width, uint32_t height, uint16_t *rgb565,
                                      void *window, void *renderer, void *texture) {
    GwyDisplaySurface s;
    memset(&s, 0, sizeof(s));
    s.width = width ? width : PLATFORM_DISPLAY_WIDTH;
    s.height = height ? height : PLATFORM_DISPLAY_HEIGHT;
    s.rgb565 = rgb565;
    s.window = window;
    s.renderer = renderer;
    s.texture = texture;
    if (!s.rgb565) {
        if (!g_local_fb_init) {
            memset(g_local_fb, 0, sizeof(g_local_fb));
            g_local_fb_init = 1;
        }
        s.rgb565 = g_local_fb;
        s.width = PLATFORM_DISPLAY_WIDTH;
        s.height = PLATFORM_DISPLAY_HEIGHT;
    }
    platform_display_bind_surface(&s);
}

static uint16_t *surface_fb(uint32_t *out_w, uint32_t *out_h) {
    if (g_surf_bound && g_surf.rgb565) {
        if (out_w) *out_w = g_surf.width;
        if (out_h) *out_h = g_surf.height;
        return g_surf.rgb565;
    }
    if (!g_local_fb_init) {
        memset(g_local_fb, 0, sizeof(g_local_fb));
        g_local_fb_init = 1;
    }
    if (out_w) *out_w = PLATFORM_DISPLAY_WIDTH;
    if (out_h) *out_h = PLATFORM_DISPLAY_HEIGHT;
    return g_local_fb;
}

static void mark_dirty(int x0, int y0, int x1, int y1) {
    if (!g_surf_bound) {
        g_surf.rgb565 = surface_fb(NULL, NULL);
        g_surf.width = PLATFORM_DISPLAY_WIDTH;
        g_surf.height = PLATFORM_DISPLAY_HEIGHT;
        g_surf_bound = 1;
    }
    if (!g_surf.dirty) {
        g_surf.dirty = 1;
        g_surf.dirty_x0 = x0;
        g_surf.dirty_y0 = y0;
        g_surf.dirty_x1 = x1;
        g_surf.dirty_y1 = y1;
        return;
    }
    if (x0 < g_surf.dirty_x0) g_surf.dirty_x0 = x0;
    if (y0 < g_surf.dirty_y0) g_surf.dirty_y0 = y0;
    if (x1 > g_surf.dirty_x1) g_surf.dirty_x1 = x1;
    if (y1 > g_surf.dirty_y1) g_surf.dirty_y1 = y1;
}

static int read_reg_u32(void *uc, int id, uint32_t *out) {
#ifdef GWY_HAVE_UNICORN
    if (!uc || !out) return 0;
    return uc_reg_read((uc_engine *)uc, id, out) == UC_ERR_OK;
#else
    (void)uc;
    (void)id;
    (void)out;
    return 0;
#endif
}

static int read_sp_arg(void *uc, uint32_t sp, uint32_t index, uint32_t *out) {
    /* AAPCS: arg4+ at SP+4*(n-4); index 0 => SP+0. */
    return guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + index * 4u, out);
}

int platform_display_decode_drawbitmap_args(void *uc, GwyDrawBitmapArgs *out) {
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, sp = 0;
    uint32_t a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0;
    int wrapper;

    if (!uc || !out) return 0;
    memset(out, 0, sizeof(*out));
    if (!read_reg_u32(uc, UC_ARM_REG_R0, &r0) || !read_reg_u32(uc, UC_ARM_REG_R1, &r1) ||
        !read_reg_u32(uc, UC_ARM_REG_R2, &r2) || !read_reg_u32(uc, UC_ARM_REG_R3, &r3) ||
        !read_reg_u32(uc, UC_ARM_REG_SP, &sp))
        return 0;
    (void)read_sp_arg(uc, sp, 0, &a0);
    (void)read_sp_arg(uc, sp, 1, &a1);
    (void)read_sp_arg(uc, sp, 2, &a2);
    (void)read_sp_arg(uc, sp, 3, &a3);
    (void)read_sp_arg(uc, sp, 4, &a4);
    (void)read_sp_arg(uc, sp, 5, &a5);

    /* Robotol wrapper: (SP+0 & 0xFFFF) == (R3 & 0xFFFF) && nonzero. */
    wrapper = (((a0 & 0xFFFFu) == (r3 & 0xFFFFu)) && ((a0 & 0xFFFFu) != 0));
    out->guest_pixels = r0;
    out->dst_x = (int16_t)(r1 & 0xFFFFu);
    out->dst_y = (int16_t)(r2 & 0xFFFFu);
    out->wrapper_abi = wrapper;
    if (wrapper) {
        out->src_pitch = (int16_t)(r3 & 0xFFFFu);
        out->copy_w = (int16_t)(a0 & 0xFFFFu);
        out->copy_h = (int16_t)(a1 & 0xFFFFu);
        out->rop = a2;
        out->src_x = (int16_t)(a4 & 0xFFFFu);
        out->src_y = 0;
        out->transparent_key = (uint16_t)(a5 & 0xFFFFu);
    } else {
        out->copy_w = (int16_t)(r3 & 0xFFFFu);
        out->copy_h = (int16_t)(a0 & 0xFFFFu);
        out->rop = a1;
        out->transparent_key = (uint16_t)(a2 & 0xFFFFu);
        out->src_x = (int16_t)(a3 & 0xFFFFu);
        out->src_y = (int16_t)(a4 & 0xFFFFu);
        out->src_pitch = (int16_t)(a5 & 0xFFFFu);
    }
    if (out->src_pitch <= 0) out->src_pitch = out->copy_w;
    /* Transparent: 0xF81F always; values < 0x100 are flag/rop, not keys. */
    out->transparent_enabled =
        (out->transparent_key == 0xF81Fu) || (out->transparent_key >= 0x100u);
    return 1;
}

static int args_safe(const GwyDrawBitmapArgs *a, const char **why) {
    uint64_t bytes;
    if (!a) {
        if (why) *why = "null_args";
        return 0;
    }
    if (!a->guest_pixels) {
        if (why) *why = "null_pixels";
        return 0;
    }
    if (a->guest_pixels < 0x1000u) {
        if (why) *why = "low_pixels";
        return 0;
    }
    if (a->copy_w <= 0 || a->copy_h <= 0) {
        if (why) *why = "nonpositive_wh";
        return 0;
    }
    if (a->src_pitch <= 0) {
        if (why) *why = "bad_pitch";
        return 0;
    }
    if (a->copy_w > a->src_pitch) {
        if (why) *why = "w_gt_pitch";
        return 0;
    }
    if (a->copy_w > MAX_BLIT_W || a->copy_h > MAX_BLIT_H) {
        if (why) *why = "wh_too_large";
        return 0;
    }
    if (a->src_x < 0 || a->src_y < 0) {
        if (why) *why = "neg_src";
        return 0;
    }
    bytes = (uint64_t)(uint32_t)a->src_pitch * (uint64_t)(uint32_t)a->copy_h * 2ull;
    if (bytes == 0 || bytes > MAX_PIXEL_BYTES) {
        if (why) *why = "byte_overflow";
        return 0;
    }
    return 1;
}

int platform_display_draw_bitmap(void *uc, const GwyDrawBitmapArgs *args) {
    uint32_t sw = 0, sh = 0;
    uint16_t *fb;
    int32_t clip_x0, clip_y0, clip_x1, clip_y1;
    int32_t y;
    const char *why = NULL;
    uint8_t probe[4];

    if (!args_safe(args, &why)) {
        g_reject_n++;
        if (platform_display_trace_enabled()) {
            printf("[DRAW_FP_REJECTED_SAFE] reason=%s pixels=0x%X w=%d h=%d pitch=%d "
                   "evidence=OBSERVED\n",
                   why ? why : "?", args ? args->guest_pixels : 0, args ? args->copy_w : 0,
                   args ? args->copy_h : 0, args ? args->src_pitch : 0);
            fflush(stdout);
        }
        return 0;
    }
    /* Prove guest pixels mapped without inventing a host bitmap. */
    if (!guest_memory_uc_peek((struct uc_struct *)uc, args->guest_pixels, probe, 2)) {
        g_reject_n++;
        if (platform_display_trace_enabled()) {
            printf("[DRAW_FP_REJECTED_SAFE] reason=pixels_unmapped pixels=0x%X evidence=OBSERVED\n",
                   args->guest_pixels);
            fflush(stdout);
        }
        return 0;
    }
    if (platform_display_trace_enabled()) {
        printf("[DRAW_FP_PIXELS_VALID] pixels=0x%X evidence=OBSERVED\n", args->guest_pixels);
        fflush(stdout);
    }

    fb = surface_fb(&sw, &sh);
    if (!fb || !sw || !sh) return 0;

    clip_x0 = args->dst_x;
    clip_y0 = args->dst_y;
    clip_x1 = args->dst_x + args->copy_w;
    clip_y1 = args->dst_y + args->copy_h;
    if (clip_x0 < 0) clip_x0 = 0;
    if (clip_y0 < 0) clip_y0 = 0;
    if (clip_x1 > (int32_t)sw) clip_x1 = (int32_t)sw;
    if (clip_y1 > (int32_t)sh) clip_y1 = (int32_t)sh;
    if (clip_x0 >= clip_x1 || clip_y0 >= clip_y1) {
        g_reject_n++;
        if (platform_display_trace_enabled()) {
            printf("[DRAW_FP_REJECTED_SAFE] reason=clipped_empty evidence=OBSERVED\n");
            fflush(stdout);
        }
        return 0;
    }

    for (y = clip_y0; y < clip_y1; y++) {
        int32_t sy = args->src_y + (y - args->dst_y);
        int32_t sx0 = args->src_x + (clip_x0 - args->dst_x);
        uint32_t src_off;
        uint32_t row_bytes;
        int32_t x;
        uint16_t row[MAX_BLIT_W];

        if (sy < 0) continue;
        src_off = (uint32_t)((uint64_t)(uint32_t)sy * (uint64_t)(uint32_t)args->src_pitch +
                             (uint64_t)(uint32_t)sx0) *
                  2u;
        row_bytes = (uint32_t)(clip_x1 - clip_x0) * 2u;
        if (row_bytes > sizeof(row)) break;
        if (!guest_memory_uc_peek((struct uc_struct *)uc, args->guest_pixels + src_off, row,
                                  row_bytes)) {
            g_reject_n++;
            if (platform_display_trace_enabled()) {
                printf("[DRAW_FP_REJECTED_SAFE] reason=row_unmapped y=%d evidence=OBSERVED\n", y);
                fflush(stdout);
            }
            return 0;
        }
        for (x = clip_x0; x < clip_x1; x++) {
            uint16_t pixel = row[x - clip_x0];
            if (args->transparent_enabled && pixel == args->transparent_key) continue;
            fb[(uint32_t)y * sw + (uint32_t)x] = pixel;
        }
    }

    mark_dirty(clip_x0, clip_y0, clip_x1, clip_y1);
    g_drawn_n++;
    if (platform_display_trace_enabled()) {
        printf("[DRAW_FP_DRAWN] dst=%d,%d clip=%d,%d-%d,%d pitch=%d key=0x%X key_en=%d "
               "abi=%s evidence=OBSERVED\n",
               args->dst_x, args->dst_y, clip_x0, clip_y0, clip_x1, clip_y1, args->src_pitch,
               args->transparent_key, args->transparent_enabled,
               args->wrapper_abi ? "wrapper" : "classic");
        fflush(stdout);
    }
    if (g_present) {
        /* Present the clipped sprite rows from fb (already key-applied). */
        int32_t pw = clip_x1 - clip_x0;
        int32_t ph = clip_y1 - clip_y0;
        if (pw > 0 && ph > 0 && pw <= MAX_BLIT_W && ph <= MAX_BLIT_H) {
            uint16_t tmp[MAX_BLIT_W * MAX_BLIT_H];
            int32_t j;
            for (j = 0; j < ph; j++) {
                memcpy(tmp + (size_t)j * (size_t)pw,
                       fb + (uint32_t)(clip_y0 + j) * sw + (uint32_t)clip_x0,
                       (size_t)pw * 2u);
            }
            g_present(tmp, clip_x0, clip_y0, pw, ph, 0, 0);
        }
    }
    return 1;
}

int platform_display_disp_up_ex(int32_t x, int32_t y, int32_t w, int32_t h) {
    uint32_t sw = 0, sh = 0;
    uint16_t *fb = surface_fb(&sw, &sh);
    int32_t x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (!fb) return 0;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int32_t)sw) x1 = (int32_t)sw;
    if (y1 > (int32_t)sh) y1 = (int32_t)sh;
    if (x0 >= x1 || y0 >= y1) return 0;
    mark_dirty(x0, y0, x1, y1);
    if (g_present) {
        int32_t pw = x1 - x0, ph = y1 - y0;
        if (pw > 0 && ph > 0 && pw <= MAX_BLIT_W && ph <= MAX_BLIT_H) {
            uint16_t tmp[MAX_BLIT_W * MAX_BLIT_H];
            int32_t j;
            for (j = 0; j < ph; j++)
                memcpy(tmp + (size_t)j * (size_t)pw, fb + (uint32_t)(y0 + j) * sw + (uint32_t)x0,
                       (size_t)pw * 2u);
            g_present(tmp, x0, y0, pw, ph, 0, 0);
        }
    }
    return 1;
}

uint32_t platform_guest_draw_bitmap(void *uc) {
    GwyDrawBitmapArgs args;
    uint32_t pc = 0, lr = 0, sp = 0, r9 = 0;
    uint32_t erw = 0, mt = 0, slot = 0;
    int drew;

    g_call_n++;
    memset(&args, 0, sizeof(args));
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &lr);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &sp);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &r9);
    }
#else
    (void)pc;
    (void)lr;
    (void)sp;
    (void)r9;
#endif
    mt = ext_chunk_provider_mr_table_guest();
    slot = mt ? (mt + PLATFORM_DRAWBITMAP_MR_TABLE_OFF) : 0;
    erw = r9;
    if (platform_display_trace_enabled()) {
        printf("[DRAW_FP_CALL_ENTER] n=%u pc=0x%X lr=0x%X sp=0x%X r9=0x%X mt=0x%X "
               "slot=0x%X erw150C_off=0x%X evidence=OBSERVED\n",
               g_call_n, pc, lr, sp, r9, mt, slot, PLATFORM_ROBOTOL_ERW_DRAWFP_OFF);
        fflush(stdout);
    }

    if (!platform_display_decode_drawbitmap_args(uc, &args)) {
        g_reject_n++;
        if (platform_display_trace_enabled()) {
            printf("[DRAW_FP_REJECTED_SAFE] reason=decode_fail evidence=OBSERVED\n");
            printf("[DRAW_FP_CALL_RETURN] r0=1 drew=0 evidence=OBSERVED\n");
            fflush(stdout);
        }
        return 1u; /* V67-proven continue */
    }
    if (platform_display_trace_enabled()) {
        printf("[DRAW_FP_ARGS] abi=%s pixels=0x%X dst=%d,%d wh=%dx%d pitch=%d sx=%d sy=%d "
               "rop=0x%X key=0x%X key_en=%d evidence=OBSERVED\n",
               args.wrapper_abi ? "wrapper" : "classic", args.guest_pixels, args.dst_x, args.dst_y,
               args.copy_w, args.copy_h, args.src_pitch, args.src_x, args.src_y, args.rop,
               args.transparent_key, args.transparent_enabled);
        fflush(stdout);
    }

    drew = platform_display_draw_bitmap(uc, &args);
    if (platform_display_trace_enabled()) {
        printf("[DRAW_FP_CALL_RETURN] r0=1 drew=%d reject_n=%u drawn_n=%u evidence=OBSERVED\n",
               drew, g_reject_n, g_drawn_n);
        fflush(stdout);
    }
    (void)erw;
    product_runtime_progress_emit("drawfp_call", drew ? "drawn" : "rejected_safe", "r0=1");
    return 1u;
}

uint32_t platform_drawfp_trampoline_guest(void *uc) {
    uint32_t mt = ext_chunk_provider_mr_table_guest();
    uint32_t fp = 0;
    if (!uc || !mt) return 0;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, mt + PLATFORM_DRAWBITMAP_MR_TABLE_OFF,
                                  &fp))
        return 0;
    return fp;
}

int platform_drawfp_cache_publish(void *uc, uint32_t er_rw, uint32_t mr_table) {
    uint32_t fp = 0, cur = 0;
    int wrote = 0;

    if (!uc || !er_rw || !mr_table) return 0;
    if (!platform_display_binding_enabled()) {
        if (platform_display_trace_enabled()) {
            printf("[DRAW_FP_ERW_SLOT_WRITE] skip binding_off erw=0x%X evidence=OBSERVED\n", er_rw);
            fflush(stdout);
        }
        return 0;
    }
    if (g_drawfp_published && g_drawfp_erw == er_rw) return 1;

    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc,
                                 mr_table + PLATFORM_DRAWBITMAP_MR_TABLE_OFF, &fp) ||
        !fp) {
        printf("[DRAW_FP_TABLE_SLOT] fail reason=empty_fp mt=0x%X evidence=OBSERVED\n", mr_table);
        fflush(stdout);
        return 0;
    }
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, er_rw + PLATFORM_ROBOTOL_ERW_DRAWFP_OFF,
                                   &cur);
    if (cur != fp) {
        if (!guest_memory_uc_poke_u32((struct uc_struct *)uc,
                                     er_rw + PLATFORM_ROBOTOL_ERW_DRAWFP_OFF, fp))
            return 0;
        wrote = 1;
    }
    g_drawfp_erw = er_rw;
    g_drawfp_published = 1;
    printf("[DRAW_FP_SLOT_PUBLISHED] erw=0x%X mt=0x%X mt+1E0=0x%X old150C=0x%X new150C=0x%X "
           "wrote=%d evidence=OBSERVED\n",
           er_rw, mr_table, fp, cur, fp, wrote);
    printf("[DRAW_FP_TABLE_SLOT] mt=0x%X off=0x1E0 fp=0x%X evidence=OBSERVED\n", mr_table, fp);
    printf("[DRAW_FP_ERW_SLOT_WRITE] erw=0x%X off=0x150C old=0x%X new=0x%X evidence=OBSERVED\n",
           er_rw, cur, fp);
    fflush(stdout);
    product_runtime_progress_emit("drawfp_slot_published", "er_rw_150C", "mt_1E0");
    return 1;
}

uint32_t platform_drawfp_call_count(void) { return g_call_n; }
uint32_t platform_drawfp_reject_count(void) { return g_reject_n; }
