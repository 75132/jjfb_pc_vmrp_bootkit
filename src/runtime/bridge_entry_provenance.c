#include "gwy_launcher/bridge_entry_provenance.h"

#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/package_scope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define BEP_INSN_RING 32
#define BEP_ENTRY_CAP 256
#define BEP_PRED_CAP 64
#define BEP_NEST_CAP 128

typedef struct BepInsn {
    uint32_t address;
    uint32_t raw;
    uint32_t size;
    uint32_t cpsr;
    uint32_t r0, r1, r2, r3, lr, sp, r9;
    uint32_t branch_target;
    char thumb;
    char kind[24]; /* bl_imm / blx_rm / bx_rm / ldr / mov / other */
    char module[40];
    char disasm[72];
    uint8_t wrote_r0;
    uint8_t wrote_r1;
} BepInsn;

typedef struct BepEntry {
    uint32_t sequence;
    uint32_t runCode_depth;
    uint64_t runCode_serial;
    uint32_t slot;
    char api[40];
    uint32_t pc;
    uint32_t lr;
    uint32_t cpsr;
    uint32_t r0, r1, r2, r3, r9, sp;
    char owner_module[48];
    char current_mrp[64];
    char logical_package[64];
    char previous_bridge_api[40];
    uint32_t previous_bridge_slot;
    uint32_t previous_bridge_entry_lr;
    uint32_t previous_bridge_leave_lr;
    uint32_t previous_bridge_return_r0;
    char classif[40];
    char branch_kind[24];
    uint32_t branch_insn_pc;
    uint32_t branch_target;
    uint32_t r0_last_writer_pc;
    char r0_source[48];
    uint32_t r1_last_writer_pc;
    char r1_source[48];
    uint32_t r0_writer_dist;
    uint32_t r1_writer_dist;
    int args_valid;
    int insn_count;
    BepInsn insn_snap[BEP_INSN_RING];
} BepEntry;

typedef struct BepPred {
    uint32_t sequence;
    char from_api[40];
    uint32_t from_slot;
    char to_api[40];
    uint32_t to_slot;
    uint32_t from_leave_lr;
    uint32_t to_entry_lr;
    uint32_t from_return_r0;
    uint32_t to_r0;
    uint32_t to_r1;
    int slot_delta;
    int same_lr;
    int r0_is_prev_ret;
    char note[64];
} BepPred;

typedef struct BepNest {
    uint32_t sequence;
    char op[12]; /* SAVE / RESTORE */
    char site[48];
    uint32_t depth;
    uint64_t serial;
    uint32_t outer_pc, outer_lr, outer_sp, outer_r9, outer_cpsr;
    uint32_t live_pc, live_lr, live_sp, live_r9, live_cpsr;
    int pc_match, lr_match, sp_match, r9_match, cpsr_match;
} BepNest;

static struct {
    int enabled;
    int bound;
    char out_dir[260];
    uint32_t seq;
    uint32_t depth;
    uint64_t serial;
    uint32_t insn_n;
    BepInsn insn[BEP_INSN_RING];
    uint32_t r0_writer_pc;
    char r0_source[48];
    uint32_t r1_writer_pc;
    char r1_source[48];
    char prev_api[40];
    uint32_t prev_slot;
    uint32_t prev_entry_lr;
    uint32_t prev_leave_lr;
    uint32_t prev_return_r0;
    int have_prev;
    uint32_t entry_n;
    BepEntry entries[BEP_ENTRY_CAP];
    uint32_t pred_n;
    BepPred preds[BEP_PRED_CAP];
    uint32_t nest_n;
    BepNest nests[BEP_NEST_CAP];
    /* last nest SAVE snapshot for restore compare */
    uint32_t last_save_pc, last_save_lr, last_save_sp, last_save_r9, last_save_cpsr;
    char last_save_site[48];
    int have_save;
} g_bep;

static int env1(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1';
}

static void copy_str(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) src = "";
    snprintf(dst, cap, "%s", src);
}

int bridge_entry_prov_enabled(void) {
    return g_bep.enabled || env1("JJFB_BRIDGE_ENTRY_PROV");
}

void bridge_entry_prov_reset(void) {
    const char *dir;
    memset(&g_bep, 0, sizeof(g_bep));
    g_bep.enabled = env1("JJFB_BRIDGE_ENTRY_PROV");
    dir = getenv("JJFB_BRIDGE_ENTRY_PROV_DIR");
    if (dir && dir[0])
        copy_str(g_bep.out_dir, sizeof(g_bep.out_dir), dir);
    else
        copy_str(g_bep.out_dir, sizeof(g_bep.out_dir), "out/p15");
}

