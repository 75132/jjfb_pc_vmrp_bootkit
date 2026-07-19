#include "gwy_launcher/mrp_member_view.h"
#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/ext_resolver.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/mrp_reg_primary.h"
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

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

static void ensure_parent_dir(const char *file_path) {
#ifdef _WIN32
    char dir[1024];
    size_t n = strlen(file_path);
    size_t i;
    if (n >= sizeof(dir)) return;
    memcpy(dir, file_path, n + 1);
    for (i = n; i > 0; i--) {
        if (dir[i - 1] == '/' || dir[i - 1] == '\\') {
            dir[i - 1] = '\0';
            break;
        }
    }
    if (i == 0 || !dir[0]) return;
    CreateDirectoryA(dir, NULL);
#else
    (void)file_path;
#endif
}

static LauncherStatus write_bytes(const char *path, const uint8_t *data, size_t size,
                                  LauncherError *err) {
    FILE *fp;
    ensure_parent_dir(path);
    fp = fopen(path, "wb");
    if (!fp) {
        launcher_error_set(err, L_ERR_IO, "mrp_member_view", "write open failed", path);
        return L_ERR_IO;
    }
    if (fwrite(data, 1, size, fp) != size) {
        fclose(fp);
        launcher_error_set(err, L_ERR_IO, "mrp_member_view", "write short", path);
        return L_ERR_IO;
    }
    fclose(fp);
    return L_OK;
}

/*
 * Build archive bytes with an extra index entry that aliases `from_name` to the
 * same stored offset/size as `to_member`. Shifts data region by entry size.
 *
 * insert_at = first_data_offset + 12 (guest index end). Inserting at
 * first_data_offset alone tears the last entry meta and causes LOOKUP 3004.
 */
LauncherStatus mrp_member_view_synthesize_alias(const MrpArchive *archive,
                                                const char *from_name,
                                                const MrpMember *to_member,
                                                uint8_t **out_data,
                                                size_t *out_size,
                                                LauncherError *err) {
    size_t name_bytes;
    size_t entry_size;
    size_t new_size;
    uint8_t *buf;
    uint32_t header_length;
    uint32_t first_data;
    uint32_t first_data_plus4;
    uint32_t old_total;
    size_t pos;
    size_t insert_at;
    size_t index_end;

    launcher_error_clear(err);
    if (!archive || !from_name || !to_member || !out_data || !out_size) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "mrp_member_view", "null", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    name_bytes = strlen(from_name) + 1; /* include NUL like typical MRP index */
    entry_size = 4 + name_bytes + 12;
    header_length = archive->header_length;
    first_data = archive->first_data_offset;
    if ((size_t)first_data + 12 > archive->size) {
        launcher_error_set(err, L_ERR_FORMAT, "mrp_member_view", "index end past archive", NULL);
        return L_ERR_FORMAT;
    }
    index_end = (size_t)first_data + 12;
    insert_at = index_end;
    new_size = archive->size + entry_size;
    buf = (uint8_t *)malloc(new_size);
    if (!buf) {
        launcher_error_set(err, L_ERR_IO, "mrp_member_view", "oom", NULL);
        return L_ERR_IO;
    }

    memcpy(buf, archive->data, insert_at);
    wr_u32_le(buf + insert_at, (uint32_t)name_bytes);
    memcpy(buf + insert_at + 4, from_name, name_bytes);
    wr_u32_le(buf + insert_at + 4 + name_bytes, to_member->offset + (uint32_t)entry_size);
    wr_u32_le(buf + insert_at + 4 + name_bytes + 4, to_member->stored_size);
    wr_u32_le(buf + insert_at + 4 + name_bytes + 8, to_member->reserved);
    memcpy(buf + insert_at + entry_size, archive->data + insert_at, archive->size - insert_at);

    first_data_plus4 = rd_u32_le(buf + 4);
    wr_u32_le(buf + 4, first_data_plus4 + (uint32_t)entry_size);

    old_total = rd_u32_le(buf + 8);
    wr_u32_le(buf + 8, old_total + (uint32_t)entry_size);

    pos = header_length;
    while (pos + 4 <= insert_at) {
        uint32_t name_len = rd_u32_le(buf + pos);
        uint32_t off;
        pos += 4;
        if (name_len < 1 || pos + name_len + 12 > insert_at) break;
        pos += name_len;
        off = rd_u32_le(buf + pos);
        wr_u32_le(buf + pos, off + (uint32_t)entry_size);
        pos += 12;
    }

    printf("[MRP_VIEW_LOOKUP] requested=%s resolved=%s insert_at=%zu entry_size=%zu "
           "index_end=%zu offset=%u stored_size=%u result=HIT\n",
           from_name, to_member->name, insert_at, entry_size, index_end,
           to_member->offset + (uint32_t)entry_size, to_member->stored_size);
    fflush(stdout);

    *out_data = buf;
    *out_size = new_size;
    return L_OK;
}

