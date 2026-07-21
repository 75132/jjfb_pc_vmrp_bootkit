#include "gwy_launcher/e10a_shell_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    int known;
    int enabled;
    uint32_t phase_n;
    uint32_t vfs_n;
    uint32_t event_n;
    uint32_t update_n;
    FILE *phase_csv;
    FILE *vfs_csv;
    FILE *event_csv;
    FILE *update_csv;
} g_e10a;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

int e10a_shell_trace_enabled(void) {
    if (!g_e10a.known) {
        g_e10a.enabled = env1("JJFB_E10A_SHELL_TRACE") || env1("JJFB_E10A_MODE") ||
                         env1("JJFB_E10A_GWY_SHELL_PRELAUNCH");
        g_e10a.known = 1;
    }
    return g_e10a.enabled;
}

static FILE *open_csv(const char *env_key, const char *fallback, const char *header) {
    const char *p = getenv(env_key);
    FILE *f;
    if (!p || !p[0]) p = fallback;
    f = fopen(p, "wb");
    if (f) {
        fputs(header, f);
        fflush(f);
    }
    return f;
}

static void ensure_files(void) {
    if (!g_e10a.phase_csv) {
        g_e10a.phase_csv = open_csv("JJFB_E10A_PHASE_CSV", "reports/e10a_shell_phase_trace.csv",
                                    "n,launch_path,phase,module,pc,lr,r0,r1,r2,r3,r9,AC8,note\n");
    }
    if (!g_e10a.vfs_csv) {
        g_e10a.vfs_csv = open_csv("JJFB_E10A_VFS_CSV", "reports/e10a_shell_vfs_trace.csv",
                                  "n,launch_path,module,op,guest_path,host_path,exists,rc,pc,lr\n");
    }
    if (!g_e10a.event_csv) {
        g_e10a.event_csv = open_csv("JJFB_E10A_EVENT_CSV", "reports/e10a_shell_event_loop_trace.csv",
                                    "n,launch_path,event_code,handler_pc,pc,lr,note\n");
    }
    if (!g_e10a.update_csv) {
        g_e10a.update_csv = open_csv("JJFB_E10A_UPDATE_CSV", "reports/e10a_shell_update_contract_trace.csv",
                                     "n,launch_path,request,response,note\n");
    }
}

void e10a_shell_phase(const char *phase, const char *module, uint32_t pc, uint32_t lr,
                      uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9,
                      uint32_t ac8, const char *note) {
    const char *lp;
    if (!e10a_shell_trace_enabled()) return;
    ensure_files();
    g_e10a.phase_n++;
    lp = getenv("JJFB_LAUNCH_PATH");
    if (!lp || !lp[0]) lp = "?";
    printf("[JJFB_E10A_SHELL] launch_path=%s phase=%s module=%s pc=0x%X lr=0x%X "
           "r0=0x%X r1=0x%X AC8=0x%X note=%s evidence=OBSERVED\n",
           lp, phase ? phase : "?", module ? module : "?", pc, lr, r0, r1, ac8,
           note ? note : "");
    if (g_e10a.phase_csv) {
        fprintf(g_e10a.phase_csv,
                "%u,%s,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", g_e10a.phase_n, lp,
                phase ? phase : "", module ? module : "", pc, lr, r0, r1, r2, r3, r9, ac8,
                note ? note : "");
        fflush(g_e10a.phase_csv);
    }
    fflush(stdout);
}

void e10a_shell_vfs(const char *op, const char *module, const char *guest_path,
                    const char *host_path, int exists, int rc, uint32_t pc, uint32_t lr) {
    const char *lp;
    if (!e10a_shell_trace_enabled()) return;
    ensure_files();
    g_e10a.vfs_n++;
    lp = getenv("JJFB_LAUNCH_PATH");
    if (!lp || !lp[0]) lp = "?";
    if (g_e10a.vfs_n <= 512) {
        printf("[JJFB_E10A_VFS] launch_path=%s op=%s module=%s guest=\"%s\" host=\"%s\" "
               "exists=%d rc=%d pc=0x%X lr=0x%X evidence=OBSERVED\n",
               lp, op ? op : "?", module ? module : "?", guest_path ? guest_path : "",
               host_path ? host_path : "", exists, rc, pc, lr);
    }
    if (g_e10a.vfs_csv) {
        fprintf(g_e10a.vfs_csv, "%u,%s,%s,%s,\"%s\",\"%s\",%d,%d,0x%X,0x%X\n", g_e10a.vfs_n, lp,
                module ? module : "", op ? op : "", guest_path ? guest_path : "",
                host_path ? host_path : "", exists, rc, pc, lr);
        fflush(g_e10a.vfs_csv);
    }
    fflush(stdout);
}

void e10a_shell_event(uint32_t event_code, uint32_t handler_pc, uint32_t pc, uint32_t lr,
                      const char *note) {
    const char *lp;
    if (!e10a_shell_trace_enabled()) return;
    ensure_files();
    g_e10a.event_n++;
    lp = getenv("JJFB_LAUNCH_PATH");
    if (!lp || !lp[0]) lp = "?";
    printf("[JJFB_E10A_EVENT] launch_path=%s event=0x%X handler=0x%X pc=0x%X lr=0x%X note=%s "
           "evidence=OBSERVED\n",
           lp, event_code, handler_pc, pc, lr, note ? note : "");
    if (g_e10a.event_csv) {
        fprintf(g_e10a.event_csv, "%u,%s,0x%X,0x%X,0x%X,0x%X,%s\n", g_e10a.event_n, lp,
                event_code, handler_pc, pc, lr, note ? note : "");
        fflush(g_e10a.event_csv);
    }
    fflush(stdout);
}

void e10a_shell_update(const char *request, const char *response, const char *note) {
    const char *lp;
    if (!e10a_shell_trace_enabled()) return;
    ensure_files();
    g_e10a.update_n++;
    lp = getenv("JJFB_LAUNCH_PATH");
    if (!lp || !lp[0]) lp = "?";
    printf("[JJFB_E10A_UPDATE] launch_path=%s request=%s response=%s note=%s evidence=OBSERVED\n",
           lp, request ? request : "?", response ? response : "?", note ? note : "");
    if (g_e10a.update_csv) {
        fprintf(g_e10a.update_csv, "%u,%s,%s,%s,%s\n", g_e10a.update_n, lp,
                request ? request : "", response ? response : "", note ? note : "");
        fflush(g_e10a.update_csv);
    }
    fflush(stdout);
}
