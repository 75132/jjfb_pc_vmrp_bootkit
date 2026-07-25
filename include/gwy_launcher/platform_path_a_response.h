#ifndef GWY_LAUNCHER_PLATFORM_PATH_A_RESPONSE_H
#define GWY_LAUNCHER_PLATFORM_PATH_A_RESPONSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generation-scoped Path-A (0x101AB) response contract.
 *
 * Product default (Task 11 B71):
 *   New Robotol generation → phase=INITIAL_RECORD → first fill embeds lifecycle record
 *   → phase=EMPTY thereafter.
 *
 * V75 empty-first (PRIMING_EMPTY → 0x2FC26C → second record) is retained as an enum /
 * note_delivered path for experiments, but is NOT the product default: current guest
 * hangs inside 0x2FC26C (drawFP/DSM loop) before leave, so E6C init via that path is
 * not yet proven.
 *
 * JJFB_101AB_WITH_RECORD=0/1 remains an A/B override only; product default
 * comes from the compatibility profile path_a_response.initial_record.
 * Never keys off fixed PCs or heap addresses.
 */

#define GWY_PATH_A_REC_NAME_MAX 64

typedef enum GwyPathAResponsePhase {
    GWY_PATH_A_RESPONSE_PRIMING_EMPTY = 0,
    GWY_PATH_A_RESPONSE_INITIAL_RECORD = 1,
    GWY_PATH_A_RESPONSE_EMPTY = 2
} GwyPathAResponsePhase;

typedef struct GwyPathAInitialRecord {
    int enabled;
    uint32_t tag;
    char name[GWY_PATH_A_REC_NAME_MAX];
    char secondary[GWY_PATH_A_REC_NAME_MAX];
    uint32_t field_c;
    uint32_t field_d;
    int deliver_once_per_generation;
} GwyPathAInitialRecord;

typedef struct GwyPathAResponseState {
    uint64_t module_id;
    uint64_t module_generation;
    uint32_t er_rw;
    GwyPathAResponsePhase phase;
    int initial_record_delivered;
} GwyPathAResponseState;

void platform_path_a_response_reset(void);
void platform_path_a_response_set_initial_record(const GwyPathAInitialRecord *rec);
const GwyPathAInitialRecord *platform_path_a_response_initial_record(void);
const GwyPathAResponseState *platform_path_a_response_state(void);

/* Bind/rebind for the live module generation. Generation change resets phase. */
void platform_path_a_response_bind(uint64_t module_id, uint64_t module_generation,
                                   uint32_t er_rw);

/*
 * Decide whether this fill should embed the initial record.
 * Returns 1 = with_record, 0 = empty body.
 */
int platform_path_a_response_decide_with_record(void);

/*
 * Advance phase after a successful guest buffer poke.
 * with_record=0 during PRIMING_EMPTY → INITIAL_RECORD.
 * with_record=1 during INITIAL_RECORD → EMPTY.
 */
void platform_path_a_response_note_delivered(int with_record);

/* 1 when product should deliver the downVersion record on the next 101AB fill. */
int platform_path_a_response_ready_for_record(void);

#ifdef __cplusplus
}
#endif

#endif
