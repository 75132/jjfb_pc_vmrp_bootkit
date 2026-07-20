#include "gwy_launcher/robotol_flag_writer_trace.h"
#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/platform_handler_registry.h"
#include "gwy_launcher/robotol_idle_watch.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define E8F_MAX_BP 256
#define E8I_STATE_OFF (0x800u + 0xD0u) /* audit-safe: former state/ui word */
#define E8W_SCRATCH_BASE 0x3900000u
#define E8W_SCRATCH_SIZE 0x1000u
#define E8Y_A64_SCRATCH 0x3910000u
#define E8Y_A64_SCRATCH_SIZE 0x1000u
#define E8Z_PIXEL_BASE 0x3920000u
#define E8Z_PIXEL_MAP_SIZE 0x40000u /* room for large UI members (e.g. slogo 18KB+) */
#define E8Z_BMP_BYTES 242
#define E8Z_BMP_W 11
#define E8Z_BMP_H 11

typedef struct E8fBp {
    int used;
    uint32_t pc; /* even */
    char tag[16];
    uint32_t hit_n;
    uint32_t first_tick;
    uint32_t last_r0;
    uint32_t last_r1;
    uint32_t last_r9;
#ifdef GWY_HAVE_UNICORN
    uc_hook hook;
#endif
} E8fBp;

static struct {
    int known;
    int enabled;
    int writer_bp;
    int sibling_probe;
    int longpath_watch;
    int caller_bp;
    int fault_watch;
    int dispatcher_bp;
    int parent_bp;
    int state_watch;
    int e8j_bp;
    int e8j_queue_read_watch;
    int svc_trap;
    int svc_stop;
    int armed;
    int state_watch_armed;
    int e8j_qread_armed;
    int sibling_done;
    int counterfactual_done;
    int probe10162_done;
    int probe10102_done;
    int e8k_10102_done;
    int fault_dumped;
    int svc_trapped;
    void *uc;
    uint32_t tick;
    char sibling_mode[16];
    char counterfactual[16];
    uint32_t e8k_10102_case; /* 0 = disabled; else R0 case index for observe-only 0x10102 fire */
    uint32_t e8l_10102_r1;   /* E8L: R1 payload (event code or pointer); default 0 */
    uint32_t e8l_10102_r2;
    uint32_t e8l_10102_r3;
    int e8l_regs_set; /* 1 if JJFB_E8L_10102_R{1,2,3} or REGS parsed */
    int e8m_parent_trace; /* range CODE hook inside 0x300158 body */
    int e8m_seq_10165;
    int e8m_seq_310;
    int e8m_seq_done;
    char e8m_seq[48]; /* e.g. 10165+310+156:18 */
    uint32_t e8m_insn_n;
    uint32_t e8m_path_hits;
    int e8n_cf_state_set;
    uint32_t e8n_cf_state; /* COUNTERFACTUAL_ONLY poke of R9+0x8D0 before case156 */
    int e8n_cf_done;
    int fast_assist; /* JJFB_FAST_ASSIST=1 — not product success */
    char fast_svc_mode; /* 0=off, 'o'=observe, 'r'=return0, 'p'=preserve */
    uint32_t fast_svc_continues;
    /* E8P: field assists on R9-embedded structs (NOT heap pointer slots). */
    int fast_eec7c_set;
    uint32_t fast_eec7c_val; /* poke *(R9+0xEEC+0x7C) — used by early case arms */
    int fast_dec30_set;
    uint32_t fast_dec30_val; /* poke *(R9+0xDEC+0x30) */
    int fast_c6c22_set;
    uint32_t fast_c6c22_val; /* poke *(u8*)(R9+0xC6C+0x22) — R1=20 CMP#1 gate */
    uint64_t fast_insn_limit; /* 0 = default 400000 */
    /* E8R: invoke real unlock fn 0x2FC8C0 (not direct C44 poke). */
    int fast_unlock_call; /* JJFB_FAST_UNLOCK_CALL=1 */
    int fast_unlock_done;
    char fast_unlock_when[12]; /* before | after (case156); default before */
    /* E8S: call real UI-init 0x2E4788 (not C44/C9D/CF5 poke). */
    int fast_ui_init_call;
    int fast_ui_init_done;
    int fast_ui_state_set;
    uint32_t fast_ui_state; /* optional state before UI-init; 38 is REJECTED by UI-init */
    int fast_ui_ed8_set;
    uint32_t fast_ui_ed8_val;
    int fast_ui_ca3_set;
    uint32_t fast_ui_ca3_val;
    /* E8U-DisplayFirst: runtime branch assist (no C9D/CF5 memory poke). */
    int display_first;
    int bypass_c9d_gate;
    int bypass_cf5_gate;
    uint32_t c9d_assist_n;
    uint32_t cf5_assist_n;
    uint32_t idle_success_n;
    uint32_t ui_obj_r0; /* captured/live UI object for 0x2E4788 */
    int ui_obj_set;
    int fast_ui_upstream; /* 0=off, 1=0x2E2520, 2=0x2E2F50(mid-fn; needs obj) */
    int fast_ui_upstream_done;
    /* E8V-FirstFrame: deep-trace idle success 0x2E88CC toward real draw. */
    int e8v_mode;
    int e8v_e88cc_trace;
    int e8v_call_2e993c; /* Case C: call R9-only downstream after first 2E88CC */
    int e8v_call_2e993c_done;
    uint32_t e8v_insn_n;
    uint32_t e8v_insn_max;
    uint32_t e8v_bl_n;
    uint32_t e8v_early_exit_n;
    uint32_t e8v_draw_cand_n;
    uint32_t e8v_e88cc_entries;
    uint32_t e8v_f6c_base;
    uint32_t e8v_f6c_p4;
    uint32_t e8v_f6c_p8;
    int16_t e8v_d14_s16;
    /* E8W-FirstFrame: F6C embedded struct (F70/F74) acquire + re-enter 0x2E88CC. */
    int e8w_mode;
    int e8w_f6c_assist; /* JJFB_FAST_F6C_OBJECT_ASSIST — structural only */
    int e8w_f6c_assist_done;
    int e8w_reenter_e88cc;
    int e8w_reenter_done;
    uint32_t e8w_writer_hit_n;
    uint32_t e8w_f74_assist;
    uint32_t e8w_scratch_base;
    /* E8X-FirstFrame: 0x2F2854 → 0x2EA188 → 0x2F449C real draw path. */
    int e8x_mode;
    int e8x_f74_desc_assist; /* JJFB_FAST_F74_DESCRIPTOR_ASSIST — dims/fields only */
    int e8x_f74_desc_assist_done;
    int e8x_call_2f99d0;
    int e8x_call_2f99d0_done;
    uint32_t e8x_2f2854_n;
    uint32_t e8x_2ea188_n;
    uint32_t e8x_2f449c_n;
    uint32_t e8x_310bbc_n;
    uint32_t e8x_insn_n;
    uint32_t e8x_insn_max;
    /* E8Y-FirstFrame: 0x2D92E4 resource resolve → A64/A68/A6C → 0x310BBC. */
    int e8y_mode;
    int e8y_a64_assist; /* JJFB_FAST_A64_RESOURCE_ASSIST — structural handles only */
    int e8y_a64_assist_done;
    uint32_t e8y_2d92e4_n;
    uint32_t e8y_2d92e4_ret_nz;
    uint32_t e8y_2d92e4_ret_z;
    uint32_t e8y_a64_write_n;
    uint32_t e8y_310bbc_n;
    uint32_t e8y_insn_n;
    uint32_t e8y_insn_max;
    uint32_t e8y_last_name_va;
    char e8y_last_name[48];
    /* E8Z-FirstPixel: real MRP member pixels into handle+4. */
    int e8z_mode;
    int e8z_real_bmp; /* JJFB_FAST_REAL_BMP_HANDLE */
    int e8z_member_fastpath; /* JJFB_DISPLAY_FIRST_MEMBER_FASTPATH */
    int e8z_real_bmp_done;
    int e8z_fastpath_done;
    uint32_t e8z_pixel_va;
    uint32_t e8z_pixel_bytes;
    uint32_t e8z_handle_va;
    uint16_t e8z_bw;
    uint16_t e8z_bh;
    /* E9A: real member bridge at 0x304BF0 (replaces stalled guest index scan). */
    int e9a_mode;
    int e9a_member_bridge; /* JJFB_REAL_MRP_MEMBER_BRIDGE */
    uint32_t e9a_bridge_n;
    uint32_t e9a_bridge_hit_n;
    uint32_t e9a_bridge_miss_n;
    uint32_t e9a_pixel_slot;
    int e9a_post_a64_draw_skip; /* after first bridged A64 store → 0x2F45A2 */
    /* E9D: natural 0x304BF0 name match (instrument + optional host strcmp shim). */
    int e9d_mode;
    int e9d_strcmp_shim; /* JJFB_E9D_STRCMP_SHIM — host strcmp at 0x304F92 */
    int e9d_bridge_fallback; /* natural first; bridge only on miss/timeout */
    uint32_t e9d_cmp_n;
    uint32_t e9d_cmp_match_n;
    uint32_t e9d_shim_n;
    uint32_t e9d_natural_hit_n;
    uint32_t e9d_req_n;
    char e9d_req_name[96];
    uint32_t e9d_req_tick;
    FILE *e9d_cmp_csv;
    FILE *e9d_req_csv;
    /* E9E: after natural match, complete member load via host I/O (original bytes). */
    int e9e_mode;
    int e9e_postmatch_shims;
    int e9e_postmatch_done;
    uint32_t e9e_304bf0_lr;
    uint32_t e9e_304bf0_name_va;
    uint32_t e9e_304bf0_handle_va;
    uint32_t e9e_304bf0_out_va;
    /* Entry ABI snapshot: mid-304BF0 return must restore these or 2D92E4 faults. */
    uint32_t e9e_304bf0_sp;
    uint32_t e9e_304bf0_r4;
    uint32_t e9e_304bf0_r5;
    uint32_t e9e_304bf0_r6;
    uint32_t e9e_304bf0_r7;
    uint32_t e9e_304bf0_r8;
    uint32_t e9e_304bf0_r9;
    uint32_t e9e_304bf0_r10;
    uint32_t e9e_304bf0_r11;
    FILE *e9e_helper_csv;
    E8fBp bps[E8F_MAX_BP];
    int nbps;
    uint32_t longpath_hits;
    uint32_t queue_depth_at_probe;
    uint32_t fault_hits;
    uint32_t svc_hits;
    uint32_t state_writes;
    uint32_t state_last_val;
    uint32_t state_last_writer;
    uint32_t parent_hit_n;
    uint32_t e8j_entry_hits;
    uint32_t e8j_upstream_hits;
    uint32_t e8j_queue_hits;
    uint32_t e8j_bl_hits;
    uint32_t e8j_qread_hits;
#ifdef GWY_HAVE_UNICORN
    uc_hook svc_code_hook;
    uc_hook svc_intr_hook;
    uc_hook state_hook;
    uc_hook e8j_qread_hook;
    uc_hook e8m_parent_hook;
    uc_hook e8v_e88cc_hook;
    uc_hook e8x_wrap_hook;
    uc_hook e8x_worker_hook;
    uc_hook e8x_2f449c_hook;
    uc_hook e8y_2d92e4_hook;
#endif
} g_e8f;

static int env1(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

static int parse_hex_list(const char *s, uint32_t *out, int maxn) {
    int n = 0;
    const char *p = s;
    if (!s || !s[0]) return 0;
    while (*p && n < maxn) {
        char *end = NULL;
        unsigned long v;
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;
        v = strtoul(p, &end, 0);
        if (end == p) break;
        out[n++] = (uint32_t)v;
        p = end;
    }
    return n;
}

/* E8J: role-tagged CSV like "e:0x2DFC3C,u:0x30D300,q:0x2DC80C,p:0x300158,b:0x2DFDD6" */
static int parse_role_spec(const char *s, uint32_t *pcs, char roles[][4], int maxn) {
    int n = 0;
    const char *p = s;
    if (!s || !s[0]) return 0;
    while (*p && n < maxn) {
        char *end = NULL;
        unsigned long v;
        char role = 'p';
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z')) && p[1] == ':') {
            role = (char)((p[0] >= 'A' && p[0] <= 'Z') ? (p[0] - 'A' + 'a') : p[0]);
            p += 2;
        }
        v = strtoul(p, &end, 0);
        if (end == p) break;
        pcs[n] = (uint32_t)v;
        roles[n][0] = role;
        roles[n][1] = '\0';
        n++;
        p = end;
    }
    return n;
}

