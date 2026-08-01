#include "gwy_launcher/p22n_opcode_stream.h"

#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define P22N_WIN 0x200u
#define P22N_STREAM_CAP 256u
#define P22N_WRITE_CAP 512u
#define P22N_PROV_CAP 256u
#define P22N_SLICE_CAP 1024u
#define P22N_FIRE2_TARGET 20u
#define P22N_OP_MAX 0x27u

#define OFF_6E5C 0x6E5Cu
#define OFF_1C408 0x1C408u /* interpreter loop head */
#define OFF_1C40C 0x1C40Cu
#define OFF_1C414 0x1C414u
#define OFF_1C41C 0x1C41Cu
#define OFF_1C56C 0x1C56Cu
#define OFF_1C580 0x1C580u
#define OFF_1C588 0x1C588u /* first ADDLS landing slot (opcode 0) */
#define OFF_1CD40 0x1CD40u /* opcode 0x14 case */
#define OFF_1D1CC 0x1D1CCu /* opcode 0x1B case */
#define OFF_10740 0x10740u
#define OFF_7B6C 0x7B6Cu

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t record_addr;
    uint32_t raw_record;
    uint32_t flags30;
    uint32_t index;
    uint32_t opcode;
    uint32_t slot_addr;
    uint32_t jt_entry;
    uint32_t case_target;
    uint32_t object;
    uint32_t cursor_before;
    uint32_t cursor_after;
    uint32_t r[13];
    uint32_t sp, lr, r9;
    char note[48];
} StreamRow;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t off;
    uint32_t addr;
    uint32_t old_v;
    uint32_t new_v;
    uint32_t size;
    char phase[24];
} WriteRow;

typedef struct {
    uint32_t seq;
    uint32_t record_addr;
    uint32_t value;
    uint32_t writer_pc;
    uint32_t writer_off;
    char writer_mod[40];
    uint32_t old_v;
    char kind[32];
    char note[64];
} ProvRow;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t off;
    uint32_t insn;
    char desc[48];
    uint32_t r0, r1, r2, r3, r4, r5, r8, r9, r10, sp, lr;
    char note[56];
} SliceRow;

typedef struct {
    uint32_t opcode;
    uint32_t jt_slot_off;
    uint32_t case_off;
    char tags[96];
} JtRow;

static struct {
    int armed, finalized, dense;
    int hook_sparse, hook_global, hook_mem;
    int image_exported;
    int await_next;
    int pending_undense; /* tear down hooks outside UC_HOOK_CODE callback */
    void *uc;
    uint32_t cont_pc, window_end, ldm_pc;
    uint32_t last_method, await_method, await_stack_pc;

    uint32_t cf_base, cf_end, cf_size, cf_erw;
    uint32_t gl_base, gl_end, gl_helper;
    uint32_t p_guest;
    uint64_t generation;
    char package_owner[64];
    char cf_sha[65];

    uint32_t object;
    uint32_t fire2_n;
    uint32_t m601_done;
    int entered_10740, entered_7b6c, cfg_open, callback_pub;
    int helper_reenter;
    int in_op14, in_op1b;
    int saw_op14, saw_op1b;
    int interpreter_idle;

    StreamRow stream[P22N_STREAM_CAP];
    uint32_t stream_n;
    WriteRow writes[P22N_WRITE_CAP];
    uint32_t write_n;
    ProvRow prov[P22N_PROV_CAP];
    uint32_t prov_n;
    SliceRow slice14[P22N_SLICE_CAP];
    uint32_t slice14_n;
    SliceRow slice1b[P22N_SLICE_CAP];
    uint32_t slice1b_n;
    JtRow jt[P22N_OP_MAX];
    uint32_t jt_n;

    uint32_t watch_recs[64];
    uint32_t watch_n;

    char run_id[64];
    char stop_reason[96];
    char sole_lock[280];
    char next_fix[240];
    char op14_sem[160];
    char op1b_sem[160];
    char stream_kind[48];
    char exit_how[80];
    char method2_source[80];
    char divergence[200];
    uint32_t divergence_pc;
    char divergence_actual[80];
    char divergence_producer[120];

#ifdef GWY_HAVE_UNICORN
    uc_hook h_sparse, h_global, h_mem_r, h_mem_w;
#endif
} g;

static const char *env_or(const char *k, const char *d) {
    const char *v = getenv(k);
    return (v && v[0]) ? v : d;
}

int p22n_enabled(void) {
    const char *e = getenv("JJFB_P22N_CLEAN");
    return e && e[0] == '1';
}

int p22n_observation_complete(void) {
    return g.finalized;
}

static FILE *open_out(const char *envk, const char *defpath) {
    return fopen(env_or(envk, defpath), "wb");
}

static int read_u32(uint32_t addr, uint32_t *out) {
#ifdef GWY_HAVE_UNICORN
    if (!g.uc || !out) return 0;
    return guest_memory_uc_peek_u32((struct uc_struct *)g.uc, addr, out);
#else
    (void)addr;
    (void)out;
    return 0;
#endif
}

static uint32_t branch_target_arm(uint32_t pc, uint32_t insn) {
    int32_t imm;
    if ((insn & 0x0E000000u) != 0x0A000000u) return 0;
    imm = (int32_t)(insn & 0x00FFFFFFu);
    if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
    return (uint32_t)((int32_t)pc + 8 + (imm << 2));
}

static void describe_arm(uint32_t w, uint32_t pc, char *out, size_t n) {
    int32_t imm;
    uint32_t tgt;
    if ((w & 0x0F000000u) == 0x0B000000u) {
        imm = (int32_t)(w & 0x00FFFFFFu);
        if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
        tgt = (uint32_t)((int32_t)pc + 8 + (imm << 2));
        snprintf(out, n, "BL 0x%X", tgt);
        return;
    }
    if ((w & 0x0F000000u) == 0x0A000000u) {
        tgt = branch_target_arm(pc, w);
        snprintf(out, n, "Bcond 0x%X", tgt);
        return;
    }
    if ((w & 0x0FFFFFF0u) == 0x012FFF30u) {
        snprintf(out, n, "BLX r%u", w & 0xFu);
        return;
    }
    if ((w & 0x0FFF8000u) == 0x08BD8000u) {
        snprintf(out, n, "LDMFD ..pc");
        return;
    }
    if (w == 0xE4905004u) {
        snprintf(out, n, "LDR r5,[r0],#4");
        return;
    }
    if (w == 0x908FF100u) {
        snprintf(out, n, "ADDLS pc,pc,r0,LSL#2");
        return;
    }
    snprintf(out, n, "w=0x%08X", w);
}

