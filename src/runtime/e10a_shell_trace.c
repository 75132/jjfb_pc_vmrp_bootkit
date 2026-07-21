#include "gwy_launcher/e10a_shell_trace.h"
#include "gwy_launcher/ext_post_cont_audit.h"
#include "gwy_launcher/guest_memory.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

static struct {
    int known;
    int enabled;
    unsigned long long run_id;
    uint32_t phase_n;
    uint32_t vfs_n;
    uint32_t event_n;
    uint32_t update_n;
    FILE *phase_csv;
    FILE *vfs_csv;
    FILE *vfs_badpath_csv;
    FILE *cfg_csv;
    FILE *event_csv;
    FILE *update_csv;
    char current_phase[64];
    char last_success_guest[160];
    char last_success_host[260];
} g_e10a;

typedef struct E10AVfsCallCtx {
    unsigned long long run_id;
    uint32_t seq;
    int valid;
    uint32_t pc, lr, sp;
    uint32_t r[8];
    uint32_t r9;
    uint32_t erw;
    uint32_t path_ptr;
    char module[64];
    char phase[64];
    char api_name[64];
    char arg_source[32];
    char chosen_arg[16];
    char best_candidate[16];
    char r0_decoded[160];
    char r1_decoded[160];
    char r2_decoded[160];
    char sp0_decoded[160];
    char sp4_decoded[160];
    char raw64_hex[129];
} E10AVfsCallCtx;

static E10AVfsCallCtx g_vfs_ctx;
static uint32_t g_vfs_ctx_seq;
static char g_instr_ring[30][64];
static uint32_t g_instr_ring_n;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

int e10a_shell_trace_enabled(void) {
    if (!g_e10a.known) {
        g_e10a.enabled = env1("JJFB_E10A_SHELL_TRACE") || env1("JJFB_E10A_MODE") ||
                         env1("JJFB_E10A_GWY_SHELL_PRELAUNCH");
        {
            const char *rid = getenv("JJFB_E10A_RUN_ID");
            if (rid && rid[0]) g_e10a.run_id = strtoull(rid, NULL, 10);
        }
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
                                    "run_id,n,launch_path,phase,module,pc,lr,r0,r1,r2,r3,r9,AC8,note\n");
    }
    if (!g_e10a.vfs_csv) {
        g_e10a.vfs_csv = open_csv("JJFB_E10A_VFS_CSV", "reports/e10a_shell_vfs_trace.csv",
                                  "run_id,n,launch_path,module,op,guest_path,host_path,exists,rc,ctx_pc,ctx_lr,ctx_sp,"
                                  "ctx_r0,ctx_r1,ctx_r2,ctx_r3,ctx_r4,ctx_r5,ctx_r6,ctx_r7,ctx_r9,"
                                  "ctx_erw_guest,ctx_caller_module,ctx_phase,ctx_api,ctx_path_ptr,ctx_arg_source,"
                                  "ctx_chosen_arg,ctx_best_candidate,ctx_caller_offset\n");
    }
    if (!g_e10a.vfs_badpath_csv) {
        g_e10a.vfs_badpath_csv =
            open_csv("JJFB_E10A_VFS_BADPATH_CSV",
                     "reports/e10a_shell_vfs_badpath_trace.csv",
                     "run_id,seq,module,phase,op,decoded_path,path_len,path_ptr,raw64_hex,pc,lr,sp,"
                     "r0,r1,r2,r3,r4,r5,r6,r7,r9,erw,arg_source_guess,previous_file_op,"
                     "previous_successful_path,last_10_bl,last_30_instr,r0_decoded,r1_decoded,r2_decoded,"
                     "sp0_decoded,sp4_decoded,chosen_arg,best_candidate,note\n");
    }
    if (!g_e10a.cfg_csv) {
        g_e10a.cfg_csv = open_csv("JJFB_E10A_CFG_RUNTIME_CSV", "reports/e10a_gamelist_cfg_runtime_trace.csv",
                                  "run_id,seq,stage,cfg_buffer_ptr,cfg_buffer_size,record_offset,title,target_path,icon,target_ptr,target_bytes,file_api_args,note\n");
    }
    if (!g_e10a.event_csv) {
        g_e10a.event_csv = open_csv("JJFB_E10A_EVENT_CSV", "reports/e10a_shell_event_loop_trace.csv",
                                    "run_id,n,launch_path,event_code,handler_pc,pc,lr,note\n");
    }
    if (!g_e10a.update_csv) {
        g_e10a.update_csv = open_csv("JJFB_E10A_UPDATE_CSV", "reports/e10a_shell_update_contract_trace.csv",
                                     "run_id,n,launch_path,request,response,note\n");
    }
}

