#include "gwy_launcher/platform_mrp_resource.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Task 12 §14 regression sample from E9E natural postmatch. */
static const char *k_wy_jiao1_sha =
    "edfe428dfb2daa8deea599915b7c5d4db75b6bfbfe78671cecd33e4ca4662a13";

static const char *fixture_jjfb(void) {
    const char *root = getenv("GWY_FIXTURE_ROOT");
    static char path[1024];
    if (!root || !root[0]) {
        fprintf(stderr, "GWY_FIXTURE_ROOT not set\n");
        return NULL;
    }
    snprintf(path, sizeof(path), "%s/gwy/jjfb.mrp", root);
    return path;
}

int main(void) {
    const char *path;
    GwyMrpResourceResult res;
    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    int ok;

    path = fixture_jjfb();
    if (!path) return 1;

    ok = platform_mrp_resource_load_host(path, "wy_jiao1!11!11.bmp", &res, &decoded, &decoded_len);
    if (!ok) {
        fprintf(stderr, "load_host failed for wy_jiao1!11!11.bmp path=%s\n", path);
        return 1;
    }
    if (decoded_len != 242u) {
        fprintf(stderr, "decoded_len=%zu expect=242\n", decoded_len);
        free(decoded);
        return 1;
    }
    if (res.width != 11 || res.height != 11) {
        fprintf(stderr, "wh=%u x %u expect 11x11\n", res.width, res.height);
        free(decoded);
        return 1;
    }
    if (strcmp(res.sha256_hex, k_wy_jiao1_sha) != 0) {
        fprintf(stderr, "sha mismatch got=%s expect=%s\n", res.sha256_hex, k_wy_jiao1_sha);
        free(decoded);
        return 1;
    }
    if (res.member_offset != 8414u || res.stored_size != 90u) {
        fprintf(stderr, "offset/stored=%u/%u expect 8414/90\n", res.member_offset, res.stored_size);
        free(decoded);
        return 1;
    }

    printf("platform_mrp_resource ok name=wy_jiao1!11!11.bmp decoded=%u sha256=%s\n",
           res.decoded_size, res.sha256_hex);
    free(decoded);
    return 0;
}
