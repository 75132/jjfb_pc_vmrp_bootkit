#ifndef GWY_LAUNCHER_PACKAGE_SCOPE_H
#define GWY_LAUNCHER_PACKAGE_SCOPE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Package-scoped c_load / primary resolution (Full Boot Stage A).
 * Env: JJFB_PACKAGE_SCOPED_CLOAD=1
 * When active, guest cfunction.ext / primary handoff resolves to the
 * current package's reg.ext primary — not global dsm:cfunction.ext.
 */

typedef struct PackageScopeEntry {
    const char *package_guest_path; /* e.g. gwy/gamelist.mrp */
    const char *primary_ext;        /* e.g. gamelist.ext */
    const char *class_name;         /* shell_core | mrc_loader_game */
} PackageScopeEntry;

void package_scope_reset(void);

/* True when JJFB_PACKAGE_SCOPED_CLOAD=1 (or JJFB_NATIVE_BOOT_FULL=1). */
int package_scope_enabled(void);

/* Built-in map for shell + game packages from the Full Boot pack. */
const PackageScopeEntry *package_scope_lookup(const char *package_guest_path);

/* Activate scope for a package (start_dsm / continue). Emits [JJFB_PACKAGE_SCOPE]. */
int package_scope_set_active(const char *package_guest_path);

const char *package_scope_active_package(void);
const char *package_scope_active_primary(void);

/* Resolve logical member under active scope. Returns 1 on hit. */
int package_scope_resolve_cfunction(const char *requested_member,
                                    char *out_primary,
                                    size_t out_cap);

/* True if name matches the active package primary (case-insensitive basename). */
int package_scope_is_active_primary(const char *module_name);

#ifdef __cplusplus
}
#endif

#endif
