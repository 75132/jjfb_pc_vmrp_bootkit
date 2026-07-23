#include "gwy_launcher/product_p5_event_advance.h"
#include <stdio.h>
#include <string.h>

static int test_names(void) {
    if (strcmp(gwy_p5_family_class_name(GWY_P5_FAMILY_EVENT_FALSE_ACK),
               "FAMILY_EVENT_FALSE_ACK") != 0)
        return 1;
    if (strcmp(gwy_p5_txn_verdict_name(GWY_P5_TXN_GUEST_REISSUED), "GUEST_REISSUED_REQUEST") != 0)
        return 2;
    if (strcmp(gwy_p5_10165_role_name(GWY_P5_10165_SECOND_STAGE_MISSING),
               "SECOND_STAGE_COMPLETION_MISSING") != 0)
        return 3;
    return 0;
}

static int test_reset(void) {
    product_p5_reset();
    product_p5_set_run_id("unit_p5");
    if (strcmp(product_p5_run_id(), "unit_p5") != 0) return 10;
    if (product_p5_state_advanced() != 0) return 11;
    return 0;
}

int main(void) {
    int rc;
    if ((rc = test_names()) != 0) {
        printf("FAIL names rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_reset()) != 0) {
        printf("FAIL reset rc=%d\n", rc);
        return rc;
    }
    printf("OK product_p5_event_advance\n");
    return 0;
}
