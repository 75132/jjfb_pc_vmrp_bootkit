#include "gwy_launcher/platform_identity.h"
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

static int host_exists(const char *path) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
#endif
}

static void ensure_dir(const char *d) {
#ifdef _WIN32
    CreateDirectoryA(d, NULL);
#endif
}

int main(void) {
    const char *root = getenv("GWY_FIXTURE_ROOT");
    char tmpbase[512];
    char overlay[512];
    char forbidden[512];
    GuestVfs vfs;
    VmFileService *svc = NULL;
    LauncherError err;
    int32_t h;
    char buf[64];
    int32_t n;
    int32_t len;
    PlatformIdentity id;
    SdkKey key;

    if (!root || !root[0]) {
        fprintf(stderr, "GWY_FIXTURE_ROOT required\n");
        return 1;
    }

#ifdef _WIN32
    {
        char tmp[MAX_PATH];
        GetTempPathA(sizeof(tmp), tmp);
        snprintf(tmpbase, sizeof(tmpbase), "%sgwy_vfsvc_%u", tmp, (unsigned)GetCurrentProcessId());
    }
#else
    snprintf(tmpbase, sizeof(tmpbase), "/tmp/gwy_vfsvc");
#endif
    snprintf(overlay, sizeof(overlay), "%s/overlay", tmpbase);
    snprintf(forbidden, sizeof(forbidden), "%s/mythroad/sdk_key.dat", tmpbase);
    ensure_dir(tmpbase);
    ensure_dir(overlay);

    if (gwy_vm_file_bootstrap(root, overlay, &vfs, &svc, &err) != L_OK) {
        fprintf(stderr, "bootstrap failed: %s\n", err.message);
        return 1;
    }
    if (!gwy_vm_file_is_bound()) {
        fprintf(stderr, "not bound\n");
        return 1;
    }
    if (host_exists(forbidden)) {
        fprintf(stderr, "forbidden cwd-style sdk_key appeared: %s\n", forbidden);
        return 1;
    }

    /* sdk_key via generated */
    h = vm_file_service_open(svc, "mythroad/sdk_key.dat", GWY_MR_FILE_RDONLY);
    if (h <= 0) {
        fprintf(stderr, "open sdk_key failed\n");
        return 1;
    }
    len = vm_file_service_get_len(svc, "mythroad/sdk_key.dat");
    if (len != GWY_SDK_KEY_SIZE) {
        fprintf(stderr, "sdk_key len=%d\n", (int)len);
        return 1;
    }
    n = vm_file_service_read(svc, h, buf, 16);
    if (n != 16) {
        fprintf(stderr, "sdk_key read n=%d\n", (int)n);
        return 1;
    }
    platform_identity_set_defaults(&id);
    platform_sdk_key_generate(&id, &key, &err);
    if (memcmp(buf, key.bytes, 16) != 0) {
        fprintf(stderr, "sdk_key bytes mismatch\n");
        return 1;
    }
    if (vm_file_service_close(svc, h) != GWY_MR_SUCCESS) return 1;

    /* jjfb canonical */
    h = vm_file_service_open(svc, "gwy/jjfb.mrp", GWY_MR_FILE_RDONLY);
    if (h <= 0) {
        fprintf(stderr, "open jjfb failed\n");
        return 1;
    }
    if (vm_file_service_seek(svc, h, 0, GWY_MR_SEEK_END) != GWY_MR_SUCCESS) return 1;
    if (vm_file_service_seek(svc, h, 0, GWY_MR_SEEK_SET) != GWY_MR_SUCCESS) return 1;
    n = vm_file_service_read(svc, h, buf, 4);
    if (n != 4 || memcmp(buf, "MRPG", 4) != 0) {
        fprintf(stderr, "jjfb magic bad\n");
        return 1;
    }
    /* partial EOF */
    vm_file_service_seek(svc, h, -2, GWY_MR_SEEK_END);
    n = vm_file_service_read(svc, h, buf, 100);
    if (n < 0 || n > 2) {
        fprintf(stderr, "partial eof n=%d\n", (int)n);
        return 1;
    }
    vm_file_service_close(svc, h);

    /* overlay write does not touch resource root */
    {
        char poison[512];
        snprintf(poison, sizeof(poison), "%s/__no_touch__.bin", root);
        h = vm_file_service_open(svc, "save/probe.bin", GWY_MR_FILE_CREATE | GWY_MR_FILE_WRONLY);
        if (h <= 0) {
            fprintf(stderr, "overlay open write failed\n");
            return 1;
        }
        if (vm_file_service_write(svc, h, "XYZ", 3) != 3) return 1;
        vm_file_service_close(svc, h);
        if (host_exists(poison)) {
            fprintf(stderr, "resource root mutated\n");
            return 1;
        }
    }

    /* MISS — no host fallback */
    h = vm_file_service_open(svc, "gwy/__missing__.mrp", GWY_MR_FILE_RDONLY);
    if (h != 0) {
        fprintf(stderr, "expected miss\n");
        return 1;
    }

    /* close then read fails */
    h = vm_file_service_open(svc, "gwy/cfg.bin", GWY_MR_FILE_RDONLY);
    if (h <= 0) return 1;
    vm_file_service_close(svc, h);
    if (vm_file_service_read(svc, h, buf, 4) != GWY_MR_FAILED) {
        fprintf(stderr, "read after close should fail\n");
        return 1;
    }

    /* generated write rejected */
    if (vm_file_service_open(svc, "mythroad/sdk_key.dat",
                             GWY_MR_FILE_CREATE | GWY_MR_FILE_WRONLY) != 0) {
        fprintf(stderr, "generated write open should fail\n");
        return 1;
    }

    if (vm_file_service_open_count(svc) != 0) {
        fprintf(stderr, "open_count should be 0\n");
        return 1;
    }

    /* bind_owned lifecycle */
    {
        VmFileService *svc2 = NULL;
        if (vm_file_service_create(&vfs, &svc2, &err) != L_OK) return 1;
        gwy_vm_file_unbind(); /* clear bootstrap owner=1 */
        if (gwy_vm_file_bind_owned(svc2, 42) != 0) {
            fprintf(stderr, "bind_owned 42 should succeed\n");
            return 1;
        }
        if (gwy_vm_file_bound_owner() != 42) {
            fprintf(stderr, "owner expected 42\n");
            return 1;
        }
        if (gwy_vm_file_bind_owned(svc, 99) == 0) {
            fprintf(stderr, "bind other owner should refuse\n");
            return 1;
        }
        gwy_vm_file_unbind_owned(99); /* refuse */
        if (!gwy_vm_file_is_bound()) {
            fprintf(stderr, "unbind wrong owner should leave bound\n");
            return 1;
        }
        gwy_vm_file_unbind_owned(42);
        if (gwy_vm_file_is_bound()) {
            fprintf(stderr, "should be unbound\n");
            return 1;
        }
        if (gwy_vm_file_open("gwy/jjfb.mrp", GWY_MR_FILE_RDONLY) != 0) {
            fprintf(stderr, "ABI open after unbind must fail\n");
            return 1;
        }
        if (gwy_vm_file_bind_owned(svc, 7) != 0) return 1;
        gwy_vm_file_unbind_owned(7);
        vm_file_service_destroy(svc2);
    }

    /* re-bind original for destroy cleanup */
    gwy_vm_file_bind(svc);
    vm_file_service_destroy(svc);
    if (gwy_vm_file_is_bound()) {
        fprintf(stderr, "still bound after destroy\n");
        return 1;
    }

    puts("test_vm_file_service OK");
    return 0;
}
