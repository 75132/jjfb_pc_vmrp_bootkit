#include "gwy_launcher/product_callback_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_run_id[64];
static char g_fb_sha[68];
static uint32_t g_seq;
static uint32_t g_ok_returns;
static uint32_t g_draw;
static uint32_t g_refresh;
static int g_halted;
static int g_in_callback;
static GwyP3FaultClass g_fault_class = GWY_P3_NO_GUEST_FAULT;
static GwyP3CallbackSnap g_last;
static uint32_t g_fault_pc;
static uint32_t g_fault_addr;
static uint32_t g_fault_size;
static int g_fault_write;
static int g_fault_fetch;
static char g_end_reason[48];
static uint32_t g_uc_err;
static uint64_t g_frame_token;
static int g_fault_csv_written;

static void report_path(char *out, size_t n, const char *name) {
    const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (root && root[0])
        snprintf(out, n, "%s/%s", root, name);
    else
        snprintf(out, n, "reports/%s", name);
}

void product_callback_trace_reset(void) {
    g_seq = 0;
    g_ok_returns = 0;
    g_draw = 0;
    g_refresh = 0;
    g_halted = 0;
    g_in_callback = 0;
    g_fault_class = GWY_P3_NO_GUEST_FAULT;
    memset(&g_last, 0, sizeof(g_last));
    g_fault_pc = g_fault_addr = g_fault_size = 0;
    g_fault_write = g_fault_fetch = 0;
    g_end_reason[0] = 0;
    g_uc_err = 0;
    g_frame_token = 0;
    g_fault_csv_written = 0;
    g_fb_sha[0] = 0;
}

void product_callback_trace_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_callback_trace_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

const char *gwy_p3_fault_class_name(GwyP3FaultClass c) {
    switch (c) {
    case GWY_P3_NO_GUEST_FAULT: return "NO_GUEST_FAULT";
    case GWY_P3_CALLBACK_RETURN_SENTINEL_OK: return "CALLBACK_RETURN_SENTINEL_OK";
    case GWY_P3_CALLBACK_GUEST_EXECUTION_FAULT: return "CALLBACK_GUEST_EXECUTION_FAULT";
    case GWY_P3_CALLBACK_RETURN_SENTINEL_MISCLASSIFIED:
        return "CALLBACK_RETURN_SENTINEL_MISCLASSIFIED";
    case GWY_P3_CALLBACK_FRAME_RESTORE_BROKEN: return "CALLBACK_FRAME_RESTORE_BROKEN";
    case GWY_P3_CALLBACK_REENTRANCY: return "CALLBACK_REENTRANCY";
    case GWY_P3_TIMER_DUPLICATE_DELIVERY: return "TIMER_DUPLICATE_DELIVERY";
    case GWY_P3_CALLBACK_STALE_GENERATION: return "CALLBACK_STALE_GENERATION";
    case GWY_P3_CALLBACK_PLATFORM_CONTRACT_FAILURE: return "CALLBACK_PLATFORM_CONTRACT_FAILURE";
    case GWY_P3_CALLBACK_RESOURCE_MEMORY_FAULT: return "CALLBACK_RESOURCE_MEMORY_FAULT";
    case GWY_P3_CALLBACK_HOST_FAILURE: return "CALLBACK_HOST_FAILURE";
    default: return "UNKNOWN";
    }
}

int product_callback_trace_halted(void) { return g_halted; }
GwyP3FaultClass product_callback_trace_fault_class(void) { return g_fault_class; }
uint32_t product_callback_trace_seq(void) { return g_seq; }
uint32_t product_callback_trace_ok_returns(void) { return g_ok_returns; }

void product_callback_trace_note_draw(void) { g_draw++; }
void product_callback_trace_note_refresh(void) { g_refresh++; }
uint32_t product_callback_trace_draw_count(void) { return g_draw; }
uint32_t product_callback_trace_refresh_count(void) { return g_refresh; }

void product_callback_trace_set_fb_sha256(const char *hex64) {
    if (!hex64) {
        g_fb_sha[0] = 0;
        return;
    }
    snprintf(g_fb_sha, sizeof(g_fb_sha), "%s", hex64);
}

const char *product_callback_trace_fb_sha256(void) { return g_fb_sha[0] ? g_fb_sha : ""; }

