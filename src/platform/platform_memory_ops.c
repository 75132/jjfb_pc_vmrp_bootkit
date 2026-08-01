#include "gwy_launcher/platform_memory_ops.h"

#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/p22_selection_gates.h"
#include "gwy_launcher/p22g_callback_publication.h"
#include "gwy_launcher/p22h_helper_handoff.h"
#include "gwy_launcher/product_runtime_progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
/* Match third_party/vmrp_upstream/header/gwy_ext_obs_abi.h host-callback surface. */
void gwy_ext_obs_host_callback_enter(void *uc, uint32_t slot_addr, const char *name);
void gwy_ext_obs_host_callback_leave(void *uc, uint32_t slot_addr, const char *name);
void gwy_ext_obs_host_callback_resume(void *uc, uint32_t slot_addr, const char *name);
#endif

#define COPY_CHUNK 4096u
#define STRLEN_MAX 0x100000u
/* DSM guest memset body; Robotol has been observed to park this in ER_RW+0x1450. */
#define DSM_MEMSET_BODY_VA 0x94F04u

static int g_import_known;
static int g_import_en;
static void *g_uc;
static int g_hook_armed;
static int g_memcpy_dsm_hook_armed;
static int g_strlen_hook_armed;
static int g_strcpy_hook_armed;
#ifdef GWY_HAVE_UNICORN
static uc_hook g_import_hook;
static uc_hook g_memcpy_dsm_hook;
static uc_hook g_strlen_hook;
static uc_hook g_strcpy_hook;
#endif
static uint32_t g_import_calls;
static uint32_t g_import_fails;
static uint32_t g_memcpy_dsm_calls;
static uint32_t g_strlen_calls;
static uint32_t g_strcpy_calls;
static uint32_t g_libc_cache_erw;
static int g_libc_cache_published;

static int env_explicit_zero(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '0' && e[1] == '\0';
}

