/* Thin ABI for upstream bridge → ExtLoader observation (Gwy link only). */
#ifndef GWY_EXT_OBS_ABI_H
#define GWY_EXT_OBS_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bind Unicorn engine for observe-only GuestCallObserver (Gwy builds). */
void gwy_ext_obs_bind_uc(void *uc);

/* Called from br__mr_c_function_new after P alloc (rw may be 0 until guest fills P). */
void gwy_ext_obs_c_function_new(uint32_t helper,
                                uint32_t p_len,
                                uint32_t p_guest_addr,
                                uint32_t rw_base,
                                uint32_t rw_size,
                                uint32_t stack_base);

/*
 * DOCUMENTED (FixR9 / mythroad): C_FUNCTION_P lives at mr_c_function_load-1 (= image+4).
 * Resolve guest VA for the slot given helper and/or LR. Returns 0 if unknown.
 */
uint32_t gwy_ext_obs_c_function_p_slot_va(uint32_t helper, uint32_t lr);

/* Observe-only log after bridge writes the slot (no uc_mem_* here — audit). */
void gwy_ext_obs_note_c_function_p_slot(uint32_t p_guest, uint32_t helper, uint32_t slot,
                                        uint32_t lr, int wrote);

/* Same as above with explicit origin: HOST_BRIDGE | LOG_PARSE | GUEST_NESTED. */
void gwy_ext_obs_c_function_new_ex(uint32_t helper,
                                   uint32_t p_len,
                                   uint32_t p_guest_addr,
                                   uint32_t rw_base,
                                   uint32_t rw_size,
                                   uint32_t stack_base,
                                   const char *origin);

/* Called when guest may have filled mr_c_function_P (e.g. after load / before helper). */
void gwy_ext_obs_p_update(uint32_t helper,
                          uint32_t rw_base,
                          uint32_t rw_size,
                          uint32_t stack_base);

void gwy_ext_obs_helper_call(uint32_t helper, uint32_t method, int32_t ret_value);

/*
 * Stage E3: queue DOCUMENTED init sequence (case_801 codes 6→8→0) after game_package
 * ER_RW/R9 restore. Delivered on next host mr_extHelper enter (not from Unicorn hooks).
 * Gated by JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE=1.
 * request lives in launcher_core; bridge consumes via take (PE has no reliable weak).
 */
void gwy_ext_obs_request_ext_init_seq(void);
/* Returns 1 once if a pending init-seq was queued; clears the flag. */
int gwy_ext_obs_take_ext_init_seq(void);

void gwy_ext_obs_alloc(uint32_t guest_addr, uint32_t size);

/* Observe-only: guest memcpy/block copy may fill chunk fields. */
void gwy_ext_obs_block_copy(uint32_t dst, uint32_t src, uint32_t len);

/* Guest DSM cacheSync / --- ext image (from br_log parse). */
void gwy_ext_obs_code_image(uint32_t guest_addr, uint32_t size);
/* DOCUMENTED mythroad case800: raw EXT buffer (--- ext: @addr), not cacheSync align-down. */
void gwy_ext_obs_ext_image_raw(uint32_t raw_base);

void gwy_ext_obs_member_open(const char *guest_path);

/* Host pre-runCode entry observe (method/input known). */
void gwy_ext_obs_entry_begin(uint32_t helper,
                             uint32_t method,
                             uint32_t p_guest,
                             uint32_t input,
                             uint32_t input_len,
                             uint32_t er_rw,
                             uint32_t sp);

/* Unicorn mem-invalid → structured fault report (do not map fault VA). */
void gwy_ext_obs_mem_fault(void *uc,
                           uint32_t access_type,
                           uint64_t address,
                           uint32_t size,
                           int64_t value);

/* Phase 6C-D1: observe-only host MAP_FUNC callback frame boundaries (read regs only). */
void gwy_ext_obs_host_callback_enter(void *uc, uint32_t slot_addr, const char *name);
void gwy_ext_obs_host_callback_leave(void *uc, uint32_t slot_addr, const char *name);
void gwy_ext_obs_host_callback_resume(void *uc, uint32_t slot_addr, const char *name);

/* Phase 6C-A: nested EXT R9 switch (MODULE_R9_SWITCH_ONLY). */
int gwy_ext_obs_r9_switch_enter(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                                int call_kind);
/* Resolve callee by helper address (host mr_extHelper path). */
int gwy_ext_obs_r9_switch_enter_helper(void *uc, uint32_t helper_address, int call_kind);
int gwy_ext_obs_r9_switch_leave(void *uc);
void gwy_ext_obs_r9_switch_abort(void *uc, const char *reason);

/* Phase 6C-D2: observe raw R9 writes / emu exit hints from bridge. */
void gwy_ext_obs_r9_write_raw(void *uc, uint32_t new_r9, const char *callsite);
void gwy_ext_obs_emu_exit(int reason);

/* Phase 6D-A: observe unimplemented MAP_FUNC before process exit (no behavior change). */
void gwy_ext_obs_unimplemented_api(void *uc, uint32_t slot_addr, const char *name);

/* Phase 6L: observe mr_exit / mythroad exit (process still exits unless continue). */
void gwy_ext_obs_mr_exit(void *uc);
/* Phase 6P: return 1 if first post-gbrwcore mr_exit was consumed (do not exit(0)). */
int gwy_shell_shim_try_continue_after_mr_exit(void *uc);

