#include "gwy_launcher/vm_file_service.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/platform_identity.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

typedef struct VmFileHandleEntry {
    int in_use;
    uint32_t generation;
    FILE *fp;
    uint32_t flags;
    VfsBackend backend;
    char guest_normalized[VFS_PATH_MAX];
    char guest_canonical[VFS_PATH_MAX];
    char host_path[VFS_PATH_MAX];
} VmFileHandleEntry;

struct VmFileService {
    GuestVfs *vfs;
    VmFileHandleEntry table[GWY_VM_FILE_HANDLE_MAX];
    uint32_t next_generation;
};

static VmFileService *g_bound_svc = NULL;
static uint64_t g_bound_owner = 0;

#define GWY_VM_FILE_BOOTSTRAP_OWNER ((uint64_t)1)

static const char *mode_name(int want_write) {
    return want_write ? "write" : "read";
}

static void trace_line(const char *line) {
    fputs(line, stdout);
    fputs(line, stderr);
    fflush(stdout);
    fflush(stderr);
}

static void trace_open(uint64_t runtime,
                       const char *requested,
                       const VfsResolution *res,
                       const char *mode,
                       int hit,
                       int32_t handle) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "[VM_FILE_OPEN] runtime=%llu requested=\"%s\" normalized=\"%s\" canonical=\"%s\" "
             "backend=%s mode=%s result=%s handle=%d\n",
             (unsigned long long)runtime,
             requested ? requested : "",
             res && res->guest_normalized[0] ? res->guest_normalized : "",
             res && res->guest_canonical[0] ? res->guest_canonical : "",
             res ? vfs_backend_name(res->backend) : "none",
             mode ? mode : "?",
             hit ? "HIT" : "MISS",
             (int)handle);
    trace_line(buf);
}

static void trace_miss(const char *requested, const char *canonical, const char *kind) {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "[VM_FILE_MISS] requested=\"%s\" canonical=\"%s\" kind=%s\n",
             requested ? requested : "",
             canonical ? canonical : "",
             kind ? kind : "unresolved");
    trace_line(buf);
}

static void ensure_dir(const char *dir) {
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#else
    mkdir(dir, 0755);
#endif
}

static void mkdir_p_parent(const char *file_path) {
    char dir[VFS_PATH_MAX];
    size_t n = strlen(file_path);
    size_t i, start;
    if (n >= sizeof(dir) || n == 0) return;
    memcpy(dir, file_path, n + 1);
    for (i = n; i > 0; i--) {
        if (dir[i - 1] == '/' || dir[i - 1] == '\\') {
            dir[i - 1] = '\0';
            break;
        }
    }
    if (i == 0 || dir[0] == '\0') return;
    start = 0;
#ifdef _WIN32
    if (((dir[0] >= 'A' && dir[0] <= 'Z') || (dir[0] >= 'a' && dir[0] <= 'z')) && dir[1] == ':')
        start = 2;
    if (dir[start] == '/' || dir[start] == '\\') start++;
#endif
    for (i = start; dir[i]; i++) {
        if (dir[i] == '/' || dir[i] == '\\') {
            char c = dir[i];
            dir[i] = '\0';
            if (dir[0]) ensure_dir(dir);
            dir[i] = c;
        }
    }
    ensure_dir(dir);
}

static VmFileHandleEntry *entry_from_handle(VmFileService *svc, int32_t handle) {
    int idx;
    if (!svc || handle < 1 || handle > GWY_VM_FILE_HANDLE_MAX) return NULL;
    idx = handle - 1;
    if (!svc->table[idx].in_use) return NULL;
    return &svc->table[idx];
}

