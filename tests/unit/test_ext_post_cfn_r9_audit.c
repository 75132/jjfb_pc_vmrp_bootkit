#include "gwy_launcher/ext_post_cfn_r9_audit.h"
#include "gwy_launcher/module_r9_switch.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <stdlib.h>
#endif

static void env_on(void) {
#ifdef _WIN32
    _putenv("GWY_POST_CFN_R9_AUDIT=1");
#else
    setenv("GWY_POST_CFN_R9_AUDIT", "1", 1);
#endif
}

static void env_off(void) {
#ifdef _WIN32
    _putenv("GWY_POST_CFN_R9_AUDIT=");
#else
    unsetenv("GWY_POST_CFN_R9_AUDIT");
#endif
}

int main(void) {
    uint32_t regs[16];
    ModuleR9Frame fr;

    ext_post_cfn_r9_audit_reset();
    env_off();
    ext_post_cfn_r9_audit_reset();

    if (strcmp(ext_post_cfn_r9_class_name(PCFN_POST_CFN_R9_PROMOTION_REQUIRED),
               "POST_CFN_R9_PROMOTION_REQUIRED") != 0)
        return 1;
    if (strcmp(ext_post_cfn_r0_source_name(PCFN_R0_R9_REL_GLOBAL), "R9_REL_GLOBAL") != 0)
        return 2;
    if (strcmp(ext_post_cfn_registry_pending_name(PCFN_REG_PUBLICATION_LATE),
               "ER_RW_REGISTRY_PUBLICATION_LATE") != 0)
        return 3;
    if (ext_post_cfn_r9_gate_open()) return 4;
    if (module_r9_switch_peek_at(0, &fr) != 0) return 5;

    env_on();
    ext_post_cfn_r9_audit_reset();
    if (!ext_post_cfn_r9_audit_enabled()) return 6;
    if (ext_post_cfn_r9_audit_armed()) return 7;

    memset(regs, 0, sizeof(regs));
    regs[9] = 0x1000;
    ext_post_cfn_r9_audit_on_continuation_resume(NULL, 3, "robotol.ext", 0x100, 0x102, regs, 0x2000,
                                                 0x3000, 0);
    if (!ext_post_cfn_r9_audit_armed()) return 8;

    ext_post_cfn_r9_audit_finalize("HARNESS_TIMEOUT");
    /* Class may be UNKNOWN/CONTINUATION_* without fault; just ensure finalize ran. */
    if (!ext_post_cfn_r9_audit_armed()) return 9;

    env_off();
    printf("test_ext_post_cfn_r9_audit OK\n");
    return 0;
}
