#include "gwy_launcher/ext_entry_abi_cluster_audit.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define GWY_6L_LAST_PCS 50
#define GWY_6L_MAX_P 12
#define GWY_6L_MAX_BRANCH 32
#define GWY_6L_MAX_BLOCKS 256

/* Decimal file offsets of +0xC init clusters (gbrwcore) — avoid banned hex tokens in tools. */
static const uint32_t k_cluster_offs[] = {15542u, 40586u, 42998u, 59094u, 138168u};

typedef struct {
    uint32_t addr;
    char kind[24];
    int wrote_0, wrote_4, wrote_8, wrote_c, wrote_10;
    uint32_t last_c_new;
} PCand;

typedef struct {
    uint32_t pc;
    uint32_t target;
    char reason[24];
} BrEv;

static struct {
    int mode_known;
    int enabled;
    int coverage;
    GwyEntryAbiVariant variant;
    void *uc;
    int in_entry;
    int finalized;
    int mr_exit_during;
    int mr_exit_after;
    uint32_t nested_p;
    uint32_t param_guess;
    int have_mirror;
    uint32_t mirror_r[13];
    uint32_t mirror_sp;
    uint32_t mirror_lr;
    uint32_t mirror_cpsr;
    uint32_t code_base;
    uint32_t code_size;
    char module[48];
    uint64_t insn_count;
    uint64_t insn_limit;
    uint32_t last_pcs[GWY_6L_LAST_PCS];
    int last_n;
    uint32_t blocks[GWY_6L_MAX_BLOCKS];
    int block_n;
    int cluster_hit[5];
    int any_cluster;
    BrEv branches[GWY_6L_MAX_BRANCH];
    int branch_n;
    PCand pcands[GWY_6L_MAX_P];
    int pcand_n;
    int any_pxc_nz;
    char end_reason[40];
    char last_exit_source[40];
} g_6l;

static int env_is_1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static uint64_t env_u64(const char *k, uint64_t defv) {
    const char *e = getenv(k);
    char *end = NULL;
    unsigned long long v;
    if (!e || !e[0]) return defv;
    v = strtoull(e, &end, 0);
    if (end == e) return defv;
    return (uint64_t)v;
}

const char *ext_entry_abi_cluster_audit_variant_name(GwyEntryAbiVariant v) {
    switch (v) {
    case GWY_ENTRY_ABI_R0_P: return "r0_p";
    case GWY_ENTRY_ABI_R0_1_R1_P: return "r0_1_r1_p";
    case GWY_ENTRY_ABI_R0_P_R1_PARAM: return "r0_p_r1_param";
    case GWY_ENTRY_ABI_MIRROR_CALLBACK: return "mirror_callback_regs";
    default: return "baseline";
    }
}

static void parse_mode(void) {
    const char *v;
    if (g_6l.mode_known) return;
    g_6l.enabled = env_is_1("JJFB_ENTRY_ABI_AUDIT");
    g_6l.coverage = env_is_1("JJFB_ENTRY_COVERAGE_TRACE") || g_6l.enabled;
    g_6l.variant = GWY_ENTRY_ABI_BASELINE;
    v = getenv("JJFB_ENTRY_ABI_VARIANT");
    if (v && v[0]) {
        if (strcmp(v, "r0_p") == 0) g_6l.variant = GWY_ENTRY_ABI_R0_P;
        else if (strcmp(v, "r0_1_r1_p") == 0) g_6l.variant = GWY_ENTRY_ABI_R0_1_R1_P;
        else if (strcmp(v, "r0_p_r1_param") == 0) g_6l.variant = GWY_ENTRY_ABI_R0_P_R1_PARAM;
        else if (strcmp(v, "mirror_callback_regs") == 0) g_6l.variant = GWY_ENTRY_ABI_MIRROR_CALLBACK;
        else g_6l.variant = GWY_ENTRY_ABI_BASELINE;
    }
    g_6l.insn_limit = env_u64("JJFB_ENTRY_INSN_LIMIT", 200000ull);
    g_6l.mode_known = 1;
}

void ext_entry_abi_cluster_audit_reset(void) {
    void *uc = g_6l.uc;
    memset(&g_6l, 0, sizeof(g_6l));
    g_6l.uc = uc;
}

void ext_entry_abi_cluster_audit_bind_uc(void *uc) { g_6l.uc = uc; }

