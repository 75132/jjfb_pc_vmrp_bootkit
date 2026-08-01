#include "gwy_launcher/p22l_parent_return.h"

#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define P22L_WIN 0x200u
#define P22L_LDM_OFF 0x48u /* 0x89C38 - 0x89BF0 */
#define P22L_RET_CAP 16
#define P22L_LDM_CAP 8
#define P22L_NEXT_CAP 8
#define P22L_SLICE_CAP 768
#define P22L_POST_CAP 256
#define P22L_MEM_CAP 128
#define OFF_F670 0xF670u
#define OFF_8CDC 0x8CDCu
#define OFF_D978 0xD978u
#define OFF_10740 0x10740u
#define OFF_10814 0x10814u
#define OFF_7B6C 0x7B6Cu

typedef struct {
    uint32_t seq;
    uint32_t method;
    int32_t helper_return_r0;
    uint32_t cont_pc;
    uint32_t sp;
    uint32_t r9;
    uint32_t wrapper_final_r0;
    int saw_mov_r0_2;
    int saw_ldm;
    char note[48];
} HelperRet;

typedef struct {
    uint32_t seq;
    uint32_t method;
    uint32_t ldm_pc;
    uint32_t insn;
    uint32_t pre_ldm_sp;
    uint32_t reglist;
    uint32_t pc_index;
    uint32_t pc_stack_address;
    uint32_t pc_stack_value;
    uint32_t post_ldm_sp_expected;
    uint32_t r0_at_ldm;
    char decoded_reglist[80];
} LdmRow;

typedef struct {
    uint32_t seq;
    uint32_t method;
    uint32_t stack_decoded_pc;
    uint32_t next_executed_pc;
    uint32_t next_module_base;
    uint32_t next_module_offset;
    char next_module[48];
    int match;
    uint32_t r0_at_next;
    uint32_t sp_at_next;
    uint32_t lr_at_next;
    uint32_t cpsr_at_next;
    uint32_t r9_at_next;
} NextPcRow;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t insn;
    char desc[56];
    uint32_t r[13];
    uint32_t sp, lr, cpsr, r9;
    char module[40];
    uint32_t module_offset;
    char note[64];
    char mem[48];
} SliceRow;

typedef struct {
    uint32_t seq;
    char event[40];
    uint32_t pc;
    uint32_t off;
    uint32_t r0, r1, r9;
    char detail[80];
} PostRow;

static struct {
    int armed;
    int finalized;
    int hook_sparse;
    int hook_global;
    int hook_mem;
    void *uc;
    uint32_t cont_pc;
    uint32_t window_end;
    uint32_t ldm_pc;
    uint32_t last_method;
    uint32_t cf_base, cf_end, gl_base, gl_end, gl_erw, cf_erw;

    HelperRet rets[P22L_RET_CAP];
    uint32_t ret_n;
    int have_m6, have_m0, have_m1;
    int32_t r0_m6, r0_m0, r0_m1;
    uint32_t wrapper_final_r0;
    int saw_wrapper_final;

    LdmRow ldms[P22L_LDM_CAP];
    uint32_t ldm_n;
    int await_next;
    uint32_t await_method;
    uint32_t await_stack_pc;
    NextPcRow nexts[P22L_NEXT_CAP];
    uint32_t next_n;

    int parent_slice;
    uint32_t slice_n;
    SliceRow slice[P22L_SLICE_CAP];
    uint32_t parent_entry_pc;
    uint32_t parent_entry_sp;
    char parent_module[48];
    uint32_t parent_module_offset;
    uint32_t parent_callsite;
    int r0_consumed;
    uint32_t r0_consume_pc;
    char r0_consume_insn[56];
    uint32_t cmp_lhs, cmp_rhs;
    char branch_taken[48];
    char branch_target[48];
    char r0_meaning[80];
    uint32_t stop_slice_n;

    int entered_f670, entered_8cdc, entered_d978;
    int entered_10740, entered_10814, entered_7b6c;
    int callback_pub;
    int cfg_open;
    int saw_indirect_call;
    int parent_returned;

    PostRow posts[P22L_POST_CAP];
    uint32_t post_n;
    uint32_t mem_n;

    char run_id[64];
    char verdict_class[8];
    char sole_lock[200];
    char next_fix[200];
    char stop_reason[80];
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

int p22l_enabled(void) {
    const char *e = getenv("JJFB_P22L_CLEAN");
    return e && e[0] == '1';
}

static int popcount16(uint32_t x) {
    int n = 0;
    x &= 0xFFFFu;
    while (x) {
        n += (int)(x & 1u);
        x >>= 1;
    }
    return n;
}

static void format_reglist(uint32_t reglist, char *out, size_t n) {
    size_t used = 0;
    int i, first = 1;
    used = (size_t)snprintf(out, n, "{");
    for (i = 0; i < 16; i++) {
        if (!(reglist & (1u << i))) continue;
        if (used + 8 >= n) break;
        if (i == 15)
            used += (size_t)snprintf(out + used, n - used, "%spc", first ? "" : ",");
        else if (i == 14)
            used += (size_t)snprintf(out + used, n - used, "%slr", first ? "" : ",");
        else if (i == 13)
            used += (size_t)snprintf(out + used, n - used, "%ssp", first ? "" : ",");
        else
            used += (size_t)snprintf(out + used, n - used, "%sr%d", first ? "" : ",", i);
        first = 0;
    }
    if (used + 2 < n) snprintf(out + used, n - used, "}");
}

static void resolve_mod(uint32_t pc, char *name, size_t nlen, uint32_t *base_out, uint32_t *off_out) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    uint32_t norm = pc & ~1u;
    if (name && nlen) name[0] = 0;
    if (base_out) *base_out = 0;
    if (off_out) *off_out = 0;
    if (g.cf_base && norm >= g.cf_base && norm < g.cf_end) {
        if (name) snprintf(name, nlen, "cfunction.ext");
        if (base_out) *base_out = g.cf_base;
        if (off_out) *off_out = norm - g.cf_base;
        return;
    }
    if (g.gl_base && norm >= g.gl_base && norm < g.gl_end) {
        if (name) snprintf(name, nlen, "gamelist.ext");
        if (base_out) *base_out = g.gl_base;
        if (off_out) *off_out = norm - g.gl_base;
        return;
    }
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_code_addr(reg, norm) : NULL;
    if (m) {
        const char *nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
        if (name) snprintf(name, nlen, "%s", nm ? nm : "?");
        if (base_out) *base_out = m->map.guest_code_base;
        if (off_out && m->map.guest_code_base && norm >= m->map.guest_code_base)
            *off_out = norm - m->map.guest_code_base;
    } else if (name) {
        snprintf(name, nlen, "UNKNOWN");
    }
}

