#include "gwy_launcher/e10a31d_provenance.h"

#include "gwy_launcher/e10a31_gamelist_context.h"
#include "gwy_launcher/e10a31c_dispatch.h"
#include "gwy_launcher/e10a31f_failsite.h"
#include "gwy_launcher/e10a31g_strcmp.h"
#include "gwy_launcher/e10a31h_smscfg.h"
#include "gwy_launcher/e10a31j_smscfg_long.h"
#include "gwy_launcher/e10a31l_config_map.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/package_metadata.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define E10A31D_HIST_MAX 512
#define E10A31D_INSN_MAX 20000
#define E10A31D_CALL_MAX 2048
#define E10A31D_API_MAX 256
#define E10A31D_APPINFO_READ_MAX 64

typedef struct {
    unsigned long long run_id;
    uint64_t init_epoch;
    char source[24];
    uint32_t helper;
    uint32_t method;
    uint32_t caller_pc;
    uint32_t caller_lr;
    uint32_t entry_pc;
    uint32_t p_guest;
    uint32_t chunk;
    uint32_t erw;
    uint32_t input;
    uint32_t input_len;
    int32_t ret;
    uint64_t insn_at_enter;
    int timer_registered;
    int event_handler_registered;
    int platform_state_allocated;
    uint32_t previous_method0_count;
} HistRow;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t lr;
    uint32_t sp;
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r9;
    uint32_t size;
    uint8_t bytes[8];
    char kind[24];
    uint32_t target;
    int32_t r0_delta;
} InsnRow;

typedef struct {
    uint32_t seq;
    uint32_t caller_pc;
    uint32_t target;
    uint32_t lr;
    uint32_t r0_in;
    int32_t r0_out;
    char note[48];
} CallRow;

typedef struct {
    uint32_t seq;
    char api[72];
    uint32_t slot;
    int32_t r0;
    uint32_t pc;
    uint32_t lr;
} ApiRow;

typedef struct {
    uint32_t seq;
    uint32_t addr;
    uint32_t size;
    uint32_t value;
    uint32_t pc;
    char field[16];
} AppinfoReadRow;

static struct {
    int known;
    int enabled;
    int history;
    int m0_trace;
    int appinfo;
    int recording; /* after GAMELIST_EXT_FIRST_PC */
    unsigned long long run_id;
    uint64_t init_epoch;
    uint64_t insn_count_global;
    uint32_t method0_count;
    uint32_t gamelist_helper;
    uint32_t pkg_appid;
    uint32_t pkg_appver;
    int pkg_meta_ok;
    char pkg_internal[32];
    char env_appid[32];
    char env_appver[32];

    HistRow hist[E10A31D_HIST_MAX];
    uint32_t hist_n;
    FILE *hist_csv;

    /* active helper call */
    int in_helper;
    E10a31dSource cur_source;
    uint32_t cur_helper;
    uint32_t cur_method;
    uint32_t cur_p;
    uint32_t cur_erw;
    uint32_t cur_input;
    uint32_t cur_input_len;
    uint32_t cur_caller_pc;
    uint32_t cur_caller_lr;
    uint32_t prev_m0_at_enter;

    /* method0 trace */
    int m0_active;
    uint32_t m0_helper;
    uint32_t m0_entry;
    uint64_t m0_insn_budget;
    uint64_t m0_insn_used;
#ifdef GWY_HAVE_UNICORN
    uc_hook m0_hook;
    uc_hook m0_mem_hook;
    int m0_hook_armed;
    int m0_mem_armed;
#endif
    InsnRow *insn_rows;
    uint32_t insn_n;
    CallRow call_rows[E10A31D_CALL_MAX];
    uint32_t call_n;
    ApiRow api_rows[E10A31D_API_MAX];
    uint32_t api_n;
    AppinfoReadRow appinfo_reads[E10A31D_APPINFO_READ_MAX];
    uint32_t appinfo_read_n;
    uint32_t appinfo_guest;
    int first_failure_proven;
    uint32_t first_fail_seq;
    uint32_t first_fail_pc;
    uint32_t first_fail_lr;
    char first_fail_class[48];
    char first_fail_evidence[160];
    uint32_t last_bl_pc;
    uint32_t last_bl_target;
    int32_t last_bl_ret;
    int last_bl_ret_valid;
    uint32_t last_neg1_imm_pc;
    int last_neg1_imm_valid;
    uint32_t prev_sampled_r0;
    int prev_sampled_r0_valid;
    FILE *insn_csv;
    FILE *call_csv;
    FILE *prov_csv;
    FILE *appinfo_csv;
    FILE *abi_csv;
} g_d;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31D_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A31_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
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

static const char *src_name(E10a31dSource s) {
    switch (s) {
    case E10A31D_SRC_HOST_FAST_REAL: return "host_fast_real";
    case E10A31D_SRC_TIMER: return "timer";
    case E10A31D_SRC_PLATFORM_CALLBACK: return "platform_callback";
    default: return "native_guest";
    }
}

static void load_package_metadata(void) {
    const GwyPackageMetadata *m;
    const char *aid = getenv("GWY_PACKAGE_APPID");
    const char *aver = getenv("GWY_PACKAGE_APPVER");
    g_d.pkg_meta_ok = 0;
    g_d.pkg_appid = 0;
    g_d.pkg_appver = 0;
    g_d.pkg_internal[0] = 0;
    snprintf(g_d.env_appid, sizeof(g_d.env_appid), "%s", (aid && aid[0]) ? aid : "");
    snprintf(g_d.env_appver, sizeof(g_d.env_appver), "%s", (aver && aver[0]) ? aver : "");
    /* Prefer live package-scoped registry (E10A-3.1e); fall back to gamelist MRP open. */
    m = gwy_package_registry_active_metadata();
    if (m && m->valid) {
        g_d.pkg_appid = m->appid;
        g_d.pkg_appver = m->appver;
        snprintf(g_d.pkg_internal, sizeof(g_d.pkg_internal), "%.31s", m->internal_name);
        g_d.pkg_meta_ok = 1;
        return;
    }
    {
        const char *root = getenv("GWY_RESOURCE_ROOT");
        char path[1100];
        MrpArchive *arch = NULL;
        LauncherError err;
        if (!root || !root[0]) return;
        snprintf(path, sizeof(path), "%s/gwy/gamelist.mrp", root);
        memset(&err, 0, sizeof(err));
        if (mrp_archive_open(path, &arch, &err) != L_OK || !arch) return;
        g_d.pkg_appid = arch->appid_le;
        g_d.pkg_appver = arch->appver_le;
        snprintf(g_d.pkg_internal, sizeof(g_d.pkg_internal), "%.31s", arch->internal_name);
        g_d.pkg_meta_ok = 1;
        mrp_archive_close(arch);
    }
}