static int env_explicit_one(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

int platform_memcpy_import_enabled(void) {
    if (!g_import_known) {
        /* Default ON; explicit 0 disables for A/B baseline. */
        if (env_explicit_zero("JJFB_PLATFORM_MEMCPY_IMPORT"))
            g_import_en = 0;
        else if (env_explicit_one("JJFB_PLATFORM_MEMCPY_IMPORT"))
            g_import_en = 1;
        else
            g_import_en = 1;
        g_import_known = 1;
    }
    return g_import_en;
}

void platform_memcpy_import_reset(void) {
    g_import_known = 0;
    g_import_en = 0;
    g_uc = NULL;
    /* Keep g_hook_armed / g_strlen_hook_armed: Unicorn CODE hooks are process-lifetime. */
    g_import_calls = 0;
    g_import_fails = 0;
    g_memcpy_dsm_calls = 0;
    g_strlen_calls = 0;
    g_strcpy_calls = 0;
    g_libc_cache_erw = 0;
    g_libc_cache_published = 0;
}

uint32_t platform_guest_strlen(void *uc, uint32_t str_guest) {
    uint32_t n = 0;
    uint8_t b = 0;

    if (!uc || !str_guest) return 0;
    /* Cursor indices / status codes (e.g. 0x16) must not enter DSM strlen body. */
    if (str_guest < 0x1000u) {
        printf("[PLATFORM_STRLEN] reject low_ptr=0x%X evidence=OBSERVED\n", str_guest);
        fflush(stdout);
        return 0;
    }
    while (n < STRLEN_MAX) {
        if (!guest_memory_uc_peek((struct uc_struct *)uc, str_guest + n, &b, 1)) {
            printf("[PLATFORM_STRLEN] fail reason=unmapped str=0x%X off=0x%X evidence=OBSERVED\n",
                   str_guest, n);
            fflush(stdout);
            return n;
        }
        if (b == 0) return n;
        n++;
    }
    printf("[PLATFORM_STRLEN] fail reason=too_long str=0x%X cap=0x%X evidence=OBSERVED\n",
           str_guest, STRLEN_MAX);
    fflush(stdout);
    return STRLEN_MAX;
}

uint32_t platform_guest_strcpy(void *uc, uint32_t dst_guest, uint32_t src_guest) {
    uint32_t n;
    uint8_t z = 0;
    if (!uc || !dst_guest) return 0;
    if (!src_guest || src_guest < 0x1000u) {
        printf("[PLATFORM_STRCPY] reject low_src=0x%X dst=0x%X evidence=OBSERVED\n", src_guest,
               dst_guest);
        fflush(stdout);
        (void)guest_memory_uc_poke((struct uc_struct *)uc, dst_guest, &z, 1);
        return dst_guest;
    }
    n = platform_guest_strlen(uc, src_guest);
    if (n >= STRLEN_MAX) return 0;
    if (!platform_guest_memcpy(uc, dst_guest, src_guest, n + 1u)) return 0;
    return dst_guest;
}

int platform_libc_cache_publish(void *uc, uint32_t er_rw, uint32_t mr_table) {
    uint32_t memcpy_fp = 0, memset_fp = 0, strlen_fp = 0;
    uint32_t cur_memcpy = 0, cur_memset = 0, cur_strlen = 0;
    int wrote = 0;

    if (!uc || !er_rw || !mr_table) return 0;

    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, mr_table + PLATFORM_MEMCPY_MR_TABLE_OFF,
                                 &memcpy_fp) ||
        !guest_memory_uc_peek_u32((struct uc_struct *)uc, mr_table + PLATFORM_MEMSET_MR_TABLE_OFF,
                                 &memset_fp) ||
        !guest_memory_uc_peek_u32((struct uc_struct *)uc, mr_table + PLATFORM_STRLEN_MR_TABLE_OFF,
                                 &strlen_fp)) {
        printf("[PLATFORM_LIBC_CACHE] fail reason=mr_table_unmapped mt=0x%X evidence=OBSERVED\n",
               mr_table);
        fflush(stdout);
        return 0;
    }
    if (!memcpy_fp || !memset_fp || !strlen_fp) {
        printf("[PLATFORM_LIBC_CACHE] fail reason=empty_fps memcpy=0x%X memset=0x%X strlen=0x%X "
               "evidence=OBSERVED\n",
               memcpy_fp, memset_fp, strlen_fp);
        fflush(stdout);
        return 0;
    }

    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, er_rw + PLATFORM_ROBOTOL_ERW_MEMCPY_OFF,
                                   &cur_memcpy);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, er_rw + PLATFORM_ROBOTOL_ERW_MEMSET_OFF,
                                   &cur_memset);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, er_rw + PLATFORM_ROBOTOL_ERW_STRLEN_OFF,
                                   &cur_strlen);

    /*
     * Always re-install if guest re-filled DSM bodies after an earlier publish.
     * +0x1420 DSM memcpy (0x94E94) → 2FDD5C nested-cfunction hang inside 2FC26C.
     * +0x1450 DSM memset (0x94F04) → 0x2D9648 treats strlen as huge fill.
     */
    if (cur_memcpy != memcpy_fp) {
        if (!guest_memory_uc_poke_u32((struct uc_struct *)uc, er_rw + PLATFORM_ROBOTOL_ERW_MEMCPY_OFF,
                                      memcpy_fp))
            return 0;
        wrote = 1;
    }
    if (cur_memset != memset_fp) {
        if (!guest_memory_uc_poke_u32((struct uc_struct *)uc, er_rw + PLATFORM_ROBOTOL_ERW_MEMSET_OFF,
                                      memset_fp))
            return 0;
        wrote = 1;
    }
    if (cur_strlen != strlen_fp) {
        if (!guest_memory_uc_poke_u32((struct uc_struct *)uc, er_rw + PLATFORM_ROBOTOL_ERW_STRLEN_OFF,
                                      strlen_fp))
            return 0;
        wrote = 1;
    }

    if (!wrote && g_libc_cache_published && g_libc_cache_erw == er_rw) return 1;

    g_libc_cache_erw = er_rw;
    g_libc_cache_published = 1;
    printf("[PLATFORM_LIBC_CACHE] erw=0x%X mt=0x%X memcpy=0x%X->0x%X memset=0x%X->0x%X "
           "strlen=0x%X->0x%X wrote=%d dsm_memcpy_misbind=%s dsm_memset_misbind=%s "
           "evidence=OBSERVED\n",
           er_rw, mr_table, cur_memcpy, memcpy_fp, cur_memset, memset_fp, cur_strlen, strlen_fp,
           wrote, (cur_memcpy == 0x94E94u || (cur_memcpy >= 0x80000u && cur_memcpy < 0xD2000u))
                      ? "yes"
                      : "no",
           (cur_strlen == DSM_MEMSET_BODY_VA || cur_strlen == cur_memset) ? "yes" : "no");
    fflush(stdout);
    product_runtime_progress_emit("platform_libc_cache", "memcpy_strlen", "0x1420_0x1450");
    return 1;
}

