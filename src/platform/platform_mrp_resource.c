#include "gwy_launcher/platform_mrp_resource.h"

#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/jjfbol_catalog.h"
#include "gwy_launcher/jjfbol_scope.h"
#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/package_metadata.h"
#include "gwy_launcher/platform_memory_ops.h"
#include "gwy_launcher/product_runtime_progress.h"
#include "gwy_launcher/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define POSTMATCH_MAX 256u
#define NAME_MAX 255u
#define PENDING_MAX 128u

typedef struct {
    uint32_t package_id;
    uint64_t package_generation;
    char member_name[NAME_MAX + 1];
} CompletedResourceKey;

typedef enum {
    PENDING_FREE = 0,
    PENDING_READY = 1,
    PENDING_RESERVED = 2
} PendingState;

typedef struct {
    uint64_t sequence;
    PendingState state;
    char package_name[128];
    char member_name[256];
    uint32_t decoded_pixels;
    uint32_t decoded_bytes;
    uint16_t width;
    uint16_t height;
    uint32_t guest_handle;
    uint32_t lookup_lr;
} PendingBitmapConstruct;

static int g_en_known;
static int g_en;
static void *g_uc;
static int g_entry_hook_armed;
static int g_maps_ok;
static uint32_t g_pixel_slot;
static uint32_t g_postmatch_n;
static CompletedResourceKey g_done_keys[POSTMATCH_MAX];
static PendingBitmapConstruct g_pending[PENDING_MAX];
static uint32_t g_pending_n;
static uint64_t g_pending_seq;
static uint32_t g_last_hit_package_id;
static char g_last_package_name[128];

/* 0x304BF0 entry ABI snapshot (frame capture only). */
static int g_entry_valid;
static uint32_t g_entry_sp, g_entry_lr, g_entry_r9;
static uint32_t g_entry_r4, g_entry_r5, g_entry_r6, g_entry_r7;
static uint32_t g_entry_r8, g_entry_r10, g_entry_r11;
static uint32_t g_entry_r0, g_entry_r1, g_entry_r2, g_entry_r3;

#ifdef GWY_HAVE_UNICORN
static uc_hook g_entry_hook;
static uint8_t g_pixel_host[PLATFORM_MRP_PIXEL_MAP_SIZE];
static uint8_t g_handle_host[PLATFORM_MRP_HANDLE_MAP_SIZE];
#endif

static int env_explicit_zero(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '0' && e[1] == '\0';
}

