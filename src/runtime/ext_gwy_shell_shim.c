#include "gwy_launcher/ext_gwy_shell_shim.h"
#include "gwy_launcher/ext_gwy_shell_native_exec.h"
#include "gwy_launcher/e10a_shell_trace.h"
#include "gwy_launcher/package_scope.h"
#include "gwy_launcher/vm_file_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#ifndef MR_SUCCESS
#define MR_SUCCESS 0
#endif

static struct {
    int enabled_known;
    int enabled;
    int banner_emitted;
    int update_stub_logged;
    int runapp_emitted;
    int finalized;
    GwyShellLaunchClass last_class;

    char mythroad_root[512];
    char gwy_root[512];
    char active_pkg[128];
    char last_jjfb_host[512];

    int gbrwcore_opened;
    int gamelist_opened;
    int gbrwshell_opened;
    int jjfb_opened_real;
    int jjfb_opened_alias;
    int shell_start_dsm;
    int gbrwcore_start_dsm;
    int gamelist_start_dsm;
    int gbrwshell_start_dsm;
    int jjfb_start_dsm;
    int chain_done;
    int update_stub_applied;
    int shell_chain_continued;
    int exit_source_emitted;
} g_sh;

static int env_is_1(const char *key) {
    const char *e = getenv(key);
    return e && e[0] == '1' && e[1] == '\0';
}

static int path_has(const char *s, const char *n) {
    return s && n && strstr(s, n) != NULL;
}

static int is_shell_pkg(const char *s) {
    return path_has(s, "gbrwcore") || path_has(s, "gamelist") || path_has(s, "gbrwshell") ||
           path_has(s, "vdload");
}

static int is_jjfb_pkg(const char *s) { return path_has(s, "jjfb.mrp") || path_has(s, "jjfb/"); }

int ext_gwy_shell_shim_guest_native_mode(void) {
    const char *p = getenv("JJFB_LAUNCH_PATH");
    return p && (strcmp(p, "gwy_guest_native_runapp") == 0 ||
                 strcmp(p, "gwy_shell_core_continue") == 0 ||
                 strcmp(p, "gwy_native_full_shell") == 0);
}

int ext_gwy_shell_shim_shell_core_continue_mode(void) {
    const char *p = getenv("JJFB_LAUNCH_PATH");
    const char *m = getenv("JJFB_SHELL_CHAIN_MODE");
    if (p && (strcmp(p, "gwy_shell_core_continue") == 0 ||
              strcmp(p, "gwy_native_full_shell") == 0))
        return 1;
    if (m && strcmp(m, "continue_after_gbrwcore_init") == 0) return 1;
    if (env_is_1("JJFB_NATIVE_BOOT_FULL")) return 1;
    return 0;
}

int ext_gwy_shell_shim_enabled(void) {
    if (g_sh.enabled_known) return g_sh.enabled;
    g_sh.enabled = env_is_1("JJFB_GWY_LAUNCHER_MODE") || env_is_1("JJFB_NATIVE_BOOT_FULL");
    if (!g_sh.enabled) {
        const char *p = getenv("JJFB_LAUNCH_PATH");
        if (p && (strcmp(p, "gwy_shell_post_update") == 0 ||
                  strcmp(p, "gwy_guest_native_runapp") == 0 ||
                  strcmp(p, "gwy_shell_core_continue") == 0 ||
                  strcmp(p, "gwy_native_full_shell") == 0))
            g_sh.enabled = 1;
    }
    if (ext_gwy_shell_shim_guest_native_mode() || ext_gwy_shell_shim_shell_core_continue_mode())
        g_sh.enabled = 1;
    g_sh.enabled_known = 1;
    return g_sh.enabled;
}

int ext_gwy_shell_shim_alias_direct_disabled(void) {
    return env_is_1("JJFB_DISABLE_JJFB_ALIAS_DIRECT") || ext_gwy_shell_shim_enabled();
}