static void ensure_enabled(void) {
    if (g_d.known) return;
    g_d.known = 1;
    g_d.enabled = env1("JJFB_E10A31D_MODE") || env1("JJFB_E10A31D_HISTORY") ||
                  env1("JJFB_E10A31D_METHOD0_TRACE") || env1("JJFB_E10A31D_APPINFO");
    g_d.history = env1("JJFB_E10A31D_HISTORY") || env1("JJFB_E10A31D_MODE");
    g_d.m0_trace = env1("JJFB_E10A31D_METHOD0_TRACE") || env1("JJFB_E10A31D_MODE");
    g_d.appinfo = env1("JJFB_E10A31D_APPINFO") || env1("JJFB_E10A31D_MODE");
    g_d.run_id = run_id_now();
    g_d.init_epoch = 1;
    g_d.m0_insn_budget = 20000;
    {
        const char *b = getenv("JJFB_E10A31D_INSN_BUDGET");
        if (b && b[0]) g_d.m0_insn_budget = strtoull(b, NULL, 10);
        if (g_d.m0_insn_budget > E10A31D_INSN_MAX) g_d.m0_insn_budget = E10A31D_INSN_MAX;
    }
    if (g_d.enabled) load_package_metadata();
}

int e10a31d_enabled(void) {
    ensure_enabled();
    return g_d.enabled;
}
int e10a31d_history_enabled(void) {
    ensure_enabled();
    return g_d.enabled && g_d.history;
}
int e10a31d_method0_trace_enabled(void) {
    ensure_enabled();
    return g_d.enabled && g_d.m0_trace;
}
int e10a31d_appinfo_enabled(void) {
    ensure_enabled();
    return g_d.enabled && g_d.appinfo;
}

void e10a31d_reset(void) {
    if (g_d.hist_csv) fclose(g_d.hist_csv);
    if (g_d.insn_csv) fclose(g_d.insn_csv);
    if (g_d.call_csv) fclose(g_d.call_csv);
    if (g_d.prov_csv) fclose(g_d.prov_csv);
    if (g_d.appinfo_csv) fclose(g_d.appinfo_csv);
    if (g_d.abi_csv) fclose(g_d.abi_csv);
    if (g_d.insn_rows) free(g_d.insn_rows);
    memset(&g_d, 0, sizeof(g_d));
}

void e10a31d_mark_milestone(const char *name, const char *note) {
    ensure_enabled();
    if (!g_d.enabled || !name) return;
    printf("[JJFB_E10A31D] milestone=%s note=%s evidence=OBSERVED\n", name,
           note ? note : "");
    fflush(stdout);
}

void e10a31d_on_ext_first_pc(void) {
    ensure_enabled();
    if (!g_d.enabled) return;
    if (g_d.recording) return;
    g_d.recording = 1;
    g_d.init_epoch++;
    e10a31d_mark_milestone("E10A31D_HISTORY_ARMED", "after_GAMELIST_EXT_FIRST_PC");
}

static int is_gamelist_helper(uint32_t helper) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    uint32_t h = helper & ~1u;
    if (!helper) return 0;
    if (g_d.gamelist_helper && (g_d.gamelist_helper & ~1u) == h) return 1;
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return 0;
    m = module_registry_find_by_helper(reg, helper);
    if (!m) m = module_registry_find_by_helper(reg, h);
    if (!m) m = module_registry_find_by_helper(reg, h | 1u);
    if (!m) return 0;
    if ((m->resolved_name[0] && strstr(m->resolved_name, "gamelist")) ||
        (m->requested_name[0] && strstr(m->requested_name, "gamelist")) ||
        (m->package_path[0] && strstr(m->package_path, "gamelist"))) {
        g_d.gamelist_helper = helper;
        return 1;
    }
    return 0;
}

static uint32_t peek_u32(void *uc, uint32_t addr, int *ok) {
    uint32_t v = 0;
    if (ok) *ok = 0;
    if (!uc || !addr) return 0;
    if (guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &v)) {
        if (ok) *ok = 1;
        return v;
    }
    return 0;
}

static void ensure_hist_csv(void) {
    if (g_d.hist_csv || !g_d.history) return;
    g_d.hist_csv = open_csv(
        "JJFB_E10A31D_HIST_CSV", "reports/e10a31d_helper_call_history.csv",
        "run_id,init_epoch,source,helper,method,caller_pc,caller_lr,entry_pc,P,chunk,ERW,"
        "input,input_len,return_value,insn_count,timer_registered,event_handler_registered,"
        "platform_state_allocated,previous_method0_count\n");
}

static void flush_hist_verdicts(void) {
    uint32_t i, n0 = 0, n0_native = 0, n0_fast = 0, first_idx = 0;
    int saw0 = 0;
    for (i = 0; i < g_d.hist_n; i++) {
        if (g_d.hist[i].method != 0) continue;
        if (!saw0) {
            saw0 = 1;
            first_idx = i;
        }
        n0++;
        if (strcmp(g_d.hist[i].source, "native_guest") == 0) n0_native++;
        if (strcmp(g_d.hist[i].source, "host_fast_real") == 0) n0_fast++;
    }
    if (!saw0) {
        e10a31d_mark_milestone("GAMELIST_NO_NATURAL_METHOD0_OBSERVED", "no_method0_in_history");
        return;
    }
    if (n0_native > 0 && n0_fast > 0) {
        e10a31d_mark_milestone("GAMELIST_METHOD0_DUPLICATE_CALL", "native_then_or_with_fast");
    } else if (n0 > 1) {
        e10a31d_mark_milestone("GAMELIST_METHOD0_DUPLICATE_CALL", "multiple_method0");
    } else if (strcmp(g_d.hist[first_idx].source, "host_fast_real") == 0) {
        e10a31d_mark_milestone("GAMELIST_METHOD0_FIRST_CALL", "first_is_host_fast_real");
        e10a31d_mark_milestone("GAMELIST_NO_NATURAL_METHOD0_OBSERVED", "only_fast_real");
    } else {
        e10a31d_mark_milestone("GAMELIST_METHOD0_FIRST_CALL", g_d.hist[first_idx].source);
    }
    /* Partial init: saw 6 or 8 before first method0 from non-fast source, or before fast. */
    {
        int saw6 = 0, saw8 = 0;
        for (i = 0; i < first_idx; i++) {
            if (g_d.hist[i].method == 6) saw6 = 1;
            if (g_d.hist[i].method == 8) saw8 = 1;
        }
        if ((saw6 || saw8) && strcmp(g_d.hist[first_idx].source, "host_fast_real") == 0 &&
            n0_native == 0) {
            /* 6/8 immediately before FAST method0 is the reconstructed sequence itself. */
        } else if (saw6 || saw8) {
            e10a31d_mark_milestone("GAMELIST_PARTIAL_INIT_BEFORE_FAST_SEQUENCE",
                                   saw6 && saw8 ? "saw_6_and_8" : (saw6 ? "saw_6" : "saw_8"));
        }
    }
}

