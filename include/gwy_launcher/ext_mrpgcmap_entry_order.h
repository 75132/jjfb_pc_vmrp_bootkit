#ifndef GWY_LAUNCHER_EXT_MRPGCMAP_ENTRY_ORDER_H
#define GWY_LAUNCHER_EXT_MRPGCMAP_ENTRY_ORDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Phase 6K: MRPGCMAP entry selection / module init order.
 * Modes (JJFB_FIX_MRPGCMAP_ENTRY_ORDER):
 *   observe       — log intended vs observed only
 *   gbrwcore_only — run guest_code_base+8 before nested CFN continuation (gbrwcore)
 *   shell         — same for gbrwcore/gamelist/gbrwshell
 * Never invents P+0xC / R9 / UI. Emulation uses guest_code_base+8 (DOCUMENTED),
 * not the header_entry_candidate symbol (audit).
 */

typedef enum GwyMrpgcmapEntryMode {
    GWY_MRPGCMAP_ENTRY_OFF = 0,
    GWY_MRPGCMAP_ENTRY_OBSERVE = 1,
    GWY_MRPGCMAP_ENTRY_GBRWCORE_ONLY = 2,
    GWY_MRPGCMAP_ENTRY_SHELL = 3
} GwyMrpgcmapEntryMode;

void ext_mrpgcmap_entry_order_reset(void);
void ext_mrpgcmap_entry_order_bind_uc(void *uc);
GwyMrpgcmapEntryMode ext_mrpgcmap_entry_order_mode(void);
int ext_mrpgcmap_entry_order_enabled(void);

void ext_mrpgcmap_entry_order_on_module_mapped(const char *module_name, uint32_t code_base,
                                               uint32_t code_size);
void ext_mrpgcmap_entry_order_on_module_registered(const char *module_name, uint32_t code_base,
                                                   uint32_t helper);
void ext_mrpgcmap_entry_order_on_first_pc(const char *module_name, uint32_t code_base,
                                          uint32_t observed_pc);
void ext_mrpgcmap_entry_order_on_code(void *uc, const char *module_name, uint32_t pc);

/* Call before nested CFN continuation resume / after nested CFN host completes. */
void ext_mrpgcmap_entry_order_before_continuation(void *uc, const char *module_name,
                                                  uint32_t continuation_pc);

void ext_mrpgcmap_entry_order_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr);
void ext_mrpgcmap_entry_order_finalize(const char *stop_reason);

int ext_mrpgcmap_entry_order_entry_hit(void);
int ext_mrpgcmap_entry_order_entry_ran(void);

#ifdef __cplusplus
}
#endif

#endif
