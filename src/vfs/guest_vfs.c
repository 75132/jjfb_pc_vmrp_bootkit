#include "gwy_launcher/guest_vfs.h"
#include "gwy_launcher/e10a_shell_trace.h"
#include "gwy_launcher/ext_post_cont_audit.h"
#include "gwy_launcher/platform_call_census.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Strong symbol from launcher_core (gwy_ext_obs.c); weak in plain vmrp. */
void gwy_ext_obs_file_open(const char *guest_path, int ok);
void gwy_ext_obs_file_open_ex(const char *guest_path, const char *host_path, int ok);

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

static int host_exists(const char *path) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

static void ensure_dir_one(const char *dir) {
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
    if (isalpha((unsigned char)dir[0]) && dir[1] == ':') start = 2;
    if (dir[start] == '/' || dir[start] == '\\') start++;
#endif
    for (i = start; dir[i]; i++) {
        if (dir[i] == '/' || dir[i] == '\\') {
            char c = dir[i];
            dir[i] = '\0';
            if (dir[0]) ensure_dir_one(dir);
            dir[i] = c;
        }
    }
    ensure_dir_one(dir);
}

static int strip_prefix(const char *in, const char *prefix, char *out, size_t out_cap) {
    size_t n = strlen(prefix);
    if (strncmp(in, prefix, n) != 0) return 0;
    if (in[n] == '/') n++;
    snprintf(out, out_cap, "%s", in + n);
    return 1;
}

static int is_win_reserved_device(const char *name) {
    static const char *devs[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
        NULL
    };
    char base[128];
    size_t i, n = 0;
    const char *dot;
    int d;
    if (!name || !name[0]) return 0;
    /* Take last segment basename */
    {
        const char *slash = strrchr(name, '/');
        const char *seg = slash ? slash + 1 : name;
        dot = strchr(seg, '.');
        n = dot ? (size_t)(dot - seg) : strlen(seg);
        if (n == 0 || n >= sizeof(base)) return 0;
        memcpy(base, seg, n);
        base[n] = '\0';
    }
    for (i = 0; base[i]; i++) base[i] = (char)toupper((unsigned char)base[i]);
    for (d = 0; devs[d]; d++) {
        if (strcmp(base, devs[d]) == 0) return 1;
    }
    return 0;
}

static void record_miss(GuestVfs *vfs, const char *guest_normalized, VfsMissKind kind) {
    size_t i;
    char canon[VFS_PATH_MAX];
    if (!vfs || !guest_normalized) return;
    guest_path_to_canonical(guest_normalized, canon, sizeof(canon));
    vfs->miss_total++;
    for (i = 0; i < VFS_MISS_MAX; i++) {
        if (vfs->misses[i].used &&
            strcmp(vfs->misses[i].guest_normalized, guest_normalized) == 0 &&
            vfs->misses[i].kind == kind) {
            vfs->misses[i].count++;
            return;
        }
    }
    for (i = 0; i < VFS_MISS_MAX; i++) {
        if (!vfs->misses[i].used) {
            vfs->misses[i].used = 1;
            vfs->misses[i].kind = kind;
            snprintf(vfs->misses[i].guest_normalized, sizeof(vfs->misses[i].guest_normalized),
                     "%s", guest_normalized);
            snprintf(vfs->misses[i].guest_canonical, sizeof(vfs->misses[i].guest_canonical),
                     "%s", canon);
            vfs->misses[i].count = 1;
            vfs->miss_unique++;
            return;
        }
    }
}

static const VfsGeneratedNode *find_generated(const GuestVfs *vfs, const char *norm) {
    size_t i;
    char alt[VFS_PATH_MAX];
    char canon[VFS_PATH_MAX];
    guest_path_to_canonical(norm, canon, sizeof(canon));
    for (i = 0; i < VFS_GENERATED_MAX; i++) {
        char gcan[VFS_PATH_MAX];
        if (!vfs->generated[i].used) continue;
        if (strcmp(vfs->generated[i].guest_normalized, norm) == 0) return &vfs->generated[i];
        guest_path_to_canonical(vfs->generated[i].guest_normalized, gcan, sizeof(gcan));
        if (strcmp(gcan, canon) == 0) return &vfs->generated[i];
    }
    if (strip_prefix(norm, "mythroad", alt, sizeof(alt))) {
        for (i = 0; i < VFS_GENERATED_MAX; i++) {
            if (!vfs->generated[i].used) continue;
            if (strcmp(vfs->generated[i].guest_normalized, alt) == 0) return &vfs->generated[i];
        }
    } else if (strlen(norm) + 10 < sizeof(alt)) {
        snprintf(alt, sizeof(alt), "mythroad/%s", norm);
        for (i = 0; i < VFS_GENERATED_MAX; i++) {
            if (!vfs->generated[i].used) continue;
            if (strcmp(vfs->generated[i].guest_normalized, alt) == 0) return &vfs->generated[i];
        }
    }
    return NULL;
}