void bridge_entry_prov_bind_uc(void *uc) {
    (void)uc;
    if (!g_bep.enabled) bridge_entry_prov_reset();
    g_bep.bound = 1;
    g_bep.enabled = env1("JJFB_BRIDGE_ENTRY_PROV");
}

/* Updated from gwy_ext_obs_p26_run_context via thin call in gwy_ext_obs. */
void bridge_entry_prov_set_run_context(uint32_t depth, uint64_t serial) {
    g_bep.depth = depth;
    g_bep.serial = serial;
}

static int bep_watch_api(const char *name) {
    static const char *k[] = {
        /* Pre-0x74: Case-9 fallout often walks here first. */
        "_mr_c_function_new", "mr_printf", "mr_mem_get", "mr_mem_free",
        /* Spec window 0x74–0x9C */
        "mr_drawBitmap", "mr_getCharBitmap", "mr_timerStart", "mr_timerStop",
        "mr_getTime",    "mr_getDatetime",   "mr_getUserInfo", "mr_sleep",
        "mr_plat",       "mr_platEx",        "mr_ferrno",      NULL};
    int i;
    if (!name) return 0;
    for (i = 0; k[i]; i++) {
        if (strcmp(name, k[i]) == 0) return 1;
    }
    return 0;
}

static int is_bridge_stub_va(uint32_t va) {
    uint32_t a = va & ~1u;
    /* mr_table stubs live near 0x280000 in this product. */
    return a >= 0x280000u && a < 0x280400u;
}

/* Any mr_table stub VA in the Case-9 aftermath window (0x58–0x9C). */
static int bep_watch_slot(uint32_t slot) {
    uint32_t a = slot & ~1u;
    if (!is_bridge_stub_va(a)) return 0;
    /* Approximate offset from common base 0x280004. */
    if (a >= 0x28005Cu && a <= 0x2800A0u) return 1;
    return 0;
}

static void decode_insn(void *uc, uint32_t pc, uint32_t size, const uint32_t regs[16],
                        uint32_t cpsr, BepInsn *out) {
    int thumb = (cpsr & (1u << 5)) != 0;
    uint32_t word = 0;
    uint32_t target = 0;
    int rm = -1;
    memset(out, 0, sizeof(*out));
    out->address = pc;
    out->size = size ? size : (thumb ? 2u : 4u);
    out->cpsr = cpsr;
    out->thumb = thumb ? 1 : 0;
    out->r0 = regs[0];
    out->r1 = regs[1];
    out->r2 = regs[2];
    out->r3 = regs[3];
    out->lr = regs[14];
    out->sp = regs[13];
    out->r9 = regs[9];
    copy_str(out->kind, sizeof(out->kind), "other");
    copy_str(out->disasm, sizeof(out->disasm), "?");

#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uint32_t peek = 0;
        if (guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &peek)) {
            if (thumb) {
                uint16_t half = (uint16_t)((pc & 2u) ? (peek >> 16) : (peek & 0xFFFFu));
                out->raw = half;
            } else {
                out->raw = peek;
                word = peek;
            }
        }
    }
#else
    (void)uc;
    (void)word;
