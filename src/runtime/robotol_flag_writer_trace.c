#include "gwy_launcher/robotol_flag_writer_trace.h"
#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/jjfb_bmp_meta.h"
#include "gwy_launcher/jjfb_plat_11f00.h"
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
    uint32_t e9e_postmatch_n;
    char e9e_done_names[8][96];
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
    /* E9F: UI sequence — prefer larger members; optional request rewrite assist. */
    int e9f_mode;
    int e9f_rewrite_request; /* rewrite first DisplayFirst name → prefer (not bridge) */
    int e9f_rewrite_done;
    int e9f_multi; /* allow multiple natural postmatches */
    char e9f_prefer[96]; /* e.g. slogo!157!58.bmp */
    FILE *e9f_req_csv;
    FILE *e9f_results_jsonl;
    FILE *e9e_helper_csv;
    /* E9G: splash/loading UI_MODE=0x45 + real 0x2EF86C (NOT rewrite success). */
    int e9g_mode;
    int e9g_splash_call; /* JJFB_FAST_SPLASH_CALL */
    int e9g_splash_done;
    int e9g_ui_mode_assist; /* optional poke R9+0x8D0=0x45 — logged NOT product */
    int e9g_debug_only; /* JJFB_E9G_DEBUG=1 — trace only, no splash call */
    uint32_t e9g_splash_enter_n;
    uint32_t e9g_uimode_45_n;
    uint32_t e9g_uimode_writer_n;
    FILE *e9g_req_csv;
    FILE *e9g_uimode_csv;
    char e9g_idx_names[64][64]; /* collect guest index names for slogo audit */
    int e9g_idx_n;
    int e9g_slogo_class_logged;
    int e9g_skip_sibling_binds; /* after first splash UI postmatch → cont toward blit */
    /* E9H: r4 provenance + real blit (0x2EFA9A → 0x2EC6B8), no rewrite. */
    int e9h_mode;
    int e9h_r4_trace;
    int e9h_r4_assist; /* JJFB_FAST_SPLASH_R4_ASSIST — only after layout known */
    int e9h_r4_assist_done;
    uint32_t e9h_r4_last;
    uint32_t e9h_r4_change_n;
    uint32_t e9h_blit_n;
    uint32_t e9h_r4_gate_n;
    FILE *e9h_r4_csv;
    FILE *e9h_seq_csv;
    /* E9I: fuller splash UI — natural coords via R9+0x830/0x834 seed; optional coord assist. */
    int e9i_mode;
    int e9i_coord_assist; /* JJFB_SPLASH_COORD_ASSIST — diagnostic only */
    int e9i_skip_sibling; /* 1=E9H-style skip bar binds; 0=natural sibling binds */
    int e9i_scr_dim_seeded;
    uint32_t e9i_drawn_mask; /* bit0 loadingbar bit1 textbar bit2 top bit3 other */
    FILE *e9i_coord_csv;
    FILE *e9i_r4_csv;
    FILE *e9i_seq_csv;
    /* E9J: progress object R9+0xBD0 / post-r4 path. */
    int e9j_mode;
    int e9j_progress_assist; /* JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST */
    int e9j_progress_assist_done;
    int e9j_post_r4_reached;
    uint32_t e9j_progress_count_assist; /* BA0+0x2C seed under assist */
    uint32_t e9j_bd0_str_va; /* guest status string VA used for BD0 */
    FILE *e9j_writer_csv;
    FILE *e9j_postr4_csv;
    /* E9K: post-r4 textbar / 0x305BFC path — keep BD0 assist; delay HWND hold. */
    int e9k_mode;
    int e9k_hold_after_post_r4; /* JJFB_E9K_HOLD_AFTER_POST_R4 */
    int e9k_stop_on_textbar;    /* JJFB_E9K_STOP_ON_TEXTBAR_DRAW */
    int e9k_min_blits;          /* diagnostic; main.c also reads env */
    int e9k_305bfc_hit;
    int e9k_textbar_drawn;
    int e9k_hold_armed;
    FILE *e9k_postr4_csv;
    FILE *e9k_draw_csv;
    /* E9L: R9+0x818/0x81C screen dims for 0x305E78 text measure → 0x305BFC. */
    int e9l_mode;
    int e9l_textctx_assist; /* JJFB_FAST_TEXTCTX_ASSIST — dims only, NOT_PRODUCT */
    int e9l_textctx_assist_done;
    int e9l_305e78_ok;
    int e9l_2f2174_hit;
    int e9l_305bfc_hit;
    FILE *e9l_writer_csv;
    FILE *e9l_305e78_csv;
    FILE *e9l_text_csv;
    /* E9M: 0x305BFC text ABI — measure return (0x12340) + layout r1 repair. */
    int e9m_mode;
    int e9m_measure_shim; /* JJFB_FAST_TEXT_MEASURE_SHIM — width/height outs only */
    int e9m_layout_assist; /* JJFB_FAST_TEXT_LAYOUT_ASSIST — fix bad x at 0x2EFBA2 */
    int e9m_measure_shim_done;
    int e9m_layout_assist_done;
    int e9m_valid_305bfc_args;
    int e9m_draw_entered; /* reached BL 0x305C3C (not clip-skip) */
    int e9m_clip_skipped;
    uint32_t e9m_str_va;
    uint32_t e9m_meas_w;
    uint32_t e9m_meas_h;
    uint32_t e9m_last_r1;
    FILE *e9m_abi_csv;
    FILE *e9m_meas_csv;
    FILE *e9m_layout_csv;
    /* E9N: 0x305C3C glyph draw / platform text render trace. */
    int e9n_mode;
    int e9n_text_draw_shim; /* JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM */
    int e9n_glyphtrace;
    int e9n_wrapper_clip_pass; /* saw 0x305C2E before 0x305C32 */
    int e9n_inner_clip_skip;
    int e9n_core_entered;
    int e9n_glyph_lookup_hit;
    int e9n_dsm_draw_hit;
    int e9n_glyph_blit_hit;
    int e9n_plat_draw_hit;
    int e9n_draw_mode_miss;
    int e9n_core_returned;
    uint32_t e9n_core_ret_r0;
    uint32_t e9n_last_glyph_type;
    FILE *e9n_305c3c_csv;
    FILE *e9n_clip_csv;
    FILE *e9n_glyph_csv;
#ifdef GWY_HAVE_UNICORN
    uc_hook e9h_splash_hook;
#endif
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
        g_e8f.e9f_mode = env1("JJFB_E9F_MODE");
        g_e8f.e9f_rewrite_request = env1("JJFB_E9F_REWRITE_REQUEST");
        g_e8f.e9f_multi = env1("JJFB_E9F_MULTI_POSTMATCH") || g_e8f.e9f_mode;
        {
            const char *pref = getenv("JJFB_E9F_PREFER");
            memset(g_e8f.e9f_prefer, 0, sizeof(g_e8f.e9f_prefer));
            if (pref && pref[0])
                snprintf(g_e8f.e9f_prefer, sizeof(g_e8f.e9f_prefer), "%s", pref);
        }
        /* E9G: splash UI_MODE=0x45 via real 0x2EF86C — never treat rewrite as success. */
        g_e8f.e9g_mode = env1("JJFB_E9G_MODE");
        g_e8f.e9g_splash_call = env1("JJFB_FAST_SPLASH_CALL");
        g_e8f.e9g_ui_mode_assist = env1("JJFB_E9G_UI_MODE_ASSIST");
        g_e8f.e9g_debug_only = env1("JJFB_E9G_DEBUG");
        /* E9H: r4 / blit path — implies E9G; disables blind skip-to-progress. */
        g_e8f.e9h_mode = env1("JJFB_E9H_MODE");
        g_e8f.e9h_r4_trace = env1("JJFB_E9H_R4_TRACE") || g_e8f.e9h_mode;
        g_e8f.e9h_r4_assist = env1("JJFB_FAST_SPLASH_R4_ASSIST");
        /* E9I: complete splash loading UI — implies E9H; prefer natural coords. */
        g_e8f.e9i_mode = env1("JJFB_E9I_MODE");
        g_e8f.e9i_coord_assist = env1("JJFB_SPLASH_COORD_ASSIST");
        g_e8f.e9i_skip_sibling = env1("JJFB_E9I_SKIP_SIBLING");
        /* E9J: R9+0xBD0 status string / post-r4 UI — implies E9I. */
        g_e8f.e9j_mode = env1("JJFB_E9J_MODE");
        g_e8f.e9j_progress_assist = env1("JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST");
        g_e8f.e9j_progress_count_assist = 4u;
        {
            const char *pc = getenv("JJFB_E9J_PROGRESS_COUNT");
            if (pc && pc[0]) {
                char *end = NULL;
                unsigned long v = strtoul(pc, &end, 0);
                if (end != pc && v > 0 && v <= 12u)
                    g_e8f.e9j_progress_count_assist = (uint32_t)v;
            }
        }
        /* E9K: post-r4 / textbar / 0x305BFC — implies E9J; keep BD0 assist. */
        g_e8f.e9k_mode = env1("JJFB_E9K_MODE");
        g_e8f.e9k_hold_after_post_r4 = env1("JJFB_E9K_HOLD_AFTER_POST_R4");
        g_e8f.e9k_stop_on_textbar = env1("JJFB_E9K_STOP_ON_TEXTBAR_DRAW");
        g_e8f.e9k_min_blits = 8;
        {
            const char *mb = getenv("JJFB_E9K_MIN_BLITS");
            if (mb && mb[0]) {
                char *end = NULL;
                unsigned long v = strtoul(mb, &end, 0);
                if (end != mb && v > 0 && v < 256u)
                    g_e8f.e9k_min_blits = (int)v;
            }
        }
        /* E9L: R9+0x818/0x81C text-measure dims — implies E9K; BD0 assist may remain. */
        g_e8f.e9l_mode = env1("JJFB_E9L_MODE");
        g_e8f.e9l_textctx_assist = env1("JJFB_FAST_TEXTCTX_ASSIST");
        /* E9M: 305BFC ABI / text-measure return — implies E9L + textctx dims. */
        g_e8f.e9m_mode = env1("JJFB_E9M_MODE");
        g_e8f.e9m_measure_shim = env1("JJFB_FAST_TEXT_MEASURE_SHIM");
        g_e8f.e9m_layout_assist = env1("JJFB_FAST_TEXT_LAYOUT_ASSIST");
        g_e8f.e9n_mode = env1("JJFB_E9N_MODE");
        g_e8f.e9n_text_draw_shim = env1("JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM");
        g_e8f.e9n_glyphtrace = env1("JJFB_E9N_GLYPHTRACE");
        if (g_e8f.e9n_mode) {
            g_e8f.e9m_mode = 1;
            g_e8f.e9l_mode = 1;
            if (!getenv("JJFB_FAST_TEXTCTX_ASSIST"))
                g_e8f.e9l_textctx_assist = 1;
            if (!getenv("JJFB_FAST_TEXT_MEASURE_SHIM"))
                g_e8f.e9m_measure_shim = 1;
        }
        if (g_e8f.e9m_mode) {
            g_e8f.e9l_mode = 1;
            if (!getenv("JJFB_FAST_TEXTCTX_ASSIST"))
                g_e8f.e9l_textctx_assist = 1;
        }
        if (g_e8f.e9l_mode) {
            g_e8f.e9k_mode = 1;
            if (!getenv("JJFB_E9K_HOLD_AFTER_POST_R4"))
                g_e8f.e9k_hold_after_post_r4 = 1;
            /* Prefer not to stop on textbar until 305BFC path is exercised. */
            if (!getenv("JJFB_E9K_STOP_ON_TEXTBAR_DRAW"))
                g_e8f.e9k_stop_on_textbar = 0;
        }
        if (g_e8f.e9k_mode) {
            g_e8f.e9j_mode = 1;
            if (!getenv("JJFB_E9K_HOLD_AFTER_POST_R4") && !g_e8f.e9l_mode)
                g_e8f.e9k_hold_after_post_r4 = 1;
            if (!getenv("JJFB_E9K_STOP_ON_TEXTBAR_DRAW") && !g_e8f.e9l_mode)
                g_e8f.e9k_stop_on_textbar = 1;
        }
        if (g_e8f.e9j_mode)
            g_e8f.e9i_mode = 1;
        if (g_e8f.e9i_mode) {
            g_e8f.e9h_mode = 1;
            g_e8f.e9h_r4_trace = 1;
            /* Default: do NOT skip sibling binds (natural bar/textbar). */
            if (!getenv("JJFB_E9I_SKIP_SIBLING"))
                g_e8f.e9i_skip_sibling = 0;
            printf("[JJFB_E9I_SPLASH_UI] enabled=1 coord_assist=%d skip_sibling=%d "
                   "note=seed_R9_830_834_natural_xy NOT_PRODUCT evidence=HYPOTHESIS\n",
                   g_e8f.e9i_coord_assist, g_e8f.e9i_skip_sibling);
            fflush(stdout);
        }
        if (g_e8f.e9j_mode) {
            printf("[JJFB_E9J_PROGRESS] enabled=1 assist=%d progress_count=%u "
                   "note=BD0_status_string_BA0_2C_count NOT_PRODUCT evidence=HYPOTHESIS\n",
                   g_e8f.e9j_progress_assist, g_e8f.e9j_progress_count_assist);
            fflush(stdout);
        }
        if (g_e8f.e9k_mode) {
            printf("[JJFB_E9K_POSTR4] enabled=1 hold_after_post_r4=%d stop_on_textbar=%d "
                   "min_blits=%d note=delay_hold_trace_2EFAFA_2EFB0E_305BFC NOT_PRODUCT "
                   "evidence=HYPOTHESIS\n",
                   g_e8f.e9k_hold_after_post_r4, g_e8f.e9k_stop_on_textbar,
                   g_e8f.e9k_min_blits);
            fflush(stdout);
        }
        if (g_e8f.e9l_mode) {
            printf("[JJFB_E9L_TEXTCTX] enabled=1 assist=%d note=R9_818_81C_dims_for_305E78 "
                   "NOT_PRODUCT evidence=HYPOTHESIS\n",
                   g_e8f.e9l_textctx_assist);
            fflush(stdout);
        }
        if (g_e8f.e9m_mode) {
            printf("[JJFB_E9M_TEXT_ABI] enabled=1 measure_shim=%d layout_assist=%d "
                   "note=305E78_304558_0x12340_outs_then_2EFBA2_r1 NOT_PRODUCT "
                   "evidence=HYPOTHESIS\n",
                   g_e8f.e9m_measure_shim, g_e8f.e9m_layout_assist);
            fflush(stdout);
        }
        if (g_e8f.e9n_mode) {
            printf("[JJFB_E9N_TEXT_RENDER] enabled=1 text_draw_shim=%d glyphtrace=%d "
                   "note=305C3C_inner_clip_glyph_2F2360 NOT_PRODUCT evidence=HYPOTHESIS\n",
                   g_e8f.e9n_text_draw_shim, g_e8f.e9n_glyphtrace);
            fflush(stdout);
        }
        if (g_e8f.e9h_mode) {
            g_e8f.e9g_mode = 1;
            if (!g_e8f.e9g_debug_only)
                g_e8f.e9g_splash_call = 1;
            if (!g_e8f.e9g_ui_mode_assist && env1("JJFB_E9G_UI_MODE_ASSIST"))
                g_e8f.e9g_ui_mode_assist = 1;
            /* Prefer UI_MODE assist for E9H/E9I blit path unless explicitly disabled. */
            if (!getenv("JJFB_E9G_UI_MODE_ASSIST") || getenv("JJFB_E9G_UI_MODE_ASSIST")[0] == '1')
                g_e8f.e9g_ui_mode_assist = 1;
            printf("[JJFB_E9H_SPLASH_BLIT] enabled=1 r4_trace=%d r4_assist=%d "
                   "note=reach_0x2EFA9A_2EC6B8_not_skip_2EFA9E NOT_PRODUCT evidence=HYPOTHESIS\n",
                   g_e8f.e9h_r4_trace, g_e8f.e9h_r4_assist);
            fflush(stdout);
        }
        if (g_e8f.e9g_mode) {
            g_e8f.e9f_mode = 1;
            g_e8f.e9e_mode = 1;
            g_e8f.e9e_postmatch_shims = 1;
            g_e8f.e9d_mode = 1;
            g_e8f.e9d_strcmp_shim = 1;
            g_e8f.e9f_multi = 1;
            /* Rewrite stays off unless JJFB_E9F_REWRITE_REQUEST=1 was explicit. */
            if (!g_e8f.state_watch) g_e8f.state_watch = 1;
            printf("[JJFB_E9G_SPLASH] enabled=1 splash_call=%d ui_mode_assist=%d rewrite=%d "
                   "note=real_0x2EF86C_not_request_rewrite NOT_PRODUCT evidence=HYPOTHESIS\n",
                   g_e8f.e9g_splash_call && !g_e8f.e9g_debug_only, g_e8f.e9g_ui_mode_assist,
                   g_e8f.e9f_rewrite_request);
            fflush(stdout);
            if (g_e8f.e9f_rewrite_request) {
                printf("[JJFB_E9G_CLASS] class=REQUEST_REWRITE_DEBUG_ONLY "
                       "note=not_success_path evidence=OBSERVED\n");
                fflush(stdout);
            }
        }
        if (g_e8f.e9f_mode) {
            g_e8f.e9e_mode = 1;
            g_e8f.e9e_postmatch_shims = 1;
            g_e8f.e9d_mode = 1;
            g_e8f.e9d_strcmp_shim = 1;
            printf("[JJFB_E9F_UI_SEQUENCE] enabled=1 prefer=\"%s\" rewrite=%d multi=%d "
                   "note=natural_postmatch_larger_ui NOT_PRODUCT evidence=HYPOTHESIS\n",
                   g_e8f.e9f_prefer[0] ? g_e8f.e9f_prefer : "(game_request)",
                   g_e8f.e9f_rewrite_request, g_e8f.e9f_multi);
            fflush(stdout);
        }
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
                        g_e8f.e8z_mode || g_e8f.e9a_mode || g_e8f.e9d_mode || g_e8f.e9e_mode ||
                        g_e8f.e9f_mode || g_e8f.e9g_mode || g_e8f.e9h_mode;
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
        if (g_e8f.e9h_splash_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.e9h_splash_hook);
            g_e8f.e9h_splash_hook = 0;
        }
    }
#endif
    jjfb_bmp_meta_reset();
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
static int e9d_peek_cstr(uc_engine *uc, uint32_t va, char *ascii, int asciicap, char *hex,
                         int hexcap, int *out_len);
static int e9d_try_strcmp_shim(uc_engine *uc, uint32_t r0, uint32_t r1);
static int e9d_try_memcpy_shim(uc_engine *uc, uint32_t pc, uint32_t r0, uint32_t r1,
                               uint32_t r2);
static int e9e_try_postmatch_complete(uc_engine *uc, const char *name);
static int e9i_is_splash_sibling(const char *name);
static int e9i_try_splash_sibling_resolve(uc_engine *uc, const char *name, uint32_t lr);
static int e9e_name_already_done(const char *name);
static void e9f_open_trace_files(void);
static int e9f_try_rewrite_request(uc_engine *uc, uint32_t *name_va_inout, uint32_t pc);
static void e9g_open_trace_files(void);
static void e9g_log_uimode_csv(uint32_t pc, uint32_t lr, uint32_t oldv, uint32_t newv,
                               uint32_t r9, const char *note);
static void e9g_log_req_csv(uint32_t pc, uint32_t lr, uint32_t ui_mode, const char *name,
                            int natural_entry, int postmatch, uint32_t handle,
                            uint32_t draw_pc, const char *note);