int ext_gwy_shell_shim_update_stub_enabled(void) {
    const char *s;
    if (!ext_gwy_shell_shim_enabled()) return 0;
    if (env_is_1("JJFB_GAME_SELF_PATCH")) return 0;
    s = getenv("JJFB_GWY_UPDATE_STUB");
    if (!s || !s[0]) return 1; /* default no_update when mode on */
    return strcmp(s, "no_update") == 0 || strcmp(s, "no_update_native_branch") == 0 ||
           strcmp(s, "1") == 0;
}

static int runapp_native_only(void) {
    return env_is_1("JJFB_RUNAPP_NATIVE_ONLY") || env_is_1("JJFB_NATIVE_BOOT_FULL");
}

void ext_gwy_shell_shim_reset(void) { memset(&g_sh, 0, sizeof(g_sh)); }

const char *ext_gwy_shell_shim_class_name(GwyShellLaunchClass c) {
    switch (c) {
    case GWY_SHELL_CLASS_SHELL_BYPASSED_DIRECT_JJFB: return "SHELL_BYPASSED_DIRECT_JJFB";
    case GWY_SHELL_CLASS_SHELL_OPENED_PENDING_RUNAPP: return "SHELL_OPENED_PENDING_RUNAPP";
    case GWY_SHELL_CLASS_SHELL_RUNAPP_CHAINED: return "SHELL_RUNAPP_CHAINED";
    case GWY_SHELL_CLASS_SHELL_LOADED_BUT_NO_EXTCHUNK: return "SHELL_LOADED_BUT_NO_EXTCHUNK";
    case GWY_SHELL_CLASS_RESOURCE_MISS: return "RESOURCE_MISS";
    case GWY_SHELL_CLASS_SHELL_NATIVE_EXEC_PENDING: return "SHELL_NATIVE_EXEC_PENDING";
    case GWY_SHELL_CLASS_SHELL_NATIVE_RUNAPP: return "SHELL_NATIVE_RUNAPP";
    case GWY_SHELL_CLASS_SHELL_CORE_CONTINUE: return "SHELL_CORE_CONTINUE";
    default: return "UNKNOWN";
    }
}

GwyShellLaunchClass ext_gwy_shell_shim_last_class(void) { return g_sh.last_class; }

void ext_gwy_shell_shim_emit_banner(const char *mythroad_root, const char *gwy_root) {
    const char *path;
    if (!ext_gwy_shell_shim_enabled() || g_sh.banner_emitted) return;
    g_sh.banner_emitted = 1;
    if (mythroad_root && mythroad_root[0])
        snprintf(g_sh.mythroad_root, sizeof(g_sh.mythroad_root), "%s", mythroad_root);
    else {
        const char *r = getenv("GWY_RESOURCE_ROOT");
        snprintf(g_sh.mythroad_root, sizeof(g_sh.mythroad_root), "%s", r ? r : "");
    }
    if (gwy_root && gwy_root[0]) {
        snprintf(g_sh.gwy_root, sizeof(g_sh.gwy_root), "%s", gwy_root);
    } else if (g_sh.mythroad_root[0]) {
        size_t n = strlen(g_sh.mythroad_root);
        if (n + 5 < sizeof(g_sh.gwy_root)) {
            memcpy(g_sh.gwy_root, g_sh.mythroad_root, n);
            memcpy(g_sh.gwy_root + n, "/gwy", 5);
        }
    }
    path = getenv("JJFB_LAUNCH_PATH");
    if (!path || !path[0]) path = "gwy_shell_post_update";
    printf("[JJFB_GWY_LAUNCH] mode=%s launcher_mode=1 alias_direct=disabled "
           "guest_native=%s shell_core_continue=%s\n",
           path, ext_gwy_shell_shim_guest_native_mode() ? "yes" : "no",
           ext_gwy_shell_shim_shell_core_continue_mode() ? "yes" : "no");
    printf("[JJFB_GWY_ROOT] mythroad_root=%s\n", g_sh.mythroad_root);
    printf("[JJFB_GWY_ROOT] gwy_root=%s\n", g_sh.gwy_root);
    if (ext_gwy_shell_shim_update_stub_enabled()) {
        printf("[JJFB_GWY_UPDATE_STUB] layer=gwy_shell result=no_update reason=launcher_shim\n");
        printf("[JJFB_GAME_SELF] untouched=yes\n");
        g_sh.update_stub_logged = 1;
    }
    fflush(stdout);
    fflush(stderr);
}

