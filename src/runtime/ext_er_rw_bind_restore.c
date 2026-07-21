#include "gwy_launcher/ext_er_rw_bind_restore.h"
#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/e10a31b_publication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Strong impl in gwy_ext_obs.c (launcher_core). */
void gwy_ext_obs_request_ext_init_seq(void);
uint32_t gwy_ext_obs_guest_malloc0(uint32_t size);

/* Observed gamelist P+4 when guest fills after entry (heap-reuse path). */
static const uint32_t k_gamelist_erw_default_len = 0xA34u;

static struct {
    int mode_known;
    GwyErRwBindMode mode;
    void *uc;
    int finalized;
    int bound;
    int deferred_switch_done;
    uint32_t last_base;
    uint32_t last_len;
    uint64_t last_module_id;
    char last_module[48];
    uint32_t last_p;
} g_bind;

static void parse_mode(void) {
    const char *e;
    if (g_bind.mode_known) return;
    g_bind.mode = GWY_ER_RW_BIND_OFF;
    e = getenv("JJFB_ER_RW_BIND_RESTORE");
    if (e && e[0]) {
        if (strcmp(e, "gbrwcore_only") == 0 || e[0] == '1')
            g_bind.mode = GWY_ER_RW_BIND_GBRWCORE_ONLY;
        else if (strcmp(e, "gwy_shell") == 0 || strcmp(e, "shell_core") == 0)
            g_bind.mode = GWY_ER_RW_BIND_SHELL_CORE;
        else if (strcmp(e, "shell_and_game") == 0)
            g_bind.mode = GWY_ER_RW_BIND_SHELL_AND_GAME;
        else if (strcmp(e, "game_package") == 0)
            g_bind.mode = GWY_ER_RW_BIND_GAME_PACKAGE;
    }
    g_bind.mode_known = 1;
}

const char *ext_er_rw_bind_restore_mode_name(GwyErRwBindMode m) {
    switch (m) {
    case GWY_ER_RW_BIND_GBRWCORE_ONLY: return "gbrwcore_only";
    case GWY_ER_RW_BIND_SHELL_CORE: return "shell_core";
    case GWY_ER_RW_BIND_SHELL_AND_GAME: return "shell_and_game";
    case GWY_ER_RW_BIND_GAME_PACKAGE: return "game_package";
    default: return "off";
    }
}

void ext_er_rw_bind_restore_reset(void) {
    void *uc = g_bind.uc;
    memset(&g_bind, 0, sizeof(g_bind));
    g_bind.uc = uc;
}

void ext_er_rw_bind_restore_bind_uc(void *uc) { g_bind.uc = uc; }

int ext_er_rw_bind_restore_enabled(void) {
    parse_mode();
    return g_bind.mode != GWY_ER_RW_BIND_OFF;
}

GwyErRwBindMode ext_er_rw_bind_restore_mode(void) {
    parse_mode();
    return g_bind.mode;
}

int ext_er_rw_bind_restore_bound(void) { return g_bind.bound; }
uint32_t ext_er_rw_bind_restore_last_base(void) { return g_bind.last_base; }
uint32_t ext_er_rw_bind_restore_last_len(void) { return g_bind.last_len; }

static int name_ok_gbrwcore(const char *n) {
    return n && strstr(n, "gbrwcore") != NULL;
}

static int name_ok_shell(const char *n) {
    return n && (strstr(n, "gbrwcore") != NULL || strstr(n, "gamelist") != NULL ||
                 strstr(n, "gbrwshell") != NULL);
}

static int name_ok_shell_and_game(const char *n) {
    return name_ok_shell(n) ||
           (n && (strstr(n, "mrc_loader") != NULL || strstr(n, "robotol") != NULL ||
                  strstr(n, "mmochat") != NULL));
}

static int name_ok_game_package(const char *n) {
    return n && (strstr(n, "mrc_loader") != NULL || strstr(n, "robotol") != NULL ||
                 strstr(n, "mmochat") != NULL);
}

static int name_ok(const char *n) {
    parse_mode();
    if (g_bind.mode == GWY_ER_RW_BIND_GAME_PACKAGE) return name_ok_game_package(n);
    if (g_bind.mode == GWY_ER_RW_BIND_SHELL_AND_GAME) return name_ok_shell_and_game(n);
    if (g_bind.mode == GWY_ER_RW_BIND_SHELL_CORE) return name_ok_shell(n);
    return name_ok_gbrwcore(n);
}