void e10a31d_helper_enter(void *uc, E10a31dSource source, uint32_t helper, uint32_t method,
                          uint32_t p_guest, uint32_t erw, uint32_t input, uint32_t input_len,
                          uint32_t caller_pc, uint32_t caller_lr) {
    uint32_t chunk = 0;
    int ok = 0;
    ensure_enabled();
    if (!g_d.enabled) return;
    if (!g_d.recording && !g_d.appinfo && !g_d.m0_trace) return;
    /* Always accept FAST_REAL / appinfo dumps even if first-PC race. */
    if (!g_d.recording && source == E10A31D_SRC_HOST_FAST_REAL) g_d.recording = 1;
    if (!is_gamelist_helper(helper) && source != E10A31D_SRC_HOST_FAST_REAL &&
        source != E10A31D_SRC_TIMER) {
        return;
    }
    /* Timer: only record once gamelist helper is known and matches. */
    if (source == E10A31D_SRC_TIMER && g_d.gamelist_helper &&
        (helper & ~1u) != (g_d.gamelist_helper & ~1u)) {
        return;
    }
    if (source == E10A31D_SRC_HOST_FAST_REAL) g_d.gamelist_helper = helper;

    g_d.in_helper = 1;
    g_d.cur_source = source;
    g_d.cur_helper = helper;
    g_d.cur_method = method;
    g_d.cur_p = p_guest;
    g_d.cur_erw = erw;
    g_d.cur_input = input;
    g_d.cur_input_len = input_len;
    g_d.cur_caller_pc = caller_pc;
    g_d.cur_caller_lr = caller_lr;
    g_d.prev_m0_at_enter = g_d.method0_count;

    if (p_guest) chunk = peek_u32(uc, p_guest + 0xCu, &ok);

    if (g_d.history && g_d.hist_n < E10A31D_HIST_MAX) {
        HistRow *r = &g_d.hist[g_d.hist_n];
        memset(r, 0, sizeof(*r));
        r->run_id = g_d.run_id;
        r->init_epoch = g_d.init_epoch;
        snprintf(r->source, sizeof(r->source), "%s", src_name(source));
        r->helper = helper;
        r->method = method;
        r->caller_pc = caller_pc;
        r->caller_lr = caller_lr;
        r->entry_pc = helper & ~1u;
        r->p_guest = p_guest;
        r->chunk = chunk;
        r->erw = erw;
        r->input = input;
        r->input_len = input_len;
        r->ret = 0x7fffffff; /* pending */
        r->insn_at_enter = g_d.insn_count_global;
        r->timer_registered = e10a31_timer_arm_observed();
        r->event_handler_registered = 0;
        r->platform_state_allocated = (p_guest && erw) ? 1 : 0;
        r->previous_method0_count = g_d.method0_count;
        g_d.hist_n++;
    }

    printf("[JJFB_E10A31D_HELPER] phase=ENTER source=%s helper=0x%X method=%u P=0x%X "
           "erw=0x%X input=0x%X len=%u prev_m0=%u evidence=OBSERVED\n",
           src_name(source), helper, method, p_guest, erw, input, input_len,
           g_d.method0_count);
    fflush(stdout);

    if (method == 0u) e10a31j_on_method0_enter(uc, helper);
    if (method == 0u) e10a31l_on_method0_enter(uc, helper);
    if (method == 0u && g_d.m0_trace) e10a31d_method0_trace_arm(uc, helper);
}

void e10a31d_helper_return(void *uc, uint32_t helper, uint32_t method, int32_t ret) {
    ensure_enabled();
    if (!g_d.enabled || !g_d.in_helper) return;
    (void)helper;

    if (method == 0u) {
        g_d.method0_count++;
        if (g_d.m0_trace) e10a31d_method0_trace_disarm(uc);
        e10a31f_on_method0_return(uc, helper, ret);
        e10a31g_on_method0_return(uc, helper, ret);
        e10a31h_on_method0_return(uc, helper, ret);
        e10a31j_on_method0_return(uc, helper, ret);
        e10a31l_on_method0_return(uc, helper, ret);
    }

    if (g_d.history && g_d.hist_n > 0) {
        HistRow *r = &g_d.hist[g_d.hist_n - 1];
        if (r->method == method && r->ret == (int32_t)0x7fffffff) r->ret = ret;
        ensure_hist_csv();
        if (g_d.hist_csv) {
            fprintf(g_d.hist_csv,
                    "%llu,%llu,%s,0x%X,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%d,%llu,%d,%d,"
                    "%d,%u\n",
                    r->run_id, (unsigned long long)r->init_epoch, r->source, r->helper, r->method,
                    r->caller_pc, r->caller_lr, r->entry_pc, r->p_guest, r->chunk, r->erw,
                    r->input, r->input_len, (int)r->ret, (unsigned long long)r->insn_at_enter,
                    r->timer_registered, r->event_handler_registered,
                    r->platform_state_allocated, r->previous_method0_count);
            fflush(g_d.hist_csv);
        }
    }

    printf("[JJFB_E10A31D_HELPER] phase=RETURN source=%s helper=0x%X method=%u ret=%d "
           "evidence=OBSERVED\n",
           src_name(g_d.cur_source), g_d.cur_helper, method, (int)ret);
    fflush(stdout);

    if (method == 6u && g_d.appinfo)
        e10a31d_after_code6(uc, g_d.cur_erw, g_d.cur_input_len, ret);
    if (method == 8u && g_d.appinfo)
        e10a31d_after_code8(uc, g_d.cur_erw, g_d.cur_input, g_d.cur_input_len, ret);

    if (method == 0u && g_d.abi_csv == NULL && g_d.history) {
        g_d.abi_csv =
            open_csv("JJFB_E10A31D_ABI_CSV", "reports/e10a31d_helper_abi_compare.csv",
                     "run_id,source,method,input,input_len,ret,mythroad_mini_input,"
                     "mythroad_mini_input_len,note\n");
        if (g_d.abi_csv) {
            fprintf(g_d.abi_csv,
                    "%llu,%s,0,0x%X,%u,%d,filebuf,2011,"
                    "mythroad_mini_passes_filebuf_not_null;dispatcher_must_confirm_use\n",
                    g_d.run_id, src_name(g_d.cur_source), g_d.cur_input, g_d.cur_input_len,
                    (int)ret);
            fflush(g_d.abi_csv);
        }
    }

    g_d.in_helper = 0;
    if (method == 0u) flush_hist_verdicts();
}

/* ---------- Lane C: method0 instruction trace ---------- */

