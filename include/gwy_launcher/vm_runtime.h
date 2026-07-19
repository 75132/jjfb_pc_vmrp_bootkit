#ifndef GWY_LAUNCHER_VM_RUNTIME_H
#define GWY_LAUNCHER_VM_RUNTIME_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/guest_vfs.h"
#include "gwy_launcher/launch_descriptor.h"
#include "gwy_launcher/vm_file_service.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_VM_DEFAULT_MEM_BASE 0x80000u
#define GWY_VM_DEFAULT_MEM_SIZE (1024u * 1024u)
#define GWY_VM_REGION_MAX 8

typedef enum VmRuntimeState {
    VM_RT_CREATED = 1,
    VM_RT_STARTED = 2,
    VM_RT_STOPPED = 3,
    VM_RT_DESTROYED = 4
} VmRuntimeState;

typedef enum VmMemoryRegionKind {
    VM_MEM_REGION_RAM = 1
} VmMemoryRegionKind;

typedef struct VmMemoryRegion {
    uint32_t base;
    uint32_t size;
    uint32_t permissions; /* bit0=read bit1=write bit2=exec */
    VmMemoryRegionKind kind;
    int used;
} VmMemoryRegion;

typedef struct VmRuntime VmRuntime;

typedef struct VmRuntimeOptions {
    const LaunchDescriptor *launch;
    GuestVfs *vfs;
    VmFileService *file_service; /* optional; destroy closes if owned_flag set */
    int own_file_service;
    uint32_t mem_base;
    uint32_t mem_size;
} VmRuntimeOptions;

LauncherStatus vm_runtime_create(const VmRuntimeOptions *opt,
                                 VmRuntime **out,
                                 LauncherError *err);
LauncherStatus vm_runtime_start(VmRuntime *rt, LauncherError *err);
LauncherStatus vm_runtime_stop(VmRuntime *rt, LauncherError *err);
void vm_runtime_destroy(VmRuntime *rt);

int vm_runtime_is_running(const VmRuntime *rt);
VmRuntimeState vm_runtime_state(const VmRuntime *rt);
uint32_t vm_runtime_call_depth(const VmRuntime *rt);
uint32_t vm_runtime_mem_base(const VmRuntime *rt);
uint32_t vm_runtime_mem_size(const VmRuntime *rt);
GuestVfs *vm_runtime_vfs(VmRuntime *rt);
VmFileService *vm_runtime_file_service(VmRuntime *rt);
const VmMemoryRegion *vm_runtime_regions(const VmRuntime *rt, size_t *out_count);

struct uc_struct;
struct uc_struct *vm_runtime_uc(VmRuntime *rt);

#ifdef __cplusplus
}
#endif

#endif