static LauncherStatus register_one(ModuleRegistry *reg,
                                   ExtResolver *resolver,
                                   const char *package_guest,
                                   const char *sha_hex,
                                   uint32_t appid,
                                   uint32_t appver,
                                   const char *requested,
                                   int decode,
                                   LauncherError *err) {
    GwyExtResolveRequest req;
    ExtResolvedMember resolved;
    ByteBuffer buf;
    LauncherStatus st;

    memset(&req, 0, sizeof(req));
    req.scope = GWY_RESOLVE_MRP_MEMBER;
    req.package_guest_path = package_guest;
    req.package_sha256 = sha_hex;
    req.package_appid = appid;
    req.package_appver = appver;
    req.requested_member = requested;

    module_registry_discover(reg, requested, err);
    st = ext_resolver_resolve_request(resolver, &req, &resolved, err);
    if (st != L_OK) return st;

    if (decode) {
        st = mrp_archive_decode_member(resolver->archive, resolved.member,
                                       8u * 1024u * 1024u, &buf, err);
        if (st != L_OK) return st;
        st = module_registry_register_resolved(reg, &resolved, &buf, err);
        byte_buffer_free(&buf);
        return st;
    }
    return module_registry_register_resolved(reg, &resolved, NULL, err);
}

LauncherStatus mrp_member_view_install(GuestVfs *vfs,
                                       const CompatibilityProfile *profile,
                                       const char *canonical_host_mrp,
                                       const char *package_guest_path,
                                       const char *view_host_path,
                                       ModuleRegistry *reg_opt,
                                       LauncherError *err) {
    MrpArchive *archive = NULL;
    ExtResolver resolver;
    ExtResolvedMember resolved;
    GwyExtResolveRequest req;
    char sha_hex[65];
    LauncherStatus st;
    size_t i;
    int need_view = 0;
    uint8_t *view_bytes = NULL;
    size_t view_size = 0;
    ModuleRegistry local_reg;
    ModuleRegistry *reg = reg_opt;

    launcher_error_clear(err);
    if (!vfs || !profile || !canonical_host_mrp || !package_guest_path || !view_host_path) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "mrp_member_view", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }

    st = mrp_archive_open(canonical_host_mrp, &archive, err);
    if (st != L_OK) return st;
    gwy_sha256_hex(archive->sha256, sha_hex);

    if (!compatibility_profile_matches_request(profile, package_guest_path, archive->appid_le,
                                               archive->appver_le, sha_hex)) {
        mrp_archive_close(archive);
        launcher_error_set(err, L_ERR_PROFILE_MISMATCH, "mrp_member_view",
                           "profile does not match package", package_guest_path);
        printf("[EXT_RESOLVE] package=%s result=REJECT reason=package_identity_mismatch\n",
               package_guest_path);
        fflush(stdout);
        return L_ERR_PROFILE_MISMATCH;
    }

    if (!reg) {
        module_registry_init(&local_reg, package_guest_path, sha_hex);
        reg = &local_reg;
    } else {
        module_registry_init(reg, package_guest_path, sha_hex);
    }

    ext_resolver_init(&resolver, archive, profile);

    /* Record start.mr / mrc_loader.ext / each alias source. */
    register_one(reg, &resolver, package_guest_path, sha_hex, archive->appid_le,
                 archive->appver_le, "start.mr", 1, err);
    register_one(reg, &resolver, package_guest_path, sha_hex, archive->appid_le,
                 archive->appver_le, "mrc_loader.ext", 1, err);

    for (i = 0; i < profile->alias_count; i++) {
        const char *from = profile->alias_from[i];
        const MrpMember *exact = NULL;
        /* exact wins: if member already exists, do not synthesize */
        if (mrp_archive_find_exact(archive, from, &exact, err) == L_OK) {
            register_one(reg, &resolver, package_guest_path, sha_hex, archive->appid_le,
                         archive->appver_le, from, 1, err);
            continue;
        }
        memset(&req, 0, sizeof(req));
        req.scope = GWY_RESOLVE_MRP_MEMBER;
        req.package_guest_path = package_guest_path;
        req.package_sha256 = sha_hex;
        req.package_appid = archive->appid_le;
        req.package_appver = archive->appver_le;
        req.requested_member = from;
        st = ext_resolver_resolve_request(&resolver, &req, &resolved, err);
        if (st != L_OK) {
            mrp_archive_close(archive);
            return st;
        }
        if (resolved.strategy == EXT_RESOLVE_PROFILE_ALIAS) {
            ByteBuffer buf;
            need_view = 1;
            module_registry_discover(reg, from, err);
            if (mrp_archive_decode_member(archive, resolved.member, 8u * 1024u * 1024u, &buf,
                                          err) != L_OK) {
                mrp_archive_close(archive);
                return L_ERR_DECOMPRESSION;
            }
            module_registry_register_resolved(reg, &resolved, &buf, err);
            byte_buffer_free(&buf);

            if (!view_bytes) {
                st = mrp_member_view_synthesize_alias(archive, from, resolved.member, &view_bytes,
                                                      &view_size, err);
                if (st != L_OK) {
                    mrp_archive_close(archive);
                    return st;
                }
            } else {
                free(view_bytes);
                view_bytes = NULL;
                st = mrp_member_view_synthesize_alias(archive, from, resolved.member, &view_bytes,
                                                      &view_size, err);
                if (st != L_OK) {
                    mrp_archive_close(archive);
                    return st;
                }
            }
            printf("[MRP_VIEW_DECODE] member=%s stored_size=%u unpacked_size=%u result=OK "
                   "sha_source=robotol.ext\n",
                   from, resolved.member->stored_size,
                   resolved.member->unpacked_known ? resolved.member->unpacked_size : 0);
            fflush(stdout);
        }
    }

    if (need_view && view_bytes) {
        int skip_vfs_remap = 0;
        const char *dis = getenv("JJFB_DISABLE_JJFB_ALIAS_DIRECT");
        const char *mode = getenv("JJFB_GWY_LAUNCHER_MODE");
        st = write_bytes(view_host_path, view_bytes, view_size, err);
        free(view_bytes);
        view_bytes = NULL;
        if (st != L_OK) {
            mrp_archive_close(archive);
            return st;
        }
        /* Phase 6G: seed registry/view file, but do not remap guest package → jjfb_alias. */
        if ((dis && dis[0] == '1' && dis[1] == '\0') ||
            (mode && mode[0] == '1' && mode[1] == '\0')) {
            skip_vfs_remap = 1;
        }
        if (!skip_vfs_remap) {
            /* Present view under guest package path; identity sha remains original. */
            st = guest_vfs_set_generated(vfs, package_guest_path, view_host_path, sha_hex, err);
            if (st != L_OK) {
                mrp_archive_close(archive);
                return st;
            }
            /* Common mythroad/ prefix form */
            {
                char myth[512];
                snprintf(myth, sizeof(myth), "mythroad/%s", package_guest_path);
                guest_vfs_set_generated(vfs, myth, view_host_path, sha_hex, err);
            }
            printf("[MRP_MEMBER_VIEW] installed package=%s view=%s original_sha256=%s\n",
                   package_guest_path, view_host_path, sha_hex);
        } else {
            printf("[MRP_MEMBER_VIEW] registry_seeded package=%s view=%s original_sha256=%s "
                   "vfs_remap=disabled reason=JJFB_DISABLE_JJFB_ALIAS_DIRECT\n",
                   package_guest_path, view_host_path, sha_hex);
        }
        fflush(stdout);
    }

    {
        const char *snap = getenv("GWY_MODULE_SNAPSHOT");
        if (snap && snap[0]) {
            module_registry_write_snapshot(reg, snap, err);
        }
    }

    mrp_archive_close(archive);
    return L_OK;
}