int product_callback_trace_classify_fire_done(int ok, uint32_t uc_err, const char *end_reason) {
    if (ok && uc_err == 0 && end_reason && strcmp(end_reason, "stop_at_base") == 0)
        return (int)GWY_P3_CALLBACK_RETURN_SENTINEL_OK;
    if (ok && uc_err == 0) return (int)GWY_P3_NO_GUEST_FAULT;
    if (!ok && uc_err != 0 && end_reason && strstr(end_reason, "stop") &&
        strstr(end_reason, "unmapped"))
        return (int)GWY_P3_CALLBACK_RETURN_SENTINEL_MISCLASSIFIED;
    if (!ok || uc_err != 0) return (int)GWY_P3_CALLBACK_GUEST_EXECUTION_FAULT;
    return (int)GWY_P3_NO_GUEST_FAULT;
}

int product_callback_trace_line_is_real_draw(const char *line) {
    if (!line) return 0;
    if (strstr(line, "[FIRST_NATURAL_DRAW]")) return 1;
    if (strstr(line, "[JJFB_DRAW]")) return 1;
    if (strstr(line, "BIND_REFRESH")) return 0;
    return 0;
}

int product_callback_trace_line_is_real_refresh(const char *line) {
    if (!line) return 0;
    if (strstr(line, "[FIRST_NATURAL_REFRESH]")) return 1;
    if (strstr(line, "[JJFB_REFRESH]") && strstr(line, "api=_DispUpEx")) return 1;
    if (strstr(line, "[JJFB_REFRESH]") && strstr(line, "api=DispUpEx")) return 1;
    if (strstr(line, "BIND_REFRESH")) return 0;
    return 0;
}

int product_callback_trace_line_is_real_fault(const char *line) {
    if (!line) return 0;
    /* Never treat uc_err=0 as fault (substring UC_ERR). */
    if (strstr(line, "uc_err=0")) return 0;
    if (strstr(line, "[EXT_FAULT]")) return 1;
    if (strstr(line, "FIRE_DONE") && strstr(line, "ok=0")) return 1;
    if (strstr(line, "uc_err=") && !strstr(line, "uc_err=0")) return 1;
    if (strstr(line, "[P3_FAULT]")) return 1;
    return 0;
}

static void ensure_callback_csv_header(void) {
    char path[512];
    FILE *f;
    report_path(path, sizeof(path), "product_p3_callback_trace.csv");
    f = fopen(path, "r");
    if (f) {
        fclose(f);
        return;
    }
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "run_id,seq,timer_id,owner_module,owner_module_id,owner_generation,source,"
               "handler,pc,lr,sp,r9,cpsr,guest_depth,forced,ok,uc_err,end_reason,ret,"
               "draw_count,refresh_count,fb_sha256,class\n");
    fclose(f);
}

static void append_callback_csv(uint32_t seq, int ok, uint32_t uc_err, const char *end_reason,
                                int32_t ret, GwyP3FaultClass cls) {
    char path[512];
    FILE *f;
    ensure_callback_csv_header();
    report_path(path, sizeof(path), "product_p3_callback_trace.csv");
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f,
            "%s,%u,%llu,%s,%llu,%llu,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%d,%d,%u,%s,%d,%u,%u,%s,%s\n",
            product_callback_trace_run_id(), seq, (unsigned long long)g_last.timer_or_event_id,
            g_last.owner_module[0] ? g_last.owner_module : "?",
            (unsigned long long)g_last.owner_module_id,
            (unsigned long long)g_last.owner_generation, g_last.source, g_last.handler, g_last.pc,
            g_last.lr, g_last.sp, g_last.r9, g_last.cpsr, g_last.guest_depth, g_last.forced, ok,
            uc_err, end_reason ? end_reason : "?", (int)ret, g_draw, g_refresh,
            g_fb_sha[0] ? g_fb_sha : "", gwy_p3_fault_class_name(cls));
    fclose(f);
}

static void write_fault_csv(void) {
    char path[512];
    FILE *f;
    if (g_fault_csv_written) return;
    g_fault_csv_written = 1;
    report_path(path, sizeof(path), "product_p3_fault_provenance.csv");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "run_id,seq,class,guest_pc,lr,sp,r9,cpsr,fault_addr,fault_size,is_write,is_fetch,"
               "uc_err,end_reason,handler,owner_generation,draw_count,refresh_count\n");
    fprintf(f,
            "%s,%u,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%d,%d,%u,%s,0x%X,%llu,%u,%u\n",
            product_callback_trace_run_id(), g_last.seq, gwy_p3_fault_class_name(g_fault_class),
            g_fault_pc ? g_fault_pc : g_last.pc, g_last.lr, g_last.sp, g_last.r9, g_last.cpsr,
            g_fault_addr, g_fault_size, g_fault_write, g_fault_fetch, g_uc_err,
            g_end_reason[0] ? g_end_reason : "?", g_last.handler,
            (unsigned long long)g_last.owner_generation, g_draw, g_refresh);
    fclose(f);
}