static int module_gated(const char *module) {
    parse_mode();
    if (g_bind.mode == GWY_ER_RW_BIND_OFF) return 0;
    if (g_bind.mode == GWY_ER_RW_BIND_GBRWCORE_ONLY) return name_ok_gbrwcore(module);
    if (g_bind.mode == GWY_ER_RW_BIND_SHELL_CORE) return name_ok_shell(module);
    if (g_bind.mode == GWY_ER_RW_BIND_SHELL_AND_GAME) return name_ok_shell_and_game(module);
    if (g_bind.mode == GWY_ER_RW_BIND_GAME_PACKAGE) return name_ok_game_package(module);
    return 0;
}

static int peek_p_fields(uint32_t p_guest, uint32_t *base, uint32_t *len) {
    /* Host pointer only (audit: no uc_mem_read outside guest_memory.c). */
    return ext_chunk_provider_peek_p_er_rw(p_guest, base, len);
}

static const GwyLoadedModule *find_shell_by_hint(ModuleRegistry *reg, const char *hint) {
    if (!reg || !hint) return NULL;
    if (strstr(hint, "gamelist")) return module_registry_find(reg, "gamelist.ext");
    if (strstr(hint, "gbrwshell")) return module_registry_find(reg, "gbrwshell.ext");
    if (strstr(hint, "gbrwcore")) return module_registry_find(reg, "gbrwcore.ext");
    if (strstr(hint, "mrc_loader")) return module_registry_find(reg, "mrc_loader.ext");
    if (strstr(hint, "robotol")) return module_registry_find(reg, "robotol.ext");
    if (strstr(hint, "mmochat")) return module_registry_find(reg, "mmochat.ext");
    return NULL;
}

static const GwyLoadedModule *resolve_module(uint32_t p_guest, const char *module_hint) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *gm = NULL;
    uint32_t helper = 0;
    if (!reg) return NULL;
    if (module_hint && name_ok(module_hint)) {
        gm = find_shell_by_hint(reg, module_hint);
        if (gm) return gm;
    }
    if (ext_chunk_provider_lookup_p(p_guest, NULL, &helper, NULL) && helper) {
        gm = module_registry_find_by_helper(reg, helper);
        if (!gm) gm = module_registry_find_by_code_addr(reg, helper);
        if (gm && name_ok(gm->resolved_name[0] ? gm->resolved_name : gm->requested_name))
            return gm;
    }
    {
        const char *lm = ext_chunk_provider_last_module();
        gm = find_shell_by_hint(reg, lm);
        if (gm) return gm;
    }
    parse_mode();
    if (g_bind.mode == GWY_ER_RW_BIND_GAME_PACKAGE) {
        gm = module_registry_find(reg, "robotol.ext");
        if (!gm) gm = module_registry_find(reg, "mmochat.ext");
        if (!gm) gm = module_registry_find(reg, "mrc_loader.ext");
        return gm;
    }
    return module_registry_find(reg, "gbrwcore.ext");
}

static void emit_game_p_timeline(const char *pkg, const char *module, const char *event,
                                 uint32_t p, uint32_t p0, uint32_t p4, uint32_t reg_base,
                                 uint32_t reg_len, uint32_t r9, uint32_t pc, const char *note) {
    const char *e = getenv("JJFB_GAME_P_TIMELINE_TRACE");
    if (e && e[0] == '0') return;
    printf("[JJFB_GAME_P_TIMELINE] package=%s module=%s event=%s P=0x%X p0=0x%X p4=0x%X "
           "registry_base=0x%X registry_len=0x%X r9=0x%X pc=0x%X note=%s evidence=OBSERVED\n",
           pkg ? pkg : "?", module ? module : "?", event ? event : "?", p, p0, p4, reg_base,
           reg_len, r9, pc, note ? note : "");
    fflush(stdout);
}

static const char *pkg_of(const GwyLoadedModule *gm) {
    if (!gm) return "?";
    if (strstr(gm->package_path, "jjfb.mrp")) return "gwy/jjfb.mrp";
    if (strstr(gm->package_path, "wxjwq.mrp")) return "gwy/wxjwq.mrp";
    return gm->package_path[0] ? gm->package_path : "?";
}

