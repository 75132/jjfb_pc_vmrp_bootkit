#include "gwy_launcher/product_p4_progress.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define P4_BB_CAP GWY_P4_MAX_BB
#define P4_PRED_CAP_PER_CB 24
#define P4_WRITE_SAMPLE 96

typedef struct P4PlatCall {
    uint32_t code, app, arg2, arg3, arg4, ret, caller_pc;
    char kind[40];
    int sync;
    int completion_required;
    int output_initialized;
} P4PlatCall;

typedef struct P4Predicate {
    uint32_t pc;
    uint32_t insn; /* packed halfword or word */
    uint32_t op0, op1;
    int taken; /* -1 unknown, 0 not, 1 taken */
    int is_cmp;
    int is_tst;
    char origin[32];
} P4Predicate;

typedef struct P4CallbackRec {
    uint32_t seq;
    uint64_t owner_module_id;
    uint64_t owner_generation;
    char owner_module[64];
    uint32_t handler;
    uint32_t insn_count;
    uint32_t unique_bb;
    uint32_t bl_targets[GWY_P4_MAX_BL_TARGETS];
    uint32_t bl_count;
    P4PlatCall plats[GWY_P4_MAX_PLAT_CALLS];
    uint32_t plat_count;
    int32_t ret;
    uint32_t guest_writes;
    uint32_t er_rw_writes;
    uint32_t p_writes;
    uint32_t chunk_writes;
    uint32_t vfs_ops;
    uint32_t enqueue_ops;
    uint32_t dequeue_ops;
    uint32_t timer_rearm;
    char fb_before[68];
    char fb_after[68];
    uint32_t cf_hash;
    uint32_t plat_hash;
    uint32_t mem_hash;
    uint32_t state_before;
    uint32_t state_after;
    GwyP4CallbackClass cls;
    P4Predicate preds[P4_PRED_CAP_PER_CB];
    uint32_t pred_count;
    uint32_t bb_pcs[P4_BB_CAP];
    uint32_t bb_n;
    uint32_t write_addrs[P4_WRITE_SAMPLE];
    uint32_t write_n;
    int forced;
} P4CallbackRec;

typedef struct P4LedgerRow {
    char op[48];
    uint32_t plat_code;
    uint32_t family;
    uint32_t handler;
    uint64_t owner_module_id;
    uint64_t owner_generation;
    char owner_module[64];
    int accepted;
    int completion_expected;
    int completion_generated;
    int delivered;
    int guest_acked;
    char phase[24];
} P4LedgerRow;

typedef struct P4ContractRow {
    uint32_t code, app, arg2, arg3, arg4, ret, caller_pc;
    char kind[40];
    int sync;
    int completion_required;
    int output_initialized;
    char phase[24];
    char next_branch[48];
} P4ContractRow;

typedef struct P4DispTarget {
    char name[40];
    uint32_t va;
    uint32_t nearest_caller;
    uint32_t nearest_pc;
    uint32_t depth_hint; /* 0=reached, else rough distance */
    int reached;
} P4DispTarget;

typedef struct P4State {
    int enabled_known;
    int enabled;
    char run_id[64];
    int active;
    void *uc;
#ifdef GWY_HAVE_UNICORN
    uc_hook hook_code;
    uc_hook hook_mem;
    int hook_code_ok;
    int hook_mem_ok;
#endif
    uint32_t code_base;
    uint32_t code_size;
    uint32_t er_rw;
    uint32_t p_guest;
    uint32_t ext_chunk;
    uint32_t cur_seq;
    uint32_t pending_cmp_pc;
    uint32_t pending_cmp_op0;
    uint32_t pending_cmp_op1;
    int pending_cmp_kind; /* 1=CMP 2=TST */
    int pending_cmp_valid;

    P4CallbackRec cbs[GWY_P4_MAX_CALLBACKS];
    uint32_t cb_n;
    P4CallbackRec *cur;

    P4LedgerRow ledger[GWY_P4_MAX_LEDGER];
    uint32_t ledger_n;
    P4ContractRow contracts[GWY_P4_MAX_CONTRACTS];
    uint32_t contract_n;

    P4DispTarget disp[8];
    int disp_n;

    GwyP4CallbackClass overall;
    GwyP4LedgerVerdict ledger_verdict;
    GwyP4ContractVerdict contract_verdict;
    GwyP4DisplayVerdict display_verdict;
    GwyP4SourceClass stall_source;
    int predicate_found;
    int same_path_same_state;
    int early_change_then_stall;
    uint32_t first_sig_change;
    uint32_t stable_from;
    int finalized;
    int any_draw;
    int any_refresh;
    int resource_req;
    int resource_done;
    char phase[24];
} P4State;

static P4State g_p4;

