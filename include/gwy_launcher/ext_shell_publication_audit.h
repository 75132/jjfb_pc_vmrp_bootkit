#ifndef GWY_LAUNCHER_EXT_SHELL_PUBLICATION_AUDIT_H
#define GWY_LAUNCHER_EXT_SHELL_PUBLICATION_AUDIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Phase 6J: observe-only P field write-chain (+0/+4/+8/+0xC).
 * Does not invent mrc_extChunk / P+0xC / R9 / UI.
 */

void ext_shell_publication_audit_reset(void);
int ext_shell_publication_audit_enabled(void);
void ext_shell_publication_audit_bind_uc(void *uc);

/* Called from P Unicorn watch on writes (reuse, no second watch). */
void ext_shell_publication_audit_on_p_write(uint32_t p_guest, uint32_t off, uint32_t old_v,
                                            uint32_t new_v, uint32_t pc, uint32_t lr,
                                            const char *module);

void ext_shell_publication_audit_on_p_candidate(uint32_t p_guest);
void ext_shell_publication_audit_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr,
                                              uint32_t p_guest, uint32_t p_plus_c);
void ext_shell_publication_audit_finalize(const char *stop_reason);

#ifdef __cplusplus
}
#endif

#endif
