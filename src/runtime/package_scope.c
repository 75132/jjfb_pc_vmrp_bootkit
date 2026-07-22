#include "gwy_launcher/package_scope.h"
#include "gwy_launcher/package_metadata.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int name_eq_ci(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static struct {
    int enabled_known;
    int enabled;
    char active_pkg[160];
    char active_primary[64];
} g_ps;

static const PackageScopeEntry k_table[] = {
    {"gwy/gbrwcore.mrp", "gbrwcore.ext", "shell_core"},
    {"gwy/gamelist.mrp", "gamelist.ext", "shell_core"},
    {"gwy/gbrwshell.mrp", "gbrwshell.ext", "shell_core"},
    {"gwy/jjfb.mrp", "robotol.ext", "mrc_loader_game"},
    {"gwy/wxjwq.mrp", "mmochat.ext", "mrc_loader_game"},
};

static int env_truthy(const char *v) {
    return v && v[0] && (v[0] == '1' || v[0] == 'y' || v[0] == 'Y' || v[0] == 't' ||
                         v[0] == 'T');
}

static void ensure_enabled(void) {
    const char *e;
    if (g_ps.enabled_known) return;
    e = getenv("JJFB_PACKAGE_SCOPED_CLOAD");
    if (env_truthy(e))
        g_ps.enabled = 1;
    else if (env_truthy(getenv("JJFB_NATIVE_BOOT_FULL")))
        g_ps.enabled = 1;
    else
        g_ps.enabled = 0;
    g_ps.enabled_known = 1;
}

static int path_has(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}

static const PackageScopeEntry *lookup_loose(const char *pkg) {
    size_t i;
    if (!pkg || !pkg[0]) return NULL;
    for (i = 0; i < sizeof(k_table) / sizeof(k_table[0]); i++) {
        if (strcmp(pkg, k_table[i].package_guest_path) == 0) return &k_table[i];
    }
    for (i = 0; i < sizeof(k_table) / sizeof(k_table[0]); i++) {
        const char *stem = strrchr(k_table[i].package_guest_path, '/');
        stem = stem ? stem + 1 : k_table[i].package_guest_path;
        if (path_has(pkg, stem)) return &k_table[i];
    }
    return NULL;
}

void package_scope_reset(void) {
    memset(&g_ps, 0, sizeof(g_ps));
    gwy_package_metadata_reset();
}

int package_scope_enabled(void) {
    ensure_enabled();
    return g_ps.enabled;
}

const PackageScopeEntry *package_scope_lookup(const char *package_guest_path) {
    return lookup_loose(package_guest_path);
}

int package_scope_set_active(const char *package_guest_path) {
    const PackageScopeEntry *e;
    ensure_enabled();
    if (!g_ps.enabled) return 0;
    e = lookup_loose(package_guest_path);
    if (!e) {
        printf("[JJFB_PACKAGE_SCOPE] package=%s primary=(none) result=MISS "
               "evidence=TARGET_OBSERVED\n",
               package_guest_path ? package_guest_path : "?");
        fflush(stdout);
        return 0;
    }
    snprintf(g_ps.active_pkg, sizeof(g_ps.active_pkg), "%s", e->package_guest_path);
    snprintf(g_ps.active_primary, sizeof(g_ps.active_primary), "%s", e->primary_ext);
    printf("[JJFB_PACKAGE_SCOPE] package=%s primary=%s class=%s "
           "cload=scoped evidence=CROSS_TARGET\n",
           g_ps.active_pkg, g_ps.active_primary, e->class_name);
    fflush(stdout);
    /* E10A-3.1e: bind MRP header metadata to this package generation. */
    (void)gwy_package_metadata_activate(g_ps.active_pkg);
    return 1;
}

const char *package_scope_active_package(void) {
    ensure_enabled();
    return g_ps.active_pkg[0] ? g_ps.active_pkg : NULL;
}

const char *package_scope_active_primary(void) {
    ensure_enabled();
    return g_ps.active_primary[0] ? g_ps.active_primary : NULL;
}

int package_scope_resolve_cfunction(const char *requested_member, char *out_primary,
                                    size_t out_cap) {
    ensure_enabled();
    if (!g_ps.enabled || !out_primary || out_cap == 0) return 0;
    if (!requested_member) return 0;
    if (strcmp(requested_member, "cfunction.ext") != 0 &&
        strcmp(requested_member, "Cfunction.ext") != 0) {
        return 0;
    }
    if (!g_ps.active_primary[0]) return 0;
    snprintf(out_primary, out_cap, "%s", g_ps.active_primary);
    return 1;
}

int package_scope_is_active_primary(const char *module_name) {
    const char *base;
    ensure_enabled();
    if (!g_ps.enabled || !module_name || !g_ps.active_primary[0]) return 0;
    base = strrchr(module_name, '/');
    base = base ? base + 1 : module_name;
    if (base[0] == '\0') return 0;
    return name_eq_ci(base, g_ps.active_primary);
}
