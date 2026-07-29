#ifndef GWY_LAUNCHER_PLATFORM_101AB_FRAME_H
#define GWY_LAUNCHER_PLATFORM_101AB_FRAME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Host-side mirror of the robotol 0x101AB / 0x30D24C framing contract.
 *
 * Observed layout (TARGET_OBSERVED + robotol.ext @0x30D24C → 0x2E4D6C):
 *   u8  type
 *   BE32 payload_length
 *   BE32 header
 *   repeat while payload bytes remain:
 *     BE32 body_size
 *     BE16 event_code
 *     body[body_size - 2]
 *
 * Decoder is verify/log only — never mutates guest memory.
 */

#define GWY_101AB_MAX_RECORDS 8
#define GWY_101AB_MAX_BODY 4096

typedef enum Gwy101AbDecodeStatus {
    GWY_101AB_DECODE_OK = 0,
    GWY_101AB_DECODE_TRUNCATED = 1,
    GWY_101AB_DECODE_BAD_OUTER_LEN = 2,
    GWY_101AB_DECODE_BODY_SIZE_LT2 = 3,
    GWY_101AB_DECODE_PARTIAL_RECORD = 4,
    GWY_101AB_DECODE_NULL = 5,
    GWY_101AB_DECODE_EMPTY = 6
} Gwy101AbDecodeStatus;

typedef struct Gwy101AbRecord {
    uint32_t body_size;   /* includes the 2-byte event_code */
    uint16_t event_code;
    uint32_t body_off;    /* offset of body bytes (after event_code) in input */
    uint32_t body_len;    /* body_size - 2 */
} Gwy101AbRecord;

typedef struct Gwy101AbDecodedFrame {
    uint8_t type;
    uint32_t payload_length;
    uint32_t header;
    uint32_t record_count;
    Gwy101AbRecord records[GWY_101AB_MAX_RECORDS];
    uint32_t bytes_consumed; /* type + 4 + payload_length when OK */
    Gwy101AbDecodeStatus status;
    uint16_t unknown_codes; /* count of event_code values outside known set */
} Gwy101AbDecodedFrame;

/* Decode a host or captured buffer. Does not allocate; body bytes stay in src. */
Gwy101AbDecodeStatus platform_101ab_decode_frame(const uint8_t *src, uint32_t src_len,
                                                 Gwy101AbDecodedFrame *out);

/* True when event_code is one of the known Path-A family codes (5,15,…). */
int platform_101ab_event_code_known(uint16_t code);

#ifdef __cplusplus
}
#endif

#endif
