#include "gwy_launcher/platform_memory_ops.h"

#include "gwy_launcher/guest_memory.h"
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

static int g_import_known;
static int g_import_en;
static void *g_uc;
static int g_hook_armed;
#ifdef GWY_HAVE_UNICORN
static uc_hook g_import_hook;
#endif
static uint32_t g_import_calls;
static uint32_t g_import_fails;

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
    /* Keep g_hook_armed: Unicorn CODE hook is process-lifetime once installed. */
    g_import_calls = 0;
    g_import_fails = 0;
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
    } else {
        printf("[PLATFORM_MEMCPY] import_ok slot=0x%X dst=0x%X src=0x%X n=0x%X ret=0x%X "
               "evidence=OBSERVED\n",
               PLATFORM_MEMCPY_IMPORT_SLOT_VA, dst, src, n, ret);
        fflush(stdout);
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
#endif

void platform_memcpy_import_bind_uc(void *uc) { g_uc = uc; }

void platform_memcpy_import_arm(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_err e;
    uint32_t slot = PLATFORM_MEMCPY_IMPORT_SLOT_VA;
    if (!uc) uc = g_uc;
    if (!uc || g_hook_armed) return;
    if (!platform_memcpy_import_enabled()) {
        printf("[PLATFORM_MEMCPY] import_arm skipped enabled=0 slot=0x%X evidence=OBSERVED\n",
               slot);
        fflush(stdout);
        return;
    }
    e = uc_hook_add((uc_engine *)uc, &g_import_hook, UC_HOOK_CODE, (void *)on_memcpy_import_slot,
                    NULL, (uint64_t)slot, (uint64_t)slot);
    if (e != UC_ERR_OK) {
        printf("[PLATFORM_MEMCPY] import_arm_fail slot=0x%X uc_err=%u evidence=OBSERVED\n", slot,
               (unsigned)e);
        fflush(stdout);
        return;
    }
    g_hook_armed = 1;
    g_uc = uc;
    printf("[PLATFORM_MEMCPY] import_armed slot=0x%X identity=dsm_misbound_copy "
           "mr_table_off=0x%X evidence=DOCUMENTED\n",
           slot, PLATFORM_MEMCPY_MR_TABLE_OFF);
    fflush(stdout);
#else
    (void)uc;
#endif
}