const char *vfs_backend_name(VfsBackend b) {
    switch (b) {
    case VFS_CANONICAL_READONLY: return "canonical";
    case VFS_OVERLAY_WRITABLE: return "overlay";
    case VFS_GENERATED: return "generated";
    default: return "unknown";
    }
}

const char *vfs_miss_kind_name(VfsMissKind k) {
    switch (k) {
    case VFS_MISS_PROBE: return "probe";
    case VFS_MISS_UNRESOLVED: return "unresolved";
    default: return "unknown";
    }
}

void guest_path_to_canonical(const char *normalized, char *out_canonical, size_t out_cap) {
    char rel[VFS_PATH_MAX];
    if (!out_canonical || out_cap == 0) return;
    out_canonical[0] = '\0';
    if (!normalized || !normalized[0]) return;
    if (strip_prefix(normalized, "mythroad", rel, sizeof(rel))) {
        snprintf(out_canonical, out_cap, "%s", rel[0] ? rel : normalized);
    } else {
        snprintf(out_canonical, out_cap, "%s", normalized);
    }
}

LauncherStatus guest_vfs_init(GuestVfs *vfs,
                              const char *resource_root,
                              const char *overlay_root,
                              LauncherError *err) {
    launcher_error_clear(err);
    if (!vfs || !resource_root || !resource_root[0]) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    memset(vfs, 0, sizeof(*vfs));
    snprintf(vfs->resource_root, sizeof(vfs->resource_root), "%s", resource_root);
    if (overlay_root && overlay_root[0]) {
        snprintf(vfs->overlay_root, sizeof(vfs->overlay_root), "%s", overlay_root);
        vfs->has_overlay = 1;
    }
    return L_OK;
}

void guest_vfs_reset_misses(GuestVfs *vfs) {
    if (!vfs) return;
    memset(vfs->misses, 0, sizeof(vfs->misses));
    vfs->miss_unique = 0;
    vfs->miss_total = 0;
}

LauncherStatus guest_vfs_set_generated(GuestVfs *vfs,
                                       const char *guest_path,
                                       const char *host_path,
                                       const char *sha256_hex,
                                       LauncherError *err) {
    char norm[VFS_PATH_MAX];
    LauncherStatus st;
    size_t i;
    VfsGeneratedNode *slot = NULL;

    launcher_error_clear(err);
    if (!vfs || !guest_path || !host_path || !host_path[0]) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    st = guest_path_canonicalize(guest_path, norm, sizeof(norm), err);
    if (st != L_OK) return st;

    for (i = 0; i < VFS_GENERATED_MAX; i++) {
        if (vfs->generated[i].used && strcmp(vfs->generated[i].guest_normalized, norm) == 0) {
            slot = &vfs->generated[i];
            break;
        }
    }
    if (!slot) {
        for (i = 0; i < VFS_GENERATED_MAX; i++) {
            if (!vfs->generated[i].used) {
                slot = &vfs->generated[i];
                vfs->generated_count++;
                break;
            }
        }
    }
    if (!slot) {
        launcher_error_set(err, L_ERR_BOUNDS, "guest_vfs", "generated table full", norm);
        return L_ERR_BOUNDS;
    }
    memset(slot, 0, sizeof(*slot));
    slot->used = 1;
    snprintf(slot->guest_normalized, sizeof(slot->guest_normalized), "%s", norm);
    snprintf(slot->host_path, sizeof(slot->host_path), "%s", host_path);
    if (sha256_hex) snprintf(slot->sha256_hex, sizeof(slot->sha256_hex), "%s", sha256_hex);
    return L_OK;
}

