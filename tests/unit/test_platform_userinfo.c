#include "gwy_launcher/platform_send_app_event.h"
#include "gwy_launcher/platform_userinfo.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    PlatformIdentity id;
    PlatformUserInfoBlob blob;
    LauncherError err;
    GwyPlatCall call;
    GwyPlatCallResult result;
    uint32_t ver = 0;

    platform_identity_set_defaults(&id);
    if (platform_userinfo_fill(&id, GWY_DEFAULT_IMSI, &blob, &err) != L_OK) {
        fprintf(stderr, "fill failed: %s\n", err.message);
        return 1;
    }
    if (blob.filled != GWY_MR_USERINFO_BYTES) {
        fprintf(stderr, "filled=%zu expected %u\n", blob.filled, GWY_MR_USERINFO_BYTES);
        return 1;
    }
    if (memcmp(blob.bytes + 0x00, "864086040622841", 15) != 0) {
        fprintf(stderr, "IMEI mismatch\n");
        return 1;
    }
    if (memcmp(blob.bytes + 0x10, GWY_DEFAULT_IMSI, 15) != 0) {
        fprintf(stderr, "IMSI mismatch\n");
        return 1;
    }
    if (memcmp(blob.bytes + 0x20, "vmrp", 4) != 0) {
        fprintf(stderr, "manufactory mismatch at +0x20\n");
        return 1;
    }
    if (memcmp(blob.bytes + 0x28, "vmrp", 4) != 0) {
        fprintf(stderr, "type mismatch\n");
        return 1;
    }
    memcpy(&ver, blob.bytes + 0x30, 4);
    if (ver != GWY_DEFAULT_USERINFO_VER) {
        fprintf(stderr, "ver=0x%X expected 0x%X\n", ver, GWY_DEFAULT_USERINFO_VER);
        return 1;
    }
    /* Tail beyond DOCUMENTED size must stay zero (unconfirmed fields). */
    {
        size_t i;
        for (i = GWY_MR_USERINFO_BYTES; i < GWY_USERINFO_BLOB_BYTES; i++) {
            if (blob.bytes[i] != 0) {
                fprintf(stderr, "non-zero pad at +0x%zX\n", i);
                return 1;
            }
        }
    }

    memset(&call, 0, sizeof(call));
    call.code = 0x10180u;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_USERINFO_BLOB) {
        fprintf(stderr, "classify kind=%d\n", (int)result.kind);
        return 1;
    }
    if (memcmp(result.userinfo.bytes, blob.bytes, GWY_MR_USERINFO_BYTES) != 0) {
        fprintf(stderr, "classify blob mismatch\n");
        return 1;
    }

    call.code = 0x1u;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_STATUS || result.status_ret != 0) {
        fprintf(stderr, "event code should default status 0\n");
        return 1;
    }

    call.code = 0x1010Cu;
    call.app = 0x100u;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_ALLOC || result.alloc_size != 0x100u) {
        fprintf(stderr, "0x1010C should request sized alloc\n");
        return 1;
    }
    call.app = 0xE7D0u;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_ALLOC || result.alloc_size != 0xE7D0u) {
        fprintf(stderr, "0x1010C large size should request alloc\n");
        return 1;
    }

    call.code = 0x10119u;
    call.app = 0x1u;
    call.arg2 = 0x9Cu;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_ALLOC || result.alloc_size != 0x9Cu) {
        fprintf(stderr, "0x10119 should alloc R2 size\n");
        return 1;
    }

    call.code = 0x10183u;
    call.app = 0x600u;
    call.arg2 = 0;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_ALLOC || result.alloc_size != 0x600u) {
        fprintf(stderr, "0x10183 should alloc R1 size\n");
        return 1;
    }

    /* DOCUMENTED classic: (0, chunk, 0, id, period@SP). */
    memset(&call, 0, sizeof(call));
    call.known_chunk = 0x682A5Cu;
    call.code = 0;
    call.app = 0x682A5Cu;
    call.arg2 = 0;
    call.arg3 = 1;
    call.arg4 = 0x2710u;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_TIMER_START || result.timer_period_ms != 0x2710u) {
        fprintf(stderr, "classic timer start failed kind=%d period=%u\n", (int)result.kind,
                result.timer_period_ms);
        return 1;
    }
    call.arg2 = 1u;
    call.arg4 = 0;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_TIMER_STOP) {
        fprintf(stderr, "classic timer stop failed kind=%d\n", (int)result.kind);
        return 1;
    }

    /* TARGET_OBSERVED gamelist: (chunk, 0, _, period) / (chunk, 1, _, _). */
    memset(&call, 0, sizeof(call));
    call.known_chunk = 0x682A5Cu;
    call.code = 0x682A5Cu;
    call.app = 0;
    call.arg3 = 0x2710u;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_TIMER_START || result.timer_period_ms != 0x2710u) {
        fprintf(stderr, "chunk-r0 timer start failed kind=%d period=%u\n", (int)result.kind,
                result.timer_period_ms);
        return 1;
    }
    call.app = 1u;
    call.arg3 = 0;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_TIMER_STOP) {
        fprintf(stderr, "chunk-r0 timer stop failed kind=%d\n", (int)result.kind);
        return 1;
    }

    /* TARGET_OBSERVED: 0x10204 get tagged buf; 0x10500 status 0. */
    memset(&call, 0, sizeof(call));
    call.code = 0x10204u;
    call.app = 4u;
    call.arg2 = 0xFFFFFFFFu;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_ALLOC || result.alloc_size != 0x40u ||
        result.alloc_u16_at0 != 1u) {
        fprintf(stderr, "0x10204 get should alloc tagged kind=%d sz=%u tag=%u\n",
                (int)result.kind, result.alloc_size, (unsigned)result.alloc_u16_at0);
        return 1;
    }
    call.app = 0xFu;
    call.arg2 = 0x12340000u;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_STATUS || result.status_ret != 0) {
        fprintf(stderr, "0x10204 mode 0xF should status 0\n");
        return 1;
    }
    call.code = 0x10500u;
    call.app = 2u;
    call.arg2 = 0x105u;
    platform_send_app_event_classify(&call, &result);
    if (result.kind != GWY_PLAT_KIND_STATUS || result.status_ret != 0) {
        fprintf(stderr, "0x10500 should status 0\n");
        return 1;
    }

    printf("layout=mr_userinfo bytes=%u blob=%u ver=%u\n", GWY_MR_USERINFO_BYTES,
           GWY_USERINFO_BLOB_BYTES, GWY_DEFAULT_USERINFO_VER);
    printf("[OK] platform_userinfo\n");
    return 0;
}
