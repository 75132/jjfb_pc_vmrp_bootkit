#include "gwy_launcher/vm_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    VmRuntimeOptions opt;
    VmRuntime *rt = NULL;
    VmRuntime *rt2 = NULL;
    LauncherError err;
    GuestVfs vfs;
    VmFileService *svc = NULL;
    const char *root = getenv("GWY_FIXTURE_ROOT");
    char overlay[512];

    memset(&opt, 0, sizeof(opt));
    opt.mem_base = GWY_VM_DEFAULT_MEM_BASE;
    opt.mem_size = 64u * 1024u;

    if (vm_runtime_create(&opt, &rt, &err) != L_OK || !rt) {
        fprintf(stderr, "create failed: %s\n", err.message);
        return 1;
    }
    if (vm_runtime_state(rt) != VM_RT_CREATED) return 1;
    if (vm_runtime_start(rt, &err) != L_OK) {
        fprintf(stderr, "start failed: %s\n", err.message);
        vm_runtime_destroy(rt);
        return 1;
    }
    if (!vm_runtime_is_running(rt) || vm_runtime_state(rt) != VM_RT_STARTED) return 1;
    if (vm_runtime_call_depth(rt) != 0) return 1;
    if (vm_runtime_start(rt, &err) == L_OK) {
        fprintf(stderr, "double start should fail\n");
        vm_runtime_destroy(rt);
        return 1;
    }
    if (vm_runtime_stop(rt, &err) != L_OK) {
        fprintf(stderr, "stop failed: %s\n", err.message);
        vm_runtime_destroy(rt);
        return 1;
    }
    if (vm_runtime_is_running(rt) || vm_runtime_state(rt) != VM_RT_STOPPED) return 1;
    if (vm_runtime_stop(rt, &err) != L_OK) return 1;
    vm_runtime_destroy(rt);
    vm_runtime_destroy(NULL);

    if (vm_runtime_create(&opt, &rt, &err) != L_OK) return 1;
    vm_runtime_destroy(rt);

    /* file_service bind/unbind with runtime owner */
    if (root && root[0]) {
#ifdef _WIN32
        snprintf(overlay, sizeof(overlay), "%s\\..\\overlay_rt_test", root);
#else
        snprintf(overlay, sizeof(overlay), "/tmp/gwy_rt_overlay");
#endif
        if (gwy_vm_file_bootstrap(root, overlay, &vfs, &svc, &err) != L_OK) {
            fprintf(stderr, "bootstrap: %s\n", err.message);
            return 1;
        }
        gwy_vm_file_unbind(); /* bootstrap bound owner=1; clear for runtime bind */
        memset(&opt, 0, sizeof(opt));
        opt.mem_base = GWY_VM_DEFAULT_MEM_BASE;
        opt.mem_size = 64u * 1024u;
        opt.file_service = svc;
        opt.own_file_service = 1;
        opt.vfs = &vfs;
        if (vm_runtime_create(&opt, &rt, &err) != L_OK) return 1;
        if (vm_runtime_start(rt, &err) != L_OK) {
            fprintf(stderr, "start+bind failed: %s\n", err.message);
            vm_runtime_destroy(rt);
            return 1;
        }
        if (!gwy_vm_file_is_bound()) {
            fprintf(stderr, "expected bound after start\n");
            vm_runtime_destroy(rt);
            return 1;
        }
        memset(&opt, 0, sizeof(opt));
        opt.mem_base = GWY_VM_DEFAULT_MEM_BASE;
        opt.mem_size = 64u * 1024u;
        opt.file_service = svc; /* same svc; second runtime must refuse bind */
        if (vm_runtime_create(&opt, &rt2, &err) != L_OK) return 1;
        if (vm_runtime_start(rt2, &err) == L_OK) {
            fprintf(stderr, "second runtime bind should fail\n");
            vm_runtime_destroy(rt2);
            vm_runtime_destroy(rt);
            return 1;
        }
        vm_runtime_destroy(rt2);
        if (vm_runtime_stop(rt, &err) != L_OK) return 1;
        if (gwy_vm_file_is_bound()) {
            fprintf(stderr, "should unbind on stop\n");
            vm_runtime_destroy(rt);
            return 1;
        }
        vm_runtime_destroy(rt);
    }

    puts("test_vm_runtime OK");
    return 0;
}
