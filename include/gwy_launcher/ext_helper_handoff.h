#ifndef GWY_LAUNCHER_EXT_HELPER_HANDOFF_H
#define GWY_LAUNCHER_EXT_HELPER_HANDOFF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Observe-only: neutral DSM module-record shape until fields are proven. */
typedef struct UnknownDsmModuleRecord {
    uint32_t guest_base;
    uint32_t size;
    uint32_t dispatch_target;
    uint32_t registered_helper_candidate;
    uint32_t module_handle;
    uint32_t flags;
} UnknownDsmModuleRecord;

typedef enum ExtHandoffCase {
    HANDOFF_CASE_UNKNOWN = 0,
    HANDOFF_CASE_RETURN_LOST = 1,
    HANDOFF_CASE_FIELD_NOT_UPDATED = 2,
    HANDOFF_CASE_WRONG_FIELD_READ = 3,
    HANDOFF_CASE_WRONG_HELPER_IDENTITY = 4,
    HANDOFF_CASE_MISSING_TRAMPOLINE = 5,
    HANDOFF_CASE_CROSS_TARGET_DISPROVES = 6
} ExtHandoffCase;

void ext_helper_handoff_reset(void);
int ext_helper_handoff_enabled(void);
void ext_helper_handoff_bind_uc(void *uc);

const char *ext_handoff_case_name(ExtHandoffCase c);

/*
 * CFUNCTION_NEW ENTER — origin: HOST_BRIDGE | GUEST_NESTED | LOG_PARSE
 * regs may be NULL; helper is typically r0 (input), not return.
 */
void ext_helper_handoff_cfunction_enter(const char *origin,
                                        uint32_t helper,
                                        uint32_t p_len,
                                        uint32_t p_guest,
                                        const uint32_t *regs,
                                        uint32_t cpsr,
                                        uint32_t caller_pc,
                                        const char *caller_module,
                                        uint32_t caller_offset,
                                        const char *module_name);

/* Called after registry registration confirms helper. */
void ext_helper_handoff_note_registered(uint64_t module_id,
                                        uint32_t helper,
                                        const char *module_name);

/* Per-instruction: EXIT detect + STR of helper; LDR provenance helpers. */
void ext_helper_handoff_on_code(void *uc,
                                uint64_t module_id,
                                uint32_t pc,
                                const uint32_t regs[16],
                                uint32_t cpsr);

void ext_helper_handoff_on_store(uint32_t value,
                                 uint32_t dest,
                                 uint64_t writer_mid,
                                 uint32_t writer_pc,
                                 const char *writer_name,
                                 uint32_t writer_off);

void ext_helper_handoff_on_block_copy(uint32_t dst, uint32_t src, uint32_t len);

/* DSM second-hop: reverse field + provenance + ENTRY_NATURE + HANDOFF_CASE. */
void ext_helper_handoff_on_second_hop(void *uc,
                                      uint32_t caller_pc,
                                      uint32_t target,
                                      uint32_t r0,
                                      uint32_t r1,
                                      uint64_t from_mid,
                                      uint64_t to_mid);

ExtHandoffCase ext_helper_handoff_last_case(void);

#ifdef __cplusplus
}
#endif

#endif