static void e9g_collect_idx_name(const char *idx);
static void e9g_audit_slogo_once(int force);
static void fast_call_splash(void *uc);
static void e9h_open_trace_files(void);
static void e9h_log_r4(uint32_t pc, uint32_t lr, uint32_t r4, uint32_t r0, uint32_t r1,
                       uint32_t r9, const char *note);
static void e9i_open_trace_files(void);
static void e9i_log_coord(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r5, uint32_t r6,
                          uint32_t r7, uint32_t a830, uint32_t a834, const char *note);
static void e9i_seed_scr_dims(void *uc, uint32_t r9);
static void e9j_open_trace_files(void);
static void e9j_log_writer(uint32_t pc, uint32_t lr, uint32_t off, uint32_t old_v, uint32_t new_v,
                           const char *note);
static void e9j_log_postr4(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r4, uint32_t b6c,
                           uint32_t bd0, uint32_t count, const char *note);
static void e9j_seed_progress_object(void *uc, uint32_t r9);
static void e9k_open_trace_files(void);
static void e9k_log_postr4(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                           uint32_t r3, uint32_t r4, uint32_t r9, uint32_t sp, uint32_t bar,
                           uint32_t textbar, uint32_t loadingbar, uint32_t count, uint32_t bd0,
                           const char *note);
static void e9k_log_draw(uint32_t pc, uint32_t lr, const char *member, int x, int y, int w, int h,
                         const char *note);
static void e9k_dump_slots(void *uc, uint32_t r9w, uint32_t r4, uint32_t pc, const char *tag);
static void e9k_try_hold(const char *reason);
static void e9l_open_trace_files(void);
static void e9l_log_writer(uint32_t pc, uint32_t lr, uint32_t off, uint32_t old_v, uint32_t new_v,
                           const char *note);
static void e9l_log_305e78(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                           uint32_t r3, uint32_t a818, uint32_t a81c, const char *note);
static void e9l_log_text(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                         uint32_t r3, const char *note);
static void e9l_seed_textctx(void *uc, uint32_t r9);
static void e9m_open_trace_files(void);
static void e9m_log_abi(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                        uint32_t r3, uint32_t w, uint32_t h, const char *note);
static void e9m_log_meas(uint32_t pc, uint32_t lr, uint32_t code, uint32_t str, uint32_t old_w,
                         uint32_t old_h, uint32_t new_w, uint32_t new_h, const char *note);
static void e9m_log_layout(uint32_t pc, uint32_t lr, uint32_t old_r1, uint32_t new_r1,
                           uint32_t r2, uint32_t w, uint32_t h, const char *note);
static int e9m_estimate_text_wh(void *uc, uint32_t str_va, uint32_t *w_out, uint32_t *h_out);
static int e9m_xy_plausible(uint32_t x, uint32_t y);
static int e9m_try_measure_shim(void *uc, uint32_t pc);
static int e9m_try_layout_assist(void *uc, uint32_t *r1_io);
static void e9n_open_trace_files(void);
static void e9n_read_textctx(void *uc, uint32_t r9, uint32_t *w, uint32_t *h, uint32_t *yoff,
                             uint32_t *scr_w);
static void e9n_log_305c3c(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                           uint32_t r3, uint32_t r4, uint32_t r9, uint32_t sp,
                           uint32_t scr_w, uint32_t scr_h, uint32_t dim818, uint32_t dim81c,
                           const char *str_ascii, const char *note);
static void e9n_log_clip(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                         uint32_t r3, uint32_t cmp_a, uint32_t cmp_b, int pass,
                         const char *note);
static void e9n_log_glyph(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                          uint32_t r3, uint32_t glyph_type, uint32_t str_va,
                          const char *str_ascii, const char *note);
