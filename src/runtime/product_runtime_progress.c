#include "gwy_launcher/product_runtime_progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

static char g_run_id[64];
static char g_path[1024];
static int g_path_ready;
static int g_enabled_known;
static int g_enabled;

static int env_set(const char *name) {
    const char *e = getenv(name);
    return e && e[0];
}

static unsigned long current_pid(void) {
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static void ensure_path(void) {
    const char *p;
    if (g_path_ready) return;
    g_path_ready = 1;
    p = getenv("GWY_RUNTIME_PROGRESS_PATH");
    if (p && p[0]) {
        snprintf(g_path, sizeof(g_path), "%s", p);
        return;
    }
    snprintf(g_path, sizeof(g_path), "%s", "out/vmrp_run/runtime_progress.jsonl");
}

static int progress_enabled(void) {
    if (!g_enabled_known) {
        /* Always write when path/run_id set by launcher diagnostic, or explicit opt-in. */
        g_enabled = env_set("GWY_RUNTIME_PROGRESS_PATH") ||
                    env_set("GWY_PRODUCT_RUN_ID") ||
                    (getenv("JJFB_RUNTIME_PROGRESS") && getenv("JJFB_RUNTIME_PROGRESS")[0] == '1');
        g_enabled_known = 1;
    }
    return g_enabled;
}

void product_runtime_progress_reset(void) {
    g_run_id[0] = 0;
    g_path[0] = 0;
    g_path_ready = 0;
    g_enabled_known = 0;
    g_enabled = 0;
}

void product_runtime_progress_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_runtime_progress_run_id(void) {
    const char *e;
    if (g_run_id[0]) return g_run_id;
    e = getenv("GWY_PRODUCT_RUN_ID");
    return (e && e[0]) ? e : "unknown";
}

static void json_escape(const char *in, char *out, size_t cap) {
    size_t o = 0;
    if (!in) in = "";
    while (*in && o + 2 < cap) {
        char c = *in++;
        if (c == '"' || c == '\\') {
            if (o + 3 >= cap) break;
            out[o++] = '\\';
            out[o++] = c;
        } else if ((unsigned char)c < 0x20) {
            continue;
        } else {
            out[o++] = c;
        }
    }
    out[o] = 0;
}

void product_runtime_progress_emit(const char *milestone,
                                   const char *source,
                                   const char *details) {
    FILE *fp;
    char ms[96], src[96], det[384];
    time_t now;
    if (!milestone || !milestone[0]) return;
    if (!progress_enabled()) return;
    ensure_path();
    json_escape(milestone, ms, sizeof(ms));
    json_escape(source ? source : "", src, sizeof(src));
    json_escape(details ? details : "", det, sizeof(det));
    now = time(NULL);
    fp = fopen(g_path, "ab");
    if (!fp) return;
    fprintf(fp,
            "{\"run_id\":\"%s\",\"timestamp\":%ld,\"pid\":%lu,\"milestone\":\"%s\","
            "\"source\":\"%s\",\"details\":\"%s\"}\n",
            product_runtime_progress_run_id(), (long)now, current_pid(), ms, src, det);
    fclose(fp);
    printf("[RUNTIME_PROGRESS] milestone=%s source=%s details=%s evidence=OBSERVED\n",
           milestone, source ? source : "", details ? details : "");
    fflush(stdout);
}
