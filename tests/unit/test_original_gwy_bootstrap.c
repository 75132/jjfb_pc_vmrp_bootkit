#include "gwy_launcher/mrp_runtime_stack.h"
#include "gwy_launcher/original_gwy_bootstrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static int fail(const char *m) {
    fprintf(stderr, "FAIL: %s\n", m);
    return 1;
}

int main(void) {
    const char *root = getenv("GWY_FIXTURE_ROOT");
    OriginalGwyBootstrapCatalog cat;
    OriginalGwyApiEntry e;
    MrpRuntimeStack st;
    char path[512];
    static char mode_env[] = "JJFB_BOOTSTRAP_MODE=original_headless";

    if (!root || !root[0]) return fail("GWY_FIXTURE_ROOT not set");

    putenv(mode_env);
    original_gwy_bootstrap_reset();
    if (original_gwy_bootstrap_mode() != JJFB_BOOTSTRAP_ORIGINAL_HEADLESS)
        return fail("mode not original_headless");
    if (!original_gwy_bootstrap_enabled()) return fail("enabled");
    if (strcmp(original_gwy_bootstrap_launch_target(), "gwy/gbrwcore.mrp") != 0)
        return fail("launch target");
    if (!strstr(original_gwy_cfg36_param(), "nmrpname=gwy/jjfb.mrp")) return fail("cfg36");

    if (!original_gwy_bootstrap_catalog_build(root, &cat)) return fail("catalog build");
    if (!cat.complete) return fail("catalog incomplete");
    if (cat.pkg_count < 6) return fail("pkg_count");

#ifdef _WIN32
    _mkdir("reports");
#else
    mkdir("reports", 0755);
#endif
    snprintf(path, sizeof(path), "%s", "reports/ORIGINAL_GWY_BOOTSTRAP_CATALOG_TEST.json");
    if (original_gwy_bootstrap_catalog_write_json(&cat, path) != 0) return fail("catalog json");

    original_gwy_api_reset();
    if (!original_gwy_api_register("lib.startGame", 0x30D001u, 0, 0x3223D4u, 0, "gbrwcore.ext",
                                  0x1000u, 1, "entry"))
        return fail("api register");
    if (!original_gwy_api_lookup("lib.startGame", &e)) return fail("api lookup");
    if (e.function_pointer != 0x30D001u) return fail("api entry pc");
    if (original_gwy_api_entry_pc("lib.startGame") != 0x30D001u) return fail("entry_pc");
    if (original_gwy_api_write_csv("reports/ORIGINAL_GWY_API_MAP_TEST.csv") != 0)
        return fail("api csv");

    mrp_runtime_stack_reset(&st);
    if (!mrp_runtime_stack_push(&st, "gwy/gbrwcore.mrp", "gbrwcore.ext", 0x1111u, 0, 1))
        return fail("stack push parent");
    if (!mrp_runtime_stack_push(&st, "gwy/jjfb.mrp", "robotol.ext", 0x2222u, 0, 2))
        return fail("stack push child");
    if (!mrp_runtime_stack_note_nested_jjfb(&st, "gwy/jjfb.mrp", original_gwy_cfg36_param()))
        return fail("nested");
    if (mrp_runtime_stack_write_json(&st, "reports/ORIGINAL_GWY_RUNTIME_STACK_TEST.json") != 0)
        return fail("stack json");

    if (!original_gwy_bootstrap_should_block_update_pkg("gwy/vdload.mrp"))
        return fail("block vdload");
    if (original_gwy_bootstrap_should_block_update_pkg("gwy/jjfb.mrp")) return fail("allow jjfb");

    printf("PASS original_gwy_bootstrap catalog+api+stack\n");
    return 0;
}
