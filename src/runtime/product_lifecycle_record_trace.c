#include "gwy_launcher/product_lifecycle_record_trace.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/product_runtime_progress.h"
#include "gwy_launcher/platform_memory_ops.h"
#include "gwy_launcher/ext_chunk_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PC_2DADC4 0x2DADC4u
#define PC_30ED2C 0x30ED2Cu
#define PC_30ED4C 0x30ED4Cu /* after BL 0x2D96BC lookup */
#define PC_30ED64 0x30ED64u /* after BL 0x2F6C44 compare */
#define PC_30ED7A 0x30ED7Au
#define PC_30ED7C 0x30ED7Cu /* success leave */
#define PC_30ED82 0x30ED82u /* failure leave */
#define PC_2FC26C 0x2FC26Cu
#define PC_2FC418 0x2FC418u
#define PC_2D96BC 0x2D96BCu
#define PC_2F6C44 0x2F6C44u

#define OFF_B58 0xB58u
#define OFF_B5C 0xB5Cu
#define OFF_B70 0xB70u
#define OFF_B71 0xB71u
#define OFF_15D 0x15Du
#define OFF_UI 0x8D0u

static int g_en;
static int g_en_known;
static int g_hook_ok;
static int g_mem_hook_ok;
static void *g_uc;
static uint32_t g_er_rw;
static int g_saw_2dadc4;
static int g_saw_30ed2c;
static int g_saw_lookup;
static int g_saw_compare;
static int g_saw_ok;
static int g_saw_fail;
static int g_saw_2fc26c;
static int g_saw_2fc418;
static int g_b71_natural;
static uint32_t g_b71_store_pc;
static uint8_t g_b71_old;
static uint8_t g_b71_new;
static int g_finalized;

#ifdef GWY_HAVE_UNICORN
static void on_lrt_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                             int64_t value, void *user);
#endif

static int env1(const char *k) {
    const char *v = getenv(k);
    return v && v[0] == '1' && v[1] == '\0';
}

int product_lrt_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_LIFECYCLE_RECORD_TRACE");
        g_en_known = 1;
    }
    return g_en;
}

void product_lrt_reset(void) {
    g_finalized = 0;
    g_hook_ok = 0;
    g_mem_hook_ok = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_saw_2dadc4 = g_saw_30ed2c = g_saw_lookup = g_saw_compare = 0;
    g_saw_ok = g_saw_fail = g_saw_2fc26c = g_saw_2fc418 = 0;
    g_b71_natural = 0;
    g_b71_store_pc = 0;
    g_b71_old = g_b71_new = 0;
    g_en_known = 0;
}

void product_lrt_bind_uc(void *uc) {
    if (uc) g_uc = uc;
}

void product_lrt_note_er_rw(uint32_t er_rw) {
    if (er_rw) g_er_rw = er_rw;
#ifdef GWY_HAVE_UNICORN
    if (product_lrt_enabled() && g_uc && g_er_rw && g_hook_ok && !g_mem_hook_ok) {
        uc_hook hm = 0;
        uint64_t a = (uint64_t)g_er_rw + (uint64_t)OFF_B71;
        if (uc_hook_add((uc_engine *)g_uc, &hm, UC_HOOK_MEM_WRITE, on_lrt_mem_write, NULL, a, a) ==
            UC_ERR_OK)
            g_mem_hook_ok = 1;
    }
    /* Module-registration libc cache: Robotol ER_RW+0x1450 must be strlen. */
    if (g_uc && er_rw) {
        uint32_t mt = ext_chunk_provider_mr_table_guest();
        if (mt) (void)platform_libc_cache_publish(g_uc, er_rw, mt);
    }
#endif
}

static int peek_u32(void *uc, uint32_t addr, uint32_t *out) {
    return guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, out);
}

static int peek_bytes(void *uc, uint32_t addr, void *buf, size_t n) {
    return guest_memory_uc_peek((struct uc_struct *)uc, addr, buf, n);
}

static void dump_hex32(const char *tag, const uint8_t *b, int mapped) {
    if (!mapped) {
        printf("[LRT] %s mapped=0\n", tag);
        return;
    }
    printf("[LRT] %s "
           "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X "
           "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
           tag, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11],
           b[12], b[13], b[14], b[15], b[16], b[17], b[18], b[19], b[20], b[21], b[22],
           b[23], b[24], b[25], b[26], b[27], b[28], b[29], b[30], b[31]);
}

static void read_cstr(void *uc, uint32_t addr, char *out, size_t cap) {
    size_t i;
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!addr || !uc) return;
    for (i = 0; i + 1 < cap; i++) {
        uint8_t c = 0;
        if (!peek_bytes(uc, addr + (uint32_t)i, &c, 1)) break;
        out[i] = (char)c;
        if (!c) return;
    }
    out[cap - 1] = '\0';
}

