#ifndef GWY_LAUNCHER_MRP_REG_PRIMARY_H
#define GWY_LAUNCHER_MRP_REG_PRIMARY_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/mrp_archive.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CROSS_TARGET: start.mr requests "cfunction.ext"; reg.ext names the real primary
 * EXT member (gbrwcore.ext / gamelist.ext / gbrwshell.ext / robotol.ext, ...).
 * Returns 1 and writes member name on success; 0 if none found.
 */
int mrp_archive_find_reg_primary(const MrpArchive *archive, char *out_name, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif
