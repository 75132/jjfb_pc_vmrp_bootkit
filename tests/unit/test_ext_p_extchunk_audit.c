#include "gwy_launcher/ext_p_extchunk_audit.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <stdlib.h>
#endif

static void env_on(void) {
#ifdef _WIN32
    _putenv("GWY_P_EXTCHUNK_AUDIT=1");
#else
    setenv("GWY_P_EXTCHUNK_AUDIT", "1", 1);
#endif
}

static void env_off(void) {
#ifdef _WIN32
    _putenv("GWY_P_EXTCHUNK_AUDIT=");
#else
    unsetenv("GWY_P_EXTCHUNK_AUDIT");
#endif
}

int main(void) {
    uint32_t regs[16];

    ext_p_extchunk_audit_reset();
    env_off();
    ext_p_extchunk_audit_reset();

    if (strcmp(ext_p_extchunk_class_name(EXTCHUNK_NEVER_WRITTEN), "EXTCHUNK_NEVER_WRITTEN") != 0)
        return 1;
    if (strcmp(ext_p_extchunk_class_name(EXTCHUNK_GWY_CONTEXT_HYPOTHESIS),
               "EXTCHUNK_GWY_CONTEXT_HYPOTHESIS") != 0)
        return 2;
    if (ext_p_extchunk_gate_open()) return 3;

    env_on();
    ext_p_extchunk_audit_reset();
    if (!ext_p_extchunk_audit_enabled()) return 4;
    if (ext_p_extchunk_audit_armed()) return 5;

    ext_p_extchunk_audit_on_cfunction_p(0x100, 0x2000, 20, 0, 0);

    memset(regs, 0, sizeof(regs));
    regs[3] = 0x2000;
    regs[9] = 0x1000;
    ext_p_extchunk_audit_on_continuation_resume(NULL, 3, "robotol.ext", 0x100, 0x102, regs, 0x3000,
                                                0x4000, 0);
    if (!ext_p_extchunk_audit_armed()) return 6;

    ext_p_extchunk_audit_finalize("HARNESS_TIMEOUT");
    if (ext_p_extchunk_gate_open()) return 7;

    env_off();
    printf("test_ext_p_extchunk_audit OK\n");
    return 0;
}
