#ifndef GWY_LAUNCHER_PRODUCT_FIELD_PARSER_TRACE_H
#define GWY_LAUNCHER_PRODUCT_FIELD_PARSER_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Observe-only trace for 0x30A0CC field parser and 0x30A100 inner loop.
 *
 * Env: JJFB_FIELD_PARSER_TRACE=1
 *
 * Does not mutate guest state, force loop exit, pop queue, or fabricate stream bytes.
 */

int product_fp_enabled(void);
void product_fp_reset(void);
void product_fp_set_run_id(const char *run_id);
const char *product_fp_run_id(void);

void product_fp_bind_uc(void *uc);
void product_fp_note_er_rw(uint32_t er_rw);
void product_fp_note_module_range(uint32_t code_base, uint32_t code_size);
void product_fp_arm_hooks(void *uc);

void product_fp_note_helper_active(int active);
void product_fp_note_callback_depth(uint32_t depth);
void product_fp_note_consumer_enter(uint32_t queue_count);
void product_fp_note_consumer_exit(uint32_t queue_count);
void product_fp_note_drain_trigger(uint32_t queue_count);
void product_fp_note_drain_scheduled(uint32_t handler);
void product_fp_note_drain_delivered(int ok);
void product_fp_note_nested_publish(uint32_t queue_count);
void product_fp_note_nested_consume(uint32_t queue_count);
void product_fp_note_timer_tick(uint32_t tick);

void product_fp_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
