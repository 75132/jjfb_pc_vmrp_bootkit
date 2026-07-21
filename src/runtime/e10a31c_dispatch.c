#include "gwy_launcher/e10a31c_dispatch.h"

#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct {
    int known;
    int enabled;
    int init_atomic;
    int mem_get_trace;
    unsigned long long run_id;
    GwyDispatchState st;
    E10a31cTimerPumpFn timer_pump;
    int in_deferred_fire;
    int deferred_timer_pending;
    uint64_t init_tx_id;
    int init_tx_active;
    int init_got6;
    int init_got8;
    int init_got0;
    int init_complete;
    int init_interrupted_by_timer;
    int init_timer_deferred;
    uint32_t mem_get_seq;
    uint32_t unimpl_seq;
    FILE *init_csv;
    FILE *mem_csv;
    FILE *unimpl_csv;
    FILE *exit_csv;
    FILE *exc_csv;
    int crash_filter_installed;
    char last_bridge_api[96];
    uint32_t last_guest_pc;
    uint32_t last_guest_lr;
    uint32_t last_guest_sp;
    uint32_t last_guest_r9;
} g_c;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
}

static void ensure_enabled(void) {
    if (g_c.known) return;
    g_c.known = 1;
    g_c.enabled = env1("JJFB_E10A31C_MODE") || env1("JJFB_E10A31C_INIT_ATOMICITY") ||
                  env1("JJFB_E10A31C_MEM_GET") || env1("JJFB_E10A31C_UNKNOWN_API");
    g_c.init_atomic = env1("JJFB_E10A31C_INIT_ATOMICITY") || env1("JJFB_E10A31C_MODE");
    g_c.mem_get_trace = env1("JJFB_E10A31C_MEM_GET") || env1("JJFB_E10A31C_MODE");
    g_c.run_id = run_id_now();
    if (g_c.enabled) e10a31c_install_crash_recorder();
}

static FILE *open_csv(const char *env_key, const char *fallback, const char *hdr) {
    const char *p = getenv(env_key);
    FILE *f;
    if (!p || !p[0]) p = fallback;
    f = fopen(p, "w");
    if (!f) return NULL;
    fputs(hdr, f);
    fflush(f);
    return f;
}

static void snap_regs(void *uc, uint32_t *pc, uint32_t *lr, uint32_t *sp, uint32_t *r9) {
#ifdef GWY_HAVE_UNICORN
    if (!uc) return;
    if (pc) uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, pc);
    if (lr) uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, lr);
    if (sp) uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, sp);
    if (r9) (void)guest_memory_uc_read_r9((struct uc_struct *)uc, r9);
#else
    (void)uc;
    (void)pc;
    (void)lr;
    (void)sp;
    (void)r9;
#endif
}

static const char *kind_name(GwyDispatchKind k) {
    switch (k) {
    case GWY_DISPATCH_DSM_EVENT: return "DSM_EVENT";
    case GWY_DISPATCH_EXT_HELPER: return "EXT_HELPER";
    case GWY_DISPATCH_TIMER: return "TIMER";
    case GWY_DISPATCH_PLATFORM_CALLBACK: return "PLATFORM_CALLBACK";
    case GWY_DISPATCH_INIT_SEQUENCE: return "INIT_SEQUENCE";
    default: return "NONE";
    }
}

int e10a31c_enabled(void) {
    ensure_enabled();
    return g_c.enabled;
}

void e10a31c_reset(void) {
    E10a31cTimerPumpFn pump = g_c.timer_pump;
    int crash_installed = g_c.crash_filter_installed;
    if (g_c.init_csv) fclose(g_c.init_csv);
    if (g_c.mem_csv) fclose(g_c.mem_csv);
    if (g_c.unimpl_csv) fclose(g_c.unimpl_csv);
    if (g_c.exit_csv) fclose(g_c.exit_csv);
    if (g_c.exc_csv) fclose(g_c.exc_csv);
    memset(&g_c, 0, sizeof(g_c));
    g_c.timer_pump = pump;
    g_c.crash_filter_installed = crash_installed;
}

