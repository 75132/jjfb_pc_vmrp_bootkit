#ifndef GWY_LAUNCHER_PLATFORM_101AB_PROVIDER_H
#define GWY_LAUNCHER_PLATFORM_101AB_PROVIDER_H

#include "gwy_launcher/platform_101ab_frame.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 0x101AB input provider.
 *
 * Product default remains SYNTHETIC_CODE5_COMPAT (Path-A downVersion baseline).
 * TRANSPORT_QUEUE / CAPTURE_REPLAY_RESEARCH only activate when a real producer
 * or verified capture is bound — never invent code15 frames.
 *
 * Env:
 *   JJFB_101AB_PROVIDER=synthetic|queue|replay   (default: synthetic)
 */

typedef enum Gwy101AbProviderMode {
    GWY_101AB_PROVIDER_SYNTHETIC_CODE5_COMPAT = 0,
    GWY_101AB_PROVIDER_TRANSPORT_QUEUE = 1,
    GWY_101AB_PROVIDER_CAPTURE_REPLAY_RESEARCH = 2
} Gwy101AbProviderMode;

/*
 * Closed ABI classification for the platform call itself (not the downstream
 * B54 queue). Determined from robotol 0x30D24C + host evidence.
 */
typedef enum Gwy101AbTransportClass {
    /* Host synthesizes Path-A bootstrap bytes into the guest buffer. */
    GWY_101AB_TRANSPORT_LOCAL_BOOTSTRAP_STREAM = 0,
    /* Real producer feeds raw framed bytes into a host queue (partial/empty). */
    GWY_101AB_TRANSPORT_QUEUE = 1,
    /* Verified capture dump replay (research only). */
    GWY_101AB_TRANSPORT_CAPTURE_REPLAY = 2,
    /* Network recv → secondary fill (not proven in this repo). */
    GWY_101AB_TRANSPORT_NETWORK_RECEIVE_STREAM = 3,
    /* Callback payload memcpy (rejected for Path-A). */
    GWY_101AB_TRANSPORT_CALLBACK_PAYLOAD_COPY = 4
} Gwy101AbTransportClass;

typedef struct Gwy101AbProvideResult {
    uint32_t bytes_written; /* poked into guest buf (0 = empty / no fill) */
    /*
     * Guest R0 semantics @0x30D2B0: initial parse cursor into the buffer
     * (bytes already consumed). Full in-place fill → 0 so type starts at buf[0].
     */
    uint32_t guest_r0_cursor;
    int empty_queue; /* 1 when provider had nothing (TRANSPORT_QUEUE) */
    Gwy101AbProviderMode mode;
    Gwy101AbTransportClass transport_class;
    const char *name;
    const char *evidence;
} Gwy101AbProvideResult;

void platform_101ab_provider_reset(void);
Gwy101AbProviderMode platform_101ab_provider_mode(void);
Gwy101AbTransportClass platform_101ab_provider_transport_class(void);
const char *platform_101ab_provider_mode_name(Gwy101AbProviderMode mode);
const char *platform_101ab_provider_transport_name(Gwy101AbTransportClass c);

/*
 * Fill dst with the next frame. Returns bytes written (0 on empty/fail).
 * with_record applies only to SYNTHETIC_CODE5_COMPAT (Path-A phase).
 */
uint32_t platform_101ab_provider_fill(uint8_t *dst, uint32_t dst_cap, int with_record,
                                      Gwy101AbProvideResult *out);

/*
 * TRANSPORT_QUEUE: enqueue a verified raw frame (full type+len+payload).
 * Rejects buffers that decode as event_code=15 unless src_sha256_hex is set
 * (capture provenance required). Returns 1 on accept.
 */
int platform_101ab_provider_queue_push(const uint8_t *frame, uint32_t frame_len,
                                      const char *src_sha256_hex, const char *source_tag);

/* CAPTURE_REPLAY: load one dump file (research). */
int platform_101ab_provider_replay_load_file(const char *path, const char *src_sha256_hex);

uint32_t platform_101ab_provider_queue_depth(void);

#ifdef __cplusplus
}
#endif

#endif
