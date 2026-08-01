#include "gwy_launcher/p22m_queue_scheduler.h"

#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define P22M_WIN 0x200u
#define P22M_LDM_OFF 0x48u
#define P22M_SLICE_CAP 4096u
#define P22M_PARENT_CAP 512u
#define P22M_WRITE_CAP 512u
#define P22M_PROV_CAP 256u
#define P22M_LIFE_CAP 128u
#define P22M_XREF_CAP 64u
#define P22M_CF_INSN_MAX 200000u
#define P22M_NODE_PAD 0x40u

#define OFF_6E5C 0x6E5Cu
#define OFF_174C8 0x174C8u
#define OFF_17970 0x17970u
#define OFF_1D098 0x1D098u
#define OFF_1D0DC 0x1D0DCu
#define OFF_1D0E0 0x1D0E0u
#define OFF_F670 0xF670u
#define OFF_8CDC 0x8CDCu
#define OFF_D978 0xD978u
#define OFF_10740 0x10740u
#define OFF_10814 0x10814u
#define OFF_7B6C 0x7B6Cu

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t off;
    uint32_t insn;
    char desc[56];
    uint32_t r[13];
    uint32_t sp, lr, cpsr, r9;
    char note[72];
    char mem[56];
} SliceRow;

typedef struct {
    uint32_t seq;
    char event[40];
    uint32_t pc;
    uint32_t off;
    uint32_t r0, r1, r2, r3, r4, r9, sp, lr;
    char detail[96];
} ParentEv;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t off;
    uint32_t addr;
    uint32_t size;
    uint32_t old_v;
    uint32_t new_v;
    char field[24];
    char phase[24];
} FieldWrite;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t off;
    uint32_t addr;
    uint32_t value;
    char kind[32];
    char note[64];
} ProvRow;

typedef struct {
    uint32_t seq;
    char event[40];
    uint32_t pc;
    uint32_t off;
    uint32_t obj;
    uint32_t node;
    uint32_t f08, f0c, f10, f14, f18, f30;
    char detail[80];
} LifeRow;

typedef struct {
    uint32_t caller_pc;
    uint32_t caller_off;
    uint32_t target_off;
    char kind[16];
} XrefRow;

static struct {
    int armed;
    int finalized;
    int hook_sparse;
    int hook_global;
    int hook_mem;
    int image_exported;
    int dense;
    int await_next;
    int in_174c8;
    int saw_174c8_enter;
    int saw_174c8_ret;
    int saw_1d098;
    int saw_1d0e0;
    int fn_1d098_entered;
    int fn_1d098_returned;
    int parent_follow;
    int parent_returned;
    int helper_reenter;
    void *uc;

    uint32_t cont_pc;
    uint32_t window_end;
    uint32_t ldm_pc;
    uint32_t last_method;
    uint32_t await_stack_pc;
    uint32_t await_method;

    uint32_t cf_base, cf_end, cf_size, cf_erw;
    uint32_t gl_base, gl_end, gl_erw, gl_helper;
    uint32_t p_guest;
    uint64_t generation;
    char package_owner[64];
    char cf_sha[65];

    uint32_t cf_insn_n;
    uint32_t dense_from_insn;

    /* discovered dynamically */
    uint32_t object;
    uint32_t derived_node;
    uint32_t index_base; /* r1 before SUB at +0x17978 */
    uint32_t index_r0;   /* usually 2 */
    uint32_t call_174c8_r0, call_174c8_r1, call_174c8_r2, call_174c8_r3;
    uint32_t call_174c8_r9, call_174c8_sp, call_174c8_lr;
    uint32_t ret_174c8_r0;
    uint32_t fn_1d098_entry;
    uint32_t fn_1d098_entry_sp;
    uint32_t fn_1d098_ret_pc;
    uint32_t fn_1d098_ret_r0;
    uint32_t fn_174c8_entry;
    uint32_t fn_174c8_end;
    uint32_t fn_17970_entry;
    uint32_t fn_17970_end;
    uint32_t parent_after_1d098;
    uint32_t parent_after_1d098_sp;

    uint32_t obj_before[6]; /* +08 +0C +10 +14 +18 +30 */
    uint32_t obj_after[6];
    int obj_before_ok;
    int obj_after_ok;
    uint8_t node_before[0x80];
    uint8_t node_after[0x80];
    int node_before_ok;
    int node_after_ok;

    SliceRow slice[P22M_SLICE_CAP];
    uint32_t slice_n;
    ParentEv parent[P22M_PARENT_CAP];
    uint32_t parent_n;
    FieldWrite writes[P22M_WRITE_CAP];
    uint32_t write_n;
    ProvRow prov[P22M_PROV_CAP];
    uint32_t prov_n;
    LifeRow life[P22M_LIFE_CAP];
    uint32_t life_n;
    XrefRow xrefs[P22M_XREF_CAP];
    uint32_t xref_n;

    int entered_f670, entered_8cdc, entered_d978;
    int entered_10740, entered_10814, entered_7b6c;
    int callback_pub;
    int cfg_open;
    int next_record_exists;
    int queue_empty_guess;

    uint32_t block_pc;
    uint32_t block_lhs, block_rhs;
    char block_path[96];
    char block_field[48];
    char block_writer[64];
    char block_producer[64];

    char run_id[64];
    char stop_reason[96];
    char sole_lock[240];
    char next_fix[240];
    char sem_17970[160];
    char sem_1d098[160];
    char sem_174c8[160];
    char node_type[80];
    char node_producer[120];

#ifdef GWY_HAVE_UNICORN
    uc_hook h_sparse;
    uc_hook h_global;
    uc_hook h_mem_r;
    uc_hook h_mem_w;
#endif
} g;

static const char *env_or(const char *k, const char *d) {
    const char *v = getenv(k);
    return (v && v[0]) ? v : d;
}

int p22m_enabled(void) {
    const char *e = getenv("JJFB_P22M_CLEAN");
    return e && e[0] == '1';
}

static FILE *open_out(const char *envk, const char *defpath) {
    const char *p = env_or(envk, defpath);
    FILE *f = fopen(p, "wb");
    return f;
}

static int popcount16(uint32_t x) {
    int n = 0;
    while (x) {
        n += (int)(x & 1u);
        x >>= 1;
    }
    return n;
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
    if ((w & 0x0FFFFFF0u) == 0x012FFF30u) {
        snprintf(out, n, "BLX r%u", w & 0xFu);
        return;
    }
    if ((w & 0x0FFFFFF0u) == 0x012FFF10u) {
        snprintf(out, n, "BX r%u", w & 0xFu);
        return;
    }
    if ((w & 0x0F000000u) == 0x0B000000u) {
        imm = (int32_t)(w & 0x00FFFFFFu);
        if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
        tgt = (uint32_t)((int32_t)pc + 8 + (imm << 2));
        snprintf(out, n, "BL 0x%X", tgt);
        return;
    }
    if ((w & 0x0F000000u) == 0x0A000000u) {
        imm = (int32_t)(w & 0x00FFFFFFu);
        if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
        tgt = (uint32_t)((int32_t)pc + 8 + (imm << 2));
        snprintf(out, n, "Bcond 0x%X", tgt);
        return;
    }
    if ((w & 0x0FFF0000u) == 0x08BD0000u && (w & 0x8000u)) {
        snprintf(out, n, "LDMFD sp!,{..pc} list=0x%X", w & 0xFFFFu);
        return;
    }
    if ((w & 0x0FFF0000u) == 0x092D0000u) {
        snprintf(out, n, "STMFD sp!,{..} list=0x%X", w & 0xFFFFu);
        return;
    }
    if ((w & 0xFFFFF000u) == 0xE3500000u) {
        snprintf(out, n, "CMP r0,#0x%X", w & 0xFFFu);
        return;
    }
    if ((w & 0xFFFFF000u) == 0xE3550000u) {
        snprintf(out, n, "CMP r5,#0x%X", w & 0xFFFu);
        return;
    }
    if ((w & 0xFFFFF000u) == 0xE3100000u) {
        snprintf(out, n, "TST r0,#0x%X", w & 0xFFFu);
        return;
    }
    if (w == 0xE0410180u) {
        snprintf(out, n, "SUB r0,r1,r0,LSL#3");
        return;
    }
    if ((w & 0xFFF00FF0u) == 0xE1A00000u) {
        snprintf(out, n, "MOV r%u,r%u", (w >> 12) & 0xFu, w & 0xFu);
        return;
    }
    if ((w & 0xFFFF0000u) == 0xE59F0000u) {
        snprintf(out, n, "LDR r%u,[pc,#0x%X]", (w >> 12) & 0xFu, w & 0xFFFu);
        return;
    }
    if ((w & 0xFFFF0000u) == 0xE59D0000u) {
        snprintf(out, n, "LDR r%u,[sp,#0x%X]", (w >> 12) & 0xFu, w & 0xFFFu);
        return;
    }
    if ((w & 0xFFFF0000u) == 0xE58D0000u) {
        snprintf(out, n, "STR r%u,[sp,#0x%X]", (w >> 12) & 0xFu, w & 0xFFFu);
        return;
    }
    if ((w & 0xFFFF0000u) == 0xE5940000u) {
        snprintf(out, n, "LDR r%u,[r4,#0x%X]", (w >> 12) & 0xFu, w & 0xFFFu);
        return;
    }
    if ((w & 0xFFFF0000u) == 0xE5840000u) {
        snprintf(out, n, "STR r%u,[r4,#0x%X]", (w >> 12) & 0xFu, w & 0xFFFu);
        return;
    }
    if ((w & 0xFFFF0000u) == 0xE5D00000u) {
        snprintf(out, n, "LDRB r%u,[r0,#0x%X]", (w >> 12) & 0xFu, w & 0xFFFu);
        return;
    }
    snprintf(out, n, "w=0x%08X", w);
}

