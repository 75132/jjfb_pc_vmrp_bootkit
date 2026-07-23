#ifndef GWY_LAUNCHER_PRODUCT_P5_EVENT_ADVANCE_H
#define GWY_LAUNCHER_PRODUCT_P5_EVENT_ADVANCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_P5_MAX_TXN 32

typedef enum GwyP5FamilyClass {
    GWY_P5_FAMILY_UNKNOWN = 0,
    GWY_P5_FAMILY_EVENT_ABI_CONFIRMED = 1,
    GWY_P5_FAMILY_EVENT_ABI_INCOMPLETE = 2,
    GWY_P5_FAMILY_EVENT_DUPLICATE_COMPLETION = 3,
    GWY_P5_FAMILY_EVENT_ACK_WITHOUT_STATE_CHANGE = 4,
    GWY_P5_FAMILY_EVENT_REQUIRES_SECOND_STAGE = 5,
    GWY_P5_FAMILY_EVENT_HANDLER_STATE_CHANGED = 6,
    GWY_P5_FAMILY_EVENT_FALSE_ACK = 7
} GwyP5FamilyClass;

typedef enum GwyP5TxnLifetime {
    GWY_P5_TXN_UNKNOWN = 0,
    GWY_P5_TXN_ONE_SHOT = 1,
    GWY_P5_TXN_HOST_REPLAYED = 2,
    GWY_P5_TXN_GUEST_REISSUED = 3,
    GWY_P5_TXN_LIFETIME_FIXED = 4
} GwyP5TxnVerdict;

typedef enum GwyP5Role10165 {
    GWY_P5_10165_UNKNOWN = 0,
    GWY_P5_10165_QUEUE_DRAIN_CONFIRMED = 1,
    GWY_P5_10165_NOT_REQUIRED = 2,
    GWY_P5_10165_ROLE_UNKNOWN = 3,
    GWY_P5_10165_SECOND_STAGE_MISSING = 4
} GwyP5Role10165;

typedef struct GwyPlatformEventTransaction {
    uint64_t request_id;
    uint64_t owner_module_id;
    uint64_t owner_generation;
    uint32_t event_code;
    uint32_t app;
    uint32_t payload;
    uint32_t caller_pc;
    uint32_t caller_lr;
    uint32_t r[8];
    uint32_t r9;
    uint32_t state_before;
    uint32_t state_after_handler;
    uint32_t state_after_10165;
    uint32_t state_next_timer;
    uint32_t handler_10102;
    uint32_t handler_10165;
    int32_t handler_ret;
    int32_t handler_10165_ret;
    int accepted;
    int completion_enqueued;
    int completion_delivered;
    int guest_acknowledged;
    int from_guest_sendappevent;
    int host_replay;
    int pointer_reads;
    int guest_writes;
    int reached_10165;
    int state_changed;
    char phase[24];
} GwyPlatformEventTransaction;

int product_p5_enabled(void);
int product_p5_one_shot_enabled(void);
void product_p5_reset(void);
void product_p5_set_run_id(const char *run_id);
const char *product_p5_run_id(void);

const char *gwy_p5_family_class_name(GwyP5FamilyClass c);
const char *gwy_p5_txn_verdict_name(GwyP5TxnVerdict v);
const char *gwy_p5_10165_role_name(GwyP5Role10165 r);

/* ER_RW digest helper (first 256 bytes). */
uint32_t product_p5_er_rw_digest(void *uc, uint32_t er_rw);

/*
 * On guest sendAppEvent family-band call. Returns:
 *  1 = accept and allow enqueue of at most one completion for this request
 *  0 = suppress completion (duplicate / one-shot block)
 */
int product_p5_on_guest_request(void *uc, uint32_t event_code, uint32_t app, uint32_t arg2,
                                uint32_t arg3, uint32_t arg4, uint32_t caller_pc, uint32_t caller_lr,
                                const uint32_t r[8], uint32_t r9, uint32_t er_rw,
                                uint64_t owner_module_id, uint64_t owner_generation,
                                uint32_t handler_10102, uint32_t handler_10165);

/* Before/after family handler delivery. */
void product_p5_on_handler_enter(void *uc, uint64_t request_id, uint32_t handler, uint32_t r0,
                                 uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9);
void product_p5_on_handler_leave(void *uc, uint64_t request_id, int ok, int32_t ret,
                                 uint32_t er_rw);
void product_p5_note_handler_mem(uint64_t request_id, int is_write, uint32_t addr, uint32_t size,
                                 uint32_t value);

void product_p5_on_10165(void *uc, uint64_t request_id, uint32_t handler, int ok, int32_t ret,
                         uint32_t er_rw);
void product_p5_on_next_timer(void *uc, uint32_t er_rw);

/* Active request awaiting delivery (0 if none). */
uint64_t product_p5_pending_request_id(uint32_t event_code, uint32_t app);
GwyPlatformEventTransaction *product_p5_txn_by_id(uint64_t request_id);

void product_p5_finalize(void);

GwyP5FamilyClass product_p5_family_class(void);
GwyP5TxnVerdict product_p5_txn_verdict(void);
GwyP5Role10165 product_p5_10165_role(void);
int product_p5_state_advanced(void);

#ifdef __cplusplus
}
#endif

#endif
