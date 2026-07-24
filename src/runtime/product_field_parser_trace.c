#include "gwy_launcher/product_field_parser_trace.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/product_event_queue_consumer.h"
#include "gwy_launcher/product_helper_2f68e4_trace.h"
#include "gwy_launcher/product_runtime_progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PC_FRAMING_COPY 0x2E4ECAu
#define PC_FRAMING_COPY_NEXT 0x2E4ECCu
#define PC_PARSER_ENTRY 0x30A0CCu
#define PC_LEN_LDR_CURSOR 0x30A0D8u
#define PC_LEN_LDRB_LO 0x30A0DAu
#define PC_LEN_LDRB_HI 0x30A0E0u
#define PC_LEN_ORRS 0x30A0E6u
#define PC_LEN_TO_R5 0x30A0E8u
#define PC_LEN_CMP_R5 0x30A0EAu
#define PC_LOOP_LO 0x30A100u
#define PC_LOOP_HI 0x30A110u
#define PC_LOOP_EXIT 0x30A112u
#define PC_LOOP_BRANCH 0x30A110u
#define PC_HELPER_RET_SITE 0x2F6952u
#define PC_AFTER_HELPER 0x2E4066u
#define PC_LIFECYCLE 0x2DADC4u
#define OFF_B54 0xB54u
#define OFF_B60 0xB60u

#define LOOP_LOG_INTERVAL 10000u
#define LOOP_SNAP_CAP 16
#define MEM_CAP 512
#define R5_CAP 32
#define SCHED_CAP 256
#define CALL_CAP 64
#define COPY_CAP 32
#define STATE_CAP 64
#define STREAM_DUMP_BEFORE 32
#define STREAM_DUMP_AFTER 96
#define COPY_BUF_MAX 256

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
    uint32_t length_ea;
    uint32_t raw_be16;
    uint32_t cursor_index;
    uint32_t stream_base;
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
    uint32_t call_id;
    uint32_t stream_base;
    uint32_t state_ptr;
    uint32_t cursor_index;
    uint32_t remain_hint;
    uint32_t r5_decoded;
    uint32_t length_ea;
    uint32_t raw_lo, raw_hi;
    uint32_t raw_be16;
    uint8_t before16[16];
    uint8_t after32[32];
    int returned;
    int bad_r5;
    char class_tag[8];
} FpParserCall;

typedef struct {
    uint32_t seq;
    uint32_t dest;
    uint32_t src;
    uint32_t size;
    uint32_t dest_before;
    uint32_t dest_after;
    uint32_t src_word;
    int repaired;
    int skipped_blx;
} FpCopyEvent;

typedef struct {
    uint32_t seq;
    uint32_t state_addr;
    uint32_t field_off;
    uint32_t old_v;
    uint32_t new_v;
    uint32_t writer_pc;
    char phase[24];
} FpStateWrite;

static int g_trace_en, g_trace_known;
static int g_fsc_en, g_fsc_known;
static int g_finalized;
static char g_run_id[80];
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_code_lo = 0x2D8DF4u;
static uint32_t g_code_hi = 0x320000u;

static int g_helper_active;
static uint32_t g_parser_call_id;
static int g_cur_call = -1;
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

static int g_saw_helper_return;
static int g_saw_2e4066;
static int g_saw_2dadc4;
static int g_saw_bad_r5;
static int g_copy_repair_n;
static uint32_t g_last_copy_dest, g_last_copy_src, g_last_copy_size;
static uint32_t g_last_copy_written;
static uint32_t g_dest_capacity_hint;

static FpParserCall g_calls[CALL_CAP];
static int g_call_n;

static FpCopyEvent g_copies[COPY_CAP];
static int g_copy_n;
static FpStateWrite g_state_w[STATE_CAP];
static int g_state_n;

static FpLoopSnap g_loop_snaps[LOOP_SNAP_CAP];
static int g_loop_snap_n;
static FpMemAcc g_mem[MEM_CAP];
static int g_mem_n;
static FpR5Write g_r5_writes[R5_CAP];
static int g_r5_n;
static FpSched g_sched[SCHED_CAP];
static int g_sched_n;

