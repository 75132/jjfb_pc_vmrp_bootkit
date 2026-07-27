#include "gwy_launcher/resource_root.h"
#include "gwy_launcher/gwy_cfg.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static int path_is_dir(const char *path) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static int path_is_file(const char *path) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

static int join2(char *out, size_t cap, const char *a, const char *b) {
    int n;
    size_t al;
    if (!out || !a || !b || cap == 0) return 0;
    al = strlen(a);
    if (al > 0 && (a[al - 1] == '/' || a[al - 1] == '\\'))
        n = snprintf(out, cap, "%s%s", a, b);
    else
        n = snprintf(out, cap, "%s/%s", a, b);
    return n > 0 && (size_t)n < cap;
}

static int hex_eq_ci(const char *a, const char *b) {
    size_t i;
    if (!a || !b) return 0;
    for (i = 0; i < 64; i++) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'F') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'F') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return a[64] == '\0' && b[64] == '\0';
}

static void normalize_slashes(char *path) {
    char *p;
    if (!path) return;
    for (p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
}

LauncherStatus gwy_resource_root_validate(const char *candidate_root,
                                          uint32_t cfg_index,
                                          const char *expected_sha256_hex,
                                          GwyResourceRootResult *out,
                                          LauncherError *err) {
    char jjfb[GWY_RESOURCE_ROOT_PATH_MAX];
    char jjfbol[GWY_RESOURCE_ROOT_PATH_MAX];
    char down_v[GWY_RESOURCE_ROOT_PATH_MAX];
    char cfg_path[GWY_RESOURCE_ROOT_PATH_MAX];
    GwyCfgFile *cfg = NULL;
    GwyCfgRecord rec;
    uint8_t digest[32];
    LauncherStatus st;

    launcher_error_clear(err);
    if (!candidate_root || !candidate_root[0] || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "resource_root", "null candidate", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->path, sizeof(out->path), "%s", candidate_root);
    normalize_slashes(out->path);

    if (!path_is_dir(out->path)) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "resource_root", "root is not a directory",
                           out->path);
        return L_ERR_NOT_FOUND;
    }
    if (!join2(jjfb, sizeof(jjfb), out->path, "gwy/jjfb.mrp") || !path_is_file(jjfb)) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "resource_root", "missing gwy/jjfb.mrp",
                           out->path);
        return L_ERR_NOT_FOUND;
    }
    if (!join2(jjfbol, sizeof(jjfbol), out->path, "gwy/jjfbol") || !path_is_dir(jjfbol)) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "resource_root", "missing gwy/jjfbol/",
                           out->path);
        return L_ERR_NOT_FOUND;
    }
    if (!join2(down_v, sizeof(down_v), out->path, "gwy/jjfbol/downVersion") ||
        !path_is_file(down_v)) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "resource_root",
                           "missing gwy/jjfbol/downVersion", out->path);
        return L_ERR_NOT_FOUND;
    }
    if (!join2(cfg_path, sizeof(cfg_path), out->path, "gwy/cfg.bin") || !path_is_file(cfg_path)) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "resource_root", "missing gwy/cfg.bin",
                           out->path);
        return L_ERR_NOT_FOUND;
    }

    st = gwy_cfg_open(cfg_path, &cfg, err);
    if (st != L_OK) return st;
    st = gwy_cfg_read_record(cfg, cfg_index, &rec, err);
    if (st != L_OK) {
        gwy_cfg_close(cfg);
        return st;
    }
    if (!rec.target_mrp.present ||
        (strcmp(rec.target_mrp.value, "gwy/jjfb.mrp") != 0 &&
         strcmp(rec.target_mrp.value, "gwy\\jjfb.mrp") != 0)) {
        char detail[256];
        snprintf(detail, sizeof(detail), "cfg[%u] target=%s", cfg_index,
                 rec.target_mrp.present ? rec.target_mrp.value : "(absent)");
        gwy_cfg_close(cfg);
        launcher_error_set(err, L_ERR_PROFILE_MISMATCH, "resource_root",
                           "cfg index does not point to gwy/jjfb.mrp", detail);
        return L_ERR_PROFILE_MISMATCH;
    }
    gwy_cfg_close(cfg);

    if (gwy_sha256_file(jjfb, digest) == 0) {
        launcher_error_set(err, L_ERR_IO, "resource_root", "sha256_file failed", jjfb);
        return L_ERR_IO;
    }
    gwy_sha256_hex(digest, out->jjfb_sha256_hex);
    snprintf(out->jjfb_mrp, sizeof(out->jjfb_mrp), "%s", jjfb);

    if (expected_sha256_hex && expected_sha256_hex[0]) {
        if (!hex_eq_ci(expected_sha256_hex, out->jjfb_sha256_hex)) {
            char detail[160];
            snprintf(detail, sizeof(detail), "expected=%s actual=%s", expected_sha256_hex,
                     out->jjfb_sha256_hex);
            launcher_error_set(err, L_ERR_HASH_MISMATCH, "resource_root",
                               "jjfb.mrp SHA-256 mismatch", detail);
            return L_ERR_HASH_MISMATCH;
        }
    }
    return L_OK;
}

