#ifndef GWY_LAUNCHER_VM_FILE_SERVICE_H
#define GWY_LAUNCHER_VM_FILE_SERVICE_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/guest_vfs.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mythroad file flags (DOCUMENTED / CROSS_TARGET) — mirrored for clean core. */
#define GWY_MR_FILE_RDONLY 1
#define GWY_MR_FILE_WRONLY 2
#define GWY_MR_FILE_RDWR 4
#define GWY_MR_FILE_CREATE 8

#define GWY_MR_IS_FILE 1
#define GWY_MR_IS_DIR 2
#define GWY_MR_IS_INVALID 8

#define GWY_MR_SUCCESS 0
#define GWY_MR_FAILED (-1)

#define GWY_MR_SEEK_SET 0
#define GWY_MR_SEEK_CUR 1
#define GWY_MR_SEEK_END 2

#define GWY_VM_FILE_HANDLE_MAX 64

typedef struct VmFileService VmFileService;

LauncherStatus vm_file_service_create(GuestVfs *vfs, VmFileService **out, LauncherError *err);
void vm_file_service_destroy(VmFileService *svc); /* closes all handles; safe on NULL */

/* Mythroad-compatible return conventions: open→0 fail, else handle>=1; close/seek→0/-1; read/write→count or -1 */
int32_t vm_file_service_open(VmFileService *svc, const char *guest_path, uint32_t flags);
int32_t vm_file_service_close(VmFileService *svc, int32_t handle);
int32_t vm_file_service_read(VmFileService *svc, int32_t handle, void *buf, uint32_t len);
int32_t vm_file_service_write(VmFileService *svc, int32_t handle, const void *buf, uint32_t len);
int32_t vm_file_service_seek(VmFileService *svc, int32_t handle, int32_t pos, int method);
int32_t vm_file_service_get_len(VmFileService *svc, const char *guest_path);
int32_t vm_file_service_info(VmFileService *svc, const char *guest_path);

GuestVfs *vm_file_service_vfs(VmFileService *svc);
int vm_file_service_open_count(const VmFileService *svc);

/*
 * Process-local bind for upstream fileLib thin hook.
 * No GuestVFS strategy lives in fileLib — only these ABI calls.
 *
 * Owned bind: only the owning runtime_id may rebind/unbind.
 * runtime_id must be non-zero. After unbind, ABI calls fail closed.
 */
void gwy_vm_file_bind(VmFileService *svc); /* bootstrap helper: owner=1 */
int gwy_vm_file_bind_owned(VmFileService *svc, uint64_t runtime_id); /* 0 ok, -1 refuse */
void gwy_vm_file_unbind(void); /* force clear (bootstrap only) */
void gwy_vm_file_unbind_owned(uint64_t runtime_id);
int gwy_vm_file_is_bound(void);
uint64_t gwy_vm_file_bound_owner(void);
VmFileService *gwy_vm_file_bound_service(void);

/* Bound ABI used by fileLib (host buffers; bridge already translated guest pointers). */
int32_t gwy_vm_file_open(const char *guest_path, uint32_t flags);
int32_t gwy_vm_file_close(int32_t handle);
int32_t gwy_vm_file_read(int32_t handle, void *buf, uint32_t len);
int32_t gwy_vm_file_write(int32_t handle, void *buf, uint32_t len);
int32_t gwy_vm_file_seek(int32_t handle, int32_t pos, int method);
int32_t gwy_vm_file_get_len(const char *guest_path);
int32_t gwy_vm_file_info(const char *guest_path);

/*
 * Bootstrap: init overlay+generated sdk_key (NO cwd/mythroad/sdk_key.dat),
 * create service, bind. overlay_root required.
 */
LauncherStatus gwy_vm_file_bootstrap(const char *resource_root,
                                     const char *overlay_root,
                                     GuestVfs *vfs_storage,
                                     VmFileService **out_svc,
                                     LauncherError *err);

#ifdef __cplusplus
}
#endif

#endif
