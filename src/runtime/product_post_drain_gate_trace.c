#include "gwy_launcher/product_post_drain_gate_trace.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* Candidate natural writers + drain/gate anchors — observation only. */
#define PC_30CBBC 0x30CBBCu
#define PC_30CCF4 0x30CCF4u /* strb 15D=1 inside 30CBBC (legacy) */
#define PC_2E2520 0x2E2520u
#define PC_2DC4D8 0x2DC4D8u
#define PC_2DC572 0x2DC572u /* strb B71=1 in 2DC4D8 (legacy) */
#define PC_2DC80C 0x2DC80Cu
#define PC_312C0C 0x312C0Cu
#define PC_305EC2 0x305EC2u
#define PC_305EF4 0x305EF4u
#define PC_2DADC4 0x2DADC4u
#define PC_2E379E 0x2E379Eu /* static: event_code==3 case → B71 chain */
#define PC_2E4040 0x2E4040u /* static: Path-A stub (codes 5/12) */
#define PC_2E4194 0x2E4194u /* static: default / out-of-range */
#define OFF_15D 0x15Du
#define OFF_B71 0xB71u
#define OFF_134D 0x134Du
#define OFF_C76 0xC76u
#define OFF_B54 0xB54u

/*
 * 0x2E2520 switch head — source: robotol.ext Thumb disasm (jjfb.mrp SHA expected).
 *   0x2E2520 PUSH; save r0; ldr r0,[r0,#0] event_code; index=code-3;
 *   cmp index,#0x1B0; bhi → BL 0x2E4194; else ADR table@0x2E2544; ADD PC case.
 * Head ends at ADD PC (0x2E253C); halfword offset table begins 0x2E2544.
 */
#define PC_2E2520_ADD_PC 0x2E253Cu
#define PC_2E2520_LDR_CODE 0x2E2524u
#define PC_2E2520_SUB3 0x2E252Au
#define PC_2E2520_CMP 0x2E252Eu
#define PC_2E2520_BHI 0x2E2532u
#define PC_2E2520_HEAD_END 0x2E2540u
#define PC_2E2520_TABLE 0x2E2544u

#define WATCH_CAP 384
#define ANCHOR_CAP 48
#define DISP_CALL_CAP 16
#define DISP_BR_CAP 256
#define DISP_READ_CAP 128
#define DISP_INSN_BUDGET 512

typedef struct {
    uint32_t seq;
    uint64_t watch_step;
    char kind[28];
    char tag[48];
    uint32_t pc, lr, sp;
    uint32_t r0, r1, r2, r3, r9;
    uint32_t addr;
    uint32_t before, after;
    uint32_t size;
    uint32_t c76, off134d, b71, off15d;
    uint32_t item;
    uint32_t list;
    uint32_t cb_depth;
    uint32_t dispatch_call_id;
    uint32_t actual_store_pc;
    int after_2dc80c;
    int after_312c0c;
    int after_305ec2;
} PdgtWatch;

typedef struct {
    char name[32];
    uint32_t pc;
    uint64_t watch_step;
    uint32_t seq;
} PdgtAnchor;

typedef struct {
    uint32_t call_id;
    uint32_t epoch;
    uint32_t entry_pc, entry_lr, entry_sp, entry_cpsr;
    uint32_t r0, r1, r2, r3, r9;
    uint32_t er_rw;
    uint32_t b54_head, b54_count;
    uint32_t b71, off15d, off134d, c76;
    uint32_t event_code;
    uint32_t event_index; /* code-3 when computed */
    int event_code_valid;
    uint32_t since_pop_312c0c; /* watch_steps since last pop */
    uint32_t until_gate_305ec2; /* filled at finalize if known */
    uint32_t local_instruction_count;
    uint32_t basic_block_count;
    uint32_t dispatch_target;
    uint32_t first_blocking_pc;
    char first_blocking_pred[64];
    char first_blocking_detail[96];
    int reached_2dc4d8;
    int returned_to_lr;
    int active;
    int finished;
} DispCall;

typedef struct {
    uint32_t call_id;
    uint32_t block_start;
    uint32_t branch_pc;
    char branch_type[20];
    uint32_t cpsr;
    char condition[12];
    int taken; /* 1 taken, 0 not, -1 n/a */
    uint32_t taken_target;
    uint32_t fallthrough_target;
    uint32_t r0, r1, r2, r3, lr;
    uint32_t source_pc;
    uint32_t resolved_target;
    int target_register; /* -1 if imm */
    uint32_t target_register_value;
} DispBranch;

typedef struct {
    uint32_t call_id;
    uint32_t pc;
    int base_reg;
    uint32_t offset;
    uint32_t effective_addr;
    uint32_t value;
    char feeds[48]; /* CMP/TST/index/indirect */
} DispRead;

static int g_en, g_en_known, g_disp_en, g_disp_en_known, g_finalized, g_hook_ok;
static char g_run_id[80];
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_seq;
static uint64_t g_watch_step;
static uint32_t g_cb_depth;

static PdgtWatch g_w[WATCH_CAP];
static int g_w_n;
static PdgtAnchor g_a[ANCHOR_CAP];
static int g_a_n;

static int g_seen_2dc80c, g_seen_312c0c, g_seen_305ec2;
static int g_seen_305ef4, g_seen_2dadc4;
static int g_enter_30cbbc, g_enter_2e2520, g_enter_2dc4d8;
static int g_store_15d, g_store_b71;
static uint32_t g_store_15d_pc, g_store_b71_pc;
static uint32_t g_store_15d_call_id, g_store_b71_call_id;
static uint32_t g_last_item, g_last_list;
static uint64_t g_last_pop_step;
static uint64_t g_first_gate_after_enter_step;
static int g_atexit_ok;

/* Dedup: only exact function entries count (ignore Thumb +2 within hook span). */
static uint32_t g_last_enter_pc_2e2520;
static uint32_t g_last_enter_lr_2e2520;
static uint32_t g_last_enter_sp_2e2520;
static uint32_t g_enter_serial_2e2520;

static DispCall g_dc[DISP_CALL_CAP];
static int g_dc_n;
static DispCall *g_dc_cur;
static DispBranch g_br[DISP_BR_CAP];
static int g_br_n;
static DispRead g_rd[DISP_READ_CAP];
static int g_rd_n;
static uint32_t g_bb_cur_start;
static uint32_t g_pending_cmp_pc;
static uint32_t g_pending_cmp_a, g_pending_cmp_b;
static int g_pending_cmp_valid;
static char g_pending_cmp_kind[8];

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

static void write_reports(void); /* forward */

static void pdgt_atexit_flush(void) {
    if (product_pdgt_enabled())
        product_pdgt_finalize();
}

int product_pdgt_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_POST_DRAIN_GATE_TRACE");
        g_en_known = 1;
    }
    return g_en;
}

int product_pdgt_dispatch_trace_enabled(void) {
    if (!g_disp_en_known) {
        g_disp_en = env1("JJFB_B71_DISPATCH_TRACE");
        g_disp_en_known = 1;
    }
    return g_disp_en;
}

