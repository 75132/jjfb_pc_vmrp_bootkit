#include "./header/bridge.h"

#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "./mythroad/include/dsm.h"
#include "./header/fileLib.h"
#include "./header/gwy_ext_obs_abi.h"
#include "./header/memory.h"
#include "./header/vmrp.h"
#include "./header/debug.h"
#include "./header/network.h"

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
    uint8 *start_of_ER_RW;  // RW段指针
    uint32 ER_RW_Length;    // RW长度
    int32 ext_type;         // ext启动类型，为1时表示ext启动
    void *mrc_extChunk;     // ext模块描述段，下面的结构体。
    int32 stack;            // stack shell 2008-2-28
} mr_c_function_P_t;

static void *mr_table;
static mr_c_function_P_t *mr_c_function_P;
static void *dsm_require_funcs;
static event_t *mr_c_event;  // 用于mrc_event参数传递的内存
static event_t *dsm_event;   // 用于传递真实事件
static start_t *mr_start_dsm_param;
static uint32_t mr_extHelper_addr;
static int g_in_ext_init_deliver;

static int32_t bridge_mr_extHelper(uc_engine *uc, uint32_t code, uint32_t input, uint32_t input_len);

/*
 * DOCUMENTED (mythroad_mini.c:mr_doExt / doc/反汇编研究.c mr_helper):
 *   801 code=6 → ER_RW+0x20 = input_len (= MR_VERSION)
 *   801 code=8 → ER_RW+0x24 = input (= &mrc_appInfo_st)
 *   801 code=0 → mrc_init(); refresh
 * mythroad_mini MR_VERSION=2011; sizeof(mrc_appInfoSt_st)=16 (id,ver,sidName,ram).
 */
#ifndef GWY_EXT_INIT_MR_VERSION
#define GWY_EXT_INIT_MR_VERSION 2011u
#endif

static uint32_t g_ext_appinfo_guest;

static uint32_t bridge_ext_appinfo_guest(void) {
    int32_t *host;
    const char *aid;
    const char *aver;
    if (g_ext_appinfo_guest) return g_ext_appinfo_guest;
    host = (int32_t *)my_mallocExt(16u);
    if (!host) return 0u;
    memset(host, 0, 16u);
    aid = getenv("GWY_PACKAGE_APPID");
    aver = getenv("GWY_PACKAGE_APPVER");
    if (aid && aid[0]) host[0] = (int32_t)strtoul(aid, NULL, 10);
    if (aver && aver[0]) host[1] = (int32_t)strtoul(aver, NULL, 10);
    /* host[2]=sidName ptr 0; host[3]=ram 0 */
    g_ext_appinfo_guest = toMrpMemAddr(host);
    return g_ext_appinfo_guest;
}

/* Deliver DOCUMENTED 6→8→0 once; caller must hold g_in_ext_init_deliver=0. */
static void bridge_deliver_ext_init_seq(uc_engine *uc, uint32_t before_or_after_code,
                                        const char *when) {
    int32_t r6, r8, r0;
    uint32_t appinfo;
    uint32_t ver = GWY_EXT_INIT_MR_VERSION;
    if (!mr_extHelper_addr) return;
    if (!gwy_ext_obs_take_ext_init_seq()) return;
    appinfo = bridge_ext_appinfo_guest();
    g_in_ext_init_deliver = 1;
    printf("[JJFB_INIT_SEQ] action=deliver_%s_code_%u helper=0x%X version=%u appinfo=0x%X "
           "evidence=DOCUMENTED source=mythroad_mini.c:mr_doExt\n",
           when, before_or_after_code, mr_extHelper_addr, ver, appinfo);
    fflush(stdout);
    /* code 6: only input_len (MR_VERSION) is consumed */
    r6 = bridge_mr_extHelper(uc, 6u, 0u, ver);
    /* code 8: guest pointer to mrc_appInfo_st */
    r8 = bridge_mr_extHelper(uc, 8u, appinfo, 16u);
    /* code 0: mrc_init; len conventionally MR_VERSION */
    r0 = bridge_mr_extHelper(uc, 0u, 0u, ver);
    printf("[JJFB_INIT_SEQ] delivered ret6=%d ret8=%d ret0=%d evidence=OBSERVED\n", (int)r6,
           (int)r8, (int)r0);
    fflush(stdout);
    g_in_ext_init_deliver = 0;
}

static const char MUTEX_LOCK_FAIL[] = "mutex lock fail";
static const char MUTEX_UNLOCK_FAIL[] = "mutex unlock fail";
static pthread_mutex_t mutex;

static void runCode(uc_engine *uc, uint32_t startAddr, uint32_t stopAddr, bool isThumb);

////////////////////////////////////////////////////////////////////////////////////////////////////

/* Phase 6N/C2: sendAppEvent → typed platform dispatch (0x10180 userinfo blob, else status 0). */
static void br_mrc_extMainSendAppEventShell(BridgeMap *o, uc_engine *uc) {
    uint32_t ret;
    (void)o;
    ret = gwy_ext_obs_sendappevent_dispatch(uc);
    SET_RET_V(ret);
}

