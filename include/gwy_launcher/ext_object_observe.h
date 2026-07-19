#ifndef GWY_LAUNCHER_EXT_OBJECT_OBSERVE_H
#define GWY_LAUNCHER_EXT_OBJECT_OBSERVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Neutral DSM entry-selection object — not assumed to be mrc_extChunk. */
typedef enum ExtObjectKind {
    EXT_OBJECT_UNKNOWN = 0,
    EXT_OBJECT_C_FUNCTION_ST = 1,
    EXT_OBJECT_MRC_EXT_CHUNK = 2,
    EXT_OBJECT_MODULE_DESCRIPTOR = 3,
    EXT_OBJECT_LOADER_RECORD = 4,
    EXT_OBJECT_FUNCTION_TABLE = 5,
    EXT_OBJECT_OTHER = 6
} ExtObjectKind;

typedef enum GuestAddressClass {
    ADDR_DSM_CODE = 1,
    ADDR_DSM_RW = 2,
    ADDR_MODULE_CODE = 3,
    ADDR_MODULE_RW = 4,
    ADDR_HEAP = 5,
    ADDR_STACK = 6,
    ADDR_PLATFORM_TABLE = 7,
    ADDR_UNKNOWN = 0
} GuestAddressClass;

typedef enum ExtIdentityConfidence {
    EXT_IDENTITY_UNKNOWN = 0,
    EXT_IDENTITY_CANDIDATE = 1,
    EXT_IDENTITY_PROBABLE = 2,
    EXT_IDENTITY_CONFIRMED = 3
} ExtIdentityConfidence;

/* A5: hop classification for numbered transfers. */
typedef enum ExtTransferHopKind {
    EXT_HOP_OTHER = 0,
    EXT_HOP_DSM_TO_LOADER = 1,
    EXT_HOP_DSM_TO_ROBOTOL = 2
} ExtTransferHopKind;

/* A5: arg role from early disasm evidence only. */
typedef enum ExtArgRole {
    EXT_ARG_UNKNOWN = 0,
    EXT_ARG_COMMAND = 1,
    EXT_ARG_CONTEXT = 2,
    EXT_ARG_BUFFER = 3,
    EXT_ARG_LENGTH = 4
} ExtArgRole;

/* Evidence bits for candidate_chunk ↔ module bind (≥2 required to raise confidence). */
#define EXT_ID_EV_LOADER_RETURN (1u << 0)
#define EXT_ID_EV_C_FUNCTION_NEW (1u << 1)
#define EXT_ID_EV_FIELD_POINTS_MODULE (1u << 2)
#define EXT_ID_EV_LIFECYCLE_MATCH (1u << 3)
#define EXT_ID_EV_DSM_RECEIVED (1u << 4)

typedef struct ExtCallTransfer {
    int valid;
    uint32_t transfer_id;
    ExtTransferHopKind hop_kind;
    uint64_t caller_module_id;
    uint32_t caller_pc;
    uint32_t caller_target;
    uint64_t dispatch_module_id;
    uint32_t dispatch_entry;
    uint64_t callee_module_id;
    uint32_t callee_first_pc;
    uint32_t r0;
    uint32_t r1;
    uint32_t lr;
} ExtCallTransfer;

typedef struct ExtAddrClassResult {
    GuestAddressClass addr_class;
    uint64_t module_id;
    char module_name[96];
    uint32_t module_offset;
} ExtAddrClassResult;

typedef struct ExtEntryObjectTrace {
    int valid;
    int base_reg;
    uint32_t base_value;
    uint32_t field_offset;
    uint32_t field_value;
    uint32_t dispatch_target;
    char base_source[32]; /* stack|global|module_table|argument|return_value|pc_rel_add_r9|unknown */
    uint32_t source_address;
    char producer_module[96];
    uint32_t producer_offset;
    ExtObjectKind object_kind;
    ExtIdentityConfidence confidence;
} ExtEntryObjectTrace;

void ext_object_observe_reset(void);
int ext_object_identity_enabled(void);
/* A5: GWY_DISPATCH_TRACE=1 — dispatcher ENTER/EXIT, SECOND_HOP, pointer writes. */
int ext_object_dispatch_trace_enabled(void);

const char *ext_object_kind_name(ExtObjectKind k);
const char *guest_address_class_name(GuestAddressClass c);
const char *ext_identity_confidence_name(ExtIdentityConfidence c);
const char *ext_transfer_hop_kind_name(ExtTransferHopKind k);
const char *ext_arg_role_name(ExtArgRole r);

/* Classify VA using ModuleRegistry code/RW maps (never active-session inference). */
void ext_object_classify_address(uint32_t va, uint32_t sp_hint, ExtAddrClassResult *out);

void ext_object_log_addr_class(const char *role, uint32_t va, uint32_t sp_hint);

/* Record / query last CROSS_MODULE_CALL transfer (assigns transfer_id when tracing). */
void ext_object_note_cross_module_call(uint64_t from_mid,
                                       uint32_t from_pc,
                                       uint64_t to_mid,
                                       uint32_t target,
                                       uint32_t r0,
                                       uint32_t r1,
                                       uint32_t lr);
/* Full-reg variant for DISPATCH_CALL ENTER (regs may be NULL → same as basic). */
void ext_object_note_cross_module_call_ex(uint64_t from_mid,
                                          uint32_t from_pc,
                                          uint64_t to_mid,
                                          uint32_t target,
                                          const uint32_t *regs,
                                          uint32_t cpsr,
                                          void *uc);
const ExtCallTransfer *ext_object_last_transfer(void);

/* While in dispatcher / any hooked module: detect EXIT and STR→code pointers. */
void ext_object_dispatch_on_code(void *uc,
                                 uint64_t module_id,
                                 uint32_t pc,
                                 const uint32_t regs[16],
                                 uint32_t cpsr);

/* DSM select: reverse FIELD_LOAD + BASE_SOURCE + identity case. */
void ext_object_observe_on_dsm_select(void *uc,
                                      uint32_t call_pc,
                                      uint32_t r9,
                                      uint32_t sp,
                                      uint32_t r0_at_blx,
                                      int base_reg,
                                      uint32_t dispatch_target,
                                      uint32_t candidate_chunk_base,
                                      const char *caller_module,
                                      uint32_t caller_offset);

const ExtEntryObjectTrace *ext_object_last_entry_trace(void);

/* Optional ±30 disasm of dispatch target when GWY_OBJECT_IDENTITY=1. */
void ext_object_disasm_dispatch(void *uc, uint32_t dispatch_target);

#ifdef __cplusplus
}
#endif

#endif
