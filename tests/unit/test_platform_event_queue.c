#include "gwy_launcher/platform_event_queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    GwyPlatformEventQueue *q;
    platform_event_queue_reset();
    q = platform_event_queue_get();
    assert(q != NULL);
    assert(q->state == GWY_EVENT_QUEUE_NONE);

    platform_event_queue_note_ctx(0x10165u, 0x1000u, 0x2000u);
    platform_event_queue_note_ctx(0x10162u, 0x1100u, 0x2100u);
    platform_event_queue_note_enqueue_handler(0x30D2F9u);
    assert(q->context_10165 == 0x1000u);
    assert(q->context_10162 == 0x1100u);
    assert(q->owner_store_10165 == 0x2000u);
    assert(q->enqueue_handler == 0x30D2F9u);
    assert(!platform_event_queue_is_ready());

    /* ensure_list_head requires live uc — skipped in unit test. */
    printf("test_platform_event_queue: OK\n");
    return 0;
}
