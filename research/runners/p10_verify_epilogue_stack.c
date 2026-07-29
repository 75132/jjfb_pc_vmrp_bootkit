/*
 * P10 — Standalone proof that the 0x304BF0 resume-to-epilogue host contract is
 * stack-safe, using the REAL guest epilogue bytes.
 *
 * Replicates exactly what restore_304bf0_ok(mode=epilogue) sets up:
 *   - writes the PUSH save area [r0,r1,r2,r3,r4,r5,r6,r7,lr] at g_entry_sp-36
 *   - SP = g_entry_sp - 224  (prologue PUSH 36 + SUB 188)
 *   - PC = 0x304C4B (Thumb; lands on `add sp,#0xcc`)
 *   - R0 = host status
 * then emulates the REAL epilogue and asserts:
 *   - SP returns to g_entry_sp          (224 out == 224 back)
 *   - PC == landing pad (pop{pc} target) (real pop{pc} returns)
 *   - r4..r7 restored from save area
 *   - R0 preserved as host status
 *
 * The pop{pc} target (caller lr) is pointed at a mapped landing pad that holds a
 * self-branch (`b .`), so emulation halts there cleanly via `until`.
 *
 * Build (i686-mingw32, msys2):
 *   PATH=/c/msys64/mingw32/bin:$PATH \
 *   gcc -m32 -I<...>/unicorn-1.0.2-win32/include \
 *       -L<...>/unicorn-1.0.2-win32 -lunicorn \
 *       p10_verify_epilogue_stack.c -o p10_verify_epilogue_stack.exe
 * (unicorn.dll must be alongside the exe.)
 */
#include <unicorn/unicorn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define EPILOGUE_ADDR 0x304C4Au   /* add sp,#0xcc ; pop{r4,r5,r6,r7,pc} */
#define LANDING_ADDR  0x304C60u   /* mapped self-branch (b .) = pop{pc} target */

/* Real guest epilogue, little-endian Thumb halfwords:
 *   add sp,#0xcc  -> halfword 0xB033 -> LE bytes 33 B0
 *   pop{r4-r7,pc} -> halfword 0xBDF0 -> LE bytes F0 BD
 */
static const unsigned char k_epilogue[4] = {0x33, 0xB0, 0xF0, 0xBD};
/* b .  (branch to self) -> halfword 0xE7FE -> LE bytes FE E7 */
static const unsigned char k_selfbranch[2] = {0xFE, 0xE7};

static uint32_t g_entry_sp = 0x200000u;
static uint32_t g_entry_lr = LANDING_ADDR; /* caller return address (mapped pad) */
static uint32_t saved[9];                  /* [r0,r1,r2,r3,r4,r5,r6,r7,lr] */

static int g_insn_count = 0;

static void hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    uint32_t sp = 0;
    (void)user;
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    printf("[TRACE] exec pc=0x%llX size=%u sp=0x%X\n",
           (unsigned long long)address, size, sp);
    g_insn_count++;
    if (g_insn_count > 16) uc_emu_stop(uc); /* safety guard */
}

static int fail(const char *what) {
    printf("[FAIL] %s\n", what);
    return 1;
}

