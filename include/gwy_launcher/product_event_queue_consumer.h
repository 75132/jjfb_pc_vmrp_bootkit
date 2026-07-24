#ifndef GWY_LAUNCHER_PRODUCT_EVENT_QUEUE_CONSUMER_H
#define GWY_LAUNCHER_PRODUCT_EVENT_QUEUE_CONSUMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product Event Queue Consumption Closure (TraceQueueConsumer).
 * Observe enqueue→next-tick timeline, list-control contract, natural consumer
 * (periodic drain trigger → B54 pop), node schema from consumer reads, and
 * whether empty body is accepted. Enabled by JJFB_PRODUCT_TRACE_QUEUE_CONSUMER=1.
 *
 * Proven (static+live):
 *   nonempty predicate = list_control.count @ +4 (via 0x312AC4)
 *   B54 drain entry     = 0x2DC80C
 *   natural trigger     = 0x305EB8 (sole BL → 0x2DC80C @ 0x305EBE)
 *   10140@0x30630D drains a different list (R9+0x11B0), not Path-A B54.
 */

int product_eqc_enabled(void);
void product_eqc_reset(void);
void product_eqc_set_run_id(const char *run_id);
const char *product_eqc_run_id(void);

void product_eqc_bind_uc(void *uc);
void product_eqc_note_er_rw(uint32_t er_rw);
void product_eqc_arm_code_hooks(void *uc);

void product_eqc_on_path_a_begin(void *uc, uint32_t list, uint32_t entry, uint32_t count);
void product_eqc_on_path_a_linked(void *uc, uint32_t list, uint32_t head, uint32_t node,
                                  uint32_t entry, uint32_t count);
void product_eqc_on_path_a_return(void *uc, uint32_t list);
void product_eqc_on_timer_decision(void *uc, uint32_t er_rw, uint32_t tick);
void product_eqc_on_vfs(const char *path, uint64_t insn, int after_first_link);
void product_eqc_note_drain_delivered(uint32_t handler, int ok);
void product_eqc_note_drain_scheduled(uint32_t handler);

void product_eqc_finalize(void);

/* Contract helpers shared with PlatformEventQueue drain scheduling. */
uint32_t product_eqc_drain_trigger_va(void);
uint32_t product_eqc_drain_entry_va(void);
int product_eqc_list_nonempty(void *uc, uint32_t list);
uint32_t product_eqc_peek_count(void *uc, uint32_t list);
uint32_t product_eqc_peek_head(void *uc, uint32_t list);

#ifdef __cplusplus
}
#endif

#endif