const GwyDispatchState *e10a31c_state(void) {
    ensure_enabled();
    return &g_c.st;
}

int e10a31c_busy(void) {
    ensure_enabled();
    if (!g_c.enabled) return 0;
    /* Critical section = helper/init/timer/platform callback, not bare DSM_EVENT.
     * START_DSM can run for minutes; treating it as busy starves deferred FIRE. */
    return g_c.st.helper_depth > 0 || g_c.st.timer_depth > 0 || g_c.st.init_depth > 0 ||
           (g_c.st.depth > 0 && g_c.st.outer_kind == GWY_DISPATCH_PLATFORM_CALLBACK);
}

int e10a31c_should_defer_timer(void) {
    ensure_enabled();
    if (!g_c.enabled) return 0;
    /* Never nest another FIRE inside deferred pump or an active helper/init. */
    if (g_c.in_deferred_fire) return 1;
    return e10a31c_busy();
}

int e10a31c_in_deferred_fire(void) { return g_c.in_deferred_fire; }

void e10a31c_set_timer_pump(E10a31cTimerPumpFn fn) { g_c.timer_pump = fn; }

void e10a31c_mark_milestone(const char *name, const char *note) {
    ensure_enabled();
    if (!g_c.enabled || !name) return;
    printf("[JJFB_E10A31C] milestone=%s note=%s evidence=OBSERVED\n", name,
           note ? note : "");
    fflush(stdout);
}

static void maybe_pump_deferred(void *uc) {
    if (!g_c.enabled || !g_c.deferred_timer_pending) return;
    if (g_c.in_deferred_fire) return;
    /* Normal rule: only at outermost depth==0. */
    if (g_c.st.depth != 0) return;
    if (!g_c.timer_pump) return;
    g_c.deferred_timer_pending = 0;
    g_c.st.deferred_timer_count = 0;
    g_c.in_deferred_fire = 1;
    printf("[GWY_TIMER_DEFERRED_FIRE] depth=0 pending_cleared=1 evidence=OBSERVED\n");
    fflush(stdout);
    g_c.timer_pump(uc);
    g_c.in_deferred_fire = 0;
}

/* After init 6→8→0 returns: pump one deferred timer even under outer DSM_EVENT. */
static void maybe_pump_deferred_after_init(void *uc) {
    if (!g_c.enabled || !g_c.deferred_timer_pending) return;
    if (g_c.in_deferred_fire) return;
    if (g_c.st.helper_depth > 0 || g_c.st.init_depth > 0 || g_c.st.timer_depth > 0) return;
    if (!g_c.timer_pump) return;
    g_c.deferred_timer_pending = 0;
    g_c.st.deferred_timer_count = 0;
    g_c.in_deferred_fire = 1;
    printf("[GWY_TIMER_DEFERRED_FIRE] depth=%u via=after_init_tx pending_cleared=1 "
           "evidence=OBSERVED\n",
           g_c.st.depth);
    fflush(stdout);
    g_c.timer_pump(uc);
    g_c.in_deferred_fire = 0;
}

void e10a31c_note_timer_defer(void *uc, uint32_t helper) {
    ensure_enabled();
    if (!g_c.enabled) return;
    (void)uc;
    if (!g_c.deferred_timer_pending) {
        g_c.deferred_timer_pending = 1;
        g_c.st.deferred_timer_count++;
        e10a31c_mark_milestone("GUEST_TIMER_REENTRANCY_PREVENTED", kind_name(g_c.st.outer_kind));
        if (g_c.st.init_depth > 0) {
            g_c.init_timer_deferred = 1;
            e10a31c_mark_milestone("TIMER_DEFERRED_DURING_INIT", "init_depth>0");
        }
    }
    printf("[GWY_TIMER_DEFER] current_kind=%s depth=%u helper_depth=%u init_depth=%u "
           "timer_helper=0x%X deferred=%u evidence=OBSERVED\n",
           kind_name(g_c.st.outer_kind), g_c.st.depth, g_c.st.helper_depth, g_c.st.init_depth,
           helper, g_c.st.deferred_timer_count);
    fflush(stdout);
}