void product_pdgt_reset(void) {
    g_finalized = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_seq = 0;
    g_watch_step = 0;
    g_cb_depth = 0;
    g_hook_ok = 0;
    g_w_n = 0;
    g_a_n = 0;
    g_seen_2dc80c = g_seen_312c0c = g_seen_305ec2 = 0;
    g_seen_305ef4 = g_seen_2dadc4 = 0;
    g_enter_30cbbc = g_enter_2e2520 = g_enter_2dc4d8 = 0;
    g_store_15d = g_store_b71 = 0;
    g_store_15d_pc = g_store_b71_pc = 0;
    g_store_15d_call_id = g_store_b71_call_id = 0;
    g_last_item = g_last_list = 0;
    g_last_pop_step = 0;
    g_first_gate_after_enter_step = 0;
    g_en_known = 0;
    g_en = 0;
    g_disp_en_known = 0;
    g_disp_en = 0;
    g_atexit_ok = 0;
    g_last_enter_pc_2e2520 = 0;
    g_last_enter_lr_2e2520 = 0;
    g_last_enter_sp_2e2520 = 0;
    g_enter_serial_2e2520 = 0;
    g_dc_n = 0;
    g_dc_cur = NULL;
    g_br_n = 0;
    g_rd_n = 0;
    g_bb_cur_start = 0;
    g_pending_cmp_valid = 0;
    memset(g_w, 0, sizeof(g_w));
    memset(g_a, 0, sizeof(g_a));
    memset(g_dc, 0, sizeof(g_dc));
    memset(g_br, 0, sizeof(g_br));
    memset(g_rd, 0, sizeof(g_rd));
}

void product_pdgt_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_pdgt_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

void product_pdgt_bind_uc(void *uc) { g_uc = uc; }

void product_pdgt_note_er_rw(uint32_t er_rw) {
    if (er_rw) g_er_rw = er_rw;
}

void product_pdgt_note_anchor(const char *name, uint32_t pc, uint64_t watch_step) {
    PdgtAnchor *a;
    if (!product_pdgt_enabled() || !name) return;
    if (g_a_n >= ANCHOR_CAP) return;
    a = &g_a[g_a_n++];
    memset(a, 0, sizeof(*a));
    snprintf(a->name, sizeof(a->name), "%s", name);
    a->pc = pc;
    a->watch_step = watch_step;
    a->seq = ++g_seq;
    if (pc == PC_2DC80C) g_seen_2dc80c = 1;
    if (pc == PC_312C0C) g_seen_312c0c = 1;
    if (pc == PC_305EC2) g_seen_305ec2 = 1;
}

#ifdef GWY_HAVE_UNICORN
static void read_regs(uc_engine *uc, uint32_t *pc, uint32_t *lr, uint32_t *sp, uint32_t *r0,
                      uint32_t *r1, uint32_t *r2, uint32_t *r3, uint32_t *r9) {
    uc_reg_read(uc, UC_ARM_REG_PC, pc);
    uc_reg_read(uc, UC_ARM_REG_LR, lr);
    uc_reg_read(uc, UC_ARM_REG_SP, sp);
    uc_reg_read(uc, UC_ARM_REG_R0, r0);
    uc_reg_read(uc, UC_ARM_REG_R1, r1);
    uc_reg_read(uc, UC_ARM_REG_R2, r2);
    uc_reg_read(uc, UC_ARM_REG_R3, r3);
    uc_reg_read(uc, UC_ARM_REG_R9, r9);
}

static uint32_t peek_u8(uc_engine *uc, uint32_t addr) {
    uint8_t b = 0;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, addr, &b, 1);
    return b;
}

static uint32_t peek_u32(uc_engine *uc, uint32_t addr) {
    uint32_t v = 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &v);
    return v;
}

static void peek_gate(uc_engine *uc, uint32_t *c76, uint32_t *o134, uint32_t *b71, uint32_t *o15d,
                      uint32_t *list) {
    *c76 = *o134 = *b71 = *o15d = *list = 0;
    if (!g_er_rw) return;
    *c76 = peek_u8(uc, g_er_rw + OFF_C76);
    *o134 = peek_u8(uc, g_er_rw + OFF_134D);
    *b71 = peek_u8(uc, g_er_rw + OFF_B71);
    *o15d = peek_u8(uc, g_er_rw + OFF_15D);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_B54, list);
}

static uint32_t active_dispatch_call_id(void) {
    return (g_dc_cur && g_dc_cur->active) ? g_dc_cur->call_id : 0;
}

static void add_watch(uc_engine *uc, const char *kind, const char *tag, uint32_t addr,
                      uint32_t before, uint32_t after, uint32_t size, uint32_t actual_store_pc) {
    PdgtWatch *w;
    uint32_t pc = 0, lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    uint32_t c76 = 0, o134 = 0, b71 = 0, o15d = 0, list = 0;
    if (g_w_n >= WATCH_CAP) return;
    read_regs(uc, &pc, &lr, &sp, &r0, &r1, &r2, &r3, &r9);
    peek_gate(uc, &c76, &o134, &b71, &o15d, &list);
    w = &g_w[g_w_n++];
    memset(w, 0, sizeof(*w));
    w->seq = ++g_seq;
    w->watch_step = g_watch_step;
    snprintf(w->kind, sizeof(w->kind), "%s", kind ? kind : "");
    snprintf(w->tag, sizeof(w->tag), "%s", tag ? tag : "");
    w->pc = pc;
    w->lr = lr;
    w->sp = sp;
    w->r0 = r0;
    w->r1 = r1;
    w->r2 = r2;
    w->r3 = r3;
    w->r9 = r9;
    w->addr = addr;
    w->before = before;
    w->after = after;
    w->size = size;
    w->c76 = c76;
    w->off134d = o134;
    w->b71 = b71;
    w->off15d = o15d;
    w->item = g_last_item;
    w->list = list ? list : g_last_list;
    w->cb_depth = g_cb_depth;
    w->dispatch_call_id = active_dispatch_call_id();
    w->actual_store_pc = actual_store_pc;
    w->after_2dc80c = g_seen_2dc80c;
    w->after_312c0c = g_seen_312c0c;
    w->after_305ec2 = g_seen_305ec2;
}

static int cond_passed(uint32_t cpsr, int cond) {
    int n = (cpsr >> 31) & 1, z = (cpsr >> 30) & 1, c = (cpsr >> 29) & 1, v = (cpsr >> 28) & 1;
    switch (cond & 0xF) {
    case 0:
        return z;
    case 1:
        return !z;
    case 2:
        return c;
    case 3:
        return !c;
    case 4:
        return n;
    case 5:
        return !n;
    case 6:
        return v;
    case 7:
        return !v;
    case 8:
        return c && !z;
    case 9:
        return !c || z;
    case 10:
        return n == v;
    case 11:
        return n != v;
    case 12:
        return !z && n == v;
    case 13:
        return z || n != v;
    case 14:
        return 1;
    default:
        return 0;
    }
}

static const char *cond_name(int cond) {
    static const char *n[] = {"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
                              "HI", "LS", "GE", "LT", "GT", "LE", "AL", "NV"};
    return n[cond & 0xF];
}

static void add_branch(DispBranch *src) {
    if (g_br_n >= DISP_BR_CAP || !src) return;
    g_br[g_br_n++] = *src;
}