static void add_parent(const char *ev, uint32_t pc, uint32_t off, const uint32_t regs[16],
                       uint32_t sp, uint32_t lr, const char *detail) {
    ParentEv *p;
    if (g.parent_n >= P22M_PARENT_CAP) return;
    p = &g.parent[g.parent_n++];
    memset(p, 0, sizeof(*p));
    p->seq = g.parent_n;
    snprintf(p->event, sizeof(p->event), "%s", ev ? ev : "?");
    p->pc = pc;
    p->off = off;
    p->r0 = regs ? regs[0] : 0;
    p->r1 = regs ? regs[1] : 0;
    p->r2 = regs ? regs[2] : 0;
    p->r3 = regs ? regs[3] : 0;
    p->r4 = regs ? regs[4] : 0;
    p->r9 = regs ? regs[9] : 0;
    p->sp = sp;
    p->lr = lr;
    snprintf(p->detail, sizeof(p->detail), "%s", detail ? detail : "");
    printf("[JJFB_P22M] %s pc=0x%X off=0x%X r0=0x%X r2=0x%X r4=0x%X %s evidence=OBSERVED\n",
           p->event, pc, off, p->r0, p->r2, p->r4, p->detail);
    fflush(stdout);
}

static void add_life(const char *ev, uint32_t pc, uint32_t off, const char *detail) {
    LifeRow *r;
    if (g.life_n >= P22M_LIFE_CAP) return;
    r = &g.life[g.life_n++];
    memset(r, 0, sizeof(*r));
    r->seq = g.life_n;
    snprintf(r->event, sizeof(r->event), "%s", ev ? ev : "?");
    r->pc = pc;
    r->off = off;
    r->obj = g.object;
    r->node = g.derived_node;
    if (g.obj_before_ok || g.obj_after_ok) {
        const uint32_t *f = g.obj_after_ok ? g.obj_after : g.obj_before;
        r->f08 = f[0];
        r->f0c = f[1];
        r->f10 = f[2];
        r->f14 = f[3];
        r->f18 = f[4];
        r->f30 = f[5];
    }
    snprintf(r->detail, sizeof(r->detail), "%s", detail ? detail : "");
}

static void add_prov(uint32_t pc, uint32_t off, uint32_t addr, uint32_t value, const char *kind,
                     const char *note) {
    ProvRow *r;
    if (g.prov_n >= P22M_PROV_CAP) return;
    r = &g.prov[g.prov_n++];
    memset(r, 0, sizeof(*r));
    r->seq = g.prov_n;
    r->pc = pc;
    r->off = off;
    r->addr = addr;
    r->value = value;
    snprintf(r->kind, sizeof(r->kind), "%s", kind ? kind : "?");
    snprintf(r->note, sizeof(r->note), "%s", note ? note : "");
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

static int read_bytes(uint32_t addr, void *buf, size_t n) {
#ifdef GWY_HAVE_UNICORN
    if (!g.uc || !buf || !n) return 0;
    return guest_memory_uc_peek((struct uc_struct *)g.uc, addr, buf, (uint32_t)n);
#else
    (void)addr;
    (void)buf;
    (void)n;
    return 0;
#endif
}

static void snap_object(uint32_t *dst, int *ok) {
    if (!g.object || !dst || !ok) return;
    *ok = 0;
    if (!read_u32(g.object + 0x08u, &dst[0])) return;
    if (!read_u32(g.object + 0x0Cu, &dst[1])) return;
    if (!read_u32(g.object + 0x10u, &dst[2])) return;
    if (!read_u32(g.object + 0x14u, &dst[3])) return;
    if (!read_u32(g.object + 0x18u, &dst[4])) return;
    if (!read_u32(g.object + 0x30u, &dst[5])) return;
    *ok = 1;
}

static void snap_node(uint8_t *dst, int *ok) {
    if (!g.derived_node || !dst || !ok) return;
    *ok = 0;
    if (read_bytes(g.derived_node - P22M_NODE_PAD, dst, 0x80u)) *ok = 1;
}

static uint32_t find_fn_entry(uint32_t tip) {
    uint32_t pc, w;
    if (!g.cf_base || tip < g.cf_base + 4u || tip >= g.cf_end) return tip;
    for (pc = tip & ~3u; pc + 3u >= g.cf_base + 4u && tip - pc < 0x400u; pc -= 4u) {
        if (!read_u32(pc, &w)) break;
        if ((w & 0xFFFF0000u) == 0xE92D0000u) return pc; /* STMFD */
        if ((w & 0xFFFF0000u) == 0xE52D0000u) return pc; /* STR lr,[sp,#-4]! */
    }
    return tip & ~3u;
}

static uint32_t find_fn_end(uint32_t entry) {
    uint32_t pc, w, end;
    if (!g.cf_base || !entry) return 0;
    end = entry + 0x800u;
    if (end > g.cf_end) end = g.cf_end;
    for (pc = entry; pc + 3u < end; pc += 4u) {
        if (!read_u32(pc, &w)) break;
        if ((w & 0x0FFF8000u) == 0x08BD8000u) return pc + 4u; /* LDM with PC */
        if ((w & 0x0FFFFFF0u) == 0x012FFF10u && (w & 0xFu) == 14u) return pc + 4u; /* BX lr */
    }
    return 0;
}

static void scan_xrefs_live(void) {
    uint32_t pc, w, tgt, off;
    uint32_t targets[3] = {OFF_174C8, OFF_17970, OFF_1D098};
    int ti;
    if (!g.cf_base || g.xref_n) return;
    for (pc = g.cf_base; pc + 3u < g.cf_end && g.xref_n < P22M_XREF_CAP; pc += 4u) {
        if (!read_u32(pc, &w)) break;
        if ((w & 0x0F000000u) != 0x0B000000u && (w & 0x0F000000u) != 0x0A000000u) continue;
        tgt = branch_target_arm(pc, w);
        if (!tgt || tgt < g.cf_base || tgt >= g.cf_end) continue;
        off = tgt - g.cf_base;
        for (ti = 0; ti < 3; ti++) {
            if (off == targets[ti] ||
                (targets[ti] == OFF_1D098 && off + 0x80u >= OFF_1D098 && off <= OFF_1D098)) {
                XrefRow *x = &g.xrefs[g.xref_n++];
                memset(x, 0, sizeof(*x));
                x->caller_pc = pc;
                x->caller_off = pc - g.cf_base;
                x->target_off = targets[ti];
                snprintf(x->kind, sizeof(x->kind), "%s",
                         ((w & 0x0F000000u) == 0x0B000000u) ? "BL" : "B");
                break;
            }
        }
    }
}

static void adopt_cf_module(const GwyLoadedModule *m, const char *via) {
    const char *nm;
    if (!m || !m->map.guest_code_base || !m->map.guest_code_size) return;
    nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    g.cf_base = m->map.guest_code_base;
    g.cf_size = m->map.guest_code_size;
    g.cf_end = g.cf_base + g.cf_size;
    if (m->data.start_of_er_rw) g.cf_erw = m->data.start_of_er_rw;
    printf("[JJFB_P22M] ensure_cf_base=0x%X end=0x%X size=0x%X name=%s via=%s "
           "evidence=OBSERVED\n",
           g.cf_base, g.cf_end, g.cf_size, nm ? nm : "?", via ? via : "?");
    fflush(stdout);
}

static void ensure_gl_base(void) {
    ModuleRegistry *reg;
    size_t i;
    if (g.gl_base) return;
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return;
    for (i = 0; i < reg->count; i++) {
        const GwyLoadedModule *m = &reg->modules[i];
        const char *nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
        if (!nm || !strstr(nm, "gamelist")) continue;
        if (!m->map.guest_code_base) continue;
        g.gl_base = m->map.guest_code_base;
        g.gl_end = g.gl_base + m->map.guest_code_size;
        if (m->data.start_of_er_rw) g.gl_erw = m->data.start_of_er_rw;
        if (m->entries.registered_helper) g.gl_helper = m->entries.registered_helper;
        else if (m->map.helper_address) g.gl_helper = m->map.helper_address;
        break;
    }
}

/* DSM cfunction is often not delivered via ext_image_raw note_module_map. */
static void ensure_cf_base(void) {
    ModuleRegistry *reg;
    size_t i;
    if (g.cf_base && g.cf_end > g.cf_base) {
        ensure_gl_base();
        return;
    }
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return;
    for (i = 0; i < reg->count; i++) {
        const GwyLoadedModule *m = &reg->modules[i];
        const char *nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
        if (!nm || !strstr(nm, "cfunction")) continue;
        adopt_cf_module(m, "name_scan");
        break;
    }
    ensure_gl_base();
}

/* Recover base from a live PC (registry addr lookup, then Class-C epilogue anchor). */
static void ensure_cf_base_from_pc(uint32_t pc) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    uint32_t norm = pc & ~3u;
    uint32_t insn = 0;
    size_t i;
    ensure_cf_base();
    if (g.cf_base && g.cf_end > g.cf_base) return;
    reg = gwy_ext_loader_bound_registry();
    if (reg && norm) {
        m = module_registry_find_by_code_addr(reg, norm);
        if (m && m->map.guest_code_base) {
            adopt_cf_module(m, "find_by_pc");
            ensure_gl_base();
            return;
        }
    }
    /* Dynamic anchor: wrapper return epilogue is LDMFD @ cfunction+0x6E5C (Class C). */
    if (norm >= OFF_6E5C && read_u32(norm, &insn) && insn == 0xE8BD8038u) {
        uint32_t cand = norm - OFF_6E5C;
        if (reg) {
            for (i = 0; i < reg->count; i++) {
                const GwyLoadedModule *mm = &reg->modules[i];
                if (mm->map.guest_code_base == cand && mm->map.guest_code_size) {
                    adopt_cf_module(mm, "epilogue_6E5C");
                    ensure_gl_base();
                    return;
                }
            }
            m = module_registry_find_by_code_addr(reg, cand + 8u);
            if (m && m->map.guest_code_base) {
                adopt_cf_module(m, "epilogue_find");
                ensure_gl_base();
                return;
            }
        }
        printf("[JJFB_P22M] ensure_cf_base_anchor_miss cand=0x%X pc=0x%X evidence=OBSERVED\n",
               cand, norm);
        fflush(stdout);
    }
    ensure_gl_base();
}

