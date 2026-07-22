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

#ifdef GWY_USE_VM_FILE_SERVICE
#include "gwy_launcher/jjfb_bmp_meta.h"
#include "gwy_launcher/jjfb_plat_11f00.h"
#include "gwy_launcher/e10a_shell_trace.h"
#include "gwy_launcher/e10a31c_dispatch.h"
#include "gwy_launcher/e10a31d_provenance.h"
#include "gwy_launcher/e10a31e_appinfo.h"
#include "gwy_launcher/e10a31f_failsite.h"
#include "gwy_launcher/package_metadata.h"
#include "gwy_launcher/package_scope.h"
#include "gwy_launcher/gwy_sms_cfg.h"
/* E9V: declared before br_mr_drawBitmap; defined with e9h blit path below. */
static void jjfb_blit_sprite_with_optional_key(uint16_t *px, int x, int y, int w, int h,
                                               const char *member_or_handle);
#else
/* Plain build: dispatch atomicity is a no-op. */
typedef enum {
    GWY_DISPATCH_NONE = 0,
    GWY_DISPATCH_DSM_EVENT = 1,
    GWY_DISPATCH_EXT_HELPER = 2,
    GWY_DISPATCH_TIMER = 3,
    GWY_DISPATCH_PLATFORM_CALLBACK = 4,
    GWY_DISPATCH_INIT_SEQUENCE = 5
} GwyDispatchKind;
typedef enum {
    E10A31D_SRC_NATIVE_GUEST = 0,
    E10A31D_SRC_HOST_FAST_REAL = 1,
    E10A31D_SRC_TIMER = 2,
    E10A31D_SRC_PLATFORM_CALLBACK = 3
} E10a31dSource;
static inline int e10a31c_enabled(void) { return 0; }
static inline void e10a31c_enter(void *uc, GwyDispatchKind k, uint32_t h, uint32_t c, uint32_t p,
                                 uint32_t e) {
    (void)uc;
    (void)k;
    (void)h;
    (void)c;
    (void)p;
    (void)e;
}
static inline void e10a31c_leave(void *uc, GwyDispatchKind k) {
    (void)uc;
    (void)k;
}
static inline uint64_t e10a31c_init_tx_begin(void *uc, uint32_t h, uint32_t p, uint32_t e) {
    (void)uc;
    (void)h;
    (void)p;
    (void)e;
    return 0;
}
static inline void e10a31c_init_method_enter(void *uc, uint32_t m, uint32_t h, uint32_t p,
                                             uint32_t e) {
    (void)uc;
    (void)m;
    (void)h;
    (void)p;
    (void)e;
}
static inline void e10a31c_init_method_return(void *uc, uint32_t m, int32_t r) {
    (void)uc;
    (void)m;
    (void)r;
}
static inline void e10a31c_init_tx_end(void *uc, int c, const char *n) {
    (void)uc;
    (void)c;
    (void)n;
}
static inline void e10a31c_mem_get_enter(void *uc, uint32_t s) {
    (void)uc;
    (void)s;
}
static inline void e10a31c_mem_get_return(void *uc, uint32_t p, uint32_t l, int o) {
    (void)uc;
    (void)p;
    (void)l;
    (void)o;
}
static inline int e10a31c_unimpl_policy(void *uc, uint32_t s, const char *n, int32_t *o) {
    (void)uc;
    (void)s;
    (void)n;
    if (o) *o = -1;
    return 1;
}
static inline void e10a31c_note_last_bridge_api(const char *a) { (void)a; }
static inline void e10a31c_note_process_exit(int c, const char *r) {
    (void)c;
    (void)r;
}
static inline int e10a31d_enabled(void) { return 0; }
static inline void e10a31d_helper_enter(void *uc, E10a31dSource s, uint32_t h, uint32_t m,
                                        uint32_t p, uint32_t e, uint32_t in, uint32_t il,
                                        uint32_t cpc, uint32_t clr) {
    (void)uc;
    (void)s;
    (void)h;
    (void)m;
    (void)p;
    (void)e;
    (void)in;
    (void)il;
    (void)cpc;
    (void)clr;
}
static inline void e10a31d_helper_return(void *uc, uint32_t h, uint32_t m, int32_t r) {
    (void)uc;
    (void)h;
    (void)m;
    (void)r;
}
static inline void e10a31d_note_platform_api(void *uc, const char *a, uint32_t s, int32_t r) {
    (void)uc;
    (void)a;
    (void)s;
    (void)r;
}
static inline int e10a31e_enabled(void) { return 0; }
static inline void e10a31e_note_metadata(const char *p) { (void)p; }
static inline void e10a31e_note_binding(uint32_t g, uint32_t a, uint32_t v, int s) {
    (void)g;
    (void)a;
    (void)v;
    (void)s;
}
static inline void e10a31e_before_code8(void *uc, uint32_t h, uint32_t e, uint32_t a) {
    (void)uc;
    (void)h;
    (void)e;
    (void)a;
}
static inline void e10a31e_after_code6(void *uc, uint32_t e, uint32_t il, int32_t r) {
    (void)uc;
    (void)e;
    (void)il;
    (void)r;
}
static inline void e10a31e_after_code8(void *uc, uint32_t e, uint32_t a, int32_t r) {
    (void)uc;
    (void)e;
    (void)a;
    (void)r;
}
static inline void e10a31e_after_method0(void *uc, uint32_t h, int32_t r) {
    (void)uc;
    (void)h;
    (void)r;
}
static inline void e10a31e_read_proof_arm(void *uc, uint32_t e, uint32_t a) {
    (void)uc;
    (void)e;
    (void)a;
}
static inline void e10a31e_read_proof_disarm(void *uc) { (void)uc; }
static inline void e10a31e_note_ab_case(const char *n, int32_t a, int32_t b, int32_t c, uint32_t d,
                                        uint32_t e, uint32_t f, const char *g) {
    (void)n;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
}
static inline void e10a31e_mark_milestone(const char *n, const char *note) {
    (void)n;
    (void)note;
}
static inline uint32_t e10a31f_method0_input_override(uint32_t d) { return d; }
static inline int gwy_package_appinfo_resolve_id_ver(uint32_t *a, uint32_t *v, int *s) {
    if (a) *a = 0;
    if (v) *v = 0;
    if (s) *s = 0;
    return 0;
}
static inline void gwy_package_appinfo_binding_set(uint32_t g, uint32_t a, uint32_t v, int s) {
    (void)g;
    (void)a;
    (void)v;
    (void)s;
}
static inline int gwy_package_appinfo_binding_matches_active(uint32_t g,
                                                            int (*m)(uint32_t)) {
    (void)g;
    (void)m;
    return 0;
}
static inline const char *gwy_package_metadata_source_name(int s) {
    (void)s;
    return "NONE";
}
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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
/* E10A-2: after gbrwcore→gamelist continue, re-pin DSM R9 when nested MRP helpers return. */
static int g_e10a_shell_continued;
/* DSM cfunction helper/P saved before nested MRP retargets mr_extHelper_addr. */
static uint32_t g_dsm_extHelper_addr;
static uint32_t g_dsm_c_function_P_guest;
static mr_c_function_P_t *g_dsm_c_function_P;

static void bridge_note_dsm_helper(uint32_t helper, mr_c_function_P_t *p, uint32_t p_guest) {
    if (helper < CODE_ADDRESS || helper >= CODE_ADDRESS + CODE_SIZE) return;
    g_dsm_extHelper_addr = helper;
    if (p) g_dsm_c_function_P = p;
    if (p_guest) g_dsm_c_function_P_guest = p_guest;
}

static void bridge_restore_dsm_helper(const char *reason) {
    uint32_t old_h = mr_extHelper_addr;
    if (!g_dsm_extHelper_addr) return;
    mr_extHelper_addr = g_dsm_extHelper_addr;
    if (g_dsm_c_function_P_guest) {
        void *ph = getMrpMemPtr(g_dsm_c_function_P_guest);
        if (ph) mr_c_function_P = (mr_c_function_P_t *)ph;
    } else if (g_dsm_c_function_P) {
        mr_c_function_P = g_dsm_c_function_P;
    }
    printf("[JJFB_E10A_DSM_HELPER_RESTORE] old_helper=0x%X new_helper=0x%X P_guest=0x%X "
           "reason=%s evidence=TARGET_OBSERVED\n",
           old_h, mr_extHelper_addr, g_dsm_c_function_P_guest, reason ? reason : "?");
    fflush(stdout);
}

static int32_t bridge_mr_extHelper(uc_engine *uc, uint32_t code, uint32_t input, uint32_t input_len);
static int32_t bridge_ext_helper_call(uc_engine *uc, uint32_t helper, uint32_t p_guest,
                                      uint32_t code, uint32_t input, uint32_t input_len,
                                      uint32_t erw);

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

/* E10A-3.1e: package-scoped appInfo (keyed by package_id+generation). No global forever-cache. */
static int bridge_appinfo_guest_mapped(uint32_t guest_ptr) {
    return guest_ptr != 0u && getMrpMemPtr(guest_ptr) != NULL;
}

static uint32_t bridge_ext_appinfo_guest(void) {
    int32_t *host;
    uint32_t appid = 0, appver = 0;
    int src = 0;
    uint32_t guest;

#ifdef GWY_USE_VM_FILE_SERVICE
    /* Ensure active package metadata is loaded (e.g. gamelist after shell continue). */
    if (!gwy_package_registry_active_metadata()) {
        const char *pkg = package_scope_active_package();
        if (pkg) (void)gwy_package_metadata_activate(pkg);
    }
    e10a31e_note_metadata("before_appinfo_bind");
#endif

    if (!gwy_package_appinfo_resolve_id_ver(&appid, &appver, &src)) {
#ifdef GWY_USE_VM_FILE_SERVICE
        /* Product path: refuse env-only silent fill. Binding broken. */
        e10a31e_mark_milestone("ACTIVE_PACKAGE_METADATA_BINDING_BROKEN", "resolve_id_ver");
        printf("[GWY_APPINFO_BIND] FAIL reason=no_active_metadata evidence=OBSERVED\n");
        fflush(stdout);
        return 0u;
#else
        {
            const char *aid = getenv("GWY_PACKAGE_APPID");
            const char *aver = getenv("GWY_PACKAGE_APPVER");
            if (aid && aid[0]) appid = (uint32_t)strtoul(aid, NULL, 10);
            if (aver && aver[0]) appver = (uint32_t)strtoul(aver, NULL, 10);
        }
#endif
    }

#ifdef GWY_USE_VM_FILE_SERVICE
    {
        const GwyPackageAppInfoBinding *bind = gwy_package_appinfo_binding_active();
        if (bind && bind->valid && bind->appid == appid && bind->appver == appver &&
            gwy_package_appinfo_binding_matches_active(bind->appinfo_guest,
                                                       bridge_appinfo_guest_mapped)) {
            return bind->appinfo_guest;
        }
    }
#endif

    host = (int32_t *)my_mallocExt(16u);
    if (!host) return 0u;
    memset(host, 0, 16u);
    host[0] = (int32_t)appid;
    host[1] = (int32_t)appver;
    /* host[2]=sidName ptr 0; host[3]=ram 0 — do not invent */
    guest = toMrpMemAddr(host);
    gwy_package_appinfo_binding_set(guest, appid, appver, src);
#ifdef GWY_USE_VM_FILE_SERVICE
    e10a31e_note_binding(guest, appid, appver, src);
#else
    printf("[GWY_APPINFO_BIND] source=%s appid=%u appver=%u guest_ptr=0x%X sidName=0x0 ram=0\n",
           gwy_package_metadata_source_name(src), appid, appver, guest);
    fflush(stdout);
#endif
    return guest;
}