static void add_read(uint32_t call_id, uint32_t pc, int base_reg, uint32_t offset, uint32_t ea,
                     uint32_t value, const char *feeds) {
    DispRead *r;
    if (g_rd_n >= DISP_READ_CAP) return;
    r = &g_rd[g_rd_n++];
    memset(r, 0, sizeof(*r));
    r->call_id = call_id;
    r->pc = pc;
    r->base_reg = base_reg;
    r->offset = offset;
    r->effective_addr = ea;
    r->value = value;
    snprintf(r->feeds, sizeof(r->feeds), "%s", feeds ? feeds : "");
}

static void finish_dispatch_call(DispCall *c, const char *why) {
    (void)why;
    if (!c || !c->active) return;
    c->active = 0;
    c->finished = 1;
    if (g_dc_cur == c) g_dc_cur = NULL;
    g_bb_cur_start = 0;
    g_pending_cmp_valid = 0;
}

static DispCall *begin_dispatch_call(uc_engine *uc, uint32_t pc, uint32_t lr, uint32_t sp,
                                     uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                                     uint32_t r9) {
    DispCall *c;
    uint32_t cpsr = 0, c76 = 0, o134 = 0, b71 = 0, o15d = 0, list = 0;
    uint32_t count = 0;
    if (g_dc_n >= DISP_CALL_CAP) return NULL;
    if (g_dc_cur && g_dc_cur->active) finish_dispatch_call(g_dc_cur, "nested_enter");
    c = &g_dc[g_dc_n++];
    memset(c, 0, sizeof(*c));
    c->call_id = ++g_enter_serial_2e2520;
    c->epoch = (uint32_t)g_watch_step;
    c->entry_pc = pc;
    c->entry_lr = lr;
    c->entry_sp = sp;
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    c->entry_cpsr = cpsr;
    c->r0 = r0;
    c->r1 = r1;
    c->r2 = r2;
    c->r3 = r3;
    c->r9 = r9;
    c->er_rw = g_er_rw;
    peek_gate(uc, &c76, &o134, &b71, &o15d, &list);
    c->c76 = c76;
    c->off134d = o134;
    c->b71 = b71;
    c->off15d = o15d;
    c->b54_head = list;
    if (list) count = peek_u32(uc, list); /* best-effort; may be node ptr not count */
    c->b54_count = count;
    c->since_pop_312c0c =
        g_last_pop_step ? (uint32_t)(g_watch_step - g_last_pop_step) : 0xFFFFFFFFu;
    c->active = 1;
    c->first_blocking_pred[0] = 0;
    g_dc_cur = c;
    g_bb_cur_start = pc;
    return c;
}

static int is_true_2e2520_enter(uint32_t pc, uint32_t lr, uint32_t sp) {
    /* Hook span covers 0x2E2520..+3; only exact entry is a call. Dedup same frame. */
    if (pc != PC_2E2520) return 0;
    if (pc == g_last_enter_pc_2e2520 && lr == g_last_enter_lr_2e2520 && sp == g_last_enter_sp_2e2520)
        return 0;
    g_last_enter_pc_2e2520 = pc;
    g_last_enter_lr_2e2520 = lr;
    g_last_enter_sp_2e2520 = sp;
    return 1;
}

static void note_blocking(DispCall *c, uint32_t pc, const char *pred, const char *detail) {
    if (!c || c->first_blocking_pred[0]) return;
    c->first_blocking_pc = pc;
    snprintf(c->first_blocking_pred, sizeof(c->first_blocking_pred), "%s", pred ? pred : "");
    snprintf(c->first_blocking_detail, sizeof(c->first_blocking_detail), "%s", detail ? detail : "");
}

