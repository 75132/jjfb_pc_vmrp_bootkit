#include "gwy_launcher/product_c0_start_object_trace.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PC_2FEBBC 0x2FEBBCu
#define PC_2FEC3C 0x2FEC3Cu
#define OFF_E6C 0xE6Cu

static int g_en;
static int g_en_known;
static int g_hook_ok;
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_root;
static int g_root_watch_ok;
static int g_e6c_watch_ok;

static int env1(const char *k) {
    const char *v = getenv(k);
    return v && v[0] == '1' && v[1] == '\0';
}

#ifdef GWY_HAVE_UNICORN
static void on_root_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                          int64_t value, void *user);
static void on_e6c_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                         int64_t value, void *user);
#endif

int product_c0_sot_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_C0_START_OBJECT_TRACE");
        g_en_known = 1;
    }
    return g_en;
}

void product_c0_sot_reset(void) {
    g_hook_ok = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_root = 0;
    g_root_watch_ok = 0;
    g_e6c_watch_ok = 0;
    g_en_known = 0;
}

void product_c0_sot_bind_uc(void *uc) {
    if (uc) g_uc = uc;
}

void product_c0_sot_note_er_rw(uint32_t er_rw) {
    if (er_rw) g_er_rw = er_rw;
}

static void dump_regs(void *uc, const char *tag) {
    uint32_t r[13] = {0}, sp = 0, lr = 0, pc = 0, cpsr = 0;
    uint32_t e6c = 0;
    int i;
    if (!uc) return;
#ifdef GWY_HAVE_UNICORN
    {
        int ids[13] = {UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3,
                       UC_ARM_REG_R4,  UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,
                       UC_ARM_REG_R8,  UC_ARM_REG_R9,  UC_ARM_REG_R10, UC_ARM_REG_R11,
                       UC_ARM_REG_R12};
        void *ptrs[13];
        for (i = 0; i < 13; i++) ptrs[i] = &r[i];
        uc_reg_read_batch((uc_engine *)uc, ids, ptrs, 13);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &sp);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &lr);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_CPSR, &cpsr);
    }
#else
    (void)i;
#endif
    if (r[9])
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r[9] + OFF_E6C, &e6c);
    printf("[C0_SOT] tag=%s pc=0x%X lr=0x%X sp=0x%X cpsr=0x%X r9=0x%X R9+E6C=0x%X "
           "root=0x%X evidence=OBSERVED\n",
           tag ? tag : "?", pc, lr, sp, cpsr, r[9], e6c, g_root);
    printf("[C0_SOT] regs r0=0x%X r1=0x%X r2=0x%X r3=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X "
           "r8=0x%X r10=0x%X r11=0x%X r12=0x%X\n",
           r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[10], r[11], r[12]);
    if ((pc & ~1u) == PC_2FEC3C) {
        uint32_t idx = r[0];
        uint32_t base = r[1];
        uint32_t ea = base + idx;
        printf("[C0_SOT] fault_decode insn=LDRSH r1,[r1,r0] raw=0x5E09 base_r1=0x%X "
               "index_r0=0x%X ea=0x%X case=%s evidence=OBSERVED\n",
               base, idx, ea, base == 0 ? "A_nested_NULL" : "B_or_C_check_mapping");
    }
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static void on_c0_code(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    uint32_t pc = (uint32_t)address & ~1u;
    (void)size;
    (void)user;
    if (!product_c0_sot_enabled()) return;
    if (pc == PC_2FEBBC) {
        dump_regs(uc, "enter_2FEBBC");
        return;
    }
    if (pc == PC_2FEC3C) {
        dump_regs(uc, "at_2FEC3C");
        return;
    }
    /* After BL 0x2F9230 returns into 2FEBBC path: capture root in r4 (observed). */
    if (pc >= 0x2FEBC0u && pc <= 0x2FEC20u) {
        uint32_t r4 = 0;
        uc_reg_read(uc, UC_ARM_REG_R4, &r4);
        if (r4 && r4 != g_root && (r4 & 0xFFF00000u) == 0x00600000u) {
            g_root = r4;
            printf("[C0_SOT] root_capture pc=0x%X root=0x%X note=diagnostic_not_product_const "
                   "evidence=OBSERVED\n",
                   pc, g_root);
            fflush(stdout);
            if (!g_root_watch_ok) {
                uc_hook hm = 0;
                if (uc_hook_add(uc, &hm, UC_HOOK_MEM_WRITE, (void *)on_root_write, NULL,
                                (uint64_t)g_root, (uint64_t)g_root + 0x7Full) == UC_ERR_OK)
                    g_root_watch_ok = 1;
            }
        }
    }
}