static void maybe_export_image(void) {
    uint8_t *buf = NULL;
    uint8_t dig[32];
    FILE *f;
    const char *binp;
    const char *shap;
    size_t n;
    ensure_cf_base();
    if (g.image_exported || !g.uc || !g.cf_base || !g.cf_size) return;
    n = g.cf_size;
    if (n > 0x100000u) n = 0x100000u;
    buf = (uint8_t *)malloc(n);
    if (!buf) return;
    if (!guest_memory_uc_peek((struct uc_struct *)g.uc, g.cf_base, buf, (uint32_t)n)) {
        free(buf);
        return;
    }
    gwy_sha256(buf, n, dig);
    gwy_sha256_hex(dig, g.cf_sha);

    binp = env_or("JJFB_P22M_CF_BIN", "out/p22m/cfunction_runtime.bin");
    shap = env_or("JJFB_P22M_CF_SHA", "out/p22m/cfunction_runtime.sha256");
    f = fopen(binp, "wb");
    if (f) {
        fwrite(buf, 1, n, f);
        fclose(f);
    }
    f = fopen(shap, "wb");
    if (f) {
        fprintf(f, "%s\n", g.cf_sha);
        fprintf(f, "base=0x%X\nend=0x%X\nsize=0x%X\n", g.cf_base, g.cf_end, (uint32_t)n);
        fprintf(f, "erw=0x%X\nP=0x%X\ngeneration=%llu\nowner=%s\n", g.cf_erw, g.p_guest,
                (unsigned long long)g.generation,
                g.package_owner[0] ? g.package_owner : "?");
        fclose(f);
    }
    g.image_exported = 1;
    g.fn_174c8_entry = find_fn_entry(g.cf_base + OFF_174C8);
    g.fn_174c8_end = find_fn_end(g.fn_174c8_entry);
    g.fn_17970_entry = find_fn_entry(g.cf_base + OFF_17970);
    g.fn_17970_end = find_fn_end(g.fn_17970_entry);
    g.fn_1d098_entry = find_fn_entry(g.cf_base + OFF_1D098);
    {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *m;
        if (reg && g.gl_base) {
            m = module_registry_find_by_code_addr(reg, g.gl_base);
            if (m) {
                if (m->entries.registered_helper) g.gl_helper = m->entries.registered_helper;
                else if (m->map.helper_address) g.gl_helper = m->map.helper_address;
            }
        }
    }
    scan_xrefs_live();
    printf("[JJFB_P22M] export_cfunction base=0x%X end=0x%X size=0x%X sha=%s "
           "fn174c8=0x%X..0x%X fn17970=0x%X..0x%X fn1d098_entry=0x%X evidence=OBSERVED\n",
           g.cf_base, g.cf_end, (uint32_t)n, g.cf_sha, g.fn_174c8_entry, g.fn_174c8_end,
           g.fn_17970_entry, g.fn_17970_end, g.fn_1d098_entry);
    fflush(stdout);
    free(buf);
}

static void note_gl_off(uint32_t pc) {
    uint32_t off;
    if (!g.gl_base || pc < g.gl_base || pc >= g.gl_end) return;
    off = (pc & ~1u) - g.gl_base;
    if (off == OFF_F670) g.entered_f670 = 1;
    if (off == OFF_8CDC) g.entered_8cdc = 1;
    if (off == OFF_D978) g.entered_d978 = 1;
    if (off == OFF_10740) g.entered_10740 = 1;
    if (off == OFF_10814) g.entered_10814 = 1;
    if (off == OFF_7B6C) g.entered_7b6c = 1;
}

#ifdef GWY_HAVE_UNICORN
static void finish_dense(const char *why);

static void install_global_hook(void);
static void remove_global_hook(void);
static void install_mem_hooks(void);
static void remove_mem_hooks(void);

static void emit_slice(uint32_t pc, uint32_t off, uint32_t insn, const uint32_t regs[16],
                       uint32_t sp, uint32_t lr, uint32_t cpsr, const char *note) {
    SliceRow *s;
    if (g.slice_n >= P22M_SLICE_CAP) return;
    s = &g.slice[g.slice_n++];
    memset(s, 0, sizeof(*s));
    s->seq = g.slice_n;
    s->pc = pc;
    s->off = off;
    s->insn = insn;
    describe_arm(insn, pc, s->desc, sizeof(s->desc));
    memcpy(s->r, regs, sizeof(s->r));
    s->sp = sp;
    s->lr = lr;
    s->cpsr = cpsr;
    s->r9 = regs[9];
    snprintf(s->note, sizeof(s->note), "%s", note ? note : "");
}

