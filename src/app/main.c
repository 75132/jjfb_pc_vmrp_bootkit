#include "gwy_launcher/error.h"
#include "gwy_launcher/game_catalog.h"
#include "gwy_launcher/guest_vfs.h"
#include "gwy_launcher/gwy_cfg.h"
#include "gwy_launcher/launch_descriptor.h"
#include "gwy_launcher/launch_runtime.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/mrp_guest_index.h"
#include "gwy_launcher/mrp_member_view.h"
#include "gwy_launcher/compat_profile.h"
#include "gwy_launcher/platform_identity.h"
#include "gwy_launcher/sha256.h"
#include "gwy_launcher/byte_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static void print_err(const LauncherError *err) {
    fprintf(stderr, "error %s [%s] %s %s\n",
            launcher_status_name(err->code),
            err->subsystem ? err->subsystem : "",
            err->message,
            err->detail);
}

static int cmd_inspect_mrp(const char *path) {
    MrpArchive *a = NULL;
    LauncherError err;
    LauncherStatus st;
    char hex[65];
    size_t i;

    st = mrp_archive_open(path, &a, &err);
    if (st != L_OK) { print_err(&err); return 1; }
    gwy_sha256_hex(a->sha256, hex);
    printf("path=%s\n", a->path);
    printf("sha256=%s\n", hex);
    printf("size=%zu\n", a->size);
    printf("header_length=%u\n", a->header_length);
    printf("first_data_offset=%u\n", a->first_data_offset);
    printf("internal_name=%s\n", a->internal_name);
    printf("appid=%u\n", a->appid_le);
    printf("appver=%u\n", a->appver_le);
    printf("flags=%u\n", a->flags);
    printf("members=%zu\n", a->member_count);
    for (i = 0; i < a->member_count; i++) {
        const MrpMember *m = &a->members[i];
        printf("  [%zu] name=%s stored_offset=%u stored_size=%u unpacked_size=%u compression=%s\n",
               i, m->name, m->offset, m->stored_size,
               m->unpacked_known ? m->unpacked_size : 0,
               mrp_compression_name(m->compression));
    }
    mrp_archive_close(a);
    return 0;
}

