#include "gwy_launcher/launch_descriptor.h"
#include "gwy_launcher/gwy_cfg.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <string.h>

int launch_descriptor_validate_basic(const LaunchDescriptor *desc) {
    if (desc == NULL) return 0;
    if (desc->resource_root[0] == '\0') return 0;
    if (desc->target_mrp[0] == '\0') return 0;
    if (desc->entry_member[0] == '\0') return 0;
    if (strstr(desc->target_mrp, "..") != NULL) return 0;
    if (desc->param[0] == '\0') return 0;
    return 1;
}

LauncherStatus launch_param_serialize(const LaunchDescriptor *desc,
                                      char *out,
                                      size_t out_cap,
                                      LauncherError *err) {
    int n;
    launcher_error_clear(err);
    if (!desc || !out || out_cap == 0) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "launch_param", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    /* Explicit field order (TARGET_OBSERVED / CROSS_TARGET launch contract). */
    n = snprintf(out, out_cap,
                 "napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s_gwyblink",
                 (int)desc->napptype, (int)desc->nextid, (int)desc->ncode,
                 (int)desc->narg, (int)desc->narg1,
                 desc->nmrpname[0] ? desc->nmrpname : desc->target_mrp);
    if (n < 0 || (size_t)n >= out_cap) {
        launcher_error_set(err, L_ERR_BOUNDS, "launch_param", "param too long", NULL);
        return L_ERR_BOUNDS;
    }
    return L_OK;
}

static int hex_eq_ci(const char *a, const char *b) {
    size_t i;
    for (i = 0; i < 64; i++) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'F') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'F') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return a[64] == '\0' && b[64] == '\0';
}

LauncherStatus launch_descriptor_build(const char *resource_root,
                                       uint32_t cfg_index,
                                       const char *profile_id,
                                       const LaunchExpectations *expect,
                                       LaunchDescriptor *out,
                                       LauncherError *err) {
    char cfg_path[GL_PATH_MAX];
    char mrp_path[GL_PATH_MAX];
    GwyCfgFile *cfg = NULL;
    GwyCfgRecord rec;
    MrpArchive *mrp = NULL;
    const MrpMember *entry = NULL;
    LauncherStatus st;

    launcher_error_clear(err);
    if (!resource_root || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "launch_descriptor", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->profile_id, sizeof(out->profile_id), "%s", profile_id ? profile_id : "cfg");
    snprintf(out->resource_root, sizeof(out->resource_root), "%s", resource_root);
    out->cfg_index = cfg_index;
    snprintf(out->entry_member, sizeof(out->entry_member), "%s", "start.mr");

    if (snprintf(cfg_path, sizeof(cfg_path), "%s/gwy/cfg.bin", resource_root) >= (int)sizeof(cfg_path)) {
        launcher_error_set(err, L_ERR_BOUNDS, "launch_descriptor", "cfg path too long", resource_root);
        return L_ERR_BOUNDS;
    }
    st = gwy_cfg_open(cfg_path, &cfg, err);
    if (st != L_OK) return st;
    st = gwy_cfg_read_record(cfg, cfg_index, &rec, err);
    if (st != L_OK) {
        gwy_cfg_close(cfg);
        return st;
    }
    if (!rec.target_mrp.present || !rec.target_mrp.value[0]) {
        gwy_cfg_close(cfg);
        launcher_error_set(err, L_ERR_FORMAT, "launch_descriptor", "cfg record has no target", cfg_path);
        return L_ERR_FORMAT;
    }

    snprintf(out->target_mrp, sizeof(out->target_mrp), "%s", rec.target_mrp.value);
    snprintf(out->nmrpname, sizeof(out->nmrpname), "%s", rec.target_mrp.value);
    out->napptype = rec.napptype.present ? (int32_t)rec.napptype.value : 0;
    out->nextid = rec.nextid.present ? (int32_t)rec.nextid.value : 0;
    out->ncode = rec.ncode.present ? (int32_t)rec.ncode.value : 0;
    out->narg = rec.narg.present ? (int32_t)rec.narg.value : 0;
    out->narg1 = rec.narg1.present ? (int32_t)rec.narg1.value : 0;
    gwy_cfg_close(cfg);

    if (expect) {
        if (expect->has_target && strcmp(expect->target_mrp, out->target_mrp) != 0) {
            launcher_error_set(err, L_ERR_PROFILE_MISMATCH, "launch_descriptor",
                               "target mismatch vs expectation", out->target_mrp);
            return L_ERR_PROFILE_MISMATCH;
        }
        if (expect->has_entry) {
            snprintf(out->entry_member, sizeof(out->entry_member), "%s", expect->entry_member);
        }
    }

    if (snprintf(mrp_path, sizeof(mrp_path), "%s/%s", resource_root, out->target_mrp) >= (int)sizeof(mrp_path)) {
        launcher_error_set(err, L_ERR_BOUNDS, "launch_descriptor", "mrp path too long", out->target_mrp);
        return L_ERR_BOUNDS;
    }
    st = mrp_archive_open(mrp_path, &mrp, err);
    if (st != L_OK) return st;
    out->appid = mrp->appid_le;
    out->appver = mrp->appver_le;
    memcpy(out->target_sha256, mrp->sha256, 32);
    gwy_sha256_hex(mrp->sha256, out->target_sha256_hex);

    st = mrp_archive_find_exact(mrp, out->entry_member, &entry, err);
    if (st != L_OK) {
        mrp_archive_close(mrp);
        return st;
    }
    mrp_archive_close(mrp);

    if (expect) {
        if (expect->has_sha256 && !hex_eq_ci(expect->sha256_hex, out->target_sha256_hex)) {
            launcher_error_set(err, L_ERR_HASH_MISMATCH, "launch_descriptor",
                               "sha256 mismatch vs expectation", out->target_sha256_hex);
            return L_ERR_HASH_MISMATCH;
        }
        if (expect->has_appid && expect->appid != out->appid) {
            launcher_error_set(err, L_ERR_PROFILE_MISMATCH, "launch_descriptor",
                               "appid mismatch", out->target_mrp);
            return L_ERR_PROFILE_MISMATCH;
        }
        if (expect->has_appver && expect->appver != out->appver) {
            launcher_error_set(err, L_ERR_PROFILE_MISMATCH, "launch_descriptor",
                               "appver mismatch", out->target_mrp);
            return L_ERR_PROFILE_MISMATCH;
        }
    }

    st = launch_param_serialize(out, out->param, sizeof(out->param), err);
    if (st != L_OK) return st;
    if (!launch_descriptor_validate_basic(out)) {
        launcher_error_set(err, L_ERR_STATE, "launch_descriptor", "descriptor failed basic validate", NULL);
        return L_ERR_STATE;
    }
    return L_OK;
}