void e10a_shell_cfg_runtime(const char *stage, uint32_t cfg_buf_ptr, uint32_t cfg_buf_size,
                            uint32_t record_offset, const char *title, const char *target_path,
                            const char *icon, uint32_t target_ptr, const char *target_bytes,
                            const char *file_api_args, const char *note) {
    if (!e10a_shell_trace_enabled()) return;
    ensure_files();
    if (!g_e10a.cfg_csv) return;
    fprintf(g_e10a.cfg_csv, "%llu,%u,%s,0x%X,%u,0x%X,\"%s\",\"%s\",\"%s\",0x%X,\"%s\",\"%s\",\"%s\"\n",
            g_e10a.run_id, g_e10a.vfs_n + g_e10a.phase_n + g_e10a.event_n + g_e10a.update_n + 1u,
            stage ? stage : "", cfg_buf_ptr, cfg_buf_size, record_offset,
            title ? title : "", target_path ? target_path : "", icon ? icon : "", target_ptr,
            target_bytes ? target_bytes : "", file_api_args ? file_api_args : "", note ? note : "");
    fflush(g_e10a.cfg_csv);
}

static void escape_bytes(const uint8_t *bytes, size_t n, char *out, size_t cap) {
    size_t i;
    if (!out || cap == 0) return;
    out[0] = 0;
    for (i = 0; i < n; i++) {
        unsigned char b = bytes[i];
        size_t sl = strlen(out);
        if (sl + 5 >= cap) break;
        if (b == 0) break;
        if (b == 0x08) {
            strcat(out, "\\b");
        } else if (b >= 32 && b < 127) {
            out[sl] = (char)b;
            out[sl + 1] = 0;
        } else {
            char t[5];
            snprintf(t, sizeof(t), "\\x%02X", (unsigned)b);
            strcat(out, t);
        }
    }
    if (!out[0]) snprintf(out, cap, "(empty)");
}

static int decode_guest_candidate(void *uc0, uint32_t ptr, char *decoded, size_t dec_cap,
                                  char *raw_hex, size_t raw_hex_cap) {
    struct uc_struct *uc = (struct uc_struct *)uc0;
    uint8_t bytes[64];
    size_t i;
    if (decoded && dec_cap) decoded[0] = 0;
    if (raw_hex && raw_hex_cap) raw_hex[0] = 0;
    memset(bytes, 0, sizeof(bytes));
    if (!uc || !ptr) return 0;
    if (!guest_memory_uc_peek(uc, ptr, bytes, sizeof(bytes))) return 0;
    if (raw_hex && raw_hex_cap) {
        for (i = 0; i < sizeof(bytes); i++) {
            char t[3];
            if (strlen(raw_hex) + 2 >= raw_hex_cap) break;
            snprintf(t, sizeof(t), "%02X", (unsigned)bytes[i]);
            strcat(raw_hex, t);
        }
    }
    if (decoded && dec_cap) escape_bytes(bytes, sizeof(bytes), decoded, dec_cap);
    return 1;
}