/* Phase 6N: platform mrc_extChunk publication (gated by JJFB_EXTCHUNK_PROVIDER). */
void gwy_ext_obs_extchunk_set_sendappevent(uint32_t guest_addr);
void gwy_ext_obs_extchunk_set_mr_table(uint32_t guest_addr);
int gwy_ext_obs_extchunk_want(uint32_t helper);
int gwy_ext_obs_extchunk_try_reuse(void *uc, uint32_t helper, uint32_t p_guest, void *p_host);
int gwy_ext_obs_extchunk_on_c_function_new(void *uc, uint32_t helper, uint32_t p_guest, void *p_host,
                                           void *chunk_host, uint32_t chunk_guest);
void gwy_ext_obs_extchunk_slot28_call(void *uc);

/* Guest heap for platform blobs (registered from bridge: my_mallocExt / toMrpMemAddr). */
typedef void *(*GwyExtObsGuestAllocFn)(uint32_t size);
typedef uint32_t (*GwyExtObsGuestPtrFn)(void *host);
void gwy_ext_obs_set_guest_allocator(GwyExtObsGuestAllocFn alloc, GwyExtObsGuestPtrFn to_guest);

/* Host timer ABI (registered from bridge: timerStart / timerStop / SDL_GetTicks). */
typedef int32_t (*GwyExtObsTimerStartFn)(uint16_t ms);
typedef int32_t (*GwyExtObsTimerStopFn)(void);
typedef uint32_t (*GwyExtObsTimerClockFn)(void);
typedef void (*GwyExtObsTimerDeliverFn)(void *uc);
void gwy_ext_obs_set_timer_fns(GwyExtObsTimerStartFn start, GwyExtObsTimerStopFn stop);
void gwy_ext_obs_set_timer_clock(GwyExtObsTimerClockFn clock_ms);
void gwy_ext_obs_set_timer_deliver(GwyExtObsTimerDeliverFn deliver);
/* Poll due deadline while guest holds emu (call from plat stubs / runCode slices). */
void gwy_ext_obs_timer_poll_uc(void *uc);
/* SDL timer thread: mark due only — never take bridge mutex. */
void gwy_ext_obs_timer_signal_due(void);
/* After start_dsm returns: main loop take_due (1 → call timer() under host lock). */
int gwy_ext_obs_timer_take_due(void);
int gwy_ext_obs_timer_running(void);
/*
 * After take_due with no classic EXT chunk: run registered 0x10140 and re-arm.
 * Returns 1 if handled (lifecycle), 0 if caller should use MR_TIMER / other.
 */
int gwy_ext_obs_lifecycle_on_timer_due(void *uc);
/* Chunk that armed sendAppEvent timer (0 if DSM-only). */
uint32_t gwy_ext_obs_timer_armed_chunk(void);
/* DOCUMENTED EXT fire target: helper + P + ER_RW from armed chunk. */
int gwy_ext_obs_timer_ext_target(uint32_t *out_helper, uint32_t *out_p_guest,
                                 uint32_t *out_erw);

/* Stage E5: host mr_timerStart / mrc timerStart must arm platform deadline too. */
void gwy_ext_obs_timer_host_arm(uint32_t period_ms, const char *route, uint32_t pc);
void gwy_ext_obs_timer_host_disarm(const char *route, uint32_t pc);
int gwy_ext_obs_timer_arm_seen(void);
/* Emit TIMER_ARM_ABSENT once after start_dsm if no arm in the window. */
void gwy_ext_obs_on_start_dsm_return(const char *filename, int32_t ret);
/* ~1Hz summary from host SDL loop (gated by JJFB_POST_START_SCHEDULER_TRACE). */
void gwy_ext_obs_post_start_loop_tick(uint32_t t_ms);

/*
 * Typed sendAppEvent dispatch. Observe slot28, then return guest R0 value.
 * 0x10180 → persistent userinfo blob guest ptr; timer ABI → timerStart/Stop;
 * other codes → 0 (MR_SUCCESS).
 */
uint32_t gwy_ext_obs_sendappevent_dispatch(void *uc);

/* Phase 6F: observe-only start_dsm / file open / plat-testcom peeks. */
void gwy_ext_obs_start_dsm(const char *filename, const char *ext, const char *entry);
/* D5b: guest pointer for entry string after copyStrToMrp. */
void gwy_ext_obs_launch_param_mapped(uint32_t entry_va, const char *entry);
void gwy_ext_obs_file_open(const char *guest_path, int ok);

/* Phase 6G: file open with host path + shell update stub / runapp chain helpers. */
void gwy_ext_obs_file_open_ex(const char *guest_path, const char *host_path, int ok);
int gwy_shell_shim_try_init_network(void *uc, uint32_t cb, const char *mode, uint32_t userData,
                                    int32_t *out_ret);
int gwy_shell_shim_prepare_jjfb_launch(char *out_target, size_t target_cap, char *out_param,
                                       size_t param_cap);
int gwy_shell_shim_should_chain_jjfb(void);
const char *gwy_shell_shim_jjfb_target(void);
const char *gwy_shell_shim_jjfb_param(void);
void gwy_shell_shim_emit_runapp_chain(void);
void gwy_shell_shim_finalize(const char *stop_reason);
const char *gwy_shell_shim_continue_target(void);
const char *gwy_shell_shim_continue_param(void);

#ifdef __cplusplus
}
#endif

#endif