LauncherStatus launch_manifest_write(const char *path,
                                     const LaunchDescriptor *desc,
                                     LauncherError *err) {
    FILE *fp;
    launcher_error_clear(err);
    if (!path || !desc) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "launch_manifest", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        launcher_error_set(err, L_ERR_IO, "launch_manifest", "failed to open", path);
        return L_ERR_IO;
    }
    fprintf(fp,
            "{\n"
            "  \"profile_id\": \"%s\",\n"
            "  \"resource_root\": \"%s\",\n"
            "  \"cfg_index\": %u,\n"
            "  \"target_mrp\": \"%s\",\n"
            "  \"entry_member\": \"%s\",\n"
            "  \"param\": \"%s\",\n"
            "  \"napptype\": %d,\n"
            "  \"nextid\": %d,\n"
            "  \"ncode\": %d,\n"
            "  \"narg\": %d,\n"
            "  \"narg1\": %d,\n"
            "  \"appid\": %u,\n"
            "  \"appver\": %u,\n"
            "  \"target_sha256\": \"%s\"\n"
            "}\n",
            desc->profile_id, desc->resource_root, desc->cfg_index, desc->target_mrp,
            desc->entry_member, desc->param, (int)desc->napptype, (int)desc->nextid,
            (int)desc->ncode, (int)desc->narg, (int)desc->narg1, desc->appid, desc->appver,
            desc->target_sha256_hex);
    fclose(fp);
    return L_OK;
}

LauncherStatus launch_profile_reject_dangerous(const char *json_text, LauncherError *err) {
    launcher_error_clear(err);
    if (!json_text) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "launch_profile", "null json", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (strstr(json_text, "\"guest_address\"") || strstr(json_text, "\"erw_offset\"") ||
        strstr(json_text, "\"guest_pc\"") || strstr(json_text, "\"force_ui_mode\"")) {
        launcher_error_set(err, L_ERR_PROFILE_MISMATCH, "launch_profile",
                           "dangerous profile field rejected", NULL);
        return L_ERR_PROFILE_MISMATCH;
    }
    return L_OK;
}
