#include "gwy_launcher/platform_handler_registry.h"
#include <stdio.h>

int main(void) {
    platform_handler_registry_reset();
    if (platform_handler_registry_has(0x10140u)) {
        fprintf(stderr, "empty should miss\n");
        return 1;
    }
    if (!platform_handler_registry_register(0x10140u, 5u, 0x30630Du)) {
        fprintf(stderr, "register 10140 failed\n");
        return 1;
    }
    if (platform_handler_registry_get(0x10140u) != 0x30630Du ||
        platform_handler_registry_family(0x10140u) != 5u) {
        fprintf(stderr, "10140 get mismatch\n");
        return 1;
    }
    if (!platform_handler_registry_register(0x10165u, 0xE200u, 0x30D2F9u)) {
        fprintf(stderr, "register 10165 failed\n");
        return 1;
    }
    if (platform_handler_registry_get(0x10165u) != 0x30D2F9u) {
        fprintf(stderr, "10165 get mismatch\n");
        return 1;
    }
    /* update same code */
    if (!platform_handler_registry_register(0x10140u, 5u, 0x306311u) ||
        platform_handler_registry_get(0x10140u) != 0x306311u) {
        fprintf(stderr, "update failed\n");
        return 1;
    }
    if (platform_handler_registry_register(0x10140u, 0u, 0u)) {
        fprintf(stderr, "bad handler should fail\n");
        return 1;
    }
    /* Product-owned family band: 0x1E200 covers 0x1E209. */
    if (!platform_handler_registry_register_owned(0x10102u, 0x1E200u, 0x30D301u, 3u, 3u,
                                                   "robotol.ext", GWY_HANDLER_ISA_THUMB,
                                                   "plat_10102_register")) {
        fprintf(stderr, "owned 10102 register failed\n");
        return 1;
    }
    if (!platform_handler_registry_find_family_event(0x1E209u) ||
        platform_handler_registry_find_family_event(0x1E209u)->handler != 0x30D301u) {
        fprintf(stderr, "family event lookup failed\n");
        return 1;
    }
    if (platform_handler_registry_find_family_event(0x2E209u)) {
        fprintf(stderr, "unrelated band should miss\n");
        return 1;
    }
    if (!platform_handler_registry_register_owned(0x10165u, 0xE200u, 0x30D2F9u, 3u, 3u,
                                                   "robotol.ext", GWY_HANDLER_ISA_THUMB,
                                                   "plat_10165_alloc")) {
        fprintf(stderr, "owned 10165 register failed\n");
        return 1;
    }
    if (!platform_handler_registry_enqueue_handler() ||
        platform_handler_registry_enqueue_handler()->handler != 0x30D2F9u) {
        fprintf(stderr, "enqueue handler lookup failed\n");
        return 1;
    }
    printf("ok\n");
    return 0;
}