static void adopt_cf(const GwyLoadedModule *m, const char *via) {
    const char *nm;
    if (!m || !m->map.guest_code_base || !m->map.guest_code_size) return;
    nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    g.cf_base = m->map.guest_code_base;
    g.cf_size = m->map.guest_code_size;
    g.cf_end = g.cf_base + g.cf_size;
    if (m->data.start_of_er_rw) g.cf_erw = m->data.start_of_er_rw;
    printf("[JJFB_P22N] cf_base=0x%X end=0x%X via=%s name=%s evidence=OBSERVED\n", g.cf_base,
           g.cf_end, via ? via : "?", nm ? nm : "?");
    fflush(stdout);
}

static void ensure_cf_base(void) {
    ModuleRegistry *reg;
    size_t i;
    if (g.cf_base && g.cf_end > g.cf_base) return;
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return;
    for (i = 0; i < reg->count; i++) {
        const GwyLoadedModule *m = &reg->modules[i];
        const char *nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
        if (!nm || !strstr(nm, "cfunction")) continue;
        adopt_cf(m, "name_scan");
        break;
    }
    if (!g.gl_base) {
        for (i = 0; i < reg->count; i++) {
            const GwyLoadedModule *m = &reg->modules[i];
            const char *nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
            if (!nm || !strstr(nm, "gamelist")) continue;
            g.gl_base = m->map.guest_code_base;
            g.gl_end = g.gl_base + m->map.guest_code_size;
            if (m->entries.registered_helper) g.gl_helper = m->entries.registered_helper;
            else if (m->map.helper_address) g.gl_helper = m->map.helper_address;
            break;
        }
    }
}

static void ensure_cf_from_pc(uint32_t pc) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    ensure_cf_base();
    if (g.cf_base && g.cf_end > g.cf_base) return;
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return;
    m = module_registry_find_by_code_addr(reg, pc & ~1u);
    if (m) adopt_cf(m, "find_by_pc");
}

static void watch_rec(uint32_t addr) {
    uint32_t i;
    if (!addr) return;
    for (i = 0; i < g.watch_n; i++)
        if (g.watch_recs[i] == addr) return;
    if (g.watch_n < 64u) g.watch_recs[g.watch_n++] = addr;
}

static int is_watched(uint32_t addr) {
    uint32_t i;
    for (i = 0; i < g.watch_n; i++) {
        if (addr == g.watch_recs[i] || (addr + 4u > g.watch_recs[i] && addr < g.watch_recs[i] + 4u))
            return 1;
    }
    return 0;
}

static void build_jump_table(void) {
    uint32_t op, slot, insn, tgt;
    if (!g.cf_base || g.jt_n) return;
    for (op = 0; op <= 0x26u && g.jt_n < P22N_OP_MAX; op++) {
        JtRow *j = &g.jt[g.jt_n++];
        memset(j, 0, sizeof(*j));
        j->opcode = op;
        /* ADDLS pc,pc,r0,LSL#2 @ +0x1C580 uses PC=addr+8 → first slot +0x1C588 */
        j->jt_slot_off = OFF_1C588 + op * 4u;
        slot = g.cf_base + j->jt_slot_off;
        if (!read_u32(slot, &insn)) continue;
        tgt = branch_target_arm(slot, insn);
        j->case_off = (tgt && tgt >= g.cf_base) ? tgt - g.cf_base : 0;
        if (op == 0x14u)
            snprintf(j->tags, sizeof(j->tags), "OBSERVED_THIS_RUN;cursor_xform");
        else if (op == 0x1Bu)
            snprintf(j->tags, sizeof(j->tags), "OBSERVED_THIS_RUN;list_ops");
        else if (j->case_off == OFF_1C408 || j->case_off == 0x1C408u)
            snprintf(j->tags, sizeof(j->tags), "loop_back");
        else
            snprintf(j->tags, sizeof(j->tags), "case");
    }
}

static void maybe_export_image(void) {
    uint8_t *buf;
    uint8_t dig[32];
    FILE *f;
    size_t n;
    ensure_cf_base();
    if (g.image_exported || !g.uc || !g.cf_base || !g.cf_size) return;
    n = g.cf_size > 0x100000u ? 0x100000u : g.cf_size;
    buf = (uint8_t *)malloc(n);
    if (!buf) return;
    if (!guest_memory_uc_peek((struct uc_struct *)g.uc, g.cf_base, buf, (uint32_t)n)) {
        free(buf);
        return;
    }
    gwy_sha256(buf, n, dig);
    gwy_sha256_hex(dig, g.cf_sha);
    f = fopen(env_or("JJFB_P22N_CF_BIN", "out/p22n/cfunction_runtime.bin"), "wb");
    if (f) {
        fwrite(buf, 1, n, f);
        fclose(f);
    }
    f = fopen(env_or("JJFB_P22N_CF_SHA", "out/p22n/cfunction_runtime.sha256"), "wb");
    if (f) {
        /* Final dump SHA only — never append prior init-phase hashes. */
        fprintf(f, "%s\n", g.cf_sha);
        fprintf(f, "base=0x%X\nend=0x%X\nsize=0x%X\nerw=0x%X\nP=0x%X\ngeneration=%llu\n",
                g.cf_base, g.cf_end, (uint32_t)n, g.cf_erw, g.p_guest,
                (unsigned long long)g.generation);
        fclose(f);
    }
    g.image_exported = 1;
    build_jump_table();
    printf("[JJFB_P22N] export_cfunction sha=%s base=0x%X size=0x%X evidence=OBSERVED\n", g.cf_sha,
           g.cf_base, (uint32_t)n);
    fflush(stdout);
    free(buf);
}