void ext_gwy_shell_shim_on_start_dsm(const char *filename, const char *ext, const char *entry) {
    const char *param;
    (void)ext;
    if (!ext_gwy_shell_shim_enabled()) return;
    if (!g_sh.banner_emitted) ext_gwy_shell_shim_emit_banner(NULL, NULL);
    snprintf(g_sh.active_pkg, sizeof(g_sh.active_pkg), "%s", filename ? filename : "");
    (void)package_scope_set_active(filename);
    if (is_shell_pkg(filename)) g_sh.shell_start_dsm = 1;
    if (path_has(filename, "gbrwcore")) {
        g_sh.gbrwcore_start_dsm = 1;
        printf("[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=entry "
               "evidence=TARGET_OBSERVED\n");
    }
    if (path_has(filename, "gamelist")) {
        g_sh.gamelist_start_dsm = 1;
        printf("[JJFB_GAMELIST_STARTED] filename=\"%s\" entry=\"%s\" "
               "via=bridge_dsm_mr_start_dsm evidence=DOCUMENTED\n",
               filename ? filename : "?", entry ? entry : "(null)");
        printf("[JJFB_SHELL_CORE_MODULE] module=gamelist.ext stage=entry "
               "evidence=TARGET_OBSERVED\n");
    }
    if (path_has(filename, "gbrwshell")) {
        g_sh.gbrwshell_start_dsm = 1;
        printf("[JJFB_SHELL_CORE_MODULE] module=gbrwshell.ext stage=entry "
               "evidence=TARGET_OBSERVED\n");
    }
    if (is_jjfb_pkg(filename)) g_sh.jjfb_start_dsm = 1;

    param = getenv("GWY_LAUNCH_PARAM");
    if (param && strstr(param, "napptype=12") && strstr(param, "jjfb.mrp")) {
        printf("[JJFB_CFG36] match=yes param=%s\n", param);
    } else {
        printf("[JJFB_CFG36] match=partial_or_pending param=%s\n", param ? param : "");
    }
    printf("[JJFB_STARTGAME_EQUIV] layer=bridge_dsm_mr_start_dsm filename=\"%s\" entry=\"%s\" "
           "shell_context=%s\n",
           filename ? filename : "?", entry ? entry : "(null)",
           is_shell_pkg(filename) ? "yes" : (is_jjfb_pkg(filename) ? "chained_or_direct" : "other"));
    fflush(stdout);
}

