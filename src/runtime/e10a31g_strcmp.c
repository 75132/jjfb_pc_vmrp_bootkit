#include "gwy_launcher/e10a31g_strcmp.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define E10A31G_PC_READ_BL 0x2E5396u
#define E10A31G_PC_MEMCPY_BLX 0x2E31AEu
#define E10A31G_PC_STRCMP_BLX 0x2E53A6u
#define E10A31G_PC_STRCMP_ENTER 0xAC2D0u
#define E10A31G_PC_TRUE_FAIL 0xAC2E8u
#define E10A31G_GPT_OFF 0x349u

static struct {
    int known;
    int enabled;
    unsigned long long run_id;
    FILE *csv;
    int saw_read;
    int saw_memcpy;
    int saw_strcmp;
    int saw_true_fail;
    uint32_t read_id;
    uint32_t read_dst;
    uint32_t read_len;
    uint32_t memcpy_dst;
    uint32_t memcpy_src;
    uint32_t memcpy_len;
    uint32_t memcpy_src_base;
    uint8_t src_bytes[8];
    uint8_t dst_bytes[8];
    char lhs[16];
    char rhs[16];
    int src_ok;
    int dst_ok;
} g_g;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31G_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A31F_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
}

static void ensure(void) {
    if (g_g.known) return;
    g_g.known = 1;
    g_g.enabled = env1("JJFB_E10A31G_MODE");
    g_g.run_id = run_id_now();
}

static FILE *csv(void) {
    const char *p;
    if (g_g.csv) return g_g.csv;
    p = getenv("JJFB_E10A31G_CSV");
    if (!p || !p[0]) p = "reports/e10a31g_strcmp_arg_trace.csv";
    g_g.csv = fopen(p, "w");
    if (!g_g.csv) return NULL;
    fputs("run_id,event,pc,lr,r0,r1,r2,r3,r9,ptr,hex,ascii,note\n", g_g.csv);
    fflush(g_g.csv);
    return g_g.csv;
}

static void ascii8(const uint8_t *b, int n, char *out, size_t out_n) {
    size_t i;
    size_t w = 0;
    if (!out || out_n == 0) return;
    for (i = 0; i < (size_t)n && w + 1 < out_n; i++) {
        unsigned c = b[i];
        out[w++] = (c >= 32 && c < 127) ? (char)c : '.';
    }
    out[w] = 0;
}

static void hex8(const uint8_t *b, int n, char *out, size_t out_n) {
    static const char *H = "0123456789ABCDEF";
    size_t i;
    size_t w = 0;
    if (!out || out_n < 3) return;
    for (i = 0; i < (size_t)n && w + 2 < out_n; i++) {
        out[w++] = H[(b[i] >> 4) & 0xF];
        out[w++] = H[b[i] & 0xF];
    }
    out[w] = 0;
}

static int read_guest(void *uc, uint32_t addr, uint8_t *dst, size_t n) {
#ifdef GWY_HAVE_UNICORN
    if (!uc || !dst || !n) return 0;
    return uc_mem_read((uc_engine *)uc, addr, dst, n) == UC_ERR_OK;
#else
    (void)uc;
    (void)addr;
    (void)dst;
    (void)n;
    return 0;
#endif
}

static void emit(const char *event, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                 uint32_t r2, uint32_t r3, uint32_t r9, uint32_t ptr, const uint8_t *bytes,
                 int n, const char *note) {
    FILE *f = csv();
    char hx[32];
    char as[16];
    hx[0] = 0;
    as[0] = 0;
    if (bytes && n > 0) {
        hex8(bytes, n, hx, sizeof(hx));
        ascii8(bytes, n, as, sizeof(as));
    }
    if (f) {
        fprintf(f, "%llu,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s,%s,\"%s\"\n", g_g.run_id,
                event ? event : "", pc, lr, r0, r1, r2, r3, r9, ptr, hx, as, note ? note : "");
        fflush(f);
    }
    printf("[JJFB_E10A31G] event=%s pc=0x%X r0=0x%X r1=0x%X r2=0x%X r9=0x%X ptr=0x%X hex=%s "
           "ascii=%s note=%s evidence=OBSERVED\n",
           event ? event : "", pc, r0, r1, r2, r9, ptr, hx, as, note ? note : "");
    fflush(stdout);
}

int e10a31g_enabled(void) {
    ensure();
    return g_g.enabled;
}

void e10a31g_reset(void) {
    if (g_g.csv) fclose(g_g.csv);
    memset(&g_g, 0, sizeof(g_g));
}

