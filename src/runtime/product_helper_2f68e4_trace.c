#include "gwy_launcher/product_helper_2f68e4_trace.h"
#include "gwy_launcher/product_field_parser_trace.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/product_event_queue_consumer.h"
#include "gwy_launcher/product_runtime_progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PC_HELPER_LO 0x2F68E4u
#define PC_HELPER_HI 0x2F6952u
#define PC_2E4066 0x2E4066u
#define PC_2DADC4 0x2DADC4u
#define OFF_B54 0xB54u

#define BLOCK_CAP 512
#define EDGE_CAP 1024
#define SNAP_CAP 128
#define LOOP_CAP 16
#define EXIT_CAP 64
#define API_CAP 128
#define NESTED_CAP 32
#define EDGE_RING 48
#define SNAPSHOT_INTERVAL 5000u
#define STABLE_LOOP_ITERS 8u
#define MAX_HELPER_INSNS 5000000u

typedef struct {
    uint32_t start;
    uint32_t end;
    uint32_t hits;
    uint32_t first_step;
    uint32_t last_step;
    uint32_t helper_call_id;
} H2Block;

typedef struct {
    uint32_t src_block;
    uint32_t branch_pc;
    uint32_t tgt_block;
    uint32_t taken;
    uint32_t not_taken;
    uint32_t cpsr;
    uint32_t lhs;
    uint32_t rhs;
    char cmp_kind[12];
} H2Edge;

typedef struct {
    uint32_t step;
    uint32_t insn;
    uint32_t helper_call_id;
    uint32_t pc;
    uint32_t lr;
    uint32_t sp;
    uint32_t r0, r1, r2, r3;
    uint32_t r4, r5, r6, r7, r8, r9, r10, r11;
    uint32_t queue_count;
    uint32_t nested_outstanding;
    uint32_t platform_api_count;
    uint32_t allocation_hint;
} H2Snap;

typedef struct {
    uint32_t loop_id;
    uint32_t header;
    uint32_t blocks[8];
    uint32_t block_n;
    uint32_t iterations;
    uint32_t progressing;
    uint32_t first_step;
    uint32_t last_step;
    uint32_t first_r5;
    uint32_t last_r5;
    uint32_t first_queue;
    uint32_t last_queue;
} H2Loop;

typedef struct {
    uint32_t pc;
    uint32_t lhs;
    uint32_t rhs;
    uint32_t cpsr;
    uint32_t branch_target;
    uint32_t exit_dir;
    char note[96];
} H2ExitPred;

typedef struct {
    uint32_t seq;
    uint32_t api_id;
    uint32_t slot;
    uint32_t caller_pc;
    uint32_t lr;
    uint32_t r0, r1, r2, r3;
    uint32_t ret_r0;
    char kind[24];
    char cls[24];
    uint32_t count;
} H2Api;

typedef struct {
    uint32_t event_id;
    uint32_t parent_event_id;
    uint32_t helper_call_id;
    uint32_t queue_at_publish;
    uint32_t queue_at_consume;
    int consumed;
    int published_during_helper;
} H2Nested;

static int g_en, g_en_known, g_finalized;
static char g_run_id[80];
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_code_lo = 0x2D8DF4u;
static uint32_t g_code_hi = 0x320000u;
static uint32_t g_handler_call_id;
static uint32_t g_nested_event_serial;
static uint32_t g_platform_api_count;
static uint32_t g_allocation_hint;

static int g_helper_active;
static int g_helper_returned;
static int g_seen_2e4066, g_seen_2dadc4;
static int g_stable_loop;
static int g_sched_deadlock;
static int g_new_api_missing;
static char g_stop_reason[64];
static char g_verdict[64];

static uint32_t g_step;
static uint32_t g_insn;
static uint32_t g_cur_block;
static uint32_t g_last_pc;
static uint32_t g_last_size;
static uint32_t g_pending_branch_pc;
static uint32_t g_pending_fall;
static uint32_t g_pending_target;
static uint32_t g_pending_cpsr;
static uint32_t g_pending_lhs;
static uint32_t g_pending_rhs;
static char g_pending_cmp[12];

#ifdef GWY_HAVE_UNICORN
static uc_hook g_hook;
static int g_hook_armed;
static int g_atexit_ok;
#endif

static H2Block g_blocks[BLOCK_CAP];
static int g_block_n;
static H2Edge g_edges[EDGE_CAP];
static int g_edge_n;
static H2Snap g_snaps[SNAP_CAP];
static int g_snap_n;
static H2Loop g_loops[LOOP_CAP];
static int g_loop_n;
static H2ExitPred g_exits[EXIT_CAP];
static int g_exit_n;
static H2Api g_apis[API_CAP];
static int g_api_n;
static H2Nested g_nested[NESTED_CAP];
static int g_nested_n;