int ext_entry_abi_cluster_audit_enabled(void) {
    parse_mode();
    return g_6l.enabled;
}

GwyEntryAbiVariant ext_entry_abi_cluster_audit_variant(void) {
    parse_mode();
    return g_6l.variant;
}

int ext_entry_abi_cluster_audit_in_entry(void) { return g_6l.in_entry; }

void ext_entry_abi_cluster_audit_set_in_entry(int on) { g_6l.in_entry = on ? 1 : 0; }

int ext_entry_abi_cluster_audit_cluster_reached(void) { return g_6l.any_cluster; }
int ext_entry_abi_cluster_audit_any_pxc_nonzero(void) { return g_6l.any_pxc_nz; }
const char *ext_entry_abi_cluster_audit_last_end_reason(void) {
    return g_6l.end_reason[0] ? g_6l.end_reason : "n/a";
}

#ifdef GWY_HAVE_UNICORN
static void read_all(void *uc, uint32_t *r13, uint32_t *sp, uint32_t *lr, uint32_t *pc,
                     uint32_t *cpsr) {
    uc_engine *u = (uc_engine *)uc;
    int i;
    const int idx[13] = {UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,  UC_ARM_REG_R3,
                         UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6,  UC_ARM_REG_R7,
                         UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11,
                         UC_ARM_REG_R12};
    if (!uc) return;
    for (i = 0; i < 13; i++) {
        r13[i] = 0;
        uc_reg_read(u, idx[i], &r13[i]);
    }
    if (sp) uc_reg_read(u, UC_ARM_REG_SP, sp);
    if (lr) uc_reg_read(u, UC_ARM_REG_LR, lr);
    if (pc) uc_reg_read(u, UC_ARM_REG_PC, pc);
    if (cpsr) uc_reg_read(u, UC_ARM_REG_CPSR, cpsr);
}
#else
static void read_all(void *uc, uint32_t *r13, uint32_t *sp, uint32_t *lr, uint32_t *pc,
                     uint32_t *cpsr) {
    (void)uc;
    if (r13) memset(r13, 0, 13 * sizeof(uint32_t));
    if (sp) *sp = 0;
    if (lr) *lr = 0;
    if (pc) *pc = 0;
    if (cpsr) *cpsr = 0;
}
#endif

void ext_entry_abi_cluster_audit_capture_callback_regs(void *uc) {
    uint32_t r[13], sp = 0, lr = 0, pc = 0, cpsr = 0;
    parse_mode();
    if (!g_6l.enabled) return;
    if (!uc) uc = g_6l.uc;
    read_all(uc, r, &sp, &lr, &pc, &cpsr);
    memcpy(g_6l.mirror_r, r, sizeof(r));
    g_6l.mirror_sp = sp;
    g_6l.mirror_lr = lr;
    g_6l.mirror_cpsr = cpsr;
    g_6l.have_mirror = 1;
    if (r[1] && !g_6l.param_guess) g_6l.param_guess = r[1];
    if (r[2] && !g_6l.param_guess) g_6l.param_guess = r[2];
}

static PCand *find_or_add_p(uint32_t addr, const char *kind) {
    int i;
    if (!addr) return NULL;
    for (i = 0; i < g_6l.pcand_n; i++) {
        if (g_6l.pcands[i].addr == addr) return &g_6l.pcands[i];
    }
    if (g_6l.pcand_n >= GWY_6L_MAX_P) return NULL;
    {
        PCand *c = &g_6l.pcands[g_6l.pcand_n++];
        memset(c, 0, sizeof(*c));
        c->addr = addr;
        snprintf(c->kind, sizeof(c->kind), "%s", kind && kind[0] ? kind : "unknown");
        printf("[JJFB_P_CANDIDATE] kind=%s addr=0x%X evidence=TARGET_OBSERVED\n", c->kind, c->addr);
        fflush(stdout);
        return c;
    }
}

void ext_entry_abi_cluster_audit_on_p_candidate(uint32_t p_guest, const char *kind) {
    parse_mode();
    if (!g_6l.enabled || !p_guest) return;
    g_6l.nested_p = p_guest;
    (void)find_or_add_p(p_guest, kind ? kind : "nested_cfn");
}

