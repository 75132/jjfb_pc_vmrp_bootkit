/* Plain-build stubs when launcher_core is not linked.
 * Dual-target build: plain links this object; gwy never links it.
 * Do not rely on ELF-style weak symbols (unreliable on MinGW PE). */
#include "./header/gwy_vm_file_abi.h"

int gwy_vm_file_is_bound(void) { return 0; }
int32_t gwy_vm_file_open(const char *guest_path, uint32_t flags) {
    (void)guest_path; (void)flags; return 0;
}
int32_t gwy_vm_file_close(int32_t handle) { (void)handle; return -1; }
int32_t gwy_vm_file_read(int32_t handle, void *buf, uint32_t len) {
    (void)handle; (void)buf; (void)len; return -1;
}
int32_t gwy_vm_file_write(int32_t handle, void *buf, uint32_t len) {
    (void)handle; (void)buf; (void)len; return -1;
}
int32_t gwy_vm_file_seek(int32_t handle, int32_t pos, int method) {
    (void)handle; (void)pos; (void)method; return -1;
}
int32_t gwy_vm_file_get_len(const char *guest_path) {
    (void)guest_path; return -1;
}
int32_t gwy_vm_file_info(const char *guest_path) {
    (void)guest_path; return 8; /* MR_IS_INVALID */
}
int gwy_vmrp_prepare_guest_vfs(void) { return -1; }
