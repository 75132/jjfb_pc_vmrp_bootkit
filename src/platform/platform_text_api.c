#include "gwy_launcher/platform_text_api.h"

#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/jjfb_uc2_font.h"
#include "gwy_launcher/platform_display.h"
#include "gwy_launcher/product_runtime_progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static JjfbUc2Font g_font;
static int g_en_known;
static int g_en;
static int g_armed;
static int g_fallback; /* product path keeps 0 */
static uint32_t g_last_str_va;
static uint32_t g_draw_n;
static uint32_t g_meas_n;

static int env_zero(const char *k) {
    const char *v = getenv(k);
    return v && v[0] == '0' && v[1] == '\0';
}

int platform_text_api_enabled(void) {
    if (!g_en_known) {
        g_en = env_zero("JJFB_PLATFORM_TEXT_UC2") ? 0 : 1;
        g_en_known = 1;
    }
    return g_en;
}

int platform_text_api_font_ready(void) { return jjfb_uc2_font_ready(&g_font); }

int platform_text_api_font_fallback(void) { return g_fallback; }

void platform_text_api_reset(void) {
    jjfb_uc2_font_reset(&g_font);
    g_en_known = 0;
    g_en = 0;
    g_armed = 0;
    g_fallback = 0;
    g_last_str_va = 0;
    g_draw_n = 0;
    g_meas_n = 0;
}

void platform_text_api_arm(void) {
    if (!platform_text_api_enabled()) return;
    if (!jjfb_uc2_font_ready(&g_font)) (void)jjfb_uc2_font_load_product_default(&g_font);
    g_armed = 1;
    printf("[PLATFORM_TEXT_API] armed uc2=%d path=%s fallback=0 evidence=OBSERVED\n",
           jjfb_uc2_font_ready(&g_font), g_font.path[0] ? g_font.path : "?");
    fflush(stdout);
}

static int peek_cstr(void *uc, uint32_t va, uint8_t *buf, int cap, int *out_n) {
    int i;
    if (!uc || !va || !buf || cap < 2) return 0;
    memset(buf, 0, (size_t)cap);
    if (!guest_memory_uc_peek((struct uc_struct *)uc, va, buf, (uint32_t)(cap - 1))) return 0;
    for (i = 0; i < cap - 1 && buf[i]; i++) {
    }
    if (out_n) *out_n = i;
    return i > 0;
}

static int score_cstr(const uint8_t *b, int n) {
    int i, score = 0;
    if (n <= 0 || n > 64) return -100;
    for (i = 0; i < n; i++) {
        if (b[i] >= 0x20u && b[i] < 0x7Fu) score += 2;
        else if (b[i] >= 0x81u && i + 1 < n && b[i + 1] >= 0x40u) {
            score += 5;
            i++;
        } else if (b[i] == 0)
            break;
        else
            score -= 3;
    }
    return score;
}

/*
 * Resolve guest text for 0x11F00 / 0x12340.
 *
 * Proven caller 0x2F2360 (app=7): r2 = original wrapper r0 (text object),
 * r3 = local {y,x,...} on SP — not a full draw-context blob.
 * Object may be: direct cstr, length-prefixed, or a small object whose first
 * words are pointers into heap strings. Try those before giving up.
 */
