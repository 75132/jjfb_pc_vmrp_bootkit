#include "gwy_launcher/vm_runtime.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unicorn/unicorn.h>

struct VmRuntime {
    uc_engine *uc;
    uint8_t *host_mem;
    uint32_t mem_base;
    uint32_t mem_size;
    VmRuntimeState state;
    uint32_t call_depth;
    GuestVfs *vfs;
    VmFileService *file_service;
    int own_file_service;
    const LaunchDescriptor *launch;
    VmMemoryRegion regions[GWY_VM_REGION_MAX];
    size_t region_count;
    uint64_t runtime_id;
};

static uint64_t runtime_owner_id(const VmRuntime *rt) {
    return rt ? (uint64_t)(uintptr_t)rt : 0;
}

LauncherStatus vm_runtime_create(const VmRuntimeOptions *opt,
                                 VmRuntime **out,
                                 LauncherError *err) {
    VmRuntime *rt;
    launcher_error_clear(err);
    if (!opt || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "vm_runtime", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out = NULL;
    rt = (VmRuntime *)calloc(1, sizeof(*rt));
    if (!rt) {
        launcher_error_set(err, L_ERR_IO, "vm_runtime", "oom", NULL);
        return L_ERR_IO;
    }
    rt->vfs = opt->vfs;
    rt->launch = opt->launch;
    rt->file_service = opt->file_service;
    rt->own_file_service = opt->own_file_service;
    rt->mem_base = opt->mem_base ? opt->mem_base : GWY_VM_DEFAULT_MEM_BASE;
    rt->mem_size = opt->mem_size ? opt->mem_size : GWY_VM_DEFAULT_MEM_SIZE;
    rt->runtime_id = runtime_owner_id(rt);
    if (rt->mem_size == 0 || (uint64_t)rt->mem_base + (uint64_t)rt->mem_size > 0xFFFFFFFFull) {
        free(rt);
        launcher_error_set(err, L_ERR_BOUNDS, "vm_runtime", "invalid mem range", NULL);
        return L_ERR_BOUNDS;
    }
    rt->state = VM_RT_CREATED;
    *out = rt;
    return L_OK;
}

LauncherStatus vm_runtime_start(VmRuntime *rt, LauncherError *err) {
    uc_err uerr;
    launcher_error_clear(err);
    if (!rt) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "vm_runtime", "null runtime", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (rt->state == VM_RT_DESTROYED) {
        launcher_error_set(err, L_ERR_STATE, "vm_runtime", "destroyed", NULL);
        return L_ERR_STATE;
    }
    if (rt->state == VM_RT_STARTED) {
        launcher_error_set(err, L_ERR_STATE, "vm_runtime", "already started", NULL);
        return L_ERR_STATE;
    }

    if (rt->file_service) {
        if (gwy_vm_file_bind_owned(rt->file_service, rt->runtime_id) != 0) {
            launcher_error_set(err, L_ERR_STATE, "vm_runtime", "file_service bind refused", NULL);
            return L_ERR_STATE;
        }
    }

    uerr = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &rt->uc);
    if (uerr != UC_ERR_OK) {
        if (rt->file_service) gwy_vm_file_unbind_owned(rt->runtime_id);
        launcher_error_set(err, L_ERR_IO, "vm_runtime", "uc_open failed", uc_strerror(uerr));
        rt->uc = NULL;
        return L_ERR_IO;
    }

    rt->host_mem = (uint8_t *)calloc(1, rt->mem_size);
    if (!rt->host_mem) {
        if (rt->file_service) gwy_vm_file_unbind_owned(rt->runtime_id);
        uc_close(rt->uc);
        rt->uc = NULL;
        launcher_error_set(err, L_ERR_IO, "vm_runtime", "host_mem oom", NULL);
        return L_ERR_IO;
    }

    uerr = uc_mem_map_ptr(rt->uc, rt->mem_base, rt->mem_size, UC_PROT_ALL, rt->host_mem);
    if (uerr != UC_ERR_OK) {
        if (rt->file_service) gwy_vm_file_unbind_owned(rt->runtime_id);
        free(rt->host_mem);
        rt->host_mem = NULL;
        uc_close(rt->uc);
        rt->uc = NULL;
        launcher_error_set(err, L_ERR_IO, "vm_runtime", "uc_mem_map_ptr failed", uc_strerror(uerr));
        return L_ERR_IO;
    }

    memset(rt->regions, 0, sizeof(rt->regions));
    rt->regions[0].used = 1;
    rt->regions[0].base = rt->mem_base;
    rt->regions[0].size = rt->mem_size;
    rt->regions[0].permissions = 0x7; /* rwx */
    rt->regions[0].kind = VM_MEM_REGION_RAM;
    rt->region_count = 1;

    rt->call_depth = 0;
    rt->state = VM_RT_STARTED;
    return L_OK;
}

