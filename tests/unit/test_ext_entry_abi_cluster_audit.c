#include "gwy_launcher/ext_entry_abi_cluster_audit.h"
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
    set_env("JJFB_ENTRY_ABI_AUDIT=1");
    set_env("JJFB_ENTRY_ABI_VARIANT=baseline");
#else
    set_env_pair("JJFB_ENTRY_ABI_AUDIT", "1");
    set_env_pair("JJFB_ENTRY_ABI_VARIANT", "baseline");
#endif
    ext_entry_abi_cluster_audit_reset();
    if (!ext_entry_abi_cluster_audit_enabled()) {
        fprintf(stderr, "expected audit enabled\n");
        return 1;
    }
    if (ext_entry_abi_cluster_audit_variant() != GWY_ENTRY_ABI_BASELINE) return 2;
    if (strcmp(ext_entry_abi_cluster_audit_variant_name(GWY_ENTRY_ABI_R0_P), "r0_p") != 0)
        return 3;
#ifdef _WIN32
    set_env("JJFB_ENTRY_ABI_VARIANT=mirror_callback_regs");
#else
    set_env_pair("JJFB_ENTRY_ABI_VARIANT", "mirror_callback_regs");
#endif
    ext_entry_abi_cluster_audit_reset();
    if (ext_entry_abi_cluster_audit_variant() != GWY_ENTRY_ABI_MIRROR_CALLBACK) return 4;
    ext_entry_abi_cluster_audit_on_p_candidate(0x2AC8DCu, "nested_cfn");
    ext_entry_abi_cluster_audit_on_p_write(0x2AC8DCu, 0x0Cu, 0, 0, 0x1000u, "test");
    if (ext_entry_abi_cluster_audit_any_pxc_nonzero()) return 5;
    ext_entry_abi_cluster_audit_finalize("harness");
    printf("test_ext_entry_abi_cluster_audit OK\n");
    return 0;
}
