#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./header/bridge.h"
#include "./header/vmrp.h"
#include "./header/memory.h"
#include "./header/gwy_ext_obs_abi.h"

#ifdef _WIN32
// #ifdef __x86_64__
// #include "./windows/SDL2-2.0.10/x86_64-w64-mingw32/include/SDL2/SDL.h"
// #elif __i386__
#include "./windows/SDL2-2.0.10/i686-w64-mingw32/include/SDL2/SDL.h"
#include "./windows/SDL2-2.0.10/i686-w64-mingw32/include/SDL2/SDL_syswm.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// #endif
#else
#include <SDL2/SDL.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define MOUSE_DOWN 2
#define MOUSE_UP 3
#define MOUSE_MOVE 12

// http://wiki.libsdl.org/Tutorials
// http://lazyfoo.net/tutorials/SDL/index.php

static SDL_TimerID timeId = 0;
static SDL_Window *window;
static bool isMouseDown = false;
static bool isEditMode = false;
static int32_t editMaxSize = 0;
static char *holdEditText = NULL;
static int g_window_zoom = 1; /* JJFB_WINDOW_ZOOM, display scale only */
static int g_e9b_presented = 0;

static SDL_Keycode isKeyDown = SDLK_UNKNOWN;

static int env1(const char *k) {
    const char *v = getenv(k);
    return (v && v[0] == '1') ? 1 : 0;
}

static int env_int(const char *k, int defv) {
    const char *v = getenv(k);
    int n;
    if (!v || !v[0]) return defv;
    n = atoi(v);
    return n > 0 ? n : defv;
}

void guiPumpEvents(void) {
    SDL_Event ev;
    /* Keep HWND responsive; do NOT dispatch game input into Unicorn while
     * mid-bridge (nested emu). Only drain/drop events + Win32 queue. */
    SDL_PumpEvents();
    while (SDL_PollEvent(&ev)) {
        (void)ev; /* drop: QUIT/keys/mouse during emu present path */
    }
#ifdef _WIN32
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
#endif
}

#ifdef _WIN32
/* Capture actual HWND client area via GDI BitBlt (not SDL surface dump). */
static int e9b_hwnd_capture(const char *path, uint32_t *out_nonwhite) {
    SDL_SysWMinfo info;
    HWND hwnd;
    HDC hdc_win = NULL, hdc_mem = NULL;
    HBITMAP hbmp = NULL, hold = NULL;
    BITMAPINFO bmi;
    uint8_t *bits = NULL;
    int cw, ch, row, col, nonwhite = 0, ok = 0;
    FILE *fp = NULL;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_WINDOWS) {
        printf("[JJFB_E9B_HWND_CAPTURE] fail=no_hwnd err=%s evidence=OBSERVED\n",
               SDL_GetError());
        fflush(stdout);
        return 0;
    }
    hwnd = info.info.win.window;
    {
        RECT rc;
        if (!GetClientRect(hwnd, &rc)) return 0;
        cw = rc.right - rc.left;
        ch = rc.bottom - rc.top;
    }
    if (cw <= 0 || ch <= 0 || cw > 4096 || ch > 4096) return 0;
    hdc_win = GetDC(hwnd);
    if (!hdc_win) return 0;
    hdc_mem = CreateCompatibleDC(hdc_win);
    hbmp = CreateCompatibleBitmap(hdc_win, cw, ch);
    if (!hdc_mem || !hbmp) goto done;
    hold = (HBITMAP)SelectObject(hdc_mem, hbmp);
    if (!BitBlt(hdc_mem, 0, 0, cw, ch, hdc_win, 0, 0, SRCCOPY)) goto done;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cw;
    bmi.bmiHeader.biHeight = -ch; /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bits = (uint8_t *)malloc((size_t)cw * (size_t)ch * 4u);
    if (!bits) goto done;
    if (!GetDIBits(hdc_mem, hbmp, 0, (UINT)ch, bits, &bmi, DIB_RGB_COLORS)) goto done;
    for (row = 0; row < ch; row++) {
        for (col = 0; col < cw; col++) {
            uint8_t *p = bits + ((size_t)row * (size_t)cw + (size_t)col) * 4u;
            /* BGRA */
            if (!(p[0] > 250 && p[1] > 250 && p[2] > 250) &&
                !(p[0] < 5 && p[1] < 5 && p[2] < 5))
                nonwhite++;
        }
    }
    /* Write 24bpp BMP manually (simple). */
    {
        int row_pad = (4 - ((cw * 3) % 4)) % 4;
        uint32_t img_sz = (uint32_t)((cw * 3 + row_pad) * ch);
        uint32_t file_sz = 54u + img_sz;
        uint8_t hdr[54];
        int r, c;
        memset(hdr, 0, sizeof(hdr));
        hdr[0] = 'B';
        hdr[1] = 'M';
        memcpy(hdr + 2, &file_sz, 4);
        hdr[10] = 54;
        hdr[14] = 40;
        memcpy(hdr + 18, &cw, 4);
        {
            int nh = ch; /* bottom-up in file */
            memcpy(hdr + 22, &nh, 4);
        }
        hdr[26] = 1;
        hdr[28] = 24;
        fp = fopen(path, "wb");
        if (!fp) goto done;
        fwrite(hdr, 1, 54, fp);
        for (r = ch - 1; r >= 0; r--) {
            for (c = 0; c < cw; c++) {
                uint8_t *p = bits + ((size_t)r * (size_t)cw + (size_t)c) * 4u;
                fputc(p[0], fp);
                fputc(p[1], fp);
                fputc(p[2], fp);
            }
            for (c = 0; c < row_pad; c++) fputc(0, fp);
        }
        fclose(fp);
        fp = NULL;
        ok = 1;
    }
    if (out_nonwhite) *out_nonwhite = (uint32_t)nonwhite;
    printf("[JJFB_E9B_HWND_CAPTURE] path=%s w=%d h=%d nonwhite_or_nonblack=%d "
           "kind=GDI_BitBlt_client evidence=OBSERVED\n",
           path, cw, ch, nonwhite);
    fflush(stdout);