void e10a31c_enter(void *uc, GwyDispatchKind kind, uint32_t helper, uint32_t code,
                   uint32_t p_guest, uint32_t erw) {
    uint32_t pc = 0, lr = 0, sp = 0, r9 = 0;
    ensure_enabled();
    if (!g_c.enabled) return;
    snap_regs(uc, &pc, &lr, &sp, &r9);
    g_c.last_guest_pc = pc;
    g_c.last_guest_lr = lr;
    g_c.last_guest_sp = sp;
    g_c.last_guest_r9 = r9 ? r9 : erw;
    if (kind == GWY_DISPATCH_TIMER && g_c.st.init_depth > 0) {
        g_c.init_interrupted_by_timer = 1;
        e10a31c_mark_milestone("GAMELIST_INIT_INTERRUPTED_BY_TIMER",
                               "timer_enter_during_init");
    }
    if (g_c.st.depth == 0) {
        g_c.st.outer_kind = kind;
        g_c.st.outer_pc = pc;
        g_c.st.outer_lr = lr;
        g_c.st.outer_sp = sp;
        g_c.st.outer_r9 = r9 ? r9 : erw;
        g_c.st.generation++;
    }
    g_c.st.depth++;
    if (kind == GWY_DISPATCH_EXT_HELPER) g_c.st.helper_depth++;
    if (kind == GWY_DISPATCH_TIMER) g_c.st.timer_depth++;
    if (kind == GWY_DISPATCH_INIT_SEQUENCE) g_c.st.init_depth++;
    printf("[GWY_DISPATCH] stage=ENTER kind=%s depth=%u helper_depth=%u init_depth=%u "
           "timer_depth=%u helper=0x%X code=%u P=0x%X erw=0x%X r9=0x%X evidence=DOCUMENTED\n",
           kind_name(kind), g_c.st.depth, g_c.st.helper_depth, g_c.st.init_depth,
           g_c.st.timer_depth, helper, code, p_guest, erw, r9 ? r9 : erw);
    fflush(stdout);
}

void e10a31c_leave(void *uc, GwyDispatchKind kind) {
    ensure_enabled();
    if (!g_c.enabled) return;
    if (kind == GWY_DISPATCH_EXT_HELPER && g_c.st.helper_depth)
        g_c.st.helper_depth--;
    if (kind == GWY_DISPATCH_TIMER && g_c.st.timer_depth) g_c.st.timer_depth--;
    if (kind == GWY_DISPATCH_INIT_SEQUENCE && g_c.st.init_depth) g_c.st.init_depth--;
    if (g_c.st.depth) g_c.st.depth--;
    printf("[GWY_DISPATCH] stage=LEAVE kind=%s depth=%u helper_depth=%u init_depth=%u "
           "timer_depth=%u evidence=DOCUMENTED\n",
           kind_name(kind), g_c.st.depth, g_c.st.helper_depth, g_c.st.init_depth,
           g_c.st.timer_depth);
    fflush(stdout);
    if (g_c.st.depth == 0) {
        g_c.st.outer_kind = GWY_DISPATCH_NONE;
        maybe_pump_deferred(uc);
    }
}

static void ensure_init_csv(void) {
    if (g_c.init_csv || !g_c.init_atomic) return;
    g_c.init_csv =
        open_csv("JJFB_E10A31C_INIT_CSV", "reports/e10a31c_init_sequence_trace.csv",
                 "run_id,tx_id,method,phase,helper,P,erw,pc,lr,sp,r9,depth,init_depth,"
                 "ret,timer_deferred,note\n");
}

