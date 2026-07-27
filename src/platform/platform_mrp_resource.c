#include "gwy_launcher/platform_mrp_resource.h"

#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/package_metadata.h"
#include "gwy_launcher/platform_memory_ops.h"
#include "gwy_launcher/product_runtime_progress.h"
#include "gwy_launcher/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define POSTMATCH_MAX 8u
#define NAME_MAX 255u
#define PIXEL_CACHE_MAX 16u

typedef struct {
    uint32_t bytes;
    uint32_t guest_pixels;
    uint32_t handle_guest;
    uint16_t w;
    uint16_t h;
    int valid;
} PixelByBytes;

static int g_en_known;
static int g_en;
static void *g_uc;
static int g_entry_hook_armed;
static int g_maps_ok;
static uint32_t g_pixel_slot;
static uint32_t g_postmatch_n;
static char g_done_names[POSTMATCH_MAX][NAME_MAX + 1];
static PixelByBytes g_pixel_cache[PIXEL_CACHE_MAX];
static uint32_t g_pixel_cache_n;

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

void platform_mrp_resource_note_pixels(uint32_t bytes, uint32_t guest_pixels, uint16_t w,
                                      uint16_t h) {
    platform_mrp_resource_note_pixels_ex(bytes, guest_pixels, 0, w, h);
}

void platform_mrp_resource_note_pixels_ex(uint32_t bytes, uint32_t guest_pixels,
                                         uint32_t handle_guest, uint16_t w, uint16_t h) {
    uint32_t i;
    if (bytes < 16u || !guest_pixels) return;
    for (i = 0; i < g_pixel_cache_n && i < PIXEL_CACHE_MAX; i++) {
        if (g_pixel_cache[i].valid && g_pixel_cache[i].bytes == bytes) {
            g_pixel_cache[i].guest_pixels = guest_pixels;
            if (handle_guest) g_pixel_cache[i].handle_guest = handle_guest;
            g_pixel_cache[i].w = w;
            g_pixel_cache[i].h = h;
            return;
        }
    }
    if (g_pixel_cache_n >= PIXEL_CACHE_MAX) {
        i = g_pixel_cache_n % PIXEL_CACHE_MAX;
        g_pixel_cache[i].bytes = bytes;
        g_pixel_cache[i].guest_pixels = guest_pixels;
        g_pixel_cache[i].handle_guest = handle_guest;
        g_pixel_cache[i].w = w;
        g_pixel_cache[i].h = h;
        g_pixel_cache[i].valid = 1;
        g_pixel_cache_n++;
        return;
    }
    g_pixel_cache[g_pixel_cache_n].bytes = bytes;
    g_pixel_cache[g_pixel_cache_n].guest_pixels = guest_pixels;
    g_pixel_cache[g_pixel_cache_n].handle_guest = handle_guest;
    g_pixel_cache[g_pixel_cache_n].w = w;
    g_pixel_cache[g_pixel_cache_n].h = h;
    g_pixel_cache[g_pixel_cache_n].valid = 1;
    g_pixel_cache_n++;
}

uint32_t platform_mrp_resource_pixels_by_bytes(uint32_t bytes) {
    uint32_t i;
    if (bytes < 16u) return 0;
    for (i = 0; i < PIXEL_CACHE_MAX; i++) {
        if (g_pixel_cache[i].valid && g_pixel_cache[i].bytes == bytes)
            return g_pixel_cache[i].guest_pixels;
    }
    return 0;
}

int platform_mrp_resource_bind_10134_pixels(void *uc, uint32_t bytes, uint32_t user_pixels) {
    uint32_t i, handle = 0;
    if (!uc || bytes < 16u || !user_pixels) return 0;
    for (i = 0; i < PIXEL_CACHE_MAX; i++) {
        if (g_pixel_cache[i].valid && g_pixel_cache[i].bytes == bytes) {
            handle = g_pixel_cache[i].handle_guest;
            g_pixel_cache[i].guest_pixels = user_pixels; /* prefer mallocExt USER */
            break;
        }
    }
    if (!handle) return 0;
#ifdef GWY_HAVE_UNICORN
    if (!guest_memory_uc_poke_u32((struct uc_struct *)uc, handle + 4u, user_pixels)) return 0;
    printf("[PLATFORM_MRP_RES] bind_10134 handle=0x%X pixels=0x%X bytes=0x%X evidence=OBSERVED\n",
           handle, user_pixels, bytes);
    fflush(stdout);
    return 1;
#else
    (void)handle;
    return 0;
#endif
}

