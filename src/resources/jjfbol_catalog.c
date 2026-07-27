#include "gwy_launcher/jjfbol_catalog.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/product_runtime_progress.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef struct LruSlot {
    int in_use;
    uint32_t package_index;
    MrpArchive *archive;
    uint64_t last_use;
} LruSlot;

static char g_root[JJFBOL_PATH_MAX];
static JjfbolPackageIndex g_packs[JJFBOL_MAX_PACKAGES];
static uint32_t g_pack_n;
static uint32_t g_version_files;
static uint32_t g_down_version; /* host endian */
static uint32_t g_down_raw_be;
static JjfbolMemberIndex *g_members;
static uint32_t g_member_n;
static uint32_t g_member_cap;
static int g_ready;
static LruSlot g_lru[JJFBOL_LRU_SLOTS];
static uint64_t g_lru_tick;

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

static uint32_t read_be32_file(const char *path, int *ok) {
    FILE *fp;
    uint8_t b[4];
    *ok = 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    if (fread(b, 1, 4, fp) != 4) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    *ok = 1;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) |
           (uint32_t)b[3];
}

static void stem_from_name(const char *fname, char *out, size_t cap) {
    size_t n = strlen(fname);
    if (n > 4 && (strcmp(fname + n - 4, ".mrp") == 0 || strcmp(fname + n - 4, ".MRP") == 0))
        n -= 4;
    if (n >= cap) n = cap - 1;
    memcpy(out, fname, n);
    out[n] = 0;
}

static int ends_with_ci(const char *s, const char *suf) {
    size_t ns, nf;
    size_t i;
    if (!s || !suf) return 0;
    ns = strlen(s);
    nf = strlen(suf);
    if (ns < nf) return 0;
    for (i = 0; i < nf; i++) {
        char a = (char)tolower((unsigned char)s[ns - nf + i]);
        char b = (char)tolower((unsigned char)suf[i]);
        if (a != b) return 0;
    }
    return 1;
}

static int ensure_member_cap(uint32_t need) {
    JjfbolMemberIndex *nbuf;
    uint32_t ncap;
    if (need <= g_member_cap) return 1;
    ncap = g_member_cap ? g_member_cap * 2u : 4096u;
    while (ncap < need) ncap *= 2u;
    nbuf = (JjfbolMemberIndex *)realloc(g_members, (size_t)ncap * sizeof(*nbuf));
    if (!nbuf) return 0;
    g_members = nbuf;
    g_member_cap = ncap;
    return 1;
}

static void lru_close_all(void) {
    int i;
    for (i = 0; i < JJFBOL_LRU_SLOTS; i++) {
        if (g_lru[i].archive) {
            mrp_archive_close(g_lru[i].archive);
            g_lru[i].archive = NULL;
        }
        g_lru[i].in_use = 0;
    }
}

void jjfbol_catalog_reset(void) {
    lru_close_all();
    free(g_members);
    g_members = NULL;
    g_member_n = 0;
    g_member_cap = 0;
    g_pack_n = 0;
    g_version_files = 0;
    g_down_version = 0;
    g_down_raw_be = 0;
    g_root[0] = 0;
    g_ready = 0;
}