static void on_dispatch_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    DispCall *c = g_dc_cur;
    uint32_t pc = (uint32_t)address;
    uint8_t bytes[8];
    uint16_t h0, h1;
    uint32_t r[16];
    uint32_t lr = 0, sp = 0, cpsr = 0;
    int i;
    (void)user_data;
    if (!product_pdgt_dispatch_trace_enabled() || !c || !c->active) return;

    /* End when we return to the BL site LR (Thumb |1). */
    if (pc == (c->entry_lr & ~1u) || pc == c->entry_lr) {
        c->returned_to_lr = 1;
        finish_dispatch_call(c, "ret_lr");
        return;
    }

    if (c->local_instruction_count >= DISP_INSN_BUDGET) {
        finish_dispatch_call(c, "insn_budget");
        return;
    }
    c->local_instruction_count++;

    if (pc == PC_2DC4D8 || pc == (PC_2DC4D8 | 1u)) {
        c->reached_2dc4d8 = 1;
        g_enter_2dc4d8 = 1;
    }

    /* Case-entry probes outside head: confirm ADD_PC target, then end local session. */
    if (pc == PC_2E379E || pc == PC_2E4040 || pc == PC_2E4194) {
        DispBranch br;
        if (!c->dispatch_target) c->dispatch_target = pc;
        memset(&br, 0, sizeof(br));
        br.call_id = c->call_id;
        br.block_start = g_bb_cur_start ? g_bb_cur_start : PC_2E2520_ADD_PC;
        br.branch_pc = pc;
        snprintf(br.branch_type, sizeof(br.branch_type), "CASE_ENTER");
        br.taken = 1;
        br.taken_target = pc;
        br.source_pc = PC_2E2520_ADD_PC;
        br.resolved_target = pc;
        br.target_register = -1;
        add_branch(&br);
        if (pc == PC_2E379E) {
            /* B71 upstream case — keep call active only until 2DC4D8 exact hook or budget. */
        } else {
            if (pc != PC_2E379E) {
                char detail[96];
                snprintf(detail, sizeof(detail), "landed_case=0x%X event_code=%u", pc,
                         c->event_code_valid ? c->event_code : 0u);
                note_blocking(c, pc, "switch_case_not_MR_MOUSE_UP", detail);
            }
            finish_dispatch_call(c, "case_enter_non_b71");
            return;
        }
    }

    if (!g_bb_cur_start) g_bb_cur_start = pc;

    if (size > sizeof(bytes)) size = (uint32_t)sizeof(bytes);
    memset(bytes, 0, sizeof(bytes));
    (void)guest_memory_uc_peek((struct uc_struct *)uc, pc, bytes, size);
    for (i = 0; i < 15; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &r[i]);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    if (!(cpsr & (1u << 5))) return; /* expect Thumb */

    h0 = (uint16_t)(bytes[0] | (bytes[1] << 8));
    h1 = (size >= 4) ? (uint16_t)(bytes[2] | (bytes[3] << 8)) : 0;

    /* Critical read: event_code = *[r0] at 0x2E2524 (r0 still event ptr before overwrite). */
    if (pc == PC_2E2520_LDR_CODE && (h0 & 0xF800u) == 0x6800u) {
        int rn = (h0 >> 3) & 7, rd = h0 & 7;
        uint32_t imm = ((h0 >> 6) & 0x1Fu) * 4u;
        uint32_t ea = r[rn] + imm;
        uint32_t val = peek_u32(uc, ea);
        c->event_code = val;
        c->event_code_valid = 1;
        add_read(c->call_id, pc, rn, imm, ea, val, "event_code->index=code-3");
        printf("[PDGT_DISP_READ] call=%u pc=0x%X base=r%d off=0x%X ea=0x%X val=0x%X "
               "feeds=event_code evidence=OBSERVED\n",
               c->call_id, pc, rn, imm, ea, val);
        fflush(stdout);
        (void)rd;
    }

    /* index = code - 3 */
    if (pc == PC_2E2520_SUB3 && (h0 & 0xF800u) == 0x3800u) {
        int rd = (h0 >> 8) & 7;
        uint32_t imm = h0 & 0xFFu;
        if (rd == 0 && imm == 3 && c->event_code_valid) {
            c->event_index = c->event_code - 3u;
            add_read(c->call_id, pc, 0, 3, 0, c->event_index, "index=event_code-3");
        }
    }

    /* CMP index, max (r0,r3) */
    if (pc == PC_2E2520_CMP && (h0 & 0xFFC0u) == 0x4280u) {
        int rn = h0 & 7, rm = (h0 >> 3) & 7;
        g_pending_cmp_pc = pc;
        g_pending_cmp_a = r[rn];
        g_pending_cmp_b = r[rm];
        g_pending_cmp_valid = 1;
        snprintf(g_pending_cmp_kind, sizeof(g_pending_cmp_kind), "CMP");
        add_read(c->call_id, pc, rn, 0, 0, r[rn], "CMP_index_vs_max");
        printf("[PDGT_DISP_CMP] call=%u pc=0x%X %s a=0x%X b=0x%X evidence=OBSERVED\n", c->call_id,
               pc, g_pending_cmp_kind, g_pending_cmp_a, g_pending_cmp_b);
        fflush(stdout);
    }

    /* BHI out-of-range → default */
    if (pc == PC_2E2520_BHI && (h0 & 0xF000u) == 0xD000u) {
        int cond = (h0 >> 8) & 0xF;
        int32_t imm = (int8_t)(h0 & 0xFFu);
        uint32_t tgt = (pc + 4u + (uint32_t)(imm * 2)) & ~1u;
        int taken = cond_passed(cpsr, cond);
        DispBranch br;
        memset(&br, 0, sizeof(br));
        br.call_id = c->call_id;
        br.block_start = g_bb_cur_start;
        br.branch_pc = pc;
        snprintf(br.branch_type, sizeof(br.branch_type), "Bcond");
        br.cpsr = cpsr;
        snprintf(br.condition, sizeof(br.condition), "%s", cond_name(cond));
        br.taken = taken ? 1 : 0;
        br.taken_target = tgt;
        br.fallthrough_target = pc + 2u;
        br.r0 = r[0];
        br.r1 = r[1];
        br.r2 = r[2];
        br.r3 = r[3];
        br.lr = lr;
        br.source_pc = pc;
        br.resolved_target = taken ? tgt : (pc + 2u);
        br.target_register = -1;
        add_branch(&br);
        c->basic_block_count++;
        g_bb_cur_start = br.resolved_target;
        if (taken) {
            char detail[96];
            snprintf(detail, sizeof(detail), "index=0x%X max=0x%X -> default", g_pending_cmp_a,
                     g_pending_cmp_b);
            note_blocking(c, pc, "BHI_index_out_of_range", detail);
            c->dispatch_target = PC_2E4194;
        }
        g_pending_cmp_valid = 0;
    }

    /* ADD PC, Rm — table dispatch (0x2E253C: 449F add pc,r3) */
    if (pc == PC_2E2520_ADD_PC && (h0 & 0xFF87u) == 0x4487u) {
        int rm = (h0 >> 3) & 0xF;
        uint32_t off = (rm <= 14) ? r[rm] : 0;
        uint32_t tgt = ((pc + 4u) + off) & ~1u;
        DispBranch br;
        memset(&br, 0, sizeof(br));
        br.call_id = c->call_id;
        br.block_start = g_bb_cur_start;
        br.branch_pc = pc;
        snprintf(br.branch_type, sizeof(br.branch_type), "ADD_PC");
        br.cpsr = cpsr;
        snprintf(br.condition, sizeof(br.condition), "AL");
        br.taken = 1;
        br.taken_target = tgt;
        br.fallthrough_target = 0;
        br.r0 = r[0];
        br.r1 = r[1];
        br.r2 = r[2];
        br.r3 = r[3];
        br.lr = lr;
        br.source_pc = pc;
        br.resolved_target = tgt;
        br.target_register = rm;
        br.target_register_value = off;
        add_branch(&br);
        c->basic_block_count++;
        c->dispatch_target = tgt;
        g_bb_cur_start = tgt;
        printf("[PDGT_DISP_TARGET] call=%u source=0x%X resolved=0x%X reg=r%d regval=0x%X "
               "event_code=%u evidence=OBSERVED\n",
               c->call_id, pc, tgt, rm, off, c->event_code_valid ? c->event_code : 0u);
        fflush(stdout);
        if (tgt != PC_2E379E && tgt != (PC_2E379E | 1u)) {
            char detail[96];
            snprintf(detail, sizeof(detail), "event_code=%u index=%u target=0x%X (not 0x2E379E)",
                     c->event_code_valid ? c->event_code : 0u,
                     c->event_code_valid ? (c->event_code - 3u) : 0u, tgt);
            note_blocking(c, pc, "switch_case_not_MR_MOUSE_UP", detail);
            /* Head complete; case body not densely traced. */
            finish_dispatch_call(c, "add_pc_non_b71");
            return;
        }
    }

    /* BL / BLX imm */
    if (size >= 4 && (h0 & 0xF800u) == 0xF000u && (h1 & 0xC000u) == 0xC000u) {
        int32_t s = (h0 >> 10) & 1;
        int32_t imm10 = h0 & 0x3FFu;
        int32_t imm11 = h1 & 0x7FFu;
        int32_t j1 = (h1 >> 13) & 1;
        int32_t j2 = (h1 >> 11) & 1;
        int32_t i1 = !(j1 ^ s);
        int32_t i2 = !(j2 ^ s);
        int32_t imm = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1);
        int is_blx = ((h1 & 0x1000) == 0);
        uint32_t tgt;
        DispBranch br;
        if (s) imm |= ~((1 << 25) - 1);
        tgt = (uint32_t)((int32_t)pc + 4 + imm);
        if (!is_blx) tgt |= 1u;
        memset(&br, 0, sizeof(br));
        br.call_id = c->call_id;
        br.block_start = g_bb_cur_start;
        br.branch_pc = pc;
        snprintf(br.branch_type, sizeof(br.branch_type), is_blx ? "BLX_imm" : "BL");
        br.cpsr = cpsr;
        snprintf(br.condition, sizeof(br.condition), "AL");
        br.taken = 1;
        br.taken_target = tgt & ~1u;
        br.fallthrough_target = pc + 4u;
        br.r0 = r[0];
        br.r1 = r[1];
        br.r2 = r[2];
        br.r3 = r[3];
        br.lr = lr;
        br.source_pc = pc;
        br.resolved_target = tgt & ~1u;
        br.target_register = -1;
        add_branch(&br);
        c->basic_block_count++;
        g_bb_cur_start = br.fallthrough_target;
        if ((tgt & ~1u) == PC_2DC4D8) c->reached_2dc4d8 = 1;
        printf("[PDGT_DISP_CALL] call=%u source=0x%X %s target=0x%X evidence=OBSERVED\n",
               c->call_id, pc, br.branch_type, br.resolved_target);
        fflush(stdout);
    }

    /* BLX Rm / BX Rm */
    if ((h0 & 0xFF87u) == 0x4780u || (h0 & 0xFF87u) == 0x4700u) {
        int rm = (h0 >> 3) & 0xF;
        int is_blx = ((h0 & 0xFF87u) == 0x4780u);
        uint32_t tgt = (rm <= 14) ? r[rm] : 0;
        DispBranch br;
        memset(&br, 0, sizeof(br));
        br.call_id = c->call_id;
        br.block_start = g_bb_cur_start;
        br.branch_pc = pc;
        snprintf(br.branch_type, sizeof(br.branch_type), is_blx ? "BLX_reg" : "BX_reg");
        br.cpsr = cpsr;
        snprintf(br.condition, sizeof(br.condition), "AL");
        br.taken = 1;
        br.taken_target = tgt & ~1u;
        br.fallthrough_target = is_blx ? (pc + 2u) : 0;
        br.r0 = r[0];
        br.r1 = r[1];
        br.r2 = r[2];
        br.r3 = r[3];
        br.lr = lr;
        br.source_pc = pc;
        br.resolved_target = tgt & ~1u;
        br.target_register = rm;
        br.target_register_value = tgt;
        add_branch(&br);
        c->basic_block_count++;
        if ((tgt & ~1u) == PC_2DC4D8) c->reached_2dc4d8 = 1;
        printf("[PDGT_DISP_CALL] call=%u source=0x%X %s r%d=0x%X evidence=OBSERVED\n", c->call_id,
               pc, br.branch_type, rm, tgt);
        fflush(stdout);
    }

    /* Generic Bcond (outside known BHI) */
    if (pc != PC_2E2520_BHI && (h0 & 0xF000u) == 0xD000u && ((h0 >> 8) & 0xF) != 0xF) {
        int cond = (h0 >> 8) & 0xF;
        int32_t imm = (int8_t)(h0 & 0xFFu);
        uint32_t tgt = (pc + 4u + (uint32_t)(imm * 2)) & ~1u;
        int taken = cond_passed(cpsr, cond);
        DispBranch br;
        memset(&br, 0, sizeof(br));
        br.call_id = c->call_id;
        br.block_start = g_bb_cur_start;
        br.branch_pc = pc;
        snprintf(br.branch_type, sizeof(br.branch_type), "Bcond");
        br.cpsr = cpsr;
        snprintf(br.condition, sizeof(br.condition), "%s", cond_name(cond));
        br.taken = taken ? 1 : 0;
        br.taken_target = tgt;
        br.fallthrough_target = pc + 2u;
        br.r0 = r[0];
        br.r1 = r[1];
        br.r2 = r[2];
        br.r3 = r[3];
        br.lr = lr;
        br.source_pc = pc;
        br.resolved_target = taken ? tgt : (pc + 2u);
        br.target_register = -1;
        add_branch(&br);
        c->basic_block_count++;
        g_bb_cur_start = br.resolved_target;
        if (g_pending_cmp_valid) {
            /* keep pending for correlation in CSV via last cmp fields in branch cpsr */
            g_pending_cmp_valid = 0;
        }
    }

    /* Unconditional B */
    if ((h0 & 0xF800u) == 0xE000u) {
        int32_t imm = h0 & 0x7FFu;
        uint32_t tgt;
        DispBranch br;
        if (imm & 0x400) imm -= 0x800;
        tgt = (pc + 4u + (uint32_t)(imm * 2)) & ~1u;
        memset(&br, 0, sizeof(br));
        br.call_id = c->call_id;
        br.block_start = g_bb_cur_start;
        br.branch_pc = pc;
        snprintf(br.branch_type, sizeof(br.branch_type), "B");
        br.cpsr = cpsr;
        snprintf(br.condition, sizeof(br.condition), "AL");
        br.taken = 1;
        br.taken_target = tgt;
        br.source_pc = pc;
        br.resolved_target = tgt;
        br.target_register = -1;
        br.r0 = r[0];
        br.r1 = r[1];
        br.r2 = r[2];
        br.r3 = r[3];
        br.lr = lr;
        add_branch(&br);
        c->basic_block_count++;
        g_bb_cur_start = tgt;
    }
}

