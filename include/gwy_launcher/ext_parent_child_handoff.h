#ifndef GWY_LAUNCHER_EXT_PARENT_CHILD_HANDOFF_H
#define GWY_LAUNCHER_EXT_PARENT_CHILD_HANDOFF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P19: observe-only Parent→Child launch handoff.
 * Captures startGame/runapp call site, child start_dsm return, and first post-child action.
 * Does NOT nest uc_emu_start inside UC_HOOK_CODE; does NOT activate 0x10140 on product path.
 */

typedef struct ShellChildLaunchFrame {
    uint32_t parent_pc;
    uint32_t parent_continuation;
    uint32_t parent_sp;
    uint32_t parent_lr;
    uint32_t parent_cpsr;
    uint32_t parent_r9;
    uint32_t parent_regs[13];
    uint32_t child_entry;
    uint32_t child_er_rw;
    uint64_t parent_module_id;
    uint64_t child_module_id;
    char parent_module[64];
    char parent_package[96];
    char child_package[96];
    char launch_api[32]; /* lib.startGame | lib.runapp | start_dsm | unknown */
    char call_kind[24];  /* BL | BLX | BX | INDIRECT | STRING_OBSERVE | HOST_DSM */
    int valid;
} ShellChildLaunchFrame;

void ext_parent_child_handoff_reset(void);
int ext_parent_child_handoff_enabled(void);
void ext_parent_child_handoff_bind_uc(void *uc);
void ext_parent_child_handoff_set_out_dir(const char *dir);

/* Boundary 1: parent launches child (Guest-observed preferred). */
void ext_parent_child_handoff_on_parent_launch(void *uc, const char *api_name, uint32_t call_pc,
                                              const uint32_t regs[16], uint32_t cpsr,
                                              const char *module_name, uint64_t module_id,
                                              const char *call_kind);

/* Boundary 2: child start_dsm enter/return. */
void ext_parent_child_handoff_on_start_dsm(const char *filename, const char *entry);
void ext_parent_child_handoff_on_child_init_return(void *uc, const char *filename, int32_t ret);

/* Boundary 3: first real successor after child return. */
void ext_parent_child_handoff_on_guest_code(void *uc, uint32_t pc, const uint32_t regs[16],
                                           uint32_t cpsr, const char *module_name,
                                           uint64_t module_id);
void ext_parent_child_handoff_on_plat(void *uc, uint32_t code, uint32_t app, uint32_t arg2,
                                     uint32_t arg3, uint32_t ret, uint32_t pc, uint32_t lr);
void ext_parent_child_handoff_on_host_loop_tick(uint32_t t_ms);

void ext_parent_child_handoff_flush(void);
const ShellChildLaunchFrame *ext_parent_child_handoff_frame(void);
int ext_parent_child_handoff_child_returned(void);
int ext_parent_child_handoff_first_action_seen(void);

#ifdef __cplusplus
}
#endif

#endif