static void try_deferred_r9(const GwyLoadedModule *gm) {
    uint32_t caller_r9 = 0;
    int rc;
    if (!gm || !g_bind.uc || g_bind.deferred_switch_done) return;
    if (!gm->data.start_of_er_rw) return;

    (void)guest_memory_uc_read_r9((struct uc_struct *)g_bind.uc, &caller_r9);
    printf("[JJFB_R9_SWITCH_ATTEMPT] package=%s module=%s module_id=%llu callee_er_rw=0x%X "
           "caller_r9=0x%X call_kind=RUNTIME_ENTRY reason=platform_er_rw_publication_restore "
           "evidence=DOCUMENTED\n",
           pkg_of(gm), gm->resolved_name[0] ? gm->resolved_name : gm->requested_name,
           (unsigned long long)gm->module_id, gm->data.start_of_er_rw, caller_r9);
    fflush(stdout);

    g_bind.deferred_switch_done = 1;
    rc = module_r9_switch_enter(g_bind.uc, 0, gm->module_id, GWY_CALL_RUNTIME_ENTRY);
    if (rc == 1) {
        printf("[JJFB_R9_SWITCH_OK] package=%s module=%s module_id=%llu r9=0x%X er_rw_len=0x%X "
               "evidence=DOCUMENTED\n",
               pkg_of(gm), gm->resolved_name[0] ? gm->resolved_name : gm->requested_name,
               (unsigned long long)gm->module_id, gm->data.start_of_er_rw, gm->data.er_rw_size);
        /* Queue DOCUMENTED 6→8→0 for next host helper enter (not from this Unicorn hook). */
        if (g_bind.mode == GWY_ER_RW_BIND_GAME_PACKAGE)
            gwy_ext_obs_request_ext_init_seq();
        else if ((g_bind.mode == GWY_ER_RW_BIND_SHELL_CORE ||
                  g_bind.mode == GWY_ER_RW_BIND_SHELL_AND_GAME) &&
                 strstr(gm->resolved_name[0] ? gm->resolved_name : gm->requested_name,
                        "gamelist"))
            gwy_ext_obs_request_ext_init_seq();
    } else if (rc == 0) {
        printf("[JJFB_R9_SWITCH_OK] package=%s module=%s module_id=%llu r9=0x%X already_switched=1 "
               "evidence=DOCUMENTED\n",
               pkg_of(gm), gm->resolved_name[0] ? gm->resolved_name : gm->requested_name,
               (unsigned long long)gm->module_id, gm->data.start_of_er_rw);
        if (g_bind.mode == GWY_ER_RW_BIND_GAME_PACKAGE)
            gwy_ext_obs_request_ext_init_seq();
        else if ((g_bind.mode == GWY_ER_RW_BIND_SHELL_CORE ||
                  g_bind.mode == GWY_ER_RW_BIND_SHELL_AND_GAME) &&
                 strstr(gm->resolved_name[0] ? gm->resolved_name : gm->requested_name,
                        "gamelist"))
            gwy_ext_obs_request_ext_init_seq();
    } else {
        printf("[JJFB_R9_SWITCH_BLOCKED] package=%s module=%s module_id=%llu callee_er_rw=0x%X "
               "result=%d note=deferred_after_bind evidence=TARGET_OBSERVED\n",
               pkg_of(gm), gm->resolved_name[0] ? gm->resolved_name : gm->requested_name,
               (unsigned long long)gm->module_id, gm->data.start_of_er_rw, rc);
        printf("[JJFB_GAME_R9_BLOCKED] package=%s module=%s reason=deferred_switch_fail "
               "P=0x%X p0=0x%X p4=0x%X evidence=TARGET_OBSERVED\n",
               pkg_of(gm), gm->resolved_name[0] ? gm->resolved_name : gm->requested_name,
               g_bind.last_p, g_bind.last_base, g_bind.last_len);
    }
    fflush(stdout);
}

static int poke_p_er_rw(uint32_t p_guest, uint32_t base, uint32_t len) {
    void *p_host = NULL;
    uint32_t *words;
    if (!ext_chunk_provider_lookup_p(p_guest, &p_host, NULL, NULL) || !p_host) return 0;
    words = (uint32_t *)p_host;
    words[0] = base;
    words[1] = len;
    return 1;
}