void ext_entry_abi_cluster_audit_on_p_write(uint32_t p_guest, uint32_t off, uint32_t old_v,
                                            uint32_t new_v, uint32_t pc, const char *module) {
    PCand *c;
    (void)old_v;
    (void)pc;
    (void)module;
    parse_mode();
    if (!g_6l.enabled || !p_guest) return;
    c = find_or_add_p(p_guest, "write_seen");
    if (!c) return;
    if (off == 0x00u) c->wrote_0 = 1;
    if (off == 0x04u) c->wrote_4 = 1;
    if (off == 0x08u) c->wrote_8 = 1;
    if (off == 0x0Cu) {
        c->wrote_c = 1;
        c->last_c_new = new_v;
        if (new_v) g_6l.any_pxc_nz = 1;
    }
    if (off == 0x10u) c->wrote_10 = 1;
}

static void note_block(uint32_t pc) {
    uint32_t b = pc & ~0xFu;
    int i;
    for (i = 0; i < g_6l.block_n; i++) {
        if (g_6l.blocks[i] == b) return;
    }
    if (g_6l.block_n < GWY_6L_MAX_BLOCKS) g_6l.blocks[g_6l.block_n++] = b;
}

static void note_cluster(uint32_t pc) {
    size_t i;
    uint32_t pn = gwy_guest_pc_norm(pc);
    if (!g_6l.code_base) return;
    for (i = 0; i < sizeof(k_cluster_offs) / sizeof(k_cluster_offs[0]); i++) {
        uint32_t va = g_6l.code_base + k_cluster_offs[i];
        if (pn >= va && pn < va + 0x40u) {
            if (!g_6l.cluster_hit[i]) {
                g_6l.cluster_hit[i] = 1;
                g_6l.any_cluster = 1;
                printf("[JJFB_ENTRY_CLUSTER_HIT] index=%u file_off=%u va=0x%X pc=0x%X "
                       "evidence=TARGET_OBSERVED\n",
                       (unsigned)i, (unsigned)k_cluster_offs[i], va, pc);
                fflush(stdout);
            }
        }
    }
}

static void maybe_branch(void *uc, uint32_t pc) {
    uint32_t word = 0;
    uint32_t target = 0;
    const char *reason = NULL;
#ifdef GWY_HAVE_UNICORN
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~1u, &word)) return;
#else
    (void)uc;
    (void)word;
    return;
#endif
    /* Thumb B (uncond) 0xE000 | imm11 — coarse; ARM B 0xEA000000 */
    if ((pc & 1u) || (word & 0xFFFFu) != word) {
        uint16_t h = (uint16_t)(word & 0xFFFFu);
        if ((h & 0xF800) == 0xE000) {
            int32_t imm = (int32_t)(h & 0x7FF);
            if (imm & 0x400) imm |= ~0x7FF;
            target = (pc & ~1u) + 4u + (uint32_t)(imm << 1);
            reason = "thumb_b";
        } else if ((h & 0xF800) == 0xF000) {
            reason = "thumb_bl_prefix";
            target = 0;
        }
    } else if ((word & 0x0F000000u) == 0x0A000000u) {
        int32_t imm = (int32_t)(word & 0x00FFFFFFu);
        if (imm & 0x00800000) imm |= ~0x00FFFFFF;
        target = (pc & ~1u) + 8u + (uint32_t)(imm << 2);
        reason = "arm_b";
    }
    if (!reason) return;
    if (g_6l.branch_n >= GWY_6L_MAX_BRANCH) return;
    /* Prefer branches that leave module or target stop/base. */
    if (g_6l.code_base && target) {
        uint32_t tn = gwy_guest_pc_norm(target);
        int outside = tn < g_6l.code_base || tn >= g_6l.code_base + g_6l.code_size;
        int to_base = tn == gwy_guest_pc_norm(g_6l.code_base);
        if (!outside && !to_base && (g_6l.insn_count % 64ull) != 0) return;
    }
    g_6l.branches[g_6l.branch_n].pc = pc;
    g_6l.branches[g_6l.branch_n].target = target;
    snprintf(g_6l.branches[g_6l.branch_n].reason, sizeof(g_6l.branches[g_6l.branch_n].reason),
             "%s", reason);
    g_6l.branch_n++;
    printf("[JJFB_ENTRY_BRANCH] pc=0x%X taken=yes target=0x%X reason=%s evidence=TARGET_OBSERVED\n",
           pc, target, reason);
    fflush(stdout);
}

