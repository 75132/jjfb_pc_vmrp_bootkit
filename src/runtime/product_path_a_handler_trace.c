#include "gwy_launcher/product_path_a_handler_trace.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/platform_event_service.h"
#include "gwy_launcher/product_runtime_progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PC_2E2520 0x2E2520u
#define PC_2E4040 0x2E4040u
#define PC_2E4066 0x2E4066u
#define PC_2E4194 0x2E4194u
#define PC_2F68E4 0x2F68E4u
#define PC_2DADC4 0x2DADC4u
#define PC_305EC2 0x305EC2u
#define PC_312A60 0x312A60u
#define PC_312C0C 0x312C0Cu
#define PC_2DC80C 0x2DC80Cu

#define OFF_15C 0x15Cu
#define OFF_15D 0x15Du
#define OFF_B71 0xB71u
#define OFF_134D 0x134Du
#define OFF_C76 0xC76u
#define OFF_B54 0xB54u
#define OFF_UI GWY_PES_UI_MODE_OFF

#define CALL_CAP 256
#define STORE_CAP 192
#define GATE_CAP 48
#define API_CAP 64
#define INSN_BUDGET 200000u
#define STORE_LOG_CAP 64
#define POST_TICK_TARGET 100u

typedef struct {
    uint32_t seq;
    uint32_t handler_call_id;
    uint32_t depth;
    uint32_t source_pc;
    uint32_t target;
    uint32_t lr;
    uint32_t r0, r1, r2, r3;
    uint32_t ret;
    char kind[12];
} PahCall;

typedef struct {
    uint32_t seq;
    uint32_t handler_call_id;
    uint32_t store_pc;
    uint32_t lr;
    uint32_t er_off;
    uint32_t old_v, new_v;
    uint32_t size;
    char cls[16];
} PahStore;

typedef struct {
    uint32_t seq;
    char stage[40];
    uint32_t pc;
    uint32_t v15c, v15d, b71, v134d, c76, ui;
} PahGate;

typedef struct {
    uint32_t seq;
    char api[40];
    uint32_t a0, a1;
    uint32_t handler_call_id;
    int during_handler;
} PahApi;

static int g_en, g_en_known, g_finalized, g_hook_ok, g_atexit_ok;
static char g_run_id[80];
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_code_base, g_code_end;
static uint32_t g_seq;
#ifdef GWY_HAVE_UNICORN
static uc_hook g_dense_hook;
static uc_hook g_mem_hook;
static int g_dense_armed;
static int g_mem_armed;
#endif

static uint32_t g_handler_call_id;
static int g_handler_active;
static int g_handler_entered;
static int g_handler_returned;
static int g_valid_dispatch;
static uint32_t g_entry_pc, g_entry_lr, g_entry_sp, g_entry_cpsr;
static uint32_t g_entry_r0, g_entry_r1, g_entry_r2, g_entry_r3, g_entry_r9;
static uint32_t g_entry_w0, g_entry_w4, g_entry_w8, g_entry_wC;
static uint32_t g_inner;
static uint8_t g_inner_bytes[32];
static int g_inner_n;
static uint32_t g_call_depth;
static uint32_t g_insn_count;
static int g_budget_hit;
static uint32_t g_return_pc, g_return_r0;
static int g_seen_2e4066, g_seen_2f68e4, g_seen_2dadc4, g_seen_2e4194;
static int g_freed_entry, g_requeue, g_new_event, g_callback_reg;
static int g_resource_seen, g_disp_seen, g_post_event_seen;
static int g_fault;
static uint32_t g_post_ticks;
static int g_post_window_done;
static uint32_t g_dispatch_call_serial;

static PahCall g_calls[CALL_CAP];
static int g_call_n;
static PahStore g_stores[STORE_CAP];
static int g_store_n;
static PahGate g_gates[GATE_CAP];
static int g_gate_n;
static PahApi g_apis[API_CAP];
static int g_api_n;

static int env1(const char *k) {
    const char *v = getenv(k);
    return v && v[0] == '1' && v[1] == 0;
}

static const char *report_path(const char *name, char *buf, size_t n) {
    const char *dir = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (dir && dir[0])
        snprintf(buf, n, "%s/%s", dir, name);
    else
        snprintf(buf, n, "reports/%s", name);
    return buf;
}

static void write_reports(void);

int product_pah_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_PATH_A_HANDLER_TRACE");
        g_en_known = 1;
    }
    return g_en;
}