done:
    if (fp) fclose(fp);
    if (hold) SelectObject(hdc_mem, hold);
    if (hbmp) DeleteObject(hbmp);
    if (hdc_mem) DeleteDC(hdc_mem);
    if (hdc_win) ReleaseDC(hwnd, hdc_win);
    if (bits) free(bits);
    return ok;
}
#else
static int e9b_hwnd_capture(const char *path, uint32_t *out_nonwhite) {
    (void)path;
    (void)out_nonwhite;
    return 0;
}
#endif

static void e9b_after_first_frame_present(SDL_Surface *surface, uint32_t other,
                                          uint32_t white, uint32_t black) {
    const char *cap_path;
    uint32_t nw = 0;
    int hold_sec;
    Uint32 t0, now;
    (void)surface;
    (void)white;
    (void)black;
    /* Force another present + pump so OS paints before capture. */
    guiPumpEvents();
    (void)SDL_UpdateWindowSurface(window);
    guiPumpEvents();
    SDL_Delay(50);
    guiPumpEvents();

    cap_path = getenv("JJFB_E9B_HWND_CAPTURE");
    if (!cap_path || !cap_path[0])
        cap_path = "screenshots/e9b_actual_window_capture.bmp";
    if (!e9b_hwnd_capture(cap_path, &nw)) {
        printf("[JJFB_VISIBLE_WINDOW] class=WINDOW_CAPTURE_STILL_BLANK "
               "note=hwnd_capture_failed evidence=OBSERVED\n");
        fflush(stdout);
    } else if (nw == 0) {
        printf("[JJFB_VISIBLE_WINDOW] class=WINDOW_CAPTURE_STILL_BLANK "
               "nonwhite=0 path=%s evidence=OBSERVED\n",
               cap_path);
        fflush(stdout);
    } else if (other == 0 && nw > 0) {
        printf("[JJFB_VISIBLE_WINDOW] class=WINDOW_PRESENT_BLOCKED_BY_WHITE_BACKGROUND "
               "hwnd_nonwhite=%u evidence=OBSERVED\n",
               nw);
        fflush(stdout);
    } else {
        printf("[JJFB_VISIBLE_WINDOW] class=VISIBLE_WINDOW_PRESENTED "
               "hwnd_nonwhite=%u sprite_other=%u evidence=OBSERVED\n",
               nw, other);
        fflush(stdout);
    }

    hold_sec = env_int("JJFB_E9B_HOLD_SEC", 0);
    if (hold_sec <= 0 && (env1("JJFB_E9B_MODE") || env1("JJFB_E9C_MODE") ||
                           env1("JJFB_VISIBLE_WINDOW")))
        hold_sec = 30;
    if (hold_sec > 0) {
        printf("[JJFB_VISIBLE_WINDOW_HOLD] sec=%d note=pump_messages_keep_responsive "
               "evidence=OBSERVED\n",
               hold_sec);
        printf("[JJFB_WINDOW_RESPONSIVE_HOLD] sec=%d pump_ms=16 evidence=OBSERVED\n",
               hold_sec);
        fflush(stdout);
        t0 = SDL_GetTicks();
        for (;;) {
            guiPumpEvents();
            (void)SDL_UpdateWindowSurface(window);
            SDL_Delay(16);
            now = SDL_GetTicks();
            if ((int)(now - t0) >= hold_sec * 1000)
                break;
        }
        printf("[JJFB_VISIBLE_WINDOW_HOLD_DONE] sec=%d evidence=OBSERVED\n", hold_sec);
        printf("[JJFB_WINDOW_RESPONSIVE_HOLD] done=1 evidence=OBSERVED\n");
        fflush(stdout);
    }
}

