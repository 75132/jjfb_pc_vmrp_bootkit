#ifndef GWY_LAUNCHER_P22I_CFUNCTION_DISPATCHER_H
#define GWY_LAUNCHER_P22I_CFUNCTION_DISPATCHER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22I-CLEAN: close cfunction helper dispatcher + natural 6→0→1 contract.
 * Env: JJFB_P22I_CLEAN=1
 * NATURAL_ONLY — no Host helper invoke, no FAST 6→8→0, no Guest mutation.
 *
 * Corrects P22H attribution:
 *   helper_entry_pc ≠ caller PC (caller from LR/callsite)
 *   nested call stack (≥16) for return matching
 *   native Guest methods enter method matrix
 *   no hardcoded "method=1 is Host"
 *   METHOD8_REQUIREMENT_UNPROVEN unless branch evidence exists
 */

typedef enum {
    P22I_SRC_UNKNOWN = 0,
    P22I_SRC_NATIVE_GUEST = 1,
    P22I_SRC_HOST_BRIDGE_MR_EXTHELPER = 2,
    P22I_SRC_HOST_BRIDGE_EXT_HELPER = 3,
    P22I_SRC_HOST_TIMER_FIRE_EXT = 4,
    P22I_SRC_HOST_MR_EVENT = 5,
    P22I_SRC_HOST_FAST_REAL = 6,
    P22I_SRC_PLATFORM_CALLBACK = 7
} P22iCallSource;

int p22i_enabled(void);
void p22i_reset(void);
void p22i_bind_uc(void *uc);
void p22i_finalize(const char *stop_reason);

void p22i_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, uint64_t module_id,
                          const char *package_owner);
void p22i_note_gamelist_started(void);
void p22i_note_c_function_new(uint32_t helper, uint32_t p_len, uint32_t p_guest, uint32_t rw_base,
                              uint32_t rw_size, const char *origin);

void p22i_helper_enter(void *uc, P22iCallSource source, uint32_t helper, uint32_t method,
                       uint32_t p_guest, uint32_t erw, uint32_t input, uint32_t input_len,
                       uint32_t caller_pc, uint32_t caller_lr, const char *host_fn);
void p22i_helper_return(void *uc, uint32_t helper, uint32_t method, int32_t ret);

void p22i_note_entry_begin(uint32_t helper, uint32_t method, uint32_t p_guest, uint32_t input,
                           uint32_t input_len, uint32_t er_rw, uint32_t sp);
void p22i_note_helper_call(uint32_t helper, uint32_t method, int32_t ret_value);
void p22i_note_timer_fire(uint32_t helper, uint32_t p_guest, uint32_t erw, int end);
void p22i_note_mr_event(int32_t event_code, int32_t p0, int32_t p1);

void p22i_note_guest_boundary(const char *stage, uint32_t helper, uint32_t method, uint32_t pc,
                              uint32_t lr, const uint32_t regs[16], uint32_t cpsr,
                              const char *module, uint64_t module_id, const char *insn,
                              int branch_reg, uint32_t source_mem);

void p22i_note_memcpy(uint32_t dst, uint32_t src, uint32_t n, uint32_t caller_pc);

void p22i_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr);

#ifdef __cplusplus
}
#endif

#endif