void product_pah_reset(void) {
    g_finalized = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_code_base = g_code_end = 0;
    g_seq = 0;
    g_handler_call_id = 0;
    g_handler_active = g_handler_entered = g_handler_returned = 0;
    g_valid_dispatch = 0;
    g_entry_pc = g_entry_lr = g_entry_sp = g_entry_cpsr = 0;
    g_entry_r0 = g_entry_r1 = g_entry_r2 = g_entry_r3 = g_entry_r9 = 0;
    g_entry_w0 = g_entry_w4 = g_entry_w8 = g_entry_wC = 0;
    g_inner = 0;
    g_inner_n = 0;
    memset(g_inner_bytes, 0, sizeof(g_inner_bytes));
    g_call_depth = g_insn_count = 0;
    g_budget_hit = 0;
    g_return_pc = g_return_r0 = 0;
    g_seen_2e4066 = g_seen_2f68e4 = g_seen_2dadc4 = g_seen_2e4194 = 0;
    g_freed_entry = g_requeue = g_new_event = g_callback_reg = 0;
    g_resource_seen = g_disp_seen = g_post_event_seen = 0;
    g_fault = 0;
    g_post_ticks = 0;
    g_post_window_done = 0;
    g_dispatch_call_serial = 0;
    g_call_n = g_store_n = g_gate_n = g_api_n = 0;
    g_hook_ok = 0;
    g_en_known = 0;
    g_en = 0;
#ifdef GWY_HAVE_UNICORN
    g_dense_hook = 0;
    g_mem_hook = 0;
    g_dense_armed = 0;
    g_mem_armed = 0;
#endif
    memset(g_calls, 0, sizeof(g_calls));
    memset(g_stores, 0, sizeof(g_stores));
    memset(g_gates, 0, sizeof(g_gates));
    memset(g_apis, 0, sizeof(g_apis));
}

void product_pah_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_pah_run_id(void) {
    const char *e;
    if (g_run_id[0]) return g_run_id;
    e = getenv("GWY_PRODUCT_RUN_ID");
    return (e && e[0]) ? e : "unknown";
}

void product_pah_bind_uc(void *uc) { g_uc = uc; }

void product_pah_note_er_rw(uint32_t er_rw) {
    if (er_rw) g_er_rw = er_rw;
}

void product_pah_note_module_range(uint32_t code_base, uint32_t code_size) {
    if (!code_base || !code_size) return;
    g_code_base = code_base & ~1u;
    g_code_end = g_code_base + code_size;
}

static void pah_atexit(void) {
    if (product_pah_enabled()) product_pah_finalize();
}

#ifdef GWY_HAVE_UNICORN
static uint32_t peek_u32(uc_engine *uc, uint32_t a) {
    uint32_t v = 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, a, &v);
    return v;
}

static uint32_t peek_u8(uc_engine *uc, uint32_t a) {
    uint8_t b = 0;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, a, &b, 1);
    return b;
}

static void sample_gate(uc_engine *uc, const char *stage, uint32_t pc) {
    PahGate *g;
    if (!product_pah_enabled() || !uc || !g_er_rw || g_gate_n >= GATE_CAP) return;
    g = &g_gates[g_gate_n++];
    memset(g, 0, sizeof(*g));
    g->seq = ++g_seq;
    snprintf(g->stage, sizeof(g->stage), "%s", stage ? stage : "?");
    g->pc = pc;
    g->v15c = peek_u8(uc, g_er_rw + OFF_15C);
    g->v15d = peek_u8(uc, g_er_rw + OFF_15D);
    g->b71 = peek_u8(uc, g_er_rw + OFF_B71);
    g->v134d = peek_u8(uc, g_er_rw + OFF_134D);
    g->c76 = peek_u8(uc, g_er_rw + OFF_C76);
    g->ui = peek_u32(uc, g_er_rw + OFF_UI);
    printf("[PAH_GATE] stage=%s pc=0x%X 15C=%u 15D=%u B71=%u 134D=%u C76=%u UI_MODE=0x%X "
           "evidence=OBSERVED\n",
           g->stage, pc, g->v15c, g->v15d, g->b71, g->v134d, g->c76, g->ui);
    fflush(stdout);
}

static const char *classify_er_off(uint32_t off) {
    if (off == OFF_15C || off == OFF_15D || off == OFF_B71 || off == OFF_134D || off == OFF_C76 ||
        off == OFF_UI)
        return "lifecycle";
    if (off == OFF_B54 || (off >= 0xB50u && off <= 0xB60u)) return "queue";
    if (off >= 0xA90u && off <= 0xAA0u) return "lifecycle";
    if (off >= 0x800u && off <= 0x900u) return "UI";
    return "unknown";
}