static void on_dense_step(uc_engine *uc, uint32_t pc, const uint32_t regs[16], uint32_t sp,
                          uint32_t lr, uint32_t cpsr, uint32_t insn) {
    uint32_t off = 0;
    char note[72];
    int in_cf;
    (void)uc;
    note[0] = 0;
    if (!g.cf_base || !g.cf_end) ensure_cf_base_from_pc(pc);
    in_cf = g.cf_base && g.cf_end && pc >= g.cf_base && pc < g.cf_end;
    if (in_cf) {
        off = (pc & ~3u) - g.cf_base;
        g.cf_insn_n++;
    }
    note_gl_off(pc);

    /* After consume, leaving cfunction into gamelist is a terminal schedule observation. */
    if (g.saw_174c8_ret && g.gl_base && pc >= g.gl_base && pc < g.gl_end) {
        add_parent("leave_cf_to_gamelist", pc, (pc & ~1u) - g.gl_base, regs, sp, lr,
                   "post_consume");
        emit_slice(pc, 0, insn, regs, sp, lr, cpsr, "to_gamelist");
        finish_dense("returned_to_gamelist");
        return;
    }

    /* Discover object / index / node at +0x17970 path */
    if (in_cf && off == OFF_17970) {
        if (!g.object) g.object = regs[4];
        g.index_r0 = regs[0];
        snap_object(g.obj_before, &g.obj_before_ok);
        add_parent("hit_17970", pc, off, regs, sp, lr, "index_scale_entry");
        add_life("index_scale_enter", pc, off, "r0=index r4=object");
        snprintf(note, sizeof(note), "hit_17970");
    }
    if (in_cf && off == 0x17978u) {
        /* CODE hook before SUB: r0=index, r1=array_base from [object+8] */
        g.index_r0 = regs[0];
        g.index_base = regs[1];
        if (!g.object) g.object = regs[4];
        snprintf(note, sizeof(note), "SUB_index base=0x%X idx=%u", g.index_base, g.index_r0);
    }
    if (in_cf && off == 0x1797Cu) {
        /* After SUB: r0 = base - (index<<3) */
        g.derived_node = regs[0];
        snap_node(g.node_before, &g.node_before_ok);
        add_life("node_derived", pc, off, "r0=derived_node");
        add_prov(pc, off, g.derived_node, g.index_r0, "derive", "base - index*8");
        snprintf(g.sem_17970, sizeof(g.sem_17970),
                 "reads [object+8]=base, returns base-(R0<<3); R0 was wrapper index=%u; "
                 "result=node/slot ptr 0x%X",
                 g.index_r0, g.derived_node);
        snprintf(note, sizeof(note), "node=0x%X", g.derived_node);
    }

    if (in_cf && off == OFF_1D098) {
        g.saw_1d098 = 1;
        if (!g.fn_1d098_entered) {
            g.fn_1d098_entered = 1;
            if (!g.fn_1d098_entry) g.fn_1d098_entry = find_fn_entry(pc);
            g.fn_1d098_entry_sp = sp;
            add_parent("fn_1d098_at_cmp", pc, off, regs, sp, lr, "derived_ptr_check");
            snprintf(g.sem_1d098, sizeof(g.sem_1d098),
                     "in fn_entry=0x%X; CMP derived_node; bounds vs [object+8]; BL +0x174C8 "
                     "to consume/unlink node",
                     g.fn_1d098_entry);
        }
        if (!g.derived_node) g.derived_node = regs[0];
        if (!g.object) g.object = regs[4];
        snprintf(note, sizeof(note), "cmp_derived");
    }

    /* Call into +0x174C8 */
    if (in_cf && off == OFF_1D0DC && ((insn & 0x0F000000u) == 0x0B000000u)) {
        if (!g.saw_174c8_enter) {
            g.call_174c8_r0 = regs[0];
            g.call_174c8_r1 = regs[1];
            g.call_174c8_r2 = regs[2];
            g.call_174c8_r3 = regs[3];
            g.call_174c8_r9 = regs[9];
            g.call_174c8_sp = sp;
        }
        if (!g.object) g.object = regs[0] ? regs[0] : regs[4];
        if (!g.derived_node) g.derived_node = regs[2];
        if (!g.obj_before_ok) snap_object(g.obj_before, &g.obj_before_ok);
        if (!g.node_before_ok) snap_node(g.node_before, &g.node_before_ok);
        add_parent("call_174c8", pc, off, regs, sp, lr, "pre_remove");
        add_life("pre_174c8", pc, off, "snapshot_before");
        snprintf(note, sizeof(note), "BL_174C8");
    }

    if (in_cf && off == OFF_174C8) {
        g.in_174c8 = 1;
        if (!g.saw_174c8_enter) {
            g.saw_174c8_enter = 1;
            g.fn_174c8_entry = pc;
            g.call_174c8_lr = lr; /* first-call continuation, typically +0x1D0E0 */
            g.call_174c8_r0 = regs[0];
            g.call_174c8_r1 = regs[1];
            g.call_174c8_r2 = regs[2];
            g.call_174c8_r3 = regs[3];
            g.call_174c8_r9 = regs[9];
            g.call_174c8_sp = sp;
        }
        add_parent("enter_174c8", pc, off, regs, sp, lr, "container_ops");
        snprintf(note, sizeof(note), "enter_174c8 lr=0x%X", lr);
    }

    if (g.in_174c8 && in_cf && off == OFF_1D0E0) {
        g.in_174c8 = 0;
        g.saw_174c8_ret = 1;
        g.saw_1d0e0 = 1;
        g.ret_174c8_r0 = regs[0];
        snap_object(g.obj_after, &g.obj_after_ok);
        snap_node(g.node_after, &g.node_after_ok);
        add_parent("ret_174c8", pc, off, regs, sp, lr, "post_remove");
        add_life("post_174c8", pc, off, "snapshot_after");
        snprintf(g.sem_174c8, sizeof(g.sem_174c8),
                 "object=0x%X node=0x%X; mutates +0x0C/+0x14 (list unlink); ret_r0=0x%X; "
                 "continuation=+0x1D0E0",
                 g.object, g.derived_node, g.ret_174c8_r0);
        snprintf(note, sizeof(note), "post_remove r0=0x%X", regs[0]);
    }

    /* Detect return of containing +0x1D098 function — never the inner +0x174C8 LDM. */
    if (g.fn_1d098_entered && !g.fn_1d098_returned && !g.in_174c8 && g.saw_1d0e0 && in_cf &&
        (insn & 0x0FFF8000u) == 0x08BD8000u && (insn & 0xF0000000u) == 0xE0000000u) {
        uint32_t rl = insn & 0xFFFFu;
        uint32_t pci = (uint32_t)popcount16(rl & 0x7FFFu);
        uint32_t retpc = 0;
        int inside_174c8 = g.fn_174c8_entry && pc >= g.fn_174c8_entry &&
                           pc < g.fn_174c8_entry + 0x100u;
        (void)read_u32(sp + pci * 4u, &retpc);
        if (!inside_174c8 && sp + 0x10u >= g.fn_1d098_entry_sp) {
            g.await_next = 1;
            g.await_method = 200u;
            g.await_stack_pc = retpc;
            g.ldm_pc = pc;
            snprintf(note, sizeof(note), "fn_1d098_ldm_ret ->0x%X", retpc);
        }
    }

    /* Scheduler-looking branches after post-remove */
    if (g.saw_1d0e0 && in_cf && (insn & 0x0E000000u) == 0x0A000000u &&
        (insn & 0xF0000000u) != 0xE0000000u && !g.block_pc) {
        uint32_t cond = (insn >> 28) & 0xFu;
        uint32_t tgt = branch_target_arm(pc, insn);
        int z = (cpsr >> 30) & 1;
        int taken = 0;
        if (cond == 0) taken = z;
        else if (cond == 1) taken = !z;
        else taken = -1;
        if (taken >= 0) {
            /* skip the known null-check at +0x1D09C */
            if (off != 0x1D09Cu && off != 0x1D0A8u) {
                g.block_pc = pc;
                g.block_lhs = regs[0];
                g.block_rhs = regs[1];
                snprintf(g.block_path, sizeof(g.block_path), "%s ->0x%X",
                         taken ? "TAKEN" : "NOT_TAKEN", tgt);
                snprintf(note, sizeof(note), "sched_br");
                add_parent("sched_branch", pc, off, regs, sp, lr, g.block_path);
            }
        }
    }

    if (g.entered_f670 || g.entered_8cdc || g.entered_d978)
        add_parent("gamelist_sched", pc, pc - g.gl_base, regs, sp, lr, "callback_path");
    if (g.entered_10740) add_parent("enter_10740", pc, OFF_10740, regs, sp, lr, "cfg_path");
    if (g.entered_7b6c) add_parent("enter_7b6c", pc, OFF_7B6C, regs, sp, lr, "cfg_loader");

    /* Always record while dense, until cap */
    if (g.dense) emit_slice(pc, off, insn, regs, sp, lr, cpsr, note);

    /* Stop conditions — never stop solely on first CMP/Bxx */
    if (g.entered_10740 || g.entered_7b6c || g.entered_f670) {
        finish_dense("reached_schedule_target");
        return;
    }
    if (g.cf_insn_n >= P22M_CF_INSN_MAX) {
        finish_dense("cf_insn_200000");
        return;
    }
    if (g.slice_n >= P22M_SLICE_CAP) {
        finish_dense("slice_cap");
        return;
    }
    if (g.parent_returned && g.parent_follow && g.cf_insn_n > g.dense_from_insn + 64u) {
        finish_dense("parent_returned_after_consume");
        return;
    }
}

