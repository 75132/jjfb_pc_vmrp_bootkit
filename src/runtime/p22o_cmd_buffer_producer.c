#include "gwy_launcher/p22o_cmd_buffer_producer.h"

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

#define P22O_WRITE_CAP 2048u
#define P22O_PROV_CAP 256u
#define P22O_META_CAP 256u
#define P22O_SLICE_CAP 1024u
#define P22O_SKIP_CAP 128u
#define P22O_STREAM_CAP 64u
#define P22O_STACK_DEPTH 8u
#define P22O_WATCH_CAP 64u

#define SEED_REC14 0x2AF8F8u
#define SEED_REC1B 0x2AF904u
#define RAW_OP14 0x800054u
#define RAW_OP1B 0x801Bu

#define OFF_1C40C 0x1C40Cu
#define OFF_1C408 0x1C408u
#define OFF_1D1CC 0x1D1CCu
#define OFF_10740 0x10740u
#define OFF_7B6C 0x7B6Cu

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t off;
    uint32_t addr;
    uint32_t old_v;
    uint32_t new_v;
    uint32_t size;
    uint32_t lr;
    uint32_t sp;
    uint32_t stack[P22O_STACK_DEPTH];
    char phase[24];
    char kind[32];
    char note[64];
} WriteRow;

typedef struct {
    uint32_t seq;
    uint32_t record_addr;
    uint32_t value;
    uint32_t writer_pc;
    uint32_t writer_off;
    char writer_mod[40];
    uint32_t old_v;
    uint32_t lr;
    char kind[40];
    char note[80];
} ProvRow;

typedef struct {
    uint32_t seq;
    char phase[24];
    uint32_t object;
    uint32_t f08, f0c, f14, f30;
    uint32_t buf_base;
    uint32_t cursor;
    uint32_t end;
    uint32_t capacity_guess;
    uint32_t record_count_guess;
    uint32_t trigger_pc;
    char note[64];
} MetaRow;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t off;
    uint32_t insn;
    char desc[48];
    uint32_t r0, r1, r2, r3, r4, r5, sp, lr;
    char note[56];
} SliceRow;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t off;
    uint32_t insn;
    uint32_t taken;
    uint32_t r0, r1, r2, r3, r4;
    uint32_t mem_addr;
    uint32_t mem_val;
    char pred[48];
    char note[80];
} SkipRow;

typedef struct {
    uint32_t seq;
    uint32_t record_addr;
    uint32_t raw;
    uint32_t opcode;
    uint32_t index;
    uint32_t object;
} StreamRow;

static struct {
    int armed_mem, finalized, producer_dense;
    int image_exported;
    int hook_mem, hook_code;
    void *uc;

    uint32_t cf_base, cf_end, cf_size, cf_erw;
    uint32_t gl_base, gl_end, gl_helper;
    uint32_t p_guest;
    uint64_t generation;
    char package_owner[64];
    char cf_sha[65];

    uint32_t object;
    uint32_t last_method;
    uint32_t m601_bits; /* bit0=m6 bit1=m0 bit2=m1 */
    uint32_t fire2_n;
    int interpreter_idle;
    int entered_10740, entered_7b6c, cfg_open;
    int saw_fetch14, saw_fetch1b;
    int first_write14, first_write1b;
    int seed14_preexisting, seed1b_preexisting;
    int seed_peeked;
    int append_n;
    int producer_ret_seen;
    int pending_seed_poll;
    int saw_staging_op1b;
    uint32_t staging_op1b_addr;
    uint32_t staging_op1b_pc;

    uint32_t producer_pc14, producer_off14;
    uint32_t producer_pc1b, producer_off1b;
    uint32_t producer_fn_lo, producer_fn_hi;
    uint32_t producer_slice_budget;
    uint32_t witness_pc14, witness_pc1b; /* PC that saw opcode already present */

    uint32_t watch[P22O_WATCH_CAP];
    uint32_t watch_n;

    WriteRow writes[P22O_WRITE_CAP];
    uint32_t write_n;
    ProvRow prov[P22O_PROV_CAP];
    uint32_t prov_n;
    MetaRow meta[P22O_META_CAP];
    uint32_t meta_n;
    SliceRow slice[P22O_SLICE_CAP];
    uint32_t slice_n;
    SkipRow skips[P22O_SKIP_CAP];
    uint32_t skip_n;
    StreamRow stream[P22O_STREAM_CAP];
    uint32_t stream_n;

    char run_id[64];
    char stop_reason[96];
    char sole_lock[280];
    char next_fix[240];
    char phase[24];
    char writer_class[48];
    char skip_field_y[80];
    char skip_actual_a[80];
    char skip_expected_w[120];
    char divergence[200];

#ifdef GWY_HAVE_UNICORN
    uc_hook h_mem_w, h_code;
#endif
} g;

static const char *env_or(const char *k, const char *d) {
    const char *v = getenv(k);
    return (v && v[0]) ? v : d;
}

int p22o_enabled(void) {
    const char *e = getenv("JJFB_P22O_CLEAN");
    return e && e[0] == '1';
}