/* Deliver DOCUMENTED 6→8→0 once; caller must hold g_in_ext_init_deliver=0. */
static void bridge_deliver_ext_init_seq(uc_engine *uc, uint32_t before_or_after_code,
                                        const char *when) {
    int32_t r6 = MR_FAILED, r8 = MR_FAILED, r0 = MR_FAILED;
    uint32_t appinfo;
    uint32_t ver = GWY_EXT_INIT_MR_VERSION;
    uint32_t helper = mr_extHelper_addr;
    uint32_t saved_helper = mr_extHelper_addr;
    uint32_t erw = 0;
    uint32_t p_guest = 0;
    int tx = 0;

    /* After shell continue: gamelist helper + own ERW (not sticky gbrwcore P/R9). */
    if (g_e10a_shell_continued) {
        uint32_t gh = gwy_ext_obs_module_helper_by_name("gamelist");
        erw = gwy_ext_obs_module_erw_by_name("gamelist");
        p_guest = mr_c_function_P ? toMrpMemAddr(mr_c_function_P) : 0;
        if (gh) helper = gh;
        if (erw && p_guest) {
            void *ph = getMrpMemPtr(p_guest);
            if (ph) {
                uint32_t *words = (uint32_t *)ph;
                words[0] = erw;
                words[1] = 0xA34u; /* gamelist DOCUMENTED ER_RW len */
            }
        }
        /* E10A-3.1e: appInfo must come from gamelist MRP header, not prior package. */
#ifdef GWY_USE_VM_FILE_SERVICE
        (void)package_scope_set_active("gwy/gamelist.mrp");
        (void)gwy_package_metadata_activate("gwy/gamelist.mrp");
#endif
    }
    if (!helper) return;
    if (!gwy_ext_obs_take_ext_init_seq()) return;
    mr_extHelper_addr = helper;
    appinfo = bridge_ext_appinfo_guest();
    g_in_ext_init_deliver = 1;
    printf("[JJFB_INIT_SEQ] action=deliver_%s_code_%u helper=0x%X version=%u appinfo=0x%X "
           "erw=0x%X P=0x%X evidence=DOCUMENTED source=mythroad_mini.c:mr_doExt\n",
           when, before_or_after_code, helper, ver, appinfo, erw, p_guest);
    fflush(stdout);

    if (g_e10a_shell_continued && erw && p_guest) {
#ifdef GWY_USE_VM_FILE_SERVICE
        /* DSM SMSCFG must be published before method0 GPT gate. */
        (void)gwy_sms_cfg_ensure_ready(uc);
#endif
        /* Explicit R9=erw; avoid sticky gbrwcore R9 from bridge_mr_extHelper scope.
         * E10A-3.1c: one atomic transaction; no timer pump between 6/8/0. */
        (void)e10a31c_init_tx_begin(uc, helper, p_guest, erw);
        tx = 1;
        e10a31c_init_method_enter(uc, 6u, helper, p_guest, erw);
        r6 = bridge_ext_helper_call(uc, helper, p_guest, 6u, 0u, ver, erw);
        e10a31c_init_method_return(uc, 6u, r6);
        e10a31e_after_code6(uc, erw, ver, r6);
        if (r6 < 0) {
            goto init_done;
        }
        e10a31e_before_code8(uc, helper, erw, appinfo);
        e10a31c_init_method_enter(uc, 8u, helper, p_guest, erw);
        r8 = bridge_ext_helper_call(uc, helper, p_guest, 8u, appinfo, 16u, erw);
        e10a31c_init_method_return(uc, 8u, r8);
        e10a31e_after_code8(uc, erw, appinfo, r8);
        if (r8 < 0) {
            goto init_done;
        }
        e10a31e_read_proof_arm(uc, erw, appinfo);
        e10a31c_init_method_enter(uc, 0u, helper, p_guest, erw);
        {
            uint32_t m0_input = e10a31f_method0_input_override(0u);
            r0 = bridge_ext_helper_call(uc, helper, p_guest, 0u, m0_input, ver, erw);
        }
        e10a31c_init_method_return(uc, 0u, r0);
        e10a31e_read_proof_disarm(uc);
        e10a31e_after_method0(uc, helper, r0);
        {
            const char *ab = getenv("JJFB_E10A31E_AB_CASE");
            uint32_t aid = 0, aver = 0;
            int32_t *hp = appinfo ? (int32_t *)getMrpMemPtr(appinfo) : NULL;
            if (hp) {
                aid = (uint32_t)hp[0];
                aver = (uint32_t)hp[1];
            }
            e10a31e_note_ab_case(ab && ab[0] ? ab : "metadata", r6, r8, r0, aid, aver, 0u, "");
        }
    } else {
        r6 = bridge_mr_extHelper(uc, 6u, 0u, ver);
        r8 = bridge_mr_extHelper(uc, 8u, appinfo, 16u);
        r0 = bridge_mr_extHelper(uc, 0u, 0u, ver);
    }
init_done:
    printf("[JJFB_INIT_SEQ] delivered ret6=%d ret8=%d ret0=%d evidence=OBSERVED\n", (int)r6,
           (int)r8, (int)r0);
    fflush(stdout);
    g_in_ext_init_deliver = 0;
    mr_extHelper_addr = saved_helper;
    /* End tx after flag clear so deferred timer pumps outside init deliver. */
    if (tx) {
        int ok = (r6 >= 0 && r8 >= 0 && r0 >= 0);
        e10a31c_init_tx_end(uc, ok, ok ? "ret6_ret8_ret0"
                                       : (r6 < 0   ? "method6_failed"
                                          : r8 < 0 ? "method8_failed"
                                                   : "method0_failed"));
    }
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
    bridge_note_dsm_helper(p_f, mr_c_function_P, v);
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
/* E9C: present a larger original MRP UI member through the same guiDrawBitmapSprite
 * path that mr_drawBitmap uses (no invented pixels, no host-only paint API). */
static int e9c_parse_wh_from_name(const char *name, int *w, int *h) {
    const char *p, *q;
    int ww = 0, hh = 0;
    if (!name || !w || !h) return 0;
    p = strchr(name, '!');
    if (!p) return 0;
    ww = atoi(p + 1);
    q = strchr(p + 1, '!');
    if (!q) return 0;
    hh = atoi(q + 1);
    if (ww <= 0 || hh <= 0 || ww > SCREEN_WIDTH || hh > SCREEN_HEIGHT) return 0;
    *w = ww;
    *h = hh;
    return 1;
}

static int e9c_load_rgb565_file(const char *path, uint16_t *dst, int cap_bytes, int *out_n) {
    FILE *fp;
    long sz;
    if (!path || !path[0] || !dst || cap_bytes < 2) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    sz = ftell(fp);
    if (sz <= 0 || sz > cap_bytes || (sz & 1)) {
        fclose(fp);
        return 0;
    }
    rewind(fp);
    if ((long)fread(dst, 1, (size_t)sz, fp) != sz) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    if (out_n) *out_n = (int)sz;
    return 1;
}

static void e9c_present_meaningful_ui(void) {
    static int s_done;
    const char *mode;
    const char *cand;
    const char *path;
    const char *sheet;
    uint16_t *buf;
    int w = 0, h = 0, nbytes = 0, x, y;
    if (s_done) return;
    mode = getenv("JJFB_E9C_MODE");
    if (!mode || mode[0] != '1') return;
    buf = (uint16_t *)malloc(240 * 320 * 2);
    if (!buf) return;

    sheet = getenv("JJFB_E9C_CONTACTSHEET");
    if (sheet && sheet[0] == '1') {
        /* Tile several original UI members via real sprite blit API. */
        struct {
            const char *name;
            const char *path;
            int x, y;
        } items[] = {
            {"slogo!157!58.bmp", "out/e9c_resources/slogo_157_58.bmp.rgb565", 41, 40},
            {"loadingbar!201!29.bmp", "out/e9c_resources/loadingbar_201_29.bmp.rgb565", 19, 120},
            {"textbar!120!30.bmp", "out/e9c_resources/textbar_120_30.bmp.rgb565", 60, 170},
            {"top!76!28.bmp", "out/e9c_resources/top_76_28.bmp.rgb565", 82, 220},
            {NULL, NULL, 0, 0},
        };
        int i, drawn = 0;
        for (i = 0; items[i].name; i++) {
            const char *p = getenv(i == 0   ? "JJFB_E9C_PATH_SLOGO"
                                  : i == 1 ? "JJFB_E9C_PATH_LOADINGBAR"
                                  : i == 2 ? "JJFB_E9C_PATH_TEXTBAR"
                                           : "JJFB_E9C_PATH_TOP");
            if (!p || !p[0]) p = items[i].path;
            if (!e9c_parse_wh_from_name(items[i].name, &w, &h)) continue;
            if (!e9c_load_rgb565_file(p, buf, 240 * 320 * 2, &nbytes)) continue;
            if (nbytes < w * h * 2) continue;
            guiDrawBitmapSprite(buf, items[i].x, items[i].y, w, h);
            printf("[JJFB_E9C_CONTACTSHEET_DRAW] name=\"%s\" x=%d y=%d w=%d h=%d "
                   "bytes=%d via=guiDrawBitmapSprite_from_mr_drawBitmap_hook "
                   "evidence=OBSERVED\n",
                   items[i].name, items[i].x, items[i].y, w, h, nbytes);
            drawn++;
        }
        if (drawn > 0) {
            s_done = 1;
            printf("[JJFB_VISIBLE_UI_FRAME_PRESENTED] mode=contactsheet tiles=%d "
                   "note=original_jjfb_mrp_members NOT_PRODUCT evidence=OBSERVED\n",
                   drawn);
            printf("[JJFB_E9C_CLASS] class=VISIBLE_CONTACTSHEET_FROM_REAL_RESOURCES "
                   "evidence=OBSERVED\n");
            fflush(stdout);
            /* Multi-tile path deferred HWND hold until all original sprites blitted. */
            guiVisibleWindowFinalize();
        }
        free(buf);
        return;
    }

    cand = getenv("JJFB_E9C_CANDIDATE_MEMBER");
    if (!cand || !cand[0]) cand = "slogo!157!58.bmp";
    path = getenv("JJFB_E9C_CANDIDATE_PATH");
    if (!path || !path[0]) path = "out/e9c_resources/slogo_157_58.bmp.rgb565";
    if (!e9c_parse_wh_from_name(cand, &w, &h)) {
        free(buf);
        return;
    }
    if (!e9c_load_rgb565_file(path, buf, 240 * 320 * 2, &nbytes) || nbytes < w * h * 2) {
        printf("[JJFB_E9C_CANDIDATE] load_fail path=%s name=\"%s\" evidence=OBSERVED\n", path,
               cand);
        fflush(stdout);
        free(buf);
        return;
    }
    x = (SCREEN_WIDTH - w) / 2;
    y = (SCREEN_HEIGHT - h) / 3;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    guiDrawBitmapSprite(buf, x, y, w, h);
    s_done = 1;
    printf("[JJFB_E9C_CANDIDATE_DRAW] name=\"%s\" path=%s x=%d y=%d w=%d h=%d bytes=%d "
           "via=guiDrawBitmapSprite_from_mr_drawBitmap_hook note=original_mrp_pixels "
           "NOT_PRODUCT evidence=OBSERVED\n",
           cand, path, x, y, w, h, nbytes);
    printf("[JJFB_VISIBLE_UI_FRAME_PRESENTED] mode=candidate name=\"%s\" w=%d h=%d "
           "evidence=OBSERVED\n",
           cand, w, h);
    if (w * h > 11 * 11)
        printf("[JJFB_E9C_CLASS] class=MEANINGFUL_VISIBLE_UI_FRAME name=\"%s\" "
               "evidence=OBSERVED\n",
               cand);
    fflush(stdout);
    free(buf);
}

static void br_mr_drawBitmap(BridgeMap *o, uc_engine *uc) {
    // typedef void (*T_mr_drawBitmap)(uint16* bmp, int16 x, int16 y, uint16 w, uint16 h);
    uint32_t bmp, x, y, w, h;
    int sprite_blit = 0;
    const char *e8z = getenv("JJFB_E8Z_MODE");
    const char *real = getenv("JJFB_FAST_REAL_BMP_HANDLE");
    const char *e9c = getenv("JJFB_E9C_MODE");

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
    {
        uint32_t pc = 0, lr = 0;
        uc_reg_read(uc, UC_ARM_REG_PC, &pc);
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        printf("[JJFB_E8U_DRAW] api=mr_drawBitmap pc=0x%X lr=0x%X r0=0x%X r1=%d r2=%d r3=%u "
               "h=%u sp=0x%X surface=0x%X note=real_platform_draw evidence=OBSERVED\n",
               pc, lr, bmp, (int)x, (int)y, w, h, sp, bmp);
        printf("[JJFB_FIRST_REAL_DRAW_CANDIDATE] api=mr_drawBitmap pc=0x%X lr=0x%X "
               "r0=0x%X r1=%d r2=%d r3=%u h=%u note=real_platform_draw evidence=OBSERVED\n",
               pc, lr, bmp, (int)x, (int)y, w, h);
    }
    if (!bmp) {
        printf("[JJFB_E8Z_CLASS] class=DRAW_API_WITH_NULL_BMP evidence=OBSERVED\n");
        fflush(stdout);
        return;
    }
    /* E9C: once real draw API is reached, present meaningful original UI member(s). */
    if (e9c && e9c[0] == '1')
        e9c_present_meaningful_ui();
    /* E8Z: 0x310BBC passes handle+4 sprite pixels (pitch=w), not full LCD.
     * Pixels live in Unicorn-mapped scratch (0x3920000); getMrpMemPtr cannot
     * translate that — read via uc_mem_read into a host buffer. */
    if (((e8z && e8z[0] == '1') || (real && real[0] == '1')) &&
        bmp >= 0x3920000u && bmp < 0x3960000u && w > 0 && w <= 240 && h > 0 && h <= 320)
        sprite_blit = 1;
    if (bmp)
        printf("[JJFB_FIRST_REAL_FRAME_CANDIDATE] bmp=0x%X x=%d y=%d w=%u h=%u "
               "sprite_blit=%d evidence=OBSERVED\n",
               bmp, (int)x, (int)y, w, h, sprite_blit);
    printf("[JJFB_REFRESH] api=mr_drawBitmap note=mrc_refreshScreen_path evidence=DOCUMENTED\n");
    fflush(stdout);
    if (sprite_blit) {
        /* Full LCD RGB565 headroom for E9F UI members (loadingbar/slogo/…). */
        uint16_t host_pix[240 * 320];
        uint32_t nbytes;
        uc_err ue;
        /* E9E/E9F/E9G: guest blit may pass w==h==width for wide bars; prefer
         * per-bmp meta (original member w/h), then LAST_WH, then guest args. */
        {
            uint16_t mw = 0, mh = 0;
            char member[96];
            unsigned cw = 0, ch = 0;
            int got = 0;
            memset(member, 0, sizeof(member));
#ifdef GWY_USE_VM_FILE_SERVICE
            if (jjfb_bmp_meta_get(bmp, &mw, &mh, member, sizeof(member)) && mw > 0 &&
                mh > 0 && mw <= 240 && mh <= 320) {
                printf("[JJFB_DRAW_DIM_FIX] bmp=0x%X guest=%ux%u real=%ux%u member=%s "
                       "note=per_bmp_meta NOT_PRODUCT evidence=OBSERVED\n",
                       bmp, w, h, (unsigned)mw, (unsigned)mh,
                       member[0] ? member : "?");
                fflush(stdout);
                w = mw;
                h = mh;
                got = 1;
            }
#endif
            if (!got) {
                const char *whs = getenv("JJFB_E9E_LAST_WH");
                if (whs && whs[0] && sscanf(whs, "%ux%u", &cw, &ch) == 2 && cw > 0 &&
                    ch > 0 && cw <= 240 && ch <= 320) {
                    if (w != cw || h != ch) {
                        printf("[JJFB_DRAW_DIM_FIX] bmp=0x%X guest=%ux%u real=%ux%u "
                               "member=(LAST_WH) note=env_fallback NOT_PRODUCT "
                               "evidence=OBSERVED\n",
                               bmp, w, h, cw, ch);
                        fflush(stdout);
                        w = cw;
                        h = ch;
                    }
                }
            }
        }
        nbytes = (uint32_t)w * (uint32_t)h * 2u;
        if (nbytes > sizeof(host_pix)) {
            printf("[JJFB_E8Z_CLASS] class=BMP_DECODE_FAILED note=sprite_too_large "
                   "w=%u h=%u evidence=OBSERVED\n",
                   w, h);
            fflush(stdout);
            return;
        }
        memset(host_pix, 0, nbytes);
        ue = uc_mem_read(uc, bmp, host_pix, nbytes);
        if (ue != UC_ERR_OK) {
            size_t got = 0;
#ifdef GWY_USE_VM_FILE_SERVICE
            got = jjfb_bmp_meta_copy_pixels(bmp, host_pix, sizeof(host_pix));
#endif
            if (got < nbytes) {
                printf("[JJFB_E8Z_CLASS] class=BMP_DECODE_FAILED note=uc_mem_read_fail "
                       "uc_err=%u bmp=0x%X host_cache=%u evidence=OBSERVED\n",
                       (unsigned)ue, bmp, (unsigned)got);
                fflush(stdout);
                return;
            }
            printf("[JJFB_E8Z_PIXEL_READ] bmp=0x%X bytes=%u via=host_pixel_cache "
                   "uc_err=%u evidence=OBSERVED\n",
                   bmp, (unsigned)got, (unsigned)ue);
            fflush(stdout);
        } else {
            printf("[JJFB_E8Z_PIXEL_READ] bmp=0x%X bytes=%u first=0x%04X evidence=OBSERVED\n",
                   bmp, nbytes, (unsigned)host_pix[0]);
            fflush(stdout);
        }
        /* If E9C already presented a meaningful frame, skip tiny guest sprite overlay. */
        if (!(e9c && e9c[0] == '1' && getenv("JJFB_E9C_SKIP_TINY") &&
              getenv("JJFB_E9C_SKIP_TINY")[0] == '1' && w <= 16 && h <= 16)) {
#ifdef GWY_USE_VM_FILE_SERVICE
            /* E9V: magenta key transparency on generic sprite path (not text-only). */
            {
                char member_tag[96];
                char handle_tag[48];
                uint16_t mw = 0, mh = 0;
                const char *tag;
                memset(member_tag, 0, sizeof(member_tag));
                (void)jjfb_bmp_meta_get(bmp, &mw, &mh, member_tag, sizeof(member_tag));
                if (member_tag[0]) {
                    tag = member_tag;
                } else {
                    snprintf(handle_tag, sizeof(handle_tag), "bmp@0x%X", bmp);
                    tag = handle_tag;
                }
                jjfb_blit_sprite_with_optional_key(host_pix, (int)x, (int)y, (int)w, (int)h, tag);
            }
#else
            guiDrawBitmapSprite(host_pix, (int32_t)x, (int32_t)y, (int32_t)w, (int32_t)h);
#endif
        }
    } else {
        void *host = getMrpMemPtr(bmp);
        if (!host) {
            printf("[JJFB_E8Z_CLASS] class=DRAW_API_WITH_NULL_BMP note=getMrpMemPtr_null "
                   "bmp=0x%X evidence=OBSERVED\n",
                   bmp);
            fflush(stdout);
            return;
        }
        guiDrawBitmap(host, x, y, w, h);
    }
}

/* Observe-only graphics stubs: log real guest calls; no host fake paint. */
static uint16_t jjfb_rgb565_simple(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void jjfb_compat_draw_char_block(int32_t x, int32_t y, uint16_t ch, uint16_t fg) {
    uint16_t pix[12 * 16];
    int i, j, gw = (ch >= 128u) ? 12 : 8;
    int gh = 12;
    (void)memset(pix, 0, sizeof(pix));
    for (j = 0; j < gh; j++) {
        for (i = 0; i < gw; i++) {
            if (((i + j + (ch & 0xFF)) & 3) != 0) pix[j * 12 + i] = fg;
        }
    }
    guiDrawBitmapSprite(pix, x, y, gw, gh);
}

/* E9N: platform mr_platDrawChar compat — only when JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM=1. */
static void br_mr_platDrawChar_compat(BridgeMap *o, uc_engine *uc) {
    uint32_t ch = 0, x = 0, y = 0, color = 0;
    static uint32_t n;
    const char *shim = getenv("JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM");
    uint16_t fg;
    (void)o;
    if (!shim || shim[0] != '1') return;
    uc_reg_read(uc, UC_ARM_REG_R0, &ch);
    uc_reg_read(uc, UC_ARM_REG_R1, &x);
    uc_reg_read(uc, UC_ARM_REG_R2, &y);
    uc_reg_read(uc, UC_ARM_REG_R3, &color);
    n++;
    fg = (uint16_t)(color & 0xFFFFu);
    if (!fg) fg = 0xFFFFu;
    jjfb_compat_draw_char_block((int32_t)x, (int32_t)y, (uint16_t)(ch & 0xFFFFu), fg);
    if (n <= 16 || (n % 64) == 0) {
        printf("[JJFB_PLATFORM_TEXT_DRAW_COMPAT] api=mr_platDrawChar #%u ch=0x%X @%d,%d "
               "color=0x%X NOT_PRODUCT evidence=OBSERVED\n",
               n, ch & 0xFFFFu, (int)x, (int)y, color);
        fflush(stdout);
    }
}

static void br_observe_draw_text(BridgeMap *o, uc_engine *uc) {
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, pc = 0, lr = 0, sp = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    printf("[JJFB_DRAW] api=%s r0=0x%X r1=0x%X r2=0x%X r3=0x%X evidence=OBSERVED\n",
           o && o->name ? o->name : "drawText", r0, r1, r2, r3);
    printf("[JJFB_FIRST_REAL_DRAW_CANDIDATE] api=%s pc=0x%X lr=0x%X r0=0x%X r1=0x%X "
           "r2=0x%X r3=0x%X sp=0x%X note=real_platform_draw evidence=OBSERVED\n",
           o && o->name ? o->name : "drawText", pc, lr, r0, r1, r2, r3, sp);
    {
        const char *shim = getenv("JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM");
        if (shim && shim[0] == '1' && r0) {
            uint8_t buf[96];
            int i = 0, cx = (int)(int16_t)(r1 & 0xFFFFu), cy = (int)(int16_t)(r2 & 0xFFFFu);
            uint16_t fg = jjfb_rgb565_simple(255, 255, 255);
            memset(buf, 0, sizeof(buf));
            if (uc_mem_read(uc, r0, buf, sizeof(buf) - 1) == UC_ERR_OK) {
                while (i < (int)sizeof(buf) - 1 && buf[i]) {
                    uint16_t ch;
                    if (buf[i] >= 0x81u && i + 1 < (int)sizeof(buf) - 1 && buf[i + 1]) {
                        ch = (uint16_t)(((uint16_t)buf[i] << 8) | buf[i + 1]);
                        i += 2;
                    } else {
                        ch = (uint16_t)buf[i++];
                    }
                    jjfb_compat_draw_char_block(cx, cy, ch, fg);
                    cx += (ch >= 128u) ? 12 : 8;
                }
                printf("[JJFB_PLATFORM_TEXT_DRAW_COMPAT] api=%s str=0x%X @%d,%d "
                       "NOT_PRODUCT evidence=OBSERVED\n",
                       o && o->name ? o->name : "drawText", r0, (int)(int16_t)(r1 & 0xFFFFu),
                       (int)(int16_t)(r2 & 0xFFFFu));
            }
        }
    }
    fflush(stdout);
}

static void br_observe_draw_rect(BridgeMap *o, uc_engine *uc) {
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, pc = 0, lr = 0, sp = 0, a4 = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_mem_read(uc, sp, &a4, 4);
    printf("[JJFB_DRAW] api=%s x=%d y=%d w=%u h=%u evidence=OBSERVED\n",
           o && o->name ? o->name : "DrawRect", (int)r0, (int)r1, r2, r3);
    printf("[JJFB_FIRST_REAL_DRAW_CANDIDATE] api=%s pc=0x%X lr=0x%X r0=0x%X r1=0x%X "
           "r2=0x%X r3=0x%X sp0=0x%X note=real_platform_draw evidence=OBSERVED\n",
           o && o->name ? o->name : "DrawRect", pc, lr, r0, r1, r2, r3, a4);
    fflush(stdout);
}

static void br_observe_disp_up(BridgeMap *o, uc_engine *uc) {
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, pc = 0, lr = 0;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    printf("[JJFB_REFRESH] api=%s r0=0x%X r1=0x%X r2=0x%X r3=0x%X evidence=OBSERVED\n",
           o && o->name ? o->name : "_DispUpEx", r0, r1, r2, r3);
    printf("[JJFB_FIRST_REAL_DRAW_CANDIDATE] api=%s pc=0x%X lr=0x%X r0=0x%X r1=0x%X "
           "r2=0x%X r3=0x%X note=real_refresh evidence=OBSERVED\n",
           o && o->name ? o->name : "_DispUpEx", pc, lr, r0, r1, r2, r3);
    fflush(stdout);
}

static void br_mr_load_sms_cfg(BridgeMap *o, uc_engine *uc) {
    int32_t ret;
    (void)o;
#ifdef GWY_USE_VM_FILE_SERVICE
    ret = gwy_sms_cfg_load(uc);
#else
    (void)uc;
    ret = MR_FAILED;
#endif
    SET_RET_V((uint32_t)ret);
}

static void br_mr_save_sms_cfg(BridgeMap *o, uc_engine *uc) {
    int32_t ret;
    (void)o;
#ifdef GWY_USE_VM_FILE_SERVICE
    ret = gwy_sms_cfg_save(uc);
#else
    (void)uc;
    ret = MR_FAILED;
#endif
    SET_RET_V((uint32_t)ret);
}

static void br_mr_open(BridgeMap *o, uc_engine *uc) {
    // typedef int32 (*T_mr_open)(const char* filename,  uint32 mode);
    uint32_t filename, mode;
    uc_reg_read(uc, UC_ARM_REG_R0, &filename);
    uc_reg_read(uc, UC_ARM_REG_R1, &mode);
#ifdef GWY_USE_VM_FILE_SERVICE
    /* Capture live Unicorn regs at the real file API boundary before VFS. */
    if (e10a_shell_trace_enabled()) {
        e10a_vfs_set_ctx_from_uc(uc, o && o->name ? o->name : "mr_open", "bridge_mr_open",
                                 NULL, filename, "R0");
    }
#endif
    /* R0==0 is not a guest path pointer; getMrpMemPtr(0) yields host garbage (e.g. "\b"). */
    if (!filename) {
#ifdef GWY_USE_VM_FILE_SERVICE
        if (e10a_shell_trace_enabled()) {
            uint32_t pc0 = 0, lr0 = 0, r4 = 0, r9 = 0, sp = 0;
            uint32_t obj = 0, field0 = 0, field8 = 0, erw1880 = 0, erw12e4 = 0;
            uint8_t f0b[32];
            char f0hex[80];
            int i;
            uc_reg_read(uc, UC_ARM_REG_PC, &pc0);
            uc_reg_read(uc, UC_ARM_REG_LR, &lr0);
            uc_reg_read(uc, UC_ARM_REG_R4, &r4);
            uc_reg_read(uc, UC_ARM_REG_R9, &r9);
            uc_reg_read(uc, UC_ARM_REG_SP, &sp);
            printf("[JJFB_E10A_OPEN_NULL] pc=0x%X lr=0x%X r1=0x%X r4=0x%X r9=0x%X sp=0x%X "
                   "evidence=OBSERVED\n",
                   pc0, lr0, mode, r4, r9, sp);
            fflush(stdout);
            {
                uint32_t dsm_erw = 0;
                uint32_t slot2084 = 0, fn_at4 = 0;
                if (mr_c_function_P)
                    dsm_erw = (uint32_t)(uintptr_t)mr_c_function_P->start_of_ER_RW;
                if (r9) {
                    uc_mem_read(uc, r9 + 0x2084u, &slot2084, 4);
                    if (slot2084) uc_mem_read(uc, slot2084 + 4u, &fn_at4, 4);
                }
                printf("[JJFB_E10A_OPEN_NULL_R9] r9=0x%X dsm_erw=0x%X match=%s "
                       "[r9+0x2084]=0x%X [that+4]=0x%X evidence=OBSERVED\n",
                       r9, dsm_erw, (dsm_erw && r9 == dsm_erw) ? "yes" : "no", slot2084, fn_at4);
                fflush(stdout);
            }
            e10a_dump_open_enter_branches("mr_open_r0_null");
            e10a_dump_guest_pc_ring("mr_open_r0_null");
            {
                uint32_t ri;
                uint32_t rv[13];
                for (ri = 0; ri < 13u; ri++) {
                    uc_reg_read(uc, UC_ARM_REG_R0 + (int)ri, &rv[ri]);
                    if ((rv[ri] & ~1u) == (pc0 & ~1u)) {
                        printf("[JJFB_E10A_OPEN_NULL_BXREG] rm=%u val=0x%X stub=0x%X "
                               "note=reg_still_holds_open_stub evidence=OBSERVED\n",
                               ri, rv[ri], pc0);
                        fflush(stdout);
                    }
                }
            }
            {
                uint32_t call_pc = (lr0 & ~1u) >= 4u ? ((lr0 & ~1u) - 4u) : 0;
                uint8_t ib[8];
                uint8_t ib2[8];
                uint8_t *host;
                memset(ib, 0, sizeof(ib));
                memset(ib2, 0, sizeof(ib2));
                if (call_pc) {
                    uc_err e1 = uc_mem_read(uc, call_pc, ib, sizeof(ib));
                    host = (uint8_t *)getMrpMemPtr(call_pc);
                    printf("[JJFB_E10A_OPEN_NULL_CALLSITE] lr=0x%X call_pc=0x%X uc_err=%d "
                           "uc_bytes=%02X%02X%02X%02X%02X%02X%02X%02X "
                           "host=%p host_bytes=%02X%02X%02X%02X%02X%02X%02X%02X evidence=OBSERVED\n",
                           lr0, call_pc, (int)e1, ib[0], ib[1], ib[2], ib[3], ib[4], ib[5],
                           ib[6], ib[7], (void *)host,
                           host ? host[0] : 0, host ? host[1] : 0, host ? host[2] : 0,
                           host ? host[3] : 0, host ? host[4] : 0, host ? host[5] : 0,
                           host ? host[6] : 0, host ? host[7] : 0);
                    fflush(stdout);
                }
                (void)ib2;
            }
            memset(f0b, 0, sizeof(f0b));
            f0hex[0] = 0;
            if (r4) uc_mem_read(uc, r4, &obj, 4);
            if (obj) {
                uc_mem_read(uc, obj, &field0, 4);
                uc_mem_read(uc, obj + 8, &field8, 4);
            }
            if (r9) {
                uc_mem_read(uc, r9 + 0x1880u, &erw1880, 4);
                uc_mem_read(uc, r9 + 0x12E4u, &erw12e4, 4);
            }
            if (field0) uc_mem_read(uc, field0, f0b, sizeof(f0b));
            for (i = 0; i < 16; i++) {
                char t[4];
                snprintf(t, sizeof(t), "%02X", f0b[i]);
                strncat(f0hex, t, sizeof(f0hex) - strlen(f0hex) - 1);
            }
            printf("[JJFB_E10A_OPEN_NULL_DETAIL] obj=0x%X field0=0x%X field8=0x%X "
                   "erw+1880=0x%X erw+12e4=0x%X field0_bytes=%s evidence=OBSERVED\n",
                   obj, field0, field8, erw1880, erw12e4, f0hex);
            fflush(stdout);
            e10a_shell_cfg_runtime("mr_open_r0_null", r4, obj, field0, "",
                                   field0 ? (const char *)f0b : "(null)", "", erw1880, f0hex,
                                   "R0=0 R1=erw_or_mode", "SHELL_VFS_BADPATH_ORIGIN_FOUND");
            e10a_shell_vfs("miss", "bridge_mr_open", "(null)", "", 0, -1, pc0, lr0);
        }
#endif
        LOG("ext call %s(NULL, 0x%X): reject\n", o->name, mode);
        SET_RET_V(0);
        return;
    }
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
#ifdef GWY_USE_VM_FILE_SERVICE
    if (e10a_shell_trace_enabled()) {
        e10a_vfs_set_ctx_from_uc(uc, o && o->name ? o->name : "mr_getLen", "bridge_mr_getLen",
                                 NULL, filename, "R0");
    }
#endif
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
                bridge_note_dsm_helper(helper, mr_c_function_P, p_addr);
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
        if (strstr(str, "font load suc")) gwy_ext_obs_e10a31a_note_font_load(uc);
        puts(str);
    }
}

/* Guest mr_printf was MAP_FUNC NULL → process exit; stub so mrc_init/cfg can continue. */
static void br_mr_printf(BridgeMap *o, uc_engine *uc) {
    uint32_t fmt_a = 0, a1 = 0, a2 = 0, a3 = 0;
    const char *fmt;
    char buf[512];
    int n = 0;
    (void)o;
    uc_reg_read(uc, UC_ARM_REG_R0, &fmt_a);
    uc_reg_read(uc, UC_ARM_REG_R1, &a1);
    uc_reg_read(uc, UC_ARM_REG_R2, &a2);
    uc_reg_read(uc, UC_ARM_REG_R3, &a3);
    fmt = fmt_a ? (const char *)getMrpMemPtr(fmt_a) : "(null)";
    if (fmt && strstr(fmt, "%s")) {
        const char *s1 = a1 ? (const char *)getMrpMemPtr(a1) : "(null)";
        const char *s2 = a2 ? (const char *)getMrpMemPtr(a2) : "(null)";
        n = snprintf(buf, sizeof(buf), fmt, s1, s2, a3);
    } else if (fmt) {
        n = snprintf(buf, sizeof(buf), fmt, a1, a2, a3);
    }
    printf("[guest-printf] ");
    if (n > 0) {
        fwrite(buf, 1, (size_t)((n < (int)sizeof(buf)) ? n : (int)sizeof(buf) - 1), stdout);
        if (buf[(n < (int)sizeof(buf) ? n : (int)sizeof(buf)) - 1] != '\n') fputc('\n', stdout);
    } else {
        printf("fmt=%s a1=0x%X a2=0x%X a3=0x%X\n", fmt ? fmt : "(null)", a1, a2, a3);
    }
    fflush(stdout);
}

static void br_sprintf(BridgeMap *o, uc_engine *uc) {
    /* int sprintf(char *buf, const char *fmt, ...); best-effort 3 args. */
    uint32_t dst_a = 0, fmt_a = 0, a2 = 0, a3 = 0;
    char *dst;
    const char *fmt;
    int n = 0;
    (void)o;
    uc_reg_read(uc, UC_ARM_REG_R0, &dst_a);
    uc_reg_read(uc, UC_ARM_REG_R1, &fmt_a);
    uc_reg_read(uc, UC_ARM_REG_R2, &a2);
    uc_reg_read(uc, UC_ARM_REG_R3, &a3);
    dst = dst_a ? (char *)getMrpMemPtr(dst_a) : NULL;
    fmt = fmt_a ? (const char *)getMrpMemPtr(fmt_a) : NULL;
    if (dst && fmt) {
        if (strstr(fmt, "%s")) {
            const char *s2 = a2 ? (const char *)getMrpMemPtr(a2) : "";
            const char *s3 = a3 ? (const char *)getMrpMemPtr(a3) : "";
            n = sprintf(dst, fmt, s2, s3);
        } else {
            n = sprintf(dst, fmt, a2, a3);
        }
    }
    SET_RET_V((uint32_t)(n >= 0 ? n : 0));
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

    e10a31c_mem_get_enter(uc, len);
    e10a31c_note_last_bridge_api("br_mem_get");

    /*
     * Shell continue: prefer reusing the tracked 4MB DSM heap so _mr_mem_init
     * re-initializes LG_mem on the same block (avoids second-alloc OOM).
     *
     * Do NOT memset the whole block: gbrwcore/gamelist EXT images and ERW live
     * inside this DSM heap (e.g. gbrwcore @ 0x2EB7FC within 0x282A54+4MB).
     * Zeroing here wiped live code → stale LR into zeros → bogus mr_open(NULL)
     * / drawBitmap with garbage args (E10A-2 SHELL_VFS_BADPATH).
     */
    if (g_br_mem_host && g_br_mem_guest && g_br_mem_len >= len) {
        buffer = g_br_mem_guest;
        len = g_br_mem_len;
        printf("[JJFB_DSM_HEAP] action=reuse guest=0x%X len=%u reason=shell_package_restart "
               "memset=no note=preserve_ext_images evidence=TARGET_OBSERVED\n",
               buffer, len);
        printf("br_mem_get base=0x%X len=%d(%d kb) =================\n", buffer, len, len / 1024);
        fflush(stdout);
        uc_mem_write(uc, mem_base, &buffer, 4);
        uc_mem_write(uc, mem_len, &len, 4);
        SET_RET_V(MR_SUCCESS);
        e10a31c_mem_get_return(uc, buffer, len, 1);
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
        e10a31c_mem_get_return(uc, 0, 0, 0);
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
    e10a31c_mem_get_return(uc, buffer, len, 1);
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
    gwy_ext_obs_e10a31a_br_exit_enter(uc);
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
        printf("[GWY_CONTINUE_APPLY] via=br_exit target=%s evidence=OBSERVED\n",
               tgt ? tgt : "?");
        fflush(stdout);
        /*
         * E10A-2: gbrwcore RUNTIME_ENTRY left R9 on gbrwcore ER_RW. DSM/start.mr
         * loads platform fnptrs via R9+0x2084; with gbrwcore R9 that is OOB and
         * yields sticky-LR bridge entries (open R0=NULL, drawBitmap garbage).
         * Do not trust mr_c_function_P here — it may already point at gbrwcore P.
         * Use registry DSM ER_RW via ensure_dsm_r9.
         */
        {
            uint32_t cur_r9 = 0;
            uint32_t after_r9 = 0;
            int rc;
            g_e10a_shell_continued = 1;
            /*
             * MR_START_DSM must reach DSM cfunction (0xA4178), not the nested
             * gbrwcore helper that LOG_PARSE retargeted onto mr_extHelper_addr.
             */
            bridge_restore_dsm_helper("shell_core_continue");
            uc_reg_read(uc, UC_ARM_REG_R9, &cur_r9);
            rc = gwy_ext_obs_ensure_dsm_r9(uc, 0x80008u);
            uc_reg_read(uc, UC_ARM_REG_R9, &after_r9);
            printf("[JJFB_E10A_R9_RESTORE] from=0x%X to=0x%X switched=%d "
                   "reason=shell_core_continue evidence=TARGET_OBSERVED\n",
                   cur_r9, after_r9, rc);
            fflush(stdout);
        }
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
            int wait_timer = 0;
            uint32_t wait_ms = 60000u;
            const char *wt = getenv("JJFB_E10A31_WAIT_FOR_TIMER");
            const char *wms = getenv("JJFB_E10A31_WAIT_MS");
            if (wt && wt[0] == '1') wait_timer = 1;
            if (wms && wms[0]) wait_ms = (uint32_t)strtoul(wms, NULL, 10);
            if (wait_ms < 5000u) wait_ms = 5000u;
            if (wait_ms > 180000u) wait_ms = 180000u;
            printf("[PLATFORM_TIMER] op=POST_CONT_PUMP begin wait_for_timer=%d wait_ms=%u "
                   "evidence=DOCUMENTED\n",
                   wait_timer, wait_ms);
            fflush(stdout);
            for (;;) {
                gwy_ext_obs_timer_poll_uc(uc);
                if (wait_timer && gwy_ext_obs_e10a31_timer_fire_observed()) {
                    int need = 1;
                    int got = 0;
                    const char *wn = getenv("JJFB_E10A31_WAIT_FIRE_N");
                    if (wn && wn[0]) need = (int)strtol(wn, NULL, 10);
                    if (need < 1) need = 1;
                    if (need > 8) need = 8;
                    /* timer_fire_n is latched in e10a31; expose via observed + CSV count. */
                    got = gwy_ext_obs_e10a31_timer_fire_count();
                    if (got < 0) got = 1;
                    if (got >= need) {
                        printf("[JJFB_E10A31_WAIT_FOR_TIMER] stop=TIMER_FIRE_OBSERVED "
                               "fires=%d need=%d pumps=%d evidence=OBSERVED\n",
                               got, need, pumps);
                        fflush(stdout);
                        break;
                    }
                }
                if (!gwy_ext_obs_timer_running()) {
                    int need_fires = 1;
                    int got_fires = gwy_ext_obs_e10a31_timer_fire_count();
                    const char *wn2 = getenv("JJFB_E10A31_WAIT_FIRE_N");
                    if (wn2 && wn2[0]) need_fires = (int)strtol(wn2, NULL, 10);
                    if (need_fires < 1) need_fires = 1;
                    if (need_fires > 8) need_fires = 8;
                    if (wait_timer && !gwy_ext_obs_e10a31_timer_arm_observed()) {
                        /* Keep host phase alive until arm, fire, fault, or hard timeout. */
                        idle = 0;
                    } else if (wait_timer && got_fires < need_fires) {
                        /* Need more FIRE samples before ending POST_CONT_PUMP. */
                        idle = 0;
                    } else if (++idle >= 6) {
                        if (wait_timer && !gwy_ext_obs_e10a31_timer_fire_observed()) {
                            /* Armed then stopped — still wait for a fire within budget. */
                            idle = 0;
                        } else {
                            break;
                        }
                    }
                } else {
                    idle = 0;
                }
                if (((uint32_t)get_uptime_ms() - t0) > wait_ms) {
                    printf("[JJFB_E10A31_WAIT_FOR_TIMER] stop=HARD_TIMEOUT wait_ms=%u "
                           "arm=%d fire=%d pumps=%d evidence=OBSERVED\n",
                           wait_ms, gwy_ext_obs_e10a31_timer_arm_observed(),
                           gwy_ext_obs_e10a31_timer_fire_observed(), pumps);
                    fflush(stdout);
                    break;
                }
                if (++pumps > 4000) break;
                usleep(50000u);
            }
            printf("[PLATFORM_TIMER] op=POST_CONT_PUMP end pumps=%d arm=%d fire=%d "
                   "evidence=DOCUMENTED\n",
                   pumps, gwy_ext_obs_e10a31_timer_arm_observed(),
                   gwy_ext_obs_e10a31_timer_fire_observed());
            fflush(stdout);
        }
        /*
         * mr_exit was reached via a sticky DSM table walk; returning would resume
         * that walk (srand/mem_get/...). Park at stop so outer runCode unwinds and
         * the host loop owns gamelist events.
         */
        {
            uint32_t stop_pc = CODE_ADDRESS;
            uc_reg_write(uc, UC_ARM_REG_PC, &stop_pc);
            uc_reg_write(uc, UC_ARM_REG_LR, &stop_pc);
            printf("[JJFB_E10A_EXIT_PARK] pc=0x%X reason=shell_core_continue "
                   "note=no_resume_exit_caller evidence=TARGET_OBSERVED\n",
                   stop_pc);
            fflush(stdout);
        }
        return;
    }
    /*
     * After shell_core_continue has already consumed mr_exit once, a later sticky
     * DSM/gamelist mr_exit must not process-exit — park so POST_CONT_PUMP / host
     * loop can keep delivering timer/cfg. First gbrwcore exit still continues above.
     */
    if (g_e10a_shell_continued) {
        uint32_t stop_pc = CODE_ADDRESS;
        uc_reg_write(uc, UC_ARM_REG_PC, &stop_pc);
        uc_reg_write(uc, UC_ARM_REG_LR, &stop_pc);
        printf("[JJFB_E10A_EXIT_PARK] pc=0x%X reason=already_continued "
               "note=no_process_exit_after_shell_continue evidence=TARGET_OBSERVED\n",
               stop_pc);
        fflush(stdout);
        return;
    }
    gwy_ext_obs_e10a31a_br_exit_fallback(uc);
    gwy_ext_obs_mr_exit(uc);
    puts("mythroad exit.\n");
    e10a31c_note_last_bridge_api("br_exit");
    e10a31c_note_process_exit(0, "br_exit_fallback");
    gwy_ext_obs_e10a31a_process_exit(0);
    gwy_ext_obs_e10a31a_runtime_stop("br_exit", "GBRWCORE_MR_EXIT_FALLBACK", uc, 0,
                                     "process_exit_0");
    fflush(stdout);
    fflush(stderr);
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
    BRIDGE_FUNC_MAP(0x44, MAP_FUNC, sprintf, NULL, br_sprintf, 0),
    BRIDGE_FUNC_MAP(0x48, MAP_FUNC, atoi, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x4C, MAP_FUNC, strtoul, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x50, MAP_FUNC, rand, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x54, MAP_FUNC, mrc_extMainSendAppEventShell, NULL, br_mrc_extMainSendAppEventShell, 0),
    BRIDGE_FUNC_MAP(0x58, MAP_DATA, reserve1, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x5C, MAP_DATA, _mr_c_internal_table, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x60, MAP_DATA, _mr_c_port_table, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x64, MAP_FUNC, _mr_c_function_new, NULL, br__mr_c_function_new, 0),
    BRIDGE_FUNC_MAP(0x68, MAP_FUNC, mr_printf, NULL, br_mr_printf, 0),
    BRIDGE_FUNC_MAP(0x6C, MAP_FUNC, mr_mem_get, NULL, br_mem_get, 0),
    BRIDGE_FUNC_MAP(0x70, MAP_FUNC, mr_mem_free, NULL, br_mem_free, 0),
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
    BRIDGE_FUNC_MAP(0xA0, MAP_FUNC, mr_open, NULL, br_mr_open, 0),
    BRIDGE_FUNC_MAP(0xA4, MAP_FUNC, mr_close, NULL, br_mr_close, 0),
    BRIDGE_FUNC_MAP(0xA8, MAP_FUNC, mr_info, NULL, br_info, 0),
    BRIDGE_FUNC_MAP(0xAC, MAP_FUNC, mr_write, NULL, br_mr_write, 0),
    BRIDGE_FUNC_MAP(0xB0, MAP_FUNC, mr_read, NULL, br_mr_read, 0),
    BRIDGE_FUNC_MAP(0xB4, MAP_FUNC, mr_seek, NULL, br_mr_seek, 0),
    BRIDGE_FUNC_MAP(0xB8, MAP_FUNC, mr_getLen, NULL, br_mr_getLen, 0),
    BRIDGE_FUNC_MAP(0xBC, MAP_FUNC, mr_remove, NULL, br_mr_remove, 0),
    BRIDGE_FUNC_MAP(0xC0, MAP_FUNC, mr_rename, NULL, br_mr_rename, 0),
    BRIDGE_FUNC_MAP(0xC4, MAP_FUNC, mr_mkDir, NULL, br_mr_mkDir, 0),
    BRIDGE_FUNC_MAP(0xC8, MAP_FUNC, mr_rmDir, NULL, br_mr_rmDir, 0),
    BRIDGE_FUNC_MAP(0xCC, MAP_FUNC, mr_findStart, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xD0, MAP_FUNC, mr_findGetNext, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xD4, MAP_FUNC, mr_findStop, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0xD8, MAP_FUNC, mr_exit, NULL, br_exit, 0),
    BRIDGE_FUNC_MAP(0xDC, MAP_FUNC, mr_startShake, NULL, br_mr_startShake, 0),
    BRIDGE_FUNC_MAP(0xE0, MAP_FUNC, mr_stopShake, NULL, br_mr_stopShake, 0),
    BRIDGE_FUNC_MAP(0xE4, MAP_FUNC, mr_playSound, NULL, br_mr_playSound, 0),
    BRIDGE_FUNC_MAP(0xE8, MAP_FUNC, mr_stopSound, NULL, br_mr_stopSound, 0),
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
    BRIDGE_FUNC_MAP(0x114, MAP_FUNC, mr_dialogCreate, NULL, br_mr_dialogCreate, 0),
    BRIDGE_FUNC_MAP(0x118, MAP_FUNC, mr_dialogRelease, NULL, br_mr_dialogRelease, 0),
    BRIDGE_FUNC_MAP(0x11C, MAP_FUNC, mr_dialogRefresh, NULL, br_mr_dialogRefresh, 0),
    BRIDGE_FUNC_MAP(0x120, MAP_FUNC, mr_textCreate, NULL, br_mr_textCreate, 0),
    BRIDGE_FUNC_MAP(0x124, MAP_FUNC, mr_textRelease, NULL, br_mr_textRelease, 0),
    BRIDGE_FUNC_MAP(0x128, MAP_FUNC, mr_textRefresh, NULL, br_mr_textRefresh, 0),
    BRIDGE_FUNC_MAP(0x12C, MAP_FUNC, mr_editCreate, NULL, br_mr_editCreate, 0),
    BRIDGE_FUNC_MAP(0x130, MAP_FUNC, mr_editRelease, NULL, br_mr_editRelease, 0),
    BRIDGE_FUNC_MAP(0x134, MAP_FUNC, mr_editGetText, NULL, br_mr_editGetText, 0),
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
    BRIDGE_FUNC_MAP(0x1D0, MAP_FUNC, _mr_load_sms_cfg, NULL, br_mr_load_sms_cfg, 0),
    BRIDGE_FUNC_MAP(0x1D4, MAP_FUNC, _mr_save_sms_cfg, NULL, br_mr_save_sms_cfg, 0),
    BRIDGE_FUNC_MAP(0x1D8, MAP_FUNC, _DispUpEx, NULL, br_observe_disp_up, 0),
    BRIDGE_FUNC_MAP(0x1DC, MAP_FUNC, _DrawPoint, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1E0, MAP_FUNC, _DrawBitmap, NULL, br_mr_drawBitmap, 0),
    BRIDGE_FUNC_MAP(0x1E4, MAP_FUNC, _DrawBitmapEx, NULL, br_mr_drawBitmap, 0),
    BRIDGE_FUNC_MAP(0x1E8, MAP_FUNC, DrawRect, NULL, br_observe_draw_rect, 0),
    BRIDGE_FUNC_MAP(0x1EC, MAP_FUNC, _DrawText, NULL, br_observe_draw_text, 0),
    BRIDGE_FUNC_MAP(0x1F0, MAP_FUNC, _BitmapCheck, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1F4, MAP_FUNC, _mr_readFile, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1F8, MAP_FUNC, mr_wstrlen, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x1FC, MAP_FUNC, mr_registerAPP, NULL, NULL, 0),
    BRIDGE_FUNC_MAP(0x200, MAP_FUNC, _DrawTextEx, NULL, br_observe_draw_text, 0),
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
    BRIDGE_FUNC_MAP(0x244, MAP_FUNC, mr_platDrawChar, NULL, br_mr_platDrawChar_compat, 0),
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
                /*
                 * After shell_core_continue, process-exit on every missing MAP_FUNC
                 * aborts mrc_init/cfg (mr_printf/mem_get/getCharBitmap…). Soft-return
                 * so POST_CONT_PUMP can keep delivering timer/cfg; first-boot path
                 * still fails closed.
                 */
                if (g_e10a_shell_continued) {
                    int32_t pol = -1;
                    uint32_t _lr = 0;
                    uint32_t z;
                    (void)e10a31c_unimpl_policy(uc, slot, obj->name, &pol);
                    z = (uint32_t)pol;
                    e10a31d_note_platform_api(uc, obj->name, slot, (int32_t)pol);
                    printf("[JJFB_E10A_UNIMPL_SOFT] api=%s slot=0x%X r0=%d "
                           "note=policy_not_blanket_success evidence=OBSERVED\n",
                           obj->name ? obj->name : "?", slot, (int)pol);
                    fflush(stdout);
                    uc_reg_write(uc, UC_ARM_REG_R0, &z);
                    uc_reg_read(uc, UC_ARM_REG_LR, &_lr);
                    uc_reg_write(uc, UC_ARM_REG_PC, &_lr);
                    return;
                }
                exit(1);
                return;
            }
