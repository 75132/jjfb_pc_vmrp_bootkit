#include "gwy_launcher/original_gwy_bootstrap.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/mrp_reg_primary.h"
#include "gwy_launcher/mrp_runtime_stack.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define OG_ACCESS _access
#ifndef R_OK
#define R_OK 4
#endif
#else
#include <unistd.h>
#define OG_ACCESS access
#endif

static const char *k_cfg36_param =
    "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";

static struct {
    int mode_known;
    JjfbBootstrapMode mode;
    OriginalGwyApiEntry apis[ORIGINAL_GWY_API_MAX];
    int api_count;
    int banner_emitted;
    int catalog_built;
    int finalized;
    OriginalGwyBootstrapCatalog catalog;
} g_og;

static int env_eq(const char *key, const char *val) {
    const char *e = getenv(key);
    return e && val && strcmp(e, val) == 0;
}

static void join2(char *out, size_t cap, const char *a, const char *b) {
    size_t n = a ? strlen(a) : 0;
    if (!out || cap == 0) return;
    if (!a || !a[0]) {
        snprintf(out, cap, "%s", b ? b : "");
        return;
    }
    if (n && (a[n - 1] == '/' || a[n - 1] == '\\'))
        snprintf(out, cap, "%s%s", a, b ? b : "");
    else
        snprintf(out, cap, "%s/%s", a, b ? b : "");
}

static int file_exists(const char *path) { return path && path[0] && OG_ACCESS(path, R_OK) == 0; }

void original_gwy_bootstrap_reset(void) {
    memset(&g_og, 0, sizeof(g_og));
    mrp_runtime_stack_reset(mrp_runtime_stack_global());
}

JjfbBootstrapMode original_gwy_bootstrap_mode(void) {
    const char *e;
    if (g_og.mode_known) return g_og.mode;
    e = getenv("JJFB_BOOTSTRAP_MODE");
    if (e && (strcmp(e, "original_headless") == 0 || strcmp(e, "headless") == 0))
        g_og.mode = JJFB_BOOTSTRAP_ORIGINAL_HEADLESS;
    else if (e && (strcmp(e, "startgame_only") == 0 || strcmp(e, "startgame") == 0))
        g_og.mode = JJFB_BOOTSTRAP_STARTGAME_ONLY;
    else
        g_og.mode = JJFB_BOOTSTRAP_DIRECT_BOOT;
    g_og.mode_known = 1;
    return g_og.mode;
}

const char *original_gwy_bootstrap_mode_name(JjfbBootstrapMode m) {
    switch (m) {
    case JJFB_BOOTSTRAP_ORIGINAL_HEADLESS: return "original_headless";
    case JJFB_BOOTSTRAP_STARTGAME_ONLY: return "startgame_only";
    default: return "direct_boot";
    }
}

int original_gwy_bootstrap_enabled(void) {
    JjfbBootstrapMode m = original_gwy_bootstrap_mode();
    return m == JJFB_BOOTSTRAP_ORIGINAL_HEADLESS || m == JJFB_BOOTSTRAP_STARTGAME_ONLY;
}

const char *original_gwy_cfg36_param(void) { return k_cfg36_param; }