static uint32_t resolve_str_va(void *uc, uint32_t code_obj, uint8_t *buf, int cap, int *out_n) {
    uint32_t words[48];
    int i, best_score = -1;
    uint32_t best = 0;
    uint8_t tmp[64];
    int tn = 0;
    static const uint32_t k_inline_off[] = {0u, 4u, 8u, 0xCu, 0x10u, 0x14u};

    if (g_last_str_va && code_obj >= 0x10000u && peek_cstr(uc, g_last_str_va, buf, cap, out_n) &&
        score_cstr(buf, *out_n) >= 4)
        return g_last_str_va;

    if (!code_obj || code_obj < 0x10000u) return 0;

    /* 1) code_obj itself (or short header + inline payload) as cstr. */
    for (i = 0; i < (int)(sizeof(k_inline_off) / sizeof(k_inline_off[0])); i++) {
        uint32_t va = code_obj + k_inline_off[i];
        int sc;
        if (!peek_cstr(uc, va, tmp, (int)sizeof(tmp), &tn)) continue;
        sc = score_cstr(tmp, tn);
        if (sc > best_score) {
            best_score = sc;
            best = va;
            memcpy(buf, tmp, (size_t)((cap < (int)sizeof(tmp)) ? (size_t)cap : sizeof(tmp)));
            if (out_n) *out_n = tn;
        }
    }
    if (best_score >= 4) {
        g_last_str_va = best;
        return best;
    }

    /* 2) Length-prefixed: BE/LE u16/u32 length then bytes (common Mythroad). */
    {
        uint16_t be16 = 0, le16 = 0;
        uint32_t be32 = 0, le32 = 0;
        if (guest_memory_uc_peek((struct uc_struct *)uc, code_obj, (uint8_t *)&le16, 2)) {
            be16 = (uint16_t)(((le16 & 0xFFu) << 8) | ((le16 >> 8) & 0xFFu));
            if (le16 >= 1u && le16 <= 64u &&
                peek_cstr(uc, code_obj + 2u, tmp, (int)sizeof(tmp), &tn) && tn >= (int)le16) {
                /* prefer exact length slice */
                if (tn > (int)le16) tn = (int)le16;
                if (score_cstr(tmp, tn) >= 4) {
                    memcpy(buf, tmp, (size_t)((cap < tn) ? (size_t)cap : (size_t)tn));
                    if (out_n) *out_n = tn;
                    g_last_str_va = code_obj + 2u;
                    return g_last_str_va;
                }
            }
            if (be16 >= 1u && be16 <= 64u &&
                peek_cstr(uc, code_obj + 2u, tmp, (int)sizeof(tmp), &tn) && tn >= 1) {
                if (tn > (int)be16) tn = (int)be16;
                if (score_cstr(tmp, tn) >= 4) {
                    memcpy(buf, tmp, (size_t)((cap < tn) ? (size_t)cap : (size_t)tn));
                    if (out_n) *out_n = tn;
                    g_last_str_va = code_obj + 2u;
                    return g_last_str_va;
                }
            }
        }
        if (guest_memory_uc_peek_u32((struct uc_struct *)uc, code_obj, &le32)) {
            be32 = ((le32 & 0xFFu) << 24) | ((le32 & 0xFF00u) << 8) | ((le32 & 0xFF0000u) >> 8) |
                   ((le32 >> 24) & 0xFFu);
            if (le32 >= 1u && le32 <= 64u &&
                peek_cstr(uc, code_obj + 4u, tmp, (int)sizeof(tmp), &tn) && tn >= 1) {
                if (tn > (int)le32) tn = (int)le32;
                if (score_cstr(tmp, tn) >= 4) {
                    memcpy(buf, tmp, (size_t)((cap < tn) ? (size_t)cap : (size_t)tn));
                    if (out_n) *out_n = tn;
                    g_last_str_va = code_obj + 4u;
                    return g_last_str_va;
                }
            }
            if (be32 >= 1u && be32 <= 64u &&
                peek_cstr(uc, code_obj + 4u, tmp, (int)sizeof(tmp), &tn) && tn >= 1) {
                if (tn > (int)be32) tn = (int)be32;
                if (score_cstr(tmp, tn) >= 4) {
                    memcpy(buf, tmp, (size_t)((cap < tn) ? (size_t)cap : (size_t)tn));
                    if (out_n) *out_n = tn;
                    g_last_str_va = code_obj + 4u;
                    return g_last_str_va;
                }
            }
        }
        (void)be32;
    }

    /* 3) Pointer scan inside object (legacy). */
    best_score = -1;
    best = 0;
    memset(words, 0, sizeof(words));
    if (!guest_memory_uc_peek((struct uc_struct *)uc, code_obj, (uint8_t *)words, sizeof(words)))
        return 0;
    for (i = 0; i < (int)(sizeof(words) / sizeof(words[0])); i++) {
        uint32_t cand = words[i];
        int sc;
        if (cand < 0x1000u || cand > 0x04000000u) continue;
        if (!peek_cstr(uc, cand, tmp, (int)sizeof(tmp), &tn)) continue;
        sc = score_cstr(tmp, tn);
        if (sc > best_score) {
            best_score = sc;
            best = cand;
            memcpy(buf, tmp, (size_t)((cap < (int)sizeof(tmp)) ? (size_t)cap : sizeof(tmp)));
            if (out_n) *out_n = tn;
        }
    }
    if (best_score >= 4) {
        g_last_str_va = best;
        return best;
    }
    return 0;
}

