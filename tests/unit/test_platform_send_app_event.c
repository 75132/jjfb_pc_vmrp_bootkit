#include "gwy_launcher/platform_send_app_event.h"
#include <stdio.h>
#include <string.h>

static int expect_kind(const char *tag, GwyPlatCallKind got, GwyPlatCallKind want) {
    if (got != want) {
        fprintf(stderr, "%s: kind %d != %d\n", tag, (int)got, (int)want);
        return 0;
    }
    return 1;
}

int main(void) {
    GwyPlatCall call;
    GwyPlatCallResult out;

    memset(&call, 0, sizeof(call));
    call.code = 0x10102u;
    call.app = 0x1E200u;
    call.arg2 = 0x30D301u;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10102", out.kind, GWY_PLAT_KIND_REGISTER) || out.status_ret != 1u ||
        out.reg_family != 0x1E200u || out.reg_handler != 0x30D301u) {
        fprintf(stderr, "10102 register mismatch\n");
        return 1;
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10113u;
    call.app = 0x11F02u;
    call.arg3 = 0x27FFA0u;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10113", out.kind, GWY_PLAT_KIND_GRAPHICS_FP) || out.status_ret != 0u ||
        out.graphics_out != 0x27FFA0u || out.graphics_id != 0x11F02u) {
        fprintf(stderr, "10113 graphics mismatch\n");
        return 1;
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10120u;
    call.app = 4u;
    call.arg2 = 0x682A5Cu;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10120", out.kind, GWY_PLAT_KIND_REGISTER) || out.status_ret != 1u ||
        out.reg_handler != 0x682A5Cu) {
        fprintf(stderr, "10120 bind mismatch\n");
        return 1;
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10140u;
    call.app = 5u;
    call.arg2 = 0x682A5Cu;
    call.arg3 = 0x30630Du;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10140", out.kind, GWY_PLAT_KIND_REGISTER) || out.status_ret != 1u ||
        out.reg_handler != 0x30630Du) {
        fprintf(stderr, "10140 register mismatch\n");
        return 1;
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10162u;
    call.app = 0xE200u;
    call.arg2 = 0x30D249u;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10162", out.kind, GWY_PLAT_KIND_ALLOC) || out.alloc_size != 0xE200u ||
        out.reg_handler != 0x30D249u) {
        fprintf(stderr, "10162 alloc+handler mismatch\n");
        return 1;
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10165u;
    call.app = 0xE200u;
    call.arg2 = 0x30D2F9u;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10165", out.kind, GWY_PLAT_KIND_ALLOC) || out.alloc_size != 0xE200u ||
        out.reg_handler != 0x30D2F9u || out.reg_family != 0xE200u) {
        fprintf(stderr, "10165 alloc+handler mismatch\n");
        return 1;
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10800u;
    call.app = 4u;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10800", out.kind, GWY_PLAT_KIND_STATUS) || out.status_ret != 1u) {
        fprintf(stderr, "10800 ack mismatch\n");
        return 1;
    }

    printf("ok\n");
    return 0;
}
