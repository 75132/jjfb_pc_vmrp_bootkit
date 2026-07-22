#ifndef GWY_LAUNCHER_EXT_LIFECYCLE_H
#define GWY_LAUNCHER_EXT_LIFECYCLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product EXT lifecycle (P2). Orthogonal to GwyModuleState.
 * bootstrap_return and init_return are never inferred from each other.
 */

typedef enum GwyExtLifecycleState {
    EXT_LIFECYCLE_DISCOVERED = 1,
    EXT_LIFECYCLE_MAPPED = 2,
    EXT_LIFECYCLE_BOOTSTRAP_ENTERED = 3,
    EXT_LIFECYCLE_BOOTSTRAP_RETURNED = 4,
    EXT_LIFECYCLE_ABI_READY = 5,
    EXT_LIFECYCLE_VERSION_OK = 6,
    EXT_LIFECYCLE_APPINFO_OK = 7,
    EXT_LIFECYCLE_INIT_OK = 8,
    EXT_LIFECYCLE_RUNNING = 9,
    EXT_LIFECYCLE_FAILED = 10
} GwyExtLifecycleState;

typedef struct GwyExtLifecycleRecord {
    uint64_t module_id;
    uint64_t module_generation;
    char module_name[64];
    GwyExtLifecycleState state;
    int32_t bootstrap_return;
    int bootstrap_return_valid;
    int32_t init_return;
    int init_return_valid;
    int32_t version_return;
    int version_return_valid;
    int32_t appinfo_return;
    int appinfo_return_valid;
} GwyExtLifecycleRecord;

void ext_lifecycle_reset(void);
void ext_lifecycle_set_run_id(const char *run_id);
const char *ext_lifecycle_run_id(void);

const char *ext_lifecycle_state_name(GwyExtLifecycleState s);

GwyExtLifecycleRecord *ext_lifecycle_ensure(uint64_t module_id, uint64_t generation,
                                            const char *module_name);
const GwyExtLifecycleRecord *ext_lifecycle_find(uint64_t module_id);
const GwyExtLifecycleRecord *ext_lifecycle_find_by_name(const char *module_name);

void ext_lifecycle_set_state(uint64_t module_id, GwyExtLifecycleState state);

void ext_lifecycle_note_bootstrap_enter(uint64_t module_id, uint64_t generation,
                                        const char *module_name, uint32_t pc);
void ext_lifecycle_note_bootstrap_return(uint64_t module_id, int32_t bootstrap_return);
void ext_lifecycle_note_abi_ready(uint64_t module_id);
void ext_lifecycle_note_version(uint64_t module_id, int32_t ret);
void ext_lifecycle_note_appinfo(uint64_t module_id, int32_t ret);
void ext_lifecycle_note_init(uint64_t module_id, int32_t ret);
void ext_lifecycle_note_failed(uint64_t module_id);

#ifdef __cplusplus
}
#endif

#endif
