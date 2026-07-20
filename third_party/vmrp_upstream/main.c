#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "./header/bridge.h"
#include "./header/vmrp.h"
#include "./header/memory.h"
#include "./header/gwy_ext_obs_abi.h"

#ifdef _WIN32
// #ifdef __x86_64__
// #include "./windows/SDL2-2.0.10/x86_64-w64-mingw32/include/SDL2/SDL.h"
// #elif __i386__
#include "./windows/SDL2-2.0.10/i686-w64-mingw32/include/SDL2/SDL.h"
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

static SDL_Keycode isKeyDown = SDLK_UNKNOWN;

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

/* E8Z: blit sprite RGB565 with pitch=w into the SDL window (real mr_drawBitmap path). */
void guiDrawBitmapSprite(uint16_t *bmp, int32_t x, int32_t y, int32_t w, int32_t h) {
    static int s_e8z_saved = 0;
    SDL_Surface *surface;
    const char *shot;
    const char *before_path;
    const char *after_path;
    uint32_t black = 0, white = 0, other = 0, key = 0;
    int32_t j, i;
    if (!bmp || w <= 0 || h <= 0) return;
    surface = SDL_GetWindowSurface(window);
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
            Uint32 *p;
            if (xx < 0 || yy < 0 || xx >= SCREEN_WIDTH || yy >= SCREEN_HEIGHT)
                continue;
            color = bmp[(uint32_t)j * (uint32_t)w + (uint32_t)i];
            if (color == 0) black++;
            else if (color == 0xFFFFu) white++;
            else if (color == 0xF81Fu) key++;
            else other++;
            /* Present keyed pixels too for first-frame visibility (no invent). */
            p = (Uint32 *)(((Uint8 *)surface->pixels) + surface->pitch * yy) + xx;
            *p = SDL_MapRGB(surface->format, PIXEL565R(color), PIXEL565G(color), PIXEL565B(color));
        }
    }
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
    if (SDL_UpdateWindowSurface(window) != 0)
        printf("SDL_UpdateWindowSurface err\n");

    printf("[JJFB_E8Z_SPRITE_BLIT] x=%d y=%d w=%d h=%d black=%u white=%u other=%u key=%u "
           "evidence=OBSERVED\n",
           (int)x, (int)y, (int)w, (int)h, black, white, other, key);
    fflush(stdout);

    if (!s_e8z_saved && (other > 0 || (black > 0 && white > 0) || key > 0)) {
        if (SDL_SaveBMP(surface, after_path) == 0 && SDL_SaveBMP(surface, shot) == 0) {
            s_e8z_saved = 1;
            printf("[JJFB_FIRST_REAL_FRAME_REACHED] path=%s before=%s after=%s "
                   "x=%d y=%d w=%d h=%d black=%u white=%u other=%u key=%u "
                   "note=real_mrp_pixels_via_mr_drawBitmap evidence=OBSERVED\n",
                   shot, before_path, after_path, (int)x, (int)y, (int)w, (int)h, black,
                   white, other, key);
            fflush(stdout);
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

    window = SDL_CreateWindow("vmrp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    startVmrp();

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(loop, 0, 1);
#else
    loop();
#endif
    return 0;
}