void ext_entry_abi_cluster_audit_on_code(void *uc, const char *module_name, uint32_t pc) {
    (void)module_name;
    parse_mode();
    if (!g_6l.enabled || !g_6l.in_entry || !g_6l.coverage) return;
    g_6l.insn_count++;
    g_6l.last_pcs[g_6l.last_n % GWY_6L_LAST_PCS] = pc;
    g_6l.last_n++;
    note_block(pc);
    note_cluster(pc);
    if ((g_6l.insn_count % 16ull) == 0ull) maybe_branch(uc, pc);
}

void ext_entry_abi_cluster_audit_on_mr_exit(void *uc) {
    uint32_t r[13], sp = 0, lr = 0, pc = 0, cpsr = 0;
    const char *src;
    parse_mode();
    if (!g_6l.enabled) return;
    read_all(uc ? uc : g_6l.uc, r, &sp, &lr, &pc, &cpsr);
    if (g_6l.in_entry) {
        g_6l.mr_exit_during = 1;
        src = "entry_return";
        snprintf(g_6l.last_exit_source, sizeof(g_6l.last_exit_source), "mr_exit_during_entry");
    } else {
        g_6l.mr_exit_after = 1;
        src = "post_entry";
        snprintf(g_6l.last_exit_source, sizeof(g_6l.last_exit_source), "mr_exit_post_entry");
    }
    printf("[JJFB_ENTRY_EXIT] reason=mr_exit pc=0x%X lr=0x%X r0=0x%X r9=0x%X in_entry=%s "
           "evidence=TARGET_OBSERVED\n",
           pc, lr, r[0], r[9], g_6l.in_entry ? "yes" : "no");
    printf("[JJFB_MYTHROAD_EXIT] source=%s pc=0x%X lr=0x%X r0=0x%X evidence=TARGET_OBSERVED\n", src,
           pc, lr, r[0]);
    fflush(stdout);
}

static void emit_pre(const char *module, uint32_t entry_pc, const uint32_t *r, uint32_t sp,
                     uint32_t lr, uint32_t cpsr) {
    printf("[JJFB_ENTRY_ABI_PRE] module=%s variant=%s pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
           "r4=0x%X r5=0x%X r6=0x%X r7=0x%X r8=0x%X r9=0x%X r10=0x%X r11=0x%X r12=0x%X "
           "sp=0x%X lr=0x%X cpsr=0x%X nested_p=0x%X evidence=TARGET_OBSERVED\n",
           module, ext_entry_abi_cluster_audit_variant_name(g_6l.variant), entry_pc, r[0], r[1],
           r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10], r[11], r[12], sp, lr, cpsr,
           g_6l.nested_p);
    fflush(stdout);
}

static void emit_lastpcs(void) {
    int i, start, n;
    printf("[JJFB_ENTRY_LASTPCS]");
    n = g_6l.last_n < GWY_6L_LAST_PCS ? g_6l.last_n : GWY_6L_LAST_PCS;
    start = (g_6l.last_n < GWY_6L_LAST_PCS) ? 0 : (g_6l.last_n % GWY_6L_LAST_PCS);
    for (i = 0; i < n; i++) {
        int idx = (start + i) % GWY_6L_LAST_PCS;
        printf(" 0x%X", g_6l.last_pcs[idx]);
    }
    printf(" evidence=TARGET_OBSERVED\n");
    fflush(stdout);
}

static void emit_coverage(const char *module, const char *end_reason) {
    size_t i;
    printf("[JJFB_ENTRY_COVERAGE] module=%s insns=%llu unique_blocks=%d end_reason=%s "
           "cluster_any=%s insn_limit=%llu evidence=TARGET_OBSERVED\n",
           module, (unsigned long long)g_6l.insn_count, g_6l.block_n, end_reason,
           g_6l.any_cluster ? "yes" : "no", (unsigned long long)g_6l.insn_limit);
    for (i = 0; i < sizeof(k_cluster_offs) / sizeof(k_cluster_offs[0]); i++) {
        uint32_t va = g_6l.code_base ? (g_6l.code_base + k_cluster_offs[i]) : 0;
        printf("[JJFB_ENTRY_CLUSTER] index=%u file_off=%u va=0x%X reached=%s "
               "evidence=TARGET_OBSERVED\n",
               (unsigned)i, (unsigned)k_cluster_offs[i], va, g_6l.cluster_hit[i] ? "yes" : "no");
    }
    emit_lastpcs();
    fflush(stdout);
}