/* E9C contactsheet: finish HWND capture + hold after multiple real sprite blits. */
void guiVisibleWindowFinalize(void) {
    SDL_Surface *surface = SDL_GetWindowSurface(window);
    if (!surface) return;
    guiPumpEvents();
    (void)SDL_UpdateWindowSurface(window);
    e9b_after_first_frame_present(surface, 1u, 0u, 0u);
}

void saveEditText(char *str) {
    uint8_t *utf8Str = (uint8_t *)str;
    int32_t n = 0;
    while (*utf8Str && (n < editMaxSize)) {
        if (*utf8Str < 0x80) {  // 1 Byte
            utf8Str += 1;
        } else if ((*utf8Str & 0xe0) == 0xc0) {  // 2 Bytes
            utf8Str += 2;
        } else if ((*utf8Str & 0xf0) == 0xe0) {  // 3 Bytes
            utf8Str += 3;
        } else {
            break;
        }
        n++;
    }
    if (holdEditText != NULL) {
        my_freeExt(holdEditText);
        holdEditText = NULL;
    }
    uint32_t len = (uint32_t)utf8Str - (uint32_t)str;
    holdEditText = my_mallocExt(len + 1);
    memcpy(holdEditText, str, len);
    holdEditText[len] = '\0';
}

int32_t editCreate(const char *title, const char *text, int32_t type, int32_t max_size) {
    isEditMode = true;
    editMaxSize = max_size;
    SDL_Log("title: '%s', text: '%s', type: %d, max_size: %d", title, text, type, max_size);
    if (SDL_SetClipboardText(text) == 0) {
        SDL_Log("编辑内容已复制到剪贴板，按ctrl+v输入内容，按ctrl+z取消");
    } else {
        SDL_Log("无法使用剪贴板");
    }
    return 1234;
}

int32 editRelease(int32 edit) {
    isEditMode = false;
    if (holdEditText != NULL) {
        my_freeExt(holdEditText);
        holdEditText = NULL;
    }
    return MR_SUCCESS;
}

char *editGetText(int32 edit) {
    SDL_Log("editGetText(): '%s'", holdEditText);
    return holdEditText;
}

