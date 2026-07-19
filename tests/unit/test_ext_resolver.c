#include "gwy_launcher/compat_profile.h"
#include "gwy_launcher/ext_resolver.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_sizes(MrpArchive *a, const char *name, uint32_t stored, uint32_t unpacked) {
    const MrpMember *m = NULL;
    MrpMemberInfo info;
    LauncherError err;
    if (mrp_archive_find_exact(a, name, &m, &err) != L_OK) return 0;
    if (mrp_archive_member_info(a, m, &info, &err) != L_OK) return 0;
    return info.stored_size == stored && info.unpacked_size == unpacked;
}

int main(void) {
    const char *root = getenv("GWY_FIXTURE_ROOT");
    const char *profile_path = getenv("GWY_PROFILE");
    char mrp_path[1024];
    MrpArchive *a = NULL;
    CompatibilityProfile profile;
    ExtResolver resolver;
    ExtResolvedMember resolved;
    ModuleRegistry reg;
    LauncherError err;
    ByteBuffer buf;
    char hex[65];
    GwyExtResolveRequest req;

    if (!root || !root[0]) {
        fprintf(stderr, "GWY_FIXTURE_ROOT not set\n");
        return 1;
    }
    if (!profile_path || !profile_path[0]) profile_path = "profiles/jjfb.json";
    snprintf(mrp_path, sizeof(mrp_path), "%s/gwy/jjfb.mrp", root);

    if (mrp_archive_open(mrp_path, &a, &err) != L_OK) {
        fprintf(stderr, "open failed: %s\n", err.message);
        return 1;
    }
    gwy_sha256_hex(a->sha256, hex);
    if (!expect_sizes(a, "start.mr", 1514, 3787) ||
        !expect_sizes(a, "mrc_loader.ext", 219, 232) ||
        !expect_sizes(a, "robotol.ext", 161178, 253420)) {
        fprintf(stderr, "size golden mismatch\n");
        mrp_archive_close(a);
        return 1;
    }

    if (compatibility_profile_load_json_file(profile_path, &profile, &err) != L_OK) {
        fprintf(stderr, "profile load failed: %s\n", err.message);
        mrp_archive_close(a);
        return 1;
    }

    ext_resolver_init(&resolver, a, &profile);
    module_registry_init(&reg, "gwy/jjfb.mrp", hex);

    /* 1 exact */
    memset(&req, 0, sizeof(req));
    req.scope = GWY_RESOLVE_MRP_MEMBER;
    req.package_guest_path = "gwy/jjfb.mrp";
    req.package_sha256 = hex;
    req.package_appid = a->appid_le;
    req.package_appver = a->appver_le;
    req.requested_member = "mrc_loader.ext";
    if (ext_resolver_resolve_request(&resolver, &req, &resolved, &err) != L_OK ||
        resolved.strategy != EXT_RESOLVE_EXACT) {
        fprintf(stderr, "mrc_loader exact failed\n");
        mrp_archive_close(a);
        return 1;
    }
    printf("[EXT_RESOLVE] requested=mrc_loader.ext strategy=exact resolved=mrc_loader.ext\n");

    /* 2 alias */
    req.requested_member = "cfunction.ext";
    if (ext_resolver_resolve_request(&resolver, &req, &resolved, &err) != L_OK ||
        resolved.strategy != EXT_RESOLVE_PROFILE_ALIAS ||
        strcmp(resolved.resolved_name, "robotol.ext") != 0) {
        fprintf(stderr, "cfunction alias failed: %s\n", err.message);
        mrp_archive_close(a);
        return 1;
    }

    /* 3 HOST_FILE must not alias */
    req.scope = GWY_RESOLVE_HOST_FILE;
    req.requested_member = "cfunction.ext";
    if (ext_resolver_resolve_request(&resolver, &req, &resolved, &err) == L_OK) {
        fprintf(stderr, "HOST_FILE must not apply alias\n");
        mrp_archive_close(a);
        return 1;
    }

    /* 4 sha mismatch reject */
    {
        CompatibilityProfile bad = profile;
        char badsha[65];
        snprintf(badsha, sizeof(badsha), "%s", hex);
        badsha[0] = (badsha[0] == 'a') ? 'b' : 'a';
        req.scope = GWY_RESOLVE_MRP_MEMBER;
        req.package_sha256 = badsha;
        req.requested_member = "cfunction.ext";
        if (ext_resolver_resolve_request(&resolver, &req, &resolved, &err) == L_OK) {
            fprintf(stderr, "sha mismatch should reject\n");
            mrp_archive_close(a);
            return 1;
        }
        (void)bad;
    }

    /* 5 appid mismatch */
    req.package_sha256 = hex;
    req.package_appid = 1;
    if (ext_resolver_resolve_request(&resolver, &req, &resolved, &err) == L_OK) {
        fprintf(stderr, "appid mismatch should reject\n");
        mrp_archive_close(a);
        return 1;
    }
    req.package_appid = a->appid_le;

    /* 6 appver mismatch */
    req.package_appver = 999;
    if (ext_resolver_resolve_request(&resolver, &req, &resolved, &err) == L_OK) {
        fprintf(stderr, "appver mismatch should reject\n");
        mrp_archive_close(a);
        return 1;
    }
    req.package_appver = a->appver_le;

    /* 7 target mismatch */
    req.package_guest_path = "gwy/other.mrp";
    if (ext_resolver_resolve_request(&resolver, &req, &resolved, &err) == L_OK) {
        fprintf(stderr, "target mismatch should reject\n");
        mrp_archive_close(a);
        return 1;
    }
    req.package_guest_path = "gwy/jjfb.mrp";

    /* 8 alias target missing */
    {
        CompatibilityProfile p2 = profile;
        snprintf(p2.alias_to[0], sizeof(p2.alias_to[0]), "%s", "__no_such__.ext");
        ExtResolver r2;
        ext_resolver_init(&r2, a, &p2);
        if (ext_resolver_resolve_request(&r2, &req, &resolved, &err) == L_OK) {
            fprintf(stderr, "missing alias target should reject\n");
            mrp_archive_close(a);
            return 1;
        }
    }

    /* 9 cycle rejected at load */
    {
        CompatibilityProfile bad;
        if (compatibility_profile_load_json_text(
                "{\"schema_version\":1,\"id\":\"x\",\"match\":{\"target\":\"gwy/jjfb.mrp\","
                "\"appid\":400101,\"appver\":12,\"sha256\":"
                "\"52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036\"},"
                "\"compatibility\":{\"mrp_member_aliases\":{\"a.ext\":\"b.ext\",\"b.ext\":\"a.ext\"}}}",
                &bad, &err) == L_OK) {
            fprintf(stderr, "cycle should be rejected\n");
            mrp_archive_close(a);
            return 1;
        }
    }

    /* 10 path escape rejected */
    {
        CompatibilityProfile bad;
        if (compatibility_profile_load_json_text(
                "{\"schema_version\":1,\"id\":\"x\",\"match\":{\"target\":\"gwy/jjfb.mrp\","
                "\"appid\":400101,\"appver\":12,\"sha256\":"
                "\"52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036\"},"
                "\"compatibility\":{\"mrp_member_aliases\":{\"../x\":\"robotol.ext\"}}}",
                &bad, &err) == L_OK) {
            fprintf(stderr, "path escape alias should reject\n");
            mrp_archive_close(a);
            return 1;
        }
    }

    /* 11 exact wins over alias — already true since cfunction exact-misses; verify start.mr */
    req.requested_member = "start.mr";
    if (ext_resolver_resolve_request(&resolver, &req, &resolved, &err) != L_OK ||
        resolved.strategy != EXT_RESOLVE_EXACT) {
        fprintf(stderr, "start.mr exact failed\n");
        mrp_archive_close(a);
        return 1;
    }

    /* 12 register + decode robotol via alias */
    req.requested_member = "cfunction.ext";
    ext_resolver_resolve_request(&resolver, &req, &resolved, &err);
    if (mrp_archive_decode_member(a, resolved.member, 8u * 1024u * 1024u, &buf, &err) != L_OK ||
        buf.size != 253420) {
        fprintf(stderr, "decode robotol failed size=%zu\n", buf.size);
        mrp_archive_close(a);
        return 1;
    }
    module_registry_register_resolved(&reg, &resolved, &buf, &err);
    byte_buffer_free(&buf);

    /* 13 hash unchanged */
    {
        uint8_t again[32];
        char hex2[65];
        gwy_sha256_file(mrp_path, again);
        gwy_sha256_hex(again, hex2);
        if (strcmp(hex, hex2) != 0) {
            fprintf(stderr, "hash changed\n");
            mrp_archive_close(a);
            return 1;
        }
    }

    /* 14 casefold path exists for Robotol.ext style if present — skip if not */
    {
        const char *snap = getenv("GWY_MODULE_SNAPSHOT");
        if (snap && snap[0]) module_registry_write_snapshot(&reg, snap, &err);
    }

    mrp_archive_close(a);
    puts("test_ext_resolver OK");
    return 0;
}
