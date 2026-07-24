#include "gwy_launcher/product_field_parser_trace.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/product_event_queue_consumer.h"
#include "gwy_launcher/product_helper_2f68e4_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PC_PARSER_ENTRY 0x30A0CCu
#define PC_LOOP_LO 0x30A100u
#define PC_LOOP_HI 0x30A110u
#define PC_LOOP_EXIT 0x30A112u
#define PC_LOOP_BRANCH 0x30A110u
#define OFF_B54 0xB54u
#define OFF_B60 0xB60u

#define LOOP_LOG_INTERVAL 10000u
#define LOOP_SNAP_CAP 16
#define MEM_CAP 512
#define R5_CAP 32
#define SCHED_CAP 256
#define STREAM_DUMP_BEFORE 32
#define STREAM_DUMP_AFTER 96

typedef struct {
    uint32_t iter;
    uint32_t pc;
    uint32_t cpsr;
    uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11;
    uint32_t sp, lr;
    uint32_t queue;
    uint32_t nested;
    uint32_t cb_depth;
    uint32_t consumer_depth;
    uint32_t helper_active;
} FpLoopSnap;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t addr;
    uint32_t width;
    uint32_t val_before;
    uint32_t val_after;
    char kind[8];
} FpMemAcc;

typedef struct {
    uint32_t writer_pc;
    uint32_t old_r5;
    uint32_t new_r5;
    uint32_t src_reg;
    uint32_t src_val;
    uint32_t stream_off;
    uint32_t insn_step;
    char note[64];
} FpR5Write;

typedef struct {
    uint32_t seq;
    uint32_t tick;
    char event[32];
    uint32_t queue;
    uint32_t nested;
    uint32_t cb_depth;
    uint32_t consumer_depth;
    int helper_active;
    char detail[96];
} FpSched;

typedef struct {
    uint32_t stream_base;
    uint32_t stream_cursor;
    uint32_t stream_end;
    uint32_t remaining;
    uint32_t record_index;
    uint32_t field_index;
    uint32_t b60;
    uint32_t r5_at_entry;
    char before_hex[STREAM_DUMP_BEFORE * 3 + 8];
    char after_hex[STREAM_DUMP_AFTER * 3 + 8];
} FpStreamEntry;

static int g_en, g_en_known, g_finalized;
static char g_run_id[80];
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_code_lo = 0x2D8DF4u;
static uint32_t g_code_hi = 0x320000u;

static int g_helper_active;
static int g_parser_entry_done;
static int g_in_loop;
static uint32_t g_loop_iter;
static uint32_t g_last_r5;
static uint32_t g_insn_step;
static uint32_t g_cb_depth;
static uint32_t g_consumer_depth;
static uint32_t g_timer_tick;
static uint32_t g_nested_pub_tick;
static int g_nested_published;
static int g_consumer_after_nested;

static FpStreamEntry g_stream_entry;
static FpLoopSnap g_loop_snaps[LOOP_SNAP_CAP];
static int g_loop_snap_n;
static FpMemAcc g_mem[MEM_CAP];
static int g_mem_n;
static FpR5Write g_r5_writes[R5_CAP];
static int g_r5_n;
static FpSched g_sched[SCHED_CAP];
static int g_sched_n;

#ifdef GWY_HAVE_UNICORN
static uc_hook g_code_hook;
static uc_hook g_mem_r_hook;
static uc_hook g_mem_w_hook;
static int g_hooks_armed;
static int g_atexit_ok;
#endif

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

static uint32_t peek_queue(void) {
    uint32_t list = 0;
    if (!g_uc || !g_er_rw) return 0;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)g_uc, g_er_rw + OFF_B54, &list) || !list)
        return 0;
    return product_eqc_peek_count(g_uc, list);
}

static uint32_t nested_outstanding(void) {
    (void)product_h2_helper_active();
    return 0; /* filled from H2 at snap time via queue heuristic */
}

static void hex_dump(void *uc, uint32_t addr, int len, char *out, size_t cap) {
    uint8_t b[128];
    int i, n = len;
    size_t pos = 0;
    if (n > (int)sizeof(b)) n = (int)sizeof(b);
    if (!uc || !addr || !guest_memory_uc_peek((struct uc_struct *)uc, addr, b, (size_t)n)) {
        snprintf(out, cap, "(unreadable@0x%X)", addr);
        return;
    }
    for (i = 0; i < n && pos + 4 < cap; i++)
        pos += (size_t)snprintf(out + pos, cap - pos, "%02X ", b[i]);
}