static LauncherStatus index_one_mrp(const char *path, const char *fname, LauncherError *err) {
    MrpArchive *arch = NULL;
    LauncherStatus st;
    JjfbolPackageIndex *pkg;
    char vpath[JJFBOL_PATH_MAX];
    int vok = 0;
    uint32_t ver;
    size_t mi;

    if (g_pack_n >= JJFBOL_MAX_PACKAGES) {
        launcher_error_set(err, L_ERR_BOUNDS, "jjfbol_catalog", "too many packages", path);
        return L_ERR_BOUNDS;
    }
    st = mrp_archive_open(path, &arch, err);
    if (st != L_OK) return st;

    pkg = &g_packs[g_pack_n];
    memset(pkg, 0, sizeof(*pkg));
    snprintf(pkg->path, sizeof(pkg->path), "%s", path);
    stem_from_name(fname, pkg->stem, sizeof(pkg->stem));
    pkg->first_member = g_member_n;
    pkg->member_count = (uint32_t)arch->member_count;

    if (!ensure_member_cap(g_member_n + (uint32_t)arch->member_count)) {
        mrp_archive_close(arch);
        launcher_error_set(err, L_ERR_IO, "jjfbol_catalog", "member realloc failed", path);
        return L_ERR_IO;
    }
    for (mi = 0; mi < arch->member_count; mi++) {
        JjfbolMemberIndex *m = &g_members[g_member_n++];
        memset(m, 0, sizeof(*m));
        snprintf(m->name, sizeof(m->name), "%s", arch->members[mi].name);
        m->package_index = (uint16_t)g_pack_n;
        m->offset = arch->members[mi].offset;
        m->stored_size = arch->members[mi].stored_size;
    }

    /* Matching *.mrp.v (and also bare stem.v already counted separately). */
    snprintf(vpath, sizeof(vpath), "%s.v", path);
    ver = read_be32_file(vpath, &vok);
    if (vok) {
        pkg->version = ver;
        g_version_files++;
    }

    mrp_archive_close(arch); /* mandatory: do not keep archive body resident */
    g_pack_n++;
    return L_OK;
}

#ifdef _WIN32
static LauncherStatus scan_dir(const char *dir, LauncherError *err) {
    char pattern[JJFBOL_PATH_MAX];
    WIN32_FIND_DATAA fd;
    HANDLE h;

    if (!join2(pattern, sizeof(pattern), dir, "*")) {
        launcher_error_set(err, L_ERR_BOUNDS, "jjfbol_catalog", "pattern too long", dir);
        return L_ERR_BOUNDS;
    }
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "jjfbol_catalog", "jjfbol empty", dir);
        return L_ERR_NOT_FOUND;
    }
    do {
        char full[JJFBOL_PATH_MAX];
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!ends_with_ci(fd.cFileName, ".mrp")) continue;
        if (!join2(full, sizeof(full), dir, fd.cFileName)) continue;
        if (index_one_mrp(full, fd.cFileName, err) != L_OK) {
            FindClose(h);
            return err->code;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return L_OK;
}
#else
static LauncherStatus scan_dir(const char *dir, LauncherError *err) {
    DIR *d = opendir(dir);
    struct dirent *ent;
    if (!d) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "jjfbol_catalog", "jjfbol open failed", dir);
        return L_ERR_NOT_FOUND;
    }
    while ((ent = readdir(d)) != NULL) {
        char full[JJFBOL_PATH_MAX];
        if (!ends_with_ci(ent->d_name, ".mrp")) continue;
        if (!join2(full, sizeof(full), dir, ent->d_name)) continue;
        if (!path_is_file(full)) continue;
        if (index_one_mrp(full, ent->d_name, err) != L_OK) {
            closedir(d);
            return err->code;
        }
    }
    closedir(d);
    return L_OK;
}
#endif