LauncherStatus vm_runtime_stop(VmRuntime *rt, LauncherError *err) {
    launcher_error_clear(err);
    if (!rt) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "vm_runtime", "null runtime", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (rt->state == VM_RT_DESTROYED) {
        launcher_error_set(err, L_ERR_STATE, "vm_runtime", "destroyed", NULL);
        return L_ERR_STATE;
    }
    if (rt->file_service && rt->state == VM_RT_STARTED) {
        gwy_vm_file_unbind_owned(rt->runtime_id);
    }
    if (rt->state == VM_RT_STOPPED || rt->state == VM_RT_CREATED) {
        /* Idempotent stop from CREATED/STOPPED. */
        if (rt->uc) {
            uc_close(rt->uc);
            rt->uc = NULL;
        }
        free(rt->host_mem);
        rt->host_mem = NULL;
        rt->state = VM_RT_STOPPED;
        return L_OK;
    }

    if (rt->uc) {
        uc_close(rt->uc);
        rt->uc = NULL;
    }
    free(rt->host_mem);
    rt->host_mem = NULL;
    rt->call_depth = 0;
    rt->region_count = 0;
    memset(rt->regions, 0, sizeof(rt->regions));
    rt->state = VM_RT_STOPPED;
    return L_OK;
}

void vm_runtime_destroy(VmRuntime *rt) {
    LauncherError err;
    if (!rt) return;
    if (rt->state == VM_RT_DESTROYED) {
        free(rt);
        return;
    }
    if (rt->file_service && rt->state == VM_RT_STARTED) {
        gwy_vm_file_unbind_owned(rt->runtime_id);
    }
    vm_runtime_stop(rt, &err);
    if (rt->file_service) {
        if (rt->own_file_service) {
            vm_file_service_destroy(rt->file_service);
        }
        rt->file_service = NULL;
    }
    rt->state = VM_RT_DESTROYED;
    free(rt);
}

int vm_runtime_is_running(const VmRuntime *rt) {
    return rt && rt->state == VM_RT_STARTED;
}

VmRuntimeState vm_runtime_state(const VmRuntime *rt) {
    return rt ? rt->state : VM_RT_DESTROYED;
}

uint32_t vm_runtime_call_depth(const VmRuntime *rt) {
    return rt ? rt->call_depth : 0;
}

uint32_t vm_runtime_mem_base(const VmRuntime *rt) {
    return rt ? rt->mem_base : 0;
}

uint32_t vm_runtime_mem_size(const VmRuntime *rt) {
    return rt ? rt->mem_size : 0;
}

GuestVfs *vm_runtime_vfs(VmRuntime *rt) {
    return rt ? rt->vfs : NULL;
}

VmFileService *vm_runtime_file_service(VmRuntime *rt) {
    return rt ? rt->file_service : NULL;
}

const VmMemoryRegion *vm_runtime_regions(const VmRuntime *rt, size_t *out_count) {
    if (out_count) *out_count = rt ? rt->region_count : 0;
    return rt ? rt->regions : NULL;
}

struct uc_struct *vm_runtime_uc(VmRuntime *rt) {
    return rt ? rt->uc : NULL;
}
