#include "gwy_launcher/launch_descriptor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *root = getenv("GWY_FIXTURE_ROOT");
    LaunchDescriptor d;
    LaunchExpectations ex;
    LauncherError err;
    LauncherStatus st;
    const char *golden =
        "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";

    if (!root || !root[0]) {
        fprintf(stderr, "GWY_FIXTURE_ROOT not set\n");
        return 1;
    }

    memset(&ex, 0, sizeof(ex));
    ex.has_target = 1;
    snprintf(ex.target_mrp, sizeof(ex.target_mrp), "%s", "gwy/jjfb.mrp");
    ex.has_sha256 = 1;
    snprintf(ex.sha256_hex, sizeof(ex.sha256_hex), "%s",
             "52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036");
    ex.has_appid = 1;
    ex.appid = 400101;
    ex.has_appver = 1;
    ex.appver = 12;

    st = launch_descriptor_build(root, 36, "gwy.jjfb.original", &ex, &d, &err);
    if (st != L_OK) {
        fprintf(stderr, "build failed: %s %s\n", err.message, err.detail);
        return 1;
    }
    if (strcmp(d.param, golden) != 0) {
        fprintf(stderr, "param mismatch:\n  got %s\n  exp %s\n", d.param, golden);
        return 1;
    }

    /* mismatch must fail closed */
    ex.appver = 999;
    st = launch_descriptor_build(root, 36, "gwy.jjfb.original", &ex, &d, &err);
    if (st != L_ERR_PROFILE_MISMATCH) {
        fprintf(stderr, "expected appver mismatch\n");
        return 1;
    }

    st = launch_profile_reject_dangerous("{\"id\":\"x\",\"guest_address\":123}", &err);
    if (st != L_ERR_PROFILE_MISMATCH) {
        fprintf(stderr, "dangerous profile not rejected\n");
        return 1;
    }
    st = launch_profile_reject_dangerous("{\"id\":\"gwy.jjfb.original\"}", &err);
    if (st != L_OK) {
        fprintf(stderr, "clean profile rejected\n");
        return 1;
    }

    puts("test_launch_descriptor OK");
    return 0;
}
