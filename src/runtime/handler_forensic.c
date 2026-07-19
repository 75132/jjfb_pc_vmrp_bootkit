#include "gwy_launcher/handler_forensic.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/ext_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define HF_RING 64
#define HF_BYTES 8

typedef struct HfInsn {
    uint32_t pc;
    uint32_t size;
    uint8_t bytes[HF_BYTES];
    uint32_t r[13];
    uint32_t cpsr;
    uint32_t sp;
    uint32_t lr;
    uint32_t r9;
    int is_branch;
    uint32_t branch_target;
    int branch_target_thumb;
} HfInsn;

typedef struct HfState {
    int active;
    void *uc;
#ifdef GWY_HAVE_UNICORN
    uc_hook hook;
#endif
    uint32_t handler_raw;
    uint32_t handler_pc;
    uint32_t code_base;
    uint32_t code_size;
    char owner[72];
    uint32_t er_rw;
    uint32_t entry_r[13];
    uint32_t entry_sp;
    uint32_t entry_lr;
    uint32_t entry_pc;
    uint32_t entry_cpsr;
    uint32_t stack_lo;
    uint32_t stack_hi;
    HfInsn ring[HF_RING];
    unsigned ring_count;
    unsigned ring_next;
    int last_branch_valid;
    HfInsn last_branch;
} HfState;

static HfState g_hf;

int handler_forensic_enabled(void) {
    const char *e = getenv("JJFB_HANDLER_FORENSIC");
    return e && e[0] == '1' && e[1] == '\0';
}

static void read_regs(void *uc, uint32_t r[13], uint32_t *sp, uint32_t *lr, uint32_t *pc,
                      uint32_t *cpsr) {
#ifdef GWY_HAVE_UNICORN
    uc_engine *u = (uc_engine *)uc;
    int i;
    if (!u) return;
    for (i = 0; i < 13; i++) uc_reg_read(u, UC_ARM_REG_R0 + i, &r[i]);
    if (sp) uc_reg_read(u, UC_ARM_REG_SP, sp);
    if (lr) uc_reg_read(u, UC_ARM_REG_LR, lr);
    if (pc) uc_reg_read(u, UC_ARM_REG_PC, pc);
    if (cpsr) uc_reg_read(u, UC_ARM_REG_CPSR, cpsr);
#else
    (void)uc;
    (void)r;
    (void)sp;
    (void)lr;
    (void)pc;
    (void)cpsr;
#endif
}

static void find_stack_map(void *uc, uint32_t sp, uint32_t *lo, uint32_t *hi) {
    *lo = 0;
    *hi = 0;
#ifdef GWY_HAVE_UNICORN
    {
        uc_mem_region *regions = NULL;
        uint32_t count = 0;
        uint32_t i;
        if (!uc || !sp) return;
        if (uc_mem_regions((uc_engine *)uc, &regions, &count) != UC_ERR_OK || !regions) return;
        for (i = 0; i < count; i++) {
            if (sp >= (uint32_t)regions[i].begin && sp < (uint32_t)regions[i].end) {
                *lo = (uint32_t)regions[i].begin;
                *hi = (uint32_t)regions[i].end;
                break;
            }
        }
        uc_free(regions);
    }
#else
    (void)uc;
    (void)sp;
#endif
}

static const char *mod_for_addr(uint32_t addr, char *buf, size_t n, uint32_t *out_base) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m =
        reg ? module_registry_find_by_code_addr(reg, addr & ~1u) : NULL;
    if (out_base) *out_base = 0;
    if (!buf || !n) return "?";
    buf[0] = 0;
    if (!m) {
        snprintf(buf, n, "%s", "?");
        return buf;
    }
    snprintf(buf, n, "%s", m->resolved_name[0] ? m->resolved_name : m->requested_name);
    if (out_base) *out_base = m->map.guest_code_base;
    return buf;
}