static LauncherStatus try_one(const char *candidate,
                              const char *reason,
                              uint32_t cfg_index,
                              const char *expected_sha256_hex,
                              GwyResourceRootResult *out,
                              LauncherError *err) {
    LauncherStatus st;
    if (!candidate || !candidate[0]) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "resource_root", "empty candidate",
                           reason);
        return L_ERR_INVALID_ARGUMENT;
    }
    st = gwy_resource_root_validate(candidate, cfg_index, expected_sha256_hex, out, err);
    if (st == L_OK) {
        snprintf(out->reason, sizeof(out->reason), "%s", reason);
        printf("[RESOURCE_ROOT_RESOLVED] path=%s reason=%s sha=%s evidence=OBSERVED\n",
               out->path, out->reason, out->jjfb_sha256_hex);
        fflush(stdout);
    }
    return st;
}

#ifdef _WIN32
static LauncherStatus scan_mythroad(const char *mythroad_dir,
                                    uint32_t cfg_index,
                                    const char *expected_sha256_hex,
                                    GwyResourceRootResult *out,
                                    LauncherError *err) {
    char pattern[GWY_RESOURCE_ROOT_PATH_MAX];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    LauncherStatus last = L_ERR_NOT_FOUND;

    if (!join2(pattern, sizeof(pattern), mythroad_dir, "*")) {
        launcher_error_set(err, L_ERR_BOUNDS, "resource_root", "scan pattern too long",
                           mythroad_dir);
        return L_ERR_BOUNDS;
    }
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "resource_root", "mythroad scan empty",
                           mythroad_dir);
        return L_ERR_NOT_FOUND;
    }
    do {
        char cand[GWY_RESOURCE_ROOT_PATH_MAX];
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.cFileName[0] == '.') continue;
        if (!join2(cand, sizeof(cand), mythroad_dir, fd.cFileName)) continue;
        if (try_one(cand, "scan", cfg_index, expected_sha256_hex, out, err) == L_OK) {
            FindClose(h);
            return L_OK;
        }
        last = err ? err->code : L_ERR_NOT_FOUND;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (last == L_OK) last = L_ERR_NOT_FOUND;
    launcher_error_set(err, L_ERR_NOT_FOUND, "resource_root",
                       "no valid mythroad/*/gwy/jjfb.mrp candidate", mythroad_dir);
    return L_ERR_NOT_FOUND;
}
#else
static LauncherStatus scan_mythroad(const char *mythroad_dir,
                                    uint32_t cfg_index,
                                    const char *expected_sha256_hex,
                                    GwyResourceRootResult *out,
                                    LauncherError *err) {
    DIR *d = opendir(mythroad_dir);
    struct dirent *ent;
    if (!d) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "resource_root", "mythroad open failed",
                           mythroad_dir);
        return L_ERR_NOT_FOUND;
    }
    while ((ent = readdir(d)) != NULL) {
        char cand[GWY_RESOURCE_ROOT_PATH_MAX];
        if (ent->d_name[0] == '.') continue;
        if (!join2(cand, sizeof(cand), mythroad_dir, ent->d_name)) continue;
        if (!path_is_dir(cand)) continue;
        if (try_one(cand, "scan", cfg_index, expected_sha256_hex, out, err) == L_OK) {
            closedir(d);
            return L_OK;
        }
    }
    closedir(d);
    launcher_error_set(err, L_ERR_NOT_FOUND, "resource_root",
                       "no valid mythroad/*/gwy/jjfb.mrp candidate", mythroad_dir);
    return L_ERR_NOT_FOUND;
}
#endif

LauncherStatus gwy_resource_root_resolve(const GwyResourceRootRequest *req,
                                         GwyResourceRootResult *out,
                                         LauncherError *err) {
    char def240[GWY_RESOURCE_ROOT_PATH_MAX];
    char mythroad[GWY_RESOURCE_ROOT_PATH_MAX];
    LauncherStatus st;
    uint32_t cfg_index;
    const char *sha;

    launcher_error_clear(err);
    if (!req || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "resource_root", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    cfg_index = req->cfg_index ? req->cfg_index : 36u;
    sha = req->expected_sha256_hex;

    if (req->explicit_root && req->explicit_root[0]) {
        st = try_one(req->explicit_root, "explicit", cfg_index, sha, out, err);
        if (st != L_OK) {
            /* Strict: never fall back from --root. */
            return st;
        }
        return L_OK;
    }

    if (req->env_root && req->env_root[0]) {
        st = try_one(req->env_root, "env", cfg_index, sha, out, err);
        if (st != L_OK) {
            /* Strict: never fall back from GWY_RESOURCE_ROOT. */
            return st;
        }
        return L_OK;
    }

    if (!req->repo_root || !req->repo_root[0]) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "resource_root",
                           "repo_root required when no explicit/env root", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }

    if (!join2(def240, sizeof(def240), req->repo_root, "game_files/mythroad/240x320")) {
        launcher_error_set(err, L_ERR_BOUNDS, "resource_root", "default path too long",
                           req->repo_root);
        return L_ERR_BOUNDS;
    }
    st = try_one(def240, "default_240x320", cfg_index, sha, out, err);
    if (st == L_OK) return L_OK;

    if (!join2(mythroad, sizeof(mythroad), req->repo_root, "game_files/mythroad")) {
        launcher_error_set(err, L_ERR_BOUNDS, "resource_root", "mythroad path too long",
                           req->repo_root);
        return L_ERR_BOUNDS;
    }
    return scan_mythroad(mythroad, cfg_index, sha, out, err);
}
