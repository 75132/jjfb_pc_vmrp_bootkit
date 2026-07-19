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
    printf("ok\n");
    return 0;
}
