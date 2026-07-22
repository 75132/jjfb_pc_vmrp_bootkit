#include "gwy_launcher/package_metadata.h"
#include "gwy_launcher/package_scope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
static void set_env(const char *kv) { _putenv(kv); }
#else
static void set_env_pair(const char *k, const char *v) { setenv(k, v, 1); }
#endif

int main(void) {
    char root[512];
    const char *cwd;
    uint32_t id = 0, ver = 0;
    int src = 0;
    const GwyPackageMetadata *m;
    const GwyPackageAppInfoBinding *b;

#ifdef _WIN32
    set_env("JJFB_PACKAGE_SCOPED_CLOAD=1");
    set_env("JJFB_E10A31E_FORCE_ENV_APPINFO=");
    set_env("JJFB_E10A31E_DIAGNOSTIC_ENV_OVERRIDE=");
#else
    set_env_pair("JJFB_PACKAGE_SCOPED_CLOAD", "1");
    unsetenv("JJFB_E10A31E_FORCE_ENV_APPINFO");
    unsetenv("JJFB_E10A31E_DIAGNOSTIC_ENV_OVERRIDE");
#endif

    package_scope_reset();
    gwy_package_metadata_reset();

    cwd = getenv("GWY_TEST_RESOURCE_ROOT");
    if (!cwd || !cwd[0]) {
        /* Default: relative to build or repo game_files */
        snprintf(root, sizeof(root), "%s",
                 "game_files/mythroad/320x480");
#ifdef _WIN32
        {
            char buf[640];
            snprintf(buf, sizeof(buf), "GWY_RESOURCE_ROOT=%s", root);
            set_env(buf);
        }
#else
        set_env_pair("GWY_RESOURCE_ROOT", root);
#endif
    } else {
#ifdef _WIN32
        char buf[640];
        snprintf(buf, sizeof(buf), "GWY_RESOURCE_ROOT=%s", cwd);
        set_env(buf);
#else
        set_env_pair("GWY_RESOURCE_ROOT", cwd);
#endif
    }

    if (!package_scope_set_active("gwy/gamelist.mrp")) {
        printf("FAIL scope activate\n");
        return 1;
    }
    m = gwy_package_registry_active_metadata();
    if (!m || !m->valid) {
        printf("FAIL metadata missing (resource root may be wrong)\n");
        return 2;
    }
    if (m->appid == 0 || m->appver == 0) {
        printf("FAIL zero id/ver\n");
        return 3;
    }
    if (m->source != GWY_PKG_META_MRP_HEADER) {
        printf("FAIL source != MRP_HEADER\n");
        return 4;
    }
    if (!gwy_package_appinfo_resolve_id_ver(&id, &ver, &src)) {
        printf("FAIL resolve\n");
        return 5;
    }
    if (id != m->appid || ver != m->appver || src != GWY_PKG_META_MRP_HEADER) {
        printf("FAIL resolve mismatch id=%u ver=%u src=%d\n", id, ver, src);
        return 6;
    }

    gwy_package_appinfo_binding_set(0x1000u, id, ver, src);
    b = gwy_package_appinfo_binding_active();
    if (!b || !b->valid || b->package_id != m->package_id ||
        b->package_generation != m->package_generation) {
        printf("FAIL binding key\n");
        return 7;
    }

    /* Switch package → binding must invalidate */
    if (!package_scope_set_active("gwy/jjfb.mrp")) {
        printf("FAIL jjfb scope\n");
        return 8;
    }
    b = gwy_package_appinfo_binding_active();
    if (b && b->valid) {
        printf("FAIL binding not invalidated on package switch\n");
        return 9;
    }
    m = gwy_package_registry_active_metadata();
    if (!m || !m->valid || strstr(m->guest_path, "jjfb") == NULL) {
        printf("FAIL jjfb metadata\n");
        return 10;
    }

    printf("test_package_metadata OK appid=%u appver=%u\n", id, ver);
    return 0;
}