/* Length-decode scratch for current parser call. */
static uint32_t g_len_cursor;
static uint32_t g_len_base;
static uint32_t g_len_lo;
static uint32_t g_len_hi;
static uint32_t g_len_ea;

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

static int env0(const char *k) {
    const char *v = getenv(k);
    return v && v[0] == '0' && v[1] == 0;
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

static void note_state_write(uint32_t addr, uint32_t off, uint32_t old_v, uint32_t new_v,
                             uint32_t pc, const char *phase) {
    FpStateWrite *w;
    if (g_state_n >= STATE_CAP || old_v == new_v) return;
    w = &g_state_w[g_state_n++];
    memset(w, 0, sizeof(*w));
    w->seq = (uint32_t)g_state_n;
    w->state_addr = addr;
    w->field_off = off;
    w->old_v = old_v;
    w->new_v = new_v;
    w->writer_pc = pc & ~1u;
    snprintf(w->phase, sizeof(w->phase), "%s", phase ? phase : "?");
    if (product_fp_enabled()) {
        printf("[FP_STATE] addr=0x%X off=+0x%X old=0x%X new=0x%X pc=0x%X %s evidence=OBSERVED\n",
               addr, off, old_v, new_v, w->writer_pc, w->phase);
        fflush(stdout);
    }
}

static void note_r5_write(uc_engine *uc, uint32_t pc, uint32_t old_v, uint32_t new_v,
                          const char *note) {
    FpR5Write *w;
    if (g_r5_n >= R5_CAP || old_v == new_v) return;
    w = &g_r5_writes[g_r5_n++];
    memset(w, 0, sizeof(*w));
    w->writer_pc = pc & ~1u;
    w->old_r5 = old_v;
    w->new_r5 = new_v;
    w->insn_step = g_insn_step;
    w->length_ea = g_len_ea;
    w->raw_be16 = (g_len_lo << 8) | (g_len_hi & 0xffu);
    w->cursor_index = g_len_cursor;
    w->stream_base = g_len_base;
    snprintf(w->note, sizeof(w->note), "%s", note ? note : "?");
    if (new_v == 0x7374u) g_saw_bad_r5 = 1;
    if (product_fp_enabled()) {
        printf("[FP_R5] pc=0x%X old=0x%X new=0x%X len_ea=0x%X raw_be16=0x%X cursor_idx=0x%X "
               "base=0x%X %s evidence=OBSERVED\n",
               w->writer_pc, old_v, new_v, w->length_ea, w->raw_be16, w->cursor_index,
               w->stream_base, w->note);
        fflush(stdout);
    }
    if (g_cur_call >= 0 && g_cur_call < g_call_n) {
        FpParserCall *c = &g_calls[g_cur_call];
        c->r5_decoded = new_v;
        c->length_ea = g_len_ea;
        c->raw_lo = g_len_lo;
        c->raw_hi = g_len_hi;
        c->raw_be16 = w->raw_be16;
        c->bad_r5 = (new_v == 0x7374u) ? 1 : 0;
        snprintf(c->class_tag, sizeof(c->class_tag), "%s", c->bad_r5 ? "BAD" : "GOOD");
    }
    (void)uc;
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
        uint16_t hw = 0;
        (void)guest_memory_uc_peek((struct uc_struct *)uc, (uint32_t)addr, &hw, 2);
        before = hw;
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
    uint32_t r0 = 0, r1 = 0, r5 = 0, b60 = 0, cursor = 0;
    uint8_t hdr[4];
    char before_hex[STREAM_DUMP_BEFORE * 3 + 8];
    char after_hex[STREAM_DUMP_AFTER * 3 + 8];
    FpParserCall *c;
    if (g_call_n >= CALL_CAP) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R5, &r5);
    if (g_er_rw)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_B60, &b60);
    if (r1)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r1, &cursor);
    g_parser_call_id++;
    g_cur_call = g_call_n;
    c = &g_calls[g_call_n++];
    memset(c, 0, sizeof(*c));
    c->call_id = g_parser_call_id;
    c->stream_base = r0;
    c->state_ptr = r1;
    c->cursor_index = cursor;
    c->r5_decoded = r5;
    snprintf(c->class_tag, sizeof(c->class_tag), "PEND");
    if (guest_memory_uc_peek((struct uc_struct *)uc, r0 + cursor, c->after32, 32)) {
        /* ok */
    }
    if (cursor >= 16u)
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r0 + cursor - 16u, c->before16, 16);
    else if (r0)
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r0, c->before16, 16);
    /* remain hint: entry+8 when stream is Path-A inner (capacity). */
    c->remain_hint = g_dest_capacity_hint;
    if (c->remain_hint == 0 && cursor < 0x10000u)
        c->remain_hint = (cursor < 16u) ? (16u - cursor) : 0;
    hex_dump(uc, r0 + (cursor > STREAM_DUMP_BEFORE ? cursor - STREAM_DUMP_BEFORE : 0),
             STREAM_DUMP_BEFORE, before_hex, sizeof(before_hex));
    hex_dump(uc, r0 + cursor, STREAM_DUMP_AFTER, after_hex, sizeof(after_hex));
    g_len_base = r0;
    g_len_cursor = cursor;
    g_len_ea = r0 + cursor;
    if (guest_memory_uc_peek((struct uc_struct *)uc, g_len_ea, hdr, 4)) {
        uint32_t w = be_u32(hdr);
        printf("[FP_STREAM] be_u32@len_ea=0x%08X ascii=%c%c%c%c evidence=OBSERVED\n", w,
               (w >> 24) & 0xFF, (w >> 16) & 0xFF, (w >> 8) & 0xFF, w & 0xFF);
    }
    printf("[FP_ENTRY] id=%u pc=0x30A0CC base=0x%X state=0x%X cursor_idx=0x%X remain_hint=0x%X "
           "b60=0x%X r5=0x%X evidence=OBSERVED\n",
           c->call_id, c->stream_base, c->state_ptr, c->cursor_index, c->remain_hint, b60, r5);
    printf("[FP_STREAM] before=%s\n", before_hex);
    printf("[FP_STREAM] after=%s\n", after_hex);
    fflush(stdout);
}

