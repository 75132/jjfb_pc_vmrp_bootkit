/* Thin ABI decls for upstream fileLib — no VFS strategy types here. */
#ifndef GWY_VM_FILE_ABI_H
#define GWY_VM_FILE_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int gwy_vm_file_is_bound(void);
int32_t gwy_vm_file_open(const char *guest_path, uint32_t flags);
int32_t gwy_vm_file_close(int32_t handle);
int32_t gwy_vm_file_read(int32_t handle, void *buf, uint32_t len);
int32_t gwy_vm_file_write(int32_t handle, void *buf, uint32_t len);
int32_t gwy_vm_file_seek(int32_t handle, int32_t pos, int method);
int32_t gwy_vm_file_get_len(const char *guest_path);
int32_t gwy_vm_file_info(const char *guest_path);

/* Optional bootstrap provided by launcher_core when GWY_USE_VM_FILE_SERVICE. */
int gwy_vmrp_prepare_guest_vfs(void);

#ifdef __cplusplus
}
#endif

#endif