void e10a31g_mark_milestone(const char *name, const char *note) {
    ensure();
    if (!g_g.enabled || !name) return;
    printf("[JJFB_E10A31G_MILESTONE] name=%s note=%s evidence=OBSERVED\n", name,
           note ? note : "");
    fflush(stdout);
}

void e10a31g_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr,
                             const uint8_t *bytes, uint32_t size) {
    (void)r4;
    (void)cpsr;
    (void)bytes;
    (void)size;
    ensure();
    if (!g_g.enabled) return;

    if (pc == E10A31G_PC_READ_BL) {
        g_g.saw_read = 1;
        g_g.read_id = r0;
        g_g.read_dst = r1;
        g_g.read_len = r2;
        emit("GPT_FIELD_READ_CALL", pc, lr, r0, r1, r2, r3, r9, r1, NULL, 0,
             "id_dst_len");
        if (r0 == E10A31G_GPT_OFF && r2 == 3)
            e10a31g_mark_milestone("GPT_TAG_FIELD_READ_OFF_349_LEN_3", "pre_strcmp");
    }

    if (pc == E10A31G_PC_MEMCPY_BLX) {
        uint8_t src[8];
        uint8_t base16[16];
        uint32_t obj = r4;
        uint32_t slot = 0;
        uint32_t glob = 0x2D431Cu;
        uint32_t obj_from_glob = 0;
        memset(src, 0, sizeof(src));
        memset(base16, 0, sizeof(base16));
        g_g.saw_memcpy = 1;
        g_g.memcpy_dst = r0;
        g_g.memcpy_src = r1;
        g_g.memcpy_len = r2;
        g_g.memcpy_src_base = (r1 >= E10A31G_GPT_OFF) ? (r1 - E10A31G_GPT_OFF) : 0;
        g_g.src_ok = read_guest(uc, r1, src, 8);
        if (g_g.src_ok) memcpy(g_g.src_bytes, src, 8);
        emit("GPT_FIELD_MEMCPY", pc, lr, r0, r1, r2, r3, r9, r1, g_g.src_ok ? src : NULL,
             g_g.src_ok ? 8 : 0, "src_base_plus_349");

        /* Pointer chain: global+0x38 -> object; object+0x1C0 -> buffer_base; r4 live = object. */
        if (read_guest(uc, glob + 0x38u, (uint8_t *)&obj_from_glob, 4)) {
            emit("GPT_OBJ_FROM_GLOBAL", pc, lr, glob, obj_from_glob, 0x38u, 0, r9, glob + 0x38u,
                 (const uint8_t *)&obj_from_glob, 4, "*(0x2D431C+0x38)");
        }
        if (obj) {
            emit("GPT_OBJ_R4", pc, lr, obj, 0, 0, 0, r9, obj, NULL, 0, "r4_object");
            if (read_guest(uc, obj + 0x1C0u, (uint8_t *)&slot, 4)) {
                emit("GPT_BUF_SLOT", pc, lr, obj, slot, 0x1C0u, 0, r9, obj + 0x1C0u,
                     (const uint8_t *)&slot, 4, "*(object+0x1C0)");
                if (slot == g_g.memcpy_src_base)
                    e10a31g_mark_milestone("GPT_BUF_BASE_FROM_OBJ_1C0", "matches_memcpy_src_base");
                else
                    e10a31g_mark_milestone("GPT_BUF_BASE_SLOT_MISMATCH", "check_encoding");
            }
        }
        if (g_g.memcpy_src_base && read_guest(uc, g_g.memcpy_src_base, base16, 16)) {
            emit("GPT_BUF_BASE_BYTES", pc, lr, g_g.memcpy_src_base, 0, 16, 0, r9, g_g.memcpy_src_base,
                 base16, 16, "16_at_base");
        }
        if (r9) {
            uint32_t w8d0 = 0, w8d4 = 0, w8d8 = 0;
            if (read_guest(uc, r9 + 0x8D0u, (uint8_t *)&w8d0, 4) &&
                read_guest(uc, r9 + 0x8D4u, (uint8_t *)&w8d4, 4) &&
                read_guest(uc, r9 + 0x8D8u, (uint8_t *)&w8d8, 4)) {
                emit("GPT_ERW_8D0_FAMILY", pc, lr, w8d0, w8d4, w8d8, 0, r9, r9 + 0x8D0u, NULL, 0,
                     "vals_at_8d0_8d4_8d8");
                printf("[JJFB_E10A31G] erw+0x8D0=0x%X +0x8D4=0x%X +0x8D8=0x%X "
                       "buf_base=0x%X evidence=OBSERVED\n",
                       w8d0, w8d4, w8d8, g_g.memcpy_src_base);
                fflush(stdout);
                if (g_g.memcpy_src_base == r9 + 0x8D4u)
                    e10a31g_mark_milestone("GPT_BUF_BASE_EQUALS_ADDR_OF_ERW_8D4",
                                           "not_deref_8d8");
                if (w8d8 && w8d8 != g_g.memcpy_src_base)
                    e10a31g_mark_milestone("GPT_WORKBUF_8D8_DIFFERS_FROM_GPT_BASE", "check_alias");
            }
        }
        if (g_g.src_ok && src[0] == 0 && src[1] == 0 && src[2] == 0)
            e10a31g_mark_milestone("GPT_SOURCE_BYTES_ARE_NUL", "off_349");
        else if (g_g.src_ok && src[0] == 'G' && src[1] == 'P' && src[2] == 'T')
            e10a31g_mark_milestone("GPT_SOURCE_BYTES_MATCH_TAG", "off_349");
        else if (g_g.src_ok)
            e10a31g_mark_milestone("GPT_SOURCE_BYTES_MISMATCH_TAG", "off_349");
        printf("[JJFB_E10A31G] memcpy_src_base=0x%X erw_delta=0x%X obj=0x%X slot=0x%X "
               "(r9=0x%X) evidence=OBSERVED\n",
               g_g.memcpy_src_base,
               (r9 && g_g.memcpy_src_base >= r9) ? (g_g.memcpy_src_base - r9) : 0u, obj, slot, r9);
        fflush(stdout);
    }

    if (pc == E10A31G_PC_STRCMP_BLX || pc == E10A31G_PC_STRCMP_ENTER) {
        uint8_t lhs[8];
        uint8_t rhs[8];
        memset(lhs, 0, sizeof(lhs));
        memset(rhs, 0, sizeof(rhs));
        g_g.saw_strcmp = 1;
        g_g.dst_ok = read_guest(uc, r0, lhs, 8);
        g_g.src_ok = read_guest(uc, r1, rhs, 8) || g_g.src_ok;
        if (g_g.dst_ok) {
            memcpy(g_g.dst_bytes, lhs, 8);
            ascii8(lhs, 8, g_g.lhs, sizeof(g_g.lhs));
        }
        if (read_guest(uc, r1, rhs, 8)) ascii8(rhs, 8, g_g.rhs, sizeof(g_g.rhs));
        emit(pc == E10A31G_PC_STRCMP_ENTER ? "STRCMP_ENTER" : "STRCMP_CALL", pc, lr, r0, r1, r2,
             r3, r9, r0, g_g.dst_ok ? lhs : NULL, g_g.dst_ok ? 8 : 0, "lhs");
        emit("STRCMP_RHS", pc, lr, r0, r1, r2, r3, r9, r1, rhs, 8, "rhs_expect_GPT");
        if (g_g.dst_ok && lhs[0] == 0)
            e10a31g_mark_milestone("STRCMP_LHS_EMPTY_VS_GPT", "causal");
        if (rhs[0] == 'G' && rhs[1] == 'P' && rhs[2] == 'T')
            e10a31g_mark_milestone("STRCMP_RHS_IS_GPT_LITERAL", "gamelist_rodata");
    }

    if (pc == E10A31G_PC_TRUE_FAIL) {
        g_g.saw_true_fail = 1;
        e10a31g_mark_milestone("TRUE_FAIL_STRCMP_NEG1_AT_AC2E8", "cfunction");
        emit("TRUE_FAIL", pc, lr, r0, r1, r2, r3, r9, 0, NULL, 0, "MOVCS/MVNCC_path");
    }
}

void e10a31g_on_method0_return(void *uc, uint32_t helper, int32_t ret) {
    (void)uc;
    (void)helper;
    ensure();
    if (!g_g.enabled) return;
    printf("[JJFB_E10A31G] method0_return ret=%d saw_read=%d saw_memcpy=%d saw_strcmp=%d "
           "saw_true_fail=%d lhs=%s rhs=%s src_base=0x%X evidence=OBSERVED\n",
           (int)ret, g_g.saw_read, g_g.saw_memcpy, g_g.saw_strcmp, g_g.saw_true_fail, g_g.lhs,
           g_g.rhs, g_g.memcpy_src_base);
    fflush(stdout);
    if (g_g.saw_true_fail && g_g.saw_strcmp)
        e10a31g_mark_milestone("METHOD0_FAIL_IS_GPT_TAG_MISMATCH", "not_appinfo");
}