static uint32_t g_edge_ring[EDGE_RING];
static int g_edge_ring_n;
static uint32_t g_loop_iter;
static uint32_t g_loop_last_hash;
static uint32_t g_loop_same_hash;
static uint32_t g_cb_depth;
static uint32_t g_consumer_depth;

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

static int helper_pc_in_range(uint32_t pc) {
    pc &= ~1u;
    return pc >= PC_HELPER_LO && pc <= PC_HELPER_HI;
}

static H2Block *find_block(uint32_t start) {
    int i;
    start &= ~1u;
    for (i = 0; i < g_block_n; i++) {
        if (g_blocks[i].start == start) return &g_blocks[i];
    }
    return NULL;
}

static H2Block *touch_block(uint32_t start) {
    H2Block *b;
    if (g_block_n >= BLOCK_CAP) return NULL;
    b = find_block(start);
    if (b) {
        b->hits++;
        b->last_step = g_step;
        return b;
    }
    b = &g_blocks[g_block_n++];
    memset(b, 0, sizeof(*b));
    b->start = start & ~1u;
    b->end = start & ~1u;
    b->hits = 1;
    b->first_step = g_step;
    b->last_step = g_step;
    b->helper_call_id = g_handler_call_id;
    if (g_block_n <= 8 || (g_block_n % 16) == 0) {
        printf("[H2_BLOCK] start=0x%X first_step=%u call_id=%u evidence=OBSERVED\n", b->start,
               b->first_step, b->helper_call_id);
        fflush(stdout);
    }
    return b;
}

static H2Edge *find_edge(uint32_t src, uint32_t branch_pc, uint32_t tgt) {
    int i;
    for (i = 0; i < g_edge_n; i++) {
        if (g_edges[i].src_block == src && g_edges[i].branch_pc == branch_pc &&
            g_edges[i].tgt_block == tgt)
            return &g_edges[i];
    }
    return NULL;
}

static void record_edge_taken(uint32_t src, uint32_t branch_pc, uint32_t tgt, int taken) {
    H2Edge *e;
    if (g_edge_n >= EDGE_CAP) return;
    e = find_edge(src, branch_pc, tgt);
    if (!e) {
        e = &g_edges[g_edge_n++];
        memset(e, 0, sizeof(*e));
        e->src_block = src;
        e->branch_pc = branch_pc;
        e->tgt_block = tgt;
        e->cpsr = g_pending_cpsr;
        e->lhs = g_pending_lhs;
        e->rhs = g_pending_rhs;
        snprintf(e->cmp_kind, sizeof(e->cmp_kind), "%s", g_pending_cmp[0] ? g_pending_cmp : "?");
    }
    if (taken)
        e->taken++;
    else
        e->not_taken++;
}

static uint32_t peek_queue_count(void) {
    uint32_t list = 0;
    if (!g_uc || !g_er_rw) return 0;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)g_uc, g_er_rw + OFF_B54, &list) || !list)
        return 0;
    return product_eqc_peek_count(g_uc, list);
}

static uint32_t nested_outstanding(void) {
    int i, n = 0;
    for (i = 0; i < g_nested_n; i++)
        if (g_nested[i].published_during_helper && !g_nested[i].consumed) n++;
    return (uint32_t)n;
}

static uint32_t state_hash(uint32_t r5, uint32_t q) {
    return (r5 ^ (q << 1) ^ (g_platform_api_count << 2));
}

#ifdef GWY_HAVE_UNICORN
static uint32_t read_reg(uc_engine *uc, int rm) {
    uint32_t v = 0;
    if (rm >= 0 && rm <= 12)
        uc_reg_read(uc, UC_ARM_REG_R0 + rm, &v);
    else if (rm == 13)
        uc_reg_read(uc, UC_ARM_REG_SP, &v);
    else if (rm == 14)
        uc_reg_read(uc, UC_ARM_REG_LR, &v);
    return v;
}

static uint32_t thumb_insn_size(const uint8_t *b, uint32_t avail) {
    uint16_t h0;
    if (avail < 2) return 2;
    h0 = (uint16_t)(b[0] | (b[1] << 8));
    if ((h0 & 0xF800u) >= 0xE800u && avail >= 4) return 4;
    if ((h0 & 0xF800u) == 0xF000u && avail >= 4) return 4;
    return 2;
}

