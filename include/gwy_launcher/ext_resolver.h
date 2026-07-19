#ifndef GWY_LAUNCHER_EXT_RESOLVER_H
#define GWY_LAUNCHER_EXT_RESOLVER_H

#include "gwy_launcher/compat_profile.h"
#include "gwy_launcher/error.h"
#include "gwy_launcher/mrp_archive.h"

typedef enum GwyResolveScope {
    GWY_RESOLVE_HOST_FILE = 1,
    GWY_RESOLVE_MRP_MEMBER = 2
} GwyResolveScope;

typedef enum ExtResolveStrategy {
    EXT_RESOLVE_EXACT = 1,
    EXT_RESOLVE_CASE_FOLD = 2,
    EXT_RESOLVE_PROFILE_ALIAS = 3,
    /* CROSS_TARGET: cfunction.ext → reg.ext primary member (shell packages). */
    EXT_RESOLVE_REG_PRIMARY = 4
} ExtResolveStrategy;

typedef struct GwyExtResolveRequest {
    GwyResolveScope scope;
    const char *package_guest_path;
    const char *package_sha256;
    uint32_t package_appid;
    uint32_t package_appver;
    const char *requested_member;
} GwyExtResolveRequest;

typedef struct ExtResolvedMember {
    char requested_name[MRP_MEMBER_NAME_MAX];
    char resolved_name[MRP_MEMBER_NAME_MAX];
    ExtResolveStrategy strategy;
    const MrpMember *member;
    char profile_id[64];
    char package_guest_path[256];
} ExtResolvedMember;

typedef struct ExtResolver {
    const CompatibilityProfile *profile; /* optional */
    const MrpArchive *archive;
} ExtResolver;

void ext_resolver_init(ExtResolver *resolver,
                       const MrpArchive *archive,
                       const CompatibilityProfile *profile);

/*
 * Resolve order (fail-closed):
 * 1) exact member name
 * 2) case-normalized exact match
 * 3) reg.ext primary (cfunction.ext → declared primary EXT) — CROSS_TARGET
 * 4) profile alias only if scope==MRP_MEMBER AND exact miss AND profile matches
 * 5) MISS
 *
 * HOST_FILE never applies member aliases (DSM cfunction.ext isolation).
 */
LauncherStatus ext_resolver_resolve_request(const ExtResolver *resolver,
                                            const GwyExtResolveRequest *request,
                                            ExtResolvedMember *out,
                                            LauncherError *err);

/* Thin wrapper: scope=MRP_MEMBER using archive identity from resolver->archive. */
LauncherStatus ext_resolver_resolve(const ExtResolver *resolver,
                                    const char *requested_name,
                                    ExtResolvedMember *out,
                                    LauncherError *err);

const char *ext_resolve_strategy_name(ExtResolveStrategy s);
const char *gwy_resolve_scope_name(GwyResolveScope s);

#endif