static void add_call(const char *kind, uint32_t src, uint32_t tgt, uint32_t lr, uint32_t depth,
                     uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    PahCall *c;
    if (g_call_n >= CALL_CAP) return;
    c = &g_calls[g_call_n++];
    memset(c, 0, sizeof(*c));
    c->seq = ++g_seq;
    c->handler_call_id = g_handler_call_id;
    c->depth = depth;
    c->source_pc = src;
    c->target = tgt;
    c->lr = lr;
    c->r0 = r0;
    c->r1 = r1;
    c->r2 = r2;
    c->r3 = r3;
    snprintf(c->kind, sizeof(c->kind), "%s", kind ? kind : "BL");
    printf("[PAH_CALL] id=%u depth=%u kind=%s src=0x%X tgt=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X "
           "r3=0x%X evidence=OBSERVED\n",
           g_handler_call_id, depth, c->kind, src, tgt, lr, r0, r1, r2, r3);
    fflush(stdout);
}

static void on_dense(uc_engine *uc, uint64_t address, uint32_t size, void *user_data);
static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                         void *user_data);

static void arm_dense_for_handler(uc_engine *uc) {
    uint32_t dens_lo, dens_hi;
    uc_err e;
    if (!uc || g_dense_armed) return;
    dens_lo = g_code_base ? g_code_base : 0x2D8DF4u;
    dens_hi = g_code_end ? (g_code_end - 1u) : 0x320000u;
    e = uc_hook_add(uc, &g_dense_hook, UC_HOOK_CODE, (void *)on_dense, NULL, (uint64_t)dens_lo,
                    (uint64_t)dens_hi);
    if (e == UC_ERR_OK) g_dense_armed = 1;
    if (g_er_rw && !g_mem_armed) {
        e = uc_hook_add(uc, &g_mem_hook, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL,
                        (uint64_t)g_er_rw, (uint64_t)(g_er_rw + 0x1FFFu));
        if (e == UC_ERR_OK) g_mem_armed = 1;
    }
    printf("[PAH_DENSE] armed lo=0x%X hi=0x%X mem=%d evidence=OBSERVED\n", dens_lo, dens_hi,
           g_mem_armed);
    fflush(stdout);
}

static void disarm_dense_for_handler(uc_engine *uc) {
    if (!uc) return;
    if (g_dense_armed) {
        (void)uc_hook_del(uc, g_dense_hook);
        g_dense_hook = 0;
        g_dense_armed = 0;
    }
    if (g_mem_armed) {
        (void)uc_hook_del(uc, g_mem_hook);
        g_mem_hook = 0;
        g_mem_armed = 0;
    }
}

static void finish_handler(uc_engine *uc, const char *why) {
    uint32_t r0 = 0, pc = 0;
    if (!g_handler_active) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    g_return_r0 = r0;
    g_return_pc = pc;
    g_handler_active = 0;
    g_handler_returned = 1;
    disarm_dense_for_handler(uc);
    sample_gate(uc, "handler_return", pc);
    product_runtime_progress_emit("path_a_handler_returned", "pah", why ? why : "ret");
    printf("[PAH_RETURN] id=%u pc=0x%X ret=0x%X insn=%u calls=%d stores=%d why=%s "
           "seen_2E4066=%d seen_2F68E4=%d seen_2DADC4=%d seen_2E4194=%d evidence=OBSERVED\n",
           g_handler_call_id, pc, r0, g_insn_count, g_call_n, g_store_n, why ? why : "?",
           g_seen_2e4066, g_seen_2f68e4, g_seen_2dadc4, g_seen_2e4194);
    fflush(stdout);
    write_reports();
}