static int decode_cond_branch(const uint8_t *b, uint32_t sz, uint32_t pc, uint32_t *fall,
                              uint32_t *tgt, int *is_branch) {
    uint16_t h0, h1;
    *is_branch = 0;
    *fall = (pc + 2u) | 1u;
    *tgt = 0;
    if (sz < 2) return 0;
    h0 = (uint16_t)(b[0] | (b[1] << 8));
    if ((h0 & 0xF000u) == 0xD000u && ((h0 >> 8) & 0xFu) != 0xFu) {
        int8_t imm = (int8_t)(h0 & 0xFFu);
        *tgt = (pc + 4u + (uint32_t)((int32_t)imm << 1)) | 1u;
        *fall = (pc + 2u) | 1u;
        *is_branch = 1;
        return 1;
    }
    if ((h0 & 0xF800u) == 0xE000u) {
        int32_t imm = (int32_t)(h0 & 0x7FFu);
        if (imm & 0x400) imm |= ~0x7FF;
        *tgt = (pc + 4u + (uint32_t)(imm << 1)) | 1u;
        *fall = (pc + 4u) | 1u;
        *is_branch = 1;
        return 1;
    }
    if (sz >= 4) {
        h1 = (uint16_t)(b[2] | (b[3] << 8));
        if ((h0 & 0xF800u) == 0xF000u && ((h1 & 0xD000u) == 0xD000u || (h1 & 0xD000u) == 0xC000u)) {
            int32_t imm;
            int s = (h0 >> 10) & 1;
            uint32_t imm10 = h0 & 0x3FFu;
            uint32_t j1 = (h1 >> 13) & 1;
            uint32_t j2 = (h1 >> 11) & 1;
            uint32_t imm11 = h1 & 0x7FFu;
            uint32_t i1 = 1 - (j1 ^ (uint32_t)s);
            uint32_t i2 = 1 - (j2 ^ (uint32_t)s);
            imm = (int32_t)((s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1));
            if (s) imm -= (1 << 25);
            *tgt = (pc + 4u + (uint32_t)imm) | 1u;
            *fall = (pc + 4u) | 1u;
            *is_branch = 1;
            return 1;
        }
    }
    if ((h0 & 0xFF80u) == 0x4700u) {
        *is_branch = 1;
        return 1;
    }
    if ((h0 & 0xF500u) == 0xB100u) {
        int32_t imm = (int32_t)((h0 >> 3) & 0x1Fu);
        if (h0 & 0x0400u) imm |= ~0x1F;
        *tgt = (pc + 4u + (uint32_t)(imm << 1)) | 1u;
        *fall = (pc + 4u) | 1u;
        *is_branch = 1;
        return 1;
    }
    return 0;
}

static void note_cmp_tst(uc_engine *uc, const uint8_t *b, uint32_t sz) {
    uint16_t h0, h1 = 0;
    uint32_t rn, rm, imm;
    if (sz < 2) return;
    h0 = (uint16_t)(b[0] | (b[1] << 8));
    g_pending_cmp[0] = 0;
    g_pending_lhs = g_pending_rhs = 0;
    if ((h0 & 0xF800u) == 0x2800u) {
        rn = h0 & 7u;
        imm = (h0 >> 3) & 0xFFu;
        g_pending_lhs = read_reg(uc, (int)rn);
        g_pending_rhs = imm;
        snprintf(g_pending_cmp, sizeof(g_pending_cmp), "CMP");
        return;
    }
    if (sz >= 4) {
        h1 = (uint16_t)(b[2] | (b[3] << 8));
        if ((h0 & 0xFBEFu) == 0x4280u || (h0 & 0xFBFFu) == 0x4290u) {
            rn = h0 & 0xFu;
            rm = h1 & 0xFu;
            g_pending_lhs = read_reg(uc, (int)rn);
            g_pending_rhs = read_reg(uc, (int)rm);
            if ((h0 & 0x10u) == 0)
                snprintf(g_pending_cmp, sizeof(g_pending_cmp), (h0 & 0x100u) ? "TST" : "CMP");
            else
                snprintf(g_pending_cmp, sizeof(g_pending_cmp), (h0 & 0x100u) ? "TEQ" : "CMN");
        }
    }
}

static void record_exit_predicate(uint32_t branch_pc, uint32_t tgt, int taken) {
    H2ExitPred *e;
    if (g_exit_n >= EXIT_CAP) return;
    e = &g_exits[g_exit_n++];
    memset(e, 0, sizeof(*e));
    e->pc = branch_pc;
    e->lhs = g_pending_lhs;
    e->rhs = g_pending_rhs;
    e->cpsr = g_pending_cpsr;
    e->branch_target = tgt;
    e->exit_dir = taken ? 1u : 0u;
    snprintf(e->note, sizeof(e->note), "%s lhs=0x%X rhs=0x%X taken=%d",
             g_pending_cmp[0] ? g_pending_cmp : "branch", e->lhs, e->rhs, taken);
}