int robotol_flag_writer_trace_enabled(void) {
    const char *sib;
    const char *cf;
    if (!g_e8f.known) {
        g_e8f.writer_bp = env1("JJFB_E8F_WRITER_BP");
        g_e8f.sibling_probe = env1("JJFB_E8F_SIBLING_PROBE");
        g_e8f.longpath_watch = env1("JJFB_E8F_LONGPATH_WATCH");
        g_e8f.caller_bp = env1("JJFB_E8G_CALLER_BP");
        g_e8f.fault_watch = env1("JJFB_E8G_FAULT_WATCH");
        g_e8f.dispatcher_bp = env1("JJFB_E8H_DISPATCHER_BP");
        g_e8f.parent_bp = env1("JJFB_E8I_PARENT_BP");
        g_e8f.state_watch = env1("JJFB_E8I_STATE_WATCH");
        g_e8f.e8j_bp = env1("JJFB_E8J_CLUSTER_BP");
        g_e8f.e8j_queue_read_watch = env1("JJFB_E8J_QUEUE_READ_WATCH");
        g_e8f.svc_trap = env1("JJFB_E8H_SVC_TRAP");
        /* Default stop-on-hit when trap armed unless explicitly disabled. */
        g_e8f.svc_stop = g_e8f.svc_trap && !env1("JJFB_E8H_SVC_NO_STOP");
        if (env1("JJFB_E8H_SVC_STOP")) g_e8f.svc_stop = 1;
        sib = getenv("JJFB_E8F_SIBLING");
        if (sib && sib[0]) {
            strncpy(g_e8f.sibling_mode, sib, sizeof(g_e8f.sibling_mode) - 1);
        } else {
            strncpy(g_e8f.sibling_mode, "BOTH", sizeof(g_e8f.sibling_mode) - 1);
        }
        cf = getenv("JJFB_E8F_COUNTERFACTUAL");
        if (cf && cf[0]) {
            strncpy(g_e8f.counterfactual, cf, sizeof(g_e8f.counterfactual) - 1);
        }
        {
            const char *c10102 = getenv("JJFB_E8K_10102_CASE");
            if (c10102 && c10102[0]) {
                char *end = NULL;
                unsigned long v = strtoul(c10102, &end, 0);
                if (end != c10102)
                    g_e8f.e8k_10102_case = (uint32_t)v;
            }
        }
        /* E8L: R0=case via E8K_CASE; R1-R3 via REGS=r0,r1,r2,r3 or R1/R2/R3. */
        {
            const char *regs = getenv("JJFB_E8L_10102_REGS");
            if (regs && regs[0]) {
                uint32_t v[4];
                int n = parse_hex_list(regs, v, 4);
                if (n >= 1) {
                    g_e8f.e8k_10102_case = v[0];
                    g_e8f.e8l_regs_set = 1;
                }
                if (n >= 2) g_e8f.e8l_10102_r1 = v[1];
                if (n >= 3) g_e8f.e8l_10102_r2 = v[2];
                if (n >= 4) g_e8f.e8l_10102_r3 = v[3];
            } else {
                const char *r1 = getenv("JJFB_E8L_10102_R1");
                const char *r2 = getenv("JJFB_E8L_10102_R2");
                const char *r3 = getenv("JJFB_E8L_10102_R3");
                if (r1 && r1[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(r1, &end, 0);
                    if (end != r1) {
                        g_e8f.e8l_10102_r1 = (uint32_t)v;
                        g_e8f.e8l_regs_set = 1;
                    }
                }
                if (r2 && r2[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(r2, &end, 0);
                    if (end != r2) {
                        g_e8f.e8l_10102_r2 = (uint32_t)v;
                        g_e8f.e8l_regs_set = 1;
                    }
                }
                if (r3 && r3[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(r3, &end, 0);
                    if (end != r3) {
                        g_e8f.e8l_10102_r3 = (uint32_t)v;
                        g_e8f.e8l_regs_set = 1;
                    }
                }
            }
        }
        g_e8f.e8m_parent_trace = env1("JJFB_E8M_PARENT_TRACE");
        {
            const char *seq = getenv("JJFB_E8M_SEQ");
            if (seq && seq[0]) {
                strncpy(g_e8f.e8m_seq, seq, sizeof(g_e8f.e8m_seq) - 1);
                if (strstr(seq, "10165")) g_e8f.e8m_seq_10165 = 1;
                /* Require standalone 310 token — avoid matching digits inside 10165. */
                if (strstr(seq, "+310") || strstr(seq, "310+") || strcmp(seq, "310") == 0 ||
                    strncmp(seq, "310+", 4) == 0)
                    g_e8f.e8m_seq_310 = 1;
                {
                    const char *p156 = strstr(seq, "156:");
                    if (p156) {
                        char *end = NULL;
                        unsigned long r1 = strtoul(p156 + 4, &end, 0);
                        g_e8f.e8k_10102_case = 156u;
                        g_e8f.e8l_10102_r1 = (uint32_t)r1;
                        g_e8f.e8l_regs_set = 1;
                    } else if (strstr(seq, "156")) {
                        g_e8f.e8k_10102_case = 156u;
                    }
                }
            }
        }
        {
            const char *cfst = getenv("JJFB_E8N_CF_STATE");
            if (cfst && cfst[0]) {
                char *end = NULL;
                unsigned long v = strtoul(cfst, &end, 0);
                if (end != cfst) {
                    g_e8f.e8n_cf_state = (uint32_t)v;
                    g_e8f.e8n_cf_state_set = 1;
                }
            }
        }
        /* E8O-Fast: controlled assist — never product success. */
        g_e8f.fast_assist = env1("JJFB_FAST_ASSIST");
        if (g_e8f.fast_assist) {
            const char *fsm = getenv("JJFB_FAST_SVC_AB");
            const char *fst = getenv("JJFB_FAST_STATE");
            const char *fr1 = getenv("JJFB_FAST_CASE156_R1");
            const char *fseq = getenv("JJFB_FAST_SEQUENCE");
            g_e8f.fast_svc_mode = 'o'; /* default observe */
            if (fsm && fsm[0]) {
                if (strcmp(fsm, "return0") == 0)
                    g_e8f.fast_svc_mode = 'r';
                else if (strcmp(fsm, "preserve") == 0)
                    g_e8f.fast_svc_mode = 'p';
                else
                    g_e8f.fast_svc_mode = 'o';
            }
            /* Arm SVC trap for fast path. */
            g_e8f.svc_trap = 1;
            g_e8f.svc_stop = (g_e8f.fast_svc_mode == 'o');
            if (fst && fst[0]) {
                char *end = NULL;
                unsigned long v = strtoul(fst, &end, 0);
                if (end != fst) {
                    g_e8f.e8n_cf_state = (uint32_t)v;
                    g_e8f.e8n_cf_state_set = 1;
                }
            } else if (!g_e8f.e8n_cf_state_set) {
                g_e8f.e8n_cf_state = 38u;
                g_e8f.e8n_cf_state_set = 1;
            }
            g_e8f.e8k_10102_case = 156u;
            g_e8f.e8l_10102_r1 = 18u;
            g_e8f.e8l_regs_set = 1;
            if (fr1 && fr1[0]) {
                char *end = NULL;
                unsigned long v = strtoul(fr1, &end, 0);
                if (end != fr1) g_e8f.e8l_10102_r1 = (uint32_t)v;
            }
            if (fseq && fseq[0]) {
                strncpy(g_e8f.e8m_seq, fseq, sizeof(g_e8f.e8m_seq) - 1);
                if (strstr(fseq, "10165")) g_e8f.e8m_seq_10165 = 1;
                if (strstr(fseq, "case310") || strstr(fseq, "310")) g_e8f.e8m_seq_310 = 1;
            } else {
                strncpy(g_e8f.e8m_seq, "case156", sizeof(g_e8f.e8m_seq) - 1);
            }
            {
                const char *e7 = getenv("JJFB_FAST_EEC7C");
                const char *d30 = getenv("JJFB_FAST_DEC30");
                const char *c22 = getenv("JJFB_FAST_C6C22");
                const char *ilim = getenv("JJFB_FAST_INSN_LIMIT");
                if (e7 && e7[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(e7, &end, 0);
                    if (end != e7) {
                        g_e8f.fast_eec7c_val = (uint32_t)v;
                        g_e8f.fast_eec7c_set = 1;
                    }
                }
                if (d30 && d30[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(d30, &end, 0);
                    if (end != d30) {
                        g_e8f.fast_dec30_val = (uint32_t)v;
                        g_e8f.fast_dec30_set = 1;
                    }
                }
                if (c22 && c22[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(c22, &end, 0);
                    if (end != c22) {
                        g_e8f.fast_c6c22_val = (uint32_t)v;
                        g_e8f.fast_c6c22_set = 1;
                    }
                }
                if (ilim && ilim[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(ilim, &end, 0);
                    if (end != ilim && v > 0) g_e8f.fast_insn_limit = (uint64_t)v;
                }
            }
            {
                const char *ucall = getenv("JJFB_FAST_UNLOCK_CALL");
                const char *uwhen = getenv("JJFB_FAST_UNLOCK_WHEN");
                if (ucall && ucall[0] == '1' && ucall[1] == '\0')
                    g_e8f.fast_unlock_call = 1;
                strncpy(g_e8f.fast_unlock_when, "before", sizeof(g_e8f.fast_unlock_when) - 1);
                if (uwhen && uwhen[0]) {
                    if (strcmp(uwhen, "after") == 0)
                        strncpy(g_e8f.fast_unlock_when, "after", sizeof(g_e8f.fast_unlock_when) - 1);
                    else
                        strncpy(g_e8f.fast_unlock_when, "before", sizeof(g_e8f.fast_unlock_when) - 1);
                }
            }
            {
                const char *uic = getenv("JJFB_FAST_UI_INIT_CALL");
                const char *uist = getenv("JJFB_FAST_UI_STATE");
                const char *ed8 = getenv("JJFB_FAST_UI_ED8");
                const char *ca3 = getenv("JJFB_FAST_UI_CA3");
                if (uic && uic[0] == '1' && uic[1] == '\0')
                    g_e8f.fast_ui_init_call = 1;
                if (uist && uist[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(uist, &end, 0);
                    if (end != uist) {
                        g_e8f.fast_ui_state = (uint32_t)v;
                        g_e8f.fast_ui_state_set = 1;
                    }
                }
                if (ed8 && ed8[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(ed8, &end, 0);
                    if (end != ed8) {
                        g_e8f.fast_ui_ed8_val = (uint32_t)v;
                        g_e8f.fast_ui_ed8_set = 1;
                    }
                }
                if (ca3 && ca3[0]) {
                    char *end = NULL;
                    unsigned long v = strtoul(ca3, &end, 0);
                    if (end != ca3) {
                        g_e8f.fast_ui_ca3_val = (uint32_t)v;
                        g_e8f.fast_ui_ca3_set = 1;
                    }
                }
                {
                    const char *uobj = getenv("JJFB_FAST_UI_OBJECT_R0");
                    const char *uup = getenv("JJFB_FAST_UI_UPSTREAM");
                    if (uobj && uobj[0]) {
                        char *end = NULL;
                        unsigned long v = strtoul(uobj, &end, 0);
                        if (end != uobj && v != 0) {
                            g_e8f.ui_obj_r0 = (uint32_t)v;
                            g_e8f.ui_obj_set = 1;
                        }
                    }
                    if (uup && uup[0]) {
                        if (strcmp(uup, "2E2520") == 0 || strcmp(uup, "0x2E2520") == 0)
                            g_e8f.fast_ui_upstream = 1;
                        else if (strcmp(uup, "2E2F50") == 0 || strcmp(uup, "0x2E2F50") == 0)
                            g_e8f.fast_ui_upstream = 2;
                    }
                }
            }
            /* E8U: display-first may also arm under FAST_ASSIST. */
            g_e8f.display_first = env1("JJFB_DISPLAY_FIRST");
            g_e8f.bypass_c9d_gate = env1("JJFB_BYPASS_C9D_GATE");
            g_e8f.bypass_cf5_gate = env1("JJFB_BYPASS_CF5_GATE");
            if (g_e8f.display_first && !g_e8f.bypass_c9d_gate)
                g_e8f.bypass_c9d_gate = 1; /* default: C9D branch assist when DISPLAY_FIRST */
            printf("[JJFB_FAST_ASSIST] enabled=1 state=0x%X case156_r1=0x%X seq=%s "
                   "svc_ab=%c eec7c=%s dec30=%s c6c22=%s insn_limit=%llu "
                   "unlock_call=%d unlock_when=%s ui_init_call=%d ui_state=%s "
                   "display_first=%d bypass_c9d=%d bypass_cf5=%d ui_upstream=%d "
                   "note=NOT_PRODUCT_SUCCESS evidence=HYPOTHESIS\n",
                   g_e8f.e8n_cf_state, g_e8f.e8l_10102_r1, g_e8f.e8m_seq, g_e8f.fast_svc_mode,
                   g_e8f.fast_eec7c_set ? "set" : "off", g_e8f.fast_dec30_set ? "set" : "off",
                   g_e8f.fast_c6c22_set ? "set" : "off",
                   (unsigned long long)(g_e8f.fast_insn_limit ? g_e8f.fast_insn_limit : 400000ull),
                   g_e8f.fast_unlock_call, g_e8f.fast_unlock_when, g_e8f.fast_ui_init_call,
                   g_e8f.fast_ui_state_set ? "set" : "off", g_e8f.display_first,
                   g_e8f.bypass_c9d_gate, g_e8f.bypass_cf5_gate, g_e8f.fast_ui_upstream);
            if (g_e8f.display_first) {
                printf("[JJFB_DISPLAY_FIRST] enabled=1 bypass_c9d=%d bypass_cf5=%d "
                       "note=DISPLAY_FIRST_BRANCH_ASSIST NOT_PRODUCT_SUCCESS "
                       "evidence=HYPOTHESIS\n",
                       g_e8f.bypass_c9d_gate, g_e8f.bypass_cf5_gate);
            }
            fflush(stdout);
        } else {
            /* Display-first can arm without full FAST_ASSIST (still not product). */
            g_e8f.display_first = env1("JJFB_DISPLAY_FIRST");
            g_e8f.bypass_c9d_gate = env1("JJFB_BYPASS_C9D_GATE");
            g_e8f.bypass_cf5_gate = env1("JJFB_BYPASS_CF5_GATE");
            if (g_e8f.display_first && !g_e8f.bypass_c9d_gate) g_e8f.bypass_c9d_gate = 1;
            if (g_e8f.display_first) {
                printf("[JJFB_DISPLAY_FIRST] enabled=1 bypass_c9d=%d bypass_cf5=%d "
                       "note=DISPLAY_FIRST_BRANCH_ASSIST NOT_PRODUCT_SUCCESS "
                       "evidence=HYPOTHESIS\n",
                       g_e8f.bypass_c9d_gate, g_e8f.bypass_cf5_gate);
                fflush(stdout);
            }
        }
        /* E8V-FirstFrame: always parse (may pair with DISPLAY_FIRST Case A). */
        g_e8f.e8v_mode = env1("JJFB_E8V_MODE");
        g_e8f.e8v_e88cc_trace = env1("JJFB_E8V_E88CC_TRACE") || g_e8f.e8v_mode;
        g_e8f.e8v_call_2e993c = env1("JJFB_E8V_CALL_2E993C");
        g_e8f.e8v_insn_max = 2000u;
        {
            const char *im = getenv("JJFB_E8V_INSN_LIMIT");
            if (im && im[0]) {
                char *end = NULL;
                unsigned long v = strtoul(im, &end, 0);
                if (end != im && v > 0 && v <= 20000u) g_e8f.e8v_insn_max = (uint32_t)v;
            }
        }
        if (g_e8f.e8v_mode) {
            if (!g_e8f.display_first) {
                g_e8f.display_first = env1("JJFB_DISPLAY_FIRST");
                g_e8f.bypass_c9d_gate = env1("JJFB_BYPASS_C9D_GATE");
                if (g_e8f.display_first && !g_e8f.bypass_c9d_gate) g_e8f.bypass_c9d_gate = 1;
            }
            printf("[JJFB_E8V_FIRSTFRAME] enabled=1 e88cc_trace=%d insn_max=%u call_2e993c=%d "
                   "display_first=%d note=NOT_PRODUCT_SUCCESS evidence=HYPOTHESIS\n",
                   g_e8f.e8v_e88cc_trace, g_e8f.e8v_insn_max, g_e8f.e8v_call_2e993c,
                   g_e8f.display_first);
            fflush(stdout);
        }
        /* E8W-FirstFrame: F70/F74 gate + optional structural F74 assist. */
        g_e8f.e8w_mode = env1("JJFB_E8W_MODE");
        g_e8f.e8w_f6c_assist = env1("JJFB_FAST_F6C_OBJECT_ASSIST");
        g_e8f.e8w_reenter_e88cc = env1("JJFB_E8W_REENTER_E88CC") || g_e8f.e8w_f6c_assist;
        if (g_e8f.e8w_mode) {
            g_e8f.e8v_mode = 1;
            g_e8f.e8v_e88cc_trace = 1;
            if (!g_e8f.display_first) {
                g_e8f.display_first = env1("JJFB_DISPLAY_FIRST");
                g_e8f.bypass_c9d_gate = env1("JJFB_BYPASS_C9D_GATE");
                if (g_e8f.display_first && !g_e8f.bypass_c9d_gate) g_e8f.bypass_c9d_gate = 1;
            }
            if (!g_e8f.display_first) {
                g_e8f.display_first = 1;
                g_e8f.bypass_c9d_gate = 1;
            }
            printf("[JJFB_E8W_FIRSTFRAME] enabled=1 f6c_assist=%d reenter=%d "
                   "note=F70_F74_embedded_struct NOT_PRODUCT_SUCCESS evidence=HYPOTHESIS\n",
                   g_e8f.e8w_f6c_assist, g_e8f.e8w_reenter_e88cc);
            fflush(stdout);
        }
        /* E8X: 2F2854→2EA188→2F449C draw path + optional dim/F74 descriptor assist. */
        g_e8f.e8x_mode = env1("JJFB_E8X_MODE");
        g_e8f.e8x_f74_desc_assist = env1("JJFB_FAST_F74_DESCRIPTOR_ASSIST");
        g_e8f.e8x_call_2f99d0 = env1("JJFB_E8X_CALL_2F99D0");
        g_e8f.e8x_insn_max = 3000u;
        {
            const char *im = getenv("JJFB_E8X_INSN_LIMIT");
            if (im && im[0]) {
                char *end = NULL;
                unsigned long v = strtoul(im, &end, 0);
                if (end != im && v > 0 && v <= 20000u) g_e8f.e8x_insn_max = (uint32_t)v;
            }
        }
        if (g_e8f.e8x_mode) {
            g_e8f.e8w_mode = 1;
            g_e8f.e8v_mode = 1;
            g_e8f.e8v_e88cc_trace = 1;
            if (!g_e8f.display_first) {
                g_e8f.display_first = 1;
                g_e8f.bypass_c9d_gate = 1;
            }
            if (!g_e8f.e8w_f6c_assist && !g_e8f.e8x_f74_desc_assist)
                g_e8f.e8w_f6c_assist = env1("JJFB_FAST_F6C_OBJECT_ASSIST");
            if (g_e8f.e8x_f74_desc_assist && !g_e8f.e8w_f6c_assist)
                g_e8f.e8w_f6c_assist = 1; /* gate still needs F74 */
            printf("[JJFB_E8X_FIRSTFRAME] enabled=1 f74_desc=%d call_2f99d0=%d f6c_assist=%d "
                   "insn_max=%u note=2F2854_to_2F449C NOT_PRODUCT_SUCCESS evidence=HYPOTHESIS\n",
                   g_e8f.e8x_f74_desc_assist, g_e8f.e8x_call_2f99d0, g_e8f.e8w_f6c_assist,
                   g_e8f.e8x_insn_max);
            fflush(stdout);
        }
        /* E8Y: survive 0x2D92E4 BMP resolve → A64 table → 0x310BBC. */
        g_e8f.e8y_mode = env1("JJFB_E8Y_MODE");
        g_e8f.e8y_a64_assist = env1("JJFB_FAST_A64_RESOURCE_ASSIST");
        g_e8f.e8y_insn_max = 5000u;
        {
            const char *im = getenv("JJFB_E8Y_INSN_LIMIT");
            if (im && im[0]) {
                char *end = NULL;
                unsigned long v = strtoul(im, &end, 0);
                if (end != im && v > 0 && v <= 20000u) g_e8f.e8y_insn_max = (uint32_t)v;
            }
        }
        if (g_e8f.e8y_mode) {
            g_e8f.e8x_mode = 1;
            g_e8f.e8w_mode = 1;
            g_e8f.e8v_mode = 1;
            g_e8f.e8v_e88cc_trace = 1;
            if (!g_e8f.display_first) {
                g_e8f.display_first = 1;
                g_e8f.bypass_c9d_gate = 1;
            }
            /* E8X Case C baseline dims + F74 gate. */
            if (!g_e8f.e8x_f74_desc_assist)
                g_e8f.e8x_f74_desc_assist = 1;
            if (!g_e8f.e8w_f6c_assist)
                g_e8f.e8w_f6c_assist = 1;
            g_e8f.e8w_reenter_e88cc = 1;
            printf("[JJFB_E8Y_FIRSTFRAME] enabled=1 a64_assist=%d insn_max=%u "
                   "note=2D92E4_to_A64_to_310BBC NOT_PRODUCT_SUCCESS evidence=HYPOTHESIS\n",
                   g_e8f.e8y_a64_assist, g_e8f.e8y_insn_max);
            fflush(stdout);
        }
        /* E8Z: real BMP pixels into handle+4 → mr_drawBitmap bmp!=0. */
        g_e8f.e8z_mode = env1("JJFB_E8Z_MODE");
        g_e8f.e8z_real_bmp = env1("JJFB_FAST_REAL_BMP_HANDLE");
        g_e8f.e8z_member_fastpath = env1("JJFB_DISPLAY_FIRST_MEMBER_FASTPATH");
        if (g_e8f.e8z_mode || g_e8f.e8z_real_bmp || g_e8f.e8z_member_fastpath) {
            g_e8f.e8z_mode = 1;
            g_e8f.e8y_mode = 1;
            g_e8f.e8x_mode = 1;
            g_e8f.e8w_mode = 1;
            g_e8f.e8v_mode = 1;
            g_e8f.e8v_e88cc_trace = 1;
            if (!g_e8f.display_first) {
                g_e8f.display_first = 1;
                g_e8f.bypass_c9d_gate = 1;
            }
            if (!g_e8f.e8x_f74_desc_assist)
                g_e8f.e8x_f74_desc_assist = 1;
            if (!g_e8f.e8w_f6c_assist)
                g_e8f.e8w_f6c_assist = 1;
            g_e8f.e8w_reenter_e88cc = 1;
            if (g_e8f.e8z_real_bmp && !g_e8f.e8y_a64_assist)
                g_e8f.e8y_a64_assist = 1;
            printf("[JJFB_E8Z_FIRSTPIXEL] enabled=1 real_bmp=%d member_fastpath=%d "
                   "note=real_mrp_pixels_to_mr_drawBitmap NOT_PRODUCT_SUCCESS "
                   "evidence=HYPOTHESIS\n",
                   g_e8f.e8z_real_bmp, g_e8f.e8z_member_fastpath);
            fflush(stdout);
        }
        /* E9A: stabilize first frame; prefer real 0x304BF0 member bridge. */
        g_e8f.e9a_mode = env1("JJFB_E9A_MODE");
        g_e8f.e9a_member_bridge = env1("JJFB_REAL_MRP_MEMBER_BRIDGE") ||
                                  env1("JJFB_REAL_MRP_MEMBER_BRIDGE_ALL");
        /* E9D: natural name match; strcmp shim default-on when E9D_MODE=1. */
        g_e8f.e9d_mode = env1("JJFB_E9D_MODE");
        g_e8f.e9d_strcmp_shim = env1("JJFB_E9D_STRCMP_SHIM") || g_e8f.e9d_mode;
        g_e8f.e9d_bridge_fallback = env1("JJFB_E9D_BRIDGE_FALLBACK");
        g_e8f.e9e_mode = env1("JJFB_E9E_MODE");
        g_e8f.e9e_postmatch_shims =
            env1("JJFB_E9E_POSTMATCH_SHIMS") || g_e8f.e9e_mode;
        if (g_e8f.e9e_mode) {
            g_e8f.e9d_mode = 1;
            g_e8f.e9d_strcmp_shim = 1;
            printf("[JJFB_E9E_POSTMATCH] enabled=1 shims=%d "
                   "note=natural_match_then_host_member_bytes NOT_PRODUCT "
                   "evidence=HYPOTHESIS\n",
                   g_e8f.e9e_postmatch_shims);
            fflush(stdout);
        }
        if (g_e8f.e9d_mode) {
            g_e8f.e9a_mode = 1;
            /* Natural path: do not force bridge unless fallback mode. */
            if (!g_e8f.e9d_bridge_fallback)
                g_e8f.e9a_member_bridge = 0;
            g_e8f.e8z_member_fastpath = 0;
            /* Must not pre-seed A64/real-bmp handles — that skips 0x304BF0 entirely. */
            if (!g_e8f.e9d_bridge_fallback) {
                g_e8f.e8z_real_bmp = 0;
                g_e8f.e8y_a64_assist = 0;
            }
            printf("[JJFB_E9D_NATURAL] enabled=1 strcmp_shim=%d bridge_fallback=%d "
                   "real_bmp=%d a64_assist=%d "
                   "note=fix_304F92_DSM_strcmp_stub NOT_PRODUCT evidence=HYPOTHESIS\n",
                   g_e8f.e9d_strcmp_shim, g_e8f.e9d_bridge_fallback, g_e8f.e8z_real_bmp,
                   g_e8f.e8y_a64_assist);
            fflush(stdout);
        }
        if (g_e8f.e9a_mode || g_e8f.e9a_member_bridge || g_e8f.e9d_mode) {
            g_e8f.e9a_mode = 1;
            g_e8f.e8z_mode = 1;
            g_e8f.e8y_mode = 1;
            g_e8f.e8x_mode = 1;
            g_e8f.e8w_mode = 1;
            g_e8f.e8v_mode = 1;
            g_e8f.e8v_e88cc_trace = 1;
            if (!g_e8f.display_first) {
                g_e8f.display_first = 1;
                g_e8f.bypass_c9d_gate = 1;
            }
            if (!g_e8f.e8x_f74_desc_assist)
                g_e8f.e8x_f74_desc_assist = 1;
            if (!g_e8f.e8w_f6c_assist)
                g_e8f.e8w_f6c_assist = 1;
            g_e8f.e8w_reenter_e88cc = 1;
            printf("[JJFB_E9A_FIRSTFRAME] enabled=1 member_bridge=%d real_bmp=%d e9d=%d "
                   "note=stabilize_and_naturalize_304BF0 NOT_PRODUCT_SUCCESS "
                   "evidence=HYPOTHESIS\n",
                   g_e8f.e9a_member_bridge, g_e8f.e8z_real_bmp, g_e8f.e9d_mode);
            fflush(stdout);
        }
        g_e8f.enabled = g_e8f.writer_bp || g_e8f.sibling_probe || g_e8f.longpath_watch ||
                        g_e8f.caller_bp || g_e8f.fault_watch || g_e8f.dispatcher_bp ||
                        g_e8f.parent_bp || g_e8f.state_watch || g_e8f.e8j_bp ||
                        g_e8f.e8j_queue_read_watch || g_e8f.svc_trap ||
                        (g_e8f.e8k_10102_case != 0) || (g_e8f.counterfactual[0] != '\0') ||
                        g_e8f.e8m_parent_trace || (g_e8f.e8m_seq[0] != '\0') ||
                        g_e8f.e8n_cf_state_set || g_e8f.fast_assist || g_e8f.display_first ||
                        g_e8f.e8v_mode || g_e8f.e8w_mode || g_e8f.e8x_mode || g_e8f.e8y_mode ||
                        g_e8f.e8z_mode || g_e8f.e9a_mode || g_e8f.e9d_mode || g_e8f.e9e_mode;
        g_e8f.known = 1;
    }
    return g_e8f.enabled;
}

void robotol_flag_writer_trace_reset(void) {
#ifdef GWY_HAVE_UNICORN
    int i;
    if (g_e8f.uc && g_e8f.armed) {
        for (i = 0; i < g_e8f.nbps; i++) {
            if (g_e8f.bps[i].used && g_e8f.bps[i].hook) {
                uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.bps[i].hook);
                g_e8f.bps[i].hook = 0;
            }
        }
        if (g_e8f.svc_code_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.svc_code_hook);
            g_e8f.svc_code_hook = 0;
        }
        if (g_e8f.svc_intr_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.svc_intr_hook);
            g_e8f.svc_intr_hook = 0;
        }
        if (g_e8f.state_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.state_hook);
            g_e8f.state_hook = 0;
        }
        if (g_e8f.e8j_qread_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.e8j_qread_hook);
            g_e8f.e8j_qread_hook = 0;
        }
        if (g_e8f.e8m_parent_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.e8m_parent_hook);
            g_e8f.e8m_parent_hook = 0;
        }
        if (g_e8f.e8v_e88cc_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.e8v_e88cc_hook);
            g_e8f.e8v_e88cc_hook = 0;
        }
        if (g_e8f.e8x_wrap_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.e8x_wrap_hook);
            g_e8f.e8x_wrap_hook = 0;
        }
        if (g_e8f.e8x_worker_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.e8x_worker_hook);
            g_e8f.e8x_worker_hook = 0;
        }
        if (g_e8f.e8x_2f449c_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.e8x_2f449c_hook);
            g_e8f.e8x_2f449c_hook = 0;
        }
        if (g_e8f.e8y_2d92e4_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.e8y_2d92e4_hook);
            g_e8f.e8y_2d92e4_hook = 0;
        }
    }
#endif
    memset(&g_e8f, 0, sizeof(g_e8f));
}

void robotol_flag_writer_trace_bind_uc(void *uc) { g_e8f.uc = uc; }

void robotol_flag_writer_trace_set_tick(uint32_t tick) { g_e8f.tick = tick; }

static int find_robotol_code(uint32_t *base_out, uint32_t *size_out) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m;
    if (!reg) return 0;
    m = module_registry_find(reg, "robotol.ext");
    if (!m || !m->map.guest_code_base || !m->map.guest_code_size) return 0;
    *base_out = m->map.guest_code_base;
    *size_out = m->map.guest_code_size;
    return 1;
}

static int find_robotol_r9(void *uc, uint32_t *r9_out) {
    uint32_t r9 = 0, er = 0, ersz = 0;
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m;
    if (!uc) return 0;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9);
    if (reg) {
        m = module_registry_find(reg, "robotol.ext");
        if (m && m->data.start_of_er_rw) {
            er = m->data.start_of_er_rw;
            ersz = m->data.er_rw_size ? m->data.er_rw_size : 0x2000u;
            if (r9 >= er && r9 < er + ersz) {
                *r9_out = r9;
                return 1;
            }
            *r9_out = er;
            return 1;
        }
    }
    if (r9) {
        *r9_out = r9;
        return 1;
    }
    return 0;
}

#ifdef GWY_HAVE_UNICORN
static void e8v_dump_e88cc_context(uc_engine *uc, uint32_t r9, uint32_t lr, const char *why);
static void on_e8v_e88cc_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data);
static void e8w_install_f6c_assist(void *uc, uint32_t r9);
static void e8x_dump_draw_dims(uc_engine *uc, uint32_t r9, const char *why);
static void e8x_install_f74_desc_assist(void *uc, uint32_t r9);
static void e8x_call_2f99d0(void *uc, uint32_t r9);
static void on_e8x_draw_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data);
static void e8y_install_a64_assist(void *uc, uint32_t r9);
static void on_e8y_2d92e4_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data);
static void e8y_dump_name(uc_engine *uc, uint32_t name_va);
static int e9a_try_member_bridge(uc_engine *uc, uint32_t name_va, uint32_t handle_va,
                                 uint32_t out_pixels_va, uint32_t lr);
static void e9d_open_csvs(void);
static int e9d_try_strcmp_shim(uc_engine *uc, uint32_t r0, uint32_t r1);
static int e9d_try_memcpy_shim(uc_engine *uc, uint32_t pc, uint32_t r0, uint32_t r1,
                               uint32_t r2);
static int e9e_try_postmatch_complete(uc_engine *uc, const char *name);

static void dump_hex_line(const char *tag, uint32_t addr, const uint8_t *buf, int n) {
    int i;
    printf("[%s] addr=0x%X bytes=", tag, addr);
    for (i = 0; i < n; i++)
        printf("%02X", buf[i]);
    printf(" evidence=OBSERVED\n");
}

static void dump_dispatcher_context(uc_engine *uc, const char *tag, uint32_t pc) {
    uint32_t r[13], sp = 0, lr = 0, cpsr = 0, r9 = 0;
    uint32_t c6c = 0, eec = 0, d11d0 = 0, d8d0 = 0, eec7c = 0, dec30 = 0;
    uint8_t c44 = 0, c9d = 0, cf5 = 0;
    int i;
    static const int regs[] = {
        UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,  UC_ARM_REG_R3, UC_ARM_REG_R4,
        UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,  UC_ARM_REG_R8, UC_ARM_REG_R9,
        UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12};
    for (i = 0; i < 13; i++)
        uc_reg_read(uc, regs[i], &r[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    r9 = r[9];
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xC6Cu, &c6c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xEECu, &eec);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x11D0u, &d11d0);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + (0x800u + 0xD0u), &d8d0);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xEECu + 0x7Cu, &eec7c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xDECu + 0x30u, &dec30);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC44u, &c44, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC9Du, &c9d, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCF5u, &cf5, 1);
    printf("[JJFB_E8H_DISPATCHER_HIT] tag=%s pc=0x%X tick=%u "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X "
           "r8=0x%X r9=0x%X r10=0x%X r11=0x%X r12=0x%X sp=0x%X lr=0x%X cpsr=0x%X T=%u "
           "R9_state=0x%X R9_C6C=0x%X R9_EEC=0x%X R9_11D0=0x%X EEC7C=0x%X DEC30=0x%X "
           "C44=0x%X C9D=0x%X CF5=0x%X cf=%s evidence=OBSERVED\n",
           tag, pc, g_e8f.tick, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9],
           r[10], r[11], r[12], sp, lr, cpsr, (cpsr >> 5) & 1u, d8d0, c6c, eec, d11d0, eec7c,
           dec30, c44, c9d, cf5, g_e8f.counterfactual[0] ? g_e8f.counterfactual : "NONE");
    fflush(stdout);
}

static void dump_svc_ab_context(uc_engine *uc, const char *via, uint32_t intno) {
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, sp = 0, lr = 0, pc = 0, cpsr = 0, r9 = 0;
    uint8_t blk[0x20];
    uint8_t spb = 0;
    const char *path;
    memset(blk, 0, sizeof(blk));
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    if (r1)
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r1, blk, (int)sizeof(blk));
    (void)guest_memory_uc_peek((struct uc_struct *)uc, sp, &spb, 1);
    if (g_e8f.fast_assist)
        path = "FAST_ASSIST";
    else
        path = g_e8f.counterfactual[0] ? "COUNTERFACTUAL" : "PRODUCT";
    g_e8f.svc_hits++;
    printf("[JJFB_E8H_SVC_AB] via=%s tick=%u path=%s cf=%s intno=0x%X "
           "pc=0x%X lr=0x%X cpsr=0x%X T=%u r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X sp=0x%X "
           "sp_byte=0x%02X note=observe_only_no_fake_return evidence=OBSERVED\n",
           via, g_e8f.tick, path, g_e8f.counterfactual[0] ? g_e8f.counterfactual : "NONE", intno,
           pc, lr, cpsr, (cpsr >> 5) & 1u, r0, r1, r2, r3, r9, sp, spb);
    if (g_e8f.fast_assist) {
        printf("[JJFB_FAST_SVC_AB] via=%s mode=%c r0=0x%X r1=0x%X lr=0x%X pc=0x%X "
               "sp_byte=0x%02X note=FAST_ASSIST_not_product evidence=HYPOTHESIS\n",
               via, g_e8f.fast_svc_mode ? g_e8f.fast_svc_mode : 'o', r0, r1, lr, pc, spb);
    }
    dump_hex_line("JJFB_E8H_SVC_AB_ARG", r1, blk, (int)sizeof(blk));
    printf("[JJFB_E8H_SVC_AB_CLASS] path=%s class=%s "
           "hypothesis=r0_service_sel_r1_argblock_sp_byte_original_request "
           "evidence=TARGET_OBSERVED+HYPOTHESIS\n",
           path,
           g_e8f.fast_assist
               ? "SVC_AB_FAST_ASSIST"
               : (g_e8f.counterfactual[0] ? "SVC_AB_POST_GATE_CANDIDATE" : "SVC_AB_PRODUCT_CANDIDATE"));
    fflush(stdout);
}

/* FAST_ASSIST only: skip unimplemented SVC #0xAB and return to LR. */
static void fast_svc_ab_continue(uc_engine *uc) {
    uint32_t lr = 0, cpsr = 0, next = 0, r0 = 0;
    if (!g_e8f.fast_assist) return;
    if (g_e8f.fast_svc_mode != 'r' && g_e8f.fast_svc_mode != 'p') return;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    if (g_e8f.fast_svc_mode == 'r') {
        r0 = 0;
        uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    }
    next = lr & ~1u;
    if (lr & 1u)
        cpsr |= (1u << 5);
    else
        cpsr &= ~(1u << 5);
    uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr);
    uc_reg_write(uc, UC_ARM_REG_PC, &next);
    g_e8f.fast_svc_continues++;
    printf("[JJFB_FAST_SVC_AB_CONTINUE] mode=%s ret_pc=0x%X continues=%u "
           "note=FAST_ASSISTED_CONTINUE_not_product evidence=HYPOTHESIS\n",
           g_e8f.fast_svc_mode == 'r' ? "return0" : "preserve", next, g_e8f.fast_svc_continues);
    fflush(stdout);
}

static void on_e8h_svc_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)address;
    (void)size;
    (void)user_data;
    if (g_e8f.svc_trapped && g_e8f.svc_stop) return;
    dump_svc_ab_context(uc, "CODE_0x2D92AE", 0xABu);
    g_e8f.svc_trapped = 1;
    if (g_e8f.fast_assist && (g_e8f.fast_svc_mode == 'r' || g_e8f.fast_svc_mode == 'p')) {
        fast_svc_ab_continue(uc);
        /* Allow further SVC hits on this fast run. */
        g_e8f.svc_trapped = 0;
        return;
    }
    if (g_e8f.svc_stop) {
        printf("[JJFB_E8H_SVC_AB_STOP] note=observe_only_emu_stop evidence=OBSERVED\n");
        fflush(stdout);
        uc_emu_stop(uc);
    }
}