static void on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    intptr_t tag = (intptr_t)user_data;
    uint32_t pc = (uint32_t)address;
    uint32_t lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    (void)size;
    if (!product_pdgt_enabled()) return;
    g_watch_step++;

    if (tag == 1) { /* 30CBBC — exact entry only */
        if (pc != PC_30CBBC) return;
        g_enter_30cbbc = 1;
        add_watch(uc, "CODE_ENTER", "cand_15D_writer_30CBBC", pc, 0, 0, 0, 0);
        printf("[PDGT_ENTER] site=0x30CBBC role=cand_15D_writer evidence=OBSERVED\n");
        fflush(stdout);
        write_reports();
    } else if (tag == 2) { /* 30CCF4 */
        if (pc != PC_30CCF4) return;
        add_watch(uc, "CODE_SITE", "strb_15D_site_30CCF4", pc, 0, 0, 0, 0);
        printf("[PDGT_SITE] pc=0x30CCF4 tag=strb_15D=1_in_30CBBC evidence=OBSERVED\n");
        fflush(stdout);
    } else if (tag == 3) { /* 2E2520 */
        read_regs(uc, &pc, &lr, &sp, &r0, &r1, &r2, &r3, &r9);
        if (!is_true_2e2520_enter(pc, lr, sp)) return;
        g_enter_2e2520 = 1;
        if (product_pdgt_dispatch_trace_enabled()) {
            DispCall *c = begin_dispatch_call(uc, pc, lr, sp, r0, r1, r2, r3, r9);
            if (c) {
                add_watch(uc, "CODE_ENTER", "b71_upstream_2E2520", pc, 0, 0, 0, 0);
                /* patch dispatch_call_id on last watch */
                if (g_w_n > 0) g_w[g_w_n - 1].dispatch_call_id = c->call_id;
                printf("[PDGT_ENTER] site=0x2E2520 role=b71_upstream_dispatcher "
                       "dispatch_call_id=%u r0=0x%X r1=0x%X r2=0x%X r3=0x%X lr=0x%X sp=0x%X "
                       "evidence=OBSERVED\n",
                       c->call_id, r0, r1, r2, r3, lr, sp);
                fflush(stdout);
            }
        } else {
            add_watch(uc, "CODE_ENTER", "b71_upstream_2E2520", pc, 0, 0, 0, 0);
            printf("[PDGT_ENTER] site=0x2E2520 role=b71_upstream_dispatcher evidence=OBSERVED\n");
            fflush(stdout);
        }
        write_reports();
    } else if (tag == 4) { /* 2DC4D8 */
        if (pc != PC_2DC4D8) return;
        g_enter_2dc4d8 = 1;
        if (g_dc_cur && g_dc_cur->active) g_dc_cur->reached_2dc4d8 = 1;
        add_watch(uc, "CODE_ENTER", "b71_writer_chain_2DC4D8", pc, 0, 0, 0, 0);
        printf("[PDGT_ENTER] site=0x2DC4D8 role=b71_writer_chain dispatch_call_id=%u "
               "evidence=OBSERVED\n",
               active_dispatch_call_id());
        fflush(stdout);
        write_reports();
    } else if (tag == 5) { /* 2DC572 */
        if (pc != PC_2DC572) return;
        add_watch(uc, "CODE_SITE", "strb_B71_site_2DC572", pc, 0, 0, 0, 0);
        printf("[PDGT_SITE] pc=0x2DC572 tag=strb_B71=1_in_2DC4D8 evidence=OBSERVED\n");
        fflush(stdout);
        write_reports();
    } else if (tag == 10) { /* 2DC80C */
        if (pc != PC_2DC80C) return;
        g_seen_2dc80c = 1;
        product_pdgt_note_anchor("drain_2DC80C", PC_2DC80C, g_watch_step);
        add_watch(uc, "ANCHOR", "drain_2DC80C", pc, 0, 0, 0, 0);
    } else if (tag == 11) { /* 312C0C */
        if (pc != PC_312C0C) return;
        g_seen_312c0c = 1;
        g_last_pop_step = g_watch_step;
        uc_reg_read(uc, UC_ARM_REG_R0, &r0);
        g_last_item = r0;
        product_pdgt_note_anchor("pop_312C0C", PC_312C0C, g_watch_step);
        add_watch(uc, "ANCHOR", "pop_312C0C", pc, 0, 0, 0, 0);
    } else if (tag == 12) { /* 305EC2 */
        if (pc != PC_305EC2) return;
        g_seen_305ec2 = 1;
        if (g_enter_2e2520 && !g_first_gate_after_enter_step)
            g_first_gate_after_enter_step = g_watch_step;
        product_pdgt_note_anchor("gate_305EC2", PC_305EC2, g_watch_step);
        add_watch(uc, "ANCHOR", "gate_305EC2", pc, 0, 0, 0, 0);
        write_reports();
    } else if (tag == 13) { /* 305EF4 */
        if (pc != PC_305EF4) return;
        g_seen_305ef4 = 1;
        add_watch(uc, "ANCHOR", "gate_pass_305EF4", pc, 0, 0, 0, 0);
    } else if (tag == 14) { /* 2DADC4 */
        if (pc != PC_2DADC4) return;
        g_seen_2dadc4 = 1;
        add_watch(uc, "ANCHOR", "successor_2DADC4", pc, 0, 0, 0, 0);
    } else if (tag == 20) { /* dense dispatch window */
        on_dispatch_code(uc, address, size, user_data);
    }
}