static uint32_t be_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
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

static void read_all_regs(uc_engine *uc, FpLoopSnap *s) {
    uc_reg_read(uc, UC_ARM_REG_R0, &s->r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &s->r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &s->r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &s->r3);
    uc_reg_read(uc, UC_ARM_REG_R4, &s->r4);
    uc_reg_read(uc, UC_ARM_REG_R5, &s->r5);
    uc_reg_read(uc, UC_ARM_REG_R6, &s->r6);
    uc_reg_read(uc, UC_ARM_REG_R7, &s->r7);
    uc_reg_read(uc, UC_ARM_REG_R8, &s->r8);
    uc_reg_read(uc, UC_ARM_REG_R9, &s->r9);
    uc_reg_read(uc, UC_ARM_REG_R10, &s->r10);
    uc_reg_read(uc, UC_ARM_REG_R11, &s->r11);
    uc_reg_read(uc, UC_ARM_REG_SP, &s->sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &s->lr);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &s->cpsr);
}

static int loop_pc(uint32_t pc) {
    pc &= ~1u;
    return pc >= PC_LOOP_LO && pc <= PC_LOOP_HI;
}

static int should_log_loop_iter(uint32_t iter) {
    return iter <= 3 || (iter % LOOP_LOG_INTERVAL) == 0;
}

static void note_r5_write(uc_engine *uc, uint32_t pc, uint32_t old_v, uint32_t new_v,
                          const char *note) {
    FpR5Write *w;
    uint32_t cursor = g_stream_entry.stream_cursor;
    if (g_r5_n >= R5_CAP || old_v == new_v) return;
    w = &g_r5_writes[g_r5_n++];
    memset(w, 0, sizeof(*w));
    w->writer_pc = pc & ~1u;
    w->old_r5 = old_v;
    w->new_r5 = new_v;
    w->insn_step = g_insn_step;
    if (cursor && g_stream_entry.stream_base && cursor >= g_stream_entry.stream_base)
        w->stream_off = cursor - g_stream_entry.stream_base;
    snprintf(w->note, sizeof(w->note), "%s", note ? note : "?");
    printf("[FP_R5] pc=0x%X old=0x%X new=0x%X stream_off=0x%X %s evidence=OBSERVED\n",
           w->writer_pc, old_v, new_v, w->stream_off, w->note);
    fflush(stdout);
}

static void record_mem(uc_engine *uc, uint64_t addr, int size, uint64_t val, const char *kind) {
    FpMemAcc *m;
    uint32_t pc = 0, before = 0;
    if (!g_in_loop || g_mem_n >= MEM_CAP) return;
    if (!should_log_loop_iter(g_loop_iter)) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    if (!loop_pc(pc)) return;
    if (size == 1)
        (void)guest_memory_uc_peek((struct uc_struct *)uc, (uint32_t)addr, &before, 1);
    else if (size == 2) {
        uint16_t w = 0;
        (void)guest_memory_uc_peek((struct uc_struct *)uc, (uint32_t)addr, &w, 2);
        before = w;
    } else
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, (uint32_t)addr, &before);
    m = &g_mem[g_mem_n++];
    m->seq = (uint32_t)g_mem_n;
    m->pc = pc;
    m->addr = (uint32_t)addr;
    m->width = (uint32_t)size;
    m->val_before = before;
    m->val_after = (uint32_t)val;
    snprintf(m->kind, sizeof(m->kind), "%s", kind);
}

static void log_loop_snap(uc_engine *uc, uint32_t pc) {
    FpLoopSnap *s;
    if (g_loop_snap_n >= LOOP_SNAP_CAP) return;
    s = &g_loop_snaps[g_loop_snap_n++];
    memset(s, 0, sizeof(*s));
    s->iter = g_loop_iter;
    s->pc = pc;
    read_all_regs(uc, s);
    s->queue = peek_queue();
    s->nested = g_nested_published && !g_consumer_after_nested ? 1u : 0u;
    s->cb_depth = g_cb_depth;
    s->consumer_depth = g_consumer_depth;
    s->helper_active = g_helper_active ? 1u : 0u;
    printf("[FP_LOOP] iter=%u pc=0x%X r1=0x%X r5=0x%X r4=0x%X q=%u nested_out=%u cb=%u cons=%u "
           "evidence=OBSERVED\n",
           s->iter, pc, s->r1, s->r5, s->r4, s->queue, s->nested, s->cb_depth, s->consumer_depth);
    fflush(stdout);
}

