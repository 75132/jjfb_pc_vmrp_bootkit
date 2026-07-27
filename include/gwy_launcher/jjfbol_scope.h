#ifndef GWY_LAUNCHER_JJFBOL_SCOPE_H
#define GWY_LAUNCHER_JJFBOL_SCOPE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Independent of package_scope (top-level MRP to EXT). Tracks gwy/jjfbol mrp open/close. */

void jjfbol_scope_reset(void);
void jjfbol_scope_bump_generation(void);
uint64_t jjfbol_scope_generation(void);

void jjfbol_scope_on_open(const char *guest_path);
void jjfbol_scope_on_close(const char *guest_path);

/* Active package stem (e.g. "default2") or NULL if none. */
const char *jjfbol_scope_active_package(void);
/* Full guest path of active pack or NULL. */
const char *jjfbol_scope_active_guest_path(void);

#ifdef __cplusplus
}
#endif

#endif