LauncherStatus guest_path_canonicalize(const char *guest_path,
                                       char *out_normalized,
                                       size_t out_cap,
                                       LauncherError *err) {
    char tmp[VFS_PATH_MAX];
    size_t i, o = 0;
    int prev_slash = 0;

    launcher_error_clear(err);
    if (!guest_path || !out_normalized || out_cap == 0) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (guest_path[0] == '\0') {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "empty path", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    for (i = 0; guest_path[i]; i++) {
        if ((unsigned char)guest_path[i] < 0x20) {
            launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "control char in path", guest_path);
            return L_ERR_INVALID_ARGUMENT;
        }
        /* NTFS ADS / stream / trailing colon */
        if (guest_path[i] == ':') {
            launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "colon/ADS rejected", guest_path);
            return L_ERR_INVALID_ARGUMENT;
        }
    }
    /* Extended path / device namespace */
    if (strncmp(guest_path, "\\\\?\\", 4) == 0 || strncmp(guest_path, "//?/", 4) == 0 ||
        strncmp(guest_path, "\\\\.\\", 4) == 0) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "extended path rejected", guest_path);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (guest_path[0] == '/' || guest_path[0] == '\\') {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "absolute path rejected", guest_path);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (isalpha((unsigned char)guest_path[0]) && guest_path[1] == ':') {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "drive path rejected", guest_path);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (guest_path[0] == '\\' && guest_path[1] == '\\') {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "UNC path rejected", guest_path);
        return L_ERR_INVALID_ARGUMENT;
    }

    for (i = 0; guest_path[i] && o + 1 < sizeof(tmp); i++) {
        char c = guest_path[i];
        if (c == '\\') c = '/';
        if (c == '/') {
            if (prev_slash) continue;
            prev_slash = 1;
            tmp[o++] = '/';
            continue;
        }
        prev_slash = 0;
        tmp[o++] = c;
    }
    tmp[o] = '\0';
    while (o > 0 && tmp[o - 1] == '/') tmp[--o] = '\0';

    {
        char *tok;
        char copy[VFS_PATH_MAX];
        char parts[64][128];
        int nparts = 0;
        int p;
        snprintf(copy, sizeof(copy), "%s", tmp);
        for (tok = strtok(copy, "/"); tok; tok = strtok(NULL, "/")) {
            if (tok[0] == '\0' || strcmp(tok, ".") == 0) continue;
            if (strcmp(tok, "..") == 0) {
                launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "traversal rejected", guest_path);
                return L_ERR_INVALID_ARGUMENT;
            }
            if (is_win_reserved_device(tok)) {
                launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "reserved device name", guest_path);
                return L_ERR_INVALID_ARGUMENT;
            }
            if (nparts >= 64) {
                launcher_error_set(err, L_ERR_BOUNDS, "guest_vfs", "path too deep", guest_path);
                return L_ERR_BOUNDS;
            }
            snprintf(parts[nparts], sizeof(parts[nparts]), "%s", tok);
            nparts++;
        }
        o = 0;
        out_normalized[0] = '\0';
        for (p = 0; p < nparts; p++) {
            int n = snprintf(out_normalized + o, out_cap - o, "%s%s", (o ? "/" : ""), parts[p]);
            if (n < 0 || (size_t)n >= out_cap - o) {
                launcher_error_set(err, L_ERR_BOUNDS, "guest_vfs", "normalized path too long", guest_path);
                return L_ERR_BOUNDS;
            }
            o += (size_t)n;
        }
        if (out_normalized[0] == '\0') {
            launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "empty normalized path", guest_path);
            return L_ERR_INVALID_ARGUMENT;
        }
        if (is_win_reserved_device(out_normalized)) {
            launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "reserved device name", guest_path);
            return L_ERR_INVALID_ARGUMENT;
        }
    }
    return L_OK;
}

static int env_alias_on(void) {
    const char *e = getenv("JJFB_VFS_RES_ALIAS");
    return e && e[0] == '1' && e[1] == '\0';
}

/* Replace first occurrence of needle in path → out (cap). Returns 1 if replaced. */
static int path_replace_once(const char *in, const char *needle, const char *repl, char *out,
                             size_t cap) {
    const char *p;
    size_t nlen, pre;
    if (!in || !needle || !repl || !out || !cap) return 0;
    p = strstr(in, needle);
    if (!p) return 0;
    nlen = strlen(needle);
    pre = (size_t)(p - in);
    if (snprintf(out, cap, "%.*s%s%s", (int)pre, in, repl, p + nlen) >= (int)cap) return 0;
    return 1;
}

static int try_alias_host(const char *primary_host, char *alt_host, size_t alt_cap,
                          const char **out_reason) {
    if (!primary_host || !alt_host || !alt_cap) return 0;
    /* Resolution sibling: .../320x480/... → .../240x320/... */
    if (path_replace_once(primary_host, "320x480", "240x320", alt_host, alt_cap) &&
        host_exists(alt_host)) {
        if (out_reason) *out_reason = "resolution_320x480_to_240x320";
        return 1;
    }
    /* Font fallback: system/gb16.uc2 → system/gb12.uc2 */
    if (path_replace_once(primary_host, "gb16.uc2", "gb12.uc2", alt_host, alt_cap) &&
        host_exists(alt_host)) {
        if (out_reason) *out_reason = "gb16_to_gb12";
        return 1;
    }
    return 0;
}

LauncherStatus guest_vfs_resolve(GuestVfs *vfs,
                                 const char *guest_path,
                                 VfsOpenMode mode,
                                 VfsResolution *out,
                                 LauncherError *err) {
    char norm[VFS_PATH_MAX];
    char rel[VFS_PATH_MAX];
    const VfsGeneratedNode *gen;
    LauncherStatus st;

    launcher_error_clear(err);
    if (!vfs || !guest_path || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->guest_original, sizeof(out->guest_original), "%s", guest_path);
    st = guest_path_canonicalize(guest_path, norm, sizeof(norm), err);
    if (st != L_OK) return st;
    snprintf(out->guest_normalized, sizeof(out->guest_normalized), "%s", norm);
    guest_path_to_canonical(norm, out->guest_canonical, sizeof(out->guest_canonical));

    gen = find_generated(vfs, norm);
    /* Generated is immutable: reject writes that would target it. */
    if (gen && mode == VFS_OPEN_WRITE) {
        launcher_error_set(err, L_ERR_UNSUPPORTED, "guest_vfs",
                           "generated immutable; write rejected", out->guest_canonical);
        return L_ERR_UNSUPPORTED;
    }

    if (gen && mode == VFS_OPEN_READ) {
        snprintf(out->host_path, sizeof(out->host_path), "%s", gen->host_path);
        out->backend = VFS_GENERATED;
        snprintf(out->rule, sizeof(out->rule), "%s", "generated");
        out->exists = host_exists(out->host_path);
        if (out->exists) return L_OK;
    }

    if (vfs->has_overlay) {
        /* Overlay keys use normalized (may include mythroad/) for stable layout. */
        if (snprintf(out->host_path, sizeof(out->host_path), "%s/%s", vfs->overlay_root, norm) >=
            (int)sizeof(out->host_path)) {
            launcher_error_set(err, L_ERR_BOUNDS, "guest_vfs", "overlay host path too long", norm);
            return L_ERR_BOUNDS;
        }
        if (mode == VFS_OPEN_WRITE) {
            out->backend = VFS_OVERLAY_WRITABLE;
            snprintf(out->rule, sizeof(out->rule), "%s", "overlay");
            out->exists = host_exists(out->host_path);
            return L_OK;
        }
        if (host_exists(out->host_path)) {
            out->backend = VFS_OVERLAY_WRITABLE;
            snprintf(out->rule, sizeof(out->rule), "%s", "overlay");
            out->exists = 1;
            return L_OK;
        }
    } else if (mode == VFS_OPEN_WRITE) {
        launcher_error_set(err, L_ERR_UNSUPPORTED, "guest_vfs", "write without overlay rejected", norm);
        return L_ERR_UNSUPPORTED;
    }

    snprintf(rel, sizeof(rel), "%s", out->guest_canonical);
    if (strcmp(norm, out->guest_canonical) != 0) {
        snprintf(out->rule, sizeof(out->rule), "%s", "strip_mythroad_prefix");
    } else if (strncmp(rel, "gwy/", 4) == 0 || strcmp(rel, "gwy") == 0) {
        snprintf(out->rule, sizeof(out->rule), "%s", "gwy_under_root");
    } else {
        snprintf(out->rule, sizeof(out->rule), "%s", "relative_under_root");
    }

    if (snprintf(out->host_path, sizeof(out->host_path), "%s/%s", vfs->resource_root, rel) >=
        (int)sizeof(out->host_path)) {
        launcher_error_set(err, L_ERR_BOUNDS, "guest_vfs", "host path too long", rel);
        return L_ERR_BOUNDS;
    }
    out->backend = VFS_CANONICAL_READONLY;
    out->exists = host_exists(out->host_path);
    if (!out->exists && mode == VFS_OPEN_READ && env_alias_on()) {
        char alt[VFS_PATH_MAX];
        const char *reason = NULL;
        if (try_alias_host(out->host_path, alt, sizeof(alt), &reason)) {
            printf("[JJFB_VFS_ALIAS] from=\"%s\" to=\"%s\" reason=%s evidence=OBSERVED\n",
                   out->host_path, alt, reason ? reason : "?");
            fflush(stdout);
            snprintf(out->host_path, sizeof(out->host_path), "%s", alt);
            snprintf(out->rule, sizeof(out->rule), "alias:%s", reason ? reason : "fallback");
            out->exists = 1;
            return L_OK;
        }
    }
    if (!out->exists && mode == VFS_OPEN_READ) {
        record_miss(vfs, out->guest_normalized, VFS_MISS_PROBE);
        launcher_error_set(err, L_ERR_NOT_FOUND, "guest_vfs", "FILEOPEN_MISS", out->guest_normalized);
        return L_ERR_NOT_FOUND;
    }
    return L_OK;
}

