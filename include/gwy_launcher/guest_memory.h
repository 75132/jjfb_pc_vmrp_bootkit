#ifndef GWY_LAUNCHER_GUEST_MEMORY_H
#define GWY_LAUNCHER_GUEST_MEMORY_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/vm_runtime.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GwyMemoryAccess {
    GWY_MEM_READ = 1,
    GWY_MEM_WRITE = 2
} GwyMemoryAccess;

typedef enum GwyR9WriteReason {
    GWY_R9_WRITE_OTHER = 0,
    GWY_R9_WRITE_MODULE_R9_SWITCH_ENTER = 1,
    GWY_R9_WRITE_MODULE_R9_SWITCH_LEAVE = 2,
    GWY_R9_WRITE_MODULE_R9_SWITCH_ABORT = 3,
    GWY_R9_WRITE_GUEST_CONTEXT_RESET = 4,
    GWY_R9_WRITE_EMULATION_RESTART = 5,
    GWY_R9_WRITE_HELPER_WRAPPER = 6,
    GWY_R9_WRITE_CALLBACK_RESUME = 7,
    GWY_R9_WRITE_BRIDGE_RAW_UC = 8
} GwyR9WriteReason;

typedef struct GwyR9WriteAudit {
    GwyR9WriteReason reason;
    uint64_t frame_id;
    uint64_t scope_id;
    const char *host_callsite;
    uint32_t guest_pc;
    const char *guest_module;
    uint32_t depth_before;
    uint32_t depth_after;
} GwyR9WriteAudit;

LauncherStatus guest_memory_validate_range(VmRuntime *rt,
                                           uint32_t guest_address,
                                           size_t size,
                                           GwyMemoryAccess access,
                                           LauncherError *err);

LauncherStatus guest_memory_read(VmRuntime *rt,
                                 uint32_t guest_address,
                                 void *buffer,
                                 size_t size,
                                 LauncherError *err);

LauncherStatus guest_memory_write(VmRuntime *rt,
                                  uint32_t guest_address,
                                  const void *buffer,
                                  size_t size,
                                  LauncherError *err);

/* Reads until NUL or capacity-1; always NUL-terminates buffer on success. */
LauncherStatus guest_memory_read_cstring(VmRuntime *rt,
                                         uint32_t guest_address,
                                         char *buffer,
                                         size_t capacity,
                                         LauncherError *err);

/*
 * Observe-only peek for hooks that hold a raw Unicorn engine (live vmrp bridge).
 * Does not go through VmRuntime region tables; callers must already constrain addr.
 * Returns 1 on success, 0 on failure.
 */
int guest_memory_uc_peek(struct uc_struct *uc, uint32_t guest_address, void *buffer, size_t size);
int guest_memory_uc_peek_u32(struct uc_struct *uc, uint32_t guest_address, uint32_t *out);
int guest_memory_uc_poke(struct uc_struct *uc, uint32_t guest_address, const void *buffer,
                         size_t size);
int guest_memory_uc_poke_u32(struct uc_struct *uc, uint32_t guest_address, uint32_t value);

/*
 * Phase 6K: bounded nested emu for DOCUMENTED MRPGCMAP entry (guest_code_base+8).
 * Saves/restores R0–R12, SP, LR, PC, CPSR; sets R0 and LR=stop; runs until stop or insn limit.
 * Returns 1 on UC_ERR_OK (or stop), 0 on hard failure. Does not invent guest memory.
 */
int guest_memory_uc_run_bounded(struct uc_struct *uc, uint32_t start_pc, uint32_t stop_addr,
                                uint32_t r0_value, uint64_t insn_limit, char *err_buf,
                                size_t err_cap);

/* Phase 6L: ABI overlay for documented entry (still saves/restores all GPRs). */
typedef struct GwyUcEntryAbi {
    uint32_t set_r0;
    uint32_t set_r1;
    uint32_t set_r2;
    uint32_t set_r3;
    uint32_t set_lr;
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t lr;
    int mirror_full; /* write r[0..12], sp, cpsr from mirror_* then apply set_* */
    uint32_t mirror_r[13];
    uint32_t mirror_sp;
    uint32_t mirror_cpsr;
} GwyUcEntryAbi;

typedef struct GwyUcEntryRunOut {
    int ok;
    unsigned uc_err;
    uint32_t pc_after;
    uint32_t r0_after;
    uint32_t r1_after;
    uint32_t r9_after;
    uint32_t sp_after;
    uint32_t lr_after;
    uint32_t cpsr_after;
    char end_reason[40];
    char err_detail[96];
} GwyUcEntryRunOut;

int guest_memory_uc_run_entry_ex(struct uc_struct *uc, uint32_t start_pc, uint32_t stop_addr,
                                 uint64_t insn_limit, const GwyUcEntryAbi *abi,
                                 GwyUcEntryRunOut *out);

/* Phase 6C-A/D2: only allowed guest register mutation surface (R9 / SB). */
int guest_memory_uc_read_r9(struct uc_struct *uc, uint32_t *out_r9);
int guest_memory_uc_write_r9(struct uc_struct *uc, uint32_t r9);
int guest_memory_uc_write_r9_ex(struct uc_struct *uc, uint32_t r9, const GwyR9WriteAudit *audit);
int guest_memory_uc_read_sp(struct uc_struct *uc, uint32_t *out_sp);
int guest_memory_uc_write_sp(struct uc_struct *uc, uint32_t sp);

const char *gwy_r9_write_reason_name(GwyR9WriteReason r);
uint64_t guest_memory_r9_write_event_seq(void);

/* First observed loss  nonzero→0 (or any old→new) for D2 reports. */
typedef struct GwyR9WriteRecord {
    int valid;
    uint64_t event_seq;
    uint32_t old_r9;
    uint32_t new_r9;
    GwyR9WriteReason reason;
    uint64_t frame_id;
    uint64_t scope_id;
    char host_callsite[72];
    uint32_t guest_pc;
    char guest_module[72];
    uint32_t depth_before;
    uint32_t depth_after;
} GwyR9WriteRecord;

void guest_memory_r9_write_reset(void);
int guest_memory_r9_first_zeroing(GwyR9WriteRecord *out); /* old!=0 && new==0 */
int guest_memory_r9_last_write(GwyR9WriteRecord *out);

#ifdef __cplusplus
}
#endif

#endif