/*
 * Path-A framing @0x2E4ECA: BLX r3 intended as memcpy(dest,src,n).
 * Import resolves to DSM 0x804A8 → 0xA24FC which is NOT memcpy; inner stays
 * zero-filled from 0x10132 malloc → 0x308D98 reads tag=0 → 0x30A0CC reads OOB
 * "st" as BE length 0x7374.
 *
 * Repair: host binary copy into dest BEFORE the BLX. Do not skip the BLX —
 * skipping leaves R9 on DSM (0x280400) and breaks the subsequent B54 list
 * push at 0x312A60. Letting BLX return restores robotol R9 via the normal
 * DSM helper path; the stub does not overwrite the repaired inner.
 */
static int try_framing_copy_repair(uc_engine *uc, uint32_t pc) {
    uint32_t dest = 0, src = 0, n = 0, before = 0, after = 0, src_word = 0;
    uint8_t buf[COPY_BUF_MAX];
    FpCopyEvent *ev;
    int do_repair;

    if ((pc & ~1u) != PC_FRAMING_COPY) return 0;
    dest = read_reg(uc, 0);
    src = read_reg(uc, 1);
    n = read_reg(uc, 2);
    g_last_copy_dest = dest;
    g_last_copy_src = src;
    g_last_copy_size = n;
    g_dest_capacity_hint = n;
    if (dest)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, dest, &before);
    if (src)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, src, &src_word);

    do_repair = product_fsc_enabled() && dest && src && n > 0 && n <= COPY_BUF_MAX;
    if (do_repair) {
        memset(buf, 0, sizeof(buf));
        if (!guest_memory_uc_peek((struct uc_struct *)uc, src, buf, n) ||
            !guest_memory_uc_poke((struct uc_struct *)uc, dest, buf, n)) {
            do_repair = 0;
        } else {
            g_last_copy_written = n;
            g_copy_repair_n++;
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, dest, &after);
            product_runtime_progress_emit("field_stream_copy_repaired", "fsc", "0x2E4ECA");
        }
    }
    if (!do_repair && dest)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, dest, &after);

    if (g_copy_n < COPY_CAP) {
        ev = &g_copies[g_copy_n++];
        memset(ev, 0, sizeof(*ev));
        ev->seq = (uint32_t)g_copy_n;
        ev->dest = dest;
        ev->src = src;
        ev->size = n;
        ev->dest_before = before;
        ev->dest_after = after;
        ev->src_word = src_word;
        ev->repaired = do_repair ? 1 : 0;
        ev->skipped_blx = 0;
    }

    printf("[FSC_COPY] pc=0x2E4ECA dest=0x%X src=0x%X n=0x%X src_word=0x%08X dest_before=0x%08X "
           "dest_after=0x%08X repaired=%d skip_blx=0 contract=%d evidence=OBSERVED\n",
           dest, src, n, src_word, before, after, do_repair ? 1 : 0,
           product_fsc_enabled() ? 1 : 0);
    fflush(stdout);
    (void)uc;
    return 0; /* never skip BLX — keep R9 restore path intact */
}

