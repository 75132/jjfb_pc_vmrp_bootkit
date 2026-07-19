#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/vm_runtime.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    VmRuntimeOptions opt;
    VmRuntime *rt = NULL;
    LauncherError err;
    uint32_t base;
    char buf[32];
    uint8_t bytes[8];
    const char *hello = "hello";

    memset(&opt, 0, sizeof(opt));
    opt.mem_base = GWY_VM_DEFAULT_MEM_BASE;
    opt.mem_size = 64u * 1024u;

    if (vm_runtime_create(&opt, &rt, &err) != L_OK ||
        vm_runtime_start(rt, &err) != L_OK) {
        fprintf(stderr, "setup failed: %s\n", err.message);
        vm_runtime_destroy(rt);
        return 1;
    }
    base = vm_runtime_mem_base(rt);

    if (guest_memory_write(rt, base, hello, 6, &err) != L_OK) {
        fprintf(stderr, "write failed: %s\n", err.message);
        vm_runtime_destroy(rt);
        return 1;
    }
    memset(buf, 0xcc, sizeof(buf));
    if (guest_memory_read(rt, base, buf, 6, &err) != L_OK || memcmp(buf, hello, 6) != 0) {
        fprintf(stderr, "read mismatch\n");
        vm_runtime_destroy(rt);
        return 1;
    }
    memset(buf, 0, sizeof(buf));
    if (guest_memory_read_cstring(rt, base, buf, sizeof(buf), &err) != L_OK ||
        strcmp(buf, "hello") != 0) {
        fprintf(stderr, "cstring failed got='%s'\n", buf);
        vm_runtime_destroy(rt);
        return 1;
    }

    /* OOB below base */
    if (guest_memory_read(rt, base - 4, bytes, 4, &err) == L_OK) {
        fprintf(stderr, "expected OOB below\n");
        vm_runtime_destroy(rt);
        return 1;
    }
    /* OOB past end */
    if (guest_memory_write(rt, base + opt.mem_size - 2, bytes, 8, &err) == L_OK) {
        fprintf(stderr, "expected OOB past end\n");
        vm_runtime_destroy(rt);
        return 1;
    }
    /* Overflow */
    if (guest_memory_validate_range(rt, 0xFFFFFFF0u, 32, GWY_MEM_READ, &err) == L_OK) {
        fprintf(stderr, "expected overflow reject\n");
        vm_runtime_destroy(rt);
        return 1;
    }

    /* Truncated cstring buffer */
    if (guest_memory_read_cstring(rt, base, buf, 3, &err) != L_ERR_BOUNDS) {
        fprintf(stderr, "expected truncated cstring\n");
        vm_runtime_destroy(rt);
        return 1;
    }
    if (strcmp(buf, "he") != 0) {
        fprintf(stderr, "truncate content bad '%s'\n", buf);
        vm_runtime_destroy(rt);
        return 1;
    }

    vm_runtime_destroy(rt);
    puts("test_guest_memory OK");
    return 0;
}