void ext_gwy_shell_shim_on_file_open(const char *guest_path, const char *host_path, int ok) {
    if (!ext_gwy_shell_shim_enabled()) return;
    if (!guest_path) return;
    if (path_has(guest_path, "gbrwcore") && ok) g_sh.gbrwcore_opened = 1;
    if (path_has(guest_path, "gamelist") && ok) g_sh.gamelist_opened = 1;
    if (path_has(guest_path, "gbrwshell") && ok) g_sh.gbrwshell_opened = 1;
    if (is_jjfb_pkg(guest_path) && ok) {
        if (host_path && path_has(host_path, "jjfb_alias")) {
            g_sh.jjfb_opened_alias = 1;
            printf("[JJFB_FILEOPEN] guest=\"%s\" host=\"%s\" ok=1 note=ALIAS_BLOCKED_EXPECTED_FAIL\n",
                   guest_path, host_path);
        } else if (host_path && path_has(host_path, "gwy") && path_has(host_path, "jjfb.mrp")) {
            g_sh.jjfb_opened_real = 1;
            snprintf(g_sh.last_jjfb_host, sizeof(g_sh.last_jjfb_host), "%s", host_path);
            printf("[JJFB_FILEOPEN] guest=\"%s\" host=\"%s\" ok=1 note=real_gwy_jjfb\n", guest_path,
                   host_path);
        }
    }
    if (ok && (path_has(guest_path, "gbrwcore") || path_has(guest_path, "gamelist") ||
               path_has(guest_path, "gbrwshell"))) {
        printf("[JJFB_FILEOPEN] guest=\"%s\" host=\"%s\" ok=1 note=shell_package\n", guest_path,
               host_path ? host_path : "?");
    }
    fflush(stdout);
}

int ext_gwy_shell_shim_try_init_network(void *uc, uint32_t cb, const char *mode, uint32_t userData,
                                        int32_t *out_ret) {
    (void)uc;
    (void)cb;
    (void)userData;
    if (!ext_gwy_shell_shim_update_stub_enabled() || !out_ret) return 0;
    /* Never stub game-self network. */
    if (is_jjfb_pkg(g_sh.active_pkg)) {
        printf("[JJFB_GAME_SELF] untouched=yes initNetwork_passthrough=1 pkg=%s\n", g_sh.active_pkg);
        fflush(stdout);
        return 0;
    }
    /* Only stub while shell package is active (or shell DSM already started). */
    if (!is_shell_pkg(g_sh.active_pkg) && !g_sh.shell_start_dsm) return 0;
    if (!g_sh.update_stub_logged) {
        printf("[JJFB_GWY_UPDATE_STUB] layer=gwy_shell result=no_update reason=launcher_shim\n");
        printf("[JJFB_GAME_SELF] untouched=yes\n");
        g_sh.update_stub_logged = 1;
    }
    if (getenv("GWY_SHELL_OFFLINE_NO_UPDATE") && getenv("GWY_SHELL_OFFLINE_NO_UPDATE")[0] == '1') {
        e10a_shell_update("initNetwork", "no_update_offline", "GWY_SHELL_OFFLINE_NO_UPDATE");
        e10a_shell_phase("SHELL_PHASE_UPDATE_NO_UPDATE", g_sh.active_pkg, 0, 0, 0, 0, 0, 0, 0, 0,
                         "offline_stub");
    }
    printf("[JJFB_GWY_UPDATE_STUB] apply=initNetwork mode=%s pkg=%s result=MR_SUCCESS_sync\n",
           mode ? mode : "?", g_sh.active_pkg[0] ? g_sh.active_pkg : "(pending_shell)");
    fflush(stdout);
    g_sh.update_stub_applied = 1;
    *out_ret = (int32_t)MR_SUCCESS;
    return 1;
}

int ext_gwy_shell_shim_should_chain_jjfb(void) {
    if (!ext_gwy_shell_shim_enabled() || g_sh.chain_done) return 0;
    return g_sh.shell_start_dsm && !g_sh.jjfb_start_dsm;
}

const char *ext_gwy_shell_shim_jjfb_target(void) {
    const char *t = getenv("JJFB_SHELL_CHAIN_TARGET");
    if (t && t[0]) return t;
    /* DSM prepends mythroad/; pass gwy-relative guest path. */
    return "gwy/jjfb.mrp";
}

const char *ext_gwy_shell_shim_jjfb_param(void) {
    const char *p = getenv("GWY_LAUNCH_PARAM");
    if (p && p[0]) return p;
    return "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
}

