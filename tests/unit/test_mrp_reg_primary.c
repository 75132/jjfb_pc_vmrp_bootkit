#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/mrp_guest_index.h"
#include "gwy_launcher/mrp_member_view.h"
#include "gwy_launcher/mrp_reg_primary.h"
#include "gwy_launcher/ext_resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *root = getenv("GWY_FIXTURE_GWY");
    char path[1024];
    MrpArchive *arch = NULL;
    LauncherError err;
    char primary[256];
    ExtResolver resolver;
    ExtResolvedMember resolved;
    GwyExtResolveRequest req;
    uint8_t *view = NULL;
    size_t view_size = 0;
    MrpGuestLookupResult lr;
    MrpGuestLookupCode code;
    const MrpMember *pm = NULL;

    if (!root || !root[0]) {
        root = "game_files/mythroad/240x320/gwy";
    }
    snprintf(path, sizeof(path), "%s/gbrwcore.mrp", root);
    if (mrp_archive_open(path, &arch, &err) != L_OK) {
        fprintf(stderr, "open failed: %s\n", err.message);
        return 1;
    }
    if (!mrp_archive_find_reg_primary(arch, primary, sizeof(primary))) {
        fprintf(stderr, "reg primary missing\n");
        return 1;
    }
    if (strcmp(primary, "gbrwcore.ext") != 0) {
        fprintf(stderr, "unexpected primary %s\n", primary);
        return 1;
    }

    ext_resolver_init(&resolver, arch, NULL);
    memset(&req, 0, sizeof(req));
    req.scope = GWY_RESOLVE_MRP_MEMBER;
    req.package_guest_path = "gwy/gbrwcore.mrp";
    req.requested_member = "cfunction.ext";
    if (ext_resolver_resolve_request(&resolver, &req, &resolved, &err) != L_OK) {
        fprintf(stderr, "resolve failed: %s\n", err.message);
        return 1;
    }
    if (resolved.strategy != EXT_RESOLVE_REG_PRIMARY ||
        strcmp(resolved.resolved_name, "gbrwcore.ext") != 0) {
        fprintf(stderr, "bad resolve strategy/name\n");
        return 1;
    }

    if (mrp_archive_find_exact(arch, primary, &pm, &err) != L_OK) return 1;
    if (mrp_member_view_synthesize_alias(arch, "cfunction.ext", pm, &view, &view_size, &err) !=
        L_OK) {
        fprintf(stderr, "synthesize failed\n");
        return 1;
    }
    code = mrp_guest_index_lookup(view, view_size, "cfunction.ext", &lr);
    free(view);
    if (code != MRP_GUEST_LOOKUP_HIT) {
        fprintf(stderr, "guest lookup code=%d\n", (int)code);
        return 1;
    }

    mrp_archive_close(arch);
    arch = NULL;

    /* Phase 6Q-A: gamelist.mrp embeds primary name inside binary reg.ext NUL-run. */
    snprintf(path, sizeof(path), "%s/gamelist.mrp", root);
    if (mrp_archive_open(path, &arch, &err) != L_OK) {
        fprintf(stderr, "gamelist open failed: %s\n", err.message);
        return 1;
    }
    if (!mrp_archive_find_reg_primary(arch, primary, sizeof(primary))) {
        fprintf(stderr, "gamelist reg primary missing\n");
        return 1;
    }
    if (strcmp(primary, "gamelist.ext") != 0) {
        fprintf(stderr, "unexpected gamelist primary %s\n", primary);
        return 1;
    }
    mrp_archive_close(arch);

    puts("ok");
    return 0;
}