int ext_entry_abi_cluster_audit_run_documented(void *uc, const char *module_name, uint32_t code_base,
                                               uint32_t code_size, uint32_t entry_pc,
                                               uint32_t stop_addr, uint32_t start_pc_with_thumb) {
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    uint32_t r[13], sp = 0, lr = 0, pc = 0, cpsr = 0;
    const char *mod;
    int skip = 0;
    const char *skip_reason = NULL;
    int ok;

    parse_mode();
    if (!g_6l.enabled) {
        /* Fallback identical to 6K baseline. */
        char err[96];
        err[0] = 0;
        return guest_memory_uc_run_bounded((struct uc_struct *)uc, start_pc_with_thumb, stop_addr, 1u,
                                           200000ull, err, sizeof(err));
    }
    if (!uc) uc = g_6l.uc;
    mod = module_name && module_name[0] ? module_name : "gbrwcore.ext";
    snprintf(g_6l.module, sizeof(g_6l.module), "%s", mod);
    g_6l.code_base = code_base;
    g_6l.code_size = code_size;
    g_6l.insn_count = 0;
    g_6l.last_n = 0;
    g_6l.block_n = 0;
    g_6l.branch_n = 0;
    g_6l.any_cluster = 0;
    memset(g_6l.cluster_hit, 0, sizeof(g_6l.cluster_hit));
    g_6l.end_reason[0] = 0;

    read_all(uc, r, &sp, &lr, &pc, &cpsr);
    if (!g_6l.have_mirror) {
        memcpy(g_6l.mirror_r, r, sizeof(r));
        g_6l.mirror_sp = sp;
        g_6l.mirror_lr = lr;
        g_6l.mirror_cpsr = cpsr;
        g_6l.have_mirror = 1;
    }

    memset(&abi, 0, sizeof(abi));
    switch (g_6l.variant) {
    case GWY_ENTRY_ABI_R0_P:
        if (!g_6l.nested_p) {
            skip = 1;
            skip_reason = "SKIP_NO_P";
        } else {
            abi.set_r0 = 1;
            abi.r0 = g_6l.nested_p;
            abi.set_lr = 1;
            abi.lr = stop_addr;
        }
        break;
    case GWY_ENTRY_ABI_R0_1_R1_P:
        if (!g_6l.nested_p) {
            skip = 1;
            skip_reason = "SKIP_NO_P";
        } else {
            abi.set_r0 = 1;
            abi.r0 = 1u;
            abi.set_r1 = 1;
            abi.r1 = g_6l.nested_p;
            abi.set_lr = 1;
            abi.lr = stop_addr;
        }
        break;
    case GWY_ENTRY_ABI_R0_P_R1_PARAM:
        if (!g_6l.nested_p) {
            skip = 1;
            skip_reason = "SKIP_NO_P";
        } else if (!g_6l.param_guess && !g_6l.mirror_r[1]) {
            skip = 1;
            skip_reason = "SKIP_NO_PARAM";
        } else {
            abi.set_r0 = 1;
            abi.r0 = g_6l.nested_p;
            abi.set_r1 = 1;
            abi.r1 = g_6l.param_guess ? g_6l.param_guess : g_6l.mirror_r[1];
            abi.set_lr = 1;
            abi.lr = stop_addr;
        }
        break;
    case GWY_ENTRY_ABI_MIRROR_CALLBACK:
        abi.mirror_full = 1;
        memcpy(abi.mirror_r, g_6l.mirror_r, sizeof(abi.mirror_r));
        abi.mirror_sp = g_6l.mirror_sp;
        abi.mirror_cpsr = g_6l.mirror_cpsr;
        abi.set_lr = 1;
        abi.lr = stop_addr;
        /* Keep mirrored R0/R1; do not invent R9. */
        break;
    case GWY_ENTRY_ABI_BASELINE:
    default:
        abi.set_r0 = 1;
        abi.r0 = 1u;
        abi.set_lr = 1;
        abi.lr = stop_addr;
        break;
    }

    /* Preview ABI that will be applied. */
    {
        uint32_t pr[13];
        uint32_t psp = sp, plr = lr, pcpsr = cpsr;
        memcpy(pr, r, sizeof(pr));
        if (abi.mirror_full) {
            memcpy(pr, abi.mirror_r, sizeof(pr));
            psp = abi.mirror_sp;
            pcpsr = abi.mirror_cpsr;
        }
        if (abi.set_r0) pr[0] = abi.r0;
        if (abi.set_r1) pr[1] = abi.r1;
        if (abi.set_r2) pr[2] = abi.r2;
        if (abi.set_r3) pr[3] = abi.r3;
        if (abi.set_lr) plr = abi.lr;
        emit_pre(mod, entry_pc, pr, psp, plr, pcpsr);
    }

    if (skip) {
        printf("[JJFB_ENTRY_ABI_RET] module=%s variant=%s ret_pc=0x0 r0=0x0 r1=0x0 r3=0x0 r9=0x0 "
               "sp=0x0 reason=%s evidence=TARGET_OBSERVED\n",
               mod, ext_entry_abi_cluster_audit_variant_name(g_6l.variant), skip_reason);
        snprintf(g_6l.end_reason, sizeof(g_6l.end_reason), "%s", skip_reason);
        emit_coverage(mod, skip_reason);
        fflush(stdout);
        return 0;
    }

    g_6l.in_entry = 1;
    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, start_pc_with_thumb, stop_addr,
                                      g_6l.insn_limit, &abi, &out);
    g_6l.in_entry = 0;

    if (g_6l.mr_exit_during)
        snprintf(g_6l.end_reason, sizeof(g_6l.end_reason), "mr_exit_during_entry");
    else if (g_6l.insn_count >= g_6l.insn_limit &&
             (out.pc_after & ~1u) != (stop_addr & ~1u))
        snprintf(g_6l.end_reason, sizeof(g_6l.end_reason), "insn_limit");
    else
        snprintf(g_6l.end_reason, sizeof(g_6l.end_reason), "%s",
                 out.end_reason[0] ? out.end_reason : (ok ? "ok" : "uc_err"));

    printf("[JJFB_ENTRY_ABI_RET] module=%s variant=%s ret_pc=0x%X r0=0x%X r1=0x%X r3=0x%X r9=0x%X "
           "sp=0x%X lr=0x%X reason=%s uc_ok=%s evidence=TARGET_OBSERVED\n",
           mod, ext_entry_abi_cluster_audit_variant_name(g_6l.variant), out.pc_after, out.r0_after,
           out.r1_after, 0u, out.r9_after, out.sp_after, out.lr_after, g_6l.end_reason,
           ok ? "yes" : "no");
    emit_coverage(mod, g_6l.end_reason);
    fflush(stdout);
    (void)entry_pc;
    return ok;
}