static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                         void *user_data) {
    uint32_t addr = (uint32_t)address;
    uint32_t before = 0;
    uint8_t b = 0;
    uint32_t pc = 0, lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    (void)type;
    (void)user_data;
    if (!product_pdgt_enabled() || !g_er_rw) return;
    g_watch_step++;
    read_regs(uc, &pc, &lr, &sp, &r0, &r1, &r2, &r3, &r9);
    if (addr == g_er_rw + OFF_15D) {
        (void)guest_memory_uc_peek((struct uc_struct *)uc, addr, &b, 1);
        before = b;
        g_store_15d = 1;
        g_store_15d_pc = pc;
        g_store_15d_call_id = active_dispatch_call_id();
        add_watch(uc, "ER_RW_WRITE", "ER_RW+15D", addr, before, (uint32_t)(value & 0xFFu),
                  (uint32_t)size, pc);
        printf("[PDGT_ER_RW_WRITE] off=15D before=%u after=%u size=%d actual_store_pc=0x%X "
               "dispatch_call_id=%u lr=0x%X sp=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X "
               "evidence=OBSERVED\n",
               before, (unsigned)(value & 0xFFu), size, pc, g_store_15d_call_id, lr, sp, r0, r1, r2,
               r3, r9);
        fflush(stdout);
        write_reports();
    } else if (addr == g_er_rw + OFF_B71) {
        (void)guest_memory_uc_peek((struct uc_struct *)uc, addr, &b, 1);
        before = b;
        g_store_b71 = 1;
        g_store_b71_pc = pc;
        g_store_b71_call_id = active_dispatch_call_id();
        add_watch(uc, "ER_RW_WRITE", "ER_RW+B71", addr, before, (uint32_t)(value & 0xFFu),
                  (uint32_t)size, pc);
        printf("[PDGT_ER_RW_WRITE] off=B71 before=%u after=%u size=%d actual_store_pc=0x%X "
               "dispatch_call_id=%u lr=0x%X sp=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X "
               "evidence=OBSERVED\n",
               before, (unsigned)(value & 0xFFu), size, pc, g_store_b71_call_id, lr, sp, r0, r1, r2,
               r3, r9);
        fflush(stdout);
        write_reports();
    }
}
#endif

void product_pdgt_arm_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_hook h = 0;
    uc_err e;
    if (!product_pdgt_enabled() || !uc || g_hook_ok) return;
    g_uc = uc;
    /* Exact-entry spans: keep +3 for Unicorn Thumb fetch, filter in callback. */
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)1,
                      (uint64_t)PC_30CBBC, (uint64_t)PC_30CBBC + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)2,
                      (uint64_t)PC_30CCF4, (uint64_t)PC_30CCF4 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)3,
                      (uint64_t)PC_2E2520, (uint64_t)PC_2E2520 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)4,
                      (uint64_t)PC_2DC4D8, (uint64_t)PC_2DC4D8 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)5,
                      (uint64_t)PC_2DC572, (uint64_t)PC_2DC572 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)10,
                      (uint64_t)PC_2DC80C, (uint64_t)PC_2DC80C + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)11,
                      (uint64_t)PC_312C0C, (uint64_t)PC_312C0C + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)12,
                      (uint64_t)PC_305EC2, (uint64_t)PC_305EC2 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)13,
                      (uint64_t)PC_305EF4, (uint64_t)PC_305EF4 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)14,
                      (uint64_t)PC_2DADC4, (uint64_t)PC_2DADC4 + 3ull);

    if (product_pdgt_dispatch_trace_enabled()) {
        /*
         * Bounded dense window = switch head only (static: PUSH..ADD PC / table).
         * Source: robotol.ext disasm; head ends ~0x2E2540, table @0x2E2544.
         * Case bodies are NOT traced insn-by-insn; 2DC4D8 enter uses exact hook.
         * Active filtering also gated by g_dc_cur->active in on_dispatch_code.
         */
        (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)20,
                          (uint64_t)PC_2E2520, (uint64_t)PC_2E2520_HEAD_END);
        /* Case entry probes (exact): prove which alternate target was selected. */
        (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)20,
                          (uint64_t)PC_2E379E, (uint64_t)PC_2E379E + 3ull);
        (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)20,
                          (uint64_t)PC_2E4040, (uint64_t)PC_2E4040 + 3ull);
        (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)20,
                          (uint64_t)PC_2E4194, (uint64_t)PC_2E4194 + 3ull);
        printf("[PDGT_DISP] JJFB_B71_DISPATCH_TRACE=1 dense head 0x2E2520-0x2E2540 + "
               "case probes 2E379E/2E4040/2E4194 evidence=OBSERVED\n");
        fflush(stdout);
    }

    if (g_er_rw) {
        e = uc_hook_add((uc_engine *)uc, &h, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL,
                        (uint64_t)(g_er_rw + OFF_15D), (uint64_t)(g_er_rw + OFF_15D));
        (void)e;
        e = uc_hook_add((uc_engine *)uc, &h, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL,
                        (uint64_t)(g_er_rw + OFF_B71), (uint64_t)(g_er_rw + OFF_B71));
        (void)e;
    }
    g_hook_ok = 1;
    if (!g_atexit_ok) {
        atexit(pdgt_atexit_flush);
        g_atexit_ok = 1;
    }
    printf("[PDGT_HOOKS] armed 30CBBC/30CCF4/2E2520/2DC4D8/2DC572 + anchors + ER_RW 15D/B71 "
           "er_rw=0x%X disp_trace=%d evidence=OBSERVED\n",
           g_er_rw, product_pdgt_dispatch_trace_enabled());
    fflush(stdout);