static void br__mr_c_function_new(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T__mr_c_function_new)(MR_C_FUNCTION f, int32 len);
    uint32_t p_f, p_len;
    uc_reg_read(uc, UC_ARM_REG_R0, &p_f);
    uc_reg_read(uc, UC_ARM_REG_R1, &p_len);
    printf("ext call %s(0x%X[%u], 0x%X[%u])\n", o->name, p_f, p_f, p_len, p_len);
    dumpREG(uc);

    mr_extHelper_addr = p_f;
    mr_c_function_P = my_mallocExt(p_len);
    memset(mr_c_function_P, 0, p_len);

    uint32_t v = toMrpMemAddr(mr_c_function_P);
    /*
     * DOCUMENTED FixR9/mythroad: guest reads P via C_FUNCTION_P() at
     * (mr_c_function_load - 1) == caller EXT image+4. Nested mrc_loader/robotol
     * must not only publish DSM CODE_ADDRESS+4 or guest fills a dead P slot.
     */
    {
        uint32_t lr = 0;
        uint32_t slot;
        uint32_t dsm_slot = CODE_ADDRESS + 4;
        int wrote = 0;
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        slot = gwy_ext_obs_c_function_p_slot_va(p_f, lr);
        if (!slot) slot = dsm_slot;
        if (uc_mem_write(uc, slot, &v, 4) == UC_ERR_OK) wrote = 1;
        if (slot != dsm_slot) (void)uc_mem_write(uc, dsm_slot, &v, 4);
        gwy_ext_obs_note_c_function_p_slot(v, p_f, slot, lr, wrote);
    }
    /* Phase 5B: observe helper registration (rw filled by guest after return). */
    gwy_ext_obs_c_function_new(p_f, p_len, v, 0, 0, 0);
    /* Phase 6N: platform-owned mrc_extChunk alloc/publish into P+0xC (gated). */
    if (gwy_ext_obs_extchunk_want(p_f)) {
        void *chunk_host = my_mallocExt(0x40);
        if (chunk_host) {
            uint32_t chunk_guest;
            memset(chunk_host, 0, 0x40);
            chunk_guest = toMrpMemAddr(chunk_host);
            gwy_ext_obs_extchunk_on_c_function_new(uc, p_f, v, mr_c_function_P, chunk_host,
                                                  chunk_guest);
        }
    }
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
        gwy_ext_obs_alloc(ret, len);
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
    gwy_ext_obs_block_copy(dst, src, n);
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

// 获取参数的工具方法，第一个参数n=0
static uint32_t getArg(uc_engine *uc, uint32_t n) {
    uint32_t v;
    if (n <= 3) {  // 前四个参数直接从寄存器读
        uc_reg_read(uc, UC_ARM_REG_R0 + n, &v);
        return v;
    }

    uint32_t addr;
    uc_reg_read(uc, UC_ARM_REG_SP, &addr);

    addr += (n - 4) * 4;
    uc_mem_read(uc, addr, &v, 4);
    return v;
}

// 实际上mrc_refreshScreen()是调用的这个方法
static void br_mr_drawBitmap(BridgeMap *o, uc_engine *uc) {
    // typedef void (*T_mr_drawBitmap)(uint16* bmp, int16 x, int16 y, uint16 w, uint16 h);
    uint32_t bmp, x, y, w, h;

    uc_reg_read(uc, UC_ARM_REG_R0, &bmp);
    uc_reg_read(uc, UC_ARM_REG_R1, &x);
    uc_reg_read(uc, UC_ARM_REG_R2, &y);
    uc_reg_read(uc, UC_ARM_REG_R3, &w);

    uint32_t sp;
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_mem_read(uc, sp, &h, 4);

    LOG("ext call %s(0x%X, %d, %d, %u, %u)\n", o->name, bmp, x, y, w, h);
    printf("[JJFB_DRAW] api=mr_drawBitmap bmp=0x%X x=%d y=%d w=%u h=%u "
           "evidence=DOCUMENTED\n",
           bmp, (int)x, (int)y, w, h);
    printf("[JJFB_REFRESH] api=mr_drawBitmap note=mrc_refreshScreen_path evidence=DOCUMENTED\n");
    fflush(stdout);
    guiDrawBitmap(getMrpMemPtr(bmp), x, y, w, h);
}

static void br_mr_open(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_open)(const char* filename,  uint32 mode);
    uint32_t filename, mode;
    uc_reg_read(uc, UC_ARM_REG_R0, &filename);
    uc_reg_read(uc, UC_ARM_REG_R1, &mode);
    char *filenameStr = getMrpMemPtr(filename);
    int32_t ret = my_open(filenameStr, mode);
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
    /* Deadline poll: guest wait loops often call get_uptime without sendAppEvent. */
    gwy_ext_obs_timer_poll_uc(uc);
    LOG("ext call %s(): 0x%X[%u]\n", o->name, ret, ret);
    SET_RET_V(ret);
}

static void br_log(BridgeMap *o, uc_engine *uc) {
    // void (*log)(char *msg);
    uint32_t msg;
    uint32_t ext_base = 0, sync_len = 0, helper = 0, p_addr = 0, p_len = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &msg);

    char *str = (char *)getMrpMemPtr(msg);
    (void)o;
    /* Guest mr_printf → dsmInFuncs->log; observe nested EXT load (no ordinal). */
    if (str) {
        if (sscanf(str, "[WARN]mr_cacheSync(0x%x, %u)", &ext_base, &sync_len) == 2 ||
            sscanf(str, "mr_cacheSync(0x%x, %u)", &ext_base, &sync_len) == 2) {
            gwy_ext_obs_code_image(ext_base, sync_len);
        } else if (sscanf(str, "--- ext: @%x", &ext_base) == 1) {
            /* DOCUMENTED: case800 input1 (MRPG base). cacheSync uses align-down address. */
            printf("[GUEST_EXT] ext_base=0x%X\n", ext_base);
            fflush(stdout);
            gwy_ext_obs_ext_image_raw(ext_base);
        } else if (sscanf(str, "_mr_c_function_new(%x, %u)  mr_c_function_P:%x", &helper, &p_len,
                          &p_addr) == 3 ||
                   sscanf(str, "_mr_c_function_new(%x, %u)", &helper, &p_len) == 2) {
            /* Nested CFN often runs in guest DSM (not host MAP_FUNC). Publish
             * C_FUNCTION_P into the EXT image+4 that owns helper (FixR9), then
             * platform extChunk when LOG_PARSE yields P.
             * DOCUMENTED mythroad _mr_c_function_new: mr_c_function = f; also
             * retarget host bridge_mr_extHelper so later TestCom 801 reaches the
             * active nested EXT (mrc_loader/robotol/mmochat), not stale DSM. */
            if (helper) {
                uint32_t old_helper = mr_extHelper_addr;
                mr_extHelper_addr = helper;
                printf("[JJFB_HELPER_RETARGET] old=0x%X new=0x%X P=0x%X origin=LOG_PARSE "
                       "evidence=DOCUMENTED source=mythroad.c:_mr_c_function_new\n",
                       old_helper, helper, p_addr);
                fflush(stdout);
            }
            if (p_addr) {
                void *p_host = getMrpMemPtr(p_addr);
                uint32_t slot = gwy_ext_obs_c_function_p_slot_va(helper, 0);
                uint32_t dsm_slot = CODE_ADDRESS + 4;
                int wrote = 0;
                if (p_host) {
                    /* Guest-owned P for nested EXT; host must dispatch with same VA. */
                    mr_c_function_P = (mr_c_function_P_t *)p_host;
                }
                if (!slot) slot = dsm_slot;
                if (uc_mem_write(uc, slot, &p_addr, 4) == UC_ERR_OK) wrote = 1;
                if (slot != dsm_slot) (void)uc_mem_write(uc, dsm_slot, &p_addr, 4);
                gwy_ext_obs_note_c_function_p_slot(p_addr, helper, slot, 0, wrote);
            }
            if (p_addr && gwy_ext_obs_extchunk_want(helper)) {
                void *p_host = getMrpMemPtr(p_addr);
                if (!gwy_ext_obs_extchunk_try_reuse(uc, helper, p_addr, p_host)) {
                    void *chunk_host = my_mallocExt(0x40);
                    if (chunk_host) {
                        uint32_t chunk_guest;
                        memset(chunk_host, 0, 0x40);
                        chunk_guest = toMrpMemAddr(chunk_host);
                        gwy_ext_obs_extchunk_on_c_function_new(uc, helper, p_addr, p_host,
                                                              chunk_host, chunk_guest);
                    }
                }
            }
            gwy_ext_obs_c_function_new_ex(helper, p_len, p_addr, 0, 0, 0, "LOG_PARSE");
        }
        puts(str);
    }
}