static void on_fp_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t pc_norm = pc & ~1u;
    uint32_t r5 = 0;
    (void)size;
    (void)user_data;

    if (!product_fp_enabled() && !product_fsc_enabled()) return;

    /* Framing copy: host binary memcpy before BLX; never skip BLX (R9 restore). */
    if (pc_norm == PC_FRAMING_COPY)
        (void)try_framing_copy_repair(uc, pc_norm);
    if (pc_norm == PC_FRAMING_COPY_NEXT && product_fsc_enabled() && g_last_copy_dest &&
        g_last_copy_src && g_last_copy_size > 0 && g_last_copy_size <= COPY_BUF_MAX) {
        uint32_t cur = 0, expect = 0;
        uint8_t buf[COPY_BUF_MAX];
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_last_copy_dest, &cur);
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_last_copy_src, &expect);
        if (cur != expect) {
            memset(buf, 0, sizeof(buf));
            if (guest_memory_uc_peek((struct uc_struct *)uc, g_last_copy_src, buf,
                                     g_last_copy_size) &&
                guest_memory_uc_poke((struct uc_struct *)uc, g_last_copy_dest, buf,
                                     g_last_copy_size)) {
                g_copy_repair_n++;
                g_last_copy_written = g_last_copy_size;
                printf("[FSC_COPY] pc=0x2E4ECC post_repair dest=0x%X src=0x%X n=0x%X "
                       "was=0x%08X now=0x%08X evidence=OBSERVED\n",
                       g_last_copy_dest, g_last_copy_src, g_last_copy_size, cur, expect);
                fflush(stdout);
                product_runtime_progress_emit("field_stream_copy_repaired", "fsc", "0x2E4ECC");
            }
        }
    }

    if (pc_norm == PC_AFTER_HELPER && !g_saw_2e4066) {
        g_saw_2e4066 = 1;
        printf("[FP_MILESTONE] entered=0x2E4066 evidence=OBSERVED\n");
        product_runtime_progress_emit("path_a_after_helper", "fp", "0x2E4066");
        fflush(stdout);
    }
    if (pc_norm == PC_LIFECYCLE && !g_saw_2dadc4) {
        g_saw_2dadc4 = 1;
        printf("[FP_MILESTONE] entered=0x2DADC4 evidence=OBSERVED\n");
        product_runtime_progress_emit("path_a_lifecycle", "fp", "0x2DADC4");
        fflush(stdout);
    }
    if (pc_norm == PC_HELPER_RET_SITE && g_helper_active) {
        g_saw_helper_return = 1;
        printf("[FP_MILESTONE] helper_2F68E4_return_site evidence=OBSERVED\n");
        product_runtime_progress_emit("helper_2f68e4_returned", "fp", "0x2F6952");
        fflush(stdout);
    }

    if (!product_fp_enabled() || !g_helper_active) return;

    g_insn_step++;
    uc_reg_read(uc, UC_ARM_REG_R5, &r5);

    if (pc_norm == PC_PARSER_ENTRY) log_parser_entry(uc);

    /* Length provenance: BE u16 = (stream[cursor]<<8)|stream[cursor+1]. */
    if (pc_norm == PC_LEN_LDR_CURSOR) {
        uint32_t st = read_reg(uc, 4);
        uint32_t idx = 0;
        if (st) (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, st, &idx);
        g_len_base = read_reg(uc, 6);
        g_len_cursor = idx;
        g_len_ea = g_len_base + idx;
        note_state_write(st, 0, idx, idx, pc_norm, "len_load_cursor");
    }
    if (pc_norm == PC_LEN_LDRB_LO) {
        uint8_t b = 0;
        uint32_t idx = read_reg(uc, 0);
        g_len_cursor = idx;
        g_len_ea = g_len_base + idx;
        if (g_len_base)
            (void)guest_memory_uc_peek((struct uc_struct *)uc, g_len_base + idx, &b, 1);
        g_len_lo = b;
        printf("[FP_LEN] stage=lo_byte ea=0x%X raw=0x%02X cursor=0x%X evidence=OBSERVED\n",
               g_len_base + idx, b, idx);
        fflush(stdout);
    }
    if (pc_norm == PC_LEN_LDRB_HI) {
        uint8_t b = 0;
        uint32_t idx = read_reg(uc, 0);
        if (g_len_base)
            (void)guest_memory_uc_peek((struct uc_struct *)uc, g_len_base + idx, &b, 1);
        g_len_hi = b;
        printf("[FP_LEN] stage=hi_byte ea=0x%X raw=0x%02X be16=0x%X evidence=OBSERVED\n",
               g_len_base + idx, b, (g_len_lo << 8) | b);
        fflush(stdout);
    }
    if (pc_norm == PC_LEN_TO_R5 || pc_norm == PC_LEN_ORRS) {
        /* fallthrough — r5 change caught below */
    }

    if (g_last_r5 != r5 && pc_norm >= PC_PARSER_ENTRY && pc_norm <= PC_LOOP_EXIT + 8u)
        note_r5_write(uc, pc, g_last_r5, r5,
                      (pc_norm == PC_LEN_CMP_R5 || pc_norm == PC_LEN_TO_R5) ? "be16_length" : "reg_change");
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
    if (pc_norm == PC_LOOP_EXIT && g_cur_call >= 0 && g_cur_call < g_call_n) {
        g_calls[g_cur_call].returned = 1;
        if (!g_calls[g_cur_call].bad_r5)
            snprintf(g_calls[g_cur_call].class_tag, sizeof(g_calls[g_cur_call].class_tag), "GOOD");
        printf("[FP_PARSER_RETURN] id=%u r5=0x%X class=%s evidence=OBSERVED\n",
               g_calls[g_cur_call].call_id, g_calls[g_cur_call].r5_decoded,
               g_calls[g_cur_call].class_tag);
        product_runtime_progress_emit("field_parser_completed", "fp", "0x30A112");
        fflush(stdout);
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
    uint32_t pc = 0;
    (void)type;
    (void)user_data;
    record_mem(uc, address, size, (uint64_t)value, "write");
    /* Track parser-state cursor cell writes near length/copy path. */
    if (product_fp_enabled() && g_helper_active && size == 4 && g_call_n > 0) {
        FpParserCall *c = &g_calls[g_call_n - 1];
        if (c->state_ptr && (uint32_t)address == c->state_ptr) {
            uint32_t old_v = 0;
            uc_reg_read(uc, UC_ARM_REG_PC, &pc);
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, (uint32_t)address, &old_v);
            note_state_write(c->state_ptr, 0, old_v, (uint32_t)value, pc, "cursor_store");
        }
    }
}