static void sample_er_gates(void *uc, uint32_t er, uint32_t *b58, uint32_t *b5c, uint8_t *b70,
                            uint8_t *b71, uint8_t *f15d, uint32_t *ui) {
    if (b58) *b58 = 0;
    if (b5c) *b5c = 0;
    if (b70) *b70 = 0;
    if (b71) *b71 = 0;
    if (f15d) *f15d = 0;
    if (ui) *ui = 0;
    if (!uc || !er) return;
    if (b58) (void)peek_u32(uc, er + OFF_B58, b58);
    if (b5c) (void)peek_u32(uc, er + OFF_B5C, b5c);
    if (b70) (void)peek_bytes(uc, er + OFF_B70, b70, 1);
    if (b71) (void)peek_bytes(uc, er + OFF_B71, b71, 1);
    if (f15d) (void)peek_bytes(uc, er + OFF_15D, f15d, 1);
    if (ui) (void)peek_u32(uc, er + OFF_UI, ui);
}

#ifdef GWY_HAVE_UNICORN
static void on_lrt_code(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    intptr_t tag = (intptr_t)user;
    uint32_t pc = (uint32_t)address;
    uint32_t lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    uint32_t er = g_er_rw ? g_er_rw : 0;
    uint32_t b58 = 0, b5c = 0, ui = 0;
    uint8_t b70 = 0, b71 = 0, f15d = 0;
    uint8_t obj[32];
    (void)size;

    if (!product_lrt_enabled()) return;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    if (!er && r9) er = r9;

    if (tag == 1) { /* 0x2DADC4 */
        g_saw_2dadc4 = 1;
        sample_er_gates(uc, er, &b58, &b5c, &b70, &b71, &f15d, &ui);
        printf("[LRT_2DADC4_ENTER] pc=0x2DADC4 lr=0x%X sp=0x%X r0=0x%X r1=0x%X r2=0x%X "
               "r3=0x%X r9=0x%X ER_RW=0x%X B58=0x%X B5C=0x%X 15D=%u B71=%u B70=%u "
               "UI_MODE=0x%X evidence=OBSERVED\n",
               lr, sp, r0, r1, r2, r3, r9, er, b58, b5c, (unsigned)f15d, (unsigned)b71,
               (unsigned)b70, ui);
        memset(obj, 0, sizeof(obj));
        dump_hex32("B58_obj32", obj, b58 && peek_bytes(uc, b58, obj, 32));
        memset(obj, 0, sizeof(obj));
        dump_hex32("B5C_obj32", obj, b5c && peek_bytes(uc, b5c, obj, 32));
        fflush(stdout);
        product_runtime_progress_emit("lifecycle_2dadc4", "lrt", "enter");
    } else if (tag == 2) { /* 0x30ED2C */
        uint32_t w0 = 0, w4 = 0, w8 = 0, wc = 0;
        char s1[48], s2[48];
        g_saw_30ed2c = 1;
        sample_er_gates(uc, er, &b58, &b5c, &b70, &b71, &f15d, &ui);
        if (r0) {
            (void)peek_u32(uc, r0, &w0);
            (void)peek_u32(uc, r0 + 4u, &w4);
            (void)peek_u32(uc, r0 + 8u, &w8);
            (void)peek_u32(uc, r0 + 0xCu, &wc);
        }
        read_cstr(uc, w0, s1, sizeof(s1));
        read_cstr(uc, w4, s2, sizeof(s2));
        printf("[LRT_30ED2C_ENTER] pc=0x30ED2C lr=0x%X record=0x%X +0=0x%X +4=0x%X +8=0x%X "
               "+C=0x%X str1=%s str2=%s field_c=0x%X field_d=0x%X r1=0x%X B71=%u "
               "evidence=OBSERVED\n",
               lr, r0, w0, w4, w8, wc, s1[0] ? s1 : "(null)", s2[0] ? s2 : "(null)", w8, wc, r1,
               (unsigned)b71);
        fflush(stdout);
        product_runtime_progress_emit("lifecycle_30ed2c", "lrt", "enter");
    } else if (tag == 3) { /* 0x30ED4C after BL 0x2D96BC (before 0x304AC4) */
        g_saw_lookup = 1;
        printf("[LRT_LOOKUP_RET] pc=0x30ED4C r0=0x%X note=%s lr=0x%X evidence=OBSERVED\n", r0,
               r0 ? "nonzero_after_2D96BC" : "zero_miss", lr);
        fflush(stdout);
        product_runtime_progress_emit("lifecycle_lookup", "lrt", r0 ? "nonzero" : "miss");
    } else if (tag == 11) { /* 0x30ED52 after BL 0x304AC4 */
        printf("[LRT_304AC4_RET] pc=0x30ED52 r0=0x%X note=%s lr=0x%X evidence=OBSERVED\n", r0,
               r0 ? "local_version_ok" : "local_version_fail", lr);
        fflush(stdout);
        product_runtime_progress_emit("lifecycle_local_version", "lrt", r0 ? "ok" : "fail");
    } else if (tag == 4) { /* 0x30ED64 compare return */
        g_saw_compare = 1;
        printf("[LRT_COMPARE_RET] pc=0x30ED64 r0=0x%X r1=0x%X lr=0x%X evidence=OBSERVED\n", r0,
               r1, lr);
        fflush(stdout);
        product_runtime_progress_emit("lifecycle_compare", "lrt", "ret");
    } else if (tag == 5) { /* 0x30ED7A / 0x30ED7C success */
        g_saw_ok = 1;
        sample_er_gates(uc, er, NULL, NULL, &b70, &b71, NULL, NULL);
        printf("[LRT_B71_SUCCESS_PATH] pc=0x%X B71=%u B70=%u lr=0x%X natural_written=%d "
               "store_pc=0x%X evidence=OBSERVED\n",
               pc, (unsigned)b71, (unsigned)b70, lr, g_b71_natural, g_b71_store_pc);
        fflush(stdout);
    } else if (tag == 6) { /* 0x30ED82 fail */
        g_saw_fail = 1;
        sample_er_gates(uc, er, NULL, NULL, NULL, &b71, NULL, NULL);
        printf("[LRT_30ED82_FAIL] pc=0x30ED82 B71=%u lr=0x%X evidence=OBSERVED\n",
               (unsigned)b71, lr);
        fflush(stdout);
        product_runtime_progress_emit("lifecycle_match_fail", "lrt", "0x30ED82");
    } else if (tag == 7) { /* 0x2FC26C */
        g_saw_2fc26c = 1;
        sample_er_gates(uc, er, &b58, NULL, &b70, &b71, NULL, &ui);
        printf("[LRT_2FC26C_ALT] pc=0x2FC26C lr=0x%X r0=0x%X B58=0x%X B70=%u B71=%u "
               "UI_MODE=0x%X evidence=OBSERVED\n",
               lr, r0, b58, (unsigned)b70, (unsigned)b71, ui);
        fflush(stdout);
    } else if (tag == 8) { /* 0x2FC418 */
        g_saw_2fc418 = 1;
        sample_er_gates(uc, er, NULL, NULL, NULL, NULL, NULL, &ui);
        printf("[LRT_2FC418_UI] pc=0x2FC418 lr=0x%X UI_MODE=0x%X evidence=OBSERVED\n", lr, ui);
        fflush(stdout);
        product_runtime_progress_emit("lifecycle_ui_writer", "lrt", "0x2FC418");
    } else if (tag == 9) { /* 0x2D96BC lookup entry/return observe at call site already */
        printf("[LRT_2D96BC] pc=0x2D96BC r0=0x%X r1=0x%X lr=0x%X evidence=OBSERVED\n", r0, r1,
               lr);
        fflush(stdout);
    } else if (tag == 10) { /* 0x2F6C44 compare */
        printf("[LRT_2F6C44] pc=0x2F6C44 r0=0x%X r1=0x%X lr=0x%X evidence=OBSERVED\n", r0, r1,
               lr);
        fflush(stdout);
    }
}