/* Track DSM heap so shell-core continue can free before next _mr_mem_init. */
static uint32_t g_br_mem_guest;
static void *g_br_mem_host;
static uint32_t g_br_mem_len;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused))
#endif
static void br_mem_release_tracked(const char *reason) {
    if (!g_br_mem_host) return;
    printf("[JJFB_DSM_HEAP] action=free guest=0x%X len=%u reason=%s "
           "evidence=TARGET_OBSERVED\n",
           g_br_mem_guest, g_br_mem_len, reason ? reason : "?");
    fflush(stdout);
    my_freeExt(g_br_mem_host);
    g_br_mem_host = NULL;
    g_br_mem_guest = 0;
    g_br_mem_len = 0;
}

static void br_mem_get(BridgeMap *o, uc_engine *uc) {
    // int32 (*mem_get)(char **mem_base, uint32 *mem_len);
    uint32_t mem_base, mem_len;
    uc_reg_read(uc, UC_ARM_REG_R0, &mem_base);
    uc_reg_read(uc, UC_ARM_REG_R1, &mem_len);

    LOG("ext call %s()\n", o->name);

    uint32_t len = 1024 * 1024 * 4;
    void *host;
    uint32_t buffer;

    /*
     * Shell continue: prefer reusing the tracked 4MB DSM heap so _mr_mem_init
     * re-initializes LG_mem on the same block (avoids second-alloc OOM).
     */
    if (g_br_mem_host && g_br_mem_guest && g_br_mem_len >= len) {
        buffer = g_br_mem_guest;
        len = g_br_mem_len;
        memset(g_br_mem_host, 0, (size_t)len);
        printf("[JJFB_DSM_HEAP] action=reuse guest=0x%X len=%u reason=shell_package_restart "
               "evidence=TARGET_OBSERVED\n",
               buffer, len);
        printf("br_mem_get base=0x%X len=%d(%d kb) =================\n", buffer, len, len / 1024);
        fflush(stdout);
        uc_mem_write(uc, mem_base, &buffer, 4);
        uc_mem_write(uc, mem_len, &len, 4);
        SET_RET_V(MR_SUCCESS);
        return;
    }

    host = my_mallocExt(len);

    /* Fail closed: never publish a garbage guest VA when host alloc fails. */
    if (!host) {
        uint32_t zero = 0;
        printf("br_mem_get failed=no_memory len=%d =================\n", len);
        fflush(stdout);
        uc_mem_write(uc, mem_base, &zero, 4);
        uc_mem_write(uc, mem_len, &zero, 4);
        SET_RET_V(MR_FAILED);
        return;
    }
    buffer = toMrpMemAddr(host);
    g_br_mem_host = host;
    g_br_mem_guest = buffer;
    g_br_mem_len = len;

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
    void *host;
    uc_reg_read(uc, UC_ARM_REG_R0, &mem);
    uc_reg_read(uc, UC_ARM_REG_R1, &mem_len);

    LOG("ext call %s(0x%X, 0x%X)\n", o->name, mem, mem_len);
    host = getMrpMemPtr(mem);
    if (host && host == g_br_mem_host) {
        g_br_mem_host = NULL;
        g_br_mem_guest = 0;
        g_br_mem_len = 0;
    }
    my_freeExt(host);
    SET_RET_V(MR_SUCCESS);
}

static void br_timerStop(BridgeMap *o, uc_engine *uc) {
    // int32 (*timerStop)(void);
    uint32_t pc = 0;
    LOG("ext call %s()\n", o->name);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    /* Sync platform deadline + SDL; mr_table was previously NULL-stubbed. */
    gwy_ext_obs_timer_host_disarm(o && o->name ? o->name : "timerStop", pc);
    SET_RET_V(MR_SUCCESS);
}

static void br_timerStart(BridgeMap *o, uc_engine *uc) {
    // int32 (*timerStart)(uint16 t);
    uint32_t t = 0;
    uint32_t pc = 0;
    LOG("ext call %s()\n", o->name);
    uc_reg_read(uc, UC_ARM_REG_R0, &t);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    /*
     * DOCUMENTED: mr_table.mr_timerStart / mrc.timerStart.
     * Must arm platform_timer deadline — SDL_AddTimer alone is not enough for
     * host_timer_poll (signal_due no-ops when g_running==0).
     */
    gwy_ext_obs_timer_host_arm((uint16_t)t, o && o->name ? o->name : "timerStart", pc);
    SET_RET_V(MR_SUCCESS);
}

static void br_test(BridgeMap *o, uc_engine *uc) {
    // void (*test)(void);
    LOG("ext call %s()\n", o->name);
}

static int32_t bridge_dsm_mr_start_dsm_unlocked(uc_engine *uc, char *filename, char *ext,
                                                char *entry);