void product_callback_trace_write_verdict_md(void) {
    char path[512];
    FILE *f;
    report_path(path, sizeof(path), "product_p3_fault_verdict.md");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
            "# Product P3 Fault Verdict\n\n"
            "- **run_id:** %s\n"
            "- **primary_class:** %s\n"
            "- **halted:** %s\n"
            "- **ok_returns:** %u\n"
            "- **draw_count:** %u\n"
            "- **refresh_count:** %u\n"
            "- **last_seq:** %u\n"
            "- **note:** stop_at_base + uc_err=0 is CALLBACK_RETURN_SENTINEL_OK, not a guest fault\n",
            product_callback_trace_run_id(), gwy_p3_fault_class_name(g_fault_class),
            g_halted ? "yes" : "no", g_ok_returns, g_draw, g_refresh, g_seq);
    fclose(f);
}

uint32_t product_callback_trace_on_enter(const GwyP3CallbackSnap *snap) {
    if (g_halted) return 0;
    if (g_in_callback) {
        g_fault_class = GWY_P3_CALLBACK_REENTRANCY;
        g_halted = 1;
        printf("[P3_FAULT] class=CALLBACK_REENTRANCY run_id=%s evidence=OBSERVED\n",
               product_callback_trace_run_id());
        fflush(stdout);
        write_fault_csv();
        product_callback_trace_write_verdict_md();
        return 0;
    }
    if (!snap) return 0;
    g_seq++;
    g_last = *snap;
    g_last.seq = g_seq;
    g_in_callback = 1;
    printf("[P3_CALLBACK_ENTER] seq=%u handler=0x%X owner=%s generation=%llu depth=%u "
           "forced=%d run_id=%s evidence=OBSERVED\n",
           g_seq, snap->handler, snap->owner_module[0] ? snap->owner_module : "?",
           (unsigned long long)snap->owner_generation, snap->guest_depth, snap->forced,
           product_callback_trace_run_id());
    fflush(stdout);
    return g_seq;
}

void product_callback_trace_on_leave(uint32_t seq, int ok, uint32_t uc_err, const char *end_reason,
                                     uint32_t pc_after, uint32_t lr_after, uint32_t sp_after,
                                     uint32_t r9_after, int32_t ret, uint32_t restored_r9) {
    GwyP3FaultClass cls;
    (void)lr_after;
    (void)sp_after;
    if (!g_in_callback || (seq && seq != g_seq)) {
        /* ignore stale */
    }
    g_in_callback = 0;
    snprintf(g_end_reason, sizeof(g_end_reason), "%s", end_reason ? end_reason : "?");
    g_uc_err = uc_err;
    cls = (GwyP3FaultClass)product_callback_trace_classify_fire_done(ok, uc_err, end_reason);

    if (cls == GWY_P3_CALLBACK_RETURN_SENTINEL_OK || cls == GWY_P3_NO_GUEST_FAULT) {
        g_ok_returns++;
        if (g_fault_class == GWY_P3_NO_GUEST_FAULT)
            g_fault_class = GWY_P3_CALLBACK_RETURN_SENTINEL_OK;
        append_callback_csv(seq ? seq : g_seq, ok, uc_err, end_reason, ret, cls);
        printf("[P3_CALLBACK_LEAVE] seq=%u ok=%d uc_err=%u end=%s class=%s ret=%d "
               "pc_after=0x%X r9_after=0x%X restored_r9=0x%X run_id=%s evidence=OBSERVED\n",
               seq ? seq : g_seq, ok, uc_err, end_reason ? end_reason : "?",
               gwy_p3_fault_class_name(cls), (int)ret, pc_after, r9_after, restored_r9,
               product_callback_trace_run_id());
        fflush(stdout);
        product_callback_trace_write_verdict_md();
        return;
    }

    /* Real fault — freeze. */
    if (!g_halted) {
        g_halted = 1;
        g_fault_class = cls;
        g_fault_pc = pc_after;
        append_callback_csv(seq ? seq : g_seq, ok, uc_err, end_reason, ret, cls);
        write_fault_csv();
        product_callback_trace_write_verdict_md();
        printf("[P3_FAULT] class=%s seq=%u pc=0x%X uc_err=%u end=%s run_id=%s "
               "evidence=OBSERVED note=scheduler_halt\n",
               gwy_p3_fault_class_name(cls), seq ? seq : g_seq, pc_after, uc_err,
               end_reason ? end_reason : "?", product_callback_trace_run_id());
        fflush(stdout);
    }
}