static void begin_handler(uc_engine *uc, uint32_t pc, uint32_t lr, uint32_t sp, uint32_t cpsr,
                          uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9) {
    uint32_t r4 = 0;
    uint32_t entry;
    if (g_handler_active) finish_handler(uc, "reenter");
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    /*
     * 0x2E2520 switch head overwrites R0 with event_code/index before ADD PC.
     * Case body 0x2E4040 uses R4 as the event entry pointer (LDR r0,[r4,#4]).
     */
    entry = r4 ? r4 : r1;
    g_handler_call_id++;
    g_handler_active = 1;
    g_handler_entered = 1;
    g_handler_returned = 0;
    g_budget_hit = 0;
    g_entry_pc = pc;
    g_entry_lr = lr;
    g_entry_sp = sp;
    g_entry_cpsr = cpsr;
    g_entry_r0 = r0;
    g_entry_r1 = r1;
    g_entry_r2 = r2;
    g_entry_r3 = r3;
    g_entry_r9 = r9;
    g_call_depth = 0;
    g_insn_count = 0;
    g_entry_w0 = entry ? peek_u32(uc, entry) : 0;
    g_entry_w4 = entry ? peek_u32(uc, entry + 4u) : 0;
    g_entry_w8 = entry ? peek_u32(uc, entry + 8u) : 0;
    g_entry_wC = entry ? peek_u32(uc, entry + 0xCu) : 0;
    g_inner = g_entry_w4;
    g_inner_n = 0;
    memset(g_inner_bytes, 0, sizeof(g_inner_bytes));
    if (g_inner) {
        if (guest_memory_uc_peek((struct uc_struct *)uc, g_inner, g_inner_bytes,
                                 (uint32_t)sizeof(g_inner_bytes)))
            g_inner_n = (int)sizeof(g_inner_bytes);
    }
    if (!g_er_rw && r9) g_er_rw = r9;
    arm_dense_for_handler(uc);
    sample_gate(uc, "handler_enter", pc);
    product_runtime_progress_emit("path_a_handler_entered", "pah", "0x2E4040");
    printf("[PAH_ENTER] handler_call_id=%u PC=0x%X LR=0x%X SP=0x%X CPSR=0x%X R0=0x%X R1=0x%X "
           "R2=0x%X R3=0x%X R4=0x%X R9=0x%X ER_RW=0x%X entry=0x%X +0=0x%X +4=0x%X +8=0x%X "
           "+C=0x%X inner=0x%X evidence=OBSERVED\n",
           g_handler_call_id, pc, lr, sp, cpsr, r0, r1, r2, r3, r4, r9, g_er_rw, entry,
           g_entry_w0, g_entry_w4, g_entry_w8, g_entry_wC, g_inner);
    if (g_inner_n > 0) {
        int i;
        printf("[PAH_INNER] id=%u bytes=", g_handler_call_id);
        for (i = 0; i < g_inner_n && i < 32; i++) printf("%02X", g_inner_bytes[i]);
        printf(" evidence=OBSERVED\n");
    }
    fflush(stdout);
}

static int decode_bl_thumb(uc_engine *uc, uint32_t pc, uint32_t *out_tgt, int *out_blx) {
    uint8_t b[4];
    uint16_t h0, h1;
    int32_t imm;
    int s, j1, j2, i1, i2;
    uint32_t imm10, imm11;
    *out_tgt = 0;
    *out_blx = 0;
    if (!guest_memory_uc_peek((struct uc_struct *)uc, pc, b, 4)) return 0;
    h0 = (uint16_t)(b[0] | (b[1] << 8));
    h1 = (uint16_t)(b[2] | (b[3] << 8));
    if ((h0 & 0xF800u) != 0xF000u) return 0;
    if ((h1 & 0xD000u) == 0xD000u) {
        /* BL */
        s = (h0 >> 10) & 1;
        imm10 = h0 & 0x3FFu;
        j1 = (h1 >> 13) & 1;
        j2 = (h1 >> 11) & 1;
        imm11 = h1 & 0x7FFu;
        i1 = 1 - (j1 ^ s);
        i2 = 1 - (j2 ^ s);
        imm = (int32_t)((s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1));
        if (s) imm -= (1 << 25);
        *out_tgt = (pc + 4u + (uint32_t)imm) & ~1u;
        *out_blx = 0;
        return 1;
    }
    if ((h1 & 0xD000u) == 0xC000u) {
        s = (h0 >> 10) & 1;
        imm10 = h0 & 0x3FFu;
        j1 = (h1 >> 13) & 1;
        j2 = (h1 >> 11) & 1;
        imm11 = h1 & 0x7FFu;
        i1 = 1 - (j1 ^ s);
        i2 = 1 - (j2 ^ s);
        imm = (int32_t)((s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1));
        if (s) imm -= (1 << 25);
        *out_tgt = (pc + 4u + (uint32_t)imm) & ~1u;
        *out_blx = 1;
        return 1;
    }
    return 0;
}