LauncherStatus mrp_member_view_install_reg_primary(GuestVfs *vfs,
                                                   const char *canonical_host_mrp,
                                                   const char *package_guest_path,
                                                   const char *view_host_path,
                                                   ModuleRegistry *reg_opt,
                                                   LauncherError *err) {
    MrpArchive *archive = NULL;
    const MrpMember *primary_m = NULL;
    char primary[MRP_MEMBER_NAME_MAX];
    char sha_hex[65];
    uint8_t *view_bytes = NULL;
    size_t view_size = 0;
    LauncherStatus st;
    ModuleRegistry local_reg;
    ModuleRegistry *reg = reg_opt;

    launcher_error_clear(err);
    if (!vfs || !canonical_host_mrp || !package_guest_path || !view_host_path) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "mrp_member_view", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }

    st = mrp_archive_open(canonical_host_mrp, &archive, err);
    if (st != L_OK) return st;

    /* Exact cfunction.ext already present — nothing to synthesize. */
    {
        const MrpMember *exact = NULL;
        if (mrp_archive_find_exact(archive, "cfunction.ext", &exact, err) == L_OK) {
            printf("[MRP_MEMBER_VIEW] reg_primary_skip package=%s reason=exact_cfunction_present\n",
                   package_guest_path);
            fflush(stdout);
            mrp_archive_close(archive);
            return L_OK;
        }
    }

    if (!mrp_archive_find_reg_primary(archive, primary, sizeof(primary))) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "mrp_member_view", "no reg.ext primary",
                           package_guest_path);
        mrp_archive_close(archive);
        return L_ERR_NOT_FOUND;
    }
    st = mrp_archive_find_exact(archive, primary, &primary_m, err);
    if (st != L_OK) {
        mrp_archive_close(archive);
        return st;
    }

    st = mrp_member_view_synthesize_alias(archive, "cfunction.ext", primary_m, &view_bytes,
                                          &view_size, err);
    if (st != L_OK) {
        mrp_archive_close(archive);
        return st;
    }

    gwy_sha256_hex(archive->sha256, sha_hex);
    st = write_bytes(view_host_path, view_bytes, view_size, err);
    free(view_bytes);
    view_bytes = NULL;
    if (st != L_OK) {
        mrp_archive_close(archive);
        return st;
    }

    /* Always remap for shell/reg-primary — guest _mr_readFile needs index HIT. */
    st = guest_vfs_set_generated(vfs, package_guest_path, view_host_path, sha_hex, err);
    if (st != L_OK) {
        mrp_archive_close(archive);
        return st;
    }
    {
        char myth[512];
        snprintf(myth, sizeof(myth), "mythroad/%s", package_guest_path);
        guest_vfs_set_generated(vfs, myth, view_host_path, sha_hex, err);
    }

    if (!reg) {
        module_registry_init(&local_reg, package_guest_path, "reg_primary_view");
        reg = &local_reg;
    }
    {
        ExtResolver resolver;
        ExtResolvedMember resolved;
        GwyExtResolveRequest req;
        ByteBuffer buf;
        ext_resolver_init(&resolver, archive, NULL);
        memset(&req, 0, sizeof(req));
        req.scope = GWY_RESOLVE_MRP_MEMBER;
        req.package_guest_path = package_guest_path;
        req.package_sha256 = sha_hex;
        req.package_appid = archive->appid_le;
        req.package_appver = archive->appver_le;
        /* Register under primary member name (unique) so CODE_IMAGE size-match
         * is not overwritten by later shell packages' cfunction.ext rows. */
        req.requested_member = primary;
        if (ext_resolver_resolve_request(&resolver, &req, &resolved, err) == L_OK) {
            memset(&buf, 0, sizeof(buf));
            if (mrp_archive_decode_member(archive, resolved.member, 8u * 1024u * 1024u, &buf,
                                          err) == L_OK) {
                module_registry_register_resolved(reg, &resolved, &buf, err);
                byte_buffer_free(&buf);
            } else {
                module_registry_register_resolved(reg, &resolved, NULL, err);
            }
        }
    }

    printf("[MRP_MEMBER_VIEW] reg_primary_installed package=%s primary=%s view=%s "
           "original_sha256=%s vfs_remap=enabled evidence=CROSS_TARGET\n",
           package_guest_path, primary, view_host_path, sha_hex);
    fflush(stdout);

    mrp_archive_close(archive);
    return L_OK;
}
