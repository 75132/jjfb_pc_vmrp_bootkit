#include "gwy_launcher/ext_entry_null_contract.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    ext_entry_null_contract_reset();
    if (strcmp(ext_null_contract_class_name(NULL_CONTRACT_UNKNOWN), "UNKNOWN") != 0) return 1;
    if (strcmp(ext_null_contract_class_name(NULL_CONTRACT_ENTRY_NULL_CROSS_TARGET),
               "ENTRY_NULL_CROSS_TARGET") != 0)
        return 1;
    if (strcmp(ext_null_contract_class_name(NULL_CONTRACT_MISSING_MODULE_DATA_INITIALIZATION),
               "MISSING_MODULE_DATA_INITIALIZATION") != 0)
        return 1;
    if (ext_entry_null_contract_enabled()) return 1;
    if (ext_entry_null_contract_xt_crossed()) return 1;
    if (ext_entry_null_contract_last_class() != NULL_CONTRACT_UNKNOWN) return 1;
    printf("test_ext_entry_null_contract OK\n");
    return 0;
}