static void br_exit(BridgeMap *o, uc_engine *uc) {
    // void (*exit)(void);
    LOG("ext call %s()\n", o->name);
    /* Phase 6P: first post-gbrwcore exit may continue into gamelist (no process exit). */
    if (gwy_shell_shim_try_continue_after_mr_exit(uc)) {
        const char *tgt = gwy_shell_shim_continue_target();
        const char *prm = gwy_shell_shim_continue_param();
        /* Keep DSM heap tracked for br_mem_get reuse (avoid 6MB manager OOM). */
        printf("[JJFB_DSM_HEAP] action=keep_for_reuse guest=0x%X len=%u "
               "reason=shell_core_continue evidence=TARGET_OBSERVED\n",
               g_br_mem_guest, g_br_mem_len);
        printf("[JJFB_SHELL_CORE_CONTINUE] apply=bridge_dsm_mr_start_dsm target=%s "
               "reason=continue_after_gbrwcore_init\n",
               tgt ? tgt : "?");
        fflush(stdout);
        (void)bridge_dsm_mr_start_dsm_unlocked(uc, (char *)tgt, "start.mr",
                                               prm ? (char *)prm : NULL);
        /*
         * Nested continue finishes gamelist MR_START_DSM while outer start_dsm
         * still holds the bridge mutex — host loop cannot run yet.
         * DOCUMENTED: EXT timer armed during init must still deliver MR_TIMER.
         * Pump deadlines here (unlocked deliver) until idle or timeout.
         */
        {
            uint32_t t0 = (uint32_t)get_uptime_ms();
            int idle = 0;
            int pumps = 0;
            printf("[PLATFORM_TIMER] op=POST_CONT_PUMP begin evidence=DOCUMENTED\n");
            fflush(stdout);
            for (;;) {
                gwy_ext_obs_timer_poll_uc(uc);
                if (!gwy_ext_obs_timer_running()) {
                    if (++idle >= 6) break;
                } else {
                    idle = 0;
                }
                /* Allow several re-armed periods after first fire (gamelist uses 5s). */
                if (((uint32_t)get_uptime_ms() - t0) > 60000u) break;
                if (++pumps > 1500) break;
                usleep(50000u);
            }
            printf("[PLATFORM_TIMER] op=POST_CONT_PUMP end pumps=%d evidence=DOCUMENTED\n",
                   pumps);
            fflush(stdout);
        }
        return;
    }
    gwy_ext_obs_mr_exit(uc);
    puts("mythroad exit.\n");
    exit(0);
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
    /* Slice sleep so deadline timers can fire while guest holds emu. */
    while (ms > 0) {
        uint32_t slice = (ms > 50u) ? 50u : ms;
        usleep(slice * 1000u);
        ms -= slice;
        gwy_ext_obs_timer_poll_uc(uc);
    }
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
static char *readdirSharedMem;  // 文件名的共享内存
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
    LOG("ext call %s()\n", o->name);
    uint32_t cb, mode, userData;
    int32_t stub_ret = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &cb);
    uc_reg_read(uc, UC_ARM_REG_R1, &mode);
    uc_reg_read(uc, UC_ARM_REG_R2, &userData);
    /* Phase 6G: shell-layer no_update stub (never applies to jjfb game self). */
    if (gwy_shell_shim_try_init_network(uc, cb, getMrpMemPtr(mode), userData, &stub_ret)) {
        SET_RET_V(stub_ret);
        return;
    }
    SET_RET_V(my_initNetwork(uc, (void *)cb, getMrpMemPtr(mode), (void *)userData));
}

static void br_mr_socket(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_socket)(int32 type, int32 protocol);
    int32_t type, protocol;
    uc_reg_read(uc, UC_ARM_REG_R0, &type);
    uc_reg_read(uc, UC_ARM_REG_R1, &protocol);
    int32_t ret = my_socket(type, protocol);
    LOG("ext call %s(): %d \n", o->name, ret);
    SET_RET_V(ret);
}

static void br_mr_connect(BridgeMap *o, uc_engine *uc) {
    // int32 (*mr_connect)(int32 s, int32 ip, uint16 port, int32 type);
    LOG("ext call %s()\n", o->name);
    int32_t s, ip, port, type;
    uc_reg_read(uc, UC_ARM_REG_R0, &s);
    uc_reg_read(uc, UC_ARM_REG_R1, &ip);
    uc_reg_read(uc, UC_ARM_REG_R2, &port);
    uc_reg_read(uc, UC_ARM_REG_R3, &type);
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
获取socket connect 状态（主要用于TCP的异步连接）
Syntax
int32 mrc_getSocketState(int32 s);
Parameters
s
   [IN] 打开的socket句柄，由mrc_socket创建

Return Value
   MR_SUCCESS ： 连接成功
   MR_FAILED ： 连接失败
   MR_WAITING ： 连接中
   MR_IGNORE ： 不支持该功能
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
播放声音数据
type [IN] 声音数据类型，见MR_SOUND_TYPE定义，此函数支持MR_SOUND_MIDI MR_SOUND_WAV MR_SOUND_MP3
data [IN] 声音数据指针
datalen [IN] 声音数据长度
loop [IN] 0:单次播放, 1:循环播放
Return Value MR_SUCCESS 成功 MR_FAILED 失败
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
停止播放声音数据
type [IN] 声音数据类型，见MR_SOUND_TYPE定义，此函数支持MR_SOUND_MIDI MR_SOUND_WAV MR_SOUND_MP3
Return Value MR_SUCCESS 成功 MR_FAILED 失败
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
    MR_DIALOG_KEY_OK,     // 对话框/文本框等的"确定"键被点击(选择);
    MR_DIALOG_KEY_CANCEL  // 对话框/文本框等的"取消"("返回")键被点击(选择);
};

enum {
    MR_DIALOG_OK,         // 对话框有"确定"键;
    MR_DIALOG_OK_CANCEL,  // 对话框有"确定" "取消"键;
    MR_DIALOG_CANCEL      // 对话框有"返回"键
};