static uint16_t rgb888_to_565(uint32_t rgb) {
    uint32_t r = (rgb >> 16) & 0xFFu;
    uint32_t g = (rgb >> 8) & 0xFFu;
    uint32_t b = rgb & 0xFFu;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

int platform_text_api_handle_11f00(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0,
                                   uint32_t caller_pc, uint32_t caller_lr) {
    uint8_t buf[96];
    int nbytes = 0;
    uint32_t str_va;
    int16_t x = 0, y = 0;
    uint8_t rgb[4];
    uint16_t fg;
    int32_t cx = 0, cy = 0, cw = 0, ch = 0;
    uint32_t mode = 0;
    uint32_t fb_w = 0, fb_h = 0;
    uint16_t *fb;
    int bbox_w = 0, bbox_h = 0;
    int ok;
    char hex[128];
    int hi;

    if (!platform_text_api_enabled()) return 0;
    if (!g_armed) platform_text_api_arm();
    if (!jjfb_uc2_font_ready(&g_font)) {
        printf("[PLATFORM_11F00] handled=0 font_ready=0 note=no_fallback evidence=OBSERVED\n");
        fflush(stdout);
        return 0;
    }

    /*
     * Proven non-draw companion: 0x30CF92 sendAppEvent(0x11F00, app=0x10, code=0x3E8).
     * code is an immediate (1000), not a text object — never reuse last_str_va.
     */
    if (app == 0x10u && code_obj < 0x10000u) {
        printf("[PLATFORM_11F00] handled=0 note=app10_immediate_not_text code=0x%X evidence=OBSERVED\n",
               code_obj);
        fflush(stdout);
        return 0;
    }

    if (param0) {
        (void)guest_memory_uc_peek((struct uc_struct *)uc, param0, &y, 2);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, param0 + 2u, &x, 2);
        memset(rgb, 0, sizeof(rgb));
        /*
         * app=7 (0x2F2360): param0 is a tiny local {y,x,...} on SP — only y/x
         * are valid. Full draw-context offsets apply to heap objects only.
         */
        if (app != 7u && param0 >= 0x280000u) {
            if (guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x2Cu, rgb, 4))
                fg = rgb888_to_565(((uint32_t)rgb[2] << 16) | ((uint32_t)rgb[1] << 8) | rgb[0]);
            else
                fg = 0xFFFFu;
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, param0 + 0x10u, &mode);
            {
                int16_t tcx = 0, tcy = 0, tcw = 0, tch = 0;
                if (guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x08u, &tcx, 2) &&
                    guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x0Au, &tcy, 2) &&
                    guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x0Cu, &tcw, 2) &&
                    guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x0Eu, &tch, 2) &&
                    tcw > 0 && tch > 0 && tcw <= 480 && tch <= 640) {
                    cx = tcx;
                    cy = tcy;
                    cw = tcw;
                    ch = tch;
                }
            }
        } else {
            fg = 0xFFFFu;
        }
    } else {
        fg = 0xFFFFu;
    }

    str_va = resolve_str_va(uc, code_obj, buf, (int)sizeof(buf), &nbytes);
    hex[0] = 0;
    for (hi = 0; hi < nbytes && hi < 24; hi++) {
        char tmp[4];
        snprintf(tmp, sizeof(tmp), "%02X", buf[hi]);
        strncat(hex, tmp, sizeof(hex) - strlen(hex) - 1);
    }

    printf("[PLATFORM_11F00_TRACE] str_va=0x%X nbytes=%d x=%d y=%d clip=%d,%d,%d,%d mode=0x%X "
           "app=0x%X code_obj=0x%X pc=0x%X lr=0x%X hex=%s evidence=OBSERVED\n",
           str_va, nbytes, (int)x, (int)y, (int)cx, (int)cy, (int)cw, (int)ch, mode, app,
           code_obj, caller_pc, caller_lr, hex);
    fflush(stdout);

    if (!str_va || nbytes <= 0) {
        /* Dump object head so next fix can close layout without guessing. */
        uint8_t head[64];
        char hhex[200];
        int hi2, n = 0;
        memset(head, 0, sizeof(head));
        hhex[0] = 0;
        if (code_obj >= 0x1000u &&
            guest_memory_uc_peek((struct uc_struct *)uc, code_obj, head, (uint32_t)sizeof(head))) {
            for (hi2 = 0; hi2 < 32; hi2++) {
                char t[4];
                snprintf(t, sizeof(t), "%02X", head[hi2]);
                strncat(hhex, t, sizeof(hhex) - strlen(hhex) - 1);
            }
            n = 32;
        }
        printf("[PLATFORM_11F00_OBJDUMP] code_obj=0x%X app=0x%X param0=0x%X bytes=%d hex=%s "
               "evidence=OBSERVED\n",
               code_obj, app, param0, n, hhex);
        fflush(stdout);
        return 0;
    }

    fb = platform_display_framebuffer(&fb_w, &fb_h);
    if (!fb) return 0;
    ok = jjfb_uc2_font_draw_gbk(&g_font, fb, fb_w, fb_h, (int)x, (int)y, buf, nbytes, fg, (int)cx,
                                (int)cy, (int)cw, (int)ch, &bbox_w, &bbox_h);
    if (!ok) {
        printf("[PLATFORM_11F00] handled=0 draw_fail font_fallback=0 evidence=OBSERVED\n");
        fflush(stdout);
        return 0;
    }
    (void)platform_display_present_rect(x, y, bbox_w > 0 ? bbox_w : JJFB_UC2_GLYPH_W,
                                        bbox_h > 0 ? bbox_h : JJFB_UC2_GLYPH_H);
    g_draw_n++;
    g_fallback = 0;
    printf("[PLATFORM_11F00] handled=1 app=0x%X str_va=0x%X bbox=%dx%d font_fallback=0 "
           "draw_n=%u evidence=OBSERVED\n",
           app, str_va, bbox_w, bbox_h, g_draw_n);
    fflush(stdout);
    product_runtime_progress_emit("platform_text_11f00", "uc2_drawn", hex);
    (void)caller_pc;
    (void)caller_lr;
    return 1;
}