static FILE *open_out(const char *envk, const char *defpath) {
    const char *p = env_or(envk, defpath);
    return fopen(p, "wb");
}

static void add_post(const char *ev, uint32_t pc, uint32_t off, uint32_t r0, uint32_t r1,
                     uint32_t r9, const char *detail) {
    PostRow *p;
    if (g.post_n >= P22L_POST_CAP) return;
    p = &g.posts[g.post_n++];
    memset(p, 0, sizeof(*p));
    p->seq = g.post_n;
    snprintf(p->event, sizeof(p->event), "%s", ev ? ev : "?");
    p->pc = pc;
    p->off = off;
    p->r0 = r0;
    p->r1 = r1;
    p->r9 = r9;
    snprintf(p->detail, sizeof(p->detail), "%s", detail ? detail : "");
    printf("[JJFB_P22L] post=%s pc=0x%X off=0x%X r0=0x%X evidence=OBSERVED\n", p->event, pc, off,
           r0);
    fflush(stdout);
}

static void note_gl_off(uint32_t pc, const uint32_t regs[16]) {
    uint32_t off;
    if (!g.gl_base || (pc & ~1u) < g.gl_base || (pc & ~1u) >= g.gl_end) return;
    off = (pc & ~1u) - g.gl_base;
    if (off == OFF_F670 && !g.entered_f670) {
        g.entered_f670 = 1;
        g.callback_pub = 1;
        add_post("enter_+0xF670", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "callback");
    } else if (off == OFF_8CDC && !g.entered_8cdc) {
        g.entered_8cdc = 1;
        g.callback_pub = 1;
        add_post("enter_+0x8CDC", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "callback");
    } else if (off == OFF_D978 && !g.entered_d978) {
        g.entered_d978 = 1;
        add_post("enter_+0xD978", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "");
    } else if (off == OFF_10740 && !g.entered_10740) {
        g.entered_10740 = 1;
        add_post("enter_+0x10740", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "UI_init");
    } else if (off == OFF_10814 && !g.entered_10814) {
        g.entered_10814 = 1;
        add_post("enter_+0x10814", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "");
    } else if (off == OFF_7B6C && !g.entered_7b6c) {
        g.entered_7b6c = 1;
        g.cfg_open = 1;
        add_post("enter_+0x7B6C", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "cfg_loader");
    }
}

static void describe_arm(uint32_t w, uint32_t pc, char *out, size_t n) {
    int32_t imm;
    uint32_t tgt;
    uint32_t rl;
    if ((w & 0x0FFFFFF0u) == 0x012FFF30u) {
        snprintf(out, n, "BLX r%u", w & 0xFu);
        return;
    }
    if ((w & 0x0FFFFFF0u) == 0x012FFF10u) {
        snprintf(out, n, "BX r%u", w & 0xFu);
        return;
    }
    if ((w & 0x0F000000u) == 0x0A000000u) {
        imm = (int32_t)(w & 0x00FFFFFFu);
        if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
        tgt = (uint32_t)((int32_t)pc + 8 + (imm << 2));
        snprintf(out, n, "B%s 0x%X", (w & 0xF0000000u) == 0x0A000000u ? "EQ" :
                                      (w & 0xF0000000u) == 0x1A000000u ? "NE" :
                                      (w & 0xF0000000u) == 0xEA000000u ? "" : "cond",
                 tgt);
        return;
    }
    if ((w & 0x0F000000u) == 0x0B000000u) {
        imm = (int32_t)(w & 0x00FFFFFFu);
        if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
        tgt = (uint32_t)((int32_t)pc + 8 + (imm << 2));
        snprintf(out, n, "BL 0x%X", tgt);
        return;
    }
    if (w == 0xE3A00002u) {
        snprintf(out, n, "MOV r0,#2");
        return;
    }
    if (w == 0xE1A05000u) {
        snprintf(out, n, "MOV r5,r0");
        return;
    }
    if ((w & 0xFFFF0000u) == 0xE3500000u) {
        snprintf(out, n, "CMP r0,#0x%X", w & 0xFFFu);
        return;
    }
    if ((w & 0xFFF00000u) == 0xE3500000u) {
        snprintf(out, n, "CMP r%u,#0x%X", (w >> 16) & 0xFu, w & 0xFFu);
        return;
    }
    if ((w & 0x0FF00000u) == 0x03500000u) {
        snprintf(out, n, "CMP r%u,#0x%X", (w >> 16) & 0xFu, w & 0xFFu);
        return;
    }
    if ((w & 0x0FF00000u) == 0x03100000u) {
        snprintf(out, n, "TST r%u,#0x%X", (w >> 16) & 0xFu, w & 0xFFu);
        return;
    }
    if ((w & 0x0E500000u) == 0x04100000u && ((w >> 12) & 0xFu) == 0) {
        snprintf(out, n, "STR r0,[...]");
        return;
    }
    if ((w & 0xFFFF0000u) == 0xE8BD0000u) {
        rl = w & 0xFFFFu;
        {
            char rlbuf[72];
            format_reglist(rl, rlbuf, sizeof(rlbuf));
            snprintf(out, n, "LDMFD sp!,%s", rlbuf);
        }
        return;
    }
    if ((w & 0xFFF00000u) == 0xE59D0000u) {
        snprintf(out, n, "LDR r%u,[sp,#0x%X]", (w >> 12) & 0xFu, w & 0xFFFu);
        return;
    }
    snprintf(out, n, "w=0x%08X", w);
}

static HelperRet *ret_for_method(uint32_t method) {
    uint32_t i;
    for (i = 0; i < g.ret_n; i++) {
        if (g.rets[i].method == method) return &g.rets[i];
    }
    if (g.ret_n >= P22L_RET_CAP) return NULL;
    {
        HelperRet *r = &g.rets[g.ret_n++];
        memset(r, 0, sizeof(*r));
        r->seq = g.ret_n;
        r->method = method;
        return r;
    }
}