static int cmd_inspect_member_view(const char *mrp_path,
                                   const char *member,
                                   const char *to_member,
                                   const char *profile_path) {
    MrpArchive *arch = NULL;
    const MrpMember *target = NULL;
    LauncherError err;
    uint8_t *view = NULL;
    size_t view_size = 0;
    MrpGuestLookupResult lr;
    MrpGuestLookupCode code;
    char to_buf[128];
    const char *to_name = to_member;
    CompatibilityProfile profile;
    char hex[65];

    if (!mrp_path || !member) return 1;
    if (mrp_archive_open(mrp_path, &arch, &err) != L_OK) {
        print_err(&err);
        return 1;
    }
    gwy_sha256_hex(arch->sha256, hex);
    printf("path=%s\nsha256=%s\nsize=%zu\nfirst_data_offset=%u\n", mrp_path, hex, arch->size,
           arch->first_data_offset);

    code = mrp_guest_index_lookup(arch->data, arch->size, member, &lr);
    printf("[MRP_VIEW_LOOKUP] original member=%s code=%d\n", member, (int)code);

    if (code == MRP_GUEST_LOOKUP_HIT) {
        printf("exact HIT offset=%u stored_size=%u (no synthesize needed)\n", lr.offset,
               lr.stored_size);
        mrp_archive_close(arch);
        return 0;
    }

    if (!to_name && profile_path) {
        if (compatibility_profile_load_json_file(profile_path, &profile, &err) != L_OK) {
            print_err(&err);
            mrp_archive_close(arch);
            return 1;
        }
        if (!compatibility_profile_find_member_alias(&profile, member, to_buf, sizeof(to_buf))) {
            fprintf(stderr, "no profile alias for %s\n", member);
            mrp_archive_close(arch);
            return 1;
        }
        to_name = to_buf;
    }
    if (!to_name) {
        fprintf(stderr, "member missing; pass --to <member> or --profile\n");
        mrp_archive_close(arch);
        return 1;
    }
    if (mrp_archive_find_exact(arch, to_name, &target, &err) != L_OK) {
        print_err(&err);
        mrp_archive_close(arch);
        return 1;
    }
    if (mrp_member_view_synthesize_alias(arch, member, target, &view, &view_size, &err) != L_OK) {
        print_err(&err);
        mrp_archive_close(arch);
        return 1;
    }
    code = mrp_guest_index_lookup(view, view_size, member, &lr);
    printf("[MRP_VIEW_LOOKUP] synthesized member=%s alias_to=%s code=%d offset=%u stored_size=%u\n",
           member, to_name, (int)code, lr.offset, lr.stored_size);
    if (code == MRP_GUEST_LOOKUP_ERR_3004) {
        printf("[GUEST_EXT_LOAD_FAIL] producer=_mr_readFile stage=LOOKUP trigger=invalid_name_len "
               "after torn index\n");
        free(view);
        mrp_archive_close(arch);
        return 1;
    }
    if (code != MRP_GUEST_LOOKUP_HIT) {
        free(view);
        mrp_archive_close(arch);
        return 1;
    }
    {
        ByteBuffer decoded;
        uint8_t sha[32];
        char sha_hex[65];
        MrpArchive *varch = NULL;
        const MrpMember *alias_m = NULL;
        char tmp[512];
#ifdef _WIN32
        char tmpdir[MAX_PATH];
        GetTempPathA(sizeof(tmpdir), tmpdir);
        snprintf(tmp, sizeof(tmp), "%s\\gwy_inspect_view.mrp", tmpdir);
#else
        snprintf(tmp, sizeof(tmp), "/tmp/gwy_inspect_view.mrp");
#endif
        {
            FILE *fp = fopen(tmp, "wb");
            if (!fp || fwrite(view, 1, view_size, fp) != view_size) {
                if (fp) fclose(fp);
                free(view);
                mrp_archive_close(arch);
                return 1;
            }
            fclose(fp);
        }
        if (mrp_archive_open(tmp, &varch, &err) != L_OK ||
            mrp_archive_find_exact(varch, member, &alias_m, &err) != L_OK) {
            print_err(&err);
            if (varch) mrp_archive_close(varch);
            free(view);
            mrp_archive_close(arch);
            return 1;
        }
        byte_buffer_init(&decoded);
        if (mrp_archive_decode_member(varch, alias_m, 8u * 1024u * 1024u, &decoded, &err) != L_OK) {
            print_err(&err);
            byte_buffer_free(&decoded);
            mrp_archive_close(varch);
            free(view);
            mrp_archive_close(arch);
            return 1;
        }
        gwy_sha256(decoded.data, decoded.size, sha);
        gwy_sha256_hex(sha, sha_hex);
        printf("[MRP_VIEW_DECODE] member=%s unpacked_size=%zu sha256=%s result=OK\n", member,
               decoded.size, sha_hex);
        byte_buffer_free(&decoded);
        mrp_archive_close(varch);
    }
    free(view);
    mrp_archive_close(arch);
    return 0;
}

static int cmd_inspect_cfg(const char *path, uint32_t index) {
    GwyCfgFile *cfg = NULL;
    GwyCfgRecord rec;
    LauncherError err;
    LauncherStatus st;
    char hex[65];

    st = gwy_cfg_open(path, &cfg, &err);
    if (st != L_OK) { print_err(&err); return 1; }
    gwy_sha256_hex(cfg->sha256, hex);
    printf("path=%s\n", cfg->path);
    printf("sha256=%s\n", hex);
    printf("size=%zu\n", cfg->size);

    st = gwy_cfg_read_record(cfg, index, &rec, &err);
    if (st != L_OK) {
        print_err(&err);
        gwy_cfg_close(cfg);
        return 1;
    }
    printf("record.index=%u\n", rec.index);
    printf("record.offset=%u\n", rec.file_offset);
    printf("record.size=%u\n", rec.record_size);
    printf("record.layout=%s\n", rec.layout_confidence);
    printf("icon=%s\n", rec.icon.present ? rec.icon.value : "");
    printf("napptype=%u\n", rec.napptype.present ? rec.napptype.value : 0);
    printf("nextid=%u\n", rec.nextid.present ? rec.nextid.value : 0);
    printf("ncode=%u\n", rec.ncode.present ? rec.ncode.value : 0);
    printf("narg=%u\n", rec.narg.present ? rec.narg.value : 0);
    printf("narg1=%u\n", rec.narg1.present ? rec.narg1.value : 0);
    printf("target=%s\n", rec.target_mrp.present ? rec.target_mrp.value : "");
    gwy_cfg_close(cfg);
    return 0;
}

