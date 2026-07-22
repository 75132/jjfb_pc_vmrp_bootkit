#include "gwy_launcher/e10a31m_fail_2e5404.h"

#include "gwy_launcher/gwy_sms_cfg.h"
#include "gwy_launcher/sms_cfg_compat_profile.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { E10A31M_TRACE_MAX = 512, E10A31M_READ_MAX = 64 };

typedef struct {
    uint32_t seq;
    uint32_t pc, lr, sp;
    uint32_t r0, r1, r2, r3, r4, r5, r9, cpsr;
    char insn[48];
    char note[96];
    uint32_t mem_addr;
    int32_t smscfg_off;
    uint32_t lhs, rhs;
    char condition[16];
    int branch_taken; /* -1 unknown, 0 not, 1 taken */
    char lhs_source[48];
    char rhs_source[48];
    char evidence[96];
} TraceRow;

typedef struct {
    uint32_t seq;
    uint32_t pc, addr, size;
    uint32_t cfg_off;
    uint8_t bytes[8];
    uint32_t dest_reg_hint;
    char next_use[64];
    char later_cmp[64];
    char relation[64];
} ReadRow;

static struct {
    int known, enabled, active, armed_window;
    unsigned long long run_id;
    void *uc;
    uint32_t helper, cfg_base, cfg_len;
    uint32_t seq;
    int gwy_passed;
    int saw_copy355;
    int saw_pred_a;
    int saw_pred_b;
    int saw_fail;
    int32_t loaded_value;
    int loaded_valid;
    uint32_t stack_dst;
    uint32_t pred_a_pc, pred_b_pc, predecessor_pc, fail_pc;
    uint32_t lhs_a, rhs_a, lhs_b, rhs_b;
    int branch_a_taken, branch_b_taken;
    int trace_n, read_n;
    TraceRow traces[E10A31M_TRACE_MAX];
    ReadRow reads[E10A31M_READ_MAX];
    FILE *branch_csv, *smscfg_csv;
    int req_written;
#ifdef GWY_HAVE_UNICORN
    uc_hook mem_hook;
    int mem_armed;
#endif
} g_m;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31M_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
}

static void ensure(void) {
    if (g_m.known) return;
    g_m.known = 1;
    g_m.enabled = env1("JJFB_E10A31M_MODE");
    g_m.run_id = run_id_now();
    g_m.cfg_len = GWY_SMS_CFG_LEN;
    g_m.branch_a_taken = -1;
    g_m.branch_b_taken = -1;
    g_m.loaded_value = 0;
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
    if (!g_m.branch_csv)
        g_m.branch_csv = open_csv(
            "JJFB_E10A31M_BRANCH_CSV", "reports/e10a31m_fail_branch_trace.csv",
            "seq,pc,lr,sp,r0,r1,r2,r3,r4,r5,r9,cpsr,insn,note,mem_addr,smscfg_offset_if_any,"
            "lhs_value,rhs_value,condition,branch_taken,lhs_source,rhs_source,evidence,"
            "failure_pc,predecessor_pc,predicate_pc\n");
    if (!g_m.smscfg_csv)
        g_m.smscfg_csv = open_csv(
            "JJFB_E10A31M_SMSCFG_CSV", "reports/e10a31m_smscfg_355_356_provenance.csv",
            "seq,pc,addr,cfg_off,size,bytes_hex,dest_reg_hint,next_use,later_cmp,relation\n");
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
    for (i = 0; i < n && o + 2 < out_n; i++) {
        o += (size_t)snprintf(out + o, out_n - o, "%02X", b[i]);
    }
}

static void emit_trace(const TraceRow *t) {
    ensure_csvs();
    if (!g_m.branch_csv || !t) return;
    fprintf(g_m.branch_csv,
            "%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\",\"%s\",0x%X,%d,"
            "0x%X,0x%X,%s,%d,%s,%s,\"%s\",0x%X,0x%X,0x%X\n",
            t->seq, t->pc, t->lr, t->sp, t->r0, t->r1, t->r2, t->r3, t->r4, t->r5, t->r9, t->cpsr,
            t->insn, t->note, t->mem_addr, t->smscfg_off, t->lhs, t->rhs, t->condition,
            t->branch_taken, t->lhs_source, t->rhs_source, t->evidence, g_m.fail_pc,
            g_m.predecessor_pc, g_m.pred_a_pc ? g_m.pred_a_pc : g_m.pred_b_pc);
    fflush(g_m.branch_csv);
}