#endif

    if (thumb) {
        uint16_t half = (uint16_t)(out->raw & 0xFFFFu);
        if (ext_entry_decode_thumb_blx_rm(half, &rm) && rm >= 0 && rm < 16) {
            target = regs[rm];
            copy_str(out->kind, sizeof(out->kind), "blx_rm");
            snprintf(out->disasm, sizeof(out->disasm), "blx r%d ; ->0x%X", rm, target);
            out->branch_target = target;
        } else if (ext_entry_decode_thumb_bx_rm(half, &rm) && rm >= 0 && rm < 16) {
            target = regs[rm];
            copy_str(out->kind, sizeof(out->kind), "bx_rm");
            snprintf(out->disasm, sizeof(out->disasm), "bx r%d ; ->0x%X", rm, target);
            out->branch_target = target;
        } else if ((half & 0xF800u) == 0xF000u && uc) {
            uint32_t word2 = 0;
            uint32_t next = (pc + 2u) & ~1u;
            if (guest_memory_uc_peek_u32((struct uc_struct *)uc, next & ~3u, &word2)) {
                uint16_t hi = (uint16_t)((next & 2u) ? (word2 >> 16) : (word2 & 0xFFFFu));
                if ((hi & 0xD000u) == 0xD000u || (hi & 0xD000u) == 0xC000u) {
                    /* Approximate target via shared decode path patterns. */
                    int32_t imm32;
                    uint32_t S = (half >> 10) & 1u;
                    uint32_t imm10 = half & 0x3FFu;
                    uint32_t J1 = (hi >> 13) & 1u;
                    uint32_t J2 = (hi >> 11) & 1u;
                    uint32_t imm11 = hi & 0x7FFu;
                    uint32_t I1 = (J1 ^ S) ^ 1u;
                    uint32_t I2 = (J2 ^ S) ^ 1u;
                    if ((hi & 0xD000u) == 0xD000u) {
                        imm32 = (int32_t)((S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) |
                                          (imm11 << 1));
                        if (S) imm32 |= (int32_t)0xFE000000;
                        target = (pc + 4u + (uint32_t)imm32) | 1u;
                        copy_str(out->kind, sizeof(out->kind), "bl_imm");
                        snprintf(out->disasm, sizeof(out->disasm), "bl 0x%X", target);
                    } else {
                        imm11 &= ~1u;
                        imm32 = (int32_t)((S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) |
                                          imm11);
                        if (S) imm32 |= (int32_t)0xFE000000;
                        target = (pc + 4u + (uint32_t)imm32) & ~3u;
                        copy_str(out->kind, sizeof(out->kind), "blx_imm");
                        snprintf(out->disasm, sizeof(out->disasm), "blx 0x%X", target);
                    }
                    out->raw = ((uint32_t)hi << 16) | half;
                    out->size = 4;
                    out->branch_target = target;
                }
            }
        } else if ((half & 0xFF00u) == 0x2000u) {
            /* movs rd, #imm */
            int rd = (half >> 8) & 7;
            uint32_t imm = half & 0xFFu;
            snprintf(out->disasm, sizeof(out->disasm), "movs r%d, #0x%X", rd, imm);
            copy_str(out->kind, sizeof(out->kind), "mov_imm");
            if (rd == 0) {
                out->wrote_r0 = 1;
            }
            if (rd == 1) {
                out->wrote_r1 = 1;
            }
        } else if ((half & 0xF800u) == 0x6800u) {
            /* ldr rt, [rn, #imm] */
            int rt = half & 7;
            int rn = (half >> 3) & 7;
            uint32_t imm = ((half >> 6) & 0x1Fu) << 2;
            snprintf(out->disasm, sizeof(out->disasm), "ldr r%d, [r%d, #0x%X]", rt, rn, imm);
            copy_str(out->kind, sizeof(out->kind), "ldr_imm");
            if (rt == 0) out->wrote_r0 = 1;
            if (rt == 1) out->wrote_r1 = 1;
        } else {
            snprintf(out->disasm, sizeof(out->disasm), "t 0x%04X", half);
        }
    } else if (word || out->raw) {
        word = out->raw;
        if (ext_entry_decode_arm_blx_rm(word, &rm) && rm >= 0 && rm < 16) {
            target = regs[rm];
            copy_str(out->kind, sizeof(out->kind), "arm_blx_rm");
            snprintf(out->disasm, sizeof(out->disasm), "blx r%d ; ->0x%X", rm, target);
            out->branch_target = target;
        } else if (ext_entry_decode_arm_bx_rm(word, &rm) && rm >= 0 && rm < 16) {
            target = regs[rm];
            copy_str(out->kind, sizeof(out->kind), "arm_bx_rm");
            snprintf(out->disasm, sizeof(out->disasm), "bx r%d ; ->0x%X", rm, target);
            out->branch_target = target;
        } else {
            snprintf(out->disasm, sizeof(out->disasm), "a 0x%08X", word);
        }
    }
}

void bridge_entry_prov_on_guest_code(void *uc, uint32_t pc, uint32_t size, const uint32_t regs[16],
                                     uint32_t cpsr, const char *module_name) {
    BepInsn *slot;
    if (!bridge_entry_prov_enabled()) return;
    if (is_bridge_stub_va(pc)) return; /* stub itself logged on enter */

    slot = &g_bep.insn[g_bep.insn_n % BEP_INSN_RING];
    decode_insn(uc, pc, size, regs, cpsr, slot);
    copy_str(slot->module, sizeof(slot->module), module_name ? module_name : "?");
    if (slot->wrote_r0) {
        g_bep.r0_writer_pc = pc;
        copy_str(g_bep.r0_source, sizeof(g_bep.r0_source), slot->disasm);
    }
    if (slot->wrote_r1) {
        g_bep.r1_writer_pc = pc;
        copy_str(g_bep.r1_source, sizeof(g_bep.r1_source), slot->disasm);
    }
    /* Also treat any branch that loads target into PC path as not writing R0/R1. */
    g_bep.insn_n++;
}

static void resolve_owner(uint32_t lr, char *owner, size_t cap, char *pkg, size_t pkg_cap,
                          char *mrp, size_t mrp_cap) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = NULL;
    const char *ap = package_scope_active_package();
    const char *pri = package_scope_active_primary();
    copy_str(owner, cap, "?");
    copy_str(pkg, pkg_cap, ap ? ap : "?");
    copy_str(mrp, mrp_cap, ap ? ap : "?");
    if (reg) m = module_registry_find_by_code_addr(reg, lr & ~1u);
    if (m && m->requested_name[0]) copy_str(owner, cap, m->requested_name);
    else if (pri && pri[0]) copy_str(owner, cap, pri);
    (void)pkg_cap;
}