void guest_vfs_trace_open(const VfsResolution *res, int ok) {
    const char *guest;
    if (!res) return;
    printf("[JJFB_FILEOPEN] requested=\"%s\" normalized=\"%s\" canonical=\"%s\" host=\"%s\" ok=%d backend=%s rule=%s\n",
           res->guest_original, res->guest_normalized, res->guest_canonical,
           res->host_path, ok, vfs_backend_name(res->backend), res->rule);
    guest = res->guest_canonical[0] ? res->guest_canonical : res->guest_normalized;
    gwy_ext_obs_file_open_ex(guest, res->host_path, ok);
    if (e10a_shell_trace_enabled()) {
        const char *lp = getenv("JJFB_LAUNCH_PATH");
        if (lp && (strstr(lp, "shell") || strstr(lp, "gwy_native"))) {
            e10a_shell_vfs("open", "guest_vfs", res->guest_normalized, res->host_path, ok, ok, 0,
                           0);
        }
    }
}

void guest_vfs_trace_miss(const GuestVfs *vfs, const char *guest_normalized, VfsMissKind kind) {
    char canon[VFS_PATH_MAX];
    (void)vfs;
    guest_path_to_canonical(guest_normalized ? guest_normalized : "", canon, sizeof(canon));
    printf("[JJFB_FILEOPEN_MISS] guest=\"%s\" canonical=\"%s\" kind=%s\n",
           guest_normalized ? guest_normalized : "", canon, vfs_miss_kind_name(kind));
    gwy_ext_obs_file_open(guest_normalized ? guest_normalized : canon, 0);
    platform_call_census_note_file_miss(guest_normalized ? guest_normalized : canon);
    if (e10a_shell_trace_enabled()) {
        const char *lp = getenv("JJFB_LAUNCH_PATH");
        if (lp && (strstr(lp, "shell") || strstr(lp, "gwy_native"))) {
            void *uc0 = ext_post_cont_audit_last_uc();
            if (uc0) {
                e10a_vfs_set_ctx_from_uc(uc0, "vfs_miss", ext_post_cont_audit_last_module(),
                                         NULL, 0, "MISS_SITE");
            }
            e10a_shell_vfs("miss", "guest_vfs", guest_normalized ? guest_normalized : canon, "",
                           0, -1, 0, 0);
        }
    }
}