static void record_helper_return_at_cont(uint32_t method, uint32_t pc, const uint32_t regs[16],
                                         uint32_t sp) {
    HelperRet *r = ret_for_method(method);
    if (!r || r->cont_pc) return; /* first hit only */
    r->helper_return_r0 = (int32_t)regs[0];
    r->cont_pc = pc;
    r->sp = sp;
    r->r9 = regs[9];
    snprintf(r->note, sizeof(r->note), "CONT_R0_BEFORE_INSN");
    if (method == 6u) {
        g.have_m6 = 1;
        g.r0_m6 = r->helper_return_r0;
    } else if (method == 0u) {
        g.have_m0 = 1;
        g.r0_m0 = r->helper_return_r0;
    } else if (method == 1u) {
        g.have_m1 = 1;
        g.r0_m1 = r->helper_return_r0;
    }
    printf("[JJFB_P22L] helper_real_return method=%u r0=%d(0x%X) cont=0x%X evidence=OBSERVED "
           "note=CONT_R0_BEFORE_INSN\n",
           method, (int)r->helper_return_r0, (uint32_t)r->helper_return_r0, pc);
    fflush(stdout);
}

static void decode_ldm_at(void *uc, uint32_t method, uint32_t pc, uint32_t insn, uint32_t sp,
                          uint32_t r0) {
    LdmRow *row;
    uint32_t reglist = insn & 0xFFFFu;
    uint32_t pc_index;
    uint32_t pc_addr;
    uint32_t pc_val = 0;
    int nregs;
    if (g.ldm_n >= P22L_LDM_CAP) return;
    row = &g.ldms[g.ldm_n++];
    memset(row, 0, sizeof(*row));
    row->seq = g.ldm_n;
    row->method = method;
    row->ldm_pc = pc;
    row->insn = insn;
    row->pre_ldm_sp = sp;
    row->reglist = reglist;
    nregs = popcount16(reglist);
    pc_index = (uint32_t)popcount16(reglist & 0x7FFFu);
    pc_addr = sp + pc_index * 4u;
    row->pc_index = pc_index;
    row->pc_stack_address = pc_addr;
    row->post_ldm_sp_expected = sp + (uint32_t)nregs * 4u;
    row->r0_at_ldm = r0;
    format_reglist(reglist, row->decoded_reglist, sizeof(row->decoded_reglist));
#ifdef GWY_HAVE_UNICORN
    if (uc) (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc_addr, &pc_val);
#else
    (void)uc;
#endif
    row->pc_stack_value = pc_val;
    /* Also sample classic wrong slot (sp+0x20) for mismatch forensics. */
    {
        uint32_t wrong = 0;
#ifdef GWY_HAVE_UNICORN
        if (uc) (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 32u, &wrong);
#endif
        printf("[JJFB_P22L] ldm_slots method=%u sp+0x1C=0x%X sp+0x20=0x%X (legacy_wrong) "
               "evidence=OBSERVED\n",
               method, pc_val, wrong);
        fflush(stdout);
    }
    g.await_next = 1;
    g.await_method = method;
    g.await_stack_pc = pc_val;
    {
        HelperRet *hr = ret_for_method(method);
        if (hr) {
            hr->saw_ldm = 1;
            hr->wrapper_final_r0 = r0;
        }
    }
    if (method == 1u) {
        g.wrapper_final_r0 = r0;
        g.saw_wrapper_final = 1;
    }
    printf("[JJFB_P22L] ldm_decode method=%u pre_sp=0x%X reglist=0x%X %s pc_index=%u "
           "pc_addr=0x%X stack_pc=0x%X r0=0x%X evidence=OBSERVED\n",
           method, sp, reglist, row->decoded_reglist, pc_index, pc_addr, pc_val, r0);
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static void install_global_hook(void);
static void remove_global_hook(void);
static void install_mem_hooks(void);
static void remove_mem_hooks(void);

static int insn_consumes_r0(uint32_t insn, const uint32_t regs[16], uint32_t *lhs, uint32_t *rhs,
                            char *desc, size_t dlen) {
    uint32_t cond = (insn >> 28) & 0xFu;
    uint32_t op;
    /* CMP r0,#imm */
    if ((insn & 0x0FF0F000u) == 0x03500000u || (insn & 0xFFF0F000u) == 0xE3500000u) {
        *lhs = regs[0];
        *rhs = insn & 0xFFu;
        snprintf(desc, dlen, "CMP r0,#0x%X", *rhs);
        return 1;
    }
    /* CMP rn,r0 (register) */
    if ((insn & 0x0FF00FF0u) == 0x01500000u && (insn & 0xFu) == 0u) {
        uint32_t rn = (insn >> 16) & 0xFu;
        *lhs = regs[rn < 16 ? rn : 0];
        *rhs = regs[0];
        snprintf(desc, dlen, "CMP r%u,r0", rn);
        return 1;
    }
    /* TST r0,#imm */
    if ((insn & 0x0FF0F000u) == 0x03100000u || (insn & 0xFFF0F000u) == 0xE3100000u) {
        *lhs = regs[0];
        *rhs = insn & 0xFFu;
        snprintf(desc, dlen, "TST r0,#0x%X", *rhs);
        return 1;
    }
    /* STR r0,[...] */
    if ((insn & 0x0C500000u) == 0x04000000u && ((insn >> 12) & 0xFu) == 0u && cond != 0xFu) {
        *lhs = regs[0];
        *rhs = 0;
        snprintf(desc, dlen, "STR r0,[mem]");
        return 1;
    }
    /* Data-processing with Rm=r0 (e.g. SUB r0,r1,r0,LSL#3) — hard consume */
    if (cond <= 0xEu && ((insn >> 26) & 3u) == 0u && ((insn >> 4) & 1u) == 0u &&
        (insn & 0xFu) == 0u && ((insn >> 25) & 1u) == 0u) {
        op = (insn >> 21) & 0xFu;
        if (op == 0x2u || op == 0x4u || op == 0x0u || op == 0xCu || op == 0xDu || op == 0xEu ||
            op == 0xAu || op == 0xBu) {
            uint32_t rd = (insn >> 12) & 0xFu;
            uint32_t rn = (insn >> 16) & 0xFu;
            uint32_t sh = (insn >> 7) & 0x1Fu;
            *lhs = regs[rn];
            *rhs = regs[0];
            snprintf(desc, dlen, "DP(op=%u) r%u,r%u,r0,LSL#%u", op, rd, rn, sh);
            return 1;
        }
    }
    /* MOV rd,r0 soft */
    if ((insn & 0x0FFFFFF0u) == 0x01A00000u && (insn & 0xFu) == 0u) {
        uint32_t rd = (insn >> 12) & 0xFu;
        *lhs = regs[0];
        *rhs = 0;
        snprintf(desc, dlen, "MOV r%u,r0", rd);
        return 2;
    }
    return 0;
}

static uint32_t branch_target_arm(uint32_t pc, uint32_t insn) {
    int32_t imm;
    if ((insn & 0x0E000000u) != 0x0A000000u) return 0;
    imm = (int32_t)(insn & 0x00FFFFFFu);
    if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
    return (uint32_t)((int32_t)pc + 8 + (imm << 2));
}

static void maybe_finish_slice(const char *why);

static void slice_step(uc_engine *uc, uint32_t pc, const uint32_t regs[16], uint32_t sp, uint32_t lr,
                       uint32_t cpsr, uint32_t insn) {
    SliceRow *s;
    char mod[40];
    uint32_t base = 0, off = 0;
    int kind;
    uint32_t lhs = 0, rhs = 0;
    char cdesc[56];
    (void)uc;
    if (!g.parent_slice || g.slice_n >= P22L_SLICE_CAP) {
        if (g.slice_n >= P22L_SLICE_CAP) maybe_finish_slice("slice_cap");
        return;
    }
    resolve_mod(pc, mod, sizeof(mod), &base, &off);
    note_gl_off(pc, regs);

    s = &g.slice[g.slice_n++];
    memset(s, 0, sizeof(*s));
    s->seq = g.slice_n;
    s->pc = pc;
    s->insn = insn;
    describe_arm(insn, pc, s->desc, sizeof(s->desc));
    memcpy(s->r, regs, sizeof(s->r));
    s->sp = sp;
    s->lr = lr;
    s->cpsr = cpsr;
    s->r9 = regs[9];
    snprintf(s->module, sizeof(s->module), "%s", mod);
    s->module_offset = off;

    if (!g.parent_entry_pc) {
        g.parent_entry_pc = pc;
        g.parent_entry_sp = sp;
        snprintf(g.parent_module, sizeof(g.parent_module), "%s", mod);
        g.parent_module_offset = off;
        /* Parent resume PC is the continuation after the call; ARM BL is 4 bytes. */
        if (!g.parent_callsite && (pc & ~1u) >= 4u)
            g.parent_callsite = (pc & ~1u) - 4u;
        snprintf(s->note, sizeof(s->note), "parent_resume");
        add_post("parent_resume", pc, off, regs[0], regs[1], regs[9], mod);
    }

    /* R0=2 consumer */
    if (!g.r0_consumed && regs[0] == 2u) {
        kind = insn_consumes_r0(insn, regs, &lhs, &rhs, cdesc, sizeof(cdesc));
        if (kind == 1) {
            g.r0_consumed = 1;
            g.r0_consume_pc = pc;
            snprintf(g.r0_consume_insn, sizeof(g.r0_consume_insn), "%s", cdesc);
            g.cmp_lhs = lhs;
            g.cmp_rhs = rhs;
            snprintf(s->note, sizeof(s->note), "R0_CONSUME");
            add_post("r0_consume", pc, off, lhs, rhs, regs[9], cdesc);
            if (strstr(cdesc, "CMP")) {
                if (lhs == 2u && rhs == 2u)
                    snprintf(g.r0_meaning, sizeof(g.r0_meaning), "CMP_EQ_success_or_handled");
                else if (lhs == 2u && rhs == 0u)
                    snprintf(g.r0_meaning, sizeof(g.r0_meaning), "CMP_vs_0_nonzero_continue");
                else if (lhs == 2u)
                    snprintf(g.r0_meaning, sizeof(g.r0_meaning), "CMP_r0_vs_0x%X", rhs);
                else
                    snprintf(g.r0_meaning, sizeof(g.r0_meaning), "CMP_operands_0x%X_0x%X", lhs, rhs);
            } else if (strstr(cdesc, "STR")) {
                snprintf(g.r0_meaning, sizeof(g.r0_meaning), "stored_status_r0_2");
            } else if (strstr(cdesc, "DP(op=2)")) {
                snprintf(g.r0_meaning, sizeof(g.r0_meaning),
                         "scale_index_r0_2_in_SUB (r0=r1-(r0<<shift)); not boolean status");
            } else {
                snprintf(g.r0_meaning, sizeof(g.r0_meaning), "%s", cdesc);
            }
        } else if (kind == 2 && !g.r0_consume_pc) {
            /* remember first MOV sink; harden on later CMP of that reg if needed */
            snprintf(s->note, sizeof(s->note), "r0_mov_sink");
        }
    }

    /* First conditional branch after consume (may be on derived R0). */
    if (g.r0_consumed && !g.branch_taken[0] && (insn & 0x0E000000u) == 0x0A000000u &&
        (insn & 0xF0000000u) != 0xE0000000u) {
        uint32_t tgt = branch_target_arm(pc, insn);
        uint32_t cond = (insn >> 28) & 0xFu;
        int taken = 0;
        int z = (cpsr >> 30) & 1;
        if (cond == 0x0) taken = z;
        else if (cond == 0x1) taken = !z;
        else if (cond == 0x2) taken = (cpsr >> 29) & 1; /* CS */
        else if (cond == 0x3) taken = !((cpsr >> 29) & 1);
        else if (cond == 0xA) { /* GE */ taken = ((cpsr >> 31) & 1) == ((cpsr >> 28) & 1); }
        else if (cond == 0xB) { /* LT */ taken = ((cpsr >> 31) & 1) != ((cpsr >> 28) & 1); }
        else taken = -1;
        if (taken >= 0) {
            snprintf(g.branch_taken, sizeof(g.branch_taken), "%s",
                     taken ? "TAKEN" : "NOT_TAKEN");
            snprintf(g.branch_target, sizeof(g.branch_target), "0x%X%s", tgt,
                     regs[0] == 2u ? "" : " (on_derived_r0)");
            snprintf(s->note, sizeof(s->note), "branch_after_consume %s ->0x%X", g.branch_taken,
                     tgt);
            add_post("r0_branch", pc, off, (uint32_t)taken, tgt, regs[9], g.branch_taken);
        }
    }

    /* indirect call / BX/BLX reg */
    if ((insn & 0x0FFFFFF0u) == 0x012FFF30u || (insn & 0x0FFFFFF0u) == 0x012FFF10u) {
        g.saw_indirect_call = 1;
        snprintf(s->note, sizeof(s->note), "indirect_call");
        if (g.r0_consumed) maybe_finish_slice("indirect_after_r0");
    }

    /* Outer-frame LDM with PC: keep slicing into grandparent (cfunction state machine). */
    if (g.slice_n > 1u && (insn & 0x0FFF0000u) == 0x08BD0000u && (insn & 0x8000u) &&
        (insn & 0xF0000000u) == 0xE0000000u) {
        snprintf(s->note, sizeof(s->note), "frame_ldm_pc");
        g.await_next = 1;
        g.await_method = 100u + g.slice_n; /* synthetic: continue chain */
        g.await_stack_pc = 0;
        {
            uint32_t rl = insn & 0xFFFFu;
            uint32_t pci = (uint32_t)popcount16(rl & 0x7FFFu);
            uint32_t pv = 0;
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + pci * 4u, &pv);
            g.await_stack_pc = pv;
            printf("[JJFB_P22L] upward_ldm pc=0x%X stack_ret=0x%X r0=0x%X evidence=OBSERVED\n", pc,
                   pv, regs[0]);
            fflush(stdout);
        }
        /* Do not finish yet — follow return. */
    }

    if (g.entered_10740) maybe_finish_slice("entered_10740");
    if (g.entered_f670 || g.entered_8cdc) maybe_finish_slice("callback");
    if (g.entered_7b6c) maybe_finish_slice("cfg_loader");
    if (g.r0_consumed && g.branch_taken[0] && g.slice_n >= 32u)
        maybe_finish_slice("r0_consumed_and_branched");
    if (g.r0_consumed && g.slice_n >= 128u) maybe_finish_slice("slice_after_r0_consume");
    if (g.slice_n >= 256u) maybe_finish_slice("slice_256");
    if (g.slice_n >= P22L_SLICE_CAP) maybe_finish_slice("slice_cap");
}