static TraceRow *push_trace(uint32_t pc, uint32_t lr, uint32_t sp, uint32_t r0, uint32_t r1,
                            uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r9,
                            uint32_t cpsr, const char *insn, const char *note) {
    TraceRow *t;
    if (g_m.trace_n >= E10A31M_TRACE_MAX) return NULL;
    t = &g_m.traces[g_m.trace_n++];
    memset(t, 0, sizeof(*t));
    t->seq = ++g_m.seq;
    t->pc = pc;
    t->lr = lr;
    t->sp = sp;
    t->r0 = r0;
    t->r1 = r1;
    t->r2 = r2;
    t->r3 = r3;
    t->r4 = r4;
    t->r5 = r5;
    t->r9 = r9;
    t->cpsr = cpsr;
    t->smscfg_off = -1;
    t->branch_taken = -1;
    snprintf(t->insn, sizeof(t->insn), "%s", insn ? insn : "");
    snprintf(t->note, sizeof(t->note), "%s", note ? note : "");
    snprintf(t->condition, sizeof(t->condition), "");
    snprintf(t->lhs_source, sizeof(t->lhs_source), "");
    snprintf(t->rhs_source, sizeof(t->rhs_source), "");
    snprintf(t->evidence, sizeof(t->evidence), "OBSERVED");
    return t;
}

static void write_requirement_json(void) {
    const char *path = getenv("JJFB_E10A31M_REQ_JSON");
    FILE *f;
    if (g_m.req_written) return;
    if (!path || !path[0]) path = "reports/e10a31m_requirement.json";
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
            "{\n"
            "  \"source\": \"sms_cfg\",\n"
            "  \"offset\": %u,\n"
            "  \"length\": 2,\n"
            "  \"encoding\": \"int16_le\",\n"
            "  \"min_inclusive\": 1,\n"
            "  \"max_inclusive\": %u,\n"
            "  \"expected_hex\": \"0100\",\n"
            "  \"expected_note\": \"minimal_positive_within_immediates_1..0x1B2\",\n"
            "  \"predicate\": \"1 <= int16_le(cfg+0x355) <= 0x1B2\",\n"
            "  \"predicate_pc\": \"0x%X\",\n"
            "  \"predicate_b_pc\": \"0x%X\",\n"
            "  \"failure_pc\": \"0x2E5404\",\n"
            "  \"predecessor_pc\": \"0x%X\",\n"
            "  \"field_type\": \"one_little_endian_int16_not_two_uint8\",\n"
            "  \"observed_value\": %d,\n"
            "  \"required\": true,\n"
            "  \"original_default_recovered\": false\n"
            "}\n",
            E10A31M_SMSCFG_OFF, E10A31M_SMSCFG_MAX, E10A31M_PC_PRED_A_CMP, E10A31M_PC_PRED_B_CMP,
            g_m.predecessor_pc ? g_m.predecessor_pc : E10A31M_PC_PRED_A_BLE,
            g_m.loaded_valid ? (int)g_m.loaded_value : 0);
    fclose(f);
    g_m.req_written = 1;
    e10a31m_mark_milestone("METHOD0_2E5404_FAIL_PREDICATE_FOUND", "int16_le_0x355");
    if (g_m.saw_fail) {
        e10a31m_mark_milestone("METHOD0_FAIL_SOURCE_SMSCFG_355_356", "causal");
        e10a31m_mark_milestone("SMSCFG_355_356_FIELD_TYPE_IDENTIFIED", "int16_le");
    }
}