static int decode_indirect_candidate(void *uc0, uint32_t ptr, char *decoded, size_t dec_cap) {
    uint32_t p2 = 0;
    struct uc_struct *uc = (struct uc_struct *)uc0;
    if (!uc || !ptr) return 0;
    if (!guest_memory_uc_peek_u32(uc, ptr, &p2) || !p2) return 0;
    return decode_guest_candidate(uc0, p2, decoded, dec_cap, NULL, 0);
}

static const char *classify_decoded(const char *s) {
    if (!s || !s[0]) return "empty";
    if (s[0] == '\\' && s[1] == 'b' && s[2] == 0) return "control_byte";
    if (strstr(s, "mythroad/") || strstr(s, "gwy/") || strstr(s, ".mrp") || strstr(s, ".bin"))
        return "valid_path";
    return "non_string";
}

static void join_recent_instr(char *out, size_t cap) {
    uint32_t start;
    uint32_t count;
    uint32_t i;
    if (!out || cap == 0) return;
    out[0] = 0;
    count = g_instr_ring_n < 30u ? g_instr_ring_n : 30u;
    start = g_instr_ring_n > 30u ? (g_instr_ring_n % 30u) : 0u;
    for (i = 0; i < count; i++) {
        uint32_t idx = (start + i) % 30u;
        if (g_instr_ring[idx][0]) {
            if (out[0]) strncat(out, " | ", cap - strlen(out) - 1);
            strncat(out, g_instr_ring[idx], cap - strlen(out) - 1);
        }
    }
}

void e10a_vfs_note_guest_code(const char *module, uint32_t pc) {
    uint32_t idx;
    if (!e10a_shell_trace_enabled()) return;
    idx = g_instr_ring_n % 30u;
    snprintf(g_instr_ring[idx], sizeof(g_instr_ring[idx]), "%s@0x%X", module ? module : "?", pc);
    g_instr_ring_n++;
}