uint64_t e10a31c_init_tx_begin(void *uc, uint32_t helper, uint32_t p_guest, uint32_t erw) {
    ensure_enabled();
    if (!g_c.enabled) return 0;
    g_c.init_tx_id = g_c.st.generation + 1u;
    g_c.init_tx_active = 1;
    g_c.init_got6 = g_c.init_got8 = g_c.init_got0 = 0;
    g_c.init_complete = 0;
    g_c.init_interrupted_by_timer = 0;
    g_c.init_timer_deferred = 0;
    ensure_init_csv();
    e10a31c_enter(uc, GWY_DISPATCH_INIT_SEQUENCE, helper, 0, p_guest, erw);
    e10a31c_mark_milestone("FAST_REAL_GAMELIST_INIT_SEQUENCE", "NOT_PRODUCT");
    printf("[JJFB_E10A31C] init_tx=begin id=%llu helper=0x%X P=0x%X erw=0x%X "
           "label=FAST_REAL_GAMELIST_INIT_SEQUENCE NOT_PRODUCT evidence=DOCUMENTED\n",
           (unsigned long long)g_c.init_tx_id, helper, p_guest, erw);
    fflush(stdout);
    return g_c.init_tx_id;
}

void e10a31c_init_method_enter(void *uc, uint32_t method, uint32_t helper, uint32_t p_guest,
                               uint32_t erw) {
    uint32_t pc = 0, lr = 0, sp = 0, r9 = 0;
    char mile[40];
    ensure_enabled();
    if (!g_c.enabled || !g_c.init_tx_active) return;
    snap_regs(uc, &pc, &lr, &sp, &r9);
    snprintf(mile, sizeof(mile), "GAMELIST_INIT%u_ENTER", method);
    e10a31c_mark_milestone(mile, "FAST_REAL");
    ensure_init_csv();
    if (g_c.init_csv) {
        fprintf(g_c.init_csv,
                "%llu,%llu,%u,ENTER,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,,%u,"
                "method_enter\n",
                g_c.run_id, (unsigned long long)g_c.init_tx_id, method, helper, p_guest, erw, pc,
                lr, sp, r9 ? r9 : erw, g_c.st.depth, g_c.st.init_depth,
                g_c.init_timer_deferred);
        fflush(g_c.init_csv);
    }
}

void e10a31c_init_method_return(void *uc, uint32_t method, int32_t ret) {
    uint32_t pc = 0, lr = 0, sp = 0, r9 = 0;
    char mile[40];
    ensure_enabled();
    if (!g_c.enabled || !g_c.init_tx_active) return;
    snap_regs(uc, &pc, &lr, &sp, &r9);
    if (method == 6u) g_c.init_got6 = 1;
    if (method == 8u) g_c.init_got8 = 1;
    if (method == 0u) g_c.init_got0 = 1;
    snprintf(mile, sizeof(mile), "GAMELIST_INIT%u_RETURN", method);
    e10a31c_mark_milestone(mile, "reached");
    ensure_init_csv();
    if (g_c.init_csv) {
        fprintf(g_c.init_csv,
                "%llu,%llu,%u,RETURN,0x0,0x0,0x0,0x%X,0x%X,0x%X,0x%X,%u,%u,%d,%u,"
                "method_return\n",
                g_c.run_id, (unsigned long long)g_c.init_tx_id, method, pc, lr, sp, r9,
                g_c.st.depth, g_c.st.init_depth, (int)ret, g_c.init_timer_deferred);
        fflush(g_c.init_csv);
    }
    if (ret < 0) {
        char fail[48];
        snprintf(fail, sizeof(fail), "GAMELIST_INIT_METHOD%u_FAILED", method);
        e10a31c_mark_milestone(fail, "ret_neg");
    }
}