static void on_root_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                          int64_t value, void *user) {
    uint32_t pc = 0, lr = 0, r9 = 0;
    uint32_t off;
    (void)type;
    (void)user;
    if (!product_c0_sot_enabled() || !g_root) return;
    if ((uint32_t)address < g_root || (uint32_t)address >= g_root + 0x80u) return;
    off = (uint32_t)address - g_root;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    printf("[C0_SOT_WRITE] object=root base=0x%X off=0x%X size=%d new=0x%llX store_pc=0x%X "
           "lr=0x%X r9=0x%X evidence=OBSERVED\n",
           g_root, off, size, (unsigned long long)(uint32_t)value, pc, lr, r9);
    fflush(stdout);
}

static void on_e6c_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                         int64_t value, void *user) {
    uint32_t pc = 0, lr = 0, r9 = 0;
    (void)type;
    (void)user;
    if (!product_c0_sot_enabled()) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    printf("[C0_SOT_WRITE] object=R9_E6C addr=0x%X size=%d new=0x%llX store_pc=0x%X lr=0x%X "
           "r9=0x%X evidence=OBSERVED\n",
           (uint32_t)address, size, (unsigned long long)(uint32_t)value, pc, lr, r9);
    fflush(stdout);
}
#endif

void product_c0_sot_arm_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_hook h = 0;
    if (!product_c0_sot_enabled() || !uc || g_hook_ok) return;
    g_uc = uc;
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_c0_code, NULL,
                      (uint64_t)PC_2FEBBC, (uint64_t)PC_2FEBBC + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_c0_code, NULL,
                      (uint64_t)0x2FEBC0u, (uint64_t)0x2FEC50u);
    g_hook_ok = 1;
    printf("[C0_SOT] hooks_armed 2FEBBC + 2FEBC0..2FEC50 evidence=OBSERVED\n");
    fflush(stdout);
#else
    (void)uc;
#endif
}

void product_c0_sot_on_c0_enter(void *uc, uint32_t er_rw, const char *why) {
    uint32_t e6c = 0;
    if (!product_c0_sot_enabled() || !uc) return;
    if (er_rw) g_er_rw = er_rw;
    product_c0_sot_arm_hooks(uc);
#ifdef GWY_HAVE_UNICORN
    if (g_er_rw && !g_e6c_watch_ok) {
        uc_hook hm = 0;
        uint64_t a = (uint64_t)g_er_rw + OFF_E6C;
        if (uc_hook_add((uc_engine *)uc, &hm, UC_HOOK_MEM_WRITE, (void *)on_e6c_write, NULL, a,
                        a + 3ull) == UC_ERR_OK)
            g_e6c_watch_ok = 1;
    }
    if (g_root && !g_root_watch_ok) {
        uc_hook hm = 0;
        if (uc_hook_add((uc_engine *)uc, &hm, UC_HOOK_MEM_WRITE, (void *)on_root_write, NULL,
                        (uint64_t)g_root, (uint64_t)g_root + 0x7Full) == UC_ERR_OK)
            g_root_watch_ok = 1;
    }
#endif
    if (g_er_rw)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_E6C, &e6c);
    printf("[C0_SOT] c0_enter why=%s er_rw=0x%X R9+E6C=0x%X root=0x%X evidence=OBSERVED\n",
           why ? why : "?", g_er_rw, e6c, g_root);
    fflush(stdout);
    dump_regs(uc, "c0_enter_snapshot");
}
