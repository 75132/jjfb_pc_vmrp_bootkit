#include "gwy_launcher/ext_er_rw_producer.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    ext_er_rw_producer_reset();
    if (strcmp(ext_er_rw_timing_class_name(ER_RW_BOOTSTRAP_ENTRY_PRECEDES),
               "BOOTSTRAP_ENTRY_PRECEDES_ER_RW") != 0)
        return 1;
    if (strcmp(ext_er_rw_timing_class_name(ER_RW_METADATA_PUBLICATION_LATE),
               "ER_RW_METADATA_PUBLICATION_LATE") != 0)
        return 2;
    if (strcmp(ext_er_rw_timing_class_name(ER_RW_MODULE_ENTRY_ORDERING_WRONG),
               "MODULE_ENTRY_ORDERING_WRONG") != 0)
        return 3;
    if (ext_er_rw_producer_enabled()) return 4;
    if (ext_er_rw_producer_last_class() != ER_RW_TIMING_UNKNOWN) return 5;
    printf("test_ext_er_rw_producer OK\n");
    return 0;
}
