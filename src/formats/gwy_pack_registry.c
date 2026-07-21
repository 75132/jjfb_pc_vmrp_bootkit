#include "gwy_launcher/gwy_pack_registry.h"
#include "gwy_launcher/sha256.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static GwyPackRegistry g_reg;

void gwy_pack_registry_reset(void) {
    memset(&g_reg, 0, sizeof(g_reg));
}

GwyPackRegistry *gwy_pack_registry_get(void) {
    return &g_reg;
}

int gwy_pack_registry_ready(void) {
    return g_reg.ready;
}

size_t gwy_pack_registry_pack_count(void) {
    return g_reg.pack_count;
}

static void path_dirname(const char *path, char *out, size_t out_sz) {
    size_t i, len;
    if (!path || !out || out_sz < 2u) return;
    len = strlen(path);
    for (i = len; i > 0u; i--) {
        if (path[i - 1u] == '/' || path[i - 1u] == '\\') {
            len = i - 1u;
            if (len >= out_sz) len = out_sz - 1u;
            memcpy(out, path, len);
            out[len] = '\0';
            return;
        }
    }
    out[0] = '.';
    out[1] = '\0';
}

static void path_basename_stem(const char *path, char *out, size_t out_sz) {
    const char *base = path;
    const char *p;
    size_t n;
    if (!path || !out || out_sz < 2u) return;
    for (p = path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    n = strlen(base);
    if (n > 4u && (base[n - 4] == '.') &&
        (base[n - 3] == 'm' || base[n - 3] == 'M') &&
        (base[n - 2] == 'r' || base[n - 2] == 'R') &&
        (base[n - 1] == 'p' || base[n - 1] == 'P')) {
        n -= 4u;
    }
    if (n >= out_sz) n = out_sz - 1u;
    memcpy(out, base, n);
    out[n] = '\0';
}

static int str_ends_with_ci(const char *s, const char *suf) {
    size_t ls, lf, i;
    if (!s || !suf) return 0;
    ls = strlen(s);
    lf = strlen(suf);
    if (lf > ls) return 0;
    for (i = 0; i < lf; i++) {
        char a = (char)tolower((unsigned char)s[ls - lf + i]);
        char b = (char)tolower((unsigned char)suf[i]);
        if (a != b) return 0;
    }
    return 1;
}

static int name_contains_ci(const char *s, const char *needle) {
    size_t i, j, ls, ln;
    if (!s || !needle || !needle[0]) return 0;
    ls = strlen(s);
    ln = strlen(needle);
    if (ln > ls) return 0;
    for (i = 0; i + ln <= ls; i++) {
        int ok = 1;
        for (j = 0; j < ln; j++) {
            if (tolower((unsigned char)s[i + j]) != tolower((unsigned char)needle[j])) {
                ok = 0;
                break;
            }
        }
        if (ok) return 1;
    }
    return 0;
}

static int file_exists(const char *path) {
#ifdef _WIN32
    DWORD a;
    if (!path || !path[0]) return 0;
    a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    if (!path || !path[0]) return 0;
    return (stat(path, &st) == 0) && S_ISREG(st.st_mode);
#endif
}

static int dir_exists(const char *path) {
#ifdef _WIN32
    DWORD a;
    if (!path || !path[0]) return 0;
    a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    if (!path || !path[0]) return 0;
    return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
#endif
}

static int add_pack(const char *path) {
    MrpArchive *arch = NULL;
    LauncherError err;
    LauncherStatus st;
    GwyPackEntry *e;
    char stem[GWY_PACK_NAME_MAX];
    if (!path || !path[0] || g_reg.pack_count >= GWY_PACK_MAX) return 0;
    if (!file_exists(path)) return 0;
    /* Dedup by path. */
    {
        size_t i;
        for (i = 0; i < g_reg.pack_count; i++) {
            if (strcmp(g_reg.packs[i].pack_path, path) == 0) return 0;
        }
    }
    path_basename_stem(path, stem, sizeof(stem));
    st = mrp_archive_open(path, &arch, &err);
    if (st != L_OK || !arch) return 0;
    e = &g_reg.packs[g_reg.pack_count];
    memset(e, 0, sizeof(*e));
    snprintf(e->pack_name, sizeof(e->pack_name), "%s", stem);
    snprintf(e->pack_path, sizeof(e->pack_path), "%s", path);
    gwy_sha256_hex(arch->sha256, e->sha256_hex);
    e->appid = arch->appid_le;
    e->appver = arch->appver_le;
    e->member_count = arch->member_count;
    e->is_downimage = name_contains_ci(stem, "downimage");
    g_reg.pack_count++;
    printf("[GWY_PACK_REGISTRY] target=%s pack=%s members=%u sha256=%s evidence=OBSERVED\n",
           g_reg.target_mrp[0] ? g_reg.target_mrp : "?", e->pack_name,
           (unsigned)e->member_count, e->sha256_hex);
    fflush(stdout);
    mrp_archive_close(arch);
    return 1;
}

static void scan_dir_for_mrps(const char *dir) {
#ifdef _WIN32
    char pattern[1100];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    if (!dir || !dir[0] || !dir_exists(dir)) return;
    snprintf(pattern, sizeof(pattern), "%s\\*.mrp", dir);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char full[GWY_PACK_PATH_MAX];
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!str_ends_with_ci(fd.cFileName, ".mrp")) continue;
        /* Skip *.mrp.v style if FindFirst somehow returns them as .mrp — name ends .mrp only. */
        snprintf(full, sizeof(full), "%s/%s", dir, fd.cFileName);
        add_pack(full);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d;
    struct dirent *ent;
    if (!dir || !dir[0] || !dir_exists(dir)) return;
    d = opendir(dir);
    if (!d) return;
    while ((ent = readdir(d)) != NULL) {
        char full[GWY_PACK_PATH_MAX];
        if (!str_ends_with_ci(ent->d_name, ".mrp")) continue;
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
        add_pack(full);
    }
    closedir(d);
#endif
}