/*
创建一个对话框，并返回对话框句柄。当对话框显示时，如果用户按了对话框上的某个键，系统将构造Mythroad应用消息，通过mrc_event函数传送给Mythroad应用，
消息类型为MR_DIALOG_EVENT，参数为该按键的ID。"确定"键ID为：MR_DIALOG_KEY_OK；"取消"键ID为：MR_DIALOG_KEY_CANCEL。
title [IN]对话框的标题，unicode编码，网络字节序
text [IN]对话框内容，unicode编码，网络字节序
type [IN]对话框类型：MR_DIALOG_OK MR_DIALOG_OK_CANCEL MR_DIALOG_CANCEL

Return Value 正整数 对话框句柄 MR_FAILED 失败
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
刷新对话框的显示。
dialog [IN]对话框的句柄
title [IN]对话框的标题，unicode编码，网络字节序
text [IN]对话框内容，unicode编码，网络字节序
type [IN]若type为-1，表示type不变,见定义MR_DIALOG_OK MR_DIALOG_OK_CANCEL MR_DIALOG_CANCEL
Return Value MR_SUCCESS 成功 MR_FAILED 失败
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
创建一个文本框，并返回文本框句柄
title [IN]文本框的标题，unicode编码，网络字节序
text [IN]文本框内容，unicode编码，网络字节序
type [IN]文本框按键类型,见定义MR_DIALOG_OK MR_DIALOG_OK_CANCEL MR_DIALOG_CANCEL
Return Value 正整数 文本框句柄 MR_FAILED 失败
Remarks
   文本框用来显示只读的文字信息。文本框和对话框并没有本质的区别，仅仅是显示方式上的不同，在使用上它们的主要区别是：对话框的内容一般较短，文本框的内容一般较长，
   对话框一般实现为弹出式的窗口，文本框一般实现为全屏式的窗口。也可能在手机上对话框和文本框使用了相同的方式实现。文本框和对话框的消息参数是一样的。当文本框显示时，
   如果用户选择了文本框上的某个键，系统将构造Mythroad应用消息，通过mrc_event函数传送给Mythroad 平台，消息类型为MR_DIALOG_EVENT，参数为该按键的ID。
   "确定"键ID为：MR_DIALOG_KEY_OK；"取消"键ID为：MR_DIALOG_KEY_CANCEL。
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
创建一个编辑框，并返回编辑框句柄。
title [IN]文本框的标题，unicode编码，网络字节序
text [IN]文本框内容，unicode编码，网络字节序
type [IN]见MR_EDIT_ANY;MR_EDIT_NUMERIC;MR_EDIT_PASSWORD定义
max_size [IN]最多可以输入的字符（unicode）个数，这里每一个中文、字母、数字、符号都算一个字符
Return Value 正整数 编辑框句柄 MR_FAILED 失败
Remarks
   编辑框用来显示并提供用户编辑文字信息。text是编辑框显示的初始内容。当编辑框显示时，
如果用户选择了编辑框上的某个键，系统将构造Mythroad应用消息，通过mrc_event函数传送给
Mythroad应用，消息类型为MR_DIALOG_EVENT，参数为该按键的ID;"确定"键ID为：MR_DIALOG_KEY_OK；
"取消"键ID为：MR_DIALOG_KEY_CANCEL。
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
获取编辑框内容，unicode编码。调用者若需在编辑框释放后仍然使用编辑框的内容，需要自行保存该内容。该函数需要在编辑框释放之前调用。
Return Value 非NULL 编辑框的内容指针，unicode编码, NULL 失败
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

// 偏移量由./mrc/[x]_offsets.c直接从mrp中导出
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
    BRIDGE_FUNC_MAP(0x3C, MAP_FUNC, strlen, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x40, MAP_FUNC, strstr, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x44, MAP_FUNC, sprintf, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x48, MAP_FUNC, atoi, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x4C, MAP_FUNC, strtoul, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x50, MAP_FUNC, rand, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x54, MAP_FUNC, mrc_extMainSendAppEventShell, NULL, br_mrc_extMainSendAppEventShell, 0),
    BRIDGE_FUNC_MAP(0x58, MAP_DATA, reserve1, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x5C, MAP_DATA, _mr_c_internal_table, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x60, MAP_DATA, _mr_c_port_table, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x64, MAP_FUNC, _mr_c_function_new, NULL, br__mr_c_function_new, 0),
    BRIDGE_FUNC_MAP(0x68, MAP_FUNC, mr_printf, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x6C, MAP_FUNC, mr_mem_get, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x70, MAP_FUNC, mr_mem_free, NULL, NULL, 0),
    /* Stage E6: DOCUMENTED draw entry used by plat 0x10113 getProc (mr_table+0x1E0/_DrawBitmap). */
    BRIDGE_FUNC_MAP(0x74, MAP_FUNC, mr_drawBitmap, NULL, br_mr_drawBitmap, 0),
    BRIDGE_FUNC_MAP(0x78, MAP_FUNC, mr_getCharBitmap, NULL, NULL, 0),
    /* Stage E5: DOCUMENTED mr_timerStart/Stop were NULL — guest arm never reached host. */
    BRIDGE_FUNC_MAP(0x7C, MAP_FUNC, mr_timerStart, NULL, br_timerStart, 0),
    BRIDGE_FUNC_MAP(0x80, MAP_FUNC, mr_timerStop, NULL, br_timerStop, 0),
    BRIDGE_FUNC_MAP(0x84, MAP_FUNC, mr_getTime, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x88, MAP_FUNC, mr_getDatetime, NULL, NULL, 0),
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
    BRIDGE_FUNC_MAP(0xD8, MAP_FUNC, mr_exit, NULL, NULL, 0),
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
    BRIDGE_FUNC_MAP(0x144, MAP_FUNC, mr_initNetwork, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x148, MAP_FUNC, mr_closeNetwork, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x14C, MAP_FUNC, mr_getHostByName, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x150, MAP_FUNC, mr_socket, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x154, MAP_FUNC, mr_connect, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x158, MAP_FUNC, mr_closeSocket, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x15C, MAP_FUNC, mr_recv, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x160, MAP_FUNC, mr_recvfrom, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x164, MAP_FUNC, mr_send, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x168, MAP_FUNC, mr_sendto, NULL, NULL, 0),
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
    BRIDGE_FUNC_MAP(0x1D8, MAP_FUNC, _DispUpEx, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1DC, MAP_FUNC, _DrawPoint, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1E0, MAP_FUNC, _DrawBitmap, NULL, br_mr_drawBitmap, 0),
    BRIDGE_FUNC_MAP(0x1E4, MAP_FUNC, _DrawBitmapEx, NULL, br_mr_drawBitmap, 0),
    BRIDGE_FUNC_MAP(0x1E8, MAP_FUNC, DrawRect, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1EC, MAP_FUNC, _DrawText, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1F0, MAP_FUNC, _BitmapCheck, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1F4, MAP_FUNC, _mr_readFile, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1F8, MAP_FUNC, mr_wstrlen, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1FC, MAP_FUNC, mr_registerAPP, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x200, MAP_FUNC, _DrawTextEx, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x204, MAP_FUNC, _mr_EffSetCon, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x208, MAP_FUNC, _mr_TestCom, NULL, NULL, 0),
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
    BRIDGE_FUNC_MAP(0x244, MAP_FUNC, mr_platDrawChar, NULL, NULL, 0),
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
            uint32_t slot = (uint32_t)address;
            if (obj->fn == NULL) {
                /* Phase 6D-A: observe before exit; do not stub or fake the API. */
                gwy_ext_obs_unimplemented_api(uc, slot, obj->name);
                printf("!!! %s() Not yet implemented function !!! \n", obj->name);
                exit(1);
                return;
            }
            /* Phase 6C-D1: observe-only snapshots; must not mutate r4-r11/r9. */
            gwy_ext_obs_host_callback_enter(uc, slot, obj->name);
            obj->fn(obj, uc);
            gwy_ext_obs_host_callback_leave(uc, slot, obj->name);

            uint32_t _lr;
            uc_reg_read(uc, UC_ARM_REG_LR, &_lr);
            uc_reg_write(uc, UC_ARM_REG_PC, &_lr);
            gwy_ext_obs_host_callback_resume(uc, slot, obj->name);
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
                // 默认的函数初始化，初始化为地址值，当PC寄存器执行到该地址时拦截下来进入我们的回调函数
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
    uint32_t pc;
    uc_reg_write(uc, UC_ARM_REG_LR, &stopAddr);  // 当程序执行到这里时停止运行(return)

    /* Instruction slices (unicorn 1.0.2 timeout is unreliable). Poll deadline
     * between slices so EXT timers advance while start_dsm holds the mutex. */
    pc = isThumb ? (startAddr | 1u) : startAddr;
    for (;;) {
        uint32_t cpsr = 0;
        uc_err err = uc_emu_start(uc, pc, stopAddr, 0, 100000ull);
        gwy_ext_obs_timer_poll_uc(uc);
        uc_reg_read(uc, UC_ARM_REG_PC, &pc);
        if ((pc & ~1u) == (stopAddr & ~1u)) break;
        if (err != UC_ERR_OK) {
            printf("Failed on uc_emu_start() with error returned: %u (%s)\n", err,
                   uc_strerror(err));
            exit(1);
        }
        uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
        if (cpsr & (1u << 5))
            pc |= 1u;
        else
            pc &= ~1u;
    }
    gwy_ext_obs_timer_poll_uc(uc);
}