static void on_next_or_dense(uc_engine *uc, uint32_t pc, uint32_t size) {
    uint32_t regs[16];
    uint32_t sp = 0, lr = 0, cpsr = 0, insn = 0;
    int i;
    (void)size;
    if (!p22m_enabled() || g.finalized) return;

    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~1u, &insn);

    if (g.await_next && (pc & ~1u) != (g.ldm_pc & ~1u)) {
        if (g.await_method == 1u && !g.dense) {
            ensure_cf_base_from_pc(pc);
            maybe_export_image();
            g.dense = 1;
            g.dense_from_insn = g.cf_insn_n;
            install_mem_hooks();
            add_parent("begin_dense", pc,
                       (g.cf_base && pc >= g.cf_base && pc < g.cf_end) ? pc - g.cf_base : 0, regs,
                       sp, lr, "post_wrapper_m1");
            printf("[JJFB_P22M] dense_begin pc=0x%X r0=0x%X stack=0x%X cf_base=0x%X "
                   "evidence=OBSERVED\n",
                   pc, regs[0], g.await_stack_pc, g.cf_base);
            fflush(stdout);
        } else if (g.await_method == 200u && g.fn_1d098_entered && !g.fn_1d098_returned) {
            g.fn_1d098_returned = 1;
            g.fn_1d098_ret_pc = pc;
            g.fn_1d098_ret_r0 = regs[0];
            g.parent_follow = 1;
            g.parent_after_1d098 = pc;
            g.parent_after_1d098_sp = sp;
            add_parent("fn_1d098_return", pc,
                       (g.cf_base && pc >= g.cf_base && pc < g.cf_end) ? pc - g.cf_base : 0, regs,
                       sp, lr, "containing_fn_returned");
            add_life("fn_1d098_done", pc,
                     (g.cf_base && pc >= g.cf_base && pc < g.cf_end) ? pc - g.cf_base : 0,
                     "return_to_parent");
            /* Infer queue emptiness from object fields */
            if (g.obj_after_ok) {
                if (g.obj_after[1] == g.obj_after[3] ||
                    (g.obj_after[1] == 0 && g.obj_after[3] == 0)) {
                    g.queue_empty_guess = 1;
                    g.next_record_exists = 0;
                } else {
                    g.next_record_exists = 1;
                    g.queue_empty_guess = 0;
                }
            }
        } else if (g.await_method >= 201u) {
            g.parent_returned = 1;
            add_parent("parent_frame_return", pc,
                       (g.cf_base && pc >= g.cf_base && pc < g.cf_end) ? pc - g.cf_base : 0, regs,
                       sp, lr, "upper_caller");
        } else if (g.await_method != 1u && !g.dense) {
            /* intermediate m6/m0 — keep sparse only */
            remove_global_hook();
        }
        g.await_next = 0;
    }

    /* Track upper parent LDM while following */
    if (g.parent_follow && !g.parent_returned &&
        (insn & 0x0FFF8000u) == 0x08BD8000u && (insn & 0xF0000000u) == 0xE0000000u) {
        uint32_t rl = insn & 0xFFFFu;
        uint32_t pci = (uint32_t)popcount16(rl & 0x7FFFu);
        uint32_t retpc = 0;
        (void)read_u32(sp + pci * 4u, &retpc);
        g.await_next = 1;
        g.await_method = 201u;
        g.await_stack_pc = retpc;
        g.ldm_pc = pc;
    }

    /* Helper re-enter: PC at gamelist helper */
    if (g.dense && g.gl_helper && (pc & ~1u) == (g.gl_helper & ~1u)) {
        g.helper_reenter = 1;
        add_parent("helper_reenter", pc, 0, regs, sp, lr, "gamelist_helper");
        finish_dense("helper_reenter");
        return;
    }

    if (g.dense) on_dense_step(uc, pc, regs, sp, lr, cpsr, insn);
}

static void p22m_on_sparse(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t regs[16];
    uint32_t sp = 0, insn = 0;
    int i;
    (void)size;
    (void)user_data;
    if (!p22m_enabled() || g.finalized || !g.armed) return;
    if (pc < g.cont_pc || pc >= g.window_end) return;

    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &insn);

    if (insn == 0xE3A00002u && g.last_method == 1u) {
        printf("[JJFB_P22M] wrapper_mov_r0_2 method=1 evidence=OBSERVED\n");
        fflush(stdout);
    }

    if (insn == 0xE8BD8DF0u || ((insn & 0xFFFF0000u) == 0xE8BD0000u && (insn & 0x8000u))) {
        uint32_t rl = insn & 0xFFFFu;
        uint32_t pci = (uint32_t)popcount16(rl & 0x7FFFu);
        uint32_t pv = 0;
        g.ldm_pc = pc;
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + pci * 4u, &pv);
        g.await_next = 1;
        g.await_method = g.last_method;
        g.await_stack_pc = pv;
        install_global_hook();
        printf("[JJFB_P22M] ldm method=%u pc=0x%X stack_ret=0x%X r0=0x%X evidence=OBSERVED\n",
               g.last_method, pc, pv, regs[0]);
        fflush(stdout);
    }
}

static void p22m_on_global(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)user_data;
    on_next_or_dense(uc, (uint32_t)address, size);
}

static const char *field_name(uint32_t addr) {
    if (!g.object) return NULL;
    if (addr == g.object + 0x08u) return "+0x08";
    if (addr == g.object + 0x0Cu) return "+0x0C";
    if (addr == g.object + 0x10u) return "+0x10";
    if (addr == g.object + 0x14u) return "+0x14";
    if (addr == g.object + 0x18u) return "+0x18";
    if (addr == g.object + 0x30u) return "+0x30";
    return NULL;
}

static void p22m_on_mem(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                        void *user_data) {
    uint32_t pc = 0, off = 0, addr = (uint32_t)address;
    const char *fname;
    FieldWrite *w;
    (void)user_data;
    if (!g.dense || g.finalized) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    if (g.cf_base && pc >= g.cf_base && pc < g.cf_end) off = (pc & ~3u) - g.cf_base;

    if (g.slice_n > 0 && g.slice_n <= P22M_SLICE_CAP) {
        SliceRow *s = &g.slice[g.slice_n - 1];
        if (!s->mem[0]) {
            snprintf(s->mem, sizeof(s->mem), "%s:0x%X/%d", type == UC_MEM_READ ? "R" : "W", addr,
                     size);
        }
    }

    if (type != UC_MEM_WRITE) return;

    fname = field_name(addr);
    if (fname && g.write_n < P22M_WRITE_CAP) {
        uint32_t oldv = 0;
        (void)read_u32(addr, &oldv);
        w = &g.writes[g.write_n++];
        memset(w, 0, sizeof(*w));
        w->seq = g.write_n;
        w->pc = pc;
        w->off = off;
        w->addr = addr;
        w->size = (uint32_t)size;
        w->old_v = oldv;
        w->new_v = (uint32_t)value;
        snprintf(w->field, sizeof(w->field), "%s", fname);
        snprintf(w->phase, sizeof(w->phase), "%s", g.in_174c8 ? "in_174c8" : "other");
        if (!g.block_writer[0])
            snprintf(g.block_writer, sizeof(g.block_writer), "pc=0x%X off=0x%X %s", pc, off, fname);
    }

    if (g.derived_node && addr + (uint32_t)size > g.derived_node - P22M_NODE_PAD &&
        addr < g.derived_node + P22M_NODE_PAD) {
        add_prov(pc, off, addr, (uint32_t)value, "node_write",
                 g.in_174c8 ? "during_174c8" : (g.saw_174c8_ret ? "after_174c8" : "before_174c8"));
    }

    /* Early provenance: writes into node region before consume */
    if (!g.saw_174c8_enter && g.derived_node &&
        addr + (uint32_t)size > g.derived_node - P22M_NODE_PAD &&
        addr < g.derived_node + P22M_NODE_PAD) {
        if (!g.node_producer[0])
            snprintf(g.node_producer, sizeof(g.node_producer),
                     "write_pc=0x%X off=0x%X addr=0x%X val=0x%X", pc, off, addr, (uint32_t)value);
    }
}