static void try_side_pack_dirs(const char *gwy_dir, const char *stem) {
    char cand[GWY_PACK_PATH_MAX];
    if (!gwy_dir || !gwy_dir[0]) return;
    /* Convention: target foo.mrp → sibling fool/ (stem + "ol"). Generic, not name-hardcoded. */
    if (stem && stem[0]) {
        snprintf(cand, sizeof(cand), "%s/%sol", gwy_dir, stem);
        if (dir_exists(cand)) {
            snprintf(g_reg.side_pack_dir, sizeof(g_reg.side_pack_dir), "%s", cand);
            scan_dir_for_mrps(cand);
            return; /* prefer target-local side-pack dir only */
        }
    }
#ifdef _WIN32
    {
        char pattern[1100];
        WIN32_FIND_DATAA fd;
        HANDLE h;
        snprintf(pattern, sizeof(pattern), "%s\\*ol", gwy_dir);
        h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                if (fd.cFileName[0] == '.') continue;
                if (!str_ends_with_ci(fd.cFileName, "ol")) continue;
                snprintf(cand, sizeof(cand), "%s/%s", gwy_dir, fd.cFileName);
                if (!g_reg.side_pack_dir[0])
                    snprintf(g_reg.side_pack_dir, sizeof(g_reg.side_pack_dir), "%s", cand);
                scan_dir_for_mrps(cand);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
#endif
}

LauncherStatus gwy_pack_registry_scan(const char *target_mrp_or_null, LauncherError *err) {
    const char *mrp = target_mrp_or_null;
    char gwy_dir[GWY_PACK_PATH_MAX];
    char stem[GWY_PACK_NAME_MAX];
    (void)err;
    gwy_pack_registry_reset();
    if (!mrp || !mrp[0]) mrp = getenv("JJFB_REAL_MRP_PATH");
    if (!mrp || !mrp[0]) mrp = "game_files/mythroad/320x480/gwy/jjfb.mrp";
    snprintf(g_reg.target_mrp, sizeof(g_reg.target_mrp), "%s", mrp);
    path_dirname(mrp, gwy_dir, sizeof(gwy_dir));
    snprintf(g_reg.gwy_root, sizeof(g_reg.gwy_root), "%s", gwy_dir);
    path_basename_stem(mrp, stem, sizeof(stem));
    try_side_pack_dirs(gwy_dir, stem);
    /* Do not index peer game MRPs under gwy/*.mrp — only side-pack dir. */
    g_reg.ready = (g_reg.pack_count > 0) ? 1 : 0;
    printf("[GWY_PACK_REGISTRY] scan_done target=%s packs=%u ready=%d side=%s evidence=OBSERVED\n",
           g_reg.target_mrp, (unsigned)g_reg.pack_count, g_reg.ready,
           g_reg.side_pack_dir[0] ? g_reg.side_pack_dir : "-");
    fflush(stdout);
    return L_OK;
}

static int stem_eq_ci(const char *a, const char *b) {
    size_t i;
    if (!a || !b) return 0;
    for (i = 0;; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (tolower(ca) != tolower(cb)) return 0;
        if (!ca) return 1;
    }
}

int gwy_pack_registry_find_pack_path(const char *stem, char *out_path, size_t out_sz) {
    size_t i;
    if (!stem || !stem[0] || !out_path || out_sz < 2u) return 0;
    for (i = 0; i < g_reg.pack_count; i++) {
        if (stem_eq_ci(g_reg.packs[i].pack_name, stem)) {
            snprintf(out_path, out_sz, "%s", g_reg.packs[i].pack_path);
            return 1;
        }
    }
    return 0;
}

int gwy_pack_registry_find_member(const char *member_name, char *out_pack_path, size_t pack_sz,
                                  char *out_member, size_t mem_sz) {
    size_t i;
    if (!member_name || !member_name[0]) return 0;
    for (i = 0; i < g_reg.pack_count; i++) {
        MrpArchive *arch = NULL;
        const MrpMember *mem = NULL;
        LauncherError err;
        LauncherStatus st = mrp_archive_open(g_reg.packs[i].pack_path, &arch, &err);
        if (st != L_OK || !arch) continue;
        st = mrp_archive_find_exact(arch, member_name, &mem, &err);
        if (st != L_OK || !mem)
            st = mrp_archive_find_casefold(arch, member_name, &mem, &err);
        if (st == L_OK && mem) {
            if (out_pack_path && pack_sz)
                snprintf(out_pack_path, pack_sz, "%s", g_reg.packs[i].pack_path);
            if (out_member && mem_sz) snprintf(out_member, mem_sz, "%s", mem->name);
            mrp_archive_close(arch);
            return 1;
        }
        mrp_archive_close(arch);
    }
    return 0;
}

int gwy_pack_registry_export_csv(const char *csv_path_or_null) {
    const char *path =
        csv_path_or_null && csv_path_or_null[0] ? csv_path_or_null : "reports/e9z_gwy_pack_registry.csv";
    FILE *fp;
    size_t i;
    fp = fopen(path, "wb");
    if (!fp) return 0;
    fprintf(fp,
            "target_mrp,side_pack_dir,pack_name,pack_path,member_count,appid,appver,sha256,"
            "is_downimage,ready\n");
    for (i = 0; i < g_reg.pack_count; i++) {
        const GwyPackEntry *e = &g_reg.packs[i];
        fprintf(fp, "%s,%s,%s,%s,%u,0x%X,0x%X,%s,%d,%d\n", g_reg.target_mrp,
                g_reg.side_pack_dir[0] ? g_reg.side_pack_dir : "", e->pack_name, e->pack_path,
                (unsigned)e->member_count, e->appid, e->appver, e->sha256_hex, e->is_downimage,
                g_reg.ready);
    }
    fclose(fp);
    return 1;
}
