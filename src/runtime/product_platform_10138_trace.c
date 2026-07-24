#include "gwy_launcher/product_platform_10138_trace.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/product_runtime_progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define LOG_CAP 48
#define SITE_HEAP 0x2D9A6Au
#define SITE_METRICS 0x30D010u

typedef struct {
    uint32_t api_call_id;
    uint32_t handler_call_id;
    uint32_t caller_pc;
    uint32_t lr;
    uint32_t site_lr;
    uint32_t ret;
    int mode_metrics;
    uint32_t outs[6];
    uint32_t vals[6];
    uint32_t ed8;
    uint8_t f7dc, f7dd;
    int wrote_gates;
} P10138Rec;

static int g_en, g_en_known, g_helper_active, g_finalized;
static char g_run_id[80];
static uint32_t g_api_n;
static P10138Rec g_log[LOG_CAP];
static uint32_t g_log_n;
static int g_ms_entered_emitted, g_ms_completed_emitted;

static int env_on(void) {
    const char *e = getenv("JJFB_PLATFORM_10138_TRACE");
    return e && e[0] == '1';
}

int product_p10138_enabled(void) {
    if (!g_en_known) {
        g_en = env_on();
        g_en_known = 1;
    }
    return g_en;
}

void product_p10138_reset(void) {
    g_en = 0;
    g_en_known = 0;
    g_helper_active = 0;
    g_finalized = 0;
    g_api_n = 0;
    g_log_n = 0;
    g_ms_entered_emitted = 0;
    g_ms_completed_emitted = 0;
    g_run_id[0] = 0;
    memset(g_log, 0, sizeof(g_log));
}

void product_p10138_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

void product_p10138_note_helper_enter(void) {
    g_helper_active = 1;
    g_ms_entered_emitted = 0;
    g_ms_completed_emitted = 0;
}

void product_p10138_note_helper_leave(void) { g_helper_active = 0; }

int product_p10138_helper_active(void) { return g_helper_active; }

static int site_in_scope(uint32_t site_lr) {
    uint32_t s = site_lr & ~1u;
    if (g_helper_active) return 1;
    if (s == SITE_HEAP || s == (SITE_HEAP + 4u) || s == SITE_METRICS) return 1;
    return 0;
}

static void dump_ptr(void *uc, const char *tag, uint32_t gp) {
    uint8_t b[32];
    char hex[72];
    size_t i, o = 0;
    if (!gp) {
        printf("[P10138_%s] ptr=0 mapped=0\n", tag);
        return;
    }
    memset(b, 0, sizeof(b));
#ifdef GWY_HAVE_UNICORN
    if (!uc || !guest_memory_uc_peek((struct uc_struct *)uc, gp, b, sizeof(b))) {
        printf("[P10138_%s] ptr=0x%X mapped=0\n", tag, gp);
        return;
    }
#else
    (void)uc;
    printf("[P10138_%s] ptr=0x%X mapped=?\n", tag, gp);
    return;
#endif
    for (i = 0; i < 16u && o + 3 < sizeof(hex); i++)
        o += (size_t)snprintf(hex + o, sizeof(hex) - o, "%02X", b[i]);
    printf("[P10138_%s] ptr=0x%X mapped=1 first16=%s evidence=OBSERVED\n", tag, gp, hex);
}

void product_p10138_on_enter(void *uc, uint32_t api_call_id, uint32_t handler_call_id,
                             uint32_t caller_pc, uint32_t lr, uint32_t sp, uint32_t cpsr,
                             uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9,
                             uint32_t site_lr, uint32_t slot_fn, const uint32_t outs[6]) {
    int i;
    /* Status-window milestone once per helper session (avoid 10138 spam). */
    if (!g_ms_entered_emitted) {
        product_runtime_progress_emit("platform_10138_entered", "p10138",
                                      "Completing platform 0x10138");
        g_ms_entered_emitted = 1;
    }
    if (!product_p10138_enabled() || !site_in_scope(site_lr)) return;
    printf("[P10138_ENTER] api_id=%u handler_id=%u caller_pc=0x%X lr=0x%X sp=0x%X cpsr=0x%X "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X site_lr=0x%X slot_fn=0x%X "
           "sp0=0x%X sp4=0x%X sp8=0x%X helper=%d evidence=OBSERVED\n",
           api_call_id, handler_call_id, caller_pc, lr, sp, cpsr, r0, r1, r2, r3, r9, site_lr,
           slot_fn, outs ? outs[3] : 0, outs ? outs[4] : 0, outs ? outs[5] : 0, g_helper_active);
    fflush(stdout);
    if (outs) {
        for (i = 0; i < 6; i++) {
            char t[16];
            snprintf(t, sizeof(t), "OUT%d_PRE", i);
            dump_ptr(uc, t, outs[i]);
        }
    }
}