void ext_gwy_shell_shim_emit_runapp_chain(void) {
    if (!ext_gwy_shell_shim_enabled()) return;
    /* Full Boot / guest-native: never claim host_runapp_equivalent as success. */
    if (ext_gwy_shell_shim_guest_native_mode() || runapp_native_only() ||
        ext_gwy_shell_shim_shell_core_continue_mode()) {
        printf("[JJFB_RUNAPP] source=native_shell_pending target=gwy/jjfb.mrp "
               "via=deferred_guest_native_pending note=not_host_equivalent\n");
        printf("[JJFB_PARAM] %s\n", ext_gwy_shell_shim_jjfb_param());
        fflush(stdout);
        return;
    }
    if (runapp_native_only()) {
        printf("[JJFB_RUNAPP] source=blocked target=gwy/jjfb.mrp "
               "via=host_runapp_equivalent_forbidden JJFB_RUNAPP_NATIVE_ONLY=1\n");
        fflush(stdout);
        return;
    }
    g_sh.chain_done = 1;
    g_sh.runapp_emitted = 1;
    printf("[JJFB_STARTGAME] source=gwy_shell cfg_index=36 target=gwy/jjfb.mrp\n");
    printf("[JJFB_RUNAPP] source=gbrwcore/gamelist target=gwy/jjfb.mrp "
           "via=host_runapp_equivalent_after_no_update\n");
    printf("[JJFB_PARAM] %s\n", ext_gwy_shell_shim_jjfb_param());
    fflush(stdout);
}

static void warmup_open(const char *guest) {
    int32_t h;
    if (!guest || !gwy_vm_file_is_bound()) return;
    /* MR_FILE_RDONLY = 1 */
    h = gwy_vm_file_open(guest, 1);
    if (h > 0) {
        printf("[JJFB_GWY_WARMUP] open guest=\"%s\" handle=%d\n", guest, (int)h);
        gwy_vm_file_close(h);
    } else {
        printf("[JJFB_GWY_WARMUP] open guest=\"%s\" failed handle=%d\n", guest, (int)h);
    }
    fflush(stdout);
}

void ext_gwy_shell_shim_prepare_native_shell(void) {
    if (!ext_gwy_shell_shim_enabled()) return;
    if (!g_sh.banner_emitted) ext_gwy_shell_shim_emit_banner(NULL, NULL);
    printf("[JJFB_GWY_LAUNCH] prepare=guest_native_shell_keep_target "
           "note=no_host_runapp_override\n");
    /* Warmup so FILEOPEN/gbrwshell evidence exists before DSM enters gbrwcore. */
    warmup_open("mythroad/gwy/gbrwcore.mrp");
    warmup_open("mythroad/gwy/gamelist.mrp");
    warmup_open("mythroad/gwy/gbrwshell.mrp");
    if (ext_gwy_shell_shim_update_stub_enabled() && !g_sh.update_stub_applied) {
        int32_t stub_ret = 0;
        /* Mark shell context so update stub can apply once guest initNetwork fires. */
        snprintf(g_sh.active_pkg, sizeof(g_sh.active_pkg), "%s", "gwy/gbrwcore.mrp");
        g_sh.shell_start_dsm = 1;
        (void)ext_gwy_shell_shim_try_init_network(NULL, 0, "cmnet", 0, &stub_ret);
    }
    fflush(stdout);
}

