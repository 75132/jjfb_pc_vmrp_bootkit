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
    char primary[64];

#ifdef _WIN32
    set_env("JJFB_PACKAGE_SCOPED_CLOAD=");
    set_env("JJFB_NATIVE_BOOT_FULL=");
#else
    unsetenv("JJFB_PACKAGE_SCOPED_CLOAD");
    unsetenv("JJFB_NATIVE_BOOT_FULL");
#endif
    package_scope_reset();
    if (package_scope_enabled()) return 1;

#ifdef _WIN32
    set_env("JJFB_PACKAGE_SCOPED_CLOAD=1");
#else
    set_env_pair("JJFB_PACKAGE_SCOPED_CLOAD", "1");
#endif
    package_scope_reset();
    if (!package_scope_enabled()) return 2;

    if (!package_scope_set_active("gwy/gamelist.mrp")) return 3;
    if (!package_scope_active_package() ||
        strcmp(package_scope_active_package(), "gwy/gamelist.mrp") != 0)
        return 4;
    if (!package_scope_active_primary() ||
        strcmp(package_scope_active_primary(), "gamelist.ext") != 0)
        return 5;
    if (!package_scope_is_active_primary("gamelist.ext")) return 6;
    if (!package_scope_resolve_cfunction("cfunction.ext", primary, sizeof(primary))) return 7;
    if (strcmp(primary, "gamelist.ext") != 0) return 8;

    if (!package_scope_set_active("mythroad/gwy/jjfb.mrp")) return 9;
    if (strcmp(package_scope_active_primary(), "robotol.ext") != 0) return 10;

    if (!package_scope_lookup("gwy/wxjwq.mrp") ||
        strcmp(package_scope_lookup("gwy/wxjwq.mrp")->primary_ext, "mmochat.ext") != 0)
        return 11;

    printf("test_package_scope OK\n");
    return 0;
}