static void snap_insns(BepEntry *e) {
    uint32_t n = g_bep.insn_n < BEP_INSN_RING ? g_bep.insn_n : BEP_INSN_RING;
    uint32_t start = g_bep.insn_n > BEP_INSN_RING ? (g_bep.insn_n % BEP_INSN_RING) : 0u;
    uint32_t i;
    e->insn_count = (int)n;
    for (i = 0; i < n; i++) {
        uint32_t idx = (start + i) % BEP_INSN_RING;
        e->insn_snap[i] = g_bep.insn[idx];
    }
}

static int find_call_to_slot(const BepEntry *e, uint32_t slot, char *kind_out, size_t kind_cap,
                             uint32_t *insn_pc, uint32_t *target_out) {
    int i;
    for (i = e->insn_count - 1; i >= 0; i--) {
        const BepInsn *in = &e->insn_snap[i];
        uint32_t t = in->branch_target & ~1u;
        if (!t) continue;
        if (t == (slot & ~1u) || t == slot) {
            if (strstr(in->kind, "bl") || strstr(in->kind, "bx")) {
                copy_str(kind_out, kind_cap, in->kind);
                if (insn_pc) *insn_pc = in->address;
                if (target_out) *target_out = in->branch_target;
                return 1;
            }
        }
    }
    return 0;
}

static int lr_is_continuation_of(uint32_t lr, uint32_t call_pc, const char *kind) {
    uint32_t lra = lr & ~1u;
    uint32_t cpa = call_pc & ~1u;
    if (!lra || !cpa) return 0;
    if (kind && strstr(kind, "bl")) {
        /* Thumb BL is 4 bytes; Thumb BLX rm / BX is 2. */
        if (strstr(kind, "imm")) return lra == cpa + 4u || lra == cpa + 2u;
        return lra == cpa + 2u || lra == cpa + 4u;
    }
    return 0; /* BX does not set LR */
}

static void classify_entry(BepEntry *e) {
    int found;
    char kind[24];
    uint32_t insn_pc = 0, tgt = 0;
    int linear = 0;
    int same_lr = 0;
    int r0_prev = 0;
    int stale_case9 = 0;

    copy_str(kind, sizeof(kind), "");
    found = find_call_to_slot(e, e->slot, kind, sizeof(kind), &insn_pc, &tgt);
    if (found) {
        copy_str(e->branch_kind, sizeof(e->branch_kind), kind);
        e->branch_insn_pc = insn_pc;
        e->branch_target = tgt;
    }

    if (g_bep.have_prev) {
        linear = (e->slot == g_bep.prev_slot + 4u);
        same_lr = ((e->lr & ~1u) == (g_bep.prev_entry_lr & ~1u));
        r0_prev = (e->r0 == g_bep.prev_return_r0);
    }

    /* Case-9 stale: R1==9 and R0 null/fail chain. */
    if (e->r1 == 9u && (e->r0 == 0u || e->r0 == 0xFFFFFFFFu)) stale_case9 = 1;

    if (g_bep.have_prev && (e->pc & ~1u) == (g_bep.prev_leave_lr & ~1u) && !found) {
        copy_str(e->classif, sizeof(e->classif), "RETURN_TO_BRIDGE_SLOT");
        return;
    }

    if (found) {
        int is_bl_imm = (strcmp(kind, "bl_imm") == 0 || strcmp(kind, "blx_imm") == 0 ||
                         strcmp(kind, "arm_bl_imm") == 0 || strcmp(kind, "arm_blx_imm") == 0);
        int is_blx_rm = (strcmp(kind, "blx_rm") == 0 || strcmp(kind, "arm_blx_rm") == 0);
        int is_bx_rm = (strcmp(kind, "bx_rm") == 0 || strcmp(kind, "arm_bx_rm") == 0);

        if (is_bl_imm && lr_is_continuation_of(e->lr, insn_pc, kind)) {
            copy_str(e->classif, sizeof(e->classif), "GENUINE_DIRECT_CALL");
            e->args_valid = !stale_case9 && !r0_prev;
            return;
        }
        if (is_blx_rm && lr_is_continuation_of(e->lr, insn_pc, kind)) {
            copy_str(e->classif, sizeof(e->classif), "GENUINE_INDIRECT_CALL");
            e->args_valid = !stale_case9 && !r0_prev;
            return;
        }
        if (is_bx_rm || is_blx_rm) {
            copy_str(e->classif, sizeof(e->classif), "GENUINE_INDIRECT_CALL");
            e->args_valid = !stale_case9 && !r0_prev;
            if (linear && same_lr && (r0_prev || stale_case9)) {
                copy_str(e->classif, sizeof(e->classif), "LINEAR_SLOT_FALLTHROUGH");
                e->args_valid = 0;
            }
            return;
        }
    }

    if (linear && same_lr && (r0_prev || stale_case9)) {
        copy_str(e->classif, sizeof(e->classif), "LINEAR_SLOT_FALLTHROUGH");
        e->args_valid = 0;
        return;
    }

    if (same_lr && g_bep.have_prev && !found) {
        copy_str(e->classif, sizeof(e->classif), "STALE_LR_REENTRY");
        e->args_valid = 0;
        return;
    }

    if (!found && is_bridge_stub_va(e->lr)) {
        copy_str(e->classif, sizeof(e->classif), "TABLE_DATA_EXECUTION");
        e->args_valid = 0;
        return;
    }

    copy_str(e->classif, sizeof(e->classif), "UNKNOWN");
    e->args_valid = 0;
}

