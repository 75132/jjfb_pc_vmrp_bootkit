#include "gwy_launcher/jjfb_plat_11f00.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/jjfb_bmp_meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static JjfbPlat11f00DrawFn g_draw_fn;
static uint32_t g_last_str_va;
static int16_t g_last_x, g_last_y;
static uint32_t g_last_color = 0xFFu;
static FILE *g_calls_csv;
static FILE *g_draw_csv;
static uint32_t g_call_n;

void jjfb_plat_11f00_set_draw_fn(JjfbPlat11f00DrawFn fn) {
    g_draw_fn = fn;
}

void jjfb_plat_11f00_note_guest_cstr(uint32_t str_va, int16_t x, int16_t y, uint32_t color) {
    if (!str_va) return;
    g_last_str_va = str_va;
    if (x != 0 || y != 0) {
        g_last_x = x;
        g_last_y = y;
    }
    if (color) g_last_color = color;
}

uint32_t jjfb_plat_11f00_last_guest_cstr(void) {
    return g_last_str_va;
}

static int env1(const char *k) {
    const char *v = getenv(k);
    return v && v[0] == '1';
}

static void open_csvs(void) {
    const char *c, *d;
    if (!g_calls_csv) {
        c = getenv("JJFB_E9O_11F00_CSV");
        if (!c || !c[0]) c = "reports/e9o_platform_11f00_calls.csv";
        g_calls_csv = fopen(c, "w");
        if (g_calls_csv) {
            fprintf(g_calls_csv,
                    "n,pc,lr,app,code,param0,str_va,str_hex,enc,x,y,color,clip_x,clip_y,clip_w,"
                    "clip_h,mode,ret,handled,font,fallback,note\n");
            fflush(g_calls_csv);
        }
    }
    if (!g_draw_csv) {
        d = getenv("JJFB_E9O_DRAW_CSV");
        if (!d || !d[0]) d = "reports/e9o_text_draw_trace.csv";
        g_draw_csv = fopen(d, "w");
        if (g_draw_csv) {
            fprintf(g_draw_csv, "n,str_va,x,y,bytes,color,font,fallback,ok,note\n");
            fflush(g_draw_csv);
        }
    }
}

static void hex_escape(const uint8_t *b, int n, char *out, int out_cap) {
    int i, o = 0;
    if (!out || out_cap < 4) return;
    out[0] = 0;
    for (i = 0; i < n && o + 3 < out_cap; i++) {
        o += snprintf(out + o, (size_t)(out_cap - o), "%02X", b[i]);
        if (i + 1 < n && o + 1 < out_cap) out[o++] = ' ';
    }
    out[o] = 0;
}

static const char *guess_enc(const uint8_t *b, int n) {
    int i, gbk = 0, ascii = 0;
    if (n >= 2 && b[0] == 0xFFu && b[1] == 0xFEu) return "UCS2_LE_BOM";
    if (n >= 3 && b[0] == 0xEFu && b[1] == 0xBBu && b[2] == 0xBFu) return "UTF8_BOM";
    for (i = 0; i < n; i++) {
        if (b[i] >= 0x20u && b[i] < 0x7Fu) ascii++;
        else if (b[i] >= 0x81u && i + 1 < n && b[i + 1] >= 0x40u) {
            gbk++;
            i++;
        }
    }
    if (gbk > 0) return "GBK";
    if (ascii == n) return "ASCII";
    return "BINARY_OR_MIXED";
}

static int peek_cstr(void *uc, uint32_t va, uint8_t *buf, int cap, int *out_n) {
    int i;
    if (!uc || !va || !buf || cap < 2) return 0;
    memset(buf, 0, (size_t)cap);
    if (!guest_memory_uc_peek((struct uc_struct *)uc, va, buf, cap - 1)) return 0;
    for (i = 0; i < cap - 1 && buf[i]; i++) {
    }
    if (out_n) *out_n = i;
    return i > 0;
}

static int score_cstr(const uint8_t *b, int n) {
    int i, score = 0;
    if (n <= 0 || n > 48) return -100;
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

static uint32_t resolve_str_va(void *uc, uint32_t code_obj, uint8_t *buf, int cap, int *out_n,
                               const char **how) {
    uint32_t words[48];
    int i, best_score = -1;
    uint32_t best = 0;
    uint8_t tmp[64];
    int tn = 0;

    if (g_last_str_va && peek_cstr(uc, g_last_str_va, buf, cap, out_n) &&
        score_cstr(buf, *out_n) >= 4) {
        if (how) *how = "chain_305bfc_cstr";
        return g_last_str_va;
    }

    if (!code_obj) return 0;
    memset(words, 0, sizeof(words));
    if (!guest_memory_uc_peek((struct uc_struct *)uc, code_obj, words, (int)sizeof(words)))
        return 0;

    for (i = 0; i < (int)(sizeof(words) / sizeof(words[0])); i++) {
        uint32_t p = words[i];
        int sc;
        if (p < 0x10000u || p > 0x04000000u) continue;
        if (!peek_cstr(uc, p, tmp, (int)sizeof(tmp), &tn)) continue;
        sc = score_cstr(tmp, tn);
        if (sc > best_score) {
            best_score = sc;
            best = p;
            memcpy(buf, tmp, (size_t)cap);
            if (out_n) *out_n = tn;
        }
    }
    if (best && best_score >= 4) {
        if (how) *how = "code_obj_ptr_scan";
        return best;
    }
    return 0;
}

static uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

int jjfb_plat_11f00_handle(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0,
                           uint32_t caller_pc, uint32_t caller_lr) {
    int16_t x = 0, y = 0;
    uint8_t rgb[4];
    uint16_t fg = 0xFFFFu;
    uint32_t color_u32 = 0xFFu;
    int clip_x = 0, clip_y = 0, clip_w = 240, clip_h = 320;
    uint32_t mode = 0;
    uint8_t buf[96];
    char hex[200];
    int nbytes = 0;
    uint32_t str_va = 0;
    const char *how = "none";
    const char *enc;
    const char *font_name = "none";
    int font_fallback = 0;
    int handled = 0;
    int ok = 0;
    int api_on = env1("JJFB_PLATFORM_TEXT_API_11F00");
    int trace_only = env1("JJFB_E9O_MODE") && !api_on;
    int ret = 0; /* MR_SUCCESS — caller observed to continue after 0 */

    open_csvs();
    memset(buf, 0, sizeof(buf));
    memset(rgb, 0, sizeof(rgb));

    if (param0) {
        (void)guest_memory_uc_peek((struct uc_struct *)uc, param0, &y, 2);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, param0 + 2u, &x, 2);
        if (guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x2Cu, rgb, 4) &&
            (rgb[0] | rgb[1] | rgb[2])) {
            fg = rgb888_to_565(rgb[0], rgb[1], rgb[2]);
            color_u32 = ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | rgb[2];
        }
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, param0 + 0x10u, &mode);
        {
            int16_t cx = 0, cy = 0, cw = 0, ch = 0;
            if (guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x08u, &cx, 2) &&
                guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x0Au, &cy, 2) &&
                guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x0Cu, &cw, 2) &&
                guest_memory_uc_peek((struct uc_struct *)uc, param0 + 0x0Eu, &ch, 2) &&
                cw > 0 && ch > 0 && cw <= 240 && ch <= 320) {
                clip_x = cx;
                clip_y = cy;
                clip_w = cw;
                clip_h = ch;
            }
        }
    }

    /* Prefer live param pack; else last 305BFC coords (still guest-originated). */
    if ((x == 0 && y == 0) || x < 0 || y < 0 || x > 240 || y > 320) {
        if (g_last_str_va) {
            x = g_last_x;
            y = g_last_y;
        }
    }
    if (!(rgb[0] | rgb[1] | rgb[2]) && g_last_color) {
        /* 305BFC R3 often 0xFF = white-ish flag, not RGB888 — keep white fg. */
        if (g_last_color <= 0xFFu) fg = 0xFFFFu;
        color_u32 = g_last_color;
    }

    str_va = resolve_str_va(uc, code_obj, buf, (int)sizeof(buf), &nbytes, &how);
    enc = guess_enc(buf, nbytes);
    hex_escape(buf, nbytes > 24 ? 24 : nbytes, hex, (int)sizeof(hex));

    g_call_n++;
    if (api_on && str_va && nbytes > 0) {
        if (g_draw_fn) {
            ok = g_draw_fn((int)x, (int)y, buf, nbytes, fg, clip_x, clip_y, clip_w, clip_h,
                           &font_name, &font_fallback);
        } else {
            /* Fallback: E9N block glyph path via existing host hook. */
            ok = jjfb_e9n_host_draw_gbk((int)x, (int)y, buf, nbytes);
            font_name = "fallback_block_glyph";
            font_fallback = 1;
        }
        handled = ok ? 1 : 0;
        if (g_draw_csv) {
            fprintf(g_draw_csv, "%u,0x%X,%d,%d,%d,0x%X,\"%s\",%d,%d,%s\n", g_call_n, str_va,
                    (int)x, (int)y, nbytes, color_u32, font_name ? font_name : "?",
                    font_fallback, ok, how);
            fflush(g_draw_csv);
        }
        if (ok) {
            printf("[JJFB_PLATFORM_TEXT_API_11F00] app=0x%X code=0x%X str=0x%X @%d,%d "
                   "bytes=%d enc=%s font=%s fallback=%d how=%s ret=0 "
                   "NOT_PRODUCT evidence=OBSERVED\n",
                   app, code_obj, str_va, (int)x, (int)y, nbytes, enc,
                   font_name ? font_name : "?", font_fallback, how);
            if (font_fallback)
                printf("[JJFB_E9O_CLASS] class=PLATFORM_TEXT_API_11F00_RENDERED_WITH_FONT_FALLBACK "
                       "evidence=OBSERVED\n");
            else
                printf("[JJFB_E9O_CLASS] class=PLATFORM_TEXT_API_11F00_RENDERED "
                       "evidence=OBSERVED\n");
            fflush(stdout);
        } else {
            printf("[JJFB_E9O_CLASS] class=PLATFORM_TEXT_API_11F00_BLOCKED_BY_SURFACE "
                   "str=0x%X evidence=OBSERVED\n",
                   str_va);
            fflush(stdout);
        }
    } else if (api_on && !str_va) {
        printf("[JJFB_E9O_CLASS] class=PLATFORM_TEXT_API_11F00_BLOCKED_BY_ENCODING "
               "note=no_guest_cstr code=0x%X evidence=OBSERVED\n",
               code_obj);
        fflush(stdout);
    } else if (trace_only || env1("JJFB_E9O_MODE")) {
        printf("[JJFB_E9O_11F00_TRACE] app=0x%X code=0x%X p0=0x%X str=0x%X @%d,%d "
               "enc=%s how=%s hex=[%s] evidence=OBSERVED\n",
               app, code_obj, param0, str_va, (int)x, (int)y, enc, how, hex);
        fflush(stdout);
    }

    if (g_calls_csv) {
        fprintf(g_calls_csv,
                "%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\",%s,%d,%d,0x%X,%d,%d,%d,%d,0x%X,%d,%d,"
                "\"%s\",%d,%s\n",
                g_call_n, caller_pc, caller_lr, app, code_obj, param0, str_va, hex, enc, (int)x,
                (int)y, color_u32, clip_x, clip_y, clip_w, clip_h, mode, ret, handled,
                font_name ? font_name : "", font_fallback, how);
        fflush(g_calls_csv);
    }

    return handled;
}