/* Minimal Thumb branch decode for forensics (not a full disassembler). */
static int decode_thumb_branch(const uint8_t *b, uint32_t size, uint32_t pc, uint32_t *regs,
                               uint32_t *out_tgt, int *out_thumb) {
    uint16_t h0, h1;
    if (!b || size < 2 || !out_tgt || !out_thumb) return 0;
    h0 = (uint16_t)(b[0] | (b[1] << 8));
    *out_thumb = 1;
    /* B (cond) T1: 1101 CCCC iiii iiii */
    if ((h0 & 0xF000u) == 0xD000u && ((h0 >> 8) & 0xFu) != 0xFu) {
        int8_t imm = (int8_t)(h0 & 0xFFu);
        *out_tgt = (pc + 4u + (uint32_t)((int32_t)imm << 1)) | 1u;
        return 1;
    }
    /* B T2: 1110 0iii iiii iiii */
    if ((h0 & 0xF800u) == 0xE000u) {
        int32_t imm = (int32_t)(h0 & 0x7FFu);
        if (imm & 0x400) imm |= ~0x7FF;
        *out_tgt = (pc + 4u + (uint32_t)(imm << 1)) | 1u;
        return 1;
    }
  /* BX Rm handled by caller; keep decode focused on imm branches / BL. */
  (void)regs;
  if ((h0 & 0xFF80u) == 0x4700u) return 0;
    if (size < 4) return 0;
    h1 = (uint16_t)(b[2] | (b[3] << 8));
    /* BL/BLX T1: 1111 0S ii.. / 11J1 J ii.. */
    if ((h0 & 0xF800u) == 0xF000u && (h1 & 0xC000u) == 0xC000u) {
        uint32_t s = (h0 >> 10) & 1u;
        uint32_t imm10 = h0 & 0x3FFu;
        uint32_t j1 = (h1 >> 13) & 1u;
        uint32_t j2 = (h1 >> 11) & 1u;
        uint32_t imm11 = h1 & 0x7FFu;
        uint32_t i1 = ~(j1 ^ s) & 1u;
        uint32_t i2 = ~(j2 ^ s) & 1u;
        int32_t imm32 = (int32_t)((s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1));
        if (s) imm32 |= (int32_t)0xFE000000;
        *out_tgt = (pc + 4u + (uint32_t)imm32) | (((h1 & 0x1000u) != 0) ? 1u : 0u);
        *out_thumb = (int)(*out_tgt & 1u);
        if ((h1 & 0x1000u) == 0) *out_thumb = 0; /* BLX imm → ARM */
        return 1;
    }
    return 0;
}

#ifdef GWY_HAVE_UNICORN
static void hf_on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    HfState *st = (HfState *)user;
    HfInsn *e;
    uint32_t r[13], sp = 0, lr = 0, cpsr = 0;
    uint32_t tgt = 0;
    int thumb_tgt = 1;
    uint32_t sz = size;
    uint16_t h0;
    (void)uc;
    if (!st || !st->active) return;
    if (sz == 0 || sz > HF_BYTES) sz = HF_BYTES;
    e = &st->ring[st->ring_next % HF_RING];
    memset(e, 0, sizeof(*e));
    e->pc = (uint32_t)address;
    e->size = sz;
    (void)guest_memory_uc_peek((struct uc_struct *)st->uc, e->pc, e->bytes, sz);
    read_regs(st->uc, r, &sp, &lr, NULL, &cpsr);
    memcpy(e->r, r, sizeof(r));
    e->cpsr = cpsr;
    e->sp = sp;
    e->lr = lr;
    e->r9 = r[9];
    if ((cpsr & (1u << 5)) == 0) {
        st->ring_next++;
        if (st->ring_count < HF_RING) st->ring_count++;
        return;
    }
    h0 = (uint16_t)(e->bytes[0] | (e->bytes[1] << 8));
    /* BX Rm / BLX Rm */
    if ((h0 & 0xFF80u) == 0x4700u) {
        int rm = (h0 >> 3) & 0xF;
        uint32_t t = 0;
        if (rm <= 12)
            t = r[rm];
        else if (rm == 13)
            t = sp;
        else if (rm == 14)
            t = lr;
        else
            t = e->pc + 4u;
        e->is_branch = 1;
        e->branch_target = t;
        e->branch_target_thumb = (int)(t & 1u);
        st->last_branch = *e;
        st->last_branch_valid = 1;
    } else if (decode_thumb_branch(e->bytes, sz, e->pc, r, &tgt, &thumb_tgt)) {
        e->is_branch = 1;
        e->branch_target = tgt;
        e->branch_target_thumb = thumb_tgt;
        st->last_branch = *e;
        st->last_branch_valid = 1;
    }
    st->ring_next++;
    if (st->ring_count < HF_RING) st->ring_count++;
}
#endif