#ifdef GWY_USE_VM_FILE_SERVICE
            /* E10A-2: bridge entry while LR still in gbrwcore dispatch or DSM CODE. */
            if (e10a_shell_trace_enabled()) {
                uint32_t lr = 0, r0 = 0, r1 = 0, sp = 0, r9v = 0;
                uint32_t lra;
                uc_reg_read(uc, UC_ARM_REG_LR, &lr);
                uc_reg_read(uc, UC_ARM_REG_R0, &r0);
                uc_reg_read(uc, UC_ARM_REG_R1, &r1);
                uc_reg_read(uc, UC_ARM_REG_SP, &sp);
                uc_reg_read(uc, UC_ARM_REG_R9, &r9v);
                lra = lr & ~1u;
                if ((lra >= 0x30CFE0u && lra < 0x30D100u) ||
                    (lra >= 0x80000u && lra < 0xD2000u)) {
                    printf("[JJFB_E10A_BRIDGE_STALE_LR] api=%s stub=0x%X lr=0x%X r0=0x%X r1=0x%X "
                           "r9=0x%X sp=0x%X evidence=OBSERVED\n",
                           obj->name ? obj->name : "?", slot, lr, r0, r1, r9v, sp);
                    fflush(stdout);
                }
            }
#endif
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
#ifdef GWY_USE_VM_FILE_SERVICE
        if (obj->name && strcmp(obj->name, "open") == 0) {
            e10a_note_mr_open_stub(addr);
        }
