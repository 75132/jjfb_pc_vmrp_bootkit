#include "gwy_launcher/ext_resolver.h"
#include "gwy_launcher/mrp_reg_primary.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <string.h>

void ext_resolver_init(ExtResolver *resolver,
                       const MrpArchive *archive,
                       const CompatibilityProfile *profile) {
    if (!resolver) return;
    resolver->archive = archive;
    resolver->profile = profile;
}

const char *ext_resolve_strategy_name(ExtResolveStrategy s) {
    switch (s) {
    case EXT_RESOLVE_EXACT: return "exact";
    case EXT_RESOLVE_CASE_FOLD: return "case_fold";
    case EXT_RESOLVE_PROFILE_ALIAS: return "profile_alias";
    case EXT_RESOLVE_REG_PRIMARY: return "reg_primary";
    default: return "unknown";
    }
}

const char *gwy_resolve_scope_name(GwyResolveScope s) {
    switch (s) {
    case GWY_RESOLVE_HOST_FILE: return "host_file";
    case GWY_RESOLVE_MRP_MEMBER: return "mrp_member";
    default: return "unknown";
    }
}

static void log_request(const GwyExtResolveRequest *req) {
    if (!req || req->scope != GWY_RESOLVE_MRP_MEMBER) return;
    printf("[MRP_MEMBER_REQUEST] package=%s requested=%s scope=%s\n",
           req->package_guest_path ? req->package_guest_path : "",
           req->requested_member ? req->requested_member : "",
           gwy_resolve_scope_name(req->scope));
    fflush(stdout);
}

static void log_resolve(const GwyExtResolveRequest *req,
                        const ExtResolvedMember *out,
                        const char *result,
                        const char *reason) {
    printf("[EXT_RESOLVE] package=%s requested=%s resolved=%s strategy=%s scope=%s "
           "result=%s%s%s\n",
           req && req->package_guest_path ? req->package_guest_path : "",
           req && req->requested_member ? req->requested_member : "",
           out && out->resolved_name[0] ? out->resolved_name : "",
           out ? ext_resolve_strategy_name(out->strategy) : "none",
           req ? gwy_resolve_scope_name(req->scope) : "?",
           result ? result : "?",
           reason && reason[0] ? " reason=" : "",
           reason && reason[0] ? reason : "");
    fflush(stdout);
}

