/*
 * P17: permanent nested-emu guard.
 * Proves: Hook enqueue zone raises hook_depth; Guest run blocked inside hook;
 * PC=LR exit modeled by hook leave; drain depth tracks on outer boundary.
 */
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

int main(void) {
    GwyUcEntryRunOut out;
    GwyUcEntryAbi abi;
    int ok;
    uint64_t blocked0;

    gwy_emu_nest_depths_reset();
    if (gwy_emu_hook_depth() || gwy_emu_guest_run_depth() || gwy_emu_family_drain_depth())
        return fail("depths not zero after reset");
    if (gwy_emu_nested_in_code_hook_blocked())
        return fail("should not block with hook_depth=0");

    /* 1) Hook-inner enqueue succeeds as bookkeeping; Guest must not run. */
    gwy_emu_hook_enter("unit_test_map_func");
    if (gwy_emu_hook_depth() != 1) return fail("hook_depth after enter");
    if (!gwy_emu_nested_in_code_hook_blocked()) return fail("must block while in hook");

    blocked0 = gwy_emu_nested_block_count();
    memset(&abi, 0, sizeof(abi));
    abi.set_lr = 1;
    abi.lr = 0x80000u;
    memset(&out, 0, sizeof(out));
    ok = guest_memory_uc_run_entry_ex(NULL, 0x30D311u, 0x80000u, 1000ull, &abi, &out);
    if (ok != 0) return fail("run_entry must return 0 when nested blocked");
    if (out.end_class != GWY_ENTRY_NESTED_EMU_BLOCKED)
        return fail("expected NESTED_EMU_BLOCKED");
    if (gwy_emu_nested_block_count() != blocked0 + 1)
        return fail("block_count must increment");
    if (gwy_emu_guest_run_depth() != 0) return fail("guest must not run inside hook");

    /* 2) Hook leave = PC=LR exit from MAP_FUNC; outer boundary free. */
    gwy_emu_hook_leave("unit_test_map_func");
    if (gwy_emu_hook_depth() != 0) return fail("hook_depth after leave");
    if (gwy_emu_nested_in_code_hook_blocked()) return fail("must not block after leave");

    /* 3) Drain executes only on outer boundary (depth track). */
    gwy_emu_family_drain_enter("unit_drain");
    if (gwy_emu_family_drain_depth() != 1) return fail("family_drain_depth");
    gwy_emu_family_drain_leave("unit_drain");
    if (gwy_emu_family_drain_depth() != 0) return fail("family_drain leave");

    if (strcmp(gwy_uc_entry_end_class_name(GWY_ENTRY_NESTED_EMU_BLOCKED),
               "NESTED_EMU_BLOCKED") != 0)
        return fail("end class name");

    puts("test_nested_emu_guard OK");
    return 0;
}