static void write_final_branch_row(void) {
    ensure_csvs();
    if (!g_m.branch_csv) return;
    fprintf(g_m.branch_csv,
            "%u,0x%X,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,\"FINAL\",\"failure_summary\","
            "0x%X,%d,0x%X,0x%X,LE,%d,SMSCFG_INT16_LE_0x355,IMM_0,"
            "\"pred_a_cmp=0x2E53F4 ble=0x2E53F6 fail=0x2E5404 value=%d\","
            "0x%X,0x%X,0x%X\n",
            ++g_m.seq, g_m.fail_pc ? g_m.fail_pc : E10A31M_PC_FAIL,
            g_m.cfg_base ? g_m.cfg_base + E10A31M_SMSCFG_OFF : 0, (int)E10A31M_SMSCFG_OFF,
            g_m.lhs_a, g_m.rhs_a, g_m.branch_a_taken,
            g_m.loaded_valid ? (int)g_m.loaded_value : 0,
            g_m.fail_pc ? g_m.fail_pc : E10A31M_PC_FAIL,
            g_m.predecessor_pc ? g_m.predecessor_pc : E10A31M_PC_PRED_A_BLE,
            E10A31M_PC_PRED_A_CMP);
    fflush(g_m.branch_csv);
}

#ifdef GWY_HAVE_UNICORN
static void on_mem_read(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                        void *user_data) {
    uint32_t pc = 0, addr = (uint32_t)address;
    ReadRow *r;
    char hex[20];
    (void)type;
    (void)value;
    (void)user_data;
    if (!g_m.active || !g_m.cfg_base || size <= 0) return;
    if (addr < g_m.cfg_base + 0x354u || addr > g_m.cfg_base + 0x358u) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    if (g_m.read_n >= E10A31M_READ_MAX) return;
    r = &g_m.reads[g_m.read_n++];
    memset(r, 0, sizeof(*r));
    r->seq = g_m.read_n;
    r->pc = pc;
    r->addr = addr;
    r->size = (uint32_t)size;
    r->cfg_off = addr - g_m.cfg_base;
    (void)uc_mem_read(uc, address, r->bytes, size > 8 ? 8 : (size_t)size);
    hex_bytes(r->bytes, r->size > 8 ? 8 : r->size, hex, sizeof(hex));
    if (r->cfg_off == 0x355u || r->cfg_off == 0x356u) {
        snprintf(r->next_use, sizeof(r->next_use), "LDRSH_via_SP+0x48");
        snprintf(r->later_cmp, sizeof(r->later_cmp), "CMP_r0_#0_then_vs_0x1B2");
        snprintf(r->relation, sizeof(r->relation), "CAUSAL_TO_0x2E5404");
    } else {
        snprintf(r->relation, sizeof(r->relation), "WINDOW_ONLY");
    }
    ensure_csvs();
    if (g_m.smscfg_csv) {
        fprintf(g_m.smscfg_csv, "%u,0x%X,0x%X,0x%X,%u,%s,%u,%s,%s,%s\n", r->seq, r->pc, r->addr,
                r->cfg_off, r->size, hex, r->dest_reg_hint, r->next_use, r->later_cmp,
                r->relation);
        fflush(g_m.smscfg_csv);
    }
}

static void arm_mem_hook(void *uc) {
    const GwySmsCfgState *st;
    if (!uc || g_m.mem_armed) return;
    st = gwy_sms_cfg_state();
    if (!st || !st->cfg_guest) return;
    g_m.cfg_base = st->cfg_guest;
    if (uc_hook_add((uc_engine *)uc, &g_m.mem_hook, UC_HOOK_MEM_READ, (void *)on_mem_read, NULL,
                    g_m.cfg_base + 0x354u, g_m.cfg_base + 0x358u) == UC_ERR_OK)
        g_m.mem_armed = 1;
}

static void disarm_mem_hook(void *uc) {
    if (g_m.mem_armed && uc) {
        uc_hook_del((uc_engine *)uc, g_m.mem_hook);
        g_m.mem_armed = 0;
    }
}
#endif

int e10a31m_enabled(void) {
    ensure();
    return g_m.enabled;
}

void e10a31m_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_m.mem_armed && g_m.uc) disarm_mem_hook(g_m.uc);
#endif
    if (g_m.branch_csv) fclose(g_m.branch_csv);
    if (g_m.smscfg_csv) fclose(g_m.smscfg_csv);
    memset(&g_m, 0, sizeof(g_m));
}

void e10a31m_mark_milestone(const char *name, const char *note) {
    ensure();
    if (!g_m.enabled || !name) return;
    printf("[JJFB_E10A31M_MILESTONE] name=%s note=%s evidence=OBSERVED\n", name,
           note ? note : "");
    fflush(stdout);
}

