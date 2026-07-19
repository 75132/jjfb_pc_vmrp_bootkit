#ifndef GWY_LAUNCHER_EXT_GWY_STARTGAME_AUDIT_H
#define GWY_LAUNCHER_EXT_GWY_STARTGAME_AUDIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6F: GWY startGame/runapp context audit — observe-only. */

typedef enum GwyStartgameClass {
    GWY_SG_CLASS_UNKNOWN = 0,
    GWY_SG_SHELL_BYPASSED_DIRECT_JJFB = 1,
    GWY_SG_SHELL_LOADED_BUT_NO_EXTCHUNK = 2,
    GWY_SG_PARAM_MISMATCH = 3,
    GWY_SG_RESOURCE_MISS_BLOCKS_CONTEXT = 4
} GwyStartgameClass;

void ext_gwy_startgame_audit_reset(void);
int ext_gwy_startgame_audit_enabled(void);
void ext_gwy_startgame_audit_bind_uc(void *uc);

const char *ext_gwy_startgame_class_name(GwyStartgameClass c);
GwyStartgameClass ext_gwy_startgame_audit_last_class(void);
int ext_gwy_startgame_context_gate_open(void);

void ext_gwy_startgame_audit_emit_launch_context(void);

void ext_gwy_startgame_audit_on_start_dsm(const char *filename, const char *ext,
                                          const char *entry);

void ext_gwy_startgame_audit_on_file_open(const char *guest_path, int ok);

void ext_gwy_startgame_audit_on_plat_or_testcom(void *uc, const char *api_name, uint32_t slot);

void ext_gwy_startgame_audit_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len);

void ext_gwy_startgame_audit_on_continuation_resume(void *uc, uint64_t module_id, const char *module,
                                                    uint32_t continuation_pc, const uint32_t regs[16],
                                                    uint32_t cpsr);

void ext_gwy_startgame_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                          const uint32_t regs[16], uint32_t cpsr);

void ext_gwy_startgame_audit_finalize(const char *stop_reason);

#ifdef __cplusplus
}
#endif

#endif
