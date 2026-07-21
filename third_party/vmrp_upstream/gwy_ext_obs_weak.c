#include "./header/gwy_ext_obs_abi.h"

/* Strong unbound stubs for plain build (MinGW PE does not honor ELF weak). */

void gwy_ext_obs_bind_uc(void *uc) {
    (void)uc;
}

void gwy_ext_obs_c_function_new(uint32_t helper,
                                uint32_t p_len,
                                uint32_t p_guest_addr,
                                uint32_t rw_base,
                                uint32_t rw_size,
                                uint32_t stack_base) {
    (void)helper;
    (void)p_len;
    (void)p_guest_addr;
    (void)rw_base;
    (void)rw_size;
    (void)stack_base;
}

int gwy_ext_obs_publish_c_function_p_slot(void *uc, uint32_t p_guest, uint32_t helper) {
    (void)uc;
    (void)p_guest;
    (void)helper;
    return 0;
}

uint32_t gwy_ext_obs_c_function_p_slot_va(uint32_t helper, uint32_t lr) {
    (void)helper;
    (void)lr;
    return 0;
}

void gwy_ext_obs_note_c_function_p_slot(uint32_t p_guest, uint32_t helper, uint32_t slot,
                                        uint32_t lr, int wrote) {
    (void)p_guest;
    (void)helper;
    (void)slot;
    (void)lr;
    (void)wrote;
}

void gwy_ext_obs_c_function_new_ex(uint32_t helper,
                                   uint32_t p_len,
                                   uint32_t p_guest_addr,
                                   uint32_t rw_base,
                                   uint32_t rw_size,
                                   uint32_t stack_base,
                                   const char *origin) {
    (void)helper;
    (void)p_len;
    (void)p_guest_addr;
    (void)rw_base;
    (void)rw_size;
    (void)stack_base;
    (void)origin;
}

void gwy_ext_obs_p_update(uint32_t helper,
                          uint32_t rw_base,
                          uint32_t rw_size,
                          uint32_t stack_base) {
    (void)helper;
    (void)rw_base;
    (void)rw_size;
    (void)stack_base;
}

void gwy_ext_obs_helper_call(uint32_t helper, uint32_t method, int32_t ret_value) {
    (void)helper;
    (void)method;
    (void)ret_value;
}

void gwy_ext_obs_request_ext_init_seq(void) {}

int gwy_ext_obs_take_ext_init_seq(void) {
    return 0;
}

void gwy_ext_obs_alloc(uint32_t guest_addr, uint32_t size) {
    (void)guest_addr;
    (void)size;
}

void gwy_ext_obs_block_copy(uint32_t dst, uint32_t src, uint32_t len) {
    (void)dst;
    (void)src;
    (void)len;
}

void gwy_ext_obs_code_image(uint32_t guest_addr, uint32_t size) {
    (void)guest_addr;
    (void)size;
}

void gwy_ext_obs_ext_image_raw(uint32_t raw_base) {
    (void)raw_base;
}

void gwy_ext_obs_member_open(const char *guest_path) {
    (void)guest_path;
}

void gwy_ext_obs_entry_begin(uint32_t helper,
                             uint32_t method,
                             uint32_t p_guest,
                             uint32_t input,
                             uint32_t input_len,
                             uint32_t er_rw,
                             uint32_t sp) {
    (void)helper;
    (void)method;
    (void)p_guest;
    (void)input;
    (void)input_len;
    (void)er_rw;
    (void)sp;
}

void gwy_ext_obs_mem_fault(void *uc,
                           uint32_t access_type,
                           uint64_t address,
                           uint32_t size,
                           int64_t value) {
    (void)uc;
    (void)access_type;
    (void)address;
    (void)size;
    (void)value;
}

void gwy_ext_obs_host_callback_enter(void *uc, uint32_t slot_addr, const char *name) {
    (void)uc;
    (void)slot_addr;
    (void)name;
}

void gwy_ext_obs_host_callback_leave(void *uc, uint32_t slot_addr, const char *name) {
    (void)uc;
    (void)slot_addr;
    (void)name;
}

void gwy_ext_obs_host_callback_resume(void *uc, uint32_t slot_addr, const char *name) {
    (void)uc;
    (void)slot_addr;
    (void)name;
}

