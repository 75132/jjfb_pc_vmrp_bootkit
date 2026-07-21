#ifndef GWY_LAUNCHER_E10A3_POSTSELECT_TRACE_H
#define GWY_LAUNCHER_E10A3_POSTSELECT_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stage E10A-3: gamelist post-selection service contract instrumentation. */

int e10a3_enabled(void);
void e10a3_reset(void);

/* Lane A: post-cfg / post-init guest control-flow samples. */
void e10a3_note_postselect(uint32_t pc, uint32_t lr, const uint32_t r[8], uint32_t r9,
                           uint32_t sp, const char *module, const char *instruction,
                           uint32_t call_target, const char *call_target_module,
                           const char *platform_api, uint32_t event_code,
                           const char *string_arg, const char *state_before,
                           const char *state_after, const char *note);

/* Lane B: full 0x10180 contract (request + platform + return). */
void e10a3_note_event_10180(const char *side, uint32_t pc, uint32_t lr, const char *module,
                            uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4,
                            uint32_t r5, uint32_t r6, uint32_t r7, uint32_t r9, uint32_t sp,
                            uint32_t payload_ptr, uint32_t payload_len, const char *payload_hex,
                            const char *decoded, uint32_t callback_ptr, uint32_t userdata,
                            uint32_t state_id, const char *dispatcher, int provider_exists,
                            const char *provider_module, int out_written, int callback_scheduled,
                            uint32_t ret, const char *classification, const char *note);

/* Lane C: named-service registry ops. */
void e10a3_note_named_service(const char *operation, const char *name, const char *caller_module,
                              const char *provider_module, uint32_t target_pc, const char *args,
                              uint32_t callback, int rc, const char *note);
void e10a3_note_service_registry(const char *name, uint32_t string_va, uint32_t entry_pc,
                                 const char *kind, const char *provider_module, int registered,
                                 const char *note);

/* Lane E: gamelist timer wait predicate. */
void e10a3_note_wait_state(uint32_t tick, uint32_t callback_pc, uint32_t state_id,
                           uint32_t predicate_pc, uint32_t predicate_value,
                           uint32_t expected_value, const char *responsible,
                           int changed_since_cfg36, const char *note);

void e10a3_mark_gamelist_init_ok(void);
void e10a3_mark_real_cfg_selected(const char *note);
int e10a3_postselect_armed(void);
void e10a3_on_timer_fire(uint32_t helper, uint32_t method, uint32_t p, uint32_t erw, int32_t ret);

#ifdef __cplusplus
}
#endif

#endif