#ifdef GWY_HAVE_UNICORN
static void on_e9h_splash_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data);
#endif

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

    /* E9G/E9H: splash / UI_MODE writer / bind / blit sites. */
    if ((g_e8f.e9g_mode || g_e8f.e9h_mode || g_e8f.e9n_mode) &&
        (bp->pc == 0x2EF86Cu || bp->pc == 0x30662Cu || bp->pc == 0x306344u ||
         bp->pc == 0x2FC418u || bp->pc == 0x2EFA33u || bp->pc == 0x2EFA43u ||
         bp->pc == 0x2EFA53u || bp->pc == 0x2EFA46u || bp->pc == 0x2EFA56u ||
         bp->pc == 0x2EFA5Cu || bp->pc == 0x2EFA7Cu || bp->pc == 0x2EFA9Au ||
         bp->pc == 0x2EC6B8u || bp->pc == 0x2EFAF2u || bp->pc == 0x2EFA9Eu ||
         bp->pc == 0x2EFAFAu || bp->pc == 0x2EFAE2u || bp->pc == 0x2EFB0Eu ||
         bp->pc == 0x2EFBA2u || bp->pc == 0x2EFBA6u || bp->pc == 0x305BFCu ||
         bp->pc == 0x2FC444u || bp->pc == 0x30ED98u || bp->pc == 0x2F2174u ||
         bp->pc == 0x2EFB08u || bp->pc == 0x2EFB0Au || bp->pc == 0x305EA0u ||
         bp->pc == 0x2EFB48u || bp->pc == 0x303C50u || bp->pc == 0x305C2Eu ||
         bp->pc == 0x305C32u || bp->pc == 0x305C3Cu || bp->pc == 0x305C54u ||
         bp->pc == 0x305CA0u || bp->pc == 0x305C90u || bp->pc == 0x305C98u ||
         bp->pc == 0x305D08u || bp->pc == 0x305D16u || bp->pc == 0x305D58u ||
         bp->pc == 0x305D5Cu || bp->pc == 0x2F2360u || bp->pc == 0x2F99A4u ||
         bp->pc == 0x304558u)) {
        uint32_t ui_mode = 0;
        uint32_t r9w = r9;
        uint32_t r4 = 0, r2 = 0, r3 = 0, sp = 0;
        uc_reg_read(uc, UC_ARM_REG_R4, &r4);
        uc_reg_read(uc, UC_ARM_REG_R2, &r2);
        uc_reg_read(uc, UC_ARM_REG_R3, &r3);
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        if ((!r9w || !find_robotol_r9(uc, &r9w)) && r9) r9w = r9;
        if (r9w)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + E8I_STATE_OFF, &ui_mode);
        e9g_open_trace_files();
        if (g_e8f.e9h_mode) e9h_open_trace_files();
        if (bp->pc == 0x2EF86Cu) {
            g_e8f.e9g_splash_enter_n++;
            g_e8f.e9h_r4_last = r4;
            printf("[JJFB_E9G_SPLASH_ENTER] lr=0x%X r0=0x%X r1=0x%X r4=0x%X ui_mode=0x%X "
                   "tick=%u class=SPLASH_2EF86C_REACHED evidence=OBSERVED\n",
                   lr, r0, r1, r4, ui_mode, g_e8f.tick);
            e9g_log_uimode_csv(bp->pc, lr, ui_mode, ui_mode, r9w, "splash_enter");
            e9h_log_r4(bp->pc, lr, r4, r0, r1, r9w, "splash_enter");
            fflush(stdout);
        } else if (bp->pc == 0x2FC418u) {
            g_e8f.e9g_uimode_writer_n++;
            printf("[JJFB_E9G_UIMODE_WRITER] pc=0x2FC418 lr=0x%X r0=0x%X r1=0x%X "
                   "ui_mode=0x%X tick=%u evidence=OBSERVED\n",
                   lr, r0, r1, ui_mode, g_e8f.tick);
            e9g_log_uimode_csv(bp->pc, lr, ui_mode, ui_mode, r9w, "writer_2FC418");
            fflush(stdout);
        } else if (bp->pc == 0x306344u) {
            printf("[JJFB_E9G_UIMODE_CMP] pc=0x306344 lr=0x%X r0=0x%X ui_mode=0x%X "
                   "tick=%u note=cmp_0x45_gate evidence=OBSERVED\n",
                   lr, r0, ui_mode, g_e8f.tick);
            fflush(stdout);
        } else if (bp->pc == 0x30662Cu) {
            printf("[JJFB_E9G_SPLASH_BL] pc=0x30662C lr=0x%X r0=0x%X r1=0x%X ui_mode=0x%X "
                   "tick=%u note=bl_0x2EF86C evidence=OBSERVED\n",
                   lr, r0, r1, ui_mode, g_e8f.tick);
            fflush(stdout);
        } else if (bp->pc == 0x2EFA33u) {
            printf("[JJFB_E9H_BIND] pc=0x2EFA33 role=loadingbar r4=0x%X lr=0x%X tick=%u "
                   "evidence=OBSERVED\n",
                   r4, lr, g_e8f.tick);
            e9h_log_r4(bp->pc, lr, r4, r0, r1, r9w, "bind_loadingbar");
            fflush(stdout);
        } else if (bp->pc == 0x2EFA46u) {
            /* Bar bind BL @ 0x2EFA46. E9H may skip to y-calc; E9I natural keeps binds. */
            printf("[JJFB_E9H_BIND] pc=0x2EFA46 role=bar_bl r0=0x%X r4=0x%X tick=%u "
                   "evidence=OBSERVED\n",
                   r0, r4, g_e8f.tick);
            e9h_log_r4(bp->pc, lr, r4, r0, r1, r9w, "bind_bar_bl");
            if (g_e8f.e9h_mode && g_e8f.e9g_skip_sibling_binds &&
                (!g_e8f.e9i_mode || g_e8f.e9i_skip_sibling)) {
                uint32_t cont = 0x2EFA5Cu | 1u;
                uint32_t r5 = 0, r6 = 0, r7 = 0;
                uc_reg_read(uc, UC_ARM_REG_R5, &r5);
                uc_reg_read(uc, UC_ARM_REG_R6, &r6);
                uc_reg_read(uc, UC_ARM_REG_R7, &r7);
                uc_reg_write(uc, UC_ARM_REG_PC, &cont);
                printf("[JJFB_E9H_SPLASH_CONT_BLIT] from=0x2EFA46 cont=0x2EFA5C "
                       "r5=0x%X r6=0x%X r7=0x%X note=preserve_regs_run_y_calc NOT_PRODUCT "
                       "evidence=OBSERVED\n",
                       r5, r6, r7);
                fflush(stdout);
                return;
            }
            if (g_e8f.e9i_mode && !g_e8f.e9i_skip_sibling)
                printf("[JJFB_E9I_SIBLING] keep=bar_bind note=natural_no_skip evidence=OBSERVED\n");
            fflush(stdout);
        } else if (bp->pc == 0x2EFA5Cu) {
            uint32_t a830 = 0, a834 = 0, r5 = 0, r6 = 0, r7 = 0;
            uint32_t slot20 = 0, slot24 = 0, slot28 = 0, r4src = 0, gate_b6c = 0;
            if (r9w) {
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x830u, &a830);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x834u, &a834);
                /* Splash UI obj = R9+0xBA0: +0x20 bar, +0x24 textbar, +0x28 loadingbar.
                 * r4 source = *(R9+0xBD0) = splash_obj+0x30. Gate peek = *(R9+0xB6C). */
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x20u,
                                               &slot20);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x24u,
                                               &slot24);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x28u,
                                               &slot28);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBD0u, &r4src);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xB6Cu, &gate_b6c);
            }
            uc_reg_read(uc, UC_ARM_REG_R5, &r5);
            uc_reg_read(uc, UC_ARM_REG_R6, &r6);
            uc_reg_read(uc, UC_ARM_REG_R7, &r7);
            printf("[JJFB_E9I_YCALC] pc=0x2EFA5C phase=enter r0=0x%X r5=0x%X r6=0x%X r7=0x%X "
                   "R9_830=0x%X R9_834=0x%X note=getter_2F9970_then_center_x_to_r6 "
                   "evidence=OBSERVED\n",
                   r0, r5, r6, r7, a830, a834);
            printf("[JJFB_E9I_SPLASH_SLOTS] R9_BA0_plus20_bar=0x%X plus24_textbar=0x%X "
                   "plus28_loadingbar=0x%X R9_BD0_r4_src=0x%X R9_B6C_gate=0x%X r4_reg=0x%X "
                   "note=r4_is_splash_obj_plus_0x30_not_sibling_bind evidence=OBSERVED\n",
                   slot20, slot24, slot28, r4src, gate_b6c, r4);
            if (r4src && !r4)
                printf("[JJFB_E9I_CLASS] class=SPLASH_R4_SOURCE_FOUND_NEXT_GAP "
                       "R9_BD0=0x%X r4_reg=0x%X note=field_set_but_reg_stale "
                       "evidence=OBSERVED\n",
                       r4src, r4);
            else if (!r4src)
                printf("[JJFB_E9I_CLASS] class=SPLASH_R4_SOURCE_FOUND_NEXT_GAP "
                       "R9_BD0=0 r4_reg=0x%X "
                       "note=r4_is_R9_BD0_splash_obj_plus30_not_bar_textbar_slots "
                       "evidence=OBSERVED\n",
                       r4);
            if (!slot20 || !slot24)
                printf("[JJFB_E9I_CLASS] class=SPLASH_R4_REQUIRES_SIBLING_BIND "
                       "bar=0x%X textbar=0x%X loadingbar=0x%X "
                       "note=sibling_slots_incomplete_for_fuller_UI_not_for_r4 "
                       "evidence=OBSERVED\n",
                       slot20, slot24, slot28);
            e9i_log_coord(bp->pc, lr, r0, r5, r6, r7, a830, a834, "ycalc_enter");
            e9h_log_r4(bp->pc, lr, r4, r4src, slot28, r9w, "ycalc_r4_vs_R9_BD0");
            fflush(stdout);
        } else if (bp->pc == 0x2EFA56u) {
            printf("[JJFB_E9H_BIND] pc=0x2EFA56 role=textbar_bl r0=0x%X r4=0x%X tick=%u "
                   "evidence=OBSERVED\n",
                   r0, r4, g_e8f.tick);
            e9h_log_r4(bp->pc, lr, r4, r0, r1, r9w, "bind_textbar_bl");
            fflush(stdout);
        } else if (bp->pc == 0x2EFA53u) {
            printf("[JJFB_E9H_BIND] pc=0x2EFA53 role=textbar r4=0x%X lr=0x%X tick=%u "
                   "evidence=OBSERVED\n",
                   r4, lr, g_e8f.tick);
            e9h_log_r4(bp->pc, lr, r4, r0, r1, r9w, "bind_textbar");
            fflush(stdout);
        } else if (bp->pc == 0x2EFA7Cu) {
            uint32_t r5 = 0, r6 = 0, r7 = 0, a830 = 0, a834 = 0;
            uc_reg_read(uc, UC_ARM_REG_R5, &r5);
            uc_reg_read(uc, UC_ARM_REG_R6, &r6);
            uc_reg_read(uc, UC_ARM_REG_R7, &r7);
            if (r9w) {
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x830u, &a830);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x834u, &a834);
            }
            printf("[JJFB_E9H_BLIT_SETUP] pc=0x2EFA7C r0=0x%X r1=0x%X r2=0x%X r4=0x%X "
                   "r5=%d r6=%d R9_830=0x%X R9_834=0x%X tick=%u evidence=OBSERVED\n",
                   r0, r1, r2, r4, (int)r5, (int)r6, a830, a834, g_e8f.tick);
            e9h_log_r4(bp->pc, lr, r4, r0, r1, r9w, "blit_setup_2EFA7C");
            e9i_log_coord(bp->pc, lr, r0, r5, r6, r7, a830, a834, "pre_blit_setup");
            fflush(stdout);
        } else if (bp->pc == 0x2EFA9Au) {
            printf("[JJFB_E9H_BLIT_SITE] pc=0x2EFA9A role=loadingbar_2EC6B8_call r0=0x%X "
                   "r1=0x%X r2=0x%X r4=0x%X tick=%u evidence=OBSERVED\n",
                   r0, r1, r2, r4, g_e8f.tick);
            e9h_log_r4(bp->pc, lr, r4, r0, r1, r9w, "pre_blit_2EFA9A");
            fflush(stdout);
        } else if (bp->pc == 0x2EC6B8u) {
            uint32_t px = 0, fp150c = 0;
            uint16_t mw = 0, mh = 0;
            char member[96];
            int blitted = 0;
            int x = (int)r1, y = (int)r2;
            int onscreen;
            g_e8f.e9h_blit_n++;
            memset(member, 0, sizeof(member));
            if (r0)
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r0 + 4u, &px);
            if (r9w)
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x150Cu, &fp150c);
            if (px)
                (void)jjfb_bmp_meta_get(px, &mw, &mh, member, sizeof(member));
            if (!mw && g_e8f.e8z_bw) {
                mw = g_e8f.e8z_bw;
                mh = g_e8f.e8z_bh;
                px = g_e8f.e8z_pixel_va ? g_e8f.e8z_pixel_va : px;
            }
            onscreen = (x > -40 && x < 240 && y > -40 && y < 320);
            printf("[JJFB_E9H_2EC6B8] lr=0x%X obj=0x%X x=%d y=%d flags=0x%X pixels=0x%X "
                   "fp150C=0x%X r4=0x%X member=%s onscreen=%d tick=%u evidence=OBSERVED\n",
                   lr, r0, x, y, r3, px, fp150c, r4, member[0] ? member : "?", onscreen,
                   g_e8f.tick);
            e9h_log_r4(bp->pc, lr, r4, r0, r1, r9w, "blit_2EC6B8");
            /* Natural xy: R9+0x830=screen_w → ASRS R6 center-x; R9+0x834 → R5; R5-=100 → y.
             * COORD_ASSIST only when JJFB_SPLASH_COORD_ASSIST=1 (E9I diagnostic). */
            if ((g_e8f.e9i_coord_assist || (!g_e8f.e9i_mode && g_e8f.e9h_mode)) &&
                px && mw > 0 && mh > 0 && !onscreen && member[0] &&
                (strstr(member, "loadingbar") || strstr(member, "textbar") ||
                 strstr(member, "top!") || strstr(member, "target"))) {
                int ax = (240 - (int)mw) / 2;
                int ay = 220; /* equiv: seeded R9+0x834=320 then SUBS #100 */
                if (ax < 0) ax = 0;
                printf("[JJFB_SPLASH_COORD_ASSIST] member=%s guest=(%d,%d) fixed=(%d,%d) "
                       "w=%u h=%u reason=R9_830_834_zero_or_bad NOT_PRODUCT evidence=OBSERVED\n",
                       member, x, y, ax, ay, (unsigned)mw, (unsigned)mh);
                e9i_log_coord(bp->pc, lr, (uint32_t)x, (uint32_t)y, (uint32_t)ax, (uint32_t)ay,
                              0, 0, "coord_assist");
                x = ax;
                y = ay;
                onscreen = 1;
            }
            if ((g_e8f.e9h_mode || g_e8f.e9i_mode) && px && mw > 0 && mh > 0 && onscreen) {
                int is_lb = member[0] && strstr(member, "loadingbar") != NULL;
                int is_tb = member[0] && strstr(member, "textbar") != NULL;
                int is_top = member[0] && strstr(member, "top!") != NULL;
                int is_bar = member[0] && strncmp(member, "bar!", 4) == 0;
                /* Progress loop BL @ 0x2EFAE2 is 4-byte Thumb → LR = 0x2EFAE7. */
                int is_progress_blit =
                    is_bar && ((lr & ~1u) == 0x2EFAE6u || (lr & ~1u) == 0x2EFAE2u);
                printf("[JJFB_E9H_CLASS] class=%s obj=0x%X pixels=0x%X x=%d y=%d w=%u h=%u "
                       "member=%s evidence=OBSERVED\n",
                       is_lb ? "SPLASH_LOADINGBAR_DRAWN_NO_REWRITE"
                             : (is_tb ? "SPLASH_TEXTBAR_DRAWN_NO_REWRITE"
                                      : (is_top ? "SPLASH_TOP_DRAWN_NO_REWRITE"
                                                : (is_progress_blit
                                                       ? "SPLASH_PROGRESS_DRAWN_NO_REWRITE"
                                                       : "SPLASH_RESOURCE_DRAWN_NO_REWRITE"))),
                       r0, px, x, y, (unsigned)mw, (unsigned)mh,
                       member[0] ? member : "?");
                if (is_tb) {
                    printf("[JJFB_E9I_CLASS] class=SPLASH_TEXTBAR_DRAWN_NO_REWRITE "
                           "member=%s evidence=OBSERVED\n",
                           member);
                    printf("[JJFB_E9J_CLASS] class=SPLASH_TEXTBAR_DRAWN_NO_REWRITE "
                           "member=%s evidence=OBSERVED\n",
                           member);
                    if (g_e8f.e9k_mode) {
                        g_e8f.e9k_textbar_drawn = 1;
                        e9k_log_draw(bp->pc, lr, member, x, y, (int)mw, (int)mh, "textbar_blit");
                        printf("[JJFB_E9K_CLASS] class=SPLASH_TEXTBAR_DRAWN_NO_REWRITE "
                               "member=%s x=%d y=%d w=%u h=%u evidence=OBSERVED\n",
                               member, x, y, (unsigned)mw, (unsigned)mh);
                        if (g_e8f.e9k_stop_on_textbar)
                            e9k_try_hold("textbar_draw");
                    }
                }
                if (is_top)
                    printf("[JJFB_E9I_CLASS] class=SPLASH_TOP_DRAWN_NO_REWRITE "
                           "member=%s evidence=OBSERVED\n",
                           member);
                if (is_progress_blit) {
                    printf("[JJFB_E9J_CLASS] class=SPLASH_PROGRESS_DRAWN_NO_REWRITE "
                           "member=%s x=%d y=%d evidence=OBSERVED\n",
                           member, x, y);
                }
                if (is_lb && !g_e8f.e9i_coord_assist && x > -40 && y > -40)
                    printf("[JJFB_E9I_CLASS] class=SPLASH_LOADINGBAR_NATURAL_COORD_VISIBLE "
                           "x=%d y=%d evidence=OBSERVED\n",
                           x, y);
                fflush(stdout);
                blitted = jjfb_e9h_blit_guest_pixels(uc, px, x, y, (int)mw, (int)mh,
                                                     member[0] ? member : "?");
                if (blitted) {
                    uint32_t ret = lr | 1u;
                    uint32_t r0ret = 1u;
                    if (is_lb) g_e8f.e9i_drawn_mask |= 1u;
                    if (is_tb) g_e8f.e9i_drawn_mask |= 2u;
                    if (is_top) g_e8f.e9i_drawn_mask |= 4u;
                    if (is_progress_blit) g_e8f.e9i_drawn_mask |= 8u;
                    if (!is_lb && !is_tb && !is_top && !is_progress_blit)
                        g_e8f.e9i_drawn_mask |= 16u;
                    if ((g_e8f.e9i_drawn_mask & 1u) && (g_e8f.e9i_drawn_mask & ~1u)) {
                        printf("[JJFB_E9I_CLASS] class=SPLASH_LOADING_UI_VISIBLE "
                               "mask=0x%X evidence=OBSERVED\n",
                               g_e8f.e9i_drawn_mask);
                        printf("[JJFB_E9J_CLASS] class=SPLASH_LOADING_UI_VISIBLE "
                               "mask=0x%X evidence=OBSERVED\n",
                               g_e8f.e9i_drawn_mask);
                    }
                    uc_reg_write(uc, UC_ARM_REG_R0, &r0ret);
                    uc_reg_write(uc, UC_ARM_REG_PC, &ret);
                    return;
                }
            } else if ((g_e8f.e9h_mode || g_e8f.e9i_mode) && px && mw > 0 && mh > 0 &&
                       !onscreen) {
                printf("[JJFB_E9H_CLASS] class=SPLASH_BLIT_BAD_XY obj=0x%X x=%d y=%d "
                       "member=%s note=need_scr_dim_seed_or_coord_assist evidence=OBSERVED\n",
                       r0, x, y, member[0] ? member : "?");
                printf("[JJFB_E9I_CLASS] class=SPLASH_COORD_ASSIST_STILL_REQUIRED "
                       "evidence=OBSERVED\n");
            }
            fflush(stdout);
        } else if (bp->pc == 0x2EFAFAu) {
            uint32_t bd0 = 0, b6c = 0, count = 0;
            uint32_t r5 = 0, r6 = 0, r7 = 0;
            if (r9w) {
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBD0u, &bd0);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xB6Cu, &b6c);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x2Cu,
                                               &count);
            }
            printf("[JJFB_E9I_R4_CMP] pc=0x2EFAFA r4=0x%X r0=0x%X tick=%u note=%s "
                   "evidence=OBSERVED\n",
                   r4, r0, g_e8f.tick, r4 ? "r4_nz" : "r4_zero_blocks");
            fflush(stdout);
            printf("[JJFB_E9J_R4_GATE] pc=0x2EFAFA r4=0x%X BD0=0x%X B6C=0x%X count=0x%X "
                   "tick=%u note=%s evidence=OBSERVED\n",
                   r4, bd0, b6c, count, g_e8f.tick,
                   r4 ? "r4_pass" : (b6c ? "b6c_should_have_skipped" : "blocked_bd0_zero"));
            fflush(stdout);
            e9j_log_postr4(bp->pc, lr, r0, r4, b6c, bd0, count,
                           r4 ? "r4_cmp_nz" : "r4_cmp_zero");
            e9h_log_r4(bp->pc, lr, r4, r0, r1, r9w, r4 ? "r4_cmp_nz" : "r4_cmp_zero");
            if (g_e8f.e9k_mode) {
                uc_reg_read(uc, UC_ARM_REG_R5, &r5);
                uc_reg_read(uc, UC_ARM_REG_R6, &r6);
                uc_reg_read(uc, UC_ARM_REG_R7, &r7);
                printf("[JJFB_E9K_R4_GATE] pc=0x2EFAFA r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "r4=0x%X r5=0x%X r6=0x%X r7=0x%X r9=0x%X sp=0x%X lr=0x%X "
                       "tick=%u evidence=OBSERVED\n",
                       r0, r1, r2, r3, r4, r5, r6, r7, r9w, sp, lr, g_e8f.tick);
                fflush(stdout);
                e9k_dump_slots(uc, r9w, r4, bp->pc, "r4_gate");
                e9k_log_postr4(bp->pc, lr, r0, r1, r2, r3, r4, r9w, sp, 0, 0, 0, count, bd0,
                               r4 ? "r4_cmp_nz" : "r4_cmp_zero");
            }
            if (!r4) {
                printf("[JJFB_E9I_CLASS] class=SPLASH_BLOCKED_BY_R4_ZERO evidence=OBSERVED\n");
                printf("[JJFB_E9J_CLASS] class=SPLASH_BLOCKED_BY_BD0_ZERO evidence=OBSERVED\n");
                if (g_e8f.e9k_mode) {
                    printf("[JJFB_E9K_CLASS] class=SPLASH_POST_R4_BLOCKED_BY_R4_FIELD "
                           "evidence=OBSERVED\n");
                    fflush(stdout);
                    e9k_try_hold("r4_zero_blocked");
                }
            } else {
                g_e8f.e9j_post_r4_reached = 1;
                if (g_e8f.e9j_progress_assist)
                    printf("[JJFB_E9J_CLASS] class=SPLASH_PROGRESS_OBJECT_ASSIST_REACHED_POST_R4 "
                           "r4=0x%X evidence=OBSERVED\n",
                           r4);
            }
            fflush(stdout);
        } else if (bp->pc == 0x2EFAF2u) {
            /* Hook fires BEFORE ldr r0,[r0]; r0 is &B6C (R9+0xB6C), not the value. */
            uint32_t b6c_addr = r0;
            uint32_t b6c = 0, bd0 = 0, count = 0;
            g_e8f.e9h_r4_gate_n++;
            if (r9w) {
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xB6Cu, &b6c);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBD0u, &bd0);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x2Cu,
                                               &count);
            } else if (b6c_addr)
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, b6c_addr, &b6c);
            printf("[JJFB_E9H_R4_GATE] pc=0x2EFAF2 B6C=0x%X b6c_addr=0x%X r4=0x%X BD0=0x%X "
                   "count=0x%X tick=%u note=%s evidence=OBSERVED\n",
                   b6c, b6c_addr, r4, bd0, count, g_e8f.tick,
                   b6c ? "b6c_nz_skip_r4_to_post_r4" : "b6c_zero_need_r4");
            e9j_log_postr4(bp->pc, lr, b6c, r4, b6c, bd0, count,
                           b6c ? "b6c_nz" : "b6c_zero");
            e9h_log_r4(bp->pc, lr, r4, b6c, r1, r9w, b6c ? "gate_b6c_nz" : "gate_b6c_zero");
            if (b6c) {
                g_e8f.e9j_post_r4_reached = 1;
                /* B6C!=0 skips r4 cmp and branches to 0x2EFB0E — not a hard block. */
                printf("[JJFB_E9J_CLASS] class=SPLASH_PROGRESS_OBJECT_ASSIST_REACHED_POST_R4 "
                       "note=b6c_nz_bypass_r4_to_2EFB0E B6C=0x%X evidence=OBSERVED\n",
                       b6c);
                if (g_e8f.e9k_mode)
                    printf("[JJFB_E9K_CLASS] class=SPLASH_POST_R4_REACHED_NEXT_GAP "
                           "note=via_b6c_nz_skip_r4 B6C=0x%X evidence=OBSERVED\n",
                           b6c);
            }
            fflush(stdout);
        } else if (bp->pc == 0x2EFA9Eu) {
            uint32_t count = 0, bar = 0, bd0 = 0;
            if (r9w) {
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x2Cu,
                                               &count);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x20u,
                                               &bar);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBD0u, &bd0);
            }
            printf("[JJFB_E9H_PROGRESS] pc=0x2EFA9E r4=0x%X count=%u bar=0x%X BD0=0x%X "
                   "tick=%u evidence=OBSERVED\n",
                   r4, count, bar, bd0, g_e8f.tick);
            e9j_log_postr4(bp->pc, lr, count, r4, 0, bd0, count, "progress_loop_enter");
            e9h_log_r4(bp->pc, lr, r4, count, bar, r9w, "progress_loop");
            fflush(stdout);
        } else if (bp->pc == 0x2EFAE2u) {
            printf("[JJFB_E9J_PROGRESS_BLIT] pc=0x2EFAE2 r0=0x%X r1=0x%X r2=0x%X r4=0x%X "
                   "tick=%u note=bar_segment_callsite evidence=OBSERVED\n",
                   r0, r1, r2, r4, g_e8f.tick);
            e9j_log_postr4(bp->pc, lr, r0, r4, 0, 0, 0, "progress_blit_2EFAE2");
            fflush(stdout);
        } else if (bp->pc == 0x2EFB0Eu) {
            g_e8f.e9j_post_r4_reached = 1;
            printf("[JJFB_E9J_POST_R4] pc=0x2EFB0E r4=0x%X tick=%u "
                   "note=text_progress_path_enter evidence=OBSERVED\n",
                   r4, g_e8f.tick);
            e9j_log_postr4(bp->pc, lr, r0, r4, 0, 0, 0, "post_r4_2EFB0E");
            if (g_e8f.e9j_progress_assist)
                printf("[JJFB_E9J_CLASS] class=SPLASH_PROGRESS_OBJECT_ASSIST_REACHED_POST_R4 "
                       "evidence=OBSERVED\n");
            if (g_e8f.e9k_mode) {
                uint32_t r5 = 0, r6 = 0, r7 = 0;
                uint32_t slot818 = 0, slot81c = 0;
                uc_reg_read(uc, UC_ARM_REG_R5, &r5);
                uc_reg_read(uc, UC_ARM_REG_R6, &r6);
                uc_reg_read(uc, UC_ARM_REG_R7, &r7);
                if (r9w) {
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x818u, &slot818);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x81Cu, &slot81c);
                }
                printf("[JJFB_E9K_POST_R4] pc=0x2EFB0E r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "r4=0x%X r5=0x%X r6=0x%X r7=0x%X r9=0x%X sp=0x%X lr=0x%X "
                       "R9_818=0x%X R9_81C=0x%X tick=%u evidence=OBSERVED\n",
                       r0, r1, r2, r3, r4, r5, r6, r7, r9w, sp, lr, slot818, slot81c,
                       g_e8f.tick);
                e9k_dump_slots(uc, r9w, r4, bp->pc, "post_r4_2EFB0E");
                e9k_log_postr4(bp->pc, lr, r0, r1, r2, r3, r4, r9w, sp, 0, 0, 0, 0, 0,
                               "post_r4_2EFB0E");
                printf("[JJFB_E9K_CLASS] class=SPLASH_POST_R4_REACHED_NEXT_GAP "
                       "note=await_305BFC_or_textbar evidence=OBSERVED\n");
                if (g_e8f.e9l_mode)
                    e9l_seed_textctx(uc, r9w);
                fflush(stdout);
            }
            fflush(stdout);
        } else if (bp->pc == 0x2EFBA2u || bp->pc == 0x305BFCu) {
            if (g_e8f.e9m_mode && bp->pc == 0x2EFBA2u) {
                uint32_t old_r1 = r1;
                e9m_open_trace_files();
                if (e9m_try_layout_assist(uc, &r1)) {
                    printf("[JJFB_TEXT_LAYOUT_ASSIST] pc=0x2EFBA2 old_r1=0x%X new_r1=0x%X "
                           "r2=0x%X meas_w=%u meas_h=%u note=x_from_W_minus_height_div2 "
                           "FAST_TEXT_LAYOUT_ASSIST NOT_PRODUCT_SUCCESS evidence=OBSERVED\n",
                           old_r1, r1, r2, g_e8f.e9m_meas_w, g_e8f.e9m_meas_h);
                    e9m_log_layout(bp->pc, lr, old_r1, r1, r2, g_e8f.e9m_meas_w,
                                   g_e8f.e9m_meas_h, "layout_assist");
                    fflush(stdout);
                }
            }
            printf("[JJFB_E9J_POST_R4_DRAW] pc=0x%X r0=0x%X r1=0x%X r2=0x%X r4=0x%X "
                   "tick=%u note=text_draw_305bfc evidence=OBSERVED\n",
                   bp->pc, r0, r1, r2, r4, g_e8f.tick);
            e9j_log_postr4(bp->pc, lr, r0, r4, 0, 0, 0, "text_draw");
            printf("[JJFB_E9J_CLASS] class=SPLASH_PROGRESS_OBJECT_ASSIST_REACHED_POST_R4 "
                   "note=real_text_draw_api evidence=OBSERVED\n");
            if (g_e8f.e9k_mode) {
                uint32_t r5 = 0, r6 = 0, r7 = 0;
                uint32_t stk0 = 0, stk1 = 0, stk2 = 0, stk3 = 0;
                uc_reg_read(uc, UC_ARM_REG_R5, &r5);
                uc_reg_read(uc, UC_ARM_REG_R6, &r6);
                uc_reg_read(uc, UC_ARM_REG_R7, &r7);
                if (sp) {
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp, &stk0);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 4u, &stk1);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 8u, &stk2);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 12u, &stk3);
                }
                g_e8f.e9k_305bfc_hit = 1;
                printf("[JJFB_E9K_305BFC] pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "r4=0x%X r5=0x%X r6=0x%X r7=0x%X sp=[0x%X,0x%X,0x%X,0x%X] "
                       "lr=0x%X tick=%u note=text_draw_ABI evidence=OBSERVED\n",
                       bp->pc, r0, r1, r2, r3, r4, r5, r6, r7, stk0, stk1, stk2, stk3, lr,
                       g_e8f.tick);
                e9k_dump_slots(uc, r9w, r0 ? r0 : r4, bp->pc, "305BFC");
                e9k_log_postr4(bp->pc, lr, r0, r1, r2, r3, r4, r9w, sp, 0, 0, 0, 0, 0,
                               bp->pc == 0x305BFCu ? "305BFC" : "2EFBA2_bl");
                e9k_log_draw(bp->pc, lr, "status_text", (int)r1, (int)r2, 0, 0, "305BFC");
                printf("[JJFB_E9K_CLASS] class=SPLASH_305BFC_REACHED_NEXT_GAP "
                       "pc=0x%X evidence=OBSERVED\n",
                       bp->pc);
                if (g_e8f.e9l_mode) {
                    g_e8f.e9l_305bfc_hit = 1;
                    e9l_log_text(bp->pc, lr, r0, r1, r2, r3, "305BFC");
                    printf("[JJFB_E9L_CLASS] class=TEXTCTX_ASSIST_REACHED_305BFC "
                           "r0=0x%X r1=0x%X r2=0x%X evidence=OBSERVED\n",
                           r0, r1, r2);
                    printf("[JJFB_E9L_CLASS] class=SPLASH_305BFC_REACHED_NEXT_GAP "
                           "evidence=OBSERVED\n");
                }
                if (g_e8f.e9m_mode && bp->pc == 0x305BFCu) {
                    g_e8f.e9m_last_r1 = r1;
                    e9m_log_abi(bp->pc, lr, r0, r1, r2, r3, g_e8f.e9m_meas_w, g_e8f.e9m_meas_h,
                                "305BFC_enter");
                    if (e9m_xy_plausible(r1, r2) && r0) {
                        g_e8f.e9m_valid_305bfc_args = 1;
                        g_e8f.e9m_str_va = r0;
                        jjfb_plat_11f00_note_guest_cstr(r0, (int16_t)(r1 & 0xFFFFu),
                                                        (int16_t)(r2 & 0xFFFFu), r3);
                        printf("[JJFB_E9M_CLASS] class=TEXT_305BFC_REACHED_VALID_ARGS_NEXT_GAP "
                               "r0=0x%X r1=0x%X r2=0x%X r3=0x%X evidence=OBSERVED\n",
                               r0, r1, r2, r3);
                    } else {
                        printf("[JJFB_E9M_CLASS] class=TEXT_305BFC_BLOCKED_BY_BAD_R1 "
                               "r1=0x%X r2=0x%X note=expect_x_0_240_y_0_320 evidence=OBSERVED\n",
                               r1, r2);
                    }
                    fflush(stdout);
                }
                /* Do NOT hold at 305BFC entry — text draw must run first. */
            }
            fflush(stdout);
        } else if (bp->pc == 0x305EA0u) {
            /* After BL 0x304558 (plat 0x12340): r4=&width r7=&height still live. */
            if (g_e8f.e9m_mode) {
                uint32_t r4m = 0, r7m = 0, old_w = 0, old_h = 0;
                uc_reg_read(uc, UC_ARM_REG_R4, &r4m);
                uc_reg_read(uc, UC_ARM_REG_R7, &r7m);
                if (r4m)
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r4m, &old_w);
                if (r7m)
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r7m, &old_h);
                printf("[JJFB_E9M_MEASURE_OUT] pc=0x305EA0 width_ptr=0x%X height_ptr=0x%X "
                       "old_w=%u old_h=%u str=0x%X tick=%u note=plat_0x12340_outs "
                       "evidence=OBSERVED\n",
                       r4m, r7m, old_w, old_h, g_e8f.e9m_str_va, g_e8f.tick);
                e9m_log_meas(bp->pc, lr, 0x12340u, g_e8f.e9m_str_va, old_w, old_h, old_w, old_h,
                             "plat_return");
                if (e9m_try_measure_shim(uc, bp->pc)) {
                    uint32_t nw = 0, nh = 0;
                    if (r4m)
                        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r4m, &nw);
                    if (r7m)
                        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r7m, &nh);
                    printf("[JJFB_TEXT_MEASURE_SHIM] text_va=0x%X w=%u h=%u old_w=%u old_h=%u "
                           "note=GBK_char_est_12x16 FAST_TEXT_MEASURE_SHIM NOT_PRODUCT_SUCCESS "
                           "evidence=OBSERVED\n",
                           g_e8f.e9m_str_va, nw, nh, old_w, old_h);
                    e9m_log_meas(bp->pc, lr, 0x12340u, g_e8f.e9m_str_va, old_w, old_h, nw, nh,
                                 "shim_write");
                    printf("[JJFB_E9M_CLASS] class=TEXT_MEASURE_ABI_FIXED_NEXT_GAP "
                           "w=%u h=%u evidence=OBSERVED\n",
                           nw, nh);
                } else if (old_w == 0 || old_h == 0 || old_h > 320u ||
                           (int32_t)old_h < 0 || old_w > 240u) {
                    printf("[JJFB_E9M_CLASS] class=TEXT_305BFC_BLOCKED_BY_BAD_R1 "
                           "note=measure_outs_garbage_need_shim old_w=%u old_h=%u "
                           "evidence=OBSERVED\n",
                           old_w, old_h);
                }
                fflush(stdout);
            }
        } else if (bp->pc == 0x2EFB48u) {
            /* Return from 0x305E78: log measured width/height locals. */
            if (g_e8f.e9m_mode && sp) {
                uint32_t mw = 0, mh = 0;
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x18u, &mw);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x1Cu, &mh);
                g_e8f.e9m_meas_w = mw;
                g_e8f.e9m_meas_h = mh;
                printf("[JJFB_E9M_305E78_RET] pc=0x2EFB48 width=%u height=%u "
                       "note=x_will_be_(W-height)/2 evidence=OBSERVED\n",
                       mw, mh);
                e9m_log_meas(bp->pc, lr, 0, g_e8f.e9m_str_va, mw, mh, mw, mh, "305E78_ret");
                fflush(stdout);
            }
        } else if (bp->pc == 0x303C50u) {
            if (g_e8f.e9m_mode) {
                printf("[JJFB_E9M_303C50] r0=0x%X r1=0x%X r2=0x%X r3=0x%X lr=0x%X tick=%u "
                       "note=DSM_measure_trampoline evidence=OBSERVED\n",
                       r0, r1, r2, r3, lr, g_e8f.tick);
                e9m_log_meas(bp->pc, lr, 0, r0, 0, 0, 0, 0, "303C50_enter");
                fflush(stdout);
            }
        } else if (bp->pc == 0x305C2Eu) {
            if (g_e8f.e9m_mode) {
                g_e8f.e9m_draw_entered = 1;
                if (g_e8f.e9n_mode) g_e8f.e9n_wrapper_clip_pass = 1;
                printf("[JJFB_E9M_305C3C_CALL] pc=0x305C2E r0=0x%X r1=0x%X r2=0x%X tick=%u "
                       "note=clip_pass_enter_real_text_draw evidence=OBSERVED\n",
                       r0, r1, r2, g_e8f.tick);
                e9m_log_abi(bp->pc, lr, r0, r1, r2, r3, g_e8f.e9m_meas_w, g_e8f.e9m_meas_h,
                            "draw_305C3C");
                fflush(stdout);
            }
        } else if (bp->pc == 0x305C32u) {
            if (g_e8f.e9m_mode) {
                if (g_e8f.e9n_mode && g_e8f.e9n_wrapper_clip_pass) {
                    g_e8f.e9n_core_returned = 1;
                    g_e8f.e9n_core_ret_r0 = r0;
                    printf("[JJFB_E9N_305C32_RET] pc=0x305C32 r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                           "lr=0x%X tick=%u note=305C3C_normal_return NOT_clip_skip "
                           "evidence=OBSERVED\n",
                           r0, r1, r2, r3, lr, g_e8f.tick);
                    e9n_log_clip(bp->pc, lr, r0, r1, r2, r3, 0, 0, 1, "core_return");
                    if (g_e8f.e9n_glyph_blit_hit || g_e8f.e9n_plat_draw_hit) {
                        printf("[JJFB_E9N_CLASS] class=TEXT_305C3C_REACHED_PLATFORM_DRAW "
                               "evidence=OBSERVED\n");
                    } else if (g_e8f.e9n_inner_clip_skip) {
                        printf("[JJFB_E9N_CLASS] class=TEXT_305C3C_BLOCKED_BY_CLIP "
                               "evidence=OBSERVED\n");
                    } else if (g_e8f.e9n_draw_mode_miss && !g_e8f.e9n_glyph_blit_hit) {
                        printf("[JJFB_E9N_CLASS] class=TEXT_305C3C_BLOCKED_BY_PLATFORM_TEXT_API "
                               "note=draw_mode_or_303C50_gate evidence=OBSERVED\n");
                    } else if (!g_e8f.e9n_dsm_draw_hit && !g_e8f.e9n_glyph_lookup_hit) {
                        printf("[JJFB_E9N_CLASS] class=TEXT_305C3C_BLOCKED_BY_GLYPH_MISSING "
                               "evidence=OBSERVED\n");
                    } else {
                        printf("[JJFB_E9N_CLASS] class=TEXT_DRAW_RETURNS_NO_PIXELS "
                               "evidence=OBSERVED\n");
                    }
                    fflush(stdout);
                } else {
                    g_e8f.e9m_clip_skipped = 1;
                    printf("[JJFB_E9M_CLASS] class=TEXT_305BFC_RETURNS_NO_DRAW "
                           "note=clip_skip_W_lt_x evidence=OBSERVED\n");
                    if (g_e8f.e9n_mode) {
                        printf("[JJFB_E9N_CLASS] class=TEXT_305C3C_BLOCKED_BY_CLIP "
                               "note=wrapper_clip_skip evidence=OBSERVED\n");
                        e9n_log_clip(bp->pc, lr, r0, r1, r2, r3, 0, 0, 0, "wrapper_clip_skip");
                    }
                }
                e9m_log_abi(bp->pc, lr, r0, r1, r2, r3, g_e8f.e9m_meas_w, g_e8f.e9m_meas_h,
                            g_e8f.e9n_wrapper_clip_pass ? "core_return" : "clip_skip");
                fflush(stdout);
            }
        } else if (bp->pc == 0x305C3Cu) {
            if (g_e8f.e9m_mode) {
                uint32_t scr_w = 0, scr_h = 0, yoff = 0, dim818 = 0;
                char sbuf[48];
                memset(sbuf, 0, sizeof(sbuf));
                if (g_e8f.e9n_mode) {
                    g_e8f.e9n_core_entered = 1;
                    e9n_open_trace_files();
                    e9n_read_textctx(uc, r9w, &dim818, &scr_h, &yoff, &scr_w);
                    {
                        uint32_t dim81c = 0;
                        if (r9w)
                            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x81Cu,
                                                           &dim81c);
                        (void)e9d_peek_cstr((uc_engine *)uc, r0, sbuf, (int)sizeof(sbuf), NULL, 0,
                                            NULL);
                        printf("[JJFB_E9N_305C3C_ENTER] r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                               "str=\"%s\" R9_830=%u R9_824=%u R9_828=%u R9_818=%u R9_81C=%u "
                               "tick=%u evidence=OBSERVED\n",
                               r0, r1, r2, r3, sbuf, scr_w, scr_h, yoff, dim818, dim81c,
                               g_e8f.tick);
                        if (r0)
                            jjfb_plat_11f00_note_guest_cstr(r0, (int16_t)(r1 & 0xFFFFu),
                                                            (int16_t)(r2 & 0xFFFFu), 0xFFu);
                        e9n_log_305c3c(bp->pc, lr, r0, r1, r2, r3, r4, r9w, sp, scr_w, scr_h,
                                       dim818, dim81c, sbuf, "305C3C_enter");
                    }
                    e9n_log_clip(bp->pc, lr, r0, r1, r2, r3, scr_w, r1,
                                 scr_w >= r1 ? 1 : 0, "inner_clip_precheck");
                    fflush(stdout);
                }
                printf("[JJFB_E9M_TEXT_DRAW_CORE] pc=0x305C3C r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "tick=%u note=inner_text_draw evidence=OBSERVED\n",
                       r0, r1, r2, r3, g_e8f.tick);
                e9m_log_abi(bp->pc, lr, r0, r1, r2, r3, g_e8f.e9m_meas_w, g_e8f.e9m_meas_h,
                            "305C3C_enter");
                fflush(stdout);
            }
        } else if (bp->pc == 0x305C54u) {
            if (g_e8f.e9n_mode) {
                uint32_t scr_w = 0;
                e9n_read_textctx(uc, r9w, NULL, NULL, NULL, &scr_w);
                printf("[JJFB_E9N_INNER_CLIP] pc=0x305C54 scr_w=%u x=%u pass=%d tick=%u "
                       "evidence=OBSERVED\n",
                       scr_w, r1, scr_w >= r1 ? 1 : 0, g_e8f.tick);
                e9n_log_clip(bp->pc, lr, r0, r1, r2, r3, scr_w, r1, scr_w >= r1 ? 1 : 0,
                             "inner_w_vs_x");
                if (scr_w < r1) g_e8f.e9n_inner_clip_skip = 1;
                fflush(stdout);
            }
        } else if (bp->pc == 0x305CA0u) {
            if (g_e8f.e9n_mode) {
                /* Shared epilogue: early clip exit AND normal return after 2F2360.
                 * Positive-x path may BL 2F2360 from 0x305D90 (skip 0x305D58). */
                if (g_e8f.e9n_glyph_blit_hit || g_e8f.e9n_plat_draw_hit) {
                    printf("[JJFB_E9N_305CA0_EPILOGUE] pc=0x305CA0 r0=0x%X tick=%u "
                           "note=normal_exit_after_glyph_blit evidence=OBSERVED\n",
                           r0, g_e8f.tick);
                    e9n_log_clip(bp->pc, lr, r0, r1, r2, r3, 0, 0, 1, "epilogue_after_blit");
                } else {
                    g_e8f.e9n_inner_clip_skip = 1;
                    printf("[JJFB_E9N_INNER_CLIP_SKIP] pc=0x305CA0 r0=0x%X r4=0x%X tick=%u "
                           "note=305C3C_early_exit evidence=OBSERVED\n",
                           r0, r4, g_e8f.tick);
                    e9n_log_clip(bp->pc, lr, r0, r1, r2, r3, 0, 0, 0, "inner_clip_skip");
                    printf("[JJFB_E9N_CLASS] class=TEXT_305C3C_BLOCKED_BY_CLIP "
                           "evidence=OBSERVED\n");
                }
                fflush(stdout);
            }
        } else if (bp->pc == 0x305C90u) {
            if (g_e8f.e9n_mode) {
                char sbuf[48];
                memset(sbuf, 0, sizeof(sbuf));
                (void)e9d_peek_cstr((uc_engine *)uc, r4, sbuf, (int)sizeof(sbuf), NULL, 0, NULL);
                g_e8f.e9n_glyph_lookup_hit = 1;
                g_e8f.e9n_last_glyph_type = r0;
                printf("[JJFB_E9N_GLYPH_LOOKUP] pc=0x305C90 pre_bl str=0x%X \"%s\" tick=%u "
                       "evidence=OBSERVED\n",
                       r4, sbuf, g_e8f.tick);
                e9n_log_glyph(bp->pc, lr, r4, r1, r2, r3, 0, r4, sbuf, "pre_2F99A4");
                fflush(stdout);
            }
        } else if (bp->pc == 0x305C98u) {
            if (g_e8f.e9n_mode) {
                g_e8f.e9n_last_glyph_type = r0;
                printf("[JJFB_E9N_GLYPH_TYPE] pc=0x305C98 type=%u branch=%s tick=%u "
                       "evidence=OBSERVED\n",
                       r0, r0 > 1u ? "wide_path" : "narrow_path", g_e8f.tick);
                e9n_log_glyph(bp->pc, lr, r0, r1, r2, r3, r0, r4, "", "glyph_type_check");
                fflush(stdout);
            }
        } else if (bp->pc == 0x305D08u) {
            if (g_e8f.e9n_mode) {
                uint32_t r5v = 0;
                char sbuf[48];
                memset(sbuf, 0, sizeof(sbuf));
                uc_reg_read((uc_engine *)uc, UC_ARM_REG_R5, &r5v);
                (void)e9d_peek_cstr((uc_engine *)uc, r0, sbuf, (int)sizeof(sbuf), NULL, 0, NULL);
                g_e8f.e9n_dsm_draw_hit = 1;
                printf("[JJFB_E9N_DSM_DRAW] pc=0x305D08 str=0x%X \"%s\" x=%u tick=%u "
                       "note=BL_303C50_setup evidence=OBSERVED\n",
                       r0, sbuf, r5v, g_e8f.tick);
                e9n_log_glyph(bp->pc, lr, r0, r1, r2, r3, g_e8f.e9n_last_glyph_type, r0, sbuf,
                              "303C50_call");
                fflush(stdout);
            }
        } else if (bp->pc == 0x305D16u) {
            if (g_e8f.e9n_mode) {
                uint32_t draw_mode = 0;
                if (sp)
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x68u, &draw_mode);
                printf("[JJFB_E9N_DRAW_GATE] pc=0x305D16 draw_mode=%u dsm_ret=0x%X pass=%d "
                       "tick=%u evidence=OBSERVED\n",
                       draw_mode, r0, draw_mode == 1u ? 1 : 0, g_e8f.tick);
                e9n_log_glyph(bp->pc, lr, r0, draw_mode, r2, r3, g_e8f.e9n_last_glyph_type,
                              g_e8f.e9m_str_va, "", "draw_mode_gate");
                if (draw_mode != 1u) g_e8f.e9n_draw_mode_miss = 1;
                fflush(stdout);
            }
        } else if (bp->pc == 0x305D58u) {
            if (g_e8f.e9n_mode) {
                g_e8f.e9n_glyph_blit_hit = 1;
                printf("[JJFB_E9N_GLYPH_BLIT] pc=0x305D58 r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "lr=0x%X tick=%u note=BL_2F2360 evidence=OBSERVED\n",
                       r0, r1, r2, r3, lr, g_e8f.tick);
                e9n_log_glyph(bp->pc, lr, r0, r1, r2, r3, g_e8f.e9n_last_glyph_type,
                              g_e8f.e9m_str_va, "", "BL_2F2360");
                printf("[JJFB_E9N_CLASS] class=TEXT_305C3C_REACHED_PLATFORM_DRAW "
                       "evidence=OBSERVED\n");
                fflush(stdout);
            }
        } else if (bp->pc == 0x305D5Cu) {
            if (g_e8f.e9n_mode) {
                printf("[JJFB_E9N_DRAW_SKIP] pc=0x305D5C r0=0x%X note=glyph_blit_skipped "
                       "tick=%u evidence=OBSERVED\n",
                       r0, g_e8f.tick);
                if (!g_e8f.e9n_glyph_blit_hit) g_e8f.e9n_draw_mode_miss = 1;
                e9n_log_glyph(bp->pc, lr, r0, r1, r2, r3, g_e8f.e9n_last_glyph_type, r4, "",
                              "draw_skip");
                fflush(stdout);
            }
        } else if (bp->pc == 0x2F2360u) {
            if (g_e8f.e9n_mode) {
                /* Positive-x path: BL 0x2F2360 @ 0x305D90 (not always via 0x305D58). */
                g_e8f.e9n_glyph_blit_hit = 1;
                printf("[JJFB_E9N_GLYPH_BLIT_CORE] pc=0x2F2360 r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "lr=0x%X tick=%u evidence=OBSERVED\n",
                       r0, r1, r2, r3, lr, g_e8f.tick);
                e9n_log_glyph(bp->pc, lr, r0, r1, r2, r3, g_e8f.e9n_last_glyph_type,
                              g_e8f.e9m_str_va, "", "2F2360_enter");
                printf("[JJFB_E9N_CLASS] class=TEXT_305C3C_REACHED_PLATFORM_DRAW "
                       "evidence=OBSERVED\n");
                fflush(stdout);
            }
        } else if (bp->pc == 0x2F99A4u) {
            if (g_e8f.e9n_mode && g_e8f.e9n_glyphtrace) {
                printf("[JJFB_E9N_GLYPH_TBL] pc=0x2F99A4 r0=0x%X lr=0x%X tick=%u "
                       "evidence=OBSERVED\n",
                       r0, lr, g_e8f.tick);
                e9n_log_glyph(bp->pc, lr, r0, r1, r2, r3, 0, r0, "", "2F99A4_enter");
                fflush(stdout);
            }
        } else if (bp->pc == 0x304558u) {
            if (g_e8f.e9n_mode) {
                printf("[JJFB_E9N_PLATFORM_TEXT] pc=0x304558 r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "lr=0x%X tick=%u note=%s evidence=OBSERVED\n",
                       r0, r1, r2, r3, lr, g_e8f.tick,
                       r0 == 0x11F00u ? "plat_11F00_drawText"
                       : (r0 == 0x12340u ? "plat_12340_measure" : "plat_bridge"));
                if (r0 == 0x11F00u) {
                    g_e8f.e9n_plat_draw_hit = 1;
                    printf("[JJFB_E9N_11F00] pc=0x304558 app=0x%X code=0x%X param0=0x%X "
                           "tick=%u note=drawText_platform_api evidence=OBSERVED\n",
                           r1, r2, r3, g_e8f.tick);
                }
                e9n_log_glyph(bp->pc, lr, r0, r1, r2, r3, 0, 0, "",
                              r0 == 0x11F00u ? "304558_11F00" : "304558_plat_bridge");
                fflush(stdout);
            }
        } else if (bp->pc == 0x2EFBA6u) {
            /* Return site after BL 0x305BFC — text path attempted. */
            if (g_e8f.e9k_mode) {
                printf("[JJFB_E9K_305BFC_RET] pc=0x2EFBA6 r0=0x%X r4=0x%X tick=%u "
                       "note=after_text_draw evidence=OBSERVED\n",
                       r0, r4, g_e8f.tick);
                e9k_log_postr4(bp->pc, lr, r0, r1, r2, r3, r4, r9w, sp, 0, 0, 0, 0, 0,
                               "305BFC_ret");
                if (g_e8f.e9l_mode) {
                    e9l_log_text(bp->pc, lr, r0, r1, r2, r3, "305BFC_ret");
                    printf("[JJFB_E9L_305BFC_RET] r0=0x%X tick=%u note=text_api_returned "
                           "evidence=OBSERVED\n",
                           r0, g_e8f.tick);
                }
                if (g_e8f.e9m_mode) {
                    e9m_log_abi(bp->pc, lr, r0, r1, r2, r3, g_e8f.e9m_meas_w, g_e8f.e9m_meas_h,
                                "305BFC_ret");
                    if (g_e8f.e9m_valid_305bfc_args && g_e8f.e9m_draw_entered &&
                        (!g_e8f.e9m_clip_skipped ||
                         (g_e8f.e9n_mode && g_e8f.e9n_wrapper_clip_pass))) {
                        if (g_e8f.e9n_mode && g_e8f.e9n_glyph_blit_hit &&
                            getenv("JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM") &&
                            getenv("JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM")[0] == '1') {
                            /* pixels may come from 11F00 compat — still NOT_PRODUCT */
                            printf("[JJFB_E9M_CLASS] class=SPLASH_TEXT_DRAWN_NO_REWRITE "
                                   "note=valid_args_and_305C3C_entered NOT_PRODUCT "
                                   "evidence=OBSERVED\n");
                        } else if (g_e8f.e9n_mode) {
                            printf("[JJFB_E9M_CLASS] class=TEXT_305BFC_REACHED_VALID_ARGS_NEXT_GAP "
                                   "note=e9n_defer_drawn_claim evidence=OBSERVED\n");
                        } else {
                            printf("[JJFB_E9M_CLASS] class=SPLASH_TEXT_DRAWN_NO_REWRITE "
                                   "note=valid_args_and_305C3C_entered NOT_PRODUCT "
                                   "evidence=OBSERVED\n");
                        }
                    } else if (g_e8f.e9m_layout_assist_done) {
                        printf("[JJFB_E9M_CLASS] class=TEXT_LAYOUT_R1_FIXED_NEXT_GAP "
                               "evidence=OBSERVED\n");
                    } else if (g_e8f.e9m_measure_shim_done) {
                        printf("[JJFB_E9M_CLASS] class=TEXT_MEASURE_ABI_FIXED_NEXT_GAP "
                               "evidence=OBSERVED\n");
                    }
                    fflush(stdout);
                }
                if (!g_e8f.e9k_stop_on_textbar || g_e8f.e9k_305bfc_hit || g_e8f.e9l_mode ||
                    g_e8f.e9m_mode || g_e8f.e9n_mode)
                    e9k_try_hold(g_e8f.e9n_mode ? "after_305BFC_e9n"
                                : (g_e8f.e9m_mode ? "after_305BFC_e9m"
                                                  : (g_e8f.e9l_mode ? "after_305BFC_e9l"
                                                                    : "after_305BFC")));
            }
            fflush(stdout);
        } else if (bp->pc == 0x2EFB08u) {
            /* blx r2 with r0=r4 — R9+0x143C strlen/validate before post-r4. */
            uint32_t fptr = 0;
            if (r9w)
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x143Cu, &fptr);
            printf("[JJFB_E9K_PRE_POST_R4_CALL] pc=0x2EFB08 r0=0x%X r2=0x%X fptr_R9_143C=0x%X "
                   "r4=0x%X tick=%u note=status_string_validate evidence=OBSERVED\n",
                   r0, r2, fptr, r4, g_e8f.tick);
            fflush(stdout);
            if (g_e8f.e9k_mode) {
                e9k_dump_slots(uc, r9w, r4, bp->pc, "pre_post_r4_blx");
                if (!fptr && !r2) {
                    printf("[JJFB_E9K_CLASS] class=SPLASH_POST_R4_BLOCKED_BY_NEW_PLATFORM_API "
                           "note=R9_143C_fptr_zero evidence=OBSERVED\n");
                    fflush(stdout);
                    e9k_try_hold("validate_fptr_zero");
                }
            }
            fflush(stdout);
        } else if (bp->pc == 0x2EFB0Au) {
            /* After blx validate: cmp r0,#0; beq exit. */
            printf("[JJFB_E9K_PRE_POST_R4_RET] pc=0x2EFB0A r0=0x%X r4=0x%X tick=%u "
                   "note=%s evidence=OBSERVED\n",
                   r0, r4, g_e8f.tick, r0 ? "validate_ok" : "validate_fail_exit");
            fflush(stdout);
            if (g_e8f.e9k_mode && !r0) {
                printf("[JJFB_E9K_CLASS] class=SPLASH_POST_R4_BLOCKED_BY_R4_FIELD "
                       "note=validate_returned_zero evidence=OBSERVED\n");
                fflush(stdout);
                e9k_try_hold("validate_returned_zero");
            }
            fflush(stdout);
        } else if (bp->pc == 0x2F2174u) {
            printf("[JJFB_E9K_TEXTBAR_LAYOUT] pc=0x2F2174 r0=0x%X r1=0x%X r4=0x%X tick=%u "
                   "note=uses_BA0_plus24_width evidence=OBSERVED\n",
                   r0, r1, r4, g_e8f.tick);
            if (g_e8f.e9k_mode) {
                e9k_dump_slots(uc, r9w, r4, bp->pc, "textbar_layout_2F2174");
                e9k_log_postr4(bp->pc, lr, r0, r1, r2, r3, r4, r9w, sp, 0, 0, 0, 0, 0,
                               "2F2174_layout");
            }
            if (g_e8f.e9l_mode) {
                g_e8f.e9l_2f2174_hit = 1;
                e9l_log_text(bp->pc, lr, r0, r1, r2, r3, "2F2174_layout");
                printf("[JJFB_E9L_CLASS] class=TEXTCTX_ASSIST_REACHED_2F2174 "
                       "r0=0x%X r1=0x%X evidence=OBSERVED\n",
                       r0, r1);
            }
            fflush(stdout);
        } else if (bp->pc == 0x2FC444u) {
            /* STR r0,[r4,#0x30] — natural BD0 writer after 2d9648 concat. */
            printf("[JJFB_E9J_BD0_WRITER] pc=0x2FC444 value=0x%X lr=0x%X tick=%u "
                   "note=status_string_store evidence=OBSERVED\n",
                   r0, lr, g_e8f.tick);
            e9j_log_writer(bp->pc, lr, 0xBD0u, 0, r0, "2FC444_str_BA0_plus30");
            printf("[JJFB_E9J_CLASS] class=SPLASH_PROGRESS_OBJECT_SOURCE_FOUND_NEXT_GAP "
                   "writer=0x2FC444 value=0x%X evidence=OBSERVED\n",
                   r0);
            fflush(stdout);
        } else if (bp->pc == 0x30ED98u) {
            printf("[JJFB_E9J_B6C_WRITER] pc=0x30ED98 value=0x%X lr=0x%X tick=%u "
                   "evidence=OBSERVED\n",
                   r0, lr, g_e8f.tick);
            e9j_log_writer(bp->pc, lr, 0xB6Cu, 0, r0, "30ED98_str_B6C");
            fflush(stdout);
        }
    }

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
            /* E9G: arm splash for lifecycle (do not nest uc_emu_start from CODE hook). */
            if (bp->pc == 0x2E88CCu && g_e8f.e9g_mode && g_e8f.e9g_splash_call &&
                !g_e8f.e9g_splash_done && !g_e8f.e9g_debug_only) {
                printf("[JJFB_E9G_SPLASH_ARM] via=0x2E88CC tick=%u note=defer_to_lifecycle "
                       "NOT_PRODUCT evidence=OBSERVED\n",
                       g_e8f.tick);
                fflush(stdout);
            }
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
                if (g_e8f.e9f_mode) {
                    e9f_open_trace_files();
                    if (e9f_try_rewrite_request(uc, &r1, bp->pc)) {
                        g_e8f.e8y_last_name_va = r1;
                        e8y_dump_name(uc, r1);
                    }
                    if (g_e8f.e9f_req_csv) {
                        fprintf(g_e8f.e9f_req_csv,
                                "%u,%u,0x2D92E4,0x%X,\"%s\",0,0,0x0,0,enter\n",
                                g_e8f.e8y_2d92e4_n, g_e8f.tick, lr, g_e8f.e8y_last_name);
                        fflush(g_e8f.e9f_req_csv);
                    }
                }
                if (g_e8f.e9g_mode) {
                    uint32_t ui_mode = 0;
                    if (r9)
                        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc,
                                                       r9 + E8I_STATE_OFF, &ui_mode);
                    e9g_open_trace_files();
                    e9g_log_req_csv(0x2D92E4u, lr, ui_mode, g_e8f.e8y_last_name, 0, 0, 0, 0,
                                    "enter");
                    /* E9H: skip is done at splash PC 0x2EFA46→0x2EFA5C (preserve regs).
                     * Never jump from 2D92E4. */
                    if (g_e8f.e9g_skip_sibling_binds && lr >= 0x2EF86Cu && lr < 0x2EFB00u &&
                        strstr(g_e8f.e8y_last_name, "loadingbar") == NULL && !g_e8f.e9h_mode) {
                        uint32_t cont = 0x2EFA5Cu | 1u;
                        uc_reg_write(uc, UC_ARM_REG_PC, &cont);
                        printf("[JJFB_E9H_SPLASH_CONT_BLIT] name=\"%s\" lr=0x%X "
                               "cont=0x2EFA5C note=legacy_2D92E4_skip NOT_PRODUCT "
                               "evidence=OBSERVED\n",
                               g_e8f.e8y_last_name, lr);
                        fflush(stdout);
                        return;
                    }
                    /* E9I: bar/textbar/top stall in guest index miss path — complete via
                     * same original-MRP postmatch bytes, return handle to splash LR. */
                    if (g_e8f.e9i_mode && !g_e8f.e9i_skip_sibling &&
                        lr >= 0x2EF86Cu && lr < 0x2EFB00u &&
                        e9i_is_splash_sibling(g_e8f.e8y_last_name)) {
                        if (e9i_try_splash_sibling_resolve(uc, g_e8f.e8y_last_name, lr))
                            return;
                    }
                }
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
                if (g_e8f.e9f_mode) {
                    e9f_open_trace_files();
                    if (e9f_try_rewrite_request(uc, &r1, bp->pc))
                        e8y_dump_name(uc, r1);
                }
                printf("[JJFB_E8Z_304BF0] lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "name=\"%s\" note=mrp_member_lookup evidence=OBSERVED\n",
                       lr, r0, r1, r2, r3, g_e8f.e8y_last_name);
                if (g_e8f.e9d_mode || g_e8f.e9e_mode || g_e8f.e9f_mode) {
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
                    if (g_e8f.e9g_mode) {
                        uint32_t ui_mode = 0;
                        if (r9)
                            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc,
                                                           r9 + E8I_STATE_OFF, &ui_mode);
                        e9g_open_trace_files();
                        e9g_log_req_csv(0x304BF0u, lr, ui_mode, g_e8f.e8y_last_name, 0, 0, 0, 0,
                                        "enter_304BF0");
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
                    int allow_skip = 1;
                    /* E9F multi: only skip siblings after preferred UI member is loaded. */
                    if (g_e8f.e9f_multi && g_e8f.e9f_prefer[0] &&
                        !strstr(g_e8f.e8y_last_name, g_e8f.e9f_prefer) &&
                        strstr(g_e8f.e8y_last_name, "slogo") == NULL &&
                        strstr(g_e8f.e8y_last_name, "loadingbar") == NULL)
                        allow_skip = 0;
                    if (allow_skip) {
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
            if (g_e8f.e9k_mode && bp->pc == 0x305E78u &&
                ((lr & ~1u) == 0x2EFB48u || (lr & ~1u) == 0x2EFB44u)) {
                uint32_t slot818 = 0, slot81c = 0, r9w = r9;
                if ((!r9w || !find_robotol_r9(uc, &r9w)) && r9) r9w = r9;
                if (r9w) {
                    if (g_e8f.e9l_mode)
                        e9l_seed_textctx(uc, r9w);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x818u, &slot818);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0x81Cu, &slot81c);
                }
                printf("[JJFB_E9K_TEXT_MEASURE] pc=0x305E78 r0=0x%X r1=0x%X r2=0x%X "
                       "R9_818=0x%X R9_81C=0x%X lr=0x%X tick=%u "
                       "note=pre_305BFC_needs_font_ctx evidence=OBSERVED\n",
                       r0, r1, r2, slot818, slot81c, lr, g_e8f.tick);
                if (g_e8f.e9l_mode) {
                    printf("[JJFB_E9L_305E78] r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
                           "R9_818=0x%X R9_81C=0x%X lr=0x%X tick=%u evidence=OBSERVED\n",
                           r0, r1, r2, r3, slot818, slot81c, lr, g_e8f.tick);
                    e9l_log_305e78(bp->pc, lr, r0, r1, r2, r3, slot818, slot81c, "enter");
                }
                if (g_e8f.e9m_mode) {
                    g_e8f.e9m_str_va = r0;
                    jjfb_plat_11f00_note_guest_cstr(r0, 0, 0, 0);
                    e9m_open_trace_files();
                    e9m_log_abi(bp->pc, lr, r0, r1, r2, r3, 0, 0, "305E78_enter");
                    printf("[JJFB_E9M_305E78_ENTER] str=0x%X r1=0x%X r2=0x%X r3=0x%X "
                           "note=r3_height_out_sp0_width_out evidence=OBSERVED\n",
                           r0, r1, r2, r3);
                    fflush(stdout);
                }
                if ((!slot81c || !r1) && !(g_e8f.e9l_mode && g_e8f.e9l_textctx_assist &&
                                           slot818 && slot81c)) {
                    printf("[JJFB_E9K_CLASS] class=SPLASH_POST_R4_BLOCKED_BY_R4_FIELD "
                           "note=R9_81C_or_r1_zero_text_measure evidence=OBSERVED\n");
                    if (g_e8f.e9l_mode)
                        printf("[JJFB_E9L_CLASS] class=SPLASH_TEXT_BLOCKED_BY_305E78_FIELD "
                               "evidence=OBSERVED\n");
                    fflush(stdout);
                    if (!g_e8f.e9l_mode)
                        e9k_try_hold("text_measure_missing_R9_81C");
                } else if (g_e8f.e9l_mode && slot818 && slot81c) {
                    g_e8f.e9l_305e78_ok = 1;
                    printf("[JJFB_E9L_CLASS] class=TEXTCTX_INIT_REACHED_NEXT_GAP "
                           "note=305E78_dims_ok_continue NOT_PRODUCT evidence=OBSERVED\n");
                    fflush(stdout);
                }
            }
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
    if (g_e8f.e9g_mode) {
        e9g_open_trace_files();
        e9g_log_uimode_csv(pc, lr, oldv, newv, r9, "state_watch");
        if (newv == 0x45u) {
            g_e8f.e9g_uimode_45_n++;
            printf("[JJFB_E9G_UI_MODE_45] pc=0x%X tick=%u class=SPLASH_UI_MODE_45_REACHED "
                   "evidence=OBSERVED\n",
                   pc, g_e8f.tick);
            fflush(stdout);
        }
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
    if (g_e8f.e9g_mode && idx[0]) {
        e9g_collect_idx_name(idx);
        e9g_audit_slogo_once(0);
    }
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
        /* E9E/E9F: skip slow post-match DSM file/inflate; fill object with original bytes. */
        if (g_e8f.e9e_postmatch_shims &&
            (!g_e8f.e9e_postmatch_done || g_e8f.e9f_multi) && !e9e_name_already_done(req)) {
            if (e9e_try_postmatch_complete(uc, req)) {
                if (g_e8f.e9g_mode) e9g_audit_slogo_once(1);
                return 1;
            }
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
        if (g_e8f.e9g_mode || g_e8f.e9h_mode) {
            add_bp(0x2EF86Cu, "gSplash");
            add_bp(0x30662Cu, "gSplashBl");
            add_bp(0x306344u, "gModeCmp");
            add_bp(0x2FC418u, "gUiWrite");
            add_bp(0x2EFA33u, "gLoadBar");
            add_bp(0x2EFA46u, "gBarBl");
            add_bp(0x2EFA43u, "gBar");
            add_bp(0x2EFA56u, "gTextBarBl");
            add_bp(0x2EFA53u, "gTextBar");
            add_bp(0x2EFA5Cu, "gYCalc");
            add_bp(0x2EFA7Cu, "gBlitSetup");
            add_bp(0x2EFA9Au, "gBlitCall");
            add_bp(0x2EC6B8u, "g2EC6B8");
            add_bp(0x2EFA9Eu, "gProg");
            add_bp(0x2EFAE2u, "gProgBlit");
            add_bp(0x2EFAF2u, "gR4Gate");
            add_bp(0x2EFAFAu, "gR4Cmp");
            if (g_e8f.e9j_mode || g_e8f.e9k_mode) {
                add_bp(0x2EFB0Eu, "jPostR4");
                add_bp(0x2EFBA2u, "jTextCall");
                add_bp(0x2EFBA6u, "jTextRet");
                add_bp(0x305BFCu, "j305BFC");
                add_bp(0x2EFB08u, "jPrePost");
                add_bp(0x2EFB0Au, "jPreRet");
                add_bp(0x2F2174u, "jTbLayout");
                add_bp(0x2FC444u, "jBd0Str");
                add_bp(0x30ED98u, "jB6cStr");
                if (g_e8f.e9m_mode) {
                    add_bp(0x305EA0u, "mMeasRet");
                    add_bp(0x2EFB48u, "mMeasCallerRet");
                    add_bp(0x303C50u, "m303C50");
                    add_bp(0x305C2Eu, "mClipPass");
                    add_bp(0x305C32u, "mClipSkip");
                    add_bp(0x305C3Cu, "mTextCore");
                    e9m_open_trace_files();
                }
                if (g_e8f.e9n_mode) {
                    add_bp(0x305C54u, "nInnerClip");
                    add_bp(0x305CA0u, "nInnerSkip");
                    add_bp(0x305C90u, "nGlyphPre");
                    add_bp(0x305C98u, "nGlyphType");
                    add_bp(0x305D08u, "nDsmDraw");
                    add_bp(0x305D16u, "nDrawGate");
                    add_bp(0x305D58u, "nGlyphBlit");
                    add_bp(0x305D5Cu, "nDrawSkip");
                    add_bp(0x2F2360u, "n2F2360");
                    add_bp(0x2F99A4u, "n2F99A4");
                    add_bp(0x304558u, "n304558");
                    e9n_open_trace_files();
                }
                e9j_open_trace_files();
                if (g_e8f.e9k_mode) e9k_open_trace_files();
                if (g_e8f.e9l_mode) e9l_open_trace_files();
                printf("[JJFB_E9J_BP] post_r4=0x2EFB0E text=0x2EFBA2/0x305BFC "
                       "bd0_writer=0x2FC444 b6c_writer=0x30ED98 evidence=HYPOTHESIS\n");
                if (g_e8f.e9k_mode)
                    printf("[JJFB_E9K_BP] gate=0x2EFAFA pre=0x2EFB08 post=0x2EFB0E "
                           "text=0x2EFBA2/0x305BFC ret=0x2EFBA6 layout=0x2F2174 "
                           "evidence=HYPOTHESIS\n");
                if (g_e8f.e9l_mode)
                    printf("[JJFB_E9L_BP] dims=R9+0x818/0x81C measure=0x305E78 "
                           "layout=0x2F2174 text=0x305BFC evidence=HYPOTHESIS\n");
                if (g_e8f.e9m_mode)
                    printf("[JJFB_E9M_BP] measure_ret=0x305EA0 caller_ret=0x2EFB48 "
                           "dsm=0x303C50 clip=0x305C2E/32 core=0x305C3C "
                           "evidence=HYPOTHESIS\n");
                if (g_e8f.e9n_mode)
                    printf("[JJFB_E9N_BP] core=0x305C3C inner_clip=0x305C54/0x305CA0 "
                           "glyph=0x305C90/98 blit=0x305D58/0x2F2360 plat=0x304558 "
                           "evidence=HYPOTHESIS\n");
            }
            e9g_open_trace_files();
            if (g_e8f.e9h_mode) e9h_open_trace_files();
            if (g_e8f.e9i_mode) e9i_open_trace_files();
            if (g_e8f.e9j_mode) e9j_open_trace_files();
            printf("[JJFB_E9H_BP] splash=0x2EF86C blit=0x2EFA9A,0x2EC6B8 ycalc=0x2EFA5C "
                   "gate=0x2EFAFA bind=0x2EFA36/46/56 base=0x2D8DF4 evidence=HYPOTHESIS\n");
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
    if (g_e8f.e9h_r4_trace) {
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.e9h_splash_hook, UC_HOOK_CODE,
                         (void *)on_e9h_splash_insn, NULL, 0x2EF86Cull, 0x2EFB20ull);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E9H_R4_TRACE] arm_fail uc_err=%u evidence=OBSERVED\n", (unsigned)ue);
        } else {
            printf("[JJFB_E9H_R4_TRACE] armed=1 range=0x2EF86C..0x2EFB20 evidence=HYPOTHESIS\n");
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

static int e9e_name_already_done(const char *name) {
    uint32_t i;
    if (!name || !name[0]) return 1;
    for (i = 0; i < g_e8f.e9e_postmatch_n && i < 8u; i++) {
        if (strcmp(g_e8f.e9e_done_names[i], name) == 0) return 1;
    }
    return 0;
}

static void e9f_open_trace_files(void) {
    const char *req = getenv("JJFB_E9F_REQUEST_CSV");
    const char *res = getenv("JJFB_E9F_RESULTS_JSONL");
    if (!g_e8f.e9f_req_csv && req && req[0]) {
        g_e8f.e9f_req_csv = fopen(req, "w");
        if (g_e8f.e9f_req_csv) {
            fprintf(g_e8f.e9f_req_csv,
                    "n,tick,pc,lr,name,natural_entry,postmatch,handle,bridge,note\n");
            fflush(g_e8f.e9f_req_csv);
        }
    }
    if (!g_e8f.e9f_results_jsonl && res && res[0]) {
        g_e8f.e9f_results_jsonl = fopen(res, "w");
    }
}

static void e9g_open_trace_files(void) {
    const char *req = getenv("JJFB_E9G_REQUEST_CSV");
    const char *ui = getenv("JJFB_E9G_UIMODE_CSV");
    if (!g_e8f.e9g_mode) return;
    if (!g_e8f.e9g_req_csv && req && req[0]) {
        g_e8f.e9g_req_csv = fopen(req, "w");
        if (g_e8f.e9g_req_csv) {
            fprintf(g_e8f.e9g_req_csv,
                    "n,tick,pc,lr,ui_mode,name,natural_entry,postmatch,handle,draw_pc,note\n");
            fflush(g_e8f.e9g_req_csv);
        }
    }
    if (!g_e8f.e9g_uimode_csv && ui && ui[0]) {
        g_e8f.e9g_uimode_csv = fopen(ui, "w");
        if (g_e8f.e9g_uimode_csv) {
            fprintf(g_e8f.e9g_uimode_csv, "n,tick,pc,lr,old,new,r9,note\n");
            fflush(g_e8f.e9g_uimode_csv);
        }
    }
}

static void e9h_open_trace_files(void) {
    const char *r4 = getenv("JJFB_E9H_R4_CSV");
    const char *seq = getenv("JJFB_E9H_SEQ_CSV");
    if (!g_e8f.e9h_mode) return;
    if (!g_e8f.e9h_r4_csv && r4 && r4[0]) {
        g_e8f.e9h_r4_csv = fopen(r4, "w");
        if (g_e8f.e9h_r4_csv) {
            fprintf(g_e8f.e9h_r4_csv, "n,tick,pc,lr,r4,r0,r1,r9,note\n");
            fflush(g_e8f.e9h_r4_csv);
        }
    }
    if (!g_e8f.e9h_seq_csv && seq && seq[0]) {
        g_e8f.e9h_seq_csv = fopen(seq, "w");
        if (g_e8f.e9h_seq_csv) {
            fprintf(g_e8f.e9h_seq_csv,
                    "n,tick,pc,lr,name,natural_entry,postmatch,handle,pixels,w,h,note\n");
            fflush(g_e8f.e9h_seq_csv);
        }
    }
}

static void e9h_log_r4(uint32_t pc, uint32_t lr, uint32_t r4, uint32_t r0, uint32_t r1,
                       uint32_t r9, const char *note) {
    static uint32_t n;
    if (!g_e8f.e9h_r4_csv) return;
    n++;
    fprintf(g_e8f.e9h_r4_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", n, g_e8f.tick, pc, lr,
            r4, r0, r1, r9, note ? note : "");
    fflush(g_e8f.e9h_r4_csv);
}

static void e9i_open_trace_files(void) {
    const char *coord = getenv("JJFB_E9I_COORD_CSV");
    const char *r4 = getenv("JJFB_E9I_R4_CSV");
    const char *seq = getenv("JJFB_E9I_SEQ_CSV");
    if (!g_e8f.e9i_mode) return;
    if (!g_e8f.e9i_coord_csv && coord && coord[0]) {
        g_e8f.e9i_coord_csv = fopen(coord, "w");
        if (g_e8f.e9i_coord_csv) {
            fprintf(g_e8f.e9i_coord_csv,
                    "n,tick,pc,lr,r0,r5,r6,r7,R9_830,R9_834,note\n");
            fflush(g_e8f.e9i_coord_csv);
        }
    }
    if (!g_e8f.e9i_r4_csv && r4 && r4[0]) {
        g_e8f.e9i_r4_csv = fopen(r4, "w");
        if (g_e8f.e9i_r4_csv) {
            fprintf(g_e8f.e9i_r4_csv, "n,tick,pc,lr,r4,r0,r1,r9,note\n");
            fflush(g_e8f.e9i_r4_csv);
        }
    }
    if (!g_e8f.e9i_seq_csv && seq && seq[0]) {
        g_e8f.e9i_seq_csv = fopen(seq, "w");
        if (g_e8f.e9i_seq_csv) {
            fprintf(g_e8f.e9i_seq_csv,
                    "n,tick,pc,lr,name,natural_entry,postmatch,handle,pixels,w,h,note\n");
            fflush(g_e8f.e9i_seq_csv);
        }
    }
    /* Mirror into E9H CSV handles so existing e9h_log_r4 / seq writers work. */
    if (!g_e8f.e9h_r4_csv && g_e8f.e9i_r4_csv)
        g_e8f.e9h_r4_csv = g_e8f.e9i_r4_csv;
    if (!g_e8f.e9h_seq_csv && g_e8f.e9i_seq_csv)
        g_e8f.e9h_seq_csv = g_e8f.e9i_seq_csv;
    e9h_open_trace_files();
}

static void e9i_log_coord(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r5, uint32_t r6,
                          uint32_t r7, uint32_t a830, uint32_t a834, const char *note) {
    static uint32_t n;
    if (!g_e8f.e9i_coord_csv) return;
    n++;
    fprintf(g_e8f.e9i_coord_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", n,
            g_e8f.tick, pc, lr, r0, r5, r6, r7, a830, a834, note ? note : "");
    fflush(g_e8f.e9i_coord_csv);
}

static void e9i_seed_scr_dims(void *uc, uint32_t r9) {
    uint32_t a830 = 0, a834 = 0;
    if (!uc || !r9 || g_e8f.e9i_scr_dim_seeded) return;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x830u, &a830);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x834u, &a834);
    printf("[JJFB_E9I_SCR_DIM] before R9_830=0x%X R9_834=0x%X note=2F9970_W_2F9964_Ybase "
           "evidence=OBSERVED\n",
           a830, a834);
    if (!a830) {
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x830u, 240u);
        a830 = 240u;
    }
    if (!a834) {
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x834u, 320u);
        a834 = 320u;
    }
    g_e8f.e9i_scr_dim_seeded = 1;
    printf("[JJFB_E9I_SCR_DIM_SEED] R9_830=%u R9_834=%u "
           "note=natural_xy_formula_x=(W-bw)/2_y=H-100 NOT_PRODUCT evidence=OBSERVED\n",
           a830, a834);
    e9i_log_coord(0, 0, 0, 0, 0, 0, a830, a834, "scr_dim_seed");
    fflush(stdout);
}

