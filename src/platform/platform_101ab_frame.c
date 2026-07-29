#include "gwy_launcher/platform_101ab_frame.h"
#include <string.h>

static uint32_t be32_at(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static uint16_t be16_at(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

int platform_101ab_event_code_known(uint16_t code) {
    /* Path-A / B54 dispatch family observed in robotol 0x2E2520 table. */
    switch (code) {
    case 5u:
    case 15u:
    case 2u:
    case 3u:
    case 4u:
    case 6u:
    case 7u:
    case 8u:
    case 9u:
    case 10u:
        return 1;
    default:
        return 0;
    }
}

Gwy101AbDecodeStatus platform_101ab_decode_frame(const uint8_t *src, uint32_t src_len,
                                                 Gwy101AbDecodedFrame *out) {
    uint32_t payload_len;
    uint32_t o;
    uint32_t end;
    uint32_t header = 0;
    uint32_t rec_n = 0;

    if (!out) return GWY_101AB_DECODE_NULL;
    memset(out, 0, sizeof(*out));
    if (!src) {
        out->status = GWY_101AB_DECODE_NULL;
        return out->status;
    }
    if (src_len == 0u) {
        out->status = GWY_101AB_DECODE_EMPTY;
        return out->status;
    }
    if (src_len < 5u) {
        out->status = GWY_101AB_DECODE_TRUNCATED;
        return out->status;
    }

    out->type = src[0];
    payload_len = be32_at(src + 1);
    out->payload_length = payload_len;
    if (payload_len > src_len - 5u) {
        out->status = GWY_101AB_DECODE_BAD_OUTER_LEN;
        return out->status;
    }
    if (payload_len < 4u) {
        out->status = GWY_101AB_DECODE_TRUNCATED;
        return out->status;
    }

    o = 5u;
    end = 5u + payload_len;
    header = be32_at(src + o);
    out->header = header;
    o += 4u;

    while (o + 6u <= end && rec_n < GWY_101AB_MAX_RECORDS) {
        uint32_t body_size = be32_at(src + o);
        uint16_t code;
        uint32_t body_len;
        Gwy101AbRecord *rec;

        o += 4u;
        if (body_size < 2u) {
            out->record_count = rec_n;
            out->status = GWY_101AB_DECODE_BODY_SIZE_LT2;
            return out->status;
        }
        if (o + body_size > end) {
            out->record_count = rec_n;
            out->status = GWY_101AB_DECODE_PARTIAL_RECORD;
            return out->status;
        }
        code = be16_at(src + o);
        body_len = body_size - 2u;
        rec = &out->records[rec_n++];
        rec->body_size = body_size;
        rec->event_code = code;
        rec->body_off = o + 2u;
        rec->body_len = body_len;
        if (!platform_101ab_event_code_known(code)) out->unknown_codes++;
        o += body_size;
    }

    if (o < end && rec_n >= GWY_101AB_MAX_RECORDS) {
        /* Remaining payload not decoded — still OK for leading records. */
    } else if (o + 6u <= end) {
        /* Could not form another record but bytes remain. */
        out->record_count = rec_n;
        out->bytes_consumed = end;
        out->status = GWY_101AB_DECODE_PARTIAL_RECORD;
        return out->status;
    }

    out->record_count = rec_n;
    out->bytes_consumed = end;
    out->status = GWY_101AB_DECODE_OK;
    return out->status;
}