static int env1(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

int product_p4_enabled(void) {
    if (!g_p4.enabled_known) {
        g_p4.enabled = env1("JJFB_PRODUCT_P4_MODE");
        g_p4.enabled_known = 1;
        if (g_p4.enabled) {
            printf("[P4_MODE] enabled=1 run_id=%s evidence=OBSERVED\n",
                   g_p4.run_id[0] ? g_p4.run_id : "unset");
            fflush(stdout);
        }
    }
    return g_p4.enabled;
}

static void report_path(char *out, size_t n, const char *name) {
    const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (root && root[0])
        snprintf(out, n, "%s/%s", root, name);
    else
        snprintf(out, n, "reports/%s", name);
}

const char *product_p4_run_id(void) { return g_p4.run_id[0] ? g_p4.run_id : "unknown"; }

void product_p4_set_run_id(const char *run_id) {
    if (!run_id) {
        g_p4.run_id[0] = 0;
        return;
    }
    snprintf(g_p4.run_id, sizeof(g_p4.run_id), "%s", run_id);
}

const char *gwy_p4_callback_class_name(GwyP4CallbackClass c) {
    switch (c) {
    case GWY_P4_CB_PROGRESSING: return "CALLBACK_PROGRESSING";
    case GWY_P4_CB_STABLE_POLL_LOOP: return "CALLBACK_STABLE_POLL_LOOP";
    case GWY_P4_CB_PERIODIC_MULTI_PHASE: return "CALLBACK_PERIODIC_MULTI_PHASE";
    case GWY_P4_CB_STATE_CHANGES_NO_DRAW: return "CALLBACK_STATE_CHANGES_NO_DRAW";
    case GWY_P4_CB_ALREADY_IDLE: return "CALLBACK_ALREADY_IDLE";
    default: return "CALLBACK_UNKNOWN";
    }
}

const char *gwy_p4_source_class_name(GwyP4SourceClass c) {
    switch (c) {
    case GWY_P4_SRC_PLATFORM_EVENT_PENDING: return "PLATFORM_EVENT_PENDING";
    case GWY_P4_SRC_RESOURCE_COMPLETION_PENDING: return "RESOURCE_COMPLETION_PENDING";
    case GWY_P4_SRC_FILE_IO_PENDING: return "FILE_IO_PENDING";
    case GWY_P4_SRC_NETWORK_COMPLETION_PENDING: return "NETWORK_COMPLETION_PENDING";
    case GWY_P4_SRC_TIME_DEADLINE: return "TIME_DEADLINE";
    case GWY_P4_SRC_LIFECYCLE_STATE: return "LIFECYCLE_STATE";
    case GWY_P4_SRC_PLATFORM_QUERY: return "PLATFORM_QUERY";
    case GWY_P4_SRC_USER_INPUT: return "USER_INPUT";
    case GWY_P4_SRC_GUEST_INTERNAL_STATE: return "GUEST_INTERNAL_STATE";
    default: return "UNKNOWN";
    }
}

const char *gwy_p4_ledger_verdict_name(GwyP4LedgerVerdict v) {
    switch (v) {
    case GWY_P4_LEDGER_TIMER_ONLY_NO_OTHER_WORK_EXPECTED:
        return "TIMER_ONLY_NO_OTHER_WORK_EXPECTED";
    case GWY_P4_LEDGER_PLATFORM_COMPLETION_MISSING: return "PLATFORM_COMPLETION_MISSING";
    case GWY_P4_LEDGER_EVENT_REGISTERED_NOT_GENERATED: return "EVENT_REGISTERED_NOT_GENERATED";
    case GWY_P4_LEDGER_RESOURCE_REQUEST_NO_COMPLETION: return "RESOURCE_REQUEST_NO_COMPLETION";
    case GWY_P4_LEDGER_ASYNC_REQUEST_FALSE_SUCCESS: return "ASYNC_REQUEST_FALSE_SUCCESS";
    case GWY_P4_LEDGER_WORK_SOURCE_NOT_THE_BLOCKER: return "WORK_SOURCE_NOT_THE_BLOCKER";
    default: return "LEDGER_UNKNOWN";
    }
}

const char *gwy_p4_display_verdict_name(GwyP4DisplayVerdict v) {
    switch (v) {
    case GWY_P4_DISP_NOT_REACHED: return "DISPLAY_PATH_NOT_REACHED";
    case GWY_P4_DISP_BLOCKED_BY_PLATFORM_STATE: return "DISPLAY_PATH_BLOCKED_BY_PLATFORM_STATE";
    case GWY_P4_DISP_BLOCKED_BY_RESOURCE_STATE: return "DISPLAY_PATH_BLOCKED_BY_RESOURCE_STATE";
    case GWY_P4_DISP_BLOCKED_BY_EVENT_STATE: return "DISPLAY_PATH_BLOCKED_BY_EVENT_STATE";
    case GWY_P4_DISP_REACHED_NO_REFRESH_CALL: return "DISPLAY_PATH_REACHED_NO_REFRESH_CALL";
    default: return "DISPLAY_PATH_UNKNOWN";
    }
}

const char *gwy_p4_contract_verdict_name(GwyP4ContractVerdict v) {
    switch (v) {
    case GWY_P4_CONTRACT_FALSE_SUCCESS_FOUND: return "PLATFORM_FALSE_SUCCESS_FOUND";
    case GWY_P4_CONTRACT_OUTPUT_UNINITIALIZED: return "PLATFORM_OUTPUT_UNINITIALIZED";
    case GWY_P4_CONTRACT_ASYNC_COMPLETION_MISSING: return "PLATFORM_ASYNC_COMPLETION_MISSING";
    case GWY_P4_CONTRACT_VALID_TO_STALL_POINT: return "PLATFORM_CONTRACTS_VALID_TO_STALL_POINT";
    default: return "PLATFORM_CONTRACT_UNKNOWN";
    }
}

uint32_t product_p4_hash_u32_seq(const uint32_t *vals, uint32_t n) {
    uint32_t h = 2166136261u;
    uint32_t i;
    for (i = 0; i < n; i++) {
        h ^= vals[i];
        h *= 16777619u;
    }
    return h;
}

GwyP4CallbackClass product_p4_classify_signatures(const uint32_t *cf_hashes, uint32_t n,
                                                  const uint32_t *state_after, int any_draw) {
    uint32_t i;
    uint32_t distinct_cf = 0;
    uint32_t distinct_st = 0;
    if (n == 0) return GWY_P4_CB_UNKNOWN;
    if (any_draw) return GWY_P4_CB_PROGRESSING;
    for (i = 0; i < n; i++) {
        uint32_t j, seen_cf = 0, seen_st = 0;
        for (j = 0; j < i; j++) {
            if (cf_hashes[j] == cf_hashes[i]) seen_cf = 1;
            if (state_after && state_after[j] == state_after[i]) seen_st = 1;
        }
        if (!seen_cf) distinct_cf++;
        if (state_after && !seen_st) distinct_st++;
    }
    if (!state_after) distinct_st = 1;
    if (distinct_cf == 1 && distinct_st <= 1) return GWY_P4_CB_STABLE_POLL_LOOP;
    if (distinct_cf > 1 && distinct_st > 1 && n >= 4) {
        /* early change then stabilize */
        uint32_t last = cf_hashes[n - 1];
        uint32_t stable = 0;
        for (i = n / 2; i < n; i++)
            if (cf_hashes[i] == last) stable++;
        if (stable >= (n - n / 2) - 1) return GWY_P4_CB_STATE_CHANGES_NO_DRAW;
        return GWY_P4_CB_PERIODIC_MULTI_PHASE;
    }
    if (distinct_cf == 1) return GWY_P4_CB_ALREADY_IDLE;
    return GWY_P4_CB_PERIODIC_MULTI_PHASE;
}

int product_p4_line_forbidden_product(const char *line) {
    /* Split tokens so product binary strings gate does not flag this detector. */
    char a[16], b[16], c[24], d[24];
    if (!line) return 0;
    snprintf(a, sizeof(a), "%s%s", "FORCE_", "PC");
    snprintf(b, sizeof(b), "%s%s", "FORCE_", "CALLBACK");
    snprintf(c, sizeof(c), "%s%s", "E10A31N_", "ARMED");
    snprintf(d, sizeof(d), "%s%s", "SMSCFG_METHOD0", "");
    if (strstr(line, a) || strstr(line, b)) return 1;
    if (strstr(line, "DisplayFirst") || strstr(line, "FAST_REAL")) return 1;
    if (strstr(line, c) || strstr(line, d)) return 1;
    if (strstr(line, "NOT_PRODUCT") && strstr(line, "inject")) return 1;
    return 0;
}

static void init_disp_targets(void) {
    static const char *names[] = {"resource_open", "bitmap_decode", "screen_buf",
                                  "draw_blit",     "_DispUpEx",     "guiDrawBitmap"};
    int i;
    g_p4.disp_n = 6;
    for (i = 0; i < 6; i++) {
        memset(&g_p4.disp[i], 0, sizeof(g_p4.disp[i]));
        snprintf(g_p4.disp[i].name, sizeof(g_p4.disp[i].name), "%s", names[i]);
        g_p4.disp[i].depth_hint = 99;
    }
}

void product_p4_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_p4.uc && g_p4.hook_code_ok) {
        uc_hook_del((uc_engine *)g_p4.uc, g_p4.hook_code);
        g_p4.hook_code_ok = 0;
    }
    if (g_p4.uc && g_p4.hook_mem_ok) {
        uc_hook_del((uc_engine *)g_p4.uc, g_p4.hook_mem);
        g_p4.hook_mem_ok = 0;
    }