#endif
    }
    return ptr;
end:
    my_freeExt(ptr);
    exit(1);
    return NULL;
}

/* Nested runCode (timer/event/helper) must not clobber the outer guest context.
 * Especially LR/PC: after an inner BL, LR can remain a gbrwcore cookie (e.g. 0x30D043)
 * while pop{pc} returned to stop — outer then returns from bridge stubs to the wrong place
 * (E10A-2: srand→mem_get→open with stale LR). */
static const int g_bridge_uc_regs[17] = {
    UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3,  UC_ARM_REG_R4,
    UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,  UC_ARM_REG_R8,  UC_ARM_REG_R9,
    UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12, UC_ARM_REG_SP,  UC_ARM_REG_LR,
    UC_ARM_REG_PC,  UC_ARM_REG_CPSR};

static void bridge_uc_save_regs(uc_engine *uc, uint32_t out[17]) {
    int i;
    for (i = 0; i < 17; i++) {
        out[i] = 0;
        uc_reg_read(uc, g_bridge_uc_regs[i], &out[i]);
    }
}

static void bridge_uc_restore_regs(uc_engine *uc, const uint32_t in[17]) {
    int i;
    for (i = 0; i < 17; i++) {
        uint32_t v = in[i];
        uc_reg_write(uc, g_bridge_uc_regs[i], &v);
    }
}

