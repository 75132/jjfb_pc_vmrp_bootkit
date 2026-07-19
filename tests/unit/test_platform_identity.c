#include "gwy_launcher/platform_identity.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define GOLDEN_SHA256 "5d87a42f3d47ac8ddaf892f08409373b18936af761c6b9c8331750dbad3cc436"
#define GOLDEN_KEY_HEX \
    "d6aaa1b23878829303d1b9bcca42e183" \
    "9f9b63153641f4c4e9743434427125b2" \
    "9b31f52a08537f4bdd1b71ab70686d35"

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex48(const char *hex, uint8_t out[GWY_SDK_KEY_SIZE]) {
    size_t i;
    if (strlen(hex) != GWY_SDK_KEY_SIZE * 2) return 0;
    for (i = 0; i < GWY_SDK_KEY_SIZE; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

int main(void) {
    PlatformIdentity id;
    SdkKey key;
    LauncherError err;
    uint8_t expect[GWY_SDK_KEY_SIZE];

    platform_identity_set_defaults(&id);
    if (platform_identity_validate(&id, &err) != L_OK) {
        fprintf(stderr, "validate failed: %s\n", err.message);
        return 1;
    }
    if (platform_sdk_key_generate(&id, &key, &err) != L_OK) {
        fprintf(stderr, "generate failed: %s\n", err.message);
        return 1;
    }
    if (strcmp(key.sha256_hex, GOLDEN_SHA256) != 0) {
        fprintf(stderr, "sha256 mismatch\n  got %s\n  exp %s\n", key.sha256_hex, GOLDEN_SHA256);
        return 1;
    }
    if (!parse_hex48(GOLDEN_KEY_HEX, expect) ||
        memcmp(key.bytes, expect, GWY_SDK_KEY_SIZE) != 0) {
        fprintf(stderr, "key bytes mismatch\n");
        return 1;
    }

    /* Reject bad IMEI. */
    snprintf(id.imei, sizeof(id.imei), "%s", "123");
    if (platform_sdk_key_generate(&id, &key, &err) == L_OK) {
        fprintf(stderr, "expected reject short IMEI\n");
        return 1;
    }

    printf("sdk_key_len=%d\n", GWY_SDK_KEY_SIZE);
    printf("sdk_key_sha256=%s\n", GOLDEN_SHA256);
    printf("identity=vmver=%s imei=%s hsman=%s hstype=%s\n",
           "1968", "864086040622841", "vmrp", "vmrp");
    printf("[OK] platform_identity golden\n");
    return 0;
}
