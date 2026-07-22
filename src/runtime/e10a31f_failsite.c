#include "gwy_launcher/e10a31f_failsite.h"

#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define E10A31F_DENSE_MAX 4096
#define E10A31F_WIN_LO 0x2E1BBCu
#define E10A31F_WIN_HI 0x2E1C80u
#define E10A31F_SENTINEL_PC 0x2E1C24u

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t lr;
    uint32_t r0, r1, r2, r3, r4, r9;
    uint32_t cpsr;
    uint32_t size;
    uint8_t bytes[4];
    char note[64];
} DenseRow;

static struct {
    int known;
    int enabled;
    int continue_sentinel;
    int dense;
    int abi_filebuf;
    unsigned long long run_id;
    FILE *branch_csv;
    FILE *dense_csv;
    FILE *abi_csv;
    DenseRow dense_rows[E10A31F_DENSE_MAX];
    uint32_t dense_n;
    int saw_sentinel;
    int saw_sentinel_strh;
    uint32_t post_sentinel_insns;
    uint32_t true_fail_pc;
    char true_fail_class[48];
    int32_t helper_ret;
} g_f;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31F_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A31E_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
}

static FILE *open_csv(const char *env_key, const char *fallback, const char *hdr) {
    const char *p = getenv(env_key);
    FILE *f;
    if (!p || !p[0]) p = fallback;
    f = fopen(p, "w");
    if (!f) return NULL;
    fputs(hdr, f);
    fflush(f);
    return f;
}

static void ensure(void) {
    if (g_f.known) return;
    g_f.known = 1;
    g_f.enabled = env1("JJFB_E10A31F_MODE") || env1("JJFB_E10A31F_CONTINUE_PAST_SENTINEL") ||
                  env1("JJFB_E10A31F_DENSE_WINDOW") || env1("JJFB_E10A31F_ABI_FILEBUF");
    g_f.continue_sentinel =
        env1("JJFB_E10A31F_CONTINUE_PAST_SENTINEL") || env1("JJFB_E10A31F_MODE");
    g_f.dense = env1("JJFB_E10A31F_DENSE_WINDOW") || env1("JJFB_E10A31F_MODE");
    g_f.abi_filebuf = env1("JJFB_E10A31F_ABI_FILEBUF");
    g_f.run_id = run_id_now();
}

int e10a31f_enabled(void) {
    ensure();
    return g_f.enabled;
}

int e10a31f_continue_past_sentinel(void) {
    ensure();
    return g_f.enabled && g_f.continue_sentinel;
}

int e10a31f_abi_filebuf(void) {
    ensure();
    return g_f.enabled && g_f.abi_filebuf;
}

void e10a31f_reset(void) {
    if (g_f.branch_csv) fclose(g_f.branch_csv);
    if (g_f.dense_csv) fclose(g_f.dense_csv);
    if (g_f.abi_csv) fclose(g_f.abi_csv);
    memset(&g_f, 0, sizeof(g_f));
}

void e10a31f_mark_milestone(const char *name, const char *note) {
    ensure();
    if (!g_f.enabled || !name) return;
    printf("[JJFB_E10A31F_MILESTONE] name=%s note=%s evidence=OBSERVED\n", name,
           note ? note : "");
    fflush(stdout);
}

int e10a31f_is_neg1_sentinel_store(uint32_t pc, const uint8_t *bytes, uint32_t size) {
    uint16_t h;
    if (pc != E10A31F_SENTINEL_PC || !bytes || size < 2) return 0;
    h = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    /* Thumb MVNS r0, r4 = 0x43E0 (stored LE as E0 43) */
    return h == 0x43E0u;
}

