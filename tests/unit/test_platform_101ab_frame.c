#include "gwy_launcher/platform_101ab_frame.h"
#include "gwy_launcher/platform_101ab_provider.h"
#include "gwy_launcher/platform_send_app_event.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <string.h>

static void be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xffu);
    p[1] = (uint8_t)((v >> 16) & 0xffu);
    p[2] = (uint8_t)((v >> 8) & 0xffu);
    p[3] = (uint8_t)(v & 0xffu);
}

static void be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xffu);
    p[1] = (uint8_t)(v & 0xffu);
}

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

int main(void) {
    uint8_t buf[192];
    uint32_t n;
    Gwy101AbDecodedFrame dec;
    Gwy101AbProvideResult prov;

    platform_101ab_provider_reset();

    /* Synthetic code5 with downVersion record. */
    n = platform_101ab_fill_path_a(buf, sizeof(buf), 1);
    if (n < 30u || buf[0] != 2) return fail("synthetic fill");
    if (platform_101ab_decode_frame(buf, n, &dec) != GWY_101AB_DECODE_OK)
        return fail("decode synthetic");
    if (dec.type != 2u || dec.header != 5u || dec.record_count < 1u) return fail("synthetic fields");
    if (dec.records[0].event_code != 5u) return fail("expected event_code=5");

    /* Provider default = synthetic. */
    memset(&prov, 0, sizeof(prov));
    n = platform_101ab_provider_fill(buf, sizeof(buf), 1, &prov);
    if (!n || prov.mode != GWY_101AB_PROVIDER_SYNTHETIC_CODE5_COMPAT) return fail("provider synth");
    if (prov.transport_class != GWY_101AB_TRANSPORT_LOCAL_BOOTSTRAP_STREAM)
        return fail("transport class");
    if (prov.guest_r0_cursor != 0u) return fail("cursor must be 0 for in-place fill");

    /* Truncated frame. */
    if (platform_101ab_decode_frame(buf, 3, &dec) != GWY_101AB_DECODE_TRUNCATED)
        return fail("truncated");

    /* Bad outer length. */
    {
        uint8_t bad[16];
        memset(bad, 0, sizeof(bad));
        bad[0] = 2;
        be32(bad + 1, 100u); /* claims 100 but only 11 payload bytes possible */
        if (platform_101ab_decode_frame(bad, sizeof(bad), &dec) != GWY_101AB_DECODE_BAD_OUTER_LEN)
            return fail("bad outer len");
    }

    /* body_size < 2. */
    {
        uint8_t bad[20];
        memset(bad, 0, sizeof(bad));
        bad[0] = 2;
        be32(bad + 1, 12u);
        be32(bad + 5, 5u);  /* header */
        be32(bad + 9, 1u);  /* body_size=1 < 2 */
        be16(bad + 13, 5u);
        if (platform_101ab_decode_frame(bad, sizeof(bad), &dec) != GWY_101AB_DECODE_BODY_SIZE_LT2)
            return fail("body_size<2");
    }

    /* Multi-record frame. */
    {
        uint8_t multi[64];
        uint32_t o = 0;
        uint32_t payload_len;
        memset(multi, 0, sizeof(multi));
        multi[0] = 2;
        o = 5;
        be32(multi + o, 5u);
        o += 4; /* header */
        be32(multi + o, 6u);
        o += 4; /* body_size */
        be16(multi + o, 5u);
        o += 2;
        be32(multi + o, 0xFFFFFFFFu);
        o += 4; /* body */
        be32(multi + o, 6u);
        o += 4;
        be16(multi + o, 7u);
        o += 2;
        be32(multi + o, 0xFFFFFFFFu);
        o += 4;
        payload_len = o - 5u;
        be32(multi + 1, payload_len);
        if (platform_101ab_decode_frame(multi, o, &dec) != GWY_101AB_DECODE_OK)
            return fail("multi decode");
        if (dec.record_count != 2u) return fail("multi count");
        if (dec.records[0].event_code != 5u || dec.records[1].event_code != 7u)
            return fail("multi codes");
    }

    /* Unknown event code counted. */
    {
        uint8_t unk[32];
        uint32_t o = 0;
        memset(unk, 0, sizeof(unk));
        unk[0] = 2;
        o = 5;
        be32(unk + o, 5u);
        o += 4;
        be32(unk + o, 6u);
        o += 4;
        be16(unk + o, 99u);
        o += 2;
        be32(unk + o, 0xFFFFFFFFu);
        o += 4;
        be32(unk + 1, o - 5u);
        if (platform_101ab_decode_frame(unk, o, &dec) != GWY_101AB_DECODE_OK) return fail("unk decode");
        if (dec.unknown_codes != 1u) return fail("unknown_codes");
    }

    /* Partial / empty. */
    if (platform_101ab_decode_frame(NULL, 0, &dec) != GWY_101AB_DECODE_NULL) return fail("null");
    if (platform_101ab_decode_frame(buf, 0, &dec) != GWY_101AB_DECODE_EMPTY) return fail("empty");

    /* Queue rejects hand-built code15 without matching SHA. */
    {
        uint8_t c15[32];
        uint32_t o = 5;
        memset(c15, 0, sizeof(c15));
        c15[0] = 2;
        be32(c15 + o, 5u);
        o += 4;
        be32(c15 + o, 6u);
        o += 4;
        be16(c15 + o, 15u);
        o += 2;
        be32(c15 + o, 0xFFFFFFFFu);
        o += 4;
        be32(c15 + 1, o - 5u);
        if (platform_101ab_provider_queue_push(c15, o, NULL, "hand")) return fail("code15 no sha");
        /* Wrong SHA rejected. */
        if (platform_101ab_provider_queue_push(c15, o, "deadbeef", "hand")) return fail("code15 bad sha");
    }

    /* Partial record. */
    {
        uint8_t part[20];
        memset(part, 0, sizeof(part));
        part[0] = 2;
        be32(part + 1, 12u);
        be32(part + 5, 5u);
        be32(part + 9, 20u); /* body_size claims 20 but only 7 bytes left in payload */
        if (platform_101ab_decode_frame(part, sizeof(part), &dec) != GWY_101AB_DECODE_PARTIAL_RECORD &&
            platform_101ab_decode_frame(part, sizeof(part), &dec) != GWY_101AB_DECODE_BAD_OUTER_LEN)
            return fail("partial");
    }

    printf("test_platform_101ab_frame: OK\n");
    return 0;
}
