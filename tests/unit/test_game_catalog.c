#include "gwy_launcher/game_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *root = getenv("GWY_FIXTURE_ROOT");
    GwyGameCatalog cat;
    LauncherError err;
    LauncherStatus st;
    size_t i;
    int found_jjfb = 0;

    if (!root || !root[0]) {
        fprintf(stderr, "GWY_FIXTURE_ROOT not set\n");
        return 1;
    }
    st = gwy_game_catalog_scan(root, &cat, &err);
    if (st != L_OK) {
        fprintf(stderr, "scan failed: %s\n", err.message);
        return 1;
    }
    if (cat.count < 5) {
        fprintf(stderr, "expected several games, got %zu\n", cat.count);
        return 1;
    }
    for (i = 0; i < cat.count; i++) {
        if (strcmp(cat.entries[i].target_mrp, "gwy/jjfb.mrp") == 0) {
            found_jjfb = 1;
            if (cat.entries[i].cfg_index != 36) {
                fprintf(stderr, "jjfb preferred cfg_index=%u\n", cat.entries[i].cfg_index);
                return 1;
            }
        }
    }
    if (!found_jjfb) {
        fprintf(stderr, "jjfb missing from catalog\n");
        return 1;
    }
    printf("test_game_catalog OK count=%zu\n", cat.count);
    return 0;
}