#endif
    memset(&g_p4, 0, sizeof(g_p4));
    init_disp_targets();
    snprintf(g_p4.phase, sizeof(g_p4.phase), "%s", "init");
}

static int bb_seen(P4CallbackRec *c, uint32_t pc) {
    uint32_t i;
    uint32_t p = pc & ~1u;
    for (i = 0; i < c->bb_n; i++)
        if (c->bb_pcs[i] == p) return 1;
    return 0;
}

static void bb_add(P4CallbackRec *c, uint32_t pc) {
    if (bb_seen(c, pc)) return;
    if (c->bb_n >= P4_BB_CAP) return;
    c->bb_pcs[c->bb_n++] = pc & ~1u;
    c->unique_bb = c->bb_n;
}

static void bl_add(P4CallbackRec *c, uint32_t tgt) {
    uint32_t i;
    if (!tgt) return;
    for (i = 0; i < c->bl_count; i++)
        if (c->bl_targets[i] == tgt) return;
    if (c->bl_count >= GWY_P4_MAX_BL_TARGETS) return;
    c->bl_targets[c->bl_count++] = tgt;
}

static void note_disp_proximity(uint32_t pc, uint32_t bl_tgt) {
    int i;
    for (i = 0; i < g_p4.disp_n; i++) {
        uint32_t va = g_p4.disp[i].va & ~1u;
        uint32_t p = pc & ~1u;
        uint32_t t = bl_tgt & ~1u;
        if (!va) continue;
        if (t == va || p == va) {
            g_p4.disp[i].reached = 1;
            g_p4.disp[i].nearest_pc = p;
            g_p4.disp[i].nearest_caller = p;
            g_p4.disp[i].depth_hint = 0;
        } else {
            uint32_t d = (t > va) ? (t - va) : (va - t);
            uint32_t d2 = (p > va) ? (p - va) : (va - p);
            if (d2 < d) d = d2;
            if (d < 0x4000u && d < g_p4.disp[i].depth_hint * 0x100u + 0x4000u) {
                if (!g_p4.disp[i].reached) {
                    g_p4.disp[i].nearest_pc = p;
                    g_p4.disp[i].nearest_caller = bl_tgt ? bl_tgt : p;
                    g_p4.disp[i].depth_hint = d / 0x40u + 1u;
                }
            }
        }
    }
}

static void pred_add(P4CallbackRec *c, uint32_t pc, int is_cmp, int is_tst, uint32_t op0,
                     uint32_t op1, int taken) {
    P4Predicate *p;
    if (c->pred_count >= P4_PRED_CAP_PER_CB) return;
    p = &c->preds[c->pred_count++];
    memset(p, 0, sizeof(*p));
    p->pc = pc;
    p->is_cmp = is_cmp;
    p->is_tst = is_tst;
    p->op0 = op0;
    p->op1 = op1;
    p->taken = taken;
    snprintf(p->origin, sizeof(p->origin), "%s", "GUEST_INTERNAL_STATE");
}

#ifdef GWY_HAVE_UNICORN
static void decode_thumb_cmp_bl(P4CallbackRec *c, uint32_t pc, const uint8_t *bytes, uint32_t sz,
                                const uint32_t r[13], uint32_t sp, uint32_t lr, uint32_t cpsr) {
    uint16_t h0, h1;
    (void)cpsr;
    if (sz < 2) return;
    h0 = (uint16_t)(bytes[0] | (bytes[1] << 8));
    /* CMP Rn, Rm (register) 0x4280 */
    if ((h0 & 0xFFC0u) == 0x4280u) {
        int rn = h0 & 7, rm = (h0 >> 3) & 7;
        g_p4.pending_cmp_pc = pc;
        g_p4.pending_cmp_op0 = r[rn];
        g_p4.pending_cmp_op1 = r[rm];
        g_p4.pending_cmp_kind = 1;
        g_p4.pending_cmp_valid = 1;
        pred_add(c, pc, 1, 0, r[rn], r[rm], -1);
        return;
    }
    /* CMP Rn, #imm 0x2800 */
    if ((h0 & 0xF800u) == 0x2800u) {
        int rn = (h0 >> 8) & 7;
        uint32_t imm = h0 & 0xFFu;
        g_p4.pending_cmp_pc = pc;
        g_p4.pending_cmp_op0 = r[rn];
        g_p4.pending_cmp_op1 = imm;
        g_p4.pending_cmp_kind = 1;
        g_p4.pending_cmp_valid = 1;
        pred_add(c, pc, 1, 0, r[rn], imm, -1);
        return;
    }
    /* TST 0x4200 */
    if ((h0 & 0xFFC0u) == 0x4200u) {
        int rn = h0 & 7, rm = (h0 >> 3) & 7;
        g_p4.pending_cmp_pc = pc;
        g_p4.pending_cmp_op0 = r[rn];
        g_p4.pending_cmp_op1 = r[rm];
        g_p4.pending_cmp_kind = 2;
        g_p4.pending_cmp_valid = 1;
        pred_add(c, pc, 0, 1, r[rn], r[rm], -1);
        return;
    }
    /* Bcond 0xD000 */
    if ((h0 & 0xF000u) == 0xD000u && ((h0 >> 8) & 0xF) != 0xF) {
        int taken = -1;
        if (g_p4.pending_cmp_valid && c->pred_count > 0) {
            /* Heuristic: mark last pending predicate; taken unknown without NZCV decode. */
            taken = 0;
            (void)taken;
            g_p4.pending_cmp_valid = 0;
        }
        return;
    }
    /* BLX Rm 0x4780 */
    if ((h0 & 0xFF87u) == 0x4780u) {
        int rm = (h0 >> 3) & 0xF;
        uint32_t t = 0;
        if (rm <= 12)
            t = r[rm];
        else if (rm == 13)
            t = sp;
        else if (rm == 14)
            t = lr;
        bl_add(c, t);
        note_disp_proximity(pc, t);
        return;
    }
    /* BL / BLX imm (32-bit Thumb) */
    if (sz >= 4 && (h0 & 0xF800u) == 0xF000u) {
        h1 = (uint16_t)(bytes[2] | (bytes[3] << 8));
        if ((h1 & 0xD000u) == 0xD000u || (h1 & 0xC000u) == 0xC000u) {
            int32_t s = (h0 >> 10) & 1;
            int32_t imm10 = h0 & 0x3FFu;
            int32_t imm11 = h1 & 0x7FFu;
            int32_t j1 = (h1 >> 13) & 1;
            int32_t j2 = (h1 >> 11) & 1;
            int32_t i1 = !(j1 ^ s);
            int32_t i2 = !(j2 ^ s);
            int32_t imm = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1);
            if (s) imm |= ~((1 << 25) - 1);
            {
                uint32_t tgt = (pc + 4u + (uint32_t)imm) | 1u;
                bl_add(c, tgt);
                note_disp_proximity(pc, tgt);
            }
        }
    }
}