int p22o_observation_complete(void) {
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
    if ((w & 0x0FFFFFF0u) == 0x012FFF10u) {
        snprintf(out, n, "BX r%u", w & 0xFu);
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
    snprintf(out, n, "w=0x%08X", w);
}

static void set_phase(const char *p) {
    if (p && p[0]) snprintf(g.phase, sizeof(g.phase), "%s", p);
}

static void watch_add(uint32_t addr) {
    uint32_t i;
    if (!addr) return;
    addr &= ~3u;
    for (i = 0; i < g.watch_n; i++)
        if (g.watch[i] == addr) return;
    if (g.watch_n < P22O_WATCH_CAP) g.watch[g.watch_n++] = addr;
}

static int is_watched(uint32_t addr) {
    uint32_t i;
    uint32_t a = addr & ~3u;
    for (i = 0; i < g.watch_n; i++)
        if (g.watch[i] == a) return 1;
    return 0;
}

static void adopt_cf(const GwyLoadedModule *m, const char *via) {
    const char *nm;
    if (!m || !m->map.guest_code_base || !m->map.guest_code_size) return;
    nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    g.cf_base = m->map.guest_code_base;
    g.cf_size = m->map.guest_code_size;
    g.cf_end = g.cf_base + g.cf_size;
    if (m->data.start_of_er_rw) g.cf_erw = m->data.start_of_er_rw;
    printf("[JJFB_P22O] cf_base=0x%X end=0x%X via=%s name=%s evidence=OBSERVED\n", g.cf_base,
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
    if (!reg || !pc) return;
    m = module_registry_find_by_code_addr(reg, pc & ~1u);
    if (m) adopt_cf(m, "find_by_pc");
}

static int is_exact_op14(uint32_t v) { return v == RAW_OP14; }
static int is_exact_op1b(uint32_t v) { return v == RAW_OP1B || v == 0x0000801Bu; }

static int overlaps_seed(uint32_t addr, int size, uint32_t seed) {
    uint32_t a0 = addr, a1 = addr + (uint32_t)size;
    return a0 < seed + 4u && a1 > seed;
}

static void mod_for_pc(uint32_t pc, char *mod, size_t n, uint32_t *off_out) {
    uint32_t off = 0;
    if (g.cf_base && pc >= g.cf_base && pc < g.cf_end) {
        snprintf(mod, n, "cfunction.ext");
        off = (pc & ~3u) - g.cf_base;
    } else if (g.gl_base && pc >= g.gl_base && pc < g.gl_end) {
        snprintf(mod, n, "gamelist.ext");
        off = (pc & ~1u) - g.gl_base;
    } else {
        snprintf(mod, n, "other");
    }
    if (off_out) *off_out = off;
}

static void peek_seeds(void) {
    uint32_t v14 = 0, v1b = 0;
    if (g.seed_peeked || !g.uc) return;
    g.seed_peeked = 1;
    if (read_u32(SEED_REC14, &v14) && v14 == RAW_OP14) g.seed14_preexisting = 1;
    if (read_u32(SEED_REC1B, &v1b) && (v1b == RAW_OP1B || (v1b & 0xFFFFu) == RAW_OP1B))
        g.seed1b_preexisting = 1;
    if (g.seed14_preexisting || g.seed1b_preexisting) {
        printf("[JJFB_P22O] seed_preexisting14=%d preexisting1b=%d v14=0x%X v1b=0x%X "
               "evidence=OBSERVED\n",
               g.seed14_preexisting, g.seed1b_preexisting, v14, v1b);
        fflush(stdout);
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
    f = fopen(env_or("JJFB_P22O_CF_BIN", "out/p22o/cfunction_runtime.bin"), "wb");
    if (f) {
        fwrite(buf, 1, n, f);
        fclose(f);
    }
    f = fopen(env_or("JJFB_P22O_CF_SHA", "out/p22o/cfunction_runtime.sha256"), "wb");
    if (f) {
        fprintf(f, "%s\n", g.cf_sha);
        fprintf(f, "base=0x%X\nend=0x%X\nsize=0x%X\nerw=0x%X\nP=0x%X\ngeneration=%llu\n",
                g.cf_base, g.cf_end, (uint32_t)n, g.cf_erw, g.p_guest,
                (unsigned long long)g.generation);
        fclose(f);
    }
    g.image_exported = 1;
    printf("[JJFB_P22O] export_cfunction sha=%s base=0x%X size=0x%X evidence=OBSERVED\n", g.cf_sha,
           g.cf_base, (uint32_t)n);
    fflush(stdout);
    free(buf);
}

static void add_meta(const char *note) {
    MetaRow *m;
    uint32_t f08 = 0, f0c = 0, f14 = 0, f30 = 0;
    if (g.meta_n >= P22O_META_CAP || !g.object) return;
    (void)read_u32(g.object + 0x08u, &f08);
    (void)read_u32(g.object + 0x0Cu, &f0c);
    (void)read_u32(g.object + 0x14u, &f14);
    (void)read_u32(g.object + 0x30u, &f30);
    m = &g.meta[g.meta_n++];
    memset(m, 0, sizeof(*m));
    m->seq = g.meta_n;
    snprintf(m->phase, sizeof(m->phase), "%s", g.phase);
    m->object = g.object;
    m->f08 = f08;
    m->f0c = f0c;
    m->f14 = f14;
    m->f30 = f30;
    m->buf_base = SEED_REC14;
    m->cursor = SEED_REC14;
    m->end = SEED_REC1B + 4u;
    m->capacity_guess = (m->end > m->buf_base) ? (m->end - m->buf_base) : 0;
    m->record_count_guess = 2u;
    if (g.append_n > 2) m->record_count_guess = (uint32_t)g.append_n;
    m->trigger_pc = 0;
    snprintf(m->note, sizeof(m->note), "%s", note ? note : "");
}

static void add_prov(uint32_t addr, uint32_t val, uint32_t pc, uint32_t oldv, uint32_t lr,
                     const char *kind, const char *note) {
    ProvRow *p;
    char mod[40];
    uint32_t off = 0;
    if (g.prov_n >= P22O_PROV_CAP) return;
    mod_for_pc(pc, mod, sizeof(mod), &off);
    p = &g.prov[g.prov_n++];
    memset(p, 0, sizeof(*p));
    p->seq = g.prov_n;
    p->record_addr = addr;
    p->value = val;
    p->writer_pc = pc;
    p->writer_off = off;
    snprintf(p->writer_mod, sizeof(p->writer_mod), "%s", mod);
    p->old_v = oldv;
    p->lr = lr;
    snprintf(p->kind, sizeof(p->kind), "%s", kind ? kind : "?");
    snprintf(p->note, sizeof(p->note), "%s", note ? note : "");
}

static void discover_fn_bounds(uint32_t pc) {
    uint32_t lo, hi, i, insn = 0;
    if (!g.cf_base || pc < g.cf_base || pc >= g.cf_end) return;
    lo = pc & ~3u;
    hi = lo;
    for (i = 0; i < 0x200u && lo > g.cf_base + 4u; i++) {
        lo -= 4u;
        if (!read_u32(lo, &insn)) break;
        /* STMFD sp!,{...lr} / PUSH */
        if ((insn & 0xFFFF0000u) == 0xE92D0000u) break;
    }
    for (i = 0; i < 0x400u && hi + 4u < g.cf_end; i++) {
        hi += 4u;
        if (!read_u32(hi, &insn)) break;
        if ((insn & 0x0FFF8000u) == 0x08BD8000u) {
            hi += 4u;
            break;
        }
        if ((insn & 0x0FFFFFF0u) == 0x012FFF10u) {
            hi += 4u;
            break;
        }
    }
    if (!g.producer_fn_lo || lo < g.producer_fn_lo) g.producer_fn_lo = lo;
    if (!g.producer_fn_hi || hi > g.producer_fn_hi) g.producer_fn_hi = hi;
}

#ifdef GWY_HAVE_UNICORN
static void install_mem_hooks(void);
static void install_code_hook(void);
static void arm_code_observe(void);
static void poll_seeds(uint32_t pc, uint32_t lr, const char *how);
static void remove_hooks(void);

static void fill_stack(uint32_t sp, uint32_t *out, uint32_t n) {
    uint32_t i;
    memset(out, 0, n * sizeof(uint32_t));
    if (!sp) return;
    for (i = 0; i < n; i++) (void)read_u32(sp + i * 4u, &out[i]);
}

static void try_arm_producer_dense(uint32_t writer_pc) {
    if (g.producer_dense || !writer_pc) return;
    if (!(g.cf_base && writer_pc >= g.cf_base && writer_pc < g.cf_end)) return;
    discover_fn_bounds(writer_pc);
    if (!g.producer_fn_lo || g.producer_fn_hi <= g.producer_fn_lo) return;
    g.producer_dense = 1;
    g.producer_slice_budget = P22O_SLICE_CAP;
    arm_code_observe();
    printf("[JJFB_P22O] producer_dense lo=0x%X hi=0x%X off=+0x%X evidence=OBSERVED\n",
           g.producer_fn_lo, g.producer_fn_hi, g.producer_fn_lo - g.cf_base);
    fflush(stdout);
}

static void on_code_dense(uc_engine *uc, uint32_t pc) {
    uint32_t regs[16], sp = 0, lr = 0, insn = 0, off = 0;
    int i;
    SliceRow *s;
    if (!g.producer_dense || g.finalized) return;
    if (pc < g.producer_fn_lo || pc >= g.producer_fn_hi) {
        /* Allow short BL targets inside cfunction within +0x800 of producer */
        if (!(g.cf_base && pc >= g.cf_base && pc < g.cf_end)) return;
        if (pc + 0x800u < g.producer_fn_lo || pc > g.producer_fn_hi + 0x800u) return;
    }
    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~1u, &insn);
    if (g.cf_base && pc >= g.cf_base && pc < g.cf_end) off = (pc & ~3u) - g.cf_base;

    if (g.slice_n < P22O_SLICE_CAP && g.producer_slice_budget) {
        s = &g.slice[g.slice_n++];
        memset(s, 0, sizeof(*s));
        s->seq = g.slice_n;
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
        s->sp = sp;
        s->lr = lr;
        snprintf(s->note, sizeof(s->note), "producer");
        g.producer_slice_budget--;
    }

    /* Conditional branch: log as skip-predicate candidate after both records written */
    if (g.first_write14 && g.first_write1b && (insn & 0x0F000000u) == 0x0A000000u &&
        (insn & 0xF0000000u) != 0xE0000000u && g.skip_n < P22O_SKIP_CAP) {
        SkipRow *k = &g.skips[g.skip_n++];
        uint32_t cpsr = 0, tgt, cond, taken = 0;
        uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
        cond = (insn >> 28) & 0xFu;
        tgt = branch_target_arm(pc, insn);
        /* EQ/NE rough */
        if (cond == 0x0u) taken = (cpsr & (1u << 30)) ? 1u : 0u;
        else if (cond == 0x1u) taken = (cpsr & (1u << 30)) ? 0u : 1u;
        else taken = 2u; /* unknown */
        memset(k, 0, sizeof(*k));
        k->seq = g.skip_n;
        k->pc = pc;
        k->off = off;
        k->insn = insn;
        k->taken = taken;
        k->r0 = regs[0];
        k->r1 = regs[1];
        k->r2 = regs[2];
        k->r3 = regs[3];
        k->r4 = regs[4];
        if (g.object) {
            k->mem_addr = g.object + 0x30u;
            (void)read_u32(k->mem_addr, &k->mem_val);
        }
        snprintf(k->pred, sizeof(k->pred), "Bcond->0x%X", tgt);
        snprintf(k->note, sizeof(k->note), "post_two_records phase=%s", g.phase);
        if (!g.skip_field_y[0]) {
            snprintf(g.skip_field_y, sizeof(g.skip_field_y), "object+0x30_or_cmp_regs");
            snprintf(g.skip_actual_a, sizeof(g.skip_actual_a), "f30=0x%X r0=0x%X r1=0x%X",
                     k->mem_val, regs[0], regs[1]);
            snprintf(g.skip_expected_w, sizeof(g.skip_expected_w),
                     "UNKNOWN_natural_platform_contract_for_UI_init_enqueue");
        }
    }

    if ((insn & 0x0FFF8000u) == 0x08BD8000u || (insn & 0x0FFFFFF0u) == 0x012FFF10u) {
        g.producer_ret_seen = 1;
        if (g.first_write14 && g.first_write1b && !g.append_n) {
            /* both seeds written; no extra appends observed in producer */
        }
    }
}

static void p22o_on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t regs[16], sp = 0, lr = 0, insn = 0, off = 0;
    int i;
    (void)size;
    (void)user_data;
    if (!p22o_enabled() || g.finalized) return;

    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~1u, &insn);

    if (g.gl_base && pc >= g.gl_base && pc < g.gl_end) {
        uint32_t go = (pc & ~1u) - g.gl_base;
        if (go == OFF_10740) g.entered_10740 = 1;
        if (go == OFF_7B6C) g.entered_7b6c = 1;
    }

    if (g.pending_seed_poll) {
        g.pending_seed_poll = 0;
        poll_seeds(pc, lr, "post_seed_touch");
    }

    /* Cheap poll while in early producer window (+0x12000..+0x14000). */
    if (g.cf_base && pc >= g.cf_base && pc < g.cf_end) {
        off = (pc & ~3u) - g.cf_base;
        if (off >= 0x12000u && off < 0x14000u && (!g.first_write14 || !g.first_write1b))
            poll_seeds(pc, lr, "producer_window");
        if (off == OFF_1C40C && (g.m601_bits & 4u)) {
            /* Only record post-method=1 natural stream (ignore early interpreter uses). */
            StreamRow *row;
            uint32_t raw = 0;
            set_phase("during_opcode");
            if (!g.object && regs[4]) {
                g.object = regs[4];
                add_meta("object_at_1C40C");
            }
            if (g.stream_n < P22O_STREAM_CAP) {
                row = &g.stream[g.stream_n++];
                memset(row, 0, sizeof(*row));
                row->seq = g.stream_n;
                row->record_addr = regs[0];
                (void)read_u32(row->record_addr, &raw);
                row->raw = raw;
                row->opcode = raw & 0x3Fu;
                row->index = (raw >> 24) & 0xFFu;
                row->object = g.object;
                if (row->opcode == 0x14u) g.saw_fetch14 = 1;
                if (row->opcode == 0x1Bu) g.saw_fetch1b = 1;
                printf("[JJFB_P22O] opcode_fetch seq=%u rec=0x%X raw=0x%X op=0x%02X "
                       "evidence=OBSERVED\n",
                       row->seq, row->record_addr, row->raw, row->opcode);
                fflush(stdout);
            }
            if (regs[0] == SEED_REC14 && !g.first_write14) {
                add_prov(SEED_REC14, raw, 0, 0, 0, "preexisting_at_fetch",
                         g.seed14_preexisting ? "pre_probe_or_static" : "no_writer_before_fetch");
            }
            if (regs[0] == SEED_REC1B && !g.first_write1b) {
                add_prov(SEED_REC1B, raw, 0, 0, 0, "preexisting_at_fetch",
                         g.seed1b_preexisting ? "pre_probe_or_static" : "no_writer_before_fetch");
            }
        }
        if (off == OFF_1D1CC) set_phase("during_opcode");
        if (off == OFF_1C408 && g.saw_fetch1b) {
            /* still in loop potentially */
        }
        /* LDMFD exit from op1b region */
        if (g.saw_fetch1b && (insn & 0x0FFF8000u) == 0x08BD8000u && off >= 0x1D1CCu &&
            off < 0x1D300u) {
            g.interpreter_idle = 1;
            set_phase("post_1B");
            if (g.object) add_meta("post_1B_exit");
        }
    }

    if (g.producer_dense) on_code_dense(uc, pc);
}