static void write_csv_files(void);

static void record_entry(void *uc, uint32_t slot_addr, const char *api_name) {
    BepEntry *e;
    uint32_t regs[16];
    uint32_t cpsr = 0;
    int i;
#ifdef GWY_HAVE_UNICORN
    static const int k_reg[16] = {
        UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3, UC_ARM_REG_R4,
        UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,  UC_ARM_REG_R8, UC_ARM_REG_R9,
        UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR,
        UC_ARM_REG_PC};
#endif

    if (!bridge_entry_prov_enabled()) return;
    if (!bep_watch_api(api_name) && !bep_watch_slot(slot_addr)) return;
    if (g_bep.entry_n >= BEP_ENTRY_CAP) return;

    e = &g_bep.entries[g_bep.entry_n];
    memset(e, 0, sizeof(*e));
    g_bep.seq++;
    e->sequence = g_bep.seq;
    e->runCode_depth = g_bep.depth;
    e->runCode_serial = g_bep.serial;
    e->slot = slot_addr;
    copy_str(e->api, sizeof(e->api), api_name ? api_name : "?");
    e->pc = slot_addr;

#ifdef GWY_HAVE_UNICORN
    if (uc) {
        for (i = 0; i < 16; i++) {
            regs[i] = 0;
            uc_reg_read((uc_engine *)uc, k_reg[i], &regs[i]);
        }
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_CPSR, &cpsr);
        e->r0 = regs[0];
        e->r1 = regs[1];
        e->r2 = regs[2];
        e->r3 = regs[3];
        e->r9 = regs[9];
        e->sp = regs[13];
        e->lr = regs[14];
        e->cpsr = cpsr;
    }
#else
    (void)uc;
    (void)regs;
    (void)cpsr;
    (void)i;