LauncherStatus vm_file_service_create(GuestVfs *vfs, VmFileService **out, LauncherError *err) {
    VmFileService *svc;
    launcher_error_clear(err);
    if (!vfs || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "vm_file", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out = NULL;
    svc = (VmFileService *)calloc(1, sizeof(*svc));
    if (!svc) {
        launcher_error_set(err, L_ERR_IO, "vm_file", "oom", NULL);
        return L_ERR_IO;
    }
    svc->vfs = vfs;
    svc->next_generation = 1;
    *out = svc;
    return L_OK;
}

void vm_file_service_destroy(VmFileService *svc) {
    int i;
    if (!svc) return;
    if (g_bound_svc == svc) {
        g_bound_svc = NULL;
        g_bound_owner = 0;
    }
    for (i = 0; i < GWY_VM_FILE_HANDLE_MAX; i++) {
        if (svc->table[i].in_use && svc->table[i].fp) {
            fclose(svc->table[i].fp);
        }
    }
    free(svc);
}

GuestVfs *vm_file_service_vfs(VmFileService *svc) {
    return svc ? svc->vfs : NULL;
}

int vm_file_service_open_count(const VmFileService *svc) {
    int i, n = 0;
    if (!svc) return 0;
    for (i = 0; i < GWY_VM_FILE_HANDLE_MAX; i++) {
        if (svc->table[i].in_use) n++;
    }
    return n;
}

int32_t vm_file_service_open(VmFileService *svc, const char *guest_path, uint32_t flags) {
    VfsResolution res;
    LauncherError err;
    LauncherStatus st;
    VfsOpenMode mode;
    FILE *fp = NULL;
    const char *fmode;
    int slot = -1;
    int i;
    int want_write;
    int want_read;
    int want_create;
    int32_t handle;
    uint64_t runtime = (svc == g_bound_svc) ? g_bound_owner : 0;

    memset(&res, 0, sizeof(res));
    if (!svc || !guest_path || !guest_path[0]) {
        trace_miss(guest_path, "", "unresolved");
        return 0;
    }

    want_write = (flags & (GWY_MR_FILE_WRONLY | GWY_MR_FILE_RDWR | GWY_MR_FILE_CREATE)) != 0;
    want_read = (flags & (GWY_MR_FILE_RDONLY | GWY_MR_FILE_RDWR)) != 0 || !want_write;
    want_create = (flags & GWY_MR_FILE_CREATE) != 0;

    mode = want_write ? VFS_OPEN_WRITE : VFS_OPEN_READ;
    st = guest_vfs_resolve(svc->vfs, guest_path, mode, &res, &err);
    if (st != L_OK) {
        guest_vfs_trace_miss(svc->vfs, res.guest_normalized[0] ? res.guest_normalized : guest_path,
                             VFS_MISS_UNRESOLVED);
        trace_miss(guest_path, res.guest_canonical, "unresolved");
        trace_open(runtime, guest_path, &res, mode_name(want_write), 0, 0);
        return 0;
    }

    /* Never fopen arbitrary cwd paths — host_path always from VFS resolve. */
    if (want_write) {
        if (res.backend != VFS_OVERLAY_WRITABLE) {
            printf("[JJFB_FILEOPEN_MISS] guest=\"%s\" reason=write_not_overlay backend=%s\n",
                   res.guest_normalized, vfs_backend_name(res.backend));
            trace_miss(guest_path, res.guest_canonical, "unresolved");
            trace_open(runtime, guest_path, &res, mode_name(want_write), 0, 0);
            return 0;
        }
        mkdir_p_parent(res.host_path);
        if ((flags & GWY_MR_FILE_RDWR) || (want_read && want_write)) {
            fmode = want_create ? "w+b" : "r+b";
        } else {
            fmode = "wb";
        }
        fp = fopen(res.host_path, fmode);
        if (!fp && !want_create && (flags & GWY_MR_FILE_RDWR)) {
            fp = fopen(res.host_path, "w+b");
        }
    } else {
        if (!res.exists) {
            guest_vfs_trace_miss(svc->vfs, res.guest_normalized, VFS_MISS_UNRESOLVED);
            trace_miss(guest_path, res.guest_canonical, "unresolved");
            trace_open(runtime, guest_path, &res, mode_name(want_write), 0, 0);
            return 0;
        }
        fp = fopen(res.host_path, "rb");
    }
    if (!fp) {
        guest_vfs_trace_miss(svc->vfs, res.guest_normalized, VFS_MISS_UNRESOLVED);
        trace_miss(guest_path, res.guest_canonical, "unresolved");
        trace_open(runtime, guest_path, &res, mode_name(want_write), 0, 0);
        return 0;
    }

    for (i = 0; i < GWY_VM_FILE_HANDLE_MAX; i++) {
        if (!svc->table[i].in_use) { slot = i; break; }
    }
    if (slot < 0) {
        fclose(fp);
        trace_miss(guest_path, res.guest_canonical, "unresolved");
        trace_open(runtime, guest_path, &res, mode_name(want_write), 0, 0);
        return 0;
    }
    memset(&svc->table[slot], 0, sizeof(svc->table[slot]));
    svc->table[slot].in_use = 1;
    svc->table[slot].generation = svc->next_generation++;
    svc->table[slot].fp = fp;
    svc->table[slot].flags = flags;
    svc->table[slot].backend = res.backend;
    snprintf(svc->table[slot].guest_normalized, sizeof(svc->table[slot].guest_normalized),
             "%s", res.guest_normalized);
    snprintf(svc->table[slot].guest_canonical, sizeof(svc->table[slot].guest_canonical),
             "%s", res.guest_canonical);
    snprintf(svc->table[slot].host_path, sizeof(svc->table[slot].host_path), "%s", res.host_path);
    guest_vfs_trace_open(&res, 1);
    handle = (int32_t)(slot + 1);
    trace_open(runtime, guest_path, &res, mode_name(want_write), 1, handle);
    ext_loader_on_member_open(gwy_ext_loader_ensure(), guest_path);
    return handle;
}

int32_t vm_file_service_close(VmFileService *svc, int32_t handle) {
    VmFileHandleEntry *e = entry_from_handle(svc, handle);
    if (!e) return GWY_MR_FAILED;
    if (e->fp) fclose(e->fp);
    memset(e, 0, sizeof(*e));
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "[VM_FILE_CLOSE] handle=%d result=ok\n", (int)handle);
        trace_line(buf);
    }
    return GWY_MR_SUCCESS;
}