static void install_sparse(void *uc, uint32_t cont) {
    uc_err ue;
    if (!uc || !cont || g.hook_sparse) return;
    g.cont_pc = cont & ~3u;
    g.window_end = g.cont_pc + P22M_WIN;
    g.ldm_pc = g.cont_pc + P22M_LDM_OFF;
    ue = uc_hook_add((uc_engine *)uc, &g.h_sparse, UC_HOOK_CODE, (void *)p22m_on_sparse, NULL,
                     (uint64_t)g.cont_pc, (uint64_t)(g.window_end - 1u));
    if (ue == UC_ERR_OK) {
        g.hook_sparse = 1;
        g.uc = uc;
        printf("[JJFB_P22M] sparse_hook=[0x%X,0x%X) evidence=DOCUMENTED\n", g.cont_pc,
               g.window_end);
        fflush(stdout);
    }
}

static void install_global_hook(void) {
    uc_err ue;
    if (!g.uc || g.hook_global) return;
    ue = uc_hook_add((uc_engine *)g.uc, &g.h_global, UC_HOOK_CODE, (void *)p22m_on_global, NULL, 1,
                     0);
    if (ue == UC_ERR_OK) {
        g.hook_global = 1;
        printf("[JJFB_P22M] global_hook=1 evidence=DOCUMENTED\n");
        fflush(stdout);
    }
}

static void remove_global_hook(void) {
    if (!g.uc || !g.hook_global) return;
    (void)uc_hook_del((uc_engine *)g.uc, g.h_global);
    g.hook_global = 0;
    g.h_global = 0;
}

static void install_mem_hooks(void) {
    uc_err ue;
    if (!g.uc || g.hook_mem) return;
    ue = uc_hook_add((uc_engine *)g.uc, &g.h_mem_r, UC_HOOK_MEM_READ, (void *)p22m_on_mem, NULL, 1,
                     0);
    if (ue != UC_ERR_OK) return;
    ue = uc_hook_add((uc_engine *)g.uc, &g.h_mem_w, UC_HOOK_MEM_WRITE, (void *)p22m_on_mem, NULL, 1,
                     0);
    if (ue == UC_ERR_OK) g.hook_mem = 1;
}

static void remove_mem_hooks(void) {
    if (!g.uc || !g.hook_mem) return;
    (void)uc_hook_del((uc_engine *)g.uc, g.h_mem_r);
    (void)uc_hook_del((uc_engine *)g.uc, g.h_mem_w);
    g.hook_mem = 0;
}

static void finish_dense(const char *why) {
    if (!g.dense || g.finalized) return;
    g.dense = 0;
    if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", why ? why : "dense_done");
    remove_mem_hooks();
    remove_global_hook();
    add_parent("end_dense", 0, 0, NULL, 0, 0, why);
}
#endif /* GWY_HAVE_UNICORN */

void p22m_reset(void) {
#ifdef GWY_HAVE_UNICORN
    void *uc = g.uc;
    int hs = g.hook_sparse, hg = g.hook_global, hm = g.hook_mem;
    uc_hook a = g.h_sparse, b = g.h_global, c = g.h_mem_r, d = g.h_mem_w;
#endif
    memset(&g, 0, sizeof(g));
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        if (hs) (void)uc_hook_del((uc_engine *)uc, a);
        if (hg) (void)uc_hook_del((uc_engine *)uc, b);
        if (hm) {
            (void)uc_hook_del((uc_engine *)uc, c);
            (void)uc_hook_del((uc_engine *)uc, d);
        }
    }
#endif
}

void p22m_bind_uc(void *uc) {
    const char *rid;
    if (!p22m_enabled()) return;
    g.uc = uc;
    rid = getenv("JJFB_P22M_RUN_ID");
    if (!rid || !rid[0]) rid = getenv("JJFB_P22I_RUN_ID");
    if (rid && rid[0]) snprintf(g.run_id, sizeof(g.run_id), "%s", rid);
}

void p22m_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, const char *package_owner) {
    if (!p22m_enabled() || !module_name) return;
    if (strstr(module_name, "gamelist")) {
        g.gl_base = base;
        g.gl_end = base + size;
        if (erw) g.gl_erw = erw;
    }
    if (strstr(module_name, "cfunction")) {
        g.cf_base = base;
        g.cf_size = size;
        g.cf_end = base + size;
        if (erw) g.cf_erw = erw;
#ifdef GWY_HAVE_UNICORN
        if (g.uc) maybe_export_image();
#endif
    }
    if (p_guest) g.p_guest = p_guest;
    if (generation) g.generation = generation;
    if (package_owner && package_owner[0])
        snprintf(g.package_owner, sizeof(g.package_owner), "%s", package_owner);
}

void p22m_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       uint32_t sp) {
    if (!p22m_enabled() || g.finalized || !continuation_pc) return;
    if (!g.uc) g.uc = uc;
    /* Dense slice owns the global hook — do not re-arm sparse on FIRE_EXT method=2 etc. */
    if (g.dense) {
        printf("[JJFB_P22M] skip_arm_while_dense cont=0x%X method=%u evidence=DOCUMENTED\n",
               continuation_pc, method);
        fflush(stdout);
        return;
    }
    g.armed = 1;
    g.last_method = method;
#ifdef GWY_HAVE_UNICORN
    ensure_cf_base_from_pc(continuation_pc);
    maybe_export_image();
    install_sparse(g.uc ? g.uc : uc, continuation_pc);
#else
    (void)uc;
#endif
    printf("[JJFB_P22M] arm_continuation=0x%X method=%u sp=0x%X cf_base=0x%X evidence=OBSERVED\n",
           continuation_pc, method, sp, g.cf_base);
    fflush(stdout);
    (void)sp;
}

void p22m_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr) {
    (void)uc;
    (void)module_name;
    (void)pc;
    (void)regs;
    (void)lr;
    (void)sp;
    (void)cpsr;
    if (!p22m_enabled() || g.finalized) return;
    note_gl_off(pc);
}

static void classify(void) {
    if (g.entered_10740) {
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "post-consume path reached +0x10740 (unexpected for this gate)");
        snprintf(g.next_fix, sizeof(g.next_fix), "observe once-flag/mode inside +0x10740");
    } else if (g.saw_174c8_ret && g.block_pc && !g.callback_pub && !g.entered_f670) {
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "init node 0x%X unlinked via +0x174C8; dispatcher branch @0x%X %s "
                 "(lhs=0x%X); continues draining object list but never enqueues "
                 "callback/UI/+0x10740 after natural 6→0→1",
                 g.derived_node, g.block_pc, g.block_path[0] ? g.block_path : "?", g.block_lhs);
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "find natural producer that sets object flag bits (TST #0xC path) or enqueues "
                 "UI/init record after helper 6/0/1; do not inject");
        if (!g.block_field[0])
            snprintf(g.block_field, sizeof(g.block_field),
                     "object+0x30 flags / list +0x0C+0x14; missing UI enqueue");
        if (!g.block_producer[0])
            snprintf(g.block_producer, sizeof(g.block_producer),
                     "missing_UI_init_enqueuer_after_helper_6_0_1");
        g.queue_empty_guess = (g.obj_after_ok && g.obj_before_ok &&
                               g.obj_after[1] != g.obj_before[1])
                                  ? 0
                                  : g.queue_empty_guess;
    } else if (g.saw_174c8_ret && g.fn_1d098_returned && !g.helper_reenter && !g.callback_pub) {
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "init/index node consumed via +0x174C8; containing state machine returned "
                 "without callback/UI/init schedule");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "find natural producer that enqueues UI/init record after method 6/0/1; do not "
                 "inject");
        if (!g.block_producer[0])
            snprintf(g.block_producer, sizeof(g.block_producer),
                     "missing_UI_init_enqueuer_after_helper_6_0_1");
    } else if (g.saw_174c8_ret && g.next_record_exists && !g.callback_pub) {
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "node consumed but residual list entry exists; dispatcher did not publish "
                 "callback (ptr/owner/R9 may be invalid)");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "dump residual node fields; validate callback/helper/method pointers");
    } else if (g.saw_174c8_enter && !g.saw_174c8_ret) {
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "+0x174C8 entered but did not return within observation window");
        snprintf(g.next_fix, sizeof(g.next_fix), "raise insn budget / check hang inside unlink");
    } else if (!g.saw_1d098) {
        snprintf(g.sole_lock, sizeof(g.sole_lock), "did not reach +0x1D098 in this run");
        snprintf(g.next_fix, sizeof(g.next_fix), "verify m1 wrapper return arming");
    } else {
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "post-consume state observed without UI/callback schedule");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "inspect p22m_parent_state_timeline / post_remove_slice for first idle branch");
    }

    if (!g.sem_17970[0])
        snprintf(g.sem_17970, sizeof(g.sem_17970),
                 "index→node: R0=slot_index, [r4+8]=array_base, return base-(R0<<3)");
    if (!g.sem_1d098[0])
        snprintf(g.sem_1d098, sizeof(g.sem_1d098),
                 "validate derived node then call +0x174C8(object, ?, node)");
    if (!g.sem_174c8[0])
        snprintf(g.sem_174c8, sizeof(g.sem_174c8),
                 "container unlink/pop on object list fields (+0x0C/+0x14/+0x30 flags)");

    if (!g.node_type[0]) {
        if (g.index_r0 == 2u)
            snprintf(g.node_type, sizeof(g.node_type),
                     "slot_index=%u_temp_stack_or_array_node_from_wrapper_R0", g.index_r0);
        else
            snprintf(g.node_type, sizeof(g.node_type), "derived_node_index=%u", g.index_r0);
    }
    if (!g.node_producer[0])
        snprintf(g.node_producer, sizeof(g.node_producer),
                 "wrapper_final_R0=2_selects_preexisting_slot; slot table filled earlier in "
                 "cfunction init (see provenance writes)");
}

