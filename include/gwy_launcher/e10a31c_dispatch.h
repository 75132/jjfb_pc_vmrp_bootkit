#ifndef GWY_LAUNCHER_E10A31C_DISPATCH_H
#define GWY_LAUNCHER_E10A31C_DISPATCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1c: Guest dispatch atomicity and post-init provenance.
 * Env: JJFB_E10A31C_MODE=1 | JJFB_E10A31C_INIT_ATOMICITY | JJFB_E10A31C_MEM_GET
 *
 * Product rule: never nest TIMER FIRE inside an active guest helper/init/event.
 * Diagnostic 6→8→0 is labeled FAST_REAL_GAMELIST_INIT_SEQUENCE / NOT_PRODUCT.
 */

typedef enum GwyDispatchKind {
    GWY_DISPATCH_NONE = 0,
    GWY_DISPATCH_DSM_EVENT = 1,
    GWY_DISPATCH_EXT_HELPER = 2,
    GWY_DISPATCH_TIMER = 3,
    GWY_DISPATCH_PLATFORM_CALLBACK = 4,
    GWY_DISPATCH_INIT_SEQUENCE = 5
} GwyDispatchKind;

typedef struct GwyDispatchState {
    uint32_t depth;
    uint32_t helper_depth;
    uint32_t timer_depth;
    uint32_t init_depth;

    uint64_t generation;
    GwyDispatchKind outer_kind;

    uint32_t outer_pc;
    uint32_t outer_lr;
    uint32_t outer_sp;
    uint32_t outer_r9;

    uint64_t package_id;
    uint64_t module_id;

    uint32_t deferred_timer_count;
    uint32_t deferred_event_count;
} GwyDispatchState;

int e10a31c_enabled(void);
void e10a31c_reset(void);
const GwyDispatchState *e10a31c_state(void);

int e10a31c_busy(void);
int e10a31c_should_defer_timer(void);

/* Enter/leave guest-entry boundaries. leave may pump one deferred timer when depth→0. */
void e10a31c_enter(void *uc, GwyDispatchKind kind, uint32_t helper, uint32_t code,
                   uint32_t p_guest, uint32_t erw);
void e10a31c_leave(void *uc, GwyDispatchKind kind);

/* Timer due while busy: record defer, do not deliver. */
void e10a31c_note_timer_defer(void *uc, uint32_t helper);

/* After outermost leave: deliver at most one deferred timer via callback. */
typedef void (*E10a31cTimerPumpFn)(void *uc);
void e10a31c_set_timer_pump(E10a31cTimerPumpFn fn);
int e10a31c_in_deferred_fire(void);

/* Init 6→8→0 transaction (diagnostic reconstruction). */
uint64_t e10a31c_init_tx_begin(void *uc, uint32_t helper, uint32_t p_guest, uint32_t erw);
void e10a31c_init_method_enter(void *uc, uint32_t method, uint32_t helper, uint32_t p_guest,
                               uint32_t erw);
void e10a31c_init_method_return(void *uc, uint32_t method, int32_t ret);
void e10a31c_init_tx_end(void *uc, int complete, const char *note);

int e10a31c_init_sequence_complete(void);
uint64_t e10a31c_init_tx_id(void);

/* mem_get observe (Lane D). */
void e10a31c_mem_get_enter(void *uc, uint32_t size_hint);
void e10a31c_mem_get_return(void *uc, uint32_t ptr, uint32_t len, int ok);

/* Unknown API policy (Lane F). Returns 1 if soft-return allowed; *out_r0 set. */
int e10a31c_unimpl_policy(void *uc, uint32_t slot, const char *name, int32_t *out_r0);

void e10a31c_mark_milestone(const char *name, const char *note);

/* Lane E: process exit / native exception provenance. */
void e10a31c_install_crash_recorder(void);
void e10a31c_note_process_exit(int exit_code, const char *reason);
void e10a31c_note_last_bridge_api(const char *api);

#ifdef __cplusplus
}
#endif

#endif