void e10a31c_init_tx_end(void *uc, int complete, const char *note) {
    ensure_enabled();
    if (!g_c.enabled || !g_c.init_tx_active) return;
    g_c.init_tx_active = 0;
    /* COMPLETE = all three returned. Deferred timer during init is success of Lane B. */
    if (complete && g_c.init_got6 && g_c.init_got8 && g_c.init_got0) {
        g_c.init_complete = 1;
        e10a31c_mark_milestone("GAMELIST_INIT_SEQUENCE_COMPLETE",
                               note ? note : "ret6_ret8_ret0");
    } else {
        e10a31c_mark_milestone("GAMELIST_INIT_SEQUENCE_INCOMPLETE", note ? note : "?");
    }
    e10a31c_leave(uc, GWY_DISPATCH_INIT_SEQUENCE);
    /* Product rule: pump deferred timer only after method 0 returns — even if an
     * outer DSM_EVENT (START_DSM) still holds depth>0. */
    maybe_pump_deferred_after_init(uc);
    printf("[JJFB_E10A31C] init_tx=end id=%llu complete=%d got6=%d got8=%d got0=%d "
           "timer_deferred=%d note=%s evidence=OBSERVED\n",
           (unsigned long long)g_c.init_tx_id, g_c.init_complete, g_c.init_got6, g_c.init_got8,
           g_c.init_got0, g_c.init_timer_deferred, note ? note : "");
    fflush(stdout);
}

int e10a31c_init_sequence_complete(void) { return g_c.init_complete; }
uint64_t e10a31c_init_tx_id(void) { return g_c.init_tx_id; }

void e10a31c_mem_get_enter(void *uc, uint32_t size_hint) {
    uint32_t pc = 0, lr = 0, sp = 0, r9 = 0;
    ensure_enabled();
    if (!g_c.enabled || !g_c.mem_get_trace) return;
    g_c.mem_get_seq++;
    snap_regs(uc, &pc, &lr, &sp, &r9);
    if (e10a31c_busy() && g_c.st.depth > 1)
        e10a31c_mark_milestone("DSM_HEAP_REENTRANCY", "mem_get_nested");
    printf("[MEM_GET_ENTER] seq=%u size=0x%X pc=0x%X lr=0x%X sp=0x%X r9=0x%X depth=%u "
           "kind=%s init_tx=%llu evidence=OBSERVED\n",
           g_c.mem_get_seq, size_hint, pc, lr, sp, r9, g_c.st.depth, kind_name(g_c.st.outer_kind),
           (unsigned long long)g_c.init_tx_id);
    fflush(stdout);
    if (!g_c.mem_csv) {
        g_c.mem_csv = open_csv("JJFB_E10A31C_MEM_CSV", "reports/e10a31c_mem_get_trace.csv",
                               "run_id,seq,phase,size,ptr,len,pc,lr,sp,r9,depth,kind,init_tx,"
                               "ok,note\n");
    }
    if (g_c.mem_csv) {
        fprintf(g_c.mem_csv, "%llu,%u,ENTER,0x%X,0x0,0,0x%X,0x%X,0x%X,0x%X,%u,%s,%llu,,"
                             "enter\n",
                g_c.run_id, g_c.mem_get_seq, size_hint, pc, lr, sp, r9, g_c.st.depth,
                kind_name(g_c.st.outer_kind), (unsigned long long)g_c.init_tx_id);
        fflush(g_c.mem_csv);
    }
}

void e10a31c_mem_get_return(void *uc, uint32_t ptr, uint32_t len, int ok) {
    uint32_t pc = 0, lr = 0, sp = 0, r9 = 0;
    ensure_enabled();
    if (!g_c.enabled || !g_c.mem_get_trace) return;
    snap_regs(uc, &pc, &lr, &sp, &r9);
    printf("[MEM_GET_RETURN] seq=%u ptr=0x%X len=0x%X ok=%d pc=0x%X depth=%u "
           "evidence=OBSERVED\n",
           g_c.mem_get_seq, ptr, len, ok, pc, g_c.st.depth);
    fflush(stdout);
    if (g_c.mem_csv) {
        fprintf(g_c.mem_csv, "%llu,%u,RETURN,0x0,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%s,%llu,%d,"
                             "return\n",
                g_c.run_id, g_c.mem_get_seq, ptr, len, pc, lr, sp, r9, g_c.st.depth,
                kind_name(g_c.st.outer_kind), (unsigned long long)g_c.init_tx_id, ok);
        fflush(g_c.mem_csv);
    }
}

static int name_is(const char *n, const char *k) { return n && k && strstr(n, k) != NULL; }