static void classify_and_emit_provenance(int32_t helper_ret) {
    const char *cls = "RETURN_NEG1_UNKNOWN";
    char ev[160];
    ensure_enabled();
    if (!g_d.prov_csv) {
        g_d.prov_csv = open_csv(
            "JJFB_E10A31D_PROV_CSV", "reports/e10a31d_method0_return_provenance.csv",
            "run_id,first_failure_seq,failure_pc,failure_lr,enclosing_function,failing_callee,"
            "platform_api,input_args,return_value,branch_pc,predicate,final_helper_return_path,"
            "evidence,class\n");
    }
    snprintf(ev, sizeof(ev), "helper_ret=%d", (int)helper_ret);
    if (g_d.prev_m0_at_enter > 0) {
        cls = "RETURN_NEG1_FROM_DUPLICATE_INIT";
        snprintf(ev, sizeof(ev), "previous_method0_count=%u", g_d.prev_m0_at_enter);
    } else if (g_d.first_failure_proven) {
        cls = g_d.first_fail_class;
        snprintf(ev, sizeof(ev), "%s", g_d.first_fail_evidence);
    } else if (g_d.last_neg1_imm_valid) {
        cls = "RETURN_NEG1_IMMEDIATE";
        snprintf(ev, sizeof(ev), "mov_mvn_neg1_pc=0x%X", g_d.last_neg1_imm_pc);
        g_d.first_fail_pc = g_d.last_neg1_imm_pc;
        g_d.first_failure_proven = 1;
    } else if (g_d.last_bl_ret_valid && g_d.last_bl_ret < 0) {
        cls = "RETURN_NEG1_FROM_CALLEE";
        snprintf(ev, sizeof(ev), "bl_pc=0x%X target=0x%X ret=%d", g_d.last_bl_pc,
                 g_d.last_bl_target, (int)g_d.last_bl_ret);
        g_d.first_fail_pc = g_d.last_bl_pc;
        g_d.first_failure_proven = 1;
    } else if (g_d.api_n > 0 && g_d.api_rows[g_d.api_n - 1].r0 < 0) {
        cls = "RETURN_NEG1_FROM_PLATFORM_API";
        snprintf(ev, sizeof(ev), "api=%s r0=%d", g_d.api_rows[g_d.api_n - 1].api,
                 (int)g_d.api_rows[g_d.api_n - 1].r0);
        g_d.first_failure_proven = 1;
    }

    if (helper_ret < 0) {
        e10a31d_mark_milestone("MRC_INIT_FIRST_FAILURE_FOUND", cls);
        e10a31d_mark_milestone("MRC_INIT_RETURN_PROVENANCE_COMPLETE", cls);
        if (strcmp(cls, "RETURN_NEG1_FROM_PLATFORM_API") == 0)
            e10a31d_mark_milestone("MRC_INIT_PLATFORM_CALL_FAILED", ev);
        else if (strcmp(cls, "RETURN_NEG1_IMMEDIATE") == 0 ||
                 strcmp(cls, "RETURN_NEG1_FROM_GLOBAL_STATE") == 0 ||
                 strcmp(cls, "RETURN_NEG1_FROM_MISSING_APPINFO") == 0)
            e10a31d_mark_milestone("MRC_INIT_INTERNAL_PRECONDITION_FAILED", cls);
    }

    if (g_d.prov_csv) {
        fprintf(g_d.prov_csv,
                "%llu,%u,0x%X,0x%X,method0_dispatch,0x%X,\"%s\",input=0x%X_len=%u,%d,0x%X,"
                "\"%s\",helper_r0,%s,%s\n",
                g_d.run_id, g_d.first_fail_seq, g_d.first_fail_pc, g_d.first_fail_lr,
                g_d.last_bl_target, g_d.api_n ? g_d.api_rows[g_d.api_n - 1].api : "",
                g_d.cur_input, g_d.cur_input_len, (int)helper_ret, g_d.first_fail_pc, cls, ev,
                cls);
        fflush(g_d.prov_csv);
    }
}

#ifdef GWY_HAVE_UNICORN
static int decode_thumb_bl(uint16_t h0, uint16_t h1, uint32_t pc, uint32_t *tgt, int *is_blx) {
    int32_t imm32;
    int s, j1, j2, i1, i2;
    if ((h0 & 0xF800) != 0xF000 || (h1 & 0xC000) != 0xC000) return 0;
    s = (h0 >> 10) & 1;
    j1 = (h1 >> 13) & 1;
    j2 = (h1 >> 11) & 1;
    i1 = (~(j1 ^ s)) & 1;
    i2 = (~(j2 ^ s)) & 1;
    imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | ((h0 & 0x3FF) << 12) | ((h1 & 0x7FF) << 1);
    if (imm32 & (1 << 24)) imm32 |= ~((1 << 25) - 1);
    *tgt = (uint32_t)((int32_t)pc + 4 + imm32);
    *is_blx = ((h1 & 0x1000) == 0);
    if (!*is_blx) *tgt |= 1u;
    return 1;
}

static int is_neg1_imm_thumb(uint16_t h0, uint16_t h1, int size) {
    /* MOV r0, #imm8 with imm==0xFF in low encoding rare; MVN r0,#0 = 0x43C0 (rs=r0,rd=r0)?
     * Thumb MVN rd,rm: 010000 1111 Rm Rd -> 0x43C0 | (rm<<3) | rd  for rm=0 rd=0 => 0x43C0
     * Actually MVN r0,r0 is not -1. MOV/MVN immediate Thumb-2:
     * MVN.W r0, #0 => produces 0xFFFFFFFF
     * Encoding T1 MVN: 0100001111 mmddd
     * Common pattern: movs r0,#0; mvns r0,r0  or  mov.w r0,#0xFFFFFFFF
     */
    (void)h1;
    if (size == 2) {
        /* mvns r0, r0 鈫?not necessarily; movs r0, #0 then separate.
         * ldr r0, [pc, #imm] loading 0xFFFFFFFF handled elsewhere.
         * movs r0, #imm: 00100 000 iiiiiiii 鈫?0x2000|imm 鈥?imm 0..255, not -1.
         */
        if ((h0 & 0xFFC0) == 0x4240) return 0; /* negs */
        /* Thumb-1 cannot encode -1 as MOV imm. */
        return 0;
    }
    /* Thumb-2 MOVW/MOVT or MVN immediate: rough detect MVN rd,#imm with result -1 */
    if ((h0 & 0xFBE0) == 0xF06F && (h1 & 0x8000) == 0x0000) {
        /* MVN.W Rd, #const 鈥?if const expands to 0 => Rd=-1 */
        return 1; /* treat MVN.W as candidate; refine via runtime r0 */
    }
    return 0;
}