void product_callback_trace_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr, uint32_t access_size,
                                         int is_write, int is_fetch) {
    if (g_halted) return;
    if (!g_in_callback) return;
    g_halted = 1;
    g_fault_class = GWY_P3_CALLBACK_GUEST_EXECUTION_FAULT;
    g_fault_pc = fault_pc;
    g_fault_addr = fault_addr;
    g_fault_size = access_size;
    g_fault_write = is_write;
    g_fault_fetch = is_fetch;
    write_fault_csv();
    product_callback_trace_write_verdict_md();
    printf("[P3_FAULT] class=CALLBACK_GUEST_EXECUTION_FAULT pc=0x%X addr=0x%X size=%u "
           "write=%d fetch=%d run_id=%s evidence=OBSERVED\n",
           fault_pc, fault_addr, access_size, is_write, is_fetch, product_callback_trace_run_id());
    fflush(stdout);
}

void product_callback_trace_frame_enter(uint32_t seq, uint64_t frame_token, uint32_t handler,
                                        uint32_t r9, uint32_t stop_sentinel) {
    g_frame_token = frame_token;
    printf("[CALLBACK_FRAME_ENTER] seq=%u frame_token=%llu handler=0x%X r9=0x%X "
           "stop_sentinel=0x%X run_id=%s evidence=OBSERVED\n",
           seq, (unsigned long long)frame_token, handler, r9, stop_sentinel,
           product_callback_trace_run_id());
    fflush(stdout);
}

void product_callback_trace_handler_return(uint32_t seq, uint32_t pc, int32_t ret) {
    printf("[CALLBACK_HANDLER_RETURN] seq=%u pc=0x%X ret=%d frame_token=%llu run_id=%s "
           "evidence=OBSERVED\n",
           seq, pc, (int)ret, (unsigned long long)g_frame_token, product_callback_trace_run_id());
    fflush(stdout);
}

void product_callback_trace_frame_restore_begin(uint32_t seq, uint32_t saved_r9) {
    printf("[CALLBACK_FRAME_RESTORE_BEGIN] seq=%u saved_r9=0x%X frame_token=%llu run_id=%s "
           "evidence=OBSERVED\n",
           seq, saved_r9, (unsigned long long)g_frame_token, product_callback_trace_run_id());
    fflush(stdout);
}

void product_callback_trace_frame_restore_complete(uint32_t seq, uint32_t restored_r9, int ok) {
    printf("[CALLBACK_FRAME_RESTORE_COMPLETE] seq=%u restored_r9=0x%X ok=%d frame_token=%llu "
           "run_id=%s evidence=OBSERVED\n",
           seq, restored_r9, ok, (unsigned long long)g_frame_token, product_callback_trace_run_id());
    fflush(stdout);
    if (!ok && !g_halted) {
        g_halted = 1;
        g_fault_class = GWY_P3_CALLBACK_FRAME_RESTORE_BROKEN;
        write_fault_csv();
        product_callback_trace_write_verdict_md();
    }
}

void product_callback_trace_note_visual_row(const char *api, int32_t x, int32_t y, int32_t w,
                                           int32_t h, const char *fb_sha, int nonempty,
                                           int hwnd_visible, int captured) {
    char path[512];
    FILE *f;
    report_path(path, sizeof(path), "product_p3_visual_trace.csv");
    f = fopen(path, "r");
    if (!f) {
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "run_id,api,x,y,w,h,fb_sha256,nonempty,hwnd_visible,captured\n");
            fclose(f);
        }
    } else {
        fclose(f);
    }
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s,%s,%d,%d,%d,%d,%s,%d,%d,%d\n", product_callback_trace_run_id(),
            api && api[0] ? api : "?", (int)x, (int)y, (int)w, (int)h,
            fb_sha && fb_sha[0] ? fb_sha : "", nonempty, hwnd_visible, captured);
    fclose(f);
}

void product_callback_trace_append_fb_hash(const char *api, const char *fb_sha, uint32_t nbytes) {
    char path[512];
    FILE *f;
    report_path(path, sizeof(path), "product_p3_framebuffer_hashes.csv");
    f = fopen(path, "r");
    if (!f) {
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "run_id,api,nbytes,fb_sha256\n");
            fclose(f);
        }
    } else {
        fclose(f);
    }
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s,%s,%u,%s\n", product_callback_trace_run_id(), api && api[0] ? api : "?", nbytes,
            fb_sha && fb_sha[0] ? fb_sha : "");
    fclose(f);
    if (fb_sha && fb_sha[0]) product_callback_trace_set_fb_sha256(fb_sha);
}
