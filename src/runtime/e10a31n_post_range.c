#include "gwy_launcher/e10a31n_post_range.h"

#include "gwy_launcher/gwy_sms_cfg.h"
#include "gwy_launcher/sms_cfg_compat_profile.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    E10A31N_CALL_MAX = 512,
    E10A31N_PRED_MAX = 256,
    E10A31N_READ_MAX = 256,
    E10A31N_CMP_MAX = 128
};

typedef struct {
    uint32_t seq;
    uint32_t caller_pc;
    uint32_t target;
    uint32_t lr;
    uint32_t r0_in;
    int32_t r0_out;
    int r0_out_valid;
    char note[64];
} CallRow;

typedef struct {
    uint32_t seq;
    uint32_t pc, lr;
    uint32_t r0, r1;
    char insn[40];
    char condition[16];
    int branch_taken; /* -1 unknown */
    char note[96];
} PredRow;

typedef struct {
    uint32_t seq;
    uint32_t pc, addr, size, cfg_off;
    uint8_t bytes[8];
    char interpretation[48];
    char causal_status[32];
    char later_cmp[48];
} ReadRow;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t r0, r1;
    char kind[24];
    char lhs[40];
    char rhs[40];
    char note[96];
} CmpRow;

static struct {
    int known, enabled, active, armed;
    unsigned long long run_id;
    void *uc;
    uint32_t helper, cfg_base, cfg_len;
    uint32_t seq;
    int saw_post_gate;
    int saw_sms_377;
    int saw_failfn;
    int saw_cmp_blx;
    int saw_bne;
    int saw_mvns;
    int saw_first_neg;
    int applied_int16;
    uint32_t sms377_dst;
    uint8_t sms377_val;
    int sms377_val_valid;
    char cmp_lhs[40];
    char cmp_rhs[40];
    uint32_t cmp_r0, cmp_r1;
    int32_t cmp_r0_out;
    int cmp_r0_out_valid;
    int bne_taken;
    int lhs_smscfg_derived;
    int call_n, pred_n, read_n, cmp_n;
    CallRow calls[E10A31N_CALL_MAX];
    PredRow preds[E10A31N_PRED_MAX];
    ReadRow reads[E10A31N_READ_MAX];
    CmpRow cmps[E10A31N_CMP_MAX];
    int pending_call; /* index+1 of open BL awaiting LR */
    FILE *call_csv, *prov_csv, *pred_csv, *smscfg_csv, *compare_csv, *timing_csv;
    int manifest_written;
    int timing_hdr;
    int content_valid_printed;
#ifdef GWY_HAVE_UNICORN
    uc_hook mem_hook;
    int mem_armed;
#endif
} g_n;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31N_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
}