static void m0_on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r9 = 0, cpsr = 0;
    uint8_t raw[8];
    uint16_t h0 = 0, h1 = 0;
    int is_bl = 0, is_blx = 0;
    uint32_t tgt = 0;
    InsnRow *row;
    (void)user_data;
    if (!g_d.m0_active || g_d.first_failure_proven) return;
    if (g_d.m0_insn_used >= g_d.m0_insn_budget) return;
    g_d.m0_insn_used++;
    g_d.insn_count_global++;

    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    memset(raw, 0, sizeof(raw));
    if (size > 8) size = 8;
    (void)uc_mem_read(uc, address, raw, size);
    if (size >= 2) h0 = (uint16_t)(raw[0] | (raw[1] << 8));
    if (size >= 4) h1 = (uint16_t)(raw[2] | (raw[3] << 8));

    if (size >= 4 && decode_thumb_bl(h0, h1, pc, &tgt, &is_blx)) is_bl = 1;
    else if (size == 2 && (h0 & 0xFF80) == 0x4780) {
        /* BLX Rm */
        uint32_t rm = (h0 >> 3) & 0xF;
        uint32_t rv = 0;
        uc_reg_read(uc, UC_ARM_REG_R0 + (int)rm, &rv);
        tgt = rv;
        is_bl = 1;
        is_blx = 1;
    }

    if (!g_d.insn_rows) {
        g_d.insn_rows = (InsnRow *)calloc(E10A31D_INSN_MAX, sizeof(InsnRow));
    }
    if (g_d.insn_rows && g_d.insn_n < E10A31D_INSN_MAX) {
        row = &g_d.insn_rows[g_d.insn_n];
        memset(row, 0, sizeof(*row));
        row->seq = g_d.insn_n + 1;
        row->pc = pc;
        row->lr = lr;
        row->sp = sp;
        row->r0 = r0;
        row->r1 = r1;
        row->r2 = r2;
        row->r3 = r3;
        row->r9 = r9;
        row->size = size;
        memcpy(row->bytes, raw, size);
        if (is_bl) {
            snprintf(row->kind, sizeof(row->kind), "%s", is_blx ? "BLX" : "BL");
            row->target = tgt;
        } else if (e10a31f_is_neg1_sentinel_store(pc, raw, size)) {
            snprintf(row->kind, sizeof(row->kind), "NEG1_SENTINEL");
        } else if (is_neg1_imm_thumb(h0, h1, (int)size)) {
            snprintf(row->kind, sizeof(row->kind), "NEG1_IMM_CAND");
        } else {
            snprintf(row->kind, sizeof(row->kind), "INSN");
        }
        g_d.insn_n++;
    }

    /* Fill r0_out for previous BL when we land at its LR. */
    if (g_d.call_n > 0) {
        CallRow *prevc = &g_d.call_rows[g_d.call_n - 1];
        if (prevc->lr == pc || (prevc->lr | 1u) == (pc | 1u)) {
            prevc->r0_out = (int32_t)r0;
        }
    }

    if (is_bl && g_d.call_n < E10A31D_CALL_MAX) {
        CallRow *c = &g_d.call_rows[g_d.call_n++];
        memset(c, 0, sizeof(*c));
        c->seq = g_d.call_n;
        c->caller_pc = pc;
        c->target = tgt;
        c->lr = pc + size;
        c->r0_in = r0;
        snprintf(c->note, sizeof(c->note), "%s", is_blx ? "blx" : "bl");
        g_d.last_bl_pc = pc;
        g_d.last_bl_target = tgt;
        g_d.last_bl_ret_valid = 0;
    }

    e10a31f_on_method0_insn(uc, pc, lr, r0, r1, r2, r3, r4, r9, cpsr, raw, size);
    e10a31g_on_method0_insn(uc, pc, lr, r0, r1, r2, r3, r4, r9, cpsr, raw, size);
    e10a31h_on_method0_insn(uc, pc, lr, r0, r1, r2, r3, r4, r9, cpsr, raw, size);
    e10a31j_on_method0_insn(uc, pc, lr, r0, r1, r2, r3, r4, r9);
    e10a31l_on_method0_insn(uc, pc, lr, r0, r1, r2, r3, r4, r9, cpsr, raw, size);

    /* Detect R0 *becoming* -1 (edge), not remaining -1 across stores. */
    if (r0 == 0xFFFFFFFFu &&
        (!g_d.prev_sampled_r0_valid || g_d.prev_sampled_r0 != 0xFFFFFFFFu)) {
        if (g_d.insn_n >= 2) {
            InsnRow *prev = &g_d.insn_rows[g_d.insn_n - 2];
            /* E10A-3.1f: ignore known unconditional MVNS sentinel store at 0x2E1C24. */
            if (e10a31f_should_ignore_neg1_at(prev->pc, pc, prev->bytes, prev->size)) {
                e10a31d_mark_milestone("FAILSITE_2E1C24_IGNORED_AS_SENTINEL", "continue_trace");
                /* do not set first_failure_proven */
            } else if (strcmp(prev->kind, "BL") == 0 || strcmp(prev->kind, "BLX") == 0) {
                g_d.last_bl_ret = -1;
                g_d.last_bl_ret_valid = 1;
                if (!g_d.first_failure_proven) {
                    g_d.first_failure_proven = 1;
                    g_d.first_fail_seq = prev->seq;
                    g_d.first_fail_pc = prev->pc;
                    g_d.first_fail_lr = prev->lr;
                    snprintf(g_d.first_fail_class, sizeof(g_d.first_fail_class),
                             "RETURN_NEG1_FROM_CALLEE");
                    snprintf(g_d.first_fail_evidence, sizeof(g_d.first_fail_evidence),
                             "after_bl pc=0x%X target=0x%X r0=-1", prev->pc, prev->target);
                    e10a31d_mark_milestone("MRC_INIT_FIRST_FAILURE_FOUND",
                                           g_d.first_fail_class);
                    {
                        char n[48];
                        snprintf(n, sizeof(n), "pc=0x%X_%s", g_d.first_fail_pc,
                                 g_d.first_fail_class);
                        e10a31d_mark_milestone("MRC_INIT_TRUE_FAILURE_PC_FOUND", n);
                    }
                }
            } else {
                g_d.last_neg1_imm_pc = prev->pc;
                g_d.last_neg1_imm_valid = 1;
                if (!g_d.first_failure_proven) {
                    g_d.first_failure_proven = 1;
                    g_d.first_fail_seq = prev->seq;
                    g_d.first_fail_pc = prev->pc;
                    g_d.first_fail_lr = lr;
                    snprintf(g_d.first_fail_class, sizeof(g_d.first_fail_class),
                             "RETURN_NEG1_IMMEDIATE");
                    snprintf(g_d.first_fail_evidence, sizeof(g_d.first_fail_evidence),
                             "r0_became_-1_at_pc=0x%X after=0x%X", pc, prev->pc);
                    e10a31d_mark_milestone("MRC_INIT_FIRST_FAILURE_FOUND",
                                           g_d.first_fail_class);
                    {
                        char n[48];
                        snprintf(n, sizeof(n), "pc=0x%X_%s", g_d.first_fail_pc,
                                 g_d.first_fail_class);
                        e10a31d_mark_milestone("MRC_INIT_TRUE_FAILURE_PC_FOUND", n);
                    }
                }
            }
        }
    }
    g_d.prev_sampled_r0 = r0;
    g_d.prev_sampled_r0_valid = 1;

    (void)cpsr;
}