static void on_dense(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t lr = 0, sp = 0, cpsr = 0;
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    uint32_t tgt = 0;
    int is_blx = 0;
    uint8_t hw[2];
    uint16_t h0;
    (void)size;
    (void)user_data;
    if (!product_pah_enabled() || !g_handler_active) return;

    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);

    /* Return to caller of 0x2E4040 (Thumb LR may have LSB). */
    if (pc == (g_entry_lr & ~1u) || pc == g_entry_lr) {
        finish_handler(uc, "ret_lr");
        return;
    }

    if (g_insn_count >= INSN_BUDGET) {
        if (!g_budget_hit) {
            g_budget_hit = 1;
            printf("[PAH_BUDGET] id=%u insn=%u note=stop_logging_keep_waiting_ret_lr "
                   "evidence=OBSERVED\n",
                   g_handler_call_id, g_insn_count);
            fflush(stdout);
            /* Drop heavy MEM watch; keep CODE so we still see BL and ret_lr. */
            if (g_mem_armed) {
                (void)uc_hook_del(uc, g_mem_hook);
                g_mem_hook = 0;
                g_mem_armed = 0;
            }
        }
        /* Still watch for return / key PCs below; do not finish_handler here. */
    } else {
        g_insn_count++;
    }

    if (pc == PC_2E4066) {
        g_seen_2e4066 = 1;
        sample_gate(uc, "inside_2E4066", pc);
    }
    if (pc == PC_2F68E4) g_seen_2f68e4 = 1;
    if (pc == PC_2DADC4) {
        g_seen_2dadc4 = 1;
        sample_gate(uc, "inside_2DADC4", pc);
        product_runtime_progress_emit("post_dispatch_event_seen", "pah", "2DADC4");
        g_post_event_seen = 1;
    }
    if (pc == PC_2E4194) g_seen_2e4194 = 1;

    if (decode_bl_thumb(uc, pc, &tgt, &is_blx)) {
        if (!g_budget_hit || tgt == PC_2DADC4 || tgt == PC_2E4066 || tgt == PC_2F68E4 ||
            tgt == PC_2E4194) {
            add_call(is_blx ? "BLX" : "BL", pc, tgt, (pc + 4u) | 1u, g_call_depth + 1u, r0, r1, r2,
                     r3);
        }
        g_call_depth++;
        if (tgt == PC_2DADC4) {
            g_seen_2dadc4 = 1;
            g_post_event_seen = 1;
            sample_gate(uc, "bl_2DADC4", pc);
            product_runtime_progress_emit("post_dispatch_event_seen", "pah", "2DADC4");
        }
        if (tgt == PC_2E4066) g_seen_2e4066 = 1;
        if (tgt == PC_2F68E4) g_seen_2f68e4 = 1;
        return;
    }

    if (g_budget_hit) return;

    /* BLX Rm / BX Rm */
    if (guest_memory_uc_peek((struct uc_struct *)uc, pc, hw, 2)) {
        h0 = (uint16_t)(hw[0] | (hw[1] << 8));
        if ((h0 & 0xFF80u) == 0x4780u) { /* BLX Rm */
            int rm = (h0 >> 3) & 0xF;
            uint32_t rv = 0;
            if (rm <= 12)
                uc_reg_read(uc, UC_ARM_REG_R0 + rm, &rv);
            else if (rm == 14)
                rv = lr;
            add_call("BLX_Rm", pc, rv & ~1u, (pc + 2u) | 1u, g_call_depth + 1u, r0, r1, r2, r3);
            g_call_depth++;
        } else if ((h0 & 0xFF87u) == 0x4700u) { /* BX Rm */
            int rm = (h0 >> 3) & 0xF;
            if (rm == 14) {
                /* likely return */
                if (g_call_depth > 0) g_call_depth--;
            }
        }
    }
    (void)sp;
    (void)cpsr;
    (void)r9;
}

static void on_site(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    intptr_t tag = (intptr_t)user_data;
    uint32_t pc = (uint32_t)address;
    uint32_t lr = 0, sp = 0, cpsr = 0;
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    (void)size;
    if (!product_pah_enabled()) return;

    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    if (!g_er_rw && r9) g_er_rw = r9;

    if (tag == 1) { /* 2E2520 */
        uint32_t code = r0 ? peek_u32(uc, r0) : 0;
        if (pc != PC_2E2520) return;
        g_dispatch_call_serial++;
        if (code == 5u) {
            g_valid_dispatch = 1;
            sample_gate(uc, "before_2E4040_via_2E2520", pc);
            product_runtime_progress_emit("path_a_valid_dispatch", "pah", "code5->2E4040");
            printf("[PAH_DISPATCH] call=%u event_code=%u entry=0x%X index=%u target=0x2E4040 "
                   "evidence=OBSERVED\n",
                   g_dispatch_call_serial, code, r0, code >= 3u ? code - 3u : 0u);
            fflush(stdout);
        }
    } else if (tag == 2) { /* 2E4040 */
        if (pc != PC_2E4040) return;
        begin_handler(uc, pc, lr, sp, cpsr, r0, r1, r2, r3, r9);
    } else if (tag == 3) { /* 305EC2 gate */
        if (pc != PC_305EC2) return;
        if (g_handler_entered)
            sample_gate(uc, g_handler_active ? "gate_during_handler" : "gate_after_handler", pc);
        else
            sample_gate(uc, "gate_305EC2", pc);
    } else if (tag == 4) { /* 312A60 push */
        if (pc != PC_312A60) return;
        if (g_handler_active || g_handler_returned) {
            g_new_event = 1;
            g_requeue = 1;
            g_post_event_seen = 1;
            product_runtime_progress_emit("post_dispatch_event_seen", "pah", "push_312A60");
            printf("[PAH_QUEUE] op=PUSH pc=0x312A60 during=%d evidence=OBSERVED\n",
                   g_handler_active);
            fflush(stdout);
        }
    } else if (tag == 5) { /* 312C0C pop */
        if (pc != PC_312C0C) return;
        if (g_handler_active) {
            g_freed_entry = 1;
            printf("[PAH_QUEUE] op=POP_OR_FREE pc=0x312C0C evidence=OBSERVED\n");
            fflush(stdout);
        }
    }
}