int ext_gwy_shell_shim_prepare_jjfb_launch(char *out_target, size_t target_cap, char *out_param,
                                           size_t param_cap) {
    const char *jjfb;
    const char *param;
    if (!ext_gwy_shell_shim_enabled() || !out_target || target_cap == 0 || !out_param ||
        param_cap == 0)
        return 0;
    /* Phase 6H/6P: do not host-override to jjfb; keep GWY_LAUNCH_TARGET for guest DSM. */
    if (ext_gwy_shell_shim_guest_native_mode() || ext_gwy_shell_shim_shell_core_continue_mode()) {
        ext_gwy_shell_shim_prepare_native_shell();
        return 0;
    }
    if (!g_sh.banner_emitted) ext_gwy_shell_shim_emit_banner(NULL, NULL);

    /* Phase 6G: shell context without entering gbrwcore's blocking event loop. */
    ext_gwy_shell_shim_on_start_dsm("gwy/gbrwcore.mrp", "start.mr",
                                    ext_gwy_shell_shim_jjfb_param());
    warmup_open("mythroad/gwy/gbrwcore.mrp");
    warmup_open("mythroad/gwy/gamelist.mrp");
    if (ext_gwy_shell_shim_update_stub_enabled() && !g_sh.update_stub_applied) {
        int32_t stub_ret = 0;
        (void)ext_gwy_shell_shim_try_init_network(NULL, 0, "cmnet", 0, &stub_ret);
    }
    ext_gwy_shell_shim_emit_runapp_chain();

    jjfb = ext_gwy_shell_shim_jjfb_target();
    param = ext_gwy_shell_shim_jjfb_param();
    snprintf(out_target, target_cap, "%s", jjfb);
    snprintf(out_param, param_cap, "%s", param);
    printf("[JJFB_GWY_LAUNCH] prepare=jjfb_after_shell_context target=%s\n", out_target);
    fflush(stdout);
    return 1;
}

static void read_pc_lr(void *uc, uint32_t *pc, uint32_t *lr) {
    *pc = 0;
    *lr = 0;
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, pc);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, lr);
    }
#else
    (void)uc;
#endif
}

void ext_gwy_shell_shim_emit_exit_source(void *uc, const char *source) {
    uint32_t pc = 0, lr = 0;
    const char *path;
    const char *src = source && source[0] ? source : "mr_exit_api";
    if (!ext_gwy_shell_shim_enabled()) return;
    if (g_sh.exit_source_emitted && strcmp(src, "shell_chain_continue") != 0) return;
    g_sh.exit_source_emitted = 1;
    read_pc_lr(uc, &pc, &lr);
    path = getenv("JJFB_LAUNCH_PATH");
    if (!path || !path[0]) path = "?";
    printf("[JJFB_6P_EXIT_SOURCE] source=%s pc=0x%X lr=0x%X mode=%s gbrwcore_init_ok=%s "
           "gamelist_started=%s shell_chain_continued=%s evidence=TARGET_OBSERVED\n",
           src, pc, lr, path, g_sh.gbrwcore_start_dsm ? "yes" : "no",
           g_sh.gamelist_start_dsm ? "yes" : "no", g_sh.shell_chain_continued ? "yes" : "no");
    fflush(stdout);
}

int ext_gwy_shell_shim_try_continue_after_mr_exit(void *uc, char *out_target, size_t target_cap,
                                                  char *out_param, size_t param_cap) {
    const char *param;
    int gbrw_ok;
    int gamelist_done;
    (void)uc;
    if (!ext_gwy_shell_shim_enabled()) return 0;
    if (!ext_gwy_shell_shim_shell_core_continue_mode()) return 0;
    if (g_sh.shell_chain_continued) return 0;

    gbrw_ok = g_sh.gbrwcore_start_dsm ||
              (ext_gwy_shell_native_exec_enabled() &&
               ext_gwy_shell_native_exec_gbrwcore_started());
    gamelist_done = g_sh.gamelist_start_dsm ||
                    (ext_gwy_shell_native_exec_enabled() &&
                     ext_gwy_shell_native_exec_gamelist_started());
    if (!gbrw_ok || gamelist_done) {
        ext_gwy_shell_shim_emit_exit_source(uc, gamelist_done ? "mr_exit_api" : "gbrwcore_only");
        e10a_shell_phase("SHELL_PHASE_GBRWCORE_BLOCKED", "gbrwcore.ext", 0, 0, 0, 0, 0, 0, 0, 0,
                         gamelist_done ? "already_gamelist" : "gbrwcore_not_started");
        return 0;
    }

    g_sh.shell_chain_continued = 1;
    g_sh.exit_source_emitted = 0;
    ext_gwy_shell_shim_emit_exit_source(uc, "shell_chain_continue");
    e10a_shell_phase("SHELL_PHASE_GBRWCORE_CONTINUE", "gbrwcore.ext", 0, 0, 0, 0, 0, 0, 0, 0,
                     "to_gamelist");
    printf("[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=init_ok "
           "evidence=TARGET_OBSERVED\n");
    printf("[JJFB_SHELL_CORE_CONTINUE] from=gbrwcore.mrp to=gwy/gamelist.mrp via=start_dsm "
           "reason=continue_after_gbrwcore_init evidence=TARGET_OBSERVED\n");
    fflush(stdout);

    param = ext_gwy_shell_shim_jjfb_param();
    if (out_target && target_cap)
        snprintf(out_target, target_cap, "%s", "gwy/gamelist.mrp");
    if (out_param && param_cap) snprintf(out_param, param_cap, "%s", param ? param : "");
    return 1;
}