#ifdef _WIN32
static void count_extra_version_files(const char *dir) {
    char pattern[JJFBOL_PATH_MAX];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    if (!join2(pattern, sizeof(pattern), dir, "*")) return;
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        /* Count downVersion.v and any .v not already counted as .mrp.v */
        if (strcmp(fd.cFileName, "downVersion.v") == 0) g_version_files++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
#else
static void count_extra_version_files(const char *dir) {
    char p[JJFBOL_PATH_MAX];
    if (join2(p, sizeof(p), dir, "downVersion.v") && path_is_file(p)) g_version_files++;
}
#endif

LauncherStatus jjfbol_catalog_init(const char *resource_root, LauncherError *err) {
    char jjfbol[JJFBOL_PATH_MAX];
    char down_path[JJFBOL_PATH_MAX];
    LauncherStatus st;
    int ok = 0;
    char details[160];

    launcher_error_clear(err);
    jjfbol_catalog_reset();
    if (!resource_root || !resource_root[0]) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "jjfbol_catalog", "null root", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (!join2(jjfbol, sizeof(jjfbol), resource_root, "gwy/jjfbol") || !path_is_dir(jjfbol)) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "jjfbol_catalog", "missing gwy/jjfbol",
                           resource_root);
        return L_ERR_NOT_FOUND;
    }
    snprintf(g_root, sizeof(g_root), "%s", jjfbol);

    st = scan_dir(jjfbol, err);
    if (st != L_OK) {
        jjfbol_catalog_reset();
        return st;
    }

    if (join2(down_path, sizeof(down_path), jjfbol, "downVersion")) {
        g_down_version = read_be32_file(down_path, &ok);
        if (ok) {
            /* raw BE as stored in file bytes */
            g_down_raw_be = g_down_version; /* already assembled as BE numeric value */
        }
    }
    count_extra_version_files(jjfbol);

    g_ready = 1;
    printf("[JJFBOL_CATALOG] root=%s\n", g_root);
    printf("[JJFBOL_CATALOG] packages=%u versions=%u\n", g_pack_n, g_version_files);
    printf("[JJFBOL_CATALOG] downVersion=%u raw=%08X\n", g_down_version, g_down_raw_be);
    fflush(stdout);

    snprintf(details, sizeof(details), "packages=%u versions=%u downVersion=%u", g_pack_n,
             g_version_files, g_down_version);
    product_runtime_progress_emit("jjfbol_catalog_ready", "JJFBOL_CATALOG_READY", details);
    return L_OK;
}

int jjfbol_catalog_ready(void) { return g_ready; }
const char *jjfbol_catalog_root(void) { return g_ready ? g_root : ""; }
uint32_t jjfbol_catalog_package_count(void) { return g_pack_n; }
uint32_t jjfbol_catalog_version_file_count(void) { return g_version_files; }
uint32_t jjfbol_catalog_down_version(void) { return g_down_version; }
uint32_t jjfbol_catalog_down_version_raw_be(void) { return g_down_raw_be; }

const JjfbolPackageIndex *jjfbol_catalog_package(uint32_t index) {
    if (!g_ready || index >= g_pack_n) return NULL;
    return &g_packs[index];
}

int jjfbol_catalog_find_package_stem(const char *stem, uint32_t *out_index) {
    uint32_t i;
    if (!stem || !g_ready) return 0;
    for (i = 0; i < g_pack_n; i++) {
        if (strcmp(g_packs[i].stem, stem) == 0) {
            if (out_index) *out_index = i;
            return 1;
        }
    }
    return 0;
}

static int ascii_eq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static JjfbolLookupKind lookup_impl(const char *member_name, int casefold,
                                    JjfbolLookupResult *out) {
    uint32_t i;
    JjfbolLookupResult local;
    memset(&local, 0, sizeof(local));
    if (!g_ready || !member_name || !member_name[0]) {
        if (out) *out = local;
        return JJFBOL_LOOKUP_MISS;
    }
    for (i = 0; i < g_member_n; i++) {
        int hit = casefold ? ascii_eq_ci(g_members[i].name, member_name)
                           : (strcmp(g_members[i].name, member_name) == 0);
        if (!hit) continue;
        if (local.hit_count < 16)
            local.hit_packages[local.hit_count] = g_members[i].package_index;
        local.hit_count++;
        if (local.hit_count == 1) local.package_index = g_members[i].package_index;
    }
    if (local.hit_count == 0)
        local.kind = JJFBOL_LOOKUP_MISS;
    else if (local.hit_count == 1)
        local.kind = JJFBOL_LOOKUP_UNIQUE;
    else {
        /* Deduplicate package indices for multi. */
        uint16_t uniq[16];
        uint16_t un = 0;
        uint16_t k, j;
        for (k = 0; k < local.hit_count && k < 16; k++) {
            int dup = 0;
            for (j = 0; j < un; j++) {
                if (uniq[j] == local.hit_packages[k]) {
                    dup = 1;
                    break;
                }
            }
            if (!dup) uniq[un++] = local.hit_packages[k];
        }
        if (un == 1) {
            local.kind = JJFBOL_LOOKUP_UNIQUE;
            local.package_index = uniq[0];
            local.hit_count = 1;
            local.hit_packages[0] = uniq[0];
        } else {
            local.kind = JJFBOL_LOOKUP_MULTI;
            local.hit_count = un;
            memcpy(local.hit_packages, uniq, un * sizeof(uniq[0]));
        }
    }
    if (out) *out = local;
    return local.kind;
}