int32_t vm_file_service_read(VmFileService *svc, int32_t handle, void *buf, uint32_t len) {
    VmFileHandleEntry *e = entry_from_handle(svc, handle);
    size_t n;
    int eof;
    char line[160];
    if (!e || !buf) return GWY_MR_FAILED;
    if (len == 0) {
        snprintf(line, sizeof(line), "[VM_FILE_READ] handle=%d requested=0 got=0 eof=0\n", (int)handle);
        trace_line(line);
        return 0;
    }
    n = fread(buf, 1, (size_t)len, e->fp);
    eof = feof(e->fp) ? 1 : 0;
    snprintf(line, sizeof(line), "[VM_FILE_READ] handle=%d requested=%u got=%d eof=%d\n",
             (int)handle, (unsigned)len, (int)n, eof);
    trace_line(line);
    return (int32_t)n;
}

int32_t vm_file_service_write(VmFileService *svc, int32_t handle, const void *buf, uint32_t len) {
    VmFileHandleEntry *e = entry_from_handle(svc, handle);
    size_t n;
    if (!e || !buf) return GWY_MR_FAILED;
    if (len == 0) return 0;
    if (!(e->flags & (GWY_MR_FILE_WRONLY | GWY_MR_FILE_RDWR | GWY_MR_FILE_CREATE))) {
        return GWY_MR_FAILED;
    }
    n = fwrite(buf, 1, (size_t)len, e->fp);
    if (n != (size_t)len) return GWY_MR_FAILED;
    return (int32_t)n;
}

int32_t vm_file_service_seek(VmFileService *svc, int32_t handle, int32_t pos, int method) {
    VmFileHandleEntry *e = entry_from_handle(svc, handle);
    int origin;
    if (!e) return GWY_MR_FAILED;
    if (method == GWY_MR_SEEK_SET) origin = SEEK_SET;
    else if (method == GWY_MR_SEEK_CUR) origin = SEEK_CUR;
    else if (method == GWY_MR_SEEK_END) origin = SEEK_END;
    else return GWY_MR_FAILED;
    if (fseek(e->fp, (long)pos, origin) != 0) {
        char line[160];
        snprintf(line, sizeof(line), "[VM_FILE_SEEK] handle=%d pos=%d method=%d result=fail\n",
                 (int)handle, (int)pos, method);
        trace_line(line);
        return GWY_MR_FAILED;
    }
    {
        char line[160];
        snprintf(line, sizeof(line), "[VM_FILE_SEEK] handle=%d pos=%d method=%d result=ok\n",
                 (int)handle, (int)pos, method);
        trace_line(line);
    }
    return GWY_MR_SUCCESS;
}

