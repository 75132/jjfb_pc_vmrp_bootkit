#ifndef GWY_LAUNCHER_GUEST_VFS_H
#define GWY_LAUNCHER_GUEST_VFS_H

#include "gwy_launcher/error.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define VFS_PATH_MAX 1024
#define VFS_GENERATED_MAX 16
#define VFS_MISS_MAX 64
#define VFS_HANDLE_MAX 32

typedef enum VfsBackend {
    VFS_CANONICAL_READONLY = 1,
    VFS_OVERLAY_WRITABLE = 2,
    VFS_GENERATED = 3
} VfsBackend;

typedef enum VfsOpenMode {
    VFS_OPEN_READ = 1,
    VFS_OPEN_WRITE = 2
} VfsOpenMode;

typedef enum VfsMissKind {
    VFS_MISS_PROBE = 1,      /* candidate miss; may be recovered by resolver */
    VFS_MISS_UNRESOLVED = 2  /* final unresolved open */
} VfsMissKind;

typedef struct VfsResolution {
    char guest_original[VFS_PATH_MAX];   /* requested */
    char guest_normalized[VFS_PATH_MAX]; /* slash-normalized, still may have mythroad/ */
    char guest_canonical[VFS_PATH_MAX];  /* mythroad/ stripped unique form */
    char host_path[VFS_PATH_MAX];
    VfsBackend backend;
    char rule[64];
    int exists;
} VfsResolution;

typedef struct VfsGeneratedNode {
    char guest_normalized[VFS_PATH_MAX];
    char host_path[VFS_PATH_MAX];
    char sha256_hex[65];
    int used;
} VfsGeneratedNode;

typedef struct VfsMissEntry {
    char guest_normalized[VFS_PATH_MAX];
    char guest_canonical[VFS_PATH_MAX];
    VfsMissKind kind;
    uint32_t count;
    int used;
} VfsMissEntry;

typedef struct VfsFile {
    FILE *fp;
    VfsOpenMode mode;
    VfsBackend backend;
    char guest_normalized[VFS_PATH_MAX];
    char host_path[VFS_PATH_MAX];
    int used;
} VfsFile;

typedef struct GuestVfs {
    char resource_root[VFS_PATH_MAX];
    char overlay_root[VFS_PATH_MAX];
    int has_overlay;
    VfsGeneratedNode generated[VFS_GENERATED_MAX];
    size_t generated_count;
    VfsMissEntry misses[VFS_MISS_MAX];
    size_t miss_unique;
    uint32_t miss_total;
    VfsFile files[VFS_HANDLE_MAX];
} GuestVfs;

typedef int VfsHandle; /* 1..VFS_HANDLE_MAX, 0 invalid */

LauncherStatus guest_vfs_init(GuestVfs *vfs,
                              const char *resource_root,
                              const char *overlay_root,
                              LauncherError *err);

void guest_vfs_reset_misses(GuestVfs *vfs);

/*
 * Register a platform-generated immutable file.
 * Alias forms "sdk_key.dat" and "mythroad/sdk_key.dat" share one host.
 * Writes to generated paths are rejected (generated stays highest priority on read).
 */
LauncherStatus guest_vfs_set_generated(GuestVfs *vfs,
                                       const char *guest_path,
                                       const char *host_path,
                                       const char *sha256_hex,
                                       LauncherError *err);

LauncherStatus guest_path_canonicalize(const char *guest_path,
                                       char *out_normalized,
                                       size_t out_cap,
                                       LauncherError *err);

/* Strip mythroad/ prefix to unique canonical guest form. */
void guest_path_to_canonical(const char *normalized, char *out_canonical, size_t out_cap);

/*
 * Resolve order: generated → overlay (write if not generated / read if exists) → canonical.
 * Generated paths are immutable: write returns L_ERR_UNSUPPORTED.
 */
LauncherStatus guest_vfs_resolve(GuestVfs *vfs,
                                 const char *guest_path,
                                 VfsOpenMode mode,
                                 VfsResolution *out,
                                 LauncherError *err);

LauncherStatus guest_vfs_open(GuestVfs *vfs,
                              const char *guest_path,
                              VfsOpenMode mode,
                              VfsHandle *out_handle,
                              VfsResolution *out_res,
                              LauncherError *err);

LauncherStatus guest_vfs_read(GuestVfs *vfs,
                              VfsHandle handle,
                              void *buf,
                              size_t len,
                              size_t *out_read,
                              LauncherError *err);

LauncherStatus guest_vfs_write(GuestVfs *vfs,
                               VfsHandle handle,
                               const void *buf,
                               size_t len,
                               size_t *out_written,
                               LauncherError *err);

LauncherStatus guest_vfs_close(GuestVfs *vfs, VfsHandle handle, LauncherError *err);

LauncherStatus guest_vfs_read_all(GuestVfs *vfs,
                                  const char *guest_path,
                                  uint8_t **out_data,
                                  size_t *out_size,
                                  LauncherError *err);

LauncherStatus guest_vfs_write_all(GuestVfs *vfs,
                                   const char *guest_path,
                                   const void *data,
                                   size_t size,
                                   LauncherError *err);

void guest_vfs_trace_open(const VfsResolution *res, int ok);
void guest_vfs_trace_miss(const GuestVfs *vfs, const char *guest_normalized, VfsMissKind kind);

size_t guest_vfs_miss_summary(const GuestVfs *vfs);

const char *vfs_backend_name(VfsBackend b);
const char *vfs_miss_kind_name(VfsMissKind k);

#endif