static int env_explicit_one(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

int platform_mrp_resource_enabled(void) {
    if (!g_en_known) {
        if (env_explicit_zero("JJFB_PLATFORM_MRP_RESOURCE"))
            g_en = 0;
        else if (env_explicit_one("JJFB_PLATFORM_MRP_RESOURCE"))
            g_en = 1;
        else
            g_en = 1; /* default ON — Task 12 Phase C product surface */
        g_en_known = 1;
    }
    return g_en;
}

void platform_mrp_resource_pending_enqueue(const char *package_name, const char *member_name,
                                          uint32_t decoded_pixels, uint32_t decoded_bytes,
                                          uint16_t w, uint16_t h, uint32_t guest_handle,
                                          uint32_t lookup_lr) {
    uint32_t i;
    PendingBitmapConstruct *slot = NULL;
    if (decoded_bytes < 16u || !decoded_pixels) return;
    for (i = 0; i < PENDING_MAX; i++) {
        if (g_pending[i].state == PENDING_FREE) {
            slot = &g_pending[i];
            break;
        }
    }
    if (!slot) {
        /* Drop oldest READY (not RESERVED). */
        uint64_t oldest = UINT64_MAX;
        int victim = -1;
        for (i = 0; i < PENDING_MAX; i++) {
            if (g_pending[i].state == PENDING_READY && g_pending[i].sequence < oldest) {
                oldest = g_pending[i].sequence;
                victim = (int)i;
            }
        }
        if (victim < 0) return;
        slot = &g_pending[victim];
        memset(slot, 0, sizeof(*slot));
    }
    g_pending_seq++;
    slot->sequence = g_pending_seq;
    slot->state = PENDING_READY;
    snprintf(slot->package_name, sizeof(slot->package_name), "%s",
             package_name ? package_name : "");
    snprintf(slot->member_name, sizeof(slot->member_name), "%s", member_name ? member_name : "");
    slot->decoded_pixels = decoded_pixels;
    slot->decoded_bytes = decoded_bytes;
    slot->width = w;
    slot->height = h;
    slot->guest_handle = guest_handle;
    slot->lookup_lr = lookup_lr;
    g_pending_n++;
    {
        char det[192];
        snprintf(det, sizeof(det), "seq=%llu bytes=0x%X member=%s depth=%u",
                 (unsigned long long)slot->sequence, decoded_bytes,
                 member_name ? member_name : "?", platform_mrp_resource_pending_depth());
        product_runtime_progress_emit("pending_bitmap_enqueue", "fifo", det);
    }
}

uint64_t platform_mrp_resource_pending_reserve(uint32_t bytes, uint32_t *out_guest_pixels) {
    uint32_t i;
    int best = -1;
    uint64_t best_seq = UINT64_MAX;
    if (out_guest_pixels) *out_guest_pixels = 0;
    if (bytes < 16u) return 0;
    for (i = 0; i < PENDING_MAX; i++) {
        if (g_pending[i].state == PENDING_READY && g_pending[i].decoded_bytes == bytes &&
            g_pending[i].sequence < best_seq) {
            best_seq = g_pending[i].sequence;
            best = (int)i;
        }
    }
    if (best < 0) return 0;
    g_pending[best].state = PENDING_RESERVED;
    if (out_guest_pixels) *out_guest_pixels = g_pending[best].decoded_pixels;
    return g_pending[best].sequence;
}

int platform_mrp_resource_pending_commit(uint64_t pending_id) {
    uint32_t i;
    if (!pending_id) return 0;
    for (i = 0; i < PENDING_MAX; i++) {
        if (g_pending[i].sequence == pending_id && g_pending[i].state == PENDING_RESERVED) {
            memset(&g_pending[i], 0, sizeof(g_pending[i]));
            product_runtime_progress_emit("pending_bitmap_commit", "fifo", "ok");
            return 1;
        }
    }
    return 0;
}

int platform_mrp_resource_pending_release(uint64_t pending_id) {
    uint32_t i;
    if (!pending_id) return 0;
    for (i = 0; i < PENDING_MAX; i++) {
        if (g_pending[i].sequence == pending_id && g_pending[i].state == PENDING_RESERVED) {
            g_pending[i].state = PENDING_READY; /* allow retry */
            product_runtime_progress_emit("pending_bitmap_release", "fifo", "retry");
            return 1;
        }
    }
    return 0;
}

uint32_t platform_mrp_resource_pending_depth(void) {
    uint32_t i, n = 0;
    for (i = 0; i < PENDING_MAX; i++) {
        if (g_pending[i].state != PENDING_FREE) n++;
    }
    return n;
}

void platform_mrp_resource_note_pixels(uint32_t bytes, uint32_t guest_pixels, uint16_t w,
                                      uint16_t h) {
    platform_mrp_resource_note_pixels_ex(bytes, guest_pixels, 0, w, h);
}

void platform_mrp_resource_note_pixels_ex(uint32_t bytes, uint32_t guest_pixels,
                                         uint32_t handle_guest, uint16_t w, uint16_t h) {
    platform_mrp_resource_pending_enqueue(g_last_package_name, "", guest_pixels, bytes, w, h,
                                          handle_guest, 0);
}

uint32_t platform_mrp_resource_pixels_by_bytes(uint32_t bytes) {
    uint32_t i;
    uint64_t best_seq = UINT64_MAX;
    uint32_t px = 0;
    if (bytes < 16u) return 0;
    for (i = 0; i < PENDING_MAX; i++) {
        if ((g_pending[i].state == PENDING_READY || g_pending[i].state == PENDING_RESERVED) &&
            g_pending[i].decoded_bytes == bytes && g_pending[i].sequence < best_seq) {
            best_seq = g_pending[i].sequence;
            px = g_pending[i].decoded_pixels;
        }
    }
    return px;
}

int platform_mrp_resource_bind_10134_pixels(void *uc, uint32_t bytes, uint32_t user_pixels) {
    (void)uc;
    (void)bytes;
    (void)user_pixels;
    printf("[PLATFORM_MRP_RES] bind_10134 FORBIDDEN note=guest_owns_handle_pixels "
           "evidence=TASK16\n");
    fflush(stdout);
    return 0;
}

void platform_mrp_resource_reset(void) {
    g_en_known = 0;
    g_en = 0;
    g_uc = NULL;
    g_entry_valid = 0;
    g_pixel_slot = 0;
    g_postmatch_n = 0;
    g_pending_n = 0;
    g_pending_seq = 0;
    g_last_hit_package_id = 0;
    g_last_package_name[0] = 0;
    memset(g_done_keys, 0, sizeof(g_done_keys));
    memset(g_pending, 0, sizeof(g_pending));
    jjfbol_scope_bump_generation();
    /* Keep unicorn hooks / maps for process lifetime. */
}

void platform_mrp_resource_bind_uc(void *uc) {
    if (uc) g_uc = uc;
}

uint32_t platform_mrp_resource_postmatch_count(void) { return g_postmatch_n; }

static int name_already_done(const char *name, uint32_t package_id) {
    uint32_t i;
    uint64_t gen = jjfbol_scope_generation();
    if (!name || !name[0]) return 1;
    for (i = 0; i < g_postmatch_n && i < POSTMATCH_MAX; i++) {
        if (g_done_keys[i].package_id == package_id &&
            g_done_keys[i].package_generation == gen &&
            strcmp(g_done_keys[i].member_name, name) == 0)
            return 1;
    }
    return 0;
}

static void mark_done(const char *name, uint32_t package_id) {
    if (!name || !name[0] || g_postmatch_n >= POSTMATCH_MAX) return;
    g_done_keys[g_postmatch_n].package_id = package_id;
    g_done_keys[g_postmatch_n].package_generation = jjfbol_scope_generation();
    snprintf(g_done_keys[g_postmatch_n].member_name, sizeof(g_done_keys[0].member_name), "%s",
             name);
    g_postmatch_n++;
}

static int looks_like_member_name(const char *s) {
    size_t n;
    if (!s || !s[0]) return 0;
    n = strlen(s);
    if (n < 5 || n > NAME_MAX) return 0;
    if (strchr(s, '!')) return 1;
    if (n >= 4 && (strcmp(s + n - 4, ".bmp") == 0 || strcmp(s + n - 4, ".BMP") == 0 ||
                   strcmp(s + n - 4, ".gif") == 0 || strcmp(s + n - 4, ".GIF") == 0))
        return 1;
    return 0;
}

static int parse_name_wh(const char *name, int *out_w, int *out_h) {
    const char *p;
    int w = 0, h = 0;
    if (!name || !out_w || !out_h) return 0;
    p = strchr(name, '!');
    if (!p) return 0;
    w = atoi(p + 1);
    p = strchr(p + 1, '!');
    if (!p) return 0;
    h = atoi(p + 1);
    if (w <= 0 || h <= 0 || w > 240 || h > 320) return 0;
    *out_w = w;
    *out_h = h;
    return 1;
}

static int read_guest_cstr(void *uc, uint32_t va, char *out, size_t cap) {
    size_t i;
    if (!uc || !va || !out || cap < 2) return 0;
    out[0] = 0;
    for (i = 0; i + 1 < cap; i++) {
        uint8_t c = 0;
        if (!guest_memory_uc_peek((struct uc_struct *)uc, va + (uint32_t)i, &c, 1)) break;
        if (!c) {
            out[i] = 0;
            return 1;
        }
        out[i] = (char)c;
    }
    out[cap - 1] = 0;
    return out[0] != 0;
}

static const char *resolve_package_host_path(char *buf, size_t buf_sz) {
    const char *env;
    const GwyPackageMetadata *meta;
    env = getenv("JJFB_REAL_MRP_PATH");
    if (env && env[0]) {
        snprintf(buf, buf_sz, "%s", env);
        return buf;
    }
    meta = gwy_package_registry_active_metadata();
    if (meta && meta->valid && meta->archive_path[0]) {
        snprintf(buf, buf_sz, "%s", meta->archive_path);
        return buf;
    }
    {
        const char *rr = getenv("GWY_RESOURCE_ROOT");
        if (rr && rr[0]) {
            snprintf(buf, buf_sz, "%s/gwy/jjfb.mrp", rr);
            return buf;
        }
    }
    snprintf(buf, buf_sz, "game_files/mythroad/240x320/gwy/jjfb.mrp");
    return buf;
}

/*
 * Lookup order (Phase 3):
 * 1. active jjfbol package exact
 * 2. main jjfb.mrp exact
 * 3. catalog unique exact
 * 4. catalog unique case-fold
 * 5/6. multi-hit / miss → do not complete (return 0 paths)
 */
#define MRP_PATH_CANDIDATES 4
static int resolve_member_package(const char *member_name, char *out_path, size_t out_cap,
                                  uint32_t *out_package_id) {
    char primary[1024];
    const char *active;
    JjfbolLookupResult lr;
    uint32_t pkg_idx = 0;
    const JjfbolPackageIndex *pi;

    if (out_package_id) *out_package_id = 0;
    if (!member_name || !out_path || out_cap == 0) return 0;
    out_path[0] = 0;
    resolve_package_host_path(primary, sizeof(primary));

    /* 1. active package exact */
    active = jjfbol_scope_active_package();
    if (active && jjfbol_catalog_ready() &&
        jjfbol_catalog_find_package_stem(active, &pkg_idx)) {
        pi = jjfbol_catalog_package(pkg_idx);
        if (pi) {
            MrpArchive *a = NULL;
            const MrpMember *m = NULL;
            LauncherError e;
            if (mrp_archive_open(pi->path, &a, &e) == L_OK && a) {
                if (mrp_archive_find_exact(a, member_name, &m, &e) == L_OK && m) {
                    snprintf(out_path, out_cap, "%s", pi->path);
                    if (out_package_id) *out_package_id = pkg_idx + 1u;
                    mrp_archive_close(a);
                    return 1;
                }
                mrp_archive_close(a);
            }
        }
    }

    /* 2. main jjfb.mrp exact */
    {
        MrpArchive *a = NULL;
        const MrpMember *m = NULL;
        LauncherError e;
        if (mrp_archive_open(primary, &a, &e) == L_OK && a) {
            if (mrp_archive_find_exact(a, member_name, &m, &e) == L_OK && m) {
                snprintf(out_path, out_cap, "%s", primary);
                if (out_package_id) *out_package_id = 0; /* main pack id 0 */
                mrp_archive_close(a);
                return 1;
            }
            mrp_archive_close(a);
        }
    }

    if (!jjfbol_catalog_ready()) return 0;

    /* 3. catalog unique exact */
    if (jjfbol_catalog_lookup_exact(member_name, &lr) == JJFBOL_LOOKUP_UNIQUE) {
        pi = jjfbol_catalog_package(lr.package_index);
        if (pi) {
            snprintf(out_path, out_cap, "%s", pi->path);
            if (out_package_id) *out_package_id = lr.package_index + 1u;
            return 1;
        }
    } else if (lr.kind == JJFBOL_LOOKUP_MULTI) {
        uint16_t i;
        printf("[PLATFORM_MRP_RES] multi_hit name=\"%s\" packs=%u", member_name, lr.hit_count);
        for (i = 0; i < lr.hit_count && i < 16; i++) {
            pi = jjfbol_catalog_package(lr.hit_packages[i]);
            printf(" %s", pi ? pi->stem : "?");
        }
        printf(" note=no_complete evidence=OBSERVED\n");
        fflush(stdout);
        return 0;
    }

    /* 4. catalog unique case-fold (only after exact unique failed) */
    if (jjfbol_catalog_lookup_casefold(member_name, &lr) == JJFBOL_LOOKUP_UNIQUE) {
        pi = jjfbol_catalog_package(lr.package_index);
        if (pi) {
            snprintf(out_path, out_cap, "%s", pi->path);
            if (out_package_id) *out_package_id = lr.package_index + 1u;
            return 1;
        }
    } else if (lr.kind == JJFBOL_LOOKUP_MULTI) {
        printf("[PLATFORM_MRP_RES] multi_hit_casefold name=\"%s\" hits=%u note=no_complete "
               "evidence=OBSERVED\n",
               member_name, lr.hit_count);
        fflush(stdout);
        return 0;
    }

    /* 6. miss */
    return 0;
}

#ifdef GWY_HAVE_UNICORN
static int ensure_guest_maps(uc_engine *uc) {
    uc_err ue;
    if (!uc) return 0;
    if (g_maps_ok) return 1;
    ue = uc_mem_map_ptr(uc, PLATFORM_MRP_PIXEL_BASE, PLATFORM_MRP_PIXEL_MAP_SIZE, UC_PROT_ALL,
                        g_pixel_host);
    if (ue != UC_ERR_OK) {
        ue = uc_mem_map(uc, PLATFORM_MRP_PIXEL_BASE, PLATFORM_MRP_PIXEL_MAP_SIZE, UC_PROT_ALL);
        if (ue != UC_ERR_OK) {
            printf("[PLATFORM_MRP_RES] map_fail pixel base=0x%X uc_err=%u evidence=OBSERVED\n",
                   PLATFORM_MRP_PIXEL_BASE, (unsigned)ue);
            fflush(stdout);
            return 0;
        }
    }
    ue = uc_mem_map_ptr(uc, PLATFORM_MRP_HANDLE_BASE, PLATFORM_MRP_HANDLE_MAP_SIZE, UC_PROT_ALL,
                        g_handle_host);
    if (ue != UC_ERR_OK) {
        ue = uc_mem_map(uc, PLATFORM_MRP_HANDLE_BASE, PLATFORM_MRP_HANDLE_MAP_SIZE, UC_PROT_ALL);
        if (ue != UC_ERR_OK) {
            printf("[PLATFORM_MRP_RES] map_fail handle base=0x%X uc_err=%u evidence=OBSERVED\n",
                   PLATFORM_MRP_HANDLE_BASE, (unsigned)ue);
            fflush(stdout);
            return 0;
        }
    }
    g_maps_ok = 1;
    printf("[PLATFORM_MRP_RES] maps_ok pixel=0x%X handle=0x%X evidence=OBSERVED\n",
           PLATFORM_MRP_PIXEL_BASE, PLATFORM_MRP_HANDLE_BASE);
    fflush(stdout);
    return 1;
}

static int try_304bf0_entry_complete(uc_engine *uc);

static void on_lookup_entry(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)address;
    (void)size;
    (void)user_data;
    if (!platform_mrp_resource_enabled()) return;
    uc_reg_read(uc, UC_ARM_REG_SP, &g_entry_sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &g_entry_lr);
    uc_reg_read(uc, UC_ARM_REG_R9, &g_entry_r9);
    uc_reg_read(uc, UC_ARM_REG_R0, &g_entry_r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &g_entry_r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &g_entry_r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &g_entry_r3);
    uc_reg_read(uc, UC_ARM_REG_R4, &g_entry_r4);
    uc_reg_read(uc, UC_ARM_REG_R5, &g_entry_r5);
    uc_reg_read(uc, UC_ARM_REG_R6, &g_entry_r6);
    uc_reg_read(uc, UC_ARM_REG_R7, &g_entry_r7);
    uc_reg_read(uc, UC_ARM_REG_R8, &g_entry_r8);
    uc_reg_read(uc, UC_ARM_REG_R10, &g_entry_r10);
    uc_reg_read(uc, UC_ARM_REG_R11, &g_entry_r11);
    g_entry_valid = 1;
    /*
     * V75: guest eventually opens default2 after jjfb miss. With chrome skips,
     * that open never happens and the wrong-pack scan + R9 storm stalls leave.
     * Exact archive hit (primary or jjfbol/default2) → complete 304BF0 here.
     */
    (void)try_304bf0_entry_complete(uc);
}
#endif