/* E9L: R9+0x818/0x81C are screen/layout dims (same family as 830/834) read by caller
 * 0x2EFB38/40 into r2/r1 before BL 0x305E78. No robotol STR writer observed; seed under
 * FAST_TEXTCTX_ASSIST only — dims from real 240x320 surface, not invented pixels. */
static void e9l_seed_textctx(void *uc, uint32_t r9) {
    uint32_t a818 = 0, a81c = 0, a830 = 0, a834 = 0;
    if (!uc || !r9 || !g_e8f.e9l_mode) return;
    e9l_open_trace_files();
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x818u, &a818);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x81Cu, &a81c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x830u, &a830);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x834u, &a834);
    printf("[JJFB_E9L_818_81C] before R9_818=0x%X R9_81C=0x%X R9_830=0x%X R9_834=0x%X "
           "note=screen_dims_for_305E78 evidence=OBSERVED\n",
           a818, a81c, a830, a834);
    e9l_log_writer(0, 0, 0x818u, a818, a818, "before_seed");
    e9l_log_writer(0, 0, 0x81Cu, a81c, a81c, "before_seed");
    if (!a818 && !a81c)
        printf("[JJFB_E9L_CLASS] class=TEXTCTX_818_81C_WRITER_FOUND_NEXT_GAP "
               "note=no_robotol_STR_writer_dims_like_830_834 evidence=OBSERVED\n");
    if (!g_e8f.e9l_textctx_assist) {
        if (!a818 || !a81c)
            printf("[JJFB_E9L_CLASS] class=SPLASH_TEXT_BLOCKED_BY_FONT_CONTEXT "
                   "note=need_FAST_TEXTCTX_ASSIST_or_natural_writer evidence=OBSERVED\n");
        fflush(stdout);
        return;
    }
    if (g_e8f.e9l_textctx_assist_done && a818 && a81c) return;
    if (!a818) {
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x818u, 240u);
        e9l_log_writer(0, 0, 0x818u, 0, 240u, "assist_seed_818_w");
        a818 = 240u;
    }
    if (!a81c) {
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x81Cu, 320u);
        e9l_log_writer(0, 0, 0x81Cu, 0, 320u, "assist_seed_81C_h");
        a81c = 320u;
    }
    if (!a830)
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x830u, 240u);
    if (!a834)
        (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x834u, 320u);
    {
        uint32_t a824 = 0;
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x824u, &a824);
        if (!a824) {
            (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0x824u, 320u);
            e9l_log_writer(0, 0, 0x824u, 0, 320u, "assist_seed_824_h_clip");
        }
    }
    g_e8f.e9l_textctx_assist_done = 1;
    printf("[JJFB_FAST_TEXTCTX_ASSIST] R9_818=%u R9_81C=%u note=real_240x320_dims_only "
           "FAST_TEXTCTX_ASSIST NOT_PRODUCT_SUCCESS evidence=OBSERVED\n",
           a818, a81c);
    printf("[JJFB_E9L_CLASS] class=TEXTCTX_INIT_REACHED_NEXT_GAP "
           "note=dims_seeded_await_305E78 evidence=OBSERVED\n");
    fflush(stdout);
}