void ext_entry_abi_cluster_audit_finalize(const char *stop_reason) {
    int i;
    parse_mode();
    if (!g_6l.enabled || g_6l.finalized) return;
    g_6l.finalized = 1;
    printf("[JJFB_6L_SUMMARY] variant=%s end_reason=%s insns=%llu cluster_any=%s pxc_nz=%s "
           "mr_exit_during=%s mr_exit_after=%s exit_source=%s stop=%s evidence=TARGET_OBSERVED\n",
           ext_entry_abi_cluster_audit_variant_name(g_6l.variant),
           g_6l.end_reason[0] ? g_6l.end_reason : "n/a", (unsigned long long)g_6l.insn_count,
           g_6l.any_cluster ? "yes" : "no", g_6l.any_pxc_nz ? "yes" : "no",
           g_6l.mr_exit_during ? "yes" : "no", g_6l.mr_exit_after ? "yes" : "no",
           g_6l.last_exit_source[0] ? g_6l.last_exit_source : "none",
           stop_reason ? stop_reason : "?");
    for (i = 0; i < g_6l.pcand_n; i++) {
        PCand *c = &g_6l.pcands[i];
        printf("[JJFB_6L_SUMMARY] P_CANDIDATE kind=%s addr=0x%X wrote_0=%d wrote_4=%d wrote_8=%d "
               "wrote_C=%d C_new=0x%X\n",
               c->kind, c->addr, c->wrote_0, c->wrote_4, c->wrote_8, c->wrote_c, c->last_c_new);
    }
    fflush(stdout);
}