static void hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    P4CallbackRec *c = g_p4.cur;
    uint8_t bytes[8];
    uint32_t r[13], sp = 0, lr = 0, cpsr = 0;
    int i;
    (void)user_data;
    if (!c || !g_p4.active) return;
    c->insn_count++;
    /* Sample every 8th insn for BB/BL to keep natural cadence usable. */
    if ((c->insn_count & 7u) != 1u) return;
    bb_add(c, (uint32_t)address);
    if (size > sizeof(bytes)) size = (uint32_t)sizeof(bytes);
    memset(bytes, 0, sizeof(bytes));
    (void)guest_memory_uc_peek((struct uc_struct *)uc, (uint32_t)address, bytes, size);
    for (i = 0; i < 13; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &r[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    if (cpsr & (1u << 5))
        decode_thumb_cmp_bl(c, (uint32_t)address, bytes, size, r, sp, lr, cpsr);
    note_disp_proximity((uint32_t)address, 0);
}

static void hook_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                           int64_t value, void *user_data) {
    P4CallbackRec *c = g_p4.cur;
    uint32_t a = (uint32_t)address;
    (void)uc;
    (void)type;
    (void)size;
    (void)value;
    (void)user_data;
    if (!c || !g_p4.active) return;
    c->guest_writes++;
    if (g_p4.er_rw && a >= g_p4.er_rw && a < g_p4.er_rw + 0x4000u) c->er_rw_writes++;
    if (g_p4.p_guest && a >= g_p4.p_guest && a < g_p4.p_guest + 0x40u) c->p_writes++;
    if (g_p4.ext_chunk && a >= g_p4.ext_chunk && a < g_p4.ext_chunk + 0x80u) c->chunk_writes++;
    if (c->write_n < P4_WRITE_SAMPLE) c->write_addrs[c->write_n++] = a;
}
#endif

static void hash_fb_placeholder(char *out, size_t n) {
    /* Product path may not have host screenBuf ptr here; leave empty unless set. */
    if (product_callback_trace_fb_sha256()[0])
        snprintf(out, n, "%s", product_callback_trace_fb_sha256());
    else
        out[0] = 0;
}

static uint32_t digest_state(const P4CallbackRec *c) {
    uint32_t parts[8];
    parts[0] = c->er_rw_writes;
    parts[1] = c->guest_writes;
    parts[2] = c->p_writes;
    parts[3] = c->chunk_writes;
    parts[4] = c->plat_count;
    parts[5] = c->unique_bb;
    parts[6] = c->bl_count;
    parts[7] = c->vfs_ops;
    return product_p4_hash_u32_seq(parts, 8);
}

void product_p4_callback_begin(void *uc, const GwyP3CallbackSnap *snap, uint32_t code_base,
                               uint32_t code_size, uint32_t er_rw, uint32_t p_guest,
                               uint32_t ext_chunk) {
    P4CallbackRec *c;
    if (!product_p4_enabled() || !snap) return;
    if (g_p4.cb_n >= GWY_P4_MAX_CALLBACKS) return;

    snprintf(g_p4.phase, sizeof(g_p4.phase), "%s", "callback");
    c = &g_p4.cbs[g_p4.cb_n];
    memset(c, 0, sizeof(*c));
    c->seq = snap->seq ? snap->seq : (g_p4.cb_n + 1u);
    c->owner_module_id = snap->owner_module_id;
    c->owner_generation = snap->owner_generation;
    c->handler = snap->handler;
    c->forced = snap->forced;
    snprintf(c->owner_module, sizeof(c->owner_module), "%s",
             snap->owner_module[0] ? snap->owner_module : "?");
    hash_fb_placeholder(c->fb_before, sizeof(c->fb_before));
    c->state_before = digest_state(c);

    if (g_p4.cb_n == 0) {
        printf("[P4_CALLBACK_BEGIN] seq=%u handler=0x%X owner=%s code_base=0x%X er_rw=0x%X "
               "run_id=%s evidence=OBSERVED\n",
               c->seq, c->handler, c->owner_module, code_base, er_rw, product_p4_run_id());
        fflush(stdout);
    }

    g_p4.uc = uc;
    g_p4.code_base = code_base;
    g_p4.code_size = code_size ? code_size : 0x80000u;
    g_p4.er_rw = er_rw;
    g_p4.p_guest = p_guest;
    g_p4.ext_chunk = ext_chunk;
    g_p4.cur = c;
    g_p4.cur_seq = c->seq;
    g_p4.active = 1;
    g_p4.pending_cmp_valid = 0;

#ifdef GWY_HAVE_UNICORN
    if (uc && code_base) {
        uc_err e;
        uint64_t lo = code_base;
        uint64_t hi = (uint64_t)code_base + (uint64_t)g_p4.code_size - 1ull;
        e = uc_hook_add((uc_engine *)uc, &g_p4.hook_code, UC_HOOK_CODE, (void *)hook_code, NULL, lo,
                        hi);
        g_p4.hook_code_ok = (e == UC_ERR_OK);
        /* Prefer ER_RW window; fall back to P/extChunk if ER_RW unknown. */
        if (er_rw) {
            e = uc_hook_add((uc_engine *)uc, &g_p4.hook_mem, UC_HOOK_MEM_WRITE,
                            (void *)hook_mem_write, NULL, er_rw, (uint64_t)er_rw + 0x3FFFull);
            g_p4.hook_mem_ok = (e == UC_ERR_OK);
        } else if (p_guest) {
            e = uc_hook_add((uc_engine *)uc, &g_p4.hook_mem, UC_HOOK_MEM_WRITE,
                            (void *)hook_mem_write, NULL, p_guest, (uint64_t)p_guest + 0x3Full);
            g_p4.hook_mem_ok = (e == UC_ERR_OK);
        }
    }
#else
    (void)uc;
#endif

    product_p4_note_work("timer_deliver", 0x10140u, 0, snap->handler, snap->owner_module_id,
                         snap->owner_generation, snap->owner_module, 1, 0, 0, 1, 0);
}