static int add_pkg(OriginalGwyBootstrapCatalog *cat, const char *guest, const char *role,
                   int load_order) {
    OriginalGwyPackageEntry *e;
    MrpArchive *arch = NULL;
    LauncherError err;
    uint8_t dig[32];
    char primary[64];
    if (!cat || cat->pkg_count >= ORIGINAL_GWY_CATALOG_MAX) return 0;
    e = &cat->pkgs[cat->pkg_count];
    memset(e, 0, sizeof(*e));
    snprintf(e->guest_path, sizeof(e->guest_path), "%s", guest ? guest : "");
    snprintf(e->role, sizeof(e->role), "%s", role ? role : "");
    e->load_order = load_order;
    join2(e->host_path, sizeof(e->host_path), cat->resource_root, guest);
    if (!file_exists(e->host_path)) {
        /* VFS alias candidates (do not modify originals). */
        if (strcmp(guest, "cfg.bin") == 0) {
            join2(e->host_path, sizeof(e->host_path), cat->resource_root, "gwy/cfg.bin");
            if (file_exists(e->host_path) && cat->alias_count < 8) {
                snprintf(cat->vfs_aliases[cat->alias_count++], 96, "cfg.bin->gwy/cfg.bin");
            }
        } else if (strcmp(guest, "gamelist.mrp") == 0) {
            join2(e->host_path, sizeof(e->host_path), cat->resource_root, "gwy/gamelist.mrp");
            if (file_exists(e->host_path) && cat->alias_count < 8) {
                snprintf(cat->vfs_aliases[cat->alias_count++], 96, "gamelist.mrp->gwy/gamelist.mrp");
            }
        }
    }
    e->present = file_exists(e->host_path);
    if (!e->present) {
        cat->pkg_count++;
        return 0;
    }
    if (gwy_sha256_file(e->host_path, dig)) {
        gwy_sha256_hex(dig, e->sha256_hex);
    }
    {
        FILE *fp = fopen(e->host_path, "rb");
        if (fp) {
            long sz;
            fseek(fp, 0, SEEK_END);
            sz = ftell(fp);
            fclose(fp);
            if (sz > 0) e->size = (uint32_t)sz;
        }
    }
    snprintf(e->start_mr, sizeof(e->start_mr), "%s", "start.mr");
    snprintf(e->reg_ext, sizeof(e->reg_ext), "%s", "reg.ext");
    if (strstr(guest, ".mrp") && mrp_archive_open(e->host_path, &arch, &err) == L_OK) {
        primary[0] = 0;
        if (mrp_archive_find_reg_primary(arch, primary, sizeof(primary))) {
            snprintf(e->primary_ext, sizeof(e->primary_ext), "%s", primary);
        } else if (strstr(guest, "jjfb")) {
            snprintf(e->primary_ext, sizeof(e->primary_ext), "%s", "robotol.ext");
        }
        mrp_archive_close(arch);
    } else if (strstr(guest, "cfg.bin")) {
        snprintf(e->primary_ext, sizeof(e->primary_ext), "%s", "(cfg)");
        e->start_mr[0] = 0;
        e->reg_ext[0] = 0;
    } else if (strstr(guest, "jjfbol")) {
        snprintf(e->primary_ext, sizeof(e->primary_ext), "%s", "(dir)");
        e->start_mr[0] = 0;
        e->reg_ext[0] = 0;
    }
    cat->pkg_count++;
    return 1;
}

int original_gwy_bootstrap_catalog_build(const char *resource_root,
                                        OriginalGwyBootstrapCatalog *out) {
    static const struct {
        const char *guest;
        const char *role;
        int order;
        int required;
    } k_pkgs[] = {
        {"gwy.mrp", "parent_platform", 0, 1},
        {"gwy/gbrwcore.mrp", "shared_core", 1, 1},
        {"gwy/gbrwshell.mrp", "mrp_shell", 2, 1},
        {"gwy/font.mrp", "font", 3, 1},
        {"gwy/gamelist.mrp", "game_selector", 4, 0},
        {"gwy/jjfb.mrp", "game", 5, 1},
        {"gwy/jjfbol", "game_assets", 6, 1},
        {"gwy/cfg.bin", "cfg", 7, 1},
    };
    size_t i;
    int req_ok = 1;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    snprintf(out->resource_root, sizeof(out->resource_root), "%s",
             resource_root && resource_root[0] ? resource_root : "game_files/mythroad/240x320");
    snprintf(out->cfg36_param, sizeof(out->cfg36_param), "%s", k_cfg36_param);
    out->cfg36_ok = 1;
    for (i = 0; i < sizeof(k_pkgs) / sizeof(k_pkgs[0]); i++) {
        int ok = add_pkg(out, k_pkgs[i].guest, k_pkgs[i].role, k_pkgs[i].order);
        if (k_pkgs[i].required && !ok) req_ok = 0;
    }
    out->complete = req_ok;
    return out->pkg_count > 0;
}