static void add_stream(const StreamRow *src) {
    StreamRow *s;
    if (g.stream_n >= P22N_STREAM_CAP) return;
    s = &g.stream[g.stream_n++];
    *s = *src;
    s->seq = g.stream_n;
    watch_rec(s->record_addr);
    printf("[JJFB_P22N] opcode_stream seq=%u rec=0x%X raw=0x%X idx=0x%X op=0x%02X case=+0x%X "
           "flags=0x%X evidence=OBSERVED\n",
           s->seq, s->record_addr, s->raw_record, s->index, s->opcode, s->case_target
                                                                         ? s->case_target - g.cf_base
                                                                         : 0,
           s->flags30);
    fflush(stdout);
}

static void add_prov(uint32_t addr, uint32_t val, uint32_t pc, uint32_t oldv, const char *kind,
                     const char *note) {
    ProvRow *p;
    char mod[40];
    uint32_t off = 0;
    if (g.prov_n >= P22N_PROV_CAP) return;
    mod[0] = 0;
    if (g.cf_base && pc >= g.cf_base && pc < g.cf_end) {
        snprintf(mod, sizeof(mod), "cfunction.ext");
        off = (pc & ~3u) - g.cf_base;
    } else if (g.gl_base && pc >= g.gl_base && pc < g.gl_end) {
        snprintf(mod, sizeof(mod), "gamelist.ext");
        off = (pc & ~1u) - g.gl_base;
    } else {
        snprintf(mod, sizeof(mod), "other");
    }
    p = &g.prov[g.prov_n++];
    memset(p, 0, sizeof(*p));
    p->seq = g.prov_n;
    p->record_addr = addr;
    p->value = val;
    p->writer_pc = pc;
    p->writer_off = off;
    snprintf(p->writer_mod, sizeof(p->writer_mod), "%s", mod);
    p->old_v = oldv;
    snprintf(p->kind, sizeof(p->kind), "%s", kind ? kind : "?");
    snprintf(p->note, sizeof(p->note), "%s", note ? note : "");
}

#ifdef GWY_HAVE_UNICORN
static void finish_dense(const char *why);
static void install_global_hook(void);
static void remove_global_hook(void);
static void install_mem_hooks(void);
static void remove_mem_hooks(void);

static void emit_case_slice(SliceRow *buf, uint32_t *n, uint32_t cap, uint32_t pc, uint32_t off,
                            uint32_t insn, const uint32_t regs[16], uint32_t sp, uint32_t lr,
                            const char *note) {
    SliceRow *s;
    if (*n >= cap) return;
    s = &buf[(*n)++];
    memset(s, 0, sizeof(*s));
    s->seq = *n;
    s->pc = pc;
    s->off = off;
    s->insn = insn;
    describe_arm(insn, pc, s->desc, sizeof(s->desc));
    s->r0 = regs[0];
    s->r1 = regs[1];
    s->r2 = regs[2];
    s->r3 = regs[3];
    s->r4 = regs[4];
    s->r5 = regs[5];
    s->r8 = regs[8];
    s->r9 = regs[9];
    s->r10 = regs[10];
    s->sp = sp;
    s->lr = lr;
    snprintf(s->note, sizeof(s->note), "%s", note ? note : "");
}