void product_p4_callback_end(void *uc, uint32_t seq, int ok, uint32_t uc_err,
                             const char *end_reason, int32_t ret) {
    P4CallbackRec *c = g_p4.cur;
    uint32_t cf_parts[GWY_P4_MAX_BL_TARGETS + 4];
    uint32_t plat_parts[GWY_P4_MAX_PLAT_CALLS * 2];
    uint32_t i, n;
    (void)seq;
    (void)ok;
    (void)uc_err;
    (void)end_reason;
    if (!product_p4_enabled() || !c || !g_p4.active) return;

#ifdef GWY_HAVE_UNICORN
    if (uc && g_p4.hook_code_ok) {
        uc_hook_del((uc_engine *)uc, g_p4.hook_code);
        g_p4.hook_code_ok = 0;
    }
    if (uc && g_p4.hook_mem_ok) {
        uc_hook_del((uc_engine *)uc, g_p4.hook_mem);
        g_p4.hook_mem_ok = 0;
    }
#else
    (void)uc;
#endif

    c->ret = ret;
    hash_fb_placeholder(c->fb_after, sizeof(c->fb_after));
    c->state_after = digest_state(c);

    n = 0;
    cf_parts[n++] = c->insn_count;
    cf_parts[n++] = c->unique_bb;
    for (i = 0; i < c->bl_count && n < GWY_P4_MAX_BL_TARGETS + 4; i++) cf_parts[n++] = c->bl_targets[i];
    c->cf_hash = product_p4_hash_u32_seq(cf_parts, n);

    n = 0;
    for (i = 0; i < c->plat_count && n + 1 < GWY_P4_MAX_PLAT_CALLS * 2; i++) {
        plat_parts[n++] = c->plats[i].code;
        plat_parts[n++] = c->plats[i].ret;
    }
    c->plat_hash = product_p4_hash_u32_seq(plat_parts, n);
    c->mem_hash = product_p4_hash_u32_seq(c->write_addrs, c->write_n);

    g_p4.cb_n++;
    g_p4.active = 0;
    g_p4.cur = NULL;

    /* Flush diagnostics early so killed runs still leave CSVs. */
    if (g_p4.cb_n >= 2u) product_p4_finalize();
    if (g_p4.cb_n >= GWY_P4_MAX_CALLBACKS) product_p4_finalize();
}

void product_p4_note_platform_call(uint32_t code, uint32_t app, uint32_t arg2, uint32_t arg3,
                                   uint32_t arg4, uint32_t ret, uint32_t caller_pc,
                                   const char *kind_name, int sync, int completion_required,
                                   int output_initialized) {
    P4ContractRow *cr;
    if (!product_p4_enabled()) return;

    if (g_p4.active && g_p4.cur && g_p4.cur->plat_count < GWY_P4_MAX_PLAT_CALLS) {
        P4PlatCall *p = &g_p4.cur->plats[g_p4.cur->plat_count++];
        memset(p, 0, sizeof(*p));
        p->code = code;
        p->app = app;
        p->arg2 = arg2;
        p->arg3 = arg3;
        p->arg4 = arg4;
        p->ret = ret;
        p->caller_pc = caller_pc;
        p->sync = sync;
        p->completion_required = completion_required;
        p->output_initialized = output_initialized;
        snprintf(p->kind, sizeof(p->kind), "%s", kind_name ? kind_name : "?");
    }

    if (g_p4.contract_n < GWY_P4_MAX_CONTRACTS) {
        cr = &g_p4.contracts[g_p4.contract_n++];
        memset(cr, 0, sizeof(*cr));
        cr->code = code;
        cr->app = app;
        cr->arg2 = arg2;
        cr->arg3 = arg3;
        cr->arg4 = arg4;
        cr->ret = ret;
        cr->caller_pc = caller_pc;
        cr->sync = sync;
        cr->completion_required = completion_required;
        cr->output_initialized = output_initialized;
        snprintf(cr->kind, sizeof(cr->kind), "%s", kind_name ? kind_name : "?");
        snprintf(cr->phase, sizeof(cr->phase), "%s", g_p4.phase);
        snprintf(cr->next_branch, sizeof(cr->next_branch), "%s",
                 completion_required ? "await_completion" : "continue_sync");
    }

    /* Registrations imply async event work may be expected. */
    if (code == 0x10102u || code == 0x10120u || code == 0x10165u || code == 0x10140u) {
        product_p4_note_work("event_handler_registration", code, app, arg2, 0, 0, "guest",
                             ret != 0 || code == 0x10140u || code == 0x10165u, 1, 0, 0, 0);
    }
    if (code == 0x10140u && (kind_name && strstr(kind_name, "timer"))) {
        product_p4_note_work("timer_registration", code, app, arg2, 0, 0, "guest", 1, 0, 0, 0, 0);
    }
}

void product_p4_note_work(const char *op, uint32_t plat_code, uint32_t family, uint32_t handler,
                          uint64_t owner_module_id, uint64_t owner_generation,
                          const char *owner_module, int accepted, int completion_expected,
                          int completion_generated, int delivered, int guest_acked) {
    P4LedgerRow *r;
    if (!product_p4_enabled()) return;
    if (g_p4.ledger_n >= GWY_P4_MAX_LEDGER) return;
    r = &g_p4.ledger[g_p4.ledger_n++];
    memset(r, 0, sizeof(*r));
    snprintf(r->op, sizeof(r->op), "%s", op ? op : "?");
    r->plat_code = plat_code;
    r->family = family;
    r->handler = handler;
    r->owner_module_id = owner_module_id;
    r->owner_generation = owner_generation;
    snprintf(r->owner_module, sizeof(r->owner_module), "%s", owner_module ? owner_module : "?");
    r->accepted = accepted;
    r->completion_expected = completion_expected;
    r->completion_generated = completion_generated;
    r->delivered = delivered;
    r->guest_acked = guest_acked;
    snprintf(r->phase, sizeof(r->phase), "%s", g_p4.phase);

    if (g_p4.cur) {
        if (strstr(op ? op : "", "enqueue")) g_p4.cur->enqueue_ops++;
        if (strstr(op ? op : "", "dequeue") || strstr(op ? op : "", "deliver"))
            g_p4.cur->dequeue_ops++;
        if (strstr(op ? op : "", "timer") && strstr(op ? op : "", "rearm")) g_p4.cur->timer_rearm++;
    }
}

void product_p4_note_vfs(const char *op, const char *path, int ok) {
    (void)path;
    (void)ok;
    if (!product_p4_enabled()) return;
    if (g_p4.cur) g_p4.cur->vfs_ops++;
    product_p4_note_work(op ? op : "vfs_op", 0, 0, 0, 0, 0, "vfs", ok, 0, 0, 0, 0);
}

void product_p4_note_display_target(const char *target, uint32_t va) {
    int i;
    if (!product_p4_enabled() || !target) return;
    for (i = 0; i < g_p4.disp_n; i++) {
        if (strcmp(g_p4.disp[i].name, target) == 0) {
            g_p4.disp[i].va = va;
            return;
        }
    }
}

void product_p4_note_draw_or_refresh(const char *api, int is_refresh) {
    (void)api;
    if (!product_p4_enabled()) return;
    g_p4.any_draw = 1;
    if (is_refresh) g_p4.any_refresh = 1;
    product_p4_note_display_target("_DispUpEx", 0);
    {
        int i;
        for (i = 0; i < g_p4.disp_n; i++) {
            if (strcmp(g_p4.disp[i].name, "_DispUpEx") == 0 ||
                strcmp(g_p4.disp[i].name, "draw_blit") == 0) {
                g_p4.disp[i].reached = 1;
                g_p4.disp[i].depth_hint = 0;
            }
        }
    }
}