static void m0_on_mem_read(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                           int64_t value, void *user_data) {
    uint32_t pc = 0;
    uint32_t addr = (uint32_t)address;
    (void)type;
    (void)value;
    (void)user_data;
    if (!g_d.m0_active || !g_d.appinfo_guest) return;
    if (addr < g_d.appinfo_guest || addr >= g_d.appinfo_guest + 16u) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    if (g_d.appinfo_read_n < E10A31D_APPINFO_READ_MAX) {
        AppinfoReadRow *r = &g_d.appinfo_reads[g_d.appinfo_read_n++];
        uint32_t off = addr - g_d.appinfo_guest;
        memset(r, 0, sizeof(*r));
        r->seq = g_d.appinfo_read_n;
        r->addr = addr;
        r->size = (uint32_t)size;
        r->pc = pc;
        if (off < 4) snprintf(r->field, sizeof(r->field), "id");
        else if (off < 8) snprintf(r->field, sizeof(r->field), "ver");
        else if (off < 12) snprintf(r->field, sizeof(r->field), "sidName");
        else snprintf(r->field, sizeof(r->field), "ram");
        if (size == 4) {
            uint32_t v = 0;
            if (guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &v)) r->value = v;
        }
        printf("[JJFB_E10A31D_APPINFO_READ] field=%s addr=0x%X val=0x%X pc=0x%X "
               "evidence=OBSERVED\n",
               r->field, addr, r->value, pc);
        fflush(stdout);
        if (strcmp(r->field, "id") == 0 || strcmp(r->field, "ver") == 0) {
            if (r->value == 0)
                e10a31d_mark_milestone("RETURN_NEG1_FROM_MISSING_APPINFO", r->field);
        }
        if (strcmp(r->field, "sidName") == 0 && r->value == 0)
            e10a31d_mark_milestone("APPINFO_SIDNAME_NULL", "dereferenced_or_loaded");
    }
}
#endif /* GWY_HAVE_UNICORN */

void e10a31d_method0_trace_arm(void *uc, uint32_t helper) {
    ensure_enabled();
    if (!g_d.enabled || !g_d.m0_trace || !uc) return;
    g_d.m0_active = 1;
    g_d.m0_helper = helper;
    g_d.m0_entry = helper & ~1u;
    g_d.m0_insn_used = 0;
    g_d.insn_n = 0;
    g_d.call_n = 0;
    g_d.api_n = 0;
    g_d.first_failure_proven = 0;
    g_d.first_fail_seq = 0;
    g_d.first_fail_pc = 0;
    g_d.first_fail_lr = 0;
    g_d.first_fail_class[0] = 0;
    g_d.first_fail_evidence[0] = 0;
    g_d.last_bl_ret_valid = 0;
    g_d.last_neg1_imm_valid = 0;
    g_d.prev_sampled_r0 = 0;
    g_d.prev_sampled_r0_valid = 0;
    if (!g_d.insn_csv) {
        g_d.insn_csv = open_csv(
            "JJFB_E10A31D_INSN_CSV", "reports/e10a31d_method0_instruction_trace.csv",
            "run_id,seq,pc,lr,sp,r0,r1,r2,r3,r9,size,bytes,kind,target\n");
    }
    if (!g_d.call_csv) {
        g_d.call_csv = open_csv("JJFB_E10A31D_CALL_CSV", "reports/e10a31d_method0_call_tree.csv",
                                "run_id,seq,caller_pc,target,lr,r0_in,r0_out,note\n");
    }
#ifdef GWY_HAVE_UNICORN
    if (!g_d.m0_hook_armed) {
        if (uc_hook_add((uc_engine *)uc, &g_d.m0_hook, UC_HOOK_CODE, (void *)m0_on_code, NULL, 1,
                        0) == UC_ERR_OK)
            g_d.m0_hook_armed = 1;
    }
    if (g_d.appinfo_guest && !g_d.m0_mem_armed) {
        if (uc_hook_add((uc_engine *)uc, &g_d.m0_mem_hook, UC_HOOK_MEM_READ, (void *)m0_on_mem_read,
                        NULL, g_d.appinfo_guest, g_d.appinfo_guest + 15) == UC_ERR_OK)
            g_d.m0_mem_armed = 1;
    }
#else
    (void)uc;
    (void)helper;
#endif
    e10a31d_mark_milestone("GAMELIST_METHOD0_TARGET_IDENTIFIED", "trace_armed");
    printf("[JJFB_E10A31D] method0_trace=arm helper=0x%X budget=%llu evidence=DOCUMENTED\n",
           helper, (unsigned long long)g_d.m0_insn_budget);
    fflush(stdout);
}

void e10a31d_method0_trace_disarm(void *uc) {
    uint32_t i;
    int32_t helper_ret = 0;
#ifdef GWY_HAVE_UNICORN
    uint32_t r0 = 0;
#endif
    ensure_enabled();
    if (!g_d.m0_active) return;
#ifdef GWY_HAVE_UNICORN
    if (uc) uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0, &r0);
    helper_ret = (int32_t)r0;
    if (g_d.m0_hook_armed) {
        uc_hook_del((uc_engine *)uc, g_d.m0_hook);
        g_d.m0_hook_armed = 0;
        g_d.m0_hook = 0;
    }
    if (g_d.m0_mem_armed) {
        uc_hook_del((uc_engine *)uc, g_d.m0_mem_hook);
        g_d.m0_mem_armed = 0;
        g_d.m0_mem_hook = 0;
    }
#else
    (void)uc;
#endif
    g_d.m0_active = 0;

    /* Emit bounded insn CSV: BL/BLX + NEG1 + first 32 + around failure */
    if (g_d.insn_csv && g_d.insn_rows) {
        for (i = 0; i < g_d.insn_n; i++) {
            InsnRow *r = &g_d.insn_rows[i];
            int keep = 0;
            if (strcmp(r->kind, "BL") == 0 || strcmp(r->kind, "BLX") == 0) keep = 1;
            if (strcmp(r->kind, "NEG1_IMM_CAND") == 0) keep = 1;
            if (strcmp(r->kind, "NEG1_SENTINEL") == 0) keep = 1;
            if (i < 32) keep = 1;
            if (g_d.first_fail_seq &&
                r->seq + 8 >= g_d.first_fail_seq && r->seq <= g_d.first_fail_seq + 8)
                keep = 1;
            if (!keep) continue;
            fprintf(g_d.insn_csv,
                    "%llu,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%02X%02X%02X%02X,"
                    "%s,0x%X\n",
                    g_d.run_id, r->seq, r->pc, r->lr, r->sp, r->r0, r->r1, r->r2, r->r3, r->r9,
                    r->size, r->bytes[0], r->bytes[1], r->bytes[2], r->bytes[3], r->kind,
                    r->target);
        }
        fflush(g_d.insn_csv);
    }
    if (g_d.call_csv) {
        for (i = 0; i < g_d.call_n; i++) {
            CallRow *c = &g_d.call_rows[i];
            fprintf(g_d.call_csv, "%llu,%u,0x%X,0x%X,0x%X,0x%X,%d,%s\n", g_d.run_id, c->seq,
                    c->caller_pc, c->target, c->lr, c->r0_in, (int)c->r0_out, c->note);
        }
        fflush(g_d.call_csv);
    }

    classify_and_emit_provenance(helper_ret);
    printf("[JJFB_E10A31D] method0_trace=disarm insn=%llu calls=%u apis=%u fail=%s "
           "evidence=OBSERVED\n",
           (unsigned long long)g_d.m0_insn_used, g_d.call_n, g_d.api_n,
           g_d.first_fail_class[0] ? g_d.first_fail_class : "none");
    fflush(stdout);
}