static void ensure(void) {
    if (g_n.known) return;
    g_n.known = 1;
    g_n.enabled = env1("JJFB_E10A31N_MODE");
    g_n.run_id = run_id_now();
    g_n.cfg_len = GWY_SMS_CFG_LEN;
    g_n.bne_taken = -1;
    g_n.pending_call = 0;
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

static void ensure_csvs(void) {
    if (!g_n.call_csv)
        g_n.call_csv = open_csv(
            "JJFB_E10A31N_CALL_CSV", "reports/e10a31n_call_tree.csv",
            "run_id,seq,caller_pc,target,lr,r0_in,r0_out,note\n");
    if (!g_n.prov_csv)
        g_n.prov_csv = open_csv(
            "JJFB_E10A31N_PROV_CSV", "reports/e10a31n_return_provenance.csv",
            "run_id,first_negative_pc,producer_function,producer_callee,negative_value,"
            "caller_pc,predicate_pc,predicate,branch_taken,final_fail_pc,source_class,"
            "evidence,primary\n");
    if (!g_n.pred_csv)
        g_n.pred_csv = open_csv(
            "JJFB_E10A31N_PRED_CSV", "reports/e10a31n_predicate_chain.csv",
            "run_id,seq,pc,lr,r0,r1,insn,condition,branch_taken,note\n");
    if (!g_n.smscfg_csv)
        g_n.smscfg_csv = open_csv(
            "JJFB_E10A31N_SMSCFG_CSV", "reports/e10a31n_new_smscfg_reads.csv",
            "run_id,seq,pc,addr,cfg_off,size,bytes_hex,interpretation,dest_hint,later_cmp,"
            "expected,failure_branch,causal_status\n");
    if (!g_n.compare_csv)
        g_n.compare_csv = open_csv(
            "JJFB_E10A31N_COMPARE_CSV", "reports/e10a31n_post_range_compare_chain.csv",
            "run_id,seq,pc,kind,r0,r1,lhs_str,rhs_str,note\n");
}

static void ensure_timing_csv(void) {
    const char *p = getenv("JJFB_E10A31N_TIMING_CSV");
    if (!p || !p[0]) p = "reports/e10a31n_smscfg_application_timing.csv";
    if (!g_n.timing_csv) {
        g_n.timing_csv = fopen(p, g_n.timing_hdr ? "a" : "w");
        if (g_n.timing_csv && !g_n.timing_hdr) {
            fputs("run_id,phase,event,dsm_generation,cfg_guest,erw_base,note,evidence\n",
                  g_n.timing_csv);
            g_n.timing_hdr = 1;
            fflush(g_n.timing_csv);
        }
    }
}

static void timing_row(const char *phase, const char *event, uint32_t dsm_gen, uint32_t cfg,
                       uint32_t erw, const char *note) {
    ensure();
    if (!g_n.enabled) return;
    ensure_timing_csv();
    if (!g_n.timing_csv) return;
    fprintf(g_n.timing_csv, "%llu,%s,%s,%u,0x%X,0x%X,\"%s\",OBSERVED\n", g_n.run_id, phase,
            event, dsm_gen, cfg, erw, note ? note : "");
    fflush(g_n.timing_csv);
}

#ifdef GWY_HAVE_UNICORN
static int read_guest(void *uc, uint32_t addr, void *dst, size_t n) {
    return uc_mem_read((uc_engine *)uc, addr, dst, n) == UC_ERR_OK;
}
#else
static int read_guest(void *uc, uint32_t addr, void *dst, size_t n) {
    (void)uc;
    (void)addr;
    (void)dst;
    (void)n;
    return 0;
}
#endif

static void hex_bytes(const uint8_t *b, uint32_t n, char *out, size_t out_n) {
    uint32_t i;
    size_t o = 0;
    out[0] = 0;
    for (i = 0; i < n && o + 2 < out_n; i++)
        o += (size_t)snprintf(out + o, out_n - o, "%02X", b[i]);
}

static void peek_cstr(void *uc, uint32_t addr, char *out, size_t out_n) {
    uint8_t buf[32];
    size_t i, o = 0;
    out[0] = 0;
    if (!uc || !addr || out_n < 2) return;
    memset(buf, 0, sizeof(buf));
    if (!read_guest(uc, addr, buf, sizeof(buf))) return;
    for (i = 0; i < sizeof(buf) && o + 1 < out_n; i++) {
        if (buf[i] == 0) break;
        if (buf[i] >= 32 && buf[i] < 127)
            out[o++] = (char)buf[i];
        else {
            o += (size_t)snprintf(out + o, out_n - o, "\\x%02X", buf[i]);
            break;
        }
    }
    out[o] = 0;
}

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

static void emit_call(const CallRow *c) {
    ensure_csvs();
    if (!g_n.call_csv || !c) return;
    fprintf(g_n.call_csv, "%llu,%u,0x%X,0x%X,0x%X,0x%X,%d,\"%s\"\n", g_n.run_id, c->seq,
            c->caller_pc, c->target, c->lr, c->r0_in,
            c->r0_out_valid ? (int)c->r0_out : 0x7fffffff, c->note);
    fflush(g_n.call_csv);
}

static void emit_pred(const PredRow *p) {
    ensure_csvs();
    if (!g_n.pred_csv || !p) return;
    fprintf(g_n.pred_csv, "%llu,%u,0x%X,0x%X,0x%X,0x%X,\"%s\",%s,%d,\"%s\"\n", g_n.run_id,
            p->seq, p->pc, p->lr, p->r0, p->r1, p->insn, p->condition, p->branch_taken,
            p->note);
    fflush(g_n.pred_csv);
}

static void emit_cmp(const CmpRow *c) {
    ensure_csvs();
    if (!g_n.compare_csv || !c) return;
    fprintf(g_n.compare_csv, "%llu,%u,0x%X,%s,0x%X,0x%X,\"%s\",\"%s\",\"%s\"\n", g_n.run_id,
            c->seq, c->pc, c->kind, c->r0, c->r1, c->lhs, c->rhs, c->note);
    fflush(g_n.compare_csv);
}

static void push_call(uint32_t pc, uint32_t tgt, uint32_t lr, uint32_t r0_in, const char *note) {
    CallRow *c;
    if (g_n.call_n >= E10A31N_CALL_MAX) return;
    c = &g_n.calls[g_n.call_n++];
    memset(c, 0, sizeof(*c));
    c->seq = ++g_n.seq;
    c->caller_pc = pc;
    c->target = tgt;
    c->lr = lr;
    c->r0_in = r0_in;
    c->r0_out = 0;
    c->r0_out_valid = 0;
    snprintf(c->note, sizeof(c->note), "%s", note ? note : "");
    g_n.pending_call = g_n.call_n; /* 1-based */
    emit_call(c);
}

static void fill_pending_r0_out(uint32_t pc, uint32_t r0) {
    CallRow *c;
    if (!g_n.pending_call) return;
    c = &g_n.calls[g_n.pending_call - 1];
    if (pc != c->lr) return;
    c->r0_out = (int32_t)r0;
    c->r0_out_valid = 1;
    emit_call(c);
    if (c->caller_pc == E10A31N_PC_CMP_BLX) {
        g_n.cmp_r0_out = (int32_t)r0;
        g_n.cmp_r0_out_valid = 1;
    }
    g_n.pending_call = 0;
}

static void push_pred(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, const char *insn,
                      const char *cond, int taken, const char *note) {
    PredRow *p;
    if (g_n.pred_n >= E10A31N_PRED_MAX) return;
    p = &g_n.preds[g_n.pred_n++];
    memset(p, 0, sizeof(*p));
    p->seq = ++g_n.seq;
    p->pc = pc;
    p->lr = lr;
    p->r0 = r0;
    p->r1 = r1;
    p->branch_taken = taken;
    snprintf(p->insn, sizeof(p->insn), "%s", insn ? insn : "");
    snprintf(p->condition, sizeof(p->condition), "%s", cond ? cond : "");
    snprintf(p->note, sizeof(p->note), "%s", note ? note : "");
    emit_pred(p);
}

static void write_provenance_primary(void) {
    const char *src = "UNKNOWN";
    ensure_csvs();
    if (!g_n.prov_csv || g_n.saw_first_neg) return;
    g_n.saw_first_neg = 1;
    if (g_n.lhs_smscfg_derived)
        src = "SMSCFG";
    else if (g_n.saw_cmp_blx)
        src = "PLATFORM_API";
    fprintf(g_n.prov_csv,
            "%llu,0x%X,failfn_0x2E3F85,0xAC4A4_strstr,%d,0x%X,0x%X,strstr_NULL_then_MVNS,%d,0x%X,%s,"
            "\"MVNS after strstr(haystack,needle) NULL lhs='%s' needle='%s'\",1\n",
            g_n.run_id, E10A31N_PC_MVNS, -1, E10A31N_PC_CMP_BLX, E10A31N_PC_BNE,
            g_n.bne_taken, E10A31N_PC_MVNS, src, g_n.cmp_lhs, g_n.cmp_rhs);
    fflush(g_n.prov_csv);
    e10a31n_mark_milestone("METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND", "0x2E3FBA_MVNS");
}

static void write_manifest(void) {
    const char *path = getenv("JJFB_E10A31N_MANIFEST_JSON");
    FILE *f;
    int req_n = 3;
    if (g_n.manifest_written) return;
    if (!path || !path[0]) path = "reports/e10a31n_required_platform_state.json";
    f = fopen(path, "w");
    if (!f) return;
    if (g_n.saw_sms_377 && g_n.cmp_rhs[0] && g_n.lhs_smscfg_derived) req_n = 4;
    fprintf(f,
            "{\n"
            "  \"stage\": \"e10a31n\",\n"
            "  \"original_default_recovered\": false,\n"
            "  \"requirements\": [\n"
            "    {\"source\":\"sms_cfg\",\"offset\":%u,\"length\":3,\"bytes_hex\":\"475054\","
            "\"ascii\":\"GPT\",\"required\":true,\"note\":\"proven_e10a31k\"},\n"
            "    {\"source\":\"sms_cfg\",\"offset\":%u,\"length\":3,\"bytes_hex\":\"677779\","
            "\"ascii\":\"gwy\",\"required\":true,\"note\":\"proven_e10a31l\"},\n"
            "    {\"source\":\"sms_cfg\",\"offset\":%u,\"length\":2,\"encoding\":\"int16_le\","
            "\"expected_hex\":\"0100\",\"min_inclusive\":1,\"max_inclusive\":434,"
            "\"required\":true,\"note\":\"proven_e10a31m_diag_apply_method0_enter\"}",
            GWY_SMS_CFG_GPT_OFF, 0x34Cu, 0x355u);
    if (req_n == 4) {
        char hex[8];
        hex_bytes(&g_n.sms377_val, g_n.sms377_val_valid ? 1u : 0u, hex, sizeof(hex));
        fprintf(f,
                ",\n"
                "    {\"source\":\"sms_cfg\",\"offset\":%u,\"length\":1,"
                "\"observed_hex\":\"%s\",\"compare_literal\":\"%s\","
                "\"evidence_pc\":\"0x%X\",\"causal_status\":\"%s\","
                "\"required\":false,\"note\":\"0x377 is C-string haystack for strstr(,napptype); "
                "0x355 is copy length (not a separate gate once length>=needle). "
                "Apply paired len+string diagnostically — original_default unknown\","
                "\"helper\":\"strstr@0xAC4A4\",\"strlen_helper\":\"0xAC374\","
                "\"original_default_recovered\":false}",
                E10A31N_SMS_OFF_377, hex[0] ? hex : "", g_n.cmp_rhs, E10A31N_PC_CMP_BLX,
                (g_n.cmp_r0_out_valid && g_n.cmp_r0_out == 0 && g_n.saw_mvns)
                    ? "CAUSAL_PATH_TO_0x2E3FBA"
                    : "OBSERVED");
        if (g_n.cmp_r0_out_valid && g_n.cmp_r0_out == 0 && g_n.saw_mvns && g_n.cmp_rhs[0]) {
            e10a31n_mark_milestone("SMSCFG_377_COMPARE_CAUSAL", g_n.cmp_rhs);
        }
    }
    fprintf(f,
            "\n  ],\n"
            "  \"requirement_count\": %d,\n"
            "  \"first_negative_pc\": \"0x%X\",\n"
            "  \"producer\": \"MVNS_after_strstr_NULL_from_0xAC4A4\",\n"
            "  \"predicate_pc\": \"0x%X\",\n"
            "  \"application_timing\": {\n"
            "    \"profile_content_valid\": true,\n"
            "    \"early_bootstrap_0x355_unsafe\": true,\n"
            "    \"diagnostic_apply_point\": \"method0_enter\",\n"
            "    \"product_bootstrap_ready\": false\n"
            "  }\n"
            "}\n",
            req_n, g_n.saw_mvns ? E10A31N_PC_MVNS : 0u, E10A31N_PC_BNE);
    fclose(f);
    g_n.manifest_written = 1;
}

#ifdef GWY_HAVE_UNICORN
static void on_mem_read(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                        int64_t value, void *user_data) {
    uint32_t pc = 0, addr = (uint32_t)address;
    ReadRow *r;
    char hex[20];
    (void)type;
    (void)value;
    (void)user_data;
    if (!g_n.active || !g_n.armed || !g_n.cfg_base || size <= 0) return;
    if (addr < g_n.cfg_base || addr >= g_n.cfg_base + GWY_SMS_CFG_LEN) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    if (g_n.read_n >= E10A31N_READ_MAX) return;
    r = &g_n.reads[g_n.read_n++];
    memset(r, 0, sizeof(*r));
    r->seq = ++g_n.seq;
    r->pc = pc;
    r->addr = addr;
    r->size = (uint32_t)size;
    r->cfg_off = addr - g_n.cfg_base;
    (void)uc_mem_read(uc, address, r->bytes, size > 8 ? 8 : (size_t)size);
    hex_bytes(r->bytes, r->size > 8 ? 8 : r->size, hex, sizeof(hex));
    if (r->cfg_off == E10A31N_SMS_OFF_377) {
        g_n.sms377_val = r->bytes[0];
        g_n.sms377_val_valid = 1;
        snprintf(r->interpretation, sizeof(r->interpretation), "SMSCFG_0x377");
        snprintf(r->later_cmp, sizeof(r->later_cmp), "BLX_0x2E3FB4");
        snprintf(r->causal_status, sizeof(r->causal_status), "CANDIDATE");
    } else {
        snprintf(r->interpretation, sizeof(r->interpretation), "POST_RANGE_READ");
        snprintf(r->causal_status, sizeof(r->causal_status), "COPY_ONLY");
    }
    ensure_csvs();
    if (g_n.smscfg_csv) {
        fprintf(g_n.smscfg_csv,
                "%llu,%u,0x%X,0x%X,0x%X,%u,%s,%s,,%s,,,%s\n", g_n.run_id, r->seq, r->pc,
                r->addr, r->cfg_off, r->size, hex, r->interpretation, r->later_cmp,
                r->causal_status);
        fflush(g_n.smscfg_csv);
    }
}

static void arm_mem_hook(void *uc) {
    const GwySmsCfgState *st;
    if (!uc || g_n.mem_armed) return;
    st = gwy_sms_cfg_state();
    if (!st || !st->cfg_guest) return;
    g_n.cfg_base = st->cfg_guest;
    if (uc_hook_add((uc_engine *)uc, &g_n.mem_hook, UC_HOOK_MEM_READ, (void *)on_mem_read, NULL,
                    (uint64_t)g_n.cfg_base,
                    (uint64_t)g_n.cfg_base + (uint64_t)GWY_SMS_CFG_LEN - 1ull) == UC_ERR_OK)
        g_n.mem_armed = 1;
}

static void disarm_mem_hook(void *uc) {
    if (g_n.mem_armed && uc) {
        uc_hook_del((uc_engine *)uc, g_n.mem_hook);
        g_n.mem_armed = 0;
    }
}
#endif

static int env_flag1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

/* E10A-3.1o/p: @0x355 = copy length for C-string at @0x377; helper 0xAC4A4 is strstr. */
static void apply_napptype_method0(void *uc) {
    const char *s;
    const char *src_name;
    size_t n;
    uint8_t le[2];
    uint8_t entry_skip[4];
    int use_p;
    int skip_entry;
    const GwySmsCfgState *st;
    static const char k_case1[] =
        "napptype=1_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
    /* E10A-3.1q: after case1 n* PASS, next failfn needle=bapptype. Append b* mirroring
     * launch n* field values; format evidence from other MRPs — not original default. */
    static const char k_case1_plus_b[] =
        "napptype=1_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink_"
        "bapptype=12_bextid=482_bcode=512_barg=0_barg1=1_bmrpname=gwy/jjfb.mrp_gwyblink";

    use_p = env_flag1("JJFB_E10A31P_APPLY") || env_flag1("JJFB_E10A31Q_APPLY");
    if (!uc || (!env_flag1("JJFB_E10A31O_APPLY_NAPPTYPE") && !use_p)) return;
    skip_entry = use_p || env_flag1("JJFB_E10A31P_SKIP_ENTRY");

    s = getenv("JJFB_E10A31O_NAPPTYPE_STR");
    src_name = "JJFB_E10A31O_NAPPTYPE_STR";
    if ((!s || !s[0]) && env_flag1("JJFB_E10A31Q_APPLY")) {
        s = k_case1_plus_b;
        src_name = "case1_n_plus_b_mirrored";
    }
    if ((!s || !s[0]) && env_flag1("JJFB_E10A31P_APPLY")) {
        s = k_case1;
        src_name = "case1_n_keys_aligned";
    }
    if (!s || !s[0]) {
        s = getenv("GWY_LAUNCH_PARAM");
        src_name = "GWY_LAUNCH_PARAM";
    }
    if (!s || !s[0]) {
        s = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
        src_name = "builtin_launch_param";
    }
    n = strlen(s);
    if (n == 0 || n > 0x1B2u) return;
    st = gwy_sms_cfg_state();
    if (!st || !st->cfg_guest) return;
    if ((uint32_t)E10A31N_SMS_OFF_377 + (uint32_t)n > st->cfg_len) return;
    le[0] = (uint8_t)(n & 0xFF);
    le[1] = (uint8_t)((n >> 8) & 0xFF);
    if (gwy_sms_cfg_set_bytes(uc, 0x355, le, 2, GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY) != 0) return;
    if (gwy_sms_cfg_set_bytes(uc, (int32_t)E10A31N_SMS_OFF_377, s, (int32_t)n,
                              GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY) != 0)
        return;
    if (skip_entry) {
        /* Guest: sms_get(0x35F,dst,4); if (*dst) skip strstr(entry). Minimal nonzero. */
        if (0x35Fu + 4u > st->cfg_len) return;
        entry_skip[0] = 1;
        entry_skip[1] = 0;
        entry_skip[2] = 0;
        entry_skip[3] = 0;
        if (gwy_sms_cfg_set_bytes(uc, 0x35F, entry_skip, 4, GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY) != 0)
            return;
        printf("[SMSCFG_APPLY_35F] phase=method0_enter offset=0x35F value=1 "
               "note=skip_optional_entry_strstr source=DIAGNOSTIC_ONLY "
               "original_default_recovered=false evidence=OBSERVED\n");
        fflush(stdout);
        e10a31n_mark_milestone("SMSCFG_35F_SKIP_ENTRY_APPLY", "int32_1");
    }
    g_n.applied_int16 = 1;
    printf("[SMSCFG_APPLY_NAPPTYPE] phase=method0_enter off_len=0x355 len=%u off_str=0x377 "
           "src=%s str=\"%s\" helper=strstr@0xAC4A4 skip_entry=%d source=DIAGNOSTIC_ONLY "
           "original_default_recovered=false evidence=OBSERVED\n",
           (unsigned)n, src_name, s, skip_entry);
    fflush(stdout);
    timing_row("method0_enter",
               env_flag1("JJFB_E10A31Q_APPLY") ? "APPLY_Q_N_PLUS_B"
                                              : (use_p ? "APPLY_P_CASE1" : "APPLY_NAPPTYPE"),
               st->dsm_generation, st->cfg_guest, st->erw_base,
               env_flag1("JJFB_E10A31Q_APPLY") ? "diag_case1_n_plus_b_plus_0x35F"
                                              : (use_p ? "diag_case1_plus_0x35F_skip_entry"
                                                       : "diag_0x355_len_plus_0x377"));
    e10a31n_mark_milestone(env_flag1("JJFB_E10A31Q_APPLY") ? "SMSCFG_METHOD0_ENTER_APPLY_Q"
                           : (use_p ? "SMSCFG_METHOD0_ENTER_APPLY_P"
                                    : "SMSCFG_METHOD0_ENTER_APPLY_NAPPTYPE"),
                           use_p ? "0x355+0x377+0x35F" : "0x355+0x377");
    if (env_flag1("JJFB_E10A31Q_APPLY"))
        e10a31n_mark_milestone("SMSCFG_Q_BKEYS_MIRRORED", "bapptype_from_launch_n_parallel");
    e10a31n_mark_milestone("HELPER_0xAC4A4_IS_STRSTR", "needle=napptype");
    e10a31n_mark_milestone("HELPER_0xAC374_IS_STRLEN", "failfn_pre_strstr");
}

static void apply_int16_method0(void *uc) {
    const char *extra = getenv("JJFB_E10A31N_APPLY_INT16");
    unsigned long off = 0x355ul;
    long val = 1;
    int do_apply = 1;
    const GwySmsCfgState *st;
    uint8_t le[2];

    if (!uc) return;
    /* Prefer paired length+string apply when requested (E10A-3.1o/p). */
    if (env_flag1("JJFB_E10A31O_APPLY_NAPPTYPE") || env_flag1("JJFB_E10A31P_APPLY") ||
        env_flag1("JJFB_E10A31Q_APPLY")) {
        apply_napptype_method0(uc);
        return;
    }
    if (extra && extra[0]) {
        if ((extra[0] == '0' && extra[1] == '\0') || strcmp(extra, "0") == 0) {
            do_apply = 0;
        } else {
            char *colon = strchr(extra, ':');
            if (colon && colon > extra) {
                off = strtoul(extra, NULL, 0);
                val = strtol(colon + 1, NULL, 0);
            } else {
                do_apply = 0;
            }
        }
    }
    /* Default when MODE on and env unset: 0x355:1 */
    if (!do_apply) return;
    st = gwy_sms_cfg_state();
    if (!st || !st->cfg_guest) return;
    le[0] = (uint8_t)(val & 0xFF);
    le[1] = (uint8_t)((val >> 8) & 0xFF);
    if (off + 2u > st->cfg_len) return;
    if (gwy_sms_cfg_set_bytes(uc, (int32_t)off, le, 2, GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY) != 0)
        return;
    g_n.applied_int16 = 1;
    printf("[SMSCFG_APPLY_INT16] phase=method0_enter offset=0x%lX value=%ld "
           "le_hex=%02X%02X source=DIAGNOSTIC_ONLY evidence=OBSERVED\n",
           off, val, le[0], le[1]);
    fflush(stdout);
    timing_row("method0_enter", "APPLY_INT16", st->dsm_generation, st->cfg_guest, st->erw_base,
               "diag_0x355_not_product_bootstrap");
    e10a31n_mark_milestone("SMSCFG_METHOD0_ENTER_APPLY", "0x355");
}

int e10a31n_enabled(void) {
    ensure();
    return g_n.enabled;
}

void e10a31n_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_n.mem_armed && g_n.uc) disarm_mem_hook(g_n.uc);
#endif
    if (g_n.call_csv) fclose(g_n.call_csv);
    if (g_n.prov_csv) fclose(g_n.prov_csv);
    if (g_n.pred_csv) fclose(g_n.pred_csv);
    if (g_n.smscfg_csv) fclose(g_n.smscfg_csv);
    if (g_n.compare_csv) fclose(g_n.compare_csv);
    if (g_n.timing_csv) fclose(g_n.timing_csv);
    memset(&g_n, 0, sizeof(g_n));
}

