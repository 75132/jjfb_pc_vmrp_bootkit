#include "gwy_launcher/ext_mrpgcmap_entry_order.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
static void set_env(const char *kv) { _putenv(kv); }
#else
static void set_env_pair(const char *k, const char *v) { setenv(k, v, 1); }
#endif

int main(void) {
#ifdef _WIN32
    set_env("JJFB_FIX_MRPGCMAP_ENTRY_ORDER=observe");
#else
    set_env_pair("JJFB_FIX_MRPGCMAP_ENTRY_ORDER", "observe");
#endif
    ext_mrpgcmap_entry_order_reset();
    if (ext_mrpgcmap_entry_order_mode() != GWY_MRPGCMAP_ENTRY_OBSERVE) {
        fprintf(stderr, "expected observe mode\n");
        return 1;
    }
    if (!ext_mrpgcmap_entry_order_enabled()) return 2;
    /* Observe mode must not run emu — before_continuation only logs. */
    ext_mrpgcmap_entry_order_on_module_mapped("gbrwcore.ext", 0x2EB7E0u, 147196u);
    ext_mrpgcmap_entry_order_on_first_pc("gbrwcore.ext", 0x2EB7E0u, 0x30CA96u);
    ext_mrpgcmap_entry_order_before_continuation(NULL, "gbrwcore.ext", 0x30CA96u);
    if (ext_mrpgcmap_entry_order_entry_ran()) {
        fprintf(stderr, "observe mode must not run entry emu\n");
        return 3;
    }
    ext_mrpgcmap_entry_order_finalize("harness");
#ifdef _WIN32
    set_env("JJFB_FIX_MRPGCMAP_ENTRY_ORDER=gbrwcore_only");
#else
    set_env_pair("JJFB_FIX_MRPGCMAP_ENTRY_ORDER", "gbrwcore_only");
#endif
    ext_mrpgcmap_entry_order_reset();
    if (ext_mrpgcmap_entry_order_mode() != GWY_MRPGCMAP_ENTRY_GBRWCORE_ONLY) {
        fprintf(stderr, "expected gbrwcore_only\n");
        return 4;
    }
#ifdef _WIN32
    set_env("JJFB_FIX_MRPGCMAP_ENTRY_ORDER=shell_core");
#else
    set_env_pair("JJFB_FIX_MRPGCMAP_ENTRY_ORDER", "shell_core");
#endif
    ext_mrpgcmap_entry_order_reset();
    if (ext_mrpgcmap_entry_order_mode() != GWY_MRPGCMAP_ENTRY_SHELL) {
        fprintf(stderr, "expected shell_core alias -> shell\n");
        return 5;
    }
    printf("test_ext_mrpgcmap_entry_order OK\n");
    return 0;
}