static void maybe_snapshot(uc_engine *uc) {
    H2Snap *s;
    uint32_t r[12], sp = 0, lr = 0, pc = 0;
    int i;
    if (!g_helper_active || g_snap_n >= SNAP_CAP) return;
    if (g_insn == 0 || (g_insn % SNAPSHOT_INTERVAL) != 0) return;
    s = &g_snaps[g_snap_n++];
    memset(s, 0, sizeof(*s));
    s->step = g_step;
    s->insn = g_insn;
    s->helper_call_id = g_handler_call_id;
    for (i = 0; i < 12; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &r[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    s->pc = pc;
    s->lr = lr;
    s->sp = sp;
    s->r0 = r[0];
    s->r1 = r[1];
    s->r2 = r[2];
    s->r3 = r[3];
    s->r4 = r[4];
    s->r5 = r[5];
    s->r6 = r[6];
    s->r7 = r[7];
    s->r8 = r[8];
    s->r9 = r[9];
    s->r10 = r[10];
    s->r11 = r[11];
    s->queue_count = peek_queue_count();
    s->nested_outstanding = nested_outstanding();
    s->platform_api_count = g_platform_api_count;
    s->allocation_hint = g_allocation_hint;
    printf("[H2_SNAP] id=%u insn=%u pc=0x%X lr=0x%X r5=0x%X q=%u nested=%u api=%u evidence=OBSERVED\n",
           g_handler_call_id, g_insn, pc, lr, s->r5, s->queue_count, s->nested_outstanding,
           s->platform_api_count);
    fflush(stdout);
}

static void detect_loop(uint32_t block_start, uint32_t r5, uint32_t q) {
    int rep, j;
    if (g_edge_ring_n >= EDGE_RING) {
        memmove(g_edge_ring, g_edge_ring + 1, (EDGE_RING - 1) * sizeof(g_edge_ring[0]));
        g_edge_ring_n--;
    }
    g_edge_ring[g_edge_ring_n++] = block_start;
    for (rep = 2; rep <= 6 && rep * 2 <= g_edge_ring_n; rep++) {
        int match = 1;
        for (j = 0; j < rep && match; j++) {
            if (g_edge_ring[g_edge_ring_n - rep + j] != g_edge_ring[g_edge_ring_n - 2 * rep + j])
                match = 0;
        }
        if (match) {
            H2Loop *lp;
            uint32_t header = g_edge_ring[g_edge_ring_n - rep];
            uint32_t h = state_hash(r5, q);
            if (h == g_loop_last_hash)
                g_loop_same_hash++;
            else {
                g_loop_same_hash = 0;
                g_loop_last_hash = h;
            }
            g_loop_iter++;
            if (g_loop_n < LOOP_CAP) {
                lp = &g_loops[g_loop_n++];
                memset(lp, 0, sizeof(*lp));
                lp->loop_id = (uint32_t)g_loop_n;
                lp->header = header;
                lp->block_n = (uint32_t)rep;
                for (j = 0; j < rep && j < 8; j++) lp->blocks[j] = g_edge_ring[g_edge_ring_n - rep + j];
                lp->first_step = g_step;
                lp->last_step = g_step;
                lp->first_r5 = r5;
                lp->last_r5 = r5;
                lp->first_queue = q;
                lp->last_queue = q;
            } else {
                lp = &g_loops[g_loop_n - 1];
                lp->iterations++;
                lp->last_step = g_step;
                lp->last_r5 = r5;
                lp->last_queue = q;
            }
            lp = &g_loops[g_loop_n ? g_loop_n - 1 : 0];
            if (g_loop_same_hash == 0) lp->progressing = 1;
            if (g_loop_same_hash >= STABLE_LOOP_ITERS && !g_stable_loop) {
                g_stable_loop = 1;
                snprintf(g_stop_reason, sizeof(g_stop_reason), "STABLE_HELPER_LOOP header=0x%X",
                         header);
                product_runtime_progress_emit("path_a_helper_stable_loop", "h2", g_stop_reason);
                printf("[H2_LOOP] stable header=0x%X iters=%u r5=0x%X q=%u evidence=OBSERVED\n",
                       header, g_loop_iter, r5, q);
                fflush(stdout);
            }
            break;
        }
    }
}

static void check_sched_deadlock(uint32_t q) {
    if (g_sched_deadlock || !g_helper_active || q == 0) return;
    if (g_cb_depth > 0 && g_consumer_depth == 0 && nested_outstanding() > 0) {
        g_sched_deadlock = 1;
        snprintf(g_stop_reason, sizeof(g_stop_reason), "NESTED_EVENT_SCHEDULING_DEADLOCK q=%u",
                 q);
        product_runtime_progress_emit("nested_event_pending", "h2", g_stop_reason);
        printf("[H2_DEADLOCK] callback_depth=%u consumer_depth=%u q=%u nested=%u evidence=OBSERVED\n",
               g_cb_depth, g_consumer_depth, q, nested_outstanding());
        fflush(stdout);
    }
}

static void h2_atexit(void) {
    if (product_h2_enabled()) product_h2_finalize();
}

static void on_helper_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t fall = 0, tgt = 0, cpsr = 0, r5 = 0, q = 0;
    uint8_t bytes[4];
    uint32_t isz;
    int is_branch = 0;
    H2Block *blk;
    (void)size;
    (void)user_data;
    if (!product_h2_enabled() || !g_helper_active) return;

    g_step++;
    g_insn++;
    if (g_insn > MAX_HELPER_INSNS) {
        snprintf(g_stop_reason, sizeof(g_stop_reason), "HELPER_INSN_CAP");
        return;
    }

    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    uc_reg_read(uc, UC_ARM_REG_R5, &r5);
    q = peek_queue_count();

    /* Returned from BL 0x2F68E4 to 0x2E4062..0x2E4066. */
    if ((pc & ~1u) >= 0x2E4062u && (pc & ~1u) <= 0x2E4066u) {
        product_h2_on_handler_leave(uc, "ret_caller");
        return;
    }

    maybe_snapshot(uc);
    check_sched_deadlock(q);

    /* Global PC ring loop detect (includes callee bodies like 0x30A0CC). */
    {
        uint32_t pc_norm = pc & ~1u;
        if (g_edge_ring_n >= EDGE_RING) {
            memmove(g_edge_ring, g_edge_ring + 1, (EDGE_RING - 1) * sizeof(g_edge_ring[0]));
            g_edge_ring_n--;
        }
        g_edge_ring[g_edge_ring_n++] = pc_norm;
        detect_loop(pc_norm, r5, q);
    }

    if (!helper_pc_in_range(pc)) return;

    if (g_pending_branch_pc) {
        int taken = (pc & ~1u) == (g_pending_target & ~1u);
        record_edge_taken(g_cur_block, g_pending_branch_pc, g_pending_target, taken);
        record_edge_taken(g_cur_block, g_pending_branch_pc, g_pending_fall, !taken);
        record_exit_predicate(g_pending_branch_pc, g_pending_target, taken);
        g_pending_branch_pc = 0;
    }

    if (g_last_pc == 0 || (pc & ~1u) != ((g_last_pc + g_last_size) & ~1u)) {
        blk = touch_block(pc);
        if (blk) g_cur_block = blk->start;
    } else {
        blk = find_block(g_cur_block);
        if (blk) {
            blk->hits++;
            blk->last_step = g_step;
            blk->end = pc & ~1u;
        }
    }

    (void)guest_memory_uc_peek((struct uc_struct *)uc, pc, bytes, 4);
    isz = thumb_insn_size(bytes, 4);
    note_cmp_tst(uc, bytes, isz);
    g_pending_cpsr = cpsr;
    if (decode_cond_branch(bytes, isz, pc, &fall, &tgt, &is_branch) && is_branch) {
        g_pending_branch_pc = pc;
        g_pending_fall = fall;
        g_pending_target = tgt;
    }

    g_last_pc = pc;
    g_last_size = isz;
}
#endif

int product_h2_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_HELPER_2F68E4_TRACE");
        g_en_known = 1;
    }
    return g_en;
}