void product_p10138_on_complete(void *uc, uint32_t api_call_id, uint32_t ret_r0, int mode_metrics,
                                uint32_t site_lr, const uint32_t outs[6], const uint32_t vals[6],
                                uint32_t er_rw, uint32_t ed8, uint8_t f7dc, uint8_t f7dd,
                                int wrote_gates) {
    int i;
    /* Milestone once per helper session — status window / progress IPC. */
    if (!g_ms_completed_emitted) {
        product_runtime_progress_emit("platform_10138_completed", "p10138",
                                      mode_metrics ? "metrics" : "heap");
        g_ms_completed_emitted = 1;
    }
    if (!product_p10138_enabled() || !site_in_scope(site_lr)) return;
    printf("[P10138_DONE] api_id=%u ret_r0=%u mode=%s site_lr=0x%X erw=0x%X ED8=%u 7DC=%u "
           "7DD=%u gates=%d evidence=OBSERVED\n",
           api_call_id, ret_r0, mode_metrics ? "metrics" : "heap", site_lr, er_rw, ed8,
           (unsigned)f7dc, (unsigned)f7dd, wrote_gates);
    fflush(stdout);
    if (outs && vals) {
        for (i = 0; i < 6; i++) {
            char t[16];
            if (!outs[i]) continue;
            snprintf(t, sizeof(t), "OUT%d_POST", i);
            dump_ptr(uc, t, outs[i]);
            printf("[P10138_WRITE] out%d=0x%X val=0x%X evidence=OBSERVED\n", i, outs[i], vals[i]);
        }
        fflush(stdout);
    }
    if (g_log_n < LOG_CAP) {
        P10138Rec *r = &g_log[g_log_n++];
        memset(r, 0, sizeof(*r));
        r->api_call_id = api_call_id;
        r->ret = ret_r0;
        r->mode_metrics = mode_metrics;
        r->site_lr = site_lr;
        r->ed8 = ed8;
        r->f7dc = f7dc;
        r->f7dd = f7dd;
        r->wrote_gates = wrote_gates;
        if (outs) memcpy(r->outs, outs, sizeof(r->outs));
        if (vals) memcpy(r->vals, vals, sizeof(r->vals));
    }
    (void)uc;
    g_api_n++;
}

void product_p10138_finalize(void) {
    const char *dir;
    char path[512];
    FILE *f;
    uint32_t i;
    if (g_finalized || !product_p10138_enabled()) return;
    g_finalized = 1;
    dir = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (!dir || !dir[0]) dir = "reports";
    snprintf(path, sizeof(path), "%s/platform_10138_calls.csv", dir);
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,api_id,mode,site_lr,ret,out5,val5,ed8,7dc,7dd,gates\n");
    for (i = 0; i < g_log_n; i++) {
        P10138Rec *r = &g_log[i];
        fprintf(f, "%s,%u,%s,0x%X,%u,0x%X,0x%X,%u,%u,%u,%d\n", g_run_id[0] ? g_run_id : "-",
                r->api_call_id, r->mode_metrics ? "metrics" : "heap", r->site_lr, r->ret,
                r->outs[5], r->vals[5], r->ed8, (unsigned)r->f7dc, (unsigned)r->f7dd,
                r->wrote_gates);
    }
    fclose(f);
    printf("[P10138_FINALIZE] n=%u csv=%s evidence=OBSERVED\n", g_log_n, path);
    fflush(stdout);
}
