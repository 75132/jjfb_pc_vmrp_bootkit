#ifndef GWY_LAUNCHER_PLATFORM_EVENT_QUEUE_H
#define GWY_LAUNCHER_PLATFORM_EVENT_QUEUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-owned event list control object (proven from robotol 0x312AA4).
 * Layout: { uint32_t head; uint32_t count; } — both zero after ctor.
 * Published into the live ER_RW+0xB54 slot observed at runtime (never hardcoded).
 */

typedef enum {
    GWY_EVENT_QUEUE_NONE = 0,
    GWY_EVENT_QUEUE_ALLOCATED,
    GWY_EVENT_QUEUE_PUBLISHED,
    GWY_EVENT_QUEUE_INITIALIZED,
    GWY_EVENT_QUEUE_READY,
    GWY_EVENT_QUEUE_DESTROYED
} GwyEventQueueState;

typedef struct {
    uint64_t owner_module_id;
    uint64_t owner_generation;

    uint32_t context_10162;
    uint32_t context_10165;

    uint32_t owner_store_10162;
    uint32_t owner_store_10165;

    uint32_t list_object;
    uint32_t list_head; /* absolute ER_RW+0xB54 slot */
    uint32_t enqueue_handler;

    GwyEventQueueState state;
} GwyPlatformEventQueue;

void platform_event_queue_reset(void);
GwyPlatformEventQueue *platform_event_queue_get(void);

void platform_event_queue_note_ctx(uint32_t plat_code, uint32_t guest_ptr, uint32_t owner_store);
void platform_event_queue_note_enqueue_handler(uint32_t handler);

/*
 * If ER_RW+0xB54 is still null, allocate the proven 8-byte list control object
 * via guest alloc and publish the pointer into the live B54 slot.
 * Returns 1 when list head is ready (already was, or just published).
 */
int platform_event_queue_ensure_list_head(void *uc, uint32_t er_rw, uint64_t owner_module_id,
                                         uint64_t owner_generation);

int platform_event_queue_is_ready(void);
GwyEventQueueState platform_event_queue_state(void);

/*
 * After Path-A links a node, schedule the proven natural drain *trigger*
 * (periodic entry that BLs the B54 consumer). Does not call the consumer
 * directly. Returns 1 if a family/completion slot should deliver `*out_handler`.
 */
int platform_event_queue_need_drain_trigger(void *uc, uint32_t er_rw, uint32_t *out_handler);
void platform_event_queue_note_drain_scheduled(uint32_t handler);
void platform_event_queue_note_drain_delivered(uint32_t handler, int ok);
uint32_t platform_event_queue_drain_trigger(void);

#ifdef __cplusplus
}
#endif

#endif