void product_h2_reset(void) {
    g_finalized = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_handler_call_id = 0;
    g_nested_event_serial = 0;
    g_platform_api_count = 0;
    g_allocation_hint = 0;
    g_helper_active = g_helper_returned = 0;
    g_seen_2e4066 = g_seen_2dadc4 = 0;
    g_stable_loop = g_sched_deadlock = g_new_api_missing = 0;
    g_stop_reason[0] = g_verdict[0] = 0;
    g_step = g_insn = 0;
    g_cur_block = g_last_pc = g_last_size = 0;
    g_pending_branch_pc = g_pending_fall = g_pending_target = 0;
    g_pending_cpsr = g_pending_lhs = g_pending_rhs = 0;
    g_pending_cmp[0] = 0;
    g_block_n = g_edge_n = g_snap_n = g_loop_n = g_exit_n = g_api_n = g_nested_n = 0;
    g_edge_ring_n = 0;
    g_loop_iter = g_loop_same_hash = g_loop_last_hash = 0;
    g_cb_depth = g_consumer_depth = 0;
    g_en_known = 0;
    g_en = 0;
#ifdef GWY_HAVE_UNICORN
    g_hook = 0;
    g_hook_armed = 0;
#endif
}

void product_h2_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_h2_run_id(void) {
    const char *e;
    if (g_run_id[0]) return g_run_id;
    e = getenv("GWY_PRODUCT_RUN_ID");
    return (e && e[0]) ? e : "unknown";
}

void product_h2_bind_uc(void *uc) { g_uc = uc; }

void product_h2_note_er_rw(uint32_t er_rw) {
    if (er_rw) g_er_rw = er_rw;
}

void product_h2_note_module_range(uint32_t code_base, uint32_t code_size) {
    if (!code_base || !code_size) return;
    g_code_lo = code_base & ~1u;
    g_code_hi = g_code_lo + code_size - 1u;
}

void product_h2_note_handler_call_id(uint32_t id) { g_handler_call_id = id; }

int product_h2_helper_active(void) { return g_helper_active; }

