#include "gwy_launcher/guest_vfs.h"
#include "gwy_launcher/platform_identity.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

typedef struct Case {
    const char *in;
    int expect_ok;
    const char *expect_norm;
} Case;

static int host_file_exists(const char *path) {
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

static void ensure_dir(const char *dir) {
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#else
    /* tests run on Windows primarily */
    (void)dir;
#endif
}

int main(void) {
    static const Case cases[] = {
        {"gwy/jjfb.mrp", 1, "gwy/jjfb.mrp"},
        {"gwy\\\\jjfb.mrp", 1, "gwy/jjfb.mrp"},
        {"mythroad/gwy/jjfb.mrp", 1, "mythroad/gwy/jjfb.mrp"},
        {"mythroad//gwy///jjfb.mrp", 1, "mythroad/gwy/jjfb.mrp"},
        {"gwy/./jjfb.mrp", 1, "gwy/jjfb.mrp"},
        {"../escape.mrp", 0, NULL},
        {"gwy/../jjfb.mrp", 0, NULL},
        {"C:/windows/x.mrp", 0, NULL},
        {"/abs/x.mrp", 0, NULL},
        {"\\\\unc\\share\\x", 0, NULL},
        {"gwy/jjfb.mrp/", 1, "gwy/jjfb.mrp"},
        {".", 0, NULL},
        {"", 0, NULL},
        {"gwy/a/b/c.dat", 1, "gwy/a/b/c.dat"},
        {"mythroad/sdk_key.dat", 1, "mythroad/sdk_key.dat"},
        {"save/slot1.dat", 1, "save/slot1.dat"},
        {"gwy\\gifs\\x.gif", 1, "gwy/gifs/x.gif"},
        {"..\\x", 0, NULL},
        {"gwy/../../etc/passwd", 0, NULL},
        {"D:\\\\x.mrp", 0, NULL},
        {"CON", 0, NULL},
        {"NUL", 0, NULL},
        {"AUX", 0, NULL},
        {"gwy/CON", 0, NULL},
        {"file.txt:", 0, NULL},
        {"file.txt::$DATA", 0, NULL},
        {"\\\\?\\C:\\x", 0, NULL},
        {"C:relative.txt", 0, NULL},
    };
    static const char *ok_paths[] = {
        "gwy/a.mrp", "gwy/b/c.dat", "mythroad/sdk_key.dat", "gwy/jjfbol/0@s0.map",
        "gwy/gifs/x.gif", "save/x.dat", "sound/a.mid", "gwy/cfg.bin",
        "mythroad/gwy/cfg.bin", "gwy/start.mr", "a", "gwy/x/y/z.bin",
        "mythroad/320x480/gwy/x.mrp", "gwy/robotol.ext", "gwy/mrc_loader.ext",
        "disk/a/x", "gwy/foo_bar.mrp", "gwy/FOO.MRP", "gwy/1.2.3.mrp",
        "gwy/x-y.mrp", "gwy/jjfbol/downVersion", "system/gb16.uc2",
        "mythroad/system/gb16.uc2", "gwy/sound/a.mid", "cache/tmp.bin"
    };
    size_t i;
    int failed = 0;
    LauncherError err;
    char norm[1024];
    const char *root = getenv("GWY_FIXTURE_ROOT");
    char overlay[1024];
    char tmpbase[1024];
    GuestVfs vfs;
    VfsResolution res;
    PlatformIdentity id;
    SdkKey key;
    uint8_t *rdata = NULL;
    size_t rsize = 0;
    char poison[256];
    FILE *fp;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        LauncherStatus st = guest_path_canonicalize(cases[i].in, norm, sizeof(norm), &err);
        if (cases[i].expect_ok) {
            if (st != L_OK) {
                fprintf(stderr, "case %zu expected ok: %s\n", i, cases[i].in);
                failed++;
                continue;
            }
            if (cases[i].expect_norm && strcmp(norm, cases[i].expect_norm) != 0) {
                fprintf(stderr, "case %zu norm '%s' != '%s'\n", i, norm, cases[i].expect_norm);
                failed++;
            }
        } else if (st == L_OK) {
            fprintf(stderr, "case %zu expected fail: %s -> %s\n", i, cases[i].in, norm);
            failed++;
        }
    }

    for (i = 0; i < sizeof(ok_paths) / sizeof(ok_paths[0]); i++) {
        if (guest_path_canonicalize(ok_paths[i], norm, sizeof(norm), &err) != L_OK) {
            fprintf(stderr, "ok_path failed: %s\n", ok_paths[i]);
            failed++;
        }
    }
    /* Total path cases: cases + ok_paths >= 30 */
    printf("canonicalize_cases=%zu\n",
           (sizeof(cases) / sizeof(cases[0])) + (sizeof(ok_paths) / sizeof(ok_paths[0])));

    if (!root || !root[0]) {
        fprintf(stderr, "GWY_FIXTURE_ROOT not set — skip resolve/IO tests\n");
        if (failed) {
            fprintf(stderr, "test_guest_vfs failed (%d)\n", failed);
            return 1;
        }
        puts("test_guest_vfs OK (canonicalize only)");
        return 0;
    }

#ifdef _WIN32
    {
        char tmp[MAX_PATH];
        GetTempPathA(sizeof(tmp), tmp);
        snprintf(tmpbase, sizeof(tmpbase), "%sgwy_vfs_%u", tmp, (unsigned)GetCurrentProcessId());
    }
#else
    snprintf(tmpbase, sizeof(tmpbase), "/tmp/gwy_vfs_test");
#endif
    snprintf(overlay, sizeof(overlay), "%s/overlay", tmpbase);
    ensure_dir(tmpbase);
    ensure_dir(overlay);

    if (guest_vfs_init(&vfs, root, overlay, &err) != L_OK) {
        fprintf(stderr, "vfs init failed: %s\n", err.message);
        return 1;
    }

    /* Resolve golden MRP paths */
    if (guest_vfs_resolve(&vfs, "gwy/jjfb.mrp", VFS_OPEN_READ, &res, &err) != L_OK || !res.exists) {
        fprintf(stderr, "resolve gwy/jjfb.mrp failed\n");
        failed++;
    } else {
        printf("resolve gwy/jjfb.mrp host=%s rule=%s\n", res.host_path, res.rule);
    }
    if (guest_vfs_resolve(&vfs, "mythroad/gwy/jjfb.mrp", VFS_OPEN_READ, &res, &err) != L_OK || !res.exists) {
        fprintf(stderr, "resolve mythroad/gwy/jjfb.mrp failed\n");
        failed++;
    } else {
        printf("resolve mythroad/gwy/jjfb.mrp host=%s rule=%s\n", res.host_path, res.rule);
    }

    /* Overlay write must not touch resource root */
    snprintf(poison, sizeof(poison), "%s/__vfs_overlay_probe__.bin", root);
    if (host_file_exists(poison)) {
        fprintf(stderr, "unexpected probe already in resource root\n");
        failed++;
    }
    if (guest_vfs_write_all(&vfs, "save/overlay_probe.bin", "PROBE", 5, &err) != L_OK) {
        fprintf(stderr, "overlay write failed: %s\n", err.message);
        failed++;
    } else if (host_file_exists(poison)) {
        fprintf(stderr, "overlay write mutated resource root!\n");
        failed++;
    } else {
        char over_host[1024];
        snprintf(over_host, sizeof(over_host), "%s/save/overlay_probe.bin", overlay);
        if (!host_file_exists(over_host)) {
            fprintf(stderr, "overlay file missing at %s\n", over_host);
            failed++;
        } else {
            printf("overlay write ok host=%s\n", over_host);
        }
    }

    /* Write without overlay must fail */
    {
        GuestVfs ro;
        if (guest_vfs_init(&ro, root, NULL, &err) != L_OK) return 1;
        if (guest_vfs_write_all(&ro, "save/x.bin", "X", 1, &err) == L_OK) {
            fprintf(stderr, "write without overlay should fail\n");
            failed++;
        }
    }

    /* Generated sdk_key */
    platform_identity_set_defaults(&id);
    if (platform_sdk_key_generate(&id, &key, &err) != L_OK) {
        fprintf(stderr, "sdk key gen failed\n");
        failed++;
    } else {
        char key_host[1024];
        snprintf(key_host, sizeof(key_host), "%s/sdk_key.dat", tmpbase);
        if (platform_sdk_key_write_file(key_host, &key, &err) != L_OK) {
            fprintf(stderr, "sdk key write failed\n");
            failed++;
        } else if (guest_vfs_set_generated(&vfs, "mythroad/sdk_key.dat", key_host, key.sha256_hex, &err) != L_OK) {
            fprintf(stderr, "set_generated failed\n");
            failed++;
        } else if (guest_vfs_resolve(&vfs, "mythroad/sdk_key.dat", VFS_OPEN_READ, &res, &err) != L_OK ||
                   res.backend != VFS_GENERATED || !res.exists) {
            fprintf(stderr, "generated resolve failed backend=%d\n", (int)res.backend);
            failed++;
        } else if (guest_vfs_read_all(&vfs, "sdk_key.dat", &rdata, &rsize, &err) != L_OK ||
                   rsize != GWY_SDK_KEY_SIZE || memcmp(rdata, key.bytes, GWY_SDK_KEY_SIZE) != 0) {
            fprintf(stderr, "generated read_all mismatch size=%zu\n", rsize);
            failed++;
        } else if (strcmp(key.sha256_hex, "5d87a42f3d47ac8ddaf892f08409373b18936af761c6b9c8331750dbad3cc436") != 0) {
            fprintf(stderr, "sdk key sha mismatch %s\n", key.sha256_hex);
            failed++;
        } else if (strcmp(res.guest_canonical, "sdk_key.dat") != 0) {
            fprintf(stderr, "canonical expected sdk_key.dat got %s\n", res.guest_canonical);
            failed++;
        } else if (guest_vfs_write_all(&vfs, "mythroad/sdk_key.dat", "X", 1, &err) == L_OK) {
            fprintf(stderr, "generated write should be rejected\n");
            failed++;
        } else {
            printf("generated sdk_key ok sha=%s backend=%s canonical=%s (write rejected)\n",
                   key.sha256_hex, vfs_backend_name(res.backend), res.guest_canonical);
        }
        free(rdata);
        rdata = NULL;
    }

    /* mythroad prefix → same canonical as bare gwy path */
    if (guest_vfs_resolve(&vfs, "mythroad/gwy/jjfb.mrp", VFS_OPEN_READ, &res, &err) == L_OK) {
        if (strcmp(res.guest_canonical, "gwy/jjfb.mrp") != 0) {
            fprintf(stderr, "canonical mismatch %s\n", res.guest_canonical);
            failed++;
        }
    }

    /* Miss recording */
    guest_vfs_reset_misses(&vfs);
    if (guest_vfs_resolve(&vfs, "gwy/__no_such_file__.mrp", VFS_OPEN_READ, &res, &err) == L_OK) {
        fprintf(stderr, "expected miss\n");
        failed++;
    }
    guest_vfs_resolve(&vfs, "gwy/__no_such_file__.mrp", VFS_OPEN_READ, &res, &err);
    if (vfs.miss_unique < 1 || vfs.miss_total < 2) {
        fprintf(stderr, "miss counters unique=%zu total=%u\n",
                (size_t)vfs.miss_unique, (unsigned)vfs.miss_total);
        failed++;
    } else {
        guest_vfs_miss_summary(&vfs);
    }

    /* Cleanup probe markers — do not leave files in resource root (none expected) */
    (void)fp;

    if (failed) {
        fprintf(stderr, "test_guest_vfs failed (%d)\n", failed);
        return 1;
    }
    puts("test_guest_vfs OK");
    return 0;
}
