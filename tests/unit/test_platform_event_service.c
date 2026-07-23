#include "gwy_launcher/platform_event_service.h"
#include "gwy_launcher/product_first_frame_push.h"
#include <stdio.h>
#include <string.h>

static int test_names(void) {
    if (strcmp(gwy_event_state_name(GWY_EVT_QUEUED), "QUEUED") != 0) return 1;
    if (strcmp(gwy_event_identity_class_name(GWY_EVT_ID_SAME_UNFINISHED),
               "SAME_UNFINISHED_REQUEST") != 0)
        return 2;
    return 0;
}

static int test_identity_not_code_app_alone(void) {
    uint32_t r_a[8] = {0x1E209u, 9u, 0, 0, 0, 0, 0, 0};
    uint32_t r_b[8] = {0x1E209u, 9u, 0x690CF4u, 0, 0, 0, 0, 0};
    uint64_t ha, hb;
    ha = platform_event_identity_hash(0x1E209u, 9u, r_a, 0, 0, 0, 0);
    hb = platform_event_identity_hash(0x1E209u, 9u, r_b, 0, 0, 0x690CF4u, 0);
    if (ha == hb) return 10; /* context must change identity */
    return 0;
}

static int test_same_identity_suppresses_requeue(void) {
    uint32_t r[8] = {0x1E209u, 9u, 0, 0, 0, 0, 0, 0};
    GwyPlatformEvent *ev = NULL;
    int a1, a2;
    platform_event_service_reset();
    platform_event_service_set_run_id("unit_pes");
    (void)platform_event_service_note_alloc(0x10165u, 0xE200u, 0x690CF4u, 0x30D2F9u, 3u);
    platform_event_service_note_ctx_owner_store(0x690CF4u, 0x2B215Cu);
    a1 = platform_event_service_on_guest_request(NULL, 0x1E209u, 9u, 0x1E200u, 0x30D301u,
                                                 0x30D2F9u, 0x304589u, 0x304589u, r, 0x2B1854u, 0, 0,
                                                 0, 3u, 3u, &ev);
    if (!a1 || !ev) return 20;
    platform_event_service_mark_queued(ev->request_id);
    a2 = platform_event_service_on_guest_request(NULL, 0x1E209u, 9u, 0x1E200u, 0x30D301u,
                                                 0x30D2F9u, 0x304589u, 0x304589u, r, 0x2B1854u, 0, 0,
                                                 0, 3u, 3u, &ev);
    if (a2 != 0) return 21; /* still queued → suppress requeue */
    if (platform_event_service_event_count() != 1) return 22;
    return 0;
}

static int test_ffp_reset(void) {
    product_ffp_reset();
    product_ffp_set_run_id("unit_ffp");
    if (strcmp(product_ffp_run_id(), "unit_ffp") != 0) return 30;
    return 0;
}

int main(void) {
    int rc;
    if ((rc = test_names()) != 0) {
        printf("FAIL names rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_identity_not_code_app_alone()) != 0) {
        printf("FAIL identity rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_same_identity_suppresses_requeue()) != 0) {
        printf("FAIL requeue rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_ffp_reset()) != 0) {
        printf("FAIL ffp rc=%d\n", rc);
        return rc;
    }
    printf("OK platform_event_service\n");
    return 0;
}