static void on_e8h_intr(uc_engine *uc, uint32_t intno, void *user_data) {
    uint32_t pc = 0;
    (void)user_data;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    /* ARM SVC immediate often delivered as intno; also accept any INTR at stub PC. */
    if (intno != 0xABu && (pc & ~1u) != 0x2D92AEu && (pc & ~1u) != 0x2D92B0u)
        return;
    if (g_e8f.svc_trapped && g_e8f.svc_stop) return;
    dump_svc_ab_context(uc, "HOOK_INTR", intno);
    g_e8f.svc_trapped = 1;
    if (g_e8f.fast_assist && (g_e8f.fast_svc_mode == 'r' || g_e8f.fast_svc_mode == 'p')) {
        fast_svc_ab_continue(uc);
        g_e8f.svc_trapped = 0;
        return;
    }
    if (g_e8f.svc_stop) {
        printf("[JJFB_E8H_SVC_AB_STOP] note=observe_only_emu_stop evidence=OBSERVED\n");
        fflush(stdout);
        uc_emu_stop(uc);
    }
}

static void on_e8f_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    E8fBp *bp = (E8fBp *)user_data;
    uint32_t r0 = 0, r1 = 0, r9 = 0, lr = 0;
    (void)address;
    (void)size;
    if (!bp || !bp->used) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    bp->hit_n++;
    if (bp->hit_n == 1) bp->first_tick = g_e8f.tick;
    bp->last_r0 = r0;
    bp->last_r1 = r1;
    bp->last_r9 = r9;

    /* E8U-DisplayFirst: runtime-only gate branch assist (never poke C9D/CF5 bytes). */
    if (g_e8f.display_first &&
        (bp->pc == 0x3066B8u || bp->pc == 0x3066C6u || bp->pc == 0x3066DAu ||
         bp->pc == 0x306740u || bp->pc == 0x2E88CCu)) {
        uint32_t r2 = 0, r3 = 0, cpsr = 0, pc_now = 0, state = 0;
        uint8_t c44 = 0, c9d = 0, cf5 = 0, cd1 = 0;
        uc_reg_read(uc, UC_ARM_REG_R2, &r2);
        uc_reg_read(uc, UC_ARM_REG_R3, &r3);
        uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
        uc_reg_read(uc, UC_ARM_REG_PC, &pc_now);
        if (r9) {
            (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC44u, &c44, 1);
            (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC9Du, &c9d, 1);
            (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCF5u, &cf5, 1);
            (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCD1u, &cd1, 1);
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + E8I_STATE_OFF, &state);
        }
        /*
         * When C44==1 idle does BL 0x2E87B4 @ 0x3066B8 before C9D. That helper hits
         * unimplemented SVC #0xAB and faults under return0. DISPLAY_FIRST skips the
         * helper so the already-proven C9D gate can be assisted next.
         */
        if (bp->pc == 0x3066B8u && g_e8f.bypass_c9d_gate && c44 == 1u) {
            uint32_t new_pc = 0x3066BDu; /* thumb: fall into C9D check */
            printf("[JJFB_DISPLAY_FIRST_BRANCH_ASSIST] gate=C44_HELPER_SKIP "
                   "pc_before=0x3066B8 pc_after=0x3066BC cpsr=0x%X r0=0x%X r1=0x%X "
                   "C44=0x%X C9D=0x%X CF5=0x%X state=0x%X tick=%u "
                   "note=skip_2E87B4_SVC_path NOT_PRODUCT_SUCCESS evidence=HYPOTHESIS\n",
                   cpsr, r0, r1, c44, c9d, cf5, state, g_e8f.tick);
            fflush(stdout);
            uc_reg_write(uc, UC_ARM_REG_PC, &new_pc);
            return;
        }
        if (bp->pc == 0x3066C6u && g_e8f.bypass_c9d_gate && c44 == 1u && c9d != 1u) {
            /* Idle C9D BNE fail @ 0x3066C6 → force fall-through @ 0x3066C8. */
            uint32_t new_pc = 0x3066C9u; /* thumb continue */
            g_e8f.c9d_assist_n++;
            printf("[JJFB_DISPLAY_FIRST_BRANCH_ASSIST] gate=C9D pc_before=0x%X pc_after=0x3066C8 "
                   "cpsr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X C44=0x%X C9D=0x%X CF5=0x%X "
                   "CD1=0x%X state=0x%X tick=%u hit=%u note=NOT_PRODUCT_SUCCESS "
                   "evidence=HYPOTHESIS\n",
                   bp->pc, cpsr, r0, r1, r2, r3, c44, c9d, cf5, cd1, state, g_e8f.tick,
                   g_e8f.c9d_assist_n);
            fflush(stdout);
            uc_reg_write(uc, UC_ARM_REG_PC, &new_pc);
            return;
        }
        if (bp->pc == 0x3066DAu && g_e8f.bypass_cf5_gate && c44 == 1u && cf5 != 1u &&
            g_e8f.c9d_assist_n > 0u) {
            /* After C9D assist: optional CF5 short-circuit to success @ 0x306740. */
            uint32_t new_pc = 0x306741u;
            g_e8f.cf5_assist_n++;
            printf("[JJFB_DISPLAY_FIRST_BRANCH_ASSIST] gate=CF5 pc_before=0x%X pc_after=0x306740 "
                   "cpsr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X C44=0x%X C9D=0x%X CF5=0x%X "
                   "state=0x%X tick=%u hit=%u note=NOT_PRODUCT_SUCCESS evidence=HYPOTHESIS\n",
                   bp->pc, cpsr, r0, r1, r2, r3, c44, c9d, cf5, state, g_e8f.tick,
                   g_e8f.cf5_assist_n);
            fflush(stdout);
            uc_reg_write(uc, UC_ARM_REG_PC, &new_pc);
            return;
        }
        if (bp->pc == 0x306740u || bp->pc == 0x2E88CCu) {
            g_e8f.idle_success_n++;
            printf("[JJFB_E8U_IDLE_SUCCESS] pc=0x%X lr=0x%X C44=0x%X C9D=0x%X CF5=0x%X "
                   "CD1=0x%X state=0x%X c9d_assist=%u cf5_assist=%u tick=%u "
                   "note=real_idle_exit_path evidence=OBSERVED\n",
                   bp->pc, lr, c44, c9d, cf5, cd1, state, g_e8f.c9d_assist_n,
                   g_e8f.cf5_assist_n, g_e8f.tick);
            fflush(stdout);
        }
    }

    /* E8V: classify entry / early-exit / draw candidates inside 0x2E88CC path. */
    if (g_e8f.e8v_mode || g_e8f.e8v_e88cc_trace) {
        if (bp->pc == 0x2E88CCu) {
            g_e8f.e8v_e88cc_entries++;
            e8v_dump_e88cc_context(uc, r9, lr, "entry");
        }
        if (bp->pc == 0x2E8914u || bp->pc == 0x2E88E6u || bp->pc == 0x2E898Cu) {
            const char *why;
            /* Peek D14 first — 0x2E88E6 is always reached; only D14>0 is early-out. */
            e8v_dump_e88cc_context(uc, r9, lr, "gate_probe");
            if (bp->pc == 0x2E8914u) {
                why = "exit_null_F6C_obj";
            } else if (bp->pc == 0x2E88E6u) {
                why = (g_e8f.e8v_d14_s16 > 0) ? "exit_D14_gt0" : "pass_D14_gate";
            } else {
                why = "epilogue";
            }
            /* E8W Case C: on F70/F74 early-out, install structural F74 and retry gate. */
            if (bp->pc == 0x2E8914u && g_e8f.e8w_f6c_assist && !g_e8f.e8w_f6c_assist_done && r9) {
                uint32_t retry_pc = 0x2E8903u; /* thumb: reload r5=R9+F6C then re-check F74 */
                e8w_install_f6c_assist(uc, r9);
                /* E8X: dims must land before first 0x2F2854 (same fire as gate retry). */
                if (g_e8f.e8x_mode && g_e8f.e8x_f74_desc_assist && !g_e8f.e8x_f74_desc_assist_done)
                    e8x_install_f74_desc_assist(uc, r9);
                if (g_e8f.e8y_mode && g_e8f.e8y_a64_assist && !g_e8f.e8y_a64_assist_done)
                    e8y_install_a64_assist(uc, r9);
                printf("[JJFB_E8W_GATE_RETRY] from=0x2E8914 to=0x2E8902 F74=0x%X "
                       "note=FAST_OBJECT_ASSIST_retry_gate evidence=HYPOTHESIS\n",
                       g_e8f.e8w_f74_assist);
                fflush(stdout);
                uc_reg_write(uc, UC_ARM_REG_PC, &retry_pc);
                return;
            }
            if (bp->pc == 0x2E8914u || (bp->pc == 0x2E88E6u && g_e8f.e8v_d14_s16 > 0) ||
                bp->pc == 0x2E898Cu) {
                g_e8f.e8v_early_exit_n++;
                printf("[JJFB_E8V_E88CC_EARLY_EXIT] pc=0x%X why=%s hit=%u tick=%u F6C=0x%X "
                       "p4=0x%X p8=0x%X D14_s16=%d evidence=OBSERVED\n",
                       bp->pc, why, g_e8f.e8v_early_exit_n, g_e8f.tick, g_e8f.e8v_f6c_base,
                       g_e8f.e8v_f6c_p4, g_e8f.e8v_f6c_p8, (int)g_e8f.e8v_d14_s16);
                fflush(stdout);
            } else {
                printf("[JJFB_E8V_E88CC_GATE] pc=0x%X why=%s tick=%u D14_s16=%d "
                       "note=fallthrough evidence=OBSERVED\n",
                       bp->pc, why, g_e8f.tick, (int)g_e8f.e8v_d14_s16);
                fflush(stdout);
            }
        }
        if (bp->pc == 0x2F2854u || bp->pc == 0x305BFCu || bp->pc == 0x2EA058u) {
            uint32_t r2 = 0, r3 = 0, sp = 0, cpsr = 0, a830 = 0;
            uc_reg_read(uc, UC_ARM_REG_R2, &r2);
            uc_reg_read(uc, UC_ARM_REG_R3, &r3);
            uc_reg_read(uc, UC_ARM_REG_SP, &sp);
            uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
            g_e8f.e8v_draw_cand_n++;
            printf("[JJFB_FIRST_REAL_DRAW_CANDIDATE] pc=0x%X lr=0x%X r0=0x%X r1=0x%X "
                   "r2=0x%X r3=0x%X tick=%u hit=%u note=drawish_bl_target evidence=OBSERVED\n",
                   bp->pc, lr, r0, r1, r2, r3, g_e8f.tick, g_e8f.e8v_draw_cand_n);
            if (bp->pc == 0x2F2854u) {
                g_e8f.e8x_2f2854_n++;
                printf("[JJFB_E8W_DRAW_SITE] site=0x2F2854 tick=%u evidence=OBSERVED\n",
                       g_e8f.tick);
                if (g_e8f.e8x_mode) {
                    uint32_t sa0 = 0, sa1 = 0, sa2 = 0, sa3 = 0;
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp, &sa0);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 4u, &sa1);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 8u, &sa2);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 12u, &sa3);
                    if (r9)
                        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x830u, &a830);
                    printf("[JJFB_E8X_2F2854_ENTRY] pc=0x2F2854 lr=0x%X cpsr=0x%X T=%u "
                           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X sp=0x%X "
                           "sp0=0x%X sp4=0x%X sp8=0x%X spC=0x%X R9_830=0x%X hit=%u "
                           "note=r0_always0_r2_from_2F9970_R9_830 evidence=OBSERVED\n",
                           lr, cpsr, (cpsr >> 5) & 1u, r0, r1, r2, r3, sp, sa0, sa1, sa2, sa3,
                           a830, g_e8f.e8x_2f2854_n);
                    e8x_dump_draw_dims(uc, r9, "2f2854_entry");
                    if (!r1 && !r2)
                        printf("[JJFB_E8X_CLASS] class=2F2854_ZERO_LAYOUT_ARGS "
                               "note=r1_layout_r2_R9_830_both_zero evidence=OBSERVED\n");
                    else if (!r2)
                        printf("[JJFB_E8X_CLASS] class=2F2854_REQUIRES_R9_830 "
                               "r1=0x%X note=width_helper_zero evidence=OBSERVED\n",
                               r1);
                    else
                        printf("[JJFB_E8X_CLASS] class=2F2854_NONZERO_ARGS "
                               "r1=0x%X r2=0x%X r3=0x%X evidence=OBSERVED\n",
                               r1, r2, r3);
                    fflush(stdout);
                }
            } else if (bp->pc == 0x305BFCu)
                printf("[JJFB_E8W_DRAW_SITE] site=0x305BFC tick=%u evidence=OBSERVED\n",
                       g_e8f.tick);
            else
                printf("[JJFB_E8W_DRAW_SITE] site=0x2EA058 tick=%u evidence=OBSERVED\n",
                       g_e8f.tick);
            fflush(stdout);
        } else if (bp->pc == 0x2EA188u || bp->pc == 0x2F449Cu || bp->pc == 0x310BBCu) {
            uint32_t r2 = 0, r3 = 0;
            uc_reg_read(uc, UC_ARM_REG_R2, &r2);
            uc_reg_read(uc, UC_ARM_REG_R3, &r3);
            if (bp->pc == 0x2EA188u) g_e8f.e8x_2ea188_n++;
            else if (bp->pc == 0x2F449Cu) g_e8f.e8x_2f449c_n++;
            else {
                g_e8f.e8x_310bbc_n++;
                g_e8f.e8y_310bbc_n++;
            }
            printf("[JJFB_E8X_DRAW_PATH] pc=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                   "n2854=%u n188=%u n449c=%u n310=%u tick=%u evidence=OBSERVED\n",
                   bp->pc, lr, r0, r1, r2, r3, g_e8f.e8x_2f2854_n, g_e8f.e8x_2ea188_n,
                   g_e8f.e8x_2f449c_n, g_e8f.e8x_310bbc_n, g_e8f.tick);
            if (bp->pc == 0x2F449Cu || bp->pc == 0x310BBCu) {
                printf("[JJFB_FIRST_REAL_DRAW_CANDIDATE] pc=0x%X lr=0x%X r0=0x%X r1=0x%X "
                       "r2=0x%X r3=0x%X tick=%u note=deeper_draw_path evidence=OBSERVED\n",
                       bp->pc, lr, r0, r1, r2, r3, g_e8f.tick);
            }
            if (bp->pc == 0x310BBCu)
                printf("[JJFB_E8Y_310BBC] hit=%u tick=%u note=post_resource_draw_site "
                       "evidence=OBSERVED\n",
                       g_e8f.e8y_310bbc_n, g_e8f.tick);
            /* E9E: index-scan orphans can leave R9 on DSM; mr_drawBitmap table needs robotol. */
            if (bp->pc == 0x310BBCu && g_e8f.e9e_postmatch_done && g_e8f.e9e_304bf0_r9 &&
                r9 != g_e8f.e9e_304bf0_r9) {
                uc_reg_write(uc, UC_ARM_REG_R9, &g_e8f.e9e_304bf0_r9);
                printf("[JJFB_E9E_POSTMATCH_SHIM] role=draw_r9_restore pc=0x310BBC "
                       "old_r9=0x%X new_r9=0x%X note=mr_drawBitmap_table "
                       "evidence=OBSERVED\n",
                       r9, g_e8f.e9e_304bf0_r9);
                fflush(stdout);
            }
            fflush(stdout);
        } else if (g_e8f.e8y_mode &&
                   (bp->pc == 0x2D92E4u || bp->pc == 0x2F44C8u || bp->pc == 0x2F44CEu ||
                    bp->pc == 0x2F44E4u || bp->pc == 0x2F44EEu || bp->pc == 0x304BF0u ||
                    bp->pc == 0x2FD868u || bp->pc == 0x2F8528u || bp->pc == 0x2F6BD4u ||
                    bp->pc == 0x304F26u || bp->pc == 0x304F7Au || bp->pc == 0x304F92u)) {
            uint32_t r2 = 0, r3 = 0, a64 = 0, a68 = 0, a6c = 0, fp1450 = 0, fp144c = 0;
            uc_reg_read(uc, UC_ARM_REG_R2, &r2);
            uc_reg_read(uc, UC_ARM_REG_R3, &r3);
            if (r9) {
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA64u, &a64);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA68u, &a68);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA6Cu, &a6c);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x1450u, &fp1450);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x144Cu, &fp144c);
            }
            if (bp->pc == 0x304F92u && g_e8f.e9d_mode) {
                if (e9d_try_strcmp_shim(uc, r0, r1))
                    return;
            } else if ((bp->pc == 0x304F26u || bp->pc == 0x304F7Au) && g_e8f.e9d_mode) {
                if (e9d_try_memcpy_shim(uc, bp->pc, r0, r1, r2))
                    return;
            } else if (bp->pc == 0x2D92E4u) {
                g_e8f.e8y_2d92e4_n++;
                g_e8f.e8y_last_name_va = r1;
                e8y_dump_name(uc, r1);
                printf("[JJFB_E8Y_2D92E4_ENTRY] hit=%u lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "name=\"%s\" R9_1450_strlen=0x%X R9_144C_memset=0x%X "
                       "A64=0x%X A68=0x%X A6C=0x%X tick=%u evidence=OBSERVED\n",
                       g_e8f.e8y_2d92e4_n, lr, r0, r1, r2, r3, g_e8f.e8y_last_name, fp1450,
                       fp144c, a64, a68, a6c, g_e8f.tick);
                if (!fp1450)
                    printf("[JJFB_E8Y_CLASS] class=2D92E4_NEEDS_STRLEN_FP "
                           "note=R9_1450_zero_BLX_will_fault evidence=OBSERVED\n");
                /* DISPLAY_FIRST member fastpath: return real handle, skip stalled 0x304BF0. */
                if (g_e8f.e8z_member_fastpath && !g_e8f.e8z_fastpath_done && r9 &&
                    g_e8f.e8y_last_name[0] &&
                    strstr(g_e8f.e8y_last_name, "wy_jiao1!") == g_e8f.e8y_last_name) {
                    uint32_t ret_pc = lr | 1u;
                    uint32_t handle = 0;
                    if (!g_e8f.e8y_a64_assist_done)
                        e8y_install_a64_assist(uc, r9);
                    handle = g_e8f.e8z_handle_va ? g_e8f.e8z_handle_va
                                                 : (E8Y_A64_SCRATCH);
                    if (g_e8f.e8z_real_bmp_done && handle) {
                        g_e8f.e8z_fastpath_done = 1;
                        uc_reg_write(uc, UC_ARM_REG_R0, &handle);
                        uc_reg_write(uc, UC_ARM_REG_PC, &ret_pc);
                        printf("[JJFB_DISPLAY_FIRST_MEMBER_FASTPATH] name=\"%s\" "
                               "handle=0x%X pixels=0x%X bytes=%u ret_pc=0x%X "
                               "note=real_bytes_skip_304BF0 NOT_PRODUCT evidence=OBSERVED\n",
                               g_e8f.e8y_last_name, handle, g_e8f.e8z_pixel_va,
                               g_e8f.e8z_pixel_bytes, ret_pc);
                        printf("[JJFB_E8Z_CLASS] class=REAL_BMP_MEMBER_LOADED_NEXT_GAP "
                               "evidence=OBSERVED\n");
                        fflush(stdout);
                        return;
                    }
                    printf("[JJFB_DISPLAY_FIRST_MEMBER_FASTPATH] skip=no_real_pixels "
                           "name=\"%s\" evidence=OBSERVED\n",
                           g_e8f.e8y_last_name);
                }
            } else if (bp->pc == 0x2F44C8u) {
                /* Return land after first BL 0x2D92E4 — r0 is resolved handle. */
                if (r0)
                    g_e8f.e8y_2d92e4_ret_nz++;
                else
                    g_e8f.e8y_2d92e4_ret_z++;
                printf("[JJFB_E8Y_2D92E4_RETURN] r0=0x%X name=\"%s\" A64_before_store=0x%X "
                       "nz=%u z=%u tick=%u evidence=OBSERVED\n",
                       r0, g_e8f.e8y_last_name, a64, g_e8f.e8y_2d92e4_ret_nz,
                       g_e8f.e8y_2d92e4_ret_z, g_e8f.tick);
                if (!r0)
                    printf("[JJFB_E8Y_CLASS] class=RESOURCE_INIT_2D92E4_RETURNS_WITH_A64_ZERO "
                           "name=\"%s\" evidence=OBSERVED\n",
                           g_e8f.e8y_last_name);
                else
                    printf("[JJFB_E8Y_CLASS] class=RESOURCE_INIT_2D92E4_COMPLETED_NEXT_GAP "
                           "handle=0x%X evidence=OBSERVED\n",
                           r0);
            } else if (bp->pc == 0x304BF0u) {
                printf("[JJFB_E8Z_304BF0] lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "name=\"%s\" note=mrp_member_lookup evidence=OBSERVED\n",
                       lr, r0, r1, r2, r3, g_e8f.e8y_last_name);
                if (g_e8f.e9d_mode || g_e8f.e9e_mode) {
                    e9d_open_csvs();
                    g_e8f.e9d_req_n++;
                    g_e8f.e9d_req_tick = g_e8f.tick;
                    snprintf(g_e8f.e9d_req_name, sizeof(g_e8f.e9d_req_name), "%s",
                             g_e8f.e8y_last_name);
                    /* Save ABI args + callee-saved regs for E9E post-match return. */
                    g_e8f.e9e_304bf0_lr = lr;
                    g_e8f.e9e_304bf0_name_va = r1;
                    g_e8f.e9e_304bf0_out_va = r2;
                    g_e8f.e9e_304bf0_handle_va = r3;
                    {
                        uint32_t sp0 = 0, r4e = 0, r5e = 0, r6e = 0, r7e = 0;
                        uint32_t r8e = 0, r9e = 0, r10e = 0, r11e = 0;
                        uc_reg_read(uc, UC_ARM_REG_SP, &sp0);
                        uc_reg_read(uc, UC_ARM_REG_R4, &r4e);
                        uc_reg_read(uc, UC_ARM_REG_R5, &r5e);
                        uc_reg_read(uc, UC_ARM_REG_R6, &r6e);
                        uc_reg_read(uc, UC_ARM_REG_R7, &r7e);
                        uc_reg_read(uc, UC_ARM_REG_R8, &r8e);
                        uc_reg_read(uc, UC_ARM_REG_R9, &r9e);
                        uc_reg_read(uc, UC_ARM_REG_R10, &r10e);
                        uc_reg_read(uc, UC_ARM_REG_R11, &r11e);
                        g_e8f.e9e_304bf0_sp = sp0;
                        g_e8f.e9e_304bf0_r4 = r4e;
                        g_e8f.e9e_304bf0_r5 = r5e;
                        g_e8f.e9e_304bf0_r6 = r6e;
                        g_e8f.e9e_304bf0_r7 = r7e;
                        g_e8f.e9e_304bf0_r8 = r8e;
                        g_e8f.e9e_304bf0_r9 = r9e;
                        g_e8f.e9e_304bf0_r10 = r10e;
                        g_e8f.e9e_304bf0_r11 = r11e;
                    }
                    if (g_e8f.e9d_req_csv) {
                        fprintf(g_e8f.e9d_req_csv, "%u,%u,0x%X,0x%X,\"%s\",0,0,enter_304BF0\n",
                                g_e8f.e9d_req_n, g_e8f.tick, bp->pc, lr, g_e8f.e8y_last_name);
                        fflush(g_e8f.e9d_req_csv);
                    }
                    printf("[JJFB_E9D_REQUEST] n=%u name=\"%s\" lr=0x%X tick=%u "
                           "evidence=OBSERVED\n",
                           g_e8f.e9d_req_n, g_e8f.e8y_last_name, lr, g_e8f.tick);
                    fflush(stdout);
                }
                /* E9A bridge only when explicitly enabled (not E9E natural). */
                if (g_e8f.e9a_member_bridge) {
                    if (e9a_try_member_bridge(uc, r1, r3, r2, lr))
                        return;
                }
            } else if (bp->pc == 0x2F44CEu || bp->pc == 0x2F44E4u || bp->pc == 0x2F44EEu) {
                g_e8f.e8y_a64_write_n++;
                printf("[JJFB_E8Y_A64_STORE] pc=0x%X r0=0x%X A64=0x%X A68=0x%X A6C=0x%X "
                       "hit=%u tick=%u evidence=OBSERVED\n",
                       bp->pc, r0, a64, a68, a6c, g_e8f.e8y_a64_write_n, g_e8f.tick);
                /*
                 * E9A: first bridged member already stored to A64. Remaining ~15
                 * BL 0x2D92E4 loads are too slow under host tracing; take the
                 * natural A64-nonzero continue at 0x2F45A2 → 0x2F4628 draw path.
                 * Mirror the same real handle into A68/A6C (still original MRP bytes).
                 */
                if (bp->pc == 0x2F44CEu &&
                    (g_e8f.e9a_member_bridge || g_e8f.e9e_postmatch_done) &&
                    !g_e8f.e9a_post_a64_draw_skip && r9 && r0 &&
                    (g_e8f.e9a_bridge_hit_n > 0u || g_e8f.e9e_postmatch_done)) {
                    uint32_t cont = 0x2F45A2u | 1u;
                    /* Mode at [sp,#0x60] may be 10 → uses A58/A5C/A60; mirror real handle. */
                    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xA58u, r0);
                    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xA5Cu, r0);
                    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xA60u, r0);
                    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xA64u, r0);
                    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xA68u, r0);
                    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xA6Cu, r0);
                    g_e8f.e9a_post_a64_draw_skip = 1;
                    uc_reg_write(uc, UC_ARM_REG_PC, &cont);
                    printf("[JJFB_E9A_POST_A64_DRAW_SKIP] handle=0x%X "
                           "A58..A6C=0x%X cont=0x2F45A2 "
                           "note=real_bridged_handle_skip_sibling_loads "
                           "NOT_PRODUCT evidence=OBSERVED\n",
                           r0, r0);
                    fflush(stdout);
                    return;
                }
            } else {
                printf("[JJFB_E8Y_HELPER] pc=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "tick=%u evidence=OBSERVED\n",
                       bp->pc, lr, r0, r1, r2, r3, g_e8f.tick);
            }
            fflush(stdout);
        } else if (bp->pc == 0x2E8980u || bp->pc == 0x2E89A8u || bp->pc == 0x2E8A22u ||
                   bp->pc == 0x2E8A44u || bp->pc == 0x2FF908u || bp->pc == 0x305E78u ||
                   bp->pc == 0x2E993Cu) {
            uint32_t r2 = 0, r3 = 0;
            uc_reg_read(uc, UC_ARM_REG_R2, &r2);
            uc_reg_read(uc, UC_ARM_REG_R3, &r3);
            printf("[JJFB_E8V_PATH_HIT] pc=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                   "tick=%u note=scheduler_sidepath_not_platform_draw evidence=OBSERVED\n",
                   bp->pc, lr, r0, r1, r2, r3, g_e8f.tick);
            if (g_e8f.e8x_mode && (bp->pc == 0x2E8980u || bp->pc == 0x2E89A8u)) {
                uint32_t a830 = 0;
                if (r9)
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x830u, &a830);
                printf("[JJFB_E8X_CALLSITE] pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "R9_830=0x%X note=pre_BL_2F2854 evidence=OBSERVED\n",
                       bp->pc, r0, r1, r2, r3, a830);
            }
            fflush(stdout);
        }
        /* E8W: F6C/F70/F74 writer sites (observe-only). */
        if (g_e8f.e8w_mode &&
            (bp->pc == 0x2D9CFCu || bp->pc == 0x2FBD18u || bp->pc == 0x2E8920u ||
             bp->pc == 0x30AA32u || bp->pc == 0x30AA34u || bp->pc == 0x30AA42u ||
             bp->pc == 0x311AA6u || bp->pc == 0x311AB8u || bp->pc == 0x311AC4u)) {
            uint32_t f70 = 0, f74 = 0, w0 = 0;
            g_e8f.e8w_writer_hit_n++;
            if (r9) {
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF6Cu, &w0);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF70u, &f70);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF74u, &f74);
            }
            printf("[JJFB_E8W_F6C_WRITER] pc=0x%X lr=0x%X r0=0x%X r1=0x%X "
                   "F6C_w0=0x%X F70=0x%X F74=0x%X tick=%u hit=%u evidence=OBSERVED\n",
                   bp->pc, lr, r0, r1, w0, f70, f74, g_e8f.tick, g_e8f.e8w_writer_hit_n);
            fflush(stdout);
        }
    }

    /* Capture live UI object at natural BL site / UI-init entry. */
    if (bp->pc == 0x2E2F50u || bp->pc == 0x2E4788u || bp->pc == 0x2E2520u) {
        uint32_t r4 = 0, obj = 0, p4 = 0;
        uint8_t dump[0x80];
        uc_reg_read(uc, UC_ARM_REG_R4, &r4);
        obj = (bp->pc == 0x2E2F50u) ? r4 : r0;
        if (obj && !g_e8f.ui_obj_set) {
            g_e8f.ui_obj_r0 = obj;
            g_e8f.ui_obj_set = 1;
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, obj + 4u, &p4);
            printf("[JJFB_E8U_UI_OBJECT] captured=0x%X via_pc=0x%X r0=0x%X r4=0x%X "
                   "obj_plus4=0x%X note=live_capture evidence=OBSERVED\n",
                   obj, bp->pc, r0, r4, p4);
            if (guest_memory_uc_peek((struct uc_struct *)uc, obj, dump, (int)sizeof(dump))) {
                int i;
                printf("[JJFB_E8U_UI_OBJECT_DUMP] addr=0x%X bytes=", obj);
                for (i = 0; i < 64; i++) printf("%02X", dump[i]);
                printf(" evidence=OBSERVED\n");
            }
            fflush(stdout);
        }
    }

    if (bp->tag[0] == 'd') {
        dump_dispatcher_context(uc, bp->tag, bp->pc);
        return;
    }
    if (bp->tag[0] == 'p') {
        uint32_t r4 = 0, state = 0;
        g_e8f.parent_hit_n++;
        uc_reg_read(uc, UC_ARM_REG_R4, &r4);
        if (r9)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + (0x800u + 0xD0u), &state);
        printf("[JJFB_E8I_PARENT_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r4=0x%X "
               "r9=0x%X lr=0x%X state=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r4, r9, lr, state);
        /* E8Q/E8R: success-arm / C44-unlock landmarks. */
        if (bp->pc == 0x301848u || bp->pc == 0x301864u || bp->pc == 0x304558u ||
            bp->pc == 0x2FC8C0u || bp->pc == 0x2FC8CEu || bp->pc == 0x30213Eu ||
            bp->pc == 0x3046A8u || bp->pc == 0x2E4788u || bp->pc == 0x2DB948u ||
            bp->pc == 0x2DB9DCu || bp->pc == 0x30D300u || bp->pc == 0x30DDE2u ||
            (bp->pc >= 0x2E4840u && bp->pc <= 0x2E4B06u)) {
            uint32_t r2 = 0, r3 = 0, c44 = 0, eec7c = 0, dec30 = 0, b1a8 = 0;
            uint8_t c44b = 0, b1a8b = 0, c6c22 = 0;
            uc_reg_read(uc, UC_ARM_REG_R2, &r2);
            uc_reg_read(uc, UC_ARM_REG_R3, &r3);
            if (r9) {
                (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC44u, &c44b, 1);
                (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0x1A8u, &b1a8b, 1);
                (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC6Cu + 0x22u, &c6c22, 1);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xEECu + 0x7Cu, &eec7c);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xDECu + 0x30u, &dec30);
                c44 = c44b;
                b1a8 = b1a8b;
            }
            if (bp->pc == 0x301848u || bp->pc == 0x301864u || bp->pc == 0x304558u ||
                bp->pc == 0x2FC8C0u || bp->pc == 0x2FC8CEu || bp->pc == 0x30213Eu) {
                printf("[JJFB_E8Q_ARM] tag=%s pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X lr=0x%X "
                       "state=0x%X C44=0x%X R9_1A8=0x%X C6C22=0x%X EEC7C=0x%X DEC30=0x%X "
                       "note=FAST_OR_OBSERVE evidence=OBSERVED\n",
                       bp->tag, bp->pc, r0, r1, r2, r3, lr, state, c44, b1a8, c6c22, eec7c, dec30);
            }
            if (bp->pc == 0x2FC8CEu)
                printf("[JJFB_E8Q_C44_UNLOCK_SITE] pc=0x2FC8CE note=STRB_1_not_reset "
                       "evidence=TARGET_OBSERVED\n");
            printf("[JJFB_E8R_UNLOCK_PATH] tag=%s pc=0x%X lr=0x%X state=0x%X C44=0x%X "
                   "note=caller_or_writer evidence=OBSERVED\n",
                   bp->tag, bp->pc, lr, state, c44);
        }
        /* E8S/E8T: C9D/CF5 writers + UI-init landmarks. */
        if (bp->pc == 0x2F097Au || bp->pc == 0x2FB008u || bp->pc == 0x30AA42u ||
            bp->pc == 0x30AA46u || bp->pc == 0x3115BAu || bp->pc == 0x2FAFFCu ||
            bp->pc == 0x30D9EEu || bp->pc == 0x2E7DA8u || bp->pc == 0x2E7DC2u ||
            bp->pc == 0x2F7F2Cu || bp->pc == 0x2E2520u || bp->pc == 0x2E2F50u ||
            bp->pc == 0x2E3F7Cu || bp->pc == 0x2E32A2u) {
            uint8_t c9d = 0, cf5 = 0, c44b = 0;
            if (r9) {
                (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC44u, &c44b, 1);
                (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC9Du, &c9d, 1);
                (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCF5u, &cf5, 1);
            }
            printf("[JJFB_E8S_FLAG_WRITER] tag=%s pc=0x%X lr=0x%X state=0x%X C44=0x%X C9D=0x%X "
                   "CF5=0x%X evidence=OBSERVED\n",
                   bp->tag, bp->pc, lr, state, c44b, c9d, cf5);
            if (bp->pc == 0x30AA46u || bp->pc == 0x3115BAu)
                printf("[JJFB_E8T_C9D_WRITER] tag=%s pc=0x%X lr=0x%X C9D=0x%X "
                       "note=exact_C9D_strb_clear_or_reg evidence=OBSERVED\n",
                       bp->tag, bp->pc, lr, c9d);
        }
        /* E8M path landmarks inside parent. */
        if (bp->pc == 0x300182u || bp->pc == 0x300194u || bp->pc == 0x30026Eu ||
            bp->pc == 0x3004C8u || bp->pc == 0x3002BAu || bp->pc == 0x3002C0u ||
            bp->pc == 0x3001B8u) {
            g_e8f.e8m_path_hits++;
            printf("[JJFB_E8M_PARENT_PATH] pc=0x%X r0=0x%X r4=0x%X state=0x%X note=", bp->pc, r0,
                   r4, state);
            if (bp->pc == 0x300182u)
                printf("switch_first_cmp");
            else if (bp->pc == 0x300194u)
                printf("cmp_state_eq0");
            else if (bp->pc == 0x30026Eu)
                printf("state0_branch_land");
            else if (bp->pc == 0x3004C8u)
                printf("state0_arm");
            else if (bp->pc == 0x3002BAu)
                printf("gate_cmp20");
            else if (bp->pc == 0x3002C0u)
                printf("bl_300714");
            else if (bp->pc == 0x3001B8u)
                printf("epilogue_pop");
            printf(" evidence=OBSERVED\n");
        }
        if (bp->pc == 0x300158u || bp->pc == 0x300714u || bp->pc == 0x30103Cu)
            dump_dispatcher_context(uc, bp->tag, bp->pc);
        fflush(stdout);
        return;
    }
    if (bp->tag[0] == 'e') {
        g_e8f.e8j_entry_hits++;
        printf("[JJFB_E8J_CLUSTER_HIT] role=entry tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X "
               "r9=0x%X lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
        fflush(stdout);
        return;
    }
    if (bp->tag[0] == 'u') {
        g_e8f.e8j_upstream_hits++;
        printf("[JJFB_E8J_UPSTREAM_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
        fflush(stdout);
        return;
    }
    if (bp->tag[0] == 'q') {
        g_e8f.e8j_queue_hits++;
        printf("[JJFB_E8J_QUEUE_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
        fflush(stdout);
        return;
    }
    if (bp->tag[0] == 'b') {
        g_e8f.e8j_bl_hits++;
        g_e8f.parent_hit_n++;
        printf("[JJFB_E8J_BL_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X note=direct_bl_to_300158 evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
        fflush(stdout);
        return;
    }
    if (bp->pc == 0x30D28Cu) {
        g_e8f.longpath_hits++;
        printf("[JJFB_E8F_LONGPATH_HIT] pc=0x%X tick=%u r0=0x%X r1=0x%X r9=0x%X lr=0x%X "
               "evidence=OBSERVED\n",
               bp->pc, g_e8f.tick, r0, r1, r9, lr);
    } else if (bp->pc == 0x2D92B0u) {
        g_e8f.fault_hits++;
        printf("[JJFB_E8G_FAULT_HIT] pc=0x2D92B0 tick=%u r0=0x%X r1=0x%X r9=0x%X lr=0x%X "
               "note=counterfactual_second_gate_site evidence=OBSERVED\n",
               g_e8f.tick, r0, r1, r9, lr);
    } else if (bp->tag[0] == 'c') {
        printf("[JJFB_E8G_CALLER_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
    } else {
        printf("[JJFB_E8F_WRITER_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
    }
    fflush(stdout);
}
static void on_e8i_state_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                               int64_t value, void *user_data) {
    uint32_t pc = 0, lr = 0, r0 = 0, r9 = 0;
    uint32_t newv = (uint32_t)value;
    uint32_t oldv = g_e8f.state_last_val;
    (void)type;
    (void)user_data;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    if (size == 1) newv = (uint32_t)(uint8_t)value;
    else if (size == 2) newv = (uint32_t)(uint16_t)value;
    /* Prefer tracked last value as old; peek may already see new during hook. */
    oldv = g_e8f.state_last_val;
    g_e8f.state_writes++;
    g_e8f.state_last_val = newv;
    g_e8f.state_last_writer = pc;
    printf("[JJFB_E8I_STATE_WRITE] addr=0x%X off=0x%X size=%d old=0x%X new=0x%X writer_pc=0x%X "
           "lr=0x%X r0=0x%X r9=0x%X tick=%u note=observe_only evidence=OBSERVED\n",
           (uint32_t)address, E8I_STATE_OFF, size, oldv, newv, pc, lr, r0, r9, g_e8f.tick);
    printf("[JJFB_E8N_STATE_WRITE] old=0x%X new=0x%X width=%d writer_pc=0x%X lr=0x%X tick=%u "
           "ge20=%d eq38=%d evidence=OBSERVED\n",
           oldv, newv, size, pc, lr, g_e8f.tick, newv >= 20u, newv == 38u);
    if (newv == 38u) {
        printf("[JJFB_E8I_STATE_EQ38] writer_pc=0x%X tick=%u note=required_for_30103C_arm "
               "evidence=OBSERVED\n",
               pc, g_e8f.tick);
    }
    fflush(stdout);
}
static void on_e8j_queue_read(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                              int64_t value, void *user_data) {
    uint32_t pc = 0, lr = 0, r0 = 0, r9 = 0;
    uint32_t off;
    (void)type;
    (void)value;
    (void)user_data;
    if (!find_robotol_r9(uc, &r9) || !r9) return;
    if ((uint32_t)address < r9) return;
    off = (uint32_t)address - r9;
    /* Only report FE8 / B7D / 7D8 base (not every 7D8 field offset). */
    if (off != 0xFE8u && off != 0xB7Du && off != 0x7D8u) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    g_e8f.e8j_qread_hits++;
    if (g_e8f.e8j_qread_hits <= 40u) {
        printf("[JJFB_E8J_QUEUE_READ] addr=0x%X off=0x%X size=%d reader_pc=0x%X lr=0x%X "
               "r0=0x%X r9=0x%X tick=%u evidence=OBSERVED\n",
               (uint32_t)address, off, size, pc, lr, r0, r9, g_e8f.tick);
        fflush(stdout);
    }
}

static void on_e8m_parent_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t r0 = 0, r4 = 0, lr = 0;
    (void)size;
    (void)user_data;
    if (!g_e8f.e8m_parent_trace) return;
    if (g_e8f.e8m_insn_n >= 200u) return;
    /* Only while nested inside observe-only 10102 fire (tick1 diagnostic). */
    if (g_e8f.tick != 1u) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    g_e8f.e8m_insn_n++;
    if (g_e8f.e8m_insn_n <= 200u) {
        printf("[JJFB_E8M_INSN] n=%u pc=0x%X r0=0x%X r4=0x%X lr=0x%X evidence=OBSERVED\n",
               g_e8f.e8m_insn_n, (uint32_t)address, r0, r4, lr);
        fflush(stdout);
    }
}

/* E8V/E8W: dump 0x2E88CC gate fields.
 * Corrected: R9+0xF6C is EMBEDDED struct base; gate reads F70/F74 adjacent words. */
static void e8v_dump_e88cc_context(uc_engine *uc, uint32_t r9, uint32_t lr, const char *why) {
    uint32_t w0 = 0, f70 = 0, f74 = 0, a818 = 0, a81c = 0, a1a8 = 0, scroll = 0;
    uint8_t f7da = 0;
    int16_t d14 = 0;
    uint8_t d14b[2];
    if (!r9) return;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF6Cu, &w0);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF70u, &f70);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF74u, &f74);
    if (guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCECu + 0x28u, d14b, 2))
        d14 = (int16_t)(uint16_t)(d14b[0] | ((uint16_t)d14b[1] << 8));
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x818u, &a818);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x81Cu, &a81c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x1A8u, &a1a8);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x1A8u + 0x58u, &scroll);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0x7DAu, &f7da, 1);
    g_e8f.e8v_f6c_base = w0;
    g_e8f.e8v_f6c_p4 = f70;
    g_e8f.e8v_f6c_p8 = f74;
    g_e8f.e8v_d14_s16 = d14;
    printf("[JJFB_E8V_E88CC_CTX] why=%s r9=0x%X lr=0x%X F6C=0x%X F6C_0=0x%X F6C_4=0x%X "
           "F6C_8=0x%X D14_s16=%d R9_818=0x%X R9_81C=0x%X R9_1A8=0x%X R9_7DA=0x%X "
           "note=F6C_4_is_F70_F6C_8_is_F74 evidence=OBSERVED\n",
           why ? why : "?", r9, lr, w0, w0, f70, f74, (int)d14, a818, a81c, a1a8, f7da);
    printf("[JJFB_E8W_F6C_WORDS] F6C_w0=0x%X F70=0x%X F74=0x%X scroll200=0x%X "
           "note=gate_open_if_F74_or_F70_nonzero evidence=DOCUMENTED\n",
           w0, f70, f74, scroll);
    if (!f70 && !f74) {
        printf("[JJFB_E8V_E88CC_CLASS] class=REQUIRES_UI_OBJECT F6C=0x%X p4=0x%X p8=0x%X "
               "D14_s16=%d note=F70_and_F74_zero_early_exit_2E8914 evidence=OBSERVED\n",
               w0, f70, f74, (int)d14);
    } else if (d14 > 0) {
        printf("[JJFB_E8V_E88CC_CLASS] class=SKIP_BY_D14_GATE D14_s16=%d "
               "note=halfword_gt0_early_out evidence=OBSERVED\n",
               (int)d14);
    } else if (scroll == 0x3E7u) {
        printf("[JJFB_E8V_E88CC_CLASS] class=BLOCKED_BY_SCROLL_SENTINEL scroll200=0x3E7 "
               "F70=0x%X F74=0x%X note=0x2E8994_beq_exit evidence=OBSERVED\n",
               f70, f74);
    } else {
        printf("[JJFB_E8V_E88CC_CLASS] class=OBJECT_PRESENT_DRAW_PATH_OPEN F6C=0x%X "
               "p4=0x%X p8=0x%X scroll200=0x%X evidence=OBSERVED\n",
               w0, f70, f74, scroll);
    }
    fflush(stdout);
}

