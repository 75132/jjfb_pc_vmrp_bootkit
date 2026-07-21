#ifndef GWY_LAUNCHER_EXT_GWY_SHELL_SHIM_H
#define GWY_LAUNCHER_EXT_GWY_SHELL_SHIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6G/6H/6P: GWY shell launcher shim (gated; no fake extChunk). */

typedef enum GwyShellLaunchClass {
    GWY_SHELL_CLASS_UNKNOWN = 0,
    GWY_SHELL_CLASS_SHELL_BYPASSED_DIRECT_JJFB = 1,
    GWY_SHELL_CLASS_SHELL_OPENED_PENDING_RUNAPP = 2,
    GWY_SHELL_CLASS_SHELL_RUNAPP_CHAINED = 3,
    GWY_SHELL_CLASS_SHELL_LOADED_BUT_NO_EXTCHUNK = 4,
    GWY_SHELL_CLASS_RESOURCE_MISS = 5,
    /* Phase 6H: host override skipped; guest shell DSM is the launch target. */
    GWY_SHELL_CLASS_SHELL_NATIVE_EXEC_PENDING = 6,
    GWY_SHELL_CLASS_SHELL_NATIVE_RUNAPP = 7,
    /* Phase 6P: host continued into gamelist after gbrwcore mr_exit. */
    GWY_SHELL_CLASS_SHELL_CORE_CONTINUE = 8
} GwyShellLaunchClass;

void ext_gwy_shell_shim_reset(void);
int ext_gwy_shell_shim_enabled(void);
int ext_gwy_shell_shim_alias_direct_disabled(void);
int ext_gwy_shell_shim_update_stub_enabled(void);
/* Phase 6H: true when JJFB_LAUNCH_PATH=gwy_guest_native_runapp. */
int ext_gwy_shell_shim_guest_native_mode(void);
/* Phase 6P: gwy_shell_core_continue / continue_after_gbrwcore_init. */
int ext_gwy_shell_shim_shell_core_continue_mode(void);

/* Emit [JJFB_GWY_LAUNCH] / [JJFB_GWY_ROOT] once VFS root is known. */
void ext_gwy_shell_shim_emit_banner(const char *mythroad_root, const char *gwy_root);

void ext_gwy_shell_shim_on_start_dsm(const char *filename, const char *ext, const char *entry);
void ext_gwy_shell_shim_on_file_open(const char *guest_path, const char *host_path, int ok);

/*
 * Shell-layer initNetwork stub. Returns 1 if handled (*out_ret set).
 * Must not apply while active package is jjfb (game self untouched).
 */
int ext_gwy_shell_shim_try_init_network(void *uc, uint32_t cb, const char *mode, uint32_t userData,
                                        int32_t *out_ret);

/*
 * Phase 6G host path: warmup-open shell packages, emit no_update +
 * startGame/runapp tags, then return jjfb target for the sole DSM start.
 * Returns 0 in Phase 6H/6P guest-native modes (do not override to jjfb).
 */
int ext_gwy_shell_shim_prepare_jjfb_launch(char *out_target, size_t target_cap, char *out_param,
                                           size_t param_cap);
/*
 * Phase 6H/6P: warmup shell packages (incl. gbrwshell) without host runapp tags.
 * Caller keeps GWY_LAUNCH_TARGET (gbrwcore/gamelist) as the DSM start.
 */
void ext_gwy_shell_shim_prepare_native_shell(void);
const char *ext_gwy_shell_shim_jjfb_target(void);
const char *ext_gwy_shell_shim_jjfb_param(void);
void ext_gwy_shell_shim_emit_runapp_chain(void);

/* Legacy: true only if shell DSM already started and jjfb not yet started. */
int ext_gwy_shell_shim_should_chain_jjfb(void);

/*
 * Phase 6P: decide whether first post-gbrwcore mr_exit should continue into gamelist.
 * Returns 1 if continue should be consumed (caller starts next DSM; must not exit(0)).
 * Fills out_target/out_param when non-NULL. Emits EXIT_SOURCE / SHELL_CORE_* tags.
 */
int ext_gwy_shell_shim_try_continue_after_mr_exit(void *uc, char *out_target, size_t target_cap,
                                                  char *out_param, size_t param_cap);
/* Observe-only exit classify when not continuing (real process exit). */
void ext_gwy_shell_shim_emit_exit_source(void *uc, const char *source);

void ext_gwy_shell_shim_finalize(const char *stop_reason);
GwyShellLaunchClass ext_gwy_shell_shim_last_class(void);
const char *ext_gwy_shell_shim_class_name(GwyShellLaunchClass c);

/* E10A-3.1a: read-only gate state for continue diagnostics. */
int ext_gwy_shell_shim_gbrwcore_started_flag(void);
int ext_gwy_shell_shim_gamelist_started_flag(void);
int ext_gwy_shell_shim_continued_flag(void);
const char *ext_gwy_shell_shim_active_package(void);

#ifdef __cplusplus
}
#endif

#endif