/* E9J: R9+0xBD0 is a status C-string (writer 0x2FC444 via 2d9648 concat), loaded into
 * r4 at splash 0x2EF886. BA0+0x2C is progress tick count for bar segments at 0x2EFAE2.
 * Assist only under JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST — uses real robotol string. */
static void e9j_seed_progress_object(void *uc, uint32_t r9) {
    uint32_t bd0 = 0, count = 0, b6c = 0;
    uint32_t str_va = 0x314338u; /* GBK "请稍候" in robotol.ext */
    uint8_t probe[8];
    if (!uc || !r9 || !g_e8f.e9j_mode) return;
    e9j_open_trace_files();
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xBD0u, &bd0);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xB6Cu, &b6c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xBA0u + 0x2Cu, &count);
    printf("[JJFB_E9J_BD0] before BD0=0x%X B6C=0x%X count=%u note=splash_obj_plus30 "
           "evidence=OBSERVED\n",
           bd0, b6c, count);
    e9j_log_writer(0, 0, 0xBD0u, bd0, bd0, "before_seed");
    if (!bd0)
        printf("[JJFB_E9J_CLASS] class=SPLASH_PROGRESS_OBJECT_SOURCE_FOUND_NEXT_GAP "
               "writer=0x2FC418/0x2FC444 note=natural_writer_not_reached_yet "
               "evidence=OBSERVED\n");
    if (!g_e8f.e9j_progress_assist) {
        if (!bd0)
            printf("[JJFB_E9J_CLASS] class=SPLASH_BLOCKED_BY_BD0_ZERO "
                   "note=natural_mode_no_assist evidence=OBSERVED\n");
        fflush(stdout);
        return;
    }
    if (g_e8f.e9j_progress_assist_done) return;
    memset(probe, 0, sizeof(probe));
    if (!guest_memory_uc_peek((struct uc_struct *)uc, str_va, probe, 6) ||
        probe[0] != 0xC7u || probe[1] != 0xEBu) {
        /* Fallback first hit of same GBK sequence if layout drifts. */
        str_va = 0x3136D7u;
        memset(probe, 0, sizeof(probe));
        (void)guest_memory_uc_peek((struct uc_struct *)uc, str_va, probe, 6);
    }
    if (probe[0] != 0xC7u || probe[1] != 0xEBu) {
        printf("[JJFB_E9J_CLASS] class=SPLASH_BLOCKED_BY_PROGRESS_OBJECT_FIELD "
               "note=status_string_not_in_guest_map evidence=OBSERVED\n");
        fflush(stdout);
        return;
    }
    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xBD0u, str_va);
    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9 + 0xBA0u + 0x2Cu,
                                   g_e8f.e9j_progress_count_assist);
    g_e8f.e9j_bd0_str_va = str_va;
    g_e8f.e9j_progress_assist_done = 1;
    printf("[JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST] BD0=0x%X str_va=0x%X "
           "count=%u note=real_robotol_status_string_plus_progress_count "
           "FAST_PROGRESS_OBJECT_ASSIST NOT_PRODUCT_SUCCESS evidence=OBSERVED\n",
           str_va, str_va, g_e8f.e9j_progress_count_assist);
    e9j_log_writer(0, 0, 0xBD0u, bd0, str_va, "assist_seed_BD0");
    e9j_log_writer(0, 0, 0xBA0u + 0x2Cu, count, g_e8f.e9j_progress_count_assist,
                   "assist_seed_count");
    fflush(stdout);
}

