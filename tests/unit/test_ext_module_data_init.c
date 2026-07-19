#include "gwy_launcher/ext_module_data_init.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    ext_module_data_init_reset();
    if (strcmp(ext_data_init_class_name(DATA_INIT_MISSING_MODULE_R9_SWITCH),
               "MISSING_MODULE_R9_SWITCH") != 0)
        return 1;
    if (strcmp(ext_data_init_class_name(DATA_INIT_MISSING_RW_TEMPLATE_INITIALIZATION),
               "MISSING_RW_TEMPLATE_INITIALIZATION") != 0)
        return 1;
    if (strcmp(ext_data_init_class_name(DATA_INIT_REGISTRY_DATA_METADATA_MISSING),
               "REGISTRY_DATA_METADATA_MISSING") != 0)
        return 1;
    if (ext_module_data_init_enabled()) return 1;
    if (ext_module_data_init_last_class() != DATA_INIT_UNKNOWN) return 1;
    printf("test_ext_module_data_init OK\n");
    return 0;
}
