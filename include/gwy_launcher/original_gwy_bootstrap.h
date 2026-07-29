#ifndef GWY_LAUNCHER_ORIGINAL_GWY_BOOTSTRAP_H
#define GWY_LAUNCHER_ORIGINAL_GWY_BOOTSTRAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P16–P18: Original GWY launcher headless rehost.
 *
 * Env:
 *   JJFB_BOOTSTRAP_MODE=direct_boot|original_headless|startgame_only
 *     (default: direct_boot — product-safe baseline)
 *   JJFB_ORIGINAL_API_MAP_CSV=reports/ORIGINAL_GWY_API_MAP.csv
 *   JJFB_ORIGINAL_RUNTIME_STACK_JSON=reports/ORIGINAL_GWY_RUNTIME_STACK.json
 *
 * Rules:
 *   - Never hardcode startGame entry; capture runtime registration.
 *   - Never synthesize code15 / force-write E6C / forge next screen.
 *   - Keep direct_boot as safe baseline.
 */

typedef enum JjfbBootstrapMode {
    JJFB_BOOTSTRAP_DIRECT_BOOT = 0,
    JJFB_BOOTSTRAP_ORIGINAL_HEADLESS = 1,
    JJFB_BOOTSTRAP_STARTGAME_ONLY = 2
} JjfbBootstrapMode;

#define ORIGINAL_GWY_CATALOG_MAX 16
#define ORIGINAL_GWY_API_MAX 32

typedef struct OriginalGwyPackageEntry {
    char guest_path[96];
    char host_path[512];
    char sha256_hex[65];
    char primary_ext[64];
    char start_mr[32];
    char reg_ext[32];
    int present;
    uint32_t size;
    int load_order;
    char role[32]; /* parent_platform | shared_core | mrp_shell | font | game | cfg */
} OriginalGwyPackageEntry;

typedef struct OriginalGwyBootstrapCatalog {
    char resource_root[512];
    OriginalGwyPackageEntry pkgs[ORIGINAL_GWY_CATALOG_MAX];
    int pkg_count;
    int cfg36_ok;
    char cfg36_param[192];
    char vfs_aliases[8][96];
    int alias_count;
    int complete; /* required set present */
} OriginalGwyBootstrapCatalog;

typedef struct OriginalGwyApiEntry {
    char api_name[48];
    uint32_t function_pointer; /* runtime entry if known; 0 if only string VA */
    uint32_t wrapper_pointer;
    uint32_t string_va;
    uint32_t context;
    char owner_module[48];
    uint32_t r9;
    uint32_t generation;
    char kind[32]; /* entry | string_va_not_entry | lookup | nested_start */
    int registered;
} OriginalGwyApiEntry;

void original_gwy_bootstrap_reset(void);
JjfbBootstrapMode original_gwy_bootstrap_mode(void);
const char *original_gwy_bootstrap_mode_name(JjfbBootstrapMode m);
int original_gwy_bootstrap_enabled(void); /* original_headless | startgame_only */

/* Proven cfg36 descriptor (napptype=12 from gwy/cfg.bin index 36). */
const char *original_gwy_cfg36_param(void);

int original_gwy_bootstrap_catalog_build(const char *resource_root,
                                        OriginalGwyBootstrapCatalog *out);
int original_gwy_bootstrap_catalog_write_json(const OriginalGwyBootstrapCatalog *cat,
                                             const char *path);

void original_gwy_api_reset(void);
int original_gwy_api_register(const char *api_name, uint32_t function_pointer,
                              uint32_t wrapper_pointer, uint32_t string_va, uint32_t context,
                              const char *owner_module, uint32_t r9, uint32_t generation,
                              const char *kind);
int original_gwy_api_lookup(const char *api_name, OriginalGwyApiEntry *out);
uint32_t original_gwy_api_entry_pc(const char *api_name); /* 0 if unknown */
int original_gwy_api_write_csv(const char *path);
void original_gwy_api_emit_banner(void);

/* Block update packages (dload/vdload) in headless mode — observe + deny start. */
int original_gwy_bootstrap_should_block_update_pkg(const char *package);

/* Preferred DSM start target for each bootstrap mode. */
const char *original_gwy_bootstrap_launch_target(void);
const char *original_gwy_bootstrap_launch_param(void);

/* Runtime stack helpers used by shell native path. */
void original_gwy_bootstrap_on_start_dsm(const char *filename, const char *ext,
                                         const char *entry, uint32_t r9);
void original_gwy_bootstrap_on_nested_jjfb(const char *target, const char *param, uint32_t r9);
void original_gwy_bootstrap_finalize(const char *stop_reason);

#ifdef __cplusplus
}
#endif

#endif