size_t guest_vfs_miss_summary(const GuestVfs *vfs) {
    size_t i;
    if (!vfs) return 0;
    printf("[JJFB_VFS_MISS_SUMMARY] unique=%zu total=%u\n",
           (size_t)vfs->miss_unique, (unsigned)vfs->miss_total);
    for (i = 0; i < VFS_MISS_MAX; i++) {
        if (!vfs->misses[i].used) continue;
        printf("[JJFB_VFS_MISS] guest=\"%s\" canonical=\"%s\" kind=%s count=%u\n",
               vfs->misses[i].guest_normalized,
               vfs->misses[i].guest_canonical,
               vfs_miss_kind_name(vfs->misses[i].kind),
               (unsigned)vfs->misses[i].count);
    }
    return vfs->miss_unique;
}

LauncherStatus guest_vfs_open(GuestVfs *vfs,
                              const char *guest_path,
                              VfsOpenMode mode,
                              VfsHandle *out_handle,
                              VfsResolution *out_res,
                              LauncherError *err) {
    VfsResolution res;
    LauncherStatus st;
    int slot = -1;
    int i;
    FILE *fp;
    const char *fmode;

    launcher_error_clear(err);
    if (!vfs || !guest_path || !out_handle) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out_handle = 0;
    st = guest_vfs_resolve(vfs, guest_path, mode, &res, err);
    if (st != L_OK) {
        if (st == L_ERR_NOT_FOUND) guest_vfs_trace_miss(vfs, res.guest_normalized, VFS_MISS_PROBE);
        return st;
    }
    if (mode == VFS_OPEN_WRITE) {
        if (res.backend != VFS_OVERLAY_WRITABLE) {
            launcher_error_set(err, L_ERR_UNSUPPORTED, "guest_vfs", "write only to overlay", res.host_path);
            return L_ERR_UNSUPPORTED;
        }
        mkdir_p_parent(res.host_path);
        fmode = "wb";
    } else {
        fmode = "rb";
    }
    fp = fopen(res.host_path, fmode);
    if (!fp) {
        if (mode == VFS_OPEN_READ) {
            record_miss(vfs, res.guest_normalized, VFS_MISS_UNRESOLVED);
            guest_vfs_trace_miss(vfs, res.guest_normalized, VFS_MISS_UNRESOLVED);
            launcher_error_set(err, L_ERR_NOT_FOUND, "guest_vfs", "FILEOPEN_MISS", res.guest_normalized);
            return L_ERR_NOT_FOUND;
        }
        launcher_error_set(err, L_ERR_IO, "guest_vfs", "fopen failed", res.host_path);
        return L_ERR_IO;
    }
    for (i = 0; i < VFS_HANDLE_MAX; i++) {
        if (!vfs->files[i].used) { slot = i; break; }
    }
    if (slot < 0) {
        fclose(fp);
        launcher_error_set(err, L_ERR_BOUNDS, "guest_vfs", "too many open files", NULL);
        return L_ERR_BOUNDS;
    }
    memset(&vfs->files[slot], 0, sizeof(vfs->files[slot]));
    vfs->files[slot].used = 1;
    vfs->files[slot].fp = fp;
    vfs->files[slot].mode = mode;
    vfs->files[slot].backend = res.backend;
    snprintf(vfs->files[slot].guest_normalized, sizeof(vfs->files[slot].guest_normalized),
             "%s", res.guest_normalized);
    snprintf(vfs->files[slot].host_path, sizeof(vfs->files[slot].host_path), "%s", res.host_path);
    *out_handle = slot + 1;
    guest_vfs_trace_open(&res, 1);
    if (out_res) *out_res = res;
    return L_OK;
}

static VfsFile *file_from_handle(GuestVfs *vfs, VfsHandle handle, LauncherError *err) {
    if (!vfs || handle < 1 || handle > VFS_HANDLE_MAX || !vfs->files[handle - 1].used) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "bad handle", NULL);
        return NULL;
    }
    return &vfs->files[handle - 1];
}

LauncherStatus guest_vfs_read(GuestVfs *vfs,
                              VfsHandle handle,
                              void *buf,
                              size_t len,
                              size_t *out_read,
                              LauncherError *err) {
    VfsFile *f;
    size_t n;
    launcher_error_clear(err);
    if (out_read) *out_read = 0;
    f = file_from_handle(vfs, handle, err);
    if (!f) return L_ERR_INVALID_ARGUMENT;
    if (f->mode != VFS_OPEN_READ) {
        launcher_error_set(err, L_ERR_STATE, "guest_vfs", "not open for read", f->guest_normalized);
        return L_ERR_STATE;
    }
    n = fread(buf, 1, len, f->fp);
    if (out_read) *out_read = n;
    return L_OK;
}

