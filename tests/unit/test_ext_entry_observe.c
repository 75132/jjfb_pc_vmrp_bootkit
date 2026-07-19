#include "gwy_launcher/ext_entry_observe.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    int rt = -1, rn = -1, is_load = 0, rm = -1;
    uint32_t imm = 0;
    /* ARM LDR r3, [r0, #0x28]  e.g. e5903028 */
    uint32_t arm_ldr = 0xE5903028u;
    if (!ext_entry_decode_ldr_imm(arm_ldr, 0, &rt, &rn, &imm, &is_load)) {
        fprintf(stderr, "ARM LDR decode failed\n");
        return 1;
    }
    if (rt != 3 || rn != 0 || imm != 0x28 || !is_load) {
        fprintf(stderr, "ARM LDR fields wrong rt=%d rn=%d imm=%u load=%d\n", rt, rn, imm, is_load);
        return 1;
    }

    /* Thumb LDR r0, [r1, #0x28]: imm5=10 → 01101|01010|001|000 = 0x6A88 */
    {
        uint32_t thumb = 0x6A88u;
        if (!ext_entry_decode_ldr_imm(thumb, 1, &rt, &rn, &imm, &is_load)) {
            fprintf(stderr, "Thumb LDR decode failed\n");
            return 1;
        }
        if (rt != 0 || rn != 1 || imm != 0x28 || !is_load) {
            fprintf(stderr, "Thumb LDR fields wrong rt=%d rn=%d imm=%u\n", rt, rn, imm);
            return 1;
        }
    }

    /* Thumb BLX r0: 010001111 0000 000 = 0x4780 */
    if (!ext_entry_decode_thumb_blx_rm(0x4780u, &rm) || rm != 0) {
        fprintf(stderr, "Thumb BLX r0 decode failed rm=%d\n", rm);
        return 1;
    }
    /* Thumb BLX r7: Rm=7 → 0x4780 | (7<<3) = 0x47B8 */
    if (!ext_entry_decode_thumb_blx_rm(0x47B8u, &rm) || rm != 7) {
        fprintf(stderr, "Thumb BLX r7 decode failed rm=%d\n", rm);
        return 1;
    }
    if (!ext_entry_decode_arm_blx_rm(0xE12FFF37u, &rm) || rm != 7) {
        fprintf(stderr, "ARM BLX r7 decode failed rm=%d\n", rm);
        return 1;
    }

    if (strcmp(ext_fault_root_cause_name(EXT_FAULT_CLASS_ENTRY_ARGUMENT), "ENTRY_ARGUMENT") != 0)
        return 1;
    if (strcmp(helper_abi_stage_name(HELPER_ABI_STAGE_CALL_SITE_BEFORE), "CALL_SITE_BEFORE") != 0)
        return 1;
    if (strcmp(helper_abi_stage_name(HELPER_ABI_STAGE_THUNK_ENTER), "THUNK_ENTER") != 0)
        return 1;
    if (strcmp(helper_abi_stage_name(HELPER_ABI_STAGE_ROBOTOL_ENTER), "ROBOTOL_ENTER") != 0)
        return 1;

    puts("test_ext_entry_observe OK");
    return 0;
}
