#include "gwy_launcher/platform_event_queue.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/platform_path_a_response.h"
#include "gwy_launcher/product_event_queue_consumer.h"
#include "gwy_launcher/product_field_parser_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Proven list control object size from robotol 0x312AA4 (MOVS r0,#8). */
#define GWY_EVENT_LIST_CTRL_SIZE 8u
#define GWY_EVENT_LIST_HEAD_OFF 0xB54u
/* Lifecycle record list slot: 0x2E4040 loads *B58 as r0 into 0x2F68E4. */
#define GWY_LIFECYCLE_LIST_HEAD_OFF 0xB58u
/* Path-A hdr-consume gate byte (LDRSB [R9+0x15C,#0] at 0x2E4D88) — not 0x15D. */
#define GWY_PATH_A_HDR_FLAG_OFF 0x15Cu
/* A90+4 receives BE hdr when gate is armed (STR at 0x2E4DA8). */
#define GWY_PATH_A_HDR_SLOT_OFF (0xA90u + 4u)
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

/* Path-A framing arm state — one poke per module generation. */
static int g_path_a_armed;
static uint64_t g_path_a_arm_module_id;
static uint64_t g_path_a_arm_generation;
static uint32_t g_path_a_arm_er_rw;

static int path_a_module_is_jjfb_robotol(const char *module_name);

void platform_event_queue_reset(void) {
    memset(&g_q, 0, sizeof(g_q));
    g_drain_sched = 0;
    g_drain_done = 0;
    g_path_a_armed = 0;
    g_path_a_arm_module_id = 0;
    g_path_a_arm_generation = 0;
    g_path_a_arm_er_rw = 0;
    platform_path_a_response_reset();
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
    product_fp_note_drain_scheduled(handler);
    printf("[EVENT_QUEUE_NONEMPTY_VISIBLE] drain_trigger=0x%X source=platform_event_queue "
           "evidence=OBSERVED\n",
           handler);
    fflush(stdout);
}

void platform_event_queue_note_drain_delivered(uint32_t handler, int ok) {
    g_drain_done = 1;
    g_drain_sched = 0; /* clear so a later nonempty push can re-arm */
    product_eqc_note_drain_delivered(handler, ok);
    product_fp_note_drain_delivered(ok);
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

/*
 * Empty lifecycle-record list at ER_RW+B58 (same 8-byte {head,count} as B54).
 *
 * Proven: 0x2E4040 does LDR r0,[R9+B58] then BL 0x2F68E4; helper pushes each
 * parsed record via 0x312A60(list, record). Guest's natural ctor is at
 * 0x2FE970 (BL 0x312AA4 → STR to B58) but that init is not reached before the
 * first cold-start Path-A with a body record.
 *
 * This only publishes an empty list control object — never fabricates records,
 * never writes B71/15D/B70/UI_MODE, never host-enqueues.
 */
int platform_event_queue_ensure_lifecycle_list(void *uc, uint32_t er_rw, uint64_t module_id,
                                               uint64_t module_generation,
                                               const char *module_name) {
    uint32_t slot;
    uint32_t cur = 0;
    uint32_t list = 0;

    if (!uc || !er_rw) return 0;
    if (!path_a_module_is_jjfb_robotol(module_name)) {
        printf("[LIFECYCLE_LIST] op=SKIP_NON_ROBOTOL er_rw=0x%X module=%s evidence=OBSERVED\n",
               er_rw, module_name ? module_name : "?");
        fflush(stdout);
        return 0;
    }

    slot = er_rw + GWY_LIFECYCLE_LIST_HEAD_OFF;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, slot, &cur)) return 0;
    if (cur) {
        printf("[LIFECYCLE_LIST] op=ALREADY er_rw=0x%X slot=0x%X list=0x%X "
               "module_id=%llu generation=%llu evidence=OBSERVED\n",
               er_rw, slot, cur, (unsigned long long)module_id,
               (unsigned long long)module_generation);
        fflush(stdout);
        return 1;
    }

    list = gwy_ext_obs_guest_malloc0(GWY_EVENT_LIST_CTRL_SIZE);
    if (!list) {
        printf("[LIFECYCLE_LIST] op=ALLOC_FAIL size=8 slot=0x%X evidence=OBSERVED\n", slot);
        fflush(stdout);
        return 0;
    }
    if (!guest_memory_uc_poke_u32((struct uc_struct *)uc, slot, list)) {
        printf("[LIFECYCLE_LIST] op=PUBLISH_FAIL list=0x%X slot=0x%X evidence=OBSERVED\n", list,
               slot);
        fflush(stdout);
        return 0;
    }
    printf("[LIFECYCLE_LIST_INITIALIZED] er_rw=0x%X slot=0x%X list=0x%X size=8 "
           "ctor=0x312AA4_contract site=0x2FE970_equiv module_id=%llu generation=%llu "
           "evidence=OBSERVED\n",
           er_rw, slot, list, (unsigned long long)module_id,
           (unsigned long long)module_generation);
    fflush(stdout);
    return 1;
}