int gwy_ext_obs_r9_switch_enter(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                                int call_kind) {
    (void)uc;
    (void)caller_module_id;
    (void)callee_module_id;
    (void)call_kind;
    return 0;
}

int gwy_ext_obs_r9_switch_enter_helper(void *uc, uint32_t helper_address, int call_kind) {
    (void)uc;
    (void)helper_address;
    (void)call_kind;
    return 0;
}

int gwy_ext_obs_r9_switch_leave(void *uc) {
    (void)uc;
    return 0;
}

void gwy_ext_obs_r9_switch_abort(void *uc, const char *reason) {
    (void)uc;
    (void)reason;
}

void gwy_ext_obs_r9_write_raw(void *uc, uint32_t new_r9, const char *callsite) {
    (void)uc;
    (void)new_r9;
    (void)callsite;
}

int gwy_ext_obs_ensure_dsm_r9(void *uc, uint32_t guest_pc_hint) {
    (void)uc;
    (void)guest_pc_hint;
    return 0;
}

void gwy_ext_obs_emu_exit(int reason) { (void)reason; }

void gwy_ext_obs_unimplemented_api(void *uc, uint32_t slot_addr, const char *name) {
    (void)uc;
    (void)slot_addr;
    (void)name;
}

void gwy_ext_obs_mr_exit(void *uc) { (void)uc; }

int gwy_shell_shim_try_continue_after_mr_exit(void *uc) {
    (void)uc;
    return 0;
}

const char *gwy_shell_shim_continue_target(void) { return "gwy/gamelist.mrp"; }

const char *gwy_shell_shim_continue_param(void) {
    return "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
}

void gwy_ext_obs_e10a31a_br_exit_enter(void *uc) { (void)uc; }
void gwy_ext_obs_e10a31a_br_exit_fallback(void *uc) { (void)uc; }
void gwy_ext_obs_e10a31a_process_exit(int code) { (void)code; }
void gwy_ext_obs_e10a31a_note_font_load(void *uc) { (void)uc; }
void gwy_ext_obs_e10a31a_runtime_stop(const char *source, const char *reason, void *uc,
                                      int32_t return_code, const char *detail) {
    (void)source;
    (void)reason;
    (void)uc;
    (void)return_code;
    (void)detail;
}

void gwy_ext_obs_extchunk_set_sendappevent(uint32_t guest_addr) { (void)guest_addr; }

void gwy_ext_obs_extchunk_set_mr_table(uint32_t guest_addr) { (void)guest_addr; }

int gwy_ext_obs_extchunk_want(uint32_t helper) {
    (void)helper;
    return 0;
}

int gwy_ext_obs_extchunk_try_reuse(void *uc, uint32_t helper, uint32_t p_guest, void *p_host) {
    (void)uc;
    (void)helper;
    (void)p_guest;
    (void)p_host;
    return 0;
}

int gwy_ext_obs_extchunk_on_c_function_new(void *uc, uint32_t helper, uint32_t p_guest, void *p_host,
                                          void *chunk_host, uint32_t chunk_guest) {
    (void)uc;
    (void)helper;
    (void)p_guest;
    (void)p_host;
    (void)chunk_host;
    (void)chunk_guest;
    return 0;
}

void gwy_ext_obs_extchunk_slot28_call(void *uc) { (void)uc; }

void gwy_ext_obs_set_guest_allocator(GwyExtObsGuestAllocFn alloc, GwyExtObsGuestPtrFn to_guest) {
    (void)alloc;
    (void)to_guest;
}

uint32_t gwy_ext_obs_guest_malloc0(uint32_t size) {
    (void)size;
    return 0;
}

void gwy_ext_obs_set_timer_fns(GwyExtObsTimerStartFn start, GwyExtObsTimerStopFn stop) {
    (void)start;
    (void)stop;
}

void gwy_ext_obs_set_timer_clock(GwyExtObsTimerClockFn clock_ms) { (void)clock_ms; }

void gwy_ext_obs_set_timer_deliver(GwyExtObsTimerDeliverFn deliver) { (void)deliver; }

void gwy_ext_obs_timer_poll_uc(void *uc) { (void)uc; }

void gwy_ext_obs_timer_signal_due(void) {}

int gwy_ext_obs_timer_take_due(void) { return 0; }