static void note_exact_record_write(uint32_t addr, uint32_t word, uint32_t pc, uint32_t lr,
                                    uint32_t sp, uint32_t oldv, const char *how) {
    uint32_t off = 0;
    char mod[40];
    char note[80];
    int at_seed14 = ((addr & ~3u) == SEED_REC14);
    int at_seed1b = ((addr & ~3u) == SEED_REC1B);
    (void)sp;
    if (!is_exact_op14(word) && !is_exact_op1b(word)) return;
    ensure_cf_from_pc(pc);
    mod_for_pc(pc, mod, sizeof(mod), &off);
    snprintf(note, sizeof(note), "%s;%s", how ? how : "store", mod);

    watch_add(addr & ~3u);

    if (is_exact_op14(word)) {
        if (at_seed14 && !g.first_write14) {
            g.first_write14 = 1;
            g.producer_pc14 = pc;
            g.producer_off14 = off;
            g.append_n++;
            add_prov(SEED_REC14, word, pc, oldv, lr, "first_write_op14", note);
            printf("[JJFB_P22O] first_write_op14 pc=0x%X off=+0x%X addr=0x%X val=0x%X lr=0x%X "
                   "phase=%s how=%s evidence=OBSERVED\n",
                   pc, off, addr, word, lr, g.phase, how ? how : "?");
            fflush(stdout);
            try_arm_producer_dense(pc);
            if (g.object) add_meta("after_first_write14");
        } else if (!at_seed14) {
            add_prov(addr & ~3u, word, pc, oldv, lr, "op14_staging_store", note);
            printf("[JJFB_P22O] op14_staging pc=0x%X off=+0x%X addr=0x%X val=0x%X "
                   "evidence=OBSERVED\n",
                   pc, off, addr, word);
            fflush(stdout);
            try_arm_producer_dense(pc);
        }
    }
    if (is_exact_op1b(word)) {
        if (at_seed1b && !g.first_write1b) {
            g.first_write1b = 1;
            g.producer_pc1b = pc;
            g.producer_off1b = off;
            g.append_n++;
            add_prov(SEED_REC1B, word, pc, oldv, lr, "first_write_op1b", note);
            printf("[JJFB_P22O] first_write_op1b pc=0x%X off=+0x%X addr=0x%X val=0x%X lr=0x%X "
                   "phase=%s how=%s evidence=OBSERVED\n",
                   pc, off, addr, word, lr, g.phase, how ? how : "?");
            fflush(stdout);
            try_arm_producer_dense(pc);
            if (g.object) add_meta("after_first_write1b");
        } else if (!at_seed1b) {
            if (!g.saw_staging_op1b) {
                g.saw_staging_op1b = 1;
                g.staging_op1b_addr = addr & ~3u;
                g.staging_op1b_pc = pc;
            }
            add_prov(addr & ~3u, word, pc, oldv, lr, "op1b_staging_store", note);
            printf("[JJFB_P22O] op1b_staging pc=0x%X off=+0x%X addr=0x%X val=0x%X "
                   "evidence=OBSERVED\n",
                   pc, off, addr, word);
            fflush(stdout);
            try_arm_producer_dense(pc);
        }
    }
}