void e10a31d_note_platform_api(void *uc, const char *api, uint32_t slot, int32_t r0) {
    uint32_t pc = 0, lr = 0;
    ensure_enabled();
    if (!g_d.enabled || !g_d.m0_active) return;
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &lr);
    }
#else
    (void)uc;
#endif
    if (g_d.api_n < E10A31D_API_MAX) {
        ApiRow *a = &g_d.api_rows[g_d.api_n++];
        memset(a, 0, sizeof(*a));
        a->seq = g_d.api_n;
        snprintf(a->api, sizeof(a->api), "%s", api ? api : "?");
        a->slot = slot;
        a->r0 = r0;
        a->pc = pc;
        a->lr = lr;
    }
    if (r0 < 0 && !g_d.first_failure_proven) {
        g_d.first_failure_proven = 1;
        g_d.first_fail_seq = g_d.m0_insn_used ? (uint32_t)g_d.m0_insn_used : 1;
        g_d.first_fail_pc = pc;
        g_d.first_fail_lr = lr;
        snprintf(g_d.first_fail_class, sizeof(g_d.first_fail_class),
                 "RETURN_NEG1_FROM_PLATFORM_API");
        snprintf(g_d.first_fail_evidence, sizeof(g_d.first_fail_evidence), "api=%s r0=%d",
                 api ? api : "?", (int)r0);
        e10a31d_mark_milestone("MRC_INIT_PLATFORM_CALL_FAILED", api ? api : "?");
    }
    printf("[JJFB_E10A31D_API] api=%s slot=0x%X r0=%d pc=0x%X evidence=OBSERVED\n",
           api ? api : "?", slot, (int)r0, pc);
    fflush(stdout);
}

/* ---------- Lane D: appInfo contract ---------- */

void e10a31d_after_code6(void *uc, uint32_t erw, uint32_t input_len, int32_t ret) {
    int ok = 0;
    uint32_t slot = 0;
    uint32_t scan[16];
    int i;
    ensure_enabled();
    if (!g_d.enabled || !g_d.appinfo) return;
    if (!g_d.appinfo_csv) {
        g_d.appinfo_csv = open_csv(
            "JJFB_E10A31D_APPINFO_CSV", "reports/e10a31d_appinfo_contract.csv",
            "run_id,phase,erw,slot_addr,expected,actual,appinfo_ptr,raw_hex,id,ver,sidName,"
            "sidName_mapped,sidName_str,ram,pkg_appid,pkg_appver,env_appid,env_appver,"
            "ret,note\n");
    }
    slot = peek_u32(uc, erw + 0x20u, &ok);
    printf("[JJFB_E10A31D_APPINFO] phase=after_code6 erw=0x%X ERW+0x20=0x%X "
           "expected_MR_VERSION=%u ok=%d ret=%d evidence=OBSERVED\n",
           erw, slot, input_len, ok, (int)ret);
    /* Scan ERW+0x00..0x3C for where version / pointers actually landed. */
    for (i = 0; i < 16; i++) {
        int sok = 0;
        scan[i] = peek_u32(uc, erw + (uint32_t)(i * 4), &sok);
        if (!sok) scan[i] = 0;
    }
    printf("[JJFB_E10A31D_APPINFO] phase=erw_scan_after_code6 "
           "+00=0x%X +04=0x%X +08=0x%X +0C=0x%X +10=0x%X +14=0x%X +18=0x%X +1C=0x%X "
           "+20=0x%X +24=0x%X +28=0x%X +2C=0x%X +30=0x%X +34=0x%X +38=0x%X +3C=0x%X "
           "evidence=OBSERVED\n",
           scan[0], scan[1], scan[2], scan[3], scan[4], scan[5], scan[6], scan[7], scan[8],
           scan[9], scan[10], scan[11], scan[12], scan[13], scan[14], scan[15]);
    /* Live gamelist helper: method6 does STR len,[r9+0x900+0x1C] (=R9+0x91C). */
    {
        int sok = 0;
        uint32_t v91c = peek_u32(uc, erw + 0x91Cu, &sok);
        uint32_t v920 = peek_u32(uc, erw + 0x920u, &sok);
        printf("[JJFB_E10A31D_APPINFO] phase=r9_plus_900_after_code6 "
               "R9+0x91C=0x%X R9+0x920=0x%X expected_ver=%u evidence=OBSERVED\n",
               v91c, v920, input_len);
        if (v91c == input_len)
            e10a31d_mark_milestone("CODE6_MR_VERSION_AT_R9_PLUS_91C", "gamelist_helper");
        fflush(stdout);
        if (g_d.appinfo_csv) {
            fprintf(g_d.appinfo_csv,
                    "%llu,r9_plus_91c_code6,0x%X,0x%X,%u,0x%X,0,,,0,0,0,0,,0,%u,%u,\"%s\",\"%s\","
                    "%d,gamelist_method6_store\n",
                    g_d.run_id, erw, erw + 0x91Cu, input_len, v91c, g_d.pkg_appid, g_d.pkg_appver,
                    g_d.env_appid, g_d.env_appver, (int)ret);
        }
    }
    if (g_d.appinfo_csv) {
        fprintf(g_d.appinfo_csv,
                "%llu,after_code6,0x%X,0x%X,%u,0x%X,0,,,0,0,0,0,,0,%u,%u,\"%s\",\"%s\",%d,"
                "ERW_plus_0x20\n",
                g_d.run_id, erw, erw + 0x20u, input_len, slot, g_d.pkg_appid, g_d.pkg_appver,
                g_d.env_appid, g_d.env_appver, (int)ret);
        fprintf(g_d.appinfo_csv,
                "%llu,erw_scan_code6,0x%X,0x0,%u,0x%X,0,\"%08X%08X%08X%08X%08X%08X%08X%08X\","
                "0,0,0,0,,0,%u,%u,\"%s\",\"%s\",%d,scan_0_3c\n",
                g_d.run_id, erw, input_len, slot, scan[0], scan[1], scan[2], scan[3], scan[4],
                scan[5], scan[6], scan[7], g_d.pkg_appid, g_d.pkg_appver, g_d.env_appid,
                g_d.env_appver, (int)ret);
        fflush(g_d.appinfo_csv);
    }
    if (ok && slot == input_len)
        e10a31d_mark_milestone("CODE6_MR_VERSION_WRITTEN", "match");
    else {
        int found = 0;
        for (i = 0; i < 16; i++) {
            if (scan[i] == input_len) {
                char note[40];
                snprintf(note, sizeof(note), "found_at_ERW+0x%X", i * 4);
                e10a31d_mark_milestone("CODE6_MR_VERSION_ALT_OFFSET", note);
                found = 1;
                break;
            }
        }
        if (!found) e10a31d_mark_milestone("CODE6_MR_VERSION_MISMATCH", "check_offset");
    }
}