int gwy_ext_obs_timer_running(void) { return 0; }

int gwy_ext_obs_lifecycle_on_timer_due(void *uc) {
    (void)uc;
    return 0;
}

uint32_t gwy_ext_obs_timer_armed_chunk(void) { return 0; }

int gwy_ext_obs_timer_ext_target(uint32_t *out_helper, uint32_t *out_p_guest,
                                 uint32_t *out_erw) {
    (void)out_helper;
    (void)out_p_guest;
    (void)out_erw;
    return 0;
}

void gwy_ext_obs_timer_host_arm(uint32_t period_ms, const char *route, uint32_t pc) {
    (void)period_ms;
    (void)route;
    (void)pc;
}

void gwy_ext_obs_timer_host_disarm(const char *route, uint32_t pc) {
    (void)route;
    (void)pc;
}

int gwy_ext_obs_timer_arm_seen(void) { return 0; }

int gwy_ext_obs_e10a31_timer_arm_observed(void) { return 0; }

int gwy_ext_obs_e10a31_timer_fire_observed(void) { return 0; }

int gwy_ext_obs_e10a31_timer_fire_count(void) { return 0; }

void gwy_ext_obs_on_start_dsm_return(const char *filename, int32_t ret) {
    (void)filename;
    (void)ret;
}

void gwy_ext_obs_post_start_loop_tick(uint32_t t_ms) { (void)t_ms; }

uint32_t gwy_ext_obs_sendappevent_dispatch(void *uc) {
    (void)uc;
    return 0;
}

void gwy_ext_obs_on_timer_fire_ext(uint32_t helper, uint32_t p_guest, uint32_t erw, int32_t ret) {
    (void)helper;
    (void)p_guest;
    (void)erw;
    (void)ret;
}

void gwy_ext_obs_timer_fire_r9_pin(uint32_t pin_erw, uint32_t forbid_erw) {
    (void)pin_erw;
    (void)forbid_erw;
}

void gwy_ext_obs_timer_fire_r9_unpin(void) {}

uint32_t gwy_ext_obs_timer_fire_r9_pinned(void) { return 0; }

int gwy_ext_obs_timer_fire_r9_guard(void *uc) {
    (void)uc;
    return 0;
}

uint32_t gwy_ext_obs_module_erw_by_name(const char *needle) {
    (void)needle;
    return 0;
}

uint32_t gwy_ext_obs_module_helper_by_name(const char *needle) {
    (void)needle;
    return 0;
}

void gwy_ext_obs_start_dsm(const char *filename, const char *ext, const char *entry) {
    (void)filename;
    (void)ext;
    (void)entry;
}

void gwy_ext_obs_launch_param_mapped(uint32_t entry_va, const char *entry) {
    (void)entry_va;
    (void)entry;
}

void gwy_ext_obs_note_start_dsm_abi(void *uc, uint32_t start_t_guest, const char *filename,
                                    const char *ext, const char *entry) {
    (void)uc;
    (void)start_t_guest;
    (void)filename;
    (void)ext;
    (void)entry;
}

void gwy_ext_obs_file_open(const char *guest_path, int ok) {
    (void)guest_path;
    (void)ok;
}

void gwy_ext_obs_file_open_ex(const char *guest_path, const char *host_path, int ok) {
    (void)guest_path;
    (void)host_path;
    (void)ok;
}

int gwy_shell_shim_try_init_network(void *uc, uint32_t cb, const char *mode, uint32_t userData,
                                    int32_t *out_ret) {
    (void)uc;
    (void)cb;
    (void)mode;
    (void)userData;
    (void)out_ret;
    return 0;
}

int gwy_shell_shim_prepare_jjfb_launch(char *out_target, size_t target_cap, char *out_param,
                                       size_t param_cap) {
    (void)out_target;
    (void)target_cap;
    (void)out_param;
    (void)param_cap;
    return 0;
}

int gwy_shell_shim_should_chain_jjfb(void) { return 0; }

const char *gwy_shell_shim_jjfb_target(void) { return "gwy/jjfb.mrp"; }

const char *gwy_shell_shim_jjfb_param(void) {
    return "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
}

void gwy_shell_shim_emit_runapp_chain(void) {}

void gwy_shell_shim_finalize(const char *stop_reason) { (void)stop_reason; }