/* Forward: defined below with start_dsm helpers. */
static int32_t bridge_mr_event(uc_engine *uc, int32_t code, int32_t param0, int32_t param1);

static uint32_t bridge_timer_clock_ms(void) {
    return (uint32_t)get_uptime_ms();
}

/* Mutex already held by outer start_dsm / plat stub — do not lock again. */
static int32_t bridge_ext_helper_call(uc_engine *uc, uint32_t helper, uint32_t p_guest,
                                      uint32_t code, uint32_t input, uint32_t input_len,
                                      uint32_t erw) {
    uint32_t r9_save = 0;
    uint32_t ret = 0;
    int thumb;
    if (!uc || !helper || !p_guest) return MR_FAILED;
    uc_reg_read(uc, UC_ARM_REG_R9, &r9_save);
    if (erw) uc_reg_write(uc, UC_ARM_REG_R9, &erw);
    uc_reg_write(uc, UC_ARM_REG_R0, &p_guest);
    uc_reg_write(uc, UC_ARM_REG_R1, &code);
    uc_reg_write(uc, UC_ARM_REG_R2, &input);
    uc_reg_write(uc, UC_ARM_REG_R3, &input_len);
    thumb = (helper & 1u) != 0;
    runCode(uc, helper & ~1u, CODE_ADDRESS, thumb);
    uc_reg_read(uc, UC_ARM_REG_R0, &ret);
    uc_reg_write(uc, UC_ARM_REG_R9, &r9_save);
    return (int32_t)ret;
}

static int32_t bridge_deliver_timer_body(uc_engine *uc) {
    uint32_t helper = 0, p_guest = 0, erw = 0;
    int32_t v;
    const char *dtr = getenv("JJFB_TIMER_DELIVER_TRACE");
    if (!uc) return MR_FAILED;
    /* DOCUMENTED mrc_extHelper case 2 → mrc_timerTimeout (not DSM Lua MR_TIMER). */
    if (gwy_ext_obs_timer_ext_target(&helper, &p_guest, &erw)) {
        printf("[PLATFORM_TIMER] op=FIRE_EXT code=2 helper=0x%X P=0x%X erw=0x%X "
               "chunk=0x%X evidence=DOCUMENTED\n",
               helper, p_guest, erw, gwy_ext_obs_timer_armed_chunk());
        fflush(stdout);
        if (dtr && dtr[0] == '1') {
            printf("[JJFB_TIMER_FIRE] timer_id=1 owner_module=? t_ms=0 due_ms=0 "
                   "route=ext_code2 evidence=DOCUMENTED\n");
            printf("[JJFB_TIMER_DELIVER_ATTEMPT] timer_id=1 target_helper=0x%X method=2 "
                   "P=0x%X r9=0x%X route=mrc_timerTimeout evidence=DOCUMENTED\n",
                   helper, p_guest, erw);
            fflush(stdout);
        }
        v = bridge_ext_helper_call(uc, helper, p_guest, 2u, 0u, 0u, erw);
        printf("[PLATFORM_TIMER] op=FIRE_EXT_DONE ret=%d evidence=DOCUMENTED\n", (int)v);
        if (dtr && dtr[0] == '1') {
            printf("[JJFB_TIMER_DELIVERED] timer_id=1 method=2 ret=%d route=ext_code2 "
                   "evidence=OBSERVED\n",
                   (int)v);
            fflush(stdout);
        }
        fflush(stdout);
        return v;
    }
    /* No classic EXT chunk: prefer registered 0x10140 lifecycle tick (docs/06). */
    if (gwy_ext_obs_lifecycle_on_timer_due(uc)) return MR_SUCCESS;
    if (dtr && dtr[0] == '1') {
        printf("[JJFB_TIMER_FIRE] timer_id=1 owner_module=? route=MR_TIMER "
               "evidence=DOCUMENTED\n");
        printf("[JJFB_TIMER_DELIVER_ATTEMPT] timer_id=1 method=1 route=bridge_mr_event "
               "evidence=DOCUMENTED\n");
        fflush(stdout);
    }
    v = bridge_mr_event(uc, MR_TIMER, 0, 0);
    if (dtr && dtr[0] == '1') {
        printf("[JJFB_TIMER_DELIVERED] timer_id=1 method=1 ret=%d route=MR_TIMER "
               "evidence=OBSERVED\n",
               (int)v);
        fflush(stdout);
    }
    return v;
}