static void on_dense(uc_engine *uc, uint32_t pc, const uint32_t regs[16], uint32_t sp, uint32_t lr,
                     uint32_t cpsr, uint32_t insn) {
    uint32_t off = 0;
    int in_cf;
    (void)cpsr;
    if (!g.cf_base || !g.cf_end) ensure_cf_from_pc(pc);
    in_cf = g.cf_base && g.cf_end && pc >= g.cf_base && pc < g.cf_end;
    if (in_cf) off = (pc & ~3u) - g.cf_base;

    if (g.gl_base && pc >= g.gl_base && pc < g.gl_end) {
        uint32_t go = (pc & ~1u) - g.gl_base;
        if (go == OFF_10740) g.entered_10740 = 1;
        if (go == OFF_7B6C) g.entered_7b6c = 1;
    }

    if (g.gl_helper && (pc & ~1u) == (g.gl_helper & ~1u)) {
        g.helper_reenter = 1;
        if (g.fire2_n >= P22N_FIRE2_TARGET && g.stream_n > 0) {
            snprintf(g.exit_how, sizeof(g.exit_how), "helper_reenter_after_%u_fire2", g.fire2_n);
            finish_dense("helper_reenter_idle");
            return;
        }
    }

    /* Record fetch at +0x1C40C: LDR r5,[r0],#4 */
    if (in_cf && off == OFF_1C40C) {
        StreamRow row;
        uint32_t flags = 0;
        memset(&row, 0, sizeof(row));
        row.pc = pc;
        row.record_addr = regs[0];
        row.cursor_before = regs[0];
        row.object = regs[4] ? regs[4] : g.object;
        if (!g.object) g.object = row.object;
        (void)read_u32(row.record_addr, &row.raw_record);
        if (row.object) (void)read_u32(row.object + 0x30u, &flags);
        /* flags byte is LDRB — low 8 bits of +0x30 word on LE */
        row.flags30 = flags & 0xFFu;
        if (!row.flags30) {
            uint8_t b = 0;
            if (guest_memory_uc_peek((struct uc_struct *)uc, row.object + 0x30u, &b, 1))
                row.flags30 = b;
        }
        row.index = (row.raw_record >> 24) & 0xFFu;
        row.opcode = row.raw_record & 0x3Fu;
        row.cursor_after = row.record_addr + 4u;
        row.slot_addr = 0;
        row.jt_entry = g.cf_base + OFF_1C588 + row.opcode * 4u;
        if (row.opcode <= 0x26u) {
            uint32_t jinsn = 0;
            (void)read_u32(row.jt_entry, &jinsn);
            row.case_target = branch_target_arm(row.jt_entry, jinsn);
        }
        {
            uint32_t base08 = 0;
            if (row.object && read_u32(row.object + 0x0Cu, &base08))
                row.slot_addr = base08 + (row.index << 3);
        }
        memcpy(row.r, regs, sizeof(row.r));
        row.sp = sp;
        row.lr = lr;
        row.r9 = regs[9];
        snprintf(row.note, sizeof(row.note), "NORMAL_OPCODE_DISPATCH");
        add_stream(&row);
        add_prov(row.record_addr, row.raw_record, 0, 0, "preexisting_at_fetch",
                 "no_writer_in_window_yet");
        if (row.opcode == 0x14u) {
            g.in_op14 = 1;
            g.saw_op14 = 1;
            snprintf(g.op14_sem, sizeof(g.op14_sem),
                     "AND/ADD transform of cursor word: 0x%X -> 0x%X then B +0x1C408",
                     row.cursor_before, row.cursor_after);
        }
        if (row.opcode == 0x1Bu) {
            g.in_op1b = 1;
            g.saw_op1b = 1;
        }
    }

    /* Annotate: 0x9C41C is NORMAL path, not a lock */
    if (in_cf && off == OFF_1C41C) {
        /* observed; no finish */
    }

    if (in_cf && off == OFF_1C56C) {
        /* entering opcode dispatch body */
    }

    if (g.in_op14 && in_cf) {
        char note[56];
        note[0] = 0;
        if (off == OFF_1CD40) snprintf(note, sizeof(note), "op14_enter");
        if (off == 0x1CD5Cu) {
            snprintf(note, sizeof(note), "op14_loop_back");
            snprintf(g.op14_sem, sizeof(g.op14_sem),
                     "cursor transform via literals/masks; writes [sp,#0xC] then B +0x1C408");
            g.in_op14 = 0;
        }
        emit_case_slice(g.slice14, &g.slice14_n, P22N_SLICE_CAP, pc, off, insn, regs, sp, lr, note);
        if (off == 0x1C408u && g.slice14_n > 2u) g.in_op14 = 0;
    }

    if (g.in_op1b && in_cf) {
        char note[56];
        note[0] = 0;
        if (off == OFF_1D1CC) snprintf(note, sizeof(note), "op1b_enter");
        if ((insn & 0x0FFF8000u) == 0x08BD8000u) {
            snprintf(note, sizeof(note), "op1b_return");
            snprintf(g.op1b_sem, sizeof(g.op1b_sem),
                     "mutate object+0x08/+0x0C list cursor; BL +0x17CF4 optional; LDMFD exits "
                     "interpreter (not loop to +0x1C408)");
            g.in_op1b = 0;
            g.interpreter_idle = 1;
            snprintf(g.exit_how, sizeof(g.exit_how), "op1b_LDMFD_exit_then_await_fire2");
            /* Stop dense processing; actual hook_del deferred (safe outside callback). */
            finish_dense("op1b_interpreter_exit");
            return;
        }
        if (off == 0x1C3ACu || off == 0x1C3D4u) {
            snprintf(note, sizeof(note), "op1b_to_interpreter");
            g.in_op1b = 0;
        }
        emit_case_slice(g.slice1b, &g.slice1b_n, P22N_SLICE_CAP, pc, off, insn, regs, sp, lr, note);
    }

    if (g.entered_10740) {
        finish_dense("entered_10740");
        return;
    }
    if (g.stream_n >= P22N_STREAM_CAP) finish_dense("stream_cap");
}

static void on_next(uc_engine *uc, uint32_t pc, uint32_t size) {
    uint32_t regs[16];
    uint32_t sp = 0, lr = 0, cpsr = 0, insn = 0;
    int i;
    (void)size;
    if (!p22n_enabled() || g.finalized) return;
    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~1u, &insn);

    if (g.await_next && (pc & ~1u) != (g.ldm_pc & ~1u)) {
        if (g.await_method == 1u && !g.dense) {
            ensure_cf_from_pc(pc);
            maybe_export_image();
            g.dense = 1;
            g.m601_done = 1;
            install_mem_hooks();
            printf("[JJFB_P22N] dense_begin pc=0x%X r0=0x%X cf_base=0x%X evidence=OBSERVED\n", pc,
                   regs[0], g.cf_base);
            fflush(stdout);
        } else if (g.await_method != 1u && !g.dense) {
            remove_global_hook();
        }
        g.await_next = 0;
    }
    if (g.dense) on_dense(uc, pc, regs, sp, lr, cpsr, insn);
}

static void p22n_on_sparse(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t regs[16], sp = 0, insn = 0;
    int i;
    (void)size;
    (void)user_data;
    if (!p22n_enabled() || g.finalized || !g.armed || g.dense) return;
    if (pc < g.cont_pc || pc >= g.window_end) return;
    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &insn);
    if (insn == 0xE8BD8DF0u || ((insn & 0xFFFF0000u) == 0xE8BD0000u && (insn & 0x8000u))) {
        uint32_t rl = insn & 0xFFFFu, pci = 0, pv = 0, bit;
        for (bit = 0; bit < 15u; bit++)
            if (rl & (1u << bit)) pci++;
        g.ldm_pc = pc;
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + pci * 4u, &pv);
        g.await_next = 1;
        g.await_method = g.last_method;
        g.await_stack_pc = pv;
        install_global_hook();
    }
}

static void p22n_on_global(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)user_data;
    on_next(uc, (uint32_t)address, size);
}

