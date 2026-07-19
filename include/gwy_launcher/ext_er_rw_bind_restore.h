#ifndef GWY_LAUNCHER_EXT_ER_RW_BIND_RESTORE_H
#define GWY_LAUNCHER_EXT_ER_RW_BIND_RESTORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Phase 6O: sync guest mr_c_function_st P+0/+4 into module registry + extChunk var fields.
 * Env: JJFB_ER_RW_BIND_RESTORE=off|gbrwcore_only|gwy_shell|shell_core|shell_and_game|game_package
 * Never invents ER_RW addresses; only binds when both P+0 and P+4 are nonzero.
 */

typedef enum GwyErRwBindMode {
    GWY_ER_RW_BIND_OFF = 0,
    GWY_ER_RW_BIND_GBRWCORE_ONLY = 1,
    GWY_ER_RW_BIND_SHELL_CORE = 2, /* gbrwcore + gamelist + gbrwshell */
    GWY_ER_RW_BIND_SHELL_AND_GAME = 3, /* shell_core + mrc_loader/robotol/mmochat */
    GWY_ER_RW_BIND_GAME_PACKAGE = 4    /* mrc_loader/robotol/mmochat only */
} GwyErRwBindMode;

void ext_er_rw_bind_restore_reset(void);
void ext_er_rw_bind_restore_bind_uc(void *uc);

int ext_er_rw_bind_restore_enabled(void);
GwyErRwBindMode ext_er_rw_bind_restore_mode(void);
const char *ext_er_rw_bind_restore_mode_name(GwyErRwBindMode m);

/* Called on P field writes (off 0x00 / 0x04). */
void ext_er_rw_bind_restore_on_p_write(uint32_t p_guest, uint32_t off, uint32_t new_v,
                                       const char *module);

/* Peek P+0/+4 and bind if ready (post-republish / after entry order). */
void ext_er_rw_bind_restore_peek_and_bind(uint32_t p_guest, const char *reason);

void ext_er_rw_bind_restore_finalize(const char *stop_reason);

int ext_er_rw_bind_restore_bound(void);
uint32_t ext_er_rw_bind_restore_last_base(void);
uint32_t ext_er_rw_bind_restore_last_len(void);

#ifdef __cplusplus
}
#endif

#endif