static void e9j_open_trace_files(void) {
    const char *w = getenv("JJFB_E9J_WRITER_CSV");
    const char *p = getenv("JJFB_E9J_POSTR4_CSV");
    if (!g_e8f.e9j_mode) return;
    if (!g_e8f.e9j_writer_csv && w && w[0]) {
        g_e8f.e9j_writer_csv = fopen(w, "w");
        if (g_e8f.e9j_writer_csv) {
            fprintf(g_e8f.e9j_writer_csv, "n,tick,pc,lr,off,old,new,note\n");
            fflush(g_e8f.e9j_writer_csv);
        }
    }
    if (!g_e8f.e9j_postr4_csv && p && p[0]) {
        g_e8f.e9j_postr4_csv = fopen(p, "w");
        if (g_e8f.e9j_postr4_csv) {
            fprintf(g_e8f.e9j_postr4_csv, "n,tick,pc,lr,r0,r4,b6c,bd0,count,note\n");
            fflush(g_e8f.e9j_postr4_csv);
        }
    }
}

static void e9j_log_writer(uint32_t pc, uint32_t lr, uint32_t off, uint32_t old_v, uint32_t new_v,
                           const char *note) {
    static uint32_t n;
    if (!g_e8f.e9j_writer_csv) return;
    n++;
    fprintf(g_e8f.e9j_writer_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", n, g_e8f.tick, pc, lr,
            off, old_v, new_v, note ? note : "");
    fflush(g_e8f.e9j_writer_csv);
}

static void e9j_log_postr4(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r4, uint32_t b6c,
                           uint32_t bd0, uint32_t count, const char *note) {
    static uint32_t n;
    if (!g_e8f.e9j_postr4_csv) return;
    n++;
    fprintf(g_e8f.e9j_postr4_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", n,
            g_e8f.tick, pc, lr, r0, r4, b6c, bd0, count, note ? note : "");
    fflush(g_e8f.e9j_postr4_csv);
}

static void e9k_open_trace_files(void) {
    const char *p = getenv("JJFB_E9K_POSTR4_CSV");
    const char *d = getenv("JJFB_E9K_DRAW_CSV");
    if (!g_e8f.e9k_postr4_csv && p && p[0]) {
        g_e8f.e9k_postr4_csv = fopen(p, "w");
        if (g_e8f.e9k_postr4_csv) {
            fprintf(g_e8f.e9k_postr4_csv,
                    "n,tick,pc,lr,r0,r1,r2,r3,r4,r9,sp,bar,textbar,loadingbar,count,bd0,note\n");
            fflush(g_e8f.e9k_postr4_csv);
        }
    }
    if (!g_e8f.e9k_draw_csv && d && d[0]) {
        g_e8f.e9k_draw_csv = fopen(d, "w");
        if (g_e8f.e9k_draw_csv) {
            fprintf(g_e8f.e9k_draw_csv, "n,tick,pc,lr,member,x,y,w,h,note\n");
            fflush(g_e8f.e9k_draw_csv);
        }
    }
}

static void e9k_log_postr4(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                           uint32_t r3, uint32_t r4, uint32_t r9, uint32_t sp, uint32_t bar,
                           uint32_t textbar, uint32_t loadingbar, uint32_t count, uint32_t bd0,
                           const char *note) {
    static uint32_t n;
    e9k_open_trace_files();
    if (!g_e8f.e9k_postr4_csv) return;
    n++;
    fprintf(g_e8f.e9k_postr4_csv,
            "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n",
            n, g_e8f.tick, pc, lr, r0, r1, r2, r3, r4, r9, sp, bar, textbar, loadingbar, count,
            bd0, note ? note : "");
    fflush(g_e8f.e9k_postr4_csv);
}

static void e9k_log_draw(uint32_t pc, uint32_t lr, const char *member, int x, int y, int w, int h,
                         const char *note) {
    static uint32_t n;
    e9k_open_trace_files();
    if (!g_e8f.e9k_draw_csv) return;
    n++;
    fprintf(g_e8f.e9k_draw_csv, "%u,%u,0x%X,0x%X,\"%s\",%d,%d,%d,%d,%s\n", n, g_e8f.tick, pc, lr,
            member ? member : "", x, y, w, h, note ? note : "");
    fflush(g_e8f.e9k_draw_csv);
}

static void e9k_dump_slots(void *uc, uint32_t r9w, uint32_t r4, uint32_t pc, const char *tag) {
    uint32_t bar = 0, tb = 0, lb = 0, count = 0, bd0 = 0;
    char r4buf[68];
    int i;
    memset(r4buf, 0, sizeof(r4buf));
    if (!uc || !r9w) return;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x20u, &bar);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x24u, &tb);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x28u, &lb);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBA0u + 0x2Cu, &count);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9w + 0xBD0u, &bd0);
    if (r4 >= 0x80000u && r4 < 0x4000000u) {
        if (guest_memory_uc_peek((struct uc_struct *)uc, r4, (uint8_t *)r4buf, 32)) {
            for (i = 0; i < 32; i++) {
                unsigned char c = (unsigned char)r4buf[i];
                if (c == 0) break;
                if (c < 0x20u || c > 0x7eu) r4buf[i] = '.';
            }
        }
    }
    printf("[JJFB_E9K_SLOTS] pc=0x%X tag=%s bar=0x%X textbar=0x%X loadingbar=0x%X "
           "count=%u BD0=0x%X r4=0x%X r4_str=\"%s\" evidence=OBSERVED\n",
           pc, tag ? tag : "?", bar, tb, lb, count, bd0, r4, r4buf);
    fflush(stdout);
    e9k_log_postr4(pc, 0, 0, 0, 0, 0, r4, r9w, 0, bar, tb, lb, count, bd0, tag);
}