static void bridge_deliver_mr_timer_unlocked(void *uc_ptr) {
    uc_engine *uc = (uc_engine *)uc_ptr;
    if (!uc) return;
    (void)bridge_deliver_timer_body(uc);
}

uc_err bridge_init(uc_engine *uc) {
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("mutex init fail");
        exit(EXIT_FAILURE);
    }
    gwy_ext_obs_set_guest_allocator(my_mallocExt, toMrpMemAddr);
    gwy_ext_obs_set_timer_fns(timerStart, timerStop);
    gwy_ext_obs_set_timer_clock(bridge_timer_clock_ms);
    gwy_ext_obs_set_timer_deliver(bridge_deliver_mr_timer_unlocked);
    uint32_t len = 4 * countof(mr_table_funcMap);  // 因为都是指针，所以直接可以算出来总内存大小
    mr_table = hooks_init(uc, mr_table_funcMap, countof(mr_table_funcMap), len);
    /* Phase 6N: publish sendAppEvent stub guest address (+0x54 in mr_table). */
    gwy_ext_obs_extchunk_set_mr_table(toMrpMemAddr(mr_table));
    gwy_ext_obs_extchunk_set_sendappevent(toMrpMemAddr(mr_table) + 0x54u);

    dsm_require_funcs = hooks_init(uc, dsm_require_funcs_funcMap, countof(dsm_require_funcs_funcMap), sizeof(DSM_REQUIRE_FUNCS));
#ifdef __EMSCRIPTEN__
    ((DSM_REQUIRE_FUNCS *)dsm_require_funcs)->flags = FLAG_USE_UTF8_FS;  // wasm文件系统是UTF8编码
#else
    ((DSM_REQUIRE_FUNCS *)dsm_require_funcs)->flags = FLAG_USE_UTF8_EDIT;  // windows下文件系统是GBK编码，编辑框暂时用复制粘贴代替（需要utf8编码）
#endif

    mr_c_event = my_mallocExt(sizeof(event_t));
    dsm_event = my_mallocExt(sizeof(event_t));
    mr_start_dsm_param = my_mallocExt(sizeof(start_t));
    return UC_ERR_OK;
}

uc_err bridge_ext_init(uc_engine *uc) {
    uint32_t v = toMrpMemAddr(mr_table);
    uc_mem_write(uc, CODE_ADDRESS, &v, 4);  // 设置mr_table

    v = 1;  // 传参数1 使用mr_extHelper，因为mr_helper会有刷屏操作
    uc_reg_write(uc, UC_ARM_REG_R0, &v);

    // 执行ext内的mr_c_function_load()
    runCode(uc, CODE_ADDRESS + 8, CODE_ADDRESS, false);

    // mr_c_function.start_of_ER_RW 会被写入r9(SB)，指向的内存是用来存放全局变量的
    printf("-----> r9:@%p\n", mr_c_function_P->start_of_ER_RW);
    if (mr_c_function_P) {
        gwy_ext_obs_p_update(mr_extHelper_addr,
                             (uint32_t)(uintptr_t)mr_c_function_P->start_of_ER_RW,
                             mr_c_function_P->ER_RW_Length,
                             (uint32_t)mr_c_function_P->stack);
    }
    return UC_ERR_OK;
}

static int32_t bridge_mr_extHelper(uc_engine *uc, uint32_t code, uint32_t input, uint32_t input_len) {
    // int32 (*mr_extHelper)(void* P, int32 code, uint8* input, int32 input_len);
    uint32_t v = toMrpMemAddr(mr_c_function_P);
    uint32_t rw_base = 0;
    uint32_t rw_size = 0;
    uint32_t stack_base = 0;

    /*
     * After nested game EXT ER_RW/R9 restore, host still only saw method=1 events.
     * DOCUMENTED start.mr / mr_doExt issues 6→8→0 before events.
     * Deliver on enter (pending from a prior call) or on exit (queued mid-call).
     */
    if (!g_in_ext_init_deliver && code != 0u && code != 6u && code != 8u)
        bridge_deliver_ext_init_seq(uc, code, "before");

    if (mr_c_function_P) {
        /* Guest writes guest addresses into P; bit-pattern is the guest VA. */
        rw_base = (uint32_t)(uintptr_t)mr_c_function_P->start_of_ER_RW;
        rw_size = mr_c_function_P->ER_RW_Length;
        stack_base = (uint32_t)mr_c_function_P->stack;
        gwy_ext_obs_p_update(mr_extHelper_addr, rw_base, rw_size, stack_base);
    }
    /* Stage E3: observe DOCUMENTED init sequence codes (mythroad case_801 / start.mr). */
    if (code == 0u || code == 6u || code == 8u) {
        printf("[JJFB_INIT_SEQ] helper=0x%X code=%u P=0x%X input=0x%X len=%u "
               "er_rw=0x%X evidence=DOCUMENTED source=mythroad.c:case_801\n",
               mr_extHelper_addr, code, v, input, input_len, rw_base);
        fflush(stdout);
    }
    uc_reg_write(uc, UC_ARM_REG_R0, &v);          // p
    uc_reg_write(uc, UC_ARM_REG_R1, &code);       // code
    uc_reg_write(uc, UC_ARM_REG_R2, &input);      // input
    uc_reg_write(uc, UC_ARM_REG_R3, &input_len);  // input_len

    {
        uint32_t sp = 0;
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        gwy_ext_obs_entry_begin(mr_extHelper_addr, code, v, input, input_len, rw_base, sp);
    }

    /* Phase 6C-A: switch R9 to helper module ER_RW (r0=P unchanged). */
    gwy_ext_obs_r9_switch_enter_helper(uc, mr_extHelper_addr, 3 /* GWY_CALL_MR_HELPER */);

    runCode(uc, mr_extHelper_addr, CODE_ADDRESS, false);
    gwy_ext_obs_r9_switch_leave(uc);
    uc_reg_read(uc, UC_ARM_REG_R0, &v);
    gwy_ext_obs_helper_call(mr_extHelper_addr, code, (int32_t)v);

    /* Mid-call queue (R9 restore during nested guest) → deliver before returning. */
    if (!g_in_ext_init_deliver && code != 0u && code != 6u && code != 8u)
        bridge_deliver_ext_init_seq(uc, code, "after");

    return (int32_t)v;
}