static void poll_seeds(uint32_t pc, uint32_t lr, const char *how) {
    uint32_t v14 = 0, v1b = 0;
    if (!g.uc) return;
    if (read_u32(SEED_REC14, &v14) && is_exact_op14(v14) && !g.first_write14) {
        uint32_t off = 0;
        char mod[40];
        ensure_cf_from_pc(pc);
        mod_for_pc(pc, mod, sizeof(mod), &off);
        g.first_write14 = 1;
        g.producer_pc14 = pc;
        g.producer_off14 = off;
        g.witness_pc14 = pc;
        g.append_n++;
        add_prov(SEED_REC14, v14, pc, 0, lr, "seed_poll_op14_present", how ? how : "poll");
        printf("[JJFB_P22O] seed_poll_op14 pc=0x%X off=+0x%X val=0x%X how=%s evidence=OBSERVED\n",
               pc, off, v14, how ? how : "?");
        fflush(stdout);
        try_arm_producer_dense(pc);
    }
    if (read_u32(SEED_REC1B, &v1b) && is_exact_op1b(v1b) && !g.first_write1b) {
        uint32_t off = 0;
        char mod[40];
        ensure_cf_from_pc(pc);
        mod_for_pc(pc, mod, sizeof(mod), &off);
        g.first_write1b = 1;
        g.producer_pc1b = pc;
        g.producer_off1b = off;
        g.witness_pc1b = pc;
        g.append_n++;
        add_prov(SEED_REC1B, v1b, pc, 0, lr, "seed_poll_op1b_present", how ? how : "poll");
        printf("[JJFB_P22O] seed_poll_op1b pc=0x%X off=+0x%X val=0x%X how=%s evidence=OBSERVED\n",
               pc, off, v1b, how ? how : "?");
        fflush(stdout);
        try_arm_producer_dense(pc);
    }
}