LauncherStatus ext_resolver_resolve_request(const ExtResolver *resolver,
                                            const GwyExtResolveRequest *request,
                                            ExtResolvedMember *out,
                                            LauncherError *err) {
    const MrpMember *m = NULL;
    LauncherStatus st;
    char alias_to[MRP_MEMBER_NAME_MAX];
    char sha_hex[65];
    const char *pkg_sha;

    launcher_error_clear(err);
    if (!resolver || !resolver->archive || !request || !request->requested_member || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "ext_resolver", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->requested_name, sizeof(out->requested_name), "%s", request->requested_member);
    if (request->package_guest_path) {
        snprintf(out->package_guest_path, sizeof(out->package_guest_path), "%s",
                 request->package_guest_path);
    }

    log_request(request);

    /* HOST_FILE: never apply MRP member aliases (DSM isolation). */
    if (request->scope == GWY_RESOLVE_HOST_FILE) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "ext_resolver",
                           "host_file scope has no MRP member alias", request->requested_member);
        log_resolve(request, out, "REJECT", "wrong_scope_host_file");
        return L_ERR_NOT_FOUND;
    }
    if (request->scope != GWY_RESOLVE_MRP_MEMBER) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "ext_resolver", "bad scope", NULL);
        log_resolve(request, out, "REJECT", "bad_scope");
        return L_ERR_INVALID_ARGUMENT;
    }

    /* 1) exact */
    st = mrp_archive_find_exact(resolver->archive, request->requested_member, &m, err);
    if (st == L_OK) {
        out->member = m;
        snprintf(out->resolved_name, sizeof(out->resolved_name), "%s", m->name);
        out->strategy = EXT_RESOLVE_EXACT;
        log_resolve(request, out, "HIT", NULL);
        return L_OK;
    }

    /* 2) case-fold exact */
    st = mrp_archive_find_casefold(resolver->archive, request->requested_member, &m, err);
    if (st == L_OK) {
        out->member = m;
        snprintf(out->resolved_name, sizeof(out->resolved_name), "%s", m->name);
        out->strategy = EXT_RESOLVE_CASE_FOLD;
        log_resolve(request, out, "HIT", NULL);
        return L_OK;
    }

    /* 3) reg.ext primary for logical cfunction.ext (CROSS_TARGET shell/game). */
    if (strcmp(request->requested_member, "cfunction.ext") == 0) {
        char primary[MRP_MEMBER_NAME_MAX];
        if (mrp_archive_find_reg_primary(resolver->archive, primary, sizeof(primary))) {
            st = mrp_archive_find_exact(resolver->archive, primary, &m, err);
            if (st == L_OK) {
                out->member = m;
                snprintf(out->resolved_name, sizeof(out->resolved_name), "%s", m->name);
                out->strategy = EXT_RESOLVE_REG_PRIMARY;
                log_resolve(request, out, "HIT", "reg_ext_primary");
                return L_OK;
            }
        }
    }

    /* 4) profile alias only after exact miss */
    if (!resolver->profile) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "ext_resolver", "member resolve MISS",
                           request->requested_member);
        log_resolve(request, out, "MISS", "no_profile");
        return L_ERR_NOT_FOUND;
    }

    gwy_sha256_hex(resolver->archive->sha256, sha_hex);
    pkg_sha = request->package_sha256 && request->package_sha256[0]
                  ? request->package_sha256
                  : sha_hex;

    if (!compatibility_profile_matches_request(resolver->profile,
                                               request->package_guest_path,
                                               request->package_appid
                                                   ? request->package_appid
                                                   : resolver->archive->appid_le,
                                               request->package_appver
                                                   ? request->package_appver
                                                   : resolver->archive->appver_le,
                                               pkg_sha)) {
        launcher_error_set(err, L_ERR_PROFILE_MISMATCH, "ext_resolver",
                           "profile does not match archive", request->requested_member);
        log_resolve(request, out, "REJECT", "package_identity_mismatch");
        return L_ERR_PROFILE_MISMATCH;
    }

    if (!compatibility_profile_find_member_alias(resolver->profile, request->requested_member,
                                                 alias_to, sizeof(alias_to))) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "ext_resolver", "member resolve MISS",
                           request->requested_member);
        log_resolve(request, out, "MISS", NULL);
        return L_ERR_NOT_FOUND;
    }

    st = mrp_archive_find_exact(resolver->archive, alias_to, &m, err);
    if (st != L_OK) {
        launcher_error_set(err, L_ERR_PROFILE_MISMATCH, "ext_resolver",
                           "alias target missing", alias_to);
        log_resolve(request, out, "REJECT", "alias_target_missing");
        return L_ERR_PROFILE_MISMATCH;
    }
    out->member = m;
    snprintf(out->resolved_name, sizeof(out->resolved_name), "%s", m->name);
    out->strategy = EXT_RESOLVE_PROFILE_ALIAS;
    snprintf(out->profile_id, sizeof(out->profile_id), "%s", resolver->profile->id);
    log_resolve(request, out, "HIT", NULL);
    printf("[EXT_RESOLVE] profile=%s\n", out->profile_id);
    fflush(stdout);
    return L_OK;
}

LauncherStatus ext_resolver_resolve(const ExtResolver *resolver,
                                    const char *requested_name,
                                    ExtResolvedMember *out,
                                    LauncherError *err) {
    GwyExtResolveRequest req;
    char sha_hex[65];
    if (!resolver || !resolver->archive) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "ext_resolver", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    gwy_sha256_hex(resolver->archive->sha256, sha_hex);
    memset(&req, 0, sizeof(req));
    req.scope = GWY_RESOLVE_MRP_MEMBER;
    req.package_guest_path = resolver->archive->path;
    req.package_sha256 = sha_hex;
    req.package_appid = resolver->archive->appid_le;
    req.package_appver = resolver->archive->appver_le;
    req.requested_member = requested_name;
    return ext_resolver_resolve_request(resolver, &req, out, err);
}