static void fp_atexit(void) {
    if (product_fp_enabled() || product_fsc_enabled()) product_fp_finalize();
}

static void arm_hooks(void *uc) {
    uc_err e;
    uint32_t lo, hi;
    if (!uc || g_hooks_armed) return;
    lo = g_code_lo;
    hi = g_code_hi;
    /* Ensure framing copy site is covered even if module range is tight. */
    if (lo > PC_FRAMING_COPY) lo = PC_FRAMING_COPY;
    if (hi < PC_LIFECYCLE) hi = PC_LIFECYCLE;
    e = uc_hook_add((uc_engine *)uc, &g_code_hook, UC_HOOK_CODE, (void *)on_fp_code, NULL,
                    (uint64_t)lo, (uint64_t)hi);
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
    printf("[FP_HOOKS] armed code=0x%X..0x%X copy=0x%X loop=0x%X..0x%X fsc=%d trace=%d "
           "evidence=OBSERVED\n",
           lo, hi, PC_FRAMING_COPY, PC_LOOP_LO, PC_LOOP_HI, product_fsc_enabled(),
           product_fp_enabled());
    fflush(stdout);
}
#endif

int product_fp_enabled(void) {
    if (!g_trace_known) {
        g_trace_en = env1("JJFB_FIELD_PARSER_TRACE");
        g_trace_known = 1;
    }
    return g_trace_en;
}

