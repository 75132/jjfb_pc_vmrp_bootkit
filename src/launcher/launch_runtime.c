#include "gwy_launcher/launch_runtime.h"
#include "gwy_launcher/guest_vfs.h"
#include "gwy_launcher/platform_identity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

static void ensure_dir(const char *dir) {
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#else
    mkdir(dir, 0755);
#endif
}

/*
 * Install sdk_key via GuestVFS generated only (overlay host path).
 * Does NOT write cwd/mythroad/sdk_key.dat — Phase 4B kills dual filesystem.
 */
static LauncherStatus install_sdk_key_via_vfs(GuestVfs *vfs,
                                              const char *overlay_root,
                                              LauncherError *err) {
    PlatformIdentity id;
    SdkKey key;
    char key_host[VFS_PATH_MAX];
    VfsResolution res;
    LauncherStatus st;

    platform_identity_set_defaults(&id);
    st = platform_sdk_key_generate(&id, &key, err);
    if (st != L_OK) return st;

    st = guest_vfs_write_all(vfs, "mythroad/sdk_key.dat", key.bytes, GWY_SDK_KEY_SIZE, err);
    if (st != L_OK) return st;

    snprintf(key_host, sizeof(key_host), "%s/mythroad/sdk_key.dat", overlay_root);
    st = guest_vfs_set_generated(vfs, "mythroad/sdk_key.dat", key_host, key.sha256_hex, err);
    if (st != L_OK) return st;
    st = guest_vfs_set_generated(vfs, "sdk_key.dat", key_host, key.sha256_hex, err);
    if (st != L_OK) return st;

    st = guest_vfs_resolve(vfs, "mythroad/sdk_key.dat", VFS_OPEN_READ, &res, err);
    if (st != L_OK || !res.exists || res.backend != VFS_GENERATED) {
        launcher_error_set(err, L_ERR_STATE, "launch_runtime",
                           "sdk_key resolve expected generated",
                           res.host_path);
        return L_ERR_STATE;
    }
    guest_vfs_trace_open(&res, 1);
    printf("[JJFB_SDK_KEY] path=%s sha256=%s len=%d backend=generated\n",
           key_host, key.sha256_hex, GWY_SDK_KEY_SIZE);
    return L_OK;
}

LauncherStatus gwy_launch_spawn_vmrp(const LaunchDescriptor *desc,
                                     const char *vmrp_exe,
                                     const char *vmrp_cwd,
                                     const char *manifest_path,
                                     LauncherError *err) {
    GuestVfs vfs;
    VfsResolution res;
    LauncherStatus st;
    char overlay[VFS_PATH_MAX];
    char forbidden_key[VFS_PATH_MAX];

    launcher_error_clear(err);
    if (!desc || !vmrp_exe || !vmrp_cwd) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "launch_runtime", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (!launch_descriptor_validate_basic(desc)) {
        launcher_error_set(err, L_ERR_STATE, "launch_runtime", "invalid descriptor", NULL);
        return L_ERR_STATE;
    }

    snprintf(overlay, sizeof(overlay), "%s/overlay", vmrp_cwd);
    ensure_dir(overlay);

    /* Gate: remove legacy transitional copy if present; must not reappear. */
    snprintf(forbidden_key, sizeof(forbidden_key), "%s/mythroad/sdk_key.dat", vmrp_cwd);
#ifdef _WIN32
    DeleteFileA(forbidden_key);
#endif

    st = guest_vfs_init(&vfs, desc->resource_root, overlay, err);
    if (st != L_OK) return st;

    st = guest_vfs_resolve(&vfs, desc->target_mrp, VFS_OPEN_READ, &res, err);
    if (st != L_OK) {
        guest_vfs_trace_miss(&vfs, res.guest_normalized, VFS_MISS_PROBE);
        return st;
    }
    guest_vfs_trace_open(&res, 1);

    st = guest_vfs_resolve(&vfs, "gwy/cfg.bin", VFS_OPEN_READ, &res, err);
    if (st != L_OK) {
        guest_vfs_trace_miss(&vfs, res.guest_normalized, VFS_MISS_PROBE);
        return st;
    }

    st = install_sdk_key_via_vfs(&vfs, overlay, err);
    if (st != L_OK) return st;

#ifdef _WIN32
    {
        DWORD a = GetFileAttributesA(forbidden_key);
        if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) {
            launcher_error_set(err, L_ERR_STATE, "launch_runtime",
                               "forbidden cwd mythroad/sdk_key.dat present after install",
                               forbidden_key);
            return L_ERR_STATE;
        }
    }
#endif

    if (manifest_path && manifest_path[0]) {
        st = launch_manifest_write(manifest_path, desc, err);
        if (st != L_OK) return st;
        printf("[JJFB_STARTGAME] manifest=%s\n", manifest_path);
    }

    /* Product track (D6): cfg/descriptor → target MRP. Not native_shell runapp. */
    printf("[JJFB_GWY_LAUNCH] cfg_index=%u target=%s source=descriptor_launcher "
           "evidence=DOCUMENTED\n",
           desc->cfg_index, desc->target_mrp);
    printf("[JJFB_PARAM] %s\n", desc->param);
    printf("[JJFB_CWD] %s\n", vmrp_cwd);
    fflush(stdout);

#ifdef _WIN32
    {
        char cmdline[1400];
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        const char *profile = getenv("GWY_PROFILE");

        SetEnvironmentVariableA("GWY_LAUNCH", "1");
        SetEnvironmentVariableA("GWY_LAUNCH_TARGET", desc->target_mrp);
        SetEnvironmentVariableA("GWY_LAUNCH_PARAM", desc->param);
        SetEnvironmentVariableA("GWY_RESOURCE_ROOT", desc->resource_root);
        SetEnvironmentVariableA("GWY_OVERLAY_ROOT", overlay);
        /* Preserve caller GWY_PROFILE; do not invent shell-chain JJFB_* flags. */
        if (profile && profile[0]) SetEnvironmentVariableA("GWY_PROFILE", profile);

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));
        snprintf(cmdline, sizeof(cmdline), "\"%s\"", vmrp_exe);
        if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, vmrp_cwd, &si, &pi)) {
            char detail[128];
            snprintf(detail, sizeof(detail), "CreateProcess failed (%lu)", (unsigned long)GetLastError());
            launcher_error_set(err, L_ERR_IO, "launch_runtime", detail, vmrp_exe);
            return L_ERR_IO;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        printf("[JJFB_RUNAPP] source=descriptor_launcher target=%s spawned=ok "
               "evidence=DOCUMENTED\n",
               desc->target_mrp);
        return L_OK;
    }
#else
    launcher_error_set(err, L_ERR_UNSUPPORTED, "launch_runtime", "spawn only implemented on Windows", NULL);
    return L_ERR_UNSUPPORTED;
#endif
}