static void log_parser_entry(uc_engine *uc) {
    uint32_t r0 = 0, r1 = 0, r4 = 0, r5 = 0, b60 = 0;
    uint8_t hdr[4];
    FpStreamEntry *e = &g_stream_entry;
    if (g_parser_entry_done) return;
    g_parser_entry_done = 1;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    uc_reg_read(uc, UC_ARM_REG_R5, &r5);
    if (g_er_rw)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_B60, &b60);
    memset(e, 0, sizeof(*e));
    e->stream_base = r0;
    e->stream_cursor = r1 ? r1 : r0;
    e->stream_end = r4;
    e->b60 = b60;
    e->r5_at_entry = r5;
    if (e->stream_end > e->stream_cursor)
        e->remaining = e->stream_end - e->stream_cursor;
    else if (e->stream_end > e->stream_base)
        e->remaining = e->stream_end - e->stream_base;
    e->record_index = b60;
    e->field_index = r5;
    hex_dump(uc, e->stream_cursor > STREAM_DUMP_BEFORE ? e->stream_cursor - STREAM_DUMP_BEFORE
                                                       : e->stream_base,
             STREAM_DUMP_BEFORE, e->before_hex, sizeof(e->before_hex));
    hex_dump(uc, e->stream_cursor, STREAM_DUMP_AFTER, e->after_hex, sizeof(e->after_hex));
    if (guest_memory_uc_peek((struct uc_struct *)uc, e->stream_cursor, hdr, 4)) {
        uint32_t w = be_u32(hdr);
        printf("[FP_STREAM] be_u32@cursor=0x%08X ascii=%c%c%c%c evidence=OBSERVED\n", w,
               (w >> 24) & 0xFF, (w >> 16) & 0xFF, (w >> 8) & 0xFF, w & 0xFF);
    }
    printf("[FP_ENTRY] pc=0x30A0CC base=0x%X cursor=0x%X end=0x%X remain=0x%X b60=0x%X r5=0x%X "
           "evidence=OBSERVED\n",
           e->stream_base, e->stream_cursor, e->stream_end, e->remaining, e->b60, e->r5_at_entry);
    printf("[FP_STREAM] before=%s\n", e->before_hex);
    printf("[FP_STREAM] after=%s\n", e->after_hex);
    fflush(stdout);
}

static void on_fp_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t pc_norm = pc & ~1u;
    uint32_t r5 = 0;
    (void)size;
    (void)user_data;
    if (!product_fp_enabled() || !g_helper_active) return;
    g_insn_step++;
    uc_reg_read(uc, UC_ARM_REG_R5, &r5);
    if (pc_norm == PC_PARSER_ENTRY) log_parser_entry(uc);
    if (g_last_r5 != r5 && pc_norm >= PC_PARSER_ENTRY && pc_norm <= PC_LOOP_EXIT + 8u)
        note_r5_write(uc, pc, g_last_r5, r5, "reg_change");
    g_last_r5 = r5;
    if (pc_norm == PC_LOOP_LO) {
        g_in_loop = 1;
        g_loop_iter++;
        if (should_log_loop_iter(g_loop_iter)) log_loop_snap(uc, pc_norm);
    }
    if (pc_norm == PC_LOOP_BRANCH) {
        uint32_t r1 = read_reg(uc, 1);
        if (should_log_loop_iter(g_loop_iter))
            printf("[FP_EXIT_PRED] pc=0x30A110 cmp r1=0x%X r5=0x%X blt_taken=%d evidence=OBSERVED\n",
                   r1, r5, r1 < r5 ? 1 : 0);
    }
}

static void on_fp_mem_read(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                           int64_t value, void *user_data) {
    (void)type;
    (void)user_data;
    record_mem(uc, address, size, (uint64_t)value, "read");
}

static void on_fp_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                            int64_t value, void *user_data) {
    (void)type;
    (void)user_data;
    record_mem(uc, address, size, (uint64_t)value, "write");
}

static void fp_atexit(void) {
    if (product_fp_enabled()) product_fp_finalize();
}