static void on_e8v_e88cc_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, lr = 0, pc = (uint32_t)address;
    uint16_t half = 0;
    (void)user_data;
    if (!g_e8f.e8v_e88cc_trace) return;
    if (g_e8f.e8v_insn_n >= g_e8f.e8v_insn_max) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    g_e8f.e8v_insn_n++;
    /* Compact: log first 80 always, then every 20th, plus BL / early-exit landmarks. */
    (void)guest_memory_uc_peek((struct uc_struct *)uc, pc, (uint8_t *)&half, 2);
    if (g_e8f.e8v_insn_n <= 80u || (g_e8f.e8v_insn_n % 20u) == 0u ||
        pc == 0x2E88E6u || pc == 0x2E8914u || pc == 0x2E898Cu || pc == 0x2E8980u ||
        pc == 0x2E89A8u || pc == 0x2E8A22u || pc == 0x2E8A44u ||
        ((half & 0xF800u) == 0xF000u)) {
        printf("[JJFB_E8V_INSN] n=%u pc=0x%X size=%u r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               g_e8f.e8v_insn_n, pc, size, r0, r1, r2, r3, lr);
        fflush(stdout);
    }
    if ((half & 0xF800u) == 0xF000u) g_e8f.e8v_bl_n++;
}

/* E8X: screen/layout dims that feed r2 via 0x2F9970 and F74 producer via 0x818/0x81C. */
static void e8x_dump_draw_dims(uc_engine *uc, uint32_t r9, const char *why) {
    uint32_t a830 = 0, a818 = 0, a81c = 0, a64 = 0, a68 = 0, a6c = 0, f70 = 0, f74 = 0;
    if (!uc || !r9) return;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x830u, &a830);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x818u, &a818);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x81Cu, &a81c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA64u, &a64);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA68u, &a68);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA6Cu, &a6c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF70u, &f70);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF74u, &f74);
    printf("[JJFB_E8X_DIMS] why=%s R9_830=0x%X R9_818=0x%X R9_81C=0x%X "
           "R9_A64=0x%X R9_A68=0x%X R9_A6C=0x%X F70=0x%X F74=0x%X "
           "note=830_feeds_2F2854_r2 evidence=OBSERVED\n",
           why ? why : "?", a830, a818, a81c, a64, a68, a6c, f70, f74);
    fflush(stdout);
}

static void on_e8x_draw_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, lr = 0, pc = (uint32_t)address;
    uint16_t half = 0;
    (void)user_data;
    if (!g_e8f.e8x_mode) return;
    if (g_e8f.e8x_insn_n >= g_e8f.e8x_insn_max) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    g_e8f.e8x_insn_n++;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, pc, (uint8_t *)&half, 2);
    if (g_e8f.e8x_insn_n <= 120u || (g_e8f.e8x_insn_n % 25u) == 0u ||
        pc == 0x2F2874u || pc == 0x2EA1B8u || pc == 0x2F449Cu || pc == 0x2F44B8u ||
        pc == 0x2F44C4u || pc == 0x2F4764u || ((half & 0xF800u) == 0xF000u)) {
        printf("[JJFB_E8X_INSN] n=%u pc=0x%X size=%u r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               g_e8f.e8x_insn_n, pc, size, r0, r1, r2, r3, lr);
        if (pc == 0x2F44C4u)
            printf("[JJFB_E8X_RESOURCE_LOAD] pc=0x2F44C4 bl=0x2D92E4 r0=0x%X r1=0x%X "
                   "r2=0x%X r3=0x%X note=A64_zero_init_path evidence=OBSERVED\n",
                   r0, r1, r2, r3);
        fflush(stdout);
    }
}

static void e8y_dump_name(uc_engine *uc, uint32_t name_va) {
    uint8_t buf[48];
    int i, n = 0;
    memset(g_e8f.e8y_last_name, 0, sizeof(g_e8f.e8y_last_name));
    if (!uc || !name_va) {
        snprintf(g_e8f.e8y_last_name, sizeof(g_e8f.e8y_last_name), "(null)");
        return;
    }
    memset(buf, 0, sizeof(buf));
    if (!guest_memory_uc_peek((struct uc_struct *)uc, name_va, buf, (int)sizeof(buf) - 1)) {
        snprintf(g_e8f.e8y_last_name, sizeof(g_e8f.e8y_last_name), "(unmapped)");
        return;
    }
    for (i = 0; i < (int)sizeof(buf) - 1 && buf[i]; i++) {
        char c = (char)buf[i];
        if (c < 32 || c > 126) c = '.';
        g_e8f.e8y_last_name[n++] = c;
    }
    g_e8f.e8y_last_name[n] = '\0';
}

/* E9D: peek C-string + hex for name compare dumps. */
static int e9d_peek_cstr(uc_engine *uc, uint32_t va, char *ascii, int asciicap, char *hex,
                         int hexcap, int *out_len) {
    uint8_t buf[96];
    int i, n = 0, hl = 0;
    if (ascii && asciicap > 0) ascii[0] = 0;
    if (hex && hexcap > 0) hex[0] = 0;
    if (out_len) *out_len = 0;
    if (!uc || !va) return 0;
    memset(buf, 0, sizeof(buf));
    if (!guest_memory_uc_peek((struct uc_struct *)uc, va, buf, (int)sizeof(buf) - 1))
        return 0;
    for (i = 0; i < (int)sizeof(buf) - 1 && buf[i]; i++) {
        if (ascii && n < asciicap - 1) {
            char c = (char)buf[i];
            ascii[n++] = (c < 32 || c > 126) ? '.' : c;
        }
        if (hex && hl + 2 < hexcap) {
            static const char *hd = "0123456789abcdef";
            hex[hl++] = hd[buf[i] >> 4];
            hex[hl++] = hd[buf[i] & 0xF];
        }
    }
    if (ascii && asciicap > 0) ascii[n] = 0;
    if (hex && hexcap > 0) hex[hl] = 0;
    if (out_len) *out_len = i;
    return 1;
}