uint32_t platform_memcpy_import_call_count(void) { return g_import_calls; }
uint32_t platform_memcpy_import_fail_count(void) { return g_import_fails; }

uint32_t platform_guest_memcpy(void *uc, uint32_t dst_guest, uint32_t src_guest, uint32_t size) {
    uint8_t chunk[COPY_CHUNK];
    uint32_t done = 0;

    if (!uc) {
        printf("[PLATFORM_MEMCPY] fail reason=null_uc dst=0x%X src=0x%X n=0x%X evidence=OBSERVED\n",
               dst_guest, src_guest, size);
        fflush(stdout);
        return 0;
    }
    if (size == 0u) return dst_guest;
    if (!dst_guest || !src_guest) {
        printf("[PLATFORM_MEMCPY] fail reason=null_guest_addr dst=0x%X src=0x%X n=0x%X "
               "evidence=OBSERVED\n",
               dst_guest, src_guest, size);
        fflush(stdout);
        return 0;
    }
    if (p22_enabled()) p22_note_platform_memcpy(dst_guest, src_guest, size);
    if (p22g_enabled()) p22g_note_memcpy(dst_guest, src_guest, size, 0);
    if (p22h_enabled()) p22h_note_memcpy(dst_guest, src_guest, size, 0);
    /* Reject 32-bit range wrap. */
    if (dst_guest + size < dst_guest || src_guest + size < src_guest) {
        printf("[PLATFORM_MEMCPY] fail reason=range_overflow dst=0x%X src=0x%X n=0x%X "
               "evidence=OBSERVED\n",
               dst_guest, src_guest, size);
        fflush(stdout);
        return 0;
    }

    while (done < size) {
        uint32_t n = size - done;
        if (n > COPY_CHUNK) n = COPY_CHUNK;
        if (!guest_memory_uc_peek((struct uc_struct *)uc, src_guest + done, chunk, n)) {
            printf("[PLATFORM_MEMCPY] fail reason=src_unmapped dst=0x%X src=0x%X off=0x%X n=0x%X "
                   "evidence=OBSERVED\n",
                   dst_guest, src_guest, done, n);
            fflush(stdout);
            return 0;
        }
        if (!guest_memory_uc_poke((struct uc_struct *)uc, dst_guest + done, chunk, n)) {
            printf("[PLATFORM_MEMCPY] fail reason=dst_unmapped dst=0x%X src=0x%X off=0x%X n=0x%X "
                   "evidence=OBSERVED\n",
                   dst_guest, src_guest, done, n);
            fflush(stdout);
            return 0;
        }
        done += n;
    }
    return dst_guest;
}