static void note_insn(const char *note, uint32_t pc, uint32_t r0, uint32_t r4, uint32_t r9) {
    if (!g_f.branch_csv) {
        g_f.branch_csv = open_csv(
            "JJFB_E10A31F_BRANCH_CSV", "reports/e10a31f_fail_branch_trace.csv",
            "run_id,seq,pc,note,r0,r1,r2,r3,r4,r9,cpsr\n");
    }
    if (g_f.branch_csv) {
        fprintf(g_f.branch_csv, "%llu,%u,0x%X,\"%s\",0x%X,0,0,0,0x%X,0x%X,0\n", g_f.run_id,
                g_f.dense_n, pc, note ? note : "", r0, r4, r9);
        fflush(g_f.branch_csv);
    }
}

void e10a31f_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr,
                             const uint8_t *bytes, uint32_t size) {
    int in_win;
    (void)uc;
    (void)lr;
    (void)r1;
    (void)r2;
    (void)r3;
    (void)cpsr;
    ensure();
    if (!g_f.enabled) return;

    in_win = (pc >= E10A31F_WIN_LO && pc < E10A31F_WIN_HI);

    if (e10a31f_is_neg1_sentinel_store(pc, bytes, size)) {
        g_f.saw_sentinel = 1;
        e10a31f_mark_milestone("FAILSITE_2E1C24_IS_NEG1_SENTINEL_STORE", "MVNS_r0_r4");
        e10a31f_mark_milestone("FAILSITE_NO_BRANCH_PREDICATE_INTO_2E1C24", "fallthrough");
        e10a31f_mark_milestone("E10A31D_FIRST_FAILURE_FALSE_POSITIVE", "sentinel");
        note_insn("SENTINEL_MVNS_r0_r4", pc, r0, r4, r9);
        printf("[JJFB_E10A31F] sentinel pc=0x%X r4=0x%X r0_before=0x%X r9=0x%X "
               "evidence=DOCUMENTED\n",
               pc, r4, r0, r9);
        fflush(stdout);
    }

    /* STRH r0,[r1,#0] = 0x8008 immediately after sentinel */
    if (g_f.saw_sentinel && pc == 0x2E1C26u && bytes && size >= 2) {
        uint16_t h = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
        if (h == 0x8008u) {
            g_f.saw_sentinel_strh = 1;
            e10a31f_mark_milestone("FAILSITE_NEG1_STORED_AS_HALWORD", "STRH_r0_r1");
            note_insn("SENTINEL_STRH", pc, r0, r4, r9);
        }
    }

    if (g_f.saw_sentinel) g_f.post_sentinel_insns++;

    if (g_f.dense && in_win && g_f.dense_n < E10A31F_DENSE_MAX) {
        DenseRow *row = &g_f.dense_rows[g_f.dense_n++];
        memset(row, 0, sizeof(*row));
        row->seq = g_f.dense_n;
        row->pc = pc;
        row->lr = lr;
        row->r0 = r0;
        row->r1 = r1;
        row->r2 = r2;
        row->r3 = r3;
        row->r4 = r4;
        row->r9 = r9;
        row->cpsr = cpsr;
        row->size = size;
        if (bytes && size) memcpy(row->bytes, bytes, size > 4 ? 4 : size);
        if (pc == E10A31F_SENTINEL_PC)
            snprintf(row->note, sizeof(row->note), "SENTINEL_MVNS");
        else if (pc == 0x2E1BC0u)
            snprintf(row->note, sizeof(row->note), "MOVS_r4_0");
        else
            snprintf(row->note, sizeof(row->note), "dense");
    }

    /* LDR [r9, #imm] style via ADD r0,r9 then LDR — log R9+0x920 touches if any */
    if (bytes && size >= 2) {
        uint16_t h = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
        /* ADD r0, r9 = 0x4448 */
        if (h == 0x4448u && r9) {
            /* upcoming offset materializes in r0 after ADD; log base */
            if (r0 + 0 /* after add will be r9+lit */ ) {
                /* observational: any ADD r0,r9 in window */
                if (in_win) note_insn("ADD_r0_r9", pc, r0, r4, r9);
            }
        }
    }
}