int platform_mrp_resource_load_host(const char *package_host_path, const char *member_name,
                                    GwyMrpResourceResult *out, uint8_t **decoded_out,
                                    size_t *decoded_len_out) {
    MrpArchive *arch = NULL;
    const MrpMember *mem = NULL;
    ByteBuffer bb;
    LauncherError err;
    LauncherStatus st;
    uint8_t digest[32];
    int w = 0, h = 0;
    char path_buf[512];

    if (out) memset(out, 0, sizeof(*out));
    if (decoded_out) *decoded_out = NULL;
    if (decoded_len_out) *decoded_len_out = 0;
    if (!member_name || !member_name[0]) return 0;

    if (!package_host_path || !package_host_path[0])
        package_host_path = resolve_package_host_path(path_buf, sizeof(path_buf));

    st = mrp_archive_open(package_host_path, &arch, &err);
    if (st != L_OK || !arch) return 0;
    st = mrp_archive_find_exact(arch, member_name, &mem, &err);
    if (st != L_OK || !mem) {
        mrp_archive_close(arch);
        return 0;
    }
    byte_buffer_init(&bb);
    st = mrp_archive_decode_member(arch, mem, 1024u * 1024u, &bb, &err);
    if (st != L_OK || !bb.data || bb.size == 0 || bb.size > (240u * 320u * 2u)) {
        byte_buffer_free(&bb);
        mrp_archive_close(arch);
        return 0;
    }
    (void)parse_name_wh(member_name, &w, &h);
    if (w <= 0 || h <= 0) {
        w = 11;
        h = 11;
    }
    gwy_sha256(bb.data, bb.size, digest);
    if (out) {
        out->status = 0;
        out->decoded_size = (uint32_t)bb.size;
        out->stored_size = mem->stored_size;
        out->member_offset = mem->offset;
        out->width = (uint16_t)w;
        out->height = (uint16_t)h;
        gwy_sha256_hex(digest, out->sha256_hex);
    }
    if (decoded_out && decoded_len_out) {
        *decoded_out = (uint8_t *)malloc(bb.size);
        if (*decoded_out) {
            memcpy(*decoded_out, bb.data, bb.size);
            *decoded_len_out = bb.size;
        }
    }
    byte_buffer_free(&bb);
    mrp_archive_close(arch);
    return 1;
}

int platform_mrp_resource_load(void *uc, const GwyMrpResourceRequest *request,
                               GwyMrpResourceResult *result) {
    char name[NAME_MAX + 1];
    char path_buf[1024];
    const char *path;
    MrpArchive *arch = NULL;
    const MrpMember *mem = NULL;
    ByteBuffer bb;
    LauncherError err;
    LauncherStatus st;
    uint8_t digest[32];
    uint8_t stub[0x20];
    int w = 0, h = 0;
    uint32_t slot_va, handle_va, sz, px;

    if (result) memset(result, 0, sizeof(*result));
    if (!uc || !request) return 0;

    memset(name, 0, sizeof(name));
    if (request->name[0])
        snprintf(name, sizeof(name), "%s", request->name);
    else if (request->guest_name_ptr)
        (void)read_guest_cstr(uc, request->guest_name_ptr, name, sizeof(name));
    if (!name[0] || !looks_like_member_name(name)) return 0;

    {
        char path_resolved[1024];
        uint32_t pkg_id = 0;
        LauncherError e2;
        if (!resolve_member_package(name, path_resolved, sizeof(path_resolved), &pkg_id)) {
            return 0;
        }
        if (name_already_done(name, pkg_id)) return 0;
        g_last_hit_package_id = pkg_id;
        {
            const char *slash = path_resolved;
            const char *base = path_resolved;
            while (*slash) {
                if (*slash == '/' || *slash == '\\') base = slash + 1;
                slash++;
            }
            snprintf(g_last_package_name, sizeof(g_last_package_name), "%s", base);
        }
        arch = NULL;
        mem = NULL;
        path = path_buf;
        snprintf(path_buf, sizeof(path_buf), "%s", path_resolved);
        memset(&e2, 0, sizeof(e2));
        st = mrp_archive_open(path_buf, &arch, &e2);
        if (st != L_OK || !arch) return 0;
        st = mrp_archive_find_exact(arch, name, &mem, &e2);
        if (st != L_OK || !mem) {
            /* Try casefold find on same pack for catalog casefold hits. */
            st = mrp_archive_find_casefold(arch, name, &mem, &e2);
            if (st != L_OK || !mem) {
                mrp_archive_close(arch);
                return 0;
            }
        }
        if (pkg_id != 0) {
            printf("[PLATFORM_MRP_RES] catalog_or_active name=\"%s\" path=%s pkg_id=%u "
                   "evidence=OBSERVED\n",
                   name, path, pkg_id);
            fflush(stdout);
        }
    }

    memset(&err, 0, sizeof(err));

#ifdef GWY_HAVE_UNICORN
    if (!ensure_guest_maps((uc_engine *)uc)) {
        mrp_archive_close(arch);
        return 0;
    }
#endif

    byte_buffer_init(&bb);
    st = mrp_archive_decode_member(arch, mem, 1024u * 1024u, &bb, &err);
    if (st != L_OK || !bb.data || bb.size == 0 || bb.size > (240u * 320u * 2u)) {
        printf("[PLATFORM_MRP_RES] decode_fail name=\"%s\" status=%d size=%u evidence=OBSERVED\n",
               name, (int)st, (unsigned)bb.size);
        fflush(stdout);
        byte_buffer_free(&bb);
        mrp_archive_close(arch);
        return 0;
    }
    (void)parse_name_wh(name, &w, &h);
    if (w <= 0 || h <= 0) {
        w = 11;
        h = 11;
    }
    gwy_sha256(bb.data, bb.size, digest);

    /* Decode into size→pixels cache for 0x10134. Do NOT install a freeable pixel
     * pointer into the handle: guest @0x3045E4 calls DSM mr_free(old) before
     * adopting the 10134 buffer; a host-synthesized ptr trips mr_free invalid.
     * Handle keeps size/wh/flag; pixels stay 0 until 10134 returns. */
    sz = (uint32_t)bb.size;
    px = 0;
    {
        uint32_t need =
            (((uint32_t)bb.size + 0x1FFu) & ~0x1FFu);
        if (need < 0x200u) need = 0x200u;
        slot_va = PLATFORM_MRP_PIXEL_BASE + g_pixel_slot;
        if (slot_va + need > PLATFORM_MRP_PIXEL_BASE + PLATFORM_MRP_PIXEL_MAP_SIZE) {
            slot_va = PLATFORM_MRP_PIXEL_BASE;
            g_pixel_slot = 0;
        }
        g_pixel_slot += need;
        if (!guest_memory_uc_poke((struct uc_struct *)uc, slot_va, bb.data, (int)bb.size)) {
            printf("[PLATFORM_MRP_RES] poke_fail pixels slot=0x%X size=%u evidence=OBSERVED\n",
                   slot_va, (unsigned)bb.size);
            fflush(stdout);
            byte_buffer_free(&bb);
            mrp_archive_close(arch);
            return 0;
        }
        px = slot_va; /* cache source only — not a mallocExt user ptr */
    }

    handle_va = PLATFORM_MRP_HANDLE_BASE + 0x40u + (g_postmatch_n % 6u) * 0x40u;
    /* Prefer caller object when entry captured r3 as out-object. */
    if (g_entry_valid && g_entry_r3 >= 0x1000u) handle_va = g_entry_r3;

    memset(stub, 0, sizeof(stub));
    memcpy(stub + 0, &sz, 4);
    /* pixels field intentionally 0 — 10134 supplies mallocExt buffer. */
    stub[8] = (uint8_t)(w & 0xFF);
    stub[9] = (uint8_t)((w >> 8) & 0xFF);
    stub[10] = (uint8_t)(h & 0xFF);
    stub[11] = (uint8_t)((h >> 8) & 0xFF);
    stub[16] = 1;
    (void)guest_memory_uc_poke((struct uc_struct *)uc, handle_va, stub, 0x14);
    /* Do not poke entry_r2 with map VA (not freeable). */

    if (result) {
        result->status = 0;
        result->guest_data = px;
        result->decoded_size = sz;
        result->stored_size = mem->stored_size;
        result->member_offset = mem->offset;
        result->handle_guest = handle_va;
        result->width = (uint16_t)w;
        result->height = (uint16_t)h;
        gwy_sha256_hex(digest, result->sha256_hex);
    }

    mark_done(name, g_last_hit_package_id);
    platform_mrp_resource_pending_enqueue(g_last_package_name, name, px, sz, (uint16_t)w,
                                         (uint16_t)h, handle_va,
                                         g_entry_valid ? g_entry_lr : 0);
    printf("[PLATFORM_MRP_RES] loaded name=\"%s\" offset=%u stored=%u decoded=%u w=%d h=%d "
           "sha256=%s handle=0x%X cache_pixels=0x%X note=handle_pixels_deferred_to_10134 "
           "evidence=OBSERVED\n",
           name, mem->offset, mem->stored_size, sz, w, h, result ? result->sha256_hex : "?",
           handle_va, px);
    fflush(stdout);
    product_runtime_progress_emit("platform_mrp_resource", "member_loaded", name);

    byte_buffer_free(&bb);
    mrp_archive_close(arch);
    return 1;
}

#ifdef GWY_HAVE_UNICORN
static int restore_304bf0_ok(uc_engine *uc, const char *name, const GwyMrpResourceResult *res,
                             const char *via) {
    uint32_t status = 0, ret_pc;
    if (!uc || !g_entry_valid || !g_entry_lr || !g_entry_sp) return 0;
    module_r9_switch_clear_dsm_return_side_stack();
    (void)module_r9_switch_cancel_dsm_helper_blx(uc, PLATFORM_STRCMP_DSM_BODY_VA, 0);
    uc_reg_write(uc, UC_ARM_REG_SP, &g_entry_sp);
    uc_reg_write(uc, UC_ARM_REG_R4, &g_entry_r4);
    uc_reg_write(uc, UC_ARM_REG_R5, &g_entry_r5);
    uc_reg_write(uc, UC_ARM_REG_R6, &g_entry_r6);
    uc_reg_write(uc, UC_ARM_REG_R7, &g_entry_r7);
    uc_reg_write(uc, UC_ARM_REG_R8, &g_entry_r8);
    if (g_entry_r9) uc_reg_write(uc, UC_ARM_REG_R9, &g_entry_r9);
    uc_reg_write(uc, UC_ARM_REG_R10, &g_entry_r10);
    uc_reg_write(uc, UC_ARM_REG_R11, &g_entry_r11);
    uc_reg_write(uc, UC_ARM_REG_R0, &status);
    ret_pc = g_entry_lr | 1u;
    uc_reg_write(uc, UC_ARM_REG_PC, &ret_pc);
    printf("[PLATFORM_MRP_RES] %s name=\"%s\" sha256=%s handle=0x%X ret_lr=0x%X "
           "note=archive_exact_then_decode evidence=OBSERVED\n",
           via ? via : "complete", name, res ? res->sha256_hex : "?",
           res ? res->handle_guest : 0, g_entry_lr);
    fflush(stdout);
    product_runtime_progress_emit("platform_mrp_resource", via ? via : "complete", name);
    return 1;
}

static int try_304bf0_entry_complete(uc_engine *uc) {
    char name[NAME_MAX + 1];
    GwyMrpResourceRequest req;
    GwyMrpResourceResult res;

    if (!uc || !g_entry_valid || !g_entry_r1) return 0;
    memset(name, 0, sizeof(name));
    if (!read_guest_cstr(uc, g_entry_r1, name, sizeof(name))) return 0;
    if (!looks_like_member_name(name)) return 0;

    memset(&req, 0, sizeof(req));
    snprintf(req.name, sizeof(req.name), "%s", name);
    req.guest_name_ptr = g_entry_r1;
    req.caller_lr = g_entry_lr;
    req.caller_pc = PLATFORM_MRP_LOOKUP_ENTRY_PC;
    memset(&res, 0, sizeof(res));
    if (!platform_mrp_resource_load(uc, &req, &res)) return 0;
    return restore_304bf0_ok(uc, name, &res, "entry_complete");
}

static int on_strcmp_match(void *uc, uint32_t a_guest, uint32_t b_guest, int32_t cmp) {
    char a[NAME_MAX + 1], b[NAME_MAX + 1];
    const char *name = NULL;
    GwyMrpResourceRequest req;
    GwyMrpResourceResult res;

    if (!platform_mrp_resource_enabled()) return 0;
    if (cmp != 0) return 0; /* true miss — never force equal */
    if (!g_entry_valid || !g_entry_lr || !g_entry_sp) return 0;

    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));
    (void)read_guest_cstr(uc, a_guest, a, sizeof(a));
    (void)read_guest_cstr(uc, b_guest, b, sizeof(b));
    if (looks_like_member_name(a))
        name = a;
    else if (looks_like_member_name(b))
        name = b;
    else
        return 0;

    memset(&req, 0, sizeof(req));
    snprintf(req.name, sizeof(req.name), "%s", name);
    req.caller_lr = g_entry_lr;
    req.caller_pc = PLATFORM_MRP_LOOKUP_ENTRY_PC;

    if (!platform_mrp_resource_load(uc, &req, &res)) return 0;
    return restore_304bf0_ok((uc_engine *)uc, name, &res, "postmatch_complete");
}
#endif

void platform_mrp_resource_arm(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_err e;
    uint32_t entry = PLATFORM_MRP_LOOKUP_ENTRY_PC;
    if (!uc) uc = g_uc;
    if (!uc) return;
    g_uc = uc;

    platform_strcmp_set_match_hook(on_strcmp_match);

    if (!platform_mrp_resource_enabled()) {
        printf("[PLATFORM_MRP_RES] arm skipped enabled=0 evidence=OBSERVED\n");
        fflush(stdout);
        return;
    }

    /* Phase 2: catalog index only — does not change 304BF0 lookup yet. */
    if (!jjfbol_catalog_ready()) {
        const char *rr = getenv("GWY_RESOURCE_ROOT");
        if (rr && rr[0]) {
            LauncherError cerr;
            if (jjfbol_catalog_init(rr, &cerr) != L_OK) {
                printf("[JJFBOL_CATALOG] init_fail msg=%s detail=%s evidence=OBSERVED\n",
                       cerr.message, cerr.detail);
                fflush(stdout);
            }
        }
    }

    (void)ensure_guest_maps((uc_engine *)uc);

    if (!g_entry_hook_armed) {
        e = uc_hook_add((uc_engine *)uc, &g_entry_hook, UC_HOOK_CODE, (void *)on_lookup_entry, NULL,
                        (uint64_t)entry, (uint64_t)entry);
        if (e != UC_ERR_OK) {
            printf("[PLATFORM_MRP_RES] entry_hook_fail pc=0x%X uc_err=%u evidence=OBSERVED\n", entry,
                   (unsigned)e);
            fflush(stdout);
        } else {
            g_entry_hook_armed = 1;
            printf("[PLATFORM_MRP_RES] armed entry_frame_capture=0x%X strcmp_match_hook=1 "
                   "entry_complete=1 sibling=default2 note=no_force_equal evidence=OBSERVED\n",
                   entry);
            fflush(stdout);
        }
    }
#else
    (void)uc;
#endif
}