static void e9d_open_csvs(void) {
    const char *cmp_path;
    const char *req_path;
    if (!g_e8f.e9d_mode) return;
    cmp_path = getenv("JJFB_E9D_COMPARE_CSV");
    if (!cmp_path || !cmp_path[0]) cmp_path = "reports/e9d_name_compare_debug.csv";
    req_path = getenv("JJFB_E9D_REQUEST_CSV");
    if (!req_path || !req_path[0]) req_path = "reports/e9d_resource_request_trace.csv";
    if (!g_e8f.e9d_cmp_csv) {
        g_e8f.e9d_cmp_csv = fopen(cmp_path, "w");
        if (g_e8f.e9d_cmp_csv) {
            fprintf(g_e8f.e9d_cmp_csv,
                    "n,tick,req_va,idx_va,req_ascii,idx_ascii,req_hex,idx_hex,req_len,"
                    "idx_len,first_mismatch,host_strcmp,shim,cause\n");
            fflush(g_e8f.e9d_cmp_csv);
        }
    }
    if (!g_e8f.e9d_req_csv) {
        g_e8f.e9d_req_csv = fopen(req_path, "w");
        if (g_e8f.e9d_req_csv) {
            fprintf(g_e8f.e9d_req_csv,
                    "n,tick,pc,lr,name,natural_ok,bridge_used,note\n");
            fflush(g_e8f.e9d_req_csv);
        }
    }
}

static int e9d_first_mismatch(const char *a, const char *b) {
    int i = 0;
    if (!a) a = "";
    if (!b) b = "";
    while (a[i] && b[i] && a[i] == b[i]) i++;
    if (a[i] == b[i]) return -1;
    return i;
}

/* Host memcpy shim for 0x304F26 (read u32 name_len) and 0x304F7A (copy name).
 * Same DSM table+0xc stub path as strcmp — each call costs a full R9 module switch
 * and made a 20-entry scan miss the budget before reaching wy_jiao1. */
static int e9d_try_memcpy_shim(uc_engine *uc, uint32_t pc, uint32_t dst, uint32_t src,
                               uint32_t n) {
    uint8_t buf[128];
    uint32_t ret_pc;
    if (!g_e8f.e9d_mode || !g_e8f.e9d_strcmp_shim || !uc) return 0;
    if (n == 0 || n > sizeof(buf)) return 0;
    if (!dst || !src) return 0;
    memset(buf, 0, sizeof(buf));
    if (!guest_memory_uc_peek((struct uc_struct *)uc, src, buf, (int)n)) return 0;
    if (!guest_memory_uc_poke((struct uc_struct *)uc, dst, buf, (int)n)) return 0;
    ret_pc = (pc + 2u) | 1u; /* Thumb: land on insn after BLX */
    uc_reg_write(uc, UC_ARM_REG_PC, &ret_pc);
    /* Disarm only — do not restore R9 mid-scan (breaks 304BF0 index walk). */
    (void)module_r9_switch_cancel_dsm_helper_blx(uc, pc, 0);
    if (g_e8f.e9d_cmp_n < 3u && pc == 0x304F26u) {
        uint32_t len = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
                       ((uint32_t)buf[3] << 24);
        printf("[JJFB_E9D_MEMCPY_SHIM] pc=0x%X dst=0x%X src=0x%X n=%u name_len=%u "
               "evidence=OBSERVED\n",
               pc, dst, src, n, len);
        fflush(stdout);
    }
    return 1;
}

/* Host strcmp shim: DSM stub at table+0x28 (0xAC2D0) does movs r0,#1 and destroys
 * the requested-name pointer before dispatch — guest strcmp never sees both strings. */
static int e9d_try_strcmp_shim(uc_engine *uc, uint32_t r0, uint32_t r1) {
    char req[96], idx[96], req_hex[200], idx_hex[200];
    int req_len = 0, idx_len = 0, mism = -1, cmp, cause_wrong_cb = 0;
    uint32_t ret_pc;
    const char *cause = "OK_HOST_STRCMP";
    if (!g_e8f.e9d_mode || !uc) return 0;
    e9d_open_csvs();
    e9d_peek_cstr(uc, r0, req, (int)sizeof(req), req_hex, (int)sizeof(req_hex), &req_len);
    e9d_peek_cstr(uc, r1, idx, (int)sizeof(idx), idx_hex, (int)sizeof(idx_hex), &idx_len);
    mism = e9d_first_mismatch(req, idx);
    cmp = strcmp(req, idx);
    g_e8f.e9d_cmp_n++;
    if (cmp == 0) g_e8f.e9d_cmp_match_n++;
    if (strchr(req, '!') || strchr(idx, '!')) {
        if (cmp != 0 && mism >= 0 && req[mism] == '!' && idx[mism] != '!')
            cause = "EXCLAMATION_SEPARATOR_MISMATCH";
        else if (cmp != 0)
            cause = "CASE_OR_ENCODING_MISMATCH";
    } else if (cmp != 0) {
        cause = "CASE_OR_ENCODING_MISMATCH";
    }
    if (g_e8f.e9d_cmp_n <= 40u || cmp == 0 || (g_e8f.e9d_cmp_n % 10u) == 0u) {
        printf("[JJFB_E9D_NAME_COMPARE] n=%u tick=%u req_va=0x%X idx_va=0x%X "
               "req=\"%s\" idx=\"%s\" req_len=%d idx_len=%d first_mismatch=%d "
               "host_strcmp=%d cause=%s evidence=OBSERVED\n",
               g_e8f.e9d_cmp_n, g_e8f.tick, r0, r1, req, idx, req_len, idx_len, mism, cmp,
               cause);
        fflush(stdout);
    }
    if (g_e8f.e9d_cmp_csv) {
        fprintf(g_e8f.e9d_cmp_csv,
                "%u,%u,0x%X,0x%X,\"%s\",\"%s\",%s,%s,%d,%d,%d,%d,%d,%s\n",
                g_e8f.e9d_cmp_n, g_e8f.tick, r0, r1, req, idx, req_hex, idx_hex, req_len,
                idx_len, mism, cmp, g_e8f.e9d_strcmp_shim ? 1 : 0, cause);
        fflush(g_e8f.e9d_cmp_csv);
    }
    if (!g_e8f.e9d_strcmp_shim) return 0;
    /* Skip guest BLX to broken DSM stub; land on cmp r0,#0 at 0x304F94. */
    {
        uint32_t status = (uint32_t)(int32_t)cmp;
        ret_pc = 0x304F94u | 1u;
        uc_reg_write(uc, UC_ARM_REG_R0, &status);
        uc_reg_write(uc, UC_ARM_REG_PC, &ret_pc);
        /* Disarm only — R9 restore deferred to E9E postmatch / 310BBC. */
        (void)module_r9_switch_cancel_dsm_helper_blx(uc, 0x304F92u, 0);
    }
    g_e8f.e9d_shim_n++;
    if (cmp == 0) {
        g_e8f.e9d_natural_hit_n++;
        printf("[JJFB_E9D_NATURAL_MATCH] name=\"%s\" n=%u shim=%u "
               "class=NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME "
               "note=host_strcmp_shim_skips_DSM_stub NOT_PRODUCT evidence=OBSERVED\n",
               req, g_e8f.e9d_cmp_n, g_e8f.e9d_shim_n);
        printf("[NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME] name=\"%s\" evidence=OBSERVED\n",
               req);
        fflush(stdout);
        (void)cause_wrong_cb;
        /* E9E: skip slow post-match DSM file/inflate; fill object with original bytes. */
        if (g_e8f.e9e_postmatch_shims && !g_e8f.e9e_postmatch_done) {
            if (e9e_try_postmatch_complete(uc, req))
                return 1;
        }
    } else if (g_e8f.e9d_cmp_n == 1u) {
        printf("[NATURAL_MEMBER_RESOLVE_BLOCKED_BY_NAME] note=waiting_for_match "
               "evidence=OBSERVED\n");
        fflush(stdout);
    }
    return 1;
}

static void on_e8y_2d92e4_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, lr = 0, pc = (uint32_t)address;
    uint16_t half = 0;
    (void)user_data;
    if (!g_e8f.e8y_mode) return;
    if (g_e8f.e8y_insn_n >= g_e8f.e8y_insn_max) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    g_e8f.e8y_insn_n++;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, pc, (uint8_t *)&half, 2);
    if (g_e8f.e8y_insn_n <= 100u || (g_e8f.e8y_insn_n % 40u) == 0u ||
        pc == 0x2D92FAu || pc == 0x2D9326u || pc == 0x2D9354u || pc == 0x2D939Eu ||
        pc == 0x2D93CCu || pc == 0x2D93BCu || ((half & 0xF800u) == 0xF000u)) {
        printf("[JJFB_E8Y_INSN] n=%u pc=0x%X size=%u r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               g_e8f.e8y_insn_n, pc, size, r0, r1, r2, r3, lr);
        fflush(stdout);
    }
}
#endif

static void add_bp(uint32_t pc, const char *tag) {
    E8fBp *bp;
    uint32_t even = pc & ~1u;
    int i;
    if (g_e8f.nbps >= E8F_MAX_BP) return;
    for (i = 0; i < g_e8f.nbps; i++) {
        if (g_e8f.bps[i].pc == even) return;
    }
    bp = &g_e8f.bps[g_e8f.nbps++];
    memset(bp, 0, sizeof(*bp));
    bp->used = 1;
    bp->pc = even;
    strncpy(bp->tag, tag ? tag : "?", sizeof(bp->tag) - 1);
}

void robotol_flag_writer_trace_try_arm(void *uc) {
    uint32_t code_base = 0, code_size = 0;
    uint32_t pcs[E8F_MAX_BP];
    int np = 0, i;
    const char *csv;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif

    if (!robotol_flag_writer_trace_enabled()) return;
    if (!uc) uc = g_e8f.uc;
    if (!uc || g_e8f.armed) return;
    if (!g_e8f.writer_bp && !g_e8f.longpath_watch && !g_e8f.caller_bp && !g_e8f.fault_watch &&
        !g_e8f.dispatcher_bp && !g_e8f.parent_bp && !g_e8f.state_watch && !g_e8f.e8j_bp &&
        !g_e8f.e8j_queue_read_watch && !g_e8f.svc_trap && !g_e8f.e8m_parent_trace &&
        !g_e8f.e8k_10102_case && !g_e8f.display_first && !g_e8f.e8v_mode && !g_e8f.e8w_mode &&
        !g_e8f.e8x_mode && !g_e8f.e8y_mode)
        return;
    if (!find_robotol_code(&code_base, &code_size)) return;

    /* Built-in top writers (E8D/E8F list). */
    static const uint32_t k_default[] = {
        0x2E87F2u, 0x2F4E82u, 0x2F7F56u, 0x2FB286u, 0x2FC8CAu, 0x2FEDFAu, 0x2FEE4Eu,
        0x30CC72u, 0x311C3Eu, 0x2E3A68u, 0x2F097Au, 0x2F7772u, 0x2FB008u, 0x307796u,
        0x30AA42u, 0x3115B4u, 0x2D9B68u, 0x2D9CE6u, 0x2DBA82u, 0x2E7DBCu, 0x2F7F2Cu,
    };
    /* E8G priority bootstrap callers + fn entries. */
    static const uint32_t k_callers[] = {
        0x302340u, 0x302362u, 0x2FC048u, 0x2E32A2u, 0x2EFF1Cu, 0x2F08A4u, 0x2F0D6Au,
        0x2F1FF0u, 0x30D9EEu, 0x30AF8Au, 0x30DF78u, 0x2DA64Au, 0x2DE7ECu, 0x30FCEEu,
    };
    /* E8H dispatcher chain (CSV may override). */
    static const uint32_t k_disp[] = {
        0x300714u, 0x3002C0u, 0x30103Cu, 0x3020C8u, 0x302340u, 0x302362u, 0x2F4E82u,
    };
    static const char *k_disp_tags[] = {
        "d300714", "d3002c0", "d30103c", "d3020c8", "d302340", "d302362", "d2f4e82",
    };

    csv = getenv("JJFB_E8F_WRITER_PCS");
    if (csv && csv[0])
        np = parse_hex_list(csv, pcs, E8F_MAX_BP);
    else {
        for (i = 0; i < (int)(sizeof(k_default) / sizeof(k_default[0])); i++)
            pcs[np++] = k_default[i];
    }
    g_e8f.nbps = 0;
    if (g_e8f.writer_bp) {
        for (i = 0; i < np; i++)
            add_bp(pcs[i], "writer");
    }
    if (g_e8f.longpath_watch) {
        add_bp(0x30D28Cu, "longpath");
        add_bp(0x30D262u, "fe8_str");
    }
    if (g_e8f.caller_bp) {
        uint32_t cpcs[E8F_MAX_BP];
        int nc = 0;
        const char *ccsv = getenv("JJFB_E8G_CALLER_PCS");
        if (ccsv && ccsv[0])
            nc = parse_hex_list(ccsv, cpcs, E8F_MAX_BP);
        else {
            for (i = 0; i < (int)(sizeof(k_callers) / sizeof(k_callers[0])); i++)
                cpcs[nc++] = k_callers[i];
        }
        for (i = 0; i < nc; i++)
            add_bp(cpcs[i], "caller");
    }
    if (g_e8f.fault_watch)
        add_bp(0x2D92B0u, "fault");
    if (g_e8f.dispatcher_bp) {
        uint32_t dpcs[E8F_MAX_BP];
        int nd = 0;
        const char *dcsv = getenv("JJFB_E8H_DISPATCHER_PCS");
        if (dcsv && dcsv[0]) {
            nd = parse_hex_list(dcsv, dpcs, E8F_MAX_BP);
            for (i = 0; i < nd; i++) {
                char tag[16];
                snprintf(tag, sizeof(tag), "d%X", dpcs[i] & 0xFFFFFFu);
                add_bp(dpcs[i], tag);
            }
        } else {
            for (i = 0; i < (int)(sizeof(k_disp) / sizeof(k_disp[0])); i++)
                add_bp(k_disp[i], k_disp_tags[i]);
        }
    }
    if (g_e8f.parent_bp) {
        uint32_t ppcs[E8F_MAX_BP];
        int np2 = 0;
        const char *pcsv = getenv("JJFB_E8I_PARENT_PCS");
        static const uint32_t k_parent_chain[] = {
            0x300158u, 0x3002C0u, 0x300714u, 0x30103Cu, 0x3020C8u, 0x302340u, 0x302362u,
        };
        if (pcsv && pcsv[0])
            np2 = parse_hex_list(pcsv, ppcs, E8F_MAX_BP);
        else {
            for (i = 0; i < (int)(sizeof(k_parent_chain) / sizeof(k_parent_chain[0])); i++)
                ppcs[np2++] = k_parent_chain[i];
        }
        for (i = 0; i < np2; i++) {
            char tag[16];
            uint32_t p = ppcs[i];
            if (p == 0x300158u)
                snprintf(tag, sizeof(tag), "p300158");
            else if (p == 0x300714u)
                snprintf(tag, sizeof(tag), "p300714");
            else if (p == 0x30103Cu)
                snprintf(tag, sizeof(tag), "p30103c");
            else
                snprintf(tag, sizeof(tag), "p%X", p & 0xFFFFFFu);
            add_bp(p, tag);
        }
    }
    if (g_e8f.e8j_bp) {
        uint32_t jpcs[E8F_MAX_BP];
        char roles[E8F_MAX_BP][4];
        int nj = 0;
        const char *jspec = getenv("JJFB_E8J_BP_SPEC");
        if (jspec && jspec[0])
            nj = parse_role_spec(jspec, jpcs, roles, E8F_MAX_BP);
        for (i = 0; i < nj; i++) {
            char tag[16];
            char role = roles[i][0] ? roles[i][0] : 'u';
            snprintf(tag, sizeof(tag), "%c%X", role, jpcs[i] & 0xFFFFFFu);
            add_bp(jpcs[i], tag);
        }
        printf("[JJFB_E8J_CLUSTER_BP] spec_n=%d armed_via_parent_table evidence=OBSERVED\n", nj);
        fflush(stdout);
    }
    /* E8U/E8V: idle gate / success / UI landmarks under DISPLAY_FIRST or E8V. */
    if (g_e8f.display_first || g_e8f.e8v_mode) {
        add_bp(0x3066B8u, "gC44H");
        add_bp(0x3066C6u, "gC9D");
        add_bp(0x3066DAu, "gCF5");
        add_bp(0x306740u, "gOK");
        add_bp(0x2E88CCu, "gDRAW");
        add_bp(0x2E2520u, "p2E2520");
        add_bp(0x2E2F50u, "p2E2F50");
        add_bp(0x2E4788u, "p2E4788");
        add_bp(0x2E4840u, "p2E4840");
        printf("[JJFB_DISPLAY_FIRST] gate_bp=0x3066B8,0x3066C6,0x3066DA success=0x306740,0x2E88CC "
               "note=runtime_branch_assist_only evidence=HYPOTHESIS\n");
        fflush(stdout);
    }
    if (g_e8f.e8v_mode || g_e8f.e8v_e88cc_trace || g_e8f.e8w_mode) {
        add_bp(0x2E88E6u, "vD14");
        add_bp(0x2E8914u, "vNULL");
        add_bp(0x2E898Cu, "vEPI");
        add_bp(0x2E8980u, "vBL2854a");
        add_bp(0x2E89A8u, "vBL2854b");
        add_bp(0x2E8A22u, "vBL5BFC");
        add_bp(0x2E8A44u, "vBLA058");
        add_bp(0x2F2854u, "v2F2854");
        add_bp(0x305BFCu, "v305BFC");
        add_bp(0x2EA058u, "v2EA058");
        add_bp(0x2FF908u, "v2FF908");
        add_bp(0x305E78u, "v305E78");
        add_bp(0x2E993Cu, "v2E993C");
        printf("[JJFB_E8V_BP] e88cc_exits=0x2E88E6,0x2E8914,0x2E898C "
               "draw_cands=0x2F2854,0x305BFC,0x2EA058 evidence=HYPOTHESIS\n");
        fflush(stdout);
    }
    if (g_e8f.e8w_mode) {
        add_bp(0x2D9CFCu, "wF6C");
        add_bp(0x2FBD18u, "wF70");
        add_bp(0x2E8920u, "wF74");
        add_bp(0x30AA32u, "wClr70");
        add_bp(0x30AA34u, "wClr6C");
        add_bp(0x30AA42u, "wClr74");
        add_bp(0x311AA6u, "w31170");
        add_bp(0x311AB8u, "w3116C");
        add_bp(0x311AC4u, "w31174");
        printf("[JJFB_E8W_BP] f6c_writers=0x2D9CFC,0x2FBD18,0x2E8920 clear=0x30A9EC "
               "evidence=HYPOTHESIS\n");
        fflush(stdout);
    }
    if (g_e8f.e8x_mode) {
        add_bp(0x2EA188u, "x2EA188");
        add_bp(0x2F449Cu, "x2F449C");
        add_bp(0x310BBCu, "x310BBC");
        add_bp(0x2F99D0u, "x2F99D0");
        add_bp(0x2F5B38u, "x2F5B38");
        printf("[JJFB_E8X_BP] path=0x2F2854->0x2EA188->0x2F449C->0x310BBC "
               "producer=0x2F99D0 evidence=HYPOTHESIS\n");
        fflush(stdout);
    }
    if (g_e8f.e8y_mode) {
        add_bp(0x2D92E4u, "y2D92E4");
        add_bp(0x2F44C8u, "yRetA64");
        add_bp(0x2F44CEu, "yStrA64");
        add_bp(0x2F44E4u, "yStrA68");
        add_bp(0x2F44EEu, "yStrA6C");
        add_bp(0x304BF0u, "y304BF0");
        add_bp(0x2FD868u, "y2FD868");
        add_bp(0x2F8528u, "y2F8528");
        add_bp(0x2F6BD4u, "y2F6BD4");
        if (g_e8f.e9d_mode) {
            add_bp(0x304F26u, "d304F26"); /* read name_len via memcpy */
            add_bp(0x304F7Au, "d304F7A"); /* copy name bytes */
            add_bp(0x304F92u, "d304F92"); /* strcmp — shim target */
#ifdef GWY_HAVE_UNICORN
            e9d_open_csvs();
#endif
            printf("[JJFB_E9D_BP] compare=0x304F26/0x304F7A/0x304F92 shim=%d "
                   "evidence=HYPOTHESIS\n",
                   g_e8f.e9d_strcmp_shim);
        }
        printf("[JJFB_E8Y_BP] resolve=0x2D92E4 stores=0x2F44CE/E4/EE helpers=0x304BF0,"
               "0x2FD868,0x2F8528 evidence=HYPOTHESIS\n");
        fflush(stdout);
    }

#ifdef GWY_HAVE_UNICORN
    for (i = 0; i < g_e8f.nbps; i++) {
        E8fBp *bp = &g_e8f.bps[i];
        if (bp->pc < code_base || bp->pc >= code_base + code_size) {
            printf("[JJFB_E8F_WRITER_BP] skip=out_of_robotol pc=0x%X evidence=OBSERVED\n", bp->pc);
            continue;
        }
        ue = uc_hook_add((uc_engine *)uc, &bp->hook, UC_HOOK_CODE, (void *)on_e8f_code, bp,
                         (uint64_t)bp->pc, (uint64_t)bp->pc);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8F_WRITER_BP] arm_fail pc=0x%X uc_err=%u evidence=OBSERVED\n", bp->pc,
                   (unsigned)ue);
        }
    }
    if (g_e8f.svc_trap) {
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.svc_code_hook, UC_HOOK_CODE,
                         (void *)on_e8h_svc_code, NULL, 0x2D92AEull, 0x2D92AEull);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8H_SVC_TRAP] code_arm_fail uc_err=%u evidence=OBSERVED\n", (unsigned)ue);
        }
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.svc_intr_hook, UC_HOOK_INTR, (void *)on_e8h_intr,
                         NULL, 1, 0);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8H_SVC_TRAP] intr_arm_fail uc_err=%u evidence=OBSERVED\n", (unsigned)ue);
        }
        printf("[JJFB_E8H_SVC_TRAP] armed=1 stop=%d note=observe_only_no_fake_success "
               "evidence=OBSERVED\n",
               g_e8f.svc_stop);
    }
    if (g_e8f.e8m_parent_trace) {
        /* Trace first 200 insns inside parent body during tick1 diagnostic fires. */
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.e8m_parent_hook, UC_HOOK_CODE,
                         (void *)on_e8m_parent_insn, NULL, 0x300158ull, 0x3004F6ull);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8M_PARENT_TRACE] arm_fail uc_err=%u evidence=OBSERVED\n", (unsigned)ue);
        } else {
            printf("[JJFB_E8M_PARENT_TRACE] armed=1 range=0x300158..0x3004F6 max_insn=200 "
                   "evidence=OBSERVED\n");
        }
    }
    if (g_e8f.e8v_e88cc_trace) {
        /* Body of 0x2E88CC through epilogue / draw loop (before next fn @ 0x2E8A74). */
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.e8v_e88cc_hook, UC_HOOK_CODE,
                         (void *)on_e8v_e88cc_insn, NULL, 0x2E88CCull, 0x2E8A4Eull);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8V_E88CC_TRACE] arm_fail uc_err=%u evidence=OBSERVED\n", (unsigned)ue);
        } else {
            printf("[JJFB_E8V_E88CC_TRACE] armed=1 range=0x2E88CC..0x2E8A4E max_insn=%u "
                   "evidence=OBSERVED\n",
                   g_e8f.e8v_insn_max);
        }
    }
    if (g_e8f.e8x_mode) {
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.e8x_wrap_hook, UC_HOOK_CODE,
                         (void *)on_e8x_draw_insn, NULL, 0x2F2854ull, 0x2F287Cull);
        if (ue != UC_ERR_OK)
            printf("[JJFB_E8X_DRAW_TRACE] wrap_arm_fail uc_err=%u evidence=OBSERVED\n",
                   (unsigned)ue);
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.e8x_worker_hook, UC_HOOK_CODE,
                         (void *)on_e8x_draw_insn, NULL, 0x2EA188ull, 0x2EA1FCull);
        if (ue != UC_ERR_OK)
            printf("[JJFB_E8X_DRAW_TRACE] worker_arm_fail uc_err=%u evidence=OBSERVED\n",
                   (unsigned)ue);
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.e8x_2f449c_hook, UC_HOOK_CODE,
                         (void *)on_e8x_draw_insn, NULL, 0x2F449Cull, 0x2F4A80ull);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8X_DRAW_TRACE] draw_arm_fail uc_err=%u evidence=OBSERVED\n",
                   (unsigned)ue);
        } else {
            printf("[JJFB_E8X_DRAW_TRACE] armed=1 ranges=0x2F2854..287C,0x2EA188..1FC,"
                   "0x2F449C..4A80 max_insn=%u evidence=OBSERVED\n",
                   g_e8f.e8x_insn_max);
        }
    }
    if (g_e8f.e8y_mode) {
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.e8y_2d92e4_hook, UC_HOOK_CODE,
                         (void *)on_e8y_2d92e4_insn, NULL, 0x2D92E4ull, 0x2D9560ull);
        if (ue != UC_ERR_OK)
            printf("[JJFB_E8Y_RESOLVE_TRACE] arm_fail uc_err=%u evidence=OBSERVED\n",
                   (unsigned)ue);
        else
            printf("[JJFB_E8Y_RESOLVE_TRACE] armed=1 range=0x2D92E4..0x2D9560 max_insn=%u "
                   "evidence=OBSERVED\n",
                   g_e8f.e8y_insn_max);
    }
#endif
    g_e8f.armed = 1;
    g_e8f.uc = uc;
    printf("[JJFB_E8F_WRITER_BP] armed=1 n=%d code_base=0x%X size=0x%X caller_bp=%d "
           "fault_watch=%d dispatcher_bp=%d parent_bp=%d state_watch=%d e8j_bp=%d "
           "qread_watch=%d svc_trap=%d e8m_trace=%d evidence=OBSERVED\n",
           g_e8f.nbps, code_base, code_size, g_e8f.caller_bp, g_e8f.fault_watch,
           g_e8f.dispatcher_bp, g_e8f.parent_bp, g_e8f.state_watch, g_e8f.e8j_bp,
           g_e8f.e8j_queue_read_watch, g_e8f.svc_trap, g_e8f.e8m_parent_trace);
    fflush(stdout);
}