static void arm_helper_hook(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_err e;
    if (!uc || g_hook_armed) return;
    e = uc_hook_add((uc_engine *)uc, &g_hook, UC_HOOK_CODE, (void *)on_helper_code, NULL,
                    (uint64_t)g_code_lo, (uint64_t)g_code_hi);
    if (e == UC_ERR_OK) {
        g_hook_armed = 1;
        if (!g_atexit_ok) {
            atexit(h2_atexit);
            g_atexit_ok = 1;
        }
        printf("[H2_HOOKS] armed 0x%X..0x%X body=0x%X..0x%X interval=%u evidence=OBSERVED\n",
               g_code_lo, g_code_hi, PC_HELPER_LO, PC_HELPER_HI, SNAPSHOT_INTERVAL);
        fflush(stdout);
    }
#else
    (void)uc;
#endif
}

static void disarm_helper_hook(void *uc) {
#ifdef GWY_HAVE_UNICORN
    if (!uc || !g_hook_armed) return;
    (void)uc_hook_del((uc_engine *)uc, g_hook);
    g_hook = 0;
    g_hook_armed = 0;
#else
    (void)uc;
#endif
}

void product_h2_on_handler_enter(void *uc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2,
                               uint32_t r3, uint32_t r9) {
    if (!product_h2_enabled()) return;
    if (g_helper_active) product_h2_on_handler_leave(uc, "reenter");
    g_helper_active = 1;
    g_helper_returned = 0;
    g_step = g_insn = 0;
    g_last_pc = g_last_size = 0;
    g_cur_block = PC_HELPER_LO;
    touch_block(PC_HELPER_LO);
    if (r9 && !g_er_rw) g_er_rw = r9;
    arm_helper_hook(uc);
    product_fp_note_helper_active(1);
    product_runtime_progress_emit("path_a_helper_running", "h2", "0x2F68E4");
    printf("[H2_ENTER] call_id=%u lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X evidence=OBSERVED\n",
           g_handler_call_id, lr, r0, r1, r2, r3, r9);
    fflush(stdout);
}

void product_h2_on_handler_leave(void *uc, const char *reason) {
    if (!g_helper_active) return;
    g_helper_active = 0;
    g_helper_returned = 1;
    product_fp_note_helper_active(0);
    disarm_helper_hook(uc);
    snprintf(g_stop_reason, sizeof(g_stop_reason), "%s", reason ? reason : "return");
    product_runtime_progress_emit("path_a_helper_returned", "h2", g_stop_reason);
    printf("[H2_LEAVE] call_id=%u insn=%u reason=%s evidence=OBSERVED\n", g_handler_call_id, g_insn,
           reason ? reason : "?");
    fflush(stdout);
    write_reports();
}

void product_h2_on_outside_pc(void *uc, uint32_t pc, const char *site) {
    if (!product_h2_enabled()) return;
    pc &= ~1u;
    if (pc == PC_2E4066 && !g_seen_2e4066) {
        g_seen_2e4066 = 1;
        product_runtime_progress_emit("lifecycle_successor_entered", "h2", "0x2E4066");
    }
    if (pc == PC_2DADC4 && !g_seen_2dadc4) {
        g_seen_2dadc4 = 1;
        product_runtime_progress_emit("lifecycle_successor_entered", "h2", "0x2DADC4");
    }
    if (g_helper_active && !helper_pc_in_range(pc) && site && strstr(site, "ret"))
        product_h2_on_handler_leave(uc, site);
    (void)uc;
}

void product_h2_note_nested_publish(uint32_t parent_event_id, uint32_t queue_count) {
    H2Nested *n;
    if (!product_h2_enabled() || g_nested_n >= NESTED_CAP) return;
    g_nested_event_serial++;
    n = &g_nested[g_nested_n++];
    memset(n, 0, sizeof(*n));
    n->event_id = g_nested_event_serial;
    n->parent_event_id = parent_event_id;
    n->helper_call_id = g_handler_call_id;
    n->queue_at_publish = queue_count;
    n->published_during_helper = g_helper_active ? 1 : 0;
    if (g_helper_active)
        product_runtime_progress_emit("nested_event_pending", "h2", "push_312A60");
    printf("[H2_NESTED] pub id=%u parent=%u q=%u during=%d evidence=OBSERVED\n", n->event_id,
           parent_event_id, queue_count, g_helper_active);
    fflush(stdout);
}

void product_h2_note_nested_consume(uint32_t event_id, uint32_t queue_count) {
    int i;
    if (!product_h2_enabled()) return;
    for (i = g_nested_n - 1; i >= 0; i--) {
        if (!g_nested[i].consumed && g_nested[i].published_during_helper) {
            g_nested[i].consumed = 1;
            g_nested[i].queue_at_consume = queue_count;
            product_runtime_progress_emit("nested_event_consumed", "h2", "pop_312C0C");
            printf("[H2_NESTED] consume id=%u q=%u evidence=OBSERVED\n", g_nested[i].event_id,
                   queue_count);
            fflush(stdout);
            return;
        }
    }
    (void)event_id;
}