void e10a31f_on_method0_return(void *uc, uint32_t helper, int32_t ret) {
    uint32_t i;
    (void)uc;
    (void)helper;
    ensure();
    if (!g_f.enabled) return;
    g_f.helper_ret = ret;

    if (!g_f.dense_csv && g_f.dense_n) {
        g_f.dense_csv = open_csv(
            "JJFB_E10A31F_DENSE_CSV", "reports/e10a31f_failsite_dense_trace.csv",
            "run_id,seq,pc,lr,r0,r1,r2,r3,r4,r9,cpsr,size,bytes,note\n");
    }
    if (g_f.dense_csv) {
        for (i = 0; i < g_f.dense_n; i++) {
            DenseRow *r = &g_f.dense_rows[i];
            fprintf(g_f.dense_csv,
                    "%llu,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,"
                    "%02X%02X%02X%02X,\"%s\"\n",
                    g_f.run_id, r->seq, r->pc, r->lr, r->r0, r->r1, r->r2, r->r3, r->r4, r->r9,
                    r->cpsr, r->size, r->bytes[0], r->bytes[1], r->bytes[2], r->bytes[3],
                    r->note);
        }
        fflush(g_f.dense_csv);
    }

    printf("[JJFB_E10A31F] method0_return ret=%d saw_sentinel=%d sentinel_strh=%d "
           "post_sentinel_insns=%u dense_n=%u evidence=OBSERVED\n",
           (int)ret, g_f.saw_sentinel, g_f.saw_sentinel_strh, g_f.post_sentinel_insns,
           g_f.dense_n);
    fflush(stdout);

    if (ret < 0) {
        if (g_f.saw_sentinel && g_f.post_sentinel_insns > 2)
            e10a31f_mark_milestone("FAILSITE_EXECUTION_CONTINUES_PAST_2E1C24", "not_return_gate");
        /* TRUE_FAILURE_PC is emitted by e10a31d when a non-sentinel -1 edge is seen. */
    } else {
        e10a31f_mark_milestone("GAMELIST_METHOD0_RETURN_ZERO", "ret0");
    }
}

uint32_t e10a31f_method0_input_override(uint32_t default_input) {
    ModuleRegistry *reg;
    size_t i;
    ensure();
    if (!g_f.abi_filebuf) return default_input;
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return default_input;
    for (i = 0; i < reg->count; i++) {
        const char *a = reg->modules[i].resolved_name;
        const char *b = reg->modules[i].requested_name;
        if ((a && strstr(a, "gamelist")) || (b && strstr(b, "gamelist"))) {
            uint32_t base = reg->modules[i].map.guest_code_base;
            if (base) {
                if (!g_f.abi_csv) {
                    g_f.abi_csv = open_csv(
                        "JJFB_E10A31F_ABI_CSV", "reports/e10a31f_method0_abi_compare.csv",
                        "run_id,case,input,input_len,note\n");
                }
                if (g_f.abi_csv) {
                    fprintf(g_f.abi_csv, "%llu,filebuf,0x%X,2011,guest_code_base\n", g_f.run_id,
                            base);
                    fflush(g_f.abi_csv);
                }
                e10a31f_mark_milestone("METHOD0_INPUT_ABI_FILEBUF_OVERRIDE", "guest_code_base");
                printf("[JJFB_E10A31F_ABI] override input 0x%X -> 0x%X (gamelist code_base) "
                       "evidence=CROSS_TARGET\n",
                       default_input, base);
                fflush(stdout);
                return base;
            }
        }
    }
    e10a31f_mark_milestone("METHOD0_INPUT_ABI_FILEBUF_UNAVAILABLE", "no_gamelist_base");
    return default_input;
}

/* Called from e10a31d when a candidate "first failure" is about to be recorded. */
int e10a31f_should_ignore_neg1_at(uint32_t fail_pc, uint32_t next_pc, const uint8_t *fail_bytes,
                                  uint32_t fail_size) {
    ensure();
    if (!g_f.continue_sentinel) return 0;
    if (e10a31f_is_neg1_sentinel_store(fail_pc, fail_bytes, fail_size)) return 1;
    if (fail_pc == E10A31F_SENTINEL_PC) return 1;
    (void)next_pc;
    return 0;
}