static void e8j_try_arm_queue_read_watch(void *uc) {
    uint32_t r9 = 0;
    uint32_t a_fe8, a_b7d;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif
    if (!g_e8f.e8j_queue_read_watch || g_e8f.e8j_qread_armed || !uc) return;
    if (!find_robotol_r9(uc, &r9) || !r9) return;
    /* FE8 + B7D only — do not band-hook 7D8..FE8 (too hot / too slow). */
    a_fe8 = r9 + 0xFE8u;
    a_b7d = r9 + 0xB7Du;
#ifdef GWY_HAVE_UNICORN
    ue = uc_hook_add((uc_engine *)uc, &g_e8f.e8j_qread_hook, UC_HOOK_MEM_READ,
                     (void *)on_e8j_queue_read, NULL, (uint64_t)a_fe8, (uint64_t)(a_fe8 + 3u));
    if (ue != UC_ERR_OK) {
        printf("[JJFB_E8J_QUEUE_READ_WATCH] arm_fail_fe8 addr=0x%X uc_err=%u evidence=OBSERVED\n",
               a_fe8, (unsigned)ue);
        return;
    }
    /* Second hook: reuse same callback; leak-safe on reset via single del of first is incomplete,
     * so fold B7D into a second add stored only if first ok — accept one-hook FE8 if B7D fails. */
    {
        uc_hook h2 = 0;
        ue = uc_hook_add((uc_engine *)uc, &h2, UC_HOOK_MEM_READ, (void *)on_e8j_queue_read, NULL,
                         (uint64_t)a_b7d, (uint64_t)a_b7d);
        (void)h2; /* intentional: reset clears via full uc rebuild / process exit */
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8J_QUEUE_READ_WATCH] arm_fail_b7d addr=0x%X uc_err=%u evidence=OBSERVED\n",
                   a_b7d, (unsigned)ue);
        }
    }
#endif
    g_e8f.e8j_qread_armed = 1;
    printf("[JJFB_E8J_QUEUE_READ_WATCH] armed=1 r9=0x%X fe8=0x%X b7d=0x%X "
           "note=observe_only_FE8_B7D evidence=OBSERVED\n",
           r9, a_fe8, a_b7d);
    fflush(stdout);
}

static void e8i_try_arm_state_watch(void *uc) {
    uint32_t r9 = 0;
    uint32_t addr;
    uint32_t cur = 0;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif
    if (!g_e8f.state_watch || g_e8f.state_watch_armed || !uc) return;
    if (!find_robotol_r9(uc, &r9) || !r9) return;
    addr = r9 + E8I_STATE_OFF;
#ifdef GWY_HAVE_UNICORN
    ue = uc_hook_add((uc_engine *)uc, &g_e8f.state_hook, UC_HOOK_MEM_WRITE,
                     (void *)on_e8i_state_write, NULL, (uint64_t)addr, (uint64_t)(addr + 3u));
    if (ue != UC_ERR_OK) {
        printf("[JJFB_E8I_STATE_WATCH] arm_fail addr=0x%X uc_err=%u evidence=OBSERVED\n", addr,
               (unsigned)ue);
        return;
    }
#endif
    g_e8f.state_watch_armed = 1;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &cur);
    g_e8f.state_last_val = cur;
    printf("[JJFB_E8I_STATE_WATCH] armed=1 addr=0x%X r9=0x%X off=0x%X init=0x%X "
           "note=observe_only_no_force evidence=OBSERVED\n",
           addr, r9, E8I_STATE_OFF, cur);
    fflush(stdout);
}

static void snap_idle_flags(void *uc, uint32_t r9, const char *reason) {
    uint8_t c44 = 0, c9d = 0, cf5 = 0, b7d = 0;
    uint32_t fe8 = 0, queue = 0, d8d0 = 0, e6c = 0, d7d8 = 0, eec7c = 0, dec30 = 0;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC44u, &c44, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC9Du, &c9d, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCF5u, &cf5, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xB7Du, &b7d, 1);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xFE8u, &fe8);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x844u, &queue);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + (0x800u + 0xD0u), &d8d0);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xE6Cu, &e6c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x7D8u, &d7d8);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xEECu + 0x7Cu, &eec7c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xDECu + 0x30u, &dec30);
    printf("[JJFB_E8F_FLAG_SNAP] reason=%s r9=0x%X C44=0x%X C9D=0x%X CF5=0x%X B7D=0x%X "
           "FE8=0x%X queue_depth=0x%X R9_state=0x%X E6C=0x%X R9_7D8=0x%X EEC7C=0x%X "
           "DEC30=0x%X evidence=OBSERVED\n",
           reason ? reason : "-", r9, c44, c9d, cf5, b7d, fe8, queue, d8d0, e6c, d7d8, eec7c,
           dec30);
    fflush(stdout);
}

/* E8P FAST_OBJECT_ASSIST: poke R9-embedded fields only (not heap pointer invent). */
static void fast_poke_c44_dep_fields(void *uc, uint32_t r9) {
    if (!g_e8f.fast_assist || !r9) return;
    if (g_e8f.fast_c6c22_set) {
        uint32_t addr = r9 + 0xC6Cu + 0x22u;
        uint8_t oldb = 0, newb = (uint8_t)(g_e8f.fast_c6c22_val & 0xFFu);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, addr, &oldb, 1);
        (void)guest_memory_uc_poke((struct uc_struct *)uc, addr, &newb, 1);
        printf("[JJFB_FAST_OBJ] field=C6C22 old=0x%X new=0x%X addr=0x%X "
               "note=FAST_ASSIST_not_product_R1_20_gate_byte evidence=HYPOTHESIS\n",
               oldb, newb, addr);
    }
    if (g_e8f.fast_eec7c_set) {
        uint32_t addr = r9 + 0xEECu + 0x7Cu;
        uint32_t oldv = 0;
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &oldv);
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, addr, g_e8f.fast_eec7c_val);
        printf("[JJFB_FAST_OBJ] field=EEC7C old=0x%X new=0x%X addr=0x%X "
               "note=FAST_ASSIST_not_product_R9_embedded_struct evidence=HYPOTHESIS\n",
               oldv, g_e8f.fast_eec7c_val, addr);
    }
    if (g_e8f.fast_dec30_set) {
        uint32_t addr = r9 + 0xDECu + 0x30u;
        uint32_t oldv = 0;
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &oldv);
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, addr, g_e8f.fast_dec30_val);
        printf("[JJFB_FAST_OBJ] field=DEC30 old=0x%X new=0x%X addr=0x%X "
               "note=FAST_ASSIST_not_product_R9_embedded_struct evidence=HYPOTHESIS\n",
               oldv, g_e8f.fast_dec30_val, addr);
    }
    if (g_e8f.fast_c6c22_set || g_e8f.fast_eec7c_set || g_e8f.fast_dec30_set) {
        snap_idle_flags(uc, r9, "after_fast_obj");
        fflush(stdout);
    }
}

/* E8R: invoke real unlock writer path 0x2FC8C0 (BL 0x3046A8 then STRB#1@C44).
 * Not a direct C44 poke. FAST_ASSIST / COUNTERFACTUAL_ONLY. */
static void fast_call_c44_unlock(void *uc, const char *when) {
    uint32_t r9_save = 0, r9_run = 0;
    uint8_t c44_before = 0, c44_after = 0;
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    int ok;
    uint64_t lim = 200000ull;
    if (!g_e8f.fast_assist || !g_e8f.fast_unlock_call || g_e8f.fast_unlock_done || !uc)
        return;
    g_e8f.fast_unlock_done = 1;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    if (!find_robotol_r9(uc, &r9_run)) r9_run = r9_save;
    if (r9_run) (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_run);
    if (r9_run)
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC44u, &c44_before, 1);
    snap_idle_flags(uc, r9_run, "before_fast_unlock");
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = 0;
    abi.set_r1 = 1;
    abi.r1 = 0;
    abi.set_r2 = 1;
    abi.r2 = 0;
    abi.set_r3 = 1;
    abi.r3 = 0;
    abi.set_lr = 1;
    abi.lr = 0x80000u;
    if (g_e8f.fast_insn_limit) lim = g_e8f.fast_insn_limit;
    printf("[JJFB_FAST_UNLOCK_CALL] when=%s entry=0x2FC8C0 r9=0x%X C44_before=0x%X "
           "note=FAST_ASSIST_real_fn_not_C44_poke evidence=HYPOTHESIS\n",
           when ? when : "-", r9_run, c44_before);
    fflush(stdout);
    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, 0x2FC8C1u, 0x80000u, lim, &abi, &out);
    if (r9_run)
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC44u, &c44_after, 1);
    snap_idle_flags(uc, r9_run, "after_fast_unlock");
    printf("[JJFB_FAST_UNLOCK_DONE] ok=%d end=%s pc_after=0x%X r0_after=0x%X "
           "C44_before=0x%X C44_after=0x%X note=FAST_ASSIST_not_product evidence=OBSERVED\n",
           ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after, out.r0_after, c44_before,
           c44_after);
    if (c44_after == 1u)
        printf("[JJFB_E8R_C44_UNLOCKED] C44=1 via=0x2FC8C0 note=FAST_ASSIST_real_writer "
               "evidence=OBSERVED\n");
    fflush(stdout);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
}

static void e8x_install_f74_desc_assist(void *uc, uint32_t r9);
static void e8y_install_a64_assist(void *uc, uint32_t r9);

/* E8W: structural F74 table assist derived from 0x2E88CC reads (NOT product / no paint).
 * Gate opens on F74!=0; first draw candidate 0x2F2854 is reachable before F74[] loop.
 * Scratch page holds 2F5B38 header + pointer table for later 0x305BFC/0x2EA058 path. */
static void e8w_install_f6c_assist(void *uc, uint32_t r9) {
    uint32_t f70 = 0, f74 = 0, scroll = 0, w0 = 0;
    uint32_t table;
    int i;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif
    if (!uc || !r9 || g_e8f.e8w_f6c_assist_done) return;
    g_e8f.e8w_f6c_assist_done = 1;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF6Cu, &w0);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF70u, &f70);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF74u, &f74);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x1A8u + 0x58u, &scroll);
    printf("[JJFB_FAST_F6C_OBJECT_ASSIST] before F6C_w0=0x%X F70=0x%X F74=0x%X scroll200=0x%X "
           "note=FAST_OBJECT_ASSIST_NOT_PRODUCT_SUCCESS evidence=HYPOTHESIS\n",
           w0, f70, f74, scroll);
#ifdef GWY_HAVE_UNICORN
    ue = uc_mem_map((uc_engine *)uc, E8W_SCRATCH_BASE, E8W_SCRATCH_SIZE, UC_PROT_ALL);
    if (ue != UC_ERR_OK) {
        /* Already-mapped is fine; poke will confirm usability. */
        printf("[JJFB_FAST_F6C_OBJECT_ASSIST] map_status uc_err=%u "
               "note=continue_if_region_exists evidence=OBSERVED\n",
               (unsigned)ue);
    }
#endif
    g_e8f.e8w_scratch_base = E8W_SCRATCH_BASE;
    /* header @ base for 0x2F5B38 (reads ptr-4); table @ base+4 used as F74. */
    {
        uint8_t hdr[4] = {0x10, 0x00, 0x00, 0x00};
        (void)guest_memory_uc_poke((struct uc_struct *)uc, E8W_SCRATCH_BASE, hdr, 4);
    }
    table = E8W_SCRATCH_BASE + 4u;
    for (i = 0; i < 32; i++) {
        uint32_t slot = E8W_SCRATCH_BASE + 0x100u; /* stub element */
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, table + (uint32_t)i * 4u, slot);
    }
    {
        uint8_t stub[0x40];
        memset(stub, 0, sizeof(stub));
        stub[0] = 1;
        (void)guest_memory_uc_poke((struct uc_struct *)uc, E8W_SCRATCH_BASE + 0x100u, stub,
                                   sizeof(stub));
    }
    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xF74u, table);
    g_e8f.e8w_f74_assist = table;
    /* 0x3E7 sentinel at R9+0x200 forces epilogue @ 0x2E8994 — clear if present. */
    if (scroll == 0x3E7u) {
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x1A8u + 0x58u, 0u);
        printf("[JJFB_FAST_F6C_OBJECT_ASSIST] cleared_scroll_sentinel old=0x3E7 new=0 "
               "note=derived_from_0x2E8994 evidence=HYPOTHESIS\n");
    }
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF74u, &f74);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x1A8u + 0x58u, &scroll);
    printf("[JJFB_FAST_F6C_OBJECT_ASSIST] after F74=0x%X scratch=0x%X scroll200=0x%X "
           "note=structural_table_only_no_framebuffer evidence=OBSERVED\n",
           f74, E8W_SCRATCH_BASE, scroll);
    if (g_e8f.e8x_mode && g_e8f.e8x_f74_desc_assist && !g_e8f.e8x_f74_desc_assist_done)
        e8x_install_f74_desc_assist(uc, r9);
    fflush(stdout);
}

/* Structural dims only — no pixels / no fake DRAW. Derived: vmrp 240x320; r2=*(R9+0x830). */
static void e8x_install_f74_desc_assist(void *uc, uint32_t r9) {
    uint32_t a830 = 0, a818 = 0, a81c = 0;
    if (!uc || !r9 || g_e8f.e8x_f74_desc_assist_done) return;
    g_e8f.e8x_f74_desc_assist_done = 1;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x830u, &a830);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x818u, &a818);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x81Cu, &a81c);
    printf("[JJFB_FAST_F74_DESCRIPTOR_ASSIST] before R9_830=0x%X R9_818=0x%X R9_81C=0x%X "
           "note=FAST_STRUCTURAL_DIMS_NOT_PRODUCT evidence=HYPOTHESIS\n",
           a830, a818, a81c);
    if (!a830)
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x830u, 240u);
    if (!a818)
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x818u, 240u);
    if (!a81c)
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x81Cu, 320u);
#ifdef GWY_HAVE_UNICORN
    e8x_dump_draw_dims((uc_engine *)uc, r9, "after_desc_assist");
#endif
    printf("[JJFB_FAST_F74_DESCRIPTOR_ASSIST] after note=dims_only_no_framebuffer "
           "evidence=OBSERVED\n");
    fflush(stdout);
}

/* Case B: call real F74 producer 0x2F99D0 when F70 path is open (ABI from 0x2E891A). */
static void e8x_call_2f99d0(void *uc, uint32_t r9) {
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    uint32_t r9_save = 0, f70 = 0, f74 = 0, a818 = 0, r0_in = 0, r1_in = 0;
    int ok;
    uint64_t lim = 400000ull;
    if (!uc || !r9 || g_e8f.e8x_call_2f99d0_done) return;
    g_e8f.e8x_call_2f99d0_done = 1;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF70u, &f70);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF74u, &f74);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x818u, &a818);
    if (!a818)
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x818u, 240u);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x818u, &a818);
    if (!f70) {
        if (!g_e8f.e8w_scratch_base)
            e8w_install_f6c_assist(uc, r9);
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xF70u,
                                       E8W_SCRATCH_BASE + 0x80u);
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xF74u, 0u);
        f70 = E8W_SCRATCH_BASE + 0x80u;
        printf("[JJFB_E8X_2F99D0] primed F70=0x%X F74=0 note=producer_arm_open "
               "evidence=HYPOTHESIS\n",
               f70);
    }
    r0_in = f70;
    r1_in = (a818 > 0x1Eu) ? (a818 - 0x1Eu) : 0xD2u;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9);
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = r0_in;
    abi.set_r1 = 1;
    abi.r1 = r1_in;
    abi.set_r2 = 1;
    abi.r2 = 0;
    abi.set_r3 = 1;
    abi.r3 = 0;
    abi.set_lr = 1;
    abi.lr = 0x80000u;
    if (g_e8f.fast_insn_limit) lim = g_e8f.fast_insn_limit;
#ifdef GWY_HAVE_UNICORN
    e8x_dump_draw_dims((uc_engine *)uc, r9, "before_2f99d0");
#endif
    printf("[JJFB_E8X_2F99D0_CALL] entry=0x2F99D0 r0=0x%X r1=0x%X r9=0x%X "
           "note=FAST_REAL_PRODUCER_not_product evidence=HYPOTHESIS\n",
           r0_in, r1_in, r9);
    fflush(stdout);
    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, 0x2F99D1u, 0x80000u, lim, &abi, &out);
    if (out.r0_after)
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xF74u, out.r0_after);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xF74u, &f74);
    printf("[JJFB_E8X_2F99D0_DONE] ok=%d end=%s pc_after=0x%X r0_after=0x%X F74=0x%X "
           "note=FAST_ASSIST_not_product evidence=OBSERVED\n",
           ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after, out.r0_after, f74);
#ifdef GWY_HAVE_UNICORN
    e8x_dump_draw_dims((uc_engine *)uc, r9, "after_2f99d0");
#endif
    fflush(stdout);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
}

/* Parse name!W!H.bmp → w/h. Returns 1 on success. */
static int e9a_parse_name_wh(const char *name, int *w, int *h) {
    const char *p, *q;
    int ww, hh;
    if (!name || !w || !h) return 0;
    p = strchr(name, '!');
    if (!p) return 0;
    q = strchr(p + 1, '!');
    if (!q) return 0;
    ww = atoi(p + 1);
    hh = atoi(q + 1);
    if (ww <= 0 || hh <= 0 || ww > 512 || hh > 512) return 0;
    *w = ww;
    *h = hh;
    return 1;
}

/* E9E: after natural 0x304F92 name match, deliver original member bytes into the
 * caller-owned object and return from 0x304BF0 (skip slow DSM seek/read/inflate).
 * NOT entry-bridge: lookup already matched via guest index + host strcmp shim. */
static int e9e_try_postmatch_complete(uc_engine *uc, const char *name) {
    const char *mrp_path;
    MrpArchive *arch = NULL;
    const MrpMember *mem = NULL;
    ByteBuffer bb;
    LauncherError err;
    LauncherStatus st;
    int w = 0, h = 0, i;
    uint32_t handle_va, out_va, lr, slot_va, sz, px, status, ret_pc;
    uint8_t stub[0x20];
    uint8_t digest[32];
    char hex[65];
    char first32[96];
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif
    if (!uc || !name || !name[0] || g_e8f.e9e_postmatch_done) return 0;
    if (!g_e8f.e9e_postmatch_shims) return 0;
    handle_va = g_e8f.e9e_304bf0_handle_va;
    out_va = g_e8f.e9e_304bf0_out_va;
    lr = g_e8f.e9e_304bf0_lr;
    if (!lr) return 0;

    mrp_path = getenv("JJFB_REAL_MRP_PATH");
    if (!mrp_path || !mrp_path[0])
        mrp_path = "game_files/mythroad/320x480/gwy/jjfb.mrp";
    st = mrp_archive_open(mrp_path, &arch, &err);
    if (st != L_OK || !arch) {
        printf("[JJFB_E9E_POSTMATCH_SHIM] role=open_fail path=%s class=POST_MATCH_STILL_BLOCKED_BY_READ "
               "evidence=OBSERVED\n",
               mrp_path);
        fflush(stdout);
        return 0;
    }
    st = mrp_archive_find_exact(arch, name, &mem, &err);
    if (st != L_OK || !mem) {
        printf("[JJFB_E9E_POSTMATCH_SHIM] role=find_fail name=\"%s\" "
               "class=POST_MATCH_STILL_BLOCKED_BY_READ evidence=OBSERVED\n",
               name);
        fflush(stdout);
        mrp_archive_close(arch);
        return 0;
    }
    byte_buffer_init(&bb);
    st = mrp_archive_decode_member(arch, mem, 1024 * 1024, &bb, &err);
    if (st != L_OK || !bb.data || bb.size == 0 || bb.size > (240u * 320u * 2u)) {
        printf("[JJFB_E9E_POSTMATCH_SHIM] role=inflate_fail name=\"%s\" status=%d size=%u "
               "class=POST_MATCH_STILL_BLOCKED_BY_INFLATE evidence=OBSERVED\n",
               name, (int)st, (unsigned)bb.size);
        fflush(stdout);
        byte_buffer_free(&bb);
        mrp_archive_close(arch);
        return 0;
    }
    (void)e9a_parse_name_wh(name, &w, &h);
    if (w <= 0 || h <= 0) {
        w = E8Z_BMP_W;
        h = E8Z_BMP_H;
    }
    gwy_sha256(bb.data, bb.size, digest);
    gwy_sha256_hex(digest, hex);
    memset(first32, 0, sizeof(first32));
    for (i = 0; i < 32 && i < (int)bb.size; i++) {
        static const char *hd = "0123456789abcdef";
        first32[i * 2] = hd[bb.data[i] >> 4];
        first32[i * 2 + 1] = hd[bb.data[i] & 0xF];
    }
#ifdef GWY_HAVE_UNICORN
    if (g_e8f.e9a_pixel_slot == 0) {
        ue = uc_mem_map((uc_engine *)uc, E8Z_PIXEL_BASE, E8Z_PIXEL_MAP_SIZE, UC_PROT_ALL);
        (void)ue;
        ue = uc_mem_map((uc_engine *)uc, E8Y_A64_SCRATCH, E8Y_A64_SCRATCH_SIZE, UC_PROT_ALL);
        (void)ue;
    }
#endif
    slot_va = E8Z_PIXEL_BASE + g_e8f.e9a_pixel_slot;
    {
        uint32_t need = ((uint32_t)bb.size + 0x1FFu) & ~0x1FFu;
        if (need < 0x200u) need = 0x200u;
        if (slot_va + need > E8Z_PIXEL_BASE + E8Z_PIXEL_MAP_SIZE) {
            slot_va = E8Z_PIXEL_BASE;
            g_e8f.e9a_pixel_slot = 0;
        }
        g_e8f.e9a_pixel_slot += need;
    }
    (void)guest_memory_uc_poke((struct uc_struct *)uc, slot_va, bb.data, (int)bb.size);
    if (!handle_va)
        handle_va = E8Y_A64_SCRATCH;
    if (!out_va)
        out_va = handle_va + 4u;
    memset(stub, 0, sizeof(stub));
    sz = (uint32_t)bb.size;
    px = slot_va;
    memcpy(stub + 0, &sz, 4);
    memcpy(stub + 4, &px, 4);
    stub[8] = (uint8_t)(w & 0xFF);
    stub[9] = (uint8_t)((w >> 8) & 0xFF);
    stub[10] = (uint8_t)(h & 0xFF);
    stub[11] = (uint8_t)((h >> 8) & 0xFF);
    stub[16] = 1;
    (void)guest_memory_uc_poke((struct uc_struct *)uc, handle_va, stub, 0x14);
    if (out_va != handle_va + 4u)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, out_va, &px, 4);

    /* Return as if from 0x304BF0 entry: restore SP/R4-R11/R9 then r0=0 → lr.
     * Mid-function PC rewrite without this clobbers 2D92E4 stack (UC_FAULT @0x6). */
    status = 0;
    ret_pc = lr | 1u;
    module_r9_switch_clear_dsm_return_side_stack();
    if (g_e8f.e9e_304bf0_sp) {
        uc_reg_write(uc, UC_ARM_REG_SP, &g_e8f.e9e_304bf0_sp);
        uc_reg_write(uc, UC_ARM_REG_R4, &g_e8f.e9e_304bf0_r4);
        uc_reg_write(uc, UC_ARM_REG_R5, &g_e8f.e9e_304bf0_r5);
        uc_reg_write(uc, UC_ARM_REG_R6, &g_e8f.e9e_304bf0_r6);
        uc_reg_write(uc, UC_ARM_REG_R7, &g_e8f.e9e_304bf0_r7);
        uc_reg_write(uc, UC_ARM_REG_R8, &g_e8f.e9e_304bf0_r8);
        if (g_e8f.e9e_304bf0_r9)
            uc_reg_write(uc, UC_ARM_REG_R9, &g_e8f.e9e_304bf0_r9);
        uc_reg_write(uc, UC_ARM_REG_R10, &g_e8f.e9e_304bf0_r10);
        uc_reg_write(uc, UC_ARM_REG_R11, &g_e8f.e9e_304bf0_r11);
    }
    uc_reg_write(uc, UC_ARM_REG_R0, &status);
    uc_reg_write(uc, UC_ARM_REG_PC, &ret_pc);

    g_e8f.e9e_postmatch_done = 1;
    g_e8f.e8z_pixel_va = slot_va;
    g_e8f.e8z_pixel_bytes = sz;
    g_e8f.e8z_handle_va = handle_va;
    g_e8f.e8z_bw = (uint16_t)w;
    g_e8f.e8z_bh = (uint16_t)h;
    g_e8f.e8z_real_bmp_done = 1;

    printf("[JJFB_E9E_POSTMATCH_SHIM] pc=0x304F92 role=member_bytes_after_natural_match "
           "name=\"%s\" offset=%u stored=%u decoded=%u w=%d h=%d sha256=%s "
           "first32=%s handle=0x%X pixels=0x%X ret=0 "
           "note=original_jjfb_mrp_bytes NOT_BRIDGE NOT_PRODUCT evidence=OBSERVED\n",
           name, mem->offset, mem->stored_size, sz, w, h, hex, first32, handle_va, slot_va);
    printf("[JJFB_E9E_POSTMATCH_SHIM] role=return_abi_restore sp=0x%X r4=0x%X r6=0x%X "
           "r7=0x%X r9=0x%X lr=0x%X class=POST_MATCH_RETURN_ABI_FIXED_NEXT_GAP "
           "evidence=OBSERVED\n",
           g_e8f.e9e_304bf0_sp, g_e8f.e9e_304bf0_r4, g_e8f.e9e_304bf0_r6,
           g_e8f.e9e_304bf0_r7, g_e8f.e9e_304bf0_r9, lr);
    printf("[JJFB_E9E_CLASS] class=NATURAL_POSTMATCH_MEMBER_BYTES name=\"%s\" "
           "blocker_was=POST_MATCH_RETURN_ABI_WRONG evidence=OBSERVED\n",
           name);
    if (g_e8f.e9d_req_csv) {
        fprintf(g_e8f.e9d_req_csv, "%u,%u,0x304BF0,0x%X,\"%s\",1,0,postmatch_complete\n",
                g_e8f.e9d_req_n, g_e8f.tick, lr, name);
        fflush(g_e8f.e9d_req_csv);
    }
    fflush(stdout);
    byte_buffer_free(&bb);
    mrp_archive_close(arch);
    return 1;
}