static void print_regs_line(const char *tag, const uint32_t r[13], uint32_t sp, uint32_t lr,
                            uint32_t pc, uint32_t cpsr) {
    printf("[%s] R0=0x%X R1=0x%X R2=0x%X R3=0x%X R4=0x%X R5=0x%X R6=0x%X R7=0x%X "
           "R8=0x%X R9=0x%X R10=0x%X R11=0x%X R12=0x%X SP=0x%X LR=0x%X PC=0x%X "
           "CPSR=0x%X T=%d\n",
           tag, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11],
           r[12], sp, lr, pc, cpsr, (cpsr >> 5) & 1);
}

static void print_insn(const char *tag, const HfInsn *e) {
    char hex[HF_BYTES * 2 + 1];
    unsigned i;
    uint32_t base = 0;
    char mod[72];
    for (i = 0; i < e->size && i < HF_BYTES; i++)
        snprintf(hex + i * 2, 3, "%02X", e->bytes[i]);
    hex[e->size * 2] = 0;
    mod_for_addr(e->pc, mod, sizeof(mod), &base);
    printf("[%s] pc=0x%X size=%u bytes=%s T=%d SP=0x%X LR=0x%X R9=0x%X module=%s", tag, e->pc,
           e->size, hex, (e->cpsr >> 5) & 1, e->sp, e->lr, e->r9, mod);
    if (e->is_branch) {
        char tmod[72];
        uint32_t tbase = 0;
        mod_for_addr(e->branch_target, tmod, sizeof(tmod), &tbase);
        printf(" branch_tgt=0x%X tgt_lsb=%u tgt_thumb=%d tgt_module=%s", e->branch_target,
               e->branch_target & 1u, e->branch_target_thumb, tmod);
    }
    printf("\n");
}

int handler_forensic_begin(void *uc, uint32_t handler_raw, uint32_t code_base, uint32_t code_size,
                           const char *owner_name, uint32_t er_rw) {
    uint32_t r[13], sp = 0, lr = 0, pc = 0, cpsr = 0;
    uint32_t file_off = 0;
    if (!handler_forensic_enabled() || !uc || !handler_raw) return 0;
    memset(&g_hf, 0, sizeof(g_hf));
    g_hf.active = 1;
    g_hf.uc = uc;
    g_hf.handler_raw = handler_raw;
    g_hf.handler_pc = handler_raw & ~1u;
    g_hf.code_base = code_base;
    g_hf.code_size = code_size;
    g_hf.er_rw = er_rw;
    if (owner_name)
        snprintf(g_hf.owner, sizeof(g_hf.owner), "%s", owner_name);
    else
        snprintf(g_hf.owner, sizeof(g_hf.owner), "%s", "?");
    if (code_base && g_hf.handler_pc >= code_base)
        file_off = g_hf.handler_pc - code_base;

    read_regs(uc, r, &sp, &lr, &pc, &cpsr);
    memcpy(g_hf.entry_r, r, sizeof(r));
    g_hf.entry_sp = sp;
    g_hf.entry_lr = lr;
    g_hf.entry_pc = pc;
    g_hf.entry_cpsr = cpsr;
    find_stack_map(uc, sp, &g_hf.stack_lo, &g_hf.stack_hi);

    printf("[HANDLER_FORENSIC_ENTRY] handler_raw=0x%X handler_pc=0x%X owner=%s code_base=0x%X "
           "code_size=0x%X file_offset=0x%X er_rw=0x%X stack_mapped=[0x%X,0x%X) "
           "evidence=OBSERVED\n",
           handler_raw, g_hf.handler_pc, g_hf.owner, code_base, code_size, file_off, er_rw,
           g_hf.stack_lo, g_hf.stack_hi);
    print_regs_line("HANDLER_FORENSIC_ENTRY_REGS", r, sp, lr, g_hf.handler_pc, cpsr);
    fflush(stdout);

#ifdef GWY_HAVE_UNICORN
    if (code_base && code_size) {
        uc_err ue = uc_hook_add((uc_engine *)uc, &g_hf.hook, UC_HOOK_CODE, (void *)hf_on_code,
                                &g_hf, (uint64_t)code_base,
                                (uint64_t)code_base + (uint64_t)code_size - 1ull);
        if (ue != UC_ERR_OK) {
            printf("[HANDLER_FORENSIC] hook_add_failed err=%u\n", (unsigned)ue);
            fflush(stdout);
        }
    }
#endif
    return 1;
}