static void on_next_or_slice(uc_engine *uc, uint32_t pc, uint32_t size) {
    uint32_t regs[16];
    uint32_t sp = 0, lr = 0, cpsr = 0, insn = 0;
    int i;
    (void)size;
    if (!p22l_enabled() || g.finalized) return;

    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~1u, &insn);

    if (g.await_next && (pc & ~1u) != (g.ldm_pc & ~1u)) {
        NextPcRow *nr;
        char mod[48];
        uint32_t base = 0, off = 0;
        if (g.next_n < P22L_NEXT_CAP) {
            nr = &g.nexts[g.next_n++];
            memset(nr, 0, sizeof(*nr));
            nr->seq = g.next_n;
            nr->method = g.await_method;
            nr->stack_decoded_pc = g.await_stack_pc;
            nr->next_executed_pc = pc;
            resolve_mod(pc, mod, sizeof(mod), &base, &off);
            snprintf(nr->next_module, sizeof(nr->next_module), "%s", mod);
            nr->next_module_base = base;
            nr->next_module_offset = off;
            nr->match = ((pc & ~1u) == (g.await_stack_pc & ~1u)) ? 1 : 0;
            nr->r0_at_next = regs[0];
            nr->sp_at_next = sp;
            nr->lr_at_next = lr;
            nr->cpsr_at_next = cpsr;
            nr->r9_at_next = regs[9];
            printf("[JJFB_P22L] next_pc method=%u stack=0x%X actual=0x%X match=%d mod=%s "
                   "off=0x%X r0=0x%X evidence=OBSERVED\n",
                   g.await_method, g.await_stack_pc, pc, nr->match, mod, off, regs[0]);
            fflush(stdout);
            if (!nr->match) {
                printf("[JJFB_P22L] next_pc_mismatch prefer_unicorn stack=0x%X unicorn=0x%X "
                       "cpsr=0x%X thumb=%d evidence=OBSERVED\n",
                       g.await_stack_pc, pc, cpsr, (cpsr & (1u << 5)) ? 1 : 0);
                fflush(stdout);
            }
            /* Prefer unicorn actual for parent — only after final method=1 wrapper return. */
            if (g.await_method == 1u && !g.parent_slice) {
                g.parent_slice = 1;
                g.stop_slice_n = 0;
                g.parent_entry_pc = 0;
                install_mem_hooks();
                add_post("begin_parent_slice", pc, off, regs[0], regs[1], regs[9], mod);
            } else if (g.await_method >= 100u) {
                /* upward frame return inside parent slice — keep slicing */
                add_post("upward_resume", pc, off, regs[0], regs[1], regs[9], mod);
            } else if (g.await_method != 1u) {
                /* Intermediate wrapper returns (m6/m0): keep next_pc evidence, drop global hook. */
                remove_global_hook();
            }
        }
        g.await_next = 0;
    }

    if (g.parent_slice) {
        slice_step(uc, pc, regs, sp, lr, cpsr, insn);
        note_gl_off(pc, regs);
    }
}