#endif

    resolve_owner(e->lr, e->owner_module, sizeof(e->owner_module), e->logical_package,
                  sizeof(e->logical_package), e->current_mrp, sizeof(e->current_mrp));

    if (g_bep.have_prev) {
        copy_str(e->previous_bridge_api, sizeof(e->previous_bridge_api), g_bep.prev_api);
        e->previous_bridge_slot = g_bep.prev_slot;
        e->previous_bridge_entry_lr = g_bep.prev_entry_lr;
        e->previous_bridge_leave_lr = g_bep.prev_leave_lr;
        e->previous_bridge_return_r0 = g_bep.prev_return_r0;
    }

    e->r0_last_writer_pc = g_bep.r0_writer_pc;
    e->r1_last_writer_pc = g_bep.r1_writer_pc;
    copy_str(e->r0_source, sizeof(e->r0_source),
             g_bep.r0_source[0] ? g_bep.r0_source : "NONE_SINCE_RESET");
    copy_str(e->r1_source, sizeof(e->r1_source),
             g_bep.r1_source[0] ? g_bep.r1_source : "NONE_SINCE_RESET");
    /* Distance in guest instructions since writer (approx). */
    e->r0_writer_dist = g_bep.r0_writer_pc ? 0u : 0xFFFFFFFFu;
    e->r1_writer_dist = g_bep.r1_writer_pc ? 0u : 0xFFFFFFFFu;

    snap_insns(e);
    classify_entry(e);

    /* Invalidate args if clearly prior host return / Case-9 residue. */
    if (g_bep.have_prev && e->r0 == g_bep.prev_return_r0) e->args_valid = 0;
    if (strcmp(e->api, "mr_plat") == 0 || strcmp(e->api, "mr_sleep") == 0) {
        if (e->r0 == 0xFFFFFFFFu || (int32_t)e->r0 == -1) e->args_valid = 0;
        if (e->r1 == 9u) e->args_valid = 0;
    }

    printf("[JJFB_BRIDGE_ENTRY_PROV] seq=%u api=%s slot=0x%X class=%s depth=%u serial=%llu "
           "pc=0x%X lr=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X sp=0x%X "
           "prev=%s prev_slot=0x%X prev_leave_lr=0x%X prev_r0=0x%X "
           "branch=%s branch_pc=0x%X args_valid=%d owner=%s mrp=%s evidence=OBSERVED\n",
           e->sequence, e->api, e->slot, e->classif, e->runCode_depth,
           (unsigned long long)e->runCode_serial, e->pc, e->lr, e->r0, e->r1, e->r2, e->r3, e->r9,
           e->sp, e->previous_bridge_api[0] ? e->previous_bridge_api : "-", e->previous_bridge_slot,
           e->previous_bridge_leave_lr, e->previous_bridge_return_r0,
           e->branch_kind[0] ? e->branch_kind : "-", e->branch_insn_pc, e->args_valid,
           e->owner_module, e->current_mrp);
    fflush(stdout);

    if (e->insn_count > 0) {
        int show = e->insn_count < 8 ? e->insn_count : 8;
        int base = e->insn_count - show;
        for (i = base; i < e->insn_count; i++) {
            const BepInsn *in = &e->insn_snap[i];
            printf("[JJFB_BRIDGE_ENTRY_INSN] seq=%u api=%s i=%d addr=0x%X raw=0x%X thumb=%d "
                   "kind=%s disasm=\"%s\" br=0x%X mod=%s evidence=OBSERVED\n",
                   e->sequence, e->api, i - base, in->address, in->raw, (int)in->thumb, in->kind,
                   in->disasm, in->branch_target, in->module);
        }
        fflush(stdout);
    }

    if (g_bep.have_prev && g_bep.pred_n < BEP_PRED_CAP) {
        BepPred *p = &g_bep.preds[g_bep.pred_n++];
        memset(p, 0, sizeof(*p));
        p->sequence = e->sequence;
        copy_str(p->from_api, sizeof(p->from_api), g_bep.prev_api);
        p->from_slot = g_bep.prev_slot;
        copy_str(p->to_api, sizeof(p->to_api), e->api);
        p->to_slot = e->slot;
        p->from_leave_lr = g_bep.prev_leave_lr;
        p->to_entry_lr = e->lr;
        p->from_return_r0 = g_bep.prev_return_r0;
        p->to_r0 = e->r0;
        p->to_r1 = e->r1;
        p->slot_delta = (int)e->slot - (int)g_bep.prev_slot;
        p->same_lr = ((e->lr & ~1u) == (g_bep.prev_entry_lr & ~1u));
        p->r0_is_prev_ret = (e->r0 == g_bep.prev_return_r0);
        copy_str(p->note, sizeof(p->note), e->classif);
    }

    /* Track enter LR; leave fills leave_lr/return later. */
    copy_str(g_bep.prev_api, sizeof(g_bep.prev_api), e->api);
    g_bep.prev_slot = e->slot;
    g_bep.prev_entry_lr = e->lr;
    g_bep.have_prev = 1;
    g_bep.entry_n++;

    /* Clear R0/R1 writers after enter so post-return guest writes count fresh. */
    g_bep.r0_writer_pc = 0;
    g_bep.r1_writer_pc = 0;
    g_bep.r0_source[0] = '\0';
    g_bep.r1_source[0] = '\0';

    write_csv_files();
}

void bridge_entry_prov_on_host_enter(void *uc, uint32_t slot_addr, const char *api_name) {
    record_entry(uc, slot_addr, api_name);
}

void bridge_entry_prov_on_unimplemented(void *uc, uint32_t slot_addr, const char *api_name) {
    record_entry(uc, slot_addr, api_name);
}

void bridge_entry_prov_on_data_exec(void *uc, uint32_t slot_addr, const char *name) {
    char label[48];
    if (!bridge_entry_prov_enabled()) return;
    if (!bep_watch_slot(slot_addr) && !(name && name[0])) return;
    snprintf(label, sizeof(label), "DATA:%s", name ? name : "?");
    record_entry(uc, slot_addr, label);
    /* Force classification for data stubs. */
    if (g_bep.entry_n > 0) {
        BepEntry *e = &g_bep.entries[g_bep.entry_n - 1];
        copy_str(e->classif, sizeof(e->classif), "TABLE_DATA_EXECUTION");
        e->args_valid = 0;
        write_csv_files();
        printf("[JJFB_BRIDGE_ENTRY_PROV] seq=%u api=%s class=TABLE_DATA_EXECUTION "
               "note=forced_data_stub evidence=OBSERVED\n",
               e->sequence, e->api);
        fflush(stdout);
    }
}

