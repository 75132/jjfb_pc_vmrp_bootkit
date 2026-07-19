#ifndef GWY_LAUNCHER_EXT_CFUNCTION_PUBLICATION_AUDIT_H
#define GWY_LAUNCHER_EXT_CFUNCTION_PUBLICATION_AUDIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Phase 6M: cfunction publication / chunk_field_04 source audit (observe-only).
 * Env:
 *   JJFB_CFUNCTION_PUBLICATION_AUDIT=1
 *   JJFB_CHUNK_FIELD04_AUDIT=1
 *   JJFB_P_TIMELINE_TRACE=1
 * Never invents P+0xC / mrc_extChunk.
 */

void ext_cfunction_publication_audit_reset(void);
void ext_cfunction_publication_audit_bind_uc(void *uc);
int ext_cfunction_publication_audit_enabled(void);

void ext_cfunction_publication_audit_on_cfunction_new(uint32_t helper, uint32_t p_len,
                                                     uint32_t p_guest, const char *origin);
void ext_cfunction_publication_audit_on_ext_register(const char *module_name, uint64_t module_id,
                                                     uint32_t helper, uint32_t header_entry,
                                                     uint32_t chunk_field_04);
void ext_cfunction_publication_audit_on_chunk_field04(const char *module_name, uint32_t old_v,
                                                      uint32_t new_v, uint32_t pc,
                                                      const char *source);
void ext_cfunction_publication_audit_on_entry_reconcile(const char *module_name,
                                                        uint32_t chunk_field_04,
                                                        const char *writer_tag);

void ext_cfunction_publication_audit_on_p_write(uint32_t p_guest, uint32_t off, uint32_t old_v,
                                                uint32_t new_v, uint32_t pc, uint32_t lr,
                                                const char *module);

void ext_cfunction_publication_audit_on_code(void *uc, const char *module_name, uint32_t pc,
                                             const uint32_t regs[16], uint32_t cpsr);

void ext_cfunction_publication_audit_on_cross_module(uint32_t from_pc, uint32_t target,
                                                     uint32_t r0, uint32_t r1, uint32_t lr,
                                                     const char *from_mod, const char *to_mod);

void ext_cfunction_publication_audit_finalize(const char *stop_reason);

#ifdef __cplusplus
}
#endif

#endif
