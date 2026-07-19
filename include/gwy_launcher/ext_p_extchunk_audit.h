#ifndef GWY_LAUNCHER_EXT_P_EXTCHUNK_AUDIT_H
#define GWY_LAUNCHER_EXT_P_EXTCHUNK_AUDIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6E: P.mrc_extChunk (P+0xC) provider audit — observe-only. */

typedef enum ExtchunkAuditClass {
    EXTCHUNK_CLASS_UNKNOWN = 0,
    EXTCHUNK_NEVER_WRITTEN = 1,
    EXTCHUNK_WRITE_AFTER_FAULT_WINDOW = 2,
    EXTCHUNK_PROVIDER_PATH_SKIPPED = 3,
    EXTCHUNK_GWY_CONTEXT_HYPOTHESIS = 4,
    EXTCHUNK_FILLED_BUT_NULL_AT_USE = 5
} ExtchunkAuditClass;

void ext_p_extchunk_audit_reset(void);
int ext_p_extchunk_audit_enabled(void);
void ext_p_extchunk_audit_bind_uc(void *uc);

const char *ext_p_extchunk_class_name(ExtchunkAuditClass c);
ExtchunkAuditClass ext_p_extchunk_audit_last_class(void);
int ext_p_extchunk_gate_open(void);
int ext_p_extchunk_audit_armed(void);

void ext_p_extchunk_audit_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                         uint32_t rw_base, uint32_t rw_size);

void ext_p_extchunk_audit_on_module_event(uint64_t module_id, const char *module_name,
                                          const char *phase_tag);

void ext_p_extchunk_audit_on_continuation_resume(void *uc, uint64_t module_id, const char *module,
                                                 uint32_t call_pc, uint32_t continuation_pc,
                                                 const uint32_t regs[16], uint32_t sp, uint32_t lr,
                                                 uint32_t cpsr);

void ext_p_extchunk_audit_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                  uint32_t cpsr);

void ext_p_extchunk_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                       const uint32_t regs[16], uint32_t cpsr);

void ext_p_extchunk_audit_finalize(const char *stop_reason);

#ifdef __cplusplus
}
#endif

#endif
