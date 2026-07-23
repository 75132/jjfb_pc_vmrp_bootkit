#include "gwy_launcher/platform_send_app_event.h"
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

/*
 * Path-A payload for 0x101AB (TARGET_OBSERVED from robotol 0x30D24C → 0x2E4D6C).
 * Layout: char type + BE len + payload[ hdr_u32 | body_size | BE u16 code | body ].
 * body ends with BE 0xFFFFFFFF marker required by 0x2F68E4.
 */
uint32_t platform_101ab_fill_path_a(uint8_t *dst, uint32_t dst_cap, int with_record) {
    uint8_t payload[160];
    uint32_t body_size;
    uint32_t payload_len;
    uint32_t o = 0;
    const char *rec = "downVersion";
    uint32_t name_len = (uint32_t)strlen(rec);
    uint32_t total;

    if (!dst || dst_cap < 16u) return 0;
    if (with_record)
        body_size = 2u + (4u + 2u + name_len + 2u + 0u + 4u + 4u + 4u);
    else
        body_size = 6u; /* u16 + BE(-1) */
    payload_len = 4u + 4u + 2u + (body_size - 2u);
    if (payload_len > sizeof(payload)) payload_len = (uint32_t)sizeof(payload);
    memset(payload, 0, sizeof(payload));
    be32(payload + o, 5u); /* hdr → A90+4 */
    o += 4;
    be32(payload + o, body_size);
    o += 4;
    be16(payload + o, 5u); /* node[0] → Path A */
    o += 2;
    if (with_record) {
        be32(payload + o, 1u); /* tag */
        o += 4;
        be16(payload + o, (uint16_t)name_len);
        o += 2;
        memcpy(payload + o, rec, name_len);
        o += name_len;
        be16(payload + o, 0); /* str2 empty */
        o += 2;
        be32(payload + o, 0);
        o += 4;
        be32(payload + o, 0x3EEu); /* local downVersion.v */
        o += 4;
        be32(payload + o, 0xFFFFFFFFu);
        o += 4;
    } else {
        be32(payload + o, 0xFFFFFFFFu);
        o += 4;
    }
    if (o > payload_len) payload_len = o;
    total = 5u + payload_len;
    if (total > dst_cap) return 0;
    dst[0] = 2; /* type param0 from 30D24C */
    be32(dst + 1, payload_len);
    memcpy(dst + 5, payload, payload_len);
    return total;
}

static int period_ok(uint32_t ms) {
    return ms >= 1u && ms <= 60000u;
}

static void fill_timer_start(GwyPlatCallResult *out, uint32_t chunk, uint32_t period,
                             uint32_t timer_id, const char *name, const char *evidence) {
    out->kind = GWY_PLAT_KIND_TIMER_START;
    out->timer_period_ms = period;
    out->timer_id = timer_id ? timer_id : 1u;
    out->timer_chunk = chunk;
    out->status_ret = 0;
    out->name = name;
    out->evidence = evidence;
}

static void fill_timer_stop(GwyPlatCallResult *out, uint32_t chunk, const char *name,
                            const char *evidence) {
    out->kind = GWY_PLAT_KIND_TIMER_STOP;
    out->timer_chunk = chunk;
    out->status_ret = 0;
    out->name = name;
    out->evidence = evidence;
}