void e10a31n_mark_milestone(const char *name, const char *note) {
    ensure();
    if (!g_n.enabled || !name) return;
    printf("[JJFB_E10A31N_MILESTONE] name=%s note=%s evidence=OBSERVED\n", name,
           note ? note : "");
    fflush(stdout);
}

void e10a31n_on_smscfg_bootstrap_applied(uint32_t cfg_guest, uint32_t dsm_generation,
                                         const char *source) {
    ensure();
    if (!g_n.enabled) return;
    timing_row("bootstrap", "PROFILE_APPLIED", dsm_generation, cfg_guest, 0,
               source ? source : "");
    if (!g_n.content_valid_printed) {
        g_n.content_valid_printed = 1;
        e10a31n_mark_milestone("SMSCFG_PROFILE_CONTENT_VALID",
                               "GPT+gwy+int16_355_in_profile");
        e10a31n_mark_milestone("SMSCFG_EARLY_APPLICATION_UNSAFE",
                               "bootstrap_0x355_host_crash_documented");
    }
}

void e10a31n_on_smscfg_buffer_ready(uint32_t cfg_guest, uint32_t erw_base,
                                    uint32_t dsm_generation) {
    ensure();
    if (!g_n.enabled) return;
    timing_row("erw_ready", "BUFFER_RESOLVED", dsm_generation, cfg_guest, erw_base,
               "cfunction_erw_mapped");
    e10a31n_mark_milestone("SMSCFG_SAFE_APPLICATION_POINT_FOUND",
                           "candidate_after_cfunction_erw_before_method0");
}