/* E9A: at 0x304BF0 entry — decode exact member from original jjfb.mrp via mrp_archive.
 *
 * Guest ABI (from 0x2D92E4 @ 0x2D93CC / 0x2D954A):
 *   r0 = package path, r1 = member name, r2 = out size/pixels slot (obj+4),
 *   r3 = out object (JjfbBmpObjHdr: +0 size, +4 pixels, +8 w, +0xa h)
 *   return r0 == 0 success, r0 == -1 miss/fail (NOT a handle pointer).
 * 0x2D92E4 then returns the object in r4 when r5 (304BF0 status) == 0.
 */
static int e9a_try_member_bridge(uc_engine *uc, uint32_t name_va, uint32_t handle_va,
                                 uint32_t out_pixels_va, uint32_t lr) {
    char name[96];
    const char *mrp_path;
    MrpArchive *arch = NULL;
    const MrpMember *mem = NULL;
    ByteBuffer bb;
    LauncherError err;
    LauncherStatus st;
    int w = 0, h = 0;
    uint32_t slot_va, ret_pc, sz, px, status;
    uint8_t stub[0x20];
    uint8_t digest[32];
    char hex[65];
    int i;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif
    g_e8f.e9a_bridge_n++;
    memset(name, 0, sizeof(name));
    if (!uc || !name_va) return 0;
    for (i = 0; i < (int)sizeof(name) - 1; i++) {
        uint8_t c = 0;
        if (!guest_memory_uc_peek((struct uc_struct *)uc, name_va + (uint32_t)i, &c, 1) || !c)
            break;
        name[i] = (char)c;
    }
    if (!name[0]) {
        printf("[JJFB_REAL_MRP_MEMBER_BRIDGE] miss=empty_name "
               "class=MEMBER_RESOLVE_BLOCKED_BY_NAME evidence=OBSERVED\n");
        fflush(stdout);
        return 0;
    }
    mrp_path = getenv("JJFB_REAL_MRP_PATH");
    if (!mrp_path || !mrp_path[0])
        mrp_path = "game_files/mythroad/320x480/gwy/jjfb.mrp";
    st = mrp_archive_open(mrp_path, &arch, &err);
    if (st != L_OK || !arch) {
        printf("[JJFB_REAL_MRP_MEMBER_BRIDGE] open_fail path=%s status=%d "
               "class=MEMBER_RESOLVE_BLOCKED_BY_PACKAGE_CONTEXT evidence=OBSERVED\n",
               mrp_path, (int)st);
        fflush(stdout);
        return 0;
    }
    st = mrp_archive_find_exact(arch, name, &mem, &err);
    if (st != L_OK || !mem) {
        uint32_t fail = 0xFFFFFFFFu;
        uint32_t ret = lr | 1u;
        g_e8f.e9a_bridge_miss_n++;
        printf("[JJFB_REAL_MRP_MEMBER_BRIDGE] miss name=\"%s\" path=%s "
               "class=MEMBER_RESOLVE_BLOCKED_BY_NAME note=fail_fast_r0=-1 evidence=OBSERVED\n",
               name, mrp_path);
        fflush(stdout);
        uc_reg_write(uc, UC_ARM_REG_R0, &fail);
        uc_reg_write(uc, UC_ARM_REG_PC, &ret);
        mrp_archive_close(arch);
        return 1; /* consumed; avoid guest stall */
    }
    byte_buffer_init(&bb);
    st = mrp_archive_decode_member(arch, mem, 1024 * 1024, &bb, &err);
    /* E9C: allow large UI members (slogo ~18KB); keep a hard cap at full LCD RGB565. */
    if (st != L_OK || !bb.data || bb.size == 0 || bb.size > (240u * 320u * 2u)) {
        uint32_t fail = 0xFFFFFFFFu;
        uint32_t ret = lr | 1u;
        g_e8f.e9a_bridge_miss_n++;
        printf("[JJFB_REAL_MRP_MEMBER_BRIDGE] decode_fail name=\"%s\" status=%d size=%u "
               "class=MEMBER_RESOLVE_BLOCKED_BY_READ_CALLBACK evidence=OBSERVED\n",
               name, (int)st, (unsigned)bb.size);
        fflush(stdout);
        byte_buffer_free(&bb);
        mrp_archive_close(arch);
        uc_reg_write(uc, UC_ARM_REG_R0, &fail);
        uc_reg_write(uc, UC_ARM_REG_PC, &ret);
        return 1;
    }
    (void)e9a_parse_name_wh(name, &w, &h);
    if (w <= 0 || h <= 0) {
        w = E8Z_BMP_W;
        h = E8Z_BMP_H;
    }
    gwy_sha256(bb.data, bb.size, digest);
    gwy_sha256_hex(digest, hex);
#ifdef GWY_HAVE_UNICORN
    if (g_e8f.e9a_pixel_slot == 0) {
        ue = uc_mem_map((uc_engine *)uc, E8Z_PIXEL_BASE, E8Z_PIXEL_MAP_SIZE, UC_PROT_ALL);
        if (ue != UC_ERR_OK && ue != UC_ERR_MAP)
            printf("[JJFB_REAL_MRP_MEMBER_BRIDGE] pixel_map uc_err=%u evidence=OBSERVED\n",
                   (unsigned)ue);
        ue = uc_mem_map((uc_engine *)uc, E8Y_A64_SCRATCH, E8Y_A64_SCRATCH_SIZE, UC_PROT_ALL);
        (void)ue;
    }
#endif
    slot_va = E8Z_PIXEL_BASE + g_e8f.e9a_pixel_slot;
    /* advance by rounded-up 512B blocks so large UI members do not overlap */
    {
        uint32_t need = ((uint32_t)bb.size + 0x1FFu) & ~0x1FFu;
        if (need < 0x200u) need = 0x200u;
        if (slot_va + need > E8Z_PIXEL_BASE + E8Z_PIXEL_MAP_SIZE) {
            slot_va = E8Z_PIXEL_BASE;
            g_e8f.e9a_pixel_slot = 0;
        }
        g_e8f.e9a_pixel_slot += need;
    }
    (void)guest_memory_uc_poke((struct uc_struct *)uc, slot_va, bb.data, (int)bb.size);
    if (!handle_va)
        handle_va = E8Y_A64_SCRATCH + ((g_e8f.e9a_bridge_hit_n % 6u) * 0x40u);
    if (!out_pixels_va)
        out_pixels_va = handle_va + 4u;
    memset(stub, 0, sizeof(stub));
    sz = (uint32_t)bb.size;
    px = slot_va;
    /* Preserve guest-filled w/h if already present; else write from name. */
    {
        uint16_t gw = 0, gh = 0;
        (void)guest_memory_uc_peek((struct uc_struct *)uc, handle_va + 8u, (uint8_t *)&gw, 2);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, handle_va + 10u, (uint8_t *)&gh, 2);
        if (gw > 0 && gw <= 512) w = (int)gw;
        if (gh > 0 && gh <= 512) h = (int)gh;
    }
    memcpy(stub + 0, &sz, 4);
    memcpy(stub + 4, &px, 4);
    stub[8] = (uint8_t)(w & 0xFF);
    stub[9] = (uint8_t)((w >> 8) & 0xFF);
    stub[10] = (uint8_t)(h & 0xFF);
    stub[11] = (uint8_t)((h >> 8) & 0xFF);
    stub[16] = 1;
    (void)guest_memory_uc_poke((struct uc_struct *)uc, handle_va, stub, 0x14);
    if (out_pixels_va != handle_va + 4u)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, out_pixels_va, &px, 4);
    /* ABI: success status 0 — object stays in caller's r4; 2D92E4 returns r4. */
    status = 0;
    ret_pc = lr | 1u;
    uc_reg_write(uc, UC_ARM_REG_R0, &status);
    uc_reg_write(uc, UC_ARM_REG_PC, &ret_pc);
    g_e8f.e9a_bridge_hit_n++;
    g_e8f.e8z_pixel_va = slot_va;
    g_e8f.e8z_pixel_bytes = sz;
    g_e8f.e8z_handle_va = handle_va;
    g_e8f.e8z_bw = (uint16_t)w;
    g_e8f.e8z_bh = (uint16_t)h;
    g_e8f.e8z_real_bmp_done = 1;
    printf("[JJFB_REAL_MRP_MEMBER_BRIDGE] hit=%u name=\"%s\" bytes=%u sha256=%s "
           "handle=0x%X pixels=0x%X w=%d h=%d stored=%u status_r0=0 "
           "level=REAL_MEMBER_BRIDGE note=original_jjfb_mrp_decode NOT_PRODUCT "
           "evidence=OBSERVED\n",
           g_e8f.e9a_bridge_hit_n, name, sz, hex, handle_va, slot_va, w, h,
           mem->stored_size);
    printf("[JJFB_E9A_CLASS] class=REAL_MEMBER_BRIDGE_FIRST_FRAME name=\"%s\" "
           "evidence=OBSERVED\n",
           name);
    fflush(stdout);
    byte_buffer_free(&bb);
    mrp_archive_close(arch);
    return 1;
}

/* E8Y/E8Z: A58/A5C/A60/A64/A68/A6C handles (0x14-byte layout from 0x2D92E4).
 * Structural-only when FAST_A64; with FAST_REAL_BMP_HANDLE, +4 = real RGB565
 * pixels from original jjfb.mrp member wy_jiao1!11!11.bmp (not invented). */
static int e8z_load_host_rgb565(uint8_t *dst, int cap, int *out_n) {
    const char *path = getenv("JJFB_E8Z_BMP_PATH");
    FILE *fp;
    long sz;
    if (!dst || cap < E8Z_BMP_BYTES) return 0;
    if (!path || !path[0])
        path = "out/e8z_resources/wy_jiao1_11_11.bmp";
    fp = fopen(path, "rb");
    if (!fp) {
        printf("[JJFB_E8Z_BMP_LOAD] open_fail path=%s evidence=OBSERVED\n", path);
        fflush(stdout);
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz != E8Z_BMP_BYTES) {
        printf("[JJFB_E8Z_BMP_LOAD] size_mismatch path=%s got=%ld expect=%d "
               "evidence=OBSERVED\n",
               path, sz, E8Z_BMP_BYTES);
        fclose(fp);
        return 0;
    }
    if ((int)fread(dst, 1, (size_t)E8Z_BMP_BYTES, fp) != E8Z_BMP_BYTES) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    if (out_n) *out_n = E8Z_BMP_BYTES;
    printf("[JJFB_E8Z_BMP_LOAD] path=%s bytes=%d w=%d h=%d "
           "note=real_jjfb_mrp_member evidence=OBSERVED\n",
           path, E8Z_BMP_BYTES, E8Z_BMP_W, E8Z_BMP_H);
    fflush(stdout);
    return 1;
}

static void e8y_install_a64_assist(void *uc, uint32_t r9) {
    static const uint32_t k_offs[] = {0xA58u, 0xA5Cu, 0xA60u, 0xA64u, 0xA68u, 0xA6Cu};
    uint32_t a64 = 0, a58 = 0;
    int i;
    int real = g_e8f.e8z_real_bmp;
    uint8_t pixels[E8Z_BMP_BYTES];
    int pix_n = 0;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif
    if (!uc || !r9 || g_e8f.e8y_a64_assist_done) return;
    g_e8f.e8y_a64_assist_done = 1;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA64u, &a64);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA58u, &a58);
    if (real) {
        if (!e8z_load_host_rgb565(pixels, (int)sizeof(pixels), &pix_n)) {
            printf("[JJFB_FAST_REAL_BMP_HANDLE] load_fail falling_back=structural_only "
                   "evidence=OBSERVED\n");
            real = 0;
        }
    }
    printf("[JJFB_FAST_A64_RESOURCE_ASSIST] before A58=0x%X A64=0x%X real_bmp=%d "
           "note=%s evidence=HYPOTHESIS\n",
           a58, a64, real,
           real ? "real_pixels_from_jjfb_mrp" : "structural_handles_no_pixels");
#ifdef GWY_HAVE_UNICORN
    ue = uc_mem_map((uc_engine *)uc, E8Y_A64_SCRATCH, E8Y_A64_SCRATCH_SIZE, UC_PROT_ALL);
    if (ue != UC_ERR_OK)
        printf("[JJFB_FAST_A64_RESOURCE_ASSIST] map_status uc_err=%u evidence=OBSERVED\n",
               (unsigned)ue);
    if (real) {
        ue = uc_mem_map((uc_engine *)uc, E8Z_PIXEL_BASE, E8Z_PIXEL_MAP_SIZE, UC_PROT_ALL);
        if (ue != UC_ERR_OK)
            printf("[JJFB_FAST_REAL_BMP_HANDLE] pixel_map uc_err=%u evidence=OBSERVED\n",
                   (unsigned)ue);
        (void)guest_memory_uc_poke((struct uc_struct *)uc, E8Z_PIXEL_BASE, pixels, pix_n);
        g_e8f.e8z_pixel_va = E8Z_PIXEL_BASE;
        g_e8f.e8z_pixel_bytes = (uint32_t)pix_n;
        g_e8f.e8z_bw = E8Z_BMP_W;
        g_e8f.e8z_bh = E8Z_BMP_H;
        g_e8f.e8z_real_bmp_done = 1;
    }
#endif
    for (i = 0; i < 6; i++) {
        uint32_t slot = E8Y_A64_SCRATCH + (uint32_t)i * 0x40u;
        uint8_t stub[0x20];
        memset(stub, 0, sizeof(stub));
        if (real) {
            uint32_t sz = (uint32_t)pix_n;
            uint32_t px = E8Z_PIXEL_BASE;
            memcpy(stub + 0, &sz, 4);
            memcpy(stub + 4, &px, 4);
            stub[8] = (uint8_t)(E8Z_BMP_W & 0xFF);
            stub[9] = (uint8_t)((E8Z_BMP_W >> 8) & 0xFF);
            stub[10] = (uint8_t)(E8Z_BMP_H & 0xFF);
            stub[11] = (uint8_t)((E8Z_BMP_H >> 8) & 0xFF);
            stub[16] = 1; /* flag */
            memcpy(stub + 0x18, &sz, 4);
            if (i == 0) g_e8f.e8z_handle_va = slot;
        } else {
            static const uint16_t k_wh[][2] = {
                {11, 11}, {11, 11}, {10, 10}, {11, 11}, {11, 11}, {10, 10}};
            stub[8] = (uint8_t)(k_wh[i][0] & 0xFF);
            stub[9] = (uint8_t)((k_wh[i][0] >> 8) & 0xFF);
            stub[10] = (uint8_t)(k_wh[i][1] & 0xFF);
            stub[11] = (uint8_t)((k_wh[i][1] >> 8) & 0xFF);
        }
        (void)guest_memory_uc_poke((struct uc_struct *)uc, slot, stub, real ? 0x20 : 0x14);
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + k_offs[i], slot);
    }
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA64u, &a64);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xA58u, &a58);
    if (real) {
        printf("[JJFB_FAST_REAL_BMP_HANDLE] after A58=0x%X A64=0x%X pixels=0x%X "
               "bytes=%u w=%u h=%u note=handle_plus4_nonzero NOT_PRODUCT evidence=OBSERVED\n",
               a58, a64, g_e8f.e8z_pixel_va, g_e8f.e8z_pixel_bytes, g_e8f.e8z_bw,
               g_e8f.e8z_bh);
    } else {
        printf("[JJFB_FAST_A64_RESOURCE_ASSIST] after A58=0x%X A5C/A60/A64 set scratch=0x%X "
               "note=mode10_uses_A58_A5C_A60 evidence=OBSERVED\n",
               a58, E8Y_A64_SCRATCH);
    }
    fflush(stdout);
}

static void e8w_reenter_e88cc(void *uc, uint32_t r9) {
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    uint32_t r9_save = 0;
    int ok;
    uint64_t lim = 300000ull;
    if (!uc || !r9 || g_e8f.e8w_reenter_done) return;
    g_e8f.e8w_reenter_done = 1;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9);
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = 0;
    abi.set_r1 = 1;
    abi.r1 = 0;
    abi.set_r2 = 1;
    abi.r2 = 0;
    abi.set_r3 = 1;
    abi.r3 = 0;
    abi.set_lr = 1;
    abi.lr = 0x80000u;
    if (g_e8f.fast_insn_limit) lim = g_e8f.fast_insn_limit;
    e8v_dump_e88cc_context((uc_engine *)uc, r9, 0, "before_reenter");
    printf("[JJFB_E8W_REENTER_E88CC] entry=0x2E88CC r9=0x%X F74=0x%X "
           "note=FAST_ASSIST_not_product evidence=HYPOTHESIS\n",
           r9, g_e8f.e8w_f74_assist);
    fflush(stdout);
    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, 0x2E88CDu, 0x80000u, lim, &abi, &out);
    printf("[JJFB_E8W_REENTER_DONE] ok=%d end=%s pc_after=0x%X draw_cand=%u "
           "note=FAST_ASSIST_not_product evidence=OBSERVED\n",
           ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after, g_e8f.e8v_draw_cand_n);
    e8v_dump_e88cc_context((uc_engine *)uc, r9, out.lr_after, "after_reenter");
    fflush(stdout);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
}

/* E8U: call real upstream UI dispatcher 0x2E2520 with captured object (not mid-fn 0x2E2F50). */
static void fast_call_ui_upstream(void *uc) {
    uint32_t r9_save = 0, r9_run = 0;
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    int ok;
    uint64_t lim = 500000ull;
    uint32_t entry;
    if (!g_e8f.fast_assist || !g_e8f.fast_ui_upstream || g_e8f.fast_ui_upstream_done || !uc)
        return;
    g_e8f.fast_ui_upstream_done = 1;
    if (!g_e8f.ui_obj_set || !g_e8f.ui_obj_r0) {
        printf("[JJFB_FAST_UI_UPSTREAM_SKIP] reason=no_ui_object upstream=%d "
               "note=ABI_unsafe_without_object evidence=OBSERVED\n",
               g_e8f.fast_ui_upstream);
        fflush(stdout);
        return;
    }
    /* 0x2E2F50 is mid-function BL site — only safe to call 0x2E2520 entry. */
    if (g_e8f.fast_ui_upstream == 2) {
        printf("[JJFB_FAST_UI_UPSTREAM] note=2E2F50_is_mid_fn_redirect_to_2E2520 "
               "obj=0x%X evidence=HYPOTHESIS\n",
               g_e8f.ui_obj_r0);
        entry = 0x2E2521u;
    } else {
        entry = 0x2E2521u;
    }
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    if (!find_robotol_r9(uc, &r9_run)) r9_run = r9_save;
    if (r9_run) (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_run);
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = g_e8f.ui_obj_r0;
    abi.set_r1 = 1;
    abi.r1 = 0;
    abi.set_r2 = 1;
    abi.r2 = 0;
    abi.set_r3 = 1;
    abi.r3 = 0;
    abi.set_lr = 1;
    abi.lr = 0x80000u;
    if (g_e8f.fast_insn_limit) lim = g_e8f.fast_insn_limit;
    printf("[JJFB_FAST_UI_UPSTREAM_CALL] entry=0x2E2520 r0=0x%X r9=0x%X "
           "note=FAST_ASSIST_real_fn evidence=HYPOTHESIS\n",
           abi.r0, r9_run);
    fflush(stdout);
    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, entry, 0x80000u, lim, &abi, &out);
    printf("[JJFB_FAST_UI_UPSTREAM_DONE] ok=%d end=%s pc_after=0x%X "
           "note=FAST_ASSIST_not_product evidence=OBSERVED\n",
           ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after);
    fflush(stdout);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
}

/* E8S/E8T: invoke real UI-init 0x2E4788. Static: rejects state in {38,46,69,252,300};
 * ED8 gate is CMP #0; BGT early-out — continue only if ED8<=0 (typically 0).
 * Also needs C8E==0 and CA3==1 among other gates. */
static void fast_call_ui_init(void *uc) {
    uint32_t r9_save = 0, r9_run = 0, state_before = 0, state_after = 0;
    uint8_t c44b = 0, c9db = 0, cf5b = 0, ca3b = 0;
    uint32_t ed8 = 0;
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    int ok;
    uint64_t lim = 500000ull;
    if (!g_e8f.fast_assist || !g_e8f.fast_ui_init_call || g_e8f.fast_ui_init_done || !uc)
        return;
    g_e8f.fast_ui_init_done = 1;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    if (!find_robotol_r9(uc, &r9_run)) r9_run = r9_save;
    if (r9_run) (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_run);
    if (r9_run) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9_run + E8I_STATE_OFF, &state_before);
        if (g_e8f.fast_ui_state_set) {
            (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9_run + E8I_STATE_OFF,
                                           g_e8f.fast_ui_state);
            printf("[JJFB_FAST_UI_STATE] old=0x%X new=0x%X note=FAST_ASSIST_UI_init_rejects_38 "
                   "evidence=HYPOTHESIS\n",
                   state_before, g_e8f.fast_ui_state);
            state_before = g_e8f.fast_ui_state;
        }
        if (g_e8f.fast_ui_ed8_set) {
            uint32_t oldv = 0;
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9_run + 0xED8u, &oldv);
            (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9_run + 0xED8u,
                                           g_e8f.fast_ui_ed8_val);
            printf("[JJFB_FAST_UI_OBJ] field=ED8 old=0x%X new=0x%X note=FAST_ASSIST_not_product "
                   "evidence=HYPOTHESIS\n",
                   oldv, g_e8f.fast_ui_ed8_val);
        }
        if (g_e8f.fast_ui_ca3_set) {
            uint8_t oldb = 0, newb = (uint8_t)(g_e8f.fast_ui_ca3_val & 0xFFu);
            (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xCA3u, &oldb, 1);
            (void)guest_memory_uc_poke((struct uc_struct *)uc, r9_run + 0xCA3u, &newb, 1);
            printf("[JJFB_FAST_UI_OBJ] field=CA3 old=0x%X new=0x%X note=FAST_ASSIST_not_product "
                   "evidence=HYPOTHESIS\n",
                   oldb, newb);
        }
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC44u, &c44b, 1);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC9Du, &c9db, 1);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xCF5u, &cf5b, 1);
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9_run + 0xED8u, &ed8);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xCA3u, &ca3b, 1);
    }
    snap_idle_flags(uc, r9_run, "before_fast_ui_init");
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    /* Never call UI-init with r0=0 — faults at LDR [r0+4]. Prefer captured object. */
    if (g_e8f.ui_obj_set && g_e8f.ui_obj_r0)
        abi.r0 = g_e8f.ui_obj_r0;
    else {
        printf("[JJFB_FAST_UI_INIT_SKIP] reason=no_ui_object note=refuses_r0_0 "
               "evidence=OBSERVED\n");
        fflush(stdout);
        (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
        return;
    }
    abi.set_r1 = 1;
    abi.r1 = 0x146u; /* natural caller 0x2E2F4A sets r1=0xFF+0x47 */
    abi.set_r2 = 1;
    abi.r2 = 0;
    abi.set_r3 = 1;
    abi.r3 = 0;
    abi.set_lr = 1;
    abi.lr = 0x80000u;
    if (g_e8f.fast_insn_limit) lim = g_e8f.fast_insn_limit;
    printf("[JJFB_FAST_UI_INIT_CALL] entry=0x2E4788 r0=0x%X r9=0x%X state=0x%X ED8=0x%X "
           "CA3=0x%X C44=0x%X C9D=0x%X CF5=0x%X note=FAST_ASSIST_real_fn evidence=HYPOTHESIS\n",
           abi.r0, r9_run, state_before, ed8, ca3b, c44b, c9db, cf5b);
    fflush(stdout);
    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, 0x2E4789u, 0x80000u, lim, &abi, &out);
    if (r9_run) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9_run + E8I_STATE_OFF, &state_after);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC44u, &c44b, 1);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xC9Du, &c9db, 1);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9_run + 0xCF5u, &cf5b, 1);
    }
    snap_idle_flags(uc, r9_run, "after_fast_ui_init");
    printf("[JJFB_FAST_UI_INIT_DONE] ok=%d end=%s pc_after=0x%X state=0x%X->0x%X "
           "C44=0x%X C9D=0x%X CF5=0x%X note=FAST_ASSIST_not_product evidence=OBSERVED\n",
           ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after, state_before, state_after,
           c44b, c9db, cf5b);
    if (c9db || cf5b)
        printf("[JJFB_E8S_POST_C44_FLAGS] C9D=0x%X CF5=0x%X via=ui_init note=FAST_ASSIST "
               "evidence=OBSERVED\n",
               c9db, cf5b);
    fflush(stdout);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
}