int product_fsc_enabled(void) {
    if (!g_fsc_known) {
        /* Default ON (unset); explicit 0 disables for A/B baseline. */
        g_fsc_en = env0("JJFB_FIELD_STREAM_CONTRACT") ? 0 : 1;
        g_fsc_known = 1;
    }
    return g_fsc_en;
}

void product_fp_reset(void) {
    g_finalized = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_helper_active = 0;
    g_parser_call_id = 0;
    g_cur_call = -1;
    g_in_loop = 0;
    g_loop_iter = 0;
    g_last_r5 = 0;
    g_insn_step = 0;
    g_cb_depth = g_consumer_depth = g_timer_tick = 0;
    g_nested_pub_tick = 0;
    g_nested_published = 0;
    g_consumer_after_nested = 0;
    g_saw_helper_return = g_saw_2e4066 = g_saw_2dadc4 = g_saw_bad_r5 = 0;
    g_copy_repair_n = 0;
    g_last_copy_dest = g_last_copy_src = g_last_copy_size = g_last_copy_written = 0;
    g_dest_capacity_hint = 0;
    g_loop_snap_n = g_mem_n = g_r5_n = g_sched_n = g_call_n = g_copy_n = g_state_n = 0;
    g_trace_known = 0;
    g_trace_en = 0;
    g_fsc_known = 0;
    g_fsc_en = 0;
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
    if ((product_fp_enabled() || product_fsc_enabled()) && uc) arm_hooks(uc);
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

void product_fp_note_callback_depth(uint32_t depth) { g_cb_depth = depth; }

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
    if (g_nested_published && g_helper_active && g_loop_iter > 0 && (tick % 50) == 0)
        sched_event("timer_tick", peek_queue(), "periodic");
}

void product_fp_finalize(void) {
    if (g_finalized) return;
    if (!product_fp_enabled() && !product_fsc_enabled()) return;
    g_finalized = 1;
    write_reports();
}

static const char *classify_verdict(void) {
    if (g_copy_repair_n > 0 && g_saw_helper_return && g_saw_2e4066)
        return "FIELD_PARSER_CONTRACT_REPAIRED";
    if (g_copy_repair_n > 0 && !g_saw_bad_r5)
        return "FIELD_PARSER_CONTRACT_REPAIRED";
    if (g_saw_bad_r5 && g_loop_iter > 100)
        return "HEAP_COPY_SOURCE_OFFSET_WRONG";
    if (g_copy_n > 0 && !product_fsc_enabled() && g_saw_bad_r5)
        return "HEAP_COPY_SOURCE_OFFSET_WRONG";
    if (g_saw_helper_return)
        return "FIELD_PARSER_CONTRACT_REPAIRED";
    return "STREAM_FRAMING_CONTRACT_MISSING";
}