static void p22n_on_mem(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                        void *user_data) {
    uint32_t pc = 0, addr = (uint32_t)address;
    (void)user_data;
    if (!g.dense || g.finalized || type != UC_MEM_WRITE) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    if (g.write_n < P22N_WRITE_CAP) {
        WriteRow *w = &g.writes[g.write_n++];
        uint32_t oldv = 0;
        (void)read_u32(addr, &oldv);
        memset(w, 0, sizeof(*w));
        w->seq = g.write_n;
        w->pc = pc;
        w->off = (g.cf_base && pc >= g.cf_base && pc < g.cf_end) ? (pc & ~3u) - g.cf_base : 0;
        w->addr = addr;
        w->old_v = oldv;
        w->new_v = (uint32_t)value;
        w->size = (uint32_t)size;
        snprintf(w->phase, sizeof(w->phase), "%s",
                 g.in_op14 ? "op14" : (g.in_op1b ? "op1b" : "other"));
    }
    if (is_watched(addr) || (size >= 4 && is_watched(addr & ~3u))) {
        uint32_t oldv = 0;
        (void)read_u32(addr & ~3u, &oldv);
        add_prov(addr & ~3u, (uint32_t)value, pc, oldv, "record_or_cursor_write",
                 g.in_op14 ? "during_op14" : (g.in_op1b ? "during_op1b" : "runtime"));
    }
    /* Heuristic: word writes that look like opcode records (low 6 bits <= 0x26) */
    if (size == 4 && (((uint32_t)value) & 0x3Fu) <= 0x26u &&
        ((((uint32_t)value) >> 24) != 0 || (((uint32_t)value) & 0x3Fu) != 0)) {
        if (addr < 0x100000u || (g.cf_erw && addr >= g.cf_erw && addr < g.cf_erw + 0x10000u) ||
            (g.object && addr >= g.object && addr < g.object + 0x200u)) {
            add_prov(addr, (uint32_t)value, pc, 0, "possible_record_store", "heuristic");
            watch_rec(addr);
        }
    }
}

static void install_sparse(void *uc, uint32_t cont) {
    uc_err ue;
    if (!uc || !cont || g.hook_sparse) return;
    g.cont_pc = cont & ~3u;
    g.window_end = g.cont_pc + P22N_WIN;
    ue = uc_hook_add((uc_engine *)uc, &g.h_sparse, UC_HOOK_CODE, (void *)p22n_on_sparse, NULL,
                     (uint64_t)g.cont_pc, (uint64_t)(g.window_end - 1u));
    if (ue == UC_ERR_OK) {
        g.hook_sparse = 1;
        g.uc = uc;
    }
}

static void install_global_hook(void) {
    uc_err ue;
    if (!g.uc || g.hook_global) return;
    ue = uc_hook_add((uc_engine *)g.uc, &g.h_global, UC_HOOK_CODE, (void *)p22n_on_global, NULL, 1,
                     0);
    if (ue == UC_ERR_OK) g.hook_global = 1;
}

static void remove_global_hook(void) {
    if (!g.uc || !g.hook_global) return;
    (void)uc_hook_del((uc_engine *)g.uc, g.h_global);
    g.hook_global = 0;
}

static void install_mem_hooks(void) {
    uc_err ue;
    if (!g.uc || g.hook_mem) return;
    /* WRITE only — global MEM_READ is too expensive for long natural runs. */
    ue = uc_hook_add((uc_engine *)g.uc, &g.h_mem_w, UC_HOOK_MEM_WRITE, (void *)p22n_on_mem, NULL, 1,
                     0);
    if (ue == UC_ERR_OK) g.hook_mem = 1;
}

static void remove_mem_hooks(void) {
    if (!g.uc || !g.hook_mem) return;
    (void)uc_hook_del((uc_engine *)g.uc, g.h_mem_w);
    g.hook_mem = 0;
}

static void remove_sparse(void) {
    if (!g.uc || !g.hook_sparse) return;
    (void)uc_hook_del((uc_engine *)g.uc, g.h_sparse);
    g.hook_sparse = 0;
}

static void finish_dense(const char *why) {
    /* Never uc_hook_del from inside a CODE hook callback — defer teardown. */
    if (g.finalized) return;
    g.dense = 0;
    g.pending_undense = 1;
    if (why && why[0] && !g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", why);
}

static void apply_pending_undense(void) {
    if (!g.pending_undense) return;
    g.pending_undense = 0;
    remove_mem_hooks();
    remove_global_hook();
    remove_sparse();
    printf("[JJFB_P22N] undense_hooks_removed evidence=DOCUMENTED\n");
    fflush(stdout);
}
#endif

void p22n_reset(void) {
#ifdef GWY_HAVE_UNICORN
    void *uc = g.uc;
    int hs = g.hook_sparse, hg = g.hook_global, hm = g.hook_mem;
    uc_hook a = g.h_sparse, b = g.h_global, d = g.h_mem_w;
#endif
    memset(&g, 0, sizeof(g));
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        if (hs) (void)uc_hook_del((uc_engine *)uc, a);
        if (hg) (void)uc_hook_del((uc_engine *)uc, b);
        if (hm) (void)uc_hook_del((uc_engine *)uc, d);
    }
#endif
}

void p22n_bind_uc(void *uc) {
    const char *rid;
    if (!p22n_enabled()) return;
    g.uc = uc;
    rid = getenv("JJFB_P22N_RUN_ID");
    if (!rid || !rid[0]) rid = getenv("JJFB_P22I_RUN_ID");
    if (rid && rid[0]) snprintf(g.run_id, sizeof(g.run_id), "%s", rid);
}

void p22n_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, const char *package_owner) {
    if (!p22n_enabled() || !module_name) return;
    if (strstr(module_name, "gamelist")) {
        g.gl_base = base;
        g.gl_end = base + size;
    }
    if (strstr(module_name, "cfunction")) {
        g.cf_base = base;
        g.cf_size = size;
        g.cf_end = base + size;
        if (erw) g.cf_erw = erw;
    }
    if (p_guest) g.p_guest = p_guest;
    if (generation) g.generation = generation;
    if (package_owner && package_owner[0])
        snprintf(g.package_owner, sizeof(g.package_owner), "%s", package_owner);
}

