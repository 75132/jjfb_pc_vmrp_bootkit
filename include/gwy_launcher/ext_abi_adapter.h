#ifndef GWY_LAUNCHER_EXT_ABI_ADAPTER_H
#define GWY_LAUNCHER_EXT_ABI_ADAPTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Documented EXT helper method codes (mythroad_mini mr_doExt / case_801). */
#define GWY_EXT_METHOD_INIT 0u
#define GWY_EXT_METHOD_VERSION 6u
#define GWY_EXT_METHOD_APPINFO 8u
#define GWY_EXT_MR_VERSION 2011u

typedef enum GwyExtAbiContextVerdict {
    EXT_ABI_CONTEXT_READY = 0,
    EXT_ABI_CONTEXT_INCOMPLETE = 1,
    EXT_ABI_OWNER_MISMATCH = 2,
    EXT_ABI_STALE_GENERATION = 3
} GwyExtAbiContextVerdict;

typedef struct GwyExtAbiContext {
    uint64_t module_id;
    uint64_t module_generation;

    uint32_t helper;
    uint32_t p_guest;
    uint32_t er_rw_guest;
    uint32_t er_rw_size;
    uint32_t ext_chunk_guest;

    uint32_t appid;
    uint32_t appver;
    uint32_t mr_version;

    char module_name[64];
} GwyExtAbiContext;

/*
 * Host helper call: int32 mr_extHelper(P, code, input, input_len) with R9=erw.
 * Tests inject a fake; bridge registers the real Unicorn path.
 */
typedef int32_t (*GwyExtHelperCallFn)(void *uc, uint32_t helper, uint32_t p_guest, uint32_t method,
                                      uint32_t input, uint32_t input_len, uint32_t erw);

typedef uint32_t (*GwyExtAppInfoAllocFn)(uint32_t appid, uint32_t appver);

typedef enum GwyExtHandshakeResult {
    GWY_EXT_HS_OK = 0,
    GWY_EXT_HS_CONTEXT_NOT_READY = 1,
    GWY_EXT_HS_ALREADY_DONE = 2,
    GWY_EXT_HS_VERSION_FAILED = 3,
    GWY_EXT_HS_APPINFO_FAILED = 4,
    GWY_EXT_HS_INIT_FAILED = 5,
    GWY_EXT_HS_NO_CALL_FN = 6,
    GWY_EXT_HS_NOT_PENDING = 7
} GwyExtHandshakeResult;

void ext_abi_adapter_reset(void);
void ext_abi_adapter_set_run_id(const char *run_id);
const char *ext_abi_adapter_run_id(void);

void ext_abi_adapter_set_helper_call(GwyExtHelperCallFn fn);
void ext_abi_adapter_set_appinfo_alloc(GwyExtAppInfoAllocFn fn);

const char *ext_abi_context_verdict_name(GwyExtAbiContextVerdict v);

/* Validate pointers/owners; does not hardcode live addresses. */
GwyExtAbiContextVerdict ext_abi_adapter_build_context(GwyExtAbiContext *out);

/* Queue handshake once for this generation (idempotent). */
void ext_abi_adapter_request_handshake(uint64_t module_id, uint64_t generation,
                                       const char *module_name);

int ext_abi_adapter_handshake_pending(void);
uint64_t ext_abi_adapter_done_generation(void);

/*
 * Run VERSION→APPINFO→INIT exactly once per generation when pending + READY.
 * Emits EXT_ABI_* / ROBOTOL_INIT_RETURN_ZERO markers.
 */
GwyExtHandshakeResult ext_abi_adapter_try_deliver(void *uc);

/* Unit-test helpers: inject a ready context without registry. */
void ext_abi_adapter_test_set_context(const GwyExtAbiContext *ctx, GwyExtAbiContextVerdict verdict);
void ext_abi_adapter_test_clear_inject(void);

/* Observed returns from last deliver (or -1 if not called). */
int32_t ext_abi_adapter_last_version_ret(void);
int32_t ext_abi_adapter_last_appinfo_ret(void);
int32_t ext_abi_adapter_last_init_ret(void);
int ext_abi_adapter_init_was_zero(void);

/* Call-order bookkeeping for tests (methods invoked in order). */
int ext_abi_adapter_test_call_count(void);
uint32_t ext_abi_adapter_test_call_method(int index);

#ifdef __cplusplus
}
#endif

#endif