void guiDrawBitmap(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h) {
    static int s_first_real_saved = 0;
    SDL_Surface *surface = SDL_GetWindowSurface(window);
    if (!bmp || w <= 0 || h <= 0) return;
    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) != 0) printf("SDL_LockSurface err\n");
    }
    for (int32_t j = 0; j < h; j++) {
        for (int32_t i = 0; i < w; i++) {
            int32_t xx = x + i;
            int32_t yy = y + j;
            if (xx < 0 || yy < 0 || xx >= SCREEN_WIDTH || yy >= SCREEN_HEIGHT) {
                continue;
            }
            uint16_t color = *(bmp + (xx + yy * SCREEN_WIDTH));
            Uint32 *p = (Uint32 *)(((Uint8 *)surface->pixels) + surface->pitch * yy) + xx;
            *p = SDL_MapRGB(surface->format, PIXEL565R(color), PIXEL565G(color), PIXEL565B(color));
        }
    }
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
    if (SDL_UpdateWindowSurface(window) != 0)
        printf("SDL_UpdateWindowSurface err\n");

    /* E8U-DisplayFirst: save first non-trivial frame from real draw path (not host paint). */
    if (!s_first_real_saved && bmp && w > 0 && h > 0) {
        const char *df = getenv("JJFB_DISPLAY_FIRST");
        int nontrivial = 0;
        int32_t j, i;
        uint32_t black = 0, white = 0, other = 0;
        for (j = 0; j < h && j < SCREEN_HEIGHT; j++) {
            for (i = 0; i < w && i < SCREEN_WIDTH; i++) {
                uint16_t c = *(bmp + ((x + i) + (y + j) * SCREEN_WIDTH));
                if (c == 0) black++;
                else if (c == 0xFFFFu) white++;
                else other++;
            }
        }
        nontrivial = (other > 16) || (black > 0 && white > 0 && (black + white) > 64);
        if (nontrivial) {
            const char *path = getenv("JJFB_E8U_SCREENSHOT");
            if (!path || !path[0]) path = "screenshots/e8u_first_real_frame.bmp";
            if (SDL_SaveBMP(surface, path) == 0) {
                s_first_real_saved = 1;
                printf("[JJFB_E8U_FIRST_REAL_FRAME] path=%s x=%d y=%d w=%d h=%d "
                       "black=%u white=%u other=%u note=real_guiDrawBitmap "
                       "display_first=%s evidence=OBSERVED\n",
                       path, (int)x, (int)y, (int)w, (int)h, black, white, other,
                       (df && df[0] == '1') ? "1" : "0");
                fflush(stdout);
            } else {
                printf("[JJFB_E8U_FIRST_REAL_FRAME] save_fail path=%s err=%s evidence=OBSERVED\n",
                       path, SDL_GetError());
                fflush(stdout);
            }
        }
    }
}

/* E8Z/E9B: blit sprite RGB565 with pitch=w into the SDL window (real mr_drawBitmap path).
 * Screenshot is SDL software surface SaveBMP — NOT HWND capture (see e9b_hwnd_capture). */
