#ifndef GWY_LAUNCHER_BRIDGE_ENTRY_PROVENANCE_H
#define GWY_LAUNCHER_BRIDGE_ENTRY_PROVENANCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P15-GATE: Bridge Entry Provenance Ring.
 * Env: JJFB_BRIDGE_ENTRY_PROV=1
 * Optional: JJFB_BRIDGE_ENTRY_PROV_DIR=out/p15
 *
 * Watches mr_table MAP_FUNC/MAP_DATA in the Case-9 aftermath window and
 * classifies genuine calls vs stale-LR / table fall-through.
 * Observe-only; does not implement mr_plat.
 */

void bridge_entry_prov_reset(void);
void bridge_entry_prov_bind_uc(void *uc);
void bridge_entry_prov_set_run_context(uint32_t depth, uint64_t serial);

/* True when JJFB_BRIDGE_ENTRY_PROV=1. */
int bridge_entry_prov_enabled(void);

/* Guest CODE sample (from guest_call_observer); no-op when disabled. */
void bridge_entry_prov_on_guest_code(void *uc, uint32_t pc, uint32_t size, const uint32_t regs[16],
                                     uint32_t cpsr, const char *module_name);

/* MAP_FUNC enter / leave / unimplemented (mr_plat). */
void bridge_entry_prov_on_host_enter(void *uc, uint32_t slot_addr, const char *api_name);
void bridge_entry_prov_on_host_leave(void *uc, uint32_t slot_addr, const char *api_name,
                                     uint32_t leave_lr, uint32_t return_r0);
void bridge_entry_prov_on_unimplemented(void *uc, uint32_t slot_addr, const char *api_name);

/* MAP_DATA / unregistered stub executed as code (table walk). */
void bridge_entry_prov_on_data_exec(void *uc, uint32_t slot_addr, const char *name);

/* Nested runCode context save/restore audit. */
void bridge_entry_prov_on_nest_save(void *uc, const char *site, const uint32_t regs17[17]);
void bridge_entry_prov_on_nest_restore(void *uc, const char *site, const uint32_t saved17[17]);

/* Flush CSV under out dir (also called automatically on each entry). */
void bridge_entry_prov_flush(void);

#ifdef __cplusplus
}
#endif

#endif
