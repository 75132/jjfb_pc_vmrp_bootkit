#ifndef GWY_LAUNCHER_PRODUCT_EVENT_QUEUE_BOOTSTRAP_H
#define GWY_LAUNCHER_PRODUCT_EVENT_QUEUE_BOOTSTRAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product Event Queue Bootstrap (TraceQueueBootstrap).
 * Observe-only Round A: normalize bases, B50/B54 write history, 10162/10165
 * contract, list path at 0x312A60, and name the first causal break.
 * Enabled by JJFB_PRODUCT_TRACE_QUEUE_BOOTSTRAP=1.
 */

typedef enum GwyEqbPhase {
    GWY_EQB_PHASE_NONE = 0,
    GWY_EQB_ROBOTOL_BOOTSTRAP,
    GWY_EQB_EXT_INIT_ENTER,
    GWY_EQB_EXT_INIT_LEAVE,
    GWY_EQB_10162_ALLOC,
    GWY_EQB_10162_RETURN,
    GWY_EQB_10162_OWNER_STORE,
    GWY_EQB_10165_ALLOC,
    GWY_EQB_10165_RETURN,
    GWY_EQB_10165_OWNER_STORE,
    GWY_EQB_10140_TIMER,
    GWY_EQB_10102_REGISTER,
    GWY_EQB_FIRST_1E209_REQUEST,
    GWY_EQB_30D2F9_ENTER,
    GWY_EQB_101AB_ENTER,
    GWY_EQB_2E4D6C_ENTER,
    GWY_EQB_312A60_FAULT_OR_RETURN,
    GWY_EQB_PATH_A_BLOCKED
} GwyEqbPhase;

int product_eqb_enabled(void);
void product_eqb_reset(void);
void product_eqb_set_run_id(const char *run_id);
const char *product_eqb_run_id(void);

void product_eqb_bind_uc(void *uc);
void product_eqb_note_er_rw(uint32_t er_rw, const char *tag);
void product_eqb_note_owner_store(uint32_t plat_code, uint32_t guest_ptr, uint32_t store_addr);

void product_eqb_phase(void *uc, GwyEqbPhase phase, uint32_t er_rw, uint32_t r9, uint32_t p_guest,
                       uint32_t ext_chunk, uint32_t helper, uint64_t owner_module_id,
                       uint64_t owner_generation, uint64_t frame_token);

void product_eqb_on_alloc(void *uc, uint32_t plat_code, uint32_t size, uint32_t guest_ptr,
                          uint32_t handler, uint32_t er_rw, uint32_t r9, const uint32_t r[8],
                          uint32_t stack0, uint32_t stack1, uint32_t caller_pc, uint32_t caller_lr);

void product_eqb_on_alloc_stored(void *uc, uint32_t plat_code, uint32_t guest_ptr,
                                 uint32_t store_addr, uint32_t er_rw, uint32_t r9);

void product_eqb_on_platform(uint32_t code, uint32_t app, uint32_t arg2);
void product_eqb_on_path_a_blocked(void *uc, uint32_t er_rw, uint32_t b54, uint32_t owner_store,
                                   uint32_t ctx65, uint32_t ctx62);

void product_eqb_arm_write_hooks(void *uc, uint32_t er_rw);
void product_eqb_arm_code_hooks(void *uc);
void product_eqb_note_list_access(uint32_t pc, uint32_t base, uint32_t offset, uint32_t value,
                                  int is_write, const char *role);

void product_eqb_finalize(void);

/* Live B54 absolute address / value for product Path-A gate helpers. */
uint32_t product_eqb_active_er_rw(void);
uint32_t product_eqb_b54_abs(void);
uint32_t product_eqb_peek_b54(void *uc);
int product_eqb_list_head_ready(void *uc);

#ifdef __cplusplus
}
#endif

#endif