int original_gwy_bootstrap_catalog_write_json(const OriginalGwyBootstrapCatalog *cat,
                                             const char *path) {
    FILE *fp;
    int i;
    if (!cat || !path) return -1;
    fp = fopen(path, "wb");
    if (!fp) return -1;
    fprintf(fp, "{\n");
    fprintf(fp, "  \"resource_root\": \"%s\",\n", cat->resource_root);
    fprintf(fp, "  \"complete\": %s,\n", cat->complete ? "true" : "false");
    fprintf(fp, "  \"cfg36_ok\": %s,\n", cat->cfg36_ok ? "true" : "false");
    fprintf(fp, "  \"cfg36_param\": \"%s\",\n", cat->cfg36_param);
    fprintf(fp, "  \"vfs_aliases\": [");
    for (i = 0; i < cat->alias_count; i++) {
        fprintf(fp, "%s\"%s\"", i ? ", " : "", cat->vfs_aliases[i]);
    }
    fprintf(fp, "],\n");
    fprintf(fp, "  \"packages\": [\n");
    for (i = 0; i < cat->pkg_count; i++) {
        const OriginalGwyPackageEntry *e = &cat->pkgs[i];
        fprintf(fp,
                "    {\"load_order\":%d,\"guest\":\"%s\",\"role\":\"%s\",\"present\":%s,"
                "\"size\":%u,\"sha256\":\"%s\",\"primary_ext\":\"%s\",\"start_mr\":\"%s\","
                "\"reg_ext\":\"%s\"}%s\n",
                e->load_order, e->guest_path, e->role, e->present ? "true" : "false", e->size,
                e->sha256_hex, e->primary_ext, e->start_mr, e->reg_ext,
                (i + 1 < cat->pkg_count) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    return 0;
}

void original_gwy_api_reset(void) {
    g_og.api_count = 0;
    memset(g_og.apis, 0, sizeof(g_og.apis));
}

int original_gwy_api_register(const char *api_name, uint32_t function_pointer,
                              uint32_t wrapper_pointer, uint32_t string_va, uint32_t context,
                              const char *owner_module, uint32_t r9, uint32_t generation,
                              const char *kind) {
    OriginalGwyApiEntry *e = NULL;
    int i;
    if (!api_name || !api_name[0]) return 0;
    for (i = 0; i < g_og.api_count; i++) {
        if (strcmp(g_og.apis[i].api_name, api_name) == 0) {
            e = &g_og.apis[i];
            break;
        }
    }
    if (!e) {
        if (g_og.api_count >= ORIGINAL_GWY_API_MAX) return 0;
        e = &g_og.apis[g_og.api_count++];
        memset(e, 0, sizeof(*e));
        snprintf(e->api_name, sizeof(e->api_name), "%s", api_name);
    }
    if (function_pointer) e->function_pointer = function_pointer;
    if (wrapper_pointer) e->wrapper_pointer = wrapper_pointer;
    if (string_va) e->string_va = string_va;
    if (context) e->context = context;
    if (owner_module && owner_module[0])
        snprintf(e->owner_module, sizeof(e->owner_module), "%s", owner_module);
    if (r9) e->r9 = r9;
    if (generation) e->generation = generation;
    if (kind && kind[0]) snprintf(e->kind, sizeof(e->kind), "%s", kind);
    e->registered = 1;
    printf("[API_REGISTER] api_name=%s function_pointer=0x%X wrapper_pointer=0x%X "
           "string_va=0x%X context=0x%X owner_module=%s R9=0x%X generation=%u kind=%s "
           "evidence=OBSERVED\n",
           e->api_name, e->function_pointer, e->wrapper_pointer, e->string_va, e->context,
           e->owner_module[0] ? e->owner_module : "?", e->r9, e->generation,
           e->kind[0] ? e->kind : "?");
    fflush(stdout);
    return 1;
}

int original_gwy_api_lookup(const char *api_name, OriginalGwyApiEntry *out) {
    int i;
    if (!api_name || !out) return 0;
    for (i = 0; i < g_og.api_count; i++) {
        if (strcmp(g_og.apis[i].api_name, api_name) == 0) {
            *out = g_og.apis[i];
            return 1;
        }
    }
    return 0;
}

uint32_t original_gwy_api_entry_pc(const char *api_name) {
    OriginalGwyApiEntry e;
    if (!original_gwy_api_lookup(api_name, &e)) return 0;
    return e.function_pointer;
}

int original_gwy_api_write_csv(const char *path) {
    FILE *fp;
    int i;
    const char *p = path;
    if (!p || !p[0]) p = getenv("JJFB_ORIGINAL_API_MAP_CSV");
    if (!p || !p[0]) p = "reports/ORIGINAL_GWY_API_MAP.csv";
    fp = fopen(p, "wb");
    if (!fp) return -1;
    fprintf(fp, "api_name,function_pointer,wrapper_pointer,string_va,context,owner_module,R9,"
                "generation,kind,registered\n");
    for (i = 0; i < g_og.api_count; i++) {
        const OriginalGwyApiEntry *e = &g_og.apis[i];
        fprintf(fp, "%s,0x%X,0x%X,0x%X,0x%X,%s,0x%X,%u,%s,%d\n", e->api_name, e->function_pointer,
                e->wrapper_pointer, e->string_va, e->context,
                e->owner_module[0] ? e->owner_module : "", e->r9, e->generation,
                e->kind[0] ? e->kind : "", e->registered);
    }
    fclose(fp);
    return 0;
}

void original_gwy_api_emit_banner(void) {
    if (g_og.banner_emitted || !original_gwy_bootstrap_enabled()) return;
    g_og.banner_emitted = 1;
    printf("[JJFB_BOOTSTRAP] mode=%s cfg36_param=%s note=no_code15_synth no_E6C_force "
           "keep_direct_boot_baseline=yes\n",
           original_gwy_bootstrap_mode_name(original_gwy_bootstrap_mode()), k_cfg36_param);
    fflush(stdout);
}

int original_gwy_bootstrap_should_block_update_pkg(const char *package) {
    if (!original_gwy_bootstrap_enabled() || !package) return 0;
    return strstr(package, "dload") != NULL || strstr(package, "vdload") != NULL;
}

const char *original_gwy_bootstrap_launch_target(void) {
    JjfbBootstrapMode m = original_gwy_bootstrap_mode();
    const char *override = getenv("GWY_LAUNCH_TARGET");
    if (m == JJFB_BOOTSTRAP_DIRECT_BOOT) {
        if (override && override[0]) return override;
        return "gwy/jjfb.mrp";
    }
    /* Prefer gbrwcore as shared-core entry; gwy.mrp is optional platform bridge. */
    if (env_eq("JJFB_ORIGINAL_START_WITH", "gwy")) return "gwy.mrp";
    if (env_eq("JJFB_ORIGINAL_START_WITH", "gbrwshell")) return "gwy/gbrwshell.mrp";
    if (override && override[0] && !strstr(override, "jjfb")) return override;
    return "gwy/gbrwcore.mrp";
}

const char *original_gwy_bootstrap_launch_param(void) {
    const char *p = getenv("GWY_LAUNCH_PARAM");
    if (p && p[0]) return p;
    return k_cfg36_param;
}

void original_gwy_bootstrap_on_start_dsm(const char *filename, const char *ext, const char *entry,
                                        uint32_t r9) {
    char primary[64];
    if (!original_gwy_bootstrap_enabled()) return;
    original_gwy_api_emit_banner();
    if (original_gwy_bootstrap_should_block_update_pkg(filename)) {
        printf("[JJFB_BOOTSTRAP] block_update_pkg=%s reason=headless_skip_dload_vdload "
               "evidence=POLICY\n",
               filename ? filename : "?");
        fflush(stdout);
        return;
    }
    primary[0] = 0;
    if (ext && strstr(ext, ".ext"))
        snprintf(primary, sizeof(primary), "%s", ext);
    else if (filename && strstr(filename, "gbrwcore"))
        snprintf(primary, sizeof(primary), "%s", "gbrwcore.ext");
    else if (filename && strstr(filename, "gbrwshell"))
        snprintf(primary, sizeof(primary), "%s", "gbrwshell.ext");
    else if (filename && strstr(filename, "gamelist"))
        snprintf(primary, sizeof(primary), "%s", "gamelist.ext");
    else if (filename && strstr(filename, "jjfb"))
        snprintf(primary, sizeof(primary), "%s", "robotol.ext");
    (void)entry;
    mrp_runtime_stack_push(mrp_runtime_stack_global(), filename, primary, r9, 0, 0);
    if (filename && strstr(filename, "jjfb")) {
        original_gwy_bootstrap_on_nested_jjfb(filename, entry, r9);
    }
}

void original_gwy_bootstrap_on_nested_jjfb(const char *target, const char *param, uint32_t r9) {
    MrpRuntimeStack *st;
    if (!original_gwy_bootstrap_enabled()) return;
    st = mrp_runtime_stack_global();
    mrp_runtime_stack_note_nested_jjfb(st, target, param);
    mrp_runtime_stack_update_top(st, r9, 0, 0, 0, 0, 0);
    original_gwy_api_register("lib.runflashmrp_or_runapp", 0, 0, 0, 0, "nested_start_dsm", r9, 0,
                              "nested_start");
    printf("[JJFB_BOOTSTRAP] nested_jjfb keep_parent_vm=yes target=%s evidence=OBSERVED\n",
           target ? target : "gwy/jjfb.mrp");
    fflush(stdout);
}

void original_gwy_bootstrap_finalize(const char *stop_reason) {
    const char *csv;
    const char *stack_json;
    const char *reports_dir;
    char csv_buf[512];
    char stack_buf[512];
    if (!original_gwy_bootstrap_enabled() || g_og.finalized) return;
    g_og.finalized = 1;
    reports_dir = getenv("GWY_PRODUCT_REPORTS_DIR");
    csv = getenv("JJFB_ORIGINAL_API_MAP_CSV");
    stack_json = getenv("JJFB_ORIGINAL_RUNTIME_STACK_JSON");
    if ((!csv || !csv[0]) && reports_dir && reports_dir[0]) {
        snprintf(csv_buf, sizeof(csv_buf), "%s/ORIGINAL_GWY_API_MAP.csv", reports_dir);
        csv = csv_buf;
    }
    if (!csv || !csv[0]) csv = "reports/ORIGINAL_GWY_API_MAP.csv";
    if ((!stack_json || !stack_json[0]) && reports_dir && reports_dir[0]) {
        snprintf(stack_buf, sizeof(stack_buf), "%s/ORIGINAL_GWY_RUNTIME_STACK.json", reports_dir);
        stack_json = stack_buf;
    }
    if (!stack_json || !stack_json[0]) stack_json = "reports/ORIGINAL_GWY_RUNTIME_STACK.json";
    (void)original_gwy_api_write_csv(csv);
    (void)mrp_runtime_stack_write_json(mrp_runtime_stack_global(), stack_json);
    printf("[JJFB_BOOTSTRAP] finalize mode=%s stop=%s api_csv=%s stack_json=%s api_count=%d "
           "stack_depth=%d evidence=OBSERVED\n",
           original_gwy_bootstrap_mode_name(original_gwy_bootstrap_mode()),
           stop_reason ? stop_reason : "?", csv, stack_json, g_og.api_count,
           mrp_runtime_stack_global()->depth);
    fflush(stdout);
}
