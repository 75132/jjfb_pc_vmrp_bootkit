#ifndef GWY_LAUNCHER_P22H_HELPER_HANDOFF_H
#define GWY_LAUNCHER_P22H_HELPER_HANDOFF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22H-CLEAN: close gamelist helper real caller + init dispatch contract.
 * Env: JJFB_P22H_CLEAN=1
 * Anchor: natural method=1. No Host call of helper / no FAST 6→8→0.
 */

typedef enum {
    P22H_SRC_UNKNOWN = 0,
    P22H_SRC_NATIVE_GUEST = 1,
    P22H_SRC_HOST_BRIDGE_MR_EXTHELPER = 2,
    P22H_SRC_HOST_BRIDGE_EXT_HELPER = 3,
    P22H_SRC_HOST_TIMER_FIRE_EXT = 4,
    P22H_SRC_HOST_MR_EVENT = 5,
    P22H_SRC_HOST_FAST_REAL = 6,
    P22H_SRC_PLATFORM_CALLBACK = 7
} P22hCallSource;

int p22h_enabled(void);
void p22h_reset(void);
void p22h_bind_uc(void *uc);
void p22h_finalize(const char *stop_reason);

void p22h_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, uint64_t module_id,
                          const char *package_owner);
void p22h_note_gamelist_started(void);
void p22h_note_c_function_new(uint32_t helper, uint32_t p_len, uint32_t p_guest, uint32_t rw_base,
                              uint32_t rw_size, const char *origin);

/* Host/Guest helper enter — call even when e10a31d is off. */
void p22h_helper_enter(void *uc, P22hCallSource source, uint32_t helper, uint32_t method,
                       uint32_t p_guest, uint32_t erw, uint32_t input, uint32_t input_len,
                       uint32_t caller_pc, uint32_t caller_lr, const char *host_fn);
void p22h_helper_return(void *uc, uint32_t helper, uint32_t method, int32_t ret);

void p22h_note_entry_begin(uint32_t helper, uint32_t method, uint32_t p_guest, uint32_t input,
                           uint32_t input_len, uint32_t er_rw, uint32_t sp);
void p22h_note_helper_call(uint32_t helper, uint32_t method, int32_t ret_value);
void p22h_note_timer_fire(uint32_t helper, uint32_t p_guest, uint32_t erw, int end);
void p22h_note_mr_event(int32_t event_code, int32_t p0, int32_t p1);

/* Guest code: CALL_SITE / THUNK / resume. */
void p22h_note_guest_boundary(const char *stage, uint32_t helper, uint32_t method, uint32_t pc,
                              uint32_t lr, const uint32_t regs[16], uint32_t cpsr,
                              const char *module, uint64_t module_id, const char *insn,
                              int branch_reg, uint32_t source_mem);

void p22h_note_helper_ptr_write(uint32_t pc, const char *module, uint32_t addr, uint32_t old_v,
                                uint32_t new_v, int src_reg, const char *channel);
void p22h_note_helper_ptr_read(uint32_t pc, const char *module, uint32_t addr, uint32_t value,
                               int dst_reg, const char *channel);
void p22h_note_memcpy(uint32_t dst, uint32_t src, uint32_t n, uint32_t caller_pc);

void p22h_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