static void on_lrt_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                             int64_t value, void *user) {
    uint32_t pc = 0;
    uint8_t old_b = 0;
    (void)type;
    (void)user;
    if (!product_lrt_enabled() || !g_er_rw) return;
    if ((uint32_t)address != g_er_rw + OFF_B71) return;
    if (size < 1) return;
    /* 0x2FE854 inside 0x30CBBC clears B71; only treat nonzero stores as success. */
    if ((uint8_t)(value & 0xff) == 0) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    (void)peek_bytes(uc, (uint32_t)address, &old_b, 1);
    g_b71_old = old_b;
    g_b71_new = (uint8_t)(value & 0xff);
    g_b71_store_pc = pc;
    g_b71_natural = 1;
    printf("[B71_NATURALLY_WRITTEN] pc=0x%X addr=0x%X old=%u new=%u size=%d "
           "evidence=OBSERVED\n",
           pc, (uint32_t)address, (unsigned)g_b71_old, (unsigned)g_b71_new, size);
    fflush(stdout);
    product_runtime_progress_emit("b71_naturally_written", "lrt", "guest_store");
    /* V75: exit current guest before empty-B58 → 2FC26C; top-level runs 0x2FEBBC → B70. */
    gwy_ext_obs_on_b71_natural_for_b70(uc, g_er_rw);
}
#endif