JjfbolLookupKind jjfbol_catalog_lookup_exact(const char *member_name, JjfbolLookupResult *out) {
    return lookup_impl(member_name, 0, out);
}

JjfbolLookupKind jjfbol_catalog_lookup_casefold(const char *member_name, JjfbolLookupResult *out) {
    return lookup_impl(member_name, 1, out);
}

static MrpArchive *lru_get(uint32_t package_index, LauncherError *err) {
    int i, victim = 0;
    uint64_t oldest;
    LauncherStatus st;
    if (package_index >= g_pack_n) {
        launcher_error_set(err, L_ERR_BOUNDS, "jjfbol_catalog", "bad package index", NULL);
        return NULL;
    }
    g_lru_tick++;
    for (i = 0; i < JJFBOL_LRU_SLOTS; i++) {
        if (g_lru[i].in_use && g_lru[i].package_index == package_index && g_lru[i].archive) {
            g_lru[i].last_use = g_lru_tick;
            return g_lru[i].archive;
        }
    }
    oldest = UINT64_MAX;
    victim = 0;
    for (i = 0; i < JJFBOL_LRU_SLOTS; i++) {
        if (!g_lru[i].in_use) {
            victim = i;
            break;
        }
        if (g_lru[i].last_use < oldest) {
            oldest = g_lru[i].last_use;
            victim = i;
        }
    }
    if (g_lru[victim].archive) {
        mrp_archive_close(g_lru[victim].archive);
        g_lru[victim].archive = NULL;
    }
    st = mrp_archive_open(g_packs[package_index].path, &g_lru[victim].archive, err);
    if (st != L_OK) {
        g_lru[victim].in_use = 0;
        return NULL;
    }
    g_lru[victim].in_use = 1;
    g_lru[victim].package_index = package_index;
    g_lru[victim].last_use = g_lru_tick;
    return g_lru[victim].archive;
}

LauncherStatus jjfbol_catalog_decode_member(uint32_t package_index,
                                            const char *member_name,
                                            uint8_t *out_buf,
                                            size_t out_cap,
                                            size_t *out_len,
                                            LauncherError *err) {
    MrpArchive *arch;
    const MrpMember *mem = NULL;
    ByteBuffer bb;
    LauncherStatus st;

    launcher_error_clear(err);
    if (out_len) *out_len = 0;
    if (!g_ready || !member_name || !out_buf || out_cap == 0) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "jjfbol_catalog", "bad decode args",
                           NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    arch = lru_get(package_index, err);
    if (!arch) return err->code ? err->code : L_ERR_IO;
    st = mrp_archive_find_exact(arch, member_name, &mem, err);
    if (st != L_OK) return st;
    memset(&bb, 0, sizeof(bb));
    st = mrp_archive_decode_member(arch, mem, out_cap, &bb, err);
    if (st != L_OK) return st;
    if (bb.size > out_cap) {
        byte_buffer_free(&bb);
        launcher_error_set(err, L_ERR_BOUNDS, "jjfbol_catalog", "decoded too large", member_name);
        return L_ERR_BOUNDS;
    }
    memcpy(out_buf, bb.data, bb.size);
    if (out_len) *out_len = bb.size;
    byte_buffer_free(&bb);
    return L_OK;
}
