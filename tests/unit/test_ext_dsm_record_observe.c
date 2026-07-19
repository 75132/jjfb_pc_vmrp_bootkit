#include "gwy_launcher/ext_dsm_record_observe.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    ext_dsm_record_observe_reset();
    if (strcmp(ext_contract_case_name(CONTRACT_CASE_HELPER_NOT_DISPATCH_TARGET),
               "HELPER_NOT_DISPATCH_TARGET") != 0)
        return 1;
    if (strcmp(ext_evidence_level_name(EV_DOCUMENTED), "DOCUMENTED") != 0) return 1;
    if (ext_dsm_record_contract_enabled()) return 1;
    if (ext_dsm_record_phase6b_b_open()) return 1;
    printf("test_ext_dsm_record_observe OK\n");
    return 0;
}
