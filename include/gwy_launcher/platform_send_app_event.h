#ifndef GWY_LAUNCHER_PLATFORM_SEND_APP_EVENT_H
#define GWY_LAUNCHER_PLATFORM_SEND_APP_EVENT_H

#include "gwy_launcher/platform_userinfo.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Typed sendAppEvent / plat-style dispatch (docs/06).
 * Not a JJFB mega-switch: only DOCUMENTED/CROSS_TARGET codes get handlers.
 * Unknown codes → unsupported (caller keeps legacy status=0 for event-style).
 */

typedef enum GwyPlatCallKind {
    GWY_PLAT_KIND_STATUS = 0,         /* return status_ret (often 0 = MR_SUCCESS) */
    GWY_PLAT_KIND_USERINFO_BLOB = 1,  /* return guest ptr to userinfo blob */
    GWY_PLAT_KIND_UNSUPPORTED = 2,
    GWY_PLAT_KIND_ALLOC = 3,          /* zeroed guest alloc; return guest ptr */
    GWY_PLAT_KIND_TIMER_START = 4,    /* start host timer; period_ms set */
    GWY_PLAT_KIND_TIMER_STOP = 5,     /* stop host timer */
    /* CROSS_TARGET: register family/chunk handler; status_ret usually 1. */
    GWY_PLAT_KIND_REGISTER = 6,
    /* DOCUMENTED/CROSS_TARGET: write graphics fp to *out_guest; status_ret=0. */
    GWY_PLAT_KIND_GRAPHICS_FP = 7,
    /* TARGET_OBSERVED: fill guest buffer in-place (0x101AB Path A). */
    GWY_PLAT_KIND_BUFFER_FILL = 8
} GwyPlatCallKind;

typedef struct GwyPlatCall {
    uint32_t code;   /* R0 */
    uint32_t app;    /* R1 */
    uint32_t arg2;   /* R2 */
    uint32_t arg3;   /* R3 */
    uint32_t arg4;   /* SP[0] optional (DOCUMENTED 5th arg) */
    /* Published mrc_extChunk guest addr (0 if unknown). Used only for timer ABI. */
    uint32_t known_chunk;
} GwyPlatCall;

typedef struct GwyPlatCallResult {
    GwyPlatCallKind kind;
    uint32_t status_ret; /* when kind == STATUS */
    uint32_t alloc_size; /* when kind == ALLOC */
    /* TARGET_OBSERVED: write this LE halfword at alloc+0 after zeroing (0 = skip). */
    uint16_t alloc_u16_at0;
    uint32_t timer_period_ms; /* when kind == TIMER_START */
    uint32_t timer_id;        /* optional id written to chunk+0x24 */
    uint32_t timer_chunk;     /* extChunk guest for +0x24 update */
    uint32_t reg_family;      /* REGISTER: family/type id */
    uint32_t reg_handler;     /* REGISTER: guest handler/chunk */
    uint32_t graphics_id;     /* GRAPHICS_FP: query id (e.g. 0x11F02) */
    uint32_t graphics_out;    /* GRAPHICS_FP: guest *out for fp write */
    uint32_t fill_buf;        /* BUFFER_FILL: guest buffer (R1/app) */
    uint32_t fill_type;       /* BUFFER_FILL: type (R3; Path A uses 2) */
    const char *name;
    const char *evidence;
    PlatformUserInfoBlob userinfo; /* valid when kind == USERINFO_BLOB */
} GwyPlatCallResult;

void platform_send_app_event_classify(const GwyPlatCall *call, GwyPlatCallResult *out);

/* Fill Path-A 0x101AB payload into a mapped guest buffer. Returns bytes written. */
uint32_t platform_101ab_fill_path_a(uint8_t *dst, uint32_t dst_cap, int with_record);

#ifdef __cplusplus
}
#endif

#endif