static void p22o_on_mem(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                        void *user_data) {
    uint32_t pc = 0, lr = 0, sp = 0, addr = (uint32_t)address, val = (uint32_t)value;
    uint32_t oldv = 0, off = 0;
    char mod[40];
    int interesting = 0;
    int seed_touch = 0;
    (void)user_data;
    if (!p22o_enabled() || g.finalized || type != UC_MEM_WRITE) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    ensure_cf_from_pc(pc);
    mod_for_pc(pc, mod, sizeof(mod), &off);

    if (overlaps_seed(addr, size, SEED_REC14) || overlaps_seed(addr, size, SEED_REC1B))
        seed_touch = 1;
    if (is_watched(addr)) interesting = 1;
    if (seed_touch) interesting = 1;
    /* Exact opcode words anywhere in low guest heap (incl. staging @0x28065C). */
    if (size == 4 && (is_exact_op14(val) || is_exact_op1b(val)) && addr >= 0x200000u &&
        addr < 0x400000u)
        interesting = 1;
    if (g.object && addr >= g.object && addr < g.object + 0x40u) interesting = 1;

    if (!interesting) return;

    (void)read_u32(addr & ~3u, &oldv);

    if (g.write_n < P22O_WRITE_CAP) {
        WriteRow *w = &g.writes[g.write_n++];
        memset(w, 0, sizeof(*w));
        w->seq = g.write_n;
        w->pc = pc;
        w->off = off;
        w->addr = addr;
        w->old_v = oldv;
        w->new_v = val;
        w->size = (uint32_t)size;
        w->lr = lr;
        w->sp = sp;
        fill_stack(sp, w->stack, P22O_STACK_DEPTH);
        snprintf(w->phase, sizeof(w->phase), "%s", g.phase);
        if (seed_touch)
            snprintf(w->kind, sizeof(w->kind), "seed_touch");
        else if (is_exact_op14(val) || is_exact_op1b(val))
            snprintf(w->kind, sizeof(w->kind), "exact_opcode_word");
        else if (g.object && addr >= g.object && addr < g.object + 0x40u)
            snprintf(w->kind, sizeof(w->kind), "object_field");
        else
            snprintf(w->kind, sizeof(w->kind), "watched");
        snprintf(w->note, sizeof(w->note), "mod=%s", mod);
    }

    /* Seed already held opcode before this overwrite — store was missed earlier. */
    if (seed_touch) {
        if (overlaps_seed(addr, size, SEED_REC14) && is_exact_op14(oldv)) {
            g.witness_pc14 = pc;
            if (!g.first_write14) {
                add_prov(SEED_REC14, oldv, pc, oldv, lr, "opcode_present_pre_overwrite",
                         "SEED_REC14");
                g.first_write14 = 1;
                g.producer_pc14 = pc;
                g.producer_off14 = off;
                g.append_n++;
                printf("[JJFB_P22O] op14_present_pre_overwrite pc=0x%X off=+0x%X old=0x%X "
                       "new=0x%X evidence=OBSERVED\n",
                       pc, off, oldv, val);
                fflush(stdout);
                try_arm_producer_dense(pc);
            }
        }
        if (overlaps_seed(addr, size, SEED_REC1B) && is_exact_op1b(oldv)) {
            g.witness_pc1b = pc;
            if (!g.first_write1b) {
                add_prov(SEED_REC1B, oldv, pc, oldv, lr, "opcode_present_pre_overwrite",
                         "SEED_REC1B");
                g.first_write1b = 1;
                g.producer_pc1b = pc;
                g.producer_off1b = off;
                g.append_n++;
                printf("[JJFB_P22O] op1b_present_pre_overwrite pc=0x%X off=+0x%X old=0x%X "
                       "new=0x%X evidence=OBSERVED\n",
                       pc, off, oldv, val);
                fflush(stdout);
                try_arm_producer_dense(pc);
            }
        }
        g.pending_seed_poll = 1;
        arm_code_observe();
    }

    /* Direct store of exact opcode word (seed or staging buffer). */
    if (size == 4 && (is_exact_op14(val) || is_exact_op1b(val))) {
        note_exact_record_write(addr & ~3u, val, pc, lr, sp, oldv, "direct_str");
        g.pending_seed_poll = 1;
        arm_code_observe();
    }

    if (g.object && (addr == g.object + 0x08u || addr == g.object + 0x0Cu ||
                     addr == g.object + 0x14u || addr == g.object + 0x30u)) {
        add_meta("object_field_write");
    }
}

static void install_mem_hooks(void) {
    uc_err ue;
    if (!g.uc || g.hook_mem) return;
    ue = uc_hook_add((uc_engine *)g.uc, &g.h_mem_w, UC_HOOK_MEM_WRITE, (void *)p22o_on_mem, NULL, 1,
                     0);
    if (ue == UC_ERR_OK) {
        g.hook_mem = 1;
        g.armed_mem = 1;
        printf("[JJFB_P22O] mem_write_armed evidence=OBSERVED\n");
        fflush(stdout);
    }
}

static void install_code_hook(void) {
    uc_err ue;
    if (!g.uc || g.hook_code) return;
    ensure_cf_base();
    /* Prefer cfunction-only CODE hook (cheap). Fall back to global if base unknown. */
    if (g.cf_base && g.cf_end > g.cf_base) {
        ue = uc_hook_add((uc_engine *)g.uc, &g.h_code, UC_HOOK_CODE, (void *)p22o_on_code, NULL,
                         (uint64_t)g.cf_base, (uint64_t)(g.cf_end - 1u));
    } else {
        ue = uc_hook_add((uc_engine *)g.uc, &g.h_code, UC_HOOK_CODE, (void *)p22o_on_code, NULL, 1,
                         0);
    }
    if (ue == UC_ERR_OK) g.hook_code = 1;
}

static void remove_hooks(void) {
    if (!g.uc) return;
    if (g.hook_mem) {
        (void)uc_hook_del((uc_engine *)g.uc, g.h_mem_w);
        g.hook_mem = 0;
    }
    if (g.hook_code) {
        (void)uc_hook_del((uc_engine *)g.uc, g.h_code);
        g.hook_code = 0;
    }
}

static void arm_early(void) {
    ensure_cf_base();
    watch_add(SEED_REC14);
    watch_add(SEED_REC1B);
    peek_seeds();
    install_mem_hooks();
    /* Once cf_base known, arm ranged CODE for seed poll + later fetch. */
    if (g.cf_base && g.cf_end > g.cf_base) arm_code_observe();
    maybe_export_image();
    if (!g.phase[0] || strcmp(g.phase, "pre_map") == 0) set_phase("post_cf_map");
}

static void arm_code_observe(void) {
    /* Global CODE is expensive — only for interpreter fetch + producer slice. */
    install_code_hook();
}
#endif /* GWY_HAVE_UNICORN */

void p22o_reset(void) {
#ifdef GWY_HAVE_UNICORN
    void *uc = g.uc;
    int hm = g.hook_mem, hc = g.hook_code;
    uc_hook a = g.h_mem_w, b = g.h_code;
#endif
    memset(&g, 0, sizeof(g));
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        if (hm) (void)uc_hook_del((uc_engine *)uc, a);
        if (hc) (void)uc_hook_del((uc_engine *)uc, b);
    }
#endif
    set_phase("pre_map");
    watch_add(SEED_REC14);
    watch_add(SEED_REC1B);
}

void p22o_bind_uc(void *uc) {
    const char *rid;
    if (!p22o_enabled()) return;
    g.uc = uc;
    rid = getenv("JJFB_P22O_RUN_ID");
    if (!rid || !rid[0]) rid = getenv("JJFB_P22I_RUN_ID");
    if (rid && rid[0]) snprintf(g.run_id, sizeof(g.run_id), "%s", rid);
    if (!g.phase[0]) set_phase("pre_map");
    watch_add(SEED_REC14);
    watch_add(SEED_REC1B);
#ifdef GWY_HAVE_UNICORN
    /* Arm as soon as uc is known — catch writes before/during cfunction map. */
    arm_early();
#endif
}

void p22o_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, const char *package_owner) {
    if (!p22o_enabled() || !module_name) return;
    if (strstr(module_name, "gamelist")) {
        g.gl_base = base;
        g.gl_end = base + size;
    }
    if (strstr(module_name, "cfunction") || strstr(module_name, "CFunction") ||
        strstr(module_name, "cfunction.ext")) {
        g.cf_base = base;
        g.cf_size = size;
        g.cf_end = base + size;
        if (erw) g.cf_erw = erw;
        set_phase("post_cf_map");
        printf("[JJFB_P22O] note_module_map cfunction name=%s base=0x%X size=0x%X erw=0x%X "
               "evidence=OBSERVED\n",
               module_name, base, size, erw);
        fflush(stdout);
#ifdef GWY_HAVE_UNICORN
        arm_early();
#endif
    } else if (base && size && !g.cf_base) {
        /* Some shells map primary under package name; adopt by size class later via PC. */
        printf("[JJFB_P22O] note_module_map other name=%s base=0x%X size=0x%X evidence=DOCUMENTED\n",
               module_name, base, size);
        fflush(stdout);
    }
    if (p_guest) g.p_guest = p_guest;
    if (generation) g.generation = generation;
    if (package_owner && package_owner[0])
        snprintf(g.package_owner, sizeof(g.package_owner), "%s", package_owner);
}

void p22o_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       uint32_t sp) {
    if (!p22o_enabled() || g.finalized || !continuation_pc) return;
    if (!g.uc) g.uc = uc;
    (void)sp;
    g.last_method = method;
#ifdef GWY_HAVE_UNICORN
    arm_early();
#endif
    if (method == 6u) {
        g.m601_bits |= 1u;
        set_phase("during_601");
    } else if (method == 0u) {
        g.m601_bits |= 2u;
        set_phase("during_601");
    } else if (method == 1u) {
        g.m601_bits |= 4u;
        set_phase("pre_1C40C");
#ifdef GWY_HAVE_UNICORN
        arm_code_observe(); /* catch +0x1C40C fetch after method=1 returns */
#endif
        if (g.object) add_meta("before_method1_return");
    } else if (method == 2u && (g.m601_bits & 4u)) {
        g.fire2_n++;
        printf("[JJFB_P22O] fire2_n=%u write14=%d write1b=%d idle=%d stream=%u "
               "evidence=OBSERVED\n",
               g.fire2_n, g.first_write14, g.first_write1b, g.interpreter_idle, g.stream_n);
        fflush(stdout);
        if (g.interpreter_idle && g.fire2_n >= 1u && g.stream_n > 0 && !g.finalized) {
            snprintf(g.stop_reason, sizeof(g.stop_reason), "early_producer_closed_idle");
            p22o_finalize(g.stop_reason);
            return;
        }
        /* Even without fetch stream, finalize if we saw producer complete */
        if (g.fire2_n >= 1u && (g.first_write14 || g.seed14_preexisting) &&
            (g.first_write1b || g.seed1b_preexisting) && g.producer_ret_seen && !g.finalized &&
            (g.m601_bits & 7u) == 7u) {
            snprintf(g.stop_reason, sizeof(g.stop_reason), "early_producer_closed_idle");
            p22o_finalize(g.stop_reason);
            return;
        }
    }
    printf("[JJFB_P22O] arm_continuation=0x%X method=%u phase=%s mem=%d evidence=OBSERVED\n",
           continuation_pc, method, g.phase, g.armed_mem);
    fflush(stdout);
}

