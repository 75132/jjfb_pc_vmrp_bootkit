#ifndef GWY_LAUNCHER_EXT_GWY_SHELL_NATIVE_EXEC_H
#define GWY_LAUNCHER_EXT_GWY_SHELL_NATIVE_EXEC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Phase 6H: prove guest-native GWY shell MRP/EXT execution.
 * Observe-only; does not invent P+0xC / R9 / UI state.
 */

typedef enum GwyShellNativeExecClass {
    GWY_SHELL_NATIVE_NONE = 0,
    GWY_SHELL_NATIVE_PACKAGE_OPEN_ONLY = 1,
    GWY_SHELL_NATIVE_EXEC_PARTIAL = 2,
    GWY_SHELL_NATIVE_EXEC_GATE_OPEN = 3,
    GWY_SHELL_NATIVE_GUEST_RUNAPP_CALLED = 4
} GwyShellNativeExecClass;

void ext_gwy_shell_native_exec_reset(void);
int ext_gwy_shell_native_exec_enabled(void);
void ext_gwy_shell_native_exec_bind_uc(void *uc);

void ext_gwy_shell_native_exec_on_start_dsm(const char *filename, const char *ext,
                                           const char *entry);
/* D5b: guest VA of start_dsm entry string (mr heap), observe-only. */
void ext_gwy_shell_native_exec_on_launch_param(uint32_t entry_va, const char *entry);
/* D5b: sendAppEvent slot — register 0x10102 handlers / observe. */
void ext_gwy_shell_native_exec_on_slot28(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2,
                                         uint32_t r3, uint32_t ret);
void ext_gwy_shell_native_exec_on_file_open(const char *guest_path, const char *host_path, int ok);
void ext_gwy_shell_native_exec_on_member_open(const char *guest_path);
void ext_gwy_shell_native_exec_on_code_image(uint32_t guest_addr, uint32_t size);
void ext_gwy_shell_native_exec_on_module_map(const char *module_name, uint32_t base, uint32_t size);
void ext_gwy_shell_native_exec_on_code(void *uc, uint64_t module_id, const char *module_name,
                                       uint32_t pc, const uint32_t regs[16]);
void ext_gwy_shell_native_exec_on_helper_call(uint32_t helper, uint32_t method, int32_t ret);
void ext_gwy_shell_native_exec_on_strcom(uint32_t code, const char *caller);
void ext_gwy_shell_native_exec_on_mrc_init(uint32_t pc, int32_t ret);
void ext_gwy_shell_native_exec_on_pxc_write(uint32_t old_v, uint32_t new_v, uint32_t pc,
                                            uint32_t lr, const char *module);
/* Observe-only: fault PC inside mapped shell EXT proves guest execution. */
void ext_gwy_shell_native_exec_on_mem_fault(void *uc, uint32_t fault_pc);

void ext_gwy_shell_native_exec_finalize(const char *stop_reason);
int ext_gwy_shell_native_exec_gate_open(void);
int ext_gwy_shell_native_exec_gbrwcore_started(void);
int ext_gwy_shell_native_exec_gamelist_started(void);
/* 1 if any guest PC was observed inside gbrwcore.ext mapping. */
int ext_gwy_shell_native_exec_gbrwcore_pc_hit(void);
GwyShellNativeExecClass ext_gwy_shell_native_exec_last_class(void);
const char *ext_gwy_shell_native_exec_class_name(GwyShellNativeExecClass c);

#ifdef __cplusplus
}
#endif

#endif