void guiDrawBitmapSprite(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h) {
    static int s_e8z_saved = 0;
    static int s_blit_n = 0;
    static int s_deferred_hold_done = 0;
    SDL_Surface *surface;
    const char *shot;
    const char *before_path;
    const char *after_path;
    uint32_t black = 0, white = 0, other = 0, key = 0;
    int32_t j, i;
    int zoom = g_window_zoom > 0 ? g_window_zoom : 1;
    int e9b = env1("JJFB_E9B_MODE") || env1("JJFB_E9C_MODE") || env1("JJFB_VISIBLE_WINDOW");
    int e9j = env1("JJFB_E9J_MODE");
    int e9k = env1("JJFB_E9K_MODE") || env1("JJFB_E9L_MODE");
    /* E9K/E9L: do not hold on early progress blits — post-r4/text may need guest to continue.
     * Hold is armed from robotol via jjfb_e9k_request_hold → guiVisibleWindowFinalize. */
    int defer_hold = env1("JJFB_E9C_DEFER_HOLD") || e9j || e9k;
    int hold_after = env_int("JJFB_E9J_HOLD_AFTER_BLIT", e9j ? 2 : 0);
    if (e9k && (env1("JJFB_E9K_HOLD_AFTER_POST_R4") || !getenv("JJFB_E9J_HOLD_AFTER_BLIT")))
        hold_after = env_int("JJFB_E9K_MIN_BLITS", 64); /* high floor; prefer post-r4 arm */
    if (!bmp || w <= 0 || h <= 0) return;
    s_blit_n++;
    surface = SDL_GetWindowSurface(window);
    if (!surface) {
        printf("[JJFB_VISIBLE_WINDOW] class=WINDOW_PRESENT_BLOCKED_BY_SURFACE_COPY "
               "err=%s evidence=OBSERVED\n",
               SDL_GetError());
        fflush(stdout);
        return;
    }
    shot = getenv("JJFB_E8Z_SCREENSHOT");
    if (!shot || !shot[0]) shot = "screenshots/e8z_first_real_frame.bmp";
    before_path = getenv("JJFB_E8Z_SCREENSHOT_BEFORE");
    if (!before_path || !before_path[0]) before_path = "screenshots/e8z_first_real_frame_before.bmp";
    after_path = getenv("JJFB_E8Z_SCREENSHOT_AFTER");
    if (!after_path || !after_path[0]) after_path = "screenshots/e8z_first_real_frame_after.bmp";

    if (!s_e8z_saved)
        (void)SDL_SaveBMP(surface, before_path);

    if (SDL_MUSTLOCK(surface)) {
        if (SDL_LockSurface(surface) != 0) printf("SDL_LockSurface err\n");
    }
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            int32_t xx = x + i;
            int32_t yy = y + j;
            uint16_t color;
            int zi, zj;
            if (xx < 0 || yy < 0 || xx >= SCREEN_WIDTH || yy >= SCREEN_HEIGHT)
                continue;
            color = bmp[(uint32_t)j * (uint32_t)w + (uint32_t)i];
            if (color == 0) black++;
            else if (color == 0xFFFFu) white++;
            else if (color == 0xF81Fu) key++;
            else other++;
            /* Display zoom is nearest-neighbor only; pixels still from real buffer. */
            for (zj = 0; zj < zoom; zj++) {
                for (zi = 0; zi < zoom; zi++) {
                    int dx = xx * zoom + zi;
                    int dy = yy * zoom + zj;
                    Uint32 *p;
                    if (dx < 0 || dy < 0 || dx >= surface->w || dy >= surface->h)
                        continue;
                    p = (Uint32 *)(((Uint8 *)surface->pixels) + surface->pitch * dy) + dx;
                    *p = SDL_MapRGB(surface->format, PIXEL565R(color), PIXEL565G(color),
                                    PIXEL565B(color));
                }
            }
        }
    }
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);

    guiPumpEvents();
    if (SDL_UpdateWindowSurface(window) != 0) {
        printf("[JJFB_VISIBLE_WINDOW] class=WINDOW_PRESENT_BLOCKED_BY_SURFACE_COPY "
               "err=%s evidence=OBSERVED\n",
               SDL_GetError());
        fflush(stdout);
    } else {
        printf("[JJFB_VISIBLE_WINDOW_PRESENTED] via=SDL_UpdateWindowSurface "
               "source=SDL_GetWindowSurface zoom=%d x=%d y=%d w=%d h=%d "
               "note=NOT_HWND_CAPTURE_YET evidence=OBSERVED\n",
               zoom, (int)x, (int)y, (int)w, (int)h);
        fflush(stdout);
        g_e9b_presented = 1;
    }
    guiPumpEvents();

    printf("[JJFB_E8Z_SPRITE_BLIT] x=%d y=%d w=%d h=%d black=%u white=%u other=%u key=%u "
           "evidence=OBSERVED\n",
           (int)x, (int)y, (int)w, (int)h, black, white, other, key);
    fflush(stdout);

    if (!s_e8z_saved && (other > 0 || (black > 0 && white > 0) || key > 0)) {
        if (SDL_SaveBMP(surface, after_path) == 0 && SDL_SaveBMP(surface, shot) == 0) {
            s_e8z_saved = 1;
            printf("[JJFB_SCREENSHOT_SOURCE] path=%s kind=SDL_GetWindowSurface_SaveBMP "
                   "note=internal_sdl_software_surface_NOT_hwnd_PrintWindow "
                   "evidence=OBSERVED\n",
                   shot);
            printf("[JJFB_FIRST_REAL_FRAME_REACHED] path=%s before=%s after=%s "
                   "x=%d y=%d w=%d h=%d black=%u white=%u other=%u key=%u "
                   "note=real_mrp_pixels_via_mr_drawBitmap evidence=OBSERVED\n",
                   shot, before_path, after_path, (int)x, (int)y, (int)w, (int)h, black,
                   white, other, key);
            fflush(stdout);
            if (e9b && !defer_hold)
                e9b_after_first_frame_present(surface, other, white, black);
        } else {
            printf("[JJFB_FIRST_REAL_FRAME_REACHED] save_fail path=%s err=%s evidence=OBSERVED\n",
                   shot, SDL_GetError());
            fflush(stdout);
        }
    } else if (!s_e8z_saved) {
        printf("[JJFB_E8Z_CLASS] class=DRAW_API_WITH_NONZERO_BMP_NO_FRAMEBUFFER_DELTA "
               "black=%u white=%u other=%u evidence=OBSERVED\n",
               black, white, other);
        fflush(stdout);
    }
    /* E9J: first loadingbar must not HOLD (stop watcher kills before progress loop).
     * Defer capture/hold until blit N (default 2 = first progress bar segment).
     * E9K: hold_after is raised; preferred path is jjfb_e9k_request_hold after post-r4. */
    if (e9b && defer_hold && !s_deferred_hold_done && hold_after > 0 &&
        s_blit_n >= hold_after && s_e8z_saved) {
        s_deferred_hold_done = 1;
        printf("[JJFB_E9J_DEFERRED_HOLD] blit_n=%d hold_after=%d note=after_progress_blit "
               "evidence=OBSERVED\n",
               s_blit_n, hold_after);
        if (e9k)
            printf("[JJFB_E9K_DEFERRED_HOLD] blit_n=%d hold_after=%d "
                   "note=fallback_min_blits_not_post_r4_arm evidence=OBSERVED\n",
                   s_blit_n, hold_after);
        fflush(stdout);
        e9b_after_first_frame_present(surface, other > 0 ? other : 1u, white, black);
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
void setEventEnable(int v) {
    int state = v ? SDL_ENABLE : SDL_DISABLE;
    SDL_EventState(SDL_TEXTINPUT, state);
    SDL_EventState(SDL_KEYDOWN, state);
    SDL_EventState(SDL_KEYUP, state);
    SDL_EventState(SDL_MOUSEMOTION, state);
    SDL_EventState(SDL_MOUSEBUTTONDOWN, state);
    SDL_EventState(SDL_MOUSEBUTTONUP, state);
}
#endif

/* Wake host loop after SDL timer (never take bridge mutex on this thread). */
#define VMRP_USER_TIMER 1

static void host_timer_poll(void) {
    gwy_ext_obs_post_start_loop_tick((uint32_t)SDL_GetTicks());
    if (!gwy_ext_obs_timer_take_due()) return;
    printf("[PLATFORM_TIMER] op=FIRE_DUE via=host_loop evidence=DOCUMENTED\n");
    fflush(stdout);
    (void)timer();
    printf("[PLATFORM_TIMER] op=FIRE_DONE via=host_loop evidence=DOCUMENTED\n");
    fflush(stdout);
}

uint32_t timerCb(uint32_t interval, void *param) {
    SDL_Event wake;
    (void)param;
    SDL_RemoveTimer(timeId);
    timeId = 0;
    printf("[PLATFORM_TIMER] op=FIRE_CB interval=%u evidence=DOCUMENTED\n", (unsigned)interval);
    fflush(stdout);
    /* Do NOT call timer()/mutex here — may race nested start_dsm.
     * Mark due + wake main loop (WaitEventTimeout / USEREVENT). */
    gwy_ext_obs_timer_signal_due();
    SDL_zero(wake);
    wake.type = SDL_USEREVENT;
    wake.user.code = VMRP_USER_TIMER;
    SDL_PushEvent(&wake);
    return 0;
}

int32_t timerStart(uint16_t t) {
    if (!timeId) {
        timeId = SDL_AddTimer(t, timerCb, NULL);
    } else {
        SDL_RemoveTimer(timeId);
        timeId = SDL_AddTimer(t, timerCb, NULL);
    }
    return MR_SUCCESS;
}

int32_t timerStop() {
    if (timeId) {
        SDL_RemoveTimer(timeId);
        timeId = 0;
    }
    return MR_SUCCESS;
}

static void keyEvent(int16 type, SDL_Keycode code) {
    if (code >= SDLK_0 && code <= SDLK_9) {
        int32_t key = MR_KEY_0 + (code - SDLK_0);
        event(type, key, 0);  // 按键 0-9
        return;
    }
    switch (code) {
        case SDLK_KP_0:
            event(type, MR_KEY_0, 0);
            break;
        case SDLK_KP_1:
            event(type, MR_KEY_1, 0);
            break;
        case SDLK_KP_2:
            event(type, MR_KEY_2, 0);
            break;
        case SDLK_KP_3:
            event(type, MR_KEY_3, 0);
            break;
        case SDLK_KP_4:
            event(type, MR_KEY_4, 0);
            break;
        case SDLK_KP_5:
            event(type, MR_KEY_5, 0);
            break;
        case SDLK_KP_6:
            event(type, MR_KEY_6, 0);
            break;
        case SDLK_KP_7:
            event(type, MR_KEY_7, 0);
            break;
        case SDLK_KP_8:
            event(type, MR_KEY_8, 0);
            break;
        case SDLK_KP_9:
            event(type, MR_KEY_9, 0);
            break;
        case SDLK_KP_ENTER:
        case SDLK_RETURN:                   // 回车键
            event(type, MR_KEY_SELECT, 0);  // 确认/选择/ok
            break;
        case SDLK_EQUALS:                  // 等号
            event(type, MR_KEY_POUND, 0);  // 按键 #
            break;
        case SDLK_MINUS:                  // 减号
            event(type, MR_KEY_STAR, 0);  // 按键 *
            break;
        case SDLK_w:
        case SDLK_UP:  // 上
            event(type, MR_KEY_UP, 0);
            break;
        case SDLK_s:
        case SDLK_DOWN:  // 下
            event(type, MR_KEY_DOWN, 0);
            break;
        case SDLK_a:
        case SDLK_LEFT:  // 左
            event(type, MR_KEY_LEFT, 0);
            break;
        case SDLK_d:
        case SDLK_RIGHT:  // 右
            event(type, MR_KEY_RIGHT, 0);
            break;
        case SDLK_q:
        case SDLK_LEFTBRACKET:                // 左中括号
            event(type, MR_KEY_SOFTLEFT, 0);  // 左功能键
            break;
        case SDLK_e:
        case SDLK_RIGHTBRACKET:                // 右中括号
            event(type, MR_KEY_SOFTRIGHT, 0);  // 右功能键
            break;
        case SDLK_TAB:
            event(type, MR_KEY_SEND, 0);  // 接听键
            break;
        case SDLK_ESCAPE:
            event(type, MR_KEY_POWER, 0);  // 挂机键
            break;
        default:
            printf("key:%d\n", code);
            break;
    }
}

static void handle_sdl_event(SDL_Event *ev, bool *isLoop) {
    if (ev->type == SDL_QUIT) {
        *isLoop = false;
        return;
    }
    if (ev->type == SDL_USEREVENT && ev->user.code == VMRP_USER_TIMER) {
        host_timer_poll();
        return;
    }
    if (isEditMode) {
        switch (ev->type) {
            case SDL_KEYDOWN: {
                if (SDL_GetModState() & KMOD_CTRL) {
                    if (ev->key.keysym.sym == SDLK_z) {
                        event(MR_DIALOG_EVENT, 1, 0);
                        SDL_Log("取消输入");
                        return;
                    } else if (ev->key.keysym.sym == SDLK_v) {
                        char *str = SDL_GetClipboardText();
                        saveEditText(str);
                        SDL_free(str);
                        event(MR_DIALOG_EVENT, 0, 0);
                        return;
                    }
                }
            }
            case SDL_MOUSEBUTTONDOWN:
                SDL_Log("ctrl+v输入内容，ctrl+z取消输入");
                break;
            default:
                break;
        }
        return;
    }
    switch (ev->type) {
        case SDL_KEYDOWN:
            if (isKeyDown == SDLK_UNKNOWN) {
                isKeyDown = ev->key.keysym.sym;
                keyEvent(MR_KEY_PRESS, ev->key.keysym.sym);
            }
            break;
        case SDL_KEYUP:
            if (isKeyDown == ev->key.keysym.sym) {
                isKeyDown = SDLK_UNKNOWN;
                keyEvent(MR_KEY_RELEASE, ev->key.keysym.sym);
            }
            break;
        case SDL_MOUSEMOTION:
            if (isMouseDown) {
                event(MR_MOUSE_MOVE, ev->motion.x, ev->motion.y);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            isMouseDown = true;
            event(MR_MOUSE_DOWN, ev->motion.x, ev->motion.y);
            break;
        case SDL_MOUSEBUTTONUP:
            isMouseDown = false;
            event(MR_MOUSE_UP, ev->motion.x, ev->motion.y);
            break;
        default:
            break;
    }
}

void loop() {
    SDL_Event ev;
    bool isLoop = true;

#if defined(__EMSCRIPTEN__)
    host_timer_poll();
    while (SDL_PollEvent(&ev)) {
        handle_sdl_event(&ev, &isLoop);
        if (!isLoop) break;
    }
#else
    /* WaitEventTimeout: after start_dsm returns, deadline timers must still
     * advance (DOCUMENTED classic vmrp: MR_TIMER from host loop). */
    while (isLoop) {
        if (!SDL_WaitEventTimeout(&ev, 50)) {
            host_timer_poll(); /* also emits JJFB_POST_START_LOOP / TIMER_POLL */
            continue;
        }
        /* Keep scheduler visible even when SDL events arrive (mouse/key). */
        gwy_ext_obs_post_start_loop_tick((uint32_t)SDL_GetTicks());
        handle_sdl_event(&ev, &isLoop);
    }
#endif
}

int main(int argc, char *args[]) {
#ifdef __x86_64__
    printf("__x86_64__\n");
#elif __i386__
    printf("__i386__\n");
#endif

    printf("CODE_ADDRESS:0x%X, CODE_SIZE:0x%X\n", CODE_ADDRESS, CODE_SIZE);
    printf("STACK_ADDRESS:0x%X, STACK_SIZE:0x%X\n", STACK_ADDRESS, STACK_SIZE);
    printf("MEMORY_MANAGER_ADDRESS:0x%X, MEMORY_MANAGER_SIZE:0x%X\n", MEMORY_MANAGER_ADDRESS, MEMORY_MANAGER_SIZE);
    printf("START_ADDRESS:0x%X, END_ADDRESS:0x%X\n", START_ADDRESS, END_ADDRESS);
    printf("TOTAL_MEMORY:0x%X(%d)\n", TOTAL_MEMORY, TOTAL_MEMORY);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    /* E9B: software window (no OPENGL). OPENGL + UpdateWindowSurface often leaves
     * a white HWND while SDL_SaveBMP still dumps the software surface. */
    g_window_zoom = env_int("JJFB_WINDOW_ZOOM", 1);
    if (g_window_zoom < 1) g_window_zoom = 1;
    if (g_window_zoom > 8) g_window_zoom = 8;
    /* Clamp zoom so the window fits on the primary display (leave ~10% margin). */
    {
        SDL_DisplayMode dm;
        int max_z = g_window_zoom;
        int z;
        if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
            int max_w = (dm.w * 9) / 10;
            int max_h = (dm.h * 9) / 10;
            for (z = g_window_zoom; z >= 1; z--) {
                if (SCREEN_WIDTH * z <= max_w && SCREEN_HEIGHT * z <= max_h) {
                    max_z = z;
                    break;
                }
                max_z = 1;
            }
            if (max_z < g_window_zoom) {
                printf("[JJFB_WINDOW_ZOOM_CLAMP] requested=%d clamped=%d display=%dx%d "
                       "note=fit_primary_display evidence=OBSERVED\n",
                       g_window_zoom, max_z, dm.w, dm.h);
                fflush(stdout);
                g_window_zoom = max_z;
            }
        }
    }
    {
        int ww = SCREEN_WIDTH * g_window_zoom;
        int hh = SCREEN_HEIGHT * g_window_zoom;
        window = SDL_CreateWindow("vmrp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww,
                                  hh, 0);
    }
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    printf("[JJFB_WINDOW] kind=SDL_software flags=0 zoom=%d size=%dx%d "
           "note=no_OPENGL_for_UpdateWindowSurface evidence=OBSERVED\n",
           g_window_zoom, SCREEN_WIDTH * g_window_zoom, SCREEN_HEIGHT * g_window_zoom);
    fflush(stdout);
    guiPumpEvents();

    startVmrp();

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(loop, 0, 1);
#else
    loop();
#endif
    return 0;
}