void e10a_vfs_set_ctx_from_uc(void *uc, const char *api_name, const char *module,
                              const char *phase, uint32_t path_ptr, const char *arg_source) {
#ifdef GWY_HAVE_UNICORN
    uint32_t sp0 = 0, sp4 = 0;
    char tmp_hex[129];
    const char *best_kind = "empty";
    const char *kind;
    if (!e10a_shell_trace_enabled() || !uc) return;
    memset(&g_vfs_ctx, 0, sizeof(g_vfs_ctx));
    g_vfs_ctx.run_id = g_e10a.run_id;
    g_vfs_ctx.seq = ++g_vfs_ctx_seq;
    g_vfs_ctx.valid = 1;
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &g_vfs_ctx.pc);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &g_vfs_ctx.lr);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &g_vfs_ctx.sp);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0, &g_vfs_ctx.r[0]);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R1, &g_vfs_ctx.r[1]);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R2, &g_vfs_ctx.r[2]);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R3, &g_vfs_ctx.r[3]);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R4, &g_vfs_ctx.r[4]);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R5, &g_vfs_ctx.r[5]);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R6, &g_vfs_ctx.r[6]);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R7, &g_vfs_ctx.r[7]);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &g_vfs_ctx.r9);
    g_vfs_ctx.erw = ext_post_cont_audit_last_p_er_rw();
    if (!g_vfs_ctx.erw) g_vfs_ctx.erw = ext_post_cont_audit_last_registry_er_rw();
    snprintf(g_vfs_ctx.module, sizeof(g_vfs_ctx.module), "%s", module ? module : "?");
    snprintf(g_vfs_ctx.phase, sizeof(g_vfs_ctx.phase), "%s",
             phase && phase[0] ? phase : (g_e10a.current_phase[0] ? g_e10a.current_phase : "?"));
    snprintf(g_vfs_ctx.api_name, sizeof(g_vfs_ctx.api_name), "%s", api_name ? api_name : "?");
    snprintf(g_vfs_ctx.arg_source, sizeof(g_vfs_ctx.arg_source), "%s", arg_source ? arg_source : "?");
    g_vfs_ctx.path_ptr = path_ptr;
    if (g_vfs_ctx.sp) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_vfs_ctx.sp, &sp0);
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_vfs_ctx.sp + 4u, &sp4);
    }
    decode_guest_candidate(uc, g_vfs_ctx.r[0], g_vfs_ctx.r0_decoded, sizeof(g_vfs_ctx.r0_decoded),
                           g_vfs_ctx.raw64_hex, sizeof(g_vfs_ctx.raw64_hex));
    decode_guest_candidate(uc, g_vfs_ctx.r[1], g_vfs_ctx.r1_decoded, sizeof(g_vfs_ctx.r1_decoded),
                           NULL, 0);
    decode_guest_candidate(uc, g_vfs_ctx.r[2], g_vfs_ctx.r2_decoded, sizeof(g_vfs_ctx.r2_decoded),
                           NULL, 0);
    decode_guest_candidate(uc, sp0, g_vfs_ctx.sp0_decoded, sizeof(g_vfs_ctx.sp0_decoded), NULL, 0);
    decode_guest_candidate(uc, sp4, g_vfs_ctx.sp4_decoded, sizeof(g_vfs_ctx.sp4_decoded), NULL, 0);
    if ((!g_vfs_ctx.r0_decoded[0] || strcmp(classify_decoded(g_vfs_ctx.r0_decoded), "valid_path") != 0) &&
        decode_indirect_candidate(uc, g_vfs_ctx.r[0], g_vfs_ctx.r0_decoded, sizeof(g_vfs_ctx.r0_decoded))) {
        snprintf(g_vfs_ctx.chosen_arg, sizeof(g_vfs_ctx.chosen_arg), "%s", "R0_PTRPTR");
    }
    kind = classify_decoded(g_vfs_ctx.r0_decoded);
    if (strcmp(kind, "valid_path") == 0) {
        best_kind = kind;
        g_vfs_ctx.path_ptr = g_vfs_ctx.r[0];
        snprintf(g_vfs_ctx.best_candidate, sizeof(g_vfs_ctx.best_candidate), "%s", "R0");
    }
    kind = classify_decoded(g_vfs_ctx.r1_decoded);
    if (strcmp(best_kind, "valid_path") != 0 && strcmp(kind, "valid_path") == 0) {
        best_kind = kind;
        g_vfs_ctx.path_ptr = g_vfs_ctx.r[1];
        snprintf(g_vfs_ctx.best_candidate, sizeof(g_vfs_ctx.best_candidate), "%s", "R1");
    }
    kind = classify_decoded(g_vfs_ctx.r2_decoded);
    if (strcmp(best_kind, "valid_path") != 0 && strcmp(kind, "valid_path") == 0) {
        best_kind = kind;
        g_vfs_ctx.path_ptr = g_vfs_ctx.r[2];
        snprintf(g_vfs_ctx.best_candidate, sizeof(g_vfs_ctx.best_candidate), "%s", "R2");
    }
    kind = classify_decoded(g_vfs_ctx.sp0_decoded);
    if (strcmp(best_kind, "valid_path") != 0 && strcmp(kind, "valid_path") == 0) {
        best_kind = kind;
        g_vfs_ctx.path_ptr = sp0;
        snprintf(g_vfs_ctx.best_candidate, sizeof(g_vfs_ctx.best_candidate), "%s", "SP+0");
    }
    kind = classify_decoded(g_vfs_ctx.sp4_decoded);
    if (strcmp(best_kind, "valid_path") != 0 && strcmp(kind, "valid_path") == 0) {
        g_vfs_ctx.path_ptr = sp4;
        snprintf(g_vfs_ctx.best_candidate, sizeof(g_vfs_ctx.best_candidate), "%s", "SP+4");
    }
    if (!g_vfs_ctx.chosen_arg[0]) snprintf(g_vfs_ctx.chosen_arg, sizeof(g_vfs_ctx.chosen_arg), "%s",
                                           arg_source ? arg_source : "R0");
    if (!g_vfs_ctx.best_candidate[0])
        snprintf(g_vfs_ctx.best_candidate, sizeof(g_vfs_ctx.best_candidate), "%s", "NONE");
    if (g_vfs_ctx.path_ptr && !g_vfs_ctx.raw64_hex[0]) {
        tmp_hex[0] = 0;
        (void)decode_guest_candidate(uc, g_vfs_ctx.path_ptr, NULL, 0, tmp_hex, sizeof(tmp_hex));
        snprintf(g_vfs_ctx.raw64_hex, sizeof(g_vfs_ctx.raw64_hex), "%s", tmp_hex);
    }
