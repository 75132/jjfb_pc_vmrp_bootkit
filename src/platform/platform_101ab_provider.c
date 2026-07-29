#include "gwy_launcher/platform_101ab_provider.h"
#include "gwy_launcher/platform_send_app_event.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_CAP 16
#define FRAME_CAP 2048

typedef struct {
    uint8_t data[FRAME_CAP];
    uint32_t len;
    char sha_hex[65];
    char tag[48];
} QueuedFrame;

static int g_mode_known;
static Gwy101AbProviderMode g_mode;
static QueuedFrame g_q[QUEUE_CAP];
static uint32_t g_q_n;
static uint32_t g_q_rd;

static int env_eq(const char *k, const char *v) {
    const char *e = getenv(k);
    return e && v && strcmp(e, v) == 0;
}

static int hex_eq_ci(const char *a, const char *b) {
    size_t i;
    if (!a || !b) return 0;
    for (i = 0; a[i] && b[i]; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return a[i] == 0 && b[i] == 0;
}

static void ensure_mode(void) {
    if (g_mode_known) return;
    g_mode_known = 1;
    if (env_eq("JJFB_101AB_PROVIDER", "queue"))
        g_mode = GWY_101AB_PROVIDER_TRANSPORT_QUEUE;
    else if (env_eq("JJFB_101AB_PROVIDER", "replay"))
        g_mode = GWY_101AB_PROVIDER_CAPTURE_REPLAY_RESEARCH;
    else
        g_mode = GWY_101AB_PROVIDER_SYNTHETIC_CODE5_COMPAT;
}

void platform_101ab_provider_reset(void) {
    g_mode_known = 0;
    g_mode = GWY_101AB_PROVIDER_SYNTHETIC_CODE5_COMPAT;
    g_q_n = 0;
    g_q_rd = 0;
    memset(g_q, 0, sizeof(g_q));
}

Gwy101AbProviderMode platform_101ab_provider_mode(void) {
    ensure_mode();
    return g_mode;
}

const char *platform_101ab_provider_mode_name(Gwy101AbProviderMode mode) {
    switch (mode) {
    case GWY_101AB_PROVIDER_SYNTHETIC_CODE5_COMPAT:
        return "SYNTHETIC_CODE5_COMPAT";
    case GWY_101AB_PROVIDER_TRANSPORT_QUEUE:
        return "TRANSPORT_QUEUE";
    case GWY_101AB_PROVIDER_CAPTURE_REPLAY_RESEARCH:
        return "CAPTURE_REPLAY_RESEARCH";
    default:
        return "UNKNOWN";
    }
}

const char *platform_101ab_provider_transport_name(Gwy101AbTransportClass c) {
    switch (c) {
    case GWY_101AB_TRANSPORT_LOCAL_BOOTSTRAP_STREAM:
        return "LOCAL_BOOTSTRAP_STREAM";
    case GWY_101AB_TRANSPORT_QUEUE:
        return "TRANSPORT_QUEUE";
    case GWY_101AB_TRANSPORT_CAPTURE_REPLAY:
        return "CAPTURE_REPLAY";
    case GWY_101AB_TRANSPORT_NETWORK_RECEIVE_STREAM:
        return "NETWORK_RECEIVE_STREAM";
    case GWY_101AB_TRANSPORT_CALLBACK_PAYLOAD_COPY:
        return "CALLBACK_PAYLOAD_COPY";
    default:
        return "UNKNOWN";
    }
}

Gwy101AbTransportClass platform_101ab_provider_transport_class(void) {
    ensure_mode();
    switch (g_mode) {
    case GWY_101AB_PROVIDER_TRANSPORT_QUEUE:
        return GWY_101AB_TRANSPORT_QUEUE;
    case GWY_101AB_PROVIDER_CAPTURE_REPLAY_RESEARCH:
        return GWY_101AB_TRANSPORT_CAPTURE_REPLAY;
    case GWY_101AB_PROVIDER_SYNTHETIC_CODE5_COMPAT:
    default:
        /*
         * Closed classification for the current product path:
         * platform ABI is receive-into-buffer (cursor R0), but the only closed
         * producer is the local Path-A synthetic bootstrap stream.
         */
        return GWY_101AB_TRANSPORT_LOCAL_BOOTSTRAP_STREAM;
    }
}

uint32_t platform_101ab_provider_queue_depth(void) {
    if (g_q_n <= g_q_rd) return 0;
    return g_q_n - g_q_rd;
}

static int frame_has_code15(const uint8_t *frame, uint32_t len) {
    Gwy101AbDecodedFrame dec;
    uint32_t i;
    if (platform_101ab_decode_frame(frame, len, &dec) != GWY_101AB_DECODE_OK) return 0;
    for (i = 0; i < dec.record_count; i++) {
        if (dec.records[i].event_code == 15u) return 1;
    }
    return 0;
}

int platform_101ab_provider_queue_push(const uint8_t *frame, uint32_t frame_len,
                                      const char *src_sha256_hex, const char *source_tag) {
    QueuedFrame *slot;
    uint8_t dig[32];
    char hex[65];
    Gwy101AbDecodedFrame dec;

    if (!frame || frame_len < 5u || frame_len > FRAME_CAP) return 0;
    if (g_q_n >= QUEUE_CAP) return 0;
    if (platform_101ab_decode_frame(frame, frame_len, &dec) != GWY_101AB_DECODE_OK) return 0;

    /* code15 only accepted with matching provenance SHA (no hand-built frames). */
    if (frame_has_code15(frame, frame_len)) {
        if (!src_sha256_hex || !src_sha256_hex[0]) return 0;
        gwy_sha256(frame, frame_len, dig);
        gwy_sha256_hex(dig, hex);
        if (!hex_eq_ci(hex, src_sha256_hex)) return 0;
    }

    slot = &g_q[g_q_n++];
    memset(slot, 0, sizeof(*slot));
    memcpy(slot->data, frame, frame_len);
    slot->len = frame_len;
    if (src_sha256_hex) {
        strncpy(slot->sha_hex, src_sha256_hex, sizeof(slot->sha_hex) - 1u);
    } else {
        gwy_sha256(frame, frame_len, dig);
        gwy_sha256_hex(dig, slot->sha_hex);
    }
    if (source_tag) strncpy(slot->tag, source_tag, sizeof(slot->tag) - 1u);
    return 1;
}

int platform_101ab_provider_replay_load_file(const char *path, const char *src_sha256_hex) {
    FILE *f;
    uint8_t buf[FRAME_CAP];
    size_t n;
    uint8_t dig[32];
    char hex[65];

    if (!path || !path[0]) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (n < 5u) return 0;
    gwy_sha256(buf, (size_t)n, dig);
    gwy_sha256_hex(dig, hex);
    if (src_sha256_hex && src_sha256_hex[0] && !hex_eq_ci(hex, src_sha256_hex)) return 0;
    ensure_mode();
    g_mode = GWY_101AB_PROVIDER_CAPTURE_REPLAY_RESEARCH;
    g_mode_known = 1;
    return platform_101ab_provider_queue_push(buf, (uint32_t)n, hex, path);
}

static uint32_t pop_queued(uint8_t *dst, uint32_t dst_cap, Gwy101AbProvideResult *out) {
    QueuedFrame *slot;
    if (g_q_rd >= g_q_n) {
        if (out) {
            out->bytes_written = 0;
            out->guest_r0_cursor = 0;
            out->empty_queue = 1;
            out->mode = g_mode;
            out->transport_class = platform_101ab_provider_transport_class();
            out->name = "plat_101ab_queue_empty";
            out->evidence = "EMPTY_QUEUE";
        }
        return 0;
    }
    slot = &g_q[g_q_rd++];
    if (slot->len > dst_cap) {
        if (out) {
            out->bytes_written = 0;
            out->guest_r0_cursor = 0;
            out->empty_queue = 0;
            out->mode = g_mode;
            out->transport_class = platform_101ab_provider_transport_class();
            out->name = "plat_101ab_queue_overflow";
            out->evidence = "TRUNCATED";
        }
        return 0;
    }
    memcpy(dst, slot->data, slot->len);
    if (out) {
        out->bytes_written = slot->len;
        out->guest_r0_cursor = 0; /* in-place at buf[0] */
        out->empty_queue = 0;
        out->mode = g_mode;
        out->transport_class = platform_101ab_provider_transport_class();
        out->name = (g_mode == GWY_101AB_PROVIDER_CAPTURE_REPLAY_RESEARCH)
                        ? "plat_101ab_capture_replay"
                        : "plat_101ab_transport_queue";
        out->evidence = "PROVENANCE_SHA";
    }
    return slot->len;
}

uint32_t platform_101ab_provider_fill(uint8_t *dst, uint32_t dst_cap, int with_record,
                                      Gwy101AbProvideResult *out) {
    uint32_t n;
    ensure_mode();
    if (out) memset(out, 0, sizeof(*out));

    if (g_mode == GWY_101AB_PROVIDER_TRANSPORT_QUEUE ||
        g_mode == GWY_101AB_PROVIDER_CAPTURE_REPLAY_RESEARCH) {
        /* Fall back to synthetic only when queue empty AND synthetic compat allowed. */
        n = pop_queued(dst, dst_cap, out);
        if (n) return n;
        if (g_mode == GWY_101AB_PROVIDER_TRANSPORT_QUEUE) {
            /* Empty queue: do not invent frames. Guest cursor 0 + 0 bytes. */
            return 0;
        }
        /* replay with empty queue → no synthetic invent for research purity */
        return 0;
    }

    n = platform_101ab_fill_path_a(dst, dst_cap, with_record);
    if (out) {
        out->bytes_written = n;
        out->guest_r0_cursor = 0;
        out->empty_queue = 0;
        out->mode = GWY_101AB_PROVIDER_SYNTHETIC_CODE5_COMPAT;
        out->transport_class = GWY_101AB_TRANSPORT_LOCAL_BOOTSTRAP_STREAM;
        out->name = "plat_101ab_synthetic_code5";
        out->evidence = "TARGET_OBSERVED+robotol_30D24C";
    }
    return n;
}