static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                         void *user_data) {
    uint32_t addr = (uint32_t)address;
    uint32_t off;
    uint32_t before = 0;
    uint32_t pc = 0, lr = 0;
    PahStore *s;
    (void)type;
    (void)user_data;
    if (!product_pah_enabled() || !g_handler_active || !g_er_rw) return;
    if (addr < g_er_rw || addr >= g_er_rw + 0x2000u) return;
    off = addr - g_er_rw;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    if (size == 1) {
        before = peek_u8(uc, addr);
    } else if (size == 2) {
        uint16_t w = 0;
        (void)guest_memory_uc_peek((struct uc_struct *)uc, addr, &w, 2);
        before = w;
    } else {
        before = peek_u32(uc, addr);
    }
    if (before == (uint32_t)(value & (size == 1 ? 0xFFu : size == 2 ? 0xFFFFu : 0xFFFFFFFFu)) &&
        g_store_n > 8)
        return; /* skip unchanged after first samples */
    if (g_store_n >= STORE_CAP) return;
    /* After STORE_LOG_CAP unique-ish rows, only keep lifecycle/UI/resource-class changes. */
    if (g_store_n >= STORE_LOG_CAP) {
        const char *cls = classify_er_off(off);
        if (strcmp(cls, "lifecycle") != 0 && strcmp(cls, "UI") != 0 && strcmp(cls, "resource") != 0)
            return;
    }
    s = &g_stores[g_store_n++];
    memset(s, 0, sizeof(*s));
    s->seq = ++g_seq;
    s->handler_call_id = g_handler_call_id;
    s->store_pc = pc;
    s->lr = lr;
    s->er_off = off;
    s->old_v = before;
    s->new_v = (uint32_t)value;
    s->size = (uint32_t)size;
    snprintf(s->cls, sizeof(s->cls), "%s", classify_er_off(off));
    printf("[PAH_ER_RW_WRITE] id=%u pc=0x%X lr=0x%X off=0x%X old=0x%X new=0x%X size=%d cls=%s "
           "evidence=OBSERVED\n",
           g_handler_call_id, pc, lr, off, before, (uint32_t)value, size, s->cls);
    fflush(stdout);
}
#endif

void product_pah_arm_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_hook h = 0;
    if (!product_pah_enabled() || !uc || g_hook_ok) return;
    g_uc = uc;

    /* Exact sites only — dense CODE/MEM hooks arm on 0x2E4040 enter and disarm on return. */
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_site, (void *)(intptr_t)1,
                      (uint64_t)PC_2E2520, (uint64_t)PC_2E2520 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_site, (void *)(intptr_t)2,
                      (uint64_t)PC_2E4040, (uint64_t)PC_2E4040 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_site, (void *)(intptr_t)3,
                      (uint64_t)PC_305EC2, (uint64_t)PC_305EC2 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_site, (void *)(intptr_t)4,
                      (uint64_t)PC_312A60, (uint64_t)PC_312A60 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_site, (void *)(intptr_t)5,
                      (uint64_t)PC_312C0C, (uint64_t)PC_312C0C + 3ull);

    g_hook_ok = 1;
    if (!g_atexit_ok) {
        atexit(pah_atexit);
        g_atexit_ok = 1;
    }
    printf("[PAH_HOOKS] armed 2E2520/2E4040/305EC2/312A60/312C0C (dense on enter) er_rw=0x%X "
           "code=0x%X..0x%X evidence=OBSERVED\n",
           g_er_rw, g_code_base, g_code_end);
    fflush(stdout);
#else
    (void)uc;
#endif
}

void product_pah_on_10140_tick(void *uc, uint32_t er_rw) {
    char detail[64];
    if (!product_pah_enabled()) return;
    if (er_rw && !g_er_rw) g_er_rw = er_rw;
    if (!g_handler_returned || g_post_window_done) return;
    g_post_ticks++;
#ifdef GWY_HAVE_UNICORN
    if (uc && (g_post_ticks == 1u || (g_post_ticks % 25u) == 0u))
        sample_gate((uc_engine *)uc, "post_handler_tick", 0x10140u);
#else
    (void)uc;
#endif
    if (g_post_ticks >= POST_TICK_TARGET) {
        g_post_window_done = 1;
        snprintf(detail, sizeof(detail), "ticks=%u", g_post_ticks);
        product_runtime_progress_emit("waiting_for_first_frame", "pah", detail);
        printf("[PAH_POST_WINDOW] ticks=%u stop=POST_TICK_TARGET resource=%d disp=%d "
               "post_event=%d evidence=OBSERVED\n",
               g_post_ticks, g_resource_seen, g_disp_seen, g_post_event_seen);
        fflush(stdout);
        write_reports();
    }
}