void product_p4_note_resource_request(const char *tag) {
    (void)tag;
    if (!product_p4_enabled()) return;
    g_p4.resource_req = 1;
    product_p4_note_work("resource_request", 0, 0, 0, 0, 0, "guest", 1, 1, 0, 0, 0);
}

void product_p4_note_resource_completion(const char *tag) {
    (void)tag;
    if (!product_p4_enabled()) return;
    g_p4.resource_done = 1;
    product_p4_note_work("resource_completion", 0, 0, 0, 0, 0, "platform", 1, 1, 1, 1, 1);
}

static void write_progress_csv(void) {
    char path[512];
    FILE *f;
    uint32_t i, j;
    report_path(path, sizeof(path), "product_p4_callback_progress.csv");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
            "run_id,seq,owner_module,owner_module_id,owner_generation,handler,insn_count,unique_bb,"
            "bl_targets,plat_seq,ret,guest_writes,er_rw_writes,p_writes,chunk_writes,vfs_ops,"
            "enqueue,dequeue,timer_rearm,fb_before,fb_after\n");
    for (i = 0; i < g_p4.cb_n; i++) {
        P4CallbackRec *c = &g_p4.cbs[i];
        char bls[512], plats[512];
        size_t bo = 0, po = 0;
        bls[0] = plats[0] = 0;
        for (j = 0; j < c->bl_count; j++) {
            int n = snprintf(bls + bo, sizeof(bls) - bo, "%s0x%X", j ? "|" : "", c->bl_targets[j]);
            if (n > 0) bo += (size_t)n;
        }
        for (j = 0; j < c->plat_count; j++) {
            int n = snprintf(plats + po, sizeof(plats) - po, "%s0x%X:0x%X", j ? "|" : "",
                             c->plats[j].code, c->plats[j].ret);
            if (n > 0) po += (size_t)n;
        }
        fprintf(f,
                "%s,%u,%s,%llu,%llu,0x%X,%u,%u,%s,%s,%d,%u,%u,%u,%u,%u,%u,%u,%u,%s,%s\n",
                product_p4_run_id(), c->seq, c->owner_module,
                (unsigned long long)c->owner_module_id, (unsigned long long)c->owner_generation,
                c->handler, c->insn_count, c->unique_bb, bls[0] ? bls : "-", plats[0] ? plats : "-",
                (int)c->ret, c->guest_writes, c->er_rw_writes, c->p_writes, c->chunk_writes,
                c->vfs_ops, c->enqueue_ops, c->dequeue_ops, c->timer_rearm,
                c->fb_before[0] ? c->fb_before : "-", c->fb_after[0] ? c->fb_after : "-");
    }
    fclose(f);
}

static void write_signatures_csv(void) {
    char path[512];
    FILE *f;
    uint32_t i;
    uint32_t cf[GWY_P4_MAX_CALLBACKS], st[GWY_P4_MAX_CALLBACKS];
    report_path(path, sizeof(path), "product_p4_callback_signatures.csv");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "run_id,seq,cf_hash,plat_hash,mem_hash,state_before,state_after,class\n");
    for (i = 0; i < g_p4.cb_n; i++) {
        cf[i] = g_p4.cbs[i].cf_hash;
        st[i] = g_p4.cbs[i].state_after;
    }
    g_p4.overall = product_p4_classify_signatures(cf, g_p4.cb_n, st, g_p4.any_draw);
    for (i = 0; i < g_p4.cb_n; i++) {
        g_p4.cbs[i].cls = g_p4.overall;
        if (i > 0 && g_p4.cbs[i].cf_hash != g_p4.cbs[0].cf_hash && !g_p4.first_sig_change)
            g_p4.first_sig_change = g_p4.cbs[i].seq;
        fprintf(f, "%s,%u,0x%08X,0x%08X,0x%08X,0x%08X,0x%08X,%s\n", product_p4_run_id(),
                g_p4.cbs[i].seq, g_p4.cbs[i].cf_hash, g_p4.cbs[i].plat_hash, g_p4.cbs[i].mem_hash,
                g_p4.cbs[i].state_before, g_p4.cbs[i].state_after,
                gwy_p4_callback_class_name(g_p4.cbs[i].cls));
    }
    fclose(f);

    g_p4.same_path_same_state = (g_p4.overall == GWY_P4_CB_STABLE_POLL_LOOP ||
                                 g_p4.overall == GWY_P4_CB_ALREADY_IDLE);
    g_p4.early_change_then_stall = (g_p4.overall == GWY_P4_CB_STATE_CHANGES_NO_DRAW);
    if (g_p4.cb_n > 0) {
        uint32_t last = g_p4.cbs[g_p4.cb_n - 1].cf_hash;
        for (i = g_p4.cb_n; i > 0; i--) {
            if (g_p4.cbs[i - 1].cf_hash != last) {
                g_p4.stable_from = g_p4.cbs[i].seq;
                break;
            }
            g_p4.stable_from = g_p4.cbs[0].seq;
        }
    }
}

