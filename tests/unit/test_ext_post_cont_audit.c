#include "gwy_launcher/ext_post_cont_audit.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <stdlib.h>
#endif

static void env_on(void) {
#ifdef _WIN32
    _putenv("GWY_POST_CONT_AUDIT=1");
#else
    setenv("GWY_POST_CONT_AUDIT", "1", 1);
#endif
}

static void env_off(void) {
#ifdef _WIN32
    _putenv("GWY_POST_CONT_AUDIT=");
#else
    unsetenv("GWY_POST_CONT_AUDIT");
#endif
}

int main(void) {
    uint32_t regs[16];
    memset(regs, 0, sizeof(regs));

    ext_post_cont_audit_reset();
    env_off();
    /* Force re-read of env. */
    ext_post_cont_audit_reset();
    if (strcmp(ext_post_cont_class_name(PC_CLASS_MODULE_ER_RW_RUNTIME_GAP),
               "MODULE_ER_RW_RUNTIME_GAP") != 0)
        return 1;
    if (strcmp(ext_post_cont_event_name(PC_EVT_ROBOTOL_CONTINUATION), "ROBOTOL_CONTINUATION") != 0)
        return 2;
    if (strcmp(ext_post_cont_class_name(PC_CLASS_MISSING_EVENT_SCHEDULING),
               "MISSING_EVENT_SCHEDULING") != 0)
        return 3;
    if (ext_post_cont_gate_open()) return 4;
    if (ext_post_cont_graphics_gate_open()) return 5;
    if (ext_post_cont_event_scheduler_gate_open()) return 6;

    env_on();
    ext_post_cont_audit_reset();
    if (!ext_post_cont_audit_enabled()) return 7;
    if (ext_post_cont_audit_armed()) return 8;

    regs[0] = 0x11;
    regs[9] = 0x280400;
    ext_post_cont_audit_on_continuation_resume(NULL, 3, "robotol.ext", 0x100, 0x102, regs, 0x2000,
                                               0x3000, 0);
    if (!ext_post_cont_audit_armed()) return 9;

    ext_post_cont_audit_on_code(NULL, 3, 0x104, regs, 0);
    if (ext_post_cont_audit_instruction_count() < 1) return 10;

    ext_post_cont_audit_finalize("HARNESS_TIMEOUT");
    if (ext_post_cont_audit_last_class() == PC_CLASS_UNKNOWN) return 11;
    if (!ext_post_cont_gate_open()) return 12;

    /* Second finalize is no-op. */
    ext_post_cont_audit_finalize("AGAIN");
    env_off();
    printf("test_ext_post_cont_audit OK\n");
    return 0;
}
