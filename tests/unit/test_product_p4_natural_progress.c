#include "gwy_launcher/product_p4_progress.h"
#include <stdio.h>
#include <string.h>

static int test_hash_stable(void) {
    uint32_t a[] = {1, 2, 3, 4};
    uint32_t b[] = {1, 2, 3, 4};
    uint32_t c[] = {1, 2, 3, 5};
    if (product_p4_hash_u32_seq(a, 4) != product_p4_hash_u32_seq(b, 4)) return 1;
    if (product_p4_hash_u32_seq(a, 4) == product_p4_hash_u32_seq(c, 4)) return 2;
    return 0;
}

static int test_classify_stable_poll(void) {
    uint32_t cf[8];
    uint32_t st[8];
    int i;
    for (i = 0; i < 8; i++) {
        cf[i] = 0xABCDu;
        st[i] = 0x1111u;
    }
    if (product_p4_classify_signatures(cf, 8, st, 0) != GWY_P4_CB_STABLE_POLL_LOOP) return 10;
    if (product_p4_classify_signatures(cf, 8, st, 1) != GWY_P4_CB_PROGRESSING) return 11;
    return 0;
}

static int test_classify_early_change(void) {
    uint32_t cf[8];
    uint32_t st[8];
    int i;
    for (i = 0; i < 8; i++) {
        cf[i] = (i < 2) ? 0x1u : 0x2u;
        st[i] = (i < 2) ? 0x10u : 0x20u;
    }
    if (product_p4_classify_signatures(cf, 8, st, 0) != GWY_P4_CB_STATE_CHANGES_NO_DRAW) return 20;
    return 0;
}

static int test_forbidden_markers(void) {
    if (!product_p4_line_forbidden_product("FORCE_CALLBACK tick=1")) return 30;
    if (!product_p4_line_forbidden_product("E10A31N_ARMED")) return 31;
    if (!product_p4_line_forbidden_product("DisplayFirst splash")) return 32;
    if (product_p4_line_forbidden_product("[SCHEDULER_NATURAL_CALLBACK] forced=no")) return 33;
    return 0;
}

static int test_ledger_names(void) {
    if (strcmp(gwy_p4_ledger_verdict_name(GWY_P4_LEDGER_EVENT_REGISTERED_NOT_GENERATED),
               "EVENT_REGISTERED_NOT_GENERATED") != 0)
        return 40;
    if (strcmp(gwy_p4_source_class_name(GWY_P4_SRC_PLATFORM_EVENT_PENDING),
               "PLATFORM_EVENT_PENDING") != 0)
        return 41;
    if (strcmp(gwy_p4_contract_verdict_name(GWY_P4_CONTRACT_ASYNC_COMPLETION_MISSING),
               "PLATFORM_ASYNC_COMPLETION_MISSING") != 0)
        return 42;
    return 0;
}

static int test_reset_safe(void) {
    product_p4_reset();
    product_p4_set_run_id("unit_p4");
    if (strcmp(product_p4_run_id(), "unit_p4") != 0) return 50;
    if (product_p4_callback_count() != 0) return 51;
    return 0;
}

int main(void) {
    int rc;
    if ((rc = test_hash_stable()) != 0) {
        printf("FAIL hash rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_classify_stable_poll()) != 0) {
        printf("FAIL classify_poll rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_classify_early_change()) != 0) {
        printf("FAIL classify_early rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_forbidden_markers()) != 0) {
        printf("FAIL forbidden rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_ledger_names()) != 0) {
        printf("FAIL ledger_names rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_reset_safe()) != 0) {
        printf("FAIL reset rc=%d\n", rc);
        return rc;
    }
    printf("OK product_p4_natural_progress\n");
    return 0;
}