int platform_path_a_event_contract_enabled(void) {
    const char *e = getenv("JJFB_PATH_A_EVENT_CONTRACT");
    /* Default ON: cold-start framing must match the proven warm (Call3) path. */
    if (!e || !e[0]) return 1;
    if (e[0] == '0' && e[1] == '\0') return 0;
    return 1;
}

static int path_a_module_is_jjfb_robotol(const char *module_name) {
    if (!module_name || !module_name[0]) return 0;
    if (strstr(module_name, "robotol")) return 1;
    if (strstr(module_name, "mmochat")) return 1; /* JJFB alias in some packs */
    return 0;
}

int platform_event_queue_ensure_path_a_framing(void *uc, uint32_t er_rw, uint64_t module_id,
                                              uint64_t module_generation,
                                              const char *module_name) {
    uint8_t flag = 1;
    uint8_t old_flag = 0;
    uint32_t old_hdr = 0;
    uint32_t zero = 0;
    int same_generation;

    /* contract=0 → full restore of cold-start framing (no poke). */
    if (!platform_path_a_event_contract_enabled()) return 0;
    if (!uc || !er_rw) return 0;

    /* Fixed ER_RW offsets are proven only for JJFB/Robotol — never all MRPs. */
    if (!path_a_module_is_jjfb_robotol(module_name)) {
        printf("[PATH_A_EVENT_CONTRACT] op=SKIP_NON_ROBOTOL er_rw=0x%X module=%s "
               "module_id=%llu evidence=OBSERVED\n",
               er_rw, module_name ? module_name : "?", (unsigned long long)module_id);
        fflush(stdout);
        return 0;
    }

    same_generation =
        g_path_a_armed && g_path_a_arm_er_rw == er_rw &&
        g_path_a_arm_module_id == module_id &&
        g_path_a_arm_generation == module_generation && module_generation != 0;

    (void)guest_memory_uc_peek((struct uc_struct *)uc, er_rw + GWY_PATH_A_HDR_FLAG_OFF, &old_flag,
                               1);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, er_rw + GWY_PATH_A_HDR_SLOT_OFF,
                                   &old_hdr);

    /*
     * Idempotent ALREADY for the same module generation: do not re-poke 0x15C/A94
     * after Guest has begun consuming the Path-A stream.
     */
    if (same_generation || (g_path_a_armed && g_path_a_arm_er_rw == er_rw && old_flag == 1u &&
                            old_hdr == 0u && module_generation == g_path_a_arm_generation)) {
        printf("[PATH_A_CONTRACT_ARM] module_id=%llu module_generation=%llu er_rw=0x%X "
               "reason=ALREADY old_15C=%u old_A94=0x%X new_15C=%u new_A94=0x%X "
               "evidence=OBSERVED\n",
               (unsigned long long)module_id, (unsigned long long)module_generation, er_rw,
               (unsigned)old_flag, old_hdr, (unsigned)old_flag, old_hdr);
        fflush(stdout);
        return 1;
    }

    if (!guest_memory_uc_poke((struct uc_struct *)uc, er_rw + GWY_PATH_A_HDR_FLAG_OFF, &flag, 1)) {
        printf("[PATH_A_EVENT_CONTRACT] op=FLAG_POKE_FAIL er_rw=0x%X off=0x15C evidence=OBSERVED\n",
               er_rw);
        fflush(stdout);
        return 0;
    }
    if (!guest_memory_uc_poke_u32((struct uc_struct *)uc, er_rw + GWY_PATH_A_HDR_SLOT_OFF, zero)) {
        printf("[PATH_A_EVENT_CONTRACT] op=HDR_SLOT_POKE_FAIL er_rw=0x%X off=0xA94 "
               "evidence=OBSERVED\n",
               er_rw);
        fflush(stdout);
        return 0;
    }

    g_path_a_armed = 1;
    g_path_a_arm_module_id = module_id;
    g_path_a_arm_generation = module_generation ? module_generation : 1ull;
    g_path_a_arm_er_rw = er_rw;

    printf("[PATH_A_CONTRACT_ARM] module_id=%llu module_generation=%llu er_rw=0x%X "
           "reason=FIRST_PATH_A_FILL old_15C=%u old_A94=0x%X new_15C=1 new_A94=0 "
           "evidence=OBSERVED\n",
           (unsigned long long)module_id, (unsigned long long)g_path_a_arm_generation, er_rw,
           (unsigned)old_flag, old_hdr);
    fflush(stdout);
    return 1;
}

int platform_event_queue_is_ready(void) { return g_q.state >= GWY_EVENT_QUEUE_READY && g_q.list_object; }

GwyEventQueueState platform_event_queue_state(void) { return g_q.state; }
