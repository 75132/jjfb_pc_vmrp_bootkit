#include "gwy_launcher/ext_module_entry_abi.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    ext_module_entry_abi_reset();
    if (strcmp(ext_load_return_class_name(LOAD_RET_CODE_POINTER), "CODE_POINTER") != 0)
        return 1;
    if (strcmp(ext_load_return_transform_name(LOAD_XFORM_THUMB_BIT), "THUMB_BIT") != 0)
        return 1;
    if (strcmp(ext_module_entry_root_cause_name(ME_ROOT_UNKNOWN), "UNKNOWN") != 0) return 1;
    if (strcmp(ext_module_entry_root_cause_name(ME_ROOT_LOADCODE_RETURN_MISINTERPRETED),
               "LOADCODE_RETURN_MISINTERPRETED") != 0)
        return 1;
    if (strcmp(ext_r0_assign_kind_name(R0_ASSIGN_EXPLICIT_ZERO), "explicit_zero") != 0) return 1;
    if (ext_module_entry_abi_enabled()) return 1;
    if (ext_module_entry_abi_mr_helper_seen()) return 1;
    if (ext_module_entry_abi_last_root_cause() != ME_ROOT_UNKNOWN) return 1;
    printf("test_ext_module_entry_abi OK\n");
    return 0;
}
