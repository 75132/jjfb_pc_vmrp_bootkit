#ifndef GWY_LAUNCHER_PRODUCT_P11_CASE9_TRACE_H
#define GWY_LAUNCHER_PRODUCT_P11_CASE9_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* P11: Case-9 runtime contract + late P3 fault provenance.
 * Env: JJFB_P11_MODE=1
 *      JJFB_P11_REPORTS_DIR (default: reports)
 */

int product_p11_enabled(void);

/* 0x10102 registration — capture full regs/stack/owner before host stores handler. */
void product_p11_on_10102_register(void *uc, uint32_t family, uint32_t handler,
                                   uint32_t caller_pc, uint32_t lr, int32_t status_ret,
                                   const char *owner_module, uint64_t owner_module_id,
                                   uint64_t owner_generation, uint32_t er_rw,
                                   uint32_t code_base, uint32_t code_size);

/* Natural family request observed (sendAppEvent → note). */
void product_p11_on_family_request(uint64_t request_id, uint32_t event_code, uint32_t app,
                                   uint32_t handler, uint32_t caller_pc, uint32_t lr);

/* Around guest_memory_uc_run_entry_ex for family deliver. */
void product_p11_case9_deliver_begin(void *uc, uint64_t request_id, uint32_t event_code,
                                     uint32_t app, uint32_t handler, uint32_t r0, uint32_t r1,
                                     uint32_t r2, uint32_t r3, uint32_t r9, uint32_t stop);
void product_p11_case9_deliver_end(void *uc, uint64_t request_id, int ok, unsigned uc_err,
                                   uint32_t pc_after, int32_t ret);

/* Global / late fault — called from gwy_ext_obs_mem_fault. */
void product_p11_on_mem_fault(void *uc, uint32_t access_type, uint64_t address, uint32_t size,
                              int64_t value);

/* Optional: note scheduler continuation after family leave. */
void product_p11_on_scheduler_tick(const char *phase);

#ifdef __cplusplus
}
#endif

#endif
