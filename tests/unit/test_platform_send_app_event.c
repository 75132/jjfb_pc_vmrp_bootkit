#include "gwy_launcher/platform_send_app_event.h"
#include "gwy_launcher/platform_mrp_resource.h"
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

    memset(&call, 0, sizeof(call));
    call.code = 0x10133u;
    call.app = 0x1E205u;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10133", out.kind, GWY_PLAT_KIND_STATUS) || out.status_ret != 0u) {
        fprintf(stderr, "10133 free mismatch\n");
        return 1;
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10132u;
    call.app = 0x6AD120u;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10132_strdup", out.kind, GWY_PLAT_KIND_ALLOC) || out.fill_buf != 0x6AD120u ||
        out.alloc_size != 0u) {
        fprintf(stderr, "10132 strdup classify mismatch\n");
        return 1;
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10132u;
    call.app = 0x8u;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10132_malloc", out.kind, GWY_PLAT_KIND_ALLOC) || out.alloc_size != 0x8u ||
        out.fill_buf != 0u) {
        fprintf(stderr, "10132 size-malloc classify mismatch size=%u fill=0x%X\n", out.alloc_size,
                out.fill_buf);
        return 1;
    }

    /* 0x10134: size-only ALLOC fallback when no mrp_resource cache hit. */
    platform_mrp_resource_reset();
    memset(&call, 0, sizeof(call));
    call.code = 0x10134u;
    call.app = 0x2D8Au; /* loadingbar!201!29.bmp = 201*29*2 */
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10134_alloc", out.kind, GWY_PLAT_KIND_ALLOC) || out.alloc_size != 0x2D8Au ||
        out.fill_buf != 0u) {
        fprintf(stderr, "10134 size-alloc mismatch kind=%d size=%u fill=0x%X\n", (int)out.kind,
                out.alloc_size, out.fill_buf);
        return 1;
    }

    /* 0x10134: ALLOC+copy when pending has host_pixels (pending_id != 0). */
    platform_mrp_resource_note_pixels(0x2D8Au, 0x3920000u, 201, 29);
    memset(&call, 0, sizeof(call));
    call.code = 0x10134u;
    call.app = 0x2D8Au;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10134_copy", out.kind, GWY_PLAT_KIND_ALLOC) || out.alloc_size != 0x2D8Au ||
        out.resource_pending_id == 0) {
        fprintf(stderr, "10134 copy classify mismatch kind=%d size=%u pending=%llu fill=0x%X\n",
                (int)out.kind, out.alloc_size, (unsigned long long)out.resource_pending_id,
                out.fill_buf);
        return 1;
    }
    platform_mrp_resource_reset();

    memset(&call, 0, sizeof(call));
    call.code = 0x101ABu;
    call.app = 0x6AD11Cu;
    call.arg3 = 2u;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("101AB", out.kind, GWY_PLAT_KIND_BUFFER_FILL) || out.fill_buf != 0x6AD11Cu ||
        out.fill_type != 2u) {
        fprintf(stderr, "101AB fill mismatch\n");
        return 1;
    }
    {
        uint8_t buf[128];
        uint32_t n = platform_101ab_fill_path_a(buf, sizeof(buf), 0);
        if (n < 16u || buf[0] != 2) {
            fprintf(stderr, "101AB fill_path_a empty failed n=%u\n", n);
            return 1;
        }
        n = platform_101ab_fill_path_a(buf, sizeof(buf), 1);
        if (n < 30u || buf[0] != 2) {
            fprintf(stderr, "101AB fill_path_a with_record failed n=%u\n", n);
            return 1;
        }
        /* Inner after type+BE(len)+hdr+body_size+code: tag BE32(1) then BE16(11) "downVersion" */
        if (buf[15] != 0 || buf[16] != 0 || buf[17] != 0 || buf[18] != 1) {
            fprintf(stderr, "101AB with_record tag mismatch\n");
            return 1;
        }
        if (buf[19] != 0 || buf[20] != 11) {
            fprintf(stderr, "101AB with_record name_len mismatch\n");
            return 1;
        }
        if (memcmp(buf + 21, "downVersion", 11) != 0) {
            fprintf(stderr, "101AB with_record name mismatch\n");
            return 1;
        }
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10138u;
    call.app = 0x27FF68u;
    call.arg2 = 0x27FF64u;
    call.arg3 = 0x27FF60u;
    call.arg4 = 0x27FF5Cu;
    platform_send_app_event_classify(&call, &out);
    if (!expect_kind("10138", out.kind, GWY_PLAT_KIND_MULTI_OUT) || out.status_ret != 0u) {
        fprintf(stderr, "10138 multi-out mismatch kind=%d ret=%u\n", (int)out.kind, out.status_ret);
        return 1;
    }

    printf("ok\n");
    return 0;
}
