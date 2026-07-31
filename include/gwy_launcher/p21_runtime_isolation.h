#ifndef GWY_LAUNCHER_P21_RUNTIME_ISOLATION_H
#define GWY_LAUNCHER_P21_RUNTIME_ISOLATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P21: nested runtime frame isolation + fault @0x30D5D2 ownership trace.
 * Env: JJFB_P21_RUNTIME_ISOLATION=1 (auto with P20 + original bootstrap).
 */

int p21_runtime_isolation_enabled(void);
void p21_runtime_isolation_reset(void);
void p21_runtime_isolation_bind_uc(void *uc);
void p21_runtime_isolation_on_module_map(const char *module_name, uint32_t base, uint32_t size);
void p21_runtime_isolation_on_code(void *uc, uint32_t pc, const uint32_t regs[16]);
void p21_runtime_isolation_finalize(const char *stop_reason);

int p21_gate_p_isolated(void);
int p21_gate_parent_p_intact(void);
int p21_gate_callback_r9_parent(void);
int p21_gate_no_30d5d2_fault(void);
int p21_gate_startgame_enter(void);

const char *p21_fault_r9_owner(void);
uint32_t p21_slot_1914_last_writer_pc(void);

#ifdef __cplusplus
}
#endif

#endif
