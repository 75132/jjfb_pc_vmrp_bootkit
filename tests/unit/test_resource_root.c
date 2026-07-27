#include "gwy_launcher/resource_root.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *repo = getenv("GWY_REPO_ROOT");
    GwyResourceRootRequest req;
    GwyResourceRootResult out;
    LauncherError err;
    LauncherStatus st;
    char bad[1024];

    if (!repo || !repo[0]) {
        fprintf(stderr, "GWY_REPO_ROOT required\n");
        return 1;
    }

    memset(&req, 0, sizeof(req));
    req.repo_root = repo;
    req.cfg_index = 36;
    st = gwy_resource_root_resolve(&req, &out, &err);
    if (st != L_OK) {
        fprintf(stderr, "default resolve fail: %s %s\n", err.message, err.detail);
        return 1;
    }
    if (strcmp(out.reason, "default_240x320") != 0 && strcmp(out.reason, "scan") != 0) {
        fprintf(stderr, "expected default_240x320 or scan, got %s path=%s\n", out.reason,
                out.path);
        return 1;
    }
    if (!strstr(out.path, "240x320") && strcmp(out.reason, "scan") == 0) {
        /* scan may pick another valid tree; still OK if validate passed */
    } else if (!strstr(out.path, "240x320") && strcmp(out.reason, "default_240x320") == 0) {
        fprintf(stderr, "default_240x320 path missing 240x320: %s\n", out.path);
        return 1;
    }

    snprintf(bad, sizeof(bad), "%s/game_files/mythroad/.tmp_missing_root_xyz", repo);
    memset(&req, 0, sizeof(req));
    req.explicit_root = bad;
    req.repo_root = repo;
    req.cfg_index = 36;
    st = gwy_resource_root_resolve(&req, &out, &err);
    if (st == L_OK) {
        fprintf(stderr, "explicit invalid should FAIL, got path=%s\n", out.path);
        return 1;
    }

    printf("test_resource_root OK\n");
    return 0;
}