#ifdef GWY_HAVE_UNICORN
/*
 * Host callback for the misbound Robotol copy import identity (DSM 0x804A8).
 * BLX still transfers here; we never skip the call-site BLX. Guest DSM body at
 * this VA is not executed — same model as mr_table MAP_FUNC stubs.
 */
static void on_memcpy_import_slot(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t dst = 0, src = 0, n = 0, lr = 0, ret = 0;
    (void)address;
    (void)size;
    (void)user_data;

    if (!platform_memcpy_import_enabled()) return;

    uc_reg_read(uc, UC_ARM_REG_R0, &dst);
    uc_reg_read(uc, UC_ARM_REG_R1, &src);
    uc_reg_read(uc, UC_ARM_REG_R2, &n);

    g_import_calls++;
    gwy_ext_obs_host_callback_enter(uc, PLATFORM_MEMCPY_IMPORT_SLOT_VA, "platform_guest_memcpy");
    ret = platform_guest_memcpy(uc, dst, src, n);
    if (!ret && !(dst && src && n == 0u)) {
        g_import_fails++;
        printf("[PLATFORM_MEMCPY] import_fail slot=0x%X dst=0x%X src=0x%X n=0x%X "
               "note=no_pc_fallback evidence=OBSERVED\n",
               PLATFORM_MEMCPY_IMPORT_SLOT_VA, dst, src, n);
        fflush(stdout);
    } else if (g_import_calls <= 3u || (g_import_calls % 500u) == 0u) {
        printf("[PLATFORM_MEMCPY] import_ok n_calls=%u slot=0x%X dst=0x%X src=0x%X n=0x%X "
               "evidence=OBSERVED\n",
               g_import_calls, PLATFORM_MEMCPY_IMPORT_SLOT_VA, dst, src, n);
        fflush(stdout);
        if (g_import_calls == 1u)
            product_runtime_progress_emit("platform_memcpy_import", "memcpy", "0x804A8");
    }
    {
        uint32_t r0 = ret;
        uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    }
    gwy_ext_obs_host_callback_leave(uc, PLATFORM_MEMCPY_IMPORT_SLOT_VA, "platform_guest_memcpy");
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_write(uc, UC_ARM_REG_PC, &lr);
    gwy_ext_obs_host_callback_resume(uc, PLATFORM_MEMCPY_IMPORT_SLOT_VA, "platform_guest_memcpy");
}

/* DSM memcpy @0x94E94 — robotol also BLX's this body directly (e.g. 0x304F26). */
static void on_memcpy_dsm_body(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t dst = 0, src = 0, n = 0, lr = 0, ret = 0;
    (void)address;
    (void)size;
    (void)user_data;

    if (!platform_memcpy_import_enabled()) return;

    uc_reg_read(uc, UC_ARM_REG_R0, &dst);
    uc_reg_read(uc, UC_ARM_REG_R1, &src);
    uc_reg_read(uc, UC_ARM_REG_R2, &n);
    g_memcpy_dsm_calls++;
    gwy_ext_obs_host_callback_enter(uc, PLATFORM_MEMCPY_DSM_BODY_VA, "platform_guest_memcpy");
    ret = platform_guest_memcpy(uc, dst, src, n);
    if (!ret && !(dst && src && n == 0u)) {
        g_import_fails++;
        printf("[PLATFORM_MEMCPY] dsm_body_fail slot=0x%X dst=0x%X src=0x%X n=0x%X "
               "evidence=OBSERVED\n",
               PLATFORM_MEMCPY_DSM_BODY_VA, dst, src, n);
        fflush(stdout);
    } else if (g_memcpy_dsm_calls <= 3u || (g_memcpy_dsm_calls % 500u) == 0u) {
        /* Quiet path: unzip/boot issue thousands of tiny copies. */
        printf("[PLATFORM_MEMCPY] dsm_body_ok n_calls=%u slot=0x%X dst=0x%X src=0x%X n=0x%X "
               "evidence=OBSERVED\n",
               g_memcpy_dsm_calls, PLATFORM_MEMCPY_DSM_BODY_VA, dst, src, n);
        fflush(stdout);
        if (g_memcpy_dsm_calls == 1u)
            product_runtime_progress_emit("platform_memcpy_dsm", "memcpy", "0x94E94");
    }
    {
        uint32_t r0 = ret;
        uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    }
    gwy_ext_obs_host_callback_leave(uc, PLATFORM_MEMCPY_DSM_BODY_VA, "platform_guest_memcpy");
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_write(uc, UC_ARM_REG_PC, &lr);
    gwy_ext_obs_host_callback_resume(uc, PLATFORM_MEMCPY_DSM_BODY_VA, "platform_guest_memcpy");
}

