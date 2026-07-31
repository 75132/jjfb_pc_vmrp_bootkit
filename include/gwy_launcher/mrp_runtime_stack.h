#ifndef GWY_LAUNCHER_MRP_RUNTIME_STACK_H
#define GWY_LAUNCHER_MRP_RUNTIME_STACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Nested MRP runtime frames for Original GWY headless bootstrap.
 * Platform APIs may be shared; R9 / ER_RW / module generation / callbacks
 * are tracked per frame so parent state is not destroyed when jjfb starts.
 */

#define MRP_RUNTIME_STACK_MAX 4

typedef enum MrpRuntimeFrameRole {
    MRP_FRAME_NONE = 0,
    MRP_FRAME_PARENT_GWY = 1,
    MRP_FRAME_PARENT_GBRWCORE = 2,
    MRP_FRAME_PARENT_GBRWSHELL = 3,
    MRP_FRAME_PARENT_GAMELIST = 4,
    MRP_FRAME_CHILD_JJFB = 5,
    MRP_FRAME_OTHER = 6
} MrpRuntimeFrameRole;

typedef struct MrpRuntimeFrame {
    MrpRuntimeFrameRole role;
    uint32_t frame_id;
    uint32_t parent_frame_id;
    char package[96];
    char primary_ext[64];
    uint64_t module_id;
    uint32_t helper_pc;
    uint32_t p_guest;
    uint32_t chunk_guest;
    uint32_t r9;
    uint32_t erw;
    uint32_t module_generation;
    uint32_t callback_registry;
    uint32_t transport_provider;
    uint32_t provider_101ab;
    uint32_t timer_owner;
    int active;
} MrpRuntimeFrame;

typedef struct MrpRuntimeStack {
    MrpRuntimeFrame frames[MRP_RUNTIME_STACK_MAX];
    int depth;
    int nested_jjfb_intercepted;
    char last_nested_target[96];
    char last_nested_param[192];
} MrpRuntimeStack;

void mrp_runtime_stack_reset(MrpRuntimeStack *st);
MrpRuntimeStack *mrp_runtime_stack_global(void);

MrpRuntimeFrameRole mrp_runtime_stack_role_for_package(const char *package);

int mrp_runtime_stack_push(MrpRuntimeStack *st, const char *package, const char *primary_ext,
                           uint32_t r9, uint32_t erw, uint32_t module_generation);
int mrp_runtime_stack_bind_top(MrpRuntimeStack *st, uint64_t module_id, uint32_t helper_pc,
                               uint32_t p_guest, uint32_t chunk_guest);
int mrp_runtime_stack_update_top(MrpRuntimeStack *st, uint32_t r9, uint32_t erw,
                                 uint32_t callback_registry, uint32_t transport_provider,
                                 uint32_t provider_101ab, uint32_t timer_owner);
MrpRuntimeFrame *mrp_runtime_stack_top(MrpRuntimeStack *st);
MrpRuntimeFrame *mrp_runtime_stack_parent(MrpRuntimeStack *st);
int mrp_runtime_stack_note_nested_jjfb(MrpRuntimeStack *st, const char *target,
                                       const char *param);

/* Write reports/ORIGINAL_GWY_RUNTIME_STACK.json (or path). Returns 0 on success. */
int mrp_runtime_stack_write_json(const MrpRuntimeStack *st, const char *path);

#ifdef __cplusplus
}
#endif

#endif