static uint32_t alloc_distinct_erw(uint32_t len, uint32_t avoid) {
    uint32_t fresh;
    int i;
    for (i = 0; i < 3; i++) {
        fresh = gwy_ext_obs_guest_malloc0(len);
        if (fresh && fresh != avoid) return fresh;
    }
    return 0;
}

static int foreign_owner_of(ModuleRegistry *reg, uint32_t base, uint64_t self_id,
                            const char **out_name) {
    size_t i;
    if (!reg || !base) return 0;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].data.start_of_er_rw == base &&
            reg->modules[i].module_id != self_id) {
            if (out_name) {
                *out_name = reg->modules[i].resolved_name[0] ? reg->modules[i].resolved_name
                                                             : reg->modules[i].requested_name;
            }
            return 1;
        }
    }
    return 0;
}

/*
 * Replace empty/foreign P ERW with a host-allocated buffer and publish.
 * Returns 1 if P now holds a non-foreign base ready for do_bind.
 */
static int isolate_erw_into_p(uint32_t p_guest, const GwyLoadedModule *gm, uint32_t *inout_base,
                              uint32_t *inout_len, const char *foreign_name) {
    uint32_t want_len;
    uint32_t avoid;
    uint32_t fresh;
    const char *mn;

    if (!gm || !inout_base || !inout_len) return 0;
    mn = gm->resolved_name[0] ? gm->resolved_name : gm->requested_name;
    if (!mn || !strstr(mn, "gamelist")) return 0;
    if (!e10a31b_enabled()) return 0;

    want_len = *inout_len ? *inout_len : k_gamelist_erw_default_len;
    avoid = *inout_base;
    fresh = alloc_distinct_erw(want_len, avoid);
    if (!fresh) {
        printf("[JJFB_E10A31B] milestone=GAMELIST_ERW_HOST_ISOLATE_FAIL module=%s P=0x%X "
               "want_len=0x%X avoid=0x%X evidence=OBSERVED\n",
               mn, p_guest, want_len, avoid);
        fflush(stdout);
        return 0;
    }
    if (!poke_p_er_rw(p_guest, fresh, want_len)) {
        printf("[JJFB_E10A31B] milestone=GAMELIST_ERW_HOST_ISOLATE_FAIL module=%s P=0x%X "
               "note=poke_failed evidence=OBSERVED\n",
               mn, p_guest);
        fflush(stdout);
        return 0;
    }
    printf("[JJFB_E10A31B] milestone=GAMELIST_ERW_HOST_ISOLATED module=%s P=0x%X "
           "old_erw=0x%X new_erw=0x%X len=0x%X foreign_owner=%s evidence=OBSERVED\n",
           mn, p_guest, avoid, fresh, want_len, foreign_name ? foreign_name : "-");
    fflush(stdout);
    e10a31b_note_erw("GAMELIST_ERW_HOST_ISOLATED", mn, p_guest, fresh, want_len, gm->module_id,
                     foreign_name ? foreign_name : "empty_or_zero");
    *inout_base = fresh;
    *inout_len = want_len;
    return 1;
}

