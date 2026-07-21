#ifndef GWY_LAUNCHER_E10A31A_PRECONT_DIAG_H
#define GWY_LAUNCHER_E10A31A_PRECONT_DIAG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1a: gbrwcore pre-continuation determinism and exit provenance.
 * Env: JJFB_E10A31A_MODE=1 | JJFB_E10A31A_EXIT_TRACE | JJFB_E10A31A_TAIL_TRACE
 */

typedef enum GwyContinueDecision {
    GWY_CONTINUE_READY = 0,
    GWY_CONTINUE_MODE_DISABLED = 1,
    GWY_CONTINUE_ALREADY_CONSUMED = 2,
    GWY_CONTINUE_NOT_ARMED = 3,
    GWY_CONTINUE_TARGET_NOT_OBSERVED = 4,
    GWY_CONTINUE_TARGET_MISSING = 5,
    GWY_CONTINUE_PARAM_MISSING = 6,
    GWY_CONTINUE_WRONG_ACTIVE_PACKAGE = 7,
    GWY_CONTINUE_GBRWCORE_NOT_READY = 8,
    GWY_CONTINUE_EXIT_OWNER_MISMATCH = 9,
    GWY_CONTINUE_STATE_RESET = 10,
    GWY_CONTINUE_UNKNOWN_BLOCKER = 11,
    GWY_CONTINUE_GAMELIST_ALREADY = 12
} GwyContinueDecision;

typedef struct GwyContinueSnapshot {
    GwyContinueDecision decision;
    int shell_mode;
    int armed;
    int target_observed;
    int gbrwcore_started;
    int gbrwcore_first_pc;
    int gbrwcore_exit_seen;
    int already_continued;
    int gamelist_started;

    uint32_t pc;
    uint32_t lr;
    uint32_t sp;
    uint32_t r0_r12[13];
    uint32_t r9;

    uint32_t active_p;
    uint32_t active_chunk;
    uint32_t active_erw;
    uint32_t active_helper;

    uint64_t package_id;
    uint64_t module_id;
    char active_package[128];
    char active_module[64];

    char target[260];
    char param[512];
    char note[96];
} GwyContinueSnapshot;

int e10a31a_enabled(void);
void e10a31a_reset(void);

const char *e10a31a_continue_decision_name(GwyContinueDecision d);

/* Fill snapshot from current shim/state; does not mutate continuation. */
void e10a31a_continue_snapshot_fill(void *uc, GwyContinueSnapshot *out);

/* Log decision to CSV + stdout markers. */
void e10a31a_log_continue_decision(const char *phase, const GwyContinueSnapshot *snap);

/* Runtime stop provenance (first + final). */
void e10a31a_runtime_set_stop(const char *source, const char *reason, void *uc,
                              uint32_t helper, uint32_t p, uint32_t chunk, uint32_t erw,
                              int32_t return_code, const char *detail);

/* Arm gbrwcore tail capture after font-load observation. */
void e10a31a_note_font_load_suc(void *uc);
void e10a31a_on_guest_code(void *uc, uint32_t pc, uint32_t lr, const uint32_t regs[16],
                           const char *module);
void e10a31a_note_platform_api(void *uc, const char *api, uint32_t pc, uint32_t lr,
                               uint32_t r0, int32_t ret);
void e10a31a_note_br_exit_enter(void *uc);
void e10a31a_note_br_exit_fallback(void *uc, const GwyContinueSnapshot *snap);
void e10a31a_note_br_exit_process_exit(int code);

#ifdef __cplusplus
}
#endif

#endif