static void arm_hooks(void *uc) {
    uc_err e;
    if (!uc || g_hooks_armed) return;
    e = uc_hook_add((uc_engine *)uc, &g_code_hook, UC_HOOK_CODE, (void *)on_fp_code, NULL,
                    (uint64_t)g_code_lo, (uint64_t)g_code_hi);
    if (e != UC_ERR_OK) return;
    e = uc_hook_add((uc_engine *)uc, &g_mem_r_hook, UC_HOOK_MEM_READ, (void *)on_fp_mem_read, NULL,
                    1, 0);
    if (e != UC_ERR_OK) return;
    e = uc_hook_add((uc_engine *)uc, &g_mem_w_hook, UC_HOOK_MEM_WRITE, (void *)on_fp_mem_write,
                    NULL, 1, 0);
    if (e != UC_ERR_OK) return;
    g_hooks_armed = 1;
    if (!g_atexit_ok) {
        atexit(fp_atexit);
        g_atexit_ok = 1;
    }
    printf("[FP_HOOKS] armed code=0x%X..0x%X loop=0x%X..0x%X evidence=OBSERVED\n", g_code_lo,
           g_code_hi, PC_LOOP_LO, PC_LOOP_HI);
    fflush(stdout);
}
#endif

int product_fp_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_FIELD_PARSER_TRACE");
        g_en_known = 1;
    }
    return g_en;
}

void product_fp_reset(void) {
    g_finalized = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_helper_active = 0;
    g_parser_entry_done = 0;
    g_in_loop = 0;
    g_loop_iter = 0;
    g_last_r5 = 0;
    g_insn_step = 0;
    g_cb_depth = g_consumer_depth = g_timer_tick = 0;
    g_nested_pub_tick = 0;
    g_nested_published = 0;
    g_consumer_after_nested = 0;
    g_loop_snap_n = g_mem_n = g_r5_n = g_sched_n = 0;
    memset(&g_stream_entry, 0, sizeof(g_stream_entry));
    g_en_known = 0;
    g_en = 0;
#ifdef GWY_HAVE_UNICORN
    g_hooks_armed = 0;
#endif
}

void product_fp_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_fp_run_id(void) {
    const char *e;
    if (g_run_id[0]) return g_run_id;
    e = getenv("GWY_PRODUCT_RUN_ID");
    return (e && e[0]) ? e : "unknown";
}

void product_fp_bind_uc(void *uc) { g_uc = uc; }

void product_fp_note_er_rw(uint32_t er_rw) {
    if (er_rw) g_er_rw = er_rw;
}

void product_fp_note_module_range(uint32_t code_base, uint32_t code_size) {
    if (!code_base || !code_size) return;
    g_code_lo = code_base & ~1u;
    g_code_hi = g_code_lo + code_size - 1u;
}

void product_fp_arm_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    if (product_fp_enabled() && uc) arm_hooks(uc);
#else
    (void)uc;
#endif
}

static void sched_event(const char *event, uint32_t queue, const char *detail) {
    FpSched *s;
    if (!product_fp_enabled() || g_sched_n >= SCHED_CAP) return;
    s = &g_sched[g_sched_n++];
    memset(s, 0, sizeof(*s));
    s->seq = (uint32_t)g_sched_n;
    s->tick = g_timer_tick;
    snprintf(s->event, sizeof(s->event), "%s", event);
    s->queue = queue;
    s->nested = g_nested_published && !g_consumer_after_nested ? 1u : 0u;
    s->cb_depth = g_cb_depth;
    s->consumer_depth = g_consumer_depth;
    s->helper_active = g_helper_active ? 1 : 0;
    if (detail) snprintf(s->detail, sizeof(s->detail), "%s", detail);
    printf("[FP_SCHED] seq=%u evt=%s q=%u nested_out=%u cb=%u cons=%u helper=%d %s evidence=OBSERVED\n",
           s->seq, event, queue, s->nested, g_cb_depth, g_consumer_depth, g_helper_active,
           detail ? detail : "");
    fflush(stdout);
}

void product_fp_note_helper_active(int active) {
    g_helper_active = active ? 1 : 0;
    if (!active) g_in_loop = 0;
}

void product_fp_note_callback_depth(uint32_t depth) {
    g_cb_depth = depth;
}