static void e9k_try_hold(const char *reason) {
    if (!g_e8f.e9k_mode || g_e8f.e9k_hold_armed) return;
    g_e8f.e9k_hold_armed = 1;
    printf("[JJFB_E9K_HOLD_ARM] reason=%s textbar=%d hit305=%d post_r4=%d "
           "evidence=OBSERVED\n",
           reason ? reason : "?", g_e8f.e9k_textbar_drawn, g_e8f.e9k_305bfc_hit,
           g_e8f.e9j_post_r4_reached);
    fflush(stdout);
    jjfb_e9k_request_hold(reason);
}

static void e9l_open_trace_files(void) {
    const char *w = getenv("JJFB_E9L_WRITER_CSV");
    const char *m = getenv("JJFB_E9L_305E78_CSV");
    const char *t = getenv("JJFB_E9L_TEXT_CSV");
    if (!g_e8f.e9l_writer_csv && w && w[0]) {
        g_e8f.e9l_writer_csv = fopen(w, "w");
        if (g_e8f.e9l_writer_csv) {
            fprintf(g_e8f.e9l_writer_csv, "n,tick,pc,lr,off,old,new,note\n");
            fflush(g_e8f.e9l_writer_csv);
        }
    }
    if (!g_e8f.e9l_305e78_csv && m && m[0]) {
        g_e8f.e9l_305e78_csv = fopen(m, "w");
        if (g_e8f.e9l_305e78_csv) {
            fprintf(g_e8f.e9l_305e78_csv, "n,tick,pc,lr,r0,r1,r2,r3,a818,a81c,note\n");
            fflush(g_e8f.e9l_305e78_csv);
        }
    }
    if (!g_e8f.e9l_text_csv && t && t[0]) {
        g_e8f.e9l_text_csv = fopen(t, "w");
        if (g_e8f.e9l_text_csv) {
            fprintf(g_e8f.e9l_text_csv, "n,tick,pc,lr,r0,r1,r2,r3,note\n");
            fflush(g_e8f.e9l_text_csv);
        }
    }
}

static void e9l_log_writer(uint32_t pc, uint32_t lr, uint32_t off, uint32_t old_v, uint32_t new_v,
                           const char *note) {
    static uint32_t n;
    e9l_open_trace_files();
    if (!g_e8f.e9l_writer_csv) return;
    n++;
    fprintf(g_e8f.e9l_writer_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", n, g_e8f.tick, pc, lr,
            off, old_v, new_v, note ? note : "");
    fflush(g_e8f.e9l_writer_csv);
}

static void e9l_log_305e78(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                           uint32_t r3, uint32_t a818, uint32_t a81c, const char *note) {
    static uint32_t n;
    e9l_open_trace_files();
    if (!g_e8f.e9l_305e78_csv) return;
    n++;
    fprintf(g_e8f.e9l_305e78_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", n,
            g_e8f.tick, pc, lr, r0, r1, r2, r3, a818, a81c, note ? note : "");
    fflush(g_e8f.e9l_305e78_csv);
}

static void e9l_log_text(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                         uint32_t r3, const char *note) {
    static uint32_t n;
    e9l_open_trace_files();
    if (!g_e8f.e9l_text_csv) return;
    n++;
    fprintf(g_e8f.e9l_text_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", n, g_e8f.tick, pc,
            lr, r0, r1, r2, r3, note ? note : "");
    fflush(g_e8f.e9l_text_csv);
}

/* E9M: platform text-measure (0x12340 via 304558) leaves height_out garbage →
 * caller x=(W-height)/2 becomes 0xFFE7917B. Shim writes plausible w/h only. */
static void e9m_open_trace_files(void) {
    const char *a = getenv("JJFB_E9M_ABI_CSV");
    const char *m = getenv("JJFB_E9M_MEAS_CSV");
    const char *l = getenv("JJFB_E9M_LAYOUT_CSV");
    if (!g_e8f.e9m_abi_csv && a && a[0]) {
        g_e8f.e9m_abi_csv = fopen(a, "w");
        if (g_e8f.e9m_abi_csv) {
            fprintf(g_e8f.e9m_abi_csv, "n,tick,pc,lr,r0,r1,r2,r3,w,h,note\n");
            fflush(g_e8f.e9m_abi_csv);
        }
    }
    if (!g_e8f.e9m_meas_csv && m && m[0]) {
        g_e8f.e9m_meas_csv = fopen(m, "w");
        if (g_e8f.e9m_meas_csv) {
            fprintf(g_e8f.e9m_meas_csv,
                    "n,tick,pc,lr,code,str,old_w,old_h,new_w,new_h,note\n");
            fflush(g_e8f.e9m_meas_csv);
        }
    }
    if (!g_e8f.e9m_layout_csv && l && l[0]) {
        g_e8f.e9m_layout_csv = fopen(l, "w");
        if (g_e8f.e9m_layout_csv) {
            fprintf(g_e8f.e9m_layout_csv, "n,tick,pc,lr,old_r1,new_r1,r2,w,h,note\n");
            fflush(g_e8f.e9m_layout_csv);
        }
    }
}

static void e9m_log_abi(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                        uint32_t r3, uint32_t w, uint32_t h, const char *note) {
    static uint32_t n;
    e9m_open_trace_files();
    if (!g_e8f.e9m_abi_csv) return;
    n++;
    fprintf(g_e8f.e9m_abi_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,%s\n", n, g_e8f.tick,
            pc, lr, r0, r1, r2, r3, w, h, note ? note : "");
    fflush(g_e8f.e9m_abi_csv);
}

static void e9m_log_meas(uint32_t pc, uint32_t lr, uint32_t code, uint32_t str, uint32_t old_w,
                         uint32_t old_h, uint32_t new_w, uint32_t new_h, const char *note) {
    static uint32_t n;
    e9m_open_trace_files();
    if (!g_e8f.e9m_meas_csv) return;
    n++;
    fprintf(g_e8f.e9m_meas_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,%u,%u,%u,%u,%s\n", n, g_e8f.tick, pc,
            lr, code, str, old_w, old_h, new_w, new_h, note ? note : "");
    fflush(g_e8f.e9m_meas_csv);
}

static void e9m_log_layout(uint32_t pc, uint32_t lr, uint32_t old_r1, uint32_t new_r1,
                           uint32_t r2, uint32_t w, uint32_t h, const char *note) {
    static uint32_t n;
    e9m_open_trace_files();
    if (!g_e8f.e9m_layout_csv) return;
    n++;
    fprintf(g_e8f.e9m_layout_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,%s\n", n, g_e8f.tick, pc,
            lr, old_r1, new_r1, r2, w, h, note ? note : "");
    fflush(g_e8f.e9m_layout_csv);
}

static int e9m_xy_plausible(uint32_t x, uint32_t y) {
    return (int32_t)x >= 0 && x <= 240u && (int32_t)y >= 0 && y <= 320u;
}

static int e9m_wh_plausible(uint32_t w, uint32_t h) {
    return w > 0u && w <= 240u && h > 0u && h <= 64u;
}

static int e9m_estimate_text_wh(void *uc, uint32_t str_va, uint32_t *w_out, uint32_t *h_out) {
    uint8_t buf[64];
    int i = 0, nchars = 0;
    if (!uc || !str_va || !w_out || !h_out) return 0;
    memset(buf, 0, sizeof(buf));
    if (!guest_memory_uc_peek((struct uc_struct *)uc, str_va, buf, (int)sizeof(buf) - 1))
        return 0;
    while (i < (int)sizeof(buf) - 1 && buf[i]) {
        if (buf[i] >= 0x81u && i + 1 < (int)sizeof(buf) - 1 && buf[i + 1]) {
            nchars++;
            i += 2;
        } else {
            nchars++;
            i++;
        }
    }
    if (nchars <= 0) return 0;
    /* Conservative Mythroad-like metrics: 12px/CJK glyph, 16px height (pre +2 pad). */
    *w_out = (uint32_t)(nchars * 12);
    *h_out = 16u;
    return 1;
}

static int e9m_try_measure_shim(void *uc, uint32_t pc) {
    uint32_t r4 = 0, r7 = 0, old_w = 0, old_h = 0, nw = 0, nh = 0;
    (void)pc;
    if (!uc || !g_e8f.e9m_mode || !g_e8f.e9m_measure_shim) return 0;
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R4, &r4);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_R7, &r7);
    if (!r4 || !r7) return 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r4, &old_w);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r7, &old_h);
    if (e9m_wh_plausible(old_w, old_h)) {
        g_e8f.e9m_meas_w = old_w;
        g_e8f.e9m_meas_h = old_h;
        return 0;
    }
    if (!e9m_estimate_text_wh(uc, g_e8f.e9m_str_va, &nw, &nh)) {
        /* Fallback for known splash status string length. */
        nw = 36u;
        nh = 16u;
    }
    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r4, nw);
    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r7, nh);
    g_e8f.e9m_meas_w = nw;
    g_e8f.e9m_meas_h = nh;
    g_e8f.e9m_measure_shim_done = 1;
    return 1;
}

static int e9m_try_layout_assist(void *uc, uint32_t *r1_io) {
    uint32_t r9 = 0, scr_w = 0, old_r1, new_r1, h, w;
    int32_t diff;
    if (!uc || !r1_io || !g_e8f.e9m_mode || !g_e8f.e9m_layout_assist) return 0;
    old_r1 = *r1_io;
    if (e9m_xy_plausible(old_r1, 0x100u)) return 0; /* r2 checked by caller separately */
    if (!find_robotol_r9(uc, &r9) || !r9) return 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x830u, &scr_w);
    if (!scr_w) scr_w = 240u;
    h = g_e8f.e9m_meas_h;
    w = g_e8f.e9m_meas_w;
    /* Caller formula: x = (W - height_out) / 2. Prefer same once height is fixed. */
    if (!e9m_wh_plausible(w ? w : 1u, h)) {
        if (!e9m_estimate_text_wh(uc, g_e8f.e9m_str_va, &w, &h)) {
            w = 36u;
            h = 16u;
        }
    }
    diff = (int32_t)scr_w - (int32_t)h;
    new_r1 = (uint32_t)(diff / 2);
    if (!e9m_xy_plausible(new_r1, 0)) {
        /* Alternate: center by measured text width (more typical). */
        diff = (int32_t)scr_w - (int32_t)w;
        new_r1 = (uint32_t)(diff / 2);
    }
    if (!e9m_xy_plausible(new_r1, 0)) return 0;
    *r1_io = new_r1;
    uc_reg_write((uc_engine *)uc, UC_ARM_REG_R1, &new_r1);
    g_e8f.e9m_layout_assist_done = 1;
    g_e8f.e9m_last_r1 = new_r1;
    return 1;
}

/* E9N: 0x305C3C inner clip / glyph / 2F2360 blit trace. */
static void e9n_open_trace_files(void) {
    const char *a = getenv("JJFB_E9N_305C3C_CSV");
    const char *c = getenv("JJFB_E9N_CLIP_CSV");
    const char *g = getenv("JJFB_E9N_GLYPH_CSV");
    if (!g_e8f.e9n_305c3c_csv && a && a[0]) {
        g_e8f.e9n_305c3c_csv = fopen(a, "w");
        if (g_e8f.e9n_305c3c_csv) {
            fprintf(g_e8f.e9n_305c3c_csv,
                    "n,tick,pc,lr,r0,r1,r2,r3,r4,r9,sp,scr_w,scr_h,d818,d81c,str,note\n");
            fflush(g_e8f.e9n_305c3c_csv);
        }
    }
    if (!g_e8f.e9n_clip_csv && c && c[0]) {
        g_e8f.e9n_clip_csv = fopen(c, "w");
        if (g_e8f.e9n_clip_csv) {
            fprintf(g_e8f.e9n_clip_csv,
                    "n,tick,pc,lr,r0,r1,r2,r3,cmp_a,cmp_b,pass,note\n");
            fflush(g_e8f.e9n_clip_csv);
        }
    }
    if (!g_e8f.e9n_glyph_csv && g && g[0]) {
        g_e8f.e9n_glyph_csv = fopen(g, "w");
        if (g_e8f.e9n_glyph_csv) {
            fprintf(g_e8f.e9n_glyph_csv,
                    "n,tick,pc,lr,r0,r1,r2,r3,glyph_type,str_va,str,note\n");
            fflush(g_e8f.e9n_glyph_csv);
        }
    }
}

static void e9n_read_textctx(void *uc, uint32_t r9, uint32_t *w, uint32_t *h, uint32_t *yoff,
                             uint32_t *scr_w) {
    if (w) *w = 0;
    if (h) *h = 0;
    if (yoff) *yoff = 0;
    if (scr_w) *scr_w = 0;
    if (!uc || !r9) return;
    if (scr_w)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x830u, scr_w);
    if (h) (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x824u, h);
    if (yoff) (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x828u, yoff);
    if (w) (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x818u, w);
}

static void e9n_log_305c3c(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                           uint32_t r3, uint32_t r4, uint32_t r9, uint32_t sp,
                           uint32_t scr_w, uint32_t scr_h, uint32_t dim818, uint32_t dim81c,
                           const char *str_ascii, const char *note) {
    static uint32_t n;
    e9n_open_trace_files();
    if (!g_e8f.e9n_305c3c_csv) return;
    n++;
    fprintf(g_e8f.e9n_305c3c_csv,
            "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,%u,%u,\"%s\",%s\n", n,
            g_e8f.tick, pc, lr, r0, r1, r2, r3, r4, r9, sp, scr_w, scr_h, dim818, dim81c,
            str_ascii ? str_ascii : "", note ? note : "");
    fflush(g_e8f.e9n_305c3c_csv);
}

static void e9n_log_clip(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                         uint32_t r3, uint32_t cmp_a, uint32_t cmp_b, int pass,
                         const char *note) {
    static uint32_t n;
    e9n_open_trace_files();
    if (!g_e8f.e9n_clip_csv) return;
    n++;
    fprintf(g_e8f.e9n_clip_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,%d,%s\n", n,
            g_e8f.tick, pc, lr, r0, r1, r2, r3, cmp_a, cmp_b, pass, note ? note : "");
    fflush(g_e8f.e9n_clip_csv);
}

static void e9n_log_glyph(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                          uint32_t r3, uint32_t glyph_type, uint32_t str_va,
                          const char *str_ascii, const char *note) {
    static uint32_t n;
    e9n_open_trace_files();
    if (!g_e8f.e9n_glyph_csv) return;
    n++;
    fprintf(g_e8f.e9n_glyph_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,0x%X,\"%s\",%s\n", n,
            g_e8f.tick, pc, lr, r0, r1, r2, r3, glyph_type, str_va,
            str_ascii ? str_ascii : "", note ? note : "");
    fflush(g_e8f.e9n_glyph_csv);
}

/* E9N: platform 0x11F00 drawText — host renders real guest splash string at real x/y.
 * Glyph blit lives in bridge (guiDrawBitmapSprite); launcher_core only calls the hook. */
int jjfb_e9n_try_plat_11f00_text_draw(void *uc, uint32_t app, uint32_t code_obj,
                                      uint32_t param0) {
    uint32_t str_va = 0;
    int16_t x = 0, y = 0;
    uint8_t buf[64];
    int nbytes = 0;
    const char *shim;
    (void)code_obj;
    shim = getenv("JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM");
    if (!shim || shim[0] != '1') return 0;
    if (!uc || !g_e8f.e9n_mode) return 0;
    str_va = g_e8f.e9m_str_va;
    if (!str_va || !g_e8f.e9m_valid_305bfc_args) return 0;
    if (param0) {
        (void)guest_memory_uc_peek((struct uc_struct *)uc, param0, &y, 2);
        (void)guest_memory_uc_peek((struct uc_struct *)uc, param0 + 2u, &x, 2);
    }
    if ((int32_t)x < 0 || (int32_t)y < 0 || x > 240 || y > 320 || (x == 0 && y == 0)) {
        x = (int16_t)(g_e8f.e9m_last_r1 & 0xFFFFu);
        y = 0x105;
    }
    memset(buf, 0, sizeof(buf));
    if (!guest_memory_uc_peek((struct uc_struct *)uc, str_va, buf, (int)sizeof(buf) - 1))
        return 0;
    if (!buf[0]) return 0;
    while (nbytes < (int)sizeof(buf) - 1 && buf[nbytes]) nbytes++;
    if (nbytes <= 0) return 0;
    if (!jjfb_e9n_host_draw_gbk((int)x, (int)y, buf, nbytes)) {
        printf("[JJFB_PLATFORM_TEXT_DRAW_COMPAT] api=0x11F00 fail=no_text_draw_fn "
               "str=0x%X evidence=OBSERVED\n",
               str_va);
        fflush(stdout);
        return 0;
    }
    g_e8f.e9n_plat_draw_hit = 1;
    printf("[JJFB_PLATFORM_TEXT_DRAW_COMPAT] api=0x11F00 app=0x%X str=0x%X @%d,%d "
           "bytes=%d note=real_guest_string_real_xy NOT_PRODUCT evidence=OBSERVED\n",
           app, str_va, (int)x, (int)y, nbytes);
    printf("[JJFB_E9N_CLASS] class=PLATFORM_TEXT_DRAW_COMPAT_RENDERED evidence=OBSERVED\n");
    fflush(stdout);
    return 1;
}

#ifdef GWY_HAVE_UNICORN
static void on_e9h_splash_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t r4 = 0, r0 = 0, r1 = 0, lr = 0, r9 = 0;
    (void)size;
    (void)user_data;
    if (!g_e8f.e9h_r4_trace) return;
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    if (r4 == g_e8f.e9h_r4_last) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    g_e8f.e9h_r4_change_n++;
    printf("[JJFB_E9H_R4_WRITE] pc=0x%X old=0x%X new=0x%X r0=0x%X lr=0x%X tick=%u "
           "evidence=OBSERVED\n",
           (uint32_t)address, g_e8f.e9h_r4_last, r4, r0, lr, g_e8f.tick);
    e9h_open_trace_files();
    e9h_log_r4((uint32_t)address, lr, r4, r0, r1, r9, "r4_change");
    g_e8f.e9h_r4_last = r4;
    fflush(stdout);
}
#endif

static void e9g_log_uimode_csv(uint32_t pc, uint32_t lr, uint32_t oldv, uint32_t newv,
                               uint32_t r9, const char *note) {
    static uint32_t n;
    if (!g_e8f.e9g_uimode_csv) return;
    n++;
    fprintf(g_e8f.e9g_uimode_csv, "%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", n, g_e8f.tick, pc, lr,
            oldv, newv, r9, note ? note : "");
    fflush(g_e8f.e9g_uimode_csv);
}

static void e9g_log_req_csv(uint32_t pc, uint32_t lr, uint32_t ui_mode, const char *name,
                            int natural_entry, int postmatch, uint32_t handle,
                            uint32_t draw_pc, const char *note) {
    static uint32_t n;
    if (!g_e8f.e9g_req_csv) return;
    n++;
    fprintf(g_e8f.e9g_req_csv, "%u,%u,0x%X,0x%X,0x%X,\"%s\",%d,%d,0x%X,0x%X,%s\n", n,
            g_e8f.tick, pc, lr, ui_mode, name ? name : "", natural_entry, postmatch, handle,
            draw_pc, note ? note : "");
    fflush(g_e8f.e9g_req_csv);
}

static void e9g_collect_idx_name(const char *idx) {
    int i;
    if (!idx || !idx[0] || g_e8f.e9g_idx_n >= 64) return;
    for (i = 0; i < g_e8f.e9g_idx_n; i++) {
        if (strcmp(g_e8f.e9g_idx_names[i], idx) == 0) return;
    }
    snprintf(g_e8f.e9g_idx_names[g_e8f.e9g_idx_n], sizeof(g_e8f.e9g_idx_names[0]), "%s", idx);
    g_e8f.e9g_idx_n++;
}