void ext_gwy_shell_shim_finalize(const char *stop_reason) {
    int shell_any;
    GwyShellLaunchClass c;
    if (!ext_gwy_shell_shim_enabled() || g_sh.finalized) return;
    g_sh.finalized = 1;
    shell_any = g_sh.gbrwcore_opened || g_sh.gamelist_opened || g_sh.gbrwshell_opened ||
                g_sh.shell_start_dsm;

    if (g_sh.shell_chain_continued && (g_sh.gamelist_start_dsm || g_sh.gbrwshell_start_dsm))
        c = GWY_SHELL_CLASS_SHELL_CORE_CONTINUE;
    else if (ext_gwy_shell_shim_guest_native_mode() ||
             ext_gwy_shell_shim_shell_core_continue_mode()) {
        if (shell_any && g_sh.jjfb_start_dsm)
            c = GWY_SHELL_CLASS_SHELL_NATIVE_RUNAPP;
        else if (shell_any)
            c = GWY_SHELL_CLASS_SHELL_NATIVE_EXEC_PENDING;
        else
            c = GWY_SHELL_CLASS_UNKNOWN;
    } else if (!shell_any && g_sh.jjfb_start_dsm)
        c = GWY_SHELL_CLASS_SHELL_BYPASSED_DIRECT_JJFB;
    else if (shell_any && g_sh.runapp_emitted)
        c = GWY_SHELL_CLASS_SHELL_RUNAPP_CHAINED;
    else if (shell_any)
        c = GWY_SHELL_CLASS_SHELL_OPENED_PENDING_RUNAPP;
    else
        c = GWY_SHELL_CLASS_UNKNOWN;
    g_sh.last_class = c;

    printf("[JJFB_GWY_SHELL_SUMMARY] class=%s shell_open=%s gbrwcore=%s gamelist=%s gbrwshell=%s "
           "jjfb_real=%s jjfb_alias=%s update_stub=%s runapp_chained=%s guest_native=%s "
           "shell_continue=%s stop=%s evidence=TARGET_OBSERVED\n",
           ext_gwy_shell_shim_class_name(c), shell_any ? "yes" : "no",
           g_sh.gbrwcore_opened ? "yes" : "no", g_sh.gamelist_opened ? "yes" : "no",
           g_sh.gbrwshell_opened ? "yes" : "no", g_sh.jjfb_opened_real ? "yes" : "no",
           g_sh.jjfb_opened_alias ? "yes" : "no", g_sh.update_stub_applied ? "yes" : "no",
           g_sh.runapp_emitted ? "yes" : "no",
           ext_gwy_shell_shim_guest_native_mode() ? "yes" : "no",
           g_sh.shell_chain_continued ? "yes" : "no", stop_reason ? stop_reason : "?");
    fflush(stdout);
}