int platform_text_api_handle_12340(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0,
                                   uint32_t caller_pc, uint32_t caller_lr, uint32_t sp) {
    uint8_t buf[96];
    int nbytes = 0;
    uint32_t str_va;
    int mw = 0, mh = 0;
    uint32_t out_h_ptr = 0;

    if (!platform_text_api_enabled()) return 0;
    if (!g_armed) platform_text_api_arm();
    if (!jjfb_uc2_font_ready(&g_font)) return 0;

    str_va = resolve_str_va(uc, code_obj, buf, (int)sizeof(buf), &nbytes);
    printf("[PLATFORM_12340_TRACE] str_va=0x%X nbytes=%d app=0x%X pc=0x%X lr=0x%X sp=0x%X "
           "evidence=OBSERVED\n",
           str_va, nbytes, app, caller_pc, caller_lr, sp);
    fflush(stdout);
    if (!str_va || nbytes <= 0) return 0;
    if (!jjfb_uc2_font_measure_gbk(&g_font, buf, nbytes, &mw, &mh)) return 0;

    /* Prefer SP[0] as glyph height out when present (legacy classify fill_buf). */
    if (sp >= 0x1000u)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp, &out_h_ptr);
    if (out_h_ptr >= 0x1000u)
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, out_h_ptr, (uint32_t)mh);

    g_meas_n++;
    g_fallback = 0;
    printf("[PLATFORM_12340] handled=1 measure=%dx%d str_va=0x%X font_fallback=0 meas_n=%u "
           "evidence=OBSERVED\n",
           mw, mh, str_va, g_meas_n);
    fflush(stdout);
    product_runtime_progress_emit("platform_text_12340", "uc2_measure", "ok");
    (void)param0;
    (void)app;
    return 1;
}