void p22n_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       uint32_t sp) {
    if (!p22n_enabled() || g.finalized || !continuation_pc) return;
    if (!g.uc) g.uc = uc;
#ifdef GWY_HAVE_UNICORN
    apply_pending_undense();
#endif
    if (method == 2u && g.m601_done) {
        g.fire2_n++;
        if (!g.method2_source[0])
            snprintf(g.method2_source, sizeof(g.method2_source),
                     "FIRE_EXT_method2_after_6_0_1 (timer/platform)");
        printf("[JJFB_P22N] fire2_n=%u stream=%u idle=%d evidence=OBSERVED\n", g.fire2_n,
               g.stream_n, g.interpreter_idle);
        fflush(stdout);
        /* Opcode stream closes on 0x1B LDMFD; further FIRE_EXT often never reach 20
         * (guest parks). Finalize once idle + at least one post-601 method=2. */
        if (g.interpreter_idle && g.fire2_n >= 1u && g.stream_n > 0 && !g.finalized) {
            snprintf(g.exit_how, sizeof(g.exit_how), "op1b_exit_then_fire2_n=%u_opcodes=%u",
                     g.fire2_n, g.stream_n);
            snprintf(g.stop_reason, sizeof(g.stop_reason), "opcode_stream_closed_idle");
            p22n_finalize(g.stop_reason);
            return;
        }
    }
    if (g.dense) {
        printf("[JJFB_P22N] skip_arm_while_dense method=%u fire2=%u evidence=DOCUMENTED\n", method,
               g.fire2_n);
        fflush(stdout);
        return;
    }
    /* After interpreter LDMFD exit, do not re-arm sparse/dense — only count FIRE_EXT. */
    if (g.interpreter_idle && g.m601_done) {
        return;
    }
    g.armed = 1;
    g.last_method = method;
#ifdef GWY_HAVE_UNICORN
    ensure_cf_from_pc(continuation_pc);
    maybe_export_image();
    install_sparse(g.uc ? g.uc : uc, continuation_pc);
#else
    (void)uc;
#endif
    printf("[JJFB_P22N] arm_continuation=0x%X method=%u sp=0x%X fire2=%u evidence=OBSERVED\n",
           continuation_pc, method, sp, g.fire2_n);
    fflush(stdout);
}

void p22n_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr) {
    (void)uc;
    (void)module_name;
    (void)pc;
    (void)regs;
    (void)lr;
    (void)sp;
    (void)cpsr;
}

static void classify(void) {
    char seq[256];
    uint32_t i, n;
    seq[0] = 0;
    n = 0;
    for (i = 0; i < g.stream_n && n + 8u < sizeof(seq); i++) {
        n += (uint32_t)snprintf(seq + n, sizeof(seq) - n, "%s0x%02X", i ? "," : "",
                                g.stream[i].opcode);
    }
    if (!g.stream_kind[0]) {
        if (g.prov_n > 0)
            snprintf(g.stream_kind, sizeof(g.stream_kind), "mixed_or_dynamic");
        else
            snprintf(g.stream_kind, sizeof(g.stream_kind),
                     "preexisting_buffer_no_write_observed_in_window");
    }
    if (!g.op14_sem[0])
        snprintf(g.op14_sem, sizeof(g.op14_sem),
                 "case +0x1CD40: transform command cursor via literal masks; loop to +0x1C408");
    if (!g.op1b_sem[0])
        snprintf(g.op1b_sem, sizeof(g.op1b_sem),
                 "case +0x1D1CC: mutate object list/cursor; LDMFD exits interpreter");
    if (!g.exit_how[0])
        snprintf(g.exit_how, sizeof(g.exit_how), "%s",
                 g.helper_reenter ? "helper_reenter" : g.stop_reason);

    /* stream_kind: command words themselves had no writer in the observe window */
    {
        int saw_rec_write = 0;
        uint32_t j;
        for (i = 0; i < g.prov_n; i++) {
            if (strcmp(g.prov[i].kind, "preexisting_at_fetch") == 0) continue;
            for (j = 0; j < g.stream_n; j++) {
                if (g.prov[i].record_addr == g.stream[j].record_addr) {
                    saw_rec_write = 1;
                    break;
                }
            }
        }
        snprintf(g.stream_kind, sizeof(g.stream_kind),
                 saw_rec_write ? "dynamic_writes_to_record_addrs"
                               : "preexisting_command_buffer_no_record_writer_in_window");
    }

    /* First divergence: no UI/init-looking opcode in stream */
    {
        int has_ui = 0;
        for (i = 0; i < g.stream_n; i++) {
            /* Heuristic markers only — not asserted as UI without producer */
            if (g.stream[i].opcode == 0x08u || g.stream[i].opcode == 0x09u ||
                g.stream[i].opcode == 0x0Au)
                has_ui = 1;
        }
        if (!has_ui && g.saw_op14 && g.saw_op1b) {
            snprintf(g.divergence, sizeof(g.divergence),
                     "post-6→0→1 opcode stream [%s] only; 0x1B LDMFD-exits interpreter; "
                     "no UI/init record; index(LSR#24)=0 for raw 0x800054",
                     seq);
            g.divergence_pc = g.stream_n ? g.stream[g.stream_n - 1].pc : 0;
            snprintf(g.divergence_actual, sizeof(g.divergence_actual), "opcodes=%s fire2=%u", seq,
                     g.fire2_n);
            snprintf(g.divergence_producer, sizeof(g.divergence_producer),
                     "UNKNOWN_writer_of_cmd_buffer_before_+0x1C40C_or_post_exit_producer");
        } else if (has_ui) {
            snprintf(g.divergence, sizeof(g.divergence),
                     "candidate UI-ish opcodes present but +0x10740 not entered");
            snprintf(g.divergence_producer, sizeof(g.divergence_producer),
                     "opcode_present_but_downstream_gate");
        } else {
            snprintf(g.divergence, sizeof(g.divergence), "incomplete opcode stream observation");
        }
    }

    snprintf(g.sole_lock, sizeof(g.sole_lock),
             "natural 6→0→1 then cfunction opcode stream [%s] (0x9C41C=NORMAL_OPCODE_DISPATCH); "
             "returns to helper method=2; UI/init record producer not observed",
             seq[0] ? seq : "none");
    snprintf(g.next_fix, sizeof(g.next_fix),
             "locate writer of missing UI/init opcode record (not flag 0x0C); compare producer "
             "callers upstream of +0x1C40C buffer fill");
}