void product_pah_note_resource_request(const char *path) {
    if (!product_pah_enabled()) return;
    /* Ignore bootstrap VFS (sdk_key / font / mrp open) until Path-A handler session. */
    if (!g_handler_entered && !g_valid_dispatch) return;
    g_resource_seen = 1;
    if (g_api_n < API_CAP) {
        PahApi *a = &g_apis[g_api_n++];
        memset(a, 0, sizeof(*a));
        a->seq = ++g_seq;
        snprintf(a->api, sizeof(a->api), "resource_open");
        a->handler_call_id = g_handler_call_id;
        a->during_handler = g_handler_active;
        a->a0 = 0;
        a->a1 = 0;
        (void)path;
    }
    product_runtime_progress_emit("resource_request_seen", "pah", path ? path : "");
    printf("[PAH_API] api=resource_open path=%s during_handler=%d evidence=OBSERVED\n",
           path ? path : "?", g_handler_active);
    fflush(stdout);
}

void product_pah_note_disp_up(void) {
    if (!product_pah_enabled()) return;
    if (!g_handler_entered && !g_valid_dispatch) return;
    g_disp_seen = 1;
    if (g_api_n < API_CAP) {
        PahApi *a = &g_apis[g_api_n++];
        memset(a, 0, sizeof(*a));
        a->seq = ++g_seq;
        snprintf(a->api, sizeof(a->api), "DispUpEx");
        a->handler_call_id = g_handler_call_id;
        a->during_handler = g_handler_active;
    }
    printf("[PAH_API] api=DispUpEx during_handler=%d evidence=OBSERVED\n", g_handler_active);
    fflush(stdout);
}

void product_pah_note_platform_api(const char *api, uint32_t a0, uint32_t a1) {
    if (!product_pah_enabled() || !api) return;
    if (!(g_handler_active || g_handler_returned)) return;
    if (g_api_n >= API_CAP) return;
    {
        PahApi *a = &g_apis[g_api_n++];
        memset(a, 0, sizeof(*a));
        a->seq = ++g_seq;
        snprintf(a->api, sizeof(a->api), "%s", api);
        a->a0 = a0;
        a->a1 = a1;
        a->handler_call_id = g_handler_call_id;
        a->during_handler = g_handler_active;
    }
    if (strstr(api, "timer") || strstr(api, "callback") || strstr(api, "10140"))
        g_callback_reg = 1;
    printf("[PAH_API] api=%s a0=0x%X a1=0x%X during=%d evidence=OBSERVED\n", api, a0, a1,
           g_handler_active);
    fflush(stdout);
}

static const char *classify_verdict(void) {
    if (g_fault) return "PATH_A_HANDLER_FAULTED";
    if (!g_handler_entered) return "PATH_A_HANDLER_NOT_ENTERED";
    if (g_resource_seen) return "PATH_A_HANDLER_TRIGGERED_RESOURCE_FLOW";
    if (g_disp_seen) return "PATH_A_HANDLER_TRIGGERED_UI_FLOW";
    if (g_seen_2dadc4 && !g_handler_returned) return "PATH_A_HANDLER_WAITING_PLATFORM_CALLBACK";
    if (g_new_event || g_requeue || g_post_event_seen)
        return "PATH_A_HANDLER_SCHEDULED_NEXT_EVENT";
    if (g_callback_reg) return "PATH_A_HANDLER_WAITING_PLATFORM_CALLBACK";
    if (g_handler_returned && g_seen_2dadc4 && !g_resource_seen && !g_disp_seen) {
        return "NEXT_PLATFORM_CONTRACT_IDENTIFIED";
    }
    if (g_handler_returned && g_seen_2f68e4 && !g_seen_2dadc4 && g_budget_hit)
        return "PATH_A_HANDLER_WAITING_PLATFORM_CALLBACK";
    if (g_handler_returned && g_seen_2e4194 && !g_seen_2dadc4 && g_call_n <= 2)
        return "PATH_A_HANDLER_IS_TERMINATION_ONLY";
    if (g_handler_returned && !g_new_event && !g_resource_seen && !g_disp_seen && !g_callback_reg &&
        !g_seen_2dadc4)
        return "PATH_A_HANDLER_COMPLETED_NO_FOLLOWUP";
    return "NEXT_PLATFORM_CONTRACT_IDENTIFIED";
}