void platform_mrp_resource_reset(void) {
    g_en_known = 0;
    g_en = 0;
    g_uc = NULL;
    g_entry_valid = 0;
    g_pixel_slot = 0;
    g_postmatch_n = 0;
    g_pixel_cache_n = 0;
    memset(g_done_names, 0, sizeof(g_done_names));
    memset(g_pixel_cache, 0, sizeof(g_pixel_cache));
    /* Keep unicorn hooks / maps for process lifetime. */
}

void platform_mrp_resource_bind_uc(void *uc) {
    if (uc) g_uc = uc;
}

uint32_t platform_mrp_resource_postmatch_count(void) { return g_postmatch_n; }

static int name_already_done(const char *name) {
    uint32_t i;
    if (!name || !name[0]) return 1;
    for (i = 0; i < g_postmatch_n && i < POSTMATCH_MAX; i++) {
        if (strcmp(g_done_names[i], name) == 0) return 1;
    }
    return 0;
}

static void mark_done(const char *name) {
    if (!name || !name[0] || g_postmatch_n >= POSTMATCH_MAX) return;
    snprintf(g_done_names[g_postmatch_n], sizeof(g_done_names[0]), "%s", name);
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
    snprintf(buf, buf_sz, "game_files/mythroad/320x480/gwy/jjfb.mrp");
    return buf;
}

/*
 * V75: topleft!15!5.bmp misses in jjfb.mrp → guest opens jjfbol/default2.mrp.
 * Product chrome skips often never open default2, so 304BF0 scans the wrong
 * pack forever. Try primary then sibling default2 (same layout as V75).
 */
#define MRP_PATH_CANDIDATES 4
static int fill_package_candidates(char paths[MRP_PATH_CANDIDATES][1024]) {
    int n = 0;
    char primary[1024];
    const char *rr;
    const char *hit;
    int i;

    resolve_package_host_path(primary, sizeof(primary));
    snprintf(paths[n++], 1024, "%s", primary);

    hit = strstr(primary, "gwy/jjfb.mrp");
    if (!hit) hit = strstr(primary, "gwy\\jjfb.mrp");
    if (!hit) hit = strstr(primary, "gwy/jjfb.MRP");
    if (hit) {
        size_t prefix = (size_t)(hit - primary);
        snprintf(paths[n], 1024, "%.*sgwy/jjfbol/default2.mrp", (int)prefix, primary);
        n++;
    }

    rr = getenv("GWY_RESOURCE_ROOT");
    if (rr && rr[0]) {
        snprintf(paths[n], 1024, "%s/gwy/jjfbol/default2.mrp", rr);
        n++;
    }

    /* Dedupe (RESOURCE_ROOT may equal primary's parent). */
    {
        int w = 0;
        for (i = 0; i < n; i++) {
            int j, dup = 0;
            for (j = 0; j < w; j++) {
                if (strcmp(paths[i], paths[j]) == 0) {
                    dup = 1;
                    break;
                }
            }
            if (!dup) {
                if (w != i) snprintf(paths[w], 1024, "%s", paths[i]);
                w++;
            }
        }
        return w;
    }
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
    if (name_already_done(name)) return 0;

    {
        char cands[MRP_PATH_CANDIDATES][1024];
        int nc = fill_package_candidates(cands);
        int i;
        int hit_i = -1;
        arch = NULL;
        mem = NULL;
        path = path_buf;
        path_buf[0] = 0;
        for (i = 0; i < nc; i++) {
            LauncherError e2;
            MrpArchive *a2 = NULL;
            const MrpMember *m2 = NULL;
            memset(&e2, 0, sizeof(e2));
            st = mrp_archive_open(cands[i], &a2, &e2);
            if (st != L_OK || !a2) continue;
            st = mrp_archive_find_exact(a2, name, &m2, &e2);
            if (st == L_OK && m2) {
                arch = a2;
                mem = m2;
                snprintf(path_buf, sizeof(path_buf), "%s", cands[i]);
                path = path_buf;
                hit_i = i;
                break;
            }
            mrp_archive_close(a2);
        }
        if (!arch || !mem) {
            /* Not in primary or default2 — leave guest natural scan alone. */
            return 0;
        }
        if (hit_i > 0) {
            printf("[PLATFORM_MRP_RES] sibling_pack name=\"%s\" path=%s "
                   "note=V75_default2_after_jjfb_miss evidence=OBSERVED\n",
                   name, path);
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

    mark_done(name);
    platform_mrp_resource_note_pixels_ex(sz, px, handle_va, (uint16_t)w, (uint16_t)h);
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
