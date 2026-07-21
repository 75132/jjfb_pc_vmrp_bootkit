#include "gwy_launcher/e10a_shell_trace.h"
#include "gwy_launcher/ext_post_cont_audit.h"
#include "gwy_launcher/guest_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

static struct {
    int known;
    int enabled;
    uint32_t phase_n;
    uint32_t vfs_n;
    uint32_t event_n;
    uint32_t update_n;
    FILE *phase_csv;
    FILE *vfs_csv;
    FILE *vfs_badpath_csv;
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
                                  "n,launch_path,module,op,guest_path,host_path,exists,rc,ctx_pc,ctx_lr,ctx_sp,"
                                  "ctx_r0,ctx_r1,ctx_r2,ctx_r3,ctx_r4,ctx_r5,ctx_r6,ctx_r7,ctx_r9,"
                                  "ctx_erw_guest,ctx_caller_module,ctx_caller_offset\n");
    }
    if (!g_e10a.vfs_badpath_csv) {
        g_e10a.vfs_badpath_csv =
            open_csv("JJFB_E10A_VFS_BADPATH_CSV",
                     "reports/e10a_shell_vfs_badpath_trace.csv",
                     "n,launch_path,module,op,guest_path_escaped,guest_path_len,ctx_pc,ctx_lr,ctx_sp,"
                     "ctx_r0,ctx_r1,ctx_r2,ctx_r3,ctx_r4,ctx_r5,ctx_r6,ctx_r7,ctx_r9,ctx_erw_guest,"
                     "caller_module,caller_offset,bad_ptr_origin,bad_ptr_value,bad_ptr_bytes_hex,"
                     "bad_ptr_decoded_escaped,last_success_guest,last_success_host,note\n");
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
    /* Prefer post-cont-audit context over the caller-provided (often 0x0) pc/lr. */
    {
        uint32_t ctx_pc = ext_post_cont_audit_last_pc();
        uint32_t ctx_lr = ext_post_cont_audit_last_lr();
        uint32_t ctx_sp = ext_post_cont_audit_last_sp();
        uint32_t ctx_r0 = ext_post_cont_audit_last_r0();
        uint32_t ctx_r1 = ext_post_cont_audit_last_r1();
        uint32_t ctx_r2 = ext_post_cont_audit_last_r2();
        uint32_t ctx_r3 = ext_post_cont_audit_last_r3();
        uint32_t ctx_r4 = ext_post_cont_audit_last_r4();
        uint32_t ctx_r5 = ext_post_cont_audit_last_r5();
        uint32_t ctx_r6 = ext_post_cont_audit_last_r6();
        uint32_t ctx_r7 = ext_post_cont_audit_last_r7();
        uint32_t ctx_r9 = ext_post_cont_audit_last_r9();
        uint32_t ctx_erw = ext_post_cont_audit_last_p_er_rw();
        uint32_t reg_erw = ext_post_cont_audit_last_registry_er_rw();
        if (!ctx_erw) ctx_erw = reg_erw;
        const char *caller_mod = ext_post_cont_audit_last_module();
        uint32_t caller_off = ext_post_cont_audit_last_offset();

        if (g_e10a.vfs_csv) {
            fprintf(g_e10a.vfs_csv,
                    "%u,%s,%s,%s,\"%s\",\"%s\",%d,%d,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,\"%s\",0x%X\n",
                    g_e10a.vfs_n, lp, module ? module : "", op ? op : "",
                    guest_path ? guest_path : "", host_path ? host_path : "",
                    exists, rc,
                    ctx_pc, ctx_lr, ctx_sp,
                    ctx_r0, ctx_r1, ctx_r2, ctx_r3, ctx_r4, ctx_r5, ctx_r6, ctx_r7,
                    ctx_r9,
                    ctx_erw,
                    caller_mod ? caller_mod : "", caller_off);
            fflush(g_e10a.vfs_csv);
        }

        /* Bad path deep trace: guest_path starts with backspace (0x08). */
        if (guest_path && (unsigned char)guest_path[0] == 0x08 && g_e10a.vfs_badpath_csv) {
            /* Escape helper: convert non-printables to \xNN and 0x08 to \\b. */
            char guest_esc[160];
            char decoded_esc[160];
            size_t guest_len = 0;
            char tmp_hex[256];
            uint8_t bytes64[64];
            uint8_t bytes_stack0[64];
            uint8_t bytes_r0[64];
            uint8_t bytes_r1[64];
            uint32_t ptr_r0 = ctx_r0;
            uint32_t ptr_r1 = ctx_r1;
            uint32_t ptr_stack0 = 0;
            uint32_t sp_word = 0;
            int have_stack0 = 0;
            const char *origin = "UNKNOWN";
            uint32_t origin_ptr = 0;

            /* Derive stack word0 = *(uint32_t*)SP (best-effort). */
            /* For stability: skip stack memory peek here (restored in later deep-trace pass). */
            (void)sp_word;
            have_stack0 = 0;
            ptr_stack0 = 0;

            /* Escape guest_path string itself (already decoded by existing pipeline). */
            {
                guest_esc[0] = 0;
                /* guest_path should be NUL-terminated; but still cap the loop. */
                for (guest_len = 0; guest_path[guest_len] && guest_len < 64; guest_len++) {
                    unsigned char b = (unsigned char)guest_path[guest_len];
                    if (b == 0x08) {
                        snprintf(guest_esc + strlen(guest_esc),
                                 sizeof(guest_esc) - strlen(guest_esc),
                                 "%s",
                                 guest_len == 0 ? "\\b" : "\\x08");
                    } else if (b >= 32 && b < 127) {
                        size_t sl = strlen(guest_esc);
                        if (sl + 1 < sizeof(guest_esc)) {
                            guest_esc[sl] = (char)b;
                            guest_esc[sl + 1] = 0;
                        }
                    } else {
                        snprintf(guest_esc + strlen(guest_esc),
                                 sizeof(guest_esc) - strlen(guest_esc),
                                 "\\x%02X", (unsigned)b);
                    }
                }
                if (guest_esc[0] == 0) snprintf(guest_esc, sizeof(guest_esc), "(empty)");
            }

            /* Peek 64 bytes at candidate pointers and pick the best match. */
            {
                /* For stability: skip raw guest memory peek (uc_mem_read) in this pass. */
                (void)bytes_r0;
                (void)bytes_r1;
                (void)bytes_stack0;
                origin = "SKIPPED_PEEK";
                origin_ptr = ptr_r0 ? ptr_r0 : (ptr_r1 ? ptr_r1 : ptr_stack0);
                memset(&bytes64, 0, sizeof(bytes64));
                decoded_esc[0] = 0;

                /* Hex encode chosen bytes64 for CSV */
                {
                    size_t i;
                    char *out = tmp_hex;
                    size_t cap = sizeof(tmp_hex);
                    out[0] = 0;
                    for (i = 0; i < sizeof(bytes64); i++) {
                        char t[4];
                        snprintf(t, sizeof(t), "%02X", (unsigned)bytes64[i]);
                        if (strlen(out) + 2 + 1 >= cap) break;
                        strcat(out, t);
                    }
                    /* decoded_esc from candidate c-string prefix (up to NUL or 16). */
                    decoded_esc[0] = 0;
                    for (size_t di = 0; di < 16 && di < sizeof(bytes64); di++) {
                        uint8_t b = bytes64[di];
                        if (b == 0) break;
                        if (b == 0x08) {
                            if (strlen(decoded_esc) + 2 < sizeof(decoded_esc))
                                strncat(decoded_esc, "\\b", 2);
                        } else if (b >= 32 && b < 127) {
                            size_t sl = strlen(decoded_esc);
                            if (sl + 1 < sizeof(decoded_esc)) {
                                decoded_esc[sl] = (char)b;
                                decoded_esc[sl + 1] = 0;
                            }
                        } else {
                            char t[5];
                            snprintf(t, sizeof(t), "\\x%02X", (unsigned)b);
                            if (strlen(decoded_esc) + strlen(t) + 1 < sizeof(decoded_esc)) strncat(decoded_esc, t, sizeof(decoded_esc) - strlen(decoded_esc) - 1);
                        }
                    }
                }
            }

            /* last_success best-effort: scan previous open entries from vfs trace counter. */
            /* (We keep it minimal here; full instruction/callback history is Task1-deep follow-up.) */
            static char last_success_guest[160];
            static char last_success_host[160];
            static uint32_t last_success_set;
            if (!last_success_set) {
                last_success_guest[0] = 0;
                last_success_host[0] = 0;
                last_success_set = 1;
            }

            /* Update last success on ok opens (best-effort). */
            if (exists && op && strcmp(op, "open") == 0 && host_path && host_path[0]) {
                snprintf(last_success_guest, sizeof(last_success_guest), "%s", guest_path ? guest_path : "");
                snprintf(last_success_host, sizeof(last_success_host), "%s", host_path);
            }

            fprintf(g_e10a.vfs_badpath_csv,
                    "%u,%s,%s,%s,\"%s\",%u,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "\"%s\",0x%X,\"%s\",0x%X,\"%s\",\"%s\",\"%s\",\"%s\",\"note_badpath_partial\"\n",
                    g_e10a.vfs_n, lp, module ? module : "", op ? op : "",
                    guest_esc, (unsigned)guest_len,
                    ctx_pc, ctx_lr, ctx_sp, ctx_r0, ctx_r1, ctx_r2, ctx_r3, ctx_r4, ctx_r5, ctx_r6,
                    ctx_r7, ctx_r9, ctx_erw,
                    caller_mod ? caller_mod : "?", caller_off,
                    origin ? origin : "UNKNOWN",
                    origin_ptr,
                    tmp_hex, decoded_esc,
                    last_success_guest[0] ? last_success_guest : "",
                    last_success_host[0] ? last_success_host : "");
            fflush(g_e10a.vfs_badpath_csv);
        }
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
