#ifndef GWY_LAUNCHER_EXT_ENTRY_OBSERVE_H
#define GWY_LAUNCHER_EXT_ENTRY_OBSERVE_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/ext_loader.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_ENTRY_TRACE_RING 128
#define GWY_FAULT_DISASM_WINDOW 40
#define GWY_HELPER_ABI_CALLER_WINDOW 30

typedef enum ExtFaultRootCause {
    EXT_FAULT_CLASS_UNKNOWN = 0,
    EXT_FAULT_CLASS_ENTRY_ARGUMENT = 1,
    EXT_FAULT_CLASS_ER_RW_INIT = 2,
    EXT_FAULT_CLASS_RELOCATION = 3,
    EXT_FAULT_CLASS_PLATFORM_CONTEXT = 4,
    EXT_FAULT_CLASS_WRONG_ENTRY = 5,
    EXT_FAULT_CLASS_OTHER = 6
} ExtFaultRootCause;

typedef enum HelperAbiStage {
    HELPER_ABI_STAGE_CALL_SITE_BEFORE = 1,
    HELPER_ABI_STAGE_THUNK_ENTER = 2,
    HELPER_ABI_STAGE_ROBOTOL_ENTER = 3
} HelperAbiStage;

typedef struct GuestInstructionTrace {
    uint32_t pc;
    uint32_t module_offset;
    uint32_t regs[16]; /* r0-r12, sp, lr, pc */
    uint32_t cpsr;
} GuestInstructionTrace;

typedef struct ExtEntryCallContext {
    uint64_t module_id;
    uint64_t load_id;
    uint32_t helper_address;
    uint32_t entry_pc;
    uint32_t method;
    uint32_t input_address;
    uint32_t input_length;
    uint32_t er_rw;
    uint32_t stack_pointer;
    uint32_t function_table;
    uint32_t ext_chunk;
    uint32_t app_info;
    uint32_t caller_pc;
    uint64_t caller_module_id;
    char caller_module[96];
    uint32_t caller_offset;
    uint32_t r0, r1, r2, r3, r9;
    int valid;
} ExtEntryCallContext;

typedef struct HelperAbiBoundary {
    int valid;
    HelperAbiStage stage;
    uint64_t module_id;
    char module_name[96];
    uint32_t pc;
    uint32_t module_offset;
    uint64_t caller_module_id;
    char caller_module[96];
    uint32_t caller_offset;
    uint32_t helper_address;
    uint32_t target;
    uint32_t r0, r1, r2, r3;
    uint32_t r9, sp, lr, cpsr;
    char origin[24]; /* GUEST_NESTED | HOST_BRIDGE | GUEST_CALL */
} HelperAbiBoundary;

typedef struct GuestFaultReport {
    int valid;
    uint32_t access_type; /* uc_mem_type as uint */
    uint32_t invalid_address;
    uint32_t access_size;
    uint32_t fault_pc;
    uint32_t fault_lr;
    uint32_t fault_sp;
    uint32_t cpsr;
    uint32_t regs[16];
    uint64_t module_id;
    uint64_t load_id;
    uint32_t module_offset;
    uint32_t code_base;
    uint32_t code_size;
    int base_reg_index; /* -1 unknown */
    uint32_t base_reg_value;
    uint32_t imm_offset;
    char fault_instruction[96];
    ExtFaultRootCause root_cause;
    char fail_stage[32];
} GuestFaultReport;

/* Process-local observe state. */
void ext_entry_observe_reset(void);
void ext_entry_observe_bind_uc(void *uc);
const ExtEntryCallContext *ext_entry_observe_last_entry(void);
const GuestFaultReport *ext_entry_observe_last_fault(void);
const HelperAbiBoundary *ext_entry_observe_last_helper_abi(HelperAbiStage stage);

const char *ext_fault_root_cause_name(ExtFaultRootCause c);
const char *helper_abi_stage_name(HelperAbiStage s);

/* Resolve LR/PC to caller_module name + relative offset (never absolute-only). */
void ext_entry_observe_format_caller(uint32_t addr,
                                     char *name_out,
                                     size_t name_cap,
                                     uint32_t *offset_out,
                                     uint64_t *module_id_out);

/* Mark ENTRY_CALLED and fill context (first code in region or host pre-runCode). */
void ext_entry_observe_on_entry_begin(uint64_t module_id,
                                      uint32_t entry_pc,
                                      uint32_t method,
                                      uint32_t r0,
                                      uint32_t r1,
                                      uint32_t r2,
                                      uint32_t r3,
                                      uint32_t r9,
                                      uint32_t sp,
                                      uint32_t lr,
                                      uint32_t cpsr);

/* Three-boundary HELPER_ABI log (CALL_SITE / THUNK / ROBOTOL_ENTER). */
void ext_entry_observe_helper_abi(HelperAbiStage stage,
                                  uint64_t module_id,
                                  uint32_t pc,
                                  uint32_t helper_address,
                                  uint32_t target,
                                  const uint32_t regs[16],
                                  uint32_t cpsr,
                                  const char *origin,
                                  void *uc_for_caller_window);

/* Push one instruction into the ring for module_id (only while tracing). */
void ext_entry_observe_push_insn(uint64_t module_id,
                                 uint32_t pc,
                                 const uint32_t regs[16],
                                 uint32_t cpsr);

/*
 * Handle UC mem-invalid. uc is struct uc_struct*.
 * access_type matches unicorn uc_mem_type numeric values.
 */
void ext_entry_observe_on_mem_fault(void *uc,
                                    uint32_t access_type,
                                    uint64_t address,
                                    uint32_t size,
                                    int64_t value);

/* Minimal ARM LDR [Rn,#imm] decode (observe-only). Returns 1 on match. */
int ext_entry_decode_ldr_imm(uint32_t insn,
                             int thumb,
                             int *out_rt,
                             int *out_rn,
                             uint32_t *out_imm,
                             int *out_is_load);

/* Thumb BLX Rm: 010001 11 1 Rm 000. Returns 1 and Rm index. */
int ext_entry_decode_thumb_blx_rm(uint16_t half, int *out_rm);

/* Thumb BX Rm: 010001 11 0 Rm 000. Returns 1 and Rm index. */
int ext_entry_decode_thumb_bx_rm(uint16_t half, int *out_rm);

/* ARM BLX Rm. Returns 1 and Rm index. */
int ext_entry_decode_arm_blx_rm(uint32_t insn, int *out_rm);

/* ARM BX Rm. Returns 1 and Rm index. */
int ext_entry_decode_arm_bx_rm(uint32_t insn, int *out_rm);

/* Observe-only: remember P from _mr_c_function_new; optional field write watch. */
void ext_entry_observe_on_p_candidate(uint32_t p_guest, uint32_t p_len, void *uc);
void ext_entry_observe_bootstrap_event(const char *tag);

/* Classify fault entry vs header/chunk_field_04/helper (observe report). */
const char *ext_entry_observe_entry_class(uint64_t module_id, uint32_t observed_pc);

#ifdef __cplusplus
}
#endif

#endif