void product_lrt_arm_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_hook h1 = 0, h2 = 0, h3 = 0, h4 = 0, h5a = 0, h5b = 0, h6 = 0, h7 = 0, h8 = 0;
    uc_hook h9 = 0, h10 = 0, hm = 0;
    if (!product_lrt_enabled() || !uc || g_hook_ok) return;
    g_uc = uc;
    (void)uc_hook_add((uc_engine *)uc, &h1, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)1,
                      (uint64_t)PC_2DADC4, (uint64_t)PC_2DADC4 + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h2, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)2,
                      (uint64_t)PC_30ED2C, (uint64_t)PC_30ED2C + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h3, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)3,
                      (uint64_t)PC_30ED4C, (uint64_t)PC_30ED4C + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h4, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)4,
                      (uint64_t)PC_30ED64, (uint64_t)PC_30ED64 + 1ull);
    {
        uc_hook h11 = 0;
        (void)uc_hook_add((uc_engine *)uc, &h11, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)11,
                          0x30ED52ull, 0x30ED52ull + 1ull);
    }
    (void)uc_hook_add((uc_engine *)uc, &h5a, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)5,
                      (uint64_t)PC_30ED7A, (uint64_t)PC_30ED7A + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h5b, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)5,
                      (uint64_t)PC_30ED7C, (uint64_t)PC_30ED7C + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h6, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)6,
                      (uint64_t)PC_30ED82, (uint64_t)PC_30ED82 + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h7, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)7,
                      (uint64_t)PC_2FC26C, (uint64_t)PC_2FC26C + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h8, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)8,
                      (uint64_t)PC_2FC418, (uint64_t)PC_2FC418 + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h9, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)9,
                      (uint64_t)PC_2D96BC, (uint64_t)PC_2D96BC + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h10, UC_HOOK_CODE, on_lrt_code, (void *)(intptr_t)10,
                      (uint64_t)PC_2F6C44, (uint64_t)PC_2F6C44 + 1ull);
    if (g_er_rw) {
        uint64_t a = (uint64_t)g_er_rw + (uint64_t)OFF_B71;
        if (uc_hook_add((uc_engine *)uc, &hm, UC_HOOK_MEM_WRITE, on_lrt_mem_write, NULL, a, a) ==
            UC_ERR_OK)
            g_mem_hook_ok = 1;
    }
    g_hook_ok = 1;
    printf("[LRT] hooks armed er_rw=0x%X evidence=OBSERVED\n", g_er_rw);
    fflush(stdout);
#else
    (void)uc;
#endif
}

void product_lrt_finalize(void) {
    FILE *f;
    char path[512];
    const char *dir;
    if (g_finalized || !product_lrt_enabled()) return;
    g_finalized = 1;
    dir = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (dir && dir[0])
        snprintf(path, sizeof(path), "%s/lifecycle_record_trace.md", dir);
    else
        snprintf(path, sizeof(path), "reports/lifecycle_record_trace.md");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# Lifecycle record trace\n\n");
    fprintf(f, "- 0x2DADC4 entered: %s\n", g_saw_2dadc4 ? "yes" : "no");
    fprintf(f, "- 0x30ED2C entered: %s\n", g_saw_30ed2c ? "yes" : "no");
    fprintf(f, "- lookup (0x30ED4C): %s\n", g_saw_lookup ? "yes" : "no");
    fprintf(f, "- compare (0x30ED64): %s\n", g_saw_compare ? "yes" : "no");
    fprintf(f, "- success path (0x30ED7A/7C): %s\n", g_saw_ok ? "yes" : "no");
    fprintf(f, "- fail path (0x30ED82): %s\n", g_saw_fail ? "yes" : "no");
    fprintf(f, "- 0x2FC26C alt: %s\n", g_saw_2fc26c ? "yes" : "no");
    fprintf(f, "- 0x2FC418 UI writer: %s\n", g_saw_2fc418 ? "yes" : "no");
    fprintf(f, "- B71_NATURALLY_WRITTEN: %s store_pc=0x%X old=%u new=%u\n",
            g_b71_natural ? "yes" : "no", g_b71_store_pc, (unsigned)g_b71_old,
            (unsigned)g_b71_new);
    fclose(f);
    printf("[LRT_FINALIZE] 2DADC4=%d 30ED2C=%d lookup=%d compare=%d ok=%d fail=%d "
           "2FC26C=%d 2FC418=%d B71_natural=%d path=%s evidence=OBSERVED\n",
           g_saw_2dadc4, g_saw_30ed2c, g_saw_lookup, g_saw_compare, g_saw_ok, g_saw_fail,
           g_saw_2fc26c, g_saw_2fc418, g_b71_natural, path);
    fflush(stdout);
}