static void e9g_audit_slogo_once(int force) {
    int i, has_slogo = 0, has_loading = 0, has_textbar = 0, has_top = 0;
    if (!g_e8f.e9g_mode || g_e8f.e9g_slogo_class_logged) return;
    if (!force && g_e8f.e9d_cmp_n < 20u && !g_e8f.e9e_postmatch_done) return;
    for (i = 0; i < g_e8f.e9g_idx_n; i++) {
        if (strstr(g_e8f.e9g_idx_names[i], "slogo")) has_slogo = 1;
        if (strstr(g_e8f.e9g_idx_names[i], "loadingbar")) has_loading = 1;
        if (strstr(g_e8f.e9g_idx_names[i], "textbar")) has_textbar = 1;
        if (strstr(g_e8f.e9g_idx_names[i], "top!") == g_e8f.e9g_idx_names[i] ||
            strstr(g_e8f.e9g_idx_names[i], "top"))
            has_top = 1;
    }
    if (has_slogo) return;
    g_e8f.e9g_slogo_class_logged = 1;
    printf("[JJFB_E9G_CLASS] class=SLOGO_ABSENT_FROM_GUEST_INDEX_CONFIRMED "
           "note=mrp_has_slogo_guest_304BF0_index_omits evidence=OBSERVED\n");
    if ((has_loading || has_textbar || has_top) && !has_slogo) {
        printf("[JJFB_E9G_CLASS] class=SLOGO_RAW_INVENTORY_ONLY "
               "note=present_in_package_inventory_absent_from_runtime_guest_table "
               "loadingbar=%d textbar=%d top=%d evidence=OBSERVED\n",
               has_loading, has_textbar, has_top);
    }
    fflush(stdout);
}

/* E9F logo/loading: rewrite DisplayFirst request name so natural index+postmatch
 * loads a larger original UI member (not entry bridge; pixels still from jjfb.mrp).
 * Note: slogo!157!58.bmp is in jjfb.mrp but NOT in the guest 0x304BF0 index table
 * observed under DisplayFirst (27 entries). Prefer loadingbar/textbar/top instead. */
static int e9f_try_rewrite_request(uc_engine *uc, uint32_t *name_va_inout, uint32_t pc) {
    uint32_t slot = E8Y_A64_SCRATCH + 0x200u;
    size_t n;
    char from[96];
    const char *prefer;
    /* E9G: rewrite is never a product success path; require explicit env. */
    if (g_e8f.e9g_mode && !g_e8f.e9f_rewrite_request) {
        return 0;
    }
    if (g_e8f.e9g_mode && g_e8f.e9f_rewrite_request && !g_e8f.e9f_rewrite_done) {
        printf("[JJFB_E9G_CLASS] class=REQUEST_REWRITE_DEBUG_ONLY note=not_success_path "
               "evidence=OBSERVED\n");
        fflush(stdout);
    }
    if (!g_e8f.e9f_mode || !g_e8f.e9f_rewrite_request || g_e8f.e9f_rewrite_done) return 0;
    if (!uc || !name_va_inout || !g_e8f.e9f_prefer[0]) return 0;
    prefer = g_e8f.e9f_prefer;
    /* Guest index lacks slogo; map logo prefer → loadingbar (present at entry ~4). */
    if (strstr(prefer, "slogo") != NULL) {
        prefer = "loadingbar!201!29.bmp";
        printf("[JJFB_E9F_CLASS] class=SLOGO_ABSENT_FROM_GUEST_INDEX "
               "prefer_was=slogo!157!58.bmp fallback=\"%s\" "
               "note=mrp_has_slogo_index_does_not NOT_PRODUCT evidence=OBSERVED\n",
               prefer);
        fflush(stdout);
        snprintf(g_e8f.e9f_prefer, sizeof(g_e8f.e9f_prefer), "%s", prefer);
    }
    snprintf(from, sizeof(from), "%s", g_e8f.e8y_last_name[0] ? g_e8f.e8y_last_name : "?");
    /* Only rewrite when current request is the tiny DisplayFirst sprite (or empty). */
    if (from[0] && strstr(from, "wy_jiao1!") != from && strcmp(from, prefer) != 0)
        return 0;
    if (from[0] && strcmp(from, prefer) == 0) return 0;
    n = strlen(prefer);
    if (n >= 90) return 0;
#ifdef GWY_HAVE_UNICORN
    {
        uc_err ue = uc_mem_map((uc_engine *)uc, E8Y_A64_SCRATCH, E8Y_A64_SCRATCH_SIZE, UC_PROT_ALL);
        (void)ue;
    }
#endif
    (void)guest_memory_uc_poke((struct uc_struct *)uc, slot, prefer, (int)n + 1);
    *name_va_inout = slot;
    uc_reg_write(uc, UC_ARM_REG_R1, &slot);
    g_e8f.e9f_rewrite_done = 1;
    snprintf(g_e8f.e8y_last_name, sizeof(g_e8f.e8y_last_name), "%s", prefer);
    g_e8f.e8y_last_name_va = slot;
    printf("[JJFB_E9F_REQUEST_REWRITE] pc=0x%X from=\"%s\" to=\"%s\" name_va=0x%X "
           "note=assist_redirect_not_bridge NOT_PRODUCT evidence=OBSERVED\n",
           pc, from, prefer, slot);
    printf("[JJFB_E9F_CLASS] class=REQUEST_REWRITE_ASSIST prefer=\"%s\" "
           "blocker_was=GAME_ONLY_REQUESTS_WY_JIAO_ON_DISPLAYFIRST evidence=OBSERVED\n",
           prefer);
    fflush(stdout);
    return 1;
}

/* E9I: splash sibling names that stall when absent from guest 304BF0 index. */
static int e9i_is_splash_sibling(const char *name) {
    if (!name || !name[0]) return 0;
    if (strncmp(name, "bar!", 4) == 0) return 1;
    if (strncmp(name, "textbar!", 8) == 0) return 1;
    if (strncmp(name, "top!", 4) == 0) return 1;
    if (strncmp(name, "target!", 7) == 0) return 1;
    return 0;
}

/* Complete 0x2D92E4 for splash siblings with original jjfb.mrp bytes (same payload path
 * as E9E postmatch). Returns handle in r0 to splash LR — NOT REAL_MRP_MEMBER_BRIDGE. */
static int e9i_try_splash_sibling_resolve(uc_engine *uc, const char *name, uint32_t lr) {
    const char *mrp_path;
    MrpArchive *arch = NULL;
    const MrpMember *mem = NULL;
    ByteBuffer bb;
    LauncherError err;
    LauncherStatus st;
    int w = 0, h = 0, i;
    uint32_t handle_va, slot_va, sz, px, ret_pc;
    uint8_t stub[0x20];
    uint8_t digest[32];
    char hex[65];
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif
    if (!uc || !name || !name[0] || !lr) return 0;
    if (!g_e8f.e9i_mode || !g_e8f.e9e_postmatch_shims) return 0;
    if (e9e_name_already_done(name)) return 0;
    if (g_e8f.e9e_postmatch_n >= 8u) return 0;

    mrp_path = getenv("JJFB_REAL_MRP_PATH");
    if (!mrp_path || !mrp_path[0])
        mrp_path = "game_files/mythroad/320x480/gwy/jjfb.mrp";
    st = mrp_archive_open(mrp_path, &arch, &err);
    if (st != L_OK || !arch) return 0;
    st = mrp_archive_find_exact(arch, name, &mem, &err);
    if (st != L_OK || !mem) {
        printf("[JJFB_E9I_SIBLING_RESOLVE] find_fail name=\"%s\" evidence=OBSERVED\n", name);
        fflush(stdout);
        mrp_archive_close(arch);
        return 0;
    }
    byte_buffer_init(&bb);
    st = mrp_archive_decode_member(arch, mem, 1024 * 1024, &bb, &err);
    if (st != L_OK || !bb.data || bb.size == 0 || bb.size > (240u * 320u * 2u)) {
        byte_buffer_free(&bb);
        mrp_archive_close(arch);
        return 0;
    }
    (void)e9a_parse_name_wh(name, &w, &h);
    if (w <= 0 || h <= 0) {
        w = 16;
        h = 18;
    }
    gwy_sha256(bb.data, bb.size, digest);
    gwy_sha256_hex(digest, hex);
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
    handle_va = E8Y_A64_SCRATCH + 0x40u + (g_e8f.e9e_postmatch_n % 6u) * 0x40u;
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
    jjfb_bmp_meta_set(slot_va, (uint16_t)w, (uint16_t)h, name);

    ret_pc = lr | 1u;
    module_r9_switch_clear_dsm_return_side_stack();
    uc_reg_write(uc, UC_ARM_REG_R0, &handle_va);
    uc_reg_write(uc, UC_ARM_REG_PC, &ret_pc);

    g_e8f.e9e_postmatch_done = 1;
    if (g_e8f.e9e_postmatch_n < 8u) {
        snprintf(g_e8f.e9e_done_names[g_e8f.e9e_postmatch_n],
                 sizeof(g_e8f.e9e_done_names[0]), "%s", name);
        g_e8f.e9e_postmatch_n++;
    }
    if (g_e8f.e9h_seq_csv || g_e8f.e9i_seq_csv) {
        e9i_open_trace_files();
        if (g_e8f.e9h_seq_csv) {
            fprintf(g_e8f.e9h_seq_csv,
                    "%u,%u,0x2D92E4,0x%X,\"%s\",1,1,0x%X,0x%X,%d,%d,sibling_resolve\n",
                    g_e8f.e9d_req_n, g_e8f.tick, lr, name, handle_va, slot_va, w, h);
            fflush(g_e8f.e9h_seq_csv);
        }
    }
    printf("[JJFB_E9I_SIBLING_RESOLVE] name=\"%s\" handle=0x%X pixels=0x%X w=%d h=%d "
           "sha256=%s lr=0x%X note=original_mrp_bytes_complete_2D92E4 NOT_BRIDGE "
           "NOT_PRODUCT evidence=OBSERVED\n",
           name, handle_va, slot_va, w, h, hex, lr);
    fflush(stdout);
    (void)i;
    byte_buffer_free(&bb);
    mrp_archive_close(arch);
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
    if (!uc || !name || !name[0]) return 0;
    if (!g_e8f.e9e_postmatch_shims) return 0;
    if (e9e_name_already_done(name)) return 0;
    if (g_e8f.e9e_postmatch_done && !g_e8f.e9f_multi) return 0;
    if (g_e8f.e9e_postmatch_n >= 8u) return 0;
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
    if (g_e8f.e9e_postmatch_n < 8u) {
        snprintf(g_e8f.e9e_done_names[g_e8f.e9e_postmatch_n],
                 sizeof(g_e8f.e9e_done_names[0]), "%s", name);
        g_e8f.e9e_postmatch_n++;
    }
    g_e8f.e8z_pixel_va = slot_va;
    g_e8f.e8z_pixel_bytes = sz;
    g_e8f.e8z_handle_va = handle_va;
    g_e8f.e8z_bw = (uint16_t)w;
    g_e8f.e8z_bh = (uint16_t)h;
    g_e8f.e8z_real_bmp_done = 1;
    /* Prefer per-bmp meta over global LAST_WH for multi-member draw. */
    jjfb_bmp_meta_set(slot_va, (uint16_t)w, (uint16_t)h, name);
    {
        char wh[32];
        snprintf(wh, sizeof(wh), "%ux%u", (unsigned)w, (unsigned)h);
#ifdef _WIN32
        _putenv_s("JJFB_E9E_LAST_WH", wh);
#else
        setenv("JJFB_E9E_LAST_WH", wh, 1);
#endif
    }

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
    if (g_e8f.e9f_mode) {
        e9f_open_trace_files();
        if (g_e8f.e9f_req_csv) {
            fprintf(g_e8f.e9f_req_csv,
                    "%u,%u,0x304BF0,0x%X,\"%s\",%u,1,0x%X,0,postmatch_ok\n",
                    g_e8f.e9d_req_n, g_e8f.tick, lr, name, g_e8f.e9d_cmp_n, handle_va);
            fflush(g_e8f.e9f_req_csv);
        }
        if (g_e8f.e9f_results_jsonl) {
            fprintf(g_e8f.e9f_results_jsonl,
                    "{\"name\":\"%s\",\"entry\":%u,\"offset\":%u,\"stored\":%u,"
                    "\"decoded\":%u,\"w\":%d,\"h\":%d,\"sha256\":\"%s\","
                    "\"handle\":\"0x%X\",\"pixels\":\"0x%X\",\"bridge\":false,"
                    "\"rewrite\":%s}\n",
                    name, g_e8f.e9d_cmp_n, mem->offset, mem->stored_size, sz, w, h, hex,
                    handle_va, slot_va, g_e8f.e9f_rewrite_done ? "true" : "false");
            fflush(g_e8f.e9f_results_jsonl);
        }
        if (strstr(name, "slogo"))
            printf("[JJFB_E9F_CLASS] class=NATURAL_POSTMATCH_LOGO_CANDIDATE name=\"%s\" "
                   "evidence=OBSERVED\n",
                   name);
        else if (strstr(name, "loadingbar"))
            printf("[JJFB_E9F_CLASS] class=NATURAL_POSTMATCH_LOADINGBAR_CANDIDATE "
                   "name=\"%s\" evidence=OBSERVED\n",
                   name);
    }
    if (g_e8f.e9g_mode) {
        uint32_t ui_mode = 0, r9p = 0;
        if (find_robotol_r9(uc, &r9p) && r9p)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9p + E8I_STATE_OFF, &ui_mode);
        e9g_open_trace_files();
        e9g_log_req_csv(0x304BF0u, lr, ui_mode, name, 1, 1, handle_va, 0, "postmatch_ok");
        e9g_audit_slogo_once(1);
        /* Splash bind path: arm continue-to-blit after loadingbar (0x2EFA97). */
        if (strstr(name, "loadingbar") && g_e8f.e8y_2d92e4_n > 0u) {
            g_e8f.e9g_skip_sibling_binds = 1;
            if (g_e8f.e9i_mode && !g_e8f.e9i_skip_sibling) {
                g_e8f.e9g_skip_sibling_binds = 0;
                printf("[JJFB_E9I_CLASS] class=SPLASH_LOADINGBAR_POSTMATCH_KEEP_SIBLINGS "
                       "name=\"%s\" note=natural_bar_textbar_binds NOT_PRODUCT "
                       "evidence=OBSERVED\n",
                       name);
            } else {
                printf("[JJFB_E9H_CLASS] class=SPLASH_LOADINGBAR_POSTMATCH_ARM_BLIT "
                       "name=\"%s\" note=next_bar_bl_0x2EFA46_skips_to_0x2EFA5C NOT_PRODUCT "
                       "evidence=OBSERVED\n",
                       name);
            }
            fflush(stdout);
        }
        if (g_e8f.e9h_mode) {
            e9h_open_trace_files();
            if (g_e8f.e9h_seq_csv) {
                fprintf(g_e8f.e9h_seq_csv,
                        "%u,%u,0x304BF0,0x%X,\"%s\",1,1,0x%X,0x%X,%d,%d,postmatch\n",
                        g_e8f.e9d_req_n, g_e8f.tick, lr, name, handle_va, slot_va, w, h);
                fflush(g_e8f.e9h_seq_csv);
            }
        }
    }
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

/* E9G: invoke real splash/loading UI fn 0x2EF86C (legacy ABI r0=0x45,r1=0x13).
 * NOT product success — optional UI_MODE assist poke is logged as NOT_PRODUCT. */
static void fast_call_splash(void *uc) {
    uint32_t r9_save = 0, r9_run = 0, state_before = 0, state_after = 0;
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    int ok;
    uint64_t lim = 500000ull;
    if (!uc || !g_e8f.e9g_mode || !g_e8f.e9g_splash_call || g_e8f.e9g_splash_done) return;
    if (g_e8f.e9g_debug_only) {
        printf("[JJFB_FAST_SPLASH_CALL] skip=debug_only note=trace_only_no_guest_call "
               "evidence=OBSERVED\n");
        fflush(stdout);
        return;
    }
    g_e8f.e9g_splash_done = 1;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    if (!find_robotol_r9(uc, &r9_run)) r9_run = r9_save;
    if (r9_run) (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_run);
    if (r9_run) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9_run + E8I_STATE_OFF,
                                       &state_before);
        if (g_e8f.e9g_ui_mode_assist && state_before != 0x45u) {
            (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, r9_run + E8I_STATE_OFF, 0x45u);
            printf("[JJFB_E9G_UI_MODE_ASSIST] old=0x%X new=0x45 addr=0x%X NOT_PRODUCT "
                   "evidence=HYPOTHESIS\n",
                   state_before, r9_run + E8I_STATE_OFF);
            e9g_open_trace_files();
            e9g_log_uimode_csv(0, 0, state_before, 0x45u, r9_run, "ui_mode_assist");
            state_before = 0x45u;
            fflush(stdout);
        }
        /* E9I: seed splash screen dims used by 0x2F9970/0x2F9964 (R9+0x830/0x834). */
        if (g_e8f.e9i_mode || g_e8f.e9h_mode)
            e9i_seed_scr_dims(uc, r9_run);
        /* E9J: seed BD0 status string + progress count before 0x2EF886 loads r4. */
        if (g_e8f.e9j_mode)
            e9j_seed_progress_object(uc, r9_run);
        /* E9L: seed R9+0x818/0x81C dims for 0x305E78 text measure (post-r4). */
        if (g_e8f.e9l_mode)
            e9l_seed_textctx(uc, r9_run);
    }
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = 0x45u;
    abi.set_r1 = 1;
    abi.r1 = 0x13u;
    abi.set_r2 = 1;
    abi.r2 = 0xFFFFFFFFu;
    abi.set_r3 = 1;
    abi.r3 = 0;
    abi.set_lr = 1;
    abi.lr = 0x80000u;
    if (g_e8f.fast_insn_limit) lim = g_e8f.fast_insn_limit;
    printf("[JJFB_FAST_SPLASH_CALL] entry=0x2EF86C r0=0x45 r1=0x13 r9=0x%X ui_mode=0x%X "
           "note=real_fn_not_rewrite NOT_PRODUCT evidence=HYPOTHESIS\n",
           r9_run, state_before);
    fflush(stdout);
    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, 0x2EF86Du, 0x80000u, lim, &abi,
                                      &out);
    if (r9_run)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9_run + E8I_STATE_OFF,
                                       &state_after);
    printf("[JJFB_FAST_SPLASH_DONE] ok=%d end=%s pc_after=0x%X ui_mode=0x%X->0x%X "
           "note=FAST_ASSIST_not_product evidence=OBSERVED\n",
           ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after, state_before,
           state_after);
    e9g_open_trace_files();
    e9g_log_uimode_csv(0x2EF86Cu, 0x80000u, state_before, state_after, r9_run, "splash_done");
    if (ok && g_e8f.e8z_handle_va && g_e8f.e9e_postmatch_done) {
        printf("[JJFB_E9G_CLASS] class=SPLASH_2EF86C_REACHED_NEXT_GAP "
               "handle=0x%X pixels=0x%X "
               "note=loadingbar_bound_draw_needs_splash_blit_path_not_2F45A2 "
               "evidence=OBSERVED\n",
               g_e8f.e8z_handle_va, g_e8f.e8z_pixel_va);
        fflush(stdout);
    }
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
        /* E9G: splash after unlock+case156 on tick1 (lifecycle, not CODE-hook nested). */
        if (g_e8f.e9g_mode && g_e8f.e9g_splash_call && !g_e8f.e9g_splash_done)
            fast_call_splash(uc);
    }

    /* E9G: retry splash on tick2 if tick1 path skipped (e.g. unlock-only). */
    if (tick == 2u && g_e8f.e9g_mode && g_e8f.e9g_splash_call && !g_e8f.e9g_splash_done)
        fast_call_splash(uc);

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