LauncherStatus guest_vfs_write(GuestVfs *vfs,
                               VfsHandle handle,
                               const void *buf,
                               size_t len,
                               size_t *out_written,
                               LauncherError *err) {
    VfsFile *f;
    size_t n;
    launcher_error_clear(err);
    if (out_written) *out_written = 0;
    f = file_from_handle(vfs, handle, err);
    if (!f) return L_ERR_INVALID_ARGUMENT;
    if (f->mode != VFS_OPEN_WRITE) {
        launcher_error_set(err, L_ERR_STATE, "guest_vfs", "not open for write", f->guest_normalized);
        return L_ERR_STATE;
    }
    n = fwrite(buf, 1, len, f->fp);
    if (out_written) *out_written = n;
    if (n != len) {
        launcher_error_set(err, L_ERR_IO, "guest_vfs", "short write", f->host_path);
        return L_ERR_IO;
    }
    return L_OK;
}

LauncherStatus guest_vfs_close(GuestVfs *vfs, VfsHandle handle, LauncherError *err) {
    VfsFile *f;
    launcher_error_clear(err);
    f = file_from_handle(vfs, handle, err);
    if (!f) return L_ERR_INVALID_ARGUMENT;
    if (f->fp) fclose(f->fp);
    memset(f, 0, sizeof(*f));
    return L_OK;
}

LauncherStatus guest_vfs_read_all(GuestVfs *vfs,
                                  const char *guest_path,
                                  uint8_t **out_data,
                                  size_t *out_size,
                                  LauncherError *err) {
    VfsHandle h = 0;
    VfsResolution res;
    LauncherStatus st;
    long sz;
    uint8_t *buf;
    size_t nread = 0;
    VfsFile *f;

    launcher_error_clear(err);
    if (!out_data || !out_size) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "null out", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out_data = NULL;
    *out_size = 0;
    st = guest_vfs_open(vfs, guest_path, VFS_OPEN_READ, &h, &res, err);
    if (st != L_OK) return st;
    f = &vfs->files[h - 1];
    if (fseek(f->fp, 0, SEEK_END) != 0) {
        guest_vfs_close(vfs, h, err);
        launcher_error_set(err, L_ERR_IO, "guest_vfs", "seek end failed", res.host_path);
        return L_ERR_IO;
    }
    sz = ftell(f->fp);
    if (sz < 0) {
        guest_vfs_close(vfs, h, err);
        launcher_error_set(err, L_ERR_IO, "guest_vfs", "ftell failed", res.host_path);
        return L_ERR_IO;
    }
    rewind(f->fp);
    buf = (uint8_t *)malloc((size_t)sz + 1);
    if (!buf) {
        guest_vfs_close(vfs, h, err);
        launcher_error_set(err, L_ERR_IO, "guest_vfs", "oom", NULL);
        return L_ERR_IO;
    }
    st = guest_vfs_read(vfs, h, buf, (size_t)sz, &nread, err);
    guest_vfs_close(vfs, h, err);
    if (st != L_OK) {
        free(buf);
        return st;
    }
    buf[nread] = 0;
    *out_data = buf;
    *out_size = nread;
    return L_OK;
}

LauncherStatus guest_vfs_write_all(GuestVfs *vfs,
                                   const char *guest_path,
                                   const void *data,
                                   size_t size,
                                   LauncherError *err) {
    VfsHandle h = 0;
    size_t nw = 0;
    LauncherStatus st;

    launcher_error_clear(err);
    if (!data && size > 0) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "guest_vfs", "null data", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    st = guest_vfs_open(vfs, guest_path, VFS_OPEN_WRITE, &h, NULL, err);
    if (st != L_OK) return st;
    if (size > 0) {
        st = guest_vfs_write(vfs, h, data, size, &nw, err);
        if (st != L_OK) {
            guest_vfs_close(vfs, h, err);
            return st;
        }
    }
    return guest_vfs_close(vfs, h, err);
}