#else
    (void)uc; (void)api_name; (void)module; (void)phase; (void)path_ptr; (void)arg_source;
#endif
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
                "%llu,%u,%s,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n",
                g_e10a.run_id, g_e10a.phase_n, lp,
                phase ? phase : "", module ? module : "", pc, lr, r0, r1, r2, r3, r9, ac8,
                note ? note : "");
        fflush(g_e10a.phase_csv);
    }
    snprintf(g_e10a.current_phase, sizeof(g_e10a.current_phase), "%s", phase ? phase : "");
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
    {
        uint32_t caller_off = ext_post_cont_audit_last_offset();
        int suspicious;
        char path_esc[160];
        char instrs[2048];
        const char *caller_mod = g_vfs_ctx.valid && g_vfs_ctx.module[0]
                                     ? g_vfs_ctx.module
                                     : ext_post_cont_audit_last_module();
        uint32_t ctx_pc = g_vfs_ctx.valid ? g_vfs_ctx.pc : ext_post_cont_audit_last_pc();
        uint32_t ctx_lr = g_vfs_ctx.valid ? g_vfs_ctx.lr : ext_post_cont_audit_last_lr();
        uint32_t ctx_sp = g_vfs_ctx.valid ? g_vfs_ctx.sp : ext_post_cont_audit_last_sp();
        uint32_t ctx_r0 = g_vfs_ctx.valid ? g_vfs_ctx.r[0] : ext_post_cont_audit_last_r0();
        uint32_t ctx_r1 = g_vfs_ctx.valid ? g_vfs_ctx.r[1] : ext_post_cont_audit_last_r1();
        uint32_t ctx_r2 = g_vfs_ctx.valid ? g_vfs_ctx.r[2] : ext_post_cont_audit_last_r2();
        uint32_t ctx_r3 = g_vfs_ctx.valid ? g_vfs_ctx.r[3] : ext_post_cont_audit_last_r3();
        uint32_t ctx_r4 = g_vfs_ctx.valid ? g_vfs_ctx.r[4] : ext_post_cont_audit_last_r4();
        uint32_t ctx_r5 = g_vfs_ctx.valid ? g_vfs_ctx.r[5] : ext_post_cont_audit_last_r5();
        uint32_t ctx_r6 = g_vfs_ctx.valid ? g_vfs_ctx.r[6] : ext_post_cont_audit_last_r6();
        uint32_t ctx_r7 = g_vfs_ctx.valid ? g_vfs_ctx.r[7] : ext_post_cont_audit_last_r7();
        uint32_t ctx_r9 = g_vfs_ctx.valid ? g_vfs_ctx.r9 : ext_post_cont_audit_last_r9();
        uint32_t ctx_erw = g_vfs_ctx.valid && g_vfs_ctx.erw ? g_vfs_ctx.erw
                                                           : ext_post_cont_audit_last_p_er_rw();
        if (!ctx_erw) ctx_erw = ext_post_cont_audit_last_registry_er_rw();
        {
            char guest_esc[160];
            size_t glen = guest_path ? strlen(guest_path) : 0;
            escape_bytes((const uint8_t *)(guest_path ? guest_path : ""), glen, guest_esc,
                         sizeof(guest_esc));
            if (g_e10a.vfs_csv) {
                fprintf(g_e10a.vfs_csv,
                        "%llu,%u,%s,%s,%s,\"%s\",\"%s\",%d,%d,0x%X,0x%X,0x%X,"
                        "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                        "0x%X,\"%s\",\"%s\",\"%s\",0x%X,\"%s\",\"%s\",\"%s\",0x%X\n",
                        g_e10a.run_id, g_e10a.vfs_n, lp, module ? module : "", op ? op : "",
                        guest_esc, host_path ? host_path : "",
                        exists, rc,
                        ctx_pc, ctx_lr, ctx_sp,
                        ctx_r0, ctx_r1, ctx_r2, ctx_r3, ctx_r4, ctx_r5, ctx_r6, ctx_r7,
                        ctx_r9, ctx_erw,
                        caller_mod ? caller_mod : "",
                        g_vfs_ctx.phase[0] ? g_vfs_ctx.phase : g_e10a.current_phase,
                        g_vfs_ctx.api_name,
                        g_vfs_ctx.path_ptr,
                        g_vfs_ctx.arg_source,
                        g_vfs_ctx.chosen_arg,
                        g_vfs_ctx.best_candidate,
                        caller_off);
                fflush(g_e10a.vfs_csv);
            }
        }

        if (exists && op && strcmp(op, "open") == 0 && guest_path && guest_path[0] && host_path && host_path[0]) {
            snprintf(g_e10a.last_success_guest, sizeof(g_e10a.last_success_guest), "%s", guest_path);
            snprintf(g_e10a.last_success_host, sizeof(g_e10a.last_success_host), "%s", host_path);
        }

        suspicious = (!exists && (!host_path || !host_path[0])) &&
                     (!guest_path || !guest_path[0] || strlen(guest_path) <= 1 ||
                      (guest_path && (unsigned char)guest_path[0] < 0x20));
        if (!suspicious && op && strcmp(op, "miss") == 0) suspicious = 1;
        if (suspicious && g_e10a.vfs_badpath_csv) {
            size_t path_len = guest_path ? strlen(guest_path) : 0;
            escape_bytes((const uint8_t *)(guest_path ? guest_path : ""), path_len, path_esc, sizeof(path_esc));
            join_recent_instr(instrs, sizeof(instrs));
            fprintf(g_e10a.vfs_badpath_csv,
                    "%llu,%u,\"%s\",\"%s\",\"%s\",\"%s\",%u,0x%X,\"%s\",0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\","
                    "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
                    g_e10a.run_id, g_e10a.vfs_n, caller_mod ? caller_mod : (module ? module : "?"),
                    g_vfs_ctx.phase[0] ? g_vfs_ctx.phase : g_e10a.current_phase,
                    op ? op : "", path_esc, (unsigned)path_len,
                    g_vfs_ctx.path_ptr, g_vfs_ctx.raw64_hex,
                    ctx_pc, ctx_lr, ctx_sp,
                    ctx_r0, ctx_r1, ctx_r2, ctx_r3, ctx_r4, ctx_r5, ctx_r6, ctx_r7, ctx_r9, ctx_erw,
                    g_vfs_ctx.arg_source[0] ? g_vfs_ctx.arg_source : "UNKNOWN",
                    op ? op : "",
                    g_e10a.last_success_guest[0] ? g_e10a.last_success_guest : "",
                    "",
                    instrs,
                    g_vfs_ctx.r0_decoded,
                    g_vfs_ctx.r1_decoded,
                    g_vfs_ctx.r2_decoded,
                    g_vfs_ctx.sp0_decoded,
                    g_vfs_ctx.sp4_decoded,
                    g_vfs_ctx.chosen_arg[0] ? g_vfs_ctx.chosen_arg : "R0",
                    g_vfs_ctx.best_candidate[0] ? g_vfs_ctx.best_candidate : "NONE",
                    strcmp(g_vfs_ctx.best_candidate, "R1") == 0 &&
                            strcmp(classify_decoded(g_vfs_ctx.r1_decoded), "valid_path") == 0 &&
                            (ctx_r0 == 0x08u || strcmp(classify_decoded(g_vfs_ctx.r0_decoded), "control_byte") == 0)
                        ? "SHELL_FILE_API_ARG_REGISTER_WRONG"
                        : (ctx_r0 == 0
                               ? "SHELL_VFS_BADPATH_ORIGIN_FOUND:R0_NULL_at_mr_open"
                               : "SHELL_VFS_BADPATH_TRACE_WRITTEN"));
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
        fprintf(g_e10a.event_csv, "%llu,%u,%s,0x%X,0x%X,0x%X,0x%X,%s\n", g_e10a.run_id,
                g_e10a.event_n, lp,
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
        fprintf(g_e10a.update_csv, "%llu,%u,%s,%s,%s,%s\n", g_e10a.run_id, g_e10a.update_n, lp,
                request ? request : "", response ? response : "", note ? note : "");
        fflush(g_e10a.update_csv);
    }
    fflush(stdout);
}

#define E10A_OPEN_BRANCH_RING 12

static uint32_t g_mr_open_stub;
static struct {
    uint32_t call_pc;
    uint32_t target;
    uint32_t r0, r1, r9, lr;
    int rm;
    char kind[16];
} g_open_branch_ring[E10A_OPEN_BRANCH_RING];
static uint32_t g_open_branch_n;
static uint32_t g_open_branch_null_n;

void e10a_note_mr_open_stub(uint32_t stub_addr) {
    g_mr_open_stub = stub_addr & ~1u;
    if (e10a_shell_trace_enabled() && g_mr_open_stub) {
        printf("[JJFB_E10A_OPEN_STUB] addr=0x%X evidence=OBSERVED\n", g_mr_open_stub);
        fflush(stdout);
    }
}

int e10a_is_mr_open_stub(uint32_t addr) {
    if (!g_mr_open_stub) return 0;
    return (addr & ~1u) == g_mr_open_stub;
}

void e10a_note_open_enter_branch(uint32_t call_pc, uint32_t target, const char *kind, int rm,
                                 uint32_t r0, uint32_t r1, uint32_t r9, uint32_t lr) {
    uint32_t idx;
    if (!e10a_shell_trace_enabled()) return;
    if (!e10a_is_mr_open_stub(target)) return;
    idx = g_open_branch_n % E10A_OPEN_BRANCH_RING;
    g_open_branch_ring[idx].call_pc = call_pc;
    g_open_branch_ring[idx].target = target & ~1u;
    g_open_branch_ring[idx].r0 = r0;
    g_open_branch_ring[idx].r1 = r1;
    g_open_branch_ring[idx].r9 = r9;
    g_open_branch_ring[idx].lr = lr;
    g_open_branch_ring[idx].rm = rm;
    snprintf(g_open_branch_ring[idx].kind, sizeof(g_open_branch_ring[idx].kind), "%s",
             kind ? kind : "?");
    g_open_branch_n++;
    if (r0 == 0) g_open_branch_null_n++;
    /* Always log NULL-path entries; also log first few healthy ones for ABI contrast. */
    if (r0 == 0 || g_open_branch_n <= 8u) {
        printf("[JJFB_E10A_OPEN_ENTER_BRANCH] seq=%u call_pc=0x%X target=0x%X kind=%s rm=%d "
               "r0=0x%X r1=0x%X r9=0x%X lr=0x%X evidence=OBSERVED\n",
               g_open_branch_n, call_pc, target & ~1u, kind ? kind : "?", rm, r0, r1, r9, lr);
        fflush(stdout);
    }
}

void e10a_dump_open_enter_branches(const char *why) {
    uint32_t count;
    uint32_t start;
    uint32_t i;
    if (!e10a_shell_trace_enabled()) return;
    count = g_open_branch_n < E10A_OPEN_BRANCH_RING ? g_open_branch_n : E10A_OPEN_BRANCH_RING;
    start = g_open_branch_n > E10A_OPEN_BRANCH_RING ? (g_open_branch_n % E10A_OPEN_BRANCH_RING) : 0u;
    printf("[JJFB_E10A_OPEN_ENTER_BRANCH_DUMP] why=%s stub=0x%X total=%u null_r0=%u "
           "ring=%u evidence=OBSERVED\n",
           why ? why : "?", g_mr_open_stub, g_open_branch_n, g_open_branch_null_n, count);
    for (i = 0; i < count; i++) {
        uint32_t idx = (start + i) % E10A_OPEN_BRANCH_RING;
        printf("[JJFB_E10A_OPEN_ENTER_BRANCH_DUMP] i=%u call_pc=0x%X target=0x%X kind=%s rm=%d "
               "r0=0x%X r1=0x%X r9=0x%X lr=0x%X\n",
               i, g_open_branch_ring[idx].call_pc, g_open_branch_ring[idx].target,
               g_open_branch_ring[idx].kind, g_open_branch_ring[idx].rm,
               g_open_branch_ring[idx].r0, g_open_branch_ring[idx].r1,
               g_open_branch_ring[idx].r9, g_open_branch_ring[idx].lr);
    }
    fflush(stdout);
}

#define E10A_GUEST_PC_RING 24

static struct {
    uint32_t pc, r0, r1, lr;
} g_guest_pc_ring[E10A_GUEST_PC_RING];
static uint32_t g_guest_pc_n;

void e10a_note_guest_pc_sample(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t lr) {
    uint32_t idx;
    uint32_t pca;
    if (!e10a_shell_trace_enabled()) return;
    pca = pc & ~1u;
    /* Slot 0 = last PC from any watched module (always). */
    g_guest_pc_ring[0].pc = pc;
    g_guest_pc_ring[0].r0 = r0;
    g_guest_pc_ring[0].r1 = r1;
    g_guest_pc_ring[0].lr = lr;
    /* Hot-window ring in remaining slots. */
    if (!((pca >= 0x30CC00u && pca < 0x30D200u) || (pca >= 0x2F6180u && pca < 0x2F6300u) ||
          (pca >= 0x2EBD00u && pca < 0x2EBE00u)))
        return;
    idx = 1u + (g_guest_pc_n % (E10A_GUEST_PC_RING - 1u));
    g_guest_pc_ring[idx].pc = pc;
    g_guest_pc_ring[idx].r0 = r0;
    g_guest_pc_ring[idx].r1 = r1;
    g_guest_pc_ring[idx].lr = lr;
    g_guest_pc_n++;
}

void e10a_dump_guest_pc_ring(const char *why) {
    uint32_t i;
    uint32_t n;
    if (!e10a_shell_trace_enabled()) return;
    n = g_guest_pc_n < (E10A_GUEST_PC_RING - 1u) ? g_guest_pc_n : (E10A_GUEST_PC_RING - 1u);
    printf("[JJFB_E10A_GUEST_PC_RING] why=%s last_pc=0x%X last_r0=0x%X last_r1=0x%X last_lr=0x%X "
           "hot_total=%u evidence=OBSERVED\n",
           why ? why : "?", g_guest_pc_ring[0].pc, g_guest_pc_ring[0].r0, g_guest_pc_ring[0].r1,
           g_guest_pc_ring[0].lr, g_guest_pc_n);
    for (i = 0; i < n; i++) {
        uint32_t idx = 1u + ((g_guest_pc_n >= (E10A_GUEST_PC_RING - 1u)
                                  ? (g_guest_pc_n - (E10A_GUEST_PC_RING - 1u) + i)
                                  : i) %
                             (E10A_GUEST_PC_RING - 1u));
        printf("[JJFB_E10A_GUEST_PC_RING] i=%u pc=0x%X r0=0x%X r1=0x%X lr=0x%X\n", i,
               g_guest_pc_ring[idx].pc, g_guest_pc_ring[idx].r0, g_guest_pc_ring[idx].r1,
               g_guest_pc_ring[idx].lr);
    }
    fflush(stdout);
}