/* DSM strlen @0xAC374 — post-gate 0x2FD886 calls this body with R0=C-string. */
static void on_strlen_dsm_body(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t str = 0, lr = 0, ret = 0;
    (void)address;
    (void)size;
    (void)user_data;

    if (!platform_memcpy_import_enabled()) return;

    uc_reg_read(uc, UC_ARM_REG_R0, &str);
    g_strlen_calls++;
    gwy_ext_obs_host_callback_enter(uc, PLATFORM_STRLEN_DSM_BODY_VA, "platform_guest_strlen");
    ret = platform_guest_strlen(uc, str);
    printf("[PLATFORM_STRLEN] import_ok slot=0x%X str=0x%X ret=0x%X evidence=OBSERVED\n",
           PLATFORM_STRLEN_DSM_BODY_VA, str, ret);
    fflush(stdout);
    product_runtime_progress_emit("platform_strlen_import", "strlen", "0xAC374");
    {
        uint32_t r0 = ret;
        uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    }
    gwy_ext_obs_host_callback_leave(uc, PLATFORM_STRLEN_DSM_BODY_VA, "platform_guest_strlen");
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_write(uc, UC_ARM_REG_PC, &lr);
    gwy_ext_obs_host_callback_resume(uc, PLATFORM_STRLEN_DSM_BODY_VA, "platform_guest_strlen");
}

/* DSM strcpy @0xAC300 — post-gate 0x310740: R0=dst R1=src. */
static void on_strcpy_dsm_body(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t dst = 0, src = 0, lr = 0, ret = 0;
    (void)address;
    (void)size;
    (void)user_data;

    if (!platform_memcpy_import_enabled()) return;

    uc_reg_read(uc, UC_ARM_REG_R0, &dst);
    uc_reg_read(uc, UC_ARM_REG_R1, &src);
    g_strcpy_calls++;
    gwy_ext_obs_host_callback_enter(uc, PLATFORM_STRCPY_DSM_BODY_VA, "platform_guest_strcpy");
    ret = platform_guest_strcpy(uc, dst, src);
    printf("[PLATFORM_STRCPY] import_ok slot=0x%X dst=0x%X src=0x%X ret=0x%X evidence=OBSERVED\n",
           PLATFORM_STRCPY_DSM_BODY_VA, dst, src, ret);
    fflush(stdout);
    product_runtime_progress_emit("platform_strcpy_import", "strcpy", "0xAC300");
    {
        uint32_t r0 = ret;
        uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    }
    gwy_ext_obs_host_callback_leave(uc, PLATFORM_STRCPY_DSM_BODY_VA, "platform_guest_strcpy");
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_write(uc, UC_ARM_REG_PC, &lr);
    gwy_ext_obs_host_callback_resume(uc, PLATFORM_STRCPY_DSM_BODY_VA, "platform_guest_strcpy");
}
#endif

void platform_memcpy_import_bind_uc(void *uc) { g_uc = uc; }

