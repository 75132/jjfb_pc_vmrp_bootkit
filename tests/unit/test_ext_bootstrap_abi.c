#include "gwy_launcher/ext_bootstrap_abi.h"
#include "gwy_launcher/module_r9_switch.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    ext_bootstrap_abi_reset();
    if (strcmp(ext_bootstrap_abi_class_name(BS_ABI_PRESERVE_CALLER_R9_DOCUMENTED),
               "BOOTSTRAP_PRESERVE_CALLER_R9_DOCUMENTED") != 0)
        return 1;
    if (strcmp(ext_bootstrap_abi_class_name(BS_ABI_R9_UNUSED), "BOOTSTRAP_R9_UNUSED") != 0)
        return 2;
    if (strcmp(ext_bootstrap_abi_class_name(BS_ABI_ENTRY_POINTER_MISINTERPRETED),
               "BOOTSTRAP_ENTRY_POINTER_MISINTERPRETED") != 0)
        return 3;
    if (ext_bootstrap_abi_enabled()) return 4;
    if (ext_bootstrap_abi_last_class() != BS_ABI_UNKNOWN) return 5;
    if (strcmp(gwy_module_call_kind_name(GWY_CALL_BOOTSTRAP_ENTRY), "BOOTSTRAP_ENTRY") != 0)
        return 6;
    printf("test_ext_bootstrap_abi OK\n");
    return 0;
}