int e10a31c_unimpl_policy(void *uc, uint32_t slot, const char *name, int32_t *out_r0) {
    const char *cls = "NEEDS_IMPLEMENTATION";
    int32_t r0 = -1;
    ensure_enabled();
    (void)uc;
    /* Only proven void/no-op may return success without initializing outputs. */
    if (name_is(name, "stopShake") || name_is(name, "stopSound") ||
        name_is(name, "menuRelease") || name_is(name, "winRelease") ||
        name_is(name, "dialogRelease") || name_is(name, "textRelease") ||
        name_is(name, "editRelease") || name_is(name, "findStop")) {
        cls = "PROVEN_VOID_NOOP";
        r0 = 0;
    } else if (name_is(name, "getCharBitmap") || name_is(name, "mr_plat") ||
               name_is(name, "TestCom") || name_is(name, "md5") || name_is(name, "sms") ||
               name_is(name, "DrawPoint") || name_is(name, "BitmapCheck") ||
               name_is(name, "wstrlen") || name_is(name, "registerAPP") ||
               name_is(name, "EffSetCon") || name_is(name, "c2u") || name_is(name, "_mr_div") ||
               name_is(name, "_mr_mod") || name_is(name, "readFile") ||
               name_is(name, "getScreenInfo") || name_is(name, "winCreate") ||
               name_is(name, "menuCreate") || name_is(name, "menuShow") ||
               name_is(name, "menuSetItem") || name_is(name, "menuRefresh") ||
               name_is(name, "sendSms") || name_is(name, "mr_call") ||
               name_is(name, "getNetworkID") || name_is(name, "connectWAP") ||
               name_is(name, "findStart") || name_is(name, "findGetNext") ||
               name_is(name, "mr_getTime") || name_is(name, "getDatetime") ||
               name_is(name, "getUserInfo") || name_is(name, "mr_sleep") ||
               name_is(name, "mr_ferrno")) {
        cls = "DIAGNOSTIC_SOFT_RETURN_ONLY";
        r0 = -1; /* failure, not false success */
    } else {
        cls = "DIAGNOSTIC_SOFT_RETURN_ONLY";
        r0 = -1;
    }
    g_c.unimpl_seq++;
    if (!g_c.unimpl_csv && (env1("JJFB_E10A31C_UNKNOWN_API") || g_c.enabled)) {
        g_c.unimpl_csv =
            open_csv("JJFB_E10A31C_UNIMPL_CSV", "reports/e10a31c_unknown_platform_api_trace.csv",
                     "run_id,seq,slot,api,r0_policy,class,depth,kind,note\n");
    }
    if (g_c.unimpl_csv) {
        fprintf(g_c.unimpl_csv, "%llu,%u,0x%X,\"%s\",%d,%s,%u,%s,policy\n", g_c.run_id,
                g_c.unimpl_seq, slot, name ? name : "?", (int)r0, cls, g_c.st.depth,
                kind_name(g_c.st.outer_kind));
        fflush(g_c.unimpl_csv);
    }
    printf("[JJFB_E10A31C_UNIMPL] api=%s slot=0x%X class=%s r0=%d depth=%u "
           "evidence=OBSERVED\n",
           name ? name : "?", slot, cls, (int)r0, g_c.st.depth);
    fflush(stdout);
    if (out_r0) *out_r0 = r0;
    return 1; /* allow soft return with policy r0 */
}

void e10a31c_note_last_bridge_api(const char *api) {
    ensure_enabled();
    if (!api) return;
    snprintf(g_c.last_bridge_api, sizeof(g_c.last_bridge_api), "%s", api);
}

static const char *win_exit_class(unsigned long code) {
    switch (code) {
    case 0xC0000005ul: return "ACCESS_VIOLATION";
    case 0xC00000FDul: return "STACK_OVERFLOW";
    case 0xC000001Dul: return "ILLEGAL_INSTRUCTION";
    case 0xC0000374ul: return "HEAP_CORRUPTION";
    case 0: return "NORMAL_EXIT";
    default: return "OTHER";
    }
}