static int do_bind(uint32_t p_guest, const char *module_hint, const char *reason) {
    ModuleRegistry *reg;
    const GwyLoadedModule *gm;
    LauncherError err;
    uint32_t base = 0, len = 0;
    const char *mn;
    const char *r = reason && reason[0] ? reason : "mr_c_function_st_metadata_bind";
    const char *foreign_name = NULL;
    int isolated = 0;

    parse_mode();
    if (!ext_er_rw_bind_restore_enabled() || !p_guest) return 0;
    if (!peek_p_fields(p_guest, &base, &len)) return 0;
    if (!base || !len) {
        gm = resolve_module(p_guest, module_hint);
        if (gm && g_bind.mode == GWY_ER_RW_BIND_GAME_PACKAGE) {
            emit_game_p_timeline(pkg_of(gm),
                                 gm->resolved_name[0] ? gm->resolved_name : gm->requested_name,
                                 "P_ZERO_SKIP", p_guest, base, len, gm->data.start_of_er_rw,
                                 gm->data.er_rw_size, 0, 0, "await_guest_fill");
            if (!gm->map.guest_rw_base && !gm->data.start_of_er_rw) {
                printf("[JJFB_GAME_ER_RW_SOURCE_MISSING] package=%s module=%s "
                       "needed=module_map_or_mrpgcmap_metadata map_rw=0x0 P=0x%X "
                       "evidence=TARGET_OBSERVED\n",
                       pkg_of(gm),
                       gm->resolved_name[0] ? gm->resolved_name : gm->requested_name, p_guest);
                fflush(stdout);
            }
        }
        /* E10A-3.1b: empty P after package collision — host-publish isolated ERW. */
        if (gm && isolate_erw_into_p(p_guest, gm, &base, &len, NULL)) {
            isolated = 1;
            r = "e10a31b_host_isolated_erw";
        } else {
            return 0;
        }
    }

    gm = resolve_module(p_guest, module_hint);
    if (!gm) return 0;
    mn = gm->resolved_name[0] ? gm->resolved_name : gm->requested_name;
    if (!name_ok(mn)) return 0;

    reg = gwy_ext_loader_bound_registry();
    if (!reg) return 0;

    /* Refuse to publish a foreign module's ER_RW as this module's registry ERW. */
    if (!isolated && foreign_owner_of(reg, base, gm->module_id, &foreign_name)) {
        printf("[JJFB_E10A31B] milestone=GAMELIST_P_REUSES_FOREIGN_ERW module=%s "
               "P=0x%X foreign_erw=0x%X foreign_owner=%s note=refuse_registry_claim "
               "evidence=OBSERVED\n",
               mn, p_guest, base, foreign_name ? foreign_name : "?");
        fflush(stdout);
        emit_game_p_timeline(pkg_of(gm), mn, "FOREIGN_ERW_REFUSED", p_guest, base, len,
                             base, len, 0, 0, foreign_name);
        /* Prefer restoring already-published own ERW — avoid orphaning timer binding. */
        if (strstr(mn, "gamelist") && gm->data.start_of_er_rw &&
            gm->data.start_of_er_rw != base) {
            uint32_t own = gm->data.start_of_er_rw;
            uint32_t own_len = gm->data.er_rw_size ? gm->data.er_rw_size : 0xA34u;
            if (poke_p_er_rw(p_guest, own, own_len)) {
                printf("[JJFB_E10A31B] milestone=GAMELIST_ERW_RESTORED module=%s P=0x%X "
                       "erw=0x%X len=0x%X note=refuse_second_isolate evidence=OBSERVED\n",
                       mn, p_guest, own, own_len);
                fflush(stdout);
                base = own;
                len = own_len;
                isolated = 1;
                r = "e10a31b_restore_own_erw";
            }
        }
        if (!isolated && isolate_erw_into_p(p_guest, gm, &base, &len, foreign_name)) {
            isolated = 1;
            r = "e10a31b_host_isolated_erw";
        } else if (!isolated) {
            return 0;
        }
    }

    /* Same values already published — refresh var + maybe deferred switch. */
    if (gm->data.start_of_er_rw == base && gm->data.er_rw_size == len) {
        ext_chunk_provider_set_var_fields(p_guest, base, len);
        g_bind.bound = 1;
        g_bind.last_base = base;
        g_bind.last_len = len;
        g_bind.last_module_id = gm->module_id;
        g_bind.last_p = p_guest;
        snprintf(g_bind.last_module, sizeof(g_bind.last_module), "%.47s", mn);
        emit_game_p_timeline(pkg_of(gm), mn, "BIND_REFRESH", p_guest, base, len, base, len, 0, 0,
                             r);
        try_deferred_r9(gm);
        return 0;
    }

    /* Allow update when guest later fills real ER_RW (after temp/zero), or new shell module. */
    if (g_bind.last_module_id && g_bind.last_module_id != gm->module_id)
        g_bind.deferred_switch_done = 0;
    if (gm->data.start_of_er_rw &&
        (gm->data.start_of_er_rw != base || gm->data.er_rw_size != len)) {
        if (!isolated) r = "platform_er_rw_publication_restore";
        g_bind.deferred_switch_done = 0; /* allow switch to updated callee ER_RW */
    }

    launcher_error_clear(&err);
    if (module_registry_set_er_rw(reg, gm->module_id, base, len, GWY_ER_RW_SRC_MR_C_FUNCTION_LOAD,
                                  GWY_ADDR_EV_DOCUMENTED, &err) != L_OK) {
        return 0;
    }
    ext_chunk_provider_set_var_fields(p_guest, base, len);

    g_bind.bound = 1;
    g_bind.last_base = base;
    g_bind.last_len = len;
    g_bind.last_module_id = gm->module_id;
    g_bind.last_p = p_guest;
    snprintf(g_bind.last_module, sizeof(g_bind.last_module), "%.47s", mn);

    /* Re-fetch after mutator. */
    gm = module_registry_find_by_id(reg, g_bind.last_module_id);

    printf("[JJFB_ER_RW_BIND] package=%s module=%s module_id=%llu P=0x%X p_base=0x%X p_len=0x%X "
           "registry_base=0x%X registry_len=0x%X reason=%s evidence=DOCUMENTED\n",
           gm ? pkg_of(gm) : "?", mn, (unsigned long long)g_bind.last_module_id, p_guest, base,
           len, base, len, r);
    fflush(stdout);
    emit_game_p_timeline(gm ? pkg_of(gm) : "?", mn, "ER_RW_BOUND", p_guest, base, len, base, len,
                         0, 0, r);
    if (mn && strstr(mn, "gamelist")) {
        printf("[JJFB_E10A31B] milestone=GAMELIST_ERW_PUBLISHED module=%s P=0x%X erw=0x%X "
               "len=0x%X note=%s evidence=OBSERVED\n",
               mn, p_guest, base, len, isolated ? "host_isolated" : "guest_or_bind");
        fflush(stdout);
        e10a31b_note_erw("GAMELIST_ERW_PUBLISHED", mn, p_guest, base, len, g_bind.last_module_id,
                         isolated ? "host_isolated" : "bind");
    }

    if (gm) try_deferred_r9(gm);
    return 1;
}

