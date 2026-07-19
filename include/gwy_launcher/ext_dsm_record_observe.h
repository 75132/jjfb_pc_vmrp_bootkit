#ifndef GWY_LAUNCHER_EXT_DSM_RECORD_OBSERVE_H
#define GWY_LAUNCHER_EXT_DSM_RECORD_OBSERVE_H

#include "gwy_launcher/ext_object_observe.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_DSM_RECORD_FIELD_SLOTS 17 /* field_00 .. field_40 */

typedef struct DsmModuleRecord {
    uint64_t record_id;
    uint64_t module_id; /* bound ModuleRegistry id when known; 0 = unbound */
    uint32_t guest_base;
    uint32_t size;
    /* Observe-only offsets (bytes); 0xFFFFFFFF = unknown. Not named until proven. */
    uint32_t code_image_field_offset;
    uint32_t dispatch_field_offset;
    uint32_t helper_field_offset;
    uint32_t status_field_offset;
    ExtIdentityConfidence confidence;
    uint32_t field_snap[GWY_DSM_RECORD_FIELD_SLOTS]; /* last seen field_00.. */
    int valid;
} DsmModuleRecord;

typedef enum ExtContractCase {
    CONTRACT_CASE_UNKNOWN = 0,
    CONTRACT_CASE_REGISTER_FINALIZE_MISSING = 1,
    CONTRACT_CASE_WRONG_RECORD_ASSOCIATION = 2,
    CONTRACT_CASE_WRONG_FIELD_READ = 3,
    CONTRACT_CASE_HELPER_ADDRESS_ENCODING = 4,
    CONTRACT_CASE_HELPER_NOT_DISPATCH_TARGET = 5,
    CONTRACT_CASE_GENERIC_HANDOFF_MISSING = 6
} ExtContractCase;

typedef enum ExtEvidenceLevel {
    EV_UNKNOWN = 0,
    EV_INFERRED = 1,
    EV_OBSERVED = 2,
    EV_CROSS_TARGET = 3,
    EV_DOCUMENTED = 4
} ExtEvidenceLevel;

void ext_dsm_record_observe_reset(void);
int ext_dsm_record_contract_enabled(void);
void ext_dsm_record_observe_bind_uc(void *uc);

const char *ext_contract_case_name(ExtContractCase c);
const char *ext_evidence_level_name(ExtEvidenceLevel e);

/* DOCUMENTED mythroad _mr_c_function_new side effects (global helper + P slot). */
void ext_dsm_record_note_cfunction_side_effects(uint32_t helper,
                                                uint32_t p_guest,
                                                uint32_t p_len,
                                                const char *origin);

void ext_dsm_record_note_registered(uint64_t module_id, uint32_t helper);

/* After registration: observe finalize / record update window. */
void ext_dsm_record_note_post_register(uint64_t module_id, uint32_t helper);

void ext_dsm_record_on_code(void *uc,
                            uint64_t module_id,
                            uint32_t pc,
                            const uint32_t regs[16],
                            uint32_t cpsr);

void ext_dsm_record_on_store(uint32_t value,
                             uint32_t dest,
                             uint64_t writer_mid,
                             uint32_t writer_pc,
                             const char *writer_name,
                             uint32_t writer_off);

void ext_dsm_record_on_block_copy(uint32_t dst, uint32_t src, uint32_t len);

/* Second-hop: bind/create record, set dispatch_field_offset, classify contract. */
void ext_dsm_record_on_second_hop(void *uc,
                                  uint32_t caller_pc,
                                  uint32_t target,
                                  uint32_t r0,
                                  uint32_t r1,
                                  uint64_t from_mid,
                                  uint64_t to_mid);

ExtContractCase ext_dsm_record_last_case(void);
int ext_dsm_record_phase6b_b_open(void);

#ifdef __cplusplus
}
#endif

#endif