static void p22l_on_sparse(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t regs[16];
    uint32_t sp = 0, insn = 0;
    int i;
    (void)size;
    (void)user_data;
    if (!p22l_enabled() || g.finalized || !g.armed) return;
    if (pc < g.cont_pc || pc >= g.window_end) return;

    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &insn);

    if ((pc & ~1u) == (g.cont_pc & ~1u))
        record_helper_return_at_cont(g.last_method, pc, regs, sp);

    if (insn == 0xE3A00002u) {
        HelperRet *hr = ret_for_method(g.last_method);
        if (hr) {
            hr->saw_mov_r0_2 = 1;
            hr->wrapper_final_r0 = 2;
        }
        if (g.last_method == 1u) {
            g.wrapper_final_r0 = 2;
            g.saw_wrapper_final = 1;
        }
        printf("[JJFB_P22L] wrapper_mov_r0_2 method=%u evidence=OBSERVED\n", g.last_method);
        fflush(stdout);
    }

    if (insn == 0xE8BD8DF0u || ((insn & 0xFFFF0000u) == 0xE8BD0000u && (insn & 0x8000u))) {
        g.ldm_pc = pc;
        decode_ldm_at(uc, g.last_method, pc, insn, sp, regs[0]);
        install_global_hook();
    }
}

static void p22l_on_global(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)user_data;
    on_next_or_slice(uc, (uint32_t)address, size);
}

static void p22l_on_mem(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                        void *user_data) {
    SliceRow *s;
    (void)uc;
    (void)user_data;
    (void)value;
    if (!g.parent_slice || g.finalized) return;
    if (g.mem_n >= P22L_MEM_CAP) return;
    g.mem_n++;
    if (g.slice_n == 0 || g.slice_n > P22L_SLICE_CAP) return;
    s = &g.slice[g.slice_n - 1];
    if (!s->mem[0]) {
        snprintf(s->mem, sizeof(s->mem), "%s:0x%llX/%d",
                 type == UC_MEM_READ ? "R" : "W", (unsigned long long)address, size);
    }
}