void e10a31n_on_method0_enter(void *uc, uint32_t helper) {
    const GwySmsCfgState *st;
    ensure();
    if (!g_n.enabled) return;
    g_n.active = 1;
    g_n.uc = uc;
    g_n.helper = helper;
    g_n.armed = 0;
    g_n.saw_post_gate = 0;
    g_n.saw_sms_377 = 0;
    g_n.saw_failfn = 0;
    g_n.saw_cmp_blx = 0;
    g_n.saw_bne = 0;
    g_n.saw_mvns = 0;
    g_n.saw_first_neg = 0;
    g_n.applied_int16 = 0;
    g_n.sms377_dst = 0;
    g_n.sms377_val_valid = 0;
    g_n.cmp_lhs[0] = 0;
    g_n.cmp_rhs[0] = 0;
    g_n.cmp_r0_out_valid = 0;
    g_n.bne_taken = -1;
    g_n.lhs_smscfg_derived = 0;
    g_n.call_n = 0;
    g_n.pred_n = 0;
    g_n.read_n = 0;
    g_n.cmp_n = 0;
    g_n.seq = 0;
    g_n.pending_call = 0;
    g_n.manifest_written = 0;
    ensure_csvs();
    st = gwy_sms_cfg_state();
    if (st) {
        g_n.cfg_base = st->cfg_guest;
        timing_row("method0_enter", "ENTER", st->dsm_generation, st->cfg_guest, st->erw_base,
                   st->initialized ? "cfg_already_bootstrapped" : "cfg_not_ready");
        if (!g_n.content_valid_printed) {
            g_n.content_valid_printed = 1;
            e10a31n_mark_milestone("SMSCFG_PROFILE_CONTENT_VALID",
                                   "GPT+gwy+int16_355_in_profile");
            e10a31n_mark_milestone("SMSCFG_EARLY_APPLICATION_UNSAFE",
                                   "bootstrap_0x355_host_crash_documented");
        }
        if (st->cfg_guest && st->erw_base)
            e10a31n_mark_milestone("SMSCFG_SAFE_APPLICATION_POINT_FOUND",
                                   "candidate_method0_enter_after_erw");
    }
#ifdef GWY_HAVE_UNICORN
    arm_mem_hook(uc);
#endif
    apply_int16_method0(uc);
    e10a31n_mark_milestone("E10A31N_ARMED", "post_range_0x2E5410");
}

void e10a31n_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t sp, uint32_t r0,
                             uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5,
                             uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size) {
    uint16_t h0 = 0, h1 = 0;
    int is_bl = 0, is_blx = 0;
    uint32_t tgt = 0;
    (void)sp;
    (void)r2;
    (void)r3;
    (void)r4;
    (void)r5;
    (void)r9;
    (void)cpsr;
    ensure();
    if (!g_n.enabled || !g_n.active) return;

    if (bytes && size >= 2) h0 = (uint16_t)(bytes[0] | (bytes[1] << 8));
    if (bytes && size >= 4) h1 = (uint16_t)(bytes[2] | (bytes[3] << 8));

    fill_pending_r0_out(pc, r0);

    if (pc == E10A31N_PC_POST_GATE_BL) {
        g_n.saw_post_gate = 1;
        g_n.armed = 1;
        push_call(pc, E10A31N_PC_CALLEE, pc + 4u, r0, "BL_0x2E2FED_post_range_gate");
        e10a31n_mark_milestone("METHOD0_2E5410_FUNCTION_IDENTIFIED",
                               "CALL_SITE_TO_0x2E2FED");
        push_pred(pc, lr, r0, r1, "BL", "N/A", -1, "enter_post_range");
        return;
    }

    if (!g_n.armed) {
        if (pc == E10A31N_PC_SMS_377 || pc == E10A31N_PC_FAILFN_BL ||
            pc == E10A31N_PC_CMP_BLX || pc == E10A31N_PC_MVNS) {
            g_n.armed = 1;
            g_n.saw_post_gate = 1;
        } else {
            return;
        }
    }

    if (pc == E10A31N_PC_SMS_377) {
        g_n.saw_sms_377 = 1;
        g_n.sms377_dst = r1;
        push_call(pc, 0, pc + 4u, r0, "BL_sms_get_bytes_off_0x377");
        push_pred(pc, lr, r0, r1, "BL_sms_get", "N/A", -1, "copy_len1_off_0x377");
        e10a31n_mark_milestone("SMSCFG_377_COPY_OBSERVED", "len1");
        return;
    }

    if (pc == E10A31N_PC_FAILFN_BL) {
        g_n.saw_failfn = 1;
        push_call(pc, 0x2E3F85u, pc + 4u, r0, "BL_failfn_0x2E3F85");
        return;
    }

    if (pc == E10A31N_PC_CMP_BLX) {
        CmpRow *c;
        g_n.saw_cmp_blx = 1;
        g_n.cmp_r0 = r0;
        g_n.cmp_r1 = r1;
        peek_cstr(uc, r0, g_n.cmp_lhs, sizeof(g_n.cmp_lhs));
        peek_cstr(uc, r1, g_n.cmp_rhs, sizeof(g_n.cmp_rhs));
        if (g_n.sms377_dst && (r0 == g_n.sms377_dst ||
                               (r0 >= g_n.sms377_dst && r0 < g_n.sms377_dst + 16u)))
            g_n.lhs_smscfg_derived = 1;
        push_call(pc, 0xAC4A4u, pc + 2u, r0, "BLX_strstr_napptype");
        if (g_n.cmp_n < E10A31N_CMP_MAX) {
            c = &g_n.cmps[g_n.cmp_n++];
            memset(c, 0, sizeof(*c));
            c->seq = ++g_n.seq;
            c->pc = pc;
            c->r0 = r0;
            c->r1 = r1;
            snprintf(c->kind, sizeof(c->kind), "%s",
                     g_n.cmp_lhs[0] && g_n.cmp_rhs[0] ? "STRCMP_LIKE" : "COMPARE_BLX");
            snprintf(c->lhs, sizeof(c->lhs), "%s", g_n.cmp_lhs);
            snprintf(c->rhs, sizeof(c->rhs), "%s", g_n.cmp_rhs);
            snprintf(c->note, sizeof(c->note), "lhs_smscfg=%d dest=0x%X",
                     g_n.lhs_smscfg_derived, g_n.sms377_dst);
            emit_cmp(c);
        }
        push_pred(pc, lr, r0, r1, "BLX", "RET_EQ0_FAIL", -1, "compare_before_MVNS");
        return;
    }

    if (pc == E10A31N_PC_BNE) {
        int taken = (r0 != 0);
        g_n.saw_bne = 1;
        g_n.bne_taken = taken ? 1 : 0;
        push_pred(pc, lr, r0, r1, "BNE", "NE", taken ? 1 : 0,
                  taken ? "skip_MVNS_success" : "fallthrough_MVNS");
        return;
    }

    if (pc == E10A31N_PC_MVNS) {
        g_n.saw_mvns = 1;
        push_pred(pc, lr, r0, r1, "MVNS_r0_r0", "N/A", -1, "DIRECT_FAILURE_neg1");
        e10a31n_mark_milestone("METHOD0_2E3FBA_ROLE_IDENTIFIED", "MVNS_direct_neg1");
        e10a31n_mark_milestone("METHOD0_2E3FBA_DIRECT_FAILURE", "after_zero_BLX");
        write_provenance_primary();
        write_manifest();
        return;
    }

    /* Dense BL/BLX logging inside armed window. */
    if (size >= 4 && decode_thumb_bl(h0, h1, pc, &tgt, &is_blx)) {
        is_bl = 1;
    } else if (size == 2 && (h0 & 0xFF80) == 0x4780) {
        uint32_t rm = (h0 >> 3) & 0xF;
        uint32_t rv = 0;
        is_bl = 1;
        is_blx = 1;
#ifdef GWY_HAVE_UNICORN
        if (uc) uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0 + (int)rm, &rv);
#else
        (void)rm;
#endif
        tgt = rv;
    }
    if (is_bl) {
        char note[64];
        snprintf(note, sizeof(note), "%s_window", is_blx ? "BLX" : "BL");
        push_call(pc, tgt, pc + (size >= 4 ? 4u : 2u), r0, note);
    }
}

void e10a31n_on_method0_return(void *uc, uint32_t helper, int32_t ret) {
    ensure();
    if (!g_n.enabled) return;
    (void)helper;
#ifdef GWY_HAVE_UNICORN
    disarm_mem_hook(uc);
#else
    (void)uc;
#endif
    if (g_n.saw_mvns || g_n.saw_post_gate) write_manifest();
    printf("[JJFB_E10A31N] method0_return ret=%d post_gate=%d sms377=%d cmp=%d mvns=%d "
           "apply_int16=%d lhs='%s' rhs='%s' evidence=OBSERVED\n",
           (int)ret, g_n.saw_post_gate, g_n.saw_sms_377, g_n.saw_cmp_blx, g_n.saw_mvns,
           g_n.applied_int16, g_n.cmp_lhs, g_n.cmp_rhs);
    fflush(stdout);
    if (ret == 0)
        e10a31n_mark_milestone("GAMELIST_METHOD0_RETURN_ZERO", "ret0");
    else if (g_n.saw_mvns)
        e10a31n_mark_milestone("METHOD0_2E3FBA_DIRECT_FAILURE", "captured");
    g_n.active = 0;
}