void product_h2_note_platform_api(uint32_t api_id, uint32_t slot, uint32_t caller_pc, uint32_t lr,
                                  uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                                  uint32_t ret_r0, const char *kind) {
    H2Api *a;
    char cls[24];
    int i, found = 0;
    if (!product_h2_enabled() || !g_helper_active) return;
    g_platform_api_count++;
    for (i = 0; i < g_api_n; i++) {
        if (g_apis[i].api_id == api_id && g_apis[i].caller_pc == (caller_pc & ~1u)) {
            g_apis[i].count++;
            found = 1;
            break;
        }
    }
    if (!found && g_api_n < API_CAP) {
        a = &g_apis[g_api_n++];
        memset(a, 0, sizeof(*a));
        a->seq = g_platform_api_count;
        a->api_id = api_id;
        a->slot = slot;
        a->caller_pc = caller_pc & ~1u;
        a->lr = lr;
        a->r0 = r0;
        a->r1 = r1;
        a->r2 = r2;
        a->r3 = r3;
        a->ret_r0 = ret_r0;
        snprintf(a->kind, sizeof(a->kind), "%s", kind ? kind : "?");
        if (api_id == 0x10138u || api_id == 0x10132u)
            snprintf(cls, sizeof(cls), "closed");
        else if (kind && strstr(kind, "stub"))
            snprintf(cls, sizeof(cls), "stub");
        else
            snprintf(cls, sizeof(cls), "unknown");
        snprintf(a->cls, sizeof(a->cls), "%s", cls);
        a->count = 1;
        if (api_id != 0x10138u && api_id != 0x10132u) {
            g_new_api_missing = 1;
            printf("[H2_API] NEW api=0x%X slot=%u pc=0x%X ret=0x%X cls=%s evidence=OBSERVED\n",
                   api_id, slot, caller_pc, ret_r0, cls);
            fflush(stdout);
        }
    }
    if (api_id == 0x10132u) g_allocation_hint++;
    (void)lr;
    (void)r0;
    (void)r1;
    (void)r2;
    (void)r3;
}

void product_h2_note_consumer_enter(uint32_t queue_count) {
    if (!product_h2_enabled()) return;
    g_consumer_depth++;
    printf("[H2_SCHED] consumer_enter depth=%u q=%u cb=%u evidence=OBSERVED\n", g_consumer_depth,
           queue_count, g_cb_depth);
    fflush(stdout);
}

void product_h2_note_callback_depth(uint32_t depth) {
    if (!product_h2_enabled()) return;
    g_cb_depth = depth;
}

static const char *classify_verdict(void) {
    if (g_helper_returned && (g_seen_2e4066 || g_seen_2dadc4)) return "HELPER_COMPLETED";
    if (g_sched_deadlock) return "NESTED_EVENT_SCHEDULING_DEADLOCK";
    if (g_stable_loop) return "STABLE_HELPER_LOOP";
    if (g_new_api_missing) return "NEXT_PLATFORM_API_MISSING";
    if (nested_outstanding() > 0 && g_helper_active) return "HELPER_WAITING_NESTED_EVENT";
    if (g_loop_n > 0 && g_loops[g_loop_n - 1].progressing) return "HELPER_LONG_BUT_PROGRESSING";
    if (g_helper_active && g_insn > SNAPSHOT_INTERVAL * 4) return "HELPER_FINITE_STREAM_NOT_DRAINED";
    if (g_helper_active) return "HELPER_LONG_BUT_PROGRESSING";
    return "HELPER_WAITING_PLATFORM_RESULT";
}