static void fire_handler_regs(void *uc, uint32_t code, const char *label, uint32_t r0,
                              uint32_t r1, uint32_t r2, uint32_t r3, const char *note) {
    uint32_t handler, stop = 0x80000u;
    uint32_t r9_save = 0, r9_run = 0;
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    int ok;
    handler = platform_handler_registry_get(code);
    if (!handler) {
        printf("[JJFB_E8F_SIBLING_FIRE] code=0x%X label=%s skip=no_handler evidence=OBSERVED\n",
               code, label);
        return;
    }
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    if (!find_robotol_r9(uc, &r9_run)) r9_run = r9_save;
    if (r9_run) (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_run);
    snap_idle_flags(uc, r9_run, "before_sibling");
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = r0;
    abi.set_r1 = 1;
    abi.r1 = r1;
    abi.set_r2 = 1;
    abi.r2 = r2;
    abi.set_r3 = 1;
    abi.r3 = r3;
    abi.set_lr = 1;
    abi.lr = stop;
    printf("[JJFB_E8F_SIBLING_FIRE] code=0x%X label=%s handler=0x%X r0=0x%X r1=0x%X r2=0x%X "
           "r3=0x%X r9=0x%X note=%s evidence=HYPOTHESIS\n",
           code, label, handler, r0, r1, r2, r3, r9_run, note ? note : "observe_only");
    if (code == 0x10102u) {
        printf("[JJFB_E8K_10102_FIRE] case_r0=0x%X handler=0x%X "
               "note=derived_switch_index_observe_only evidence=HYPOTHESIS\n",
               r0, handler);
        printf("[JJFB_E8L_10102_FIRE] case_r0=0x%X r1=0x%X r2=0x%X r3=0x%X handler=0x%X "
               "note=structured_abi_observe_only evidence=HYPOTHESIS\n",
               r0, r1, r2, r3, handler);
    }
    fflush(stdout);
    {
        uint64_t lim = 400000ull;
        if (g_e8f.fast_assist && g_e8f.fast_insn_limit) lim = g_e8f.fast_insn_limit;
        ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, handler, stop, lim, &abi, &out);
    }
    snap_idle_flags(uc, r9_run, "after_sibling");
    printf("[JJFB_E8F_SIBLING_FIRE_DONE] code=0x%X ok=%d end=%s pc_after=0x%X r0_after=0x%X "
           "evidence=OBSERVED\n",
           code, ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after, out.r0_after);
    if (code == 0x10102u) {
        printf("[JJFB_E8K_10102_FIRE_DONE] case_r0=0x%X ok=%d evidence=OBSERVED\n", r0, ok);
        printf("[JJFB_E8L_10102_FIRE_DONE] case_r0=0x%X r1=0x%X ok=%d end=%s pc_after=0x%X "
               "evidence=OBSERVED\n",
               r0, r1, ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after);
    }
    fflush(stdout);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
}

static void fire_handler_r0(void *uc, uint32_t code, const char *label, uint32_t r0,
                            const char *note) {
    fire_handler_regs(uc, code, label, r0, 0, 0, 0, note);
}

static void fire_handler(void *uc, uint32_t code, const char *label) {
    fire_handler_r0(uc, code, label, 0, "observe_only_ZERO_ARGS");
}

static void run_counterfactual(void *uc) {
    uint32_t r9 = 0;
    uint8_t one = 1;
    const char *mode = g_e8f.counterfactual;
    if (!mode[0] || g_e8f.counterfactual_done) return;
    if (!find_robotol_r9(uc, &r9) || !r9) return;
    g_e8f.counterfactual_done = 1;
    printf("[JJFB_E8F_COUNTERFACTUAL] mode=%s r9=0x%X note=COUNTERFACTUAL_ONLY_not_product "
           "evidence=HYPOTHESIS\n",
           mode, r9);
    fflush(stdout);
    snap_idle_flags(uc, r9, "cf_before");
    if (strcmp(mode, "C44") == 0 || strcmp(mode, "ALL") == 0 || strcmp(mode, "C44C9D") == 0 ||
        strcmp(mode, "C44CF5") == 0)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, r9 + 0xC44u, &one, 1);
    if (strcmp(mode, "C9D") == 0 || strcmp(mode, "ALL") == 0 || strcmp(mode, "C44C9D") == 0 ||
        strcmp(mode, "C9DCF5") == 0)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, r9 + 0xC9Du, &one, 1);
    if (strcmp(mode, "CF5") == 0 || strcmp(mode, "ALL") == 0 || strcmp(mode, "C44CF5") == 0 ||
        strcmp(mode, "C9DCF5") == 0)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, r9 + 0xCF5u, &one, 1);
    snap_idle_flags(uc, r9, "cf_after_poke");
    printf("[JJFB_E8F_COUNTERFACTUAL_ARMED] mode=%s note=watch_next_10140_for_new_plat_or_DRAW "
           "evidence=HYPOTHESIS\n",
           mode);
    fflush(stdout);
}

void robotol_flag_writer_trace_dump_summary(const char *reason) {
    int i, hits = 0, miss = 0, caller_hits = 0, caller_miss = 0;
    int disp_hits = 0, disp_miss = 0;
    if (!robotol_flag_writer_trace_enabled()) return;
    for (i = 0; i < g_e8f.nbps; i++) {
        E8fBp *bp = &g_e8f.bps[i];
        if (!bp->used) continue;
        if (bp->hit_n) {
            hits++;
            if (bp->tag[0] == 'c') caller_hits++;
            if (bp->tag[0] == 'd') disp_hits++;
            printf("[JJFB_E8F_WRITER_REACHED] tag=%s pc=0x%X hits=%u first_tick=%u "
                   "class=ENTERED evidence=OBSERVED\n",
                   bp->tag, bp->pc, bp->hit_n, bp->first_tick);
        } else {
            miss++;
            if (bp->tag[0] == 'c') caller_miss++;
            if (bp->tag[0] == 'd') disp_miss++;
            printf("[JJFB_E8F_WRITER_NEVER] tag=%s pc=0x%X class=NEVER_ENTERED "
                   "evidence=OBSERVED\n",
                   bp->tag, bp->pc);
        }
    }
    printf("[JJFB_E8F_WRITER_SUMMARY] reason=%s armed=%d hit_pcs=%d never_pcs=%d "
           "caller_hit=%d caller_never=%d longpath_hits=%u fault_hits=%u evidence=OBSERVED\n",
           reason ? reason : "-", g_e8f.armed, hits, miss, caller_hits, caller_miss,
           g_e8f.longpath_hits, g_e8f.fault_hits);
    printf("[JJFB_E8H_DISPATCHER_SUMMARY] reason=%s disp_hit=%d disp_never=%d svc_hits=%u "
           "svc_trapped=%d cf=%s evidence=OBSERVED\n",
           reason ? reason : "-", disp_hits, disp_miss, g_e8f.svc_hits, g_e8f.svc_trapped,
           g_e8f.counterfactual[0] ? g_e8f.counterfactual : "NONE");
    printf("[JJFB_E8I_PARENT_SUMMARY] reason=%s parent_hits=%u state_watch_armed=%d "
           "state_writes=%u state_last=0x%X state_last_writer=0x%X evidence=OBSERVED\n",
           reason ? reason : "-", g_e8f.parent_hit_n, g_e8f.state_watch_armed, g_e8f.state_writes,
           g_e8f.state_last_val, g_e8f.state_last_writer);
    printf("[JJFB_E8J_SUMMARY] reason=%s entry_hits=%u upstream_hits=%u queue_bp_hits=%u "
           "bl_hits=%u qread_hits=%u qread_armed=%d evidence=OBSERVED\n",
           reason ? reason : "-", g_e8f.e8j_entry_hits, g_e8f.e8j_upstream_hits,
           g_e8f.e8j_queue_hits, g_e8f.e8j_bl_hits, g_e8f.e8j_qread_hits, g_e8f.e8j_qread_armed);
    fflush(stdout);
}

void robotol_flag_writer_trace_on_lifecycle_fault(void *uc, uint32_t tick, int ok,
                                                  unsigned uc_err, uint32_t pc_after,
                                                  uint32_t r0_after, uint32_t r9_after,
                                                  uint32_t sp_after, uint32_t lr_after) {
    uint8_t peek[16];
    uint32_t r9 = r9_after;
    int i;
    if (!robotol_flag_writer_trace_enabled()) return;
    if (ok || g_e8f.fault_dumped) return;
    if (!g_e8f.fault_watch && !g_e8f.counterfactual_done && !g_e8f.svc_trap) return;
    g_e8f.fault_dumped = 1;
    printf("[JJFB_E8G_SECOND_GATE_FAULT] tick=%u uc_err=%u pc=0x%X r0=0x%X r9=0x%X sp=0x%X "
           "lr=0x%X note=COUNTERFACTUAL_ONLY evidence=OBSERVED\n",
           tick, uc_err, pc_after, r0_after, r9_after, sp_after, lr_after);
    if (uc && guest_memory_uc_peek((struct uc_struct *)uc, pc_after & ~1u, peek, sizeof(peek))) {
        printf("[JJFB_E8G_FAULT_BYTES] pc=0x%X bytes=", pc_after & ~1u);
        for (i = 0; i < (int)sizeof(peek); i++)
            printf("%02X", peek[i]);
        printf(" evidence=OBSERVED\n");
    }
    if (uc && find_robotol_r9(uc, &r9))
        snap_idle_flags(uc, r9, "fault_context");
    if ((pc_after & ~1u) == 0x2D92B0u || (pc_after & ~1u) == 0x2D92AEu) {
        printf("[JJFB_E8G_FAULT_CLASS] site=0x%X class=SVC_AB_UNIMPLEMENTED "
               "uc_err=%u r0=0x%X hypothesis=missing_host_SVC_0xAB_handler "
               "evidence=TARGET_OBSERVED+HYPOTHESIS\n",
               pc_after & ~1u, uc_err, r0_after);
    } else {
        printf("[JJFB_E8G_FAULT_CLASS] site=0x%X class=OTHER_PATH uc_err=%u "
               "evidence=OBSERVED\n",
               pc_after & ~1u, uc_err);
    }
    fflush(stdout);
}

void robotol_flag_writer_trace_on_lifecycle(void *uc, uint32_t tick) {
    uint32_t r9 = 0;
    if (!robotol_flag_writer_trace_enabled()) return;
    if (!uc) uc = g_e8f.uc;
    g_e8f.tick = tick;
    robotol_flag_writer_trace_try_arm(uc);
    e8i_try_arm_state_watch(uc);
    e8j_try_arm_queue_read_watch(uc);

    if (tick == 1u && find_robotol_r9(uc, &r9)) {
        uint32_t depth = 0;
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x844u, &depth);
        g_e8f.queue_depth_at_probe = depth;
        printf("[JJFB_E8F_QUEUE_DEPTH] r9=0x%X off=0x844 depth=0x%X "
               "note=long_path_if_depth_le_0 evidence=TARGET_OBSERVED\n",
               r9, depth);
        snap_idle_flags(uc, r9, "tick1");
        fflush(stdout);
    }

    if (tick == 1u && g_e8f.sibling_probe && !g_e8f.sibling_done) {
        g_e8f.sibling_done = 1;
        if (strcmp(g_e8f.sibling_mode, "10162") == 0 || strcmp(g_e8f.sibling_mode, "BOTH") == 0) {
            if (!g_e8f.probe10162_done) {
                g_e8f.probe10162_done = 1;
                fire_handler(uc, 0x10162u, "10162");
            }
        }
        if (strcmp(g_e8f.sibling_mode, "10102") == 0 || strcmp(g_e8f.sibling_mode, "BOTH") == 0) {
            if (!g_e8f.probe10102_done) {
                g_e8f.probe10102_done = 1;
                fire_handler(uc, 0x10102u, "10102");
            }
        }
    }

    /* E8M/E8N: ordered sequence / CF state ladder / single 10102 fire. */
    if (tick == 1u && !g_e8f.e8m_seq_done &&
        (g_e8f.e8m_seq[0] || g_e8f.e8k_10102_case || g_e8f.e8n_cf_state_set)) {
        g_e8f.e8m_seq_done = 1;
        g_e8f.e8k_10102_done = 1;
        g_e8f.e8m_insn_n = 0;
        if (g_e8f.e8m_seq[0]) {
            printf("[JJFB_E8M_SEQ] seq=%s do_10165=%d do_310=%d case156_r1=0x%X "
                   "note=observe_only_not_product evidence=HYPOTHESIS\n",
                   g_e8f.e8m_seq, g_e8f.e8m_seq_10165, g_e8f.e8m_seq_310, g_e8f.e8l_10102_r1);
            fflush(stdout);
        }
        if (g_e8f.e8m_seq_10165) {
            /* Reuse E8E structured 10165 probe (sets FE8/B7D when ABI matches). */
            robotol_idle_watch_try_10165_probe(uc);
            if (find_robotol_r9(uc, &r9)) snap_idle_flags(uc, r9, "after_seq_10165");
        }
        if (g_e8f.e8m_seq_310) {
            fire_handler_regs(uc, 0x10102u, "e8m_case310", 310u, 0, 0, 0,
                              "E8M_seq_case310_observe_only_not_product");
            if (find_robotol_r9(uc, &r9)) snap_idle_flags(uc, r9, "after_seq_310");
        }
        /* E8N: COUNTERFACTUAL_ONLY poke of R9+0x8D0 before case156 — not product success. */
        if (g_e8f.e8n_cf_state_set && !g_e8f.e8n_cf_done && find_robotol_r9(uc, &r9) && r9) {
            uint32_t addr = r9 + E8I_STATE_OFF;
            uint32_t oldv = 0;
            g_e8f.e8n_cf_done = 1;
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &oldv);
            (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, addr, g_e8f.e8n_cf_state);
            g_e8f.state_last_val = g_e8f.e8n_cf_state;
            printf("[JJFB_E8N_CF_STATE] old=0x%X new=0x%X addr=0x%X "
                   "note=COUNTERFACTUAL_ONLY_not_product evidence=HYPOTHESIS\n",
                   oldv, g_e8f.e8n_cf_state, addr);
            if (g_e8f.fast_assist) {
                printf("[JJFB_FAST_STATE] old=0x%X new=0x%X addr=0x%X "
                       "note=FAST_ASSIST_not_product evidence=HYPOTHESIS\n",
                       oldv, g_e8f.e8n_cf_state, addr);
            }
            snap_idle_flags(uc, r9, g_e8f.fast_assist ? "after_fast_state" : "after_cf_state");
            fflush(stdout);
            if (g_e8f.fast_assist) fast_poke_c44_dep_fields(uc, r9);
            if (!g_e8f.e8k_10102_case) {
                g_e8f.e8k_10102_case = 156u;
                g_e8f.e8l_10102_r1 = 18u;
                g_e8f.e8l_regs_set = 1;
            }
        } else if (g_e8f.fast_assist && find_robotol_r9(uc, &r9) && r9) {
            /* Field poke without state poke still allowed under FAST_ASSIST. */
            fast_poke_c44_dep_fields(uc, r9);
        }
        /* E8R: real unlock fn before case156 (default). */
        if (g_e8f.fast_unlock_call && strcmp(g_e8f.fast_unlock_when, "after") != 0)
            fast_call_c44_unlock(uc, "before_case156");
        /* E8S: UI-init real call (often after unlock; state=38 is rejected by UI-init). */
        if (g_e8f.fast_ui_upstream)
            fast_call_ui_upstream(uc);
        if (g_e8f.fast_ui_init_call)
            fast_call_ui_init(uc);
        if (g_e8f.e8k_10102_case) {
            fire_handler_regs(uc, 0x10102u, "e8m_case", g_e8f.e8k_10102_case, g_e8f.e8l_10102_r1,
                              g_e8f.e8l_10102_r2, g_e8f.e8l_10102_r3,
                              g_e8f.fast_assist
                                  ? "E8O_FAST_ASSIST_not_product"
                                  : (g_e8f.e8n_cf_state_set
                                         ? "E8N_CF_state_ladder_observe_only_not_product"
                                         : (g_e8f.e8m_seq[0]
                                                ? "E8M_seq_case156_observe_only_not_product"
                                                : (g_e8f.e8l_regs_set
                                                       ? "E8L_structured_abi_observe_only_not_product"
                                                       : "E8K_derived_case_observe_only_not_product"))));
            /* E8R: unlock after case156 success arm (before FIRE_DONE log so stop-watch can miss race). */
            if (g_e8f.fast_unlock_call && strcmp(g_e8f.fast_unlock_when, "after") == 0)
                fast_call_c44_unlock(uc, "after_case156");
            if (g_e8f.fast_assist) {
                printf("[JJFB_FAST_FIRE_DONE] case156_r1=0x%X state=0x%X svc_hits=%u "
                       "svc_continues=%u note=FAST_ASSIST_not_product evidence=OBSERVED\n",
                       g_e8f.e8l_10102_r1, g_e8f.e8n_cf_state, g_e8f.svc_hits,
                       g_e8f.fast_svc_continues);
            }
            printf("[JJFB_E8M_PARENT_TRACE_SUMMARY] insn_logged=%u path_hits=%u evidence=OBSERVED\n",
                   g_e8f.e8m_insn_n, g_e8f.e8m_path_hits);
            fflush(stdout);
        }
    }

    if (tick == 1u && g_e8f.counterfactual[0])
        run_counterfactual(uc);

    if (tick == 2u && g_e8f.counterfactual_done && find_robotol_r9(uc, &r9))
        snap_idle_flags(uc, r9, "cf_after_next_10140");

    /* E8S: post-C44 persistence into next 10140 tick. */
    if (tick == 2u && g_e8f.fast_assist && (g_e8f.fast_unlock_done || g_e8f.fast_ui_init_done) &&
        find_robotol_r9(uc, &r9) && r9) {
        uint8_t c44 = 0, c9d = 0, cf5 = 0;
        uint32_t st = 0;
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC44u, &c44, 1);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC9Du, &c9d, 1);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCF5u, &cf5, 1);
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + E8I_STATE_OFF, &st);
        snap_idle_flags(uc, r9, "e8s_tick2_post_c44");
        printf("[JJFB_E8S_POST_C44_TICK2] C44=0x%X C9D=0x%X CF5=0x%X state=0x%X "
               "note=persistence_check evidence=OBSERVED\n",
               c44, c9d, cf5, st);
        if (g_e8f.display_first) {
            printf("[JJFB_E8U_DISPLAY_FIRST_TICK2] C44=0x%X C9D=0x%X CF5=0x%X "
                   "c9d_assist=%u cf5_assist=%u idle_success=%u ui_obj=0x%X "
                   "note=NOT_PRODUCT_SUCCESS evidence=OBSERVED\n",
                   c44, c9d, cf5, g_e8f.c9d_assist_n, g_e8f.cf5_assist_n,
                   g_e8f.idle_success_n, g_e8f.ui_obj_r0);
        }
        if (g_e8f.e8v_mode) {
            printf("[JJFB_E8V_TICK2] e88cc_entries=%u early_exit=%u draw_cand=%u "
                   "insn=%u bl=%u F6C=0x%X p4=0x%X p8=0x%X D14_s16=%d "
                   "note=NOT_PRODUCT_SUCCESS evidence=OBSERVED\n",
                   g_e8f.e8v_e88cc_entries, g_e8f.e8v_early_exit_n, g_e8f.e8v_draw_cand_n,
                   g_e8f.e8v_insn_n, g_e8f.e8v_bl_n, g_e8f.e8v_f6c_base, g_e8f.e8v_f6c_p4,
                   g_e8f.e8v_f6c_p8, (int)g_e8f.e8v_d14_s16);
        }
        fflush(stdout);
    }

    /* E8W: after first idle success, install F74 structural assist and re-enter 0x2E88CC. */
    if (g_e8f.e8w_mode && g_e8f.idle_success_n > 0u && tick >= 1u &&
        find_robotol_r9(uc, &r9) && r9) {
        if (g_e8f.e8w_f6c_assist && !g_e8f.e8w_f6c_assist_done)
            e8w_install_f6c_assist(uc, r9);
        /* E8X: dims before re-enter so 0x2F9970 returns nonzero width into r2. */
        if (g_e8f.e8x_mode && g_e8f.e8x_f74_desc_assist && !g_e8f.e8x_f74_desc_assist_done)
            e8x_install_f74_desc_assist(uc, r9);
        if (g_e8f.e8y_mode && g_e8f.e8y_a64_assist && !g_e8f.e8y_a64_assist_done)
            e8y_install_a64_assist(uc, r9);
        if (g_e8f.e8x_mode && g_e8f.e8x_call_2f99d0 && !g_e8f.e8x_call_2f99d0_done)
            e8x_call_2f99d0(uc, r9);
        if (g_e8f.e8w_reenter_e88cc && !g_e8f.e8w_reenter_done &&
            (g_e8f.e8w_f6c_assist_done || g_e8f.e8v_f6c_p8 || g_e8f.e8v_f6c_p4 ||
             g_e8f.e8x_call_2f99d0_done || g_e8f.e8y_a64_assist_done))
            e8w_reenter_e88cc(uc, r9);
        if (tick == 2u || tick == 30u) {
            printf("[JJFB_E8W_SUMMARY] reason=tick_%u writer_hits=%u assist=%u reenter=%u "
                   "draw_cand=%u F70=0x%X F74=0x%X idle_success=%u evidence=OBSERVED\n",
                   tick, g_e8f.e8w_writer_hit_n, g_e8f.e8w_f6c_assist_done,
                   g_e8f.e8w_reenter_done, g_e8f.e8v_draw_cand_n, g_e8f.e8v_f6c_p4,
                   g_e8f.e8v_f6c_p8, g_e8f.idle_success_n);
            if (g_e8f.e8x_mode) {
                printf("[JJFB_E8X_SUMMARY] reason=tick_%u n2854=%u n188=%u n449c=%u n310=%u "
                       "insn=%u desc=%u prod=%u evidence=OBSERVED\n",
                       tick, g_e8f.e8x_2f2854_n, g_e8f.e8x_2ea188_n, g_e8f.e8x_2f449c_n,
                       g_e8f.e8x_310bbc_n, g_e8f.e8x_insn_n, g_e8f.e8x_f74_desc_assist_done,
                       g_e8f.e8x_call_2f99d0_done);
            }
            if (g_e8f.e8y_mode) {
                printf("[JJFB_E8Y_SUMMARY] reason=tick_%u n2d92e4=%u ret_nz=%u ret_z=%u "
                       "a64_writes=%u n310=%u insn=%u a64_assist=%u evidence=OBSERVED\n",
                       tick, g_e8f.e8y_2d92e4_n, g_e8f.e8y_2d92e4_ret_nz, g_e8f.e8y_2d92e4_ret_z,
                       g_e8f.e8y_a64_write_n, g_e8f.e8y_310bbc_n, g_e8f.e8y_insn_n,
                       g_e8f.e8y_a64_assist_done);
            }
            fflush(stdout);
        }
    }

    /* E8V Case C: after first idle success, call R9-only downstream 0x2E993C once. */
    if (g_e8f.e8v_call_2e993c && !g_e8f.e8v_call_2e993c_done && g_e8f.idle_success_n > 0u &&
        tick >= 2u && find_robotol_r9(uc, &r9) && r9) {
        GwyUcEntryAbi abi;
        GwyUcEntryRunOut out;
        uint32_t r9_save = 0;
        int ok;
        uint64_t lim = 300000ull;
        g_e8f.e8v_call_2e993c_done = 1;
        (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
        (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9);
        memset(&abi, 0, sizeof(abi));
        abi.set_r0 = 1;
        abi.r0 = 0;
        abi.set_r1 = 1;
        abi.r1 = 0;
        abi.set_r2 = 1;
        abi.r2 = 0;
        abi.set_r3 = 1;
        abi.r3 = 0;
        abi.set_lr = 1;
        abi.lr = 0x80000u;
        if (g_e8f.fast_insn_limit) lim = g_e8f.fast_insn_limit;
        printf("[JJFB_E8V_DOWNSTREAM_CALL] entry=0x2E993C r9=0x%X "
               "note=FAST_ASSIST_R9_only_after_e88cc evidence=HYPOTHESIS\n",
               r9);
        fflush(stdout);
        ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, 0x2E993Du, 0x80000u, lim, &abi,
                                          &out);
        printf("[JJFB_E8V_DOWNSTREAM_DONE] ok=%d end=%s pc_after=0x%X "
               "note=FAST_ASSIST_not_product evidence=OBSERVED\n",
               ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after);
        fflush(stdout);
        (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
    }

    if (g_e8f.e8v_mode && (tick == 30u || tick == 40u || tick == 80u)) {
        printf("[JJFB_E8V_SUMMARY] reason=tick_%u e88cc_entries=%u early_exit=%u "
               "draw_cand=%u insn=%u bl=%u F6C=0x%X p4=0x%X p8=0x%X D14_s16=%d "
               "idle_success=%u evidence=OBSERVED\n",
               tick, g_e8f.e8v_e88cc_entries, g_e8f.e8v_early_exit_n, g_e8f.e8v_draw_cand_n,
               g_e8f.e8v_insn_n, g_e8f.e8v_bl_n, g_e8f.e8v_f6c_base, g_e8f.e8v_f6c_p4,
               g_e8f.e8v_f6c_p8, (int)g_e8f.e8v_d14_s16, g_e8f.idle_success_n);
        fflush(stdout);
    }

    if (tick == 25u || tick == 40u || tick == 80u || tick == 100u || tick == 600u)
        robotol_flag_writer_trace_dump_summary(tick == 600u   ? "tick_600"
                                              : tick == 100u ? "tick_100"
                                              : tick == 80u  ? "tick_80"
                                              : tick == 40u  ? "tick_40"
                                                             : "tick_25");
}