static void runCode(uc_engine *uc, uint32_t startAddr, uint32_t stopAddr, bool isThumb) {
    uint32_t pc;
    uc_reg_write(uc, UC_ARM_REG_LR, &stopAddr);  // 当程序执行到这里时停止运行(return)

    /* Instruction slices (unicorn 1.0.2 timeout is unreliable). Poll deadline
     * between slices so EXT timers advance while start_dsm holds the mutex. */
    pc = isThumb ? (startAddr | 1u) : startAddr;
    for (;;) {
        uint32_t cpsr = 0;
        uint32_t pc_before_poll = 0;
        uint32_t lr_before_poll = 0;
        uint32_t cpsr_before_poll = 0;
        uc_err err = uc_emu_start(uc, pc, stopAddr, 0, 100000ull);
        /* Preserve outer PC/LR across nested helper deliveries inside the poll. */
        uc_reg_read(uc, UC_ARM_REG_PC, &pc_before_poll);
        uc_reg_read(uc, UC_ARM_REG_LR, &lr_before_poll);
        uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr_before_poll);
        gwy_ext_obs_timer_poll_uc(uc);
        {
            uint32_t pc_after = 0;
            uc_reg_read(uc, UC_ARM_REG_PC, &pc_after);
            if ((pc_after & ~1u) == (stopAddr & ~1u) &&
                (pc_before_poll & ~1u) != (stopAddr & ~1u)) {
                uc_reg_write(uc, UC_ARM_REG_PC, &pc_before_poll);
                uc_reg_write(uc, UC_ARM_REG_LR, &lr_before_poll);
                uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr_before_poll);
            }
        }
        /* E9B: keep HWND responsive while Unicorn owns the UI thread. */
        guiPumpEvents();
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
/* DOCUMENTED mrc_extHelper case 2 → mrc_timerTimeout (not DSM Lua MR_TIMER). */
static void bridge_timer_fire_r9_guard_cb(uc_engine *uc, uint64_t address, uint32_t size,
                                          void *user_data) {
    (void)address;
    (void)size;
    (void)user_data;
    (void)gwy_ext_obs_timer_fire_r9_guard(uc);
}