void platform_send_app_event_classify(const GwyPlatCall *call, GwyPlatCallResult *out) {
    PlatformIdentity id;
    LauncherError err;
    uint32_t chunk;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->kind = GWY_PLAT_KIND_STATUS;
    out->status_ret = 0;
    out->name = "default_status";
    out->evidence = "DOCUMENTED";

    if (!call) {
        out->kind = GWY_PLAT_KIND_UNSUPPORTED;
        out->name = "null_call";
        out->evidence = "DOCUMENTED";
        return;
    }

    chunk = call->known_chunk;

    /*
     * DOCUMENTED mrc_extTimerStart/Stop (mr_helper / ext_important):
     *   sendAppEvent(0, chunk, 0, timerId, period)  — start
     *   sendAppEvent(0, chunk, 1, timerId, 0)       — stop
     * R0=extCode R1=app R2=code R3=param0 SP[0]=param1
     */
    if (chunk && call->code == 0u && call->app == chunk) {
        if (call->arg2 == 0u) {
            uint32_t period = call->arg4 ? call->arg4 : call->arg3;
            if (period_ok(period)) {
                fill_timer_start(out, chunk, period, call->arg3, "ext_timer_start_classic",
                                 "DOCUMENTED");
                return;
            }
        } else if (call->arg2 == 1u) {
            fill_timer_stop(out, chunk, "ext_timer_stop_classic", "DOCUMENTED");
            return;
        }
    }

    /*
     * TARGET_OBSERVED gamelist wrappers (call sendAppEvent via chunk+0x28):
     *   (chunk, 1, timerField, 0)           — stop/arm
     *   (chunk, 0, timerField, period)      — start; period in R3
     */
    if (chunk && call->code == chunk) {
        if (call->app == 0u && period_ok(call->arg3)) {
            fill_timer_start(out, chunk, call->arg3, call->arg2, "ext_timer_start_chunk_r0",
                             "TARGET_OBSERVED");
            return;
        }
        if (call->app == 1u) {
            fill_timer_stop(out, chunk, "ext_timer_stop_chunk_r0", "TARGET_OBSERVED");
            return;
        }
    }

    /*
     * 0x10180: device/user info pointer.
     * DOCUMENTED: docs/06 + mr_userinfo layout.
     * TARGET_OBSERVED: gamelist sendAppEvent(0x10180) then LDRB [ret+0x20].
     */
    if (call->code == 0x10180u) {
        platform_identity_set_defaults(&id);
        out->kind = GWY_PLAT_KIND_USERINFO_BLOB;
        out->name = "userinfo_blob";
        out->evidence = "DOCUMENTED+TARGET_OBSERVED";
        if (platform_userinfo_fill(&id, GWY_DEFAULT_IMSI, &out->userinfo, &err) != L_OK) {
            out->kind = GWY_PLAT_KIND_UNSUPPORTED;
            out->name = "userinfo_fill_failed";
            out->evidence = "DOCUMENTED";
        }
        return;
    }

    /*
     * 0x10162 / 0x10165: sized alloc (+ optional handler note).
     * CROSS_TARGET with 0x10102 band (docs/06 + legacy): app=size in R1.
     * docs/06: 0x10165 also registers enqueue handler in arg2 when present.
     * After 10140 ack, robotol calls 0x10162(0xE200, handler); status-0 → mrc_init -1.
     */
    if (call->code == 0x10162u || call->code == 0x10165u) {
        if (call->app >= 0x100u && call->app < 0x800000u) {
            out->kind = GWY_PLAT_KIND_ALLOC;
            out->alloc_size = call->app;
            out->name = (call->code == 0x10165u) ? "plat_10165_alloc" : "plat_10162_alloc";
            out->evidence = "CROSS_TARGET+docs/06";
            /* Side-register enqueue handler when arg2 is a guest code VA. */
            if (call->arg2 && (call->arg2 & ~1u) > 0x1000u) {
                out->reg_family = call->app;
                out->reg_handler = call->arg2;
            }
            return;
        }
        if (call->arg2 && (call->arg2 & ~1u) > 0x1000u) {
            out->kind = GWY_PLAT_KIND_REGISTER;
            out->status_ret = 1u;
            out->reg_family = call->app;
            out->reg_handler = call->arg2;
            out->name = (call->code == 0x10165u) ? "plat_10165_register" : "plat_10162_register";
            out->evidence = "CROSS_TARGET+docs/06";
            return;
        }
    }

    /*
     * Sized zeroed alloc family (CROSS_TARGET with 0x10102/10162/10165 band):
     *   0x1010C(app=size, arg2=buf)  — size in R1
     *   0x10183(app=size)            — size in R1; gamelist stores ret, beq on NULL
     *   0x10119(app=flag, arg2=size) — size in R2; gamelist memset(ret,0,size)
     * TARGET_OBSERVED: NULL ret → UC_MEM_WRITE_UNMAPPED @0 (DSM memset).
     */
    if (call->code == 0x1010Cu || call->code == 0x10183u) {
        if (call->app >= 0x20u && call->app < 0x800000u) {
            out->kind = GWY_PLAT_KIND_ALLOC;
            out->alloc_size = call->app;
            out->name = (call->code == 0x10183u) ? "plat_alloc_r1_10183" : "plat_sized_alloc";
            out->evidence = "CROSS_TARGET+TARGET_OBSERVED";
            return;
        }
    }
    if (call->code == 0x10119u) {
        if (call->arg2 >= 0x20u && call->arg2 < 0x800000u) {
            out->kind = GWY_PLAT_KIND_ALLOC;
            out->alloc_size = call->arg2;
            out->name = "plat_alloc_r2_10119";
            out->evidence = "TARGET_OBSERVED";
            return;
        }
    }

    /*
     * 0x10102: family handler register (docs/06 + CROSS_TARGET robotol/mmochat/gamelist).
     *   sendAppEvent(0x10102, family, handler, ...)
     * Ack is non-zero (legacy SET_RET_V(1)); default_status ret=0 left init failing.
     */
    if (call->code == 0x10102u && call->arg2 && (call->arg2 & ~1u) > 0x1000u) {
        out->kind = GWY_PLAT_KIND_REGISTER;
        out->status_ret = 1u;
        out->reg_family = call->app;
        out->reg_handler = call->arg2;
        out->name = "plat_10102_register";
        out->evidence = "CROSS_TARGET+docs/06";
        return;
    }

    /*
     * 0x10113: graphics getProc (docs/06).
     * CROSS_TARGET: write fp to *param0 (R3); return MR_SUCCESS (0).
     * Silent status-0 without *out write left draw slots empty → mrc_init -1.
     */
    if (call->code == 0x10113u) {
        out->kind = GWY_PLAT_KIND_GRAPHICS_FP;
        out->status_ret = 0u;
        out->graphics_id = call->app;
        out->graphics_out = call->arg3;
        out->name = "plat_10113_graphics_fp";
        out->evidence = "CROSS_TARGET+docs/06";
        return;
    }

    /*
     * 0x10120: bind ext chunk/handler context (docs/06).
     * CROSS_TARGET: ack with 1 (legacy SET_RET_V(1)).
     */
    if (call->code == 0x10120u) {
        out->kind = GWY_PLAT_KIND_REGISTER;
        out->status_ret = 1u;
        out->reg_family = call->app;
        out->reg_handler = call->arg2 ? call->arg2 : call->app;
        out->name = "plat_10120_bind";
        out->evidence = "CROSS_TARGET+docs/06";
        return;
    }

    /*
     * 0x10140: period/main handler register (docs/06).
     * CROSS_TARGET: (type, chunk, handler); handler prefers R3; ack=1.
     * After 10120 ret=1, robotol continues here; status-0 aborts mrc_init.
     */
    if (call->code == 0x10140u) {
        uint32_t handler = call->arg3 ? call->arg3 : call->arg2;
        if (handler && (handler & ~1u) > 0x1000u) {
            out->kind = GWY_PLAT_KIND_REGISTER;
            out->status_ret = 1u;
            out->reg_family = call->app;
            out->reg_handler = handler;
            out->name = "plat_10140_register";
            out->evidence = "CROSS_TARGET+docs/06";
            return;
        }
    }

    /*
     * 0x10204: multiplex buffer/query API (TARGET_OBSERVED gamelist.ext).
     * Timer path @image+0x133xx:
     *   (0x10204, 4, 0xFFFFFFFF) → guest ptr; LDRH [ptr] requires tag 1 or 3
     *   (0x10204, 0xF, ptr, ...) → status 0 (fill/use existing buf)
     * Other app codes: status 0 until schema is DOCUMENTED.
     */
    if (call->code == 0x10204u) {
        if (call->app == 4u && call->arg2 == 0xFFFFFFFFu) {
            out->kind = GWY_PLAT_KIND_ALLOC;
            out->alloc_size = 0x40u;
            out->alloc_u16_at0 = 1u; /* tag accepted by cmp #1 / #3 */
            out->name = "plat_10204_get_tagged";
            out->evidence = "TARGET_OBSERVED";
            return;
        }
        out->kind = GWY_PLAT_KIND_STATUS;
        out->status_ret = 0;
        out->name = "plat_10204_status";
        out->evidence = "TARGET_OBSERVED";
        return;
    }

    /*
     * 0x10500: companion status after 0x10204 get (TARGET_OBSERVED).
     * gamelist: cmp r0,#0 / bne fail → requires MR_SUCCESS (0).
     */
    if (call->code == 0x10500u) {
        out->kind = GWY_PLAT_KIND_STATUS;
        out->status_ret = 0;
        out->name = "plat_10500_status";
        out->evidence = "TARGET_OBSERVED";
        return;
    }

    /*
     * 0x10800: platform capability ack (docs/06 + TARGET_OBSERVED robotol).
     * Legacy bridge SET_RET_V(1); status-0 after 10165 leaves init incomplete.
     */
    if (call->code == 0x10800u) {
        out->kind = GWY_PLAT_KIND_STATUS;
        out->status_ret = 1u;
        out->name = "plat_10800_ack";
        out->evidence = "CROSS_TARGET+docs/06";
        return;
    }

    /*
     * 0x10133: free companion to 0x10132/0x10134 (docs/06 + CROSS_TARGET).
     * Guest may pass user ptr or user-4. Product returns MR_SUCCESS; host freelist
     * free is best-effort only when the block is a known guest alloc (handled by
     * caller). Case-9 passes event_code-4 (non-heap) → intentional no-op success.
     */
    if (call->code == 0x10133u) {
        out->kind = GWY_PLAT_KIND_STATUS;
        out->status_ret = 0u;
        out->name = "plat_10133_free";
        out->evidence = "DOCUMENTED+CROSS_TARGET";
        return;
    }

    /*
     * 0x101AB: Path-A buffer fill for 10165 enqueue (0x30D24C → 0x2E4D6C → B54).
     * ABI (TARGET_OBSERVED): R1=buf, R2=out, R3=type(2), SP[0]=0.
     * Missing fill left B54 empty → UI_MODE (ER_RW+0x800+0xD0) stayed 0 → endless
     * sendAppEvent(0x1E209,9).
     */
    if (call->code == 0x101ABu && call->app >= 0x1000u) {
        out->kind = GWY_PLAT_KIND_BUFFER_FILL;
        out->status_ret = 0u;
        out->fill_buf = call->app;
        out->fill_type = call->arg3 ? call->arg3 : 2u;
        out->name = "plat_101ab_path_a_fill";
        out->evidence = "TARGET_OBSERVED+robotol_30D24C";
        return;
    }
}