void platform_memcpy_import_arm(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_err e;
    uint32_t slot = PLATFORM_MEMCPY_IMPORT_SLOT_VA;
    uint32_t memcpy_dsm = PLATFORM_MEMCPY_DSM_BODY_VA;
    uint32_t strlen_va = PLATFORM_STRLEN_DSM_BODY_VA;
    uint32_t strcpy_va = PLATFORM_STRCPY_DSM_BODY_VA;
    if (!uc) uc = g_uc;
    if (!uc) return;
    if (!platform_memcpy_import_enabled()) {
        printf("[PLATFORM_MEMCPY] import_arm skipped enabled=0 slot=0x%X evidence=OBSERVED\n",
               slot);
        fflush(stdout);
        return;
    }
    if (!g_hook_armed) {
        e = uc_hook_add((uc_engine *)uc, &g_import_hook, UC_HOOK_CODE, (void *)on_memcpy_import_slot,
                        NULL, (uint64_t)slot, (uint64_t)slot);
        if (e != UC_ERR_OK) {
            printf("[PLATFORM_MEMCPY] import_arm_fail slot=0x%X uc_err=%u evidence=OBSERVED\n", slot,
                   (unsigned)e);
            fflush(stdout);
        } else {
            g_hook_armed = 1;
            printf("[PLATFORM_MEMCPY] import_armed slot=0x%X identity=dsm_misbound_copy "
                   "mr_table_off=0x%X evidence=DOCUMENTED\n",
                   slot, PLATFORM_MEMCPY_MR_TABLE_OFF);
            fflush(stdout);
        }
    }
    if (!g_memcpy_dsm_hook_armed) {
        e = uc_hook_add((uc_engine *)uc, &g_memcpy_dsm_hook, UC_HOOK_CODE, (void *)on_memcpy_dsm_body,
                        NULL, (uint64_t)memcpy_dsm, (uint64_t)memcpy_dsm);
        if (e != UC_ERR_OK) {
            printf("[PLATFORM_MEMCPY] dsm_body_arm_fail slot=0x%X uc_err=%u evidence=OBSERVED\n",
                   memcpy_dsm, (unsigned)e);
            fflush(stdout);
        } else {
            g_memcpy_dsm_hook_armed = 1;
            printf("[PLATFORM_MEMCPY] dsm_body_armed slot=0x%X identity=dsm_memcpy_body "
                   "evidence=DOCUMENTED\n",
                   memcpy_dsm);
            fflush(stdout);
        }
    }
    if (!g_strlen_hook_armed) {
        e = uc_hook_add((uc_engine *)uc, &g_strlen_hook, UC_HOOK_CODE, (void *)on_strlen_dsm_body,
                        NULL, (uint64_t)strlen_va, (uint64_t)strlen_va);
        if (e != UC_ERR_OK) {
            printf("[PLATFORM_STRLEN] import_arm_fail slot=0x%X uc_err=%u evidence=OBSERVED\n",
                   strlen_va, (unsigned)e);
            fflush(stdout);
        } else {
            g_strlen_hook_armed = 1;
            printf("[PLATFORM_STRLEN] import_armed slot=0x%X identity=dsm_strlen_body "
                   "evidence=DOCUMENTED+E10A\n",
                   strlen_va);
            fflush(stdout);
        }
    }
    if (!g_strcpy_hook_armed) {
        e = uc_hook_add((uc_engine *)uc, &g_strcpy_hook, UC_HOOK_CODE, (void *)on_strcpy_dsm_body,
                        NULL, (uint64_t)strcpy_va, (uint64_t)strcpy_va);
        if (e != UC_ERR_OK) {
            printf("[PLATFORM_STRCPY] import_arm_fail slot=0x%X uc_err=%u evidence=OBSERVED\n",
                   strcpy_va, (unsigned)e);
            fflush(stdout);
        } else {
            g_strcpy_hook_armed = 1;
            printf("[PLATFORM_STRCPY] import_armed slot=0x%X identity=dsm_strcpy_body "
                   "evidence=DOCUMENTED\n",
                   strcpy_va);
            fflush(stdout);
        }
    }
    g_uc = uc;
#else
    (void)uc;
#endif
}