#else
    (void)uc;
#endif
}

static const char *grade_15d(void) {
    if (!g_enter_30cbbc && !g_store_15d) return "candidate_unproven";
    if (g_store_15d && g_enter_30cbbc && g_store_15d_pc == PC_30CCF4)
        return "proven_natural_writer";
    if (g_store_15d && g_enter_30cbbc) return "candidate_store_by_exact_pc";
    if (g_enter_30cbbc && !g_store_15d) return "candidate_entered_no_store";
    if (g_store_15d) return "store_from_unrelated_pc";
    return "candidate_unproven";
}

static const char *grade_b71(void) {
    int i;
    int any_disp_to_2e379e = 0;
    int any_reached_writer = 0;
    if (!g_enter_2e2520 && !g_enter_2dc4d8 && !g_store_b71) return "candidate_not_reached";
    for (i = 0; i < g_dc_n; i++) {
        if ((g_dc[i].dispatch_target & ~1u) == PC_2E379E) any_disp_to_2e379e = 1;
        if (g_dc[i].reached_2dc4d8) any_reached_writer = 1;
    }
    if (g_store_b71 && g_enter_2dc4d8 &&
        (g_store_b71_pc == PC_2DC572 || (g_store_b71_pc >= PC_2DC4D8 && g_store_b71_pc < 0x2DC600u)) &&
        g_store_b71_call_id != 0)
        return "proven_natural_writer";
    if (g_store_b71 && g_enter_2dc4d8 && g_store_b71_pc == PC_2DC572)
        return "candidate_store_by_exact_pc";
    if (g_store_b71 && !g_enter_2dc4d8) return "store_from_unrelated_pc";
    if (g_enter_2dc4d8 || any_reached_writer) return "candidate_dispatched_no_store";
    if (g_enter_2e2520 && (any_disp_to_2e379e || g_dc_n > 0)) return "candidate_entered_no_dispatch";
    if (g_enter_2e2520) return "candidate_entered_no_dispatch";
    return "candidate_not_reached";
}

static const char *successor_status(void) {
    /* Strict: recheck + gates + 305EF4 + 2DADC4 — runner also enforces. */
    if (g_seen_305ec2 && g_store_15d && g_store_b71 && g_seen_305ef4 && g_seen_2dadc4)
        return "POST_DRAIN_SUCCESSOR_REACHED";
    return "POST_DRAIN_SUCCESSOR_BLOCKED";
}