static void write_reports(void) {
    char path[512];
    FILE *f;
    int i;
    const char *verdict = classify_verdict();

    snprintf(g_verdict, sizeof(g_verdict), "%s", verdict);

    report_path("helper_2f68e4_blocks.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,block_start,block_end,hit_count,first_step,last_step,helper_call_id\n");
        for (i = 0; i < g_block_n; i++) {
            H2Block *b = &g_blocks[i];
            fprintf(f, "%s,0x%X,0x%X,%u,%u,%u,%u\n", product_h2_run_id(), b->start, b->end, b->hits,
                    b->first_step, b->last_step, b->helper_call_id);
        }
        fclose(f);
    }

    report_path("helper_2f68e4_edges.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,source_block,branch_pc,target_block,taken,not_taken,cpsr,lhs,rhs,cmp\n");
        for (i = 0; i < g_edge_n; i++) {
            H2Edge *e = &g_edges[i];
            fprintf(f, "%s,0x%X,0x%X,0x%X,%u,%u,0x%X,0x%X,0x%X,%s\n", product_h2_run_id(),
                    e->src_block, e->branch_pc, e->tgt_block, e->taken, e->not_taken, e->cpsr,
                    e->lhs, e->rhs, e->cmp_kind);
        }
        fclose(f);
    }

    report_path("helper_2f68e4_snapshots.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,step,insn,call_id,pc,lr,sp,r0,r1,r2,r3,r4,r5,r9,q,nested,api,alloc\n");
        for (i = 0; i < g_snap_n; i++) {
            H2Snap *s = &g_snaps[i];
            fprintf(f, "%s,%u,%u,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,%u,%u\n",
                    product_h2_run_id(), s->step, s->insn, s->helper_call_id, s->pc, s->lr, s->sp,
                    s->r0, s->r1, s->r2, s->r3, s->r4, s->r5, s->r9, s->queue_count,
                    s->nested_outstanding, s->platform_api_count, s->allocation_hint);
        }
        fclose(f);
    }

    report_path("helper_2f68e4_platform_apis.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,api_id,slot,caller_pc,lr,r0,r1,r2,r3,ret_r0,kind,class,count\n");
        for (i = 0; i < g_api_n; i++) {
            H2Api *a = &g_apis[i];
            fprintf(f, "%s,%u,0x%X,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s,%s,%u\n",
                    product_h2_run_id(), a->seq, a->api_id, a->slot, a->caller_pc, a->lr, a->r0,
                    a->r1, a->r2, a->r3, a->ret_r0, a->kind, a->cls, a->count);
        }
        fclose(f);
    }

    report_path("helper_2f68e4_loops.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,loop_id,header,iterations,progressing,first_r5,last_r5,first_q,last_q\n");
        for (i = 0; i < g_loop_n; i++) {
            H2Loop *lp = &g_loops[i];
            fprintf(f, "%s,%u,0x%X,%u,%u,0x%X,0x%X,%u,%u\n", product_h2_run_id(), lp->loop_id,
                    lp->header, lp->iterations, lp->progressing, lp->first_r5, lp->last_r5,
                    lp->first_queue, lp->last_queue);
        }
        fclose(f);
    }

    report_path("helper_2f68e4_cfg_timeline.md", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        uint32_t entry = g_block_n ? g_blocks[0].start : PC_HELPER_LO;
        uint32_t loop_header = g_loop_n ? g_loops[g_loop_n - 1].header : 0;
        fprintf(f, "# Helper 0x2F68E4 CFG Timeline\n\n");
        fprintf(f, "- **run_id:** %s\n", product_h2_run_id());
        fprintf(f, "- **verdict:** `%s`\n", verdict);
        fprintf(f, "- **helper_call_id:** %u\n", g_handler_call_id);
        fprintf(f, "- **instructions:** %u\n", g_insn);
        fprintf(f, "- **blocks:** %d  **edges:** %d  **loops:** %d\n", g_block_n, g_edge_n,
                g_loop_n);
        fprintf(f, "- **stop:** %s\n", g_stop_reason[0] ? g_stop_reason : "(none)");
        fprintf(f, "- **seen:** 2E4066=%d 2DADC4=%d returned=%d\n", g_seen_2e4066, g_seen_2dadc4,
                g_helper_returned);
        fprintf(f, "\n## CFG summary\n\n");
        fprintf(f, "| role | PC |\n|---|---|\n");
        fprintf(f, "| entry block | 0x%X |\n", entry);
        fprintf(f, "| loop header | 0x%X |\n", loop_header);
        fprintf(f, "| platform APIs | %d distinct |\n", g_api_n);
        fprintf(f, "| nested outstanding | %u |\n", nested_outstanding());
        fprintf(f, "\n## Top blocks (by hits)\n\n");
        for (i = 0; i < g_block_n && i < 24; i++) {
            H2Block *b = &g_blocks[i];
            fprintf(f, "- 0x%X..0x%X hits=%u steps=%u..%u\n", b->start, b->end, b->hits,
                    b->first_step, b->last_step);
        }
        fprintf(f, "\n## Exit predicates (sample)\n\n");
        for (i = 0; i < g_exit_n && i < 16; i++)
            fprintf(f, "- 0x%X → 0x%X %s\n", g_exits[i].pc, g_exits[i].branch_target,
                    g_exits[i].note);
        fclose(f);
    }

    printf("[H2_FINALIZE] verdict=%s blocks=%d edges=%d insn=%u loops=%d apis=%d nested=%d "
           "returned=%d 2E4066=%d 2DADC4=%d evidence=OBSERVED\n",
           verdict, g_block_n, g_edge_n, g_insn, g_loop_n, g_api_n, g_nested_n, g_helper_returned,
           g_seen_2e4066, g_seen_2dadc4);
    fflush(stdout);
}

void product_h2_finalize(void) {
    if (g_finalized || !product_h2_enabled()) return;
    g_finalized = 1;
#ifdef GWY_HAVE_UNICORN
    if (g_helper_active && g_uc) product_h2_on_handler_leave(g_uc, "finalize");
#endif
    write_reports();
}