void e10a31c_note_process_exit(int exit_code, const char *reason) {
    ensure_enabled();
    if (!g_c.enabled) return;
    if (!g_c.exit_csv) {
        g_c.exit_csv =
            open_csv("JJFB_E10A31C_EXIT_CSV", "reports/e10a31c_process_exit_trace.csv",
                     "run_id,exit_code,exit_class,reason,last_api,last_kind,depth,"
                     "pc,lr,sp,r9,init_complete,note\n");
    }
    if (g_c.exit_csv) {
        fprintf(g_c.exit_csv,
                "%llu,%d,%s,\"%s\",\"%s\",%s,%u,0x%X,0x%X,0x%X,0x%X,%d,process_exit\n",
                g_c.run_id, exit_code, win_exit_class((unsigned long)(unsigned)exit_code),
                reason ? reason : "?", g_c.last_bridge_api[0] ? g_c.last_bridge_api : "?",
                kind_name(g_c.st.outer_kind), g_c.st.depth, g_c.last_guest_pc, g_c.last_guest_lr,
                g_c.last_guest_sp, g_c.last_guest_r9, g_c.init_complete);
        fflush(g_c.exit_csv);
    }
    printf("[JJFB_E10A31C_EXIT] code=%d class=%s reason=%s last_api=%s depth=%u "
           "init_complete=%d evidence=OBSERVED\n",
           exit_code, win_exit_class((unsigned long)(unsigned)exit_code),
           reason ? reason : "?", g_c.last_bridge_api[0] ? g_c.last_bridge_api : "?",
           g_c.st.depth, g_c.init_complete);
    fflush(stdout);
    if (exit_code == 0)
        e10a31c_mark_milestone("NO_NATIVE_CRASH_PROCESS_EXIT_NORMAL", reason ? reason : "exit0");
}

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static LONG WINAPI e10a31c_unhandled(EXCEPTION_POINTERS *info) {
    DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    void *addr =
        info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : NULL;
    ensure_enabled();
    if (!g_c.exc_csv) {
        g_c.exc_csv =
            open_csv("JJFB_E10A31C_EXC_CSV", "reports/e10a31c_native_exception_trace.csv",
                     "run_id,exception_code,fault_addr,tid,last_api,last_kind,depth,"
                     "pc,lr,sp,r9,note\n");
    }
    if (g_c.exc_csv) {
        fprintf(g_c.exc_csv,
                "%llu,0x%lX,%p,%lu,\"%s\",%s,%u,0x%X,0x%X,0x%X,0x%X,native_exception\n",
                g_c.run_id, (unsigned long)code, addr, (unsigned long)GetCurrentThreadId(),
                g_c.last_bridge_api[0] ? g_c.last_bridge_api : "?", kind_name(g_c.st.outer_kind),
                g_c.st.depth, g_c.last_guest_pc, g_c.last_guest_lr, g_c.last_guest_sp,
                g_c.last_guest_r9);
        fflush(g_c.exc_csv);
    }
    printf("[JJFB_E10A31C_NATIVE_EXCEPTION] code=0x%lX addr=%p class=%s last_api=%s "
           "depth=%u pc=0x%X evidence=OBSERVED\n",
           (unsigned long)code, addr, win_exit_class((unsigned long)code),
           g_c.last_bridge_api[0] ? g_c.last_bridge_api : "?", g_c.st.depth, g_c.last_guest_pc);
    fflush(stdout);
    e10a31c_mark_milestone("MEM_GET_HOST_CRASH", win_exit_class((unsigned long)code));
    return EXCEPTION_CONTINUE_SEARCH;
}

void e10a31c_install_crash_recorder(void) {
    ensure_enabled();
    if (!g_c.enabled || g_c.crash_filter_installed) return;
    SetUnhandledExceptionFilter(e10a31c_unhandled);
    g_c.crash_filter_installed = 1;
    printf("[JJFB_E10A31C] crash_recorder=installed evidence=DOCUMENTED\n");
    fflush(stdout);
}
#else
void e10a31c_install_crash_recorder(void) { ensure_enabled(); }
#endif