static void install_sparse(void *uc, uint32_t cont) {
    uc_err ue;
    if (!uc || !cont || g.hook_sparse) return;
    g.cont_pc = cont & ~3u;
    g.window_end = g.cont_pc + P22L_WIN;
    g.ldm_pc = g.cont_pc + P22L_LDM_OFF;
    ue = uc_hook_add((uc_engine *)uc, &g.h_sparse, UC_HOOK_CODE, (void *)p22l_on_sparse, NULL,
                     (uint64_t)g.cont_pc, (uint64_t)(g.window_end - 1u));
    if (ue == UC_ERR_OK) {
        g.hook_sparse = 1;
        g.uc = uc;
        printf("[JJFB_P22L] sparse_hook=[0x%X,0x%X) ldm_expect=0x%X evidence=DOCUMENTED\n",
               g.cont_pc, g.window_end, g.ldm_pc);
        fflush(stdout);
    }
}

static void install_global_hook(void) {
    uc_err ue;
    if (!g.uc || g.hook_global) return;
    ue = uc_hook_add((uc_engine *)g.uc, &g.h_global, UC_HOOK_CODE, (void *)p22l_on_global, NULL, 1,
                     0);
    if (ue == UC_ERR_OK) {
        g.hook_global = 1;
        printf("[JJFB_P22L] global_next_pc_hook=1 evidence=DOCUMENTED\n");
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
    ue = uc_hook_add((uc_engine *)g.uc, &g.h_mem_r, UC_HOOK_MEM_READ, (void *)p22l_on_mem, NULL, 1,
                     0);
    if (ue != UC_ERR_OK) return;
    ue = uc_hook_add((uc_engine *)g.uc, &g.h_mem_w, UC_HOOK_MEM_WRITE, (void *)p22l_on_mem, NULL, 1,
                     0);
    if (ue == UC_ERR_OK) g.hook_mem = 1;
}

static void remove_mem_hooks(void) {
    if (!g.uc || !g.hook_mem) return;
    (void)uc_hook_del((uc_engine *)g.uc, g.h_mem_r);
    (void)uc_hook_del((uc_engine *)g.uc, g.h_mem_w);
    g.hook_mem = 0;
}

static void maybe_finish_slice(const char *why) {
    if (!g.parent_slice || g.finalized) return;
    g.parent_slice = 0;
    g.stop_slice_n = g.slice_n;
    if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", why ? why : "slice_done");
    remove_mem_hooks();
    remove_global_hook();
    add_post("end_parent_slice", g.parent_entry_pc, g.parent_module_offset, g.wrapper_final_r0, 0,
             0, why);
}
#endif /* GWY_HAVE_UNICORN */

void p22l_reset(void) {
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

void p22l_bind_uc(void *uc) {
    const char *rid;
    if (!p22l_enabled()) return;
    g.uc = uc;
    rid = getenv("JJFB_P22L_RUN_ID");
    if (!rid || !rid[0]) rid = getenv("JJFB_P22I_RUN_ID");
    if (rid && rid[0]) snprintf(g.run_id, sizeof(g.run_id), "%s", rid);
}

void p22l_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw) {
    if (!p22l_enabled() || !module_name) return;
    if (strstr(module_name, "gamelist")) {
        g.gl_base = base;
        g.gl_end = base + size;
        if (erw) g.gl_erw = erw;
    }
    if (strstr(module_name, "cfunction")) {
        g.cf_base = base;
        g.cf_end = base + size;
        if (erw) g.cf_erw = erw;
    }
}

void p22l_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       uint32_t sp) {
    if (!p22l_enabled() || g.finalized || !continuation_pc) return;
    if (!g.uc) g.uc = uc;
    g.armed = 1;
    g.last_method = method;
#ifdef GWY_HAVE_UNICORN
    install_sparse(g.uc ? g.uc : uc, continuation_pc);
#else
    (void)uc;
#endif
    printf("[JJFB_P22L] arm_continuation=0x%X method=%u sp=0x%X evidence=OBSERVED\n",
           continuation_pc, method, sp);
    fflush(stdout);
    (void)sp;
}

void p22l_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr) {
    (void)uc;
    (void)module_name;
    (void)lr;
    (void)sp;
    (void)cpsr;
    if (!p22l_enabled() || g.finalized) return;
    note_gl_off(pc, regs);
}

static void classify(void) {
    NextPcRow *final_next = NULL;
    uint32_t i;
    for (i = 0; i < g.next_n; i++) {
        if (g.nexts[i].method == 1u) final_next = &g.nexts[i];
    }
    if (g.entered_10740) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "E");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "after corrected LDM return, natural path entered +0x10740");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "observe once-flag/mode gate/cfg loader inside +0x10740; no state inject");
    } else if (final_next && !final_next->match && final_next->next_executed_pc) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "D");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "stack_decoded_pc!=unicorn next_pc (stack=0x%X actual=0x%X)",
                 final_next->stack_decoded_pc, final_next->next_executed_pc);
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "audit thumb bit / exception return / SP sample timing; prefer unicorn PC");
    } else if (g.r0_consumed && g.cmp_rhs != 2u && strstr(g.r0_consume_insn, "CMP") &&
               g.cmp_lhs == 2u && g.branch_taken[0] && strstr(g.branch_taken, "NOT")) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "B");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "parent treats R0=2 as fail/early-complete via %s lhs=0x%X rhs=0x%X %s",
                 g.r0_consume_insn, g.cmp_lhs, g.cmp_rhs, g.branch_taken);
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "document expected return contract vs wrapper R0=2");
    } else if (final_next && final_next->next_executed_pc &&
               (strstr(final_next->next_module, "cfunction") ||
                (g.cf_base && (final_next->next_executed_pc & ~1u) >= g.cf_base &&
                 (final_next->next_executed_pc & ~1u) < g.cf_end)) &&
               (final_next->next_module_offset < 0x9BF0u ||
                final_next->next_module_offset > 0x9C40u)) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "C");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "wrapper returns into cfunction state machine @+0x%X (not helper); R0=2 %s",
                 final_next->next_module_offset,
                 g.r0_consumed ? g.r0_meaning : "not yet CMP/TST/STR-compared");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "trace upward from 0x%X / consume@0x%X; find schedule into +0xF670/+0x10740",
                 final_next->next_executed_pc, g.r0_consume_pc);
    } else if (g.r0_consumed && !g.entered_10740 && !g.callback_pub) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "A");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "R0=2 consumed (%s) but no callback/+0x10740 schedule observed",
                 g.r0_meaning[0] ? g.r0_meaning : g.r0_consume_insn);
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "identify natural producer of callback/+0xF670|+0x8CDC|+0xD978 after parent");
    } else if (g.saw_wrapper_final && g.next_n > 0) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "A");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "wrapper returned R0=2; parent path observed but UI schedule missing");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "continue parent branch chain; find first real blocking gate");
    } else {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "D");
        snprintf(g.sole_lock, sizeof(g.sole_lock), "incomplete LDM/next-pc observation");
        snprintf(g.next_fix, sizeof(g.next_fix), "re-run; ensure sparse+global hooks armed");
    }
}