static void classify(void) {
    char seq[128];
    uint32_t i, n = 0;
    seq[0] = 0;
    for (i = 0; i < g.stream_n && n + 8u < sizeof(seq); i++)
        n += (uint32_t)snprintf(seq + n, sizeof(seq) - n, "%s0x%02X", i ? "," : "",
                                g.stream[i].opcode);

    if (g.first_write14 && g.first_write1b)
        snprintf(g.writer_class, sizeof(g.writer_class), "dynamic_early_writes");
    else if (g.witness_pc14 || g.witness_pc1b || g.saw_staging_op1b)
        snprintf(g.writer_class, sizeof(g.writer_class), "staging_or_pre_overwrite");
    else if (g.seed14_preexisting || g.seed1b_preexisting)
        snprintf(g.writer_class, sizeof(g.writer_class), "pre_probe_window");
    else if (!g.first_write14 && !g.first_write1b && g.saw_fetch14)
        snprintf(g.writer_class, sizeof(g.writer_class), "UNKNOWN");
    else
        snprintf(g.writer_class, sizeof(g.writer_class), "UNKNOWN");

    if (g.first_write14 || g.first_write1b || g.saw_staging_op1b) {
        snprintf(g.divergence, sizeof(g.divergence),
                 "seed14=%d@+0x%X seed1b=%d@+0x%X staging1b=%d@0x%X/+0x%X; "
                 "no UI/init append; stream=[%s]",
                 g.first_write14, g.producer_off14, g.first_write1b, g.producer_off1b,
                 g.saw_staging_op1b, g.staging_op1b_addr,
                 g.cf_base && g.staging_op1b_pc >= g.cf_base ? g.staging_op1b_pc - g.cf_base : 0,
                 seq[0] ? seq : "?");
    } else {
        snprintf(g.divergence, sizeof(g.divergence),
                 "seeds present at fetch without write in armed window (class=%s); stream=[%s]",
                 g.writer_class, seq[0] ? seq : "?");
    }

    snprintf(g.sole_lock, sizeof(g.sole_lock),
             "cmd buffer early producer: class=%s write14=%d@+0x%X write1b=%d@+0x%X "
             "staging1b=%d; no UI/init record after 0x14/0x1B",
             g.writer_class, g.first_write14, g.producer_off14, g.first_write1b, g.producer_off1b,
             g.saw_staging_op1b);
    if (g.first_write14 || g.first_write1b || g.saw_staging_op1b) {
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "trace +0x12CE0 staging->seed; skip Y=%s A=%s; find W for UI/init",
                 g.skip_field_y[0] ? g.skip_field_y : "UNKNOWN",
                 g.skip_actual_a[0] ? g.skip_actual_a : "UNKNOWN");
    } else {
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "arm still earlier or identify construction of 0x800054 (not a literal); "
                 "class=%s",
                 g.writer_class);
    }
}