int32_t vm_file_service_get_len(VmFileService *svc, const char *guest_path) {
    VfsResolution res;
    LauncherError err;
    FILE *fp;
    long sz;
    if (!svc || !guest_path) return -1;
    if (guest_vfs_resolve(svc->vfs, guest_path, VFS_OPEN_READ, &res, &err) != L_OK || !res.exists) {
        char line[256];
        snprintf(line, sizeof(line), "[VM_FILE_GETLEN] requested=\"%s\" result=MISS\n", guest_path);
        trace_line(line);
        return -1;
    }
    fp = fopen(res.host_path, "rb");
    if (!fp) {
        char line[256];
        snprintf(line, sizeof(line), "[VM_FILE_GETLEN] requested=\"%s\" result=MISS\n", guest_path);
        trace_line(line);
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    sz = ftell(fp);
    fclose(fp);
    if (sz < 0) return -1;
    {
        char line[320];
        snprintf(line, sizeof(line),
                 "[VM_FILE_GETLEN] requested=\"%s\" canonical=\"%s\" backend=%s len=%ld\n",
                 guest_path, res.guest_canonical, vfs_backend_name(res.backend), sz);
        trace_line(line);
    }
    return (int32_t)sz;
}

int32_t vm_file_service_info(VmFileService *svc, const char *guest_path) {
    VfsResolution res;
    LauncherError err;
    if (!svc || !guest_path) return GWY_MR_IS_INVALID;
    if (guest_vfs_resolve(svc->vfs, guest_path, VFS_OPEN_READ, &res, &err) != L_OK || !res.exists) {
        return GWY_MR_IS_INVALID;
    }
    /* GuestVFS currently resolves regular files only. */
    return GWY_MR_IS_FILE;
}

void gwy_vm_file_bind(VmFileService *svc) {
    (void)gwy_vm_file_bind_owned(svc, GWY_VM_FILE_BOOTSTRAP_OWNER);
}

int gwy_vm_file_bind_owned(VmFileService *svc, uint64_t runtime_id) {
    char line[192];
    if (!svc || runtime_id == 0) return -1;
    if (g_bound_svc != NULL && g_bound_owner != 0 && g_bound_owner != runtime_id) {
        snprintf(line, sizeof(line),
                 "[VM_FILE_BIND] refused runtime=%llu already_owned_by=%llu\n",
                 (unsigned long long)runtime_id, (unsigned long long)g_bound_owner);
        trace_line(line);
        return -1;
    }
    g_bound_svc = svc;
    g_bound_owner = runtime_id;
    snprintf(line, sizeof(line), "[VM_FILE_BIND] runtime=%llu svc=%p\n",
             (unsigned long long)runtime_id, (void *)svc);
    trace_line(line);
    return 0;
}

void gwy_vm_file_unbind(void) {
    if (g_bound_svc) {
        char line[128];
        snprintf(line, sizeof(line), "[VM_FILE_UNBIND] force runtime=%llu\n",
                 (unsigned long long)g_bound_owner);
        trace_line(line);
    }
    g_bound_svc = NULL;
    g_bound_owner = 0;
}

void gwy_vm_file_unbind_owned(uint64_t runtime_id) {
    char line[160];
    if (!g_bound_svc) return;
    if (g_bound_owner != runtime_id) {
        snprintf(line, sizeof(line), "[VM_FILE_UNBIND] refused runtime=%llu owner=%llu\n",
                 (unsigned long long)runtime_id, (unsigned long long)g_bound_owner);
        trace_line(line);
        return;
    }
    snprintf(line, sizeof(line), "[VM_FILE_UNBIND] runtime=%llu\n",
             (unsigned long long)runtime_id);
    trace_line(line);
    g_bound_svc = NULL;
    g_bound_owner = 0;
}

int gwy_vm_file_is_bound(void) {
    return g_bound_svc != NULL;
}

uint64_t gwy_vm_file_bound_owner(void) {
    return g_bound_owner;
}

VmFileService *gwy_vm_file_bound_service(void) {
    return g_bound_svc;
}

int32_t gwy_vm_file_open(const char *guest_path, uint32_t flags) {
    if (!g_bound_svc) return 0;
    return vm_file_service_open(g_bound_svc, guest_path, flags);
}

int32_t gwy_vm_file_close(int32_t handle) {
    if (!g_bound_svc) return GWY_MR_FAILED;
    return vm_file_service_close(g_bound_svc, handle);
}

int32_t gwy_vm_file_read(int32_t handle, void *buf, uint32_t len) {
    if (!g_bound_svc) return GWY_MR_FAILED;
    return vm_file_service_read(g_bound_svc, handle, buf, len);
}

int32_t gwy_vm_file_write(int32_t handle, void *buf, uint32_t len) {
    if (!g_bound_svc) return GWY_MR_FAILED;
    return vm_file_service_write(g_bound_svc, handle, buf, len);
}

int32_t gwy_vm_file_seek(int32_t handle, int32_t pos, int method) {
    if (!g_bound_svc) return GWY_MR_FAILED;
    return vm_file_service_seek(g_bound_svc, handle, pos, method);
}

int32_t gwy_vm_file_get_len(const char *guest_path) {
    if (!g_bound_svc) return -1;
    return vm_file_service_get_len(g_bound_svc, guest_path);
}

int32_t gwy_vm_file_info(const char *guest_path) {
    if (!g_bound_svc) return GWY_MR_IS_INVALID;
    return vm_file_service_info(g_bound_svc, guest_path);
}

LauncherStatus gwy_vm_file_bootstrap(const char *resource_root,
                                     const char *overlay_root,
                                     GuestVfs *vfs_storage,
                                     VmFileService **out_svc,
                                     LauncherError *err) {
    PlatformIdentity id;
    SdkKey key;
    char key_host[VFS_PATH_MAX];
    VfsResolution res;
    LauncherStatus st;
    VmFileService *svc = NULL;

    launcher_error_clear(err);
    if (!resource_root || !overlay_root || !vfs_storage || !out_svc) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "vm_file", "null bootstrap arg", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out_svc = NULL;
    ensure_dir(overlay_root);

    st = guest_vfs_init(vfs_storage, resource_root, overlay_root, err);
    if (st != L_OK) return st;

    platform_identity_set_defaults(&id);
    st = platform_sdk_key_generate(&id, &key, err);
    if (st != L_OK) return st;

    /* Overlay-only host path — never cwd/mythroad/sdk_key.dat */
    st = guest_vfs_write_all(vfs_storage, "mythroad/sdk_key.dat", key.bytes, GWY_SDK_KEY_SIZE, err);
    if (st != L_OK) return st;

    snprintf(key_host, sizeof(key_host), "%s/mythroad/sdk_key.dat", overlay_root);
    st = guest_vfs_set_generated(vfs_storage, "mythroad/sdk_key.dat", key_host, key.sha256_hex, err);
    if (st != L_OK) return st;
    st = guest_vfs_set_generated(vfs_storage, "sdk_key.dat", key_host, key.sha256_hex, err);
    if (st != L_OK) return st;

    st = guest_vfs_resolve(vfs_storage, "mythroad/sdk_key.dat", VFS_OPEN_READ, &res, err);
    if (st != L_OK || res.backend != VFS_GENERATED || !res.exists) {
        launcher_error_set(err, L_ERR_STATE, "vm_file", "sdk_key generated resolve failed", key_host);
        return L_ERR_STATE;
    }
    guest_vfs_trace_open(&res, 1);
    printf("[JJFB_SDK_KEY] path=%s sha256=%s backend=generated (no cwd mythroad copy)\n",
           key_host, key.sha256_hex);

    st = vm_file_service_create(vfs_storage, &svc, err);
    if (st != L_OK) return st;
    gwy_vm_file_bind(svc);
    *out_svc = svc;
    return L_OK;
}
