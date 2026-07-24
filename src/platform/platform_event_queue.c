#include "gwy_launcher/platform_event_queue.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/product_event_queue_consumer.h"
#include <stdio.h>
#include <string.h>

/* Proven list control object size from robotol 0x312AA4 (MOVS r0,#8). */
#define GWY_EVENT_LIST_CTRL_SIZE 8u
#define GWY_EVENT_LIST_HEAD_OFF 0xB54u
/*
 * Proven natural drain trigger: periodic 0x305EB8 is the sole BL site to the
 * B54 consumer 0x2DC80C. Deliver Thumb entry (LSB=1) — Unicorn selects ISA
 * from uc_emu_start address LSB. Never jump to 0x2DC80C directly.
 */
#define GWY_EVENT_LIST_DRAIN_TRIGGER 0x305EB9u

extern uint32_t gwy_ext_obs_guest_malloc0(uint32_t size);

static GwyPlatformEventQueue g_q;
static int g_drain_sched;
static int g_drain_done;

void platform_event_queue_reset(void) {
    memset(&g_q, 0, sizeof(g_q));
    g_drain_sched = 0;
    g_drain_done = 0;
}

uint32_t platform_event_queue_drain_trigger(void) { return GWY_EVENT_LIST_DRAIN_TRIGGER; }

int platform_event_queue_need_drain_trigger(void *uc, uint32_t er_rw, uint32_t *out_handler) {
    uint32_t list = 0;
    uint32_t count = 0;
    if (out_handler) *out_handler = 0;
    /* Allow re-arm when a later Path-A push leaves the list nonempty again. */
    if (g_drain_sched && !g_drain_done) return 0;
    if (!uc || !er_rw) return 0;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, er_rw + GWY_EVENT_LIST_HEAD_OFF, &list))
        return 0;
    if (!list) list = g_q.list_object;
    if (!list) return 0;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, list + 4u, &count)) return 0;
    if (count == 0) return 0;
    if (out_handler) *out_handler = GWY_EVENT_LIST_DRAIN_TRIGGER;
    return 1;
}

void platform_event_queue_note_drain_scheduled(uint32_t handler) {
    g_drain_sched = 1;
    g_drain_done = 0;
    product_eqc_note_drain_scheduled(handler);
    printf("[EVENT_QUEUE_NONEMPTY_VISIBLE] drain_trigger=0x%X source=platform_event_queue "
           "evidence=OBSERVED\n",
           handler);
    fflush(stdout);
}

void platform_event_queue_note_drain_delivered(uint32_t handler, int ok) {
    g_drain_done = 1;
    g_drain_sched = 0; /* clear so a later nonempty push can re-arm */
    product_eqc_note_drain_delivered(handler, ok);
}

GwyPlatformEventQueue *platform_event_queue_get(void) { return &g_q; }

void platform_event_queue_note_ctx(uint32_t plat_code, uint32_t guest_ptr, uint32_t owner_store) {
    if (plat_code == 0x10165u) {
        g_q.context_10165 = guest_ptr;
        if (owner_store) g_q.owner_store_10165 = owner_store;
    } else if (plat_code == 0x10162u) {
        g_q.context_10162 = guest_ptr;
        if (owner_store) g_q.owner_store_10162 = owner_store;
    }
}

void platform_event_queue_note_enqueue_handler(uint32_t handler) {
    if (handler) g_q.enqueue_handler = handler;
}

int platform_event_queue_ensure_list_head(void *uc, uint32_t er_rw, uint64_t owner_module_id,
                                         uint64_t owner_generation) {
    uint32_t slot;
    uint32_t cur = 0;
    uint32_t list = 0;

    if (!uc || !er_rw) return 0;
    slot = er_rw + GWY_EVENT_LIST_HEAD_OFF;
    g_q.list_head = slot;
    g_q.owner_module_id = owner_module_id;
    g_q.owner_generation = owner_generation;

    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, slot, &cur)) return 0;
    if (cur) {
        g_q.list_object = cur;
        if (g_q.state < GWY_EVENT_QUEUE_READY) g_q.state = GWY_EVENT_QUEUE_READY;
        return 1;
    }

    /* Replicate proven 0x312AA4 ctor: alloc 8, zero head+count, publish to B54. */
    list = gwy_ext_obs_guest_malloc0(GWY_EVENT_LIST_CTRL_SIZE);
    if (!list) {
        printf("[PLATFORM_EVENT_QUEUE] op=ALLOC_FAIL size=8 slot=0x%X evidence=OBSERVED\n", slot);
        fflush(stdout);
        return 0;
    }
    g_q.list_object = list;
    g_q.state = GWY_EVENT_QUEUE_ALLOCATED;

    if (!guest_memory_uc_poke_u32((struct uc_struct *)uc, slot, list)) {
        printf("[PLATFORM_EVENT_QUEUE] op=PUBLISH_FAIL list=0x%X slot=0x%X evidence=OBSERVED\n",
               list, slot);
        fflush(stdout);
        return 0;
    }
    g_q.state = GWY_EVENT_QUEUE_READY;
    printf("[EVENT_LIST_HEAD_INITIALIZED] er_rw=0x%X slot=0x%X list=0x%X size=8 "
           "source=platform_event_queue ctor=0x312AA4_contract "
           "owner_module_id=%llu owner_generation=%llu evidence=OBSERVED\n",
           er_rw, slot, list, (unsigned long long)owner_module_id,
           (unsigned long long)owner_generation);
    fflush(stdout);
    return 1;
}

int platform_event_queue_is_ready(void) { return g_q.state >= GWY_EVENT_QUEUE_READY && g_q.list_object; }

GwyEventQueueState platform_event_queue_state(void) { return g_q.state; }