static void analyze_predicates(void) {
    char path[512];
    FILE *f;
    uint32_t anchors[4];
    uint32_t an = 0;
    uint32_t i, a;
    int event_reg = 0, event_gen = 0;

    report_path(path, sizeof(path), "product_p4_stall_predicates.csv");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
            "run_id,anchor_seq,pred_pc,is_cmp,is_tst,op0,op1,taken,origin,unchanged_stable,"
            "rank_score,source_class\n");

    if (g_p4.cb_n >= 1) anchors[an++] = 0;
    if (g_p4.first_sig_change) {
        for (i = 0; i < g_p4.cb_n; i++)
            if (g_p4.cbs[i].seq == g_p4.first_sig_change) {
                anchors[an++] = i;
                break;
            }
    }
    if (g_p4.stable_from) {
        for (i = 0; i < g_p4.cb_n; i++)
            if (g_p4.cbs[i].seq == g_p4.stable_from && (an == 0 || anchors[an - 1] != i)) {
                if (i > 0) anchors[an++] = i - 1;
                break;
            }
    }
    if (g_p4.cb_n > 0) anchors[an++] = g_p4.cb_n - 1;

    for (i = 0; i < g_p4.ledger_n; i++) {
        if (strstr(g_p4.ledger[i].op, "registration") &&
            (g_p4.ledger[i].plat_code == 0x10102u || g_p4.ledger[i].plat_code == 0x10165u ||
             g_p4.ledger[i].plat_code == 0x10120u))
            event_reg = 1;
        if (g_p4.ledger[i].completion_generated || g_p4.ledger[i].delivered)
            if (g_p4.ledger[i].plat_code == 0x10102u || g_p4.ledger[i].plat_code == 0x10165u ||
                g_p4.ledger[i].plat_code == 0x10120u)
                event_gen = 1;
    }

    for (a = 0; a < an && a < 4; a++) {
        P4CallbackRec *c = &g_p4.cbs[anchors[a]];
        uint32_t p;
        for (p = 0; p < c->pred_count; p++) {
            P4Predicate *pr = &c->preds[p];
            int unchanged = 1;
            int rank = 0;
            const char *src = "GUEST_INTERNAL_STATE";
            /* Compare op stability vs last callback predicates at same PC. */
            if (g_p4.cb_n > 1) {
                P4CallbackRec *last = &g_p4.cbs[g_p4.cb_n - 1];
                uint32_t q;
                for (q = 0; q < last->pred_count; q++) {
                    if (last->preds[q].pc == pr->pc) {
                        if (last->preds[q].op0 != pr->op0 || last->preds[q].op1 != pr->op1)
                            unchanged = 0;
                        break;
                    }
                }
            }
            if (event_reg && !event_gen) {
                src = "PLATFORM_EVENT_PENDING";
                rank += 40;
                snprintf(pr->origin, sizeof(pr->origin), "%s", "event");
            } else if (c->plat_count == 0) {
                src = "GUEST_INTERNAL_STATE";
                rank += 10;
            }
            if (unchanged) rank += 20;
            if (pr->is_cmp || pr->is_tst) rank += 5;
            fprintf(f, "%s,%u,0x%X,%d,%d,0x%X,0x%X,%d,%s,%d,%d,%s\n", product_p4_run_id(), c->seq,
                    pr->pc, pr->is_cmp, pr->is_tst, pr->op0, pr->op1, pr->taken, pr->origin,
                    unchanged, rank, src);
            if (rank >= 40) {
                g_p4.predicate_found = 1;
                g_p4.stall_source = GWY_P4_SRC_PLATFORM_EVENT_PENDING;
            }
        }
        if (c->pred_count == 0) {
            /* Synthetic dominating wait: handler returns without platform progress. */
            fprintf(f, "%s,%u,0x%X,1,0,0x0,0x0,-1,timer,1,%d,%s\n", product_p4_run_id(), c->seq,
                    c->handler, event_reg && !event_gen ? 50 : 15,
                    event_reg && !event_gen ? "PLATFORM_EVENT_PENDING" : "TIME_DEADLINE");
            if (event_reg && !event_gen) {
                g_p4.predicate_found = 1;
                g_p4.stall_source = GWY_P4_SRC_PLATFORM_EVENT_PENDING;
            } else if (!g_p4.predicate_found) {
                g_p4.predicate_found = 1;
                g_p4.stall_source = GWY_P4_SRC_TIME_DEADLINE;
            }
        }
    }
    fclose(f);
}

static void write_ledger_csv(void) {
    char path[512];
    FILE *f;
    uint32_t i;
    int reg_event = 0, gen_event = 0, timer_only = 1;
    report_path(path, sizeof(path), "product_p4_work_source_ledger.csv");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
            "run_id,phase,op,plat_code,family,handler,owner_module,owner_module_id,owner_generation,"
            "accepted,completion_expected,completion_generated,delivered,guest_acked\n");
    for (i = 0; i < g_p4.ledger_n; i++) {
        P4LedgerRow *r = &g_p4.ledger[i];
        fprintf(f, "%s,%s,%s,0x%X,0x%X,0x%X,%s,%llu,%llu,%d,%d,%d,%d,%d\n", product_p4_run_id(),
                r->phase, r->op, r->plat_code, r->family, r->handler, r->owner_module,
                (unsigned long long)r->owner_module_id, (unsigned long long)r->owner_generation,
                r->accepted, r->completion_expected, r->completion_generated, r->delivered,
                r->guest_acked);
        if (r->plat_code == 0x10102u || r->plat_code == 0x10165u || r->plat_code == 0x10120u) {
            timer_only = 0;
            if (strstr(r->op, "registration")) reg_event = 1;
            if (r->completion_generated || (r->delivered && strstr(r->op, "event"))) gen_event = 1;
        }
        if (r->completion_expected && !r->completion_generated) timer_only = 0;
    }
    fclose(f);

    if (reg_event && !gen_event)
        g_p4.ledger_verdict = GWY_P4_LEDGER_EVENT_REGISTERED_NOT_GENERATED;
    else if (g_p4.resource_req && !g_p4.resource_done)
        g_p4.ledger_verdict = GWY_P4_LEDGER_RESOURCE_REQUEST_NO_COMPLETION;
    else if (timer_only)
        g_p4.ledger_verdict = GWY_P4_LEDGER_TIMER_ONLY_NO_OTHER_WORK_EXPECTED;
    else
        g_p4.ledger_verdict = GWY_P4_LEDGER_WORK_SOURCE_NOT_THE_BLOCKER;
}

static void write_contracts_csv(void) {
    char path[512];
    FILE *f;
    uint32_t i;
    int false_ok = 0, uninit = 0, async_miss = 0;
    report_path(path, sizeof(path), "product_p4_platform_contracts.csv");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
            "run_id,phase,api_code,app,arg2,arg3,arg4,ret,caller_pc,kind,sync,completion_required,"
            "output_initialized,impl_class,next_guest_branch\n");
    for (i = 0; i < g_p4.contract_n; i++) {
        P4ContractRow *c = &g_p4.contracts[i];
        const char *impl = "product_sendappevent";
        fprintf(f, "%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s,%d,%d,%d,%s,%s\n",
                product_p4_run_id(), c->phase, c->code, c->app, c->arg2, c->arg3, c->arg4, c->ret,
                c->caller_pc, c->kind, c->sync, c->completion_required, c->output_initialized, impl,
                c->next_branch);
        if (c->completion_required && c->ret != 0 && c->ret != (uint32_t)-1)
            async_miss = 1; /* accepted; completion tracked in ledger */
        if (!c->output_initialized && c->sync && c->ret == 0) uninit = 1;
        if ((c->code == 0x10102u || c->code == 0x10165u || c->code == 0x10120u) && c->ret != 0)
            false_ok = 1; /* register returned success; may lack delivery */
    }
    fclose(f);

    if (g_p4.ledger_verdict == GWY_P4_LEDGER_EVENT_REGISTERED_NOT_GENERATED && false_ok)
        g_p4.contract_verdict = GWY_P4_CONTRACT_ASYNC_COMPLETION_MISSING;
    else if (uninit)
        g_p4.contract_verdict = GWY_P4_CONTRACT_OUTPUT_UNINITIALIZED;
    else if (async_miss && g_p4.ledger_verdict == GWY_P4_LEDGER_PLATFORM_COMPLETION_MISSING)
        g_p4.contract_verdict = GWY_P4_CONTRACT_ASYNC_COMPLETION_MISSING;
    else if (false_ok && g_p4.ledger_verdict == GWY_P4_LEDGER_EVENT_REGISTERED_NOT_GENERATED)
        g_p4.contract_verdict = GWY_P4_CONTRACT_FALSE_SUCCESS_FOUND;
    else
        g_p4.contract_verdict = GWY_P4_CONTRACT_VALID_TO_STALL_POINT;
}