void bridge_entry_prov_on_host_leave(void *uc, uint32_t slot_addr, const char *api_name,
                                     uint32_t leave_lr, uint32_t return_r0) {
    (void)uc;
    (void)slot_addr;
    if (!bridge_entry_prov_enabled()) return;
    if (!bep_watch_api(api_name)) return;
    g_bep.prev_leave_lr = leave_lr;
    g_bep.prev_return_r0 = return_r0;
    printf("[JJFB_BRIDGE_LEAVE_PROV] api=%s slot=0x%X leave_lr=0x%X return_r0=0x%X "
           "evidence=OBSERVED\n",
           api_name ? api_name : "?", slot_addr, leave_lr, return_r0);
    fflush(stdout);
}

void bridge_entry_prov_on_nest_save(void *uc, const char *site, const uint32_t regs17[17]) {
    BepNest *n;
    if (!bridge_entry_prov_enabled() || !regs17) return;
    if (g_bep.nest_n >= BEP_NEST_CAP) return;
    n = &g_bep.nests[g_bep.nest_n++];
    memset(n, 0, sizeof(*n));
    n->sequence = ++g_bep.seq;
    copy_str(n->op, sizeof(n->op), "SAVE");
    copy_str(n->site, sizeof(n->site), site ? site : "?");
    n->depth = g_bep.depth;
    n->serial = g_bep.serial;
    /* regs17: R0..R12, SP, LR, PC, CPSR */
    n->outer_pc = regs17[15];
    n->outer_lr = regs17[14];
    n->outer_sp = regs17[13];
    n->outer_r9 = regs17[9];
    n->outer_cpsr = regs17[16];
    g_bep.last_save_pc = n->outer_pc;
    g_bep.last_save_lr = n->outer_lr;
    g_bep.last_save_sp = n->outer_sp;
    g_bep.last_save_r9 = n->outer_r9;
    g_bep.last_save_cpsr = n->outer_cpsr;
    copy_str(g_bep.last_save_site, sizeof(g_bep.last_save_site), n->site);
    g_bep.have_save = 1;
    printf("[JJFB_BRIDGE_NEST_SAVE] site=%s depth=%u pc=0x%X lr=0x%X sp=0x%X r9=0x%X cpsr=0x%X "
           "evidence=OBSERVED\n",
           n->site, n->depth, n->outer_pc, n->outer_lr, n->outer_sp, n->outer_r9, n->outer_cpsr);
    fflush(stdout);
    (void)uc;
}

void bridge_entry_prov_on_nest_restore(void *uc, const char *site, const uint32_t saved17[17]) {
    BepNest *n;
    uint32_t live_pc = 0, live_lr = 0, live_sp = 0, live_r9 = 0, live_cpsr = 0;
    if (!bridge_entry_prov_enabled() || !saved17) return;
    if (g_bep.nest_n >= BEP_NEST_CAP) return;
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &live_pc);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &live_lr);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &live_sp);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &live_r9);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_CPSR, &live_cpsr);
    }
#else
    (void)uc;
#endif
    n = &g_bep.nests[g_bep.nest_n++];
    memset(n, 0, sizeof(*n));
    n->sequence = ++g_bep.seq;
    copy_str(n->op, sizeof(n->op), "RESTORE");
    copy_str(n->site, sizeof(n->site), site ? site : "?");
    n->depth = g_bep.depth;
    n->serial = g_bep.serial;
    n->outer_pc = saved17[15];
    n->outer_lr = saved17[14];
    n->outer_sp = saved17[13];
    n->outer_r9 = saved17[9];
    n->outer_cpsr = saved17[16];
    n->live_pc = live_pc;
    n->live_lr = live_lr;
    n->live_sp = live_sp;
    n->live_r9 = live_r9;
    n->live_cpsr = live_cpsr;
    n->pc_match = (live_pc == saved17[15]);
    n->lr_match = (live_lr == saved17[14]);
    n->sp_match = (live_sp == saved17[13]);
    n->r9_match = (live_r9 == saved17[9]);
    n->cpsr_match = (live_cpsr == saved17[16]);
    printf("[JJFB_BRIDGE_NEST_RESTORE] site=%s depth=%u saved_pc=0x%X saved_lr=0x%X "
           "live_pc=0x%X live_lr=0x%X pc_ok=%d lr_ok=%d sp_ok=%d r9_ok=%d cpsr_ok=%d "
           "evidence=OBSERVED\n",
           n->site, n->depth, n->outer_pc, n->outer_lr, n->live_pc, n->live_lr, n->pc_match,
           n->lr_match, n->sp_match, n->r9_match, n->cpsr_match);
    fflush(stdout);
}

