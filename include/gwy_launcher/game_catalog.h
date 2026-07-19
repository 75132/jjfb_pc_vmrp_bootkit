#ifndef GWY_LAUNCHER_GAME_CATALOG_H
#define GWY_LAUNCHER_GAME_CATALOG_H

#include "gwy_launcher/error.h"
#include <stddef.h>
#include <stdint.h>

#define GWY_GAME_MAX 128
#define GWY_GAME_TITLE_MAX 128
#define GWY_GAME_TARGET_MAX 256

typedef struct GwyGameEntry {
    uint32_t cfg_index;
    int32_t napptype;
    int32_t nextid;
    int32_t ncode;
    int32_t narg;
    int32_t narg1;
    char target_mrp[GWY_GAME_TARGET_MAX];
    char icon[64];
    char title_utf8[GWY_GAME_TITLE_MAX];
    char display_name_utf8[GWY_GAME_TITLE_MAX];
    uint32_t appid;
    uint32_t appver;
    int mrp_exists;
    int selected_prefer; /* higher = better candidate for same target */
} GwyGameEntry;

typedef struct GwyGameCatalog {
    GwyGameEntry entries[GWY_GAME_MAX];
    size_t count;
    char resource_root[1024];
} GwyGameCatalog;

LauncherStatus gwy_game_catalog_scan(const char *resource_root,
                                     GwyGameCatalog *out,
                                     LauncherError *err);

/* Host UI: show local GWY cfg game list and launch selected title via vmrp. */
int gwy_games_picker_run(const GwyGameCatalog *catalog,
                         const char *vmrp_exe,
                         const char *vmrp_cwd);

#endif
