#include "./header/fileLib.h"
#include "./header/utils.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <zlib.h>

// mrc_open需要返回0表示失败
static struct rb_root filef_map = RB_ROOT;
static uint32_t filef_count = 0;

// mrc_findStart需要返回-1表示失败
static struct rb_root dirf_map = RB_ROOT;
static uint32_t dirf_count = 0;

static int jjfb_env_true(const char *name) {
    const char *v = getenv(name);
    if (!v || !*v) return 0;
    return strcmp(v, "0") != 0 && strcmp(v, "off") != 0 &&
           strcmp(v, "false") != 0 && strcmp(v, "FALSE") != 0;
}

static void jjfb_copy_normalized(char *out, uint32_t out_size, const char *in) {
    uint32_t i = 0;
    if (!out || out_size == 0) return;
    if (!in) in = "";
    while (*in && i + 1 < out_size) {
        char c = *in++;
        if (c == '\\') c = '/';
        out[i++] = c;
    }
    out[i] = '\0';
    while (out[0] == '.' && out[1] == '/') {
        memmove(out, out + 2, strlen(out + 2) + 1);
    }
}

static int jjfb_is_absolute(const char *p) {
    if (!p || !*p) return 0;
    if (p[0] == '/' || p[0] == '\\') return 1;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') return 1;
    return 0;
}

static void jjfb_join(char *out, uint32_t out_size, const char *base, const char *rel) {
    char b[JJFB_PATH_MAX];
    char r[JJFB_PATH_MAX];
    size_t n;
    jjfb_copy_normalized(b, sizeof(b), base);
    jjfb_copy_normalized(r, sizeof(r), rel);
    n = strlen(b);
    while (n > 0 && b[n - 1] == '/') b[--n] = '\0';
    while (*r == '/') memmove(r, r + 1, strlen(r));
    if (*b && *r) snprintf(out, out_size, "%s/%s", b, r);
    else if (*b) snprintf(out, out_size, "%s", b);
    else snprintf(out, out_size, "%s", r);
}

int my_path_exists(const char *filename) {
    struct stat s;
    return filename && *filename && stat(filename, &s) == 0;
}

/* Ensure parent directory of host path exists (for download-cache creates). */
static int jjfb_ensure_parent_dir(const char *host_path) {
    char dir[JJFB_PATH_MAX];
    char *slash;
    size_t n;
    if (!host_path || !*host_path) return 0;
    snprintf(dir, sizeof(dir), "%s", host_path);
    slash = strrchr(dir, '/');
#ifdef _WIN32
    {
        char *b = strrchr(dir, '\\');
        if (b && (!slash || b > slash)) slash = b;
    }
#endif
    if (!slash || slash == dir) return 1;
    *slash = '\0';
    n = strlen(dir);
    if (n == 0) return 1;
    if (my_path_exists(dir)) return 1;
#ifdef _WIN32
    /* Walk components and mkdir each (drive: prefix ok). */
    {
        char tmp[JJFB_PATH_MAX];
        char *p;
        snprintf(tmp, sizeof(tmp), "%s", dir);
        p = tmp;
        if (((tmp[0] >= 'A' && tmp[0] <= 'Z') || (tmp[0] >= 'a' && tmp[0] <= 'z')) &&
            tmp[1] == ':' && (tmp[2] == '/' || tmp[2] == '\\'))
            p = tmp + 3;
        while (*p) {
            if (*p == '/' || *p == '\\') {
                char save = *p;
                *p = '\0';
                if (strlen(tmp) > 0 && !my_path_exists(tmp)) {
                    if (mkdir(tmp) != 0 && !my_path_exists(tmp))
                        return 0;
                }
                *p = save;
            }
            p++;
        }
        if (!my_path_exists(tmp)) {
            if (mkdir(tmp) != 0 && !my_path_exists(tmp))
                return 0;
        }
    }
#else
    if (mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO) != 0 && !my_path_exists(dir))
        return 0;
#endif
    return 1;
}

int my_resolve_path(const char *filename, char *out, uint32_t out_size) {
    char guest[JJFB_PATH_MAX];
    char candidate[JJFB_PATH_MAX];
    char canonical[JJFB_PATH_MAX];
    const char *root = getenv("JJFB_MYTHROAD_ROOT");
    const char *gwy_root = getenv("JJFB_GWY_ROOT");
    const char *rel = NULL;
    int launcher = jjfb_env_true("JJFB_GWY_LAUNCHER_MODE");

    if (!out || out_size == 0) return 0;
    out[0] = '\0';
    jjfb_copy_normalized(guest, sizeof(guest), filename);
    if (!guest[0]) return 0;

    /* Some Mythroad guests prefix virtual paths with a slash.  These are not
     * host absolute paths; keep real / or C:/ host paths untouched. */
    if (guest[0] == '/' &&
        (strncmp(guest + 1, "gwy/", 4) == 0 ||
         strncmp(guest + 1, "mythroad/", 9) == 0 ||
         strncmp(guest + 1, "320x480/", 8) == 0 ||
         strncmp(guest + 1, "240x320/", 8) == 0)) {
        memmove(guest, guest + 1, strlen(guest));
    }

    if (jjfb_is_absolute(guest)) {
        snprintf(out, out_size, "%s", guest);
        return my_path_exists(out);
    }

    canonical[0] = '\0';

    /* Strip resolution folder prefix; resolve under JJFB_MYTHROAD_ROOT (320x480). */
    if (launcher && root && *root) {
        if (strncmp(guest, "mythroad/320x480/", sizeof("mythroad/320x480/") - 1) == 0) {
            rel = guest + (sizeof("mythroad/320x480/") - 1);
            jjfb_join(candidate, sizeof(candidate), root, rel);
            snprintf(canonical, sizeof(canonical), "%s", candidate);
            if (my_path_exists(candidate)) goto found;
        } else if (strncmp(guest, "320x480/", sizeof("320x480/") - 1) == 0) {
            rel = guest + (sizeof("320x480/") - 1);
            jjfb_join(candidate, sizeof(candidate), root, rel);
            snprintf(canonical, sizeof(canonical), "%s", candidate);
            if (my_path_exists(candidate)) goto found;
        } else if (strncmp(guest, "mythroad/240x320/", sizeof("mythroad/240x320/") - 1) == 0) {
            rel = guest + (sizeof("mythroad/240x320/") - 1);
            jjfb_join(candidate, sizeof(candidate), root, rel);
            snprintf(canonical, sizeof(canonical), "%s", candidate);
            if (my_path_exists(candidate)) goto found;
        } else if (strncmp(guest, "240x320/", sizeof("240x320/") - 1) == 0) {
            rel = guest + (sizeof("240x320/") - 1);
            jjfb_join(candidate, sizeof(candidate), root, rel);
            snprintf(canonical, sizeof(canonical), "%s", candidate);
            if (my_path_exists(candidate)) goto found;
        } else if (strncmp(guest, "mythroad/gwy/", sizeof("mythroad/gwy/") - 1) == 0) {
            rel = guest + (sizeof("mythroad/") - 1);
            jjfb_join(candidate, sizeof(candidate), root, rel);
            snprintf(canonical, sizeof(canonical), "%s", candidate);
            if (my_path_exists(candidate)) goto found;
        } else if (strncmp(guest, "gwy/", 4) == 0) {
            jjfb_join(candidate, sizeof(candidate), root, guest);
            snprintf(canonical, sizeof(canonical), "%s", candidate);
            if (my_path_exists(candidate)) goto found;
        } else if (strncmp(guest, "mythroad/", sizeof("mythroad/") - 1) == 0) {
            /* Generic Mythroad paths such as mythroad/sdk_key.dat belong to
             * the selected mythroad root (320x480) when that file exists there.
             * v50 handled mythroad/gwy/ specially but otherwise fell through to
             * the legacy process-CWD copy, so a stale 4-byte sdk_key.dat won. */
            rel = guest + (sizeof("mythroad/") - 1);
            jjfb_join(candidate, sizeof(candidate), root, rel);
            snprintf(canonical, sizeof(canonical), "%s", candidate);
            if (my_path_exists(candidate)) goto found;
        } else if (gwy_root && *gwy_root) {
            jjfb_join(candidate, sizeof(candidate), gwy_root, guest);
            snprintf(canonical, sizeof(canonical), "%s", candidate);
            if (my_path_exists(candidate)) goto found;
        }
    }

    if (my_path_exists(guest)) {
        snprintf(candidate, sizeof(candidate), "%s", guest);
        goto found;
    }

    if (root && *root) {
        if (strncmp(guest, "mythroad/320x480/", sizeof("mythroad/320x480/") - 1) == 0)
            rel = guest + (sizeof("mythroad/320x480/") - 1);
        else if (strncmp(guest, "320x480/", sizeof("320x480/") - 1) == 0)
            rel = guest + (sizeof("320x480/") - 1);
        else if (strncmp(guest, "mythroad/240x320/", sizeof("mythroad/240x320/") - 1) == 0)
            rel = guest + (sizeof("mythroad/240x320/") - 1);
        else if (strncmp(guest, "240x320/", sizeof("240x320/") - 1) == 0)
            rel = guest + (sizeof("240x320/") - 1);
        else if (strncmp(guest, "mythroad/gwy/", sizeof("mythroad/gwy/") - 1) == 0)
            rel = guest + (sizeof("mythroad/") - 1);
        else if (strncmp(guest, "gwy/", 4) == 0) rel = guest;
        else rel = guest;
        jjfb_join(candidate, sizeof(candidate), root, rel);
        if (!canonical[0]) snprintf(canonical, sizeof(canonical), "%s", candidate);
        if (my_path_exists(candidate)) goto found;
    }

    if (gwy_root && *gwy_root && strncmp(guest, "gwy/", 4) != 0 &&
        strncmp(guest, "mythroad/", 9) != 0) {
        jjfb_join(candidate, sizeof(candidate), gwy_root, guest);
        if (!canonical[0]) snprintf(canonical, sizeof(canonical), "%s", candidate);
        if (my_path_exists(candidate)) goto found;
    }

    if (canonical[0]) snprintf(out, out_size, "%s", canonical);
    else snprintf(out, out_size, "%s", guest);
    return 0;

found:
    snprintf(out, out_size, "%s", candidate);
    return 1;
}

typedef struct {
    int fh;
    int is_at;
    char guest[192];
    char host[JJFB_PATH_MAX];
} JjfbOpenFile;

static JjfbOpenFile *jjfb_ofile(int32_t f) {
    uIntMap *obj = uIntMap_search(&filef_map, f);
    return obj ? (JjfbOpenFile *)obj->data : NULL;
}

int32_t my_open(const char *filename, uint32_t mode) {
    int f;
    int new_mode = 0;
    char resolved[JJFB_PATH_MAX];
    int is_at_cache;
    JjfbOpenFile *meta;

    if (mode & MR_FILE_RDONLY) new_mode = O_RDONLY;
    if (mode & MR_FILE_WRONLY) new_mode = O_WRONLY;
    if (mode & MR_FILE_RDWR) new_mode = O_RDWR;
    if (mode & MR_FILE_CREATE) new_mode |= O_CREAT;

    is_at_cache = (filename && strchr(filename, '@') != NULL);

    /* Mythroad download/cache paths like "0@s0.map":
     * Do NOT auto-create on bare RDWR — empty cache still enters parse/alloc
     * with garbage sizes (0x8EB31FB0). Only honor explicit MR_FILE_CREATE.
     * getLen reports 0 for miss/empty so getLen+1 alloc stays tiny. */
    if (is_at_cache && (mode & MR_FILE_CREATE) && !(mode & MR_FILE_RDONLY)) {
        new_mode |= O_CREAT;
        if (!(new_mode & (O_RDONLY | O_WRONLY | O_RDWR)))
            new_mode |= O_RDWR;
    }

#ifdef _WIN32
    new_mode |= O_RAW;
#endif

    my_resolve_path(filename, resolved, sizeof(resolved));
    if (is_at_cache && (new_mode & O_CREAT))
        jjfb_ensure_parent_dir(resolved);

    f = open(resolved, new_mode, S_IRWXU | S_IRWXG | S_IRWXO);

    if (f == -1) {
        return 0;
    }

    meta = (JjfbOpenFile *)calloc(1, sizeof(JjfbOpenFile));
    if (!meta) {
        close(f);
        return 0;
    }
    meta->fh = f;
    meta->is_at = is_at_cache;
    if (filename)
        snprintf(meta->guest, sizeof(meta->guest), "%s", filename);
    snprintf(meta->host, sizeof(meta->host), "%s", resolved);

    filef_count++;
    {
        uIntMap *obj = malloc(sizeof(uIntMap));
        obj->key = filef_count;
        obj->data = meta;
        uIntMap_insert(&filef_map, obj);
    }
    return filef_count;
}

int32_t my_close(int32_t f) {
    uIntMap *obj = uIntMap_delete(&filef_map, f);
    JjfbOpenFile *meta;
    int fh;
    int is_at;
    char host[JJFB_PATH_MAX];
    char guest[192];

    if (obj == NULL) {
        return MR_FAILED;
    }
    if (f == filef_count) {
        filef_count--;
    }
    meta = (JjfbOpenFile *)obj->data;
    fh = meta->fh;
    is_at = meta->is_at;
    snprintf(host, sizeof(host), "%s", meta->host);
    snprintf(guest, sizeof(guest), "%s", meta->guest);
    free(meta);
    free(obj);
    if (close(fh) != 0) {
        return MR_FAILED;
    }
    /* Note: do NOT auto-delete empty @ caches here. Game may open RDWR,
     * alloc a tiny staging buffer, close, then reopen — deleting between
     * caused UC_ERR_EXCEPTION @0x2D92B0. Runner still strips empty 0@s0
     * at start of each session. */
    (void)is_at;
    (void)host;
    (void)guest;
    return MR_SUCCESS;
}

int32_t my_seek(int32_t f, int32_t pos, int method) {
    JjfbOpenFile *meta = jjfb_ofile(f);
    off_t ret;
    if (!meta) {
        return MR_FAILED;
    }
    ret = lseek(meta->fh, (off_t)pos, method);
    if (ret == -1) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

int32_t my_read(int32_t f, void *p, uint32_t l) {
    JjfbOpenFile *meta = jjfb_ofile(f);
    int32_t readnum;
    if (!meta) {
        return MR_FAILED;
    }
    readnum = read(meta->fh, p, (size_t)l);
    if (readnum == -1) {
        return MR_FAILED;
    }
    if (meta->is_at) {
        printf("[JJFB_FILEIO] read guest=\"%s\" fd=%d req=%u got=%d\n",
               meta->guest, f, l, readnum);
        fflush(stdout);
    }
    return readnum;
}

int32_t my_write(int32_t f, void *p, uint32_t l) {
    JjfbOpenFile *meta = jjfb_ofile(f);
    int32_t writenum;
    if (!meta) {
        return MR_FAILED;
    }
    writenum = write(meta->fh, p, (size_t)l);
    if (writenum == -1) {
        return MR_FAILED;
    }
    if (meta->is_at) {
        printf("[JJFB_FILEIO] write guest=\"%s\" fd=%d req=%u got=%d\n",
               meta->guest, f, l, writenum);
        fflush(stdout);
    }
    return writenum;
}

int32_t my_rename(const char *oldname, const char *newname) {
    char old_resolved[JJFB_PATH_MAX], new_resolved[JJFB_PATH_MAX];
    int ret;
    my_resolve_path(oldname, old_resolved, sizeof(old_resolved));
    my_resolve_path(newname, new_resolved, sizeof(new_resolved));
    ret = rename(old_resolved, new_resolved);
    if (ret != 0) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

int32_t my_remove(const char *filename) {
    char resolved[JJFB_PATH_MAX];
    int ret;
    my_resolve_path(filename, resolved, sizeof(resolved));
    ret = remove(resolved);
    if (ret != 0) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

int32_t my_getLen(const char *filename) {
    struct stat s1;
    char resolved[JJFB_PATH_MAX];
    int ret;
    int is_at = (filename && strchr(filename, '@') != NULL);
    my_resolve_path(filename, resolved, sizeof(resolved));
    ret = stat(resolved, &s1);
    if (ret != 0) {
        /* Missing @ download cache: return 0 (empty), not -1.
         * -1 made guests do alloc((-1)+4)=3 then a follow-up garbage size
         * (0x8EB31FB0) and never reach initNetwork. */
        if (is_at) {
            printf("[JJFB_FILEIO] getLen guest=\"%s\" miss_at_cache -> 0\n", filename);
            fflush(stdout);
            return 0;
        }
        return -1;
    }
    /* Empty @ download caches: same as missing — length 0, not -1. */
    if (is_at && s1.st_size == 0) {
        printf("[JJFB_FILEIO] getLen guest=\"%s\" empty_at_cache -> 0\n", filename);
        fflush(stdout);
        return 0;
    }
    return s1.st_size;
}

int32_t my_mkDir(const char *name) {
    int ret;
    char resolved[JJFB_PATH_MAX];
    my_resolve_path(name, resolved, sizeof(resolved));
    if (access(resolved, F_OK) == 0) {  //检测是否已存在
        goto ok;
    }
#ifndef _WIN32
    ret = mkdir(resolved, S_IRWXU | S_IRWXG | S_IRWXO);
#else
    ret = mkdir(resolved);
#endif
    if (ret != 0) {
        return MR_FAILED;
    }
ok:
    return MR_SUCCESS;
}

int32_t my_rmDir(const char *name) {
    char resolved[JJFB_PATH_MAX];
    int ret;
    my_resolve_path(name, resolved, sizeof(resolved));
    ret = rmdir(resolved);
    if (ret != 0) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

int32_t my_info(const char *filename) {
    struct stat s1;
    char resolved[JJFB_PATH_MAX];
    int ret;
    my_resolve_path(filename, resolved, sizeof(resolved));
    ret = stat(resolved, &s1);

    if (ret != 0) {
        return MR_IS_INVALID;
    }
    if (s1.st_mode & S_IFDIR) {
        return MR_IS_DIR;
    } else if (s1.st_mode & S_IFREG) {
        return MR_IS_FILE;
    }
    return MR_IS_INVALID;
}

int32_t my_opendir(const char *name) {
    char resolved[JJFB_PATH_MAX];
    my_resolve_path(name, resolved, sizeof(resolved));
    DIR *pDir = opendir(resolved);
    if (pDir != NULL) {
        dirf_count++;
        uIntMap *obj = malloc(sizeof(uIntMap));
        obj->key = dirf_count;
        obj->data = (void *)pDir;
        uIntMap_insert(&dirf_map, obj);
        return dirf_count;
    }
    return MR_FAILED;
}

char *my_readdir(int32_t f) {
    uIntMap *obj = uIntMap_search(&dirf_map, f);
    if (obj == NULL) {
        return NULL;
    }
    struct dirent *pDt = readdir((DIR *)obj->data); // 手册说返回的内存可能是静态分配的，不要尝试free()
    if (pDt != NULL) {
        return pDt->d_name;
    }
    return NULL;
}

int32_t my_closedir(int32_t f) {
    uIntMap *obj = uIntMap_delete(&dirf_map, f);
    if (obj == NULL) {
        return MR_FAILED;
    }
    if (f == dirf_count) {
        dirf_count--;
    }
    DIR *pDir = (DIR *)obj->data;
    free(obj);
    if (closedir(pDir) != 0) {
        return MR_FAILED;
    }
    return MR_SUCCESS;
}

void writeFile(const char *filename, void *data, uint32 length) {
    int fh = my_open(filename, MR_FILE_CREATE | MR_FILE_RDWR);
    int32_t wLen = 0;
    char *ptr = (char *)data;
    do {
        ptr += wLen;
        wLen = my_write(fh, ptr, length < 1000 ? length : 1000);
        if (wLen == MR_FAILED) {
            break;
        }
        length -= wLen;
    } while (length > 0);
    my_close(fh);
}

char *readFile(const char *filename) {
    int32_t len = my_getLen(filename);
    char *p = malloc(len);
    if (p == NULL) {
        return NULL;
    }
    int32_t fh = my_open(filename, MR_FILE_RDONLY);
    if (fh) {
        int32_t rLen = 0;
        char *ptr = p;
        do {
            ptr += rLen;
            rLen = my_read(fh, ptr, len);
            if (rLen == MR_FAILED) {
                free(p);
                return NULL;
            }
            len -= rLen;
        } while (len > 0);
        my_close(fh);
        return p;
    }
    return NULL;
}
