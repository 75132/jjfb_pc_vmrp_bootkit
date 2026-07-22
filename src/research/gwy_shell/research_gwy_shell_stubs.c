/* Auto-generated research-track stubs for product builds.
 * Real implementations live in research_gwy_shell.
 * Regenerate: python tools/gen_research_stubs.py
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "gwy_launcher/e10a31a_precont_diag.h"
#include "gwy_launcher/e10a31b_publication.h"
#include "gwy_launcher/e10a31c_dispatch.h"
#include "gwy_launcher/e10a31d_provenance.h"
#include "gwy_launcher/e10a31e_appinfo.h"
#include "gwy_launcher/e10a31f_failsite.h"
#include "gwy_launcher/e10a31g_strcmp.h"
#include "gwy_launcher/e10a31h_smscfg.h"
#include "gwy_launcher/e10a31j_smscfg_long.h"
#include "gwy_launcher/e10a31l_config_map.h"
#include "gwy_launcher/e10a31m_fail_2e5404.h"
#include "gwy_launcher/e10a31n_post_range.h"
#include "gwy_launcher/e10a31_gamelist_context.h"
#include "gwy_launcher/e10a3_postselect_trace.h"
#include "gwy_launcher/e10a_shell_trace.h"
#include "gwy_launcher/ext_gwy_shell_native_exec.h"
#include "gwy_launcher/ext_gwy_shell_shim.h"
#include "gwy_launcher/jjfb_bmp_meta.h"
#include "gwy_launcher/jjfb_plat_11f00.h"
#include "gwy_launcher/robotol_flag_writer_trace.h"
#include "gwy_launcher/robotol_idle_watch.h"
#include "gwy_launcher/sms_cfg_compat_profile.h"
#include "gwy_launcher/gwy_sms_cfg.h"

int e10a31a_enabled(void) { return 0; }

void e10a31a_reset(void) {}

void e10a31a_continue_snapshot_fill(void *uc, GwyContinueSnapshot *out) {}

void e10a31a_log_continue_decision(const char *phase, const GwyContinueSnapshot *snap) {}

void e10a31a_runtime_set_stop(const char *source, const char *reason, void *uc, uint32_t helper, uint32_t p, uint32_t chunk, uint32_t erw, int32_t return_code, const char *detail) {}

void e10a31a_note_font_load_suc(void *uc) {}

void e10a31a_on_guest_code(void *uc, uint32_t pc, uint32_t lr, const uint32_t regs[16], const char *module) {}

void e10a31a_note_platform_api(void *uc, const char *api, uint32_t pc, uint32_t lr, uint32_t r0, int32_t ret) {}

void e10a31a_note_br_exit_enter(void *uc) {}

void e10a31a_note_br_exit_fallback(void *uc, const GwyContinueSnapshot *snap) {}

void e10a31a_note_br_exit_process_exit(int code) {}

int e10a31b_enabled(void) { return 0; }

void e10a31b_reset(void) {}

void e10a31b_mark(const char *milestone, const char *module, uint32_t helper, uint32_t p_guest, uint32_t chunk_guest, uint32_t erw, uint64_t module_id, const char *note) {}

void e10a31b_note_cfn(const char *module, uint32_t helper, uint32_t p_guest, uint32_t p_len, int p_reused, uint32_t prior_p) {}

void e10a31b_note_chunk(const char *event, const char *module, uint32_t helper, uint32_t p_guest, uint32_t chunk_guest, uint64_t old_mid, uint64_t new_mid, const char *note) {}

void e10a31b_note_erw(const char *event, const char *module, uint32_t p_guest, uint32_t erw, uint32_t erw_len, uint64_t module_id, const char *note) {}

void e10a31b_note_timer_arm(const char *module, uint32_t helper, uint32_t p_guest, uint32_t chunk_guest, uint32_t erw, uint32_t registry_erw, int own_erw) {}

int e10a31c_enabled(void) { return 0; }

void e10a31c_reset(void) {}

int e10a31c_busy(void) { return 0; }

int e10a31c_should_defer_timer(void) { return 0; }

void e10a31c_enter(void *uc, GwyDispatchKind kind, uint32_t helper, uint32_t code, uint32_t p_guest, uint32_t erw) {}

void e10a31c_leave(void *uc, GwyDispatchKind kind) {}

void e10a31c_note_timer_defer(void *uc, uint32_t helper) {}

void e10a31c_set_timer_pump(E10a31cTimerPumpFn fn) {}

int e10a31c_in_deferred_fire(void) { return 0; }

uint64_t e10a31c_init_tx_begin(void *uc, uint32_t helper, uint32_t p_guest, uint32_t erw) { return 0; }

void e10a31c_init_method_enter(void *uc, uint32_t method, uint32_t helper, uint32_t p_guest, uint32_t erw) {}

void e10a31c_init_method_return(void *uc, uint32_t method, int32_t ret) {}

void e10a31c_init_tx_end(void *uc, int complete, const char *note) {}

int e10a31c_init_sequence_complete(void) { return 0; }

uint64_t e10a31c_init_tx_id(void) { return 0; }

void e10a31c_mem_get_enter(void *uc, uint32_t size_hint) {}

void e10a31c_mem_get_return(void *uc, uint32_t ptr, uint32_t len, int ok) {}

int e10a31c_unimpl_policy(void *uc, uint32_t slot, const char *name, int32_t *out_r0) { return 0; }

void e10a31c_mark_milestone(const char *name, const char *note) {}

void e10a31c_install_crash_recorder(void) {}

void e10a31c_note_process_exit(int exit_code, const char *reason) {}

void e10a31c_note_last_bridge_api(const char *api) {}

int e10a31d_enabled(void) { return 0; }

int e10a31d_history_enabled(void) { return 0; }

int e10a31d_method0_trace_enabled(void) { return 0; }

int e10a31d_appinfo_enabled(void) { return 0; }

void e10a31d_reset(void) {}

void e10a31d_on_ext_first_pc(void) {}

void e10a31d_helper_enter(void *uc, E10a31dSource source, uint32_t helper, uint32_t method, uint32_t p_guest, uint32_t erw, uint32_t input, uint32_t input_len, uint32_t caller_pc, uint32_t caller_lr) {}

void e10a31d_helper_return(void *uc, uint32_t helper, uint32_t method, int32_t ret) {}

void e10a31d_method0_trace_arm(void *uc, uint32_t helper) {}

void e10a31d_method0_trace_disarm(void *uc) {}

void e10a31d_note_platform_api(void *uc, const char *api, uint32_t slot, int32_t r0) {}

void e10a31d_after_code6(void *uc, uint32_t erw, uint32_t input_len, int32_t ret) {}

void e10a31d_after_code8(void *uc, uint32_t erw, uint32_t appinfo, uint32_t input_len, int32_t ret) {}

void e10a31d_mark_milestone(const char *name, const char *note) {}

int e10a31e_enabled(void) { return 0; }

void e10a31e_reset(void) {}

void e10a31e_mark_milestone(const char *name, const char *note) {}

void e10a31e_note_metadata(const char *phase) {}

void e10a31e_note_binding(uint32_t guest_ptr, uint32_t appid, uint32_t appver, int source) {}

void e10a31e_before_code8(void *uc, uint32_t helper, uint32_t erw, uint32_t appinfo) {}

void e10a31e_after_code6(void *uc, uint32_t erw, uint32_t input_len, int32_t ret) {}

void e10a31e_after_code8(void *uc, uint32_t erw, uint32_t appinfo, int32_t ret) {}

void e10a31e_after_method0(void *uc, uint32_t helper, int32_t ret) {}

void e10a31e_read_proof_arm(void *uc, uint32_t erw, uint32_t appinfo) {}

void e10a31e_read_proof_disarm(void *uc) {}

void e10a31e_note_ab_case(const char *case_name, int32_t ret6, int32_t ret8, int32_t ret0, uint32_t appid, uint32_t appver, uint32_t fail_pc, const char *fail_class) {}

int e10a31f_enabled(void) { return 0; }

int e10a31f_continue_past_sentinel(void) { return 0; }

int e10a31f_abi_filebuf(void) { return 0; }

void e10a31f_reset(void) {}

void e10a31f_mark_milestone(const char *name, const char *note) {}

int e10a31f_is_neg1_sentinel_store(uint32_t pc, const uint8_t *bytes, uint32_t size) { return 0; }

void e10a31f_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size) {}

void e10a31f_on_method0_return(void *uc, uint32_t helper, int32_t ret) {}

uint32_t e10a31f_method0_input_override(uint32_t default_input) { return 0; }

int e10a31f_should_ignore_neg1_at(uint32_t fail_pc, uint32_t next_pc, const uint8_t *fail_bytes, uint32_t fail_size) { return 0; }

int e10a31g_enabled(void) { return 0; }

void e10a31g_reset(void) {}

void e10a31g_mark_milestone(const char *name, const char *note) {}

void e10a31g_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size) {}

void e10a31g_on_method0_return(void *uc, uint32_t helper, int32_t ret) {}

int e10a31h_enabled(void) { return 0; }

void e10a31h_reset(void) {}

void e10a31h_mark_milestone(const char *name, const char *note) {}

void e10a31h_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size) {}

void e10a31h_on_method0_return(void *uc, uint32_t helper, int32_t ret) {}

int e10a31j_enabled(void) { return 0; }

void e10a31j_reset(void) {}

void e10a31j_bind_uc(void *uc) {}

void e10a31j_mark_milestone(const char *name, const char *note) {}

void e10a31j_on_mr_table(uint32_t mr_table_guest) {}

void e10a31j_on_erw(void *uc, uint32_t erw_base, uint32_t erw_size) {}

void e10a31j_poll_pointer(void *uc, const char *phase, const char *package, const char *module) {}

void e10a31j_on_phase(void *uc, const char *phase, const char *module, const char *package) {}

void e10a31j_on_block_copy(void *uc, uint32_t dst, uint32_t src, uint32_t len) {}

void e10a31j_on_memset(void *uc, uint32_t dst, uint32_t value, uint32_t len) {}

void e10a31j_on_file_read(void *uc, uint32_t dst, uint32_t len, int32_t ret, const char *api) {}

void e10a31j_on_host_api_enter(void *uc, uint32_t slot, const char *name) {}

void e10a31j_on_host_api_leave(void *uc, uint32_t slot, const char *name) {}

void e10a31j_on_unimpl_api(void *uc, uint32_t slot, const char *name) {}

void e10a31j_on_vfs(void *uc, const char *op, const char *module, const char *guest_path, const char *host_path, int exists, int rc, uint32_t pc, uint32_t lr) {}

void e10a31j_on_method0_enter(void *uc, uint32_t helper) {}

void e10a31j_on_method0_return(void *uc, uint32_t helper, int32_t ret) {}

void e10a31j_on_ext_first_pc(void) {}

void e10a31j_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9) {}

int e10a31j_stop_requested(void) { return 0; }

int e10a31l_enabled(void) { return 0; }

void e10a31l_reset(void) {}

void e10a31l_on_method0_enter(void *uc, uint32_t helper) {}

void e10a31l_on_method0_return(void *uc, uint32_t helper, int32_t ret) {}

void e10a31l_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size) {}

void e10a31l_mark_milestone(const char *name, const char *note) {}

E10a31lLhsSource e10a31l_last_gwy_lhs_source(void) { return (E10a31lLhsSource)0; }

int e10a31m_enabled(void) { return 0; }

void e10a31m_reset(void) {}

void e10a31m_on_method0_enter(void *uc, uint32_t helper) {}

void e10a31m_on_method0_return(void *uc, uint32_t helper, int32_t ret) {}

void e10a31m_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t sp, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size) {}

void e10a31m_on_mem_read(void *uc, uint32_t pc, uint32_t addr, uint32_t size, const void *data) {}

void e10a31m_mark_milestone(const char *name, const char *note) {}

int e10a31n_enabled(void) { return 0; }

void e10a31n_reset(void) {}

void e10a31n_on_method0_enter(void *uc, uint32_t helper) {}

void e10a31n_on_method0_return(void *uc, uint32_t helper, int32_t ret) {}

void e10a31n_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t sp, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size) {}

void e10a31n_mark_milestone(const char *name, const char *note) {}

void e10a31n_on_smscfg_bootstrap_applied(uint32_t cfg_guest, uint32_t dsm_generation, const char *source) {}

void e10a31n_on_smscfg_buffer_ready(uint32_t cfg_guest, uint32_t erw_base, uint32_t dsm_generation) {}

int e10a31_enabled(void) { return 0; }

int e10a31_timer_trace(void) { return 0; }

int e10a31_param_trace(void) { return 0; }

int e10a31_cfg_gate_trace(void) { return 0; }

int e10a31_start_dsm_abi_trace(void) { return 0; }

void e10a31_reset(void) {}

void e10a31_mark_ext_first_pc(void) {}

void e10a31_mark_milestone(const char *name, const char *note) {}

void e10a31_timer_arm(void *uc, uint32_t source_pc, uint32_t source_lr, uint32_t source_r9, uint32_t timer_chunk, uint32_t timer_id, uint32_t period_ms) {}

void e10a31_timer_disarm(void) {}

uint32_t e10a31_timer_armed_chunk(void) { return 0; }

int e10a31_timer_fire_resolve(void *uc, uint32_t *out_helper, uint32_t *out_p_guest, uint32_t *out_erw) { return 0; }

int e10a31_timer_arm_observed(void) { return 0; }

int e10a31_timer_fire_observed(void) { return 0; }

int e10a31_timer_fire_count(void) { return 0; }

void e10a31_on_timer_fire(void *uc, uint32_t helper, uint32_t method, uint32_t p_guest, uint32_t erw, int32_t ret) {}

void e10a31_launch_param_mapped(void *uc, uint32_t param_va, uint32_t param_len, const char *entry) {}

void e10a31_note_start_dsm(void *uc, uint32_t start_t_guest, const char *filename, const char *ext, const char *entry) {}

void e10a31_note_cfg_site(void *uc, uint32_t pc, uint32_t base, uint32_t off, const char *site, const char *module, uint32_t r9, uint32_t erw) {}

int e10a3_enabled(void) { return 0; }

void e10a3_reset(void) {}

void e10a3_note_postselect(uint32_t pc, uint32_t lr, const uint32_t r[8], uint32_t r9, uint32_t sp, const char *module, const char *instruction, uint32_t call_target, const char *call_target_module, const char *platform_api, uint32_t event_code, const char *string_arg, const char *state_before, const char *state_after, const char *note) {}

void e10a3_note_event_10180(const char *side, uint32_t pc, uint32_t lr, const char *module, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7, uint32_t r9, uint32_t sp, uint32_t payload_ptr, uint32_t payload_len, const char *payload_hex, const char *decoded, uint32_t callback_ptr, uint32_t userdata, uint32_t state_id, const char *dispatcher, int provider_exists, const char *provider_module, int out_written, int callback_scheduled, uint32_t ret, const char *classification, const char *note) {}

void e10a3_note_named_service(const char *operation, const char *name, const char *caller_module, const char *provider_module, uint32_t target_pc, const char *args, uint32_t callback, int rc, const char *note) {}

void e10a3_note_service_registry(const char *name, uint32_t string_va, uint32_t entry_pc, const char *kind, const char *provider_module, int registered, const char *note) {}

void e10a3_note_wait_state(uint32_t tick, uint32_t callback_pc, uint32_t state_id, uint32_t predicate_pc, uint32_t predicate_value, uint32_t expected_value, const char *responsible, int changed_since_cfg36, const char *note) {}

void e10a3_mark_gamelist_init_ok(void) {}

void e10a3_mark_real_cfg_selected(const char *note) {}

int e10a3_postselect_armed(void) { return 0; }

void e10a3_on_timer_fire(uint32_t helper, uint32_t method, uint32_t p, uint32_t erw, int32_t ret) {}

int e10a_shell_trace_enabled(void) { return 0; }

void e10a_shell_phase(const char *phase, const char *module, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9, uint32_t ac8, const char *note) {}

void e10a_shell_vfs(const char *op, const char *module, const char *guest_path, const char *host_path, int exists, int rc, uint32_t pc, uint32_t lr) {}

void e10a_vfs_set_ctx_from_uc(void *uc, const char *api_name, const char *module, const char *phase, uint32_t path_ptr, const char *arg_source) {}

void e10a_vfs_note_guest_code(const char *module, uint32_t pc) {}

void e10a_note_mr_open_stub(uint32_t stub_addr) {}

int e10a_is_mr_open_stub(uint32_t addr) { return 0; }

void e10a_note_open_enter_branch(uint32_t call_pc, uint32_t target, const char *kind, int rm, uint32_t r0, uint32_t r1, uint32_t r9, uint32_t lr) {}

void e10a_dump_open_enter_branches(const char *why) {}

void e10a_note_guest_pc_sample(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t lr) {}

void e10a_dump_guest_pc_ring(const char *why) {}

void e10a_shell_cfg_runtime(const char *stage, uint32_t cfg_buf_ptr, uint32_t cfg_buf_size, uint32_t record_offset, const char *title, const char *target_path, const char *icon, uint32_t target_ptr, const char *target_bytes, const char *file_api_args, const char *note) {}

void e10a_shell_event(uint32_t event_code, uint32_t handler_pc, uint32_t pc, uint32_t lr, const char *note) {}

void e10a_shell_update(const char *request, const char *response, const char *note) {}

void ext_gwy_shell_native_exec_reset(void) {}

int ext_gwy_shell_native_exec_enabled(void) { return 0; }

void ext_gwy_shell_native_exec_bind_uc(void *uc) {}

void ext_gwy_shell_native_exec_on_start_dsm(const char *filename, const char *ext, const char *entry) {}

void ext_gwy_shell_native_exec_on_launch_param(uint32_t entry_va, const char *entry) {}

void ext_gwy_shell_native_exec_on_slot28(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t ret) {}

void ext_gwy_shell_native_exec_on_file_open(const char *guest_path, const char *host_path, int ok) {}

void ext_gwy_shell_native_exec_on_member_open(const char *guest_path) {}

void ext_gwy_shell_native_exec_on_code_image(uint32_t guest_addr, uint32_t size) {}

void ext_gwy_shell_native_exec_on_module_map(const char *module_name, uint32_t base, uint32_t size) {}

void ext_gwy_shell_native_exec_on_code(void *uc, uint64_t module_id, const char *module_name, uint32_t pc, const uint32_t regs[16]) {}

void ext_gwy_shell_native_exec_on_helper_call(uint32_t helper, uint32_t method, int32_t ret) {}

void ext_gwy_shell_native_exec_on_strcom(uint32_t code, const char *caller) {}

void ext_gwy_shell_native_exec_on_mrc_init(uint32_t pc, int32_t ret) {}

void ext_gwy_shell_native_exec_on_pxc_write(uint32_t old_v, uint32_t new_v, uint32_t pc, uint32_t lr, const char *module) {}

void ext_gwy_shell_native_exec_on_mem_fault(void *uc, uint32_t fault_pc) {}

void ext_gwy_shell_native_exec_finalize(const char *stop_reason) {}

int ext_gwy_shell_native_exec_gate_open(void) { return 0; }

int ext_gwy_shell_native_exec_gbrwcore_started(void) { return 0; }

int ext_gwy_shell_native_exec_gamelist_started(void) { return 0; }

int ext_gwy_shell_native_exec_gbrwcore_pc_hit(void) { return 0; }

GwyShellNativeExecClass ext_gwy_shell_native_exec_last_class(void) { return (GwyShellNativeExecClass)0; }

void ext_gwy_shell_shim_reset(void) {}

int ext_gwy_shell_shim_enabled(void) { return 0; }

int ext_gwy_shell_shim_alias_direct_disabled(void) { return 0; }

int ext_gwy_shell_shim_update_stub_enabled(void) { return 0; }

int ext_gwy_shell_shim_guest_native_mode(void) { return 0; }

int ext_gwy_shell_shim_shell_core_continue_mode(void) { return 0; }

void ext_gwy_shell_shim_emit_banner(const char *mythroad_root, const char *gwy_root) {}

void ext_gwy_shell_shim_on_start_dsm(const char *filename, const char *ext, const char *entry) {}

void ext_gwy_shell_shim_on_file_open(const char *guest_path, const char *host_path, int ok) {}

int ext_gwy_shell_shim_try_init_network(void *uc, uint32_t cb, const char *mode, uint32_t userData, int32_t *out_ret) { return 0; }

int ext_gwy_shell_shim_prepare_jjfb_launch(char *out_target, size_t target_cap, char *out_param, size_t param_cap) { return 0; }

void ext_gwy_shell_shim_prepare_native_shell(void) {}

void ext_gwy_shell_shim_emit_runapp_chain(void) {}

int ext_gwy_shell_shim_should_chain_jjfb(void) { return 0; }

int ext_gwy_shell_shim_try_continue_after_mr_exit(void *uc, char *out_target, size_t target_cap, char *out_param, size_t param_cap) { return 0; }

void ext_gwy_shell_shim_emit_exit_source(void *uc, const char *source) {}

void ext_gwy_shell_shim_finalize(const char *stop_reason) {}

GwyShellLaunchClass ext_gwy_shell_shim_last_class(void) { return (GwyShellLaunchClass)0; }

int ext_gwy_shell_shim_gbrwcore_started_flag(void) { return 0; }

int ext_gwy_shell_shim_gamelist_started_flag(void) { return 0; }

int ext_gwy_shell_shim_continued_flag(void) { return 0; }

void jjfb_bmp_meta_reset(void) {}

void jjfb_bmp_meta_set(uint32_t pixels_va, uint16_t w, uint16_t h, const char *member) {}

void jjfb_bmp_meta_set_pixels(uint32_t pixels_va, uint16_t w, uint16_t h, const char *member, const void *rgb565, size_t nbytes) {}

int jjfb_bmp_meta_get(uint32_t pixels_va, uint16_t *w_out, uint16_t *h_out, char *member_out, size_t member_cap) { return 0; }

size_t jjfb_bmp_meta_copy_pixels(uint32_t pixels_va, void *dst, size_t dst_cap) { return 0; }

void jjfb_e9h_set_blit_fn(JjfbE9hBlitFn fn) {}

int jjfb_e9h_blit_guest_pixels(void *uc, uint32_t pixels_va, int x, int y, int w, int h, const char *member) { return 0; }

void jjfb_e9k_set_hold_fn(JjfbE9kHoldFn fn) {}

void jjfb_e9k_request_hold(const char *reason) {}

int jjfb_e9k_hold_requested(void) { return 0; }

void jjfb_e9n_set_text_draw_fn(JjfbE9nTextDrawFn fn) {}

int jjfb_e9n_host_draw_gbk(int x, int y, const uint8_t *bytes, int nbytes) { return 0; }

void jjfb_plat_11f00_note_guest_cstr(uint32_t str_va, int16_t x, int16_t y, uint32_t color) {}

uint32_t jjfb_plat_11f00_last_guest_cstr(void) { return 0; }

int jjfb_plat_11f00_handle(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0, uint32_t caller_pc, uint32_t caller_lr) { return 0; }

void jjfb_plat_11f00_set_draw_fn(JjfbPlat11f00DrawFn fn) {}

void jjfb_plat_12340_set_measure_fn(JjfbPlat12340MeasureFn fn) {}

int jjfb_plat_12340_handle(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0, uint32_t caller_pc, uint32_t caller_lr, uint32_t sp) { return 0; }

int jjfb_plat_12340_flush_outs(void *uc, uint32_t horiz_ptr, uint32_t vert_ptr) { return 0; }

int jjfb_plat_12340_pending(uint32_t *w_out, uint32_t *h_out, uint32_t *str_va_out) { return 0; }

void jjfb_plat_11f00_note_draw_bbox(int w, int h) {}

void jjfb_plat_11f00_last_draw_bbox(int *w_out, int *h_out) {}

int robotol_flag_writer_trace_enabled(void) { return 0; }

void robotol_flag_writer_trace_reset(void) {}

void robotol_flag_writer_trace_bind_uc(void *uc) {}

void robotol_flag_writer_trace_set_tick(uint32_t tick) {}

void robotol_flag_writer_trace_try_arm(void *uc) {}

void robotol_flag_writer_trace_on_lifecycle(void *uc, uint32_t tick) {}

void robotol_flag_writer_trace_dump_summary(const char *reason) {}

void robotol_flag_writer_trace_on_lifecycle_fault(void *uc, uint32_t tick, int ok, unsigned uc_err, uint32_t pc_after, uint32_t r0_after, uint32_t r9_after, uint32_t sp_after, uint32_t lr_after) {}

int jjfb_e9n_try_plat_11f00_text_draw(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0) { return 0; }

void robotol_flag_writer_e10a_shell_phase(const char *phase) {}

int robotol_idle_watch_enabled(void) { return 0; }

void robotol_idle_watch_reset(void) {}

void robotol_idle_watch_bind_uc(void *uc) {}

void robotol_idle_watch_try_arm(void *uc) {}

void robotol_idle_watch_set_tick(uint32_t tick) {}

void robotol_idle_watch_snap(void *uc, const char *reason) {}

void robotol_idle_watch_note_stage(void *uc, const char *stage) {}

void robotol_idle_watch_try_10165_probe(void *uc) {}

int robotol_idle_watch_drain_order(void) { return 0; }

void robotol_idle_watch_helper_fx_begin(uint32_t r0, uint32_t r1) {}

void robotol_idle_watch_helper_fx_end(uint32_t r0, uint32_t r1, uint32_t ret) {}

void robotol_idle_watch_on_handler_register(uint32_t plat_code, uint32_t family, uint32_t handler) {}

void gwy_sms_cfg_reset(void) {}

int gwy_sms_cfg_diag_minimal_enabled(void) { return 0; }

int gwy_sms_cfg_bootstrap_enabled(void) { return 0; }

int gwy_sms_cfg_resolve(void *uc, uint32_t erw_base, uint32_t erw_size, uint32_t mr_table) { return 0; }

void gwy_sms_cfg_on_erw(void *uc, uint32_t helper, uint32_t erw_base, uint32_t erw_size) {}

int gwy_sms_cfg_ensure_ready(void *uc) { return 0; }

int32_t gwy_sms_cfg_get_bytes(void *uc, int32_t pos, void *dst, int32_t len) { return 0; }

int32_t gwy_sms_cfg_set_bytes(void *uc, int32_t pos, const void *src, int32_t len, GwySmsCfgSource source_tag) { return 0; }

int32_t gwy_sms_cfg_load(void *uc) { return 0; }

int32_t gwy_sms_cfg_save(void *uc) { return 0; }

const char *e10a31a_continue_decision_name(GwyContinueDecision d) {
    (void)d;
    return "stub";
}

const GwyDispatchState *e10a31c_state(void) {
    static GwyDispatchState z;
    memset(&z, 0, sizeof(z));
    return &z;
}

const char *e10a31l_lhs_source_name(E10a31lLhsSource s) {
    (void)s;
    return "stub";
}

const char *ext_gwy_shell_native_exec_class_name(GwyShellNativeExecClass c) {
    (void)c;
    return "stub";
}

const char *ext_gwy_shell_shim_class_name(GwyShellLaunchClass c) {
    (void)c;
    return "stub";
}

const char *ext_gwy_shell_shim_jjfb_target(void) { return ""; }

const char *ext_gwy_shell_shim_jjfb_param(void) { return ""; }

const char *ext_gwy_shell_shim_active_package(void) { return ""; }

const GwySmsCfgState *gwy_sms_cfg_state(void) {
    static GwySmsCfgState z;
    memset(&z, 0, sizeof(z));
    return &z;
}

const SmsCfgCompatProfile *sms_cfg_compat_profiles(uint32_t *out_count) {
    if (out_count) *out_count = 0;
    return 0;
}

const SmsCfgCompatProfile *sms_cfg_compat_select(const uint8_t cfunction_sha256[32],
                                                 uint32_t mr_version, uint32_t cfg_length) {
    (void)cfunction_sha256;
    (void)mr_version;
    (void)cfg_length;
    return 0;
}
