#include "gwy_launcher/package_metadata.h"

#include "gwy_launcher/error.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/gwy_pack_registry.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/package_scope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GWY_PKG_META_SLOTS 16

static struct {
    GwyPackageMetadata slots[GWY_PKG_META_SLOTS];
    size_t slot_n;
    uint64_t next_package_id;
    int active_idx; /* -1 if none */
    GwyPackageAppInfoBinding binding;
} g_pm;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static uint64_t hash_path(const char *s) {
    uint64_t h = 1469598103934665603ull;
    const unsigned char *p;
    if (!s) return 0;
    for (p = (const unsigned char *)s; *p; p++) {
        h ^= (uint64_t)(*p);
        h *= 1099511628211ull;
    }
    return h ? h : 1ull;
}

const char *gwy_package_metadata_source_name(int source) {
    switch (source) {
    case GWY_PKG_META_MRP_HEADER:
        return "MRP_HEADER";
    case GWY_PKG_META_PACKAGE_REGISTRY:
        return "PACKAGE_REGISTRY";
    case GWY_PKG_META_DIAGNOSTIC_ENV_OVERRIDE:
        return "DIAGNOSTIC_ENV_OVERRIDE";
    default:
        return "NONE";
    }
}

void gwy_package_metadata_reset(void) {
    memset(&g_pm, 0, sizeof(g_pm));
    g_pm.active_idx = -1;
    g_pm.next_package_id = 1;
}

void gwy_package_appinfo_binding_invalidate(void) {
    memset(&g_pm.binding, 0, sizeof(g_pm.binding));
}

void gwy_package_metadata_invalidate_active(void) {
    if (g_pm.active_idx >= 0 && (size_t)g_pm.active_idx < g_pm.slot_n)
        g_pm.slots[g_pm.active_idx].valid = 0;
    g_pm.active_idx = -1;
    gwy_package_appinfo_binding_invalidate();
}

int gwy_package_metadata_resolve_host_path(const char *package_guest_path, char *out,
                                           size_t out_sz) {
    const char *root;
    char norm[260];
    char candidate[1024];
    size_t i, n;
    ModuleRegistry *reg;
    FILE *fp;
    if (!package_guest_path || !package_guest_path[0] || !out || out_sz == 0) return 0;
    out[0] = 0;

    snprintf(norm, sizeof(norm), "%s", package_guest_path);
    for (i = 0; norm[i]; i++) {
        if (norm[i] == '\\') norm[i] = '/';
    }

    /* Primary: GWY_RESOURCE_ROOT + guest-relative path (product path). */
    root = getenv("GWY_RESOURCE_ROOT");
    if (root && root[0]) {
        if (strncmp(norm, "mythroad/", 9) == 0)
            snprintf(candidate, sizeof(candidate), "%s/%s", root, norm + 9);
        else
            snprintf(candidate, sizeof(candidate), "%s/%s", root, norm);
        fp = fopen(candidate, "rb");
        if (fp) {
            fclose(fp);
            snprintf(out, out_sz, "%s", candidate);
            return 1;
        }
    }

    /* Secondary: module registry host paths (must look like a real filesystem path). */
    reg = gwy_ext_loader_bound_registry();
    if (reg) {
        const char *stem = strrchr(norm, '/');
        stem = stem ? stem + 1 : norm;
        if (reg->package_path[0] &&
            (strchr(reg->package_path, ':') || reg->package_path[0] == '/' ||
             reg->package_path[0] == '\\') &&
            strstr(reg->package_path, stem)) {
            fp = fopen(reg->package_path, "rb");
            if (fp) {
                fclose(fp);
                snprintf(out, out_sz, "%s", reg->package_path);
                return 1;
            }
        }
        for (i = 0; i < reg->count; i++) {
            const char *pp = reg->modules[i].package_path;
            if (!pp[0]) continue;
            if (!(strchr(pp, ':') || pp[0] == '/' || pp[0] == '\\')) continue;
            if (!strstr(pp, stem)) continue;
            fp = fopen(pp, "rb");
            if (fp) {
                fclose(fp);
                snprintf(out, out_sz, "%s", pp);
                return 1;
            }
        }
    }

    /* Optional: GwyPackRegistry side-pack scan. */
    if (gwy_pack_registry_ready()) {
        const char *stem = strrchr(norm, '/');
        char stem_no_ext[96];
        stem = stem ? stem + 1 : norm;
        snprintf(stem_no_ext, sizeof(stem_no_ext), "%s", stem);
        n = strlen(stem_no_ext);
        if (n > 4 && strcmp(stem_no_ext + n - 4, ".mrp") == 0) stem_no_ext[n - 4] = 0;
        if (gwy_pack_registry_find_pack_path(stem_no_ext, out, out_sz)) {
            fp = fopen(out, "rb");
            if (fp) {
                fclose(fp);
                return 1;
            }
            out[0] = 0;
        }
    }
    return 0;
}

static int fill_from_mrp(GwyPackageMetadata *m, const char *guest_path, const char *host_path) {
    MrpArchive *arch = NULL;
    LauncherError err;
    memset(&err, 0, sizeof(err));
    if (mrp_archive_open(host_path, &arch, &err) != L_OK || !arch) return 0;
    m->appid = arch->appid_le;
    m->appver = arch->appver_le;
    m->flags = arch->flags;
    m->header_version = arch->header_length;
    snprintf(m->archive_path, sizeof(m->archive_path), "%.259s", host_path);
    snprintf(m->internal_name, sizeof(m->internal_name), "%.127s", arch->internal_name);
    /* Display name: GBK field at MRP +28 (24 bytes typical); keep raw bytes as Latin1-safe. */
    {
        char dn[25];
        memset(dn, 0, sizeof(dn));
        if (arch->data && arch->size >= 52) {
            memcpy(dn, arch->data + 28, 24);
            dn[24] = 0;
        }
        snprintf(m->display_name, sizeof(m->display_name), "%.127s", dn);
    }
    snprintf(m->guest_path, sizeof(m->guest_path), "%.159s", guest_path ? guest_path : "");
    m->archive_id = hash_path(host_path);
    m->source = GWY_PKG_META_MRP_HEADER;
    m->valid = 1;
    mrp_archive_close(arch);
    return 1;
}

static int find_slot_by_guest(const char *guest_path) {
    size_t i;
    if (!guest_path) return -1;
    for (i = 0; i < g_pm.slot_n; i++) {
        if (strcmp(g_pm.slots[i].guest_path, guest_path) == 0) return (int)i;
    }
    return -1;
}

int gwy_package_metadata_activate(const char *package_guest_path) {
    char host[1024];
    int idx;
    GwyPackageMetadata *m;
    const PackageScopeEntry *scope;
    char guest_canon[160];

    if (!package_guest_path || !package_guest_path[0]) return 0;
    scope = package_scope_lookup(package_guest_path);
    if (scope && scope->package_guest_path)
        snprintf(guest_canon, sizeof(guest_canon), "%s", scope->package_guest_path);
    else
        snprintf(guest_canon, sizeof(guest_canon), "%s", package_guest_path);

    if (!gwy_package_metadata_resolve_host_path(guest_canon, host, sizeof(host))) {
        printf("[GWY_PKG_META] activate=FAIL guest=%s reason=host_path evidence=OBSERVED\n",
               guest_canon);
        fflush(stdout);
        /* Fail closed: do not keep a previous package's metadata as "active". */
        if (g_pm.active_idx >= 0 &&
            strcmp(g_pm.slots[g_pm.active_idx].guest_path, guest_canon) != 0) {
            g_pm.active_idx = -1;
            gwy_package_appinfo_binding_invalidate();
        }
        return 0;
    }

    idx = find_slot_by_guest(guest_canon);
    if (idx < 0) {
        if (g_pm.slot_n >= GWY_PKG_META_SLOTS) {
            printf("[GWY_PKG_META] activate=FAIL guest=%s reason=slot_full evidence=OBSERVED\n",
                   guest_canon);
            fflush(stdout);
            return 0;
        }
        idx = (int)g_pm.slot_n++;
        memset(&g_pm.slots[idx], 0, sizeof(g_pm.slots[idx]));
        g_pm.slots[idx].package_id = g_pm.next_package_id++;
    }
    m = &g_pm.slots[idx];

    /* Always re-parse header on activate; bump generation so appInfo cannot be reused. */
    if (!fill_from_mrp(m, guest_canon, host)) {
        printf("[GWY_PKG_META] activate=FAIL guest=%s host=%s reason=mrp_open evidence=OBSERVED\n",
               guest_canon, host);
        fflush(stdout);
        if (g_pm.active_idx >= 0 &&
            strcmp(g_pm.slots[g_pm.active_idx].guest_path, guest_canon) != 0) {
            g_pm.active_idx = -1;
            gwy_package_appinfo_binding_invalidate();
        }
        return 0;
    }
    m->package_generation++;
    if (m->package_id == 0) m->package_id = g_pm.next_package_id++;

    g_pm.active_idx = idx;
    gwy_package_appinfo_binding_invalidate();

    printf("[GWY_PKG_META] activate=OK package=%s package_id=%llu generation=%llu "
           "archive=%s source=MRP_HEADER appid=%u appver=%u evidence=OBSERVED\n",
           m->guest_path, (unsigned long long)m->package_id,
           (unsigned long long)m->package_generation, m->archive_path, m->appid, m->appver);
    fflush(stdout);
    return 1;
}