static void write_reports(void) {
    char path[512];
    FILE *f;
    int i;
    const char *g15 = grade_15d();
    const char *gb = grade_b71();
    const char *blocker = successor_status();

    for (i = 0; i < g_dc_n; i++) {
        if (g_first_gate_after_enter_step && g_dc[i].epoch &&
            g_first_gate_after_enter_step >= g_dc[i].epoch)
            g_dc[i].until_gate_305ec2 =
                (uint32_t)(g_first_gate_after_enter_step - g_dc[i].epoch);
    }

    report_path("product_post_drain_gate_watch.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "run_id,seq,watch_step,kind,tag,pc,lr,sp,r0,r1,r2,r3,r9,addr,before,after,size,"
                "c76,134d,b71,15d,item,list,cb_depth,dispatch_call_id,actual_store_pc,"
                "after_2dc80c,after_312c0c,after_305ec2\n");
        for (i = 0; i < g_w_n; i++) {
            PdgtWatch *w = &g_w[i];
            fprintf(f,
                    "%s,%u,%llu,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,%u,"
                    "%u,%u,%u,%u,0x%X,0x%X,%u,%u,0x%X,%d,%d,%d\n",
                    product_pdgt_run_id(), w->seq, (unsigned long long)w->watch_step, w->kind,
                    w->tag, w->pc, w->lr, w->sp, w->r0, w->r1, w->r2, w->r3, w->r9, w->addr,
                    w->before, w->after, w->size, w->c76, w->off134d, w->b71, w->off15d, w->item,
                    w->list, w->cb_depth, w->dispatch_call_id, w->actual_store_pc, w->after_2dc80c,
                    w->after_312c0c, w->after_305ec2);
        }
        fclose(f);
    }

    report_path("product_b71_dispatch_calls.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "run_id,dispatch_call_id,epoch,pc,lr,sp,cpsr,r0,r1,r2,r3,r9,er_rw,b54_head,"
                "b71,15d,134d,c76,event_code,event_index,dispatch_target,reached_2dc4d8,"
                "local_instruction_count,basic_block_count,since_pop,until_gate,"
                "first_blocking_pc,first_blocking_pred,first_blocking_detail\n");
        for (i = 0; i < g_dc_n; i++) {
            DispCall *c = &g_dc[i];
            fprintf(f,
                    "%s,%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "%u,%u,%u,%u,%u,%u,0x%X,%d,%u,%u,%u,%u,0x%X,\"%s\",\"%s\"\n",
                    product_pdgt_run_id(), c->call_id, c->epoch, c->entry_pc, c->entry_lr,
                    c->entry_sp, c->entry_cpsr, c->r0, c->r1, c->r2, c->r3, c->r9, c->er_rw,
                    c->b54_head, c->b71, c->off15d, c->off134d, c->c76,
                    c->event_code_valid ? c->event_code : 0xFFFFFFFFu,
                    c->event_code_valid ? (c->event_code - 3u) : 0xFFFFFFFFu, c->dispatch_target,
                    c->reached_2dc4d8, c->local_instruction_count, c->basic_block_count,
                    c->since_pop_312c0c, c->until_gate_305ec2, c->first_blocking_pc,
                    c->first_blocking_pred, c->first_blocking_detail);
        }
        fclose(f);
    }

    report_path("product_b71_dispatch_branches.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "run_id,dispatch_call_id,block_start,branch_pc,branch_type,cpsr,condition,taken,"
                "taken_target,fallthrough_target,r0,r1,r2,r3,lr,source_pc,resolved_target,"
                "target_register,target_register_value\n");
        for (i = 0; i < g_br_n; i++) {
            DispBranch *b = &g_br[i];
            fprintf(f,
                    "%s,%u,0x%X,0x%X,%s,0x%X,%s,%d,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "%d,0x%X\n",
                    product_pdgt_run_id(), b->call_id, b->block_start, b->branch_pc, b->branch_type,
                    b->cpsr, b->condition, b->taken, b->taken_target, b->fallthrough_target, b->r0,
                    b->r1, b->r2, b->r3, b->lr, b->source_pc, b->resolved_target, b->target_register,
                    b->target_register_value);
        }
        fclose(f);
    }

    report_path("product_b71_dispatch_reads.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,dispatch_call_id,pc,base_reg,offset,effective_addr,value,feeds\n");
        for (i = 0; i < g_rd_n; i++) {
            DispRead *r = &g_rd[i];
            fprintf(f, "%s,%u,0x%X,r%d,0x%X,0x%X,0x%X,\"%s\"\n", product_pdgt_run_id(), r->call_id,
                    r->pc, r->base_reg, r->offset, r->effective_addr, r->value, r->feeds);
        }
        fclose(f);
    }

    report_path("product_post_drain_gate_timeline.md", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "# Post-Drain Gate Timeline\n\n");
        fprintf(f, "- **run_id:** %s\n", product_pdgt_run_id());
        fprintf(f, "- **er_rw:** 0x%X\n", g_er_rw);
        fprintf(f, "- **enter_30CBBC:** %d\n", g_enter_30cbbc);
        fprintf(f, "- **true_enter_2E2520:** %d\n", g_enter_serial_2e2520);
        fprintf(f, "- **enter_2DC4D8:** %d\n", g_enter_2dc4d8);
        fprintf(f, "- **store_15D:** %d actual_store_pc=0x%X\n", g_store_15d, g_store_15d_pc);
        fprintf(f, "- **store_B71:** %d actual_store_pc=0x%X\n", g_store_b71, g_store_b71_pc);
        fprintf(f, "- **15D_writer_grade:** %s\n", g15);
        fprintf(f, "- **B71_writer_grade:** %s\n", gb);
        fprintf(f, "- **successor_status:** %s\n", blocker);
        fprintf(f, "- **disp_trace:** %d calls=%d branches=%d reads=%d\n\n",
                product_pdgt_dispatch_trace_enabled(), g_dc_n, g_br_n, g_rd_n);
        fprintf(f, "## Dispatch calls\n\n");
        fprintf(f, "| id | r0 | event_code | target | block_pred |\n");
        fprintf(f, "|----|----|------------|--------|------------|\n");
        for (i = 0; i < g_dc_n; i++) {
            fprintf(f, "| %u | 0x%X | %u | 0x%X | %s |\n", g_dc[i].call_id, g_dc[i].r0,
                    g_dc[i].event_code_valid ? g_dc[i].event_code : 0u, g_dc[i].dispatch_target,
                    g_dc[i].first_blocking_pred[0] ? g_dc[i].first_blocking_pred : "-");
        }
        fprintf(f, "\n## Discipline\n\n");
        fprintf(f, "- Observe-only: no writes to 15D/B71/UI_MODE.\n");
        fprintf(f, "- Thumb +2 within hook span is not a second CODE_ENTER.\n");
        fprintf(f, "- Writer proof requires actual_store_pc on the candidate chain.\n");
        fclose(f);
    }

    report_path("product_b71_dispatch_timeline.md", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "# B71 Dispatch Predicate Timeline\n\n");
        fprintf(f, "- **run_id:** %s\n", product_pdgt_run_id());
        fprintf(f, "- **true_0x2E2520_calls:** %u\n", g_enter_serial_2e2520);
        fprintf(f, "- **B71_writer_grade:** %s\n", gb);
        fprintf(f, "- **15D_writer_grade:** %s\n\n", g15);
        for (i = 0; i < g_dc_n; i++) {
            DispCall *c = &g_dc[i];
            fprintf(f, "## call_id=%u\n\n", c->call_id);
            fprintf(f, "- entry: pc=0x%X lr=0x%X sp=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X\n",
                    c->entry_pc, c->entry_lr, c->entry_sp, c->r0, c->r1, c->r2, c->r3, c->r9);
            fprintf(f, "- gate@enter: 15D=%u B71=%u 134D=%u C76=%u\n", c->off15d, c->b71, c->off134d,
                    c->c76);
            fprintf(f, "- event_code: %s%u index=%u\n", c->event_code_valid ? "" : "unknown/",
                    c->event_code_valid ? c->event_code : 0u,
                    c->event_code_valid ? (c->event_code - 3u) : 0u);
            fprintf(f, "- dispatch_target: 0x%X reached_2DC4D8=%d\n", c->dispatch_target,
                    c->reached_2dc4d8);
            fprintf(f, "- local_instruction_count=%u basic_block_count=%u\n",
                    c->local_instruction_count, c->basic_block_count);
            if (c->first_blocking_pred[0])
                fprintf(f, "- first_blocking: pc=0x%X pred=%s detail=%s\n", c->first_blocking_pc,
                        c->first_blocking_pred, c->first_blocking_detail);
            fprintf(f, "\n");
        }
        fclose(f);
    }

    report_path("product_post_drain_gate_summary.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,metric,value\n");
        fprintf(f, "%s,enter_30CBBC,%d\n", product_pdgt_run_id(), g_enter_30cbbc);
        fprintf(f, "%s,true_enter_2E2520,%u\n", product_pdgt_run_id(), g_enter_serial_2e2520);
        fprintf(f, "%s,enter_2DC4D8,%d\n", product_pdgt_run_id(), g_enter_2dc4d8);
        fprintf(f, "%s,store_15D,%d\n", product_pdgt_run_id(), g_store_15d);
        fprintf(f, "%s,store_B71,%d\n", product_pdgt_run_id(), g_store_b71);
        fprintf(f, "%s,store_15D_pc,0x%X\n", product_pdgt_run_id(), g_store_15d_pc);
        fprintf(f, "%s,store_B71_pc,0x%X\n", product_pdgt_run_id(), g_store_b71_pc);
        fprintf(f, "%s,15D_writer_grade,%s\n", product_pdgt_run_id(), g15);
        fprintf(f, "%s,B71_writer_grade,%s\n", product_pdgt_run_id(), gb);
        fprintf(f, "%s,watch_events,%d\n", product_pdgt_run_id(), g_w_n);
        fprintf(f, "%s,dispatch_calls,%d\n", product_pdgt_run_id(), g_dc_n);
        fprintf(f, "%s,successor_status,%s\n", product_pdgt_run_id(), blocker);
        fclose(f);
    }
}

void product_pdgt_finalize(void) {
    int i;
    if (g_finalized) return;
    g_finalized = 1;
    if (!product_pdgt_enabled()) return;
    if (g_dc_cur && g_dc_cur->active) finish_dispatch_call(g_dc_cur, "finalize");
    write_reports();
    printf("[PDGT_FINALIZE] enter_30CBBC=%d true_enter_2E2520=%u enter_2DC4D8=%d store_15D=%d "
           "store_B71=%d watches=%d disp_calls=%d B71_grade=%s 15D_grade=%s run_id=%s "
           "evidence=OBSERVED\n",
           g_enter_30cbbc, g_enter_serial_2e2520, g_enter_2dc4d8, g_store_15d, g_store_b71, g_w_n,
           g_dc_n, grade_b71(), grade_15d(), product_pdgt_run_id());
    for (i = 0; i < g_dc_n; i++) {
        printf("[PDGT_DISP_SUMMARY] call=%u event_code=%u target=0x%X block=%s detail=%s "
               "evidence=OBSERVED\n",
               g_dc[i].call_id, g_dc[i].event_code_valid ? g_dc[i].event_code : 0u,
               g_dc[i].dispatch_target,
               g_dc[i].first_blocking_pred[0] ? g_dc[i].first_blocking_pred : "none",
               g_dc[i].first_blocking_detail[0] ? g_dc[i].first_blocking_detail : "-");
    }
    fflush(stdout);
}