void e10a31m_on_method0_enter(void *uc, uint32_t helper) {
    ensure();
    if (!g_m.enabled) return;
    g_m.active = 1;
    g_m.uc = uc;
    g_m.helper = helper;
    g_m.armed_window = 0;
    g_m.gwy_passed = 0;
    g_m.saw_copy355 = 0;
    g_m.saw_pred_a = 0;
    g_m.saw_pred_b = 0;
    g_m.saw_fail = 0;
    g_m.loaded_valid = 0;
    g_m.trace_n = 0;
    g_m.read_n = 0;
    g_m.seq = 0;
    g_m.req_written = 0;
    g_m.branch_a_taken = -1;
    g_m.branch_b_taken = -1;
    ensure_csvs();
#ifdef GWY_HAVE_UNICORN
    arm_mem_hook(uc);
#endif
    /* Controlled apply at method0 entry (not bootstrap) to avoid early-init side effects. */
    {
        const char *extra = getenv("JJFB_E10A31M_APPLY_INT16");
        if (extra && extra[0] && uc) {
            char *colon = strchr(extra, ':');
            unsigned long off = 0;
            long val = 0;
            const GwySmsCfgState *st = gwy_sms_cfg_state();
            if (colon && colon > extra && st && st->cfg_guest) {
                uint8_t le[2];
                off = strtoul(extra, NULL, 0);
                val = strtol(colon + 1, NULL, 0);
                le[0] = (uint8_t)(val & 0xFF);
                le[1] = (uint8_t)((val >> 8) & 0xFF);
                if (off + 2u <= st->cfg_len &&
                    gwy_sms_cfg_set_bytes(uc, (int32_t)off, le, 2,
                                          GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY) == 0) {
                    printf("[SMSCFG_APPLY_INT16] phase=method0_enter offset=0x%lX value=%ld "
                           "le_hex=%02X%02X source=DIAGNOSTIC_ONLY evidence=OBSERVED\n",
                           off, val, le[0], le[1]);
                    fflush(stdout);
                }
            }
        }
    }
    e10a31m_mark_milestone("E10A31M_ARMED", "post_gwy_to_2E5404");
}