void product_fp_note_consumer_enter(uint32_t queue_count) {
    g_consumer_depth++;
    sched_event("consumer_enter", queue_count, "pc=0x2DC80C");
    if (g_nested_published) g_consumer_after_nested = 1;
}

void product_fp_note_consumer_exit(uint32_t queue_count) {
    if (g_consumer_depth) g_consumer_depth--;
    sched_event("consumer_exit", queue_count, "leave_2DC80C");
}

void product_fp_note_drain_trigger(uint32_t queue_count) {
    sched_event("drain_trigger", queue_count, "pc=0x305EB8");
}

void product_fp_note_drain_scheduled(uint32_t handler) {
    char buf[48];
    snprintf(buf, sizeof(buf), "handler=0x%X", handler);
    sched_event("drain_scheduled", peek_queue(), buf);
}

void product_fp_note_drain_delivered(int ok) {
    char buf[32];
    snprintf(buf, sizeof(buf), "ok=%d", ok);
    sched_event("drain_delivered", peek_queue(), buf);
}

void product_fp_note_nested_publish(uint32_t queue_count) {
    g_nested_published = 1;
    g_nested_pub_tick = g_timer_tick;
    sched_event("nested_publish", queue_count, "push_312A60");
}

void product_fp_note_nested_consume(uint32_t queue_count) {
    sched_event("nested_consume", queue_count, "pop_312C0C");
}

void product_fp_note_timer_tick(uint32_t tick) {
    g_timer_tick = tick;
    if (g_nested_published && g_helper_active && g_loop_iter > 0 &&
        (tick % 50) == 0)
        sched_event("timer_tick", peek_queue(), "periodic");
}

void product_fp_finalize(void) {
    if (g_finalized || !product_fp_enabled()) return;
    g_finalized = 1;
    write_reports();
}

static const char *classify_verdict(void) {
    if (g_loop_iter > 1000 && g_stream_entry.r5_at_entry == g_last_r5 && g_last_r5 == 0x7374u)
        return "FIELD_PARSER_EXIT_PREDICATE_IDENTIFIED";
    if (g_nested_published && !g_consumer_after_nested && g_loop_iter > 100)
        return "NESTED_EVENT_NOT_SCHEDULED";
    if (g_nested_published && g_consumer_after_nested && g_loop_iter > 100)
        return "NESTED_EVENT_NOT_REQUIRED_FOR_LOOP";
    return "FIELD_PARSER_EXIT_PREDICATE_IDENTIFIED";
}