void e10a31d_after_code8(void *uc, uint32_t erw, uint32_t appinfo, uint32_t input_len,
                         int32_t ret) {
    int ok = 0, ok_id = 0, ok_ver = 0, ok_sid = 0, ok_ram = 0;
    uint32_t slot = 0, id = 0, ver = 0, sid = 0, ram = 0;
    uint8_t raw[16];
    char hex[48];
    char sid_str[64];
    int sid_mapped = 0;
    uint32_t env_id = 0, env_ver = 0;
    ensure_enabled();
    if (!g_d.enabled || !g_d.appinfo) return;
    g_d.appinfo_guest = appinfo;
    memset(raw, 0, sizeof(raw));
    memset(hex, 0, sizeof(hex));
    memset(sid_str, 0, sizeof(sid_str));
    slot = peek_u32(uc, erw + 0x24u, &ok);
    if (appinfo && guest_memory_uc_peek((struct uc_struct *)uc, appinfo, raw, 16)) {
        int i;
        for (i = 0; i < 16; i++) sprintf(hex + i * 2, "%02X", raw[i]);
        id = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2] << 16) |
             ((uint32_t)raw[3] << 24);
        ver = (uint32_t)raw[4] | ((uint32_t)raw[5] << 8) | ((uint32_t)raw[6] << 16) |
              ((uint32_t)raw[7] << 24);
        sid = (uint32_t)raw[8] | ((uint32_t)raw[9] << 8) | ((uint32_t)raw[10] << 16) |
              ((uint32_t)raw[11] << 24);
        ram = (uint32_t)raw[12] | ((uint32_t)raw[13] << 8) | ((uint32_t)raw[14] << 16) |
              ((uint32_t)raw[15] << 24);
        ok_id = ok_ver = ok_sid = ok_ram = 1;
        if (sid) {
            char tmp[64];
            memset(tmp, 0, sizeof(tmp));
            if (guest_memory_uc_peek((struct uc_struct *)uc, sid, tmp, sizeof(tmp) - 1)) {
                sid_mapped = 1;
                snprintf(sid_str, sizeof(sid_str), "%.63s", tmp);
            }
        }
    }
    if (g_d.env_appid[0]) env_id = (uint32_t)strtoul(g_d.env_appid, NULL, 10);
    if (g_d.env_appver[0]) env_ver = (uint32_t)strtoul(g_d.env_appver, NULL, 10);
    /* Refresh MRP header compare from live package-scoped metadata. */
    load_package_metadata();

    printf("[JJFB_E10A31D_APPINFO] phase=after_code8 erw=0x%X ERW+0x24=0x%X appinfo=0x%X "
           "id=%u ver=%u sidName=0x%X ram=%u pkg=%u/%u env=%u/%u ret=%d evidence=OBSERVED\n",
           erw, slot, appinfo, id, ver, sid, ram, g_d.pkg_appid, g_d.pkg_appver, env_id, env_ver,
           (int)ret);
    fflush(stdout);

    if (!g_d.appinfo_csv) {
        g_d.appinfo_csv = open_csv(
            "JJFB_E10A31D_APPINFO_CSV", "reports/e10a31d_appinfo_contract.csv",
            "run_id,phase,erw,slot_addr,expected,actual,appinfo_ptr,raw_hex,id,ver,sidName,"
            "sidName_mapped,sidName_str,ram,pkg_appid,pkg_appver,env_appid,env_appver,"
            "ret,note\n");
    }
    if (g_d.appinfo_csv) {
        fprintf(g_d.appinfo_csv,
                "%llu,after_code8,0x%X,0x%X,%u,0x%X,0x%X,%s,%u,%u,0x%X,%d,\"%s\",%u,%u,%u,"
                "\"%s\",\"%s\",%d,appinfo_16b\n",
                g_d.run_id, erw, erw + 0x24u, input_len, slot, appinfo, hex, id, ver, sid,
                sid_mapped, sid_str, ram, g_d.pkg_appid, g_d.pkg_appver, g_d.env_appid,
                g_d.env_appver, (int)ret);
        fflush(g_d.appinfo_csv);
    }

    if (id == 0 && ver == 0) e10a31d_mark_milestone("APPINFO_ID_VERSION_ZERO", "both_zero");
    else if (g_d.pkg_meta_ok && (id != g_d.pkg_appid || ver != g_d.pkg_appver))
        e10a31d_mark_milestone("APPINFO_ID_VERSION_MISMATCH", "vs_mrp_header");
    else if (g_d.pkg_meta_ok && id == g_d.pkg_appid && ver == g_d.pkg_appver)
        e10a31d_mark_milestone("APPINFO_PACKAGE_METADATA_MATCH", "mrp_header");
    if (sid == 0) e10a31d_mark_milestone("APPINFO_SIDNAME_NULL", "ptr0");
    if (ram == 0) e10a31d_mark_milestone("APPINFO_RAM_ZERO", "ram0");
    if (!appinfo || !ok)
        e10a31d_mark_milestone("APPINFO_POINTER_INVALID", "missing");
    /* Live gamelist: method8 STR input,[r9+0x900+0x20] (=R9+0x920). */
    {
        int sok = 0;
        uint32_t v91c = peek_u32(uc, erw + 0x91Cu, &sok);
        uint32_t v920 = peek_u32(uc, erw + 0x920u, &sok);
        printf("[JJFB_E10A31D_APPINFO] phase=r9_plus_900_after_code8 "
               "R9+0x91C=0x%X R9+0x920=0x%X appinfo=0x%X evidence=OBSERVED\n",
               v91c, v920, appinfo);
        if (v920 == appinfo)
            e10a31d_mark_milestone("CODE8_APPINFO_AT_R9_PLUS_920", "gamelist_helper");
        fflush(stdout);
        if (g_d.appinfo_csv) {
            fprintf(g_d.appinfo_csv,
                    "%llu,r9_plus_920_code8,0x%X,0x%X,%u,0x%X,0x%X,,,0,0,0,0,,0,%u,%u,\"%s\","
                    "\"%s\",%d,gamelist_method8_store\n",
                    g_d.run_id, erw, erw + 0x920u, appinfo, v920, appinfo, g_d.pkg_appid,
                    g_d.pkg_appver, g_d.env_appid, g_d.env_appver, (int)ret);
        }
    }
    (void)ok_id;
    (void)ok_ver;
    (void)ok_sid;
    (void)ok_ram;
}