static void write_hex_dump(FILE *f, const uint8_t *b, uint32_t base_addr) {
    uint32_t i;
    if (!f || !b) return;
    for (i = 0; i < 0x80u; i += 16u) {
        uint32_t j;
        fprintf(f, "0x%X:", base_addr + i);
        for (j = 0; j < 16u; j++) fprintf(f, " %02X", b[i + j]);
        fputc('\n', f);
    }
}

static void write_artifacts(void) {
    FILE *f;
    uint32_t i;

    f = open_out("JJFB_P22M_OBJ_CSV", "reports/p22m/p22m_object_before_after.csv");
    if (f) {
        fprintf(f, "phase,object,plus08,plus0C,plus10,plus14,plus18,plus30\n");
        if (g.obj_before_ok)
            fprintf(f, "before,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n", g.object, g.obj_before[0],
                    g.obj_before[1], g.obj_before[2], g.obj_before[3], g.obj_before[4],
                    g.obj_before[5]);
        if (g.obj_after_ok)
            fprintf(f, "after,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n", g.object, g.obj_after[0],
                    g.obj_after[1], g.obj_after[2], g.obj_after[3], g.obj_after[4],
                    g.obj_after[5]);
        fclose(f);
    }

    f = open_out("JJFB_P22M_NODE_CSV", "reports/p22m/p22m_node_memory_snapshot.csv");
    if (f) {
        fprintf(f, "phase,node,window_base,note\n");
        fprintf(f, "meta,0x%X,0x%X,\"pad=0x40 before/after\"\n", g.derived_node,
                g.derived_node ? g.derived_node - P22M_NODE_PAD : 0);
        if (g.node_before_ok) {
            fprintf(f, "before_hex,0x%X,0x%X,\"dump_follows\"\n", g.derived_node,
                    g.derived_node - P22M_NODE_PAD);
            write_hex_dump(f, g.node_before, g.derived_node - P22M_NODE_PAD);
        }
        if (g.node_after_ok) {
            fprintf(f, "after_hex,0x%X,0x%X,\"dump_follows\"\n", g.derived_node,
                    g.derived_node - P22M_NODE_PAD);
            write_hex_dump(f, g.node_after, g.derived_node - P22M_NODE_PAD);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22M_WRITES_CSV", "reports/p22m/p22m_object_field_writes.csv");
    if (f) {
        fprintf(f, "seq,pc,off,addr,size,field,phase,old,new\n");
        for (i = 0; i < g.write_n; i++) {
            FieldWrite *w = &g.writes[i];
            fprintf(f, "%u,0x%X,0x%X,0x%X,%u,%s,%s,0x%X,0x%X\n", w->seq, w->pc, w->off, w->addr,
                    w->size, w->field, w->phase, w->old_v, w->new_v);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22M_SLICE_CSV", "reports/p22m/p22m_post_remove_slice.csv");
    if (f) {
        fprintf(f,
                "seq,pc,off,insn,desc,r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,sp,lr,cpsr,note,"
                "mem\n");
        for (i = 0; i < g.slice_n; i++) {
            SliceRow *r = &g.slice[i];
            fprintf(f,
                    "%u,0x%X,0x%X,0x%08X,\"%s\",0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\",\"%s\"\n",
                    r->seq, r->pc, r->off, r->insn, r->desc, r->r[0], r->r[1], r->r[2], r->r[3],
                    r->r[4], r->r[5], r->r[6], r->r[7], r->r[8], r->r[9], r->r[10], r->r[11],
                    r->r[12], r->sp, r->lr, r->cpsr, r->note, r->mem);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22M_PARENT_CSV", "reports/p22m/p22m_parent_state_timeline.csv");
    if (f) {
        fprintf(f, "seq,event,pc,off,r0,r1,r2,r3,r4,r9,sp,lr,detail\n");
        for (i = 0; i < g.parent_n; i++) {
            ParentEv *p = &g.parent[i];
            fprintf(f, "%u,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\"\n",
                    p->seq, p->event, p->pc, p->off, p->r0, p->r1, p->r2, p->r3, p->r4, p->r9,
                    p->sp, p->lr, p->detail);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22M_CHAIN_MD", "reports/p22m/p22m_parent_return_chain.md");
    if (f) {
        fprintf(f, "# P22M parent return chain\n\n");
        fprintf(f, "- fn_1d098_entry=0x%X sp=0x%X\n", g.fn_1d098_entry, g.fn_1d098_entry_sp);
        fprintf(f, "- call_174c8 @ +0x1D0DC r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X sp=0x%X "
                   "lr/cont=0x%X\n",
                g.call_174c8_r0, g.call_174c8_r1, g.call_174c8_r2, g.call_174c8_r3,
                g.call_174c8_r9, g.call_174c8_sp, g.call_174c8_lr);
        fprintf(f, "- ret_174c8 r0=0x%X continuation=+0x1D0E0 seen=%d\n", g.ret_174c8_r0,
                g.saw_1d0e0);
        fprintf(f, "- fn_1d098_returned=%d ret_pc=0x%X ret_r0=0x%X\n", g.fn_1d098_returned,
                g.fn_1d098_ret_pc, g.fn_1d098_ret_r0);
        fprintf(f, "- parent_after=0x%X parent_returned=%d helper_reenter=%d\n\n",
                g.parent_after_1d098, g.parent_returned, g.helper_reenter);
        fprintf(f, "## Timeline\n\n");
        for (i = 0; i < g.parent_n; i++) {
            ParentEv *p = &g.parent[i];
            fprintf(f, "%u. `%s` pc=0x%X off=0x%X r0=0x%X r2=0x%X — %s\n", p->seq, p->event, p->pc,
                    p->off, p->r0, p->r2, p->detail);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22M_PROV_CSV", "reports/p22m/p22m_node_provenance.csv");
    if (f) {
        fprintf(f, "seq,pc,off,addr,value,kind,note\n");
        for (i = 0; i < g.prov_n; i++) {
            ProvRow *r = &g.prov[i];
            fprintf(f, "%u,0x%X,0x%X,0x%X,0x%X,%s,\"%s\"\n", r->seq, r->pc, r->off, r->addr,
                    r->value, r->kind, r->note);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22M_LIFE_CSV", "reports/p22m/p22m_queue_lifecycle.csv");
    if (f) {
        fprintf(f, "seq,event,pc,off,object,node,f08,f0c,f10,f14,f18,f30,detail\n");
        for (i = 0; i < g.life_n; i++) {
            LifeRow *r = &g.life[i];
            fprintf(f, "%u,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\"\n",
                    r->seq, r->event, r->pc, r->off, r->obj, r->node, r->f08, r->f0c, r->f10,
                    r->f14, r->f18, r->f30, r->detail);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22M_XREF_CSV", "reports/p22m/p22m_function_xrefs.csv");
    if (f) {
        fprintf(f, "caller_pc,caller_off,target_off,kind\n");
        for (i = 0; i < g.xref_n; i++) {
            XrefRow *x = &g.xrefs[i];
            fprintf(f, "0x%X,0x%X,0x%X,%s\n", x->caller_pc, x->caller_off, x->target_off, x->kind);
        }
        fprintf(f, "# fn_entries entry_174c8=0x%X end=0x%X entry_17970=0x%X end=0x%X "
                   "entry_1d098=0x%X\n",
                g.fn_174c8_entry, g.fn_174c8_end, g.fn_17970_entry, g.fn_17970_end,
                g.fn_1d098_entry);
        fclose(f);
    }

    f = open_out("JJFB_P22M_VERDICT", "reports/p22m/p22m_scheduler_verdict.md");
    if (f) {
        fprintf(f,
                "# P22M-CLEAN cfunction queue/scheduler verdict\n\n"
                "## Bottom line\n\n%s\n\n"
                "## PASS answers\n\n```\n"
                "cfunction runtime image SHA：%s\n"
                "+0x17970 真实函数作用：%s\n"
                "+0x1D098 所属函数：entry=0x%X — %s\n"
                "+0x174C8 真实作用：%s\n"
                "\n"
                "派生节点地址：0x%X\n"
                "节点类型/tag：%s\n"
                "节点自然生产者：%s\n"
                "\n"
                "object +0x08：before=0x%X after=0x%X\n"
                "object +0x0C：before=0x%X after=0x%X\n"
                "object +0x14：before=0x%X after=0x%X\n"
                "object +0x30：before=0x%X after=0x%X\n"
                "调用前后变化：+0x0C %s; +0x14 %s; +0x30 %s\n"
                "\n"
                "+0x174C8 返回值：0x%X\n"
                "返回 continuation：0x%X (+0x1D0E0)\n"
                "+0x1D098 所属函数最终返回：%s pc=0x%X r0=0x%X\n"
                "上层调用者：0x%X\n"
                "\n"
                "节点消费后队列状态：%s\n"
                "下一条调度记录是否存在：%s\n"
                "callback/helper/event 是否存在：callback=%s helper_reenter=%s\n"
                "为什么没有继续初始化：%s\n"
                "\n"
                "第一条真实阻断分支：pc=0x%X %s\n"
                "实际操作数：lhs=0x%X rhs=0x%X\n"
                "调度目标路径：%s\n"
                "字段最后写入者：%s\n"
                "自然生产者：%s\n"
                "\n"
                "+0x10740 是否进入：%s\n"
                "+0x7B6C 是否进入：%s\n"
                "真实 cfg open：%s\n"
                "真实游戏画面：NO\n"
                "\n"
                "是否修改 Guest：NO\n"
                "当前唯一门锁：%s\n"
                "下一处最小通用修复：%s\n"
                "```\n\n"
                "## Evidence\n\n"
                "- run_id=%s cf_base=0x%X cf_end=0x%X gen=%llu\n"
                "- call_174c8 r0=0x%X r1=0x%X r2=0x%X r3=0x%X\n"
                "- slice_n=%u parent_n=%u writes=%u cf_insn=%u\n"
                "- entered F670=%d 8CDC=%d D978=%d 10740=%d 7B6C=%d\n"
                "- stop_reason=%s\n",
                g.sole_lock, g.cf_sha[0] ? g.cf_sha : "NOT_EXPORTED", g.sem_17970,
                g.fn_1d098_entry, g.sem_1d098, g.sem_174c8, g.derived_node, g.node_type,
                g.node_producer[0] ? g.node_producer : "UNKNOWN",
                g.obj_before_ok ? g.obj_before[0] : 0, g.obj_after_ok ? g.obj_after[0] : 0,
                g.obj_before_ok ? g.obj_before[1] : 0, g.obj_after_ok ? g.obj_after[1] : 0,
                g.obj_before_ok ? g.obj_before[3] : 0, g.obj_after_ok ? g.obj_after[3] : 0,
                g.obj_before_ok ? g.obj_before[5] : 0, g.obj_after_ok ? g.obj_after[5] : 0,
                (g.obj_before_ok && g.obj_after_ok && g.obj_before[1] != g.obj_after[1])
                    ? "CHANGED"
                    : "same/unknown",
                (g.obj_before_ok && g.obj_after_ok && g.obj_before[3] != g.obj_after[3])
                    ? "CHANGED"
                    : "same/unknown",
                (g.obj_before_ok && g.obj_after_ok && g.obj_before[5] != g.obj_after[5])
                    ? "CHANGED"
                    : "same/unknown",
                g.ret_174c8_r0, g.call_174c8_lr ? g.call_174c8_lr : (g.cf_base + OFF_1D0E0),
                g.fn_1d098_returned ? "YES" : "NO", g.fn_1d098_ret_pc, g.fn_1d098_ret_r0,
                g.parent_after_1d098,
                g.queue_empty_guess ? "empty_or_idle_links" : (g.next_record_exists ? "residual" : "unknown"),
                g.next_record_exists ? "YES" : "NO", g.callback_pub ? "YES" : "NO",
                g.helper_reenter ? "YES" : "NO", g.sole_lock, g.block_pc,
                g.block_path[0] ? g.block_path : "NONE", g.block_lhs, g.block_rhs,
                g.block_path[0] ? g.block_path : "no_UI_callback_path",
                g.block_writer[0] ? g.block_writer : "see_writes_csv",
                g.block_producer[0] ? g.block_producer : g.node_producer,
                g.entered_10740 ? "YES" : "NO", g.entered_7b6c ? "YES" : "NO",
                g.cfg_open ? "YES" : "NO", g.sole_lock, g.next_fix, g.run_id, g.cf_base, g.cf_end,
                (unsigned long long)g.generation, g.call_174c8_r0, g.call_174c8_r1,
                g.call_174c8_r2, g.call_174c8_r3, g.slice_n, g.parent_n, g.write_n, g.cf_insn_n,
                g.entered_f670, g.entered_8cdc, g.entered_d978, g.entered_10740, g.entered_7b6c,
                g.stop_reason[0] ? g.stop_reason : "finalize");
        fclose(f);
    }

    f = open_out("JJFB_P22M_SUMMARY", "out/p22m/p22m_runtime_summary.txt");
    if (f) {
        fprintf(f,
                "run_id=%s\ncf_base=0x%X\ncf_end=0x%X\ncf_sha=%s\n"
                "object=0x%X\nnode=0x%X\nindex_r0=%u\nindex_base=0x%X\n"
                "ret_174c8=0x%X\nfn_1d098_ret=%d\nfn_1d098_ret_pc=0x%X\n"
                "queue_empty=%d\nnext_record=%d\n"
                "callback=%d\nentered_10740=%d\nentered_7b6c=%d\n"
                "slice_n=%u\nparent_n=%u\ncf_insn=%u\n"
                "saw_174c8_ret=%d\nsaw_1d0e0=%d\n"
                "sole_lock=%s\nnext_fix=%s\nstop_reason=%s\n"
                "guest_modified=NO\n",
                g.run_id, g.cf_base, g.cf_end, g.cf_sha[0] ? g.cf_sha : "?", g.object,
                g.derived_node, g.index_r0, g.index_base, g.ret_174c8_r0, g.fn_1d098_returned,
                g.fn_1d098_ret_pc, g.queue_empty_guess, g.next_record_exists, g.callback_pub,
                g.entered_10740, g.entered_7b6c, g.slice_n, g.parent_n, g.cf_insn_n,
                g.saw_174c8_ret, g.saw_1d0e0, g.sole_lock, g.next_fix,
                g.stop_reason[0] ? g.stop_reason : "finalize");
        fclose(f);
    }
}

void p22m_finalize(const char *stop_reason) {
    if (!p22m_enabled() || g.finalized) return;
    g.finalized = 1;
    if (stop_reason && stop_reason[0] && !g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", stop_reason);
    else if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "finalize");
#ifdef GWY_HAVE_UNICORN
    if (g.dense) finish_dense(g.stop_reason);
    remove_mem_hooks();
    remove_global_hook();
    maybe_export_image();
#endif
    classify();
    write_artifacts();
    printf("[JJFB_P22M_FINAL] sha=%s node=0x%X obj=0x%X ret174c8=0x%X fn_ret=%d "
           "10740=%d lock=%s evidence=OBSERVED\n",
           g.cf_sha[0] ? g.cf_sha : "?", g.derived_node, g.object, g.ret_174c8_r0,
           g.fn_1d098_returned, g.entered_10740, g.sole_lock);
    fflush(stdout);
}