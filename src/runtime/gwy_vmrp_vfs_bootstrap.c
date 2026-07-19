/*
 * PATCH 0003/0005/0006: bootstrap GuestVFS + VmFileService + member view + ExtLoader obs.
 */
#include "gwy_launcher/compat_profile.h"
#include "gwy_launcher/ext_gwy_shell_shim.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_vfs.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_member_view.h"
#include "gwy_launcher/package_scope.h"
#include "gwy_launcher/sha256.h"
#include "gwy_launcher/vm_file_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static GuestVfs g_vfs;
static VmFileService *g_svc = NULL;
static ModuleRegistry g_modules;
static CompatibilityProfile g_profile;
static int g_profile_loaded = 0;

static void join_path(char *out, size_t cap, const char *a, const char *b) {
    size_t n = strlen(a);
    if (n && (a[n - 1] == '/' || a[n - 1] == '\\'))
        snprintf(out, cap, "%s%s", a, b);
    else
        snprintf(out, cap, "%s/%s", a, b);
}

int gwy_vmrp_prepare_guest_vfs(void) {
    LauncherError err;
    const char *root = getenv("GWY_RESOURCE_ROOT");
    const char *overlay = getenv("GWY_OVERLAY_ROOT");
    const char *profile_path = getenv("GWY_PROFILE");
    char overlay_buf[1024];
    char host_mrp[1024];
    char view_path[1024];
    VfsResolution res;
    ExtLoader *loader;

    if (!root || !root[0]) {
        root = "mythroad";
    }
    if (!overlay || !overlay[0]) {
        snprintf(overlay_buf, sizeof(overlay_buf), "%s", "overlay");
        overlay = overlay_buf;
    }
    if (!profile_path || !profile_path[0]) {
        profile_path = "profiles/jjfb.json";
    }

    if (g_svc) {
        gwy_vm_file_unbind();
        vm_file_service_destroy(g_svc);
        g_svc = NULL;
    }
    g_profile_loaded = 0;
    memset(&g_profile, 0, sizeof(g_profile));
    module_registry_init(&g_modules, "", "");

    loader = gwy_ext_loader_ensure();
    /* Clear prior sessions; keep early DSM stash until flush_early. */
    ext_loader_destroy_all(loader);
    gwy_ext_loader_set_global(loader);

    if (gwy_vm_file_bootstrap(root, overlay, &g_vfs, &g_svc, &err) != L_OK) {
        fprintf(stderr, "[JJFB_VFS] bootstrap failed: %s %s\n", err.message, err.detail);
        return -1;
    }
    printf("[JJFB_VFS] GuestVFS bound resource_root=%s overlay_root=%s\n", root, overlay);
    printf("[JJFB_VFS] gwy_vm_file bound=1\n");
    fprintf(stderr, "[JJFB_VFS] GuestVFS bound resource_root=%s overlay_root=%s\n", root, overlay);
    fprintf(stderr, "[JJFB_VFS] gwy_vm_file bound=1\n");

    if (ext_gwy_shell_shim_enabled()) {
        char gwy_root[1024];
        join_path(gwy_root, sizeof(gwy_root), root, "gwy");
        ext_gwy_shell_shim_reset();
        ext_gwy_shell_shim_emit_banner(root, gwy_root);
    }

    /* Phase 5A: scoped MRP member view (does not touch DSM host cfunction.ext). */
    if (compatibility_profile_load_json_file(profile_path, &g_profile, &err) != L_OK) {
        fprintf(stderr, "[EXT_RESOLVE] profile load failed: %s path=%s\n", err.message, profile_path);
    } else {
        g_profile_loaded = 1;
        if (g_profile.target_mrp[0] && compatibility_profile_has_member_aliases(&g_profile)) {
            if (guest_vfs_resolve(&g_vfs, g_profile.target_mrp, VFS_OPEN_READ, &res, &err) != L_OK ||
                !res.exists) {
                fprintf(stderr, "[EXT_RESOLVE] target resolve failed: %s\n", g_profile.target_mrp);
            } else {
                snprintf(host_mrp, sizeof(host_mrp), "%s", res.host_path);
                join_path(view_path, sizeof(view_path), overlay, "mrp_member_view/jjfb_alias.mrp");
                if (mrp_member_view_install(&g_vfs, &g_profile, host_mrp, g_profile.target_mrp,
                                            view_path, &g_modules, &err) != L_OK) {
                    fprintf(stderr, "[MRP_MEMBER_VIEW] install failed: %s %s\n", err.message,
                            err.detail);
                } else {
                    module_registry_set_provenance(&g_modules, g_profile.target_mrp,
                                                   "generated_member_view", "scoped_member_alias");
                    printf("[MRP_MEMBER_VIEW] ready target=%s aliases=%zu\n", g_profile.target_mrp,
                           g_profile.alias_count);
                }
            }
        }
    }

    /*
     * Phase 6H / Full Boot / Stage E: shell and/or game MRP start.mr requests
     * cfunction.ext; reg.ext names the real primary. Install index views.
     * game_package: product track — install game views without shell shim.
     */
    {
        static const char *shell_pkgs[] = {"gwy/gbrwcore.mrp", "gwy/gamelist.mrp",
                                           "gwy/gbrwshell.mrp"};
        static const char *game_pkgs[] = {"gwy/jjfb.mrp", "gwy/wxjwq.mrp"};
        const char *mvp = getenv("JJFB_MEMBER_VIEW_PRIMARY");
        int shell_mode =
            ext_gwy_shell_shim_enabled() || ext_gwy_shell_shim_guest_native_mode();
        int game_package =
            mvp && (strcmp(mvp, "game_package") == 0 || strcmp(mvp, "game") == 0);
        int all_shell_and_game =
            (mvp && (strcmp(mvp, "all_shell_and_game") == 0 || strcmp(mvp, "1") == 0)) ||
            (getenv("JJFB_NATIVE_BOOT_FULL") && getenv("JJFB_NATIVE_BOOT_FULL")[0] == '1');
        size_t si;

        if (shell_mode && !game_package) {
            for (si = 0; si < sizeof(shell_pkgs) / sizeof(shell_pkgs[0]); si++) {
                char view_name[128];
                const char *pkg = shell_pkgs[si];
                if (guest_vfs_resolve(&g_vfs, pkg, VFS_OPEN_READ, &res, &err) != L_OK ||
                    !res.exists) {
                    printf("[MRP_MEMBER_VIEW] shell_skip package=%s reason=missing\n", pkg);
                    continue;
                }
                snprintf(host_mrp, sizeof(host_mrp), "%s", res.host_path);
                snprintf(view_name, sizeof(view_name), "mrp_member_view/shell_%s_cfunction.mrp",
                         strstr(pkg, "gbrwcore")   ? "gbrwcore"
                         : strstr(pkg, "gamelist") ? "gamelist"
                                                   : "gbrwshell");
                join_path(view_path, sizeof(view_path), overlay, view_name);
                if (mrp_member_view_install_reg_primary(&g_vfs, host_mrp, pkg, view_path,
                                                        &g_modules, &err) != L_OK) {
                    fprintf(stderr, "[MRP_MEMBER_VIEW] reg_primary failed package=%s: %s %s\n",
                            pkg, err.message, err.detail);
                }
            }
        }

        if (game_package || (shell_mode && all_shell_and_game)) {
            for (si = 0; si < sizeof(game_pkgs) / sizeof(game_pkgs[0]); si++) {
                char view_name[128];
                const char *pkg = game_pkgs[si];
                if (guest_vfs_resolve(&g_vfs, pkg, VFS_OPEN_READ, &res, &err) != L_OK ||
                    !res.exists) {
                    printf("[MRP_MEMBER_VIEW] game_skip package=%s reason=missing\n", pkg);
                    continue;
                }
                snprintf(host_mrp, sizeof(host_mrp), "%s", res.host_path);
                snprintf(view_name, sizeof(view_name), "mrp_member_view/game_%s_cfunction.mrp",
                         strstr(pkg, "jjfb") ? "jjfb" : "wxjwq");
                join_path(view_path, sizeof(view_path), overlay, view_name);
                if (mrp_member_view_install_reg_primary(&g_vfs, host_mrp, pkg, view_path,
                                                        &g_modules, &err) != L_OK) {
                    fprintf(stderr, "[MRP_MEMBER_VIEW] game reg_primary failed package=%s: %s %s\n",
                            pkg, err.message, err.detail);
                } else {
                    printf("[MRP_MEMBER_VIEW] package=%s primary=installed status=installed "
                           "mode=%s evidence=CROSS_TARGET\n",
                           pkg, game_package ? "game_package" : "all_shell_and_game");
                }
            }
        }
    }

    /* Stage E: product descriptor path — activate package-scoped c_load primary. */
    if (package_scope_enabled()) {
        const char *tgt = getenv("GWY_LAUNCH_TARGET");
        if (!tgt || !tgt[0]) tgt = getenv("JJFB_PRIMARY_TARGET");
        if ((!tgt || !tgt[0]) && g_profile_loaded && g_profile.target_mrp[0])
            tgt = g_profile.target_mrp;
        if (tgt && tgt[0]) {
            if (package_scope_set_active(tgt)) {
                printf("[JJFB_CLOAD_SCOPE] request=cfunction.ext package=%s resolved=%s "
                       "reason=package_reg_primary evidence=CROSS_TARGET\n",
                       package_scope_active_package() ? package_scope_active_package() : "?",
                       package_scope_active_primary() ? package_scope_active_primary() : "?");
                fflush(stdout);
            }
        }
    }

    /* Phase 5B: bind ExtLoader, flush DSM early stash, seed EXTRACTED sessions. */
    ext_loader_bind_registry(loader, &g_modules);
    ext_loader_flush_early(loader);
    if (ext_loader_seed_from_registry(loader, &err) != L_OK) {
        fprintf(stderr, "[EXT_LOAD] seed failed: %s\n", err.message);
    }

    {
        const char *snap = getenv("GWY_MODULE_SNAPSHOT");
        if (snap && snap[0]) {
            module_registry_write_snapshot(&g_modules, snap, &err);
        }
    }

    fflush(stdout);
    fflush(stderr);
    return 0;
}
