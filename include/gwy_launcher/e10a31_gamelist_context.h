#ifndef GWY_LAUNCHER_E10A31_GAMELIST_CONTEXT_H
#define GWY_LAUNCHER_E10A31_GAMELIST_CONTEXT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stage E10A-3.1: gamelist package-context and launch-param handoff.
 * Env: JJFB_E10A31_MODE=1 | JJFB_E10A31_TIMER_CONTEXT | JJFB_E10A31_PARAM_TRACE |
 *      JJFB_E10A31_CFG_GATE | JJFB_E10A31_START_DSM_ABI
 */

typedef struct GwyTimerBinding {
    uint64_t timer_generation;
    uint32_t timer_id;
    uint32_t period_ms;

    uint64_t package_id;
    uint64_t module_id;

    uint32_t chunk_guest;
    uint32_t helper;
    uint32_t p_guest;
    uint32_t er_rw;

    char package_name[128];
    char module_name[64];

    /* Arm-time owner audit (module_id of each tuple field). */
    uint64_t chunk_module_id;
    uint64_t helper_module_id;
    uint64_t p_module_id;
    uint64_t erw_module_id;

    uint32_t source_pc;
    uint32_t source_lr;
    uint32_t source_r9;
    uint32_t chunk_p_erw;
    uint32_t chunk_var_erw;
    uint32_t registry_erw;
    uint32_t p_generation;
    uint32_t module_generation;
} GwyTimerBinding;

int e10a31_enabled(void);
int e10a31_timer_trace(void);
int e10a31_param_trace(void);
int e10a31_cfg_gate_trace(void);
int e10a31_start_dsm_abi_trace(void);
void e10a31_reset(void);

/* Lane A: corrected milestones (do not emit INIT_COMPLETE from first PC). */
void e10a31_mark_ext_first_pc(void);
void e10a31_mark_milestone(const char *name, const char *note);

/* Lane B/C: package-scoped timer binding (product fix + trace). */
void e10a31_timer_arm(void *uc, uint32_t source_pc, uint32_t source_lr, uint32_t source_r9,
                      uint32_t timer_chunk, uint32_t timer_id, uint32_t period_ms);
void e10a31_timer_disarm(void);
uint32_t e10a31_timer_armed_chunk(void);
/*
 * Resolve fire target from immutable arm binding.
 * Repairs mixed helper/ERW when gamelist module ER_RW is available.
 * Returns 1 if EXT timer dispatch should proceed.
 */
int e10a31_timer_fire_resolve(void *uc, uint32_t *out_helper, uint32_t *out_p_guest,
                              uint32_t *out_erw);
/* 1 if TIMER_ARM seen this run (binding valid with helper+P). */
int e10a31_timer_arm_observed(void);
int e10a31_timer_fire_observed(void);
int e10a31_timer_fire_count(void);
void e10a31_on_timer_fire(void *uc, uint32_t helper, uint32_t method, uint32_t p_guest,
                          uint32_t erw, int32_t ret);

/* Lane D: launch-param memory read hook. */
void e10a31_launch_param_mapped(void *uc, uint32_t param_va, uint32_t param_len,
                                const char *entry);

/* Lane E: MR_START_DSM start_t ABI trace. */
void e10a31_note_start_dsm(void *uc, uint32_t start_t_guest, const char *filename,
                           const char *ext, const char *entry);

/* Lane F: cfg-open gate predecessor annotation. */
void e10a31_note_cfg_site(void *uc, uint32_t pc, uint32_t base, uint32_t off,
                          const char *site, const char *module, uint32_t r9, uint32_t erw);

#ifdef __cplusplus
}
#endif

#endif
