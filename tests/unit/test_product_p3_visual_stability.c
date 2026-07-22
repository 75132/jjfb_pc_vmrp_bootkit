#include "gwy_launcher/platform_scheduler.h"
#include "gwy_launcher/platform_timer_cadence.h"
#include "gwy_launcher/product_callback_trace.h"
#include <stdio.h>
#include <string.h>

static int test_sentinel_ok_not_fault(void) {
    int cls = product_callback_trace_classify_fire_done(1, 0, "stop_at_base");
    if (cls != (int)GWY_P3_CALLBACK_RETURN_SENTINEL_OK) return 1;
    if (product_callback_trace_line_is_real_fault("FIRE_DONE ok=1 end=stop_at_base uc_err=0"))
        return 2;
    if (product_callback_trace_line_is_real_fault("something UC_ERR substring in uc_err=0"))
        return 3;
    return 0;
}

static int test_first_fault_freeze(void) {
    GwyP3CallbackSnap snap;
    product_callback_trace_reset();
    product_callback_trace_set_run_id("unit_p3");
    memset(&snap, 0, sizeof(snap));
    snap.handler = 0x30630D;
    snap.forced = 0;
    snprintf(snap.owner_module, sizeof(snap.owner_module), "robotol.ext");
    if (product_callback_trace_on_enter(&snap) != 1) return 10;
    product_callback_trace_on_leave(1, 0, 10, "UC_ERR_INSN_INVALID", 0x306338, 0, 0, 0, -1, 0);
    if (!product_callback_trace_halted()) return 11;
    if (product_callback_trace_fault_class() != GWY_P3_CALLBACK_GUEST_EXECUTION_FAULT) return 12;
    /* Second fault must not overwrite class once halted. */
    product_callback_trace_on_mem_fault(0x111, 0x222, 4, 1, 0);
    if (product_callback_trace_fault_class() != GWY_P3_CALLBACK_GUEST_EXECUTION_FAULT) return 13;
    if (product_callback_trace_on_enter(&snap) != 0) return 14;
    return 0;
}

static int test_timer_inflight_blocks_dup(void) {
    platform_timer_cadence_reset();
    platform_timer_cadence_set_run_id("unit_p3");
    platform_timer_cadence_note_arm(1, 7, 50, 1, "platform_timer");
    if (!platform_timer_cadence_try_queue_occurrence(1, 42)) return 20;
    platform_timer_cadence_note_dequeue(1, 42);
    platform_timer_cadence_note_inflight_begin(1, 42);
    if (platform_timer_cadence_try_queue_occurrence(1, 42)) return 21;
    if (platform_timer_cadence_duplicate_count() < 1) return 22;
    platform_timer_cadence_note_inflight_end(1, 42, 1);
    if (!platform_timer_cadence_try_queue_occurrence(1, 43)) return 23;
    if (platform_timer_cadence_classify() != GWY_TIMER_DUPLICATE_DELIVERY_FOUND) return 24;
    return 0;
}

static int test_forced_cannot_satisfy_natural(void) {
    GwyScheduledWork w;
    platform_scheduler_reset();
    platform_scheduler_set_run_id("unit_p3");
    memset(&w, 0, sizeof(w));
    w.source = GWY_SCHED_SRC_PLATFORM_TIMER;
    w.handler_or_helper = 0x30630D;
    w.forced = 1;
    snprintf(w.owner_module, sizeof(w.owner_module), "robotol.ext");
    platform_scheduler_note_natural_callback(&w, 0, 1);
    if (platform_scheduler_natural_callback_observed()) return 30;
    w.forced = 0;
    platform_scheduler_note_natural_callback(&w, 0, 1);
    if (!platform_scheduler_natural_callback_observed()) return 31;
    return 0;
}

static int test_visual_gates_reject_bind_refresh(void) {
    if (product_callback_trace_line_is_real_draw(
            "[JJFB_GAME_P_TIMELINE] op=BIND_REFRESH evidence=OBSERVED"))
        return 40;
    if (product_callback_trace_line_is_real_refresh(
            "[JJFB_GAME_P_TIMELINE] op=BIND_REFRESH evidence=OBSERVED"))
        return 41;
    if (!product_callback_trace_line_is_real_draw("[JJFB_DRAW] api=mr_drawBitmap")) return 42;
    if (!product_callback_trace_line_is_real_draw("[FIRST_NATURAL_DRAW] api=_DispUpEx")) return 43;
    if (!product_callback_trace_line_is_real_refresh(
            "[JJFB_REFRESH] api=_DispUpEx r0=0 evidence=OBSERVED"))
        return 44;
    if (!product_callback_trace_line_is_real_refresh("[FIRST_NATURAL_REFRESH] api=_DispUpEx"))
        return 45;
    if (!product_callback_trace_line_is_real_fault("[EXT_FAULT] pc=0x1")) return 46;
    if (!product_callback_trace_line_is_real_fault("FIRE_DONE ok=0 uc_err=10")) return 47;
    return 0;
}

int main(void) {
    int rc;
    if ((rc = test_sentinel_ok_not_fault()) != 0) {
        printf("FAIL sentinel_ok rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_first_fault_freeze()) != 0) {
        printf("FAIL first_fault_freeze rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_timer_inflight_blocks_dup()) != 0) {
        printf("FAIL timer_inflight rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_forced_cannot_satisfy_natural()) != 0) {
        printf("FAIL forced_natural rc=%d\n", rc);
        return rc;
    }
    if ((rc = test_visual_gates_reject_bind_refresh()) != 0) {
        printf("FAIL visual_gates rc=%d\n", rc);
        return rc;
    }
    printf("OK product_p3_visual_stability\n");
    return 0;
}