int ext_er_rw_bind_restore_ensure_isolated_erw(uint32_t p_guest, const char *module_hint) {
    parse_mode();
    if (!ext_er_rw_bind_restore_enabled() || !p_guest) return 0;
    if (!e10a31b_enabled()) return 0;
    return do_bind(p_guest, module_hint && module_hint[0] ? module_hint : "gamelist.ext",
                   "e10a31b_ensure_isolated_erw");
}

void ext_er_rw_bind_restore_on_p_write(uint32_t p_guest, uint32_t off, uint32_t new_v,
                                       const char *module) {
    parse_mode();
    if (!ext_er_rw_bind_restore_enabled()) return;
    if (off != 0x00u && off != 0x04u) return;
    if (!new_v || !p_guest) return;
    /*
     * Only bind on gated module writes. Nested P is briefly filled by
     * dsm:cfunction.ext with temporary metadata before zero-init — ignore those.
     * game_package: allow unknown writer tag; resolve_module validates callee name.
     */
    if (!module_gated(module) && g_bind.mode != GWY_ER_RW_BIND_GAME_PACKAGE) return;

    do_bind(p_guest, module, "mr_c_function_st_metadata_bind");
}

void ext_er_rw_bind_restore_peek_and_bind(uint32_t p_guest, const char *reason) {
    const char *hint;
    parse_mode();
    if (!ext_er_rw_bind_restore_enabled() || !p_guest) return;
    hint = ext_chunk_provider_last_module();
    if (!hint || !hint[0]) {
        if (g_bind.mode == GWY_ER_RW_BIND_GAME_PACKAGE)
            hint = "robotol.ext";
        else
            hint = "gbrwcore.ext";
    }
    do_bind(p_guest, hint, reason && reason[0] ? reason : "platform_er_rw_publication_restore");
}

void ext_er_rw_bind_restore_finalize(const char *stop_reason) {
    parse_mode();
    if (g_bind.finalized || g_bind.mode == GWY_ER_RW_BIND_OFF) return;
    g_bind.finalized = 1;
    printf("[JJFB_6O_SUMMARY] mode=%s bound=%s module=%s module_id=%llu P=0x%X "
           "er_rw_base=0x%X er_rw_len=0x%X deferred_switch=%s stop=%s evidence=TARGET_OBSERVED\n",
           ext_er_rw_bind_restore_mode_name(g_bind.mode), g_bind.bound ? "yes" : "no",
           g_bind.last_module[0] ? g_bind.last_module : "?",
           (unsigned long long)g_bind.last_module_id, g_bind.last_p, g_bind.last_base,
           g_bind.last_len, g_bind.deferred_switch_done ? "yes" : "no",
           stop_reason ? stop_reason : "?");
    fflush(stdout);
}