static int32_t bridge_mr_event(uc_engine *uc, int32_t code, int32_t param0, int32_t param1) {
    mr_c_event->code = code;
    mr_c_event->p0 = param0;
    mr_c_event->p1 = param1;
    return bridge_mr_extHelper(uc, 1, toMrpMemAddr(mr_c_event), sizeof(event_t));
}

// 执行网络通信的回调
int32_t bridge_dsm_network_cb(uc_engine *uc, uint32_t addr, int32_t p0, uint32_t p1) {
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    uint32_t ret, r9;
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);

    // 因为回调不是从mr_extHelper调用，因此需要手动设置r9
    gwy_ext_obs_r9_write_raw(uc, (uint32_t)(uintptr_t)mr_c_function_P->start_of_ER_RW,
                             "bridge_dsm_network_cb_set");
    // 实际上这个r9值被设置成mythroad层的，因为mythroad层的lua部分也会调用
    // 因此由mythroad层去区分是mythroad层的回调函数还是mrp层的回调函数，这就是userData存在的意义

    uc_reg_write(uc, UC_ARM_REG_R0, &p0);
    uc_reg_write(uc, UC_ARM_REG_R1, &p1);
    runCode(uc, addr, CODE_ADDRESS, false);

    gwy_ext_obs_r9_write_raw(uc, r9, "bridge_dsm_network_cb_restore");  // 恢复r9
    uc_reg_read(uc, UC_ARM_REG_R0, &ret);
    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    return ret;
}

/* Unlocked start_dsm body — caller already holds bridge mutex (e.g. inside br_exit). */
static int32_t bridge_dsm_mr_start_dsm_unlocked(uc_engine *uc, char *filename, char *ext,
                                                char *entry) {
    int32_t v;
    mr_start_dsm_param->filename = (char *)copyStrToMrp(filename);
    mr_start_dsm_param->ext = (char *)copyStrToMrp(ext);
    mr_start_dsm_param->entry = entry ? (char *)copyStrToMrp(entry) : NULL;

    gwy_ext_obs_start_dsm(filename, ext, entry);
    /* D5b observe-only: guest VA of entry string for PARAM_MAP/READ. */
    if (entry && mr_start_dsm_param->entry)
        gwy_ext_obs_launch_param_mapped((uint32_t)(uintptr_t)mr_start_dsm_param->entry, entry);

    v = bridge_mr_event(uc, MR_START_DSM, toMrpMemAddr(mr_start_dsm_param), 0);

    /*
     * START_DSM is one long host helper; R9/ER_RW restore queues init-seq mid-call.
     * Enter-path cannot see it; exit-path may race R9 leave. Deliver here once more
     * (no-op if already taken) before unlocking / returning to the host loop.
     */
    if (!g_in_ext_init_deliver)
        bridge_deliver_ext_init_seq(uc, 1u, "post_start_dsm");

    my_freeExt(getMrpMemPtr((uint32_t)mr_start_dsm_param->filename));
    mr_start_dsm_param->filename = NULL;

    my_freeExt(getMrpMemPtr((uint32_t)mr_start_dsm_param->ext));
    mr_start_dsm_param->ext = NULL;

    if (entry) {
        my_freeExt(getMrpMemPtr((uint32_t)mr_start_dsm_param->entry));
    }
    return v;
}

int32_t bridge_dsm_mr_start_dsm(uc_engine *uc, char *filename, char *ext, char *entry) {
    int32_t v;
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }

    v = bridge_dsm_mr_start_dsm_unlocked(uc, filename, ext, entry);

    if (pthread_mutex_unlock(&mutex) != 0) {
        perror(MUTEX_UNLOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    /* Outer start_dsm done — host loop can deliver MR_TIMER without deadlock. */
    printf("[PLATFORM_TIMER] op=START_DSM_RETURN filename=%s ret=%d evidence=DOCUMENTED\n",
           filename ? filename : "?", (int)v);
    fflush(stdout);
    gwy_ext_obs_on_start_dsm_return(filename, v);
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
    printf("[PLATFORM_TIMER] op=FIRE_ENTER evidence=DOCUMENTED\n");
    fflush(stdout);
    if (pthread_mutex_lock(&mutex) != 0) {
        perror(MUTEX_LOCK_FAIL);
        exit(EXIT_FAILURE);
    }
    printf("[PLATFORM_TIMER] op=FIRE_LOCKED evidence=DOCUMENTED\n");
    fflush(stdout);
    {
        int32_t v = bridge_deliver_timer_body(uc);
        printf("[PLATFORM_TIMER] op=FIRE_DONE ret=%d evidence=DOCUMENTED\n", (int)v);
        fflush(stdout);
        if (pthread_mutex_unlock(&mutex) != 0) {
            perror(MUTEX_UNLOCK_FAIL);
            exit(EXIT_FAILURE);
        }
        return v;
    }
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