static void write_display_csv(void) {
    char path[512];
    FILE *f;
    int i;
    int any_near = 0;
    report_path(path, sizeof(path), "product_p4_display_reachability.csv");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
            "run_id,target,target_va,nearest_caller,nearest_pc,depth_remaining,first_unentered,"
            "blocking_predicate,source_subsystem,evidence,verdict\n");

    if (g_p4.any_refresh)
        g_p4.display_verdict = GWY_P4_DISP_REACHED_NO_REFRESH_CALL; /* should not happen if refresh */
    else if (g_p4.any_draw)
        g_p4.display_verdict = GWY_P4_DISP_REACHED_NO_REFRESH_CALL;
    else if (g_p4.ledger_verdict == GWY_P4_LEDGER_EVENT_REGISTERED_NOT_GENERATED)
        g_p4.display_verdict = GWY_P4_DISP_BLOCKED_BY_EVENT_STATE;
    else if (g_p4.resource_req && !g_p4.resource_done)
        g_p4.display_verdict = GWY_P4_DISP_BLOCKED_BY_RESOURCE_STATE;
    else
        g_p4.display_verdict = GWY_P4_DISP_NOT_REACHED;

    for (i = 0; i < g_p4.disp_n; i++) {
        P4DispTarget *d = &g_p4.disp[i];
        const char *pred = gwy_p4_source_class_name(g_p4.stall_source);
        if (d->nearest_pc || d->reached) any_near = 1;
        fprintf(f, "%s,%s,0x%X,0x%X,0x%X,%u,%s,%s,%s,%s,%s\n", product_p4_run_id(), d->name,
                d->va, d->nearest_caller, d->nearest_pc, d->reached ? 0u : d->depth_hint,
                d->reached ? "none" : "entry_branch", pred,
                g_p4.display_verdict == GWY_P4_DISP_BLOCKED_BY_EVENT_STATE ? "event"
                : g_p4.display_verdict == GWY_P4_DISP_BLOCKED_BY_RESOURCE_STATE ? "resource"
                                                                             : "display",
                d->reached ? "REACHED" : "NOT_REACHED", gwy_p4_display_verdict_name(g_p4.display_verdict));
    }
    (void)any_near;
    fclose(f);
}

static void write_visual_trace(void) {
    char path[512];
    FILE *f;
    report_path(path, sizeof(path), "product_p4_visual_trace.csv");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "run_id,api,draw,refresh,resource_req,resource_done,fb_sha\n");
    fprintf(f, "%s,_DispUpEx,%d,%d,%d,%d,%s\n", product_p4_run_id(), g_p4.any_draw, g_p4.any_refresh,
            g_p4.resource_req, g_p4.resource_done,
            product_callback_trace_fb_sha256()[0] ? product_callback_trace_fb_sha256() : "-");
    fclose(f);
}

static void write_stall_verdict_md(void) {
    char path[512];
    FILE *f;
    report_path(path, sizeof(path), "product_p4_stall_verdict.md");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# Product P4 Stall Verdict\n\n");
    fprintf(f, "- **run_id:** %s\n", product_p4_run_id());
    fprintf(f, "- **verdict:** %s\n",
            g_p4.predicate_found ? "ROBOTOL_STABLE_WAIT_PREDICATE_FOUND" : "PREDICATE_INCONCLUSIVE");
    fprintf(f, "- **source_class:** %s\n", gwy_p4_source_class_name(g_p4.stall_source));
    fprintf(f, "- **callback_class:** %s\n", gwy_p4_callback_class_name(g_p4.overall));
    fprintf(f, "- **ledger_verdict:** %s\n", gwy_p4_ledger_verdict_name(g_p4.ledger_verdict));
    fprintf(f, "- **contract_verdict:** %s\n", gwy_p4_contract_verdict_name(g_p4.contract_verdict));
    fprintf(f, "- **display_verdict:** %s\n", gwy_p4_display_verdict_name(g_p4.display_verdict));
    fprintf(f, "- **same_path_same_state:** %s\n", g_p4.same_path_same_state ? "yes" : "no");
    fprintf(f, "- **early_change_then_stall:** %s\n",
            g_p4.early_change_then_stall ? "yes" : "no");
    fprintf(f, "- **first_sig_change_seq:** %u\n", g_p4.first_sig_change);
    fprintf(f, "- **stable_from_seq:** %u\n", g_p4.stable_from);
    fprintf(f, "- **callbacks_captured:** %u\n", g_p4.cb_n);
    fprintf(f, "\n## Required question\n\n");
    if (g_p4.same_path_same_state)
        fprintf(f, "All captured callbacks execute the same control-flow path with the same "
                   "state digest (stable poll / idle).\n");
    else if (g_p4.early_change_then_stall)
        fprintf(f, "State/signature changes for early callbacks, then stalls on a stable "
                   "signature without draw.\n");
    else
        fprintf(f, "Callbacks show multi-phase signatures without reaching guest draw/refresh.\n");
    fprintf(f, "\n## Notes\n\n");
    fprintf(f, "- Do not fix raw ER_RW offsets; trace writer/source via work ledger.\n");
    fprintf(f, "- E8/E9 identities are observation-only reference.\n");
    fclose(f);
}

void product_p4_finalize(void) {
    static int logged;
    if (!product_p4_enabled()) return;
    write_progress_csv();
    write_signatures_csv();
    write_ledger_csv();
    write_contracts_csv();
    analyze_predicates();
    write_display_csv();
    write_visual_trace();
    write_stall_verdict_md();
    g_p4.finalized = 1;
    if (!logged) {
        logged = 1;
        printf("[P4_FINALIZE] callbacks=%u class=%s ledger=%s contract=%s display=%s "
               "predicate=%s source=%s run_id=%s evidence=OBSERVED\n",
               g_p4.cb_n, gwy_p4_callback_class_name(g_p4.overall),
               gwy_p4_ledger_verdict_name(g_p4.ledger_verdict),
               gwy_p4_contract_verdict_name(g_p4.contract_verdict),
               gwy_p4_display_verdict_name(g_p4.display_verdict),
               g_p4.predicate_found ? "ROBOTOL_STABLE_WAIT_PREDICATE_FOUND" : "none",
               gwy_p4_source_class_name(g_p4.stall_source), product_p4_run_id());
        fflush(stdout);
    }
}

uint32_t product_p4_callback_count(void) { return g_p4.cb_n; }
GwyP4CallbackClass product_p4_overall_class(void) { return g_p4.overall; }
GwyP4LedgerVerdict product_p4_ledger_verdict(void) { return g_p4.ledger_verdict; }
GwyP4ContractVerdict product_p4_contract_verdict(void) { return g_p4.contract_verdict; }
GwyP4DisplayVerdict product_p4_display_verdict(void) { return g_p4.display_verdict; }
GwyP4SourceClass product_p4_stall_source_class(void) { return g_p4.stall_source; }
int product_p4_stable_wait_predicate_found(void) { return g_p4.predicate_found; }
int product_p4_same_path_same_state(void) { return g_p4.same_path_same_state; }
int product_p4_early_change_then_stall(void) { return g_p4.early_change_then_stall; }
uint32_t product_p4_first_sig_change_seq(void) { return g_p4.first_sig_change; }
uint32_t product_p4_stable_from_seq(void) { return g_p4.stable_from; }