static int32_t bridge_ext_helper_call(uc_engine *uc, uint32_t helper, uint32_t p_guest,
                                      uint32_t code, uint32_t input, uint32_t input_len,
                                      uint32_t erw) {
    uint32_t saved[17];
    uint32_t ret = 0;
    int thumb;
    uc_hook hh = 0;
    int armed_pin = 0;
    GwyDispatchKind dkind;
    E10a31dSource dsrc;
    uint32_t caller_pc = 0, caller_lr = 0;
    if (!uc || !helper || !p_guest) return MR_FAILED;
    dkind = (code == 2u) ? GWY_DISPATCH_TIMER : GWY_DISPATCH_EXT_HELPER;
    if (code == 2u)
        dsrc = E10A31D_SRC_TIMER;
    else if (g_in_ext_init_deliver)
        dsrc = E10A31D_SRC_HOST_FAST_REAL;
    else
        dsrc = E10A31D_SRC_HOST_FAST_REAL;
    bridge_uc_save_regs(uc, saved);
    caller_pc = saved[15];
    caller_lr = saved[14];
    if (erw) uc_reg_write(uc, UC_ARM_REG_R9, &erw);
    /* Timer FIRE (code=2): pin R9 to target ERW; refuse drift to gbrwcore ERW. */
    if (code == 2u && erw && g_e10a_shell_continued) {
        uint32_t forbid = gwy_ext_obs_module_erw_by_name("gbrwcore");
        if (forbid && forbid != erw) {
            gwy_ext_obs_timer_fire_r9_pin(erw, forbid);
            if (uc_hook_add(uc, &hh, UC_HOOK_BLOCK, (void *)bridge_timer_fire_r9_guard_cb, NULL, 1,
                            0) == UC_ERR_OK)
                armed_pin = 1;
        }
    }
    uc_reg_write(uc, UC_ARM_REG_R0, &p_guest);
    uc_reg_write(uc, UC_ARM_REG_R1, &code);
    uc_reg_write(uc, UC_ARM_REG_R2, &input);
    uc_reg_write(uc, UC_ARM_REG_R3, &input_len);
    e10a31c_enter(uc, dkind, helper, code, p_guest, erw);
    e10a31d_helper_enter(uc, dsrc, helper, code, p_guest, erw, input, input_len, caller_pc,
                         caller_lr);
    thumb = (helper & 1u) != 0;
    runCode(uc, helper & ~1u, CODE_ADDRESS, thumb);
    uc_reg_read(uc, UC_ARM_REG_R0, &ret);
    e10a31d_helper_return(uc, helper, code, (int32_t)ret);
    if (armed_pin) {
        uc_hook_del(uc, hh);
        gwy_ext_obs_timer_fire_r9_unpin();
    }
    bridge_uc_restore_regs(uc, saved);
    /* Leave after context restore so deferred timer pumps at a safe point. */
    e10a31c_leave(uc, dkind);
    if (g_e10a_shell_continued)
        (void)gwy_ext_obs_ensure_dsm_r9(uc, 0x80008u);
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
        gwy_ext_obs_on_timer_fire_ext(helper, p_guest, erw, v);
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
    uint32_t saved[17];
    GwyDispatchKind dkind =
        (code == 1u) ? GWY_DISPATCH_DSM_EVENT : GWY_DISPATCH_EXT_HELPER;
    int entered = 0;

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
    bridge_uc_save_regs(uc, saved);
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

    /* E10A-3.1c: sticky-R9 helper path also owns a dispatch critical section. */
    e10a31c_enter(uc, dkind, mr_extHelper_addr, code, v, rw_base);
    entered = 1;
    e10a31d_helper_enter(uc,
                         g_in_ext_init_deliver ? E10A31D_SRC_HOST_FAST_REAL
                                               : E10A31D_SRC_NATIVE_GUEST,
                         mr_extHelper_addr, code, v, rw_base, input, input_len, saved[15],
                         saved[14]);
    runCode(uc, mr_extHelper_addr, CODE_ADDRESS, false);
    gwy_ext_obs_r9_switch_leave(uc);
    uc_reg_read(uc, UC_ARM_REG_R0, &v);
    e10a31d_helper_return(uc, mr_extHelper_addr, code, (int32_t)v);
    gwy_ext_obs_helper_call(mr_extHelper_addr, code, (int32_t)v);
    bridge_uc_restore_regs(uc, saved);
    /*
     * Nested gbrwcore helper may leave R9 on MRP ER_RW (leave REJECT or
     * ALREADY_SWITCHED save). Outer gamelist/DSM needs cfunction ER_RW.
     */
    if (g_e10a_shell_continued)
        (void)gwy_ext_obs_ensure_dsm_r9(uc, 0x80008u);

    /* Mid-call queue (R9 restore during nested guest) → deliver before returning. */
    if (!g_in_ext_init_deliver && code != 0u && code != 6u && code != 8u)
        bridge_deliver_ext_init_seq(uc, code, "after");

    if (entered) e10a31c_leave(uc, dkind);
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
    uint32_t ret;
    uint32_t saved[17];
    uint32_t erw = mr_c_function_P
                       ? (uint32_t)(uintptr_t)mr_c_function_P->start_of_ER_RW
                       : 0;
    bridge_uc_save_regs(uc, saved);

    // 因为回调不是从mr_extHelper调用，因此需要手动设置r9
    gwy_ext_obs_r9_write_raw(uc, erw, "bridge_dsm_network_cb_set");
    // 实际上这个r9值被设置成mythroad层的，因为mythroad层的lua部分也会调用
    // 因此由mythroad层去区分是mythroad层的回调函数还是mrp层的回调函数，这就是userData存在的意义

    uc_reg_write(uc, UC_ARM_REG_R0, &p0);
    uc_reg_write(uc, UC_ARM_REG_R1, &p1);
    e10a31c_enter(uc, GWY_DISPATCH_PLATFORM_CALLBACK, addr, 0u, 0u, erw);
    runCode(uc, addr, CODE_ADDRESS, false);
    e10a31c_leave(uc, GWY_DISPATCH_PLATFORM_CALLBACK);

    uc_reg_read(uc, UC_ARM_REG_R0, &ret);
    bridge_uc_restore_regs(uc, saved);
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
    gwy_ext_obs_note_start_dsm_abi(uc, toMrpMemAddr(mr_start_dsm_param), filename, ext, entry);

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

#ifdef GWY_USE_VM_FILE_SERVICE
/* E9V: generic RGB565 color-key for bitmap sprites (Maopao magenta 0xF81F / #FF00FF).
 * Detect when corners contain the key (or env force); never hardcode game member names. */
static int jjfb_sprite_pick_colorkey(const uint16_t *px, int w, int h, uint16_t *out_key,
                                     const char **out_src) {
    const char *ck;
    int corners = 0;
    if (!px || !out_key || w <= 0 || h <= 0) return 0;
    ck = getenv("JJFB_COLORKEY");
    if (ck && ck[0] == '0' && ck[1] == '\0') return 0;
    if (ck && (ck[0] == 'F' || ck[0] == 'f' ||
               (ck[0] == '0' && (ck[1] == 'x' || ck[1] == 'X')))) {
        *out_key = (uint16_t)strtoul(ck, NULL, 16);
        if (out_src) *out_src = "env";
        return *out_key ? 1 : 0;
    }
    /* auto (default): key only when ≥2 corners match classic magenta. */
    if (w > 1 && h > 1) {
        if (px[0] == 0xF81Fu) corners++;
        if (px[(unsigned)(w - 1)] == 0xF81Fu) corners++;
        if (px[(unsigned)(h - 1) * (unsigned)w] == 0xF81Fu) corners++;
        if (px[(unsigned)(h - 1) * (unsigned)w + (unsigned)(w - 1)] == 0xF81Fu) corners++;
        if (corners >= 2) {
            *out_key = 0xF81Fu;
            if (out_src) *out_src = "corners";
            return 1;
        }
    }
    /* Single-corner / dense key: first pixel is key and ≥8% of samples match. */
    if (px[0] == 0xF81Fu) {
        uint32_t i, n = (uint32_t)w * (uint32_t)h, hit = 0, step;
        step = n > 64u ? n / 64u : 1u;
        for (i = 0; i < n; i += step) {
            if (px[i] == 0xF81Fu) hit++;
        }
        if (hit >= 4u) {
            *out_key = 0xF81Fu;
            if (out_src) *out_src = "sample";
            return 1;
        }
    }
    return 0;
}

static void jjfb_blit_sprite_with_optional_key(uint16_t *px, int x, int y, int w, int h,
                                               const char *member_or_handle) {
    uint16_t key = 0;
    const char *ksrc = "-";
    uint32_t lit = 0, skipped = 0;
    int i, n;
    if (!px || w <= 0 || h <= 0) return;
    if (jjfb_sprite_pick_colorkey(px, w, h, &key, &ksrc)) {
        n = w * h;
        for (i = 0; i < n; i++) {
            if (px[i] == key)
                skipped++;
            else
                lit++;
        }
        guiDrawBitmapSpriteKey(px, (int32_t)x, (int32_t)y, (int32_t)w, (int32_t)h, key);
        printf("[JJFB_BITMAP_COLORKEY] member=%s key=0x%04X skipped=%u lit=%u source=%s "
               "@%d,%d %dx%d evidence=OBSERVED\n",
               member_or_handle && member_or_handle[0] ? member_or_handle : "?",
               (unsigned)key, skipped, lit, ksrc, x, y, w, h);
        printf("[JJFB_E9V_CLASS] class=BITMAP_COLORKEY_TRANSPARENT_RENDERED member=%s "
               "key=0x%04X evidence=OBSERVED\n",
               member_or_handle && member_or_handle[0] ? member_or_handle : "?", (unsigned)key);
        fflush(stdout);
    } else {
        guiDrawBitmapSprite(px, (int32_t)x, (int32_t)y, (int32_t)w, (int32_t)h);
    }
}

/* E9H: splash 0x2EC6B8 path — blit original guest RGB565 via real guiDrawBitmapSprite.
 * Registered into launcher_core (MinGW PE has no ELF weak override).
 * E9V: apply generic color-key when bitmap uses magenta transparent key. */
static int jjfb_e9h_blit_guest_pixels_impl(void *uc_in, uint32_t pixels_va, int x, int y, int w,
                                           int h, const char *member) {
    uc_engine *uc = (uc_engine *)uc_in;
    uint16_t host_pix[240 * 320];
    uint32_t nbytes;
    uc_err ue;
    if (!uc || !pixels_va || w <= 0 || h <= 0 || w > 240 || h > 320) return 0;
    nbytes = (uint32_t)w * (uint32_t)h * 2u;
    if (nbytes > sizeof(host_pix)) return 0;
    memset(host_pix, 0, nbytes);
    ue = uc_mem_read(uc, pixels_va, host_pix, nbytes);
    if (ue != UC_ERR_OK) {
        size_t got = jjfb_bmp_meta_copy_pixels(pixels_va, host_pix, sizeof(host_pix));
        if (got < nbytes) {
            printf("[JJFB_E9H_2EC6B8_BLIT] fail=uc_mem_read uc_err=%u pixels=0x%X "
                   "host_cache=%u evidence=OBSERVED\n",
                   (unsigned)ue, pixels_va, (unsigned)got);
            fflush(stdout);
            return 0;
        }
        printf("[JJFB_E9H_2EC6B8_BLIT] pixels=0x%X via=host_pixel_cache note=uc_read_fail "
               "uc_err=%u evidence=OBSERVED\n",
               pixels_va, (unsigned)ue);
        fflush(stdout);
    }
    printf("[JJFB_E9H_2EC6B8_BLIT] pixels=0x%X x=%d y=%d w=%d h=%d member=%s "
           "via=guiDrawBitmapSprite_auto_key note=real_mrp_pixels NOT_PRODUCT evidence=OBSERVED\n",
           pixels_va, x, y, w, h, member && member[0] ? member : "?");
    fflush(stdout);
    jjfb_blit_sprite_with_optional_key(host_pix, x, y, w, h,
                                       member && member[0] ? member : "?");
    return 1;
}

#if defined(__GNUC__)
__attribute__((constructor))
#endif
static void jjfb_e9h_register_blit(void) {
    jjfb_e9h_set_blit_fn(jjfb_e9h_blit_guest_pixels_impl);
}

/* E9K: robotol arms HWND hold after post-r4 / 305BFC / textbar (may not blit again). */
static void jjfb_e9k_hold_impl(const char *reason) {
    printf("[JJFB_E9K_HOLD] reason=%s via=guiVisibleWindowFinalize evidence=OBSERVED\n",
           reason && reason[0] ? reason : "?");
    fflush(stdout);
    guiVisibleWindowFinalize();
}

#if defined(__GNUC__)
__attribute__((constructor))
#endif
static void jjfb_e9k_register_hold(void) {
    jjfb_e9k_set_hold_fn(jjfb_e9k_hold_impl);
}

/* E9N: 0x11F00 sendAppEvent text draw — GBK bytes at real x/y via guiDrawBitmapSprite. */
static int jjfb_e9n_text_draw_impl(int x, int y, const uint8_t *bytes, int nbytes) {
    int i = 0, cx = x, cy = y, nchars = 0;
    uint16_t fg = 0xFFFFu;
    if (!bytes || nbytes <= 0) return 0;
    while (i < nbytes && bytes[i] && nchars < 16) {
        uint16_t ch;
        if (bytes[i] >= 0x81u && i + 1 < nbytes && bytes[i + 1]) {
            ch = (uint16_t)(((uint16_t)bytes[i] << 8) | bytes[i + 1]);
            i += 2;
        } else {
            ch = (uint16_t)bytes[i++];
        }
        jjfb_compat_draw_char_block(cx, cy, ch, fg);
        cx += (ch >= 128u) ? 12 : 8;
        nchars++;
    }
    return nchars > 0 ? 1 : 0;
}

#if defined(__GNUC__)
__attribute__((constructor))
#endif
static void jjfb_e9n_register_text_draw(void) {
    jjfb_e9n_set_text_draw_fn(jjfb_e9n_text_draw_impl);
}

/* E9O: system CJK font via GDI → RGB565 → guiDrawBitmapSprite (same surface as bitmaps). */
#ifdef _WIN32
static const wchar_t *jjfb_pick_cjk_face(int *fallback) {
    static const wchar_t *faces[] = {
        L"Microsoft YaHei", L"微软雅黑", L"SimSun", L"宋体", L"SimHei", L"黑体", L"NSimSun",
        L"Arial Unicode MS", NULL};
    int i;
    HDC hdc = CreateCompatibleDC(NULL);
    *fallback = 1;
    if (!hdc) return L"Arial";
    for (i = 0; faces[i]; i++) {
        HFONT f = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, faces[i]);
        if (!f) continue;
        SelectObject(hdc, f);
        {
            TEXTMETRICW tm;
            GetTextMetricsW(hdc, &tm);
            if (tm.tmCharSet == GB2312_CHARSET || tm.tmCharSet == CHINESEBIG5_CHARSET ||
                tm.tmCharSet == DEFAULT_CHARSET || tm.tmCharSet == SHIFTJIS_CHARSET) {
                DeleteObject(f);
                DeleteDC(hdc);
                *fallback = 0;
                return faces[i];
            }
        }
        DeleteObject(f);
    }
    DeleteDC(hdc);
    return L"Microsoft YaHei";
}