static void write_artifacts(void) {
    FILE *f;
    uint32_t i, j;
    NextPcRow *m1next = NULL;
    LdmRow *m1ldm = NULL;

    for (i = 0; i < g.ldm_n; i++)
        if (g.ldms[i].method == 1u) m1ldm = &g.ldms[i];
    for (i = 0; i < g.next_n; i++)
        if (g.nexts[i].method == 1u) m1next = &g.nexts[i];

    f = open_out("JJFB_P22L_LDM_CSV", "reports/p22l/p22l_ldm_decode.csv");
    if (f) {
        fprintf(f,
                "seq,method,ldm_pc,insn,pre_ldm_sp,reglist,decoded_reglist,pc_index,"
                "pc_stack_address,pc_stack_value,post_ldm_sp_expected,r0_at_ldm\n");
        for (i = 0; i < g.ldm_n; i++) {
            LdmRow *r = &g.ldms[i];
            fprintf(f, "%u,%u,0x%X,0x%08X,0x%X,0x%X,\"%s\",%u,0x%X,0x%X,0x%X,0x%X\n", r->seq,
                    r->method, r->ldm_pc, r->insn, r->pre_ldm_sp, r->reglist, r->decoded_reglist,
                    r->pc_index, r->pc_stack_address, r->pc_stack_value, r->post_ldm_sp_expected,
                    r->r0_at_ldm);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22L_NEXT_CSV", "reports/p22l/p22l_actual_next_pc.csv");
    if (f) {
        fprintf(f,
                "seq,method,stack_decoded_pc,next_executed_pc,match,next_module,"
                "next_module_offset,r0,sp,lr,cpsr,r9\n");
        for (i = 0; i < g.next_n; i++) {
            NextPcRow *r = &g.nexts[i];
            fprintf(f, "%u,%u,0x%X,0x%X,%d,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n", r->seq, r->method,
                    r->stack_decoded_pc, r->next_executed_pc, r->match, r->next_module,
                    r->next_module_offset, r->r0_at_next, r->sp_at_next, r->lr_at_next,
                    r->cpsr_at_next, r->r9_at_next);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22L_HELPER_CSV", "reports/p22l/p22l_helper_real_returns.csv");
    if (f) {
        fprintf(f,
                "seq,method,helper_return_r0,helper_return_r0_hex,cont_pc,sp,r9,"
                "wrapper_final_r0,saw_mov_r0_2,saw_ldm,note\n");
        for (i = 0; i < g.ret_n; i++) {
            HelperRet *r = &g.rets[i];
            fprintf(f, "%u,%u,%d,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%d,\"%s\"\n", r->seq, r->method,
                    (int)r->helper_return_r0, (uint32_t)r->helper_return_r0, r->cont_pc, r->sp,
                    r->r9, r->wrapper_final_r0, r->saw_mov_r0_2, r->saw_ldm, r->note);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22L_SLICE_CSV", "reports/p22l/p22l_parent_r0_slice.csv");
    if (f) {
        fprintf(f,
                "seq,pc,module,offset,insn,desc,r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,sp,lr,"
                "cpsr,note,mem\n");
        for (i = 0; i < g.slice_n; i++) {
            SliceRow *r = &g.slice[i];
            fprintf(f,
                    "%u,0x%X,%s,0x%X,0x%08X,\"%s\",0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\",\"%s\"\n",
                    r->seq, r->pc, r->module, r->module_offset, r->insn, r->desc, r->r[0], r->r[1],
                    r->r[2], r->r[3], r->r[4], r->r[5], r->r[6], r->r[7], r->r[8], r->r[9],
                    r->r[10], r->r[11], r->r[12], r->sp, r->lr, r->cpsr, r->note, r->mem);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22L_POST_CSV", "reports/p22l/p22l_post_parent_timeline.csv");
    if (f) {
        fprintf(f, "seq,event,pc,offset,r0,r1,r9,detail\n");
        for (i = 0; i < g.post_n; i++) {
            PostRow *p = &g.posts[i];
            fprintf(f, "%u,%s,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\"\n", p->seq, p->event, p->pc, p->off,
                    p->r0, p->r1, p->r9, p->detail);
        }
        fclose(f);
    }

    f = open_out("JJFB_P22L_BRANCH_MD", "reports/p22l/p22l_parent_branch_chain.md");
    if (f) {
        fprintf(f, "# P22L parent branch chain\n\n");
        fprintf(f, "## Wrapper epilogue\n\n");
        if (m1ldm) {
            fprintf(f,
                    "- LDM `0x%08X` reglist=`%s` (mask=0x%X)\n"
                    "- pre_ldm_sp=0x%X pc_index=%u pc_addr=0x%X stack_pc=0x%X\n",
                    m1ldm->insn, m1ldm->decoded_reglist, m1ldm->reglist, m1ldm->pre_ldm_sp,
                    m1ldm->pc_index, m1ldm->pc_stack_address, m1ldm->pc_stack_value);
        }
        if (m1next) {
            fprintf(f,
                    "- unicorn next_pc=0x%X module=%s off=0x%X match=%d r0=0x%X\n\n",
                    m1next->next_executed_pc, m1next->next_module, m1next->next_module_offset,
                    m1next->match, m1next->r0_at_next);
        }
        fprintf(f, "## R0=2 consumption\n\n");
        fprintf(f,
                "- consume_pc=0x%X insn=`%s`\n"
                "- cmp_lhs=0x%X cmp_rhs=0x%X\n"
                "- branch=%s target=%s\n"
                "- meaning=%s\n\n",
                g.r0_consume_pc, g.r0_consume_insn[0] ? g.r0_consume_insn : "NONE", g.cmp_lhs,
                g.cmp_rhs, g.branch_taken[0] ? g.branch_taken : "NONE",
                g.branch_target[0] ? g.branch_target : "NONE",
                g.r0_meaning[0] ? g.r0_meaning : "UNKNOWN");
        fprintf(f, "## Slice summary (first 32)\n\n```\n");
        for (i = 0; i < g.slice_n && i < 32u; i++) {
            SliceRow *r = &g.slice[i];
            fprintf(f, "%u 0x%X %s+0x%X %s r0=0x%X %s\n", r->seq, r->pc, r->module,
                    r->module_offset, r->desc, r->r[0], r->note);
        }
        fprintf(f, "```\n");
        fclose(f);
    }

    f = open_out("JJFB_P22L_VERDICT", "reports/p22l/p22l_parent_return_verdict.md");
    if (f) {
        fprintf(f,
                "# P22L-CLEAN parent return / R0=2 consumer verdict\n\n"
                "## Bottom line\n\n**Class: %s**\n\n%s\n\n"
                "## PASS answers\n\n```\n"
                "E8BD8DF0 实际寄存器列表：%s\n"
                "pre-LDM SP：0x%X\n"
                "PC 栈槽地址：0x%X\n"
                "栈中返回地址：0x%X\n"
                "Unicorn 下一条实际 PC：0x%X\n"
                "二者是否一致：%s\n"
                "\n"
                "真实父级 module：%s\n"
                "真实父级 offset：0x%X\n"
                "父级 callsite：0x%X\n"
                "wrapper 返回 R0：%u\n"
                "R0 第一消费指令：%s @ 0x%X\n"
                "比较操作数：lhs=0x%X rhs=0x%X\n"
                "实际分支：%s\n"
                "目标分支：%s\n"
                "\n"
                "method 6 真实返回：%d (0x%X)\n"
                "method 0 真实返回：%d (0x%X)\n"
                "method 1 真实返回：%d (0x%X)\n"
                "wrapper 最终返回：%u\n"
                "\n"
                "callback 是否发布：%s\n"
                "+0x10740 是否进入：%s\n"
                "+0x7B6C 是否进入：%s\n"
                "真实 cfg open 是否出现：%s\n"
                "\n"
                "当前唯一门锁：%s\n"
                "下一处最小通用修复：%s\n"
                "```\n\n"
                "## Evidence\n\n"
                "- run_id=%s\n"
                "- cont=0x%X ldm_rows=%u next_rows=%u slice_n=%u\n"
                "- entered F670=%d 8CDC=%d D978=%d 10740=%d 10814=%d 7B6C=%d\n"
                "- stop_reason=%s\n"
                "- r0_meaning=%s\n",
                g.verdict_class, g.sole_lock,
                m1ldm ? m1ldm->decoded_reglist : "UNKNOWN",
                m1ldm ? m1ldm->pre_ldm_sp : 0,
                m1ldm ? m1ldm->pc_stack_address : 0,
                m1ldm ? m1ldm->pc_stack_value : 0,
                m1next ? m1next->next_executed_pc : 0,
                m1next ? (m1next->match ? "YES" : "NO (prefer unicorn)") : "UNKNOWN",
                g.parent_module[0] ? g.parent_module : (m1next ? m1next->next_module : "UNKNOWN"),
                g.parent_module_offset ? g.parent_module_offset
                                       : (m1next ? m1next->next_module_offset : 0),
                g.parent_callsite,
                g.wrapper_final_r0,
                g.r0_consume_insn[0] ? g.r0_consume_insn : "NONE", g.r0_consume_pc, g.cmp_lhs,
                g.cmp_rhs, g.branch_taken[0] ? g.branch_taken : "NONE",
                g.branch_target[0] ? g.branch_target : "NONE",
                (int)g.r0_m6, (uint32_t)g.r0_m6, (int)g.r0_m0, (uint32_t)g.r0_m0, (int)g.r0_m1,
                (uint32_t)g.r0_m1, g.wrapper_final_r0,
                g.callback_pub ? "YES" : "NO", g.entered_10740 ? "YES" : "NO",
                g.entered_7b6c ? "YES" : "NO", g.cfg_open ? "YES" : "NO", g.sole_lock, g.next_fix,
                g.run_id, g.cont_pc, g.ldm_n, g.next_n, g.slice_n, g.entered_f670, g.entered_8cdc,
                g.entered_d978, g.entered_10740, g.entered_10814, g.entered_7b6c, g.stop_reason,
                g.r0_meaning[0] ? g.r0_meaning : "?");
        (void)j;
        fclose(f);
    }

    f = open_out("JJFB_P22L_SUMMARY", "out/p22l/p22l_runtime_summary.txt");
    if (f) {
        fprintf(f,
                "run_id=%s\nclass=%s\ncont=0x%X\n"
                "m6_r0=%d\nm0_r0=%d\nm1_r0=%d\nwrapper_final_r0=%u\n"
                "ldm_n=%u\nnext_n=%u\nslice_n=%u\n"
                "stack_pc=0x%X\nnext_pc=0x%X\nmatch=%d\n"
                "parent_module=%s\nparent_off=0x%X\nparent_callsite=0x%X\n"
                "r0_consume=%s\nr0_consume_pc=0x%X\ncmp_lhs=0x%X\ncmp_rhs=0x%X\n"
                "branch=%s\nbranch_tgt=%s\n"
                "callback=%d\nentered_10740=%d\nentered_7b6c=%d\ncfg_open=%d\n"
                "sole_lock=%s\nnext_fix=%s\nstop_reason=%s\n",
                g.run_id, g.verdict_class, g.cont_pc, (int)g.r0_m6, (int)g.r0_m0, (int)g.r0_m1,
                g.wrapper_final_r0, g.ldm_n, g.next_n, g.slice_n,
                m1ldm ? m1ldm->pc_stack_value : 0, m1next ? m1next->next_executed_pc : 0,
                m1next ? m1next->match : -1, g.parent_module, g.parent_module_offset,
                g.parent_callsite, g.r0_consume_insn, g.r0_consume_pc, g.cmp_lhs, g.cmp_rhs,
                g.branch_taken, g.branch_target, g.callback_pub, g.entered_10740, g.entered_7b6c,
                g.cfg_open, g.sole_lock, g.next_fix, g.stop_reason);
        fclose(f);
    }
}

void p22l_finalize(const char *stop_reason) {
    if (!p22l_enabled() || g.finalized) return;
    g.finalized = 1;
    if (stop_reason && stop_reason[0] && !g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", stop_reason);
    else if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "finalize");
#ifdef GWY_HAVE_UNICORN
    remove_mem_hooks();
#endif
    classify();
    write_artifacts();
    printf("[JJFB_P22L_FINAL] class=%s wrapper_r0=%u next_pc=0x%X consume=0x%X lock=%s "
           "evidence=OBSERVED\n",
           g.verdict_class, g.wrapper_final_r0,
           g.next_n ? g.nexts[g.next_n - 1].next_executed_pc : 0, g.r0_consume_pc, g.sole_lock);
    fflush(stdout);
}