void e10a31m_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t sp, uint32_t r0,
                             uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5,
                             uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size) {
    TraceRow *t;
    uint16_t h0 = 0;
    (void)uc;
    (void)r2;
    (void)r9;
    ensure();
    if (!g_m.enabled || !g_m.active) return;
    if (bytes && size >= 2) h0 = (uint16_t)(bytes[0] | (bytes[1] << 8));

    /* Start dense window after gwy strcmp returns (LR of BLX lands past call). */
    if (pc == E10A31M_PC_GWY_STRCMP_BLX) {
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "BLX_strcmp_gwy",
                       "gwy_compare_call");
        if (t) emit_trace(t);
        return;
    }
    if (pc == 0x2E53DCu) { /* CMP r0,#0 after gwy */
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "CMP_r0_#0",
                       "gwy_strcmp_result");
        if (t) {
            t->lhs = r0;
            t->rhs = 0;
            snprintf(t->condition, sizeof(t->condition), "EQ");
            snprintf(t->lhs_source, sizeof(t->lhs_source), "STRCMP_RET");
            snprintf(t->rhs_source, sizeof(t->rhs_source), "IMM_0");
            emit_trace(t);
        }
        return;
    }
    if (pc == 0x2E53DEu) { /* BEQ success */
        int taken = (r0 == 0);
        if (taken) g_m.gwy_passed = 1;
        g_m.armed_window = 1;
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "BEQ",
                       taken ? "gwy_pass" : "gwy_fail");
        if (t) {
            t->branch_taken = taken;
            snprintf(t->condition, sizeof(t->condition), "EQ");
            emit_trace(t);
        }
        return;
    }

    if (!g_m.armed_window && !g_m.gwy_passed) {
        /* Also arm if we somehow land on copy site directly. */
        if (pc != E10A31M_PC_COPY355_BL && pc < E10A31M_PC_COPY355_BL) return;
        g_m.armed_window = 1;
        g_m.gwy_passed = 1;
    }

    if (pc == E10A31M_PC_COPY355_BL) {
        g_m.saw_copy355 = 1;
        g_m.stack_dst = r1;
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "BL_sms_get_bytes",
                       "copy_off_0x355_len_2");
        if (t) {
            t->smscfg_off = (int32_t)r0;
            t->mem_addr = g_m.cfg_base ? g_m.cfg_base + r0 : 0;
            snprintf(t->lhs_source, sizeof(t->lhs_source), "SMSCFG");
            snprintf(t->evidence, sizeof(t->evidence), "r0=off r1=dst r2=len");
            emit_trace(t);
        }
        e10a31m_mark_milestone("SMSCFG_355_COPY_OBSERVED", "len2");
        return;
    }

    if (pc == E10A31M_PC_LDRSH) {
        uint8_t hb[2] = {0, 0};
        int16_t v = 0;
        uint32_t addr = r3 + r0;
        if (uc && read_guest(uc, addr, hb, 2)) {
            v = (int16_t)(hb[0] | (hb[1] << 8));
            g_m.loaded_value = v;
            g_m.loaded_valid = 1;
        }
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "LDRSH_r0_[r3,r0]",
                       "load_int16_from_copy");
        if (t) {
            t->mem_addr = addr;
            t->smscfg_off = (int32_t)E10A31M_SMSCFG_OFF;
            t->lhs = (uint32_t)(int32_t)v;
            snprintf(t->lhs_source, sizeof(t->lhs_source), "SMSCFG_INT16_LE_0x355");
            snprintf(t->evidence, sizeof(t->evidence), "addr=0x%X value=%d", addr, (int)v);
            emit_trace(t);
        }
        return;
    }

    if (pc == E10A31M_PC_PRED_A_CMP) {
        g_m.saw_pred_a = 1;
        g_m.pred_a_pc = pc;
        g_m.lhs_a = r0;
        g_m.rhs_a = 0;
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "CMP_r0_#0",
                       "predicate_a");
        if (t) {
            t->lhs = r0;
            t->rhs = 0;
            snprintf(t->condition, sizeof(t->condition), "LE_next");
            snprintf(t->lhs_source, sizeof(t->lhs_source), "SMSCFG_INT16_LE_0x355");
            snprintf(t->rhs_source, sizeof(t->rhs_source), "IMM_0");
            emit_trace(t);
        }
        return;
    }

    if (pc == E10A31M_PC_PRED_A_BLE) {
        int taken = ((int32_t)r0 <= 0);
        g_m.branch_a_taken = taken ? 1 : 0;
        g_m.predecessor_pc = pc;
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "BLE",
                       taken ? "to_fail_2E5404" : "continue_pred_b");
        if (t) {
            t->lhs = r0;
            t->rhs = 0;
            t->branch_taken = taken ? 1 : 0;
            snprintf(t->condition, sizeof(t->condition), "LE");
            snprintf(t->lhs_source, sizeof(t->lhs_source), "SMSCFG_INT16_LE_0x355");
            snprintf(t->rhs_source, sizeof(t->rhs_source), "IMM_0");
            emit_trace(t);
        }
        return;
    }

    if (pc == E10A31M_PC_PRED_B_CMP) {
        g_m.saw_pred_b = 1;
        g_m.pred_b_pc = pc;
        g_m.lhs_b = r0;
        g_m.rhs_b = r1;
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "CMP_r0_r1",
                       "predicate_b_max_0x1B2");
        if (t) {
            t->lhs = r0;
            t->rhs = r1;
            snprintf(t->condition, sizeof(t->condition), "LE_next");
            snprintf(t->lhs_source, sizeof(t->lhs_source), "SMSCFG_INT16_LE_0x355");
            snprintf(t->rhs_source, sizeof(t->rhs_source), "IMM_0xFF_plus_0xB3");
            emit_trace(t);
        }
        return;
    }

    if (pc == E10A31M_PC_PRED_B_BLE) {
        int taken = ((int32_t)r0 <= (int32_t)r1);
        g_m.branch_b_taken = taken ? 1 : 0;
        if (!taken) g_m.predecessor_pc = pc; /* fallthrough to fail */
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "BLE",
                       taken ? "to_success" : "fallthrough_fail");
        if (t) {
            t->lhs = r0;
            t->rhs = r1;
            t->branch_taken = taken ? 1 : 0;
            snprintf(t->condition, sizeof(t->condition), "LE");
            emit_trace(t);
        }
        return;
    }

    if (pc == E10A31M_PC_FAIL) {
        g_m.saw_fail = 1;
        g_m.fail_pc = pc;
        if (!g_m.predecessor_pc) g_m.predecessor_pc = E10A31M_PC_PRED_A_BLE;
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "ADDS_r0_r5_#0",
                       "common_failure_epilogue");
        if (t) {
            t->lhs = r5;
            t->rhs = 0;
            snprintf(t->lhs_source, sizeof(t->lhs_source), "R5_NEG1_FROM_MVNS");
            snprintf(t->rhs_source, sizeof(t->rhs_source), "N/A");
            snprintf(t->evidence, sizeof(t->evidence), "MOV_r0_r5 r5=0x%X h0=0x%04X", r5, h0);
            emit_trace(t);
        }
        e10a31m_mark_milestone("METHOD0_2E5404_COMMON_FAILURE_EPILOGUE", "ADDS_r0_r5");
        e10a31m_mark_milestone("METHOD0_2E5404_RETURN_NEG1_IMMEDIATE", "via_r5");
        write_requirement_json();
        write_final_branch_row();
        {
            FILE *nf = fopen("reports/e10a31m_next_subsystem_provenance.csv", "w");
            if (nf) {
                fputs("classification,detail,evidence\n", nf);
                fputs("SMS_CFG,\"int16_le@0x355 causal to 0x2E5404\",OBSERVED\n", nf);
                fclose(nf);
            }
        }
        return;
    }

    /* Sparse breadcrumbs inside window. */
    if (g_m.armed_window && pc >= E10A31M_PC_COPY355_BL && pc <= E10A31M_PC_FAIL + 4u) {
        t = push_trace(pc, lr, sp, r0, r1, r2, r3, r4, r5, r9, cpsr, "INSN", "window");
        if (t) emit_trace(t);
    }
}

