#ifndef GWY_LAUNCHER_MRP_MEMBER_VIEW_H
#define GWY_LAUNCHER_MRP_MEMBER_VIEW_H

#include "gwy_launcher/compat_profile.h"
#include "gwy_launcher/error.h"
#include "gwy_launcher/guest_vfs.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_archive.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * For a profile-matched MRP package: install a generated index view that adds
 * aliased member names (e.g. cfunction.ext → same blob as robotol.ext) without
 * modifying the canonical resource tree. Original sha256 is preserved for match.
 *
 * DSM host-file cfunction.ext is never touched.
 */
LauncherStatus mrp_member_view_install(GuestVfs *vfs,
                                       const CompatibilityProfile *profile,
                                       const char *canonical_host_mrp,
                                       const char *package_guest_path,
                                       const char *view_host_path,
                                       ModuleRegistry *reg_opt,
                                       LauncherError *err);

/*
 * Phase 6H: install cfunction.ext → reg.ext primary view for a shell/game MRP.
 * Always VFS-remaps so guest _mr_readFile("cfunction.ext") hits (CROSS_TARGET).
 * Does not modify original resource bytes / hash.
 */
LauncherStatus mrp_member_view_install_reg_primary(GuestVfs *vfs,
                                                   const char *canonical_host_mrp,
                                                   const char *package_guest_path,
                                                   const char *view_host_path,
                                                   ModuleRegistry *reg_opt,
                                                   LauncherError *err);

/* Build alias view bytes in memory (for tests / inspect-member-view). */
LauncherStatus mrp_member_view_synthesize_alias(const MrpArchive *archive,
                                                const char *from_name,
                                                const MrpMember *to_member,
                                                uint8_t **out_data,
                                                size_t *out_size,
                                                LauncherError *err);

#ifdef __cplusplus
}
#endif

#endif