int main(void) {
    uc_engine *uc;
    uc_err err;
    uc_hook htrace;
    uint32_t sp, pc, r4, r5, r6, r7, r0;
    int rc = 0;

    /* Distinct, traceable values for the caller-saved set. */
    saved[0] = 0x11111111u; /* r0 (POP does NOT restore r0; host status stays) */
    saved[1] = 0x22222222u; /* r1 */
    saved[2] = 0x33333333u; /* r2 */
    saved[3] = 0x44444444u; /* r3 */
    saved[4] = 0xAABBCCDDu; /* r4 */
    saved[5] = 0x55667788u; /* r5 */
    saved[6] = 0x99AABBCCu; /* r6 */
    saved[7] = 0xDDCCBBAAu; /* r7 */
    saved[8] = g_entry_lr | 1u; /* lr (pop{pc} target, Thumb bit set) */

    err = uc_open(UC_ARCH_ARM, UC_MODE_THUMB, &uc);
    if (err != UC_ERR_OK) { printf("[FAIL] uc_open %s\n", uc_strerror(err)); return 1; }

    /* Map code region containing the epilogue + landing pad. */
    err = uc_mem_map(uc, 0x300000u, 0x10000u, UC_PROT_ALL);
    if (err != UC_ERR_OK) { printf("[FAIL] map code %s\n", uc_strerror(err)); return 1; }
    /* Map a generous stack region (covers 0x100000..0x300000). */
    err = uc_mem_map(uc, 0x100000u, 0x200000u, UC_PROT_ALL);
    if (err != UC_ERR_OK) { printf("[FAIL] map stack %s\n", uc_strerror(err)); return 1; }

    if (uc_mem_write(uc, EPILOGUE_ADDR, k_epilogue, sizeof(k_epilogue)) != UC_ERR_OK)
        { printf("[FAIL] write epilogue bytes\n"); return 1; }
    if (uc_mem_write(uc, LANDING_ADDR, k_selfbranch, sizeof(k_selfbranch)) != UC_ERR_OK)
        { printf("[FAIL] write landing pad\n"); return 1; }

    /* Read back and confirm the epilogue halfwords decode as expected. */
    {
        unsigned char rb[4] = {0};
        uc_mem_read(uc, EPILOGUE_ADDR, rb, 4);
        uint16_t hw0 = (uint16_t)(rb[0] | (rb[1] << 8));
        uint16_t hw1 = (uint16_t)(rb[2] | (rb[3] << 8));
        printf("[VERIFY] epilogue halfwords: 0x%04X (add sp,#0xcc) 0x%04X (pop{r4-r7,pc})\n",
               hw0, hw1);
    }

    /* Write the PUSH save area the host reconstructs (at g_entry_sp - 36). */
    uint32_t save_area = g_entry_sp - 36u;
    if (uc_mem_write(uc, save_area, saved, sizeof(saved)) != UC_ERR_OK)
        { printf("[FAIL] write save area\n"); return 1; }

    /* Host setup for epilogue mode (mirrors restore_304bf0_ok). */
    uint32_t new_sp = g_entry_sp - 224u;   /* post-prologue SP */
    uint32_t status = 0;                    /* host R0 */
    uc_reg_write(uc, UC_ARM_REG_SP, &new_sp);
    uc_reg_write(uc, UC_ARM_REG_R0, &status);
    /* r1..r7 scratch: set to sentinels so we can see the POP overwrite r4-r7. */
    { uint32_t z = 0xEEEEEEEEu;
      uc_reg_write(uc, UC_ARM_REG_R4, &z);
      uc_reg_write(uc, UC_ARM_REG_R5, &z);
      uc_reg_write(uc, UC_ARM_REG_R6, &z);
      uc_reg_write(uc, UC_ARM_REG_R7, &z); }

    uc_hook_add(uc, &htrace, UC_HOOK_CODE, (void *)hook_code, NULL,
                0x300000u, 0x310000u);

    printf("[VERIFY] emulate real epilogue @0x%X sp_before=0x%X save_area=0x%X land=0x%X\n",
           EPILOGUE_ADDR, new_sp, save_area, LANDING_ADDR);

    /* Run from the epilogue until PC reaches the landing pad (pop{pc} target).
     * Unicorn selects Thumb vs ARM per emu_start by the LSB of the begin address,
     * so we MUST pass EPILOGUE_ADDR|1 or it decodes ARM-width (4-byte) instructions. */
    err = uc_emu_start(uc, EPILOGUE_ADDR | 1u, LANDING_ADDR, 0, 0);
    if (err != UC_ERR_OK) {
        printf("[FAIL] emu_start %s\n", uc_strerror(err));
        uc_close(uc);
        return 1;
    }

    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    uc_reg_read(uc, UC_ARM_REG_R5, &r5);
    uc_reg_read(uc, UC_ARM_REG_R6, &r6);
    uc_reg_read(uc, UC_ARM_REG_R7, &r7);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);

    printf("[VERIFY] sp_after=0x%X pc_after=0x%X r0=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X\n",
           sp, pc, r0, r4, r5, r6, r7);

    /* Assertions: */
    if (sp != g_entry_sp) rc |= fail("SP not balanced (expected g_entry_sp)");
    if (pc != LANDING_ADDR && pc != (LANDING_ADDR | 1u))
        rc |= fail("PC did not return to caller lr (landing pad)");
    if (r4 != saved[4] || r5 != saved[5] || r6 != saved[6] || r7 != saved[7])
        rc |= fail("r4-r7 not restored from save area");
    if (r0 != status)
        rc |= fail("R0 not preserved as host status");

    if (rc == 0) {
        printf("[PASS] resume-to-epilogue stack contract: SP 224-out/224-back balanced, "
               "PC->lr, r4-r7 restored, R0=status. Equivalent to direct_lr.\n");
    }
    uc_close(uc);
    return rc;
}