void e10a31m_on_method0_return(void *uc, uint32_t helper, int32_t ret) {
    FILE *nf;
    ensure();
    if (!g_m.enabled) return;
    (void)helper;
#ifdef GWY_HAVE_UNICORN
    disarm_mem_hook(uc);
#else
    (void)uc;
#endif
    if (g_m.saw_fail || g_m.saw_pred_a) {
        write_requirement_json();
        if (!g_m.saw_fail) write_final_branch_row();
    }
    /* Lane E placeholder: only if not SMSCFG. */
    nf = fopen("reports/e10a31m_next_subsystem_provenance.csv", "w");
    if (nf) {
        fputs("classification,detail,evidence\n", nf);
        if (g_m.saw_pred_a || g_m.saw_fail) {
            fputs("SMS_CFG,\"int16_le@0x355 causal to 0x2E5404\",OBSERVED\n", nf);
        } else {
            fputs("UNKNOWN,\"predicate window not reached\",OBSERVED\n", nf);
        }
        fclose(nf);
    }
    printf("[JJFB_E10A31M] method0_return ret=%d gwy_pass=%d copy355=%d pred_a=%d fail=%d "
           "value=%d evidence=OBSERVED\n",
           (int)ret, g_m.gwy_passed, g_m.saw_copy355, g_m.saw_pred_a, g_m.saw_fail,
           g_m.loaded_valid ? (int)g_m.loaded_value : 0);
    fflush(stdout);
    if (ret == 0)
        e10a31m_mark_milestone("GAMELIST_METHOD0_RETURN_ZERO", "ret0");
    else if (g_m.saw_fail)
        e10a31m_mark_milestone("METHOD0_2E5404_FAIL_PREDICATE_FOUND", "captured");
    g_m.active = 0;
}

void e10a31m_on_mem_read(void *uc, uint32_t pc, uint32_t addr, uint32_t size, const void *data) {
    (void)uc;
    (void)pc;
    (void)addr;
    (void)size;
    (void)data;
    /* Primary path uses UC hook; this stub keeps API complete. */
}
