#ifndef GWY_LAUNCHER_PRODUCT_P4_PROGRESS_H
#define GWY_LAUNCHER_PRODUCT_P4_PROGRESS_H

#include "gwy_launcher/product_callback_trace.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_P4_MAX_CALLBACKS 64
#define GWY_P4_MAX_BL_TARGETS 48
#define GWY_P4_MAX_PLAT_CALLS 32
#define GWY_P4_MAX_PREDICATES 64
#define GWY_P4_MAX_LEDGER 256
#define GWY_P4_MAX_CONTRACTS 128
#define GWY_P4_MAX_BB 512

typedef enum GwyP4CallbackClass {
    GWY_P4_CB_UNKNOWN = 0,
    GWY_P4_CB_PROGRESSING = 1,
    GWY_P4_CB_STABLE_POLL_LOOP = 2,
    GWY_P4_CB_PERIODIC_MULTI_PHASE = 3,
    GWY_P4_CB_STATE_CHANGES_NO_DRAW = 4,
    GWY_P4_CB_ALREADY_IDLE = 5
} GwyP4CallbackClass;

typedef enum GwyP4SourceClass {
    GWY_P4_SRC_UNKNOWN = 0,
    GWY_P4_SRC_PLATFORM_EVENT_PENDING = 1,
    GWY_P4_SRC_RESOURCE_COMPLETION_PENDING = 2,
    GWY_P4_SRC_FILE_IO_PENDING = 3,
    GWY_P4_SRC_NETWORK_COMPLETION_PENDING = 4,
    GWY_P4_SRC_TIME_DEADLINE = 5,
    GWY_P4_SRC_LIFECYCLE_STATE = 6,
    GWY_P4_SRC_PLATFORM_QUERY = 7,
    GWY_P4_SRC_USER_INPUT = 8,
    GWY_P4_SRC_GUEST_INTERNAL_STATE = 9
} GwyP4SourceClass;

typedef enum GwyP4LedgerVerdict {
    GWY_P4_LEDGER_UNKNOWN = 0,
    GWY_P4_LEDGER_TIMER_ONLY_NO_OTHER_WORK_EXPECTED = 1,
    GWY_P4_LEDGER_PLATFORM_COMPLETION_MISSING = 2,
    GWY_P4_LEDGER_EVENT_REGISTERED_NOT_GENERATED = 3,
    GWY_P4_LEDGER_RESOURCE_REQUEST_NO_COMPLETION = 4,
    GWY_P4_LEDGER_ASYNC_REQUEST_FALSE_SUCCESS = 5,
    GWY_P4_LEDGER_WORK_SOURCE_NOT_THE_BLOCKER = 6
} GwyP4LedgerVerdict;

typedef enum GwyP4DisplayVerdict {
    GWY_P4_DISP_NOT_REACHED = 0,
    GWY_P4_DISP_BLOCKED_BY_PLATFORM_STATE = 1,
    GWY_P4_DISP_BLOCKED_BY_RESOURCE_STATE = 2,
    GWY_P4_DISP_BLOCKED_BY_EVENT_STATE = 3,
    GWY_P4_DISP_REACHED_NO_REFRESH_CALL = 4
} GwyP4DisplayVerdict;

typedef enum GwyP4ContractVerdict {
    GWY_P4_CONTRACT_UNKNOWN = 0,
    GWY_P4_CONTRACT_FALSE_SUCCESS_FOUND = 1,
    GWY_P4_CONTRACT_OUTPUT_UNINITIALIZED = 2,
    GWY_P4_CONTRACT_ASYNC_COMPLETION_MISSING = 3,
    GWY_P4_CONTRACT_VALID_TO_STALL_POINT = 4
} GwyP4ContractVerdict;

int product_p4_enabled(void);
void product_p4_reset(void);
void product_p4_set_run_id(const char *run_id);
const char *product_p4_run_id(void);

const char *gwy_p4_callback_class_name(GwyP4CallbackClass c);
const char *gwy_p4_source_class_name(GwyP4SourceClass c);
const char *gwy_p4_ledger_verdict_name(GwyP4LedgerVerdict v);
const char *gwy_p4_display_verdict_name(GwyP4DisplayVerdict v);
const char *gwy_p4_contract_verdict_name(GwyP4ContractVerdict v);

/* Lifecycle: begin/end around natural guest callback UC run. */
void product_p4_callback_begin(void *uc, const GwyP3CallbackSnap *snap, uint32_t code_base,
                               uint32_t code_size, uint32_t er_rw, uint32_t p_guest,
                               uint32_t ext_chunk);
void product_p4_callback_end(void *uc, uint32_t seq, int ok, uint32_t uc_err,
                             const char *end_reason, int32_t ret);

/* Platform / work / VFS observations (any phase). */
void product_p4_note_platform_call(uint32_t code, uint32_t app, uint32_t arg2, uint32_t arg3,
                                   uint32_t arg4, uint32_t ret, uint32_t caller_pc,
                                   const char *kind_name, int sync, int completion_required,
                                   int output_initialized);
void product_p4_note_work(const char *op, uint32_t plat_code, uint32_t family, uint32_t handler,
                          uint64_t owner_module_id, uint64_t owner_generation,
                          const char *owner_module, int accepted, int completion_expected,
                          int completion_generated, int delivered, int guest_acked);
void product_p4_note_vfs(const char *op, const char *path, int ok);
void product_p4_note_display_target(const char *target, uint32_t va);
void product_p4_note_draw_or_refresh(const char *api, int is_refresh);
void product_p4_note_resource_request(const char *tag);
void product_p4_note_resource_completion(const char *tag);

/* Finalize CSVs + stall verdict after progress_map window. */
void product_p4_finalize(void);

/* Query helpers for runner / unit tests. */
uint32_t product_p4_callback_count(void);
GwyP4CallbackClass product_p4_overall_class(void);
GwyP4LedgerVerdict product_p4_ledger_verdict(void);
GwyP4ContractVerdict product_p4_contract_verdict(void);
GwyP4DisplayVerdict product_p4_display_verdict(void);
GwyP4SourceClass product_p4_stall_source_class(void);
int product_p4_stable_wait_predicate_found(void);
int product_p4_same_path_same_state(void);
int product_p4_early_change_then_stall(void);
uint32_t product_p4_first_sig_change_seq(void);
uint32_t product_p4_stable_from_seq(void);

/* Pure helpers for unit tests. */
uint32_t product_p4_hash_u32_seq(const uint32_t *vals, uint32_t n);
GwyP4CallbackClass product_p4_classify_signatures(const uint32_t *cf_hashes, uint32_t n,
                                                  const uint32_t *state_after, int any_draw);
int product_p4_line_forbidden_product(const char *line);

#ifdef __cplusplus
}
#endif

#endif