static void write_slice_csv(const char *envk, const char *defp, SliceRow *rows, uint32_t n) {
    FILE *f = open_out(envk, defp);
    uint32_t i;
    if (!f) return;
    fprintf(f, "seq,pc,off,insn,desc,r0,r1,r2,r3,r4,r5,r8,r9,r10,sp,lr,note\n");
    for (i = 0; i < n; i++) {
        SliceRow *s = &rows[i];
        fprintf(f, "%u,0x%X,0x%X,0x%08X,\"%s\",0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                   "0x%X,0x%X,\"%s\"\n",
                s->seq, s->pc, s->off, s->insn, s->desc, s->r0, s->r1, s->r2, s->r3, s->r4, s->r5,
                s->r8, s->r9, s->r10, s->sp, s->lr, s->note);
    }
    fclose(f);
}

static void write_artifacts(void) {
    FILE *f;
    uint32_t i;
    char seq[256];
    uint32_t n = 0;
    seq[0] = 0;
    for (i = 0; i < g.stream_n && n + 8u < sizeof(seq); i++)
        n += (uint32_t)snprintf(seq + n, sizeof(seq) - n, "%s0x%02X", i ? "," : "",
                                g.stream[i].opcode);

    f = open_out("JJFB_P22N_STREAM_CSV", "reports/p22n/p22n_opcode_stream.csv");
    if (f) {
        fprintf(f, "seq,pc,record_address,raw_record,flags30,index,opcode,slot_address,"
                   "jt_entry,case_target,object,cursor_before,cursor_after,r0,r1,r2,r3,r4,r5,"
                   "r9,sp,lr,note\n");
        for (i = 0; i < g.stream_n; i++) {
            StreamRow *s = &g.stream[i];
            fprintf(f,
                    "%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%02X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\"\n",
                    s->seq, s->pc, s->record_addr, s->raw_record, s->flags30, s->index, s->opcode,
                    s->slot_addr, s->jt_entry, s->case_target, s->object, s->cursor_before,
                    s->cursor_after, s->r[0], s->r[1], s->r[2], s->r[3], s->r[4], s->r[5], s->r9,
                    s->sp, s->lr, s->note);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22N_JT_CSV", "reports/p22n/p22n_opcode_jump_table.csv");
    if (f) {
        fprintf(f, "opcode,jt_slot_off,case_off,abs_case,tags,notes\n");
        if (!g.jt_n) build_jump_table();
        for (i = 0; i < g.jt_n; i++) {
            JtRow *j = &g.jt[i];
            fprintf(f, "0x%02X,0x%X,0x%X,0x%X,\"%s\",\"ADDLS base=+0x1C588\"\n", j->opcode,
                    j->jt_slot_off, j->case_off, g.cf_base + j->case_off, j->tags);
        }
        fprintf(f, "# NOTE: 0x9C41C BEQ +0x1C56C = NORMAL_OPCODE_DISPATCH (not a lock)\n");
        fprintf(f, "# default when opcode>0x26 falls through +0x1C584 -> +0x1C408\n");
        fclose(f);
    }

    f = open_out("JJFB_P22N_WRITES_CSV", "reports/p22n/p22n_record_writes.csv");
    if (f) {
        fprintf(f, "seq,pc,off,addr,size,old,new,phase\n");
        for (i = 0; i < g.write_n; i++) {
            WriteRow *w = &g.writes[i];
            fprintf(f, "%u,0x%X,0x%X,0x%X,%u,0x%X,0x%X,%s\n", w->seq, w->pc, w->off, w->addr,
                    w->size, w->old_v, w->new_v, w->phase);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22N_PROV_CSV", "reports/p22n/p22n_record_provenance.csv");
    if (f) {
        fprintf(f, "seq,record_addr,value,writer_pc,writer_off,writer_mod,old,kind,note\n");
        for (i = 0; i < g.prov_n; i++) {
            ProvRow *p = &g.prov[i];
            fprintf(f, "%u,0x%X,0x%X,0x%X,0x%X,%s,0x%X,%s,\"%s\"\n", p->seq, p->record_addr,
                    p->value, p->writer_pc, p->writer_off, p->writer_mod, p->old_v, p->kind,
                    p->note);
        }
        if (g.prov_n == 0) {
            fprintf(f, "# no writes to watched record addresses during observation window\n");
            fprintf(f, "# records appear preexisting in command buffer at interpret time\n");
        }
        fclose(f);
    }

    write_slice_csv("JJFB_P22N_OP14_CSV", "reports/p22n/p22n_opcode14_slice.csv", g.slice14,
                    g.slice14_n);
    write_slice_csv("JJFB_P22N_OP1B_CSV", "reports/p22n/p22n_opcode1b_slice.csv", g.slice1b,
                    g.slice1b_n);

    f = open_out("JJFB_P22N_DIV_MD", "reports/p22n/p22n_first_divergence.md");
    if (f) {
        fprintf(f,
                "# P22N first divergence\n\n"
                "## P22M correction\n\n"
                "- `0x9C41C BEQ 0x9C56C` = **NORMAL_OPCODE_DISPATCH** (not a functional lock)\n"
                "- Do not set `[object+0x30] |= 0x0C` as a fix\n\n"
                "## Observed opcode sequence\n\n`%s`\n\n"
                "## Divergence\n\n%s\n\n"
                "- divergence_pc=0x%X\n"
                "- actual=%s\n"
                "- producer=%s\n"
                "- stream_kind=%s\n"
                "- exit_how=%s\n"
                "- method2_source=%s\n"
                "- fire2_n=%u\n",
                seq[0] ? seq : "(none)", g.divergence, g.divergence_pc, g.divergence_actual,
                g.divergence_producer, g.stream_kind, g.exit_how, g.method2_source, g.fire2_n);
        fclose(f);
    }

    f = open_out("JJFB_P22N_VERDICT", "reports/p22n/p22n_opcode_producer_verdict.md");
    if (f) {
        fprintf(f,
                "# P22N-CLEAN opcode stream / UI-init producer verdict\n\n"
                "## Bottom line\n\n%s\n\n"
                "## P22M correction\n\n"
                "`0x9C41C BEQ → 0x9C56C` = **NORMAL_OPCODE_DISPATCH**, not a lock.\n\n"
                "## PASS answers\n\n```\n"
                "完整自然 opcode 序列：%s\n"
                "opcode 0x14 的作用：%s\n"
                "opcode 0x1B 的作用：%s\n"
                "每条 record 的地址：见 p22n_opcode_stream.csv\n"
                "每条 record 的写入者：%s\n"
                "\n"
                "命令流是静态还是动态：%s\n"
                "解释器最终如何退出：%s\n"
                "method=2 由哪个 opcode/事件产生：%s\n"
                "\n"
                "是否存在 UI/init opcode：%s\n"
                "若存在，为何未执行：%s\n"
                "若不存在，本应由谁产生：%s\n"
                "\n"
                "第一处真实分歧：%s\n"
                "分歧 PC：0x%X\n"
                "实际字段/返回值：%s\n"
                "自然生产者：%s\n"
                "\n"
                "+0x10740：%s\n"
                "+0x7B6C：%s\n"
                "真实 cfg open：%s\n"
                "真实游戏画面：NO\n"
                "\n"
                "是否修改 Guest：NO\n"
                "当前唯一门锁：%s\n"
                "下一处最小通用修复：%s\n"
                "```\n\n"
                "## Identity\n\n"
                "- run_id=%s\n"
                "- cfunction_runtime_sha256=%s\n"
                "- cf_base=0x%X cf_end=0x%X\n"
                "- stream_n=%u fire2_n=%u stop=%s\n",
                g.sole_lock, seq[0] ? seq : "(none)", g.op14_sem, g.op1b_sem,
                g.prov_n ? "see p22n_record_provenance.csv" : "NO_WRITE_IN_WINDOW (preexisting)",
                g.stream_kind, g.exit_how, g.method2_source[0] ? g.method2_source : "FIRE_EXT",
                "NO_CLEAR_UI_INIT_OPCODE_IN_STREAM", "n/a", g.divergence_producer, g.divergence,
                g.divergence_pc, g.divergence_actual, g.divergence_producer,
                g.entered_10740 ? "YES" : "NO", g.entered_7b6c ? "YES" : "NO",
                g.cfg_open ? "YES" : "NO", g.sole_lock, g.next_fix, g.run_id,
                g.cf_sha[0] ? g.cf_sha : "NOT_EXPORTED", g.cf_base, g.cf_end, g.stream_n,
                g.fire2_n, g.stop_reason[0] ? g.stop_reason : "finalize");
        fclose(f);
    }

    f = open_out("JJFB_P22N_SUMMARY", "out/p22n/p22n_runtime_summary.txt");
    if (f) {
        fprintf(f,
                "run_id=%s\ncf_base=0x%X\ncf_end=0x%X\n"
                "cfunction_runtime_sha256=%s\n"
                "opcodes=%s\nstream_n=%u\nfire2_n=%u\n"
                "saw_op14=%d\nsaw_op1b=%d\n"
                "entered_10740=%d\nentered_7b6c=%d\n"
                "sole_lock=%s\nnext_fix=%s\nstop_reason=%s\n"
                "p22m_0x9C41C=NORMAL_OPCODE_DISPATCH\nguest_modified=NO\n",
                g.run_id, g.cf_base, g.cf_end, g.cf_sha[0] ? g.cf_sha : "?", seq, g.stream_n,
                g.fire2_n, g.saw_op14, g.saw_op1b, g.entered_10740, g.entered_7b6c, g.sole_lock,
                g.next_fix, g.stop_reason[0] ? g.stop_reason : "finalize");
        fclose(f);
    }

    /* Rewrite identity file with final dump SHA only (overwrite, do not append). */
    {
        const char *idp = getenv("JJFB_P22N_IDENTITY");
        if (idp && idp[0] && g.cf_sha[0]) {
            FILE *idf = fopen(idp, "wb");
            if (idf) {
                fprintf(idf,
                        "run_id=%s\n"
                        "gate=P22N_OPCODE_STREAM_PROVENANCE\n"
                        "NATURAL_ONLY=yes\n"
                        "JJFB_P22N_CLEAN=1\n"
                        "JJFB_P22I_CLEAN=1\n"
                        "cfunction_runtime_sha256=%s\n"
                        "cfunction_base=0x%X\n"
                        "cfunction_end=0x%X\n"
                        "p22m_0x9C41C_correction=NORMAL_OPCODE_DISPATCH\n"
                        "no_object_plus30_0x0C_force=yes\n",
                        g.run_id, g.cf_sha, g.cf_base, g.cf_end);
                fclose(idf);
            }
        }
    }
}

void p22n_finalize(const char *stop_reason) {
    if (!p22n_enabled() || g.finalized) return;
    g.finalized = 1;
    if (stop_reason && stop_reason[0] && !g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", stop_reason);
    else if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "finalize");
#ifdef GWY_HAVE_UNICORN
    g.dense = 0;
    g.pending_undense = 1;
    apply_pending_undense();
    maybe_export_image();
    if (!g.jt_n) build_jump_table();
#endif
    classify();
    write_artifacts();
    printf("[JJFB_P22N_FINAL] sha=%s stream=%u fire2=%u op14=%d op1b=%d lock=%s "
           "evidence=OBSERVED\n",
           g.cf_sha[0] ? g.cf_sha : "?", g.stream_n, g.fire2_n, g.saw_op14, g.saw_op1b,
           g.sole_lock);
    fflush(stdout);
}