static void ensure_dir(const char *path) {
#ifdef _WIN32
    char cmd[320];
    snprintf(cmd, sizeof(cmd), "mkdir \"%s\" 2>nul", path);
    (void)system(cmd);
#else
    char cmd[320];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", path);
    (void)system(cmd);
#endif
}

static void write_csv_files(void) {
    char path[320];
    FILE *f;
    uint32_t i, j;

    if (!g_bep.out_dir[0]) return;
    ensure_dir(g_bep.out_dir);

    snprintf(path, sizeof(path), "%s/bridge_entry_provenance.csv", g_bep.out_dir);
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "sequence,runCode_depth,runCode_serial,slot,api,pc,lr,cpsr,r0,r1,r2,r3,r9,sp,"
                "owner_module,current_mrp,logical_package,previous_bridge_api,previous_bridge_slot,"
                "previous_bridge_entry_lr,previous_bridge_leave_lr,previous_bridge_return_r0,"
                "classification,branch_kind,branch_insn_pc,branch_target,r0_last_writer_pc,"
                "r0_source,r1_last_writer_pc,r1_source,args_valid\n");
        for (i = 0; i < g_bep.entry_n; i++) {
            BepEntry *e = &g_bep.entries[i];
            fprintf(f,
                    "%u,%u,%llu,0x%X,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "%s,%s,%s,%s,0x%X,0x%X,0x%X,0x%X,%s,%s,0x%X,0x%X,0x%X,\"%s\",0x%X,\"%s\",%d\n",
                    e->sequence, e->runCode_depth, (unsigned long long)e->runCode_serial, e->slot,
                    e->api, e->pc, e->lr, e->cpsr, e->r0, e->r1, e->r2, e->r3, e->r9, e->sp,
                    e->owner_module, e->current_mrp, e->logical_package,
                    e->previous_bridge_api[0] ? e->previous_bridge_api : "", e->previous_bridge_slot,
                    e->previous_bridge_entry_lr, e->previous_bridge_leave_lr,
                    e->previous_bridge_return_r0, e->classif, e->branch_kind, e->branch_insn_pc,
                    e->branch_target, e->r0_last_writer_pc, e->r0_source, e->r1_last_writer_pc,
                    e->r1_source, e->args_valid);
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/bridge_predecessor_ring.csv", g_bep.out_dir);
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "sequence,from_api,from_slot,to_api,to_slot,from_leave_lr,to_entry_lr,"
                "from_return_r0,to_r0,to_r1,slot_delta,same_lr,r0_is_prev_ret,note\n");
        for (i = 0; i < g_bep.pred_n; i++) {
            BepPred *p = &g_bep.preds[i];
            fprintf(f, "%u,%s,0x%X,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%d,%d,%s\n", p->sequence,
                    p->from_api, p->from_slot, p->to_api, p->to_slot, p->from_leave_lr,
                    p->to_entry_lr, p->from_return_r0, p->to_r0, p->to_r1, p->slot_delta, p->same_lr,
                    p->r0_is_prev_ret, p->note);
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/bridge_insn_ring.csv", g_bep.out_dir);
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "entry_seq,api,slot,idx,address,raw,thumb,kind,disasm,branch_target,module,"
                "r0,r1,lr\n");
        for (i = 0; i < g_bep.entry_n; i++) {
            BepEntry *e = &g_bep.entries[i];
            for (j = 0; j < (uint32_t)e->insn_count; j++) {
                BepInsn *in = &e->insn_snap[j];
                fprintf(f, "%u,%s,0x%X,%u,0x%X,0x%X,%d,%s,\"%s\",0x%X,%s,0x%X,0x%X,0x%X\n",
                        e->sequence, e->api, e->slot, j, in->address, in->raw, (int)in->thumb,
                        in->kind, in->disasm, in->branch_target, in->module, in->r0, in->r1,
                        in->lr);
            }
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/bridge_nest_audit.csv", g_bep.out_dir);
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "sequence,op,site,depth,serial,outer_pc,outer_lr,outer_sp,outer_r9,outer_cpsr,"
                "live_pc,live_lr,pc_match,lr_match,sp_match,r9_match,cpsr_match\n");
        for (i = 0; i < g_bep.nest_n; i++) {
            BepNest *n = &g_bep.nests[i];
            fprintf(f, "%u,%s,%s,%u,%llu,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%d,%d,%d,%d\n",
                    n->sequence, n->op, n->site, n->depth, (unsigned long long)n->serial,
                    n->outer_pc, n->outer_lr, n->outer_sp, n->outer_r9, n->outer_cpsr, n->live_pc,
                    n->live_lr, n->pc_match, n->lr_match, n->sp_match, n->r9_match, n->cpsr_match);
        }
        fclose(f);
    }
}

void bridge_entry_prov_flush(void) { write_csv_files(); }
