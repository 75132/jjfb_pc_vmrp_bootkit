#ifndef GWY_LAUNCHER_EXT_CHUNK_OBSERVE_H
#define GWY_LAUNCHER_EXT_CHUNK_OBSERVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_CHUNK_OBS_MAX 32
#define GWY_CHUNK_SNAP_BYTES 0x40u

typedef enum GwyChunkOrigin {
    GWY_CHUNK_ORIGIN_UNKNOWN = 0,
    GWY_CHUNK_ORIGIN_DSM_ALLOCATED = 1,
    GWY_CHUNK_ORIGIN_GUEST_ALLOCATED = 2,
    GWY_CHUNK_ORIGIN_COPIED_FROM_TEMPLATE = 3,
    GWY_CHUNK_ORIGIN_LOADER_RETURNED = 4,
    GWY_CHUNK_ORIGIN_FROM_P_PLUS_0C = 5
} GwyChunkOrigin;

typedef enum GwyChunkState {
    GWY_CHUNK_STATE_FIRST_SEEN = 1,
    GWY_CHUNK_STATE_BEFORE_SELECT = 2,
    GWY_CHUNK_STATE_AFTER_SELECT = 3
} GwyChunkState;

typedef struct ExtChunkRecord {
    uint64_t chunk_id;
    uint64_t module_id;
    uint64_t load_id;
    uint32_t guest_base;
    uint32_t size;
    uint32_t creator_pc;
    uint64_t creator_module_id;
    GwyChunkOrigin origin;
    GwyChunkState state;
    uint32_t field_04_raw;
    uint8_t snap[GWY_CHUNK_SNAP_BYTES];
    int snap_valid;
    int field_04_at_first_seen; /* 1 if offset+4 already non-zero at FIRST_SEEN */
    int field_04_write_seen;    /* scalar or block covering +0x4 after arm */
    int block_copy_seen;
    int watch_armed;
    unsigned long long write_hook; /* uc_hook when Unicorn build */
    /* A4: candidate_chunk identity — not confirmed mrc_extChunk until ≥2 evidence bits. */
    unsigned identity_evidence;
    int identity_confidence; /* ExtIdentityConfidence as int to avoid header cycle */
} ExtChunkRecord;

void ext_chunk_observe_reset(void);
void ext_chunk_observe_bind_uc(void *uc);

int ext_chunk_provenance_enabled(void);

const char *gwy_chunk_origin_name(GwyChunkOrigin o);
const char *gwy_chunk_state_name(GwyChunkState s);

/* Register or refresh a chunk candidate; arms full-region write watch when enabled. */
ExtChunkRecord *ext_chunk_observe_register(uint32_t guest_base,
                                           uint32_t size,
                                           GwyChunkOrigin origin,
                                           uint32_t creator_pc,
                                           uint64_t creator_module_id);

ExtChunkRecord *ext_chunk_observe_find(uint32_t guest_base);
ExtChunkRecord *ext_chunk_observe_find_covering(uint32_t addr);

void ext_chunk_observe_bind_module(ExtChunkRecord *rec, uint64_t module_id, uint64_t load_id);

/* Add identity evidence bit; raise confidence only when ≥2 bits set. */
void ext_chunk_observe_add_evidence(ExtChunkRecord *rec, unsigned evidence_bit);

/* Peek guest [base, base+0x40) and emit [CHUNK_SNAPSHOT]. */
void ext_chunk_observe_snapshot(ExtChunkRecord *rec, GwyChunkState stage);

/* Env GWY_CHUNK_PROVENANCE=1: size>=0x2C → register GUEST_ALLOCATED + arm watch. */
void ext_chunk_observe_on_alloc(uint32_t guest_addr, uint32_t size);

/* P+0x0C installed chunk pointer. */
void ext_chunk_observe_on_p_plus_0c(uint32_t chunk_base, uint32_t p_base);

/* Observe-only block copy (from br_memcpy). */
void ext_chunk_observe_block_copy(uint32_t dst, uint32_t src, uint32_t len);

/* DSM select lifecycle: BEFORE then AFTER snapshots + Case A–G. */
void ext_chunk_observe_on_dsm_select(uint32_t chunk_base,
                                     uint32_t field_04,
                                     uint32_t dsm_target,
                                     const char *caller_module,
                                     uint32_t caller_offset);

/* Classify raw field_04 value for logs (observe-only). */
const char *ext_chunk_field_04_class(uint32_t raw);

#ifdef __cplusplus
}
#endif

#endif