static void write_reports(void) {
    char path[512];
    FILE *f;
    int i;
    const char *verdict = classify_verdict();

    report_path("path_a_handler_calls.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,handler_call_id,depth,kind,source_pc,target,lr,r0,r1,r2,r3,ret\n");
        for (i = 0; i < g_call_n; i++) {
            PahCall *c = &g_calls[i];
            fprintf(f, "%s,%u,%u,%u,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n",
                    product_pah_run_id(), c->seq, c->handler_call_id, c->depth, c->kind,
                    c->source_pc, c->target, c->lr, c->r0, c->r1, c->r2, c->r3, c->ret);
        }
        fclose(f);
    }

    report_path("path_a_handler_er_rw_stores.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,handler_call_id,store_pc,lr,er_off,old,new,size,class\n");
        for (i = 0; i < g_store_n; i++) {
            PahStore *s = &g_stores[i];
            fprintf(f, "%s,%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%s\n", product_pah_run_id(), s->seq,
                    s->handler_call_id, s->store_pc, s->lr, s->er_off, s->old_v, s->new_v, s->size,
                    s->cls);
        }
        fclose(f);
    }

    report_path("path_a_handler_gate_timeline.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,stage,pc,15C,15D,B71,134D,C76,UI_MODE\n");
        for (i = 0; i < g_gate_n; i++) {
            PahGate *g = &g_gates[i];
            fprintf(f, "%s,%u,%s,0x%X,%u,%u,%u,%u,%u,0x%X\n", product_pah_run_id(), g->seq,
                    g->stage, g->pc, g->v15c, g->v15d, g->b71, g->v134d, g->c76, g->ui);
        }
        fclose(f);
    }

    report_path("path_a_handler_timeline.md", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "# Path-A Handler Timeline\n\n");
        fprintf(f, "- **run_id:** %s\n", product_pah_run_id());
        fprintf(f, "- **valid_dispatch:** %d\n", g_valid_dispatch);
        fprintf(f, "- **handler_entered:** %d call_id=%u\n", g_handler_entered, g_handler_call_id);
        fprintf(f, "- **handler_returned:** %d ret=0x%X insn=%u\n", g_handler_returned, g_return_r0,
                g_insn_count);
        fprintf(f, "- **entry+0/+4/+8/+C:** 0x%X / 0x%X / 0x%X / 0x%X\n", g_entry_w0, g_entry_w4,
                g_entry_w8, g_entry_wC);
        fprintf(f, "- **calls:** %d  **er_rw_stores:** %d  **gate_samples:** %d\n", g_call_n,
                g_store_n, g_gate_n);
        fprintf(f, "- **seen:** 2E4066=%d 2F68E4=%d 2DADC4=%d 2E4194=%d\n", g_seen_2e4066,
                g_seen_2f68e4, g_seen_2dadc4, g_seen_2e4194);
        fprintf(f, "- **side_effects:** new_event=%d requeue=%d freed=%d callback=%d resource=%d "
                   "disp=%d\n",
                g_new_event, g_requeue, g_freed_entry, g_callback_reg, g_resource_seen, g_disp_seen);
        fprintf(f, "- **post_ticks:** %u\n", g_post_ticks);
        fprintf(f, "- **verdict:** `%s`\n\n", verdict);
        fprintf(f, "## Gate samples\n\n");
        fprintf(f, "| stage | 15C | 15D | B71 | 134D | C76 | UI |\n|---|---|---|---|---|---|---|\n");
        for (i = 0; i < g_gate_n; i++) {
            PahGate *g = &g_gates[i];
            fprintf(f, "| %s | %u | %u | %u | %u | %u | 0x%X |\n", g->stage, g->v15c, g->v15d,
                    g->b71, g->v134d, g->c76, g->ui);
        }
        fprintf(f, "\n## Calls (first 32)\n\n");
        for (i = 0; i < g_call_n && i < 32; i++) {
            PahCall *c = &g_calls[i];
            fprintf(f, "- d=%u %s 0x%X -> 0x%X r0=0x%X\n", c->depth, c->kind, c->source_pc,
                    c->target, c->r0);
        }
        fclose(f);
    }
}

void product_pah_finalize(void) {
    const char *verdict;
    if (g_finalized || !product_pah_enabled()) return;
    g_finalized = 1;
#ifdef GWY_HAVE_UNICORN
    if (g_handler_active && g_uc) finish_handler((uc_engine *)g_uc, "finalize");
#endif
    write_reports();
    verdict = classify_verdict();
    printf("[PAH_FINALIZE] verdict=%s entered=%d returned=%d 2DADC4=%d resource=%d disp=%d "
           "post_ticks=%u stores=%d calls=%d evidence=OBSERVED\n",
           verdict, g_handler_entered, g_handler_returned, g_seen_2dadc4, g_resource_seen,
           g_disp_seen, g_post_ticks, g_store_n, g_call_n);
    fflush(stdout);
}