void handler_forensic_end(void *uc, int run_ok, unsigned uc_err, const char *end_reason,
                          uint32_t pc_after, uint32_t r0_after, uint32_t r9_after,
                          uint32_t sp_after, uint32_t lr_after, uint32_t cpsr_after) {
    uint32_t r[13], sp = 0, lr = 0, pc = 0, cpsr = 0;
    uint8_t peek_before[16], peek_after[16];
    unsigned i, n, start;
    uint32_t file_off = 0;
    const HfInsn *last = NULL;
    (void)uc;
    if (!g_hf.active) return;
#ifdef GWY_HAVE_UNICORN
    if (g_hf.hook && g_hf.uc) {
        uc_hook_del((uc_engine *)g_hf.uc, g_hf.hook);
        g_hf.hook = 0;
    }
#endif
    /* run_entry_ex restores UC; use out_* + last ring snap. */
    if (g_hf.ring_count > 0) {
        unsigned idx = (g_hf.ring_next + HF_RING - 1u) % HF_RING;
        last = &g_hf.ring[idx];
        memcpy(r, last->r, sizeof(r));
        sp = last->sp;
        lr = last->lr;
        pc = pc_after ? pc_after : last->pc;
        cpsr = last->cpsr;
        if (r9_after && !last->r9) r[9] = r9_after;
        (void)r0_after;
        (void)sp_after;
        (void)lr_after;
        (void)cpsr_after;
    } else {
        memcpy(r, g_hf.entry_r, sizeof(r));
        r[0] = r0_after;
        r[9] = r9_after;
        sp = sp_after;
        lr = lr_after;
        pc = pc_after;
        cpsr = cpsr_after;
    }
    if (g_hf.code_base && (pc & ~1u) >= g_hf.code_base)
        file_off = (pc & ~1u) - g_hf.code_base;

    printf("[HANDLER_FORENSIC_FAULT] run_ok=%d uc_err=%u end=%s pc=0x%X file_offset=0x%X "
           "CPSR=0x%X T=%d R9=0x%X SP=0x%X LR=0x%X entry_T=%d entry_R9=0x%X "
           "r9_unchanged=%d evidence=OBSERVED\n",
           run_ok, uc_err, end_reason ? end_reason : "?", pc, file_off, cpsr, (cpsr >> 5) & 1,
           r[9], sp, lr, (g_hf.entry_cpsr >> 5) & 1, g_hf.entry_r[9],
           r[9] == g_hf.entry_r[9] ? 1 : 0);
    print_regs_line("HANDLER_FORENSIC_FAULT_REGS", r, sp, lr, pc, cpsr);

    memset(peek_before, 0, sizeof(peek_before));
    memset(peek_after, 0, sizeof(peek_after));
    if (pc && g_hf.uc) {
        uint32_t pcn = pc & ~1u;
        (void)guest_memory_uc_peek((struct uc_struct *)g_hf.uc, pcn >= 16u ? pcn - 16u : pcn,
                                   peek_before, 16);
        (void)guest_memory_uc_peek((struct uc_struct *)g_hf.uc, pcn, peek_after, 16);
        printf("[HANDLER_FORENSIC_FAULT_BYTES] before=");
        for (i = 0; i < 16; i++) printf("%02X", peek_before[i]);
        printf(" at=");
        for (i = 0; i < 16; i++) printf("%02X", peek_after[i]);
        printf("\n");
    }

    n = g_hf.ring_count < 16u ? g_hf.ring_count : 16u;
    start = (g_hf.ring_next + HF_RING - n) % HF_RING;
    printf("[HANDLER_FORENSIC_RING] total=%u showing_last=%u\n", g_hf.ring_count, n);
    for (i = 0; i < n; i++) {
        unsigned idx = (start + i) % HF_RING;
        print_insn("HANDLER_FORENSIC_INSN", &g_hf.ring[idx]);
    }
    if (g_hf.last_branch_valid) {
        print_insn("HANDLER_FORENSIC_LAST_BRANCH", &g_hf.last_branch);
    } else {
        printf("[HANDLER_FORENSIC_LAST_BRANCH] none_in_ring evidence=OBSERVED\n");
    }
    fflush(stdout);
    g_hf.active = 0;
}