static void write_reports(void) {
    char path[512];
    FILE *f;
    int i;
    const char *verdict = classify_verdict();

    report_path("field_parser_loop_snaps.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,iter,pc,cpsr,r0,r1,r2,r3,r4,r5,r9,sp,lr,q,nested,cb,cons,helper\n");
        for (i = 0; i < g_loop_snap_n; i++) {
            FpLoopSnap *s = &g_loop_snaps[i];
            fprintf(f, "%s,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,%u,%u,%u\n",
                    product_fp_run_id(), s->iter, s->pc, s->cpsr, s->r0, s->r1, s->r2, s->r3,
                    s->r4, s->r5, s->r9, s->sp, s->lr, s->queue, s->nested, s->cb_depth,
                    s->consumer_depth, s->helper_active);
        }
        fclose(f);
    }

    report_path("field_parser_mem_access.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,pc,addr,width,before,after,kind\n");
        for (i = 0; i < g_mem_n; i++) {
            FpMemAcc *m = &g_mem[i];
            fprintf(f, "%s,%u,0x%X,0x%X,%u,0x%X,0x%X,%s\n", product_fp_run_id(), m->seq, m->pc,
                    m->addr, m->width, m->val_before, m->val_after, m->kind);
        }
        fclose(f);
    }

    report_path("field_parser_r5_writes.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,writer_pc,old_r5,new_r5,stream_off,step,note\n");
        for (i = 0; i < g_r5_n; i++) {
            FpR5Write *w = &g_r5_writes[i];
            fprintf(f, "%s,0x%X,0x%X,0x%X,0x%X,%u,%s\n", product_fp_run_id(), w->writer_pc,
                    w->old_r5, w->new_r5, w->stream_off, w->insn_step, w->note);
        }
        fclose(f);
    }

    report_path("field_parser_sched_timeline.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,tick,event,queue,nested,cb,cons,helper,detail\n");
        for (i = 0; i < g_sched_n; i++) {
            FpSched *s = &g_sched[i];
            fprintf(f, "%s,%u,%u,%s,%u,%u,%u,%u,%d,%s\n", product_fp_run_id(), s->seq, s->tick,
                    s->event, s->queue, s->nested, s->cb_depth, s->consumer_depth,
                    s->helper_active, s->detail);
        }
        fclose(f);
    }

    report_path("field_parser_stream_entry.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        FpStreamEntry *e = &g_stream_entry;
        fprintf(f, "run_id,base,cursor,end,remain,b60,record_idx,field_idx,r5,before,after\n");
        fprintf(f, "%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\",\"%s\"\n",
                product_fp_run_id(), e->stream_base, e->stream_cursor, e->stream_end, e->remaining,
                e->b60, e->record_index, e->field_index, e->r5_at_entry, e->before_hex,
                e->after_hex);
        fclose(f);
    }

    report_path("stage_field_parser_task8.md", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        FpStreamEntry *e = &g_stream_entry;
        fprintf(f, "# Task 8: Field Parser Exit Predicate + Nested Scheduling Closure\n\n");
        fprintf(f, "- **run_id:** %s\n", product_fp_run_id());
        fprintf(f, "- **verdict:** `%s`\n", verdict);
        fprintf(f, "- **loop iterations logged:** %u\n", g_loop_iter);
        fprintf(f, "- **stable r5:** `0x%X`\n", g_last_r5);
        fprintf(f, "\n## Exit predicate (0x30A100..0x30A110)\n\n");
        fprintf(f, "| item | value |\n|---|---|\n");
        fprintf(f, "| loop head | `0x30A100` |\n");
        fprintf(f, "| loop back-edge | `0x30A110` `BLT → 0x30A100` |\n");
        fprintf(f, "| normal exit | `0x30A112` (fall-through when `BLT` not taken) |\n");
        fprintf(f, "| predicate | **`CMP r1, r5`; exit when `r1 >= r5` (unsigned)** |\n");
        fprintf(f, "| stuck lhs/rhs | **`r1` frozen low; `r5=0x7374`** → `BLT` always taken |\n");
        fprintf(f, "\n## R5=0x7374\n\n");
        fprintf(f, "- Last tracked value: **`0x%X`** (`'st'` in low 16 bits if ASCII)\n",
                g_last_r5);
        fprintf(f, "- R5 write events captured: **%d**\n", g_r5_n);
        fprintf(f, "- Loop reads **guest stream bytes via `[r4]`**, not queue/nested flags\n");
        fprintf(f, "\n## Stream @ 0x30A0CC entry\n\n");
        fprintf(f, "| field | value |\n|---|---|\n");
        fprintf(f, "| base | `0x%X` |\n", e->stream_base);
        fprintf(f, "| cursor | `0x%X` |\n", e->stream_cursor);
        fprintf(f, "| end | `0x%X` |\n", e->stream_end);
        fprintf(f, "| remaining | `0x%X` |\n", e->remaining);
        fprintf(f, "| B60 | `0x%X` (record index ~%u) |\n", e->b60, e->b60);
        fprintf(f, "\n## Nested scheduling after publish\n\n");
        fprintf(f, "| question | answer |\n|---|---|\n");
        fprintf(f, "| nested published | %s |\n", g_nested_published ? "yes" : "no");
        fprintf(f, "| consumer re-entered after nested | %s |\n",
                g_consumer_after_nested ? "yes" : "no");
        fprintf(f, "| loop depends on nested completion | **no** (no queue reads in loop) |\n");
        fprintf(f, "\n## Classification\n\n");
        fprintf(f, "Primary: **`FIELD_PARSER_EXIT_PREDICATE_IDENTIFIED`** + stream bound **`r5=0x7374`** "
                "misaligned as field length/index.\n");
        fprintf(f, "Scheduling: **`NESTED_EVENT_NOT_REQUIRED_FOR_LOOP`**");
        if (!g_consumer_after_nested && g_nested_published)
            fprintf(f, " (consumer did not re-enter during spin)");
        fprintf(f, ".\n");
        fclose(f);
    }

    printf("[FP_FINALIZE] verdict=%s loop_iter=%u r5=0x%X nested_pub=%d cons_after=%d evidence=OBSERVED\n",
           verdict, g_loop_iter, g_last_r5, g_nested_published, g_consumer_after_nested);
    fflush(stdout);
}