static int jjfb_plat_11f00_gdi_draw(int x, int y, const uint8_t *bytes, int nbytes,
                                    uint16_t fg_rgb565, int clip_x, int clip_y, int clip_w,
                                    int clip_h, const char **font_name_out, int *font_fallback_out) {
    static char s_face_utf8[64];
    wchar_t wbuf[96];
    int wlen, tw = 0, th = 0, i, j;
    HDC hdc, mem;
    HBITMAP dib = NULL, oldbm = NULL;
    HFONT font = NULL, oldf = NULL;
    BITMAPINFO bmi;
    void *bits = NULL;
    uint16_t *out565 = NULL;
    const wchar_t *face;
    int fallback = 0;
    RECT rc;
    SIZE sz;
    if (!bytes || nbytes <= 0) return 0;
    if (!fg_rgb565) fg_rgb565 = 0xFFFFu;

    wlen = MultiByteToWideChar(936, 0, (LPCSTR)bytes, nbytes, wbuf,
                               (int)(sizeof(wbuf) / sizeof(wbuf[0])) - 1);
    if (wlen <= 0) {
        wlen = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)bytes, nbytes, wbuf,
                                   (int)(sizeof(wbuf) / sizeof(wbuf[0])) - 1);
    }
    if (wlen <= 0) {
        if (font_fallback_out) *font_fallback_out = 1;
        if (font_name_out) *font_name_out = "fallback_block_glyph";
        return jjfb_e9n_text_draw_impl(x, y, bytes, nbytes);
    }
    wbuf[wlen] = 0;

    face = jjfb_pick_cjk_face(&fallback);
    memset(s_face_utf8, 0, sizeof(s_face_utf8));
    WideCharToMultiByte(CP_UTF8, 0, face, -1, s_face_utf8, (int)sizeof(s_face_utf8) - 1, NULL,
                        NULL);
    if (font_name_out) *font_name_out = s_face_utf8[0] ? s_face_utf8 : "gdi_cjk";
    if (font_fallback_out) *font_fallback_out = fallback;

    hdc = GetDC(NULL);
    if (!hdc) {
        if (font_fallback_out) *font_fallback_out = 1;
        return jjfb_e9n_text_draw_impl(x, y, bytes, nbytes);
    }
    mem = CreateCompatibleDC(hdc);
    font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GB2312_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, face);
    if (!mem || !font) {
        if (font) DeleteObject(font);
        if (mem) DeleteDC(mem);
        ReleaseDC(NULL, hdc);
        if (font_fallback_out) *font_fallback_out = 1;
        return jjfb_e9n_text_draw_impl(x, y, bytes, nbytes);
    }
    oldf = (HFONT)SelectObject(mem, font);
    GetTextExtentPoint32W(mem, wbuf, wlen, &sz);
    tw = sz.cx + 2;
    th = sz.cy + 2;
    if (tw < 8) tw = 8;
    if (th < 12) th = 12;
    if (tw > 240) tw = 240;
    if (th > 64) th = 64;

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = tw;
    bmi.bmiHeader.biHeight = -th;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    dib = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib || !bits) {
        SelectObject(mem, oldf);
        DeleteObject(font);
        DeleteDC(mem);
        ReleaseDC(NULL, hdc);
        if (font_fallback_out) *font_fallback_out = 1;
        return jjfb_e9n_text_draw_impl(x, y, bytes, nbytes);
    }
    oldbm = (HBITMAP)SelectObject(mem, dib);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(255, 255, 255));
    rc.left = 0;
    rc.top = 0;
    rc.right = tw;
    rc.bottom = th;
    FillRect(mem, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    TextOutW(mem, 1, 1, wbuf, wlen);

    out565 = (uint16_t *)calloc((size_t)tw * (size_t)th, sizeof(uint16_t));
    if (out565) {
        uint8_t *px = (uint8_t *)bits;
        int lit = 0;
        for (j = 0; j < th; j++) {
            for (i = 0; i < tw; i++) {
                uint8_t *p = px + (size_t)j * (size_t)tw * 4u + (size_t)i * 4u;
                if (p[0] > 40 || p[1] > 40 || p[2] > 40) {
                    out565[j * tw + i] = fg_rgb565;
                    lit++;
                }
            }
        }
        if (lit > 0) {
            const char *blit_mode = getenv("JJFB_E9P_TEXT_BLIT");
            int clip_l = clip_x, clip_t = clip_y, clip_r, clip_b;
            int dx = x, dy = y, dw = tw, dh = th;
            uint16_t key = 0;
            /* Default: transparent glyph-only (skip RGB565 0). Opaque only if forced. */
            if (!blit_mode || !blit_mode[0]) blit_mode = "transparent";
            if (clip_w <= 0) clip_w = 240;
            if (clip_h <= 0) clip_h = 320;
            clip_r = clip_l + clip_w;
            clip_b = clip_t + clip_h;
            if (clip_r > 240) clip_r = 240;
            if (clip_b > 320) clip_b = 320;
            /* Clip destination to platform clip box. */
            if (dx < clip_l) {
                dw -= (clip_l - dx);
                dx = clip_l;
            }
            if (dy < clip_t) {
                dh -= (clip_t - dy);
                dy = clip_t;
            }
            if (dx + dw > clip_r) dw = clip_r - dx;
            if (dy + dh > clip_b) dh = clip_b - dy;
            if (dw <= 0 || dh <= 0) {
                printf("[JJFB_E9P_CLASS] class=PLATFORM_TEXT_ALPHA_BLEND_FAILED "
                       "note=clip_empty evidence=OBSERVED\n");
                fflush(stdout);
            } else if (blit_mode[0] == 'o' || blit_mode[0] == 'O') {
                /* opaque: legacy E9O black-box behavior (compare baseline) */
                guiDrawBitmapSprite(out565, (int32_t)x, (int32_t)y, tw, th);
                printf("[JJFB_E9P_CLASS] class=PLATFORM_TEXT_STILL_OPAQUE_BACKGROUND "
                       "blit=opaque lit=%d @%d,%d evidence=OBSERVED\n",
                       lit, x, y);
                fflush(stdout);
            } else {
                if (blit_mode[0] == 'c' || blit_mode[0] == 'C') {
                    /* colorkey: paint unused as magenta key then skip */
                    key = 0xF81Fu;
                    for (j = 0; j < th; j++) {
                        for (i = 0; i < tw; i++) {
                            if (out565[j * tw + i] == 0) out565[j * tw + i] = key;
                        }
                    }
                } else {
                    key = 0; /* transparent: skip black/zero bg */
                }
                /* If clip shrunk origin, rebuild a clipped sub-bitmap. */
                if (dx != x || dy != y || dw != tw || dh != th) {
                    uint16_t *sub = (uint16_t *)calloc((size_t)dw * (size_t)dh, sizeof(uint16_t));
                    int ox = dx - x, oy = dy - y;
                    if (sub) {
                        for (j = 0; j < dh; j++) {
                            for (i = 0; i < dw; i++) {
                                int sx = ox + i, sy = oy + j;
                                if (sx >= 0 && sy >= 0 && sx < tw && sy < th)
                                    sub[j * dw + i] = out565[sy * tw + sx];
                                else
                                    sub[j * dw + i] = key;
                            }
                        }
                        guiDrawBitmapSpriteKey(sub, dx, dy, dw, dh, key);
                        free(sub);
                    } else {
                        guiDrawBitmapSpriteKey(out565, (int32_t)x, (int32_t)y, tw, th, key);
                    }
                } else {
                    guiDrawBitmapSpriteKey(out565, (int32_t)x, (int32_t)y, tw, th, key);
                }
                if (key == 0xF81Fu) {
                    printf("[JJFB_E9P_CLASS] class=PLATFORM_TEXT_COLORKEY_RENDERED "
                           "lit=%d key=0xF81F @%d,%d evidence=OBSERVED\n",
                           lit, dx, dy);
                } else {
                    printf("[JJFB_E9P_CLASS] class=PLATFORM_TEXT_TRANSPARENT_RENDERED "
                           "lit=%d key=0 @%d,%d evidence=OBSERVED\n",
                           lit, dx, dy);
                }
                fflush(stdout);
            }
            printf("[JJFB_PLATFORM_TEXT_API_11F00] font=%s fallback=%d gdi=%dx%d lit=%d "
                   "blit=%s @%d,%d evidence=OBSERVED\n",
                   s_face_utf8, fallback, tw, th, lit, blit_mode, x, y);
            fflush(stdout);
        } else {
            free(out565);
            out565 = NULL;
            fallback = 1;
        }
    }

    SelectObject(mem, oldbm);
    SelectObject(mem, oldf);
    DeleteObject(dib);
    DeleteObject(font);
    DeleteDC(mem);
    ReleaseDC(NULL, hdc);

    if (!out565) {
        if (font_fallback_out) *font_fallback_out = 1;
        if (font_name_out) *font_name_out = "fallback_block_glyph";
        return jjfb_e9n_text_draw_impl(x, y, bytes, nbytes);
    }
    free(out565);
    jjfb_plat_11f00_note_draw_bbox(tw, th);
    if (font_fallback_out) *font_fallback_out = fallback;
    return 1;
}

