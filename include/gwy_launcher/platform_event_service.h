#ifndef GWY_LAUNCHER_PLATFORM_EVENT_SERVICE_H
#define GWY_LAUNCHER_PLATFORM_EVENT_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic platform event request lifecycle (P6+).
 * Identity is NOT (event_code, app) alone — see identity_hash / guest_context.
 */

#define GWY_PES_MAX_EVENTS 48
#define GWY_PES_MAX_CTX 8
#define GWY_PES_CTX_SNAP 64

typedef enum GwyEventState {
    GWY_EVT_NONE = 0,
    GWY_EVT_GUEST_REQUESTED = 1,
    GWY_EVT_ACCEPTED = 2,
    GWY_EVT_CONTEXT_PREPARED = 3,
    GWY_EVT_QUEUED = 4,
    GWY_EVT_DELIVERED = 5,
    GWY_EVT_GUEST_ACKNOWLEDGED = 6,
    GWY_EVT_COMPLETED = 7
} GwyEventState;

typedef enum GwyEventIdentityClass {
    GWY_EVT_ID_UNKNOWN = 0,
    GWY_EVT_ID_SAME_UNFINISHED = 1, /* same object/token, still pending */
    GWY_EVT_ID_NEW_REQUEST = 2,     /* distinct identity each call */
    GWY_EVT_ID_MIXED = 3
} GwyEventIdentityClass;

typedef struct GwyPlatformEvent {
    uint64_t request_id;
    uint64_t identity_hash;
    uint64_t owner_module_id;
    uint64_t owner_generation;

    uint32_t family;
    uint32_t event_code;
    uint32_t app;

    uint32_t guest_context;
    uint32_t guest_payload;
    uint32_t payload_len;
    uint32_t stack_arg0;
    uint32_t stack_arg1;

    uint32_t handler;
    uint32_t secondary_handler;

    uint32_t caller_pc;
    uint32_t caller_lr;
    uint32_t r[8];
    uint32_t r9;
    uint64_t guest_insn_count;

    uint32_t state_digest_before;
    uint32_t state_digest_after;
    uint32_t context_digest_before;
    uint32_t context_digest_after;

    int32_t handler_ret;
    int mem_reads;
    int mem_writes;
    int stack_arg_reads;
    int context_touched;

    GwyEventState state;
    int from_guest;
    int accepted;
    int delivered;
    int acknowledged;
    int state_changed;
} GwyPlatformEvent;

typedef struct GwyEventContextObject {
    uint32_t guest_ptr;
    uint32_t size;
    uint32_t handler; /* optional enqueue trampoline from 10165 */
    uint32_t plat_code;
    uint32_t owner_store_addr; /* ER_RW slot that holds the pointer, if found */
    uint64_t owner_module_id;
    uint64_t alloc_insn_count;
    uint32_t digest_at_alloc;
    uint32_t digest_latest;
    int read_count;
    int write_count;
    int used;
    uint8_t snap_alloc[GWY_PES_CTX_SNAP];
    uint8_t snap_latest[GWY_PES_CTX_SNAP];
} GwyEventContextObject;

typedef struct GwyEventDeliveryAbi {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t stack0;
    uint32_t stack1;
    uint32_t sp_at_entry;
    uint32_t guest_context;
    uint32_t guest_payload;
    int have_stack;
    int have_context;
    int confirmed; /* set only after observed non-null contract use */
} GwyEventDeliveryAbi;

void platform_event_service_reset(void);
void platform_event_service_set_run_id(const char *run_id);
const char *platform_event_service_run_id(void);
void platform_event_service_set_insn_count(uint64_t n);
uint64_t platform_event_service_insn_count(void);
void platform_event_service_bump_insn(uint64_t delta);

const char *gwy_event_state_name(GwyEventState s);
const char *gwy_event_identity_class_name(GwyEventIdentityClass c);

uint64_t platform_event_identity_hash(uint32_t event_code, uint32_t app, const uint32_t r[8],
                                      uint32_t stack0, uint32_t stack1, uint32_t guest_context,
                                      uint32_t guest_payload);

/* 10165 / context object publication. */
GwyEventContextObject *platform_event_service_note_alloc(uint32_t plat_code, uint32_t size,
                                                         uint32_t guest_ptr, uint32_t handler,
                                                         uint64_t owner_module_id);
GwyEventContextObject *platform_event_service_ctx_by_ptr(uint32_t guest_ptr);
int platform_event_service_ctx_count(void);
GwyEventContextObject *platform_event_service_ctx_at(int index);
void platform_event_service_note_ctx_mem(uint32_t guest_ptr, int is_write, uint32_t addr,
                                         uint32_t size);
void platform_event_service_note_ctx_owner_store(uint32_t guest_ptr, uint32_t store_addr);
void platform_event_service_refresh_ctx_digest(void *uc, GwyEventContextObject *ctx);

/*
 * Guest family-band request. Returns 1 to enqueue delivery, 0 to suppress.
 * Does NOT treat (event_code,app) alone as duplicate identity.
 */
int platform_event_service_on_guest_request(void *uc, uint32_t event_code, uint32_t app,
                                            uint32_t family, uint32_t handler,
                                            uint32_t secondary_handler, uint32_t caller_pc,
                                            uint32_t caller_lr, const uint32_t r[8], uint32_t r9,
                                            uint32_t stack0, uint32_t stack1, uint32_t er_rw,
                                            uint64_t owner_module_id, uint64_t owner_generation,
                                            GwyPlatformEvent **out_ev);

GwyPlatformEvent *platform_event_service_by_id(uint64_t request_id);
GwyPlatformEvent *platform_event_service_pending_for_handler(uint32_t handler);
int platform_event_service_event_count(void);
GwyPlatformEvent *platform_event_service_event_at(int index);

void platform_event_service_mark_queued(uint64_t request_id);
void platform_event_service_on_deliver_enter(void *uc, uint64_t request_id,
                                             const GwyEventDeliveryAbi *abi);
void platform_event_service_note_handler_mem(uint64_t request_id, int is_write, uint32_t addr,
                                             uint32_t size, uint32_t value);
void platform_event_service_note_stack_arg_read(uint64_t request_id);
void platform_event_service_on_deliver_leave(void *uc, uint64_t request_id, int ok, int32_t ret,
                                             uint32_t er_rw);
void platform_event_service_on_next_timer(void *uc, uint32_t er_rw);

/* Round-A classification after enough guest samples. */
GwyEventIdentityClass platform_event_service_classify_identity(void);
int platform_event_service_state_advanced(void);

/* Suggest delivery ABI from recovered context (hypothesis until confirmed). */
void platform_event_service_suggest_delivery_abi(const GwyPlatformEvent *ev,
                                                GwyEventDeliveryAbi *out);

/*
 * Resolve owner-scoped 10165/10162 + enqueue handler for Path-A completion.
 * Never falls back to "latest object". Requires owner_store when available.
 * Returns 1 when both buffers and enqueue handler are known.
 */
int platform_event_service_resolve_completion_objs(uint32_t *out_10165, uint32_t *out_10162,
                                                   uint32_t *out_enq_handler,
                                                   uint32_t *out_owner_store);

/* Guest-visible unfinished field: ER_RW+(0x800+0xD0) UI_MODE / state switch. */
#define GWY_PES_UI_MODE_OFF (0x800u + 0xD0u)
uint32_t platform_event_service_read_ui_mode(void *uc, uint32_t er_rw);
void platform_event_service_note_ui_mode(void *uc, uint32_t er_rw, uint64_t request_id);

void platform_event_service_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