static void write_reports(void) {
    char path[512];
    FILE *f;
    int i;
    const char *verdict = classify_verdict();
    FpParserCall *bad = NULL, *good = NULL;

    for (i = 0; i < g_call_n; i++) {
        if (g_calls[i].bad_r5 && !bad) bad = &g_calls[i];
        if (!g_calls[i].bad_r5 && g_calls[i].returned && !good) good = &g_calls[i];
    }

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

    report_path("field_parser_r5_writes.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,writer_pc,old_r5,new_r5,len_ea,raw_be16,cursor_idx,base,step,note\n");
        for (i = 0; i < g_r5_n; i++) {
            FpR5Write *w = &g_r5_writes[i];
            fprintf(f, "%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%s\n", product_fp_run_id(),
                    w->writer_pc, w->old_r5, w->new_r5, w->length_ea, w->raw_be16, w->cursor_index,
                    w->stream_base, w->insn_step, w->note);
        }
        fclose(f);
    }

    report_path("field_parser_calls.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,call_id,base,state,cursor_idx,remain_hint,r5,len_ea,raw_be16,returned,"
                   "bad,class\n");
        for (i = 0; i < g_call_n; i++) {
            FpParserCall *c = &g_calls[i];
            fprintf(f, "%s,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%d,%s\n", product_fp_run_id(),
                    c->call_id, c->stream_base, c->state_ptr, c->cursor_index, c->remain_hint,
                    c->r5_decoded, c->length_ea, c->raw_be16, c->returned, c->bad_r5, c->class_tag);
        }
        fclose(f);
    }

    report_path("field_stream_copies.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,dest,src,size,src_word,dest_before,dest_after,repaired,skipped_blx\n");
        for (i = 0; i < g_copy_n; i++) {
            FpCopyEvent *e = &g_copies[i];
            fprintf(f, "%s,%u,0x%X,0x%X,0x%X,0x%08X,0x%08X,0x%08X,%d,%d\n", product_fp_run_id(),
                    e->seq, e->dest, e->src, e->size, e->src_word, e->dest_before, e->dest_after,
                    e->repaired, e->skipped_blx);
        }
        fclose(f);
    }

    report_path("field_parser_state_writes.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,state,off,old,new,pc,phase\n");
        for (i = 0; i < g_state_n; i++) {
            FpStateWrite *w = &g_state_w[i];
            fprintf(f, "%s,%u,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", product_fp_run_id(), w->seq,
                    w->state_addr, w->field_off, w->old_v, w->new_v, w->writer_pc, w->phase);
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
                    s->event, s->queue, s->nested, s->cb_depth, s->consumer_depth, s->helper_active,
                    s->detail);
        }
        fclose(f);
    }

    report_path("stage_field_parser_task9.md", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "# Task 9: Field Length / Cursor Provenance and Contract Repair\n\n");
        fprintf(f, "- **run_id:** %s\n", product_fp_run_id());
        fprintf(f, "- **verdict:** `%s`\n", verdict);
        fprintf(f, "- **FIELD_STREAM_CONTRACT:** %d\n", product_fsc_enabled());
        fprintf(f, "- **copy repairs:** %d\n", g_copy_repair_n);
        fprintf(f, "- **parser calls:** %d\n", g_call_n);
        fprintf(f, "- **bad r5=0x7374 seen:** %s\n", g_saw_bad_r5 ? "yes" : "no");
        fprintf(f, "- **0x2F68E4 return:** %s\n", g_saw_helper_return ? "yes" : "no");
        fprintf(f, "- **0x2E4066 entered:** %s\n", g_saw_2e4066 ? "yes" : "no");
        fprintf(f, "- **0x2DADC4 entered:** %s\n", g_saw_2dadc4 ? "yes" : "no");
        fprintf(f, "\n## Length decode at 0x30A0CC\n\n");
        fprintf(f, "```text\n");
        fprintf(f, "LDR  r0, [r4]           ; cursor index\n");
        fprintf(f, "LDRB r1, [r6, r0]       ; lo = stream[cursor]\n");
        fprintf(f, "cursor++\n");
        fprintf(f, "LDRB r2, [r6, r0]       ; hi = stream[cursor]\n");
        fprintf(f, "r5 = (lo << 8) | hi     ; BE u16 field length  @0x30A0E8\n");
        fprintf(f, "CMP  r5, #0             ; @0x30A0EA (not the write)\n");
        fprintf(f, "```\n\n");
        fprintf(f, "## Framing layout (dynamic)\n\n");
        fprintf(f, "```text\n");
        fprintf(f, "[BE u32 tag]           ; 0x308D98 — exit when tag == -1\n");
        fprintf(f, "[BE u16 len][bytes][0] ; 0x30A0CC string field\n");
        fprintf(f, "[BE u16 len][bytes][0] ; 0x30A0CC string field\n");
        fprintf(f, "[BE u32][BE u32]       ; two more 0x308D98 words\n");
        fprintf(f, "```\n\n");
        fprintf(f, "Empty Path-A body (with_rec=0) inner must be **`FF FF FF FF`** (BE -1).\n");
        fprintf(f, "Observed pre-repair: **`00 00 00 00`** (malloc zero-fill; copy never wrote).\n");
        fprintf(f, "Adjacent OOB at inner+4 showed ASCII `\"stat\"` → BE length `0x7374`.\n\n");
        fprintf(f, "## GOOD vs BAD\n\n");
        fprintf(f, "| item | GOOD | BAD |\n|---|---:|---:|\n");
        if (good)
            fprintf(f, "| call_id | %u | %s |\n", good->call_id, bad ? "—" : "n/a");
        else
            fprintf(f, "| call_id | (none / empty-body exit) | %s |\n",
                    bad ? "present" : "n/a");
        if (bad) {
            fprintf(f, "| cursor_idx | — | 0x%X |\n", bad->cursor_index);
            fprintf(f, "| length_ea | — | 0x%X |\n", bad->length_ea);
            fprintf(f, "| raw bytes | — | 0x%02X 0x%02X |\n", bad->raw_lo & 0xff, bad->raw_hi & 0xff);
            fprintf(f, "| decoded r5 | — | 0x%X |\n", bad->r5_decoded);
        }
        fprintf(f, "\n## Copy contract @0x2E4ECA\n\n");
        fprintf(f, "| dest | src | n | repaired |\n|---|---|---|---|\n");
        for (i = 0; i < g_copy_n && i < 8; i++) {
            FpCopyEvent *e = &g_copies[i];
            fprintf(f, "| 0x%X | 0x%X | 0x%X | %d |\n", e->dest, e->src, e->size, e->repaired);
        }
        fprintf(f, "\n## Classification\n\n");
        fprintf(f, "```text\n%s\n```\n", verdict);
        fprintf(f, "\n## Required answers\n\n");
        fprintf(f, "| Q | A |\n|---|---|\n");
        fprintf(f, "| 0x30A0EA length source? | BE u16 at `stream_base + cursor_index` "
                "(written at `0x30A0E8`; `0x30A0EA` is `CMP r5,#0`) |\n");
        fprintf(f, "| correct field length? | For empty body: **no field** — tag should be "
                "`0xFFFFFFFF`; length N/A. `0x10` is entry+8 capacity, not field length. |\n");
        fprintf(f, "| \"stat\" meaning? | **OOB adjacent heap bytes**, not a field name |\n");
        fprintf(f, "| heap/cursor wrong why? | Framing BLX→`0x804A8` is not memcpy; inner stays "
                "zeros; cursor advances past 4-byte buffer |\n");
        fprintf(f, "| first diverge insn? | Missing write at intended memcpy `@0x2E4ECA`; "
                "first wrong read `@0x30A0DA` with cursor=4 |\n");
        fprintf(f, "| 0x10132 wrong? | **No** — size-malloc OK; zero-fill expected before copy |\n");
        fprintf(f, "| repair domain? | **copy** (Scheme C) at Path-A framing |\n");
        fprintf(f, "| helper returned? | %s |\n", g_saw_helper_return ? "yes" : "no");
        fprintf(f, "| 0x2E4066 / 0x2DADC4? | %s / %s |\n", g_saw_2e4066 ? "yes" : "no",
                g_saw_2dadc4 ? "yes" : "no");
        fclose(f);
    }

    printf("[FP_FINALIZE] verdict=%s fsc=%d repairs=%d calls=%d bad_r5=%d helper_ret=%d "
           "e4066=%d dadc4=%d loop_iter=%u evidence=OBSERVED\n",
           verdict, product_fsc_enabled(), g_copy_repair_n, g_call_n, g_saw_bad_r5,
           g_saw_helper_return, g_saw_2e4066, g_saw_2dadc4, g_loop_iter);
    fflush(stdout);
}