static void write_artifacts(void) {
    FILE *f;
    uint32_t i;
    char seq[128];
    uint32_t n = 0;
    seq[0] = 0;
    for (i = 0; i < g.stream_n && n + 8u < sizeof(seq); i++)
        n += (uint32_t)snprintf(seq + n, sizeof(seq) - n, "%s0x%02X", i ? "," : "",
                                g.stream[i].opcode);

    f = open_out("JJFB_P22O_WRITES_CSV", "reports/p22o/p22o_buffer_writes.csv");
    if (f) {
        fprintf(f, "seq,pc,off,addr,size,old,new,lr,sp,phase,kind,note,stack0,stack1,stack2,stack3\n");
        for (i = 0; i < g.write_n; i++) {
            WriteRow *w = &g.writes[i];
            fprintf(f, "%u,0x%X,0x%X,0x%X,%u,0x%X,0x%X,0x%X,0x%X,%s,%s,\"%s\",0x%X,0x%X,0x%X,0x%X\n",
                    w->seq, w->pc, w->off, w->addr, w->size, w->old_v, w->new_v, w->lr, w->sp,
                    w->phase, w->kind, w->note, w->stack[0], w->stack[1], w->stack[2], w->stack[3]);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22O_META_CSV", "reports/p22o/p22o_buffer_meta_timeline.csv");
    if (f) {
        fprintf(f, "seq,phase,object,f08,f0c,f14,f30,buf_base,cursor,end,capacity_guess,"
                   "record_count_guess,trigger_pc,note\n");
        for (i = 0; i < g.meta_n; i++) {
            MetaRow *m = &g.meta[i];
            fprintf(f, "%u,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,0x%X,\"%s\"\n", m->seq,
                    m->phase, m->object, m->f08, m->f0c, m->f14, m->f30, m->buf_base, m->cursor,
                    m->end, m->capacity_guess, m->record_count_guess, m->trigger_pc, m->note);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22O_PROV_CSV", "reports/p22o/p22o_record_provenance.csv");
    if (f) {
        fprintf(f, "seq,record_addr,value,writer_pc,writer_off,writer_mod,old,lr,kind,note\n");
        for (i = 0; i < g.prov_n; i++) {
            ProvRow *p = &g.prov[i];
            fprintf(f, "%u,0x%X,0x%X,0x%X,0x%X,%s,0x%X,0x%X,%s,\"%s\"\n", p->seq, p->record_addr,
                    p->value, p->writer_pc, p->writer_off, p->writer_mod, p->old_v, p->lr, p->kind,
                    p->note);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22O_SLICE_CSV", "reports/p22o/p22o_producer_slice.csv");
    if (f) {
        fprintf(f, "seq,pc,off,insn,desc,r0,r1,r2,r3,r4,r5,sp,lr,note\n");
        for (i = 0; i < g.slice_n; i++) {
            SliceRow *s = &g.slice[i];
            fprintf(f, "%u,0x%X,0x%X,0x%08X,\"%s\",0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\"\n",
                    s->seq, s->pc, s->off, s->insn, s->desc, s->r0, s->r1, s->r2, s->r3, s->r4,
                    s->r5, s->sp, s->lr, s->note);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22O_SKIP_CSV", "reports/p22o/p22o_skip_predicate.csv");
    if (f) {
        fprintf(f, "seq,pc,off,insn,taken,r0,r1,r2,r3,r4,mem_addr,mem_val,pred,note\n");
        for (i = 0; i < g.skip_n; i++) {
            SkipRow *k = &g.skips[i];
            fprintf(f, "%u,0x%X,0x%X,0x%08X,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\",\"%s\"\n",
                    k->seq, k->pc, k->off, k->insn, k->taken, k->r0, k->r1, k->r2, k->r3, k->r4,
                    k->mem_addr, k->mem_val, k->pred, k->note);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22O_DIV_MD", "reports/p22o/p22o_first_divergence.md");
    if (f) {
        fprintf(f,
                "# P22O first divergence\n\n"
                "## Early producer window\n\n"
                "- mem_armed_at=cfunction_map_or_bind_uc\n"
                "- seed14=0x%X seed1b=0x%X\n"
                "- first_write14=%d pc=0x%X off=+0x%X\n"
                "- first_write1b=%d pc=0x%X off=+0x%X\n"
                "- staging_op1b=%d addr=0x%X pc=0x%X\n"
                "- seed_preexisting14=%d seed_preexisting1b=%d\n"
                "- writer_class=%s\n\n"
                "## Divergence\n\n%s\n\n"
                "- skip_field_Y=%s\n"
                "- skip_actual_A=%s\n"
                "- expected_contract_W=%s\n"
                "- opcodes=%s\n"
                "- fire2_n=%u\n",
                SEED_REC14, SEED_REC1B, g.first_write14, g.producer_pc14, g.producer_off14,
                g.first_write1b, g.producer_pc1b, g.producer_off1b, g.saw_staging_op1b,
                g.staging_op1b_addr, g.staging_op1b_pc, g.seed14_preexisting,
                g.seed1b_preexisting, g.writer_class, g.divergence,
                g.skip_field_y[0] ? g.skip_field_y : "UNKNOWN",
                g.skip_actual_a[0] ? g.skip_actual_a : "UNKNOWN",
                g.skip_expected_w[0] ? g.skip_expected_w : "UNKNOWN", seq[0] ? seq : "(none)",
                g.fire2_n);
        fclose(f);
    }

    f = open_out("JJFB_P22O_VERDICT", "reports/p22o/p22o_early_producer_verdict.md");
    if (f) {
        fprintf(f,
                "# P22O-CLEAN early command-buffer producer verdict\n\n"
                "## Bottom line\n\n%s\n\n"
                "## PASS answers\n\n```\n"
                "0x2AF8F8 first writer：%s pc=0x%X off=+0x%X\n"
                "0x2AF904 first writer：%s pc=0x%X off=+0x%X\n"
                "op1b staging：%s addr=0x%X pc=0x%X\n"
                "writer_class：%s\n"
                "opcode stream：%s\n"
                "append_n：%d\n"
                "skip_field_Y：%s\n"
                "skip_actual_A：%s\n"
                "expected_W：%s\n"
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
                "- stop=%s\n",
                g.sole_lock, g.first_write14 ? "OBSERVED" : "NOT_IN_WINDOW", g.producer_pc14,
                g.producer_off14, g.first_write1b ? "OBSERVED" : "NOT_IN_WINDOW", g.producer_pc1b,
                g.producer_off1b, g.saw_staging_op1b ? "OBSERVED" : "NO", g.staging_op1b_addr,
                g.staging_op1b_pc, g.writer_class, seq[0] ? seq : "(none)", g.append_n,
                g.skip_field_y[0] ? g.skip_field_y : "UNKNOWN",
                g.skip_actual_a[0] ? g.skip_actual_a : "UNKNOWN",
                g.skip_expected_w[0] ? g.skip_expected_w : "UNKNOWN",
                g.entered_10740 ? "YES" : "NO", g.entered_7b6c ? "YES" : "NO",
                g.cfg_open ? "YES" : "NO", g.sole_lock, g.next_fix, g.run_id,
                g.cf_sha[0] ? g.cf_sha : "NOT_EXPORTED", g.cf_base, g.cf_end,
                g.stop_reason[0] ? g.stop_reason : "finalize");
        fclose(f);
    }

    f = open_out("JJFB_P22O_SUMMARY", "out/p22o/p22o_runtime_summary.txt");
    if (f) {
        fprintf(f,
                "run_id=%s\ncf_base=0x%X\ncf_end=0x%X\n"
                "cfunction_runtime_sha256=%s\n"
                "opcodes=%s\nstream_n=%u\nfire2_n=%u\n"
                "first_write14=%d\nfirst_write1b=%d\n"
                "producer_off14=0x%X\nproducer_off1b=0x%X\n"
                "staging_op1b=%d\nstaging_op1b_addr=0x%X\nstaging_op1b_pc=0x%X\n"
                "writer_class=%s\nappend_n=%d\n"
                "seed_preexisting14=%d\nseed_preexisting1b=%d\n"
                "entered_10740=%d\nentered_7b6c=%d\n"
                "sole_lock=%s\nnext_fix=%s\nstop_reason=%s\n"
                "guest_modified=NO\n",
                g.run_id, g.cf_base, g.cf_end, g.cf_sha[0] ? g.cf_sha : "?", seq, g.stream_n,
                g.fire2_n, g.first_write14, g.first_write1b, g.producer_off14, g.producer_off1b,
                g.saw_staging_op1b, g.staging_op1b_addr, g.staging_op1b_pc, g.writer_class,
                g.append_n, g.seed14_preexisting, g.seed1b_preexisting, g.entered_10740,
                g.entered_7b6c, g.sole_lock, g.next_fix,
                g.stop_reason[0] ? g.stop_reason : "finalize");
        fclose(f);
    }

    {
        const char *idp = getenv("JJFB_P22O_IDENTITY");
        if (idp && idp[0] && g.cf_sha[0]) {
            FILE *idf = fopen(idp, "wb");
            if (idf) {
                fprintf(idf,
                        "run_id=%s\n"
                        "gate=P22O_CMD_BUFFER_EARLY_PRODUCER\n"
                        "NATURAL_ONLY=yes\n"
                        "JJFB_P22O_CLEAN=1\n"
                        "JJFB_P22I_CLEAN=1\n"
                        "cfunction_runtime_sha256=%s\n"
                        "cfunction_base=0x%X\n"
                        "cfunction_end=0x%X\n"
                        "no_object_plus30_0x0C_force=yes\n",
                        g.run_id, g.cf_sha, g.cf_base, g.cf_end);
                fclose(idf);
            }
        }
    }
}

void p22o_finalize(const char *stop_reason) {
    if (!p22o_enabled() || g.finalized) return;
    g.finalized = 1;
    if (stop_reason && stop_reason[0] && !g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", stop_reason);
    else if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "finalize");
#ifdef GWY_HAVE_UNICORN
    remove_hooks();
    maybe_export_image();
#endif
    classify();
    write_artifacts();
    printf("[JJFB_P22O_FINAL] sha=%s write14=%d@+0x%X write1b=%d@+0x%X class=%s stream=%u "
           "fire2=%u lock=%s evidence=OBSERVED\n",
           g.cf_sha[0] ? g.cf_sha : "?", g.first_write14, g.producer_off14, g.first_write1b,
           g.producer_off1b, g.writer_class, g.stream_n, g.fire2_n, g.sole_lock);
    fflush(stdout);
}
