#include "./header/bridge.h"

#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "./mythroad/include/dsm.h"
#include "./header/fileLib.h"
#include "./header/memory.h"
#include "./header/vmrp.h"
#include "./header/debug.h"
#include "./header/network.h"

/* JJFB_VERBOSE=1 enables per-frame/diagnostic dumps. Default quiet so the
 * SDL window does not freeze under megabytes of fflush logging. */
static int jjfb_verbose_logs(void) {
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("JJFB_VERBOSE");
        cached = (e && e[0] == '1') ? 1 : 0;
    }
    return cached;
}

/* Forward decls used by early bridge stubs (defined with v37 bmp helpers). */
static void jjfb_note_memcpy_bmp(uc_engine *uc, uint32_t dst, uint32_t src, uint32_t n);
static void jjfb_note_drawbitmap_bmp(uc_engine *uc, uint32_t bmp, int32_t x, int32_t y,
                                     uint32_t w, uint32_t h);
static void jjfb_screen_blit_rgb565_key(const uint16_t *src, int src_w, int src_h,
                                        int dst_x, int dst_y, uint16_t key, int key_en);
static void jjfb_screen_blit_rgb565_key_pitch(const uint16_t *src, int copy_w, int copy_h,
                                              int src_pitch, int dst_x, int dst_y,
                                              uint16_t key, int key_en);
static void jjfb_screen_copy_guest_lcd(const uint16_t *src, int32_t x, int32_t y,
                                       int32_t w, int32_t h);
static void jjfb_debug_present_dirty(const char *from);
static void jjfb_bmp_req_set(const char *fullname, uint32_t name_ptr, uint32_t lr);
static void jjfb_bmp_clamp_blit_geom(uint32_t pixels, int32_t *iw, int32_t *ih,
                                     int32_t *imw, int32_t *ix, int32_t *iy,
                                     uint32_t n);
static void jjfb_present_flush(const char *from);
static void jjfb_host_draw_sky16(uint16_t ch, int32_t x, int32_t y, uint16_t color);
static void jjfb_screen_draw_ascii_fg(int32_t x, int32_t y, uint8_t ch, uint16_t fg);
static int jjfb_textbar_overlap_skip(int32_t y, int32_t h);
static void jjfb_screen_mark_dirty(int32_t x0, int32_t y0, int32_t x1, int32_t y1);

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
//////////////////////////////////////////////////////////////////////////////////////////
#ifdef LOG
#undef LOG
#endif

#ifdef DEBUG
#define LOG(format, ...) printf("   -> bridge: " format, ##__VA_ARGS__)
#else
#define LOG(format, ...)
#endif

#define SET_RET_V(ret)                        \
    {                                         \
        uint32_t _v = ret;                    \
        uc_reg_write(uc, UC_ARM_REG_R0, &_v); \
    }
typedef struct mr_c_function_P_t {
    uint8 *start_of_ER_RW;  // RW????
    uint32 ER_RW_Length;    // RW??
    int32 ext_type;         // ext??????1???ext??
    void *mrc_extChunk;     // ext??????????????
    int32 stack;            // stack shell 2008-2-28
} mr_c_function_P_t;

static void *mr_table;
static mr_c_function_P_t *mr_c_function_P;
static void *dsm_require_funcs;
static event_t *mr_c_event;  // ??mrc_event???????
static event_t *dsm_event;   // ?????????
static start_t *mr_start_dsm_param;
static uint32_t mr_extHelper_addr;

/* v53: keep the original jjfb.mrp untouched while satisfying the historical
 * mrc_loader request for cfunction.ext.  Guest mrc_loader/robotol registration
 * happens inside Mythroad's own _mr_c_function_table, not through the outer
 * bridge callback.  Track the guest log stream and patch the request literal at
 * ext_base+0xD4 after mrc_loader is registered. */
static int jjfb_v53_alias_applied;
static int jjfb_v53_robotol_loaded;
static uint32_t jjfb_v53_loader_helper;
static uint32_t jjfb_v53_robotol_helper;
static uint32_t jjfb_v53_last_guest_ext_base;
static uint32_t jjfb_v53_last_guest_ext_P;
static unsigned jjfb_v53_guest_ext_ordinal;

static int jjfb_bridge_env_true(const char *name) {
    const char *v = getenv(name);
    if (!v || !*v) return 0;
    return strcmp(v, "0") != 0 && strcmp(v, "off") != 0 &&
           strcmp(v, "false") != 0 && strcmp(v, "FALSE") != 0;
}

static int jjfb_patch_mrc_loader_member_alias(uc_engine *uc, uint32_t ext_base) {
    static const char old_name[] = "cfunction.ext";
    static const char new_name[sizeof(old_name)] = "robotol.ext";
    const uint32_t expected = ext_base + 0xD4;
    uint32_t lo, hi, addr;
    char probe[sizeof(old_name)];

    if (!uc || ext_base < CODE_ADDRESS || ext_base >= END_ADDRESS) return 0;

    /* Canonical mrc_loader.ext: request literal is at file/ext offset 0xD4. */
    if (expected >= CODE_ADDRESS && expected + sizeof(probe) <= END_ADDRESS &&
        uc_mem_read(uc, expected, probe, sizeof(probe)) == UC_ERR_OK &&
        memcmp(probe, old_name, sizeof(old_name)) == 0) {
        if (uc_mem_write(uc, expected, new_name, sizeof(new_name)) == UC_ERR_OK) {
            printf("[JJFB_MRP_ALIAS] patched ext_base=0x%X literal=0x%X "
                   "request=cfunction.ext target=robotol.ext method=ext_base_0xD4\n",
                   ext_base, expected);
            fflush(stdout);
            return 1;
        }
    }

    /* Defensive fallback stays within the 232-byte loader image neighborhood. */
    lo = ext_base;
    hi = ext_base + 0x400;
    if (hi > END_ADDRESS - sizeof(probe)) hi = END_ADDRESS - sizeof(probe);
    for (addr = lo; addr <= hi; addr++) {
        if (uc_mem_read(uc, addr, probe, sizeof(probe)) != UC_ERR_OK) continue;
        if (memcmp(probe, old_name, sizeof(old_name)) != 0) continue;
        if (uc_mem_write(uc, addr, new_name, sizeof(new_name)) != UC_ERR_OK) break;
        printf("[JJFB_MRP_ALIAS] patched ext_base=0x%X literal=0x%X "
               "request=cfunction.ext target=robotol.ext method=ext_base_scan\n",
               ext_base, addr);
        fflush(stdout);
        return 1;
    }

    printf("[JJFB_MRP_ALIAS] patch_miss ext_base=0x%X expected=0x%X "
           "request=cfunction.ext\n", ext_base, expected);
    fflush(stdout);
    return 0;
}

int jjfb_mrp_alias_applied(void) { return jjfb_v53_alias_applied; }
int jjfb_robotol_ext_loaded(void) { return jjfb_v53_robotol_loaded; }
uint32_t jjfb_current_ext_helper(void) { return mr_extHelper_addr; }
uint32_t jjfb_robotol_ext_helper(void) { return jjfb_v53_robotol_helper; }

static const char MUTEX_LOCK_FAIL[] = "mutex lock fail";
static const char MUTEX_UNLOCK_FAIL[] = "mutex unlock fail";
static pthread_mutex_t mutex;

static void runCode(uc_engine *uc, uint32_t startAddr, uint32_t stopAddr, bool isThumb);

////////////////////////////////////////////////////////////////////////////////////////////////////

static void br__mr_c_function_new(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T__mr_c_function_new)(MR_C_FUNCTION f, int32 len);
    uint32_t p_f, p_len;
    uc_reg_read(uc, UC_ARM_REG_R0, &p_f);
    uc_reg_read(uc, UC_ARM_REG_R1, &p_len);
    static int jjfb_c_function_new_count = 0;
    jjfb_c_function_new_count++;
    printf("[JJFB_LOADER] _mr_c_function_new #%d helper=%p len=%u\n",
           jjfb_c_function_new_count, (void *)(uintptr_t)p_f, p_len);
    printf("ext call %s(0x%X[%u], 0x%X[%u])\n", o->name, p_f, p_f, p_len, p_len);
    dumpREG(uc);

    mr_extHelper_addr = p_f;
    mr_c_function_P = my_mallocExt(p_len);
    memset(mr_c_function_P, 0, p_len);

    uint32_t v = toMrpMemAddr(mr_c_function_P);
    uc_mem_write(uc, CODE_ADDRESS + 4, &v, 4);
    SET_RET_V(MR_SUCCESS);
}

static void br_mr_malloc(BridgeMap *o, uc_engine *uc) {
    // typedef void* (*T_mr_malloc)(uint32 len);
    uint32_t len;
    uc_reg_read(uc, UC_ARM_REG_R0, &len);
    void *p = my_mallocExt(len);
    if (p) {
        uint32_t ret = toMrpMemAddr(p);
        LOG("ext call %s(0x%X[%u]) ret=0x%X[%u]\n", o->name, len, len, ret, ret);
        SET_RET_V(ret);
        return;
    }
    SET_RET_V((uint32_t)NULL);
}

static void br_mr_free(BridgeMap *o, uc_engine *uc) {
    // typedef void  (*T_mr_free)(void* p, uint32 len);
    uint32_t p, len;
    uc_reg_read(uc, UC_ARM_REG_R0, &p);
    uc_reg_read(uc, UC_ARM_REG_R1, &len);

    LOG("ext call %s(0x%X[%u], 0x%X[%u])\n", o->name, p, p, len, len);
    my_freeExt(getMrpMemPtr(p));
}

static void br_memcpy(BridgeMap *o, uc_engine *uc) {
    //  void* (*T_memcpy)(void* dst, const void* src, int n);
    uint32_t dst, src, n;
    uc_reg_read(uc, UC_ARM_REG_R0, &dst);
    uc_reg_read(uc, UC_ARM_REG_R1, &src);
    uc_reg_read(uc, UC_ARM_REG_R2, &n);
    jjfb_note_memcpy_bmp(uc, dst, src, n);
    SET_RET_V((uint32_t)memcpy(getMrpMemPtr(dst), getMrpMemPtr(src), n));
}

static void br_memset(BridgeMap *o, uc_engine *uc) {
    // void* (*T_memset)(void* s, int c, int n);
    uint32_t dst, value, n;
    uc_reg_read(uc, UC_ARM_REG_R0, &dst);
    uc_reg_read(uc, UC_ARM_REG_R1, &value);
    uc_reg_read(uc, UC_ARM_REG_R2, &n);
    SET_RET_V((uint32_t)memset(getMrpMemPtr(dst), value, n));
}

static void br_strlen(BridgeMap *o, uc_engine *uc) {
    uint32_t s;
    (void)o;
    uc_reg_read(uc, UC_ARM_REG_R0, &s);
    if (!s || !getMrpMemPtr(s)) {
        SET_RET_V(0);
        return;
    }
    SET_RET_V((uint32_t)strlen((const char *)getMrpMemPtr(s)));
}

// ???????????????n=0
static uint32_t getArg(uc_engine *uc, uint32_t n) {
    uint32_t v;
    if (n <= 3) {  // ????????????
        uc_reg_read(uc, UC_ARM_REG_R0 + n, &v);
        return v;
    }

    uint32_t addr;
    uc_reg_read(uc, UC_ARM_REG_SP, &addr);

    addr += (n - 4) * 4;
    uc_mem_read(uc, addr, &v, 4);
    return v;
}

/* mrc_refreshScreen / DispUpEx ultimately hit this (EXT helper drawBitmap). */
/* Platform: guest mr_platDrawChar was UNIMPL (NULL bridge) → no status text.
 * Draw guest screenBuf + mirror into host565 for coalesce present. */
static void br_mr_platDrawChar(BridgeMap *o, uc_engine *uc) {
    uint32_t ch = 0, x = 0, y = 0, color = 0;
    static uint32_t n;
    uint16_t fg;
    (void)o;
    uc_reg_read(uc, UC_ARM_REG_R0, &ch);
    uc_reg_read(uc, UC_ARM_REG_R1, &x);
    uc_reg_read(uc, UC_ARM_REG_R2, &y);
    uc_reg_read(uc, UC_ARM_REG_R3, &color);
    n++;
    /* Mirror into host565 (coalesce present). Guest DispUpEx path is secondary;
     * splash blits go through _DrawBitmap → host565. */
    fg = (uint16_t)(color & 0xFFFF);
    if ((ch & 0xFFFF) < 128)
        jjfb_screen_draw_ascii_fg((int32_t)x, (int32_t)y, (uint8_t)(ch & 0xFF), fg);
    else
        jjfb_host_draw_sky16((uint16_t)(ch & 0xFFFF), (int32_t)x, (int32_t)y, fg);
    if (n <= 12 || (n % 80) == 0) {
        printf("[JJFB_V86_CHAR] platDrawChar #%u ch=0x%X @%d,%d color=0x%X\n",
               n, ch & 0xFFFF, (int)x, (int)y, color);
        fflush(stdout);
    }
}

static void br_mr_drawBitmap(BridgeMap *o, uc_engine *uc) {
    // typedef void (*T_mr_drawBitmap)(uint16* bmp, int16 x, int16 y, uint16 w, uint16 h);
    uint32_t bmp, x, y, w, h;
    static uint32_t jjfb_draw_n;

    uc_reg_read(uc, UC_ARM_REG_R0, &bmp);
    uc_reg_read(uc, UC_ARM_REG_R1, &x);
    uc_reg_read(uc, UC_ARM_REG_R2, &y);
    uc_reg_read(uc, UC_ARM_REG_R3, &w);

    uint32_t sp;
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_mem_read(uc, sp, &h, 4);

    jjfb_draw_n++;
    if (jjfb_draw_n <= 8 || (jjfb_draw_n % 60) == 0) {
        printf("[JJFB_DRAW] drawBitmap #%u bmp=0x%X %d,%d %ux%u\n",
               jjfb_draw_n, bmp, (int)x, (int)y, w, h);
        fflush(stdout);
    }
    jjfb_note_drawbitmap_bmp(uc, bmp, (int32_t)x, (int32_t)y, w, h);
    LOG("ext call %s(0x%X, %d, %d, %u, %u)\n", o->name, bmp, x, y, w, h);
    if (!bmp || !w || !h || !getMrpMemPtr(bmp)) {
        if (jjfb_draw_n <= 8)
            printf("[JJFB_DRAW] drawBitmap skip (null/empty)\n");
        return;
    }
    /* v76: refresh guest LCD dirty rect into host buffer + coalesce present.
     * guiDrawBitmap(guest) uses SCREEN_WIDTH pitch + immediate SDL_Update →
     * flicker and twist when guest ptr is not the full LCD buffer. */
    jjfb_screen_copy_guest_lcd((uint16_t *)getMrpMemPtr(bmp),
                               (int32_t)(int16_t)(x & 0xFFFF),
                               (int32_t)(int16_t)(y & 0xFFFF),
                               (int32_t)(w & 0xFFFF),
                               (int32_t)(h & 0xFFFF));
}

/*
 * mr_table._DrawBitmap — used by robotol via ERW+0x150C (2EC6B8) after
 * plat 0x10113 resolves 11F02/03/04. Platform blit, not fake UI.
 *
 * Two ABIs observed:
 *   classic:  (p, x, y, w, h, rop, trans, sx, sy, mw)
 *   2EC6B8:   (p, x, y, mw, w, h, rop, flags, src_x, trans)
 *     r3=ldrh(obj+8)=mw, sp0=w, sp4=h — when mw==w, classic mis-reads h=w
 *     (square OOB). Detect: stack arg0 == r3 → wrapper ABI.
 */
static void br__DrawBitmap(BridgeMap *o, uc_engine *uc) {
    uint32_t p = 0, x = 0, y = 0, r3w = 0;
    uint32_t a4 = 0, a5 = 0, a6 = 0, a7 = 0, a8 = 0, a9 = 0;
    int32_t ix, iy, iw, ih, isx, isy, imw;
    uint32_t rop = 0, trans = 0;
    uint16_t *src;
    uint16_t key;
    int key_en;
    int wrapper_abi;
    static uint32_t n;
    (void)o;
    uc_reg_read(uc, UC_ARM_REG_R0, &p);
    uc_reg_read(uc, UC_ARM_REG_R1, &x);
    uc_reg_read(uc, UC_ARM_REG_R2, &y);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3w);
    a4 = getArg(uc, 4);
    a5 = getArg(uc, 5);
    a6 = getArg(uc, 6);
    a7 = getArg(uc, 7);
    a8 = getArg(uc, 8);
    a9 = getArg(uc, 9);
    /* 2EC6B8 full-blit: mw(r3)==w(sp0); classic has h(sp0) usually != w(r3). */
    wrapper_abi = ((a4 & 0xFFFF) == (r3w & 0xFFFF) && (a4 & 0xFFFF) != 0);
    if (wrapper_abi) {
        imw = (int16_t)(r3w & 0xFFFF);
        iw = (int16_t)(a4 & 0xFFFF);
        ih = (int16_t)(a5 & 0xFFFF);
        rop = a6;
        trans = a9; /* 2EC6B8 stores trans const at sp+0x14 */
        isx = (int16_t)(a8 & 0xFFFF); /* src_x */
        isy = 0;
    } else {
        iw = (int16_t)(r3w & 0xFFFF);
        ih = (int16_t)(a4 & 0xFFFF);
        rop = a5;
        trans = a6;
        isx = (int16_t)(a7 & 0xFFFF);
        isy = (int16_t)(a8 & 0xFFFF);
        imw = (int16_t)(a9 & 0xFFFF);
    }
    ix = (int16_t)(x & 0xFFFF);
    iy = (int16_t)(y & 0xFFFF);
    if (imw <= 0) imw = iw;
    if (!p || iw <= 0 || ih <= 0 || !getMrpMemPtr(p))
        return;
    src = (uint16_t *)getMrpMemPtr(p);
    if (!src) return;
    /* v76: clamp to real BMP cache w/h (guest often passes square OOB h). */
    jjfb_bmp_clamp_blit_geom(p, &iw, &ih, &imw, &ix, &iy, n);
    /* Same-tick textbar rows at y=176 and y=198 (h=30) overlap — looks like
     * duplicated wood chrome. Skip heavy overlap; JJFB_TEXTBAR_DEDUP=0 to keep. */
    if (iw == 120 && ih == 30 && jjfb_textbar_overlap_skip(iy, ih))
        return;
    n++;
    /* Do NOT log every wrapper_abi blit — that froze the UI under log flood. */
    if (jjfb_verbose_logs() && (n <= 16 || (n % 80) == 0)) {
        printf("[JJFB_DRAW] _DrawBitmap #%u p=0x%X %d,%d %dx%d rop=0x%X key=0x%X "
               "sx=%d sy=%d mw=%d abi=%s\n",
               n, p, ix, iy, iw, ih, rop, trans & 0xFFFF, isx, isy, imw,
               wrapper_abi ? "2EC6B8" : "classic");
        fflush(stdout);
    }
    if (isy != 0 || isx != 0)
        src += (int)isy * imw + isx;
    key = (uint16_t)(trans & 0xFFFF);
    /* Only treat real RGB565 keys (e.g. 0xF81F); small values are rop flags. */
    key_en = (key == 0xF81F) || (key >= 0x100);
    if (!key_en && iw > 1 && ih > 1) {
        /* Fallback: magenta corners on splash assets. */
        if (src[0] == 0xF81F ||
            src[(imw > 0 ? imw : iw) - 1] == 0xF81F) {
            key = 0xF81F;
            key_en = 1;
        }
    }
    /* Pitch is mw (imw); using crop width as pitch looks like encrypted twist. */
    jjfb_screen_blit_rgb565_key_pitch(src, iw, ih, imw > 0 ? imw : iw,
                                      ix, iy, key, key_en);
    jjfb_debug_present_dirty("_DrawBitmap");
}

/* mr_table._DispUpEx — refresh dirty rect from host RGB565 to SDL.
 * v71: restore real present (was no-op); guest rarely calls this, timer flush
 * is the backup coalesce point. */
static void br__DispUpEx(BridgeMap *o, uc_engine *uc) {
    uint32_t x, y, w, h;
    static uint32_t n;
    (void)o;
    uc_reg_read(uc, UC_ARM_REG_R0, &x);
    uc_reg_read(uc, UC_ARM_REG_R1, &y);
    uc_reg_read(uc, UC_ARM_REG_R2, &w);
    uc_reg_read(uc, UC_ARM_REG_R3, &h);
    n++;
    if ((int32_t)w <= 0 || (int32_t)h <= 0) {
        x = 0;
        y = 0;
        w = (uint32_t)SCREEN_WIDTH;
        h = (uint32_t)SCREEN_HEIGHT;
    }
    if (n <= 8 || (n % 60) == 0) {
        printf("[JJFB_DRAW] DispUpEx #%u %u,%u %ux%u\n", n, x, y, w, h);
        fflush(stdout);
    }
    (void)x; (void)y; (void)w; (void)h;
    jjfb_present_flush("DispUpEx");
    SET_RET_V(MR_SUCCESS);
}

/* Implemented later: write DrawRect into host RGB565 + DEBUG_PRESENT. */
static void jjfb_host_drawrect_present(int32_t x, int32_t y, int32_t w, int32_t h,
                                       uint32_t r, uint32_t g, uint32_t b);

static void br_DrawRect(BridgeMap *o, uc_engine *uc) {
    uint32_t x, y, w, h, r, g, b;
    int32_t ix, iy, iw, ih;
    static uint32_t n;
    (void)o;
    uc_reg_read(uc, UC_ARM_REG_R0, &x);
    uc_reg_read(uc, UC_ARM_REG_R1, &y);
    uc_reg_read(uc, UC_ARM_REG_R2, &w);
    uc_reg_read(uc, UC_ARM_REG_R3, &h);
    r = getArg(uc, 4);
    g = getArg(uc, 5);
    b = getArg(uc, 6);
    /* Mythroad DrawRect uses signed 16-bit geometry; guest often passes 0xFFFFFFF8. */
    ix = (int16_t)(x & 0xFFFF);
    iy = (int16_t)(y & 0xFFFF);
    iw = (int16_t)(w & 0xFFFF);
    ih = (int16_t)(h & 0xFFFF);
    n++;
    if (n <= 8 || (n % 60) == 0) {
        printf("[JJFB_DRAW] DrawRect #%u %d,%d %dx%d rgb=%u,%u,%u (raw h=0x%X)\n",
               n, ix, iy, iw, ih, r, g, b, h);
        fflush(stdout);
    }
    /* Guest DrawRect ??host RGB565 buffer (game content, not overlay). */
    if (iw > 0 && ih > 0 && iw < 4096 && ih < 4096)
        jjfb_host_drawrect_present(ix, iy, iw, ih, r, g, b);
}

/* Mythroad: mr_registerAPP(uint8 *p, int32 len, int32 index) ??mr_m0_files[index]=p.
 * Always return MR_SUCCESS(0); do not fail (gates later load paths). */
#define JJFB_M0_MAX 50
static uint32_t jjfb_m0_files[JJFB_M0_MAX];
static uint32_t jjfb_m0_lens[JJFB_M0_MAX];

static void br_mr_registerAPP(BridgeMap *o, uc_engine *uc) {
    uint32_t p, len, index, r3, sp, sp0 = 0, sp4 = 0;
    (void)o;
    uc_reg_read(uc, UC_ARM_REG_R0, &p);
    uc_reg_read(uc, UC_ARM_REG_R1, &len);
    uc_reg_read(uc, UC_ARM_REG_R2, &index);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_mem_read(uc, sp, &sp0, 4);
    uc_mem_read(uc, sp + 4, &sp4, 4);

    printf("[JJFB_REG] mr_registerAPP r0=0x%X r1=0x%X r2=0x%X r3=0x%X sp0=0x%X sp4=0x%X\n",
           p, len, index, r3, sp0, sp4);
    fflush(stdout);

    if (p && getMrpMemPtr(p)) {
        uint8_t buf[64];
        uint32_t i, n = 64;
        int printable = 1;
        memcpy(buf, getMrpMemPtr(p), n);
        printf("[JJFB_REG] dump p@0x%X:", p);
        for (i = 0; i < n; i++) printf(" %02X", buf[i]);
        printf("\n");
        for (i = 0; i < n && buf[i]; i++) {
            if (buf[i] < 0x20 || buf[i] > 0x7e) { printable = 0; break; }
        }
        if (printable && buf[0])
            printf("[JJFB_REG] p str=\"%s\"\n", (char *)buf);
        fflush(stdout);
    } else {
        printf("[JJFB_REG] p=0x%X unmapped or null\n", p);
        fflush(stdout);
    }
    if (len > 0x1000 && getMrpMemPtr(len)) {
        uint8_t buf[32];
        uint32_t i;
        memcpy(buf, getMrpMemPtr(len), 32);
        printf("[JJFB_REG] r1 looks like ptr@0x%X:", len);
        for (i = 0; i < 32; i++) printf(" %02X", buf[i]);
        printf("\n");
        fflush(stdout);
    }

    if (index < JJFB_M0_MAX) {
        jjfb_m0_files[index] = p;
        jjfb_m0_lens[index] = len;
        printf("[JJFB_REG] m0_files[%u]=0x%X len=0x%X -> ret=0\n", index, p, len);
    } else {
        printf("[JJFB_REG] index %d OOR -> ret=0 anyway\n", (int32_t)index);
    }
    fflush(stdout);
    SET_RET_V(MR_SUCCESS);
}

/* First-screen / splash keywords (ASCII; GBK scanned separately in text path). */
static int jjfb_is_firstscreen_name(const char *s) {
    if (!s || !s[0]) return 0;
    if (strstr(s, "slogo") || strstr(s, "logo") || strstr(s, "title") ||
        strstr(s, "loading") || strstr(s, "login") || strstr(s, "splash") ||
        strstr(s, "server") || strstr(s, "background") || strstr(s, "bg_") ||
        strstr(s, "start") || strstr(s, "enter") || strstr(s, "button") ||
        strstr(s, "hint") || strstr(s, "reglogin") || strstr(s, "dload") ||
        strstr(s, "loadingbar") || strstr(s, "textbar") ||
        strstr(s, "show1") || strstr(s, "show2") || strstr(s, "show3") ||
        strstr(s, "downimage") ||
        (strstr(s, "bar!") && strstr(s, ".bmp")))
        return 1;
    return 0;
}

static void jjfb_note_firstscreen(const char *kind, const char *detail) {
    static uint32_t n;
    n++;
    if (n <= 64 || (n % 50) == 0) {
        printf("[JJFB_FIRST_SCREEN] #%u %s %s\n", n, kind, detail ? detail : "");
        fflush(stdout);
    }
}

/* ---- v37/v38: name!W!H.bmp ??0x10134 (W*H*2) ??RGB565 ??blit ---- */
typedef struct JjfbBmpReq {
    char name[128];
    int w;
    int h;
    int bytes; /* w * h * 2 */
    uint32_t caller_lr;
    uint32_t name_ptr;
    uint32_t guest_pixels;
    uint32_t guest_object; /* optional bitmap object (v38 mode B) */
    int valid;
} JjfbBmpReq;

/* Chrome dumps show: +0 size, +4 pixels, +8 w, +0A h, +10 flag=1, +18 size */
typedef struct {
    uint32_t size_bytes;
    uint32_t pixels;
    int16_t w;
    int16_t h;
    uint32_t pad0;
    uint32_t flag;
    uint32_t pad1;
    uint32_t size2;
} JjfbBmpObjHdr;

static JjfbBmpReq jjfb_bmp_last;
static JjfbBmpReq jjfb_bmp_cache[48];
static int jjfb_bmp_cache_n;
static int jjfb_310bb4_host_blit = 0; /* v79: OFF — dual host+DrawBitmap stacks UI */

static JjfbBmpReq *jjfb_bmp_cache_find_pixels(uint32_t px);
static JjfbBmpReq *jjfb_bmp_cache_find_object(uint32_t obj);
static int jjfb_is_firstscreen_name(const char *s);

/* Forward: defined later with ER_RW tracking. */
static uint32_t jjfb_guest_ext_erw;

/* v38/v39: guest_pixels / object / slogo consumers */
static uc_hook jjfb_px_read_hooks[32];
static int jjfb_px_read_hook_n;
static uint32_t jjfb_px_watch_base[32];
static char jjfb_px_watch_name[32][64];
static char jjfb_px_watch_which[32][16];
static uint32_t jjfb_px_read_hits;
static uint32_t jjfb_obj_read_hits;
static uc_hook jjfb_slogo_read_hook;
static int jjfb_slogo_read_hook_installed;
/* Real slogo literal in robotol (was wrongly 0x314EF4 = speedlight!). */
#define JJFB_SLOGO_STR_VA 0x3142DC
#define JJFB_LOADINGBAR_STR_VA 0x314314
/* Startup-check UI strings (GBK) inside robotol guest image ??v46 watches. */
#define JJFB_STR_CHECK_NET_VA     0x313B30 /* ???????????*/
#define JJFB_STR_CONN_FAIL_VA     0x313C48 /* ???????? */
#define JJFB_STR_CONNECTING_VA    0x313C5C /* ????????*/
#define JJFB_STR_DL_RES_VA        0x313C74 /* ???????????????*/
#define JJFB_STR_CHECK_UPDATE_VA  0x313CF4 /* ???????*/
#define JJFB_STR_LOGGING_VA       0x314204 /* ????,????*/
#define JJFB_ERW_SPLASH_CNT_AC8 0xAC8 /* splash slogo-path gate: must be >0 */
#define JJFB_ERW_PROGRESS_BA0   0xBA0 /* splash progress: +0x20=bar +0x24=textbar +0x28=loadingbar +0x2C=count */
#define JJFB_ERW_STATE_EARLY    0x8D0 /* ui_mode/state (same as JJFB_ERW_UI_MODE later) */

static int jjfb_ac8_slogo_once_armed;
static int jjfb_ac8_slogo_once_done;
static int jjfb_ac8_pulse_ticks; /* JJFB_AC8_PULSE_TICKS: hold AC8=1 then release */
/* TEMPORARY: JJFB_SKIP_NET_LOGIN arms one 2EFC40 divert (def near splash cov). */
static int jjfb_skip_net_arm_2efc;
static int jjfb_ac8_pulse_armed;
static uc_hook jjfb_ac8_hook_r;
static uc_hook jjfb_ac8_hook_w;
static int jjfb_ac8_watch_installed;
static uc_hook jjfb_prog_hook_w;
static uc_hook jjfb_prog_hook_r;
static uc_hook jjfb_uimode_hook_w;
static int jjfb_state_watch_installed;
static int jjfb_startup_str_watch_installed;
static uint32_t jjfb_progress_nudge_val; /* last host-written progress for scan */
static int jjfb_progress_drew; /* saw DRAW? in progress loop */
static unsigned jjfb_handler_tick; /* timer ticks; defined early for startup probes */
static uint32_t jjfb_10140_calls;
static uint32_t jjfb_10140_handler_lr;
static int jjfb_erw_band_watch_installed;
static uc_hook jjfb_erw_band_hook;
static int jjfb_xref_scan_done;

/* Forward decls ??used by v47 progress driver before definition. */
static void jjfb_host_blit_req(uc_engine *uc, JjfbBmpReq *req, int dst_x, int dst_y,
                               const char *tag);
static void jjfb_debug_present_dirty(const char *from);

static uc_hook jjfb_obj_bind_hook;
static int jjfb_obj_bind_hook_installed;
static uc_hook jjfb_splash_flow_hook;
static int jjfb_splash_flow_hook_installed;
static uint32_t jjfb_splash_flow_hits;
static uint32_t jjfb_splash_last_pc;

static JjfbBmpReq *jjfb_bmp_cache_find_object(uint32_t obj) {
    int i;
    if (!obj) return NULL;
    for (i = jjfb_bmp_cache_n - 1; i >= 0; i--) {
        if (jjfb_bmp_cache[i].valid && jjfb_bmp_cache[i].guest_object == obj)
            return &jjfb_bmp_cache[i];
    }
    return NULL;
}

static void jjfb_log_regs_short(uc_engine *uc, uint32_t *pc, uint32_t *lr,
                                uint32_t *r0, uint32_t *r1, uint32_t *r2, uint32_t *r3,
                                uint32_t *r4, uint32_t *r5, uint32_t *r6, uint32_t *r7,
                                uint32_t *sp) {
    if (pc) uc_reg_read(uc, UC_ARM_REG_PC, pc);
    if (lr) uc_reg_read(uc, UC_ARM_REG_LR, lr);
    if (r0) uc_reg_read(uc, UC_ARM_REG_R0, r0);
    if (r1) uc_reg_read(uc, UC_ARM_REG_R1, r1);
    if (r2) uc_reg_read(uc, UC_ARM_REG_R2, r2);
    if (r3) uc_reg_read(uc, UC_ARM_REG_R3, r3);
    if (r4) uc_reg_read(uc, UC_ARM_REG_R4, r4);
    if (r5) uc_reg_read(uc, UC_ARM_REG_R5, r5);
    if (r6) uc_reg_read(uc, UC_ARM_REG_R6, r6);
    if (r7) uc_reg_read(uc, UC_ARM_REG_R7, r7);
    if (sp) uc_reg_read(uc, UC_ARM_REG_SP, sp);
}

static void jjfb_hook_px_mem_read(uc_engine *uc, uc_mem_type type,
                                 uint64_t address, int size, int64_t value,
                                 void *user_data) {
    uint32_t pc = 0, lr = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0, sp = 0;
    int idx = (int)(intptr_t)user_data;
    const char *nm = (idx >= 0 && idx < jjfb_px_read_hook_n) ? jjfb_px_watch_name[idx] : "?";
    const char *which = (idx >= 0 && idx < jjfb_px_read_hook_n) ? jjfb_px_watch_which[idx] : "?";
    uint32_t prog_addr = 0;
    (void)type; (void)value; (void)size;
    /* BA0+0x2C is progress count ??never label as loadingbar parent_slot. */
    if (jjfb_guest_ext_erw)
        prog_addr = jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C;
    if (prog_addr && (uint32_t)address == prog_addr) {
        static uint32_t npr;
        npr++;
        jjfb_log_regs_short(uc, &pc, &lr, &r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7, &sp);
        if (npr <= 40 || (npr % 50) == 0) {
            printf("[JJFB_PROGRESS_COUNT_READ] #%u addr=0x%X value=0x%X pc=0x%X lr=0x%X "
                   "idx_r6=%u\n",
                   npr, (uint32_t)address, (uint32_t)value, pc, lr, r6);
            fflush(stdout);
        }
        return;
    }
    jjfb_px_read_hits++;
    jjfb_obj_read_hits++;
    if (jjfb_obj_read_hits > 64 && (jjfb_obj_read_hits % 100) != 0)
        return;
    jjfb_log_regs_short(uc, &pc, &lr, &r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7, &sp);
    printf("[JJFB_BMP_OBJ_READ] #%u name=%s which=%s addr=0x%X pc=0x%X lr=0x%X "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X sp=0x%X\n",
           jjfb_obj_read_hits, nm, which, (uint32_t)address, pc, lr,
           r0, r1, r2, r3, r4, r5, r6, r7, sp);
    fflush(stdout);
}

static int jjfb_add_range_read_watch(uc_engine *uc, uint32_t begin, uint32_t end,
                                     const char *name, const char *which) {
    uc_err err;
    int idx;
    if (!uc || !begin || end < begin || !name || !which) return -1;
    /* Unicorn UC_HOOK_MEM_* ranges are INCLUSIVE on both ends.
     * Callers historically passed [begin, begin+4) intending 4 bytes;
     * convert half-open to inclusive when end==begin+4. */
    if (end == begin + 4)
        end = begin + 3;
    if (jjfb_px_read_hook_n >= (int)(sizeof(jjfb_px_read_hooks) / sizeof(jjfb_px_read_hooks[0])))
        return -1;
    idx = jjfb_px_read_hook_n;
    err = uc_hook_add(uc, &jjfb_px_read_hooks[idx], UC_HOOK_MEM_READ,
                      jjfb_hook_px_mem_read, (void *)(intptr_t)idx, begin, end);
    if (err) {
        printf("[JJFB_BMP_OBJ_READ] watch fail name=%s which=%s [0x%X,0x%X] err=%u\n",
               name, which, begin, end, err);
        fflush(stdout);
        return -1;
    }
    jjfb_px_watch_base[idx] = begin;
    strncpy(jjfb_px_watch_name[idx], name, sizeof(jjfb_px_watch_name[idx]) - 1);
    strncpy(jjfb_px_watch_which[idx], which, sizeof(jjfb_px_watch_which[idx]) - 1);
    jjfb_px_read_hook_n++;
    printf("[JJFB_BMP_OBJ_READ] watching name=%s which=%s [0x%X,0x%X]\n",
           name, which, begin, end);
    fflush(stdout);
    return idx;
}

static void jjfb_hook_slogo_mem_read(uc_engine *uc, uc_mem_type type,
                                    uint64_t address, int size, int64_t value,
                                    void *user_data) {
    uint32_t pc = 0, lr = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, mode = 0;
    static uint32_t n;
    const char *tag = user_data ? "LOADINGBAR_STR" : "SLOGO_STR";
    (void)type; (void)value; (void)size;
    n++;
    if (n > 40 && (n % 50) != 0) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    if (jjfb_guest_ext_erw)
        uc_mem_read(uc, jjfb_guest_ext_erw + 0x8D0, &mode, 4);
    printf("[JJFB_SLOGO_XREF] #%u tag=%s addr=0x%X pc=0x%X lr=0x%X "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X ui_mode=0x%X\n",
           n, tag, (uint32_t)address, pc, lr, r0, r1, r2, r3, mode);
    fflush(stdout);
}

static void jjfb_watch_pixels(uc_engine *uc, uint32_t addr, const char *name) {
    if (!uc || !addr || !name) return;
    jjfb_add_range_read_watch(uc, addr, addr + 4, name, "pixels");
}

static void jjfb_watch_splash_object(uc_engine *uc, JjfbBmpReq *rq, uint32_t obj) {
    const char *flag_env;
    uint32_t flag_off = 0, flag_val = 1, words[8];
    int i;
    if (!uc || !rq || !obj || !getMrpMemPtr(obj)) return;
    rq->guest_object = obj;
    /* Observe only — do not rewrite guest object w/h (that is game layout). */
    if (uc_mem_read(uc, obj, words, sizeof(words)) == UC_ERR_OK) {
        printf("[JJFB_BMP_OBJ] name=%s obj=0x%X", rq->name, obj);
        for (i = 0; i < 8; i++) printf(" +%02X=0x%X", i * 4, words[i]);
        printf("\n");
        fflush(stdout);
    }
    jjfb_add_range_read_watch(uc, obj, obj + 4, rq->name, "obj");
    jjfb_add_range_read_watch(uc, obj + 4, obj + 8, rq->name, "obj+4");
    if (rq->guest_pixels)
        jjfb_add_range_read_watch(uc, rq->guest_pixels, rq->guest_pixels + 4, rq->name, "pixels");
    flag_env = getenv("JJFB_OBJ_SET_FLAG");
    if (flag_env && flag_env[0]) {
        flag_off = (uint32_t)strtoul(flag_env, NULL, 0);
        if (flag_off == 0x0C || flag_off == 0x10 || flag_off == 0x14) {
            const char *fv = getenv("JJFB_OBJ_SET_FLAG_VAL");
            if (fv && fv[0]) flag_val = (uint32_t)strtoul(fv, NULL, 0);
            uc_mem_write(uc, obj + flag_off, &flag_val, 4);
            printf("[JJFB_OBJ_FLAG] name=%s set obj+0x%X = 0x%X\n", rq->name, flag_off, flag_val);
            fflush(stdout);
        }
    }
}

static uc_hook jjfb_ptr_store_hook;
static uc_hook jjfb_ptr_store_hook_erw;
static int jjfb_ptr_store_hook_installed;
static uint32_t jjfb_ptr_store_hits;
static uint32_t jjfb_parent_store_hits;

static void jjfb_hook_ptr_store(uc_engine *uc, uc_mem_type type,
                               uint64_t address, int size, int64_t value,
                               void *user_data) {
    JjfbBmpReq *rq;
    uint32_t pc = 0, lr = 0, sp = 0, obj = 0;
    (void)type; (void)user_data;
    if (size != 4 || value == 0) return;

    rq = jjfb_bmp_cache_find_object((uint32_t)value);
    if (rq && jjfb_is_firstscreen_name(rq->name)) {
        jjfb_parent_store_hits++;
        if (jjfb_parent_store_hits <= 40 || (jjfb_parent_store_hits % 50) == 0) {
            jjfb_log_regs_short(uc, &pc, &lr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &sp);
            printf("[JJFB_PARENT_STORE] #%u name=%s obj=0x%X -> slot@0x%X pc=0x%X lr=0x%X sp=0x%X\n",
                   jjfb_parent_store_hits, rq->name, (uint32_t)value, (uint32_t)address, pc, lr, sp);
            fflush(stdout);
            jjfb_add_range_read_watch(uc, (uint32_t)address, (uint32_t)address + 4, rq->name, "parent_slot");
        }
        return;
    }

    rq = jjfb_bmp_cache_find_pixels((uint32_t)value);
    if (!rq || !jjfb_is_firstscreen_name(rq->name)) return;
    jjfb_ptr_store_hits++;
    if (jjfb_ptr_store_hits > 40 && (jjfb_ptr_store_hits % 50) != 0) return;
    jjfb_log_regs_short(uc, &pc, &lr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &sp);
    obj = (uint32_t)address >= 4 ? (uint32_t)address - 4 : 0;
    printf("[JJFB_OBJ_BIND] #%u name=%s pixels=0x%X store@0x%X obj~=0x%X pc=0x%X lr=0x%X sp=0x%X\n",
           jjfb_ptr_store_hits, rq->name, (uint32_t)value, (uint32_t)address, obj, pc, lr, sp);
    fflush(stdout);
    if (obj) {
        /* Ensure +4 is committed to our pixels (write-hook may dump pre-commit). */
        uint32_t px = rq->guest_pixels;
        if (px)
            uc_mem_write(uc, obj + 4, &px, 4);
        jjfb_watch_splash_object(uc, rq, obj);
    }
}

static void jjfb_install_ptr_store_watch(uc_engine *uc) {
    uc_err err;
    if (!uc || jjfb_ptr_store_hook_installed) return;
    err = uc_hook_add(uc, &jjfb_ptr_store_hook, UC_HOOK_MEM_WRITE,
                      jjfb_hook_ptr_store, NULL, 0x600000, 0xA00000);
    if (err) printf("[JJFB_PTR_STORE] heap watch fail err=%u\n", err);
    else printf("[JJFB_PTR_STORE] watching heap stores @0x600000..0xA00000\n");
    err = uc_hook_add(uc, &jjfb_ptr_store_hook_erw, UC_HOOK_MEM_WRITE,
                      jjfb_hook_ptr_store, NULL, 0x280000, 0x360000);
    if (err) printf("[JJFB_PTR_STORE] erw watch fail err=%u\n", err);
    else printf("[JJFB_PTR_STORE] watching erw/mid stores @0x280000..0x360000\n");
    jjfb_ptr_store_hook_installed = 1;
    fflush(stdout);
}

static void jjfb_hook_ac8_mem(uc_engine *uc, uc_mem_type type,
                              uint64_t address, int size, int64_t value,
                              void *user_data) {
    uint32_t pc = 0, lr = 0, cur = 0;
    static uint32_t nr, nw;
    (void)user_data; (void)size;
    if (!jjfb_guest_ext_erw) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &cur, 4);
    if (type == UC_MEM_READ) {
        nr++;
        if (nr <= 40 || (nr % 50) == 0) {
            printf("[JJFB_AC8_READ] #%u value=%d pc=0x%X lr=0x%X\n",
                   nr, (int32_t)cur, pc, lr);
            fflush(stdout);
        }
    } else {
        nw++;
        printf("[JJFB_AC8_WRITE] #%u old=%d -> new=%d pc=0x%X lr=0x%X\n",
               nw, (int32_t)cur, (int32_t)value, pc, lr);
        fflush(stdout);
    }
}

static void jjfb_hook_progress_write(uc_engine *uc, uc_mem_type type,
                                    uint64_t address, int size, int64_t value,
                                    void *user_data) {
    uint32_t pc = 0, lr = 0, old = 0;
    static uint32_t n;
    (void)type; (void)size; (void)user_data;
    n++;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    if (jjfb_guest_ext_erw)
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C, &old, 4);
    printf("[JJFB_PROGRESS_WRITE] #%u addr=0x%X old=%d -> new=%d pc=0x%X lr=0x%X tick=%u\n",
           n, (uint32_t)address, (int32_t)old, (int32_t)value, pc, lr, jjfb_handler_tick);
    printf("[JJFB_PROGRESS_COUNT_WRITE] #%u addr=0x%X old=%d -> new=%d pc=0x%X lr=0x%X\n",
           n, (uint32_t)address, (int32_t)old, (int32_t)value, pc, lr);
    fflush(stdout);
}

static void jjfb_hook_progress_read(uc_engine *uc, uc_mem_type type,
                                   uint64_t address, int size, int64_t value,
                                   void *user_data) {
    uint32_t pc = 0, lr = 0, cur = 0;
    static uint32_t n;
    (void)type; (void)size; (void)user_data; (void)value;
    n++;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    /* Unicorn MEM_READ value arg is unreliable; read memory ourselves. */
    if (address)
        uc_mem_read(uc, address, &cur, 4);
    if (n <= 48 || (n % 40) == 0) {
        printf("[JJFB_PROGRESS_READ] #%u value=%d pc=0x%X lr=0x%X tick=%u\n",
               n, (int32_t)cur, pc, lr, jjfb_handler_tick);
        fflush(stdout);
    }
}

static void jjfb_hook_startup_str_read(uc_engine *uc, uc_mem_type type,
                                      uint64_t address, int size, int64_t value,
                                      void *user_data) {
    uint32_t pc = 0, lr = 0, mode = 0, ac8 = 0, prog = 0;
    static uint32_t n;
    const char *tag = (const char *)user_data;
    (void)type; (void)size; (void)value;
    n++;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    if (jjfb_guest_ext_erw) {
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE_EARLY, &mode, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &ac8, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C, &prog, 4);
    }
    if (n <= 40 || (n % 20) == 0) {
        printf("[JJFB_STARTUP_STR] #%u tag=%s va=0x%X pc=0x%X lr=0x%X "
               "ui_mode=0x%X ac8=%d prog=%d tick=%u\n",
               n, tag ? tag : "?", (uint32_t)address, pc, lr, mode,
               (int32_t)ac8, (int32_t)prog, jjfb_handler_tick);
        fflush(stdout);
    }
}

static void jjfb_install_startup_str_watch(uc_engine *uc) {
    static const struct { uint32_t va; const char *tag; } sites[] = {
        { JJFB_STR_CHECK_NET_VA, "check_net_timeout" },
        { JJFB_STR_CONN_FAIL_VA, "conn_fail" },
        { JJFB_STR_CONNECTING_VA, "connecting" },
        { JJFB_STR_DL_RES_VA, "dl_resource" },
        { JJFB_STR_CHECK_UPDATE_VA, "check_update_list" },
        { JJFB_STR_LOGGING_VA, "logging_in" },
    };
    static uc_hook hooks[8];
    uint32_t i;
    uc_err err;
    if (!uc || jjfb_startup_str_watch_installed) return;
    for (i = 0; i < (uint32_t)(sizeof(sites) / sizeof(sites[0])); i++) {
        err = uc_hook_add(uc, &hooks[i], UC_HOOK_MEM_READ, jjfb_hook_startup_str_read,
                          (void *)sites[i].tag, sites[i].va, sites[i].va + 8);
        if (err)
            printf("[JJFB_STARTUP_STR] watch fail %s @0x%X err=%u\n",
                   sites[i].tag, sites[i].va, err);
        else
            printf("[JJFB_STARTUP_STR] watching %s @0x%X\n", sites[i].tag, sites[i].va);
    }
    jjfb_startup_str_watch_installed = 1;
    fflush(stdout);
}

static const char *jjfb_erw_off_tag(uint32_t off) {
    if (off == JJFB_ERW_STATE_EARLY) return "ui_mode";
    if (off == JJFB_ERW_SPLASH_CNT_AC8) return "AC8";
    if (off == 0xB6C) return "B6C_gate_obj"; /* splash post-progress gate: *(ERW+0xB6C) */
    if (off == 0x134D) return "flag_134D";   /* BLE gate before AC8 success path */
    if (off == 0x1350) return "cnt_1350";    /* compared to 100 in success path */
    if (off == 0xA98) return "slot_A98";
    if (off == JJFB_ERW_PROGRESS_BA0) return "BA0_base";
    if (off == JJFB_ERW_PROGRESS_BA0 + 0x20) return "bar_obj";
    if (off == JJFB_ERW_PROGRESS_BA0 + 0x24) return "textbar_obj";
    if (off == JJFB_ERW_PROGRESS_BA0 + 0x28) return "loadingbar_obj";
    if (off == JJFB_ERW_PROGRESS_BA0 + 0x2C) return "progress_count";
    if (off >= JJFB_ERW_PROGRESS_BA0 && off < JJFB_ERW_PROGRESS_BA0 + 0x40)
        return "BA0_region";
    return "erw";
}

static void jjfb_hook_erw_band_write(uc_engine *uc, uc_mem_type type,
                                    uint64_t address, int size, int64_t value,
                                    void *user_data) {
    uint32_t pc = 0, lr = 0, off, old = 0;
    static uint32_t n;
    (void)type; (void)user_data;
    if (!jjfb_guest_ext_erw) return;
    off = (uint32_t)address - jjfb_guest_ext_erw;
    /* Always log critical slots; throttle the rest. */
    n++;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    if (size >= 4)
        uc_mem_read(uc, address, &old, 4);
    if (off == JJFB_ERW_STATE_EARLY || off == JJFB_ERW_SPLASH_CNT_AC8 ||
        off == JJFB_ERW_PROGRESS_BA0 + 0x2C ||
        (jjfb_verbose_logs() &&
         (off == 0xB6C || off == 0x134D || off == 0x1350 || off == 0xA98 ||
          n <= 80 || (n % 50) == 0))) {
        printf("[JJFB_ERW_WRITE] #%u off=0x%X tag=%s old=0x%X new=0x%X size=%d "
               "pc=0x%X lr=0x%X tick=%u\n",
               n, off, jjfb_erw_off_tag(off), old, (uint32_t)value, size, pc, lr,
               jjfb_handler_tick);
        fflush(stdout);
    }
}

static void jjfb_install_erw_band_watch(uc_engine *uc) {
    uc_err err;
    uint32_t begin, end;
    if (!uc || !jjfb_guest_ext_erw || jjfb_erw_band_watch_installed) return;
    begin = jjfb_guest_ext_erw + 0x800;
    /* Include 0x134D/0x1350 splash status slots (were past old 0xC50 end). */
    end = jjfb_guest_ext_erw + 0x1400;
    err = uc_hook_add(uc, &jjfb_erw_band_hook, UC_HOOK_MEM_WRITE,
                      jjfb_hook_erw_band_write, NULL, begin, end);
    if (err)
        printf("[JJFB_ERW_WRITE] band watch fail [0x%X,0x%X] err=%u\n", begin, end, err);
    else {
        jjfb_erw_band_watch_installed = 1;
        printf("[JJFB_ERW_WRITE] watching ERW+0x800..0x1400 @0x%X..0x%X "
               "(ui_mode/AC8/BA0/progress/B6C/134D)\n", begin, end);
        fflush(stdout);
    }
}

/* One-shot static scan: literal VAs / offsets in robotol code image. */
static void jjfb_scan_state_xrefs(uc_engine *uc) {
    static const uint32_t lit_vals[] = {
        0x8D0, 0xAC8, 0xBA0, 0xB6C, 0x2C, 0x134D,
        JJFB_STR_CHECK_NET_VA, JJFB_STR_CONN_FAIL_VA, JJFB_STR_CONNECTING_VA,
        JJFB_STR_DL_RES_VA, JJFB_STR_CHECK_UPDATE_VA, JJFB_STR_LOGGING_VA,
        JJFB_SLOGO_STR_VA, JJFB_LOADINGBAR_STR_VA
    };
    static const char *lit_names[] = {
        "imm_8D0", "imm_AC8", "imm_BA0", "imm_B6C", "imm_2C", "imm_134D",
        "str_check_net", "str_conn_fail", "str_connecting",
        "str_dl_res", "str_check_update", "str_logging",
        "str_slogo", "str_loadingbar"
    };
    uint32_t hits[16];
    uint32_t shown[16];
    /* Focused bands: splash + dispatch + string pool neighborhood. */
    uint32_t ranges[][2] = {
        { 0x2EF000, 0x2F0800 },
        { 0x304000, 0x308000 },
        { 0x2D9000, 0x2DC000 },
        { 0x312000, 0x316000 },
        { 0x2E8000, 0x2EF000 }, /* earlier splash helpers */
        { 0x300000, 0x304000 }
    };
    uint32_t ri, vi, addr, word, nlit;
    if (!uc || jjfb_xref_scan_done) return;
    jjfb_xref_scan_done = 1;
    nlit = (uint32_t)(sizeof(lit_vals) / sizeof(lit_vals[0]));
    memset(hits, 0, sizeof(hits));
    memset(shown, 0, sizeof(shown));
    printf("[JJFB_XREF_SCAN] begin literal scan for ui_mode/AC8/progress/strings\n");
    fflush(stdout);
    for (ri = 0; ri < (uint32_t)(sizeof(ranges) / sizeof(ranges[0])); ri++) {
        for (addr = ranges[ri][0]; addr + 4 <= ranges[ri][1]; addr += 4) {
            if (!getMrpMemPtr(addr)) {
                addr = (addr + 0xFFF) & ~0xFFFu; /* skip unmapped page */
                if (addr < 4) break;
                addr -= 4;
                continue;
            }
            if (uc_mem_read(uc, addr, &word, 4) != UC_ERR_OK) continue;
            for (vi = 0; vi < nlit; vi++) {
                if (word != lit_vals[vi]) continue;
                hits[vi]++;
                if (shown[vi] < 10) {
                    shown[vi]++;
                    printf("[JJFB_XREF_LIT] name=%s value=0x%X at=0x%X\n",
                           lit_names[vi], lit_vals[vi], addr);
                    fflush(stdout);
                }
            }
        }
    }
    for (vi = 0; vi < nlit; vi++) {
        printf("[JJFB_XREF_LIT] name=%s total_hits=%u\n", lit_names[vi], hits[vi]);
        fflush(stdout);
    }
    /* Dump ?32 bytes around BA0/AC8 splash literals for writer disasm. */
    {
        static const uint32_t sites[] = {
            0x2EFC58, 0x2EFC6C, 0x2EFC88, 0x2EFC8C,
            0x305EFC, 0x31258C, 0x312588
        };
        uint32_t si, off, b;
        uint8_t buf[64];
        for (si = 0; si < (uint32_t)(sizeof(sites) / sizeof(sites[0])); si++) {
            if (!getMrpMemPtr(sites[si] - 16)) continue;
            if (uc_mem_read(uc, sites[si] - 16, buf, sizeof(buf)) != UC_ERR_OK) continue;
            printf("[JJFB_XREF_DUMP] around 0x%X:", sites[si]);
            for (off = 0; off < sizeof(buf); off++) {
                if ((off % 16) == 0) printf("\n  %08X:", (sites[si] - 16) + off);
                printf(" %02X", buf[off]);
            }
            printf("\n");
            fflush(stdout);
        }
        (void)b;
    }
    printf("[JJFB_XREF_SCAN] done\n");
    fflush(stdout);
}

/* v47 probe: JJFB_PROGRESS_DRIVER=linear|step|off ??not a final solution. */
static int jjfb_progress_driver_want(uint32_t n_splash, uint32_t *out_want,
                                    const char **out_mode) {
    const char *drv = getenv("JJFB_PROGRESS_DRIVER");
    uint32_t want = 0;
    if (!drv || !drv[0] || strcmp(drv, "off") == 0 || drv[0] == '0') {
        /* Legacy aliases. */
        const char *scan = getenv("JJFB_PROGRESS_SCAN");
        const char *nudge = getenv("JJFB_PROGRESS_NUDGE");
        if (scan && scan[0] && scan[0] != '0') {
            if (out_mode) *out_mode = "scan";
            if (n_splash <= 2) want = 1;
            else if (n_splash <= 8) want = 6;
            else want = 12;
            *out_want = want;
            return 1;
        }
        if (nudge && nudge[0] && nudge[0] != '0') {
            if (out_mode) *out_mode = "nudge";
            *out_want = (uint32_t)strtoul(nudge, NULL, 0);
            return 1;
        }
        return 0;
    }
    if (strcmp(drv, "linear") == 0) {
        /* Grow 0..12 across splash enters / ticks ??never jump to 12 first. */
        want = n_splash;
        if (want > 12) want = 12;
        if (out_mode) *out_mode = "linear";
        *out_want = want;
        return 1;
    }
    if (strcmp(drv, "step") == 0) {
        want = (n_splash <= 12) ? n_splash : 12;
        if (out_mode) *out_mode = "step";
        *out_want = want;
        return 1;
    }
    return 0;
}

static void jjfb_progress_driver_apply(uc_engine *uc, uint32_t n_splash,
                                      const char *reason) {
    uint32_t want = 0, cur = 0, addr, bar = 0, load = 0, mode = 0;
    const char *mode_name = "?";
    JjfbBmpReq *breq;
    if (!uc || !jjfb_guest_ext_erw) return;
    if (!jjfb_progress_driver_want(n_splash, &want, &mode_name)) return;
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE_EARLY, &mode, 4);
    if (mode != 0x45) return;
    addr = jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C;
    uc_mem_read(uc, addr, &cur, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x20, &bar, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x28, &load, 4);
    /* Prefer applying after objects exist; allow first-enter write too. */
    if (want == cur) return;
    if (want < cur && strcmp(mode_name, "linear") == 0) return; /* never decrease */
    uc_mem_write(uc, addr, &want, 4);
    jjfb_progress_nudge_val = want;
    printf("[JJFB_PROGRESS_DRIVER] tick=%u count %u -> %u reason=%s mode=%s "
           "bar=0x%X loadingbar=0x%X (probe, not final)\n",
           jjfb_handler_tick, cur, want, reason ? reason : "-", mode_name, bar, load);
    fflush(stdout);
    /* If progress loop already ran once, host-blit new bar segments as probe. */
    breq = jjfb_bmp_cache_find_object(bar);
    if (breq && breq->guest_pixels && want > cur && want <= 12) {
        uint32_t idx;
        for (idx = cur; idx < want && idx < 12; idx++) {
            int x = 47 + (int)idx * 16; /* xbase from logs */
            int y = 226;
            jjfb_host_blit_req(uc, breq, x, y, "PROGRESS_DRIVER");
            printf("[JJFB_PROGRESS_DRAW] idx=%u count=%u slot=bar x=%d y=%d "
                   "name=%s (driver_probe)\n",
                   idx, want, x, y, breq->name);
            fflush(stdout);
            jjfb_progress_drew = 1;
        }
        jjfb_debug_present_dirty("PROGRESS_DRIVER");
    }
}

/* Probe-only: write progress_count so bar loop can DRAW (does NOT skip startup). */
static void jjfb_progress_probe_tick(uc_engine *uc) {
    const char *drv;
    uint32_t mode = 0, load = 0, bar = 0, text = 0;
    if (!uc || !jjfb_guest_ext_erw) return;
    drv = getenv("JJFB_PROGRESS_DRIVER");
    if (drv && drv[0] && strcmp(drv, "off") != 0 && drv[0] != '0') {
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE_EARLY, &mode, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x28, &load, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x20, &bar, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x24, &text, 4);
        if (mode == 0x45 && (load || bar || text)) {
            /* Map tick to splash-like index so linear grows 0..12. */
            uint32_t n = (jjfb_handler_tick > 10) ? (jjfb_handler_tick - 10) : 0;
            if (n > 12) n = 12;
            jjfb_progress_driver_apply(uc, n, "timer");
        }
        return;
    }
    /* Legacy SCAN/NUDGE path kept for compatibility. */
    {
        const char *nudge, *scan;
        uint32_t addr, cur = 0, want = 0;
        int do_write = 0;
        nudge = getenv("JJFB_PROGRESS_NUDGE");
        scan = getenv("JJFB_PROGRESS_SCAN");
        addr = jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C;
        uc_mem_read(uc, addr, &cur, 4);
        if (scan && scan[0] && scan[0] != '0') {
            if (jjfb_handler_tick >= 20 && jjfb_handler_tick < 40)
                want = 1;
            else if (jjfb_handler_tick >= 40 && jjfb_handler_tick < 60)
                want = 6;
            else if (jjfb_handler_tick >= 60)
                want = 12;
            if (want && want > cur)
                do_write = 1;
        } else if (nudge && nudge[0] && nudge[0] != '0') {
            want = (uint32_t)strtoul(nudge, NULL, 0);
            if (want != cur)
                do_write = 1;
        }
        if (!do_write) return;
        uc_mem_write(uc, addr, &want, 4);
        jjfb_progress_nudge_val = want;
        printf("[JJFB_PROGRESS_PHASE] probe write %u -> %u tick=%u (anim probe, not skip)\n",
               cur, want, jjfb_handler_tick);
        fflush(stdout);
    }
}

/* TEMPORARY PROBE (not official): skip splash net/login wait after ~2s.
 * Env: JJFB_SKIP_NET_LOGIN=1
 * Optional: JJFB_SKIP_NET_LOGIN_MS=2000
 * TEMPORARY probe — not official login bypass.
 * Seeds splash success gates (AC8>0, 134D!=0, B6C!=0, prog=12) and arms
 * one-shot B6C/134D/AC8 gate patches + 2EFC0A divert if still looping.
 * network.c also forces plat net APIs to succeed when env is set. */
static void jjfb_probe_skip_net_login_tick(uc_engine *uc) {
    static int done;
    static int saw_splash;
    static int64_t t0_ms;
    const char *env, *dms;
    uint32_t mode = 0, one = 1, twelve = 12;
    uint8_t one8 = 1;
    int delay_ms = 2000;

    if (done || !uc || !jjfb_guest_ext_erw)
        return;
    env = getenv("JJFB_SKIP_NET_LOGIN");
    if (!env || env[0] != '1')
        return;
    dms = getenv("JJFB_SKIP_NET_LOGIN_MS");
    if (dms && dms[0])
        delay_ms = atoi(dms);
    if (delay_ms < 200)
        delay_ms = 200;

    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE_EARLY, &mode, 4);
    if (mode != 0x45)
        return;

    if (!saw_splash) {
        saw_splash = 1;
        t0_ms = get_uptime_ms();
        printf("[JJFB_PROBE_SKIP_NET] ARMED delay=%dms tick=%u "
               "(TEMPORARY probe — not official login bypass)\n",
               delay_ms, jjfb_handler_tick);
        fflush(stdout);
        return;
    }
    if ((get_uptime_ms() - t0_ms) < delay_ms)
        return;

    done = 1;
    /* v49 gates: AC8>0 and 134D!=0 for success path; B6C!=0 to leave progress. */
    uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &one, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + 0xB6C, &one, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C, &twelve, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + 0x134D, &one8, 1);
    jjfb_skip_net_arm_2efc = 1;
    printf("[JJFB_PROBE_SKIP_NET] FIRE tick=%u ac8=1 b6c=1 prog=12 flag134D=1 "
           "arm_gates+2EFC0A (TEMPORARY probe)\n",
           jjfb_handler_tick);
    fflush(stdout);
}

static void jjfb_ac8_pulse_tick(uc_engine *uc) {
    const char *mode_ac8, *pulse;
    uint32_t addr, cur = 0, one = 1, z = 0;
    int hold;
    if (!uc || !jjfb_guest_ext_erw) return;
    mode_ac8 = getenv("JJFB_AC8_MODE");
    if (!mode_ac8 || !mode_ac8[0])
        mode_ac8 = getenv("JJFB_SPLASH_AC8_MODE");
    pulse = getenv("JJFB_AC8_PULSE_TICKS");
    if ((!mode_ac8 || strcmp(mode_ac8, "pulse") != 0) && !(pulse && pulse[0] && pulse[0] != '0'))
        return;
    hold = (pulse && pulse[0]) ? atoi(pulse) : 3;
    if (hold < 1) hold = 3;
    addr = jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8;
    uc_mem_read(uc, addr, &cur, 4);
    if (!jjfb_ac8_pulse_armed && jjfb_handler_tick >= 12) {
        uc_mem_write(uc, addr, &one, 4);
        jjfb_ac8_pulse_armed = 1;
        jjfb_ac8_pulse_ticks = hold;
        printf("[JJFB_AC8] pulse arm ac8=%d -> 1 hold=%d tick=%u\n",
               (int32_t)cur, hold, jjfb_handler_tick);
        fflush(stdout);
        return;
    }
    if (jjfb_ac8_pulse_armed && jjfb_ac8_pulse_ticks > 0) {
        jjfb_ac8_pulse_ticks--;
        if (jjfb_ac8_pulse_ticks == 0) {
            uc_mem_write(uc, addr, &z, 4);
            printf("[JJFB_AC8] pulse release -> 0 tick=%u\n", jjfb_handler_tick);
            fflush(stdout);
        }
    }
}

static void jjfb_log_startup_phase(uc_engine *uc, const char *why) {
    uint32_t mode = 0, ac8 = 0, prog = 0, bar = 0, text = 0, load = 0;
    if (!uc || !jjfb_guest_ext_erw) return;
    if ((jjfb_handler_tick % 15) != 1 && !(why && why[0] == '!'))
        return;
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE_EARLY, &mode, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &ac8, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C, &prog, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x20, &bar, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x24, &text, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x28, &load, 4);
    printf("[JJFB_STARTUP_PHASE] tick=%u why=%s ui_mode=0x%X ac8=%d prog=%d "
           "bar=0x%X textbar=0x%X loadingbar=0x%X drew=%d\n",
           jjfb_handler_tick, why ? why : "-", mode, (int32_t)ac8, (int32_t)prog,
           bar, text, load, jjfb_progress_drew);
    fflush(stdout);
}

static void jjfb_hook_uimode_write(uc_engine *uc, uc_mem_type type,
                                  uint64_t address, int size, int64_t value,
                                  void *user_data) {
    uint32_t pc = 0, lr = 0, old = 0;
    static uint32_t n;
    (void)type; (void)size; (void)user_data;
    n++;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    if (jjfb_guest_ext_erw)
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE_EARLY, &old, 4);
    printf("[JJFB_UIMODE_WRITE] #%u addr=0x%X old=0x%X -> new=0x%X pc=0x%X lr=0x%X\n",
           n, (uint32_t)address, old, (uint32_t)value, pc, lr);
    fflush(stdout);
}

static void jjfb_install_ac8_watch(uc_engine *uc) {
    uc_err err;
    uint32_t a, prog, mode_addr;
    if (!uc || !jjfb_guest_ext_erw || jjfb_ac8_watch_installed) return;
    a = jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8;
    err = uc_hook_add(uc, &jjfb_ac8_hook_r, UC_HOOK_MEM_READ, jjfb_hook_ac8_mem, NULL, a, a + 3);
    if (err) printf("[JJFB_AC8] read watch fail err=%u\n", err);
    err = uc_hook_add(uc, &jjfb_ac8_hook_w, UC_HOOK_MEM_WRITE, jjfb_hook_ac8_mem, NULL, a, a + 3);
    if (err) printf("[JJFB_AC8] write watch fail err=%u\n", err);
    else {
        jjfb_ac8_watch_installed = 1;
        printf("[JJFB_AC8] watching ERW+0xAC8 @0x%X (slogo gate)\n", a);
    }
    /* Progress count BA0+0x2C and ui_mode ERW+0x8D0 write watches. */
    if (!jjfb_state_watch_installed) {
        prog = jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C;
        mode_addr = jjfb_guest_ext_erw + JJFB_ERW_STATE_EARLY;
        err = uc_hook_add(uc, &jjfb_prog_hook_w, UC_HOOK_MEM_WRITE,
                          jjfb_hook_progress_write, NULL, prog, prog + 3);
        if (err) printf("[JJFB_PROGRESS] write watch fail err=%u\n", err);
        else printf("[JJFB_PROGRESS] watching count @0x%X (BA0+0x2C; NOT loadingbar slot)\n", prog);
        err = uc_hook_add(uc, &jjfb_prog_hook_r, UC_HOOK_MEM_READ,
                          jjfb_hook_progress_read, NULL, prog, prog + 3);
        if (err) printf("[JJFB_PROGRESS] read watch fail err=%u\n", err);
        else printf("[JJFB_PROGRESS] watching count READ @0x%X\n", prog);
        err = uc_hook_add(uc, &jjfb_uimode_hook_w, UC_HOOK_MEM_WRITE,
                          jjfb_hook_uimode_write, NULL, mode_addr, mode_addr + 3);
        if (err) printf("[JJFB_UIMODE] write watch fail err=%u\n", err);
        else printf("[JJFB_UIMODE] watching state/ui_mode @0x%X (ERW+0x8D0)\n", mode_addr);
        jjfb_state_watch_installed = 1;
        fflush(stdout);
    }
    jjfb_install_startup_str_watch(uc);
    jjfb_install_erw_band_watch(uc);
    jjfb_scan_state_xrefs(uc);
    fflush(stdout);
}

static void jjfb_install_slogo_watch(uc_engine *uc) {
    uc_err err;
    static uc_hook lb_hook;
    static int lb_on;
    if (!uc || jjfb_slogo_read_hook_installed) return;
    err = uc_hook_add(uc, &jjfb_slogo_read_hook, UC_HOOK_MEM_READ,
                      jjfb_hook_slogo_mem_read, NULL,
                      JJFB_SLOGO_STR_VA, JJFB_SLOGO_STR_VA + 20);
    if (err) {
        printf("[JJFB_SLOGO_XREF] watch install fail @0x%X err=%u\n", JJFB_SLOGO_STR_VA, err);
        fflush(stdout);
        return;
    }
    jjfb_slogo_read_hook_installed = 1;
    printf("[JJFB_SLOGO_XREF] watching string @0x%X (slogo!157!58.bmp)\n", JJFB_SLOGO_STR_VA);
    if (!lb_on) {
        err = uc_hook_add(uc, &lb_hook, UC_HOOK_MEM_READ,
                          jjfb_hook_slogo_mem_read, (void *)(intptr_t)1,
                          JJFB_LOADINGBAR_STR_VA, JJFB_LOADINGBAR_STR_VA + 20);
        if (!err) {
            lb_on = 1;
            printf("[JJFB_SLOGO_XREF] also watching loadingbar str @0x%X\n", JJFB_LOADINGBAR_STR_VA);
        }
    }
    fflush(stdout);
}

static void jjfb_hook_obj_bind_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t lr = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0;
    static uint32_t n;
    static uint32_t last_patch_obj;
    JjfbBmpReq *rq;
    (void)size; (void)user_data;
    n++;
    jjfb_log_regs_short(uc, NULL, &lr, &r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7, NULL);
    /* Patch at store site (2D9596) or once per object — heap may be >0x7F0000. */
    if ((uint32_t)address == 0x2D9596 || (uint32_t)address == 0x2D958E) {
        rq = jjfb_bmp_cache_find_pixels(r0);
        if (rq && jjfb_is_firstscreen_name(rq->name) && r4 && r4 != r0 &&
            getMrpMemPtr(r4) && r4 != last_patch_obj) {
            uint32_t px = rq->guest_pixels;
            if (px)
                uc_mem_write(uc, r4 + 4, &px, 4);
            jjfb_watch_splash_object(uc, rq, r4);
            last_patch_obj = r4;
        }
    }
    if (n > 24 && (n % 40) != 0) return;
    printf("[JJFB_OBJ_BIND_PC] #%u pc=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X\n",
           n, (uint32_t)address, lr, r0, r1, r2, r3, r4, r5, r6, r7);
    fflush(stdout);
}

static void jjfb_hook_splash_flow(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t lr = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, pc = (uint32_t)address;
    (void)size; (void)user_data;
    if (jjfb_splash_last_pc && pc > jjfb_splash_last_pc && (pc - jjfb_splash_last_pc) < 0x20 &&
        pc != 0x2EFA32 && pc != 0x2EFA42 && pc != 0x2EFA52 && pc < 0x2EF980)
        return;
    jjfb_splash_last_pc = pc;
    jjfb_splash_flow_hits++;
    if (jjfb_splash_flow_hits > 80 && (jjfb_splash_flow_hits % 50) != 0) return;
    jjfb_log_regs_short(uc, NULL, &lr, &r0, &r1, &r2, &r3, NULL, NULL, NULL, NULL, NULL);
    printf("[JJFB_SPLASH_FLOW] #%u pc=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X\n",
           jjfb_splash_flow_hits, pc, lr, r0, r1, r2, r3);
    fflush(stdout);
}

static void jjfb_install_obj_bind_pc_hook(uc_engine *uc) {
    uc_err err;
    if (!uc || jjfb_obj_bind_hook_installed) return;
    err = uc_hook_add(uc, &jjfb_obj_bind_hook, UC_HOOK_CODE,
                      jjfb_hook_obj_bind_code, NULL, 0x2D9580, 0x2D95A0, 0);
    if (!err) {
        jjfb_obj_bind_hook_installed = 1;
        printf("[JJFB_OBJ_BIND_PC] watching code @0x2D9580..0x2D95A0\n");
        fflush(stdout);
    }
}

static void jjfb_install_splash_flow_hook(uc_engine *uc) {
    static const uint32_t sites[] = {
        0x2EF902, 0x2EF908, 0x2EF986,
        0x2EFA32, 0x2EFA42, 0x2EFA52, 0x2EFA54
    };
    static uc_hook hooks[8];
    static int on;
    uint32_t i;
    uc_err err;
    if (!uc || on) return;
    for (i = 0; i < (uint32_t)(sizeof(sites) / sizeof(sites[0])); i++) {
        err = uc_hook_add(uc, &hooks[i], UC_HOOK_CODE, jjfb_hook_splash_flow, NULL,
                          sites[i], sites[i] + 2, 0);
        if (err)
            printf("[JJFB_SPLASH_FLOW] hook 0x%X fail err=%u\n", sites[i], err);
    }
    on = 1;
    jjfb_splash_flow_hook_installed = 1;
    printf("[JJFB_SPLASH_FLOW] sparse sites installed\n");
    fflush(stdout);
}

static uint32_t jjfb_bmp_make_object(uint32_t pixels, int w, int h, int bytes) {
    JjfbBmpObjHdr *hdr;
    void *p = my_mallocExt(sizeof(JjfbBmpObjHdr));
    if (!p) return 0;
    hdr = (JjfbBmpObjHdr *)p;
    memset(hdr, 0, sizeof(*hdr));
    hdr->size_bytes = (uint32_t)bytes;
    hdr->pixels = pixels;
    hdr->w = (int16_t)w;
    hdr->h = (int16_t)h;
    hdr->flag = 1;
    hdr->size2 = (uint32_t)bytes;
    return toMrpMemAddr(p);
}

static int jjfb_parse_bmp_name(const char *name, char *out_name, int out_cap,
                               int *w, int *h) {
    const char *p, *q;
    int ww = 0, hh = 0;
    size_t nlen;
    if (!name || !name[0] || !w || !h) return 0;
    p = strchr(name, '!');
    if (!p) return 0;
    q = strchr(p + 1, '!');
    if (!q) return 0;
    ww = atoi(p + 1);
    hh = atoi(q + 1);
    if (ww <= 0 || hh <= 0 || ww > 1024 || hh > 1024) return 0;
    nlen = (size_t)(p - name);
    if (nlen == 0 || nlen >= (size_t)out_cap) return 0;
    memcpy(out_name, name, nlen);
    out_name[nlen] = 0;
    *w = ww;
    *h = hh;
    return 1;
}

static JjfbBmpReq *jjfb_bmp_cache_find_bytes(int bytes) {
    int i;
    for (i = jjfb_bmp_cache_n - 1; i >= 0; i--) {
        if (jjfb_bmp_cache[i].valid && jjfb_bmp_cache[i].bytes == bytes)
            return &jjfb_bmp_cache[i];
    }
    return NULL;
}

static JjfbBmpReq *jjfb_bmp_cache_find_pixels(uint32_t px) {
    int i;
    if (!px) return NULL;
    for (i = jjfb_bmp_cache_n - 1; i >= 0; i--) {
        if (jjfb_bmp_cache[i].valid && jjfb_bmp_cache[i].guest_pixels == px)
            return &jjfb_bmp_cache[i];
    }
    return NULL;
}

static void jjfb_bmp_clamp_blit_geom(uint32_t pixels, int32_t *iw, int32_t *ih,
                                     int32_t *imw, int32_t *ix, int32_t *iy,
                                     uint32_t n) {
    JjfbBmpReq *rq;
    if (!iw || !ih || !imw) return;
    rq = jjfb_bmp_cache_find_pixels(pixels);
    if (!rq || rq->w <= 0 || rq->h <= 0) return;
    /* Platform: clamp OOB crop to real buffer size only — never rewrite x/y. */
    if (*imw < rq->w) *imw = rq->w;
    if (*iw > rq->w) *iw = rq->w;
    if (*ih > rq->h) *ih = rq->h;
    (void)ix;
    (void)iy;
    if (n <= 16 || (n % 40) == 0) {
        printf("[JJFB_V76_BMP] clamp blit name=%s -> %dx%d @%d,%d (mw=%d)\n",
               rq->name, *iw, *ih, ix ? *ix : 0, iy ? *iy : 0, *imw);
        fflush(stdout);
    }
}

static void jjfb_note_memcpy_bmp(uc_engine *uc, uint32_t dst, uint32_t src, uint32_t n) {
    JjfbBmpReq *rq;
    uint32_t lr = 0, pc = 0;
    if (!uc) return;
    rq = jjfb_bmp_cache_find_pixels(src);
    if (!rq) rq = jjfb_bmp_cache_find_pixels(dst);
    if (!rq || !jjfb_is_firstscreen_name(rq->name)) return;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    printf("[JJFB_MEMCPY_BMP] name=%s dst=0x%X src=0x%X n=0x%X pc=0x%X lr=0x%X\n",
           rq->name, dst, src, n, pc, lr);
    fflush(stdout);
}

static void jjfb_note_drawbitmap_bmp(uc_engine *uc, uint32_t bmp, int32_t x, int32_t y,
                                     uint32_t w, uint32_t h) {
    JjfbBmpReq *rq;
    uint32_t lr = 0;
    if (!uc || !bmp) return;
    rq = jjfb_bmp_cache_find_pixels(bmp);
    if (!rq) return;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    printf("[JJFB_DRAWBITMAP_BMP] name=%s bmp=0x%X x=%d y=%d w=%u h=%u lr=0x%X\n",
           rq->name, bmp, x, y, w, h, lr);
    fflush(stdout);
}

static void jjfb_bmp_req_set(const char *fullname, uint32_t name_ptr, uint32_t lr) {
    char base[96];
    int w = 0, h = 0;
    JjfbBmpReq *slot;
    if (!fullname || !jjfb_parse_bmp_name(fullname, base, (int)sizeof(base), &w, &h))
        return;
    memset(&jjfb_bmp_last, 0, sizeof(jjfb_bmp_last));
    strncpy(jjfb_bmp_last.name, fullname, sizeof(jjfb_bmp_last.name) - 1);
    jjfb_bmp_last.w = w;
    jjfb_bmp_last.h = h;
    jjfb_bmp_last.bytes = w * h * 2;
    jjfb_bmp_last.caller_lr = lr;
    jjfb_bmp_last.name_ptr = name_ptr;
    jjfb_bmp_last.valid = 1;
    if (jjfb_bmp_cache_n < (int)(sizeof(jjfb_bmp_cache) / sizeof(jjfb_bmp_cache[0])))
        slot = &jjfb_bmp_cache[jjfb_bmp_cache_n++];
    else
        slot = &jjfb_bmp_cache[jjfb_bmp_cache_n - 1];
    *slot = jjfb_bmp_last;
    printf("[JJFB_BMP_REQ] name=%s w=%d h=%d bytes=0x%X lr=0x%X\n",
           jjfb_bmp_last.name, w, h, jjfb_bmp_last.bytes, lr);
    fflush(stdout);
    if (jjfb_is_firstscreen_name(fullname))
        jjfb_note_firstscreen("bmp_req", fullname);
}

/* Platform: last successfully opened .mrp — 10134 often follows mr_open(pack)
 * and expects pixels for a member whose W*H*2 matches app (shared shell, not
 * JJFB-only size tables). */
static char jjfb_last_mrp_host[JJFB_PATH_MAX];
static char jjfb_last_mrp_guest[192];

/* Scan an MRP for a null-terminated member name with !W!H where W*H*2==bytes. */
static int jjfb_mrp_find_name_by_bytes(const char *pack_path, uint32_t bytes,
                                       char *out, int out_cap) {
    FILE *fp;
    long fsz;
    uint8_t *file = NULL;
    uint8_t *p, *end;
    int found = 0;
    if (!pack_path || !pack_path[0] || !out || out_cap < 8 || bytes < 16)
        return 0;
    fp = fopen(pack_path, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    fsz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsz <= 0 || fsz > 8 * 1024 * 1024) { fclose(fp); return 0; }
    file = (uint8_t *)malloc((size_t)fsz);
    if (!file) { fclose(fp); return 0; }
    if (fread(file, 1, (size_t)fsz, fp) != (size_t)fsz) {
        free(file); fclose(fp); return 0;
    }
    fclose(fp);
    end = file + fsz;
    for (p = file; p + 8 < end; p++) {
        const char *s;
        char base[96];
        int w = 0, h = 0;
        size_t n;
        if (*p < 0x20 || *p > 0x7e) continue;
        if (p > file && p[-1] >= 0x20 && p[-1] <= 0x7e) continue; /* mid-string */
        s = (const char *)p;
        n = 0;
        while (p + n < end && s[n] && n < 120) n++;
        if (n < 5 || n >= 120 || s[n] != 0) continue;
        if (!strchr(s, '!')) continue;
        if (!jjfb_parse_bmp_name(s, base, (int)sizeof(base), &w, &h)) continue;
        if (w <= 0 || h <= 0) continue;
        if ((uint32_t)(w * h * 2) != bytes) continue;
        if ((int)n + 1 > out_cap) continue;
        memcpy(out, s, n + 1);
        found = 1;
        break;
    }
    free(file);
    return found;
}

/* Legacy chrome sizes inside jjfb.mrp when no pack context / 2d92dc. */
static const char *jjfb_bmp_name_by_bytes(uint32_t bytes) {
    switch (bytes) {
    case 0x2D8A: return "loadingbar!201!29.bmp";
    case 0x240:  return "bar!16!18.bmp";
    case 0x1C20: return "textbar!120!30.bmp";
    case 0x4724: return "slogo!157!58.bmp";
    default: return NULL;
    }
}

/* Load RGB565 pixels for name!W!H.bmp from open packs (gzip/zlib members). */
static int jjfb_mrp_load_rgb565(const char *full_name, int expect_bytes,
                                uint8_t *out, int out_cap) {
    static const char *packs[] = {
        "mythroad/gwy/jjfb.mrp",
        "mythroad/dsm_gm.mrp",
        "mythroad/320x480/gwy/jjfb.mrp",
        "mythroad/240x320/gwy/jjfb.mrp",
        "mythroad/gwy/jjfbol/downimage1.mrp",
        "mythroad/gwy/jjfbol/downimage2.mrp",
        "mythroad/gwy/jjfbol/downimage3.mrp",
        "mythroad/gwy/jjfbol/default2.mrp",
        "mythroad/320x480/gwy/jjfbol/downimage1.mrp",
        "mythroad/320x480/gwy/jjfbol/downimage2.mrp",
        "mythroad/320x480/gwy/jjfbol/downimage3.mrp",
        "mythroad/320x480/gwy/jjfbol/default2.mrp",
        "mythroad/240x320/gwy/jjfbol/downimage1.mrp",
        "mythroad/240x320/gwy/jjfbol/downimage2.mrp",
        "mythroad/240x320/gwy/jjfbol/downimage3.mrp",
        "mythroad/240x320/gwy/jjfbol/default2.mrp",
        "jjfb_bmp_cache", /* marker: try cache file below */
        NULL
    };
    int pi;
    char cache_path[256];
    char pack_from_at[192];
    FILE *fp;
    long fsz;
    uint8_t *file = NULL;
    size_t nlen;
    uint8_t *p, *end;
    uint32_t off = 0, len = 0;
    uLongf destLen;
    int ok = 0;
    const char *at;

    if (!full_name || !out || expect_bytes <= 0 || expect_bytes > out_cap)
        return 0;

    /* Fast path: pre-extracted cache (sanitize @/! for Windows paths). */
    {
        char safe[192];
        size_t i, j = 0;
        for (i = 0; full_name[i] && j + 1 < sizeof(safe); i++) {
            char c = full_name[i];
            safe[j++] = (c == '!' || c == '@' || c == '/' || c == '\\') ? '_' : c;
        }
        safe[j] = 0;
        snprintf(cache_path, sizeof(cache_path), "jjfb_bmp_cache/%s.rgb565", safe);
    }
    fp = fopen(cache_path, "rb");
    if (fp) {
        if (fseek(fp, 0, SEEK_END) == 0) {
            fsz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            if (fsz == expect_bytes && (int)fread(out, 1, (size_t)expect_bytes, fp) == expect_bytes)
                ok = 1;
        }
        fclose(fp);
        if (ok) {
            printf("[JJFB_BMP_LOAD] cache %s -> %d bytes\n", cache_path, expect_bytes);
            fflush(stdout);
            return 1;
        }
    }

    /* show1!232!100@downimage1.bmp → prefer mythroad/gwy/jjfbol/downimage1.mrp */
    pack_from_at[0] = 0;
    at = strchr(full_name, '@');
    if (at && at[1]) {
        char pack_base[64];
        size_t k = 0;
        const char *s = at + 1;
        while (*s && *s != '.' && k + 1 < sizeof(pack_base))
            pack_base[k++] = *s++;
        pack_base[k] = 0;
        if (pack_base[0])
            snprintf(pack_from_at, sizeof(pack_from_at),
                     "mythroad/gwy/jjfbol/%s.mrp", pack_base);
    }

    nlen = strlen(full_name);
    /* pi=-2: last opened .mrp (pack-scan context); -1: @pack; then packs[]. */
    for (pi = -2; ; pi++) {
        const char *pack_path;
        if (pi == -2) {
            if (!jjfb_last_mrp_host[0])
                continue;
            pack_path = jjfb_last_mrp_host;
        } else if (pi == -1) {
            if (!pack_from_at[0])
                continue;
            pack_path = pack_from_at;
        } else if (!packs[pi]) {
            break;
        } else {
            pack_path = packs[pi];
            if (strcmp(pack_path, "jjfb_bmp_cache") == 0)
                continue;
        }
        fp = fopen(pack_path, "rb");
        if (!fp) continue;
        if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); continue; }
        fsz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fsz <= 0 || fsz > 8 * 1024 * 1024) { fclose(fp); continue; }
        file = (uint8_t *)malloc((size_t)fsz);
        if (!file) { fclose(fp); continue; }
        if (fread(file, 1, (size_t)fsz, fp) != (size_t)fsz) {
            free(file); fclose(fp); continue;
        }
        fclose(fp);
        /* Scan MRP file table for exact name + 4x uint32 (off,len,...) */
        end = file + fsz - (int)nlen - 17;
        for (p = file; p < end; p++) {
            if (memcmp(p, full_name, nlen) == 0 && p[nlen] == 0) {
                memcpy(&off, p + nlen + 1, 4);
                memcpy(&len, p + nlen + 5, 4);
                if (off + len <= (uint32_t)fsz && len > 0 && len < 2 * 1024 * 1024) {
                    destLen = (uLongf)out_cap;
                    if (uncompress((Bytef *)out, &destLen, (const Bytef *)(file + off), len) == Z_OK &&
                        (int)destLen == expect_bytes) {
                        ok = 1;
                    } else {
                        /* gzip / zlib auto (15+32); then gzip; zlib; raw. */
                        static const int k_wbits[] = {15 + 32, 15 + 16, 15, -15};
                        int wi;
                        for (wi = 0; wi < 4 && !ok; wi++) {
                            z_stream strm;
                            int zret;
                            memset(&strm, 0, sizeof(strm));
                            strm.next_in = (Bytef *)(file + off);
                            strm.avail_in = len;
                            strm.next_out = (Bytef *)out;
                            strm.avail_out = (uInt)out_cap;
                            zret = inflateInit2(&strm, k_wbits[wi]);
                            if (zret == Z_OK) {
                                zret = inflate(&strm, Z_FINISH);
                                if ((zret == Z_STREAM_END || zret == Z_OK) &&
                                    (int)strm.total_out == expect_bytes)
                                    ok = 1;
                                inflateEnd(&strm);
                            }
                        }
                    }
                }
                break;
            }
        }
        free(file);
        file = NULL;
        if (ok) {
            printf("[JJFB_BMP_LOAD] mrp %s name=%s bytes=%d\n",
                   pack_path, full_name, expect_bytes);
            fflush(stdout);
            return 1;
        }
    }
    printf("[JJFB_BMP_LOAD] FAIL name=%s expect=%d\n", full_name, expect_bytes);
    fflush(stdout);
    return 0;
}

/* fill_placeholder / blit live after jjfb_screen565 helpers below. */

static void br_mr_open(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_open)(const char* filename,  uint32 mode);
    uint32_t filename, mode;
    uc_reg_read(uc, UC_ARM_REG_R0, &filename);
    uc_reg_read(uc, UC_ARM_REG_R1, &mode);
    char *filenameStr = getMrpMemPtr(filename);
    char resolved[JJFB_PATH_MAX];
    uint32_t pc = 0, lr = 0;
    int path_exists = 0;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    path_exists = my_resolve_path(filenameStr, resolved, sizeof(resolved));
    if (filenameStr) {
        if (jjfb_is_firstscreen_name(filenameStr)) {
            printf("[JJFB_FIRST_SCREEN] open file=%s mode=%u\n", filenameStr, mode);
            jjfb_note_firstscreen("resource_open", filenameStr);
        }
        if (strstr(filenameStr, "wy_jiao") || strstr(filenameStr, "wy_xian") || strstr(filenameStr, "jiantou") ||
            strstr(filenameStr, "vmimage") || strstr(filenameStr, "taskbutton") || strstr(filenameStr, "vmright") ||
            strchr(filenameStr, '!') || strchr(filenameStr, '@')) {
            printf("[JJFB_310BB4_RESOURCE] open file=%s mode=%u\n", filenameStr, mode);
            fflush(stdout);
        }
        if (strstr(filenameStr, ".ext") || strstr(filenameStr, ".mr") ||
            strstr(filenameStr, "mrc_") || strstr(filenameStr, "robot") ||
            strstr(filenameStr, "module") || strstr(filenameStr, "jjfbol") ||
            strstr(filenameStr, "gwy/") || strstr(filenameStr, "login") ||
            strstr(filenameStr, "autologin") || strstr(filenameStr, "net") ||
            strstr(filenameStr, ".mpr") || strstr(filenameStr, ".dat") ||
            strstr(filenameStr, "cmlib") || strstr(filenameStr, "mps") ||
            strstr(filenameStr, "reg.") || strstr(filenameStr, "sgl")) {
            printf("[JJFB_FILE] open file=%s mode=%u\n", filenameStr, mode);
            fflush(stdout);
        }
    }
    int32_t ret = my_open(resolved, mode);
    if (ret) {
        printf("[JJFB_FILEOPEN] guest=\"%s\" host=\"%s\" ok=1 handle=%d mode=%u pc=0x%X lr=0x%X\n",
               filenameStr ? filenameStr : "(null)", resolved, ret, mode, pc, lr);
        /* Remember pack for subsequent 10134(size) member resolve (shell). */
        if (filenameStr && resolved[0]) {
            const char *dot = strrchr(filenameStr, '.');
            if (dot && (strcmp(dot, ".mrp") == 0 || strcmp(dot, ".MRP") == 0)) {
                snprintf(jjfb_last_mrp_host, sizeof(jjfb_last_mrp_host), "%s", resolved);
                snprintf(jjfb_last_mrp_guest, sizeof(jjfb_last_mrp_guest), "%s",
                         filenameStr);
                printf("[JJFB_MRP_CTX] last_open guest=\"%s\" host=\"%s\"\n",
                       jjfb_last_mrp_guest, jjfb_last_mrp_host);
                fflush(stdout);
            }
        }
    } else {
        printf("[JJFB_FILEOPEN_MISS] guest=\"%s\" tried=[\"%s\",\"%s\"] exists_before=%d mode=%u pc=0x%X lr=0x%X\n",
               filenameStr ? filenameStr : "(null)", resolved,
               filenameStr ? filenameStr : "(null)", path_exists, mode, pc, lr);
        /* 0@s0.map etc.: member@pack / download cache — RDWR may create host file. */
        if (filenameStr && strchr(filenameStr, '@'))
            printf("[JJFB_FILEOPEN_AT] guest=\"%s\" (member@pack / download cache; "
                   "miss on read, RDWR may create)\n",
                   filenameStr);
    }
    fflush(stdout);
    if (filenameStr && jjfb_is_firstscreen_name(filenameStr)) {
        printf("[JJFB_FIRST_SCREEN] open result file=%s ret=%d\n", filenameStr, ret);
        fflush(stdout);
    }
    LOG("ext call %s(0x%X[%s], 0x%X): %d\n", o->name, filename, filenameStr, mode, ret);
    SET_RET_V(ret);
}

static void br_mr_close(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_close)(int32 f);
    uint32_t f, ret;
    uc_reg_read(uc, UC_ARM_REG_R0, &f);
    ret = my_close(f);
    LOG("ext call %s(%d): %d\n", o->name, f, ret);
    SET_RET_V(ret);
}

static void br_mr_write(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_write)(int32 f,void *p,uint32 l);
    uint32_t f, p, l, ret;

    uc_reg_read(uc, UC_ARM_REG_R0, &f);
    uc_reg_read(uc, UC_ARM_REG_R1, &p);
    uc_reg_read(uc, UC_ARM_REG_R2, &l);

    LOG("ext call %s(0x%X, 0x%X, 0x%X)\n", o->name, f, p, l);
    LOG("ext call %s([%u], [%u], [%u])\n", o->name, f, p, l);

    char *buf = malloc(l);
    uc_mem_read(uc, p, buf, l);
    ret = my_write(f, buf, l);
    free(buf);

    SET_RET_V(ret);
}

static void br_mr_read(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_read)(int32 f,void *p,uint32 l);
    uint32_t f, p, l, ret;
    uc_reg_read(uc, UC_ARM_REG_R0, &f);
    uc_reg_read(uc, UC_ARM_REG_R1, &p);
    uc_reg_read(uc, UC_ARM_REG_R2, &l);
    char *buf = getMrpMemPtr(p);
    ret = my_read(f, buf, l);
    LOG("ext call %s(%d, 0x%X, %u): %d\n", o->name, f, p, l, ret);
    SET_RET_V(ret);
}

static void br_mr_seek(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_seek)(int32 f, int32 pos, int method);
    uint32_t f, pos, method, ret;
    uc_reg_read(uc, UC_ARM_REG_R0, &f);
    uc_reg_read(uc, UC_ARM_REG_R1, &pos);
    uc_reg_read(uc, UC_ARM_REG_R2, &method);
    ret = my_seek(f, pos, method);
    LOG("ext call %s(%d, %d, 0x%X): %d\n", o->name, f, pos, method, ret);
    SET_RET_V(ret);
}

static void br_mr_getLen(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_getLen)(const char* filename);
    uint32_t filename;
    uc_reg_read(uc, UC_ARM_REG_R0, &filename);
    char *filenameStr = getMrpMemPtr(filename);
    LOG("ext call %s(%s)\n", o->name, filenameStr);
    SET_RET_V(my_getLen(filenameStr));
}

static void br_mr_remove(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_remove)(const char* filename);
    uint32_t filename;
    uc_reg_read(uc, UC_ARM_REG_R0, &filename);
    char *filenameStr = getMrpMemPtr(filename);
    LOG("ext call %s(%s)\n", o->name, filenameStr);
    SET_RET_V(my_remove(filenameStr));
}

static void br_mr_rename(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_rename)(const char* oldname, const char* newname);
    uint32_t oldname, newname;
    uc_reg_read(uc, UC_ARM_REG_R0, &oldname);
    uc_reg_read(uc, UC_ARM_REG_R1, &newname);
    char *oldnameStr = getMrpMemPtr(oldname);
    char *newnameStr = getMrpMemPtr(newname);
    LOG("ext call %s(%s, %s)\n", o->name, oldnameStr, newnameStr);
    SET_RET_V(my_rename(oldnameStr, newnameStr));
}

static void br_mr_mkDir(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_mkDir)(const char* name);
    uint32_t name;
    uc_reg_read(uc, UC_ARM_REG_R0, &name);
    char *nameStr = getMrpMemPtr(name);
    LOG("ext call %s(%s)\n", o->name, nameStr);
    SET_RET_V(my_mkDir(nameStr));
}

static void br_mr_rmDir(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_rmDir)(const char* name);
    uint32_t name;
    uc_reg_read(uc, UC_ARM_REG_R0, &name);
    char *nameStr = getMrpMemPtr(name);
    LOG("ext call %s(%s)\n", o->name, nameStr);
    SET_RET_V(my_rmDir(nameStr));
}

static uint64_t uptime_ms;
static void br_get_uptime_ms_init(BridgeMap *o, uc_engine *uc, uint32_t addr) {
    LOG("br_%s_init() 0x%X[%u]\n", o->name, addr, addr);
    uptime_ms = (uint64_t)get_uptime_ms();
    uc_mem_write(uc, addr, &addr, 4);
}

static void br_get_uptime_ms(BridgeMap *o, uc_engine *uc) {
    // uint32 (*get_uptime_ms)(void);
    uint32_t ret = (uint32_t)((uint64_t)get_uptime_ms() - uptime_ms);
    LOG("ext call %s(): 0x%X[%u]\n", o->name, ret, ret);
    SET_RET_V(ret);
}

/* Guest mr_table.mr_getTime ??was UNIMPL (MR_FAILED), which freezes splash timers. */
static void br_mr_getTime(BridgeMap *o, uc_engine *uc) {
    uint32_t ret = (uint32_t)((uint64_t)get_uptime_ms() - uptime_ms);
    (void)o;
    SET_RET_V(ret);
}

static void jjfb_v53_probe_guest_log(uc_engine *uc, const char *str) {
    uint32_t value = 0, helper = 0, P = 0;
    unsigned len = 0;

    if (!str || !jjfb_bridge_env_true("JJFB_MRP_ALIAS_CFUNCTION_ROBOTOL")) return;

    if (sscanf(str, "--- ext: @%x", &value) == 1) {
        jjfb_v53_last_guest_ext_base = value;
        printf("[JJFB_GUEST_EXT] observed ext_base=0x%X\n", value);
        fflush(stdout);
        return;
    }

    if (sscanf(str, "_mr_c_function_new(%x, %u)  mr_c_function_P:%x",
               &helper, &len, &P) == 3) {
        jjfb_v53_guest_ext_ordinal++;
        jjfb_v53_last_guest_ext_P = P;
        if (jjfb_v53_guest_ext_ordinal == 1) {
            jjfb_v53_loader_helper = helper;
            printf("[JJFB_GUEST_EXT] ordinal=1 role=mrc_loader helper=0x%X P=0x%X "
                   "ext_base=0x%X len=%u\n",
                   helper, P, jjfb_v53_last_guest_ext_base, len);
            jjfb_v53_alias_applied = jjfb_patch_mrc_loader_member_alias(
                uc, jjfb_v53_last_guest_ext_base);
        } else if (jjfb_v53_alias_applied && !jjfb_v53_robotol_loaded &&
                   helper != jjfb_v53_loader_helper) {
            jjfb_v53_robotol_loaded = 1;
            jjfb_v53_robotol_helper = helper;
            printf("[JJFB_ROBOTOL_LOAD] ordinal=3 guest_ordinal=%u helper=0x%X "
                   "P=0x%X ext_base=0x%X loader_helper=0x%X after_alias=1 "
                   "source=br_log\n",
                   jjfb_v53_guest_ext_ordinal, helper, P,
                   jjfb_v53_last_guest_ext_base, jjfb_v53_loader_helper);
            fflush(stdout);
        }
        return;
    }
}

static void br_log(BridgeMap *o, uc_engine *uc) {
    // void (*log)(char *msg);
    uint32_t msg;
    uc_reg_read(uc, UC_ARM_REG_R0, &msg);

    char *str = (char *)getMrpMemPtr(msg);
    // LOG("ext call %s('%s')\n", o->name, str);
    jjfb_v53_probe_guest_log(uc, str);
    puts(str);
    // dumpREG(uc);
}

/* Guest mr_printf ??was UNIMPL (returned MR_FAILED) which can poison init. */
static void br_mr_printf(BridgeMap *o, uc_engine *uc) {
    uint32_t fmt_a, a1, a2, a3;
    uc_reg_read(uc, UC_ARM_REG_R0, &fmt_a);
    uc_reg_read(uc, UC_ARM_REG_R1, &a1);
    uc_reg_read(uc, UC_ARM_REG_R2, &a2);
    uc_reg_read(uc, UC_ARM_REG_R3, &a3);
    const char *fmt = fmt_a ? (const char *)getMrpMemPtr(fmt_a) : "(null)";
    /* Best-effort: treat %s args as guest pointers when format looks safe. */
    char buf[512];
    int n = 0;
    if (fmt && strstr(fmt, "%s")) {
        const char *s1 = a1 ? (const char *)getMrpMemPtr(a1) : "(null)";
        const char *s2 = a2 ? (const char *)getMrpMemPtr(a2) : "(null)";
        n = snprintf(buf, sizeof(buf), fmt, s1, s2, a3);
    } else if (fmt) {
        n = snprintf(buf, sizeof(buf), fmt, a1, a2, a3);
    }
    if (n < 0) {
        printf("[guest-printf] fmt=%s a1=%u a2=%u a3=%u\n", fmt ? fmt : "(null)", a1, a2, a3);
    } else {
        fputs("[guest-printf] ", stdout);
        fwrite(buf, 1, (size_t)((n > 0 && (size_t)n < sizeof(buf)) ? n : strlen(buf)), stdout);
        if (n == 0 || (n > 0 && buf[n - 1] != '\n')) {
            fputc('\n', stdout);
        }
    }
    fflush(stdout);
    SET_RET_V(0);
}

static void br_mem_get(BridgeMap *o, uc_engine *uc) {
    // int32 (*mem_get)(char **mem_base, uint32 *mem_len);
    uint32_t mem_base, mem_len;
    uc_reg_read(uc, UC_ARM_REG_R0, &mem_base);
    uc_reg_read(uc, UC_ARM_REG_R1, &mem_len);

    LOG("ext call %s()\n", o->name);

    uint32_t len = 1024 * 1024 * 4;
    uint32_t buffer = toMrpMemAddr(my_mallocExt(len));

    printf("br_mem_get base=0x%X len=%d(%d kb) =================\n", buffer, len, len / 1024);

    // *mem_base = buffer;
    uc_mem_write(uc, mem_base, &buffer, 4);
    // *mem_len = len;
    uc_mem_write(uc, mem_len, &len, 4);

    SET_RET_V(MR_SUCCESS);
}

static void br_mem_free(BridgeMap *o, uc_engine *uc) {
    // int32 (*mem_free)(char *mem, uint32 mem_len);
    uint32_t mem, mem_len;
    uc_reg_read(uc, UC_ARM_REG_R0, &mem);
    uc_reg_read(uc, UC_ARM_REG_R1, &mem_len);

    LOG("ext call %s(0x%X, 0x%X)\n", o->name, mem, mem_len);
    my_freeExt(getMrpMemPtr(mem));
    SET_RET_V(MR_SUCCESS);
}

static void br_timerStop(BridgeMap *o, uc_engine *uc) {
    // int32 (*timerStop)(void);
    LOG("ext call %s()\n", o->name);
    printf("[JJFB_TIMER] timerStop\n");
    fflush(stdout);
    SET_RET_V(timerStop());
}

static void br_timerStart(BridgeMap *o, uc_engine *uc) {
    // int32 (*timerStart)(uint16 t);
    LOG("ext call %s()\n", o->name);
    int32_t t;
    uc_reg_read(uc, UC_ARM_REG_R0, &t);
    printf("[JJFB_TIMER] timerStart period=%d\n", t);
    fflush(stdout);
    SET_RET_V(timerStart(t));
}

static void br_test(BridgeMap *o, uc_engine *uc) {
    // void (*test)(void);
    LOG("ext call %s()\n", o->name);
}


static void br__mr_TestCom(BridgeMap *o, uc_engine *uc) {
    // int32 _mr_TestCom(mrp_State* L, int input0, int input1);
    // Called from EXT as (L, input0, input1) in r0,r1,r2
    uint32_t r0, r1, r2;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    printf("[JJFB_801] _mr_TestCom(L=0x%X, in0=%d, in1=%d)\n", r0, r1, r2);
    fflush(stdout);
    // Common version probe: TestCom(0,7,9999) expects 9999
    if (r1 == 7) {
        SET_RET_V(r2);
        return;
    }
    SET_RET_V(r2);
}

static void br_exit(BridgeMap *o, uc_engine *uc) {
    // void (*exit)(void);
    // Debug: do not kill the process ??distinguish Lua Exit() from emulator crash.
    LOG("ext call %s()\n", o->name);
    printf("[JJFB_EXIT] br_exit called (Lua Exit / mythroad exit), suppressing process exit\n");
    fflush(stdout);
    SET_RET_V(MR_SUCCESS);
}

static void br_mr_exit(BridgeMap *o, uc_engine *uc) {
    printf("[JJFB_EXIT] mr_exit() called from guest, suppressing process exit\n");
    fflush(stdout);
    SET_RET_V(MR_SUCCESS);
}

static void br_srand(BridgeMap *o, uc_engine *uc) {
    // void (*srand)(uint32 seed);
    LOG("ext call %s()\n", o->name);
    uint32_t seed;
    uc_reg_read(uc, UC_ARM_REG_R0, &seed);
    srand(seed);
}

static void br_rand(BridgeMap *o, uc_engine *uc) {
    // int32 (*rand)(void);
    LOG("ext call %s()\n", o->name);
    SET_RET_V(rand());
}

static void br_sleep(BridgeMap *o, uc_engine *uc) {
    // int32 (*sleep)(uint32 ms);
    uint32_t ms;
    uc_reg_read(uc, UC_ARM_REG_R0, &ms);
    LOG("ext call %s(%d)\n", o->name, ms);
    usleep(ms * 1000);  //?? usleep ?????? ???? *1000
    SET_RET_V(MR_SUCCESS);
}

static void br_info(BridgeMap *o, uc_engine *uc) {
    // int32 (*info)(const char *filename);
    LOG("ext call %s()\n", o->name);
    uint32_t filename;
    uc_reg_read(uc, UC_ARM_REG_R0, &filename);
    SET_RET_V(my_info(getMrpMemPtr(filename)))
}

static void br_opendir(BridgeMap *o, uc_engine *uc) {
    // int32 (*opendir)(const char *name);
    LOG("ext call %s()\n", o->name);
    uint32_t name;
    uc_reg_read(uc, UC_ARM_REG_R0, &name);
    SET_RET_V(my_opendir(getMrpMemPtr(name)))
}

#define READDIR_SHARED_MEM_SIZE 128
static char *readdirSharedMem;  // ????????
static void br_readdir_init(BridgeMap *o, uc_engine *uc, uint32_t addr) {
    LOG("br_%s_init() 0x%X[%u]\n", o->name, addr, addr);
    readdirSharedMem = (char *)my_mallocExt(READDIR_SHARED_MEM_SIZE);
    readdirSharedMem[READDIR_SHARED_MEM_SIZE - 1] = '\0';
    uc_mem_write(uc, addr, &addr, 4);
}

static void br_readdir(BridgeMap *o, uc_engine *uc) {
    // char *(*readdir)(int32 f);
    LOG("ext call %s()\n", o->name);
    int32_t f;
    uc_reg_read(uc, UC_ARM_REG_R0, &f);

    char *r = my_readdir(f);
    if (r != NULL) {
        strncpy(readdirSharedMem, r, READDIR_SHARED_MEM_SIZE - 1);
        SET_RET_V(toMrpMemAddr(readdirSharedMem));
    } else {
        SET_RET_V((uint32_t)NULL);
    }
}

static void br_closedir(BridgeMap *o, uc_engine *uc) {
    // int32 (*closedir)(int32 f);
    LOG("ext call %s()\n", o->name);
    int32_t f;
    uc_reg_read(uc, UC_ARM_REG_R0, &f);
    SET_RET_V(my_closedir(f));
}

static void br_getDatetime(BridgeMap *o, uc_engine *uc) {
    // int32 (*getDatetime)(mr_datetime *datetime);
    LOG("ext call %s()\n", o->name);
    uint32_t datetime;
    uc_reg_read(uc, UC_ARM_REG_R0, &datetime);
    SET_RET_V(getDatetime(getMrpMemPtr(datetime)));
}

static void br_mr_initNetwork(BridgeMap *o, uc_engine *uc) {
    // int32 (*initNetwork)(NETWORK_CB cb, const char *mode, void *userData);
    uint32_t cb, mode, userData;
    uc_reg_read(uc, UC_ARM_REG_R0, &cb);
    uc_reg_read(uc, UC_ARM_REG_R1, &mode);
    uc_reg_read(uc, UC_ARM_REG_R2, &userData);
    char *modeStr = mode ? (char *)getMrpMemPtr(mode) : "";
    printf("[JJFB_NET] initNetwork cb=0x%X mode=%s user=0x%X\n", cb, modeStr ? modeStr : "(null)", userData);
    fflush(stdout);
    SET_RET_V(my_initNetwork(uc, (void *)cb, getMrpMemPtr(mode), (void *)userData));
}

static void br_mr_socket(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_socket)(int32 type, int32 protocol);
    int32_t type, protocol;
    uc_reg_read(uc, UC_ARM_REG_R0, &type);
    uc_reg_read(uc, UC_ARM_REG_R1, &protocol);
    int32_t ret = my_socket(type, protocol);
    printf("[JJFB_NET] socket type=%d proto=%d ret=%d\n", type, protocol, ret);
    fflush(stdout);
    LOG("ext call %s(): %d \n", o->name, ret);
    SET_RET_V(ret);
}

static void br_mr_connect(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_connect)(int32 s, int32 ip, uint16 port, int32 type);
    int32_t s, ip, port, type;
    uc_reg_read(uc, UC_ARM_REG_R0, &s);
    uc_reg_read(uc, UC_ARM_REG_R1, &ip);
    uc_reg_read(uc, UC_ARM_REG_R2, &port);
    uc_reg_read(uc, UC_ARM_REG_R3, &type);
    printf("[JJFB_NET] connect s=%d ip=0x%08X port=%d type=%d\n", s, ip, port, type);
    fflush(stdout);
    SET_RET_V(my_connect(s, ip, (uint16)port, type));
}

static void br_mr_closeSocket(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_closeSocket)(int32 s);
    LOG("ext call %s()\n", o->name);
    int32_t s;
    uc_reg_read(uc, UC_ARM_REG_R0, &s);
    SET_RET_V(my_closeSocket(s));
}

static void br_mr_closeNetwork(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_closeNetwork)();
    LOG("ext call %s()\n", o->name);
    SET_RET_V(my_closeNetwork());
}

static void br_mr_getHostByName(BridgeMap *o, uc_engine *uc) {
    // int32 (*getHostByName)(const char *ptr, NETWORK_CB cb, void *userData);
    LOG("ext call %s()\n", o->name);
    uint32_t name, cb, userData;
    uc_reg_read(uc, UC_ARM_REG_R0, &name);
    uc_reg_read(uc, UC_ARM_REG_R1, &cb);
    uc_reg_read(uc, UC_ARM_REG_R2, &userData);
    SET_RET_V(my_getHostByName(uc, getMrpMemPtr(name), (void *)cb, (void *)userData));
}

static void br_mr_sendto(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_sendto)(int32 s, const char *buf, int len, int32 ip, uint16 port);
    LOG("ext call %s()\n", o->name);
    uint32_t s, buf, len, ip, port;
    uc_reg_read(uc, UC_ARM_REG_R0, &s);
    uc_reg_read(uc, UC_ARM_REG_R1, &buf);
    uc_reg_read(uc, UC_ARM_REG_R2, &len);
    uc_reg_read(uc, UC_ARM_REG_R3, &ip);
    port = getArg(uc, 4);
    SET_RET_V(my_sendto(s, getMrpMemPtr(buf), len, ip, (uint16_t)port));
}

static void br_mr_send(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_send)(int32 s, const char *buf, int len);
    LOG("ext call %s()\n", o->name);
    int32_t s, buf, len;
    uc_reg_read(uc, UC_ARM_REG_R0, &s);
    uc_reg_read(uc, UC_ARM_REG_R1, &buf);
    uc_reg_read(uc, UC_ARM_REG_R2, &len);
    SET_RET_V(my_send(s, getMrpMemPtr(buf), len));
}

static void br_mr_recvfrom(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_recvfrom)(int32 s, char *buf, int len, int32 *ip, uint16 *port);
    LOG("ext call %s()\n", o->name);
    uint32_t s, buf, len, ip, port;
    uc_reg_read(uc, UC_ARM_REG_R0, &s);
    uc_reg_read(uc, UC_ARM_REG_R1, &buf);
    uc_reg_read(uc, UC_ARM_REG_R2, &len);
    uc_reg_read(uc, UC_ARM_REG_R3, &ip);
    port = getArg(uc, 4);
    SET_RET_V(my_recvfrom(s, getMrpMemPtr(buf), len, getMrpMemPtr(ip), (uint16_t *)getMrpMemPtr(port)));
}

static void br_mr_recv(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_recv)(int32 s, char *buf, int len);
    LOG("ext call %s()\n", o->name);
    int32_t s, buf, len;
    uc_reg_read(uc, UC_ARM_REG_R0, &s);
    uc_reg_read(uc, UC_ARM_REG_R1, &buf);
    uc_reg_read(uc, UC_ARM_REG_R2, &len);
    SET_RET_V(my_recv(s, getMrpMemPtr(buf), len));
}

/*
??socket connect ???????TCP??????
Syntax
int32 mrc_getSocketState(int32 s);
Parameters
s
   [IN] ???socket????mrc_socket??

Return Value
   MR_SUCCESS ??????
   MR_FAILED ??????
   MR_WAITING ??????
   MR_IGNORE ????????
*/
static void br_mr_getSocketState(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_getSocketState)(int32 s);
    int32_t s;
    uc_reg_read(uc, UC_ARM_REG_R0, &s);
    LOG("ext call %s(%d)\n", o->name, s);
    SET_RET_V(my_getSocketState(s));
}

enum {
    MR_SOUND_MIDI,
    MR_SOUND_WAV,
    MR_SOUND_MP3,
    MR_SOUND_AMR,
    MR_SOUND_PCM  // 8K 16bit PCM
} MR_SOUND_TYPE;

/*
??????
type [IN] ????????MR_SOUND_TYPE????????MR_SOUND_MIDI MR_SOUND_WAV MR_SOUND_MP3
data [IN] ??????
datalen [IN] ??????
loop [IN] 0:????, 1:????
Return Value MR_SUCCESS ?? MR_FAILED ??
*/
#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_playSound, (int type, const void *data, uint32 dataLen, int32 loop), {
    return js_playSound(type, data, dataLen, loop);
});
#endif

static void br_mr_playSound(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_playSound)(int type, const void *data, uint32 dataLen, int32 loop);
    int32_t type, data, dataLen, loop;
    uc_reg_read(uc, UC_ARM_REG_R0, &type);
    uc_reg_read(uc, UC_ARM_REG_R1, &data);
    uc_reg_read(uc, UC_ARM_REG_R2, &dataLen);
    uc_reg_read(uc, UC_ARM_REG_R3, &loop);
    LOG("ext call %s(%d, 0x%x, %d, %d)\n", o->name, type, data, dataLen, loop);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_playSound(type, getMrpMemPtr(data), dataLen, loop));
#else
    SET_RET_V(MR_SUCCESS);
#endif
}

/*
????????
type [IN] ????????MR_SOUND_TYPE????????MR_SOUND_MIDI MR_SOUND_WAV MR_SOUND_MP3
Return Value MR_SUCCESS ?? MR_FAILED ??
*/
#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_stopSound, (int type), {
    return js_stopSound(type);
});
#endif

static void br_mr_stopSound(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_stopSound)(int type);
    int32_t type;
    uc_reg_read(uc, UC_ARM_REG_R0, &type);
    LOG("ext call %s(%d)\n", o->name, type);

#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_stopSound(type));
#else
    SET_RET_V(MR_SUCCESS);
#endif
}

#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_startShake, (int32 ms), {
    return js_startShake(ms);
});
#endif

static void br_mr_startShake(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_startShake)(int32 ms);
    int32_t ms;
    uc_reg_read(uc, UC_ARM_REG_R0, &ms);
    LOG("ext call %s()\n", o->name);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_startShake(ms));
#else
    SET_RET_V(MR_SUCCESS);
#endif
}

#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_stopShake, (), {
    return js_stopShake();
});
#endif

static void br_mr_stopShake(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_stopShake)();
    LOG("ext call %s()\n", o->name);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_stopShake());
#else
    SET_RET_V(MR_SUCCESS);
#endif
}

enum {
    MR_DIALOG_KEY_OK,     // ????????????"????(??);
    MR_DIALOG_KEY_CANCEL  // ????????????"("??")????(??);
};

enum {
    MR_DIALOG_OK,         // ????"??"??
    MR_DIALOG_OK_CANCEL,  // ????"??" "??"??
    MR_DIALOG_CANCEL      // ????"??"??
};

/*
?????????????????????????????????????????????Mythroad???????mrc_event?????Mythroad????
?????MR_DIALOG_EVENT????????ID????"?ID??MR_DIALOG_KEY_OK????"?ID??MR_DIALOG_KEY_CANCEL??
title [IN]???????unicode????????
text [IN]??????unicode????????
type [IN]??????MR_DIALOG_OK MR_DIALOG_OK_CANCEL MR_DIALOG_CANCEL

Return Value ??????????MR_FAILED ??
*/
#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_dialogCreate, (const char *title, const char *text, int32 type), {
    return js_dialogCreate(title, text, type);
});
#endif

static void br_mr_dialogCreate(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_dialogCreate)(const char *title, const char *text, int32 type);
    uint32_t title, text;
    int32_t type;
    uc_reg_read(uc, UC_ARM_REG_R0, &title);
    uc_reg_read(uc, UC_ARM_REG_R1, &text);
    uc_reg_read(uc, UC_ARM_REG_R2, &type);
    LOG("ext call %s()\n", o->name);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_dialogCreate(getMrpMemPtr(title), getMrpMemPtr(text), type));
#else
    SET_RET_V(MR_FAILED);
#endif
}

#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_dialogRelease, (int32 dialog), {
    return js_dialogRelease(dialog);
});
#endif

static void br_mr_dialogRelease(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_dialogRelease)(int32 dialog);
    int32_t dialog;
    uc_reg_read(uc, UC_ARM_REG_R0, &dialog);
    LOG("ext call %s()\n", o->name);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_dialogRelease(dialog));
#else
    SET_RET_V(MR_FAILED);
#endif
}

/*
??????????
dialog [IN]??????
title [IN]???????unicode????????
text [IN]??????unicode????????
type [IN]?type??1???type??,???MR_DIALOG_OK MR_DIALOG_OK_CANCEL MR_DIALOG_CANCEL
Return Value MR_SUCCESS ?? MR_FAILED ??
*/
#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_dialogRefresh, (int32 dialog, const char *title, const char *text, int32 type), {
    return js_dialogRefresh(dialog, title, text, type);
});
#endif

static void br_mr_dialogRefresh(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_dialogRefresh)(int32 dialog, const char *title, const char *text, int32 type);
    LOG("ext call %s()\n", o->name);
    int32_t dialog, type;
    uint32_t title, text;
    uc_reg_read(uc, UC_ARM_REG_R0, &dialog);
    uc_reg_read(uc, UC_ARM_REG_R1, &title);
    uc_reg_read(uc, UC_ARM_REG_R2, &text);
    uc_reg_read(uc, UC_ARM_REG_R3, &type);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_dialogRefresh(dialog, getMrpMemPtr(title), getMrpMemPtr(text), type));
#else
    SET_RET_V(MR_FAILED);
#endif
}

/*
?????????????????
title [IN]???????unicode????????
text [IN]??????unicode????????
type [IN]???????????MR_DIALOG_OK MR_DIALOG_OK_CANCEL MR_DIALOG_CANCEL
Return Value ??????????MR_FAILED ??
Remarks
   ??????????????????????????????????????????????????????????????????????????????
   ?????????????????????????????????????????????????????????????????????????????????
   ?????????????????????Mythroad???????mrc_event?????Mythroad ????????MR_DIALOG_EVENT????????ID??
   "??"?ID??MR_DIALOG_KEY_OK????"?ID??MR_DIALOG_KEY_CANCEL??
*/
#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_textCreate, (const char *title, const char *text, int32 type), {
    return js_textCreate(title, text, type);
});
#endif

static void br_mr_textCreate(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_textCreate)(const char *title, const char *text, int32 type);
    uint32_t title, text;
    int32_t type;
    uc_reg_read(uc, UC_ARM_REG_R0, &title);
    uc_reg_read(uc, UC_ARM_REG_R1, &text);
    uc_reg_read(uc, UC_ARM_REG_R2, &type);
    LOG("ext call %s()\n", o->name);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_textCreate(getMrpMemPtr(title), getMrpMemPtr(text), type));
#else
    SET_RET_V(MR_FAILED);
#endif
}

#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_textRelease, (int32 handle), {
    return js_textRelease(handle);
});
#endif

static void br_mr_textRelease(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_textRelease)(int32 handle);
    int32_t handle;
    uc_reg_read(uc, UC_ARM_REG_R0, &handle);
    LOG("ext call %s()\n", o->name);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_textRelease(handle));
#else
    SET_RET_V(MR_FAILED);
#endif
}

#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_textRefresh, (int32 handle, const char *title, const char *text), {
    return js_textRefresh(handle, title, text);
});
#endif

static void br_mr_textRefresh(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_textRefresh)(int32 handle, const char *title, const char *text);
    LOG("ext call %s()\n", o->name);
    int32_t handle;
    uint32_t title, text;
    uc_reg_read(uc, UC_ARM_REG_R0, &handle);
    uc_reg_read(uc, UC_ARM_REG_R1, &title);
    uc_reg_read(uc, UC_ARM_REG_R2, &text);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_textRefresh(handle, getMrpMemPtr(title), getMrpMemPtr(text)));
#else
    SET_RET_V(MR_FAILED);
#endif
}

enum {
    MR_EDIT_ANY,
    MR_EDIT_NUMERIC,
    MR_EDIT_PASSWORD
};

/*
??????????????????
title [IN]???????unicode????????
text [IN]??????unicode????????
type [IN]?MR_EDIT_ANY;MR_EDIT_NUMERIC;MR_EDIT_PASSWORD??
max_size [IN]??????????unicode???????????????????????????
Return Value ??????????MR_FAILED ??
Remarks
   ???????????????????text?????????????????????
?????????????????????Mythroad???????mrc_event?????
Mythroad????????MR_DIALOG_EVENT????????ID;"??"?ID??MR_DIALOG_KEY_OK??
"??"?ID??MR_DIALOG_KEY_CANCEL??
*/
#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_editCreate, (const char *title, const char *text, int32 type, int32 max_size), {
    return js_editCreate(title, text, type, max_size);
});
#endif

static void br_mr_editCreate(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_editCreate)(const char *title, const char *text, int32 type, int32 max_size);
    LOG("ext call %s()\n", o->name);
    int32_t type, max_size;
    uint32_t title, text;
    uc_reg_read(uc, UC_ARM_REG_R0, &title);
    uc_reg_read(uc, UC_ARM_REG_R1, &text);
    uc_reg_read(uc, UC_ARM_REG_R2, &type);
    uc_reg_read(uc, UC_ARM_REG_R3, &max_size);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_editCreate(getMrpMemPtr(title), getMrpMemPtr(text), type, max_size));
#else
    SET_RET_V(editCreate(getMrpMemPtr(title), getMrpMemPtr(text), type, max_size));
#endif
}

#ifdef __EMSCRIPTEN__
EM_JS(int32, js_mr_editRelease, (int32 edit), {
    return js_editRelease(edit);
});
#endif

static void br_mr_editRelease(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_editRelease)(int32 edit);
    LOG("ext call %s()\n", o->name);
    int32_t edit;
    uc_reg_read(uc, UC_ARM_REG_R0, &edit);
#ifdef __EMSCRIPTEN__
    SET_RET_V(js_mr_editRelease(edit));
#else
    SET_RET_V(editRelease(edit));
#endif
}

/*
????????unicode?????????????????????????????????????????????????????
Return Value ?NULL ?????????unicode??, NULL ??
*/
#ifdef __EMSCRIPTEN__
EM_JS(const char *, js_mr_editGetText, (int32 edit), {
    return js_editGetText(edit);
});
#endif

static void br_mr_editGetText(BridgeMap *o, uc_engine *uc) {
    // const char *(*mr_editGetText)(int32 edit);
    LOG("ext call %s()\n", o->name);
    int32_t edit;
    uc_reg_read(uc, UC_ARM_REG_R0, &edit);
#ifdef __EMSCRIPTEN__
    char *str = (char *)js_mr_editGetText(edit);
    SET_RET_V(toMrpMemAddr(str));
#else
    char *str = editGetText(edit);
    SET_RET_V(toMrpMemAddr(str));
#endif
}

// ????./mrc/[x]_offsets.c???mrp????
static BridgeMap mr_table_funcMap[] = {
    BRIDGE_FUNC_MAP(0x0, MAP_FUNC, mr_malloc, NULL, br_mr_malloc, 0),
    BRIDGE_FUNC_MAP(0x4, MAP_FUNC, mr_free, NULL, br_mr_free, 0),
    BRIDGE_FUNC_MAP(0x8, MAP_FUNC, mr_realloc, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xC, MAP_FUNC, memcpy, NULL, br_memcpy, 0),
    BRIDGE_FUNC_MAP(0x10, MAP_FUNC, memmove, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x14, MAP_FUNC, strcpy, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x18, MAP_FUNC, strncpy, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1C, MAP_FUNC, strcat, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x20, MAP_FUNC, strncat, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x24, MAP_FUNC, memcmp, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x28, MAP_FUNC, strcmp, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x2C, MAP_FUNC, strncmp, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x30, MAP_FUNC, strcoll, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x34, MAP_FUNC, memchr, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x38, MAP_FUNC, memset, NULL, br_memset, 0),
    BRIDGE_FUNC_MAP(0x3C, MAP_FUNC, strlen, NULL, br_strlen, 0),
    BRIDGE_FUNC_MAP(0x40, MAP_FUNC, strstr, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x44, MAP_FUNC, sprintf, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x48, MAP_FUNC, atoi, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x4C, MAP_FUNC, strtoul, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x50, MAP_FUNC, rand, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x54, MAP_DATA, reserve0, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x58, MAP_DATA, reserve1, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x5C, MAP_DATA, _mr_c_internal_table, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x60, MAP_DATA, _mr_c_port_table, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x64, MAP_FUNC, _mr_c_function_new, NULL, br__mr_c_function_new, 0),
    BRIDGE_FUNC_MAP(0x68, MAP_FUNC, mr_printf, NULL, br_mr_printf, 0),
    BRIDGE_FUNC_MAP(0x6C, MAP_FUNC, mr_mem_get, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x70, MAP_FUNC, mr_mem_free, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x74, MAP_FUNC, mr_drawBitmap, NULL, br_mr_drawBitmap, 0),
    BRIDGE_FUNC_MAP(0x78, MAP_FUNC, mr_getCharBitmap, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x7C, MAP_FUNC, mr_timerStart, NULL, br_timerStart, 0),
    BRIDGE_FUNC_MAP(0x80, MAP_FUNC, mr_timerStop, NULL, br_timerStop, 0),
    BRIDGE_FUNC_MAP(0x84, MAP_FUNC, mr_getTime, NULL, br_mr_getTime, 0),
    BRIDGE_FUNC_MAP(0x88, MAP_FUNC, mr_getDatetime, NULL, br_getDatetime, 0),
    BRIDGE_FUNC_MAP(0x8C, MAP_FUNC, mr_getUserInfo, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x90, MAP_FUNC, mr_sleep, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x94, MAP_FUNC, mr_plat, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x98, MAP_FUNC, mr_platEx, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x9C, MAP_FUNC, mr_ferrno, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xA0, MAP_FUNC, mr_open, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xA4, MAP_FUNC, mr_close, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xA8, MAP_FUNC, mr_info, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xAC, MAP_FUNC, mr_write, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xB0, MAP_FUNC, mr_read, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xB4, MAP_FUNC, mr_seek, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xB8, MAP_FUNC, mr_getLen, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xBC, MAP_FUNC, mr_remove, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xC0, MAP_FUNC, mr_rename, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xC4, MAP_FUNC, mr_mkDir, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xC8, MAP_FUNC, mr_rmDir, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xCC, MAP_FUNC, mr_findStart, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xD0, MAP_FUNC, mr_findGetNext, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xD4, MAP_FUNC, mr_findStop, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xD8, MAP_FUNC, mr_exit, NULL, br_mr_exit, 0),
    BRIDGE_FUNC_MAP(0xDC, MAP_FUNC, mr_startShake, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xE0, MAP_FUNC, mr_stopShake, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xE4, MAP_FUNC, mr_playSound, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xE8, MAP_FUNC, mr_stopSound, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xEC, MAP_FUNC, mr_sendSms, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xF0, MAP_FUNC, mr_call, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xF4, MAP_FUNC, mr_getNetworkID, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xF8, MAP_FUNC, mr_connectWAP, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xFC, MAP_FUNC, mr_menuCreate, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x100, MAP_FUNC, mr_menuSetItem, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x104, MAP_FUNC, mr_menuShow, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x108, MAP_DATA, reserve, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x10C, MAP_FUNC, mr_menuRelease, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x110, MAP_FUNC, mr_menuRefresh, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x114, MAP_FUNC, mr_dialogCreate, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x118, MAP_FUNC, mr_dialogRelease, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x11C, MAP_FUNC, mr_dialogRefresh, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x120, MAP_FUNC, mr_textCreate, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x124, MAP_FUNC, mr_textRelease, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x128, MAP_FUNC, mr_textRefresh, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x12C, MAP_FUNC, mr_editCreate, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x130, MAP_FUNC, mr_editRelease, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x134, MAP_FUNC, mr_editGetText, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x138, MAP_FUNC, mr_winCreate, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x13C, MAP_FUNC, mr_winRelease, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x140, MAP_FUNC, mr_getScreenInfo, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x144, MAP_FUNC, mr_initNetwork, NULL, br_mr_initNetwork, 0),
    BRIDGE_FUNC_MAP(0x148, MAP_FUNC, mr_closeNetwork, NULL, br_mr_closeNetwork, 0),
    BRIDGE_FUNC_MAP(0x14C, MAP_FUNC, mr_getHostByName, NULL, br_mr_getHostByName, 0),
    BRIDGE_FUNC_MAP(0x150, MAP_FUNC, mr_socket, NULL, br_mr_socket, 0),
    BRIDGE_FUNC_MAP(0x154, MAP_FUNC, mr_connect, NULL, br_mr_connect, 0),
    BRIDGE_FUNC_MAP(0x158, MAP_FUNC, mr_closeSocket, NULL, br_mr_closeSocket, 0),
    BRIDGE_FUNC_MAP(0x15C, MAP_FUNC, mr_recv, NULL, br_mr_recv, 0),
    BRIDGE_FUNC_MAP(0x160, MAP_FUNC, mr_recvfrom, NULL, br_mr_recvfrom, 0),
    BRIDGE_FUNC_MAP(0x164, MAP_FUNC, mr_send, NULL, br_mr_send, 0),
    BRIDGE_FUNC_MAP(0x168, MAP_FUNC, mr_sendto, NULL, br_mr_sendto, 0),
    BRIDGE_FUNC_MAP(0x16C, MAP_DATA, mr_screenBuf, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x170, MAP_DATA, mr_screen_w, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x174, MAP_DATA, mr_screen_h, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x178, MAP_DATA, mr_screen_bit, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x17C, MAP_DATA, mr_bitmap, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x180, MAP_DATA, mr_tile, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x184, MAP_DATA, mr_map, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x188, MAP_DATA, mr_sound, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x18C, MAP_DATA, mr_sprite, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x190, MAP_DATA, pack_filename, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x194, MAP_DATA, start_filename, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x198, MAP_DATA, old_pack_filename, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x19C, MAP_DATA, old_start_filename, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1A0, MAP_DATA, mr_ram_file, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1A4, MAP_DATA, mr_ram_file_len, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1A8, MAP_DATA, mr_soundOn, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1AC, MAP_DATA, mr_shakeOn, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1B0, MAP_DATA, LG_mem_base, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1B4, MAP_DATA, LG_mem_len, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1B8, MAP_DATA, LG_mem_end, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1BC, MAP_DATA, LG_mem_left, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1C0, MAP_DATA, mr_sms_cfg_buf, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1C4, MAP_FUNC, mr_md5_init, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1C8, MAP_FUNC, mr_md5_append, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1CC, MAP_FUNC, mr_md5_finish, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1D0, MAP_FUNC, _mr_load_sms_cfg, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1D4, MAP_FUNC, _mr_save_sms_cfg, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1D8, MAP_FUNC, _DispUpEx, NULL, br__DispUpEx, 0),
    BRIDGE_FUNC_MAP(0x1DC, MAP_FUNC, _DrawPoint, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1E0, MAP_FUNC, _DrawBitmap, NULL, br__DrawBitmap, 0),
    BRIDGE_FUNC_MAP(0x1E4, MAP_FUNC, _DrawBitmapEx, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1E8, MAP_FUNC, DrawRect, NULL, br_DrawRect, 0),
    BRIDGE_FUNC_MAP(0x1EC, MAP_FUNC, _DrawText, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1F0, MAP_FUNC, _BitmapCheck, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1F4, MAP_FUNC, _mr_readFile, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1F8, MAP_FUNC, mr_wstrlen, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1FC, MAP_FUNC, mr_registerAPP, NULL, br_mr_registerAPP, 0),
    BRIDGE_FUNC_MAP(0x200, MAP_FUNC, _DrawTextEx, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x204, MAP_FUNC, _mr_EffSetCon, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x208, MAP_FUNC, _mr_TestCom, NULL, br__mr_TestCom, 0),
    BRIDGE_FUNC_MAP(0x20C, MAP_FUNC, _mr_TestCom1, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x210, MAP_FUNC, c2u, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x214, MAP_FUNC, _mr_div, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x218, MAP_FUNC, _mr_mod, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x21C, MAP_DATA, LG_mem_min, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x220, MAP_DATA, LG_mem_top, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x224, MAP_DATA, mr_updcrc, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x228, MAP_DATA, start_fileparameter, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x22C, MAP_DATA, mr_sms_return_flag, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x230, MAP_DATA, mr_sms_return_val, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x234, MAP_DATA, mr_unzip, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x238, MAP_DATA, mr_exit_cb, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x23C, MAP_DATA, mr_exit_cb_data, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x240, MAP_DATA, mr_entry, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x244, MAP_FUNC, mr_platDrawChar, NULL, br_mr_platDrawChar, 0),
};

static BridgeMap dsm_require_funcs_funcMap[] = {
    BRIDGE_FUNC_MAP(0x0, MAP_FUNC, test, NULL, br_test, 0),
    BRIDGE_FUNC_MAP(0x4, MAP_FUNC, log, NULL, br_log, 0),
    BRIDGE_FUNC_MAP(0x8, MAP_FUNC, exit, NULL, br_exit, 0),
    BRIDGE_FUNC_MAP(0xc, MAP_FUNC, srand, NULL, br_srand, 0),
    BRIDGE_FUNC_MAP(0x10, MAP_FUNC, rand, NULL, br_rand, 0),
    BRIDGE_FUNC_MAP(0x14, MAP_FUNC, mem_get, NULL, br_mem_get, 0),
    BRIDGE_FUNC_MAP(0x18, MAP_FUNC, mem_free, NULL, br_mem_free, 0),
    BRIDGE_FUNC_MAP(0x1c, MAP_FUNC, timerStart, NULL, br_timerStart, 0),
    BRIDGE_FUNC_MAP(0x20, MAP_FUNC, timerStop, NULL, br_timerStop, 0),
    BRIDGE_FUNC_MAP(0x24, MAP_FUNC, get_uptime_ms, br_get_uptime_ms_init, br_get_uptime_ms, 0),
    BRIDGE_FUNC_MAP(0x28, MAP_FUNC, getDatetime, NULL, br_getDatetime, 0),
    BRIDGE_FUNC_MAP(0x2c, MAP_FUNC, sleep, NULL, br_sleep, 0),
    BRIDGE_FUNC_MAP(0x30, MAP_FUNC, open, NULL, br_mr_open, 0),
    BRIDGE_FUNC_MAP(0x34, MAP_FUNC, close, NULL, br_mr_close, 0),
    BRIDGE_FUNC_MAP(0x38, MAP_FUNC, read, NULL, br_mr_read, 0),
    BRIDGE_FUNC_MAP(0x3c, MAP_FUNC, write, NULL, br_mr_write, 0),
    BRIDGE_FUNC_MAP(0x40, MAP_FUNC, seek, NULL, br_mr_seek, 0),
    BRIDGE_FUNC_MAP(0x44, MAP_FUNC, info, NULL, br_info, 0),
    BRIDGE_FUNC_MAP(0x48, MAP_FUNC, remove, NULL, br_mr_remove, 0),
    BRIDGE_FUNC_MAP(0x4c, MAP_FUNC, rename, NULL, br_mr_rename, 0),
    BRIDGE_FUNC_MAP(0x50, MAP_FUNC, mkDir, NULL, br_mr_mkDir, 0),
    BRIDGE_FUNC_MAP(0x54, MAP_FUNC, rmDir, NULL, br_mr_rmDir, 0),
    BRIDGE_FUNC_MAP(0x58, MAP_FUNC, opendir, NULL, br_opendir, 0),
    BRIDGE_FUNC_MAP(0x5c, MAP_FUNC, readdir, br_readdir_init, br_readdir, 0),
    BRIDGE_FUNC_MAP(0x60, MAP_FUNC, closedir, NULL, br_closedir, 0),
    BRIDGE_FUNC_MAP(0x64, MAP_FUNC, getLen, NULL, br_mr_getLen, 0),
    BRIDGE_FUNC_MAP(0x68, MAP_FUNC, drawBitmap, NULL, br_mr_drawBitmap, 0),

    BRIDGE_FUNC_MAP(0x6c, MAP_FUNC, getHostByName, NULL, br_mr_getHostByName, 0),
    BRIDGE_FUNC_MAP(0x70, MAP_FUNC, initNetwork, NULL, br_mr_initNetwork, 0),
    BRIDGE_FUNC_MAP(0x74, MAP_FUNC, mr_closeNetwork, NULL, br_mr_closeNetwork, 0),
    BRIDGE_FUNC_MAP(0x78, MAP_FUNC, mr_socket, NULL, br_mr_socket, 0),
    BRIDGE_FUNC_MAP(0x7c, MAP_FUNC, mr_connect, NULL, br_mr_connect, 0),
    BRIDGE_FUNC_MAP(0x80, MAP_FUNC, mr_getSocketState, NULL, br_mr_getSocketState, 0),
    BRIDGE_FUNC_MAP(0x84, MAP_FUNC, mr_closeSocket, NULL, br_mr_closeSocket, 0),
    BRIDGE_FUNC_MAP(0x88, MAP_FUNC, mr_recv, NULL, br_mr_recv, 0),
    BRIDGE_FUNC_MAP(0x8c, MAP_FUNC, mr_send, NULL, br_mr_send, 0),
    BRIDGE_FUNC_MAP(0x90, MAP_FUNC, mr_recvfrom, NULL, br_mr_recvfrom, 0),
    BRIDGE_FUNC_MAP(0x94, MAP_FUNC, mr_sendto, NULL, br_mr_sendto, 0),

    BRIDGE_FUNC_MAP(0x98, MAP_FUNC, mr_startShake, NULL, br_mr_startShake, 0),
    BRIDGE_FUNC_MAP(0x9c, MAP_FUNC, mr_stopShake, NULL, br_mr_stopShake, 0),
    BRIDGE_FUNC_MAP(0xa0, MAP_FUNC, mr_playSound, NULL, br_mr_playSound, 0),
    BRIDGE_FUNC_MAP(0xa4, MAP_FUNC, mr_stopSound, NULL, br_mr_stopSound, 0),
    BRIDGE_FUNC_MAP(0xa8, MAP_FUNC, mr_dialogCreate, NULL, br_mr_dialogCreate, 0),
    BRIDGE_FUNC_MAP(0xac, MAP_FUNC, mr_dialogRelease, NULL, br_mr_dialogRelease, 0),
    BRIDGE_FUNC_MAP(0xb0, MAP_FUNC, mr_dialogRefresh, NULL, br_mr_dialogRefresh, 0),
    BRIDGE_FUNC_MAP(0xb4, MAP_FUNC, mr_textCreate, NULL, br_mr_textCreate, 0),
    BRIDGE_FUNC_MAP(0xb8, MAP_FUNC, mr_textRelease, NULL, br_mr_textRelease, 0),
    BRIDGE_FUNC_MAP(0xbc, MAP_FUNC, mr_textRefresh, NULL, br_mr_textRefresh, 0),
    BRIDGE_FUNC_MAP(0xc0, MAP_FUNC, mr_editCreate, NULL, br_mr_editCreate, 0),
    BRIDGE_FUNC_MAP(0xc4, MAP_FUNC, mr_editRelease, NULL, br_mr_editRelease, 0),
    BRIDGE_FUNC_MAP(0xc8, MAP_FUNC, mr_editGetText, NULL, br_mr_editGetText, 0),
    // BRIDGE_FUNC_MAP(0x98, MAP_FUNC, drawBitmap, NULL, NULL, 0),
};
//////////////////////////////////////////////////////////////////////////////////////////

static struct rb_root root = RB_ROOT;

static void hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uIntMap *mobj = uIntMap_search(&root, address);
    if (mobj) {
        BridgeMap *obj = mobj->data;
        if (obj->type == MAP_FUNC) {
            if (obj->fn == NULL) {
                uint32_t r0, r1, r2, r3, lr, pc;
                uc_reg_read(uc, UC_ARM_REG_R0, &r0);
                uc_reg_read(uc, UC_ARM_REG_R1, &r1);
                uc_reg_read(uc, UC_ARM_REG_R2, &r2);
                uc_reg_read(uc, UC_ARM_REG_R3, &r3);
                uc_reg_read(uc, UC_ARM_REG_LR, &lr);
                uc_reg_read(uc, UC_ARM_REG_PC, &pc);
                printf("[JJFB_LOADER] UNIMPL %s() pc=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X\n",
                       obj->name, pc, lr, r0, r1, r2, r3);
                fflush(stdout);
                // Return failure instead of killing the process so we can continue diagnosing.
                SET_RET_V(MR_FAILED);
                uc_reg_write(uc, UC_ARM_REG_PC, &lr);
                return;
            }
            obj->fn(obj, uc);

            uint32_t _lr;
            uc_reg_read(uc, UC_ARM_REG_LR, &_lr);
            uc_reg_write(uc, UC_ARM_REG_PC, &_lr);
            return;
        }
        printf("!!! unregister function at 0x%" PRIX64 " !!! \n", address);
    }
}

static void *hooks_init(uc_engine *uc, BridgeMap *map, uint32_t mapCount, uint32_t size) {
    uc_err err;
    uc_hook trace;
    BridgeMap *obj;
    uIntMap *mobj;
    uint32_t addr;
    void *ptr = my_mallocExt(size);
    uint32_t startAddress = toMrpMemAddr(ptr);

    err = uc_hook_add(uc, &trace, UC_HOOK_CODE, hook_code, NULL, startAddress, startAddress + size, 0);
    if (err != UC_ERR_OK) {
        printf("add hook err %u (%s)\n", err, uc_strerror(err));
        goto end;
    }

    for (int i = 0; i < mapCount; i++) {
        obj = &map[i];
        addr = startAddress + obj->pos;
        if (obj->initFn != NULL) {
            obj->initFn(obj, uc, addr);
        } else {
            if (obj->type == MAP_FUNC) {
                // ??????????????????PC???????????????????????
                uc_mem_write(uc, addr, &addr, 4);
            }
        }
        mobj = malloc(sizeof(uIntMap));
        mobj->key = addr;
        mobj->data = obj;
        if (uIntMap_insert(&root, mobj)) {
            printf("uIntMap_insert() failed %d exists.\n", addr);
            goto end;
        }
    }
    return ptr;
end:
    my_freeExt(ptr);
    exit(1);
    return NULL;
}

static void runCode(uc_engine *uc, uint32_t startAddr, uint32_t stopAddr, bool isThumb) {
    uint32_t pc = 0, lr = 0, r0 = 0;
    static uint32_t early_stops;
    uc_reg_write(uc, UC_ARM_REG_LR, &stopAddr);  // ??????????????return)

    // Note we start at ADDRESS | 1 to indicate THUMB mode.
    startAddr = isThumb ? (startAddr | 1) : startAddr;
    uc_err err = uc_emu_start(uc, startAddr, stopAddr, 0, 0);  // ??unicorn 1.0.2??????pc==stopAddr??????
    if (err) {
        uint32_t pc = 0, lr = 0, r0 = 0, r4 = 0, r9 = 0;
        uc_reg_read(uc, UC_ARM_REG_PC, &pc);
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        uc_reg_read(uc, UC_ARM_REG_R0, &r0);
        uc_reg_read(uc, UC_ARM_REG_R4, &r4);
        uc_reg_read(uc, UC_ARM_REG_R9, &r9);
        printf("Failed on uc_emu_start() with error returned: %u (%s) "
               "pc=0x%X lr=0x%X r0=0x%X r4=0x%X r9=0x%X\n",
               err, uc_strerror(err), pc, lr, r0, r4, r9);
        fflush(stdout);
        /* Keep going for JJFB bring-up: one bad image pointer must not kill
         * the whole process. Caller sees stop as if returned. */
        if (err == UC_ERR_READ_UNMAPPED || err == UC_ERR_FETCH_UNMAPPED ||
            err == UC_ERR_WRITE_UNMAPPED) {
            static uint32_t unmap_n;
            unmap_n++;
            if (unmap_n <= 12 || (unmap_n % 50) == 0)
                printf("[JJFB_UNMAP] survive #%u pc=0x%X lr=0x%X r0=0x%X r4=0x%X r9=0x%X\n",
                       unmap_n, pc, lr, r0, r4, r9);
            {
                uint32_t stop = stopAddr;
                uc_reg_write(uc, UC_ARM_REG_PC, &stop);
            }
            return;
        }
        exit(1);
    }
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    if ((pc & ~1u) != (stopAddr & ~1u)) {
        early_stops++;
        if (early_stops <= 12 || (early_stops % 50) == 0) {
            uc_reg_read(uc, UC_ARM_REG_LR, &lr);
            uc_reg_read(uc, UC_ARM_REG_R0, &r0);
            printf("[JJFB_EMU] early stop #%u pc=0x%X stop=0x%X lr=0x%X r0=0x%X start=0x%X\n",
                   early_stops, pc, stopAddr, lr, r0, startAddr);
            fflush(stdout);
        }
    }
}

uc_err bridge_init(uc_engine *uc) {
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("mutex init fail");
        exit(EXIT_FAILURE);
    }
    uint32_t len = 4 * countof(mr_table_funcMap);  // ??????????????????????
    mr_table = hooks_init(uc, mr_table_funcMap, countof(mr_table_funcMap), len);

    dsm_require_funcs = hooks_init(uc, dsm_require_funcs_funcMap, countof(dsm_require_funcs_funcMap), sizeof(DSM_REQUIRE_FUNCS));
#ifdef __EMSCRIPTEN__
    ((DSM_REQUIRE_FUNCS *)dsm_require_funcs)->flags = FLAG_USE_UTF8_FS;  // wasm?????UTF8??
#else
    ((DSM_REQUIRE_FUNCS *)dsm_require_funcs)->flags = FLAG_USE_UTF8_EDIT;  // windows??????GBK??????????????????utf8????
#endif

    mr_c_event = my_mallocExt(sizeof(event_t));
    dsm_event = my_mallocExt(sizeof(event_t));
    mr_start_dsm_param = my_mallocExt(sizeof(start_t));
    return UC_ERR_OK;
}

uc_err bridge_ext_init(uc_engine *uc) {
    uint32_t v = toMrpMemAddr(mr_table);
    uc_mem_write(uc, CODE_ADDRESS, &v, 4);  // ??mr_table

    v = 1;  // ???? ??mr_extHelper???mr_helper??????
    uc_reg_write(uc, UC_ARM_REG_R0, &v);

    // ??ext??mr_c_function_load()
    runCode(uc, CODE_ADDRESS + 8, CODE_ADDRESS, false);

    // mr_c_function.start_of_ER_RW ????r9(SB)?????????????????
    printf("[JJFB_LOADER] mr_c_function_load done, r9/ER_RW=@%p\n", mr_c_function_P ? mr_c_function_P->start_of_ER_RW : NULL);
    printf("-----> r9:@%p\n", mr_c_function_P->start_of_ER_RW);
    return UC_ERR_OK;
}

static int32_t bridge_mr_extHelper(uc_engine *uc, uint32_t code, uint32_t input, uint32_t input_len) {
    // int32 (*mr_extHelper)(void* P, int32 code, uint8* input, int32 input_len);
    uint32_t v = toMrpMemAddr(mr_c_function_P);
    uc_reg_write(uc, UC_ARM_REG_R0, &v);          // p
    uc_reg_write(uc, UC_ARM_REG_R1, &code);       // code
    uc_reg_write(uc, UC_ARM_REG_R2, &input);      // input
    uc_reg_write(uc, UC_ARM_REG_R3, &input_len);  // input_len

    runCode(uc, mr_extHelper_addr, CODE_ADDRESS, false);
    uc_reg_read(uc, UC_ARM_REG_R0, &v);
    return v;
}

static uint32_t jjfb_guest_ext_helper;
static uint32_t jjfb_guest_ext_P;
/* jjfb_guest_ext_erw defined earlier (v38 watches). */
static uint32_t jjfb_guest_ext_base;
static uint32_t jjfb_send_hook_addr;
static uint32_t jjfb_ext_chunk_addr;
static int jjfb_ext_timer_running;
static uint32_t jjfb_ext_timer_period;
static uint32_t jjfb_ext_timer_id;
static uint32_t jjfb_plat_handler_10120;
static uint32_t jjfb_plat_handler_10140;
static uint32_t jjfb_plat_handler_1e200; /* from 0x10102 code=0x30D301 family */
static uint32_t jjfb_plat_handler_10165; /* from 0x10165 code=0x30D2F9 → B54 enqueue */
static uint32_t jjfb_plat_buf_10165;     /* alloc returned by 0x10165 */
static uint32_t jjfb_plat_buf_10162;     /* alloc returned by 0x10162 (sibling buf) */
static uint32_t jjfb_ret0_stub_addr; /* thumb: movs r0,#0; bx lr */
static uint32_t jjfb_ret1_stub_addr; /* thumb: movs r0,#1; bx lr */
/* jjfb_handler_tick declared earlier for v46 startup probes */
static int jjfb_forced_state;
static int jjfb_forced_splash;
static int jjfb_in_guest_handler;
static int jjfb_pending_1e200;
static uint32_t jjfb_pend_app, jjfb_pend_code, jjfb_pend_p0, jjfb_pend_p1;

/* robotol ER_RW app state (from handler@0x306305) */
/* Handler loads: sb+0x7D8, +0x80, then [r0,#0x78] => ER_RW+0x8D0.
 * cmp #0x45 ??bl 0x2EF86C splash (slogo/loadingbar). */
#define JJFB_ERW_STATE    0x8D0
#define JJFB_ERW_UI_MODE  0x8D0 /* alias: same word as state switch */
#define JJFB_UI_MODE_SPLASH 0x45
/* Post-1E209 refresh gates in handler tail @0x3066a4 */
#define JJFB_ERW_FLAG_C44 0xC44 /* ==1 ??bl 0x2e87ac */
#define JJFB_ERW_FLAG_C9D 0xC9D
#define JJFB_ERW_FLAG_CD1 0xCD1
#define JJFB_ERW_FLAG_CF5 0xCF5
#define JJFB_ERW_PTR_11B0 0x11B0
#define JJFB_ERW_BASE_7D8 0x7D8 /* struct used by 0x2f2a00 draw path */
#define JJFB_ERW_DRAW_FP  0x1510 /* 0x305b68 blx target */
#define JJFB_ERW_DRAW_FP_150C 0x150C /* 0x2EC6B0 ??blx splash/bmp draw */
/* 7D8 screen/clip fields (NOT clocks ??2f995c/2f9968 are getters):
 *   +0x4C @0x824 = clip/screen width
 *   +0x50 @0x828 = y origin
 *   +0x58 @0x830 = stored "H" (getter 2f9968) ??splash uses for X center
 *   +0x5C @0x834 = stored "W" (getter 2f995c) ??splash uses for Y base
 * Splash: x=(2f9968()-bmp.w)/2 — 2f9968 must return WIDTH on canvas. */
#define JJFB_ERW_SCR_W824 0x824
#define JJFB_ERW_SCR_Y828 0x828
#define JJFB_ERW_SCR_H830 0x830
#define JJFB_ERW_SCR_W834 0x834

/* v81: keep guest ERW screen slots matched to fixed SCREEN_WIDTH/HEIGHT.
 * 2f9968 reads +0x830 for X-center (must be WIDTH); 2f995c reads +0x834 for
 * Y-base (must be HEIGHT). y828 is added inside 2EC6B8 — keep 0 or draws shift.
 * Seed once only — re-writing every ext_call fights guest and stalls B71. */
static void jjfb_seed_screen_geom(uc_engine *uc, const char *why) {
    uint32_t w, h, z;
    static int seeded;
    if (!uc || !jjfb_guest_ext_erw || !getMrpMemPtr(jjfb_guest_ext_erw))
        return;
    if (seeded)
        return;
    w = (uint32_t)SCREEN_WIDTH;
    h = (uint32_t)SCREEN_HEIGHT;
    z = 0;
    uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_W824, &w, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_Y828, &z, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_H830, &w, 4); /* X center */
    uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_W834, &h, 4); /* Y base */
    /* Mirror into 7D8 clip block used by 2f2a00 / 305c34. */
    uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x0C, &w, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x10, &h, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x4C, &w, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x50, &z, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x58, &w, 4);
    uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x5C, &h, 4);
    seeded = 1;
    printf("[JJFB_V81_SCREEN] seed geom once %dx%d y828=0 why=%s\n",
           SCREEN_WIDTH, SCREEN_HEIGHT, why ? why : "?");
    fflush(stdout);
}

/* v87: pin ERW+0x828=0. Guest may write 40; 2EC6B8 adds it to every blit Y,
 * pushing textbar rows off-layout (extra bottom wood strip). Default ON.
 * Disable with JJFB_Y828_ZERO=0. */
static void jjfb_keep_y828_zero(uc_engine *uc, const char *why) {
    uint32_t cur = 0, z = 0;
    static uint32_t n_fix;
    const char *env = getenv("JJFB_Y828_ZERO");
    if (env && env[0] == '0')
        return;
    if (!uc || !jjfb_guest_ext_erw || !getMrpMemPtr(jjfb_guest_ext_erw))
        return;
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_Y828, &cur, 4);
    if (cur == 0)
        return;
    uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_Y828, &z, 4);
    n_fix++;
    if (n_fix <= 8 || (n_fix % 40) == 0) {
        printf("[JJFB_V87_Y828] clear %u -> 0 #%u why=%s (stop textbar +40 shift)\n",
               cur, n_fix, why ? why : "?");
        fflush(stdout);
    }
}

/* Same-tick textbar (120x30) overlap: guest y=176/198 collide → double wood strip.
 * Default ON (platform layout fix). JJFB_TEXTBAR_DEDUP=0 to disable. */
static int jjfb_textbar_overlap_skip(int32_t y, int32_t h) {
    static int drawn_ys[8];
    static int drawn_n;
    static uint32_t last_tick;
    int i, enable = 1;
    const char *env = getenv("JJFB_TEXTBAR_DEDUP");
    if (env && env[0] == '0')
        enable = 0;
    if (last_tick != jjfb_handler_tick) {
        last_tick = jjfb_handler_tick;
        drawn_n = 0;
    }
    if (!enable)
        return 0;
    if (h <= 0) h = 30;
    for (i = 0; i < drawn_n; i++) {
        int dy = y - drawn_ys[i];
        if (dy < 0) dy = -dy;
        if (dy == 0)
            return 0; /* same row, other half (x=0 / x=120) */
        if (dy < h - 4) {
            static uint32_t n_skip;
            n_skip++;
            if (n_skip <= 8 || (n_skip % 40) == 0) {
                printf("[JJFB_TB_DEDUP] skip y=%d overlaps y=%d #%u\n",
                       y, drawn_ys[i], n_skip);
                fflush(stdout);
            }
            return 1;
        }
    }
    if (drawn_n < (int)(sizeof(drawn_ys) / sizeof(drawn_ys[0])))
        drawn_ys[drawn_n++] = y;
    return 0;
}

/* Host-side RGB565 framebuffer for 0x11F00 side effects (not fake refresh). */
static uint16_t jjfb_screen565[SCREEN_WIDTH * SCREEN_HEIGHT];
static int jjfb_screen_dirty;
static uint32_t jjfb_screen_dirty_x, jjfb_screen_dirty_y;
static uint32_t jjfb_screen_dirty_w, jjfb_screen_dirty_h;
/* v71: coalesce SDL present until DispUpEx / timer end.
 * Immediate present after every DrawRect/_DrawBitmap showed full-screen black
 * mid-frame → flicker. Opt out: JJFB_PRESENT_IMMEDIATE=1. */
static int jjfb_present_coalesce = 1;
/* v73: after leave_2FC26C, guest ui_mode=3 clear+redraw keeps dirty every tick.
 * Presenting each redraw looks like flicker while B71 stuck. Freeze SDL after
 * the first post-gate flush until B71 advances. Opt out: JJFB_PRESENT_FREEZE=0. */
static int jjfb_present_freeze_enable = 0; /* v84: default OFF — freeze looked like 卡死 */
static int jjfb_present_freeze_armed;
static int jjfb_present_frozen;
static uint8_t jjfb_cached_b71;
/* v74: after leave_2FC26C, enqueue a second code=5 with one 2F68E4 record so
 * 2DADC4 takes nonempty-B58 → 30ED2C (natural B71 writer). Opt out:
 * JJFB_V74_B58_SECOND=0. */
static int jjfb_101ab_fill_records; /* 0=BE(-1) only; 1=one record + BE(-1) */
static int jjfb_v74_second_enq_armed;
static int jjfb_v74_second_enq_done;
static int jjfb_v74_second_enq_enable = 1;
/* v75: after natural B71=1, family app=0xC0 → 30DC44 → 2FEBBC (only B70 writer).
 * Same lifecycle helper class as app=2 (timer create); not raw FORCE B70.
 * Opt out: JJFB_FAMILY_C0_AFTER_B71=0. */
static int jjfb_v75_c0_enable = 1;
static int jjfb_v75_c0_done;

static void jjfb_dump_guest(uc_engine *uc, const char *tag, uint32_t addr, uint32_t n);
static void jjfb_dump_guest_ex(uc_engine *uc, const char *tag, uint32_t addr, uint32_t n);

static FILE *jjfb_gb16_fp;

static int jjfb_gb16_ensure(void) {
    char path[JJFB_PATH_MAX];
    const char *root;
    size_t i;
    if (jjfb_gb16_fp) return 1;
    root = getenv("JJFB_MYTHROAD_ROOT");
    if (root && root[0])
        snprintf(path, sizeof(path), "%s/system/gb16.uc2", root);
    else
        snprintf(path, sizeof(path), "mythroad/system/gb16.uc2");
    for (i = 0; path[i]; i++)
        if (path[i] == '\\') path[i] = '/';
    jjfb_gb16_fp = fopen(path, "rb");
    if (!jjfb_gb16_fp) {
        printf("[JJFB_V86_GB16] open fail path=%s\n", path);
        fflush(stdout);
        return 0;
    }
    printf("[JJFB_V86_GB16] open ok path=%s\n", path);
    fflush(stdout);
    return 1;
}

static void jjfb_host_draw_sky16(uint16_t ch, int32_t x, int32_t y, uint16_t color) {
    uint8_t bitbuf[32];
    int iy, ix;
    if (!jjfb_gb16_ensure()) return;
    if (fseek(jjfb_gb16_fp, (long)ch * 32L, SEEK_SET) != 0) return;
    if (fread(bitbuf, 1, 32, jjfb_gb16_fp) != 32) return;
    for (iy = 0; iy < 16; iy++) {
        uint16_t data = (uint16_t)bitbuf[iy * 2 + 1] | ((uint16_t)bitbuf[iy * 2] << 8);
        int32_t py = y + iy;
        if (py < 0 || py >= SCREEN_HEIGHT) continue;
        for (ix = 0; data; ix++) {
            int32_t px = x + ix;
            if (data & 0x8000u) {
                if (px >= 0 && px < SCREEN_WIDTH) {
                    jjfb_screen565[(uint32_t)py * SCREEN_WIDTH + (uint32_t)px] = color;
                }
            }
            data <<= 1;
        }
    }
    jjfb_screen_mark_dirty(x < 0 ? 0 : x, y < 0 ? 0 : y,
                           (x + 16) > SCREEN_WIDTH ? SCREEN_WIDTH : (x + 16),
                           (y + 16) > SCREEN_HEIGHT ? SCREEN_HEIGHT : (y + 16));
}

/* Draw ASCII with transparent background (no blue fill over wood bars). */
static void jjfb_screen_draw_ascii_fg(int32_t x, int32_t y, uint8_t ch, uint16_t fg);

#ifdef _WIN32
static uint16_t jjfb_gbk_to_unicode(uint8_t a, uint8_t b) {
    char mb[2];
    wchar_t wc = 0;
    mb[0] = (char)a;
    mb[1] = (char)b;
    if (MultiByteToWideChar(936, 0, mb, 2, &wc, 1) == 1)
        return (uint16_t)wc;
    return 0;
}
#else
static uint16_t jjfb_gbk_to_unicode(uint8_t a, uint8_t b) {
    (void)a; (void)b;
    return 0;
}
#endif

static uint16_t jjfb_rgb565(uint32_t r, uint32_t g, uint32_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
}

static void jjfb_screen_mark_dirty(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (x0 >= x1 || y0 >= y1) return;
    if (!jjfb_screen_dirty) {
        jjfb_screen_dirty_x = (uint32_t)x0;
        jjfb_screen_dirty_y = (uint32_t)y0;
        jjfb_screen_dirty_w = (uint32_t)(x1 - x0);
        jjfb_screen_dirty_h = (uint32_t)(y1 - y0);
    } else {
        uint32_t x2 = jjfb_screen_dirty_x + jjfb_screen_dirty_w;
        uint32_t y2 = jjfb_screen_dirty_y + jjfb_screen_dirty_h;
        if ((uint32_t)x0 < jjfb_screen_dirty_x) jjfb_screen_dirty_x = (uint32_t)x0;
        if ((uint32_t)y0 < jjfb_screen_dirty_y) jjfb_screen_dirty_y = (uint32_t)y0;
        if ((uint32_t)x1 > x2) x2 = (uint32_t)x1;
        if ((uint32_t)y1 > y2) y2 = (uint32_t)y1;
        jjfb_screen_dirty_w = x2 - jjfb_screen_dirty_x;
        jjfb_screen_dirty_h = y2 - jjfb_screen_dirty_y;
    }
    jjfb_screen_dirty = 1;
}

static void jjfb_screen_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    int32_t i, j, x0, y0, x1, y1;
    if (w <= 0 || h <= 0) return;
    x0 = x < 0 ? 0 : x;
    y0 = y < 0 ? 0 : y;
    x1 = x + w;
    y1 = y + h;
    if (x1 > SCREEN_WIDTH) x1 = SCREEN_WIDTH;
    if (y1 > SCREEN_HEIGHT) y1 = SCREEN_HEIGHT;
    if (x0 >= x1 || y0 >= y1) return;
    for (j = y0; j < y1; j++) {
        uint16_t *row = jjfb_screen565 + (uint32_t)j * SCREEN_WIDTH;
        for (i = x0; i < x1; i++)
            row[i] = color;
    }
    jjfb_screen_mark_dirty(x0, y0, x1, y1);
}

static void jjfb_bmp_fill_placeholder(uint16_t *px, int w, int h, const char *name) {
    int x, y;
    uint16_t c0 = jjfb_rgb565(40, 120, 200);
    uint16_t c1 = jjfb_rgb565(220, 180, 40);
    if (name && strstr(name, "loading")) {
        c0 = jjfb_rgb565(30, 90, 200);
        c1 = jjfb_rgb565(80, 200, 255);
    } else if (name && strstr(name, "slogo")) {
        c0 = jjfb_rgb565(20, 20, 40);
        c1 = jjfb_rgb565(240, 200, 60);
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int border = (x < 2 || y < 2 || x >= w - 2 || y >= h - 2);
            int bar = (h > 4 && y > h / 3 && y < (2 * h) / 3 && x < (w * 2) / 3);
            px[y * w + x] = (border || bar) ? c1 : c0;
        }
    }
}

/* key_en: skip pixels == key (RGB565 colorkey, typically 0xF81F magenta). */
static void jjfb_screen_blit_rgb565_key_pitch(const uint16_t *src, int copy_w, int copy_h,
                                              int src_pitch, int dst_x, int dst_y,
                                              uint16_t key, int key_en) {
    int x, y, x0, y0, x1, y1;
    if (!src || copy_w <= 0 || copy_h <= 0 || src_pitch <= 0) return;
    x0 = dst_x < 0 ? 0 : dst_x;
    y0 = dst_y < 0 ? 0 : dst_y;
    x1 = dst_x + copy_w;
    y1 = dst_y + copy_h;
    if (x1 > SCREEN_WIDTH) x1 = SCREEN_WIDTH;
    if (y1 > SCREEN_HEIGHT) y1 = SCREEN_HEIGHT;
    if (x0 >= x1 || y0 >= y1) return;
    for (y = y0; y < y1; y++) {
        const uint16_t *srow = src + (y - dst_y) * src_pitch + (x0 - dst_x);
        uint16_t *drow = jjfb_screen565 + (uint32_t)y * SCREEN_WIDTH + x0;
        for (x = x0; x < x1; x++) {
            uint16_t p = *srow++;
            if (key_en && p == key) {
                drow++;
                continue;
            }
            *drow++ = p;
        }
    }
    jjfb_screen_mark_dirty(x0, y0, x1, y1);
}

static void jjfb_screen_blit_rgb565_key(const uint16_t *src, int src_w, int src_h,
                                        int dst_x, int dst_y, uint16_t key, int key_en) {
    jjfb_screen_blit_rgb565_key_pitch(src, src_w, src_h, src_w, dst_x, dst_y, key, key_en);
}

/* mr_drawBitmap: guest LCD buffer uses SCREEN_WIDTH pitch; copy dirty rect only. */
static void jjfb_screen_copy_guest_lcd(const uint16_t *src, int32_t x, int32_t y,
                                       int32_t w, int32_t h) {
    int32_t i, j, xx, yy;
    if (!src || w <= 0 || h <= 0) return;
    for (j = 0; j < h; j++) {
        yy = y + j;
        if (yy < 0 || yy >= SCREEN_HEIGHT) continue;
        for (i = 0; i < w; i++) {
            xx = x + i;
            if (xx < 0 || xx >= SCREEN_WIDTH) continue;
            jjfb_screen565[(uint32_t)yy * SCREEN_WIDTH + (uint32_t)xx] =
                src[(uint32_t)yy * SCREEN_WIDTH + (uint32_t)xx];
        }
    }
    jjfb_screen_mark_dirty(x, y, x + w, y + h);
    jjfb_debug_present_dirty("mr_drawBitmap");
}

static void jjfb_screen_blit_rgb565(const uint16_t *src, int src_w, int src_h,
                                    int dst_x, int dst_y) {
    jjfb_screen_blit_rgb565_key(src, src_w, src_h, dst_x, dst_y, 0, 0);
}

/* Decide colorkey: env JJFB_COLORKEY=0 off, =F81F force, =auto (default). */
static int jjfb_pick_colorkey(uc_engine *uc, JjfbBmpReq *req, uint16_t *out_key,
                              const char **out_src) {
    const char *ck;
    uint16_t *px;
    uint32_t obj_word = 0;
    int corners_key = 0;
    if (!req || !out_key) return 0;
    ck = getenv("JJFB_COLORKEY");
    if (ck && ck[0] == '0' && ck[1] == 0) return 0;
    if (ck && (ck[0] == 'F' || ck[0] == 'f' || (ck[0] == '0' && ck[1] == 'x'))) {
        *out_key = (uint16_t)strtoul(ck, NULL, 16);
        if (out_src) *out_src = "env";
        return 1;
    }
    /* Prefer obj+0x1C packed key (seen 0xF81FF81F). */
    if (uc && req->guest_object && getMrpMemPtr(req->guest_object + 0x1C)) {
        uc_mem_read(uc, req->guest_object + 0x1C, &obj_word, 4);
        if ((obj_word & 0xFFFF) == 0xF81F || ((obj_word >> 16) & 0xFFFF) == 0xF81F) {
            *out_key = 0xF81F;
            if (out_src) *out_src = "obj+1C";
            return 1;
        }
    }
    px = req->guest_pixels ? (uint16_t *)getMrpMemPtr(req->guest_pixels) : NULL;
    if (px && req->w > 1 && req->h > 1) {
        uint16_t c0 = px[0];
        uint16_t c1 = px[req->w - 1];
        uint16_t c2 = px[(req->h - 1) * req->w];
        uint16_t c3 = px[(req->h - 1) * req->w + (req->w - 1)];
        if (c0 == 0xF81F) corners_key++;
        if (c1 == 0xF81F) corners_key++;
        if (c2 == 0xF81F) corners_key++;
        if (c3 == 0xF81F) corners_key++;
        if (corners_key >= 2) {
            *out_key = 0xF81F;
            if (out_src) *out_src = "corners";
            return 1;
        }
    }
    /* splash assets historically use magenta key even if corners mixed */
    if (req->name[0] && (strstr(req->name, "slogo") || strstr(req->name, "wy_") ||
                      strstr(req->name, "jiantou") || strstr(req->name, "bar"))) {
        *out_key = 0xF81F;
        if (out_src) *out_src = "splash_default";
        return 1;
    }
    return 0;
}

static void jjfb_host_blit_req(uc_engine *uc, JjfbBmpReq *req, int dst_x, int dst_y,
                               const char *tag) {
    uint16_t *src;
    uint16_t key = 0;
    const char *ksrc = "-";
    int key_en;
    uint32_t drawn = 0, skipped = 0;
    int x, y, x0, y0, x1, y1;
    static uint32_t n_key;
    if (!req || !req->guest_pixels || req->w <= 0 || req->h <= 0) return;
    src = (uint16_t *)getMrpMemPtr(req->guest_pixels);
    if (!src) return;
    key_en = jjfb_pick_colorkey(uc, req, &key, &ksrc);
    if (!key_en) {
        jjfb_screen_blit_rgb565(src, req->w, req->h, dst_x, dst_y);
        printf("[JJFB_%s] name=%s x=%d y=%d w=%d h=%d src=0x%X (no key)\n",
               tag, req->name, dst_x, dst_y, req->w, req->h, req->guest_pixels);
        fflush(stdout);
        return;
    }
    x0 = dst_x < 0 ? 0 : dst_x;
    y0 = dst_y < 0 ? 0 : dst_y;
    x1 = dst_x + req->w;
    y1 = dst_y + req->h;
    if (x1 > SCREEN_WIDTH) x1 = SCREEN_WIDTH;
    if (y1 > SCREEN_HEIGHT) y1 = SCREEN_HEIGHT;
    if (x0 < x1 && y0 < y1) {
        for (y = y0; y < y1; y++) {
            const uint16_t *srow = src + (y - dst_y) * req->w + (x0 - dst_x);
            uint16_t *drow = jjfb_screen565 + (uint32_t)y * SCREEN_WIDTH + x0;
            for (x = x0; x < x1; x++) {
                uint16_t p = *srow++;
                if (p == key) { skipped++; drow++; continue; }
                *drow++ = p;
                drawn++;
            }
        }
        jjfb_screen_mark_dirty(x0, y0, x1, y1);
    }
    n_key++;
    if (n_key <= 24 || (n_key % 40) == 0) {
        printf("[JJFB_COLORKEY] name=%s key=0x%04X source=%s\n", req->name, key, ksrc);
        printf("[JJFB_BLIT_KEYED] name=%s x=%d y=%d w=%d h=%d drawn=%u skipped=%u tag=%s\n",
               req->name, dst_x, dst_y, req->w, req->h, drawn, skipped, tag);
        printf("[JJFB_DIRTY_RECT] tag=%s name=%s x=%d y=%d w=%d h=%d\n",
               tag ? tag : "?", req->name, dst_x, dst_y, req->w, req->h);
        fflush(stdout);
    }
}

static void jjfb_debug_present_dirty(const char *from) {
    static uint32_t n;
    if (!jjfb_screen_dirty) return;
    /* v71: keep dirty for flush at DispUpEx/timer; do not show mid-frame black. */
    if (jjfb_present_coalesce)
        return;
    n++;
    guiDrawBitmap(jjfb_screen565,
                   (int32_t)jjfb_screen_dirty_x, (int32_t)jjfb_screen_dirty_y,
                   (int32_t)jjfb_screen_dirty_w, (int32_t)jjfb_screen_dirty_h);
    if (n <= 16 || (n % 40) == 0) {
        printf("[JJFB_DEBUG_PRESENT] from %s dirty %u,%u %ux%u "
               "(host debug; DispUpEx/mrc_refreshScreen untouched)\n",
               from ? from : "?",
               jjfb_screen_dirty_x, jjfb_screen_dirty_y,
               jjfb_screen_dirty_w, jjfb_screen_dirty_h);
        printf("[JJFB_DIRTY_RECT] tag=%s name=- x=%u y=%u w=%u h=%u (present)\n",
               from ? from : "?",
               jjfb_screen_dirty_x, jjfb_screen_dirty_y,
               jjfb_screen_dirty_w, jjfb_screen_dirty_h);
        fflush(stdout);
    }
    jjfb_screen_dirty = 0;
}

/* v71: one SDL present of accumulated dirty rect (or full screen). */
static void jjfb_present_flush(const char *from) {
    static uint32_t n;
    static uint32_t n_drop;
    uint32_t x, y, w, h;
    if (!jjfb_screen_dirty) return;
    /* v73: while B71==0 after Path A gate, keep last SDL frame stable. */
    if (jjfb_present_freeze_enable && jjfb_present_frozen &&
        jjfb_cached_b71 == 0) {
        n_drop++;
        if (n_drop <= 4 || (n_drop % 120) == 0) {
            printf("[JJFB_V73_FREEZE] drop dirty #%u from %s "
                   "(B71=0; keep last SDL frame)\n",
                   n_drop, from ? from : "?");
            fflush(stdout);
        }
        jjfb_screen_dirty = 0;
        return;
    }
    if (jjfb_cached_b71 != 0) {
        jjfb_present_frozen = 0;
        jjfb_present_freeze_armed = 0;
    }
    x = jjfb_screen_dirty_x;
    y = jjfb_screen_dirty_y;
    w = jjfb_screen_dirty_w;
    h = jjfb_screen_dirty_h;
    /* v76: after splash (B71), prefer full-frame present to avoid partial dirty
     * tearing while transition tiles redraw (was 0,40 240x280 flicker). */
    if (jjfb_cached_b71 != 0) {
        x = 0;
        y = 0;
        w = (uint32_t)SCREEN_WIDTH;
        h = (uint32_t)SCREEN_HEIGHT;
    }
    if ((int32_t)w <= 0 || (int32_t)h <= 0) {
        x = 0;
        y = 0;
        w = (uint32_t)SCREEN_WIDTH;
        h = (uint32_t)SCREEN_HEIGHT;
    }
    n++;
    guiDrawBitmap(jjfb_screen565, (int32_t)x, (int32_t)y, (int32_t)w, (int32_t)h);
    if (n <= 8 || (n % 60) == 0) {
        printf("[JJFB_V71_PRESENT] flush #%u from %s dirty %u,%u %ux%u "
               "(coalesce; no mid-frame black)\n",
               n, from ? from : "?", x, y, w, h);
        fflush(stdout);
    }
    jjfb_screen_dirty = 0;
    if (jjfb_present_freeze_enable && jjfb_present_freeze_armed &&
        !jjfb_present_frozen && jjfb_cached_b71 == 0) {
        jjfb_present_frozen = 1;
        printf("[JJFB_V73_FREEZE] frozen after flush #%u from %s "
               "(B71=0 post-leave_2FC26C; until B71 advances)\n",
               n, from ? from : "?");
        fflush(stdout);
    }
}

static void jjfb_host_drawrect_present(int32_t x, int32_t y, int32_t w, int32_t h,
                                       uint32_t r, uint32_t g, uint32_t b) {
    static uint32_t n;
    static uint32_t n_clear;
    /* v80: ALWAYS apply guest clear to host565. Skipping fill caused frame
     * stacking (duplicate bars/tiles). Flicker is avoided only by coalescing
     * SDL present (v71) / freeze (v73) — not by skipping the buffer clear. */
    if (x <= 0 && y <= 0 &&
        w >= (int32_t)SCREEN_WIDTH && h >= (int32_t)SCREEN_HEIGHT &&
        r == 0 && g == 0 && b == 0) {
        n_clear++;
        if (n_clear <= 8 || (n_clear % 60) == 0) {
            printf("[JJFB_V80_PRESENT] fullscreen clear #%u B71=%u "
                   "(buffer wipe; SDL still coalesced)\n",
                   n_clear, jjfb_cached_b71);
            fflush(stdout);
        }
    }
    jjfb_screen_fill_rect(x, y, w, h, jjfb_rgb565(r, g, b));
    if (!jjfb_screen_dirty) return;
    /* v71: fill only; present at DispUpEx/timer flush. */
    if (jjfb_present_coalesce) {
        n++;
        if (n <= 8 || (n % 60) == 0) {
            printf("[JJFB_DRAW] DrawRect coalesce #%u dirty %u,%u %ux%u "
                   "(defer present)\n",
                   n, jjfb_screen_dirty_x, jjfb_screen_dirty_y,
                   jjfb_screen_dirty_w, jjfb_screen_dirty_h);
            fflush(stdout);
        }
        return;
    }
    n++;
    guiDrawBitmap(jjfb_screen565,
                   (int32_t)jjfb_screen_dirty_x, (int32_t)jjfb_screen_dirty_y,
                   (int32_t)jjfb_screen_dirty_w, (int32_t)jjfb_screen_dirty_h);
    if (n <= 8 || (n % 60) == 0) {
        printf("[JJFB_DEBUG_PRESENT] from DrawRect dirty %u,%u %ux%u "
               "(host debug; DispUpEx/mrc_refreshScreen untouched)\n",
               jjfb_screen_dirty_x, jjfb_screen_dirty_y,
               jjfb_screen_dirty_w, jjfb_screen_dirty_h);
        fflush(stdout);
    }
    jjfb_screen_dirty = 0;
}

/* 8x8 font for ASCII 0x20..0x7E; drawn as 8x16 (each row doubled). */
#include "jjfb_font8x8.inc.c"

static void jjfb_screen_draw_ascii_fg(int32_t x, int32_t y, uint8_t ch, uint16_t fg) {
    const uint8_t *g;
    int row, col, yy;
    if (ch < 0x20 || ch > 0x7E) return;
    g = jjfb_font8x8[ch - 0x20];
    for (row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (yy = 0; yy < 2; yy++) {
            int py = y + row * 2 + yy;
            uint16_t *dst;
            if (py < 0 || py >= SCREEN_HEIGHT) continue;
            dst = jjfb_screen565 + (uint32_t)py * SCREEN_WIDTH;
            for (col = 0; col < 8; col++) {
                int px = x + col;
                if (px < 0 || px >= SCREEN_WIDTH) continue;
                if (bits & (0x80u >> col))
                    dst[px] = fg;
            }
        }
    }
    jjfb_screen_mark_dirty(x, y, x + 8, y + 16);
}

static void jjfb_screen_draw_glyph_block(int32_t x, int32_t y, uint16_t fg, uint16_t bg) {
    /* Unknown / non-ASCII placeholder: filled 8x16 with 1px inset. */
    jjfb_screen_fill_rect(x, y, 8, 16, fg);
    if (bg != fg)
        jjfb_screen_fill_rect(x + 1, y + 1, 6, 14, bg);
}

static void jjfb_screen_draw_ascii(int32_t x, int32_t y, uint8_t ch, uint16_t fg, uint16_t bg) {
    const uint8_t *g;
    int row, col, yy;
    int32_t x0, y0, x1, y1;
    if (ch < 0x20 || ch > 0x7E) {
        jjfb_screen_draw_glyph_block(x, y, fg, bg);
        return;
    }
    g = jjfb_font8x8[ch - 0x20];
    x0 = x < 0 ? 0 : x;
    y0 = y < 0 ? 0 : y;
    x1 = x + 8;
    y1 = y + 16;
    if (x1 > SCREEN_WIDTH) x1 = SCREEN_WIDTH;
    if (y1 > SCREEN_HEIGHT) y1 = SCREEN_HEIGHT;
    if (x0 >= x1 || y0 >= y1) return;
    for (row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (yy = 0; yy < 2; yy++) {
            int py = y + row * 2 + yy;
            uint16_t *dst;
            if (py < 0 || py >= SCREEN_HEIGHT) continue;
            dst = jjfb_screen565 + (uint32_t)py * SCREEN_WIDTH;
            for (col = 0; col < 8; col++) {
                int px = x + col;
                if (px < 0 || px >= SCREEN_WIDTH) continue;
                dst[px] = (bits & (0x80u >> col)) ? fg : bg;
            }
        }
    }
    jjfb_screen_mark_dirty(x0, y0, x1, y1);
}

static uint32_t jjfb_hash32(const uint8_t *p, uint32_t n) {
    uint32_t h = 2166136261u;
    uint32_t i;
    for (i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/* Dump each distinct code object once (ptr + content hash). */
static void jjfb_dump_code_once(uc_engine *uc, uint32_t code) {
    static uint32_t seen_ptr[32];
    static uint32_t seen_hash[32];
    static uint32_t seen_n;
    uint8_t buf[0x100];
    uint32_t hash, i, tag0 = 0, f0c = 0, h10 = 0, len = 0, ref34 = 0;
    char ascii[96];
    uint32_t alen = 0;
    if (!code || !getMrpMemPtr(code)) return;
    memset(buf, 0, sizeof(buf));
    if (uc_mem_read(uc, code, buf, sizeof(buf)) != UC_ERR_OK) return;
    hash = jjfb_hash32(buf, sizeof(buf));
    for (i = 0; i < seen_n; i++) {
        if (seen_ptr[i] == code && seen_hash[i] == hash)
            return;
    }
    if (seen_n < 32) {
        seen_ptr[seen_n] = code;
        seen_hash[seen_n] = hash;
        seen_n++;
    }
    memcpy(&tag0, buf + 0x00, 4);
    memcpy(&f0c, buf + 0x0C, 4);
    memcpy(&h10, buf + 0x10, 4);
    memcpy(&len, buf + 0x14, 4);
    memcpy(&ref34, buf + 0x34, 4);
    for (i = 0; i < 80 && i < sizeof(buf) - 0x18; i++) {
        uint8_t c = buf[0x18 + i];
        if (c == 0) break;
        ascii[alen++] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
    }
    ascii[alen] = 0;
    if (!jjfb_verbose_logs())
        return;
    printf("[JJFB_CODE_OBJ] ptr=0x%X hash=0x%08X tag=0x%X +0C=%u +10=0x%X "
           "len=+14:%u ref34=0x%X ascii=\"%s\"\n",
           code, hash, tag0, f0c, h10, len, ref34, ascii);
    jjfb_dump_guest_ex(uc, "11F00_code100", code, 0x100);
    fflush(stdout);
}

static uint32_t jjfb_b64_val(uint8_t c) {
    if (c >= 'A' && c <= 'Z') return (uint32_t)(c - 'A');
    if (c >= 'a' && c <= 'z') return (uint32_t)(c - 'a' + 26);
    if (c >= '0' && c <= '9') return (uint32_t)(c - '0' + 52);
    if (c == '+') return 62;
    if (c == '/') return 63;
    return 0x100;
}

/* Decode base64 payload in-place to out[]; returns decoded length (0 if not b64). */
static uint32_t jjfb_try_b64_decode(const uint8_t *in, uint32_t in_len,
                                    uint8_t *out, uint32_t out_cap) {
    uint32_t i = 0, o = 0, pad = 0, n = 0;
    uint32_t acc = 0;
    if (in_len < 4 || (in_len & 3)) return 0;
    for (i = 0; i < in_len; i++) {
        uint8_t c = in[i];
        uint32_t v;
        if (c == '=') {
            pad++;
            v = 0;
        } else {
            v = jjfb_b64_val(c);
            if (v > 63) return 0;
        }
        acc = (acc << 6) | v;
        n++;
        if (n == 4) {
            if (o < out_cap) out[o++] = (uint8_t)((acc >> 16) & 0xFF);
            if (pad < 2 && o < out_cap) out[o++] = (uint8_t)((acc >> 8) & 0xFF);
            if (pad < 1 && o < out_cap) out[o++] = (uint8_t)(acc & 0xFF);
            acc = 0;
            n = 0;
        }
    }
    return o;
}

static int jjfb_payload_printable_ratio(const uint8_t *p, uint32_t n) {
    uint32_t i, ok = 0;
    if (!n) return 0;
    for (i = 0; i < n; i++) {
        if (p[i] >= 0x20 && p[i] <= 0x7E) ok++;
        else if (p[i] == 0) break;
    }
    return (int)((ok * 100) / n);
}

/* Scan guest memory for C-string looking ASCII / GBK and log first-screen hits. */
static void jjfb_scan_guest_cstr(uc_engine *uc, uint32_t addr, const char *tag) {
    char buf[96];
    uint32_t i, n = 0;
    int has_ascii = 0, has_gbk = 0;
    if (!addr || !getMrpMemPtr(addr)) return;
    memset(buf, 0, sizeof(buf));
    if (uc_mem_read(uc, addr, buf, sizeof(buf) - 1) != UC_ERR_OK) return;
    for (i = 0; i < sizeof(buf) - 1 && buf[i]; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c >= 0x20 && c < 0x7F) has_ascii = 1;
        else if (c >= 0x81) has_gbk = 1;
        else if (c < 0x20 && c != '\t') break;
        n++;
    }
    if (n < 2) return;
    if (has_gbk || (has_ascii && (jjfb_is_firstscreen_name(buf) ||
                                  strstr(buf, "login") || strstr(buf, "load") ||
                                  strstr(buf, "start") || strstr(buf, "logo")))) {
        printf("[JJFB_FIRST_SCREEN] text_%s @0x%X \"%.*s\"\n", tag, addr, (int)n, buf);
        fflush(stdout);
    }
}

/* 0x11F00 via 0x2f2358: drawText / glyphDraw.
 *   app=7, code=rich-text/glyph object (NOT a C string)
 *   p0 -> { i16 y@+0, i16 x@+2, RGB888@+0x2C, ... }
 * v86: default NO glyph_block (blue tiles). Prefer gb16.uc2 into host565.
 * Never host overlay / fake "BOOTING" text. */
static void jjfb_plat_11f00(uc_engine *uc, uint32_t app, uint32_t code,
                            uint32_t param0, uint32_t param1,
                            uint32_t param2, uint32_t param3) {
    int16_t y = 0, x = 0;
    uint16_t fg = jjfb_rgb565(255, 255, 255);
    uint32_t nchars = 0, len_field = 0, p0_n = 0;
    uint8_t payload[128];
    uint8_t decoded[96];
    uint32_t ascii_n = 0, block_n = 0, dec_n = 0, gb16_n = 0, skip_n = 0;
    const uint8_t *draw_bytes;
    uint32_t draw_n;
    int32_t cx, cy, drawn_w;
    static uint32_t n11;
    static int logged_policy;
    const char *env_block, *env_gb16;
    int allow_block, use_gb16;
    uint32_t i;
    n11++;
    memset(payload, 0, sizeof(payload));
    memset(decoded, 0, sizeof(decoded));

    /* v86: JJFB_11F00_GLYPH_BLOCK=1 to restore blue placeholders (debug only).
     * JJFB_11F00_GB16=0 to disable sky16 host draw. */
    env_block = getenv("JJFB_11F00_GLYPH_BLOCK");
    env_gb16 = getenv("JJFB_11F00_GB16");
    allow_block = (env_block && env_block[0] == '1');
    use_gb16 = !(env_gb16 && env_gb16[0] == '0');
    if (!logged_policy) {
        logged_policy = 1;
        printf("[JJFB_V86_11F00] glyph_block=%d gb16=%d "
               "(default: block OFF, gb16 ON)\n",
               allow_block, use_gb16);
        fflush(stdout);
    }

    if (param0 && getMrpMemPtr(param0)) {
        uint8_t rgb[4];
        uc_mem_read(uc, param0, &y, 2);
        uc_mem_read(uc, param0 + 2, &x, 2);
        uc_mem_read(uc, param0 + 0x38, &p0_n, 4);
        if (uc_mem_read(uc, param0 + 0x2C, rgb, 4) == UC_ERR_OK &&
            (rgb[0] | rgb[1] | rgb[2])) {
            fg = jjfb_rgb565(rgb[0], rgb[1], rgb[2]);
        }
    }

    if (code && getMrpMemPtr(code)) {
        uint32_t ptrs[6];
        jjfb_dump_code_once(uc, code);
        uc_mem_read(uc, code + 0x14, &len_field, 4);
        if (len_field > 0 && len_field < sizeof(payload)) {
            nchars = len_field;
            uc_mem_read(uc, code + 0x18, payload, nchars);
        } else if (p0_n > 0 && p0_n < 64) {
            nchars = p0_n;
        } else {
            nchars = 6;
        }
        memset(ptrs, 0, sizeof(ptrs));
        uc_mem_read(uc, code + 0x40, ptrs, sizeof(ptrs));
        for (i = 0; i < 6; i++) {
            if (ptrs[i] > 0x1000 && ptrs[i] < 0x800000)
                jjfb_scan_guest_cstr(uc, ptrs[i], "code_ptr");
        }
        jjfb_scan_guest_cstr(uc, code + 0x18, "code_payload");
    } else if (p0_n > 0 && p0_n < 64) {
        nchars = p0_n;
    } else {
        nchars = 6;
    }
    if (nchars == 0) nchars = 1;
    if (nchars > 48) nchars = 48;

    draw_bytes = payload;
    draw_n = nchars;
    dec_n = jjfb_try_b64_decode(payload, nchars, decoded, sizeof(decoded));
    if (dec_n > 0) {
        int pr_raw = jjfb_payload_printable_ratio(payload, nchars);
        int pr_dec = jjfb_payload_printable_ratio(decoded, dec_n);
        if (n11 <= 4) {
            printf("[JJFB_CODE_B64] raw_len=%u dec_len=%u printable%% raw=%d dec=%d\n",
                   nchars, dec_n, pr_raw, pr_dec);
            fflush(stdout);
        }
        if (pr_dec >= 50 && pr_dec > pr_raw) {
            draw_bytes = decoded;
            draw_n = dec_n > 48 ? 48 : dec_n;
        }
    }

    /* Rich-text objects often stash pointers at +0x18 (len can be 0xC0).
     * Do not treat that blob as GBK glyphs — that caused blue tiles before. */
    {
        int nul = 0, i2;
        int pr = jjfb_payload_printable_ratio(draw_bytes, draw_n > 32 ? 32 : draw_n);
        for (i2 = 0; i2 < (int)draw_n && i2 < 32; i2++)
            if (draw_bytes[i2] == 0) nul++;
        if (len_field > 64 || pr < 30 || nul >= 6) {
            if (n11 <= 12 || (n11 % 40) == 0) {
                printf("[JJFB_DRAW_TEXT] 11F00 #%u xy=%d,%d skip_binary_obj "
                       "len_field=%u printable%%=%d nul=%d (no glyph_block)\n",
                       n11, (int)x, (int)y, len_field, pr, nul);
                fflush(stdout);
            }
            /* Rich-text objs are scene graphs (not flat GBK/UCS2). Do not
             * speculate-draw from +0xA8 heap ptrs — that painted garbage
             * (head=FBCA…) over status rows. Real glyphs need object walk
             * or guest platDrawChar; platDrawChar bridge is now implemented. */
            if (n11 <= 4) {
                printf("[JJFB_V86_11F00] skip rich-text obj (no speculative "
                       "string blit); platDrawChar bridge armed\n");
                fflush(stdout);
            }
            (void)use_gb16;
            (void)param2;
            (void)param3;
            return;
        }
    }

    cx = x;
    cy = y;
    ascii_n = 0;
    block_n = 0;
    for (i = 0; i < draw_n; i++) {
        uint8_t ch = draw_bytes[i];
        if (ch >= 0x20 && ch <= 0x7E) {
            jjfb_screen_draw_ascii_fg(cx, cy, ch, fg);
            ascii_n++;
            cx += 8;
        } else if (ch >= 0x81 && i + 1 < draw_n) {
            uint8_t c2 = draw_bytes[i + 1];
            uint16_t uni = 0;
            int drew = 0;
            if (use_gb16) {
                uni = jjfb_gbk_to_unicode(ch, c2);
                if (uni) {
                    jjfb_host_draw_sky16(uni, cx, cy, fg);
                    gb16_n++;
                    drew = 1;
                }
            }
            if (!drew && allow_block) {
                jjfb_screen_draw_glyph_block(cx, cy, fg, jjfb_rgb565(0, 0, 80));
                jjfb_screen_draw_glyph_block(cx + 8, cy, fg, jjfb_rgb565(0, 0, 80));
                block_n += 2;
            } else if (!drew) {
                skip_n++;
            }
            cx += 16;
            i++;
        } else {
            if (allow_block) {
                jjfb_screen_draw_glyph_block(cx, cy, fg, jjfb_rgb565(0, 0, 80));
                block_n++;
            } else {
                skip_n++;
            }
            cx += 8;
        }
        if (cx >= SCREEN_WIDTH) {
            cx = x;
            cy += 16;
            if (cy >= SCREEN_HEIGHT) break;
        }
    }
    drawn_w = (int32_t)(cx - x);
    if (drawn_w < 0) drawn_w = 0;
    if (drawn_w > SCREEN_WIDTH - x) drawn_w = SCREEN_WIDTH - x;

    if (n11 <= 12 || (n11 % 40) == 0) {
        printf("[JJFB_DRAW_TEXT] 11F00 #%u app=0x%X code=0x%X xy=%d,%d glyphs=%u "
               "ascii=%u gb16=%u block=%u skip=%u %dx16 p1=0x%X dirty=%u\n",
               n11, app, code, (int)x, (int)y, draw_n, ascii_n, gb16_n, block_n,
               skip_n, drawn_w, param1, jjfb_screen_dirty);
        if (ascii_n)
            printf("[JJFB_DRAW_TEXT_ASCII] #%u n=%u sample=\"%.*s\"\n",
                   n11, ascii_n, (int)draw_n, draw_bytes);
        if (n11 <= 4 && param0)
            jjfb_dump_guest(uc, "11F00_p0", param0, 64);
        fflush(stdout);
    }
    if (ascii_n >= 3)
        jjfb_note_firstscreen("11f00_ascii", (const char *)draw_bytes);

    if (jjfb_screen_dirty && !jjfb_present_coalesce) {
        guiDrawBitmap(jjfb_screen565,
                       (int32_t)jjfb_screen_dirty_x, (int32_t)jjfb_screen_dirty_y,
                       (int32_t)jjfb_screen_dirty_w, (int32_t)jjfb_screen_dirty_h);
        jjfb_screen_dirty = 0;
    }
    (void)uc;
    (void)param2;
    (void)param3;
}

static int jjfb_ui_hooks_installed;
static uc_hook jjfb_ui_hooks[48];
static uc_hook jjfb_ctx_mem_hook;
static int jjfb_ctx_mem_hook_installed;
/* v55: natural ui_mode=0x45 writer chain coverage (NO FORCE). */
static int jjfb_uimode_writer_hooks_installed;
static uc_hook jjfb_uimode_writer_hooks[96];
static uc_hook jjfb_v62_flag_mem_hooks[3];
static int jjfb_v62_flag_hooks_installed;
static int jjfb_allow_chrome;          /* env: JJFB_ALLOW_CHROME=1 */
static int jjfb_chrome_skip_310bb4 = 1;/* env: JJFB_CHROME_SKIP_310BB4=0 to observe */
static uint32_t jjfb_chrome_allow_calls = 0; /* env: JJFB_CHROME_ALLOW_CALLS (default 30 when enabled) */
static uint32_t jjfb_310bb4_dump_n = 0; /* env: JJFB_310BB4_DUMP_N (default 1 when enabled) */
static uint32_t jjfb_chrome_allow_calls_init;
static uc_hook jjfb_310bb4_sweep_hook;
static int jjfb_310bb4_sweep_installed;
static uint32_t jjfb_310bb4_sweep_hits;

/* v48: 0x2EF86C..0x2EFD00 basic-block coverage */
#define JJFB_SPLASH_COV_LO 0x2EF86C
#define JJFB_SPLASH_COV_HI 0x2EFD00
#define JJFB_SPLASH_COV_N  ((JJFB_SPLASH_COV_HI - JJFB_SPLASH_COV_LO) / 2)
static uint16_t jjfb_splash_cov[JJFB_SPLASH_COV_N];
static uint32_t jjfb_splash_cov_max_pc;
static uint32_t jjfb_splash_cov_last_pc;
static uint32_t jjfb_splash_cov_last_lr;
static uint32_t jjfb_2efc40_hits;
static uint32_t jjfb_2efc_gate_last_skip_pc;
static int jjfb_splash_cov_installed;
static uc_hook jjfb_splash_cov_hook;
static int jjfb_2efc_tail_forced;
static int jjfb_force_r4_done;
static int jjfb_force_b6c_done;
static int jjfb_force_134d_done;
static uint32_t jjfb_2efb00_hits;
static uint32_t jjfb_2efb06_hits;
static int jjfb_2efc_disasm_dumped;

static void jjfb_dump_2efc_tail_disasm(uc_engine *uc) {
    uint8_t buf[0x60];
    uint32_t i, base = 0x2EFC40;
    if (jjfb_2efc_disasm_dumped || !uc || !getMrpMemPtr(base)) return;
    if (uc_mem_read(uc, base, buf, sizeof(buf)) != UC_ERR_OK) return;
    jjfb_2efc_disasm_dumped = 1;
    printf("[JJFB_2EFC_DISASM] bytes 0x2EFC40..0x2EFC9F:\n");
    for (i = 0; i < sizeof(buf); i++) {
        if ((i % 16) == 0) printf("  %08X:", base + i);
        printf(" %02X", buf[i]);
        if ((i % 16) == 15) printf("\n");
    }
    if ((sizeof(buf) % 16) != 0) printf("\n");
    printf("[JJFB_2EFC_DISASM] NOTE: 0x2EFC58=word 0xBA0 and 0x2EFC6C=word 0xAC8 are "
           "LITERAL POOL data (after B @0x2EFC54), not STR writers.\n");
    /* Dump gate zone: r4 is already 0 by ~0x2EFA58; CMP/BEQ at 0x2EFAF2/F4. */
    {
        uint8_t epi[0xB0];
        uint32_t eb = 0x2EFA50;
        if (getMrpMemPtr(eb) && uc_mem_read(uc, eb, epi, sizeof(epi)) == UC_ERR_OK) {
            printf("[JJFB_2EFC_DISASM] gate bytes 0x2EFA50..0x2EFAFF:\n");
            for (i = 0; i < 0xB0; i++) {
                if ((i % 16) == 0) printf("  %08X:", eb + i);
                printf(" %02X", epi[i]);
                if ((i % 16) == 15) printf("\n");
            }
            printf("\n");
            /* Thumb decode CMP/BEQ/MOV/LDR around r4 gate. */
            for (i = 0; i + 1 < 0xB0; ) {
                uint16_t h = (uint16_t)epi[i] | ((uint16_t)epi[i + 1] << 8);
                uint32_t pc = eb + i;
                if ((h & 0xF800) == 0x2000) {
                    /* MOVS Rd,#imm8 */
                    printf("[JJFB_GATE_DISASM] %08X: MOVS r%u,#0x%X\n", pc, (h >> 8) & 7, h & 0xFF);
                    i += 2;
                } else if ((h & 0xFFC0) == 0x0000 && ((h >> 6) & 0x1F) == 0) {
                    /* LSLS Rd,Rm,#0 == MOVS Rd,Rm */
                    printf("[JJFB_GATE_DISASM] %08X: MOVS r%u,r%u (lsl#0)\n",
                           pc, h & 7, (h >> 3) & 7);
                    i += 2;
                } else if ((h & 0xF800) == 0x2800) {
                    printf("[JJFB_GATE_DISASM] %08X: CMP r%u,#0x%X\n",
                           pc, (h >> 8) & 7, h & 0xFF);
                    i += 2;
                } else if ((h & 0xF000) == 0xD000 && ((h >> 8) & 0xF) != 0xF &&
                           ((h >> 8) & 0xF) != 0xE) {
                    int32_t imm = (int8_t)(h & 0xFF);
                    printf("[JJFB_GATE_DISASM] %08X: Bcond cond=%u -> 0x%X\n",
                           pc, (h >> 8) & 0xF, pc + 4 + imm * 2);
                    i += 2;
                } else if ((h & 0xF800) == 0x6800 || (h & 0xF800) == 0x6000) {
                    uint32_t imm = ((h >> 6) & 0x1F) * 4;
                    printf("[JJFB_GATE_DISASM] %08X: %s r%u,[r%u,#0x%X]\n",
                           pc, (h & 0x0800) ? "LDR" : "STR", h & 7, (h >> 3) & 7, imm);
                    i += 2;
                } else if ((h & 0xFF00) == 0x4700) {
                    printf("[JJFB_GATE_DISASM] %08X: BX/BLX r%u\n", pc, (h >> 3) & 0xF);
                    i += 2;
                } else if ((h & 0xF800) == 0xE000) {
                    int32_t imm = (int32_t)(h & 0x7FF);
                    if (imm & 0x400) imm |= ~0x7FF;
                    printf("[JJFB_GATE_DISASM] %08X: B -> 0x%X\n", pc, pc + 4 + imm * 2);
                    i += 2;
                } else {
                    i += 2;
                }
            }
        }
    }
    /* Deep path after gate: 0x2EFB00..0x2EFB7F (FORCE_R4 stops ~0x2EFB04; B6C BNE @0x2EFB06). */
    {
        uint8_t deep[0x80];
        uint32_t db = 0x2EFB00;
        if (getMrpMemPtr(db) && uc_mem_read(uc, db, deep, sizeof(deep)) == UC_ERR_OK) {
            printf("[JJFB_2EFC_DISASM] deep bytes 0x2EFB00..0x2EFB7F:\n");
            for (i = 0; i < sizeof(deep); i++) {
                if ((i % 16) == 0) printf("  %08X:", db + i);
                printf(" %02X", deep[i]);
                if ((i % 16) == 15) printf("\n");
            }
            printf("\n");
            for (i = 0; i + 1 < sizeof(deep); ) {
                uint16_t h = (uint16_t)deep[i] | ((uint16_t)deep[i + 1] << 8);
                uint32_t pc = db + i;
                if ((h & 0xF800) == 0x2000) {
                    printf("[JJFB_DEEP_DISASM] %08X: MOVS r%u,#0x%X\n", pc, (h >> 8) & 7, h & 0xFF);
                    i += 2;
                } else if ((h & 0xF800) == 0x2800) {
                    printf("[JJFB_DEEP_DISASM] %08X: CMP r%u,#0x%X\n", pc, (h >> 8) & 7, h & 0xFF);
                    i += 2;
                } else if ((h & 0xF000) == 0xD000 && ((h >> 8) & 0xF) != 0xF &&
                           ((h >> 8) & 0xF) != 0xE) {
                    int32_t imm = (int8_t)(h & 0xFF);
                    printf("[JJFB_DEEP_DISASM] %08X: Bcond cond=%u -> 0x%X\n",
                           pc, (h >> 8) & 0xF, pc + 4 + imm * 2);
                    i += 2;
                } else if ((h & 0xF800) == 0xE000) {
                    int32_t imm = (int32_t)(h & 0x7FF);
                    if (imm & 0x400) imm |= ~0x7FF;
                    printf("[JJFB_DEEP_DISASM] %08X: B -> 0x%X\n", pc, pc + 4 + imm * 2);
                    i += 2;
                } else if ((h & 0xF800) == 0x6800 || (h & 0xF800) == 0x6000) {
                    uint32_t imm = ((h >> 6) & 0x1F) * 4;
                    printf("[JJFB_DEEP_DISASM] %08X: %s r%u,[r%u,#0x%X]\n",
                           pc, (h & 0x0800) ? "LDR" : "STR", h & 7, (h >> 3) & 7, imm);
                    i += 2;
                } else if ((h & 0xFF00) == 0x4700) {
                    printf("[JJFB_DEEP_DISASM] %08X: BX/BLX r%u\n", pc, (h >> 3) & 0xF);
                    i += 2;
                } else if ((h & 0xF800) == 0x4800) {
                    uint32_t imm = (h & 0xFF) << 2;
                    uint32_t rd = (h >> 8) & 7;
                    uint32_t lit = ((pc + 4) & ~3u) + imm;
                    uint32_t val = 0;
                    if (getMrpMemPtr(lit)) uc_mem_read(uc, lit, &val, 4);
                    printf("[JJFB_DEEP_DISASM] %08X: LDR r%u,[pc,#0x%X] lit@0x%X=0x%X\n",
                           pc, rd, imm, lit, val);
                    i += 2;
                } else if ((h & 0xFF00) == 0x4400) {
                    uint32_t rm = (h >> 3) & 0xF;
                    uint32_t rd = ((h >> 4) & 0x8) | (h & 7);
                    printf("[JJFB_DEEP_DISASM] %08X: ADD r%u,r%u\n", pc, rd, rm);
                    i += 2;
                } else if ((h & 0xFF00) == 0x1C00) {
                    printf("[JJFB_DEEP_DISASM] %08X: ADDS r%u,r%u,#%u\n",
                           pc, h & 7, (h >> 3) & 7, (h >> 6) & 7);
                    i += 2;
                } else {
                    i += 2;
                }
            }
        }
    }
    /* AC8-tail: 0x2EFBA0..0x2EFC3F - natural stop ~0x2EFC00 after loading ERW+AC8. */
    {
        uint8_t tail[0xA0];
        uint32_t tb = 0x2EFBA0;
        if (getMrpMemPtr(tb) && uc_mem_read(uc, tb, tail, sizeof(tail)) == UC_ERR_OK) {
            printf("[JJFB_2EFC_DISASM] AC8-tail bytes 0x2EFBA0..0x2EFC3F:\n");
            for (i = 0; i < sizeof(tail); i++) {
                if ((i % 16) == 0) printf("  %08X:", tb + i);
                printf(" %02X", tail[i]);
                if ((i % 16) == 15) printf("\n");
            }
            printf("\n");
            for (i = 0; i + 1 < sizeof(tail); ) {
                uint16_t h = (uint16_t)tail[i] | ((uint16_t)tail[i + 1] << 8);
                uint32_t pc = tb + i;
                if ((h & 0xF800) == 0x2000) {
                    printf("[JJFB_AC8TAIL_DISASM] %08X: MOVS r%u,#0x%X\n",
                           pc, (h >> 8) & 7, h & 0xFF);
                    i += 2;
                } else if ((h & 0xF800) == 0x2800) {
                    printf("[JJFB_AC8TAIL_DISASM] %08X: CMP r%u,#0x%X\n",
                           pc, (h >> 8) & 7, h & 0xFF);
                    i += 2;
                } else if ((h & 0xF000) == 0xD000 && ((h >> 8) & 0xF) != 0xF &&
                           ((h >> 8) & 0xF) != 0xE) {
                    int32_t imm = (int8_t)(h & 0xFF);
                    printf("[JJFB_AC8TAIL_DISASM] %08X: Bcond cond=%u -> 0x%X\n",
                           pc, (h >> 8) & 0xF, pc + 4 + imm * 2);
                    i += 2;
                } else if ((h & 0xF800) == 0xE000) {
                    int32_t imm = (int32_t)(h & 0x7FF);
                    if (imm & 0x400) imm |= ~0x7FF;
                    printf("[JJFB_AC8TAIL_DISASM] %08X: B -> 0x%X\n", pc, pc + 4 + imm * 2);
                    i += 2;
                } else if ((h & 0xF800) == 0x6800 || (h & 0xF800) == 0x6000) {
                    uint32_t imm = ((h >> 6) & 0x1F) * 4;
                    printf("[JJFB_AC8TAIL_DISASM] %08X: %s r%u,[r%u,#0x%X]\n",
                           pc, (h & 0x0800) ? "LDR" : "STR", h & 7, (h >> 3) & 7, imm);
                    i += 2;
                } else if ((h & 0xFF00) == 0x4700) {
                    printf("[JJFB_AC8TAIL_DISASM] %08X: BX/BLX r%u\n", pc, (h >> 3) & 0xF);
                    i += 2;
                } else if ((h & 0xF800) == 0x4800) {
                    uint32_t imm = (h & 0xFF) << 2;
                    uint32_t rd = (h >> 8) & 7;
                    uint32_t lit = ((pc + 4) & ~3u) + imm;
                    uint32_t val = 0;
                    if (getMrpMemPtr(lit)) uc_mem_read(uc, lit, &val, 4);
                    printf("[JJFB_AC8TAIL_DISASM] %08X: LDR r%u,[pc,#0x%X] lit@0x%X=0x%X\n",
                           pc, rd, imm, lit, val);
                    i += 2;
                } else if ((h & 0xFF00) == 0x4400) {
                    uint32_t rm = (h >> 3) & 0xF;
                    uint32_t rd = ((h >> 4) & 0x8) | (h & 7);
                    printf("[JJFB_AC8TAIL_DISASM] %08X: ADD r%u,r%u\n", pc, rd, rm);
                    i += 2;
                } else {
                    i += 2;
                }
            }
        }
    }
    /* Quick Thumb decode for STR/LDR/B patterns (16-bit only). */
    for (i = 0; i + 1 < sizeof(buf); ) {
        uint16_t h = (uint16_t)buf[i] | ((uint16_t)buf[i + 1] << 8);
        uint32_t pc = base + i;
        if ((h & 0xF800) == 0xE000) {
            /* B (uncond T2): imm11 */
            int32_t imm = (int32_t)(h & 0x7FF);
            if (imm & 0x400) imm |= ~0x7FF;
            printf("[JJFB_2EFC_DISASM] %08X: B -> 0x%X (h=0x%04X)\n",
                   pc, pc + 4 + imm * 2, h);
            i += 2;
        } else if ((h & 0xF000) == 0xD000 && ((h >> 8) & 0xF) != 0xF && ((h >> 8) & 0xF) != 0xE) {
            int32_t imm = (int8_t)(h & 0xFF);
            uint32_t cond = (h >> 8) & 0xF;
            printf("[JJFB_2EFC_DISASM] %08X: Bcond cond=%u -> 0x%X (h=0x%04X)\n",
                   pc, cond, pc + 4 + imm * 2, h);
            i += 2;
        } else if ((h & 0xF800) == 0x6000 || (h & 0xF800) == 0x6800) {
            /* STR/LDR Rd,[Rn,#imm5] */
            uint32_t imm = ((h >> 6) & 0x1F) * 4;
            uint32_t rn = (h >> 3) & 7, rd = h & 7;
            printf("[JJFB_2EFC_DISASM] %08X: %s r%u,[r%u,#0x%X] (h=0x%04X)\n",
                   pc, ((h & 0x0800) ? "LDR" : "STR"), rd, rn, imm, h);
            i += 2;
        } else if ((h & 0xFF00) == 0x4700) {
            printf("[JJFB_2EFC_DISASM] %08X: BX/BLX r%u (h=0x%04X)\n", pc, (h >> 3) & 0xF, h);
            i += 2;
        } else if ((h & 0xF800) == 0x4800) {
            uint32_t imm = (h & 0xFF) << 2;
            uint32_t rd = (h >> 8) & 7;
            uint32_t lit = (pc & ~2u) + 4 + imm;
            uint32_t val = 0;
            if (getMrpMemPtr(lit)) uc_mem_read(uc, lit, &val, 4);
            printf("[JJFB_2EFC_DISASM] %08X: LDR r%u,[pc,#0x%X] lit@0x%X=0x%X\n",
                   pc, rd, imm, lit, val);
            i += 2;
        } else {
            /* Thumb-2 32-bit? if high half looks like it */
            if ((h & 0xE000) == 0xE000 && (h & 0x1800) != 0) {
                uint16_t h2 = 0;
                if (i + 3 < sizeof(buf))
                    h2 = (uint16_t)buf[i + 2] | ((uint16_t)buf[i + 3] << 8);
                printf("[JJFB_2EFC_DISASM] %08X: T32? 0x%04X 0x%04X\n", pc, h, h2);
                i += 4;
            } else {
                printf("[JJFB_2EFC_DISASM] %08X: h=0x%04X\n", pc, h);
                i += 2;
            }
        }
    }
    fflush(stdout);
}

static void jjfb_splash_cov_summary(void) {
    uint32_t i, blocks = 0, first_miss = 0, last_hit = 0;
    int have_miss = 0;
    for (i = 0; i < JJFB_SPLASH_COV_N; i++) {
        if (jjfb_splash_cov[i]) {
            blocks++;
            last_hit = JJFB_SPLASH_COV_LO + i * 2;
        } else if (!have_miss && i > 0 && jjfb_splash_cov[i - 1]) {
            first_miss = JJFB_SPLASH_COV_LO + i * 2;
            have_miss = 1;
        }
    }
    printf("[JJFB_2EF86C_COV] summary hit_halfwords=%u/%u max_pc=0x%X last_pc=0x%X "
           "2EFB00_hits=%u 2EFB06_hits=%u 2EFC40_hits=%u first_gap_after_hit=0x%X "
           "force_r4=%d force_b6c=%d\n",
           blocks, (unsigned)JJFB_SPLASH_COV_N, jjfb_splash_cov_max_pc,
           jjfb_splash_cov_last_pc, jjfb_2efb00_hits, jjfb_2efb06_hits, jjfb_2efc40_hits,
           have_miss ? first_miss : 0, jjfb_force_r4_done, jjfb_force_b6c_done);
    if (jjfb_2efc40_hits == 0)
        printf("[JJFB_2EFC_GATE] not reached, last_pc=0x%X last_lr=0x%X "
               "skip_hint_pc=0x%X 2EFB00_hits=%u 2EFB06_hits=%u\n",
               jjfb_splash_cov_last_pc, jjfb_splash_cov_last_lr,
               jjfb_2efc_gate_last_skip_pc, jjfb_2efb00_hits, jjfb_2efb06_hits);
    fflush(stdout);
}

static void jjfb_hook_splash_cov(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t idx, lr = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0;
    const char *force;
    (void)size; (void)user_data;
    if (pc < JJFB_SPLASH_COV_LO || pc >= JJFB_SPLASH_COV_HI) return;
    idx = (pc - JJFB_SPLASH_COV_LO) / 2;
    if (idx >= JJFB_SPLASH_COV_N) return;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    jjfb_splash_cov_last_pc = pc;
    jjfb_splash_cov_last_lr = lr;
    if (pc > jjfb_splash_cov_max_pc)
        jjfb_splash_cov_max_pc = pc;
    if (jjfb_splash_cov[idx] < 0xFFFF)
        jjfb_splash_cov[idx]++;
    if (jjfb_splash_cov[idx] == 1 && jjfb_verbose_logs()) {
        /* First hit of this halfword ??log sparse + always for tail/gate zone. */
        if (pc >= 0x2EFA50 || pc >= 0x2EFC00 || (idx % 8) == 0 || pc <= 0x2EF880) {
            uc_reg_read(uc, UC_ARM_REG_R0, &r0);
            uc_reg_read(uc, UC_ARM_REG_R1, &r1);
            uc_reg_read(uc, UC_ARM_REG_R2, &r2);
            uc_reg_read(uc, UC_ARM_REG_R3, &r3);
            uc_reg_read(uc, UC_ARM_REG_R4, &r4);
            uc_reg_read(uc, UC_ARM_REG_R5, &r5);
            uc_reg_read(uc, UC_ARM_REG_R6, &r6);
            uc_reg_read(uc, UC_ARM_REG_R7, &r7);
            printf("[JJFB_2EF86C_COV] first pc=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                   "r4=0x%X r5=0x%X r6=0x%X r7=0x%X\n",
                   pc, lr, r0, r1, r2, r3, r4, r5, r6, r7);
            fflush(stdout);
        }
    }
    /* r4 gate: log on r4 change in 0x2EFA50..0x2EFB20, and always at CMP/BEQ. */
    if (jjfb_verbose_logs() && pc >= 0x2EFA50 && pc <= 0x2EFB20) {
        static uint32_t s_last_r4 = 0xFFFFFFFFu;
        static uint32_t s_r4_logs;
        uc_reg_read(uc, UC_ARM_REG_R4, &r4);
        if (r4 != s_last_r4 || pc == 0x2EFAF0 || pc == 0x2EFAF2 || pc == 0x2EFAF4 ||
            pc == 0x2EFB00 || (pc & 0xF) == 0) {
            if (s_r4_logs < 80) {
                uc_reg_read(uc, UC_ARM_REG_R0, &r0);
                uc_reg_read(uc, UC_ARM_REG_R1, &r1);
                uc_reg_read(uc, UC_ARM_REG_R5, &r5);
                uc_reg_read(uc, UC_ARM_REG_R6, &r6);
                uc_reg_read(uc, UC_ARM_REG_R7, &r7);
                printf("[JJFB_R4_GATE] pc=0x%X r4=0x%X%s r0=0x%X r1=0x%X r5=0x%X r6=0x%X r7=0x%X lr=0x%X\n",
                       pc, r4, (r4 != s_last_r4) ? " (chg)" : "", r0, r1, r5, r6, r7, lr);
                fflush(stdout);
                s_r4_logs++;
            }
            s_last_r4 = r4;
        }
    }
    if (pc >= 0x2EFB00 && pc < 0x2EFC40) {
        jjfb_2efb00_hits++;
        if (pc >= 0x2EFB06) jjfb_2efb06_hits++;
        if (jjfb_2efb00_hits <= 48) {
            uc_reg_read(uc, UC_ARM_REG_R0, &r0);
            uc_reg_read(uc, UC_ARM_REG_R1, &r1);
            uc_reg_read(uc, UC_ARM_REG_R2, &r2);
            uc_reg_read(uc, UC_ARM_REG_R4, &r4);
            printf("[JJFB_2EFB00] hit pc=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X r4=0x%X hits=%u\n",
                   pc, lr, r0, r1, r2, r4, jjfb_2efb00_hits);
            fflush(stdout);
        }
    }
    if (pc >= 0x2EFC40 && pc < 0x2EFC90) {
        jjfb_2efc40_hits++;
        if (jjfb_2efc40_hits <= 16) {
            uc_reg_read(uc, UC_ARM_REG_R0, &r0);
            uc_reg_read(uc, UC_ARM_REG_R1, &r1);
            printf("[JJFB_2EFC_TAIL] hit pc=0x%X lr=0x%X r0=0x%X r1=0x%X hits=%u\n",
                   pc, lr, r0, r1, jjfb_2efc40_hits);
            fflush(stdout);
        }
    }
    /* Detect likely skip: past progress loop, never entered 0x2EFC40. */
    if (pc >= 0x2EFAE0 && pc < 0x2EFB80 && jjfb_2efc40_hits == 0)
        jjfb_2efc_gate_last_skip_pc = pc;

    /* Dense AC8-tail trace (success vs fail at ~0x2EFBA8). */
    if (pc >= 0x2EFBA0 && pc <= 0x2EFC20) {
        static uint32_t s_ac8tail_n;
        if (s_ac8tail_n < 64) {
            uint32_t ac8v = 0, b6cv = 0, f134d = 0;
            uc_reg_read(uc, UC_ARM_REG_R0, &r0);
            uc_reg_read(uc, UC_ARM_REG_R1, &r1);
            uc_reg_read(uc, UC_ARM_REG_R2, &r2);
            uc_reg_read(uc, UC_ARM_REG_R3, &r3);
            if (jjfb_guest_ext_erw) {
                uc_mem_read(uc, jjfb_guest_ext_erw + 0xAC8, &ac8v, 4);
                uc_mem_read(uc, jjfb_guest_ext_erw + 0xB6C, &b6cv, 4);
                uc_mem_read(uc, jjfb_guest_ext_erw + 0x134D, &f134d, 1);
            }
            printf("[JJFB_AC8TAIL] pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                   "AC8=0x%X B6C=0x%X flag134D=0x%X lr=0x%X\n",
                   pc, r0, r1, r2, r3, ac8v, b6cv, f134d & 0xFF, lr);
            fflush(stdout);
            s_ac8tail_n++;
        }
    }

    /* At LDR *[ERW+0xB6C]: log the gate pointer (verbose only — per-hit spam froze UI). */
    if (jjfb_verbose_logs() && (pc == 0x2EFAEA || pc == 0x2EFAEE)) {
        static uint32_t n_b6c;
        uint32_t b6c = 0, erw = jjfb_guest_ext_erw;
        n_b6c++;
        if (n_b6c <= 8 || (n_b6c % 100) == 0) {
            if (erw) uc_mem_read(uc, erw + 0xB6C, &b6c, 4);
            uc_reg_read(uc, UC_ARM_REG_R0, &r0);
            uc_reg_read(uc, UC_ARM_REG_R9, &r5); /* reuse r5 tmp for r9 */
            printf("[JJFB_B6C_GATE] pc=0x%X r0=0x%X r9=0x%X *(ERW+0xB6C)=0x%X ERW=0x%X\n",
                   pc, r0, r5, b6c, erw);
            fflush(stdout);
        }
    }

    /* Probe: make *(ERW+0xB6C) appear non-null so BNE @0x2EFAF0 -> 0x2EFB06.
     * Must patch r0 BEFORE CMP @0x2EFAEE (flags); hook runs before insn.
     * Only force when r0==0 — never clobber a real object pointer. */
    force = getenv("JJFB_FORCE_B6C");
    if (((force && force[0] && force[0] != '0') || jjfb_skip_net_arm_2efc) &&
        !jjfb_force_b6c_done &&
        (pc == 0x2EFAEC || pc == 0x2EFAEE)) {
        uc_reg_read(uc, UC_ARM_REG_R0, &r0);
        if (r0 == 0) {
            uint32_t nr0 = 1;
            jjfb_force_b6c_done = 1;
            if (force && force[0] && !(force[0] == '1' && !force[1]))
                nr0 = (uint32_t)strtoul(force, NULL, 0);
            if (nr0 == 0) nr0 = 1;
            uc_reg_write(uc, UC_ARM_REG_R0, &nr0);
            printf("[JJFB_FORCE_B6C] probe pc=0x%X old_r0=0 -> r0=0x%X (BNE -> 0x2EFB06)%s\n",
                   pc, nr0, jjfb_skip_net_arm_2efc ? " via SKIP_NET_LOGIN" : "");
            fflush(stdout);
        }
    }

    /* Probe: *(ERW+0x134D)!=0 so BLE @0x2EFBA8 does NOT skip success path.
     * Patch AFTER LDR (hook before CMP @0x2EFBA6), or write slot before LDR @BA4. */
    force = getenv("JJFB_FORCE_134D");
    if (((force && force[0] && force[0] != '0') || jjfb_skip_net_arm_2efc) &&
        !jjfb_force_134d_done) {
        uint32_t nr0 = 1;
        if (force && force[0] && !(force[0] == '1' && !force[1]))
            nr0 = (uint32_t)strtoul(force, NULL, 0);
        if (nr0 == 0) nr0 = 1;
        if (pc == 0x2EFBA4 && jjfb_guest_ext_erw) {
            /* Before LDR: seed memory so natural load yields nonzero. */
            uc_mem_write(uc, jjfb_guest_ext_erw + 0x134D, &nr0, 4);
            jjfb_force_134d_done = 1;
            printf("[JJFB_FORCE_134D] seed *(ERW+0x134D)=0x%X before LDR @0x2EFBA4%s\n",
                   nr0, jjfb_skip_net_arm_2efc ? " via SKIP_NET_LOGIN" : "");
            fflush(stdout);
        } else if (pc == 0x2EFBA6) {
            jjfb_force_134d_done = 1;
            uc_reg_read(uc, UC_ARM_REG_R0, &r0);
            uc_reg_write(uc, UC_ARM_REG_R0, &nr0);
            printf("[JJFB_FORCE_134D] probe pc=0x%X old_r0=0x%X -> r0=0x%X (CMP)%s\n",
                   pc, r0, nr0, jjfb_skip_net_arm_2efc ? " via SKIP_NET_LOGIN" : "");
            fflush(stdout);
        }
    }

    /* Probe: AC8>0 ? patch r1 AFTER LDR @FA, before CMP @FC (or seed AC8 mem). */
    force = getenv("JJFB_FORCE_AC8_GATE");
    if ((force && force[0] && force[0] != '0') || jjfb_skip_net_arm_2efc) {
        static int s_ac8gate_done;
        uint32_t nr1 = 1;
        if (force && force[0] && !(force[0] == '1' && !force[1]))
            nr1 = (uint32_t)strtoul(force, NULL, 0);
        if (nr1 == 0) nr1 = 1;
        if (!s_ac8gate_done && pc == 0x2EFBFA && jjfb_guest_ext_erw) {
            uc_mem_write(uc, jjfb_guest_ext_erw + 0xAC8, &nr1, 4);
            s_ac8gate_done = 1;
            printf("[JJFB_FORCE_AC8_GATE] seed *(ERW+AC8)=0x%X before LDR @0x2EFBFA%s\n",
                   nr1, jjfb_skip_net_arm_2efc ? " via SKIP_NET_LOGIN" : "");
            fflush(stdout);
        } else if (!s_ac8gate_done && pc == 0x2EFBFC) {
            s_ac8gate_done = 1;
            uc_reg_read(uc, UC_ARM_REG_R1, &r1);
            uc_reg_write(uc, UC_ARM_REG_R1, &nr1);
            printf("[JJFB_FORCE_AC8_GATE] probe pc=0x%X old_r1=0x%X -> r1=0x%X (CMP)%s\n",
                   pc, r1, nr1, jjfb_skip_net_arm_2efc ? " via SKIP_NET_LOGIN" : "");
            fflush(stdout);
        }
    }

    /* Probe: force r4!=0 at CMP so guest takes fall-through into 0x2EFB00+. */
    force = getenv("JJFB_FORCE_R4");
    if (force && force[0] && force[0] != '0' && !jjfb_force_r4_done &&
        (pc == 0x2EFAF0 || pc == 0x2EFAF2)) {
        uint32_t nr4 = 1;
        if (force[0] == 'x' || (force[0] == '0' && force[1] == 'x'))
            nr4 = (uint32_t)strtoul(force, NULL, 0);
        else if (force[0] != '1')
            nr4 = (uint32_t)strtoul(force, NULL, 0);
        if (nr4 == 0) nr4 = 1;
        jjfb_force_r4_done = 1;
        uc_reg_read(uc, UC_ARM_REG_R4, &r4);
        uc_reg_write(uc, UC_ARM_REG_R4, &nr4);
        printf("[JJFB_FORCE_R4] probe pc=0x%X old_r4=0x%X -> r4=0x%X (unlock 0x2EFB00+)\n",
               pc, r4, nr4);
        fflush(stdout);
    }

    force = getenv("JJFB_FORCE_2EFC_TAIL");
    /* SKIP_NET: only divert the fail-loop branch B @0x2EFC08 -> 0x2EFA20.
     * Do NOT divert at 0x2EFC06 (BGT success) — that path is already correct. */
    if (jjfb_skip_net_arm_2efc && !jjfb_2efc_tail_forced && pc == 0x2EFC08) {
        uint32_t target = 0x2EFC0B; /* Thumb: success path after AC8>0 */
        jjfb_2efc_tail_forced = 1;
        jjfb_skip_net_arm_2efc = 0;
        printf("[JJFB_FORCE_2EFC_TAIL] probe divert pc=0x%X -> 0x2EFC0A "
               "(SKIP_NET_LOGIN success path)\n", pc);
        fflush(stdout);
        uc_reg_write(uc, UC_ARM_REG_PC, &target);
    } else if (force && force[0] == '1' &&
               !jjfb_2efc_tail_forced &&
               jjfb_2efc40_hits == 0 && pc >= 0x2EFAE8 && pc <= 0x2EFB10) {
        uint32_t target = 0x2EFC41; /* Thumb */
        jjfb_2efc_tail_forced = 1;
        printf("[JJFB_FORCE_2EFC_TAIL] probe divert pc=0x%X -> 0x2EFC40 (guest exec)\n",
               pc);
        fflush(stdout);
        uc_reg_write(uc, UC_ARM_REG_PC, &target);
    }
}

static void jjfb_install_splash_cov(uc_engine *uc) {
    uc_err err;
    if (!uc || jjfb_splash_cov_installed) return;
    err = uc_hook_add(uc, &jjfb_splash_cov_hook, UC_HOOK_CODE, jjfb_hook_splash_cov, NULL,
                      JJFB_SPLASH_COV_LO, JJFB_SPLASH_COV_HI);
    if (err) {
        printf("[JJFB_2EF86C_COV] install fail err=%u\n", err);
        fflush(stdout);
        return;
    }
    jjfb_splash_cov_installed = 1;
    printf("[JJFB_2EF86C_COV] installed 0x%X..0x%X\n", JJFB_SPLASH_COV_LO, JJFB_SPLASH_COV_HI);
    fflush(stdout);
    jjfb_dump_2efc_tail_disasm(uc);
}

static void jjfb_hook_310bb4_sweep(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)size;
    (void)user_data;
    if (!jjfb_allow_chrome) return;
    jjfb_310bb4_sweep_hits++;
    if (jjfb_310bb4_sweep_hits <= 8 || (jjfb_310bb4_sweep_hits % 200) == 0) {
        uint32_t lr = 0, sp = 0;
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        printf("[JJFB_310_SWEEP] hit #%u pc=0x%X lr=0x%X sp=0x%X\n",
               jjfb_310bb4_sweep_hits, (uint32_t)address, lr, sp);
        fflush(stdout);
    }
}

static void jjfb_dump_uc_mem(uc_engine *uc, const char *tag, uint32_t addr, uint32_t n) {
    uint8_t buf[0x80];
    uint32_t i, len;
    if (!uc || !tag || !addr || n == 0) return;
    len = n > sizeof(buf) ? (uint32_t)sizeof(buf) : n;
    if (uc_mem_read(uc, addr, buf, len) != UC_ERR_OK) {
        printf("[JJFB_DUMP] %s @0x%X (%u bytes) read failed\n", tag, addr, len);
        fflush(stdout);
        return;
    }
    printf("[JJFB_DUMP] %s @0x%X %u bytes:", tag, addr, len);
    for (i = 0; i < len; i++) {
        if ((i % 32) == 0) printf("\n ");
        printf(" %02X", buf[i]);
    }
    printf("\n");
    fflush(stdout);
}

/* 303d94 loads: tbl = *(ext_base); r7 = *(tbl + 0x1E8)  [via +0x1C0 then +0x28].
 * Watch writes that land in CODE junk (e.g. 0x80278). */
static void jjfb_hook_ctx_mem_write(uc_engine *uc, uc_mem_type type,
                                    uint64_t address, int size, int64_t value,
                                    void *user_data) {
    uint32_t pc = 0, lr = 0;
    (void)type;
    (void)user_data;
    if (size != 4) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    printf("[JJFB_CTX_WRITE] addr=0x%X old? size=%d new=0x%X pc=0x%X lr=0x%X\n",
           (uint32_t)address, size, (uint32_t)value, pc, lr);
    fflush(stdout);
}

/* Natural writer: 0x2FC418 does MOVS r0,#0x45; STR [ERW+0x8D0].
 * Upstream: 0x2FC03C -> 0x2FC418; gated init 0x2DADC4 (ERW+B70/B58/DB0);
 * callers 0x2FECA2 / 0x2E4066 / 0x305EF4; alt 0x30EE50. */
static void jjfb_install_uimode_writer_hooks(uc_engine *uc);
static void jjfb_install_v62_flag_mem_hooks(uc_engine *uc);
static void jjfb_flush_1e200(uc_engine *uc);

/* v75: after natural B71=1, arm family C0→2FEBBC for B70.
 * Must NOT nest uc_emu_start from a CODE hook (hangs timer; v61).
 * Arm pending + emu_stop current guest; jjfb_run_guest_thumb flushes
 * deferred 1E200 after return (top-level C0). */
static void jjfb_v75_try_c0_before_b70_gate(uc_engine *uc) {
    uint8_t b71 = 0, b70 = 0;
    if (!uc) return;
    if (jjfb_v75_c0_done) {
        printf("[JJFB_V75_B70] try_c0 skip: already done\n");
        fflush(stdout);
        return;
    }
    if (!jjfb_v75_c0_enable) {
        printf("[JJFB_V75_B70] try_c0 skip: disabled\n");
        fflush(stdout);
        return;
    }
    if (!jjfb_plat_handler_1e200 || !jjfb_guest_ext_erw) {
        printf("[JJFB_V75_B70] try_c0 skip: handler=0x%X erw=0x%X\n",
               jjfb_plat_handler_1e200, jjfb_guest_ext_erw);
        fflush(stdout);
        return;
    }
    uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &b71, 1);
    uc_mem_read(uc, jjfb_guest_ext_erw + 0xB70, &b70, 1);
    if (b71 == 0 || b70 != 0) {
        printf("[JJFB_V75_B70] try_c0 skip: B71=%u B70=%u\n", b71, b70);
        fflush(stdout);
        return;
    }
    jjfb_v75_c0_done = 1;
    jjfb_pend_app = 0xC0;
    jjfb_pend_code = 0;
    jjfb_pend_p0 = 0;
    jjfb_pend_p1 = 0;
    jjfb_pending_1e200 = 1;
    printf("[JJFB_V75_B70] arm family C0 deferred + emu_stop "
           "(B71=%u B70=%u; top-level flush after guest; no nest)\n",
           b71, b70);
    fflush(stdout);
    /* Exit second-enqueue / 30ED2C before empty-B58 → stuck 2FC26C. */
    uc_emu_stop(uc);
}

static void jjfb_hook_uimode_writer(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t lr = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
    uint32_t mode = 0;
    uint8_t b70 = 0;
    uint32_t b58 = 0, db0 = 0, f134d = 0;
    static uint32_t n_writer, n_2fc03c, n_2dadc4, n_2dae00, n_2dae24;
    static uint32_t n_30ee50, n_2fecaa, n_2e4066, n_305ef4;
    static uint32_t n_event_dispatch, n_queue_2dc80c, n_queue_2e7b7c;
    static uint32_t n_reset_entry, n_reset_calls, n_family_dispatch;
    static uint32_t n_cb_register, n_cb_entry, n_cb_tail, n_periodic_gate;
    (void)size;
    (void)user_data;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    uc_reg_read(uc, UC_ARM_REG_R5, &r5);
    if (jjfb_guest_ext_erw) {
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_UI_MODE, &mode, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0xB70, &b70, 1);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0xB58, &b58, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0xDB0, &db0, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0x134D, &f134d, 1);
    }
    switch ((uint32_t)address) {
    /* v62: observe-only sites that write 15D / B71 / B70 (no FORCE / no inject) */
    case 0x30CCF4:
    case 0x2FE854:
    case 0x30ED7A:
    case 0x2DC572:
    case 0x2DC576:
    case 0x2FEC9A:
    case 0x2FE184: {
        static uint32_t n_v62_wr;
        uint8_t v15d = 0, vb71 = 0;
        const char *tag = "?";
        n_v62_wr++;
        if ((uint32_t)address == 0x30CCF4) tag = "strb_15D=1_in_30CBBC";
        else if ((uint32_t)address == 0x2FE854) tag = "strb_B71=0_in_2FE82C";
        else if ((uint32_t)address == 0x30ED7A) tag = "strb_B71=1_in_30ED2C";
        else if ((uint32_t)address == 0x2DC572) tag = "strb_B71=1_in_2DC4D8";
        else if ((uint32_t)address == 0x2DC576) tag = "strb_15D=1_in_2DC4D8";
        else if ((uint32_t)address == 0x2FEC9A) tag = "strb_B70_in_2FEBBC";
        else if ((uint32_t)address == 0x2FE184) tag = "strb_B70=0_in_2FE17C";
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + 0x15D, &v15d, 1);
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &vb71, 1);
        }
        printf("[JJFB_V62_FLAG] writer #%u pc=0x%X lr=0x%X %s "
               "r0-r3=0x%X,0x%X,0x%X,0x%X before 15D=%u B71=%u B70=%u\n",
               n_v62_wr, (uint32_t)address, lr, tag, r0, r1, r2, r3,
               v15d, vb71, b70);
        break;
    }
    case 0x2FC418:
        n_writer++;
        printf("[JJFB_GAME_SELF] uimode_writer ENTER #%u pc=0x2FC418 lr=0x%X "
               "ui_mode=0x%X (will store 0x45)\n", n_writer, lr, mode);
        break;
    case 0x2FC448:
        printf("[JJFB_GAME_SELF] uimode_writer STORE #%u pc=0x2FC448 lr=0x%X "
               "r0=0x%X ui_mode_before=0x%X\n", n_writer, lr, r0, mode);
        break;
    case 0x2FC03C:
        n_2fc03c++;
        printf("[JJFB_GAME_SELF] init_2FC03C #%u lr=0x%X ui_mode=0x%X "
               "B70=%u B58=0x%X DB0=0x%X\n",
               n_2fc03c, lr, mode, b70, b58, db0);
        break;
    case 0x2DADC4:
        n_2dadc4++;
        printf("[JJFB_GAME_SELF] gate_init_2DADC4 #%u lr=0x%X ui_mode=0x%X "
               "B70=%u B58=0x%X DB0=0x%X f134d=%u\n",
               n_2dadc4, lr, mode, b70, b58, db0, f134d);
        break;
    case 0x2DAE00: {
        uint8_t b71g = 0;
        /* UC_HOOK_CODE runs before the insn — C0 can set B70 before LDR. */
        if (jjfb_guest_ext_erw)
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &b71g, 1);
        printf("[JJFB_V75_B70] at_gate_B70_check B71=%u B70=%u c0_done=%d enable=%d\n",
               b71g, b70, jjfb_v75_c0_done, jjfb_v75_c0_enable);
        fflush(stdout);
        jjfb_v75_try_c0_before_b70_gate(uc);
        if (jjfb_guest_ext_erw)
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB70, &b70, 1);
        n_2dae00++;
        printf("[JJFB_GAME_SELF] gate_B70_check #%u lr=0x%X B70=%u "
               "(nonzero -> BL 2FC03C)\n", n_2dae00, lr, b70);
        break;
    }
    case 0x2DAE24:
        n_2dae24++;
        printf("[JJFB_GAME_SELF] BL_2FC03C_site #%u lr=0x%X B70=%u\n",
               n_2dae24, lr, b70);
        break;
    case 0x2DAE1A: {
        /* v67: BL 2FC26C site (B70==0 alt path after gate_B70_check) */
        static uint32_t n_bl_2fc26c;
        n_bl_2fc26c++;
        printf("[JJFB_V67_GATE] BL_2FC26C_site #%u lr=0x%X B70=%u ui_mode=0x%X "
               "(alt when B70==0; not writer)\n",
               n_bl_2fc26c, lr, b70, mode);
        break;
    }
    case 0x2FC26C: {
        static uint32_t n_2fc26c;
        n_2fc26c++;
        printf("[JJFB_V67_GATE] enter_2FC26C #%u lr=0x%X r0-r3=0x%X,0x%X,0x%X,0x%X "
               "ui_mode=0x%X B70=%u\n",
               n_2fc26c, lr, r0, r1, r2, r3, mode, b70);
        break;
    }
    case 0x2FC3E6: {
        /* pop {r4-r7,pc} — 2FC26C completed (v68: chrome nop lets us reach here) */
        static uint32_t n_2fc26c_leave;
        n_2fc26c_leave++;
        printf("[JJFB_V68_GATE] leave_2FC26C #%u lr=0x%X ui_mode=0x%X B70=%u "
               "(expect → 2E2520 ret → 2DC8D8)\n",
               n_2fc26c_leave, lr, mode, b70);
        /* v73: arm present freeze — next flush sticks until B71 advances */
        if (jjfb_present_freeze_enable) {
            jjfb_present_freeze_armed = 1;
            printf("[JJFB_V73_FREEZE] arm after leave_2FC26C #%u "
                   "(next flush then hold SDL)\n",
                   n_2fc26c_leave);
            fflush(stdout);
        }
        /* v74: arm second 10165 with B58 body record → 30ED2C */
        if (jjfb_v74_second_enq_enable && !jjfb_v74_second_enq_done) {
            jjfb_v74_second_enq_armed = 1;
            printf("[JJFB_V74_B58] arm second enqueue after leave_2FC26C #%u "
                   "(101AB one-record → 2F68E4 → 30ED2C)\n",
                   n_2fc26c_leave);
            fflush(stdout);
        }
        break;
    }
    case 0x30ED2C: {
        static uint32_t n_ed2c;
        uint32_t item0 = 0, item_c = 0, list820 = 0, cnt820 = 0;
        uint8_t vb71 = 0;
        n_ed2c++;
        if (r0) {
            uc_mem_read(uc, r0, &item0, 4);
            uc_mem_read(uc, r0 + 0xc, &item_c, 4);
        }
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &vb71, 1);
            uc_mem_read(uc, jjfb_guest_ext_erw + 0x820, &list820, 4);
            if (list820)
                uc_mem_read(uc, list820 + 4, &cnt820, 4);
        }
        printf("[JJFB_V74_B58] enter_30ED2C #%u item=0x%X str0=0x%X field_c=0x%X "
               "r1=%u B71=%u list820=0x%X cnt=%u lr=0x%X\n",
               n_ed2c, r0, item0, item_c, r1, vb71, list820, cnt820, lr);
        if (item0)
            jjfb_dump_guest(uc, "30ED2C_str0", item0, 32);
        if (list820)
            jjfb_dump_guest(uc, "30ED2C_820", list820, 64);
        /* walk first few name-table entries if list-like */
        if (list820 && cnt820 > 0 && cnt820 < 64) {
            uint32_t i, node = 0, name = 0;
            uc_mem_read(uc, list820, &node, 4);
            for (i = 0; i < cnt820 && i < 12; i++) {
                uint32_t next = 0;
                if (!node) break;
                uc_mem_read(uc, node, &name, 4);
                uc_mem_read(uc, node + 4, &next, 4);
                printf("[JJFB_V74_B58] 820[%u] node=0x%X name=0x%X next=0x%X\n",
                       i, node, name, next);
                if (name)
                    jjfb_dump_guest(uc, "820_name", name, 48);
                node = next;
            }
        }
        break;
    }
    case 0x30ED4C: {
        /* after BL 2D96BC — r0 = lookup result */
        static uint32_t n_lookup;
        n_lookup++;
        printf("[JJFB_V74_B58] after_2D96BC #%u r0=0x%X (0=miss) lr=0x%X\n",
               n_lookup, r0, lr);
        break;
    }
    case 0x304AC4: {
        static uint32_t n_ac4e;
        uint32_t r2 = 0, slot = 0, fn = 0;
        n_ac4e++;
        /* Reconstruct thin wrapper: base=pc+lit, then [base+0x38]+0x80+0x20 */
        {
            int32_t lit = (int32_t)0xFFFD42F0;
            uint32_t base = (uint32_t)(0x304ACC + lit);
            uint32_t p38 = 0;
            uc_mem_read(uc, base + 0x38, &p38, 4);
            slot = p38 + 0x80;
            uc_mem_read(uc, slot + 0x20, &fn, 4);
            printf("[JJFB_V74_B58] enter_304AC4 #%u obj=0x%X r1=%u "
                   "base=0x%X p38=0x%X fn=0x%X\n",
                   n_ac4e, r0, r1, base, p38, fn);
        }
        if (r0)
            jjfb_dump_guest(uc, "304AC4_obj", r0, 64);
        break;
    }
    case 0x30ED64: {
        static uint32_t n_cmp;
        n_cmp++;
        printf("[JJFB_V74_B58] after_2F6C44_cmp #%u r0=0x%X r1=0x%X "
               "(need r0==field_c) lr=0x%X\n",
               n_cmp, r0, r1, lr);
        break;
    }
    case 0x30ED7C: {
        /* after optional B71=1 store; movs r0,#0; return */
        static uint32_t n_ed7c;
        uint8_t vb71 = 0;
        n_ed7c++;
        if (jjfb_guest_ext_erw)
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &vb71, 1);
        printf("[JJFB_V74_B58] leave_30ED2C_ok #%u B71=%u lr=0x%X\n",
               n_ed7c, vb71, lr);
        /* v75: set B70 via family C0 before later empty-B58 gate takes 2FC26C */
        if (vb71)
            jjfb_v75_try_c0_before_b70_gate(uc);
        break;
    }
    case 0x30ED82: {
        static uint32_t n_ed82;
        uint8_t vb71 = 0;
        n_ed82++;
        if (jjfb_guest_ext_erw)
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &vb71, 1);
        printf("[JJFB_V74_B58] leave_30ED2C_fail #%u B71=%u lr=0x%X "
               "(lookup/match failed; not FORCE)\n",
               n_ed82, vb71, lr);
        break;
    }
    case 0x2DC8D8: {
        /* v67: immediately after BL 2E2520 — r0 is handler return.
         * drain: CMP r0,#1; BEQ skip_remove; else 312C0C+free */
        static uint32_t n_ret;
        uint32_t b54 = 0, cnt = 0;
        n_ret++;
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB54, &b54, 4);
            if (b54)
                uc_mem_read(uc, b54 + 4, &cnt, 4);
        }
        printf("[JJFB_V67_DRAIN] after_2E2520 #%u ret=0x%X (%s) "
               "B54=0x%X count=%u lr=0x%X\n",
               n_ret, r0,
               (r0 == 1) ? "KEEP_IN_QUEUE" : "EXPECT_DEQUEUE",
               b54, cnt, lr);
        break;
    }
    case 0x312C0C: {
        static uint32_t n_rm;
        uint32_t cnt = 0, head = 0;
        n_rm++;
        if (r0) {
            uc_mem_read(uc, r0 + 4, &cnt, 4);
            uc_mem_read(uc, r0 + 0, &head, 4);
        }
        printf("[JJFB_V67_DRAIN] 312C0C remove #%u list=0x%X idx=%u "
               "count_before=%u head=0x%X lr=0x%X\n",
               n_rm, r0, r1, cnt, head, lr);
        break;
    }
    case 0x30EE50:
        n_30ee50++;
        printf("[JJFB_GAME_SELF] alt_30EE50 #%u lr=0x%X r0-r3=0x%X,0x%X,0x%X,0x%X "
               "r4=0x%X r5=0x%X ui_mode=0x%X\n",
               n_30ee50, lr, r0, r1, r2, r3, r4, r5, mode);
        break;
    case 0x2FECA2:
        n_2fecaa++;
        printf("[JJFB_GAME_SELF] caller_2FECA2 #%u lr=0x%X -> 2DADC4\n",
               n_2fecaa, lr);
        break;
    case 0x2E4040: {
        static uint32_t n_4040;
        uint32_t b5c = 0, b60 = 0, b58 = 0;
        n_4040++;
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB5C, &b5c, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB60, &b60, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB58, &b58, 4);
        }
        printf("[JJFB_V66_PATH_A] enter_2E4040 #%u r0=0x%X r4_ev=0x%X "
               "B5C=0x%X B60=0x%X B58=0x%X lr=0x%X\n",
               n_4040, r0, r4, b5c, b60, b58, lr);
        break;
    }
    case 0x2F68E4: {
        static uint32_t n_68e4;
        uint32_t b5c = 0, b60 = 0;
        n_68e4++;
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB5C, &b5c, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB60, &b60, 4);
        }
        printf("[JJFB_V66_PATH_A] enter_2F68E4 #%u list=0x%X ev=0x%X "
               "B5C=0x%X B60=0x%X lr=0x%X\n",
               n_68e4, r0, r1, b5c, b60, lr);
        break;
    }
    case 0x2F6952: {
        static uint32_t n_68e4_ret;
        n_68e4_ret++;
        printf("[JJFB_V66_PATH_A] leave_2F68E4 #%u count_r5=%u lr=0x%X "
               "(expect → 2E4062 → 2E4066 → 2DADC4)\n",
               n_68e4_ret, r5, lr);
        break;
    }
    case 0x2E4066:
        n_2e4066++;
        printf("[JJFB_GAME_SELF] caller_2E4066 #%u lr=0x%X -> 2DADC4 "
               "B58=0x%X\n", n_2e4066, lr, b58);
        break;
    case 0x305EF4:
        n_305ef4++;
        printf("[JJFB_GAME_SELF] caller_305EF4 #%u lr=0x%X -> 2DADC4 "
               "f134d=%u B70=%u\n", n_305ef4, lr, f134d, b70);
        break;
    case 0x2DC80C:
        n_queue_2dc80c++;
        printf("[JJFB_V56_QUEUE] drain_2DC80C ENTER #%u lr=0x%X\n", n_queue_2dc80c, lr);
        break;
    case 0x2E7B7C:
        n_queue_2e7b7c++;
        printf("[JJFB_V56_QUEUE] drain_2E7B7C ENTER #%u lr=0x%X\n", n_queue_2e7b7c, lr);
        break;
    case 0x2DC8D4:
    case 0x2E7B9E:
    case 0x2E2520: {
        uint32_t ev[5] = {0,0,0,0,0};
        n_event_dispatch++;
        if (r0 && uc_mem_read(uc, r0, ev, sizeof(ev)) == UC_ERR_OK) {
            printf("[JJFB_V56_EVENT] #%u site=0x%X ptr=0x%X code=%u(0x%X) "
                   "p0=0x%X p1=0x%X p2=0x%X p3=0x%X lr=0x%X%s\n",
                   n_event_dispatch, (uint32_t)address, r0, ev[0], ev[0],
                   ev[1], ev[2], ev[3], ev[4], lr,
                   (ev[0] == 5 || ev[0] == 12) ? " TARGETS_2DADC4" : "");
            if (ev[0] == 5 || ev[0] == 12)
                printf("[JJFB_V63_PATH_A] event code=%u at 0x%X -> 2E4040/2E4066 -> 2DADC4 "
                       "(bypasses 305EB8 B71 gate)\n",
                       ev[0], (uint32_t)address);
            else if (ev[0] == 3)
                printf("[JJFB_V63_PATH_A] event MR_MOUSE_UP at 0x%X -> 2DC4D8 "
                       "(sets B71+134D=2; NOT Path C bootstrap)\n",
                       (uint32_t)address);
        } else {
            printf("[JJFB_V56_EVENT] #%u site=0x%X ptr=0x%X unreadable lr=0x%X\n",
                   n_event_dispatch, (uint32_t)address, r0, lr);
        }
        break;
    }
    case 0x30D2F8:
    case 0x30D24C:
    case 0x2E4D6C: {
        static uint32_t n_enq;
        n_enq++;
        printf("[JJFB_V64_ENQ] site=0x%X #%u r0-r3=0x%X,0x%X,0x%X,0x%X lr=0x%X\n",
               (uint32_t)address, n_enq, r0, r1, r2, r3, lr);
        if ((uint32_t)address == 0x2E4D6C) {
            uint32_t b54 = 0;
            uint8_t peek[16];
            if (jjfb_guest_ext_erw)
                uc_mem_read(uc, jjfb_guest_ext_erw + 0xB54, &b54, 4);
            printf("[JJFB_V65_ENQ] 2E4D6C buf=0x%X len/r1=0x%X B54_head=0x%X%s\n",
                   r0, r1, b54, r1 ? "" : " (r1==0 early-exit; need 101AB fill)");
            if (r0 && r1 && uc_mem_read(uc, r0, peek, sizeof(peek)) == UC_ERR_OK) {
                printf("[JJFB_V65_ENQ] 2E4D6C peek "
                       "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X "
                       "%02X%02X%02X%02X\n",
                       peek[0], peek[1], peek[2], peek[3], peek[4], peek[5],
                       peek[6], peek[7], peek[8], peek[9], peek[10], peek[11],
                       peek[12], peek[13], peek[14], peek[15]);
            }
        }
        break;
    }
    case 0x312A60: {
        static uint32_t n_push;
        n_push++;
        printf("[JJFB_V65_ENQ] 312A60 push #%u list=0x%X item=0x%X lr=0x%X\n",
               n_push, r0, r1, lr);
        break;
    }
    case 0x30D300:
        n_family_dispatch++;
        if (n_family_dispatch <= 12 || r0 == 0xC0 || (n_family_dispatch % 100) == 0)
            printf("[JJFB_V56_FAMILY] dispatch #%u app=0x%X code=0x%X p0=0x%X p1=0x%X lr=0x%X%s\n",
                   n_family_dispatch, r0, r1, r2, r3, lr,
                   r0 == 0xC0 ? " TARGETS_2FEBBC" : "");
        break;
    case 0x2FEBBC:
        n_reset_entry++;
        printf("[JJFB_V56_RESET] entry_2FEBBC #%u lr=0x%X r0-r3=0x%X,0x%X,0x%X,0x%X\n",
               n_reset_entry, lr, r0, r1, r2, r3);
        break;
    case 0x2DCC60: case 0x2DCCC4: case 0x2DCD4E: case 0x2DCDEE:
    case 0x2DCF4C: case 0x2DD626: case 0x2DDA82: case 0x2E02E0:
    case 0x2E351A: case 0x2E7528: case 0x2E77B4: case 0x2E799E:
    case 0x2FBED6: case 0x30DC44:
        n_reset_calls++;
        printf("[JJFB_V56_RESET] call_2FEBBC #%u pc=0x%X lr=0x%X r0-r3=0x%X,0x%X,0x%X,0x%X\n",
               n_reset_calls, (uint32_t)address, lr, r0, r1, r2, r3);
        break;
    case 0x2F5390:
    case 0x2F53AC:
    case 0x30D128:
    case 0x3054A4: {
        uint32_t tslot = 0;
        n_cb_register++;
        if (jjfb_guest_ext_erw)
            uc_mem_read(uc, jjfb_guest_ext_erw + 0x8C4, &tslot, 4);
        printf("[JJFB_V56_CALLBACK] register #%u pc=0x%X lr=0x%X "
               "r0-r3=0x%X,0x%X,0x%X,0x%X ERW8C4=0x%X%s\n",
               n_cb_register, (uint32_t)address, lr, r0, r1, r2, r3, tslot,
               (r0 == 0x2F5405 || r1 == 0x2F5405 || r2 == 0x2F5405 || r3 == 0x2F5405 ||
                r0 == 0x2F5404 || r1 == 0x2F5404 || r2 == 0x2F5404 || r3 == 0x2F5404)
                   ? " CALLBACK_2F5404" : "");
        break;
    }
    case 0x2F5404:
        n_cb_entry++;
        printf("[JJFB_V56_CALLBACK] entry_2F5404 #%u lr=0x%X r0-r3=0x%X,0x%X,0x%X,0x%X\n",
               n_cb_entry, lr, r0, r1, r2, r3);
        break;
    case 0x2F5734:
        n_cb_tail++;
        printf("[JJFB_V56_CALLBACK] tail_call_305EB8 #%u lr=0x%X\n", n_cb_tail, lr);
        break;
    case 0x305EB8: {
        uint8_t v15d = 0, vb71 = 0;
        const char *fail = "ok_to_2DADC4";
        n_periodic_gate++;
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + 0x15D, &v15d, 1);
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &vb71, 1);
            jjfb_cached_b71 = vb71;
        }
        /* Mirror 305EB8 static gates: 15D==1, B71!=0, 134D==0 */
        if (v15d != 1) fail = "fail_15D_ne_1";
        else if (vb71 == 0) fail = "fail_B71_eq_0";
        else if (f134d != 0) fail = "fail_134D_ne_0";
        if (n_periodic_gate <= 8 || (n_periodic_gate % 50) == 0 ||
            strcmp(fail, "ok_to_2DADC4") == 0)
            printf("[JJFB_V56_PERIODIC] entry_305EB8 #%u lr=0x%X "
                   "15D=%u B71=%u f134d=%u B70=%u B58=0x%X DB0=0x%X %s\n",
                   n_periodic_gate, lr, v15d, vb71, f134d, b70, b58, db0, fail);
        break;
    }
    /* v57: producers that should reach family app=0xC0 / callback register */
    case 0x304418:
    case 0x3053BA: {
        static uint32_t n_prep;
        n_prep++;
        printf("[JJFB_V57_SRC] call_2F5390_prep #%u pc=0x%X lr=0x%X "
               "r0-r3=0x%X,0x%X,0x%X,0x%X\n",
               n_prep, (uint32_t)address, lr, r0, r1, r2, r3);
        break;
    }
    case 0x2F53BC:
    case 0x3056C6:
    case 0x30D136:
    case 0x31068E: {
        static uint32_t n_bl_reg;
        int hit = (r0 == 0x2F5405 || r1 == 0x2F5405 || r2 == 0x2F5405 || r3 == 0x2F5405 ||
                   r0 == 0x2F5404 || r1 == 0x2F5404 || r2 == 0x2F5404 || r3 == 0x2F5404);
        n_bl_reg++;
        printf("[JJFB_V57_SRC] BL_3054A4 #%u pc=0x%X lr=0x%X "
               "r0-r3=0x%X,0x%X,0x%X,0x%X%s\n",
               n_bl_reg, (uint32_t)address, lr, r0, r1, r2, r3,
               hit ? " CALLBACK_2F5404" : "");
        break;
    }
    case 0x31309C: {
        static uint32_t n_cmp_c0;
        n_cmp_c0++;
        printf("[JJFB_V57_SRC] cmp_r0_C0 #%u pc=0x31309C lr=0x%X r0=0x%X "
               "(EQ would select family C0 path)\n",
               n_cmp_c0, lr, r0);
        break;
    }
    case 0x2EDA48:
    case 0x2F5D8E: {
        static uint32_t n_movs_c0;
        n_movs_c0++;
        printf("[JJFB_V57_SRC] movs_imm_C0 #%u pc=0x%X lr=0x%X "
               "r0-r3=0x%X,0x%X,0x%X,0x%X\n",
               n_movs_c0, (uint32_t)address, lr, r0, r1, r2, r3);
        break;
    }
    /* v58: enclosing fn entries / BL sites that should reach v57 producers */
    case 0x304AEC:
    case 0x3053B8:
    case 0x2ED8E4:
    case 0x2E8C00:
    case 0x2F5CA4:
    case 0x2DB044:
    case 0x2EB770: {
        static uint32_t n_fn;
        const char *tag =
            ((uint32_t)address == 0x304AEC) ? "fn_304AEC_reg" :
            ((uint32_t)address == 0x3053B8) ? "fn_3053B8_wrap" :
            ((uint32_t)address == 0x2ED8E4) ? "fn_2ED8E4_C0" :
            ((uint32_t)address == 0x2E8C00) ? "fn_2E8C00_to_2ED8E4" :
            ((uint32_t)address == 0x2F5CA4) ? "fn_2F5CA4_C0" :
            ((uint32_t)address == 0x2DB044) ? "fn_2DB044_to_2F5CA4" :
            "fn_2EB770_to_2E8C00";
        n_fn++;
        printf("[JJFB_V58_FN] %s #%u pc=0x%X lr=0x%X "
               "r0-r3=0x%X,0x%X,0x%X,0x%X\n",
               tag, n_fn, (uint32_t)address, lr, r0, r1, r2, r3);
        break;
    }
    case 0x304B5A:
    case 0x2E8C9A:
    case 0x2E8CD8:
    case 0x30DAE8:
    case 0x2DB084:
    case 0x2EB8AA:
    case 0x30DEE6: {
        static uint32_t n_bl;
        const char *tag =
            ((uint32_t)address == 0x304B5A) ? "BL_3053B8" :
            ((uint32_t)address == 0x2E8C9A || (uint32_t)address == 0x2E8CD8 ||
             (uint32_t)address == 0x30DAE8) ? "BL_2ED8E4" :
            ((uint32_t)address == 0x2DB084) ? "BL_2F5CA4" :
            "BL_2E8C00";
        n_bl++;
        printf("[JJFB_V58_FN] %s #%u pc=0x%X lr=0x%X "
               "r0-r3=0x%X,0x%X,0x%X,0x%X\n",
               tag, n_bl, (uint32_t)address, lr, r0, r1, r2, r3);
        break;
    }
    case 0x30CBBC:
    case 0x305404: {
        static uint32_t n_tcreate;
        n_tcreate++;
        printf("[JJFB_V60_TIMER] %s #%u pc=0x%X lr=0x%X r0-r3=0x%X,0x%X,0x%X,0x%X\n",
               ((uint32_t)address == 0x305404) ? "mrc_timerCreate" : "fn_30CBBC_init",
               n_tcreate, (uint32_t)address, lr, r0, r1, r2, r3);
        break;
    }
    default:
        break;
    }
    fflush(stdout);
}

static void jjfb_install_uimode_writer_hooks(uc_engine *uc) {
    static const uint32_t addrs[] = {
        /* v55 natural writer chain */
        0x2FC418, 0x2FC448, 0x2FC03C, 0x2DADC4, 0x2DAE00, 0x2DAE24,
        0x30EE50, 0x2FECA2, 0x2E4040, 0x2E4066, 0x305EF4,
        /* v66: 2E4040 → 2F68E4(B5C stream) → 2E4066 */
        0x2F68E4, 0x2F6952,
        /* v67/v68: B70==0 → 2FC26C; leave+drain ret/dequeue */
        0x2DAE1A, 0x2FC26C, 0x2FC3E6, 0x2DC8D8, 0x312C0C,
        /* v56 event queue -> 2E2520 -> event 5/12 -> 2E4066 */
        0x2DC80C, 0x2DC8D4, 0x2E7B7C, 0x2E7B9E, 0x2E2520,
        /* v64/v65: B54 enqueue producer (0x10165 → 30D2F8 → 30D24C → 2E4D6C → 312A60) */
        0x30D2F8, 0x30D24C, 0x2E4D6C, 0x312A60,
        /* v56 reset/start function 2FEBBC and every direct BL site */
        0x2FEBBC, 0x2DCC60, 0x2DCCC4, 0x2DCD4E, 0x2DCDEE,
        0x2DCF4C, 0x2DD626, 0x2DDA82, 0x2E02E0, 0x2E351A,
        0x2E7528, 0x2E77B4, 0x2E799E, 0x2FBED6, 0x30DC44,
        /* v56 family dispatcher: app 0xC0 selects 30DC44 */
        0x30D300,
        /* v56 callback registration/invocation -> 305EB8 -> 305EF4 */
        0x2F5390, 0x2F53AC, 0x30D128, 0x3054A4,
        0x2F5404, 0x2F5734, 0x305EB8,
        /* v57: producers of registration / C0 (NO inject; observe only) */
        0x304418, 0x3053BA,                 /* BL 2F5390 */
        0x2F53BC, 0x3056C6, 0x30D136, 0x31068E, /* BL 3054A4 */
        0x31309C,                           /* CMP r0,#0xC0 */
        0x2EDA48, 0x2F5D8E,                 /* MOVS #0xC0 */
        /* v58: enclosing fn / BL sites (still observe only) */
        0x304AEC, 0x3053B8, 0x2ED8E4, 0x2E8C00, 0x2F5CA4, 0x2DB044, 0x2EB770,
        0x304B5A, 0x2E8C9A, 0x2E8CD8, 0x30DAE8, 0x2DB084, 0x2EB8AA, 0x30DEE6,
        /* v60: timerCreate path (family app=2 → 30CBBC → 305404) */
        0x30CBBC, 0x305404,
        /* v62: 15D/B71/B70 writers + 305EF4 (observe only) */
        0x30CCF4, 0x2FE854, 0x30ED7A, 0x2DC572, 0x2DC576, 0x2FEC9A, 0x2FE184,
        /* v74: 30ED2C natural B71 path (nonempty B58) */
        0x30ED2C, 0x30ED4C, 0x304AC4, 0x30ED52, 0x30ED64, 0x30ED7C, 0x30ED82
    };
    uint32_t i;
    uc_err err;
    if (!uc || jjfb_uimode_writer_hooks_installed) return;
    if ((uint32_t)(sizeof(addrs) / sizeof(addrs[0])) >
        (uint32_t)(sizeof(jjfb_uimode_writer_hooks) / sizeof(jjfb_uimode_writer_hooks[0]))) {
        printf("[JJFB_GAME_SELF] uimode hook table overflow\n");
        return;
    }
    for (i = 0; i < (uint32_t)(sizeof(addrs) / sizeof(addrs[0])); i++) {
        err = uc_hook_add(uc, &jjfb_uimode_writer_hooks[i], UC_HOOK_CODE,
                          jjfb_hook_uimode_writer, NULL,
                          addrs[i], addrs[i] + 2, 0);
        if (err)
            printf("[JJFB_GAME_SELF] hook 0x%X failed err=%u\n", addrs[i], err);
    }
    jjfb_uimode_writer_hooks_installed = 1;
    printf("[JJFB_V56_UPSTREAM] coverage installed "
           "(event_queue/event5_12/family_C0/reset_2FEBBC/callback_2F5404/305EB8/writer)\n");
    printf("[JJFB_V57_SRC] coverage installed "
           "(call_2F5390/BL_3054A4/cmp_C0/movs_C0) NO_INJECT NO_FORCE\n");
    printf("[JJFB_V58_FN] coverage installed "
           "(reg_304AEC/C0_2ED8E4_2F5CA4/callers) NO_INJECT NO_FORCE\n");
    printf("[JJFB_V60_TIMER] coverage installed "
           "(30CBBC/timerCreate/ERW8C4) family_app2_before_resume\n");
    printf("[JJFB_V61_NEST] defer_1E209_during_ext_call "
           "(fix nested emu hang on first timer/app7)\n");
    printf("[JJFB_V62_FLAG] coverage installed "
           "(15D/B71/B70 writers + enriched 305EB8 gates) NO_FORCE NO_INJECT\n");
    printf("[JJFB_V63_PATH_A] contract=opt_in_MENU_RETURN_or_MOUSE_MOVE_once "
           "via JJFB_PATH_A_EVENT_ONCE=5|12 (PROBE; mrc_event 1..5 is no-op)\n");
    printf("[JJFB_V64_ENQ] contract=opt_in_10165_enqueue_once "
           "via JJFB_V64_ENQUEUE_ONCE=1 (30D2F8→B54; not FORCE)\n");
    printf("[JJFB_V67_DRAIN] contract=trace_2E2520_ret_and_312C0C_dequeue "
           "+2FC26C_alt_path +bad_drawFP150C_skip_BLX NO_FORCE NO_INJECT\n");
    printf("[JJFB_V68_CHROME] contract=nop_real_entry_2F449C "
           "(was mid-BL 2F4494; stop flicker / finish Path A) NO_FORCE\n");
    printf("[JJFB_V69_DRAWFP] contract=10113_11F02_03_04_write__DrawBitmap "
           "to_out +br__DrawBitmap NO_FORCE\n");
    printf("[JJFB_V70_PLAT] contract=10180_userinfo_blob_for_2F65BC "
           "+10130_large_alloc_noop_ok NO_FORCE NO_C0\n");
    {
        const char *imm = getenv("JJFB_PRESENT_IMMEDIATE");
        if (imm && imm[0] == '1')
            jjfb_present_coalesce = 0;
        printf("[JJFB_V71_PRESENT] contract=coalesce_SDL_until_timer_or_DispUpEx "
               "coalesce=%d (set JJFB_PRESENT_IMMEDIATE=1 to restore mid-frame present)\n",
               jjfb_present_coalesce);
        printf("[JJFB_V72_FLICKER] contract=skip_fullscreen_black_DrawRect "
               "keep_previous_frame NO_FORCE\n");
        {
            const char *fz = getenv("JJFB_PRESENT_FREEZE");
            /* v84: default OFF (卡死观感). Set JJFB_PRESENT_FREEZE=1 to re-enable. */
            if (fz && fz[0] == '1')
                jjfb_present_freeze_enable = 1;
            else if (fz && fz[0] == '0')
                jjfb_present_freeze_enable = 0;
            printf("[JJFB_V73_FREEZE] contract=hold_SDL_after_leave_2FC26C "
                   "while_B71_eq_0 enable=%d (JJFB_PRESENT_FREEZE=1 to enable)\n",
                   jjfb_present_freeze_enable);
        }
        {
            const char *b58 = getenv("JJFB_V74_B58_SECOND");
            if (b58 && b58[0] == '0')
                jjfb_v74_second_enq_enable = 0;
            printf("[JJFB_V74_B58] contract=second_101AB_one_record_after_2FC26C "
                   "enable=%d (JJFB_V74_B58_SECOND=0 to disable) NO_FORCE\n",
                   jjfb_v74_second_enq_enable);
        }
        {
            const char *c0 = getenv("JJFB_FAMILY_C0_AFTER_B71");
            if (c0 && c0[0] == '0')
                jjfb_v75_c0_enable = 0;
            printf("[JJFB_V75_B70] contract=family_C0_after_B71_to_2FEBBC "
                   "enable=%d (JJFB_FAMILY_C0_AFTER_B71=0 to disable) "
                   "NO_FORCE_B70_BYTE\n",
                   jjfb_v75_c0_enable);
        }
    }
    fflush(stdout);
    jjfb_install_v62_flag_mem_hooks(uc);
}

/* v62: mem-write watch on ERW flag bytes (installed when ERW known). */
static void jjfb_hook_v62_flag_mem(uc_engine *uc, uc_mem_type type,
                                  uint64_t address, int size, int64_t value,
                                  void *user_data) {
    uint32_t pc = 0, lr = 0;
    uint32_t off;
    (void)type;
    (void)user_data;
    if (!jjfb_guest_ext_erw || size < 1) return;
    off = (uint32_t)address - jjfb_guest_ext_erw;
    if (off != 0x15D && off != 0xB71 && off != 0xB70) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    if (off == 0xB71)
        jjfb_cached_b71 = (uint8_t)((uint32_t)value & 0xFF);
    printf("[JJFB_V62_FLAG] mem_write ERW+0x%X size=%d new=0x%X pc=0x%X lr=0x%X\n",
           off, size, (uint32_t)value & 0xFF, pc, lr);
    fflush(stdout);
}

static void jjfb_install_v62_flag_mem_hooks(uc_engine *uc) {
    static const uint32_t offs[3] = {0x15D, 0xB71, 0xB70};
    uint32_t i;
    uc_err err;
    if (!uc || !jjfb_guest_ext_erw || jjfb_v62_flag_hooks_installed) return;
    for (i = 0; i < 3; i++) {
        uint32_t a = jjfb_guest_ext_erw + offs[i];
        err = uc_hook_add(uc, &jjfb_v62_flag_mem_hooks[i], UC_HOOK_MEM_WRITE,
                          jjfb_hook_v62_flag_mem, NULL, a, a, 0);
        if (err)
            printf("[JJFB_V62_FLAG] mem hook ERW+0x%X failed err=%u\n", offs[i], err);
    }
    jjfb_v62_flag_hooks_installed = 1;
    printf("[JJFB_V62_FLAG] mem watch ERW+0x15D/0xB71/0xB70 installed\n");
    fflush(stdout);
}

static void jjfb_hook_ui_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    static uint32_t n87, nbf4, nc34, n2358, n284c, n_a180, n4494, n885a;
    uint32_t lr = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0;
    (void)size;
    (void)user_data;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    if ((uint32_t)address == 0x2E87AC) {
        n87++;
        if (n87 <= 6 || (n87 % 40) == 0)
            printf("[JJFB_UI] enter 0x2e87ac #%u lr=0x%X\n", n87, lr);
    } else if ((uint32_t)address == 0x2F284C) {
        n284c++;
        if (n284c <= 8 || (n284c % 40) == 0)
            printf("[JJFB_UI] enter 0x2f284c #%u lr=0x%X r0=0x%X r1=%d r2=%d r3=0x%X\n",
                   n284c, lr, r0, (int32_t)r1, (int32_t)r2, r3);
        /* Pass through: 2ea180->2f4494. 2f4494 itself may early-return. */
    } else if ((uint32_t)address == 0x2EA180) {
        uint32_t mode = 0;
        uint32_t sp = 0;
        n_a180++;
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        /* Hook fires before prologue; caller placed mode at sp+0xC. */
        uc_mem_read(uc, sp + 0xC, &mode, 4);
        if (n_a180 <= 8 || (n_a180 % 40) == 0)
            printf("[JJFB_UI] enter 0x2ea180 #%u lr=0x%X r0=0x%X r1=%d r2=%d mode=%u\n",
                   n_a180, lr, r0, (int32_t)r1, (int32_t)r2, mode);
    } else if ((uint32_t)address == 0x2F449C) {
        /*
         * v68: real chrome entry is 0x2F449C (PUSH). Old hook 0x2F4494 sat
         * mid-BL of the previous function, so nop never ran and chrome kept
         * re-drawing (flicker) while aborting 2FC26C before 2E2520 returns.
         * Default: skip decorative frame — platform bypass, not fake UI.
         */
        uint32_t mode = 0, sp = 0, flag = 0;
        uint32_t tbl = 0, fp = 0, slot = 0;
        n4494++;
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        /* Hook fires before push; caller (2ea180) left mode at sp+0. */
        uc_mem_read(uc, sp, &mode, 4);
        if (jjfb_guest_ext_erw)
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xA64, &flag, 4);
        if (jjfb_guest_ext_base) {
            slot = jjfb_guest_ext_base;
            uc_mem_read(uc, slot, &tbl, 4);
            if (tbl && getMrpMemPtr(tbl + 0x1E8))
                uc_mem_read(uc, tbl + 0x1E8, &fp, 4);
        }
        if (n4494 <= 12 || (n4494 % 40) == 0)
            printf("[JJFB_UI] enter 0x2f449c #%u lr=0x%X y=%d w=%d mode=%u flagA64=0x%X "
                   "(nop chrome; unblock Path A dequeue)\n",
                   n4494, lr, (int32_t)r1, (int32_t)r2, mode, flag);
        if (n4494 <= 8 || (n4494 % 40) == 0) {
            printf("[JJFB_CTX] chrome-bypass dump #%u slot@0x%X tbl=0x%X fp@+1E8=0x%X "
                   "mr_table=0x%X expect_DrawRect=0x%X\n",
                   n4494, slot, tbl, fp,
                   mr_table ? toMrpMemAddr(mr_table) : 0,
                   mr_table ? toMrpMemAddr(mr_table) + 0x1E8 : 0);
            fflush(stdout);
        }
        if (!jjfb_allow_chrome || jjfb_chrome_allow_calls == 0) {
            /* JJFB_ALLOW_CHROME=1 to temporarily enter decorative frame. */
            uc_reg_write(uc, UC_ARM_REG_PC, &lr);
            return;
        }
        if (jjfb_chrome_allow_calls_init && jjfb_chrome_allow_calls > jjfb_chrome_allow_calls_init)
            jjfb_chrome_allow_calls = jjfb_chrome_allow_calls_init;
        jjfb_chrome_allow_calls--;
        printf("[JJFB_2F449C_ALLOW] #%u allow_left=%u mode=%u lr=0x%X\n",
               n4494, jjfb_chrome_allow_calls, mode, lr);
        fflush(stdout);
    } else if ((uint32_t)address == 0x2F46D6) {
        /* ldrh r0,[r4,#0xa] — only reached if JJFB_ALLOW_CHROME=1 */
        static uint32_t n46d6;
        uint32_t r5v = 0;
        uint16_t wh = 0;
        n46d6++;
        uc_reg_read(uc, UC_ARM_REG_R5, &r5v);
        if (r4 && getMrpMemPtr(r4))
            uc_mem_read(uc, r4 + 0xa, &wh, 2);
        if (n46d6 <= 12 || (n46d6 % 40) == 0)
            printf("[JJFB_UI] 2f449c@46d6 #%u r4=0x%X r5=0x%X obj+0xa=%u (before 303d9c)\n",
                   n46d6, r4, r5v, (unsigned)wh);
    } else if ((uint32_t)address == 0x2F995C || (uint32_t)address == 0x2F9968) {
        /*
         * Full axis map for 240?320 splash layout:
         *   x = (2F9968() - bmp.w) / 2  ??needs WIDTH 240
         *   y = 2F995C() - 100          ??needs HEIGHT 320
         * Disable with JJFB_AXIS_FIX=0.
         */
        static uint32_t n_axis;
        uint32_t want, got = 0;
        int do_fix = 1;
        const char *ax = getenv("JJFB_AXIS_FIX");
        if (ax && ax[0] == '0')
            do_fix = 0;
        n_axis++;
        if (jjfb_guest_ext_erw) {
            uint32_t off = ((uint32_t)address == 0x2F9968) ? JJFB_ERW_SCR_H830 : JJFB_ERW_SCR_W834;
            uc_mem_read(uc, jjfb_guest_ext_erw + off, &got, 4);
        }
        if (do_fix) {
            want = ((uint32_t)address == 0x2F9968)
                       ? (uint32_t)SCREEN_WIDTH
                       : (uint32_t)SCREEN_HEIGHT;
            /* Keep ERW slot in sync — guest sometimes reads memory, not getter. */
            if (jjfb_guest_ext_erw && got != want) {
                uint32_t off = ((uint32_t)address == 0x2F9968)
                                   ? JJFB_ERW_SCR_H830
                                   : JJFB_ERW_SCR_W834;
                uc_mem_write(uc, jjfb_guest_ext_erw + off, &want, 4);
            }
            uc_reg_write(uc, UC_ARM_REG_R0, &want);
            uc_reg_write(uc, UC_ARM_REG_PC, &lr);
            if (n_axis <= 16 || (n_axis % 80) == 0) {
                printf("[JJFB_DIM] #%u pc=0x%X natural=%u -> ret %u (screen=%dx%d)\n",
                       n_axis, (uint32_t)address, got, want, SCREEN_WIDTH, SCREEN_HEIGHT);
                fflush(stdout);
            }
            return;
        }
        if (n_axis <= 8 || (n_axis % 80) == 0) {
            printf("[JJFB_AXIS] #%u pc=0x%X natural=%u (pass-through)\n",
                   n_axis, (uint32_t)address, got);
            fflush(stdout);
        }
    } else if ((uint32_t)address == 0x2D92DC) {
        /* Resource/bmp lazy loader used by chrome: name string in r1. */
        static uint32_t n92dc;
        n92dc++;
        {
            char *s1 = (r1 && getMrpMemPtr(r1)) ? (char *)getMrpMemPtr(r1) : NULL;
            if (s1)
                jjfb_bmp_req_set(s1, r1, lr);
            if (n92dc <= 40 || (n92dc % 200) == 0) {
                char *s0 = (r0 && getMrpMemPtr(r0)) ? (char *)getMrpMemPtr(r0) : NULL;
                printf("[JJFB_310BB4_RESOURCE] 2d92dc #%u lr=0x%X r0=0x%X r1(name)=0x%X r2=0x%X r3=0x%X name=\"%s\"\n",
                       n92dc, lr, r0, r1, r2, r3, s1 ? s1 : "(null/unmapped)");
                if (s0 && !s1) {
                    printf("[JJFB_310BB4_RESOURCE] 2d92dc #%u r0_str=\"%s\"\n", n92dc, s0);
                }
                fflush(stdout);
            }
            if (s1 && jjfb_is_firstscreen_name(s1))
                jjfb_note_firstscreen("2d92dc_name", s1);
            /* Game-state breadcrumbs (login/server/update) ??not chrome polish. */
            if (s1 && (strstr(s1, "login") || strstr(s1, "server") ||
                       strstr(s1, "slogo") || strstr(s1, "update") ||
                       strstr(s1, "autologin"))) {
                printf("[JJFB_GAME_STATE] 2d92dc name=\"%s\" lr=0x%X\n", s1, lr);
                fflush(stdout);
            }
        }
    } else if ((uint32_t)address == 0x306305) {
        /* 0x10140 app tick handler ??source of event_code fed into dispatch/splash. */
        static uint32_t n305;
        uint32_t r5 = 0, r6 = 0, r7 = 0, sp = 0, sp0 = 0, mode = 0;
        n305++;
        uc_reg_read(uc, UC_ARM_REG_R5, &r5);
        uc_reg_read(uc, UC_ARM_REG_R6, &r6);
        uc_reg_read(uc, UC_ARM_REG_R7, &r7);
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        if (sp && getMrpMemPtr(sp))
            uc_mem_read(uc, sp, &sp0, 4);
        if (jjfb_guest_ext_erw)
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_UI_MODE, &mode, 4);
        if (n305 <= 16 || (n305 % 40) == 0) {
            printf("[JJFB_HANDLER_306305] #%u lr=0x%X ui_mode=0x%X "
                   "r0-r3=0x%X,0x%X,0x%X,0x%X r4-r7=0x%X,0x%X,0x%X,0x%X sp0=0x%X\n",
                   n305, lr, mode, r0, r1, r2, r3, r4, r5, r6, r7, sp0);
            fflush(stdout);
        }
    } else if ((uint32_t)address == 0x306344 || (uint32_t)address == 0x30662C) {
        /* UI mode dispatch: load ERW+0x8D0, cmp #0x45 ??bl 0x2EF86C (lr??x306631). */
        static uint32_t n_disp;
        uint32_t mode = 0, ac8 = 0, prog = 0, r5 = 0, r6 = 0, r7 = 0;
        const char *evov;
        n_disp++;
        uc_reg_read(uc, UC_ARM_REG_R5, &r5);
        uc_reg_read(uc, UC_ARM_REG_R6, &r6);
        uc_reg_read(uc, UC_ARM_REG_R7, &r7);
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_UI_MODE, &mode, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &ac8, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C, &prog, 4);
        }
        /* Optional: override event_code (r1) before bl 0x2EF86C. */
        evov = getenv("JJFB_EVENT_CODE");
        if ((uint32_t)address == 0x30662C && evov && evov[0]) {
            uint32_t nev = (uint32_t)strtoul(evov, NULL, 0);
            printf("[JJFB_EVENT_CODE] override at 0x30662C r1=0x%X -> 0x%X\n", r1, nev);
            fflush(stdout);
            r1 = nev;
            uc_reg_write(uc, UC_ARM_REG_R1, &r1);
        }
        if (n_disp <= 24 || (n_disp % 40) == 0 || (uint32_t)address == 0x30662C) {
            printf("[JJFB_UI_DISPATCH] #%u pc=0x%X lr=0x%X ui_mode=0x%X ac8=%d prog=%d "
                   "target=%s event_r1=0x%X r0-r3=0x%X,0x%X,0x%X,0x%X r4-r7=0x%X,0x%X,0x%X,0x%X\n",
                   n_disp, (uint32_t)address, lr, mode, (int32_t)ac8, (int32_t)prog,
                   ((uint32_t)address == 0x30662C) ? "0x2EF86C" : "dispatch_head",
                   r1, r0, r1, r2, r3, r4, r5, r6, r7);
            if ((uint32_t)address == 0x306344)
                printf("[JJFB_HANDLER_306344] event_code_before=0x%X ui_mode=0x%X\n", r1, mode);
            if ((uint32_t)address == 0x30662C)
                printf("[JJFB_DISPATCH_30662C] ui_mode=0x%X event=0x%X\n", mode, r1);
            fflush(stdout);
        }
    } else if ((uint32_t)address == 0x2EF86C) {
        /* Real splash/loading UI (slogo / loadingbar), called when UI_MODE==0x45. */
        static uint32_t n_splash;
        uint32_t mode = 0, ac8 = 0;
        const char *evov;
        n_splash++;
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_UI_MODE, &mode, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &ac8, 4);
        }
        evov = getenv("JJFB_EVENT_CODE");
        if (evov && evov[0]) {
            uint32_t nev = (uint32_t)strtoul(evov, NULL, 0);
            if (r1 != nev) {
                printf("[JJFB_EVENT_CODE] override at 0x2EF86C r1=0x%X -> 0x%X\n", r1, nev);
                fflush(stdout);
                r1 = nev;
                uc_reg_write(uc, UC_ARM_REG_R1, &r1);
            }
        }
        printf("[JJFB_UI_DISPATCH] ui_mode=0x%X target=0x2EF86C caller=0x%X ac8=%d event=0x%X\n",
               mode, lr, (int32_t)ac8, r1);
        printf("[JJFB_FIRST_SCREEN] SPLASH_ENTER #%u lr=0x%X ui_mode=0x%X ac8=%d r0=0x%X r1=0x%X\n",
               n_splash, lr, mode, (int32_t)ac8, r0, r1);
        jjfb_note_firstscreen("splash_fn", "0x2EF86C");
        fflush(stdout);
        jjfb_install_splash_cov(uc);
        /* v47 progress driver OFF by default in v48; only if explicitly set. */
        {
            const char *drv = getenv("JJFB_PROGRESS_DRIVER");
            if (drv && drv[0] && strcmp(drv, "off") != 0 && drv[0] != '0')
                jjfb_progress_driver_apply(uc, n_splash, "splash_enter");
        }
        /*
         * AC8 modes (JJFB_AC8_MODE or JJFB_SPLASH_AC8_MODE):
         *   natural         ??no write (default; chase real startup SM)
         *   force_loading   ??keep AC8=0 (loadingbar/bar/textbar path)
         *   force_slogo_once / force_slogo_once_then_release ??AC8=1 until slogo blit
         *   pulse           ??handled in timer via JJFB_AC8_PULSE_TICKS
         * Legacy JJFB_SLOGO_NUDGE=1 maps to force_slogo_once (deprecated).
         */
        {
            const char *mode_ac8 = getenv("JJFB_AC8_MODE");
            const char *legacy = getenv("JJFB_SLOGO_NUDGE");
            if (!mode_ac8 || !mode_ac8[0])
                mode_ac8 = getenv("JJFB_SPLASH_AC8_MODE");
            if ((!mode_ac8 || !mode_ac8[0]) && legacy && legacy[0] == '1')
                mode_ac8 = "force_slogo_once";
            if (!mode_ac8 || !mode_ac8[0])
                mode_ac8 = "natural";
            if (strcmp(mode_ac8, "force_slogo_once_then_release") == 0)
                mode_ac8 = "force_slogo_once";
            if (jjfb_guest_ext_erw) {
                if (strcmp(mode_ac8, "force_loading") == 0) {
                    uint32_t z = 0;
                    if ((int32_t)ac8 != 0) {
                        uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &z, 4);
                        printf("[JJFB_AC8] force_loading write %d -> 0\n", (int32_t)ac8);
                        fflush(stdout);
                    }
                } else if (strcmp(mode_ac8, "force_slogo_once") == 0) {
                    if (!jjfb_ac8_slogo_once_done) {
                        uint32_t one = 1;
                        if (!jjfb_ac8_slogo_once_armed || (int32_t)ac8 <= 0) {
                            uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &one, 4);
                            jjfb_ac8_slogo_once_armed = 1;
                            printf("[JJFB_AC8] force_slogo_once arm ac8=%d -> 1\n", (int32_t)ac8);
                            fflush(stdout);
                        }
                    }
                } else if (strcmp(mode_ac8, "pulse") == 0) {
                    /* Timer pulse owns AC8; just log. */
                    printf("[JJFB_SPLASH_PHASE] ac8=%d phase=pulse_pending pc=0x2EF86C\n",
                           (int32_t)ac8);
                    fflush(stdout);
                } else {
                    printf("[JJFB_SPLASH_PHASE] ac8=%d phase=%s pc=0x2EF86C (natural)\n",
                           (int32_t)ac8, (int32_t)ac8 > 0 ? "slogo?" : "loading?");
                    fflush(stdout);
                }
            }
        }
        jjfb_install_ac8_watch(uc);
        jjfb_install_slogo_watch(uc);
        jjfb_install_obj_bind_pc_hook(uc);
        jjfb_install_splash_flow_hook(uc);
        jjfb_install_ptr_store_watch(uc);
    } else if ((uint32_t)address == 0x2EF9F4) {
        static uint32_t n_f9f4;
        uint32_t fp150c = 0, y828 = 0, w834 = 0, h830 = 0;
        n_f9f4++;
        jjfb_keep_y828_zero(uc, "2EF9F4");
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_DRAW_FP_150C, &fp150c, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_Y828, &y828, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_W834, &w834, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_H830, &h830, 4);
        }
        if (n_f9f4 <= 8 || (n_f9f4 % 40) == 0) {
            printf("[JJFB_2EF9F4] #%u lr=0x%X drawFP@150C=0x%X scr y828=%d w834=%d h830=%d\n",
                   n_f9f4, lr, fp150c, (int32_t)y828, (int32_t)w834, (int32_t)h830);
            fflush(stdout);
        }
    } else if ((uint32_t)address == 0x2EC6B0 || (uint32_t)address == 0x2EC6B8) {
        /*
         * Splash/bmp draw wrapper (real entry 0x2EC6B8; 0x2EC6B0 is literal pool).
         *   r0=obj  r1=x  r2=y  r3=flags
         *   sp: src_x, w, h, ...
         * Then blx *(ERW+0x150C). Bad FP stops runCode — also guarded at 2EC71A.
         */
        static uint32_t n_c6b0;
        JjfbBmpReq *req;
        uint32_t fp150c = 0, sp = 0, a0 = 0, a1 = 0, a2 = 0;
        int32_t dst_x, dst_y;
        int host_blit = 0; /* v79: default OFF — dual host+DrawBitmap stacked UI.
                            * Natural BLX _DrawBitmap only. JJFB_SPLASH_HOST_BLIT=1 to force. */
        const char *hb;
        n_c6b0++;
        jjfb_keep_y828_zero(uc, "2EC6B8");
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        if (sp && getMrpMemPtr(sp)) {
            uc_mem_read(uc, sp + 0x00, &a0, 4);
            uc_mem_read(uc, sp + 0x04, &a1, 4);
            uc_mem_read(uc, sp + 0x08, &a2, 4);
        }
        if (jjfb_guest_ext_erw)
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_DRAW_FP_150C, &fp150c, 4);
        req = jjfb_bmp_cache_find_object(r0);
        dst_x = (int16_t)r1;
        dst_y = (int16_t)r2;
        if (jjfb_verbose_logs() &&
            (n_c6b0 <= 40 || (n_c6b0 % 40) == 0 ||
             (req && jjfb_is_firstscreen_name(req->name) && (n_c6b0 % 20) == 0))) {
            printf("[JJFB_2EC6B0] #%u lr=0x%X obj=0x%X x=%d y=%d flags=0x%X "
                   "sp0=0x%X sp4=0x%X sp8=0x%X fp150C=0x%X name=%s\n",
                   n_c6b0, lr, r0, dst_x, dst_y, r3, a0, a1, a2, fp150c,
                   req ? req->name : "-");
            fflush(stdout);
        }
        /* obj=0: never blit / never dirty present (avoids purple garbage). */
        if (r0 == 0) {
            uint32_t ret = lr;
            uint32_t r0ret = 0;
            if (n_c6b0 <= 24 || (n_c6b0 % 40) == 0) {
                printf("[JJFB_2EC6B0_SKIP] obj=0 x=%d y=%d -> no blit/dirty, r0=0 ret lr\n",
                       dst_x, dst_y);
                fflush(stdout);
            }
            uc_reg_write(uc, UC_ARM_REG_R0, &r0ret);
            uc_reg_write(uc, UC_ARM_REG_PC, &ret);
            return;
        }
        hb = getenv("JJFB_SPLASH_HOST_BLIT");
        if (hb && hb[0] == '1')
            host_blit = 1;
        if (host_blit && req && req->guest_pixels && req->w > 0 && req->h > 0 &&
            jjfb_is_firstscreen_name(req->name)) {
            jjfb_host_blit_req(uc, req, dst_x, dst_y, "2EC6B0_BLIT");
            jjfb_debug_present_dirty("2EC6B0_BLIT");
            /* force_slogo_once: after first slogo present, release AC8 so loading can run. */
            if (strstr(req->name, "slogo") && jjfb_ac8_slogo_once_armed &&
                !jjfb_ac8_slogo_once_done && jjfb_guest_ext_erw) {
                uint32_t z = 0;
                uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &z, 4);
                jjfb_ac8_slogo_once_done = 1;
                printf("[JJFB_AC8] force_slogo_once release after slogo blit -> ac8=0\n");
                fflush(stdout);
            }
            if (strstr(req->name, "loadingbar") && n_c6b0 <= 8) {
                printf("[JJFB_LAYOUT] loadingbar x=%d y=%d (expect x=(240-201)/2=19 y=320-100=220)\n",
                       dst_x, dst_y);
                fflush(stdout);
            }
        }
        /*
         * v67: ALWAYS skip BLX when drawFP@150C is junk/CODE_ADDRESS.
         * Bad BLX stops runCode at stopAddr mid-2FC26C → 2E2520 never returns
         * → B54 code=5 never dequeues (re-drain loop). Platform draw-FP gap,
         * not FORCE ui / not C0.
         */
        if (fp150c == 0 || fp150c == 0x270F || fp150c < 0x1000 ||
            (fp150c & ~1u) == CODE_ADDRESS ||
            !getMrpMemPtr(fp150c & ~1u)) {
            uint32_t ret = lr;
            uint32_t r0ret = 1;
            const char *rv = getenv("JJFB_2EC6B0_RET");
            if (rv && rv[0]) {
                if (strcmp(rv, "obj") == 0)
                    r0ret = r0;
                else if (strcmp(rv, "pixels") == 0 && req)
                    r0ret = req->guest_pixels;
                else
                    r0ret = (uint32_t)strtoul(rv, NULL, 0);
            }
            if (n_c6b0 <= 24 || (n_c6b0 % 40) == 0) {
                printf("[JJFB_V67_DRAWFP] bad fp150C=0x%X obj=0x%X -> skip BLX "
                       "ret lr (unblock 2FC26C/2E2520 dequeue)\n",
                       fp150c, r0);
                fflush(stdout);
            }
            uc_reg_write(uc, UC_ARM_REG_R0, &r0ret);
            uc_reg_write(uc, UC_ARM_REG_PC, &ret);
            return;
        }
    } else if ((uint32_t)address == 0x2EC6E2) {
        static uint32_t n_c6e2;
        uint32_t r5 = 0, r6 = 0, r7 = 0;
        JjfbBmpReq *req;
        n_c6e2++;
        uc_reg_read(uc, UC_ARM_REG_R5, &r5);
        uc_reg_read(uc, UC_ARM_REG_R6, &r6);
        uc_reg_read(uc, UC_ARM_REG_R7, &r7);
        req = jjfb_bmp_cache_find_object(r4);
        if (!req) req = jjfb_bmp_cache_find_object(r7);
        if (jjfb_verbose_logs() && (n_c6e2 <= 24 || (n_c6e2 % 80) == 0)) {
            printf("[JJFB_2EC6E2] #%u lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                   "r4=0x%X r5=%d r6=%d r7=0x%X name=%s\n",
                   n_c6e2, lr, r0, r1, r2, r3, r4, (int32_t)(int16_t)r5,
                   (int32_t)(int16_t)r6, r7, req ? req->name : "-");
            fflush(stdout);
        }
    } else if ((uint32_t)address == 0x2EC712) {
        static uint32_t n_c712;
        uint32_t r5 = 0, r6 = 0, r7 = 0, fp = 0;
        JjfbBmpReq *req;
        n_c712++;
        uc_reg_read(uc, UC_ARM_REG_R5, &r5);
        uc_reg_read(uc, UC_ARM_REG_R6, &r6);
        uc_reg_read(uc, UC_ARM_REG_R7, &r7);
        /* At 2EC712 r4 is still offset 0x150C; real FP loaded at 2EC714. */
        fp = r4;
        req = jjfb_bmp_cache_find_pixels(r0);
        /* Observe draw args only — do not rewrite x/y or host-blit UI. */
        if (jjfb_verbose_logs() && (n_c712 <= 24 || (n_c712 % 80) == 0)) {
            printf("[JJFB_2EC712_BLX] #%u lr=0x%X fp_off=0x%X px=0x%X x=%d y=%d w_r3=0x%X name=%s\n",
                   n_c712, lr, fp, r0, (int32_t)(int16_t)r1, (int32_t)(int16_t)r2, r3,
                   req ? req->name : "-");
            fflush(stdout);
        }
    } else if ((uint32_t)address == 0x2EC71A) {
        /* v67: BLX r4 — r4 already holds *(ERW+0x150C). Bad target aborts
         * runCode at CODE_ADDRESS mid-2FC26C (2EC6B0 is a literal pool; real
         * entry is 2EC6B8, so entry-site guard never ran). */
        static uint32_t n_blx;
        uint32_t fp = 0, ret_lr = 0, sp = 0;
        uint32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0;
        JjfbBmpReq *req;
        n_blx++;
        uc_reg_read(uc, UC_ARM_REG_R4, &fp);
        uc_reg_read(uc, UC_ARM_REG_LR, &ret_lr);
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        if (sp && getMrpMemPtr(sp)) {
            uc_mem_read(uc, sp + 0x00, &s0, 4);
            uc_mem_read(uc, sp + 0x04, &s1, 4);
            uc_mem_read(uc, sp + 0x08, &s2, 4);
            uc_mem_read(uc, sp + 0x0C, &s3, 4);
            uc_mem_read(uc, sp + 0x10, &s4, 4);
            uc_mem_read(uc, sp + 0x14, &s5, 4);
        }
        req = jjfb_bmp_cache_find_pixels(r0);
        if (jjfb_verbose_logs() && (n_blx <= 24 || (n_blx % 80) == 0)) {
            printf("[JJFB_2EC71A] #%u fp=0x%X p=0x%X x=%d y=%d mw_r3=%u "
                   "w=%u h=%u rop=0x%X fl=0x%X sx=0x%X tr=0x%X name=%s\n",
                   n_blx, fp, r0, (int32_t)(int16_t)r1, (int32_t)(int16_t)r2,
                   r3 & 0xFFFF, s0 & 0xFFFF, s1 & 0xFFFF, s2 & 0xFFFF,
                   s3 & 0xFFFF, s4 & 0xFFFF, s5 & 0xFFFF,
                   req ? req->name : "-");
            fflush(stdout);
        }
        if (fp == 0 || fp == 0x270F || fp < 0x1000 ||
            (fp & ~1u) == CODE_ADDRESS ||
            !getMrpMemPtr(fp & ~1u)) {
            /* Mid-2EC6B8: push{r4-r7,lr}+sub#0x1c — must unwind or stack dies. */
            uint32_t r0ret = 1;
            sp += 0x30;
            if (n_blx <= 24 || (n_blx % 40) == 0) {
                printf("[JJFB_V67_DRAWFP] bad fp150C=0x%X at BLX -> unwind "
                       "ret lr=0x%X (unblock 2FC26C/2E2520 dequeue)\n",
                       fp, ret_lr);
                fflush(stdout);
            }
            uc_reg_write(uc, UC_ARM_REG_SP, &sp);
            uc_reg_write(uc, UC_ARM_REG_R0, &r0ret);
            uc_reg_write(uc, UC_ARM_REG_PC, &ret_lr);
            return;
        }
        if (n_blx <= 8 || (n_blx % 40) == 0) {
            printf("[JJFB_V67_DRAWFP] BLX ok fp=0x%X lr=0x%X\n", fp, ret_lr);
            fflush(stdout);
        }
    } else if ((uint32_t)address == 0x2EFA9E || (uint32_t)address == 0x2EFAB2 ||
               (uint32_t)address == 0x2EFADA) {
        /* Progress/bar loop: draws bar segments when *(BA0+0x2C) > index. */
        static uint32_t n_prog;
        uint32_t r5 = 0, r6 = 0, r7 = 0, prog = 0, bar_obj = 0;
        JjfbBmpReq *breq;
        n_prog++;
        uc_reg_read(uc, UC_ARM_REG_R5, &r5);
        uc_reg_read(uc, UC_ARM_REG_R6, &r6);
        uc_reg_read(uc, UC_ARM_REG_R7, &r7);
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C, &prog, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x20, &bar_obj, 4);
        }
        breq = jjfb_bmp_cache_find_object(bar_obj);
        if ((int32_t)prog > (int32_t)r6) {
            jjfb_progress_drew = 1;
            if (jjfb_verbose_logs() && (n_prog <= 32 || (n_prog % 40) == 0)) {
                printf("[JJFB_PROGRESS_DRAW] #%u pc=0x%X idx=%u count=%d bar_obj=0x%X "
                       "y=%d xbase=%d name=%s\n",
                       n_prog, (uint32_t)address, r6, (int32_t)prog, bar_obj,
                       (int32_t)(int16_t)r5, (int32_t)(int16_t)r7,
                       breq ? breq->name : "-");
                fflush(stdout);
            }
        } else if (jjfb_verbose_logs() && (n_prog <= 24 || (n_prog % 80) == 0)) {
            printf("[JJFB_PROGRESS] #%u pc=0x%X idx=%u count=%d bar_obj=0x%X y=%d "
                   "xbase=%d name=%s SKIP(count<=idx)\n",
                   n_prog, (uint32_t)address, r6, (int32_t)prog, bar_obj,
                   (int32_t)(int16_t)r5, (int32_t)(int16_t)r7,
                   breq ? breq->name : "-");
            fflush(stdout);
        }
    } else if ((uint32_t)address == 0x30680C || (uint32_t)address == 0x30684C) {
        /* Splash draw helper: r0=src, r1/r2 often y/x (clip against screen). */
        static uint32_t n680c;
        uint32_t sp = 0, a0 = 0, a1 = 0, a2 = 0, a3 = 0;
        JjfbBmpReq *req = NULL;
        n680c++;
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        if (sp && getMrpMemPtr(sp)) {
            uc_mem_read(uc, sp + 0x00, &a0, 4);
            uc_mem_read(uc, sp + 0x04, &a1, 4);
            uc_mem_read(uc, sp + 0x08, &a2, 4);
            uc_mem_read(uc, sp + 0x0C, &a3, 4);
        }
        req = jjfb_bmp_cache_find_pixels(r0);
        if (!req) req = jjfb_bmp_cache_find_object(r0);
        if (!req && r3) {
            req = jjfb_bmp_cache_find_pixels(r3);
            if (!req) req = jjfb_bmp_cache_find_object(r3);
        }
        if (n680c <= 40 || (n680c % 80) == 0 || req) {
            printf("[JJFB_30680C] #%u pc=0x%X lr=0x%X r0=0x%X r1=%d r2=%d r3=0x%X "
                   "sp0=0x%X sp4=0x%X sp8=0x%X spC=0x%X name=%s\n",
                   n680c, (uint32_t)address, lr, r0, (int32_t)r1, (int32_t)r2, r3,
                   a0, a1, a2, a3, req ? req->name : "-");
            fflush(stdout);
        }
        /* v79: host blit OFF by default (dual path stacked UI). Opt-in only. */
        {
            const char *hb680 = getenv("JJFB_SPLASH_HOST_BLIT");
            if (hb680 && hb680[0] == '1' &&
                req && req->guest_pixels && req->w > 0 && req->h > 0) {
                int dst_y = (int32_t)r1;
                int dst_x = (int32_t)r2;
                if ((dst_x < 0 || dst_x >= SCREEN_WIDTH || dst_y < 0 || dst_y >= SCREEN_HEIGHT) &&
                    (int32_t)r1 >= 0 && (int32_t)r1 < SCREEN_WIDTH &&
                    (int32_t)r2 >= 0 && (int32_t)r2 < SCREEN_HEIGHT) {
                    dst_x = (int32_t)r1;
                    dst_y = (int32_t)r2;
                }
                if (dst_x < SCREEN_WIDTH && dst_y < SCREEN_HEIGHT) {
                    jjfb_host_blit_req(uc, req, dst_x, dst_y, "30680C_BLIT");
                    jjfb_debug_present_dirty("30680C_BLIT");
                }
            }
        }
    } else if ((uint32_t)address == 0x303D9C) {
        /* v68: real DrawRect wrapper entry (old 0x303D94 was prev-func epilogue). */
        static uint32_t n3d9c;
        uint32_t tbl = 0, fp = 0, slot = 0;
        n3d9c++;
        if (jjfb_guest_ext_base) {
            slot = jjfb_guest_ext_base;
            uc_mem_read(uc, slot, &tbl, 4);
            if (tbl && getMrpMemPtr(tbl + 0x1E8))
                uc_mem_read(uc, tbl + 0x1E8, &fp, 4);
        }
        if (n3d9c <= 12 || (n3d9c % 40) == 0) {
            printf("[JJFB_UI] enter 0x303d9c #%u lr=0x%X r0=0x%X r1=%d r2=%d r3=0x%X\n",
                   n3d9c, lr, r0, (int32_t)r1, (int32_t)r2, r3);
            printf("[JJFB_303D9C] ctx_slot=0x%X tbl=0x%X fp@+1E8=0x%X "
                   "mr_table=0x%X DrawRect=0x%X\n",
                   slot, tbl, fp,
                   mr_table ? toMrpMemAddr(mr_table) : 0,
                   mr_table ? toMrpMemAddr(mr_table) + 0x1E8 : 0);
            fflush(stdout);
        }
    } else if ((uint32_t)address == 0x303DC2) {
        /* blx r7 — DrawRect from *(mr_table+0x1E8). Heal bad FP (platform gap). */
        static uint32_t n3dc2;
        uint32_t r7 = 0, host_dr = 0;
        int bad = 0;
        n3dc2++;
        uc_reg_read(uc, UC_ARM_REG_R7, &r7);
        if (mr_table)
            uc_mem_read(uc, toMrpMemAddr(mr_table) + 0x1E8, &host_dr, 4);
        if (r7 < 0x1000 || r7 == 0x270F ||
            (r7 >= CODE_ADDRESS && r7 < CODE_ADDRESS + 0x10000) ||
            !getMrpMemPtr(r7 & ~1u))
            bad = 1;
        if (n3dc2 <= 16 || (n3dc2 % 40) == 0 || bad)
            printf("[JJFB_UI] 303d9c blx r7=#%u r7=0x%X host_DrawRect=0x%X "
                   "r0=0x%X r1=%d r2=%d bad=%d\n",
                   n3dc2, r7, host_dr, r0, (int32_t)r1, (int32_t)r2, bad);
        if (bad) {
            if (host_dr && getMrpMemPtr(host_dr & ~1u)) {
                printf("[JJFB_303D9C] heal BLX r7 0x%X -> host DrawRect 0x%X\n",
                       r7, host_dr);
                fflush(stdout);
                uc_reg_write(uc, UC_ARM_REG_R7, &host_dr);
            } else {
                /* Mid-303D9C: push{r4-r7,lr}+sub#0xc — unwind before ret. */
                uint32_t sp = 0, ret = lr, zero = 0;
                uc_reg_read(uc, UC_ARM_REG_SP, &sp);
                sp += 0x20;
                printf("[JJFB_303D9C] BAD FP r7=0x%X -> unwind skip ret lr=0x%X\n",
                       r7, ret);
                fflush(stdout);
                uc_reg_write(uc, UC_ARM_REG_SP, &sp);
                uc_reg_write(uc, UC_ARM_REG_R0, &zero);
                uc_reg_write(uc, UC_ARM_REG_PC, &ret);
                return;
            }
        }
    } else if ((uint32_t)address >= 0x310B5C && (uint32_t)address <= 0x310BC0) {
        /* Decorative bmp blit loop ??ABI probe (only hits if chrome un-nop'd).
         * Some Unicorn builds may report a near-by PC inside the prologue, so
         * accept a small window. Note: robotol has a related helper at 0x310B5C
         * and the classic loop at 0x310BB4. */
        static uint32_t n10bb4;
        static uint32_t dump_left;
        uint32_t r5 = 0, r6 = 0, r7 = 0, sp = 0, pc = 0;
        n10bb4++;
        uc_reg_read(uc, UC_ARM_REG_R5, &r5);
        uc_reg_read(uc, UC_ARM_REG_R6, &r6);
        uc_reg_read(uc, UC_ARM_REG_R7, &r7);
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        uc_reg_read(uc, UC_ARM_REG_PC, &pc);
        if (n10bb4 <= 16 || (n10bb4 % 40) == 0) {
            printf("[JJFB_310BB4] #%u lr=0x%X r0=0x%X r1=%d r2=%d r3=0x%X "
                   "r4=0x%X r5=0x%X r6=0x%X r7=0x%X\n",
                   n10bb4, lr, r0, (int32_t)r1, (int32_t)r2, r3, r4, r5, r6, r7);
            fflush(stdout);
        }
        if (jjfb_allow_chrome) {
            if (dump_left == 0) {
                dump_left = 2;
                {
                    const char *d = getenv("JJFB_310BB4_DUMP_N");
                    if (d && d[0]) {
                        int v = atoi(d);
                        if (v > 0 && v < 16) dump_left = (uint32_t)v;
                    }
                }
            }
        }
        if (jjfb_allow_chrome && dump_left) {
            dump_left--;
            printf("[JJFB_310BB4_ENTER] pc=0x%X lr=0x%X sp=0x%X "
                   "r0=0x%X r1=%d r2=%d r3=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X\n",
                   pc, lr, sp, r0, (int32_t)r1, (int32_t)r2, r3, r4, r5, r6, r7);
            fflush(stdout);
            /* Stack window (caller args / locals) */
            jjfb_dump_uc_mem(uc, "310BB4_sp_0x00", sp, 0x40);
            /* Bmp object candidate */
            if (r5) {
                jjfb_dump_guest_ex(uc, "310BB4_r5_obj", r5, 0x80);
                /* Some paths use *(r5+4) as a secondary pointer */
                if (getMrpMemPtr(r5 + 4)) {
                    uint32_t p = 0;
                    uc_mem_read(uc, r5 + 4, &p, 4);
                    if (p) jjfb_dump_guest_ex(uc, "310BB4_r5_p4", p, 0x80);
                }
            }
        }
        if (jjfb_allow_chrome && jjfb_chrome_skip_310bb4) {
            /* Guard: do not enter long/buggy blit loop ??host blit instead. */
            if (jjfb_310bb4_host_blit) {
                uint32_t px = 0;
                JjfbBmpReq *req = NULL;
                int dst_x = (int32_t)r2;
                int dst_y = (int32_t)r1;
                const char *hb = getenv("JJFB_310BB4_HOST_BLIT");
                if (hb && hb[0] == '0')
                    jjfb_310bb4_host_blit = 0;
                if (jjfb_310bb4_host_blit) {
                    if (r5 && getMrpMemPtr(r5 + 4)) {
                        uc_mem_read(uc, r5 + 4, &px, 4);
                        if (px)
                            req = jjfb_bmp_cache_find_pixels(px);
                    }
                    /* Only blit when we can bind the object to a known buffer.
                     * Do NOT fall back to an unrelated last-cache entry. */
                    if (req && req->guest_pixels && req->w > 0 && req->h > 0 &&
                        dst_x < SCREEN_WIDTH && dst_y < SCREEN_HEIGHT) {
                        uint16_t *src = (uint16_t *)getMrpMemPtr(req->guest_pixels);
                        if (src) {
                            int clip_w = req->w;
                            int clip_h = req->h;
                            if (dst_x < 0) { clip_w += dst_x; }
                            if (dst_y < 0) { clip_h += dst_y; }
                            if (dst_x + clip_w > SCREEN_WIDTH)
                                clip_w = SCREEN_WIDTH - (dst_x < 0 ? 0 : dst_x);
                            if (dst_y + clip_h > SCREEN_HEIGHT)
                                clip_h = SCREEN_HEIGHT - (dst_y < 0 ? 0 : dst_y);
                            jjfb_screen_blit_rgb565(src, req->w, req->h, dst_x, dst_y);
                            if (n10bb4 <= 24 || (n10bb4 % 40) == 0) {
                                printf("[JJFB_310BB4_BLIT] name=%s x=%d y=%d w=%d h=%d "
                                       "src=0x%X clip=%dx%d\n",
                                       req->name, dst_x, dst_y, req->w, req->h,
                                       req->guest_pixels, clip_w, clip_h);
                                fflush(stdout);
                            }
                            jjfb_debug_present_dirty("310BB4_BLIT");
                        }
                    } else if (n10bb4 <= 16 || (n10bb4 % 40) == 0) {
                        printf("[JJFB_310BB4_BLIT] skip unbound r5=0x%X px=0x%X "
                               "x=%d y=%d\n",
                               r5, px, dst_x, dst_y);
                        fflush(stdout);
                    }
                }
            }
            if (n10bb4 <= 16 || (n10bb4 % 40) == 0) {
                printf("[JJFB_310BB4_GUARD] skip body, return to lr=0x%X\n", lr);
                fflush(stdout);
            }
            {
                uint32_t one = 1;
                uc_reg_write(uc, UC_ARM_REG_R0, &one);
                uc_reg_write(uc, UC_ARM_REG_PC, &lr);
            }
            return;
        }
    } else if ((uint32_t)address == 0x2E885A) {
        n885a++;
        if (n885a <= 8 || (n885a % 40) == 0)
            printf("[JJFB_UI] after 0x2f284c @0x2e885a #%u (about to setup 305bf4)\n", n885a);
    } else if ((uint32_t)address == 0x305BF4) {
        nbf4++;
        if (nbf4 <= 8 || (nbf4 % 40) == 0)
            printf("[JJFB_UI] enter 0x305bf4 #%u lr=0x%X r0=0x%X y=%d xish=%d r3=0x%X\n",
                   nbf4, lr, r0, (int32_t)r1, (int32_t)r2, r3);
    } else if ((uint32_t)address == 0x305C34) {
        nc34++;
        if (nc34 <= 8 || (nc34 % 40) == 0)
            printf("[JJFB_UI] enter 0x305c34 #%u lr=0x%X r0=0x%X r1=%d r2=%d r3=0x%X\n",
                   nc34, lr, r0, (int32_t)r1, (int32_t)r2, r3);
    } else if ((uint32_t)address == 0x2F2358) {
        n2358++;
        printf("[JJFB_UI] enter 0x2f2358 #%u lr=0x%X r0=0x%X r1=%d r2=%d r3=0x%X\n",
               n2358, lr, r0, (int32_t)r1, (int32_t)r2, r3);
    }
    fflush(stdout);
}

/* Heal CODE-junk graphics FPs in mythroad's shadow mr_table copy.
 * Do NOT retarget *(ext_base) wholesale to host mr_table ??that makes
 * c2u/sprintf/strncpy hit UNIMPL host stubs and kills text objects
 * (code becomes 0xFFFFFFFF). Only patch draw-related slots. */
static void jjfb_heal_shadow_gfx(uc_engine *uc, const char *why) {
    uint32_t shadow = 0, host = 0;
    static const uint32_t gfx_off[] = {
        0x74,  /* mr_drawBitmap */
        0x1D8, /* _DispUpEx */
        0x1DC, /* _DrawPoint */
        0x1E0, /* _DrawBitmap */
        0x1E4, /* _DrawBitmapEx */
        0x1E8, /* DrawRect ??was 0x80278 junk */
        0x1EC, /* _DrawText */
        0x200  /* _DrawTextEx */
    };
    uint32_t gi, healed = 0;
    if (!uc || !mr_table || !jjfb_guest_ext_base) return;
    host = toMrpMemAddr(mr_table);
    uc_mem_read(uc, jjfb_guest_ext_base, &shadow, 4);
    if (!shadow || shadow == host || !getMrpMemPtr(shadow)) return;
    for (gi = 0; gi < (uint32_t)(sizeof(gfx_off) / sizeof(gfx_off[0])); gi++) {
        uint32_t src = 0, dst = 0;
        uc_mem_read(uc, host + gfx_off[gi], &src, 4);
        uc_mem_read(uc, shadow + gfx_off[gi], &dst, 4);
        if (src && (dst < 0x1000 ||
                    (dst >= CODE_ADDRESS && dst < CODE_ADDRESS + 0x10000) ||
                    dst == 0x270F)) {
            uc_mem_write(uc, shadow + gfx_off[gi], &src, 4);
            printf("[JJFB_CTX] heal(%s) shadow@0x%X +0x%X 0x%X -> 0x%X\n",
                   why, shadow, gfx_off[gi], dst, src);
            healed++;
        }
    }
    if (healed)
        printf("[JJFB_CTX] heal(%s) shadow@0x%X fixed %u gfx slots "
               "(GOT left as shadow; host mr_table=0x%X)\n",
               why, shadow, healed, host);
    fflush(stdout);
}

static void jjfb_install_ui_hooks(uc_engine *uc) {
    static const uint32_t addrs[] = {
        0x2D92DC,
        0x2F995C, /* screen W getter (ERW+0x834) ??splash Y base */
        0x2F9968, /* screen H getter (ERW+0x830) ??splash X center needs WIDTH */
        0x306344, /* UI dispatch head: load ui_mode, cmp #0x45 */
        0x30662C, /* bl 0x2EF86C when ui_mode==0x45 (lr??x306631) */
        0x306305, /* 0x10140 timer handler ??event_code source for splash */
        0x2EF86C, /* splash/loading screen: loads slogo/loadingbar */
        0x2EF9F4, /* splash load helpers (2d92dc loadingbar/bar/textbar) */
        0x2EC6B0, /* literal pool near draw wrapper (kept for xref) */
        0x2EC6B8, /* real Thumb entry (PUSH) of splash bmp draw wrapper */
        0x2EC6E2, /* inside 2EC6B0: prepare args / path when sp flag==0 */
        0x2EC712, /* load draw FP offset */
        0x2EC71A, /* v67: BLX draw FP — guard bad target */
        0x2EFA9E, /* progress loop head (r6 index) */
        0x2EFAB2, /* progress: load bar obj from BA0+0x20 */
        0x2EFADA, /* progress: bl 2EC6B0 bar segment */
        0x30680C, /* likely splash text/bmp draw (r1/r2 coords) */
        0x30684C, /* 30680c helper */
        0x2E87AC, 0x2F284C, 0x2EA180, 0x2F449C, 0x2F46D6,
        0x303D9C, 0x303DC2,
        /* 310bb4 family is Thumb; hook both 0x310B5C helper and 0x310BB4 loop. */
        0x310B5C, 0x310B5D, 0x310B5E, 0x310B5F,
        0x310BB0, 0x310BB4, 0x310BB5, 0x310BB6, 0x310BB7, 0x310BB8, 0x310BB9,
        0x2E885A, 0x305BF4, 0x305C34, 0x2F2358
    };
    uint32_t i;
    uc_err err;
    if (!uc || jjfb_ui_hooks_installed) return;
    /* Read chrome experiment switches once. */
    if (!jjfb_allow_chrome) {
        const char *e = getenv("JJFB_ALLOW_CHROME");
        if (e && e[0] && e[0] != '0') {
            jjfb_allow_chrome = 1;
            jjfb_chrome_allow_calls = 30;
            {
                const char *n = getenv("JJFB_CHROME_ALLOW_CALLS");
                if (n && n[0]) {
                    int v = atoi(n);
                    if (v > 0) jjfb_chrome_allow_calls = (uint32_t)v;
                }
            }
            jjfb_chrome_allow_calls_init = jjfb_chrome_allow_calls;
            {
                const char *s = getenv("JJFB_CHROME_SKIP_310BB4");
                if (s && s[0] == '0') jjfb_chrome_skip_310bb4 = 0;
            }
            printf("[JJFB_CHROME] enabled allow_calls=%u skip_310bb4=%d\n",
                   jjfb_chrome_allow_calls, jjfb_chrome_skip_310bb4);
            fflush(stdout);
        }
    }
    jjfb_heal_shadow_gfx(uc, "ui_hooks");
    if (jjfb_allow_chrome && !jjfb_310bb4_sweep_installed) {
        /* Wide net: confirm whether we ever execute in/near 310bb4 region. */
        err = uc_hook_add(uc, &jjfb_310bb4_sweep_hook, UC_HOOK_CODE,
                          jjfb_hook_310bb4_sweep, NULL,
                          0x310B00, 0x310C80, 0);
        if (!err) {
            jjfb_310bb4_sweep_installed = 1;
            printf("[JJFB_310_SWEEP] installed range 0x310B00..0x310C80\n");
            fflush(stdout);
        }
    }
    for (i = 0; i < (uint32_t)(sizeof(addrs) / sizeof(addrs[0])); i++) {
        err = uc_hook_add(uc, &jjfb_ui_hooks[i], UC_HOOK_CODE, jjfb_hook_ui_code, NULL,
                          addrs[i], addrs[i] + 2, 0);
        if (err)
            printf("[JJFB_UI] hook 0x%X failed err=%u\n", addrs[i], err);
    }
    /* Watch the graphics-table pointer slot used by 303d94 (ext_base+0). */
    if (jjfb_guest_ext_base && !jjfb_ctx_mem_hook_installed) {
        err = uc_hook_add(uc, &jjfb_ctx_mem_hook, UC_HOOK_MEM_WRITE,
                          jjfb_hook_ctx_mem_write, NULL,
                          jjfb_guest_ext_base, jjfb_guest_ext_base + 4);
        if (err)
            printf("[JJFB_CTX_WRITE] hook slot 0x%X failed err=%u\n",
                   jjfb_guest_ext_base, err);
        else {
            jjfb_ctx_mem_hook_installed = 1;
            printf("[JJFB_CTX_WRITE] watching tbl slot @0x%X (303d94 source)\n",
                   jjfb_guest_ext_base);
        }
    }
    jjfb_ui_hooks_installed = 1;
    jjfb_install_slogo_watch(uc);
    jjfb_install_obj_bind_pc_hook(uc);
    jjfb_install_ptr_store_watch(uc);
    jjfb_install_splash_cov(uc);
    jjfb_install_uimode_writer_hooks(uc);
    /* Wide net around suspected splash draw helper. */
    {
        static uc_hook h680;
        static int h680_on;
        if (!h680_on) {
            err = uc_hook_add(uc, &h680, UC_HOOK_CODE, jjfb_hook_ui_code, NULL,
                              0x306800, 0x3068C0, 0);
            if (!err) {
                h680_on = 1;
                printf("[JJFB_30680C] sweep installed 0x306800..0x3068C0\n");
            }
        }
    }
    printf("[JJFB_UI] installed probes through 2f449c/303d9c/310bb4/305bf4/30680c\n");
    fflush(stdout);
}

static void jjfb_dump_robotol_state(uc_engine *uc, const char *why) {
    uint32_t st = 0, ptr = 0, v7e4 = 0, draw_fp = 0, hw = 0;
    uint32_t scr_w824 = 0, scr_y828 = 0, scr_h830 = 0, scr_w834 = 0;
    uint8_t f_c44 = 0, f_c9d = 0, f_cd1 = 0, f_cf5 = 0;
    if (!uc || !jjfb_guest_ext_erw) return;
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE, &st, 4);
    {
        uint32_t ui_mode = 0;
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_UI_MODE, &ui_mode, 4);
        printf("[JJFB_SEND] %s state=%u(0x%X) ui_mode=0x%X tick=%u uptime=%u\n",
               why, st, st, ui_mode, jjfb_handler_tick,
               (uint32_t)((uint64_t)get_uptime_ms() - uptime_ms));
        {
            static uint32_t last_st = 0xFFFFFFFFu;
            static uint32_t last_mode = 0xFFFFFFFFu;
            if (st != last_st) {
                printf("[JJFB_FIRST_SCREEN] state_change %u -> %u (tick=%u why=%s)\n",
                       last_st == 0xFFFFFFFFu ? 0 : last_st, st, jjfb_handler_tick, why);
                fflush(stdout);
                last_st = st;
            }
            if (ui_mode != last_mode) {
                printf("[JJFB_FIRST_SCREEN] ui_mode_change 0x%X -> 0x%X (tick=%u) "
                       "(0x45=splash/slogo)\n",
                       last_mode == 0xFFFFFFFFu ? 0 : last_mode, ui_mode, jjfb_handler_tick);
                fflush(stdout);
                last_mode = ui_mode;
            }
            if (st <= 3 || ui_mode == JJFB_UI_MODE_SPLASH)
                jjfb_note_firstscreen("state", why);
        }
    }
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_FLAG_C44, &f_c44, 1);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_FLAG_C9D, &f_c9d, 1);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_FLAG_CD1, &f_cd1, 1);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_FLAG_CF5, &f_cf5, 1);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PTR_11B0, &ptr, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_BASE_7D8 + 0xC, &v7e4, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + 0xD14, &hw, 2);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_DRAW_FP, &draw_fp, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_W824, &scr_w824, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_Y828, &scr_y828, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_H830, &scr_h830, 4);
    uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SCR_W834, &scr_w834, 4);
    printf("[JJFB_SEND] %s gates C44=%d C9D=%d CD1=%d CF5=%d ptr11B0=0x%X\n",
           why, f_c44, f_c9d, f_cd1, f_cf5, ptr);
    printf("[JJFB_SEND] %s drawGate 7D8+0xC=%d(0x%X) hwD14=%d drawFP@1510=0x%X\n",
           why, (int32_t)v7e4, v7e4, (int16_t)hw, draw_fp);
    {
        static uint32_t last_fp1510;
        if (draw_fp != last_fp1510) {
            printf("[JJFB_1510_WRITE] observed change old=0x%X new=0x%X (%s)\n",
                   last_fp1510, draw_fp, why);
            last_fp1510 = draw_fp;
        }
    }
    printf("[JJFB_SEND] %s screen 824w=%u 828y=%u 830h=%u 834w=%u\n",
           why, scr_w824, scr_y828, scr_h830, scr_w834);
    {
        uint32_t fp144c = 0, fp1450 = 0;
        uc_mem_read(uc, jjfb_guest_ext_erw + 0x144C, &fp144c, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0x1450, &fp1450, 4);
        printf("[JJFB_SEND] %s fp 144C(memset)=0x%X 1450(strlen)=0x%X\n",
               why, fp144c, fp1450);
    }
    fflush(stdout);
}

int jjfb_robotol_timer_active(void) {
    return jjfb_ext_timer_running;
}

void jjfb_arm_robotol_timer(uint16_t period_ms) {
    if (period_ms == 0) period_ms = 50;
    if (period_ms > 10000) period_ms = 50;
    jjfb_ext_timer_period = period_ms;
    jjfb_ext_timer_running = 1;
    timerStart(period_ms);
    printf("[JJFB_SEND] ARM robotol timer period=%u RUNNING=1\n", period_ms);
    fflush(stdout);
}

/* Forward: defined below with flush helper. */
void jjfb_lifecycle_family_app(uc_engine *uc, uint32_t app);

static uint32_t jjfb_ensure_ret_stub(uc_engine *uc, uint32_t *slot, uint8_t retv) {
    if (*slot) return *slot;
    void *p = my_mallocExt(4);
    if (!p) return 0;
    /* Thumb: movs r0, #retv; bx lr */
    uint8_t code[4] = {(uint8_t)(retv & 0xff), 0x20, 0x70, 0x47};
    memcpy(p, code, 4);
    *slot = toMrpMemAddr(p);
    (void)uc;
    printf("[JJFB_SEND] installed ret%u stub @0x%X\n", retv, *slot);
    fflush(stdout);
    return *slot;
}

/* Call a guest Thumb handler with R9=ER_RW. Must not be nested inside runCode. */
static void jjfb_run_guest_thumb4(uc_engine *uc, uint32_t addr,
                                  uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    uint32_t r9, sp, lr, z = 0;
    int verbose;
    if (!addr || !uc) return;
    verbose = ((jjfb_handler_tick % 20) == 1);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    if (jjfb_guest_ext_erw) {
        uc_reg_write(uc, UC_ARM_REG_R9, &jjfb_guest_ext_erw);
    }
    /* Family dispatcher may read two stack args at entry+0x20/0x24. */
    sp -= 8;
    uc_mem_write(uc, sp, &z, 4);
    uc_mem_write(uc, sp + 4, &z, 4);
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    uc_reg_write(uc, UC_ARM_REG_R1, &r1);
    uc_reg_write(uc, UC_ARM_REG_R2, &r2);
    uc_reg_write(uc, UC_ARM_REG_R3, &r3);
    lr = CODE_ADDRESS;
    uc_reg_write(uc, UC_ARM_REG_LR, &lr);
    if (verbose) {
        printf("[JJFB_SEND] call fam@0x%X r0=%u r1=0x%X r2=0x%X r3=0x%X\n",
               addr, r0, r1, r2, r3);
        fflush(stdout);
    }
    runCode(uc, addr & ~1u, CODE_ADDRESS, true);
    if (verbose) {
        uint32_t rv = 0;
        uc_reg_read(uc, UC_ARM_REG_R0, &rv);
        printf("[JJFB_SEND] fam@0x%X ret=%d\n", addr, (int32_t)rv);
        fflush(stdout);
    }
    sp += 8;
    uc_reg_write(uc, UC_ARM_REG_R9, &r9);
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
}

static void jjfb_flush_1e200(uc_engine *uc) {
    int guard = 0;
    if (!uc || !jjfb_plat_handler_1e200) return;
    while (jjfb_pending_1e200 && guard++ < 8) {
        uint32_t a = jjfb_pend_app, c = jjfb_pend_code, p0 = jjfb_pend_p0, p1 = jjfb_pend_p1;
        jjfb_pending_1e200 = 0;
        if ((jjfb_handler_tick % 20) == 1 || (a != 9 && a != 0x18)) {
            printf("[JJFB_SEND] flush 0x1E200 fam app=%u code=0x%X\n", a, c);
            fflush(stdout);
        }
        jjfb_in_guest_handler = 1;
        /* app=24 ??robotol 0x3130ec(code); app=9 ??0x305e00(code) [no-op if code==0] */
        if (a == 0x18 && ((jjfb_handler_tick % 20) == 1)) {
            printf("[JJFB_SEND] app24 -> 0x3130EC(code=%d) via fam\n", (int16_t)c);
            fflush(stdout);
        }
        jjfb_run_guest_thumb4(uc, jjfb_plat_handler_1e200, a, c, p0, p1);
        jjfb_in_guest_handler = 0;
    }
}

void jjfb_lifecycle_family_app(uc_engine *uc, uint32_t app) {
    uint32_t tptr = 0;
    if (!uc) return;
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    if (!jjfb_plat_handler_1e200) {
        printf("[JJFB_V60_LIFECYCLE] family app=%u skipped (no 0x10102 handler yet)\n",
               app);
        fflush(stdout);
        pthread_mutex_unlock(&mutex);
        return;
    }
    printf("[JJFB_V60_LIFECYCLE] family app=%u via handler=0x%X "
           "(app2→30E15E→30CBBC timerCreate; not C0)\n",
           app, jjfb_plat_handler_1e200);
    fflush(stdout);
    jjfb_pend_app = app;
    jjfb_pend_code = 0;
    jjfb_pend_p0 = 0;
    jjfb_pend_p1 = 0;
    jjfb_pending_1e200 = 1;
    jjfb_flush_1e200(uc);
    if (jjfb_guest_ext_erw) {
        /* 2F5390 loads timer from ERW+0x858+0x6C = ERW+0x8C4 */
        uint8_t v15d = 0, vb71 = 0, vb70 = 0, v134d = 0;
        uc_mem_read(uc, jjfb_guest_ext_erw + 0x8C4, &tptr, 4);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0x15D, &v15d, 1);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &vb71, 1);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0xB70, &vb70, 1);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0x134D, &v134d, 1);
        printf("[JJFB_V60_LIFECYCLE] after family app=%u ERW+0x8C4 timer=0x%X%s\n",
               app, tptr, tptr ? " OK" : " STILL_NULL");
        printf("[JJFB_V62_FLAG] after family app=%u 15D=%u B71=%u B70=%u 134D=%u "
               "(305EB8 needs 15D==1 B71!=0 134D==0)\n",
               app, v15d, vb71, vb70, v134d);
        fflush(stdout);
        jjfb_install_v62_flag_mem_hooks(uc);
    }
    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
}

static void jjfb_run_guest_thumb(uc_engine *uc, uint32_t addr, uint32_t r0, uint32_t r1) {
    uint32_t r9, sp, lr;
    int verbose;
    if (!addr || !uc) return;
    verbose = ((jjfb_handler_tick % 20) == 1);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    if (jjfb_guest_ext_erw) {
        uc_reg_write(uc, UC_ARM_REG_R9, &jjfb_guest_ext_erw);
    }
    uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    uc_reg_write(uc, UC_ARM_REG_R1, &r1);
    lr = CODE_ADDRESS;
    uc_reg_write(uc, UC_ARM_REG_LR, &lr);
    if (verbose) {
        printf("[JJFB_SEND] call guest handler@0x%X r0=0x%X r1=0x%X\n", addr, r0, r1);
        fflush(stdout);
    }
    jjfb_in_guest_handler = 1;
    runCode(uc, addr & ~1u, CODE_ADDRESS, true);
    jjfb_in_guest_handler = 0;
    if (verbose) {
        uint32_t rv = 0;
        uc_reg_read(uc, UC_ARM_REG_R0, &rv);
        printf("[JJFB_SEND] guest handler@0x%X ret=%d\n", addr, (int32_t)rv);
        fflush(stdout);
    }
    uc_reg_write(uc, UC_ARM_REG_R9, &r9);
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    /* Deliver deferred 0x1E20x family work outside nested emu. */
    jjfb_flush_1e200(uc);
}

/* v63 PROBE: deliver one Mythroad event into robotol helper (code=1 / mrc_event).
 * Path A: code 5 (MR_MENU_RETURN) or 12 (MR_MOUSE_MOVE) → 2E4040 → 2DADC4.
 * Must be called with mutex already held (same as timer tick / ext_call). */
static int jjfb_robotol_inject_mrc_event(uc_engine *uc, int32_t ev_code,
                                         int32_t p0, int32_t p1) {
    int32_t ev[5];
    void *emem;
    uint32_t Paddr, helper, erw, in_addr, code_u, il;
    uint32_t r9, sp, oh_a, ol_a, rv = 0;
    uint32_t *oh = NULL;
    int32_t *ol = NULL;
    if (!uc || !jjfb_guest_ext_P || !jjfb_guest_ext_helper) return -1;
    memset(ev, 0, sizeof(ev));
    ev[0] = ev_code;
    ev[1] = p0;
    ev[2] = p1;
    emem = my_mallocExt(sizeof(ev));
    if (!emem) return -1;
    memcpy(emem, ev, sizeof(ev));
    Paddr = jjfb_guest_ext_P;
    helper = jjfb_guest_ext_helper;
    erw = jjfb_guest_ext_erw;
    in_addr = toMrpMemAddr(emem);
    code_u = 1;
    il = (uint32_t)sizeof(ev);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    if (erw) uc_reg_write(uc, UC_ARM_REG_R9, &erw);
    oh = (uint32_t *)my_mallocExt(4);
    ol = (int32_t *)my_mallocExt(4);
    if (!oh || !ol) {
        if (oh) my_freeExt(oh);
        if (ol) my_freeExt(ol);
        my_freeExt(emem);
        uc_reg_write(uc, UC_ARM_REG_R9, &r9);
        return -1;
    }
    *oh = 0;
    *ol = 0;
    oh_a = toMrpMemAddr(oh);
    ol_a = toMrpMemAddr(ol);
    sp -= 8;
    uc_mem_write(uc, sp, &oh_a, 4);
    uc_mem_write(uc, sp + 4, &ol_a, 4);
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    uc_reg_write(uc, UC_ARM_REG_R0, &Paddr);
    uc_reg_write(uc, UC_ARM_REG_R1, &code_u);
    uc_reg_write(uc, UC_ARM_REG_R2, &in_addr);
    uc_reg_write(uc, UC_ARM_REG_R3, &il);
    runCode(uc, helper & ~1u, CODE_ADDRESS, (helper & 1u) != 0);
    uc_reg_read(uc, UC_ARM_REG_R0, &rv);
    sp += 8;
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    uc_reg_write(uc, UC_ARM_REG_R9, &r9);
    my_freeExt(oh);
    my_freeExt(ol);
    my_freeExt(emem);
    return (int32_t)rv;
}

/* v64 PROBE: call plat 0x10165 enqueue handler (30D2F8→30D24C→2E4D6C→B54).
 * v63 helper mrc_event(5) is a no-op and never feeds 2E2520. */
static void jjfb_v64_enqueue_probe_tick(uc_engine *uc) {
    static int done;
    const char *env;
    uint32_t r0, r1, rv = 0;
    uint8_t v15d = 0;
    if (done || !uc) return;
    if (jjfb_handler_tick < 12) return;
    env = getenv("JJFB_V64_ENQUEUE_ONCE");
    if (!env || !env[0] || env[0] == '0') return;
    done = 1;
    if (!jjfb_plat_handler_10165) {
        printf("[JJFB_V64_ENQ] PROBE skipped: 0x10165 handler never registered\n");
        fflush(stdout);
        return;
    }
    r0 = jjfb_plat_buf_10165 ? jjfb_plat_buf_10165 : jjfb_plat_buf_10162;
    r1 = jjfb_plat_buf_10162 ? jjfb_plat_buf_10162 : jjfb_plat_buf_10165;
    if (!r0 || !r1) {
        printf("[JJFB_V64_ENQ] PROBE skipped: missing 10165/10162 buf "
               "b65=0x%X b62=0x%X\n",
               jjfb_plat_buf_10165, jjfb_plat_buf_10162);
        fflush(stdout);
        return;
    }
    if (jjfb_guest_ext_erw)
        uc_mem_read(uc, jjfb_guest_ext_erw + 0x15D, &v15d, 1);
    printf("[JJFB_V64_ENQ] PROBE once handler=0x%X r0=0x%X r1=0x%X tick=%u 15D=%u "
           "(30D2F8→2E4D6C→B54; not FORCE ui_mode / not C0 / not mrc_event)\n",
           jjfb_plat_handler_10165, r0, r1, jjfb_handler_tick, v15d);
    fflush(stdout);
    jjfb_in_guest_handler = 1;
    jjfb_run_guest_thumb(uc, jjfb_plat_handler_10165, r0, r1);
    jjfb_in_guest_handler = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &rv);
    printf("[JJFB_V64_ENQ] PROBE ret=%d (expect next 2DC80C to reach 2DC8D4/2E2520)\n",
           (int32_t)rv);
    fflush(stdout);
}

/* v74: second 10165 after leave_2FC26C; 101AB fills one 2F68E4 record so
 * 2DADC4 hits nonempty-B58 → 30ED2C (natural B71). Not FORCE B71/B70/C0. */
static void jjfb_v74_b58_second_enqueue_tick(uc_engine *uc) {
    static int wait_ticks;
    uint32_t r0, r1, rv = 0;
    uint8_t v15d = 0, vb71 = 0;
    if (!uc || !jjfb_v74_second_enq_armed || jjfb_v74_second_enq_done) return;
    if (!jjfb_v74_second_enq_enable) {
        jjfb_v74_second_enq_armed = 0;
        return;
    }
    wait_ticks++;
    /* Let first Path A fully leave + drain before second enqueue. */
    if (wait_ticks < 4) return;
    if (!jjfb_plat_handler_10165) {
        printf("[JJFB_V74_B58] second enqueue skipped: no 10165 handler\n");
        fflush(stdout);
        jjfb_v74_second_enq_done = 1;
        jjfb_v74_second_enq_armed = 0;
        return;
    }
    r0 = jjfb_plat_buf_10165 ? jjfb_plat_buf_10165 : jjfb_plat_buf_10162;
    r1 = jjfb_plat_buf_10162 ? jjfb_plat_buf_10162 : jjfb_plat_buf_10165;
    if (!r0 || !r1) {
        printf("[JJFB_V74_B58] second enqueue skipped: missing buf\n");
        fflush(stdout);
        jjfb_v74_second_enq_done = 1;
        jjfb_v74_second_enq_armed = 0;
        return;
    }
    jjfb_101ab_fill_records = 1;
    if (jjfb_guest_ext_erw) {
        uc_mem_read(uc, jjfb_guest_ext_erw + 0x15D, &v15d, 1);
        uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &vb71, 1);
    }
    printf("[JJFB_V74_B58] second enqueue handler=0x%X tick=%u 15D=%u B71=%u "
           "(101AB one-record; expect 2F68E4 count>=1 → 30ED2C)\n",
           jjfb_plat_handler_10165, jjfb_handler_tick, v15d, vb71);
    fflush(stdout);
    jjfb_in_guest_handler = 1;
    jjfb_run_guest_thumb(uc, jjfb_plat_handler_10165, r0, r1);
    jjfb_in_guest_handler = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &rv);
    printf("[JJFB_V74_B58] second enqueue ret=%d\n", (int32_t)rv);
    fflush(stdout);
    jjfb_101ab_fill_records = 0;
    jjfb_v74_second_enq_done = 1;
    jjfb_v74_second_enq_armed = 0;
}

/* v75: after B71=1, family app=0xC0 → 30DC44 → 2FEBBC → strb B70.
 * Primary: leave_30ED2C arms deferred C0 + emu_stop; run_guest_thumb flushes.
 * Timer backup: if still pending, flush at top level. */
static void jjfb_v75_family_c0_after_b71_tick(uc_engine *uc) {
    uint8_t vb71 = 0, vb70 = 0;
    const char *env;
    if (!uc) return;
    env = getenv("JJFB_FAMILY_C0_AFTER_B71");
    if (env && env[0] == '0') {
        jjfb_v75_c0_enable = 0;
        jjfb_v75_c0_done = 1;
        return;
    }
    if (!jjfb_v75_c0_enable) return;
    /* Top-level delivery if arm left pending (should already be flushed). */
    if (jjfb_pending_1e200 && jjfb_pend_app == 0xC0 && !jjfb_in_guest_handler) {
        printf("[JJFB_V75_B70] timer flush deferred family C0 tick=%u\n",
               jjfb_handler_tick);
        fflush(stdout);
        jjfb_flush_1e200(uc);
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB70, &vb70, 1);
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &vb71, 1);
            jjfb_cached_b71 = vb71;
            printf("[JJFB_V75_B70] after deferred C0: B71=%u B70=%u\n",
                   vb71, vb70);
            fflush(stdout);
        }
        return;
    }
    if (jjfb_v75_c0_done) return;
    if (jjfb_guest_ext_erw)
        uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &vb71, 1);
    else
        vb71 = jjfb_cached_b71;
    if (vb71 == 0) return;
    if (!jjfb_plat_handler_1e200) return;
    printf("[JJFB_V75_B70] timer backup arm family C0 B71=%u tick=%u\n",
           vb71, jjfb_handler_tick);
    fflush(stdout);
    jjfb_v75_try_c0_before_b70_gate(uc);
    if (jjfb_pending_1e200 && !jjfb_in_guest_handler)
        jjfb_flush_1e200(uc);
}

static void jjfb_v63_path_a_probe_tick(uc_engine *uc) {
    static int done;
    const char *env;
    int code;
    uint8_t v15d = 0;
    if (done || !uc || !jjfb_guest_ext_erw) return;
    /* Fire once after app2 has set 15D and timer is alive. */
    if (jjfb_handler_tick < 12) return;
    env = getenv("JJFB_PATH_A_EVENT_ONCE");
    if (!env || !env[0] || env[0] == '0') return;
    code = atoi(env);
    if (code != 5 && code != 12) {
        printf("[JJFB_V63_PATH_A] ignore JJFB_PATH_A_EVENT_ONCE=%s "
               "(only 5=MENU_RETURN or 12=MOUSE_MOVE)\n", env);
        done = 1;
        return;
    }
    uc_mem_read(uc, jjfb_guest_ext_erw + 0x15D, &v15d, 1);
    printf("[JJFB_V63_PATH_A] PROBE once code=%d (%s) tick=%u 15D=%u "
           "(NOTE: mrc_event 1..5 is no-op; prefer JJFB_V64_ENQUEUE_ONCE)\n",
           code, code == 5 ? "MR_MENU_RETURN" : "MR_MOUSE_MOVE",
           jjfb_handler_tick, v15d);
    fflush(stdout);
    {
        int32_t rr = jjfb_robotol_inject_mrc_event(uc, code, 0, 0);
        printf("[JJFB_V63_PATH_A] mrc_event(%d) ret=%d\n", code, rr);
        fflush(stdout);
    }
    done = 1;
}

void jjfb_on_robotol_timer_tick(uc_engine *uc) {
    uint32_t st = 0;
    if (!uc || !jjfb_ext_timer_running) return;
    jjfb_handler_tick++;

    /* v87: pin y828=0 before guest splash draws (2EC6B8 adds this to every Y). */
    jjfb_keep_y828_zero(uc, "timer");

    /* v85/v72: begin-frame wipe is opt-in only (JJFB_FRAME_CLEAR=1).
     * Default OFF so guest DispUp / DrawRect own the framebuffer — host wipe
     * made the splash look like a simulated black-clear loop. */
    {
        const char *fc = getenv("JJFB_FRAME_CLEAR");
        int do_clear = (fc && fc[0] == '1');
        if (do_clear) {
            static uint32_t n_fc;
            jjfb_screen_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
            n_fc++;
            if (n_fc <= 4 || (n_fc % 60) == 0) {
                printf("[JJFB_V85_FRAME] begin-tick clear #%u tick=%u "
                       "(opt-in JJFB_FRAME_CLEAR=1)\n",
                       n_fc, jjfb_handler_tick);
                fflush(stdout);
            }
        }
    }
    /* Clocks come from mr_getTime / get_uptime_ms. Do NOT write uptime into
     * ER_RW+0x830/0x834 ??those are screen H/W (2f9968/2f995c). */

    /* v46: AC8 pulse + progress anim probes (never leave 0x45 / skip startup). */
    jjfb_ac8_pulse_tick(uc);
    jjfb_progress_probe_tick(uc);
    /* TEMPORARY: skip splash net/login wait (~2s). */
    jjfb_probe_skip_net_login_tick(uc);
    jjfb_log_startup_phase(uc, "timer");
    if ((jjfb_handler_tick % 30) == 0)
        jjfb_splash_cov_summary();

    if ((jjfb_handler_tick % 20) == 1) {
        jjfb_dump_robotol_state(uc, "pre-handler");
    }

    /* Splash/loading is selected when ER_RW+0x8D0 == 0x45 (handler@0x306344).
     * Old nudge wrote 1, which takes a different case and NEVER reaches 0x2EF86C
     * (slogo/loadingbar). First-screen bring-up must use 0x45.
     * Disable with JJFB_FORCE_SPLASH_NUDGE=0 or JJFB_FORCE_UI_MODE=0 (v44 Run A).
     * v54: JJFB_GWY_LAUNCHER_MODE defaults to natural (no FORCE). Explicit
     * FORCE_UI_MODE=45 (or other non-zero) still opts into the old probe. */
    if (!jjfb_forced_state && jjfb_handler_tick == 10 && jjfb_guest_ext_erw) {
        const char *nudge = getenv("JJFB_FORCE_SPLASH_NUDGE");
        const char *fui = getenv("JJFB_FORCE_UI_MODE");
        const char *gwy = getenv("JJFB_GWY_LAUNCHER_MODE");
        int skip_force = 0;
        int fui_off = (fui && fui[0] == '0' && (!fui[1] || fui[1] == '\0'));
        int nudge_off = (nudge && nudge[0] == '0' && (!nudge[1] || nudge[1] == '\0'));
        int fui_on = (fui && fui[0] && !fui_off);
        int gwy_on = (gwy && gwy[0] && gwy[0] != '0' &&
                      strcmp(gwy, "off") != 0 && strcmp(gwy, "false") != 0 &&
                      strcmp(gwy, "FALSE") != 0);
        /* FORCE_UI_MODE=0 / SPLASH_NUDGE=0 disables. */
        if (fui_off || nudge_off)
            skip_force = 1;
        /* GWY launcher mode: natural game-self unless FORCE is explicitly on. */
        if (gwy_on && !fui_on)
            skip_force = 1;
        if (jjfb_handler_tick == 10) {
            printf("[JJFB_V81_SCREEN] fixed SCREEN=%dx%d mythroad via JJFB_MYTHROAD_ROOT\n",
                   SCREEN_WIDTH, SCREEN_HEIGHT);
            fflush(stdout);
        }
        if (fui_on) {
            /* Prefer FORCE_UI_MODE as hex splash mode when not disabling. */
            skip_force = 0;
            if (!nudge || !nudge[0] || nudge_off)
                nudge = fui;
        }
        if (skip_force) {
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE, &st, 4);
            jjfb_seed_screen_geom(uc, "natural_mode");
            printf("[JJFB_GAME_SELF] natural_mode=1 gwy=%d no_force_ui_mode "
                   "state=0x%X tick=%u\n", gwy_on ? 1 : 0, st, jjfb_handler_tick);
            printf("[JJFB_FIRST_SCREEN] NO FORCE ui_mode (natural) state=0x%X tick=%u\n",
                   st, jjfb_handler_tick);
            fflush(stdout);
            jjfb_forced_state = 1; /* don't retry force */
            jjfb_install_ui_hooks(uc);
            jjfb_install_ac8_watch(uc);
            jjfb_install_uimode_writer_hooks(uc);
        } else {
        uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE, &st, 4);
        if (st == 0 || st == 1) {
            uint32_t mode = JJFB_UI_MODE_SPLASH;
            if (nudge && nudge[0])
                mode = (uint32_t)strtoul(nudge, NULL, 16); /* always hex: 45 => 0x45 */
            printf("[JJFB_FIRST_SCREEN] FORCE state/ui_mode 0x%X -> 0x%X "
                   "(splash path; slogo/loadingbar @0x2EF86C)\n", st, mode);
            fflush(stdout);
            uc_mem_write(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE, &mode, 4);
            jjfb_forced_state = 1;
            jjfb_forced_splash = 1;
            /* robotol 0x2fc8b8: set ER_RW+C44=1 so handler tail can bl 0x2e87ac.
             * Without this, app=24 runs but refresh gate stays closed (DRAW=0). */
            printf("[JJFB_SEND] call 0x2FC8B8 to enable refresh gate C44\n");
            fflush(stdout);
            jjfb_install_ui_hooks(uc);
            jjfb_run_guest_thumb(uc, 0x2FC8B9, 0, 0);
            /* 0x2f2a00 needs [7D8+0xC]>0 and [+0x10]!=[+0x4C].
             * 305c34 clips with 0x824/0x830 (=7D8+0x4C/0x58). Seeding +0x4C=0
             * made x+w > 0 always fail ??negative width ??never reached 0x11F00. */
            {
                uint32_t z = 0, w = SCREEN_WIDTH, h = SCREEN_HEIGHT;
                uint16_t one16 = 1;
                jjfb_seed_screen_geom(uc, "force_splash");
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x0C, &w, 4);
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x10, &h, 4);
                /* +0x4C must != +0x10; use screen width for clip too. */
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x4C, &w, 4);
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x50, &z, 4);
                /* X-center slot needs WIDTH; Y-base slot needs HEIGHT. */
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x58, &w, 4);
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D8 + 0x5C, &h, 4);
                uc_mem_write(uc, jjfb_guest_ext_erw + 0xD14, &one16, 2);
                /* Do NOT seed drawFP@1510 ??drawBitmap (ABI mismatch: bmp=0
                 * 240x0). Only observe natural writers/callers. */
                {
                    uint32_t cur_fp = 0;
                    uc_mem_read(uc, jjfb_guest_ext_erw + 0x1510, &cur_fp, 4);
                    printf("[JJFB_SEND] drawFP@1510 natural=0x%X (no seed)\n", cur_fp);
                }
                /* 0x2d92dc blx's ER_RW+0x1450 (strlen) / 0x144C (memset).
                 * Only seed if empty/garbage ??robotol may already have filled
                 * them (seen 0x94DF8/0xAC178). Overwriting can change ABI. */
                if (mr_table) {
                    uint32_t cur_memset = 0, cur_strlen = 0;
                    uint32_t memset_fp = 0, strlen_fp = 0;
                    uc_mem_read(uc, jjfb_guest_ext_erw + 0x144C, &cur_memset, 4);
                    uc_mem_read(uc, jjfb_guest_ext_erw + 0x1450, &cur_strlen, 4);
                    uc_mem_read(uc, toMrpMemAddr(mr_table) + 0x38, &memset_fp, 4);
                    uc_mem_read(uc, toMrpMemAddr(mr_table) + 0x3C, &strlen_fp, 4);
                    if ((cur_memset < 0x1000 || cur_memset == 0x270F) && memset_fp) {
                        uc_mem_write(uc, jjfb_guest_ext_erw + 0x144C, &memset_fp, 4);
                        cur_memset = memset_fp;
                    }
                    if ((cur_strlen < 0x1000 || cur_strlen == 0x270F) && strlen_fp) {
                        uc_mem_write(uc, jjfb_guest_ext_erw + 0x1450, &strlen_fp, 4);
                        cur_strlen = strlen_fp;
                    }
                    printf("[JJFB_SEND] FP 144C(memset)=0x%X 1450(strlen)=0x%X\n",
                           cur_memset, cur_strlen);
                }
                printf("[JJFB_SEND] seed draw/screen 7D8+0xC=%u +0x10=%u +0x4c=%u "
                       "+0x58h=%u +0x5cw=%u hwD14=1\n",
                       w, h, w, h, w);
                fflush(stdout);
            }
        }
        } /* else: force splash path */
    }

    /* 0x10140 registered an app tick/state handler during mrc_init. */
    if (jjfb_plat_handler_10140 && (jjfb_plat_handler_10140 & ~1u) > 0x1000) {
        uint32_t before_mode = 0, before_ac8 = 0, before_prog = 0;
        uint32_t after_mode = 0, after_ac8 = 0, after_prog = 0;
        uint32_t r0b = 0, r1b = 0, r2b = 0, r3b = 0, spb = 0;
        jjfb_10140_calls++;
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE_EARLY, &before_mode, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &before_ac8, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C, &before_prog, 4);
        }
        uc_reg_read(uc, UC_ARM_REG_R0, &r0b);
        uc_reg_read(uc, UC_ARM_REG_R1, &r1b);
        uc_reg_read(uc, UC_ARM_REG_R2, &r2b);
        uc_reg_read(uc, UC_ARM_REG_R3, &r3b);
        uc_reg_read(uc, UC_ARM_REG_SP, &spb);
        if (jjfb_10140_calls <= 8 || (jjfb_10140_calls % 20) == 1) {
            printf("[JJFB_TIMER_DISPATCH] tick=%u handler=0x%X calls=%u "
                   "before ui_mode=0x%X ac8=%d prog=%d r0-r3=0x%X,0x%X,0x%X,0x%X sp=0x%X\n",
                   jjfb_handler_tick, jjfb_plat_handler_10140, jjfb_10140_calls,
                   before_mode, (int32_t)before_ac8, (int32_t)before_prog,
                   r0b, r1b, r2b, r3b, spb);
            fflush(stdout);
        }
        /* r1=0 selects the periodic path inside the handler. */
        jjfb_run_guest_thumb(uc, jjfb_plat_handler_10140, 0, 0);
        if (jjfb_guest_ext_erw) {
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_STATE_EARLY, &after_mode, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_SPLASH_CNT_AC8, &after_ac8, 4);
            uc_mem_read(uc, jjfb_guest_ext_erw + JJFB_ERW_PROGRESS_BA0 + 0x2C, &after_prog, 4);
        }
        if (before_mode != after_mode || before_ac8 != after_ac8 || before_prog != after_prog ||
            jjfb_10140_calls <= 8 || (jjfb_10140_calls % 20) == 1) {
            printf("[JJFB_TIMER_DISPATCH] tick=%u after ui_mode=0x%X ac8=%d prog=%d "
                   "delta_mode=%d delta_ac8=%d delta_prog=%d\n",
                   jjfb_handler_tick, after_mode, (int32_t)after_ac8, (int32_t)after_prog,
                   (int)(after_mode - before_mode), (int)(after_ac8 - before_ac8),
                   (int)(after_prog - before_prog));
            fflush(stdout);
        }
    }

    if ((jjfb_handler_tick % 20) == 1) {
        jjfb_dump_robotol_state(uc, "post-handler");
    }

    /* v56 pure coverage: optionally suppress the historical synthetic event=0. */
    if (jjfb_handler_tick == 5 && !(getenv("JJFB_DISABLE_MRC_EVENT0_INJECT") &&
        getenv("JJFB_DISABLE_MRC_EVENT0_INJECT")[0] &&
        getenv("JJFB_DISABLE_MRC_EVENT0_INJECT")[0] != '0')) {
        printf("[JJFB_SEND] inject mrc_event(0,0,0) via helper code=1\n");
        fflush(stdout);
        (void)jjfb_robotol_inject_mrc_event(uc, 0, 0, 0);
    }

    /* v63: optional one-shot Path A via mrc_event (proven no-op for codes 1..5). */
    jjfb_v63_path_a_probe_tick(uc);
    /* v64: call registered 0x10165 enqueue handler once (real B54 producer). */
    jjfb_v64_enqueue_probe_tick(uc);
    /* v74: second enqueue with B58 record after leave_2FC26C. */
    jjfb_v74_b58_second_enqueue_tick(uc);
    /* v75: family C0 after B71 → 2FEBBC / B70. */
    jjfb_v75_family_c0_after_b71_tick(uc);

    /* v71: one present per timer tick after all guest draws (DispUpEx rarely used). */
    if (jjfb_present_coalesce) {
        if (jjfb_guest_ext_erw)
            uc_mem_read(uc, jjfb_guest_ext_erw + 0xB71, &jjfb_cached_b71, 1);
        jjfb_present_flush("timer");
    }
}

static void jjfb_dump_guest(uc_engine *uc, const char *tag, uint32_t addr, uint32_t n) {
    jjfb_dump_guest_ex(uc, tag, addr, n > 64 ? 64 : n);
}

static void jjfb_dump_guest_ex(uc_engine *uc, const char *tag, uint32_t addr, uint32_t n) {
    uint8_t buf[0x100];
    uint32_t i, len;
    int printable;
    (void)uc;
    if (!jjfb_verbose_logs())
        return;
    if (!addr || n == 0) {
        printf("[JJFB_SEND] dump %s addr=0x%X (null)\n", tag, addr);
        return;
    }
    len = n > sizeof(buf) ? (uint32_t)sizeof(buf) : n;
    if (!getMrpMemPtr(addr)) {
        printf("[JJFB_SEND] dump %s addr=0x%X (unmapped)\n", tag, addr);
        return;
    }
    memcpy(buf, getMrpMemPtr(addr), len);
    printf("[JJFB_SEND] dump %s @0x%X %u bytes:", tag, addr, len);
    for (i = 0; i < len; i++) {
        if ((i % 32) == 0) printf("\n ");
        printf(" %02X", buf[i]);
    }
    printf("\n");
    printable = 1;
    for (i = 0; i < len; i++) {
        if (buf[i] == 0) break;
        if (buf[i] < 0x20 || buf[i] > 0x7e) {
            printable = 0;
            break;
        }
    }
    if (printable && buf[0]) {
        printf("[JJFB_SEND] dump %s str=\"%.*s\"\n", tag, (int)len, (char *)buf);
    }
    fflush(stdout);
}

/* robotol mrc_init calls through P->mrc_extChunk->sendAppEvent (+0x28).
 * Mythroad 800 load never sets mrc_extChunk (only plugin mrc_extLoad does),
 * so we synthesize a minimal chunk + host hook.
 *
 * ABI (doc): sendAppEvent(extCode, app, code, param0, param1)
 *   R0..R3 + SP[0] = param1
 * Init also calls the same slot via plat-style wrapper with R0=0x101xx.
 */
static void br_jjfb_sendAppEvent(BridgeMap *o, uc_engine *uc) {
    uint32_t extCode, app, code, param0, param1 = 0;
    uint32_t sp;
    (void)o;
    uc_reg_read(uc, UC_ARM_REG_R0, &extCode);
    uc_reg_read(uc, UC_ARM_REG_R1, &app);
    uc_reg_read(uc, UC_ARM_REG_R2, &code);
    uc_reg_read(uc, UC_ARM_REG_R3, &param0);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_mem_read(uc, sp, &param1, 4);

    /* Throttle spammy plat lines. Quiet by default — log flood froze SDL. */
    {
        int spam = (extCode == 0x1E209 || extCode == 0x10138 || extCode == 0x10133 ||
                    extCode == 0x10132 || extCode == 0x2829DC || extCode == 0x12340 ||
                    extCode == 0x11F00);
        if (jjfb_verbose_logs()) {
            if (!spam || (jjfb_handler_tick % 20) == 1 ||
                (extCode == 0x1E209 && app != 9 && app != 0x18)) {
                printf("[JJFB_SEND] extCode=0x%X app=0x%X code=0x%X param0=0x%X param1=0x%X\n",
                       extCode, app, code, param0, param1);
                fflush(stdout);
            }
        } else if (!spam && (jjfb_handler_tick <= 3 || (jjfb_handler_tick % 30) == 1)) {
            printf("[JJFB_SEND] extCode=0x%X app=0x%X code=0x%X param0=0x%X param1=0x%X\n",
                   extCode, app, code, param0, param1);
            fflush(stdout);
        }
    }

    /* A) Classic timer start/stop: (0, chunk, 0|1, timerId, period) */
    if (extCode == 0 && (app == jjfb_ext_chunk_addr || app == 0)) {
        if (code == 0) {
            uint32_t period = param1 ? param1 : (param0 ? param0 : 50);
            if (period > 10000) period = 50;
            jjfb_ext_timer_id = param0;
            jjfb_ext_timer_period = period;
            jjfb_ext_timer_running = 1;
            if (jjfb_ext_chunk_addr) {
                uc_mem_write(uc, jjfb_ext_chunk_addr + 0x24, &jjfb_ext_timer_id, 4);
            }
            timerStart((uint16_t)period);
            printf("[JJFB_SEND] TIMER START id=0x%X period=%u -> RUNNING\n",
                   jjfb_ext_timer_id, period);
            fflush(stdout);
            SET_RET_V(MR_SUCCESS);
            return;
        }
        if (code == 1) {
            jjfb_ext_timer_running = 0;
            timerStop();
            printf("[JJFB_SEND] TIMER STOP id=0x%X\n", param0);
            fflush(stdout);
            SET_RET_V(MR_SUCCESS);
            return;
        }
    }

    /* B) Plat / alloc style (robotol wrapper 0x304550): R0=platCode, R1=size/arg */
    if (extCode == 0x10102 || extCode == 0x10162 || extCode == 0x10165) {
        uint32_t sz = app;
        /* 0x10102(app=0x1E200, code=handler) registers the 0x1E20x family dispatcher. */
        if (extCode == 0x10102 && code && (code & ~1u) > 0x1000) {
            jjfb_plat_handler_1e200 = code;
            printf("[JJFB_SEND] register 0x10102 family=0x%X handler=0x%X\n", app, code);
            fflush(stdout);
        }
        /* 0x10165(app=size, code=0x30D2F9): B54 enqueue entry (30D2F8→30D24C→2E4D6C).
         * v63 wrongly treated this as alloc-only and dropped the handler — queue stayed empty. */
        if (extCode == 0x10165 && code && (code & ~1u) > 0x1000) {
            jjfb_plat_handler_10165 = code;
            printf("[JJFB_V64_ENQ] register 0x10165 enqueue_handler=0x%X size=0x%X "
                   "(targets 30D24C→2E4D6C→B54→2DC80C→2E2520 Path A)\n",
                   code, sz);
            fflush(stdout);
        }
        if (extCode == 0x10162 && code && (code & ~1u) > 0x1000) {
            printf("[JJFB_V64_ENQ] note 0x10162 code=0x%X size=0x%X (sibling alloc)\n",
                   code, sz);
            fflush(stdout);
        }
        jjfb_dump_guest(uc, "plat_a2", code, 64);
        if (sz >= 0x100 && sz < 0x800000) {
            void *p = my_mallocExt(sz);
            if (p) {
                memset(p, 0, sz);
                uint32_t addr = toMrpMemAddr(p);
                if (extCode == 0x10165) jjfb_plat_buf_10165 = addr;
                if (extCode == 0x10162) jjfb_plat_buf_10162 = addr;
                printf("[JJFB_SEND] plat 0x%X alloc %u -> 0x%X\n", extCode, sz, addr);
                fflush(stdout);
                SET_RET_V(addr);
                return;
            }
            SET_RET_V(0);
            return;
        }
        SET_RET_V(1);
        return;
    }
    if (extCode == 0x10113) {
        /*
         * getProc: 2FD484 resolves 11F02/03/04 into ERW+0x1508/150C/1510.
         * Must WRITE fp to *param0 (r3 out); returning only a ret-stub left
         * *out=0x270F which then got stored into draw slots (no bmp blit).
         */
        uint32_t fp = 0;
        int mapped = 0;
        if (mr_table && (app == 0x11F02 || app == 0x11F03 || app == 0x11F04)) {
            uc_mem_read(uc, toMrpMemAddr(mr_table) + 0x1E0, &fp, 4);
            if (fp && getMrpMemPtr(fp & ~1u))
                mapped = 1;
        }
        if (mapped && param0 && getMrpMemPtr(param0)) {
            uc_mem_write(uc, param0, &fp, 4);
            printf("[JJFB_V69_10113] id=0x%X -> _DrawBitmap fp=0x%X out@0x%X "
                   "(ERW draw slots 1508/150C/1510)\n",
                   app, fp, param0);
            fflush(stdout);
            SET_RET_V(MR_SUCCESS);
            return;
        }
        /* Unknown id: Thumb stub that returns 1 (avoid blx to 0x1). */
        {
            uint32_t stub = jjfb_ensure_ret_stub(uc, &jjfb_ret1_stub_addr, 1);
            jjfb_dump_guest(uc, "10113_param0", param0, 64);
            printf("[JJFB_SEND] plat query 0x10113 id=0x%X -> stub@0x%X\n", app, stub | 1);
            fflush(stdout);
            SET_RET_V(stub ? (stub | 1) : 1);
            return;
        }
    }
    /* v70: 0x10180 — 2F65BC loads phone/user blob (app=1).
     * Unhandled path returned 1 → guest read [r4+0x20] at addr 0x21 (corrupt ERW).
     * ABI from 2F65BC: ret ptr; +0x20 word; +4/+0x3e 26B strings; +0x58 u16;
     * +0x5a/+0x5b u8; +0x5c 34B. */
    if (extCode == 0x10180) {
        static uint32_t blob_guest;
        const char *imei = getenv("JJFB_IMEI");
        const char *imsi = getenv("JJFB_IMSI");
        const char *hsman = getenv("JJFB_HSMAN");
        const char *hstype = getenv("JJFB_HSTYPE");
        uint32_t ver = 101000000;
        uint16_t u16z = 0;
        if (!imei || !imei[0]) imei = "864086040622841";
        if (!imsi || !imsi[0]) imsi = "460019707327302";
        if (!hsman || !hsman[0]) hsman = "vmrp";
        if (!hstype || !hstype[0]) hstype = "vmrp";
        if (!blob_guest) {
            void *p = my_mallocExt(0x100);
            if (p) {
                memset(p, 0, 0x100);
                blob_guest = toMrpMemAddr(p);
                strncpy((char *)p + 4, imei, 25);
                strncpy((char *)p + 0x3e, imsi, 25);
                memcpy((char *)p + 0x20, &ver, 4);
                memcpy((char *)p + 0x58, &u16z, 2);
                strncpy((char *)p + 0x5c, hsman, 7);
                strncpy((char *)p + 0x5c + 8, hstype, 7);
            }
        }
        printf("[JJFB_V70_10180] app=0x%X -> blob@0x%X (2F65BC userinfo; was unhandled ret=1)\n",
               app, blob_guest);
        fflush(stdout);
        SET_RET_V(blob_guest ? blob_guest : 0);
        return;
    }
    /* v70: 0x10130 — family app2 init (30CFE8) with app=0xAF000 on H<480.
     * Return ignored; previously excluded from alloc fallback and returned 1. */
    if (extCode == 0x10130) {
        if (app >= 0x1000 && app < 0x200000) {
            void *p = my_mallocExt(app);
            uint32_t addr = 0;
            if (p) {
                memset(p, 0, app);
                addr = toMrpMemAddr(p);
            }
            printf("[JJFB_V70_10130] alloc sz=0x%X -> 0x%X (ret ignored by 30CFEE)\n",
                   app, addr);
            fflush(stdout);
            SET_RET_V(addr ? addr : MR_SUCCESS);
            return;
        }
        printf("[JJFB_V70_10130] app=0x%X -> SUCCESS (notify)\n", app);
        fflush(stdout);
        SET_RET_V(MR_SUCCESS);
        return;
    }
    if (extCode == 0x10120) {
        /* (0x10120, type=4, chunk, ...) ??remember chunk binding */
        jjfb_plat_handler_10120 = code ? code : app;
        printf("[JJFB_SEND] register 0x10120 type=%u chunk/handler=0x%X\n", app, jjfb_plat_handler_10120);
        fflush(stdout);
        SET_RET_V(1);
        return;
    }
    if (extCode == 0x10140) {
        /* (0x10140, type=5, chunk, handler) ??app tick/state handler */
        uint32_t lr = 0;
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        jjfb_plat_handler_10140 = param0 ? param0 : code;
        jjfb_10140_handler_lr = lr;
        jjfb_dump_guest(uc, "10140_handler", jjfb_plat_handler_10140, 32);
        printf("[JJFB_10140_REG] app=0x%X code=0x%X param0=0x%X param1=0x%X "
               "handler=0x%X lr=0x%X\n",
               app, code, param0, param1, jjfb_plat_handler_10140, lr);
        printf("[JJFB_SEND] register 0x10140 type=%u chunk=0x%X handler=0x%X\n",
               app, code, jjfb_plat_handler_10140);
        fflush(stdout);
        SET_RET_V(1);
        return;
    }
    if (extCode == 0x10133) {
        /* free block start (user_ptr - 4). Companion to 0x10132. */
        if (app && getMrpMemPtr(app)) {
            my_freeExt(getMrpMemPtr(app));
        }
        if ((jjfb_handler_tick % 40) == 1) {
            printf("[JJFB_SEND] plat 0x10133 free block=0x%X\n", app);
            fflush(stdout);
        }
        SET_RET_V(MR_SUCCESS);
        return;
    }
    if (extCode == 0x10132) {
        /* Guest malloc: size already includes +4 header. Return block base;
         * caller does return block+4 as user pointer (robotol 0x2d99a4).
         * v65: 2E4D6C allocs tiny payloads (sz=2 observed) before 312A60 push;
         * rejecting sz<4 made alloc FAIL → memcpy storm → UNMAP mid-enqueue. */
        uint32_t sz = app;
        if (sz >= 1 && sz < 0x800000) {
            void *p = my_mallocExt(sz);
            if (p) {
                memset(p, 0, sz);
                /* Store payload size in header word when there is room. */
                if (sz >= 4)
                    *(uint32_t *)p = sz - 4;
                uint32_t addr = toMrpMemAddr(p);
                static uint32_t jjfb_10132_n;
                jjfb_10132_n++;
                if (jjfb_10132_n <= 6 || (jjfb_10132_n % 100) == 0 || sz > 64 || sz < 4) {
                    printf("[JJFB_SEND] plat 0x10132 alloc #%u sz=%u -> block 0x%X\n",
                           jjfb_10132_n, sz, addr);
                    fflush(stdout);
                }
                SET_RET_V(addr);
                return;
            }
        }
        printf("[JJFB_SEND] plat 0x10132 alloc FAIL sz=%u\n", sz);
        fflush(stdout);
        SET_RET_V(0);
        return;
    }
    if (extCode == 0x10138) {
        /* Two call sites share this opcode (via wrapper 0x304550):
         *  - ret 0x2d9a6a: heap query ??large free + clear ED8 ??0x10132
         *  - ret 0x30d010: screen/metrics ??width/height outs
         * Wrapper saved LR sits at SP+0x24 on entry to sendAppEvent.
         */
        uint32_t out4 = 0, out5 = 0, site_lr = 0;
        uint32_t v0 = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0, v5 = 0;
        uc_mem_read(uc, sp + 0x24, &site_lr, 4);
        uc_mem_read(uc, sp + 4, &out4, 4);
        uc_mem_read(uc, sp + 8, &out5, 4);

        if ((site_lr & ~1u) == 0x30D010) {
            v0 = SCREEN_WIDTH;
            v5 = SCREEN_HEIGHT;
            static int jjfb_10138_metrics_n;
            if (jjfb_10138_metrics_n++ < 4) {
                printf("[JJFB_SEND] plat 0x10138 metrics %ux%u (site=0x%X)\n",
                       SCREEN_WIDTH, SCREEN_HEIGHT, site_lr);
            }
        } else {
            v5 = 0x200000;
            if (jjfb_guest_ext_erw) {
                uint32_t z = 0;
                uint8_t one = 1;
                /* robotol 0x2d99a4:
                 *  - @0x7DC entry gate for direct 0x10132
                 *  - @0x7DD post-10138 gate (0x2d9a70)
                 *  - @0x7D9 / @0x11 alternate gates
                 *  - @0xED8 capacity must be <=0 after query
                 */
                uc_mem_write(uc, jjfb_guest_ext_erw + 0xED8, &z, 4);
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x7DC, &one, 1);
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x7DD, &one, 1);
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x7D9, &one, 1);
                uc_mem_write(uc, jjfb_guest_ext_erw + 0x11, &one, 1);
            }
            static uint32_t jjfb_10138_heap_n;
            jjfb_10138_heap_n++;
            if (jjfb_10138_heap_n <= 3 || (jjfb_10138_heap_n % 200) == 0) {
                printf("[JJFB_SEND] plat 0x10138 heap #%u free=%u (site=0x%X out5=0x%X)\n",
                       jjfb_10138_heap_n, v5, site_lr, out5);
            }
        }
        fflush(stdout);
        if (app) uc_mem_write(uc, app, &v0, 4);
        if (code) uc_mem_write(uc, code, &v1, 4);
        if (param0) uc_mem_write(uc, param0, &v2, 4);
        if (param1) uc_mem_write(uc, param1, &v3, 4);
        if (out4) uc_mem_write(uc, out4, &v4, 4);
        if (out5) uc_mem_write(uc, out5, &v5, 4);
        /* One-shot readback: prove ED8/flags/out5 actually stuck. */
        if ((site_lr & ~1u) == 0x2D9A6A && jjfb_guest_ext_erw) {
            static int jjfb_10138_dbg;
            if (jjfb_10138_dbg < 3) {
                uint32_t ed8 = 0, got = 0, r9 = 0;
                uint8_t f7dc = 0, f7dd = 0, f7d9 = 0, f11 = 0;
                uc_reg_read(uc, UC_ARM_REG_R9, &r9);
                uc_mem_read(uc, jjfb_guest_ext_erw + 0xED8, &ed8, 4);
                uc_mem_read(uc, jjfb_guest_ext_erw + 0x7DC, &f7dc, 1);
                uc_mem_read(uc, jjfb_guest_ext_erw + 0x7DD, &f7dd, 1);
                uc_mem_read(uc, jjfb_guest_ext_erw + 0x7D9, &f7d9, 1);
                uc_mem_read(uc, jjfb_guest_ext_erw + 0x11, &f11, 1);
                if (out5) uc_mem_read(uc, out5, &got, 4);
                printf("[JJFB_SEND] 10138 dbg erw=0x%X r9=0x%X ED8=%u 7DC=%u 7DD=%u 7D9=%u @11=%u *out5=%u\n",
                       jjfb_guest_ext_erw, r9, ed8, f7dc, f7dd, f7d9, f11, got);
                fflush(stdout);
                jjfb_10138_dbg++;
            }
        }
        SET_RET_V(MR_SUCCESS);
        return;
    }
    /* v65/v66: 0x101AB — buffer fill for 30D24C → unpack ">c" ">i" → 2E4D6C(B54).
     * ABI from 30D24C: (buf=app, out=code, type=param0=2, param1=0).
     * 31103C is unpack:
     *   ret==0 → unpack starts at buf[0]
     *   buf[0]=char, buf[1..4]=BE length → r1 of 2E4D6C
     *   buf[5..] = payload; 2E4D6C parses from buf+5.
     *
     * 2E4D6C when ERW+15C==1:
     *   hdr_u32 @+0 → A90+4; size_u32 @+4; u16 @+8 → node[0] (event code);
     *   body[size-2] memcpy'd; B5C=body for 2E4040→2F68E4.
     *
     * v66: 2F68E4 does `308D98; ADDS r3,r0,#1; BEQ leave` — exits only when
     * the BE u32 is 0xFFFFFFFF (-1), NOT 0. A leading 0 still enters the
     * record path and can spin. Body must be BE 0xFFFFFFFF end-marker.
     */
    if (extCode == 0x101AB) {
        /* mode0: body=BE(-1) only → empty B58 → 2FC26C.
         * mode1 (v74): one record + BE(-1) → nonempty B58 → 30ED2C. */
        uint32_t body_size;
        uint32_t payload_len;
        uint32_t hdr_u32 = 5; /* first-parse → A90+4 */
        uint8_t hdr[5];
        uint8_t payload[128];
        uint32_t o;
        const char *rec_name = "downVersion";
        uint32_t name_len;
        int with_rec = jjfb_101ab_fill_records;

        if (with_rec) {
            /* tag(4) + str1(2+n) + str2(2+0) + u32(4) + u32(4) + (-1)(4) */
            name_len = (uint32_t)strlen(rec_name);
            body_size = 2 + (4 + 2 + name_len + 2 + 0 + 4 + 4 + 4);
        } else {
            body_size = 6; /* u16 + BE(-1) */
        }
        payload_len = 4 + 4 + 2 + (body_size - 2);
        if (payload_len > sizeof(payload))
            payload_len = (uint32_t)sizeof(payload);
        memset(payload, 0, sizeof(payload));
        hdr[0] = (uint8_t)(param0 & 0xff);
        hdr[1] = (uint8_t)((payload_len >> 24) & 0xff);
        hdr[2] = (uint8_t)((payload_len >> 16) & 0xff);
        hdr[3] = (uint8_t)((payload_len >> 8) & 0xff);
        hdr[4] = (uint8_t)(payload_len & 0xff);

        o = 0;
        payload[o++] = (uint8_t)((hdr_u32 >> 24) & 0xff);
        payload[o++] = (uint8_t)((hdr_u32 >> 16) & 0xff);
        payload[o++] = (uint8_t)((hdr_u32 >> 8) & 0xff);
        payload[o++] = (uint8_t)(hdr_u32 & 0xff);
        payload[o++] = (uint8_t)((body_size >> 24) & 0xff);
        payload[o++] = (uint8_t)((body_size >> 16) & 0xff);
        payload[o++] = (uint8_t)((body_size >> 8) & 0xff);
        payload[o++] = (uint8_t)(body_size & 0xff);
        payload[o++] = 0; /* BE u16 hi */
        payload[o++] = 5; /* BE u16 lo = 5 → node[0] → 2E2520 Path A */
        if (with_rec) {
            /* 2F68E4 record: tag!=-1, len-prefixed str1/str2, two u32s, then -1 */
            payload[o++] = 0;
            payload[o++] = 0;
            payload[o++] = 0;
            payload[o++] = 1; /* tag = 1 (not -1) */
            payload[o++] = (uint8_t)((name_len >> 8) & 0xff);
            payload[o++] = (uint8_t)(name_len & 0xff);
            memcpy(payload + o, rec_name, name_len);
            o += name_len;
            payload[o++] = 0; /* str2 len hi */
            payload[o++] = 0; /* str2 len lo = 0 → NULL */
            /* field C @node+8 / field D @node+0xc — 30ED2C compares 2F6C44==field D.
             * With downVersion.v present, 2F6C44 returns BE version 0x3EE. */
            payload[o++] = 0;
            payload[o++] = 0;
            payload[o++] = 0;
            payload[o++] = 0;
            payload[o++] = 0;
            payload[o++] = 0;
            payload[o++] = 0x03;
            payload[o++] = 0xEE;
        }
        /* BE u32 0xFFFFFFFF — 2F68E4 end marker */
        payload[o++] = 0xFF;
        payload[o++] = 0xFF;
        payload[o++] = 0xFF;
        payload[o++] = 0xFF;

        if (app)
            uc_mem_write(uc, app, hdr, 5);
        if (app)
            uc_mem_write(uc, app + 5, payload, payload_len);
        if (code)
            uc_mem_write(uc, code, &payload_len, 4);

        if (with_rec) {
            printf("[JJFB_V74_101AB] fill buf=0x%X out=0x%X type=%u "
                   "payload_len=%u body_sz=%u record=\"%s\"+BE(-1) "
                   "(2F68E4 → nonempty B58 → 30ED2C)\n",
                   app, code, param0, payload_len, body_size, rec_name);
        } else {
            printf("[JJFB_V66_101AB] fill buf=0x%X out=0x%X type=%u "
                   "payload_len=%u hdr_be=%u body_sz=%u u16=5 body=BE(-1) "
                   "(2F68E4 terminate → 2E4066 → 2DADC4)\n",
                   app, code, param0, payload_len, hdr_u32, body_size);
        }
        fflush(stdout);
        SET_RET_V(0);
        return;
    }
    if (extCode == 0x10800) {
        printf("[JJFB_SEND] plat 0x10800 arg=0x%X\n", app);
        fflush(stdout);
        SET_RET_V(1);
        return;
    }
    /* 0x12340: per-glyph text/measure notify from 0x305e70 via 0x304550.
     * Ret ignored by caller; outs are pointer side-effects:
     *   param1 = SP[0] = &glyph_h (or metric out)
     *   SP[4]  = &cursor/width slot (guest does *slot += 2 after return)
     * app=1 observed on FPS/UI path. */
    if (extCode == 0x12340) {
        static uint32_t n12340;
        uint32_t param2 = 0, lr = 0;
        uint32_t out_h = 16, out_w = 8;
        uint32_t before1 = 0, before2 = 0;
        n12340++;
        uc_mem_read(uc, sp + 4, &param2, 4);
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        /* 304550 saved LR (return into 0x305e70) sits at SP+0x24 on entry. */
        {
            uint32_t site = 0;
            uc_mem_read(uc, sp + 0x24, &site, 4);
            if (n12340 <= 8 || (n12340 % 100) == 0)
                printf("[JJFB_12340] site_lr(sp+24)=0x%X wrapper_lr=0x%X\n", site, lr);
        }
        if (param1 && getMrpMemPtr(param1))
            uc_mem_read(uc, param1, &before1, 4);
        if (param2 && getMrpMemPtr(param2))
            uc_mem_read(uc, param2, &before2, 4);
        /* Measure-ish: write font metrics into out pointers when empty.
         * Height goes to param1 (used as r7 after 305e70 in 305c34). */
        if (param1 && getMrpMemPtr(param1) && before1 == 0)
            uc_mem_write(uc, param1, &out_h, 4);
        if (n12340 <= 8 || (n12340 % 100) == 0) {
            printf("[JJFB_12340] n=%u app=0x%X code=0x%X p0=0x%X p1=0x%X p2=0x%X "
                   "lr=0x%X before1=%u before2=%u write_h=%u\n",
                   n12340, app, code, param0, param1, param2, lr,
                   before1, before2, (before1 == 0) ? out_h : 0);
            if (code) jjfb_dump_guest(uc, "12340_code", code, 64);
            if (param1) jjfb_dump_guest(uc, "12340_p1", param1, 32);
            fflush(stdout);
        }
        (void)out_w;
        SET_RET_V(MR_SUCCESS);
        return;
    }
    /* 0x11F00: drawText/blit from 0x2f2358 (app=7, code=obj, p0=y/x pack).
     * Real screen-buffer side effect; DEBUG present only when dirty. */
    if (extCode == 0x11F00) {
        uint32_t param2 = 0, param3 = 0;
        uc_mem_read(uc, sp + 4, &param2, 4);
        uc_mem_read(uc, sp + 8, &param3, 4);
        jjfb_plat_11f00(uc, app, code, param0, param1, param2, param3);
        SET_RET_V(MR_SUCCESS);
        return;
    }
    /* 0x10134: RGB565 bitmap buffer construct (app == W*H*2). */
    if (extCode == 0x10134) {
        static uint32_t n10134;
        JjfbBmpReq *req = NULL;
        n10134++;
        if (jjfb_bmp_last.valid && jjfb_bmp_last.bytes == (int)app)
            req = &jjfb_bmp_last;
        if (!req)
            req = jjfb_bmp_cache_find_bytes((int)app);
        /* Shell: resolve name by (1) pending BMP_REQ / 2d92dc, (2) last opened
         * MRP member with matching W*H*2, (3) legacy chrome sizes in jjfb.mrp. */
        if (!req) {
            char found[128];
            const char *known = NULL;
            int from_pack = 0;
            found[0] = 0;
            if (jjfb_last_mrp_host[0] &&
                jjfb_mrp_find_name_by_bytes(jjfb_last_mrp_host, app, found,
                                            (int)sizeof(found))) {
                known = found;
                from_pack = 1;
                printf("[JJFB_V78_BMP] 10134 pack-scan app=0x%X pack=\"%s\" -> %s\n",
                       app, jjfb_last_mrp_guest[0] ? jjfb_last_mrp_guest
                                                   : jjfb_last_mrp_host,
                       known);
            }
            if (!known)
                known = jjfb_bmp_name_by_bytes(app);
            if (known) {
                jjfb_bmp_req_set(known, 0, 0);
                if (jjfb_bmp_last.valid && jjfb_bmp_last.bytes == (int)app)
                    req = &jjfb_bmp_last;
                if (!from_pack) {
                    printf("[JJFB_V76_BMP] 10134 size-map app=0x%X -> %s\n",
                           app, known);
                }
                fflush(stdout);
            }
        }
        if (req && req->bytes == (int)app && app >= 16 && app < 0x200000) {
            void *p = my_mallocExt(app);
            if (p) {
                int loaded = jjfb_mrp_load_rgb565(req->name, (int)app, (uint8_t *)p, (int)app);
                if (!loaded)
                    jjfb_bmp_fill_placeholder((uint16_t *)p, req->w, req->h, req->name);
                req->guest_pixels = toMrpMemAddr(p);
                if (&jjfb_bmp_last != req && jjfb_bmp_last.valid &&
                    jjfb_bmp_last.bytes == req->bytes) {
                    jjfb_bmp_last.guest_pixels = req->guest_pixels;
                    strncpy(jjfb_bmp_last.name, req->name, sizeof(jjfb_bmp_last.name) - 1);
                    jjfb_bmp_last.w = req->w;
                    jjfb_bmp_last.h = req->h;
                }
                /* Keep cache slot in sync when req was jjfb_bmp_last */
                if (req == &jjfb_bmp_last) {
                    JjfbBmpReq *slot = jjfb_bmp_cache_find_bytes((int)app);
                    if (slot)
                        slot->guest_pixels = req->guest_pixels;
                }
                printf("[JJFB_10134_BMP] #%u name=%s w=%d h=%d bytes=0x%X "
                       "guest_pixels=0x%X loaded=%d\n",
                       n10134, req->name, req->w, req->h, app,
                       req->guest_pixels, loaded);
                fflush(stdout);
                if (jjfb_is_firstscreen_name(req->name)) {
                    jjfb_note_firstscreen("10134_bmp", req->name);
                    jjfb_watch_pixels(uc, req->guest_pixels, req->name);
                    jjfb_install_ptr_store_watch(uc);
                }
                /* v77: always return pixels (guest 2D9580 wraps object itself).
                 * Force: JJFB_10134_RET=object|pixels|zero */
                {
                    const char *rm = getenv("JJFB_10134_RET");
                    uint32_t retv = req->guest_pixels;
                    uint32_t site_lr = 0;
                    int want_obj = 0;
                    uc_reg_read(uc, UC_ARM_REG_LR, &site_lr);
                    printf("[JJFB_10134_SITE] lr=0x%X code=0x%X p0=0x%X p1=0x%X\n",
                           site_lr, code, param0, param1);
                    fflush(stdout);
                    if (rm && (strcmp(rm, "object") == 0 || strcmp(rm, "obj") == 0))
                        want_obj = 1;
                    if (rm && (strcmp(rm, "pixels") == 0 || strcmp(rm, "pixel") == 0))
                        want_obj = 0;
                    if (rm && strcmp(rm, "zero") == 0) {
                        retv = 0;
                        want_obj = 0;
                        printf("[JJFB_10134_RET] mode=zero (pixels kept @0x%X)\n",
                               req->guest_pixels);
                    }
                    if (want_obj) {
                        uint32_t obj = jjfb_bmp_make_object(
                            req->guest_pixels, req->w, req->h, req->bytes);
                        if (obj) {
                            req->guest_object = obj;
                            {
                                JjfbBmpReq *slot = jjfb_bmp_cache_find_bytes((int)app);
                                if (slot) slot->guest_object = obj;
                            }
                            retv = obj;
                            printf("[JJFB_10134_RET] mode=object obj=0x%X pixels=0x%X "
                                   "w=%d h=%d\n",
                                   obj, req->guest_pixels, req->w, req->h);
                        } else {
                            printf("[JJFB_10134_RET] mode=pixels(fallback) r0=0x%X\n",
                                   retv);
                        }
                    } else if (!rm || strcmp(rm, "zero") != 0) {
                        printf("[JJFB_10134_RET] mode=pixels r0=0x%X\n", retv);
                    }
                    fflush(stdout);
                    /* Eager blit: default OFF in v38 unless explicitly enabled. */
                    {
                        const char *eb = getenv("JJFB_10134_EAGER_BLIT");
                        int eager = (eb && eb[0] == '1');
                        if (eager && jjfb_is_firstscreen_name(req->name)) {
                            int dx = (SCREEN_WIDTH - req->w) / 2;
                            int dy = 180;
                            if (strstr(req->name, "loadingbar")) dy = 200;
                            else if (strstr(req->name, "textbar")) dy = 240;
                            else if (strstr(req->name, "bar!")) dy = 210;
                            else if (strstr(req->name, "slogo")) dy = 80;
                            if (dx < 0) dx = 0;
                            if (dy < 0) dy = 0;
                            jjfb_screen_blit_rgb565((uint16_t *)p, req->w, req->h, dx, dy);
                            printf("[JJFB_10134_EAGER_BLIT] name=%s x=%d y=%d w=%d h=%d\n",
                                   req->name, dx, dy, req->w, req->h);
                            fflush(stdout);
                            jjfb_debug_present_dirty("10134_EAGER");
                        }
                    }
                    SET_RET_V(retv);
                    return;
                }
            }
        }
        if (n10134 <= 24 || (n10134 % 100) == 0) {
            printf("[JJFB_10134] #%u no-match app=0x%X code=0x%X p0=0x%X p1=0x%X "
                   "last_bytes=0x%X last_name=%s\n",
                   n10134, app, code, param0, param1,
                   jjfb_bmp_last.valid ? (uint32_t)jjfb_bmp_last.bytes : 0,
                   jjfb_bmp_last.valid ? jjfb_bmp_last.name : "(none)");
            fflush(stdout);
        }
        /* Size-only fallback: still return a real buffer, not empty success. */
        if (app >= 16 && app < 0x100000) {
            void *p = my_mallocExt(app);
            if (p) {
                memset(p, 0, app);
                SET_RET_V(toMrpMemAddr(p));
                return;
            }
        }
        SET_RET_V(0);
        return;
    }
    /* Seen from 0x10140 handler: plat(0x1E209, app, ...).
     * 0x10102 registered family dispatcher @0x30D2F9 (switch on app).
     * Defer call to avoid nested uc_emu_start while inside 0x10140. */
    if (extCode == 0x1E209 || (extCode >= 0x1E200 && extCode <= 0x1E2FF)) {
        if ((jjfb_handler_tick % 20) == 1 || (app != 9 && app != 0x18)) {
            printf("[JJFB_SEND] plat UI/ops 0x%X arg=%u code=0x%X p0=0x%X p1=0x%X (fam=0x%X)\n",
                   extCode, app, code, param0, param1, jjfb_plat_handler_1e200);
            fflush(stdout);
        }
        if (jjfb_plat_handler_1e200) {
            jjfb_pend_app = app;
            jjfb_pend_code = code;
            jjfb_pend_p0 = param0;
            jjfb_pend_p1 = param1;
            jjfb_pending_1e200 = 1;
            if (jjfb_in_guest_handler) {
                /* v61: never nest uc_emu_start while already inside helper/timer.
                 * v60 hung on first code=2 when 2F5404 sent app=7 and flushed inline. */
                if (app != 9 && app != 0x18) {
                    printf("[JJFB_V61_NEST] defer 1E209 app=%u code=0x%X "
                           "(avoid nested emu during guest)\n",
                           app, code);
                    fflush(stdout);
                }
            } else {
                jjfb_flush_1e200(uc);
            }
        }
        SET_RET_V(MR_SUCCESS);
        return;
    }
    /* First init probe sometimes: (1, 100, ...) */
    if (extCode == 1) {
        printf("[JJFB_SEND] plat/probe code=1 arg=%u\n", app);
        fflush(stdout);
        SET_RET_V(MR_SUCCESS);
        return;
    }

    /* v72: 0x10110(app=0xE201/E209/...) — family notify/unreg, NOT alloc.
     * app is the low 16 bits of 0x1E20x / similar opcodes. Falling through to
     * malloc(0xE201) OOMs ("my_malloc no memory") then UC_ERR_EXCEPTION. */
    if (extCode == 0x10110) {
        static uint32_t n_10110;
        n_10110++;
        if (n_10110 <= 8 || (n_10110 % 40) == 0) {
            printf("[JJFB_V72_10110] #%u app=0x%X code=0x%X -> SUCCESS "
                   "(family notify/unreg; never alloc)\n",
                   n_10110, app, code);
            fflush(stdout);
        }
        SET_RET_V(MR_SUCCESS);
        return;
    }

    /* High-frequency guest callback used as plat code — ack quietly.
     * (0x12340 has a real handler above; do not short-circuit it here.) */
    if (extCode == 0x2829DC) {
        SET_RET_V(MR_SUCCESS);
        return;
    }

    {
        static uint32_t n_unh;
        n_unh++;
        if (jjfb_verbose_logs() || n_unh <= 12 || (n_unh % 80) == 0) {
            printf("[JJFB_SEND] unhandled plat/msg 0x%X app=0x%X code=0x%X p0=0x%X p1=0x%X\n",
                   extCode, app, code, param0, param1);
            fflush(stdout);
        }
    }
    /* Only treat as alloc when app looks like a size, not an opcode fragment.
     * Reject 0xExxx/0xCxxx-style family ids that used to OOM via 0x10110. */
    if (app >= 0x100 && app < 0x10000 && extCode >= 0x10000 &&
        !(extCode >= 0x10110 && extCode <= 0x1011F) &&
        !(extCode >= 0x10130 && extCode <= 0x1013F) &&
        (app & 0xF000) < 0xC000) {
        void *p = my_mallocExt(app);
        if (p) {
            memset(p, 0, app);
            SET_RET_V(toMrpMemAddr(p));
            return;
        }
        SET_RET_V(0);
        return;
    }

    SET_RET_V(1);
}

static int jjfb_ensure_ext_chunk(uc_engine *uc, uint32_t Paddr) {
    uint32_t chunk_ptr = 0;
    uc_mem_read(uc, Paddr + 12, &chunk_ptr, 4);
    if (chunk_ptr) {
        printf("[JJFB_801] mrc_extChunk already set @0x%X\n", chunk_ptr);
        return 1;
    }

    if (!jjfb_send_hook_addr) {
        void *slot = my_mallocExt(4);
        if (!slot) return 0;
        jjfb_send_hook_addr = toMrpMemAddr(slot);
        uc_mem_write(uc, jjfb_send_hook_addr, &jjfb_send_hook_addr, 4);

        BridgeMap *obj = (BridgeMap *)malloc(sizeof(BridgeMap));
        uIntMap *mobj = (uIntMap *)malloc(sizeof(uIntMap));
        if (!obj || !mobj) return 0;
        obj->pos = 0;
        obj->type = MAP_FUNC;
        obj->name = "jjfb_sendAppEvent";
        obj->initFn = NULL;
        obj->fn = br_jjfb_sendAppEvent;
        obj->extraData = 0;
        mobj->key = jjfb_send_hook_addr;
        mobj->data = obj;
        if (uIntMap_insert(&root, mobj)) {
            printf("[JJFB_801] sendAppEvent hook insert failed\n");
            return 0;
        }
        {
            uc_err err;
            uc_hook trace;
            err = uc_hook_add(uc, &trace, UC_HOOK_CODE, hook_code, NULL,
                              jjfb_send_hook_addr, jjfb_send_hook_addr + 4, 0);
            if (err != UC_ERR_OK) {
                printf("[JJFB_801] sendAppEvent uc_hook_add err %u\n", err);
                return 0;
            }
        }
        printf("[JJFB_801] installed sendAppEvent hook @0x%X\n", jjfb_send_hook_addr);
    }

    /* mrc_extChunk_st ??at least through sendAppEvent @0x28 and extMrTable @0x2c */
    enum { CHUNK_SIZE = 0x40 };
    void *chunk = my_mallocExt(CHUNK_SIZE);
    if (!chunk) return 0;
    memset(chunk, 0, CHUNK_SIZE);
    jjfb_ext_chunk_addr = toMrpMemAddr(chunk);

    uint32_t check = 0x7FD854EB;
    uint32_t init_func = jjfb_guest_ext_base ? (jjfb_guest_ext_base + 8) : 0;
    uint32_t event = jjfb_guest_ext_helper;
    uint32_t code_buf = jjfb_guest_ext_base;
    int32_t code_len = 253420;
    uint32_t var_buf = jjfb_guest_ext_erw;
    int32_t var_len = 5404;
    uint32_t global_p = Paddr;
    int32_t global_p_len = 20;
    uint32_t send = jjfb_send_hook_addr;
    uint32_t ext_table = 0;
    /* Keep mythroad shadow table pointer for extMrTable / GOT ??it has working
     * c2u/sprintf/etc. Only heal graphics FPs inside it (see jjfb_heal_shadow_gfx). */
    if (jjfb_guest_ext_base)
        uc_mem_read(uc, jjfb_guest_ext_base, &ext_table, 4);

    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x00, &check, 4);
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x04, &init_func, 4);
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x08, &event, 4);
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x0c, &code_buf, 4);
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x10, &code_len, 4);
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x14, &var_buf, 4);
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x18, &var_len, 4);
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x1c, &global_p, 4);
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x20, &global_p_len, 4);
    /* 0x24 timer left 0 */
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x28, &send, 4);
    uc_mem_write(uc, jjfb_ext_chunk_addr + 0x2c, &ext_table, 4);

    jjfb_heal_shadow_gfx(uc, "synth_chunk");

    uc_mem_write(uc, Paddr + 12, &jjfb_ext_chunk_addr, 4);
    printf("[JJFB_801] synthesized mrc_extChunk @0x%X -> P+0xc (send=@0x%X extMrTable=0x%X)\n",
           jjfb_ext_chunk_addr, send, ext_table);
    fflush(stdout);
    return 1;
}

/* Guest DSM keeps its own mr_c_function_P; host only sees the outer DSM.
 * Recover robotol addresses from the guest printf line already in our log file. */
static int jjfb_sync_guest_ext_from_log(void) {
    fflush(stdout);
    FILE *fp = fopen("jjfb_loader_stdout.txt", "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    long sz = ftell(fp);
    if (sz <= 0) {
        fclose(fp);
        return 0;
    }
    long back = sz > 256 * 1024 ? 256 * 1024 : sz;
    if (fseek(fp, -back, SEEK_END) != 0) {
        fseek(fp, 0, SEEK_SET);
        back = sz;
    }
    char *buf = (char *)malloc((size_t)back + 1);
    if (!buf) {
        fclose(fp);
        return 0;
    }
    size_t n = fread(buf, 1, (size_t)back, fp);
    buf[n] = 0;
    fclose(fp);

    uint32_t helper = 0, P = 0, erw = 0;
    char *p = buf;
    char *last_new = NULL;
    while ((p = strstr(p, "_mr_c_function_new(")) != NULL) {
        last_new = p;
        p += 18;
    }
    if (last_new && sscanf(last_new, "_mr_c_function_new(%x, %*d)  mr_c_function_P:%x", &helper, &P) == 2) {
        char *er = strstr(last_new, "start_of_ER_RW = @");
        if (er) {
            sscanf(er, "start_of_ER_RW = @%x", &erw);
        }
        /* find last "--- ext: @" before this _mr_c_function_new */
        char *extmark = NULL;
        char *q = buf;
        while ((q = strstr(q, "--- ext: @")) != NULL) {
            if (q < last_new) extmark = q;
            q += 10;
        }
        uint32_t extbase = 0;
        if (extmark) {
            sscanf(extmark, "--- ext: @%x", &extbase);
        }
        if (helper && P) {
            jjfb_guest_ext_helper = helper;
            jjfb_guest_ext_P = P;
            jjfb_guest_ext_erw = erw;
            jjfb_guest_ext_base = extbase;
            printf("[JJFB_801] synced guest EXT helper=0x%X P=0x%X ER_RW=0x%X ext=@%X\n",
                   helper, P, erw, extbase);
            fflush(stdout);
            free(buf);
            return 1;
        }
    }
    free(buf);
    return 0;
}

int32_t bridge_dsm_ext_call(uc_engine *uc, int32_t code, const void *input, int32_t input_len) {
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }

    if (!jjfb_guest_ext_helper || !jjfb_guest_ext_P) {
        jjfb_sync_guest_ext_from_log();
    }

    uint32_t helper = jjfb_guest_ext_helper ? jjfb_guest_ext_helper : mr_extHelper_addr;
    uint32_t Paddr = jjfb_guest_ext_P ? jjfb_guest_ext_P : toMrpMemAddr(mr_c_function_P);
    uint32_t erw = jjfb_guest_ext_erw;

    if (!helper || !Paddr) {
        printf("[JJFB_801] ext_call code=%d skipped (no EXT helper/P)\n", code);
        fflush(stdout);
        pthread_mutex_unlock(&mutex);
        return MR_FAILED;
    }
    if (jjfb_guest_ext_erw)
        jjfb_seed_screen_geom(uc, "ext_call");

    /* Install writer coverage before mrc_init so early BL 2DADC4 is visible. */
    if (code == 0) {
        jjfb_install_uimode_writer_hooks(uc);
        jjfb_install_v62_flag_mem_hooks(uc);
    }

    uint32_t in_addr = 0;
    void *in_mem = NULL;
    if (input && input_len > 0) {
        in_mem = my_mallocExt((uint32_t)input_len);
        if (!in_mem) {
            pthread_mutex_unlock(&mutex);
            return MR_FAILED;
        }
        memcpy(in_mem, input, (size_t)input_len);
        in_addr = toMrpMemAddr(in_mem);
    } else if (!input && jjfb_guest_ext_base && (code == 0 || code == 6)) {
        /* C mr_doExt: filebuf + MR_VERSION for both code 6 and code 0 */
        in_addr = jjfb_guest_ext_base;
    }

    /* Dump mr_c_function_st so we can see if mrc_extChunk / ER_RW look sane. */
    static unsigned jjfb_ext_call_tick2;
    int quiet = (code == 2);
    if (!quiet) {
        uint32_t p0 = 0, p4 = 0, p8 = 0, p12 = 0, p16 = 0;
        uc_mem_read(uc, Paddr, &p0, 4);
        uc_mem_read(uc, Paddr + 4, &p4, 4);
        uc_mem_read(uc, Paddr + 8, &p8, 4);
        uc_mem_read(uc, Paddr + 12, &p12, 4);
        uc_mem_read(uc, Paddr + 16, &p16, 4);
        printf("[JJFB_801] P@0x%X = {ER_RW=0x%X len=%u type=%d chunk=0x%X stack=0x%X}\n",
               Paddr, p0, p4, (int)p8, p12, p16);
    }

    if (code == 0) {
        jjfb_ensure_ext_chunk(uc, Paddr);
    }

    uint32_t r9;
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    if (erw) {
        uc_reg_write(uc, UC_ARM_REG_R9, &erw);
    }

    if (!quiet || (jjfb_ext_call_tick2 % 40) == 0) {
        printf("[JJFB_801] ext_call code=%d input=0x%X len=%d P=0x%X erw=0x%X helper=0x%X%s\n",
               code, in_addr, input_len, Paddr, erw, helper,
               quiet ? " (timer)" : "");
        fflush(stdout);
    }
    if (quiet) jjfb_ext_call_tick2++;

    /* MR_C_FUNCTION is 6-arg; R0-R3 + two stack slots for output/output_len. */
    uint32_t *out_holder = (uint32_t *)my_mallocExt(4);
    int32_t *out_len_holder = (int32_t *)my_mallocExt(4);
    if (!out_holder || !out_len_holder) {
        if (out_holder) my_freeExt(out_holder);
        if (out_len_holder) my_freeExt(out_len_holder);
        if (in_mem) my_freeExt(in_mem);
        uc_reg_write(uc, UC_ARM_REG_R9, &r9);
        pthread_mutex_unlock(&mutex);
        return MR_FAILED;
    }
    *out_holder = 0;
    *out_len_holder = 0;
    uint32_t out_holder_addr = toMrpMemAddr(out_holder);
    uint32_t out_len_addr = toMrpMemAddr(out_len_holder);

    uint32_t sp;
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    sp -= 8;
    uc_mem_write(uc, sp, &out_holder_addr, 4);      /* 5th arg: uint8** output */
    uc_mem_write(uc, sp + 4, &out_len_addr, 4);      /* 6th arg: int32* output_len */
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);

    uint32_t code_u = (uint32_t)code;
    uint32_t il = (uint32_t)input_len;
    uc_reg_write(uc, UC_ARM_REG_R0, &Paddr);
    uc_reg_write(uc, UC_ARM_REG_R1, &code_u);
    uc_reg_write(uc, UC_ARM_REG_R2, &in_addr);
    uc_reg_write(uc, UC_ARM_REG_R3, &il);
    /* Helper is Thumb when address is odd (robotol mr_helper). */
    bool thumb = (helper & 1u) != 0;
    /* v61: mark guest so 1E209 from timer/callback is deferred (no nested emu). */
    jjfb_in_guest_handler = 1;
    runCode(uc, thumb ? (helper & ~1u) : helper, CODE_ADDRESS, thumb);
    jjfb_in_guest_handler = 0;
    uint32_t v = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &v);
    if (!quiet || ((jjfb_ext_call_tick2 - 1) % 40) == 0 || (int32_t)v != 0) {
        printf("[JJFB_801] ext_call code=%d ret=%d out_len=%d\n", code, (int32_t)v, (int)*out_len_holder);
        fflush(stdout);
    }

    sp += 8;
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    uc_reg_write(uc, UC_ARM_REG_R9, &r9);
    my_freeExt(out_holder);
    my_freeExt(out_len_holder);
    if (in_mem) {
        my_freeExt(in_mem);
    }

    /* Deliver family work deferred during the helper (e.g. app=7 from 2F5404). */
    if (jjfb_pending_1e200) {
        printf("[JJFB_V61_NEST] flush deferred 1E200 after ext_call code=%d\n", code);
        fflush(stdout);
        jjfb_flush_1e200(uc);
    }

    /* After EXT timerTimeout, drive the 0x10140 app handler (same mutex). */
    if (code == 2 && jjfb_ext_timer_running) {
        jjfb_on_robotol_timer_tick(uc);
    }

    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    return (int32_t)v;
}

static inline int32_t bridge_mr_event(uc_engine *uc, int32_t code, int32_t param0, int32_t param1) {
    mr_c_event->code = code;
    mr_c_event->p0 = param0;
    mr_c_event->p1 = param1;
    return bridge_mr_extHelper(uc, 1, toMrpMemAddr(mr_c_event), sizeof(event_t));
}

// ??????????
int32_t bridge_dsm_network_cb(uc_engine *uc, uint32_t addr, int32_t p0, uint32_t p1) {
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    uint32_t ret, r9;
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);

    // ???????mr_extHelper???????????r9
    uc_reg_write(uc, UC_ARM_REG_R9, &mr_c_function_P->start_of_ER_RW);
    // ?????r9?????mythroad?????mythroad??lua??????
    // ???mythroad?????mythroad????????mrp??????????userData??????

    uc_reg_write(uc, UC_ARM_REG_R0, &p0);
    uc_reg_write(uc, UC_ARM_REG_R1, &p1);
    runCode(uc, addr, CODE_ADDRESS, false);

    uc_reg_write(uc, UC_ARM_REG_R9, &r9);  // ??r9
    uc_reg_read(uc, UC_ARM_REG_R0, &ret);
    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    return ret;
}

int32_t bridge_dsm_mr_start_dsm(uc_engine *uc, char *filename, char *ext, char *entry) {
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }

    printf("[JJFB_LOADER] bridge_dsm_mr_start_dsm filename=%s extName=%s entry=%s\n",
           filename ? filename : "(null)",
           ext ? ext : "(null)",
           entry ? entry : "(null)");

    mr_start_dsm_param->filename = (char *)copyStrToMrp(filename);
    mr_start_dsm_param->ext = (char *)copyStrToMrp(ext);
    mr_start_dsm_param->entry = entry ? (char *)copyStrToMrp(entry) : NULL;

    int32_t v = bridge_mr_event(uc, MR_START_DSM, toMrpMemAddr(mr_start_dsm_param), 0);
    printf("[JJFB_LOADER] bridge_dsm_mr_start_dsm ret=0x%X\n", v);

    my_freeExt(getMrpMemPtr((uint32_t)mr_start_dsm_param->filename));
    mr_start_dsm_param->filename = NULL;

    my_freeExt(getMrpMemPtr((uint32_t)mr_start_dsm_param->ext));
    mr_start_dsm_param->ext = NULL;

    if (entry) {
        my_freeExt(getMrpMemPtr((uint32_t)mr_start_dsm_param->entry));
    }

    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    return v;
}

int32_t bridge_dsm_mr_pauseApp(uc_engine *uc) {
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    int32_t v = bridge_mr_event(uc, MR_PAUSEAPP, 0, 0);
    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    return v;
}

int32_t bridge_dsm_mr_resumeApp(uc_engine *uc) {
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    int32_t v = bridge_mr_event(uc, MR_RESUMEAPP, 0, 0);
    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    return v;
}

int32_t bridge_dsm_mr_timer(uc_engine *uc) {
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    int32_t v = bridge_mr_event(uc, MR_TIMER, 0, 0);
    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    return v;
}

int32_t bridge_dsm_mr_event(uc_engine *uc, int32_t code, int32_t p0, int32_t p1) {
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    dsm_event->code = code;
    dsm_event->p0 = p0;
    dsm_event->p1 = p1;
    int32_t v = bridge_mr_event(uc, MR_EVENT, toMrpMemAddr(dsm_event), 0);
    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    return v;
}

int32_t bridge_dsm_init(uc_engine *uc) {
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    int32_t v = bridge_mr_event(uc, DSM_INIT, toMrpMemAddr(dsm_require_funcs), 0);

    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    if (v == VMRP_VER) {
        return MR_SUCCESS;
    } else {
        printf("err: dsm_version got %d expect %d\n", v, VMRP_VER);
    }
    return MR_FAILED;
}