static int cmd_validate_root(const char *root) {
    char mrp_path[1024];
    char cfg_path[1024];
    MrpArchive *a = NULL;
    GwyCfgFile *cfg = NULL;
    GwyCfgRecord rec;
    LauncherError err;
    LauncherStatus st;
    char hex[65];
    const MrpMember *m = NULL;
    ByteBuffer buf;
    int rc = 0;

    snprintf(mrp_path, sizeof(mrp_path), "%s/gwy/jjfb.mrp", root);
    snprintf(cfg_path, sizeof(cfg_path), "%s/gwy/cfg.bin", root);

    printf("[validate] root=%s\n", root);
    st = mrp_archive_open(mrp_path, &a, &err);
    if (st != L_OK) { print_err(&err); return 1; }
    gwy_sha256_hex(a->sha256, hex);
    printf("[validate] jjfb.sha256=%s\n", hex);
    if (strcmp(hex, "52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036") != 0) {
        fprintf(stderr, "[validate] jjfb hash mismatch\n");
        rc = 1;
    }
    printf("[validate] appid=%u appver=%u members=%zu\n", a->appid_le, a->appver_le, a->member_count);

    st = mrp_archive_find_exact(a, "start.mr", &m, &err);
    if (st != L_OK) { print_err(&err); rc = 1; }
    else {
        st = mrp_archive_decode_member(a, m, 8u * 1024u * 1024u, &buf, &err);
        if (st != L_OK) { print_err(&err); rc = 1; }
        else {
            printf("[validate] start.mr decoded=%zu\n", buf.size);
            byte_buffer_free(&buf);
        }
    }
    st = mrp_archive_find_exact(a, "robotol.ext", &m, &err);
    if (st != L_OK) { print_err(&err); rc = 1; }
    else {
        st = mrp_archive_decode_member(a, m, 8u * 1024u * 1024u, &buf, &err);
        if (st != L_OK) { print_err(&err); rc = 1; }
        else {
            printf("[validate] robotol.ext decoded=%zu\n", buf.size);
            byte_buffer_free(&buf);
        }
    }
    st = mrp_archive_find_exact(a, "cfunction.ext", &m, &err);
    printf("[validate] cfunction.ext exact=%s\n", st == L_OK ? "HIT" : "MISS");

    st = gwy_cfg_open(cfg_path, &cfg, &err);
    if (st != L_OK) { print_err(&err); mrp_archive_close(a); return 1; }
    st = gwy_cfg_read_record(cfg, 36, &rec, &err);
    if (st != L_OK) { print_err(&err); rc = 1; }
    else {
        printf("[validate] cfg36 target=%s napptype=%u nextid=%u ncode=%u narg=%u narg1=%u\n",
               rec.target_mrp.value, rec.napptype.value, rec.nextid.value,
               rec.ncode.value, rec.narg.value, rec.narg1.value);
        if (strcmp(rec.target_mrp.value, "gwy/jjfb.mrp") != 0 ||
            rec.napptype.value != 12 || rec.nextid.value != 482 ||
            rec.ncode.value != 512 || rec.narg.value != 0 || rec.narg1.value != 1) {
            fprintf(stderr, "[validate] cfg36 contract mismatch\n");
            rc = 1;
        }
    }

    /* Formal descriptor + VFS preflight (integration path). */
    {
        LaunchDescriptor d;
        LaunchExpectations ex;
        GuestVfs vfs;
        VfsResolution res;
        memset(&ex, 0, sizeof(ex));
        ex.has_target = 1;
        snprintf(ex.target_mrp, sizeof(ex.target_mrp), "%s", "gwy/jjfb.mrp");
        ex.has_sha256 = 1;
        snprintf(ex.sha256_hex, sizeof(ex.sha256_hex), "%s",
                 "52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036");
        st = launch_descriptor_build(root, 36, "gwy.jjfb.original", &ex, &d, &err);
        if (st != L_OK) {
            print_err(&err);
            rc = 1;
        } else {
            printf("[validate] param=%s\n", d.param);
            printf("[validate] descriptor_ok=%d\n", launch_descriptor_validate_basic(&d));
            if (!launch_descriptor_validate_basic(&d)) rc = 1;
            if (guest_vfs_init(&vfs, root, NULL, &err) != L_OK ||
                guest_vfs_resolve(&vfs, d.target_mrp, VFS_OPEN_READ, &res, &err) != L_OK) {
                print_err(&err);
                rc = 1;
            } else {
                printf("[validate] vfs host=%s ok=%d\n", res.host_path, res.exists);
            }
        }
    }

    gwy_cfg_close(cfg);
    mrp_archive_close(a);
    printf("[validate] result=%s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}

static int cmd_scan(const char *root) {
    GwyGameCatalog cat;
    LauncherError err;
    LauncherStatus st;
    size_t i;

    st = gwy_game_catalog_scan(root, &cat, &err);
    if (st != L_OK) { print_err(&err); return 1; }
    printf("resource_root=%s\n", cat.resource_root);
    printf("games=%zu\n", cat.count);
    for (i = 0; i < cat.count; i++) {
        const GwyGameEntry *e = &cat.entries[i];
        const char *title = e->title_utf8[0] ? e->title_utf8 :
                            (e->display_name_utf8[0] ? e->display_name_utf8 : e->target_mrp);
        printf("[%u] napptype=%d nextid=%d ncode=%d target=%s title=%s app=%u/%u\n",
               e->cfg_index, (int)e->napptype, (int)e->nextid, (int)e->ncode,
               e->target_mrp, title, e->appid, e->appver);
    }
    return 0;
}

static int cmd_games(const char *root, const char *vmrp_exe, const char *vmrp_cwd) {
    GwyGameCatalog cat;
    LauncherError err;
    LauncherStatus st;

    st = gwy_game_catalog_scan(root, &cat, &err);
    if (st != L_OK) { print_err(&err); return 1; }
    printf("loaded %zu games from cfg; opening list window...\n", cat.count);
    fflush(stdout);
    return gwy_games_picker_run(&cat, vmrp_exe, vmrp_cwd);
}

static int cmd_sdk_key(const char *out_path) {
    PlatformIdentity id;
    SdkKey key;
    LauncherError err;
    LauncherStatus st;

    platform_identity_set_defaults(&id);
    st = platform_sdk_key_generate(&id, &key, &err);
    if (st != L_OK) { print_err(&err); return 1; }
    if (out_path && out_path[0]) {
        st = platform_sdk_key_write_file(out_path, &key, &err);
        if (st != L_OK) { print_err(&err); return 1; }
        printf("output=%s\n", out_path);
    }
    printf("key_len=%d\n", GWY_SDK_KEY_SIZE);
    printf("key_sha256=%s\n", key.sha256_hex);
    printf("vmver=%s imei=%s manufacturer=%s model=%s\n",
           id.vmver, id.imei, id.manufacturer, id.model);
    return 0;
}

static int cmd_launch(const char *root, uint32_t cfg_index,
                      const char *vmrp_exe, const char *vmrp_cwd,
                      const char *manifest_path) {
    LaunchDescriptor desc;
    LauncherError err;
    LauncherStatus st;

    st = launch_descriptor_build(root, cfg_index, "cli", NULL, &desc, &err);
    if (st != L_OK) { print_err(&err); return 1; }
    printf("[launch] target=%s param=%s sha=%s\n", desc.target_mrp, desc.param, desc.target_sha256_hex);
    st = gwy_launch_spawn_vmrp(&desc, vmrp_exe, vmrp_cwd, manifest_path, &err);
    if (st != L_OK) { print_err(&err); return 1; }
    puts("[launch] spawned");
    return 0;
}

static int cmd_vfs_check(const char *root, const char *overlay) {
    GuestVfs vfs;
    VfsResolution res;
    LauncherError err;
    PlatformIdentity id;
    SdkKey key;
    char key_path[VFS_PATH_MAX];
    char overlay_buf[VFS_PATH_MAX];
    const char *paths[] = {
        "gwy/jjfb.mrp",
        "mythroad/gwy/jjfb.mrp",
        "gwy/cfg.bin",
        "mythroad/sdk_key.dat",
        "sdk_key.dat"
    };
    size_t i;
    int rc = 0;
    const char *ov = overlay;

    if (!ov || !ov[0]) {
        snprintf(overlay_buf, sizeof(overlay_buf), "%s", "logs/vfs_overlay_check");
        ov = overlay_buf;
    }
#ifdef _WIN32
    CreateDirectoryA("logs", NULL);
    CreateDirectoryA(ov, NULL);
#endif
    if (guest_vfs_init(&vfs, root, ov, &err) != L_OK) {
        print_err(&err);
        return 1;
    }

    platform_identity_set_defaults(&id);
    if (platform_sdk_key_generate(&id, &key, &err) != L_OK) {
        print_err(&err);
        return 1;
    }
    snprintf(key_path, sizeof(key_path), "%s/sdk_key.dat", ov);
    if (platform_sdk_key_write_file(key_path, &key, &err) != L_OK ||
        guest_vfs_set_generated(&vfs, "mythroad/sdk_key.dat", key_path, key.sha256_hex, &err) != L_OK ||
        guest_vfs_set_generated(&vfs, "sdk_key.dat", key_path, key.sha256_hex, &err) != L_OK) {
        print_err(&err);
        return 1;
    }
    printf("[vfs-check] root=%s overlay=%s\n", root, ov);
    printf("[vfs-check] sdk_key_sha256=%s\n", key.sha256_hex);

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if (guest_vfs_resolve(&vfs, paths[i], VFS_OPEN_READ, &res, &err) != L_OK) {
            guest_vfs_trace_miss(&vfs, res.guest_normalized[0] ? res.guest_normalized : paths[i], VFS_MISS_PROBE);
            rc = 1;
            continue;
        }
        guest_vfs_trace_open(&res, 1);
        if (!res.exists) rc = 1;
    }
    guest_vfs_miss_summary(&vfs);
    printf("[vfs-check] result=%s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}

static void usage(void) {
    puts("gwy_launcher <command> [args]");
    puts("  inspect-mrp <file.mrp>");
    puts("  inspect-member-view --mrp <file.mrp> --member <name> [--to <name>|--profile <json>]");
    puts("  inspect-cfg <cfg.bin> [--index N]");
    puts("  validate --root <mythroad-resolution-root>");
    puts("  scan --root <mythroad-resolution-root>");
    puts("  games --root <root> --vmrp <main.exe> [--cwd <run-dir>]");
    puts("  launch --root <root> --index N --vmrp <main.exe> [--cwd <run-dir>] [--manifest path]");
    puts("  sdk-key [--out <sdk_key.dat>]");
    puts("  vfs-check --root <root> [--overlay <dir>]");
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 1; }
    if (strcmp(argv[1], "inspect-mrp") == 0) {
        if (argc < 3) { usage(); return 1; }
        return cmd_inspect_mrp(argv[2]);
    }
    if (strcmp(argv[1], "inspect-member-view") == 0) {
        const char *mrp = NULL;
        const char *member = NULL;
        const char *to = NULL;
        const char *profile = NULL;
        int i;
        for (i = 2; i + 1 < argc; i++) {
            if (strcmp(argv[i], "--mrp") == 0) mrp = argv[++i];
            else if (strcmp(argv[i], "--member") == 0) member = argv[++i];
            else if (strcmp(argv[i], "--to") == 0) to = argv[++i];
            else if (strcmp(argv[i], "--profile") == 0) profile = argv[++i];
        }
        if (!mrp || !member) { usage(); return 1; }
        return cmd_inspect_member_view(mrp, member, to, profile);
    }
    if (strcmp(argv[1], "inspect-cfg") == 0) {
        uint32_t index = 36;
        int i;
        if (argc < 3) { usage(); return 1; }
        for (i = 3; i + 1 < argc; i++) {
            if (strcmp(argv[i], "--index") == 0) {
                index = (uint32_t)strtoul(argv[i + 1], NULL, 10);
                i++;
            }
        }
        return cmd_inspect_cfg(argv[2], index);
    }
    if (strcmp(argv[1], "validate") == 0) {
        const char *root = NULL;
        int i;
        for (i = 2; i + 1 < argc; i++) {
            if (strcmp(argv[i], "--root") == 0) {
                root = argv[i + 1];
                i++;
            }
        }
        if (!root) root = getenv("GWY_RESOURCE_ROOT");
        if (!root || !root[0]) { usage(); return 1; }
        return cmd_validate_root(root);
    }
    if (strcmp(argv[1], "scan") == 0) {
        const char *root = NULL;
        int i;
        for (i = 2; i + 1 < argc; i++) {
            if (strcmp(argv[i], "--root") == 0) {
                root = argv[i + 1];
                i++;
            }
        }
        if (!root) root = getenv("GWY_RESOURCE_ROOT");
        if (!root || !root[0]) { usage(); return 1; }
        return cmd_scan(root);
    }
    if (strcmp(argv[1], "games") == 0) {
        const char *root = NULL;
        const char *vmrp = NULL;
        const char *cwd = NULL;
        int i;
        for (i = 2; i + 1 < argc; i++) {
            if (strcmp(argv[i], "--root") == 0) { root = argv[++i]; }
            else if (strcmp(argv[i], "--vmrp") == 0) { vmrp = argv[++i]; }
            else if (strcmp(argv[i], "--cwd") == 0) { cwd = argv[++i]; }
        }
        if (!root) root = getenv("GWY_RESOURCE_ROOT");
        if (!root || !vmrp) { usage(); return 1; }
        if (!cwd) cwd = ".";
        return cmd_games(root, vmrp, cwd);
    }
    if (strcmp(argv[1], "launch") == 0) {
        const char *root = NULL;
        const char *vmrp = NULL;
        const char *cwd = NULL;
        const char *manifest = NULL;
        uint32_t index = 36;
        int i;
        for (i = 2; i + 1 < argc; i++) {
            if (strcmp(argv[i], "--root") == 0) { root = argv[++i]; }
            else if (strcmp(argv[i], "--vmrp") == 0) { vmrp = argv[++i]; }
            else if (strcmp(argv[i], "--cwd") == 0) { cwd = argv[++i]; }
            else if (strcmp(argv[i], "--manifest") == 0) { manifest = argv[++i]; }
            else if (strcmp(argv[i], "--index") == 0) {
                index = (uint32_t)strtoul(argv[++i], NULL, 10);
            }
        }
        if (!root) root = getenv("GWY_RESOURCE_ROOT");
        if (!root || !vmrp) { usage(); return 1; }
        if (!cwd) cwd = ".";
        if (!manifest) {
            static char def_manifest[1024];
            snprintf(def_manifest, sizeof(def_manifest), "%s/launch_manifest.json", cwd);
            manifest = def_manifest;
        }
        return cmd_launch(root, index, vmrp, cwd, manifest);
    }
    if (strcmp(argv[1], "sdk-key") == 0) {
        const char *out = NULL;
        int i;
        for (i = 2; i + 1 < argc; i++) {
            if (strcmp(argv[i], "--out") == 0) { out = argv[++i]; }
        }
        return cmd_sdk_key(out);
    }
    if (strcmp(argv[1], "vfs-check") == 0) {
        const char *root = NULL;
        const char *overlay = NULL;
        int i;
        for (i = 2; i + 1 < argc; i++) {
            if (strcmp(argv[i], "--root") == 0) { root = argv[++i]; }
            else if (strcmp(argv[i], "--overlay") == 0) { overlay = argv[++i]; }
        }
        if (!root) root = getenv("GWY_RESOURCE_ROOT");
        if (!root || !root[0]) { usage(); return 1; }
        return cmd_vfs_check(root, overlay);
    }
    usage();
    return 1;
}