static int jjfb_plat_12340_gdi_measure(const uint8_t *bytes, int nbytes, int *w_out, int *h_out,
                                       const char **font_name_out, int *font_fallback_out) {
    static char s_face_utf8[64];
    wchar_t wbuf[96];
    int wlen, fallback = 0;
    HDC hdc, mem;
    HFONT font = NULL, oldf = NULL;
    const wchar_t *face;
    SIZE sz;
    if (!bytes || nbytes <= 0 || !w_out || !h_out) return 0;
    wlen = MultiByteToWideChar(936, 0, (LPCSTR)bytes, nbytes, wbuf,
                               (int)(sizeof(wbuf) / sizeof(wbuf[0])) - 1);
    if (wlen <= 0)
        wlen = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)bytes, nbytes, wbuf,
                                   (int)(sizeof(wbuf) / sizeof(wbuf[0])) - 1);
    if (wlen <= 0) return 0;
    wbuf[wlen] = 0;
    face = jjfb_pick_cjk_face(&fallback);
    memset(s_face_utf8, 0, sizeof(s_face_utf8));
    WideCharToMultiByte(CP_UTF8, 0, face, -1, s_face_utf8, (int)sizeof(s_face_utf8) - 1, NULL,
                        NULL);
    if (font_name_out) *font_name_out = s_face_utf8[0] ? s_face_utf8 : "gdi_cjk";
    if (font_fallback_out) *font_fallback_out = fallback;
    hdc = GetDC(NULL);
    if (!hdc) return 0;
    mem = CreateCompatibleDC(hdc);
    font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GB2312_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, face);
    if (!mem || !font) {
        if (font) DeleteObject(font);
        if (mem) DeleteDC(mem);
        ReleaseDC(NULL, hdc);
        return 0;
    }
    oldf = (HFONT)SelectObject(mem, font);
    GetTextExtentPoint32W(mem, wbuf, wlen, &sz);
    *w_out = sz.cx + 2;
    *h_out = sz.cy + 2;
    if (*w_out < 1) *w_out = 1;
    if (*h_out < 1) *h_out = 1;
    if (*w_out > 240) *w_out = 240;
    if (*h_out > 64) *h_out = 64;
    SelectObject(mem, oldf);
    DeleteObject(font);
    DeleteDC(mem);
    ReleaseDC(NULL, hdc);
    return 1;
}
#else
static int jjfb_plat_11f00_gdi_draw(int x, int y, const uint8_t *bytes, int nbytes,
                                    uint16_t fg_rgb565, int clip_x, int clip_y, int clip_w,
                                    int clip_h, const char **font_name_out, int *font_fallback_out) {
    (void)fg_rgb565;
    (void)clip_x;
    (void)clip_y;
    (void)clip_w;
    (void)clip_h;
    if (font_name_out) *font_name_out = "fallback_block_glyph";
    if (font_fallback_out) *font_fallback_out = 1;
    return jjfb_e9n_text_draw_impl(x, y, bytes, nbytes);
}

static int jjfb_plat_12340_gdi_measure(const uint8_t *bytes, int nbytes, int *w_out, int *h_out,
                                       const char **font_name_out, int *font_fallback_out) {
    int i = 0, nchars = 0;
    (void)font_name_out;
    if (font_fallback_out) *font_fallback_out = 1;
    if (font_name_out) *font_name_out = "fallback_12x16_est";
    if (!bytes || nbytes <= 0 || !w_out || !h_out) return 0;
    while (i < nbytes && bytes[i] && nchars < 32) {
        if (bytes[i] >= 0x81u && i + 1 < nbytes && bytes[i + 1]) {
            nchars++;
            i += 2;
        } else {
            nchars++;
            i++;
        }
    }
    if (nchars <= 0) return 0;
    *w_out = nchars * 12;
    *h_out = 16;
    return 1;
}
#endif

#if defined(__GNUC__)
__attribute__((constructor))
#endif
static void jjfb_plat_11f00_register_draw(void) {
    jjfb_plat_11f00_set_draw_fn(jjfb_plat_11f00_gdi_draw);
    jjfb_plat_12340_set_measure_fn(jjfb_plat_12340_gdi_measure);
}
#endif
