#ifndef GWY_LAUNCHER_PRODUCT_CALLBACK_TRACE_H
#define GWY_LAUNCHER_PRODUCT_CALLBACK_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GwyP3FaultClass {
    GWY_P3_NO_GUEST_FAULT = 0,
    GWY_P3_CALLBACK_RETURN_SENTINEL_OK = 1,
    GWY_P3_CALLBACK_GUEST_EXECUTION_FAULT = 2,
    GWY_P3_CALLBACK_RETURN_SENTINEL_MISCLASSIFIED = 3,
    GWY_P3_CALLBACK_FRAME_RESTORE_BROKEN = 4,
    GWY_P3_CALLBACK_REENTRANCY = 5,
    GWY_P3_TIMER_DUPLICATE_DELIVERY = 6,
    GWY_P3_CALLBACK_STALE_GENERATION = 7,
    GWY_P3_CALLBACK_PLATFORM_CONTRACT_FAILURE = 8,
    GWY_P3_CALLBACK_RESOURCE_MEMORY_FAULT = 9,
    GWY_P3_CALLBACK_HOST_FAILURE = 10
} GwyP3FaultClass;

typedef struct GwyP3CallbackSnap {
    uint32_t seq;
    uint64_t timer_or_event_id;
    uint64_t owner_module_id;
    uint64_t owner_generation;
    uint32_t source; /* GwySchedulerSource */
    uint64_t due_time;
    uint64_t enqueue_time;
    uint64_t dequeue_time;
    uint32_t handler;
    uint32_t pc;
    uint32_t lr;
    uint32_t sp;
    uint32_t r9;
    uint32_t cpsr;
    uint32_t guest_depth;
    int forced;
    char owner_module[64];
} GwyP3CallbackSnap;

void product_callback_trace_reset(void);
void product_callback_trace_set_run_id(const char *run_id);
const char *product_callback_trace_run_id(void);

const char *gwy_p3_fault_class_name(GwyP3FaultClass c);

/* True after first real fault; scheduler must stop new work. */
int product_callback_trace_halted(void);
GwyP3FaultClass product_callback_trace_fault_class(void);
uint32_t product_callback_trace_seq(void);
uint32_t product_callback_trace_ok_returns(void);

void product_callback_trace_note_draw(void);
void product_callback_trace_note_refresh(void);
uint32_t product_callback_trace_draw_count(void);
uint32_t product_callback_trace_refresh_count(void);

void product_callback_trace_set_fb_sha256(const char *hex64);
const char *product_callback_trace_fb_sha256(void);

/* Enter natural/lifecycle callback; returns seq (0 if halted). */
uint32_t product_callback_trace_on_enter(const GwyP3CallbackSnap *snap);

/*
 * Leave after UC run. ok=1 + end_reason stop_at_base + uc_err=0 → sentinel OK.
 * Real fault: ok=0 or uc_err!=0.
 */
void product_callback_trace_on_leave(uint32_t seq, int ok, uint32_t uc_err, const char *end_reason,
                                     uint32_t pc_after, uint32_t lr_after, uint32_t sp_after,
                                     uint32_t r9_after, int32_t ret, uint32_t restored_r9);

/* Mem-invalid / host fault while callback in flight. */
void product_callback_trace_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr, uint32_t access_size,
                                         int is_write, int is_fetch);

void product_callback_trace_frame_enter(uint32_t seq, uint64_t frame_token, uint32_t handler,
                                        uint32_t r9, uint32_t stop_sentinel);
void product_callback_trace_handler_return(uint32_t seq, uint32_t pc, int32_t ret);
void product_callback_trace_frame_restore_begin(uint32_t seq, uint32_t saved_r9);
void product_callback_trace_frame_restore_complete(uint32_t seq, uint32_t restored_r9, int ok);

/* Unit-test helpers */
int product_callback_trace_classify_fire_done(int ok, uint32_t uc_err, const char *end_reason);
int product_callback_trace_line_is_real_draw(const char *line);
int product_callback_trace_line_is_real_refresh(const char *line);
int product_callback_trace_line_is_real_fault(const char *line);

void product_callback_trace_write_verdict_md(void);

/* Visual present bookkeeping (DispUpEx / draw path). */
void product_callback_trace_note_visual_row(const char *api, int32_t x, int32_t y, int32_t w,
                                           int32_t h, const char *fb_sha, int nonempty,
                                           int hwnd_visible, int captured);
void product_callback_trace_append_fb_hash(const char *api, const char *fb_sha, uint32_t nbytes);

#ifdef __cplusplus
}
#endif

#endif