const GwyPackageMetadata *gwy_package_registry_active_metadata(void) {
    const char *active;
    if (g_pm.active_idx >= 0 && (size_t)g_pm.active_idx < g_pm.slot_n &&
        g_pm.slots[g_pm.active_idx].valid)
        return &g_pm.slots[g_pm.active_idx];

    /* Lazy: activate from package_scope if set. */
    active = package_scope_active_package();
    if (active && gwy_package_metadata_activate(active))
        return &g_pm.slots[g_pm.active_idx];
    return NULL;
}

int gwy_package_appinfo_resolve_id_ver(uint32_t *appid, uint32_t *appver, int *source_out) {
    const GwyPackageMetadata *meta;
    int force_env = env1("JJFB_E10A31E_FORCE_ENV_APPINFO") ||
                    env1("JJFB_E10A31E_DIAGNOSTIC_ENV_OVERRIDE");
    const char *aid = getenv("GWY_PACKAGE_APPID");
    const char *aver = getenv("GWY_PACKAGE_APPVER");

    if (appid) *appid = 0;
    if (appver) *appver = 0;
    if (source_out) *source_out = GWY_PKG_META_NONE;

    if (force_env && aid && aid[0] && aver && aver[0]) {
        uint32_t id = (uint32_t)strtoul(aid, NULL, 10);
        uint32_t ver = (uint32_t)strtoul(aver, NULL, 10);
        if (appid) *appid = id;
        if (appver) *appver = ver;
        if (source_out) *source_out = GWY_PKG_META_DIAGNOSTIC_ENV_OVERRIDE;
        printf("[GWY_APPINFO_BIND] DIAGNOSTIC_OVERRIDE appid=%u appver=%u "
               "source=DIAGNOSTIC_ENV_OVERRIDE evidence=DIAGNOSTIC\n",
               id, ver);
        fflush(stdout);
        return 1;
    }

    meta = gwy_package_registry_active_metadata();
    if (meta && meta->valid) {
        if (appid) *appid = meta->appid;
        if (appver) *appver = meta->appver;
        if (source_out) *source_out = meta->source;
        return 1;
    }
    return 0;
}

const GwyPackageAppInfoBinding *gwy_package_appinfo_binding_active(void) {
    return g_pm.binding.valid ? &g_pm.binding : NULL;
}

void gwy_package_appinfo_binding_set(uint32_t appinfo_guest, uint32_t appid, uint32_t appver,
                                     int source) {
    const GwyPackageMetadata *meta = gwy_package_registry_active_metadata();
    memset(&g_pm.binding, 0, sizeof(g_pm.binding));
    if (meta) {
        g_pm.binding.package_id = meta->package_id;
        g_pm.binding.package_generation = meta->package_generation;
    }
    g_pm.binding.appinfo_guest = appinfo_guest;
    g_pm.binding.appid = appid;
    g_pm.binding.appver = appver;
    g_pm.binding.sidname_guest = 0;
    g_pm.binding.ram = 0;
    g_pm.binding.source = source;
    g_pm.binding.valid = appinfo_guest != 0;
}

int gwy_package_appinfo_binding_matches_active(uint32_t guest_ptr,
                                               int (*guest_mapped)(uint32_t guest_ptr)) {
    const GwyPackageMetadata *meta;
    if (!g_pm.binding.valid || !guest_ptr) return 0;
    if (g_pm.binding.appinfo_guest != guest_ptr) return 0;
    meta = gwy_package_registry_active_metadata();
    if (!meta || !meta->valid) return 0;
    if (g_pm.binding.package_id != meta->package_id) return 0;
    if (g_pm.binding.package_generation != meta->package_generation) return 0;
    if (guest_mapped && !guest_mapped(guest_ptr)) return 0;
    return 1;
}
