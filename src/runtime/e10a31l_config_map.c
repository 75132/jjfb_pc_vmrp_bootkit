#include "gwy_launcher/e10a31l_config_map.h"

#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/gwy_sms_cfg.h"
#include "gwy_launcher/package_scope.h"
#include "gwy_launcher/sha256.h"
#include "gwy_launcher/sms_cfg_compat_profile.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

enum { E10A31L_READ_MAX = 4096, E10A31L_CMP_MAX = 512, E10A31L_COPY_MAX = 256 };

typedef struct {
    uint32_t seq;
    uint32_t pc, lr, sp, r0, r1, r2, r3, r4, r9;
    uint32_t addr;
    uint32_t cfg_off;
    uint32_t size;
    uint8_t bytes[32];
    int later_strcmp;
    char compared_literal[48];
    int causal;
} ReadRow;

typedef struct {
    uint32_t seq;
    uint32_t call_pc, enter_pc, lr;
    uint32_t lhs, rhs;
    uint32_t r9, sp, cfg_base;
    uint8_t lhs_bytes[64];
    uint8_t rhs_bytes[64];
    char lhs_ascii[68];
    char rhs_ascii[68];
    E10a31lLhsSource lhs_source;
    int32_t cfg_offset; /* >=0 if SMSCFG or STACK_FROM_SMSCFG */
    int32_t copy_len;
    int is_gwy;
    int is_gpt;
    int leads_to_neg1;
    char package[64];
    char module[48];
} CmpRow;

typedef struct {
    uint32_t dst;
    uint32_t cfg_off;
    uint32_t len;
    uint32_t pc;
} CopyProv;

static struct {
    int known, enabled, mem_read, compare, active;
    unsigned long long run_id;
    void *uc;
    uint32_t helper, cfg_base, cfg_len, erw_base, mr_table, seq;
    uint32_t param_hint; /* best-effort launch-param guest VA if known */
    int read_n, cmp_n, copy_n;
    ReadRow reads[E10A31L_READ_MAX];
    CmpRow cmps[E10A31L_CMP_MAX];
    CopyProv copies[E10A31L_COPY_MAX];
    E10a31lLhsSource last_gwy_src;
    int saw_gwy_cmp, saw_gpt_pass, true_fail;
    uint32_t true_fail_pc;
    FILE *read_csv, *cmp_csv, *gwy_csv;
#ifdef GWY_HAVE_UNICORN
    uc_hook mem_read_hook;
    int mem_read_armed;
    uint32_t hooked_base;
#endif
} g_l;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31L_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
}

static void ensure(void) {
    if (g_l.known) return;
    g_l.known = 1;
    g_l.enabled = env1("JJFB_E10A31L_MODE");
    g_l.mem_read = g_l.enabled && (env1("JJFB_E10A31L_MEM_READ") || !getenv("JJFB_E10A31L_MEM_READ"));
    g_l.compare = g_l.enabled && (env1("JJFB_E10A31L_COMPARE") || !getenv("JJFB_E10A31L_COMPARE"));
    g_l.run_id = run_id_now();
    g_l.cfg_len = GWY_SMS_CFG_LEN;
    g_l.last_gwy_src = E10A31L_LHS_UNKNOWN;
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

static void ensure_csvs(void) {
    if (!g_l.read_csv)
        g_l.read_csv = open_csv("JJFB_E10A31L_READ_CSV", "reports/e10a31l_method0_smscfg_read_map.csv",
            "run_id,seq,pc,lr,sp,r0,r1,r2,r3,r4,r9,addr,cfg_off,size,bytes_hex,later_strcmp,"
            "compared_literal,causal,note\n");
    if (!g_l.cmp_csv)
        g_l.cmp_csv = open_csv("JJFB_E10A31L_COMPARE_CSV", "reports/e10a31l_method0_compare_chain.csv",
            "run_id,seq,call_pc,enter_pc,lr,lhs,rhs,r9,sp,cfg_base,lhs_source,cfg_offset,copy_len,"
            "lhs_hex,rhs_hex,lhs_ascii,rhs_ascii,is_gwy,is_gpt,leads_to_neg1,package,module\n");
    if (!g_l.gwy_csv)
        g_l.gwy_csv = open_csv("JJFB_E10A31L_GWY_CSV", "reports/e10a31l_gwy_strcmp_provenance.csv",
            "run_id,seq,strcmp_pc,caller_pc,lr,lhs,rhs,lhs_source,cfg_offset,cfg_base,r9,sp,"
            "lhs_hex,rhs_hex,lhs_ascii,rhs_ascii,package,module,verdict\n");
}

static int read_guest(void *uc, uint32_t addr, void *dst, size_t n) {
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

static void hex_n(const uint8_t *b, int n, char *out, size_t out_n) {
    static const char *H = "0123456789ABCDEF";
    size_t i, w = 0;
    if (!out || out_n < 3) return;
    for (i = 0; i < (size_t)n && w + 2 < out_n; i++) {
        out[w++] = H[(b[i] >> 4) & 0xF];
        out[w++] = H[b[i] & 0xF];
    }
    out[w] = 0;
}

static void ascii_n(const uint8_t *b, int n, char *out, size_t out_n) {
    int i;
    if (!out || out_n < 2) return;
    for (i = 0; i < n && (size_t)i + 1 < out_n; i++) {
        unsigned char c = b[i];
        out[i] = (c >= 32 && c < 127) ? (char)c : '.';
    }
    out[i < (int)out_n ? i : (int)out_n - 1] = 0;
}

const char *e10a31l_lhs_source_name(E10a31lLhsSource s) {
    switch (s) {
        case E10A31L_LHS_SMSCFG_FIELD:
            return "SMSCFG_FIELD";
        case E10A31L_LHS_LAUNCH_PARAM:
            return "LAUNCH_PARAM";
        case E10A31L_LHS_DESCRIPTOR_FIELD:
            return "DESCRIPTOR_FIELD";
        case E10A31L_LHS_PACKAGE_NAME:
            return "PACKAGE_NAME";
        case E10A31L_LHS_VFS_PATH:
            return "VFS_PATH";
        case E10A31L_LHS_GLOBAL_PLATFORM_STATE:
            return "GLOBAL_PLATFORM_STATE";
        case E10A31L_LHS_STACK_LOCAL:
            return "STACK_LOCAL";
        case E10A31L_LHS_HEAP_BUFFER:
            return "HEAP_BUFFER";
        case E10A31L_LHS_STACK_FROM_SMSCFG:
            return "STACK_FROM_SMSCFG";
        default:
            return "UNKNOWN";
    }
}

E10a31lLhsSource e10a31l_last_gwy_lhs_source(void) {
    ensure();
    return g_l.last_gwy_src;
}

void e10a31l_mark_milestone(const char *name, const char *note) {
    ensure();
    if (!g_l.enabled || !name) return;
    printf("[JJFB_E10A31L_MILESTONE] name=%s note=%s cfg_base=0x%X evidence=OBSERVED\n", name,
           note ? note : "", g_l.cfg_base);
    fflush(stdout);
}

int e10a31l_enabled(void) {
    ensure();
    return g_l.enabled;
}

void e10a31l_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_l.mem_read_armed && g_l.uc) {
        uc_hook_del((uc_engine *)g_l.uc, g_l.mem_read_hook);
        g_l.mem_read_armed = 0;
    }
#endif
    if (g_l.read_csv) fclose(g_l.read_csv);
    if (g_l.cmp_csv) fclose(g_l.cmp_csv);
    if (g_l.gwy_csv) fclose(g_l.gwy_csv);
    memset(&g_l, 0, sizeof(g_l));
}

static void resolve_cfg(void *uc) {
    const GwySmsCfgState *st = gwy_sms_cfg_state();
    uint32_t mt = ext_chunk_provider_mr_table_guest();
    uint32_t slot = 0;
    if (uc) g_l.uc = uc;
    if (st && st->cfg_guest) {
        g_l.cfg_base = st->cfg_guest;
        g_l.erw_base = st->erw_base;
        g_l.mr_table = st->mr_table;
        return;
    }
    if (mt) g_l.mr_table = mt;
    if (g_l.mr_table && read_guest(uc, g_l.mr_table + GWY_SMS_CFG_MR_TABLE_OFF, &slot, 4) && slot) {
        g_l.cfg_base = slot;
        return;
    }
    if (st && st->erw_base) g_l.cfg_base = st->erw_base + GWY_SMS_CFG_ERW_OFF;
}

static int in_cfg(uint32_t addr, uint32_t len) {
    uint64_t a0, a1, c0, c1;
    if (!g_l.cfg_base || !len) return 0;
    a0 = addr;
    a1 = (uint64_t)addr + len;
    c0 = g_l.cfg_base;
    c1 = (uint64_t)g_l.cfg_base + g_l.cfg_len;
    return a0 < c1 && a1 > c0;
}

static void note_copy(uint32_t dst, uint32_t cfg_off, uint32_t len, uint32_t pc) {
    if (g_l.copy_n >= E10A31L_COPY_MAX) return;
    g_l.copies[g_l.copy_n].dst = dst;
    g_l.copies[g_l.copy_n].cfg_off = cfg_off;
    g_l.copies[g_l.copy_n].len = len;
    g_l.copies[g_l.copy_n].pc = pc;
    g_l.copy_n++;
}

static int find_copy_for_lhs(uint32_t lhs, int32_t *out_off, int32_t *out_len) {
    int i;
    for (i = g_l.copy_n - 1; i >= 0; i--) {
        uint32_t d0 = g_l.copies[i].dst;
        uint32_t d1 = d0 + g_l.copies[i].len;
        if (lhs >= d0 && lhs < d1) {
            if (out_off) *out_off = (int32_t)(g_l.copies[i].cfg_off + (lhs - d0));
            if (out_len) *out_len = (int32_t)(d1 - lhs);
            return 1;
        }
    }
    return 0;
}

static int bytes_look_like_launch_param(const uint8_t *b, int n) {
    char tmp[68];
    const char *lp;
    int i;
    ascii_n(b, n > 64 ? 64 : n, tmp, sizeof(tmp));
    if (strstr(tmp, "napptype") || strstr(tmp, "gwyblink") || strstr(tmp, "nmrpname") ||
        strstr(tmp, "nextid="))
        return 1;
    lp = getenv("GWY_LAUNCH_PARAM");
    if (lp && lp[0]) {
        for (i = 0; lp[i] && i < n; i++) {
            if ((uint8_t)lp[i] != b[i]) break;
        }
        if (i >= 3) return 1;
    }
    return 0;
}

static int bytes_look_like_package(const uint8_t *b, int n) {
    char tmp[68];
    const char *pkg;
    ascii_n(b, n > 64 ? 64 : n, tmp, sizeof(tmp));
    if (strstr(tmp, "gamelist") || strstr(tmp, ".mrp") || strstr(tmp, "gbrwcore") ||
        strstr(tmp, "cfunction"))
        return 1;
    pkg = package_scope_active_package();
    if (pkg && pkg[0] && strstr(tmp, pkg)) return 1;
    return 0;
}

static E10a31lLhsSource classify_lhs(uint32_t lhs, uint32_t sp, const uint8_t *lb, int ln,
                                     int32_t *out_off, int32_t *out_len) {
    int32_t off = -1, clen = 0;
    if (out_off) *out_off = -1;
    if (out_len) *out_len = 0;
    if (g_l.cfg_base && lhs >= g_l.cfg_base && lhs < g_l.cfg_base + g_l.cfg_len) {
        if (out_off) *out_off = (int32_t)(lhs - g_l.cfg_base);
        if (out_len) *out_len = (int32_t)(g_l.cfg_base + g_l.cfg_len - lhs);
        return E10A31L_LHS_SMSCFG_FIELD;
    }
    if (find_copy_for_lhs(lhs, &off, &clen)) {
        if (out_off) *out_off = off;
        if (out_len) *out_len = clen;
        return E10A31L_LHS_STACK_FROM_SMSCFG;
    }
    if (bytes_look_like_launch_param(lb, ln)) return E10A31L_LHS_LAUNCH_PARAM;
    if (bytes_look_like_package(lb, ln)) return E10A31L_LHS_PACKAGE_NAME;
    if (sp && lhs >= (sp > 0x8000u ? sp - 0x8000u : 0) && lhs <= sp + 0x100u)
        return E10A31L_LHS_STACK_LOCAL;
    if (lhs >= 0x200000u && lhs < 0x800000u) return E10A31L_LHS_HEAP_BUFFER;
    return E10A31L_LHS_UNKNOWN;
}

static const char *gwy_verdict_for(E10a31lLhsSource s) {
    switch (s) {
        case E10A31L_LHS_SMSCFG_FIELD:
        case E10A31L_LHS_STACK_FROM_SMSCFG:
            return "GWY_COMPARE_SOURCE_SMSCFG";
        case E10A31L_LHS_LAUNCH_PARAM:
            return "GWY_COMPARE_SOURCE_LAUNCH_PARAM";
        case E10A31L_LHS_DESCRIPTOR_FIELD:
            return "GWY_COMPARE_SOURCE_DESCRIPTOR";
        case E10A31L_LHS_PACKAGE_NAME:
        case E10A31L_LHS_VFS_PATH:
        case E10A31L_LHS_GLOBAL_PLATFORM_STATE:
            return "GWY_COMPARE_SOURCE_PLATFORM_STATE";
        default:
            return "GWY_COMPARE_SOURCE_UNKNOWN";
    }
}

#ifdef GWY_HAVE_UNICORN
static void on_mem_read(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                        void *user_data) {
    uint32_t addr = (uint32_t)address;
    uint32_t pc = 0, lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r9 = 0;
    uint8_t buf[32];
    char hex[72];
    ReadRow *row;
    (void)type;
    (void)value;
    (void)user_data;
    if (!g_l.active || !g_l.mem_read || size <= 0) return;
    if (!in_cfg(addr, (uint32_t)size)) return;
    if (g_l.read_n >= E10A31L_READ_MAX) return;
    memset(buf, 0, sizeof(buf));
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    (void)uc_mem_read(uc, address, buf, size > 32 ? 32 : (size_t)size);
    row = &g_l.reads[g_l.read_n++];
    memset(row, 0, sizeof(*row));
    g_l.seq++;
    row->seq = g_l.seq;
    row->pc = pc;
    row->lr = lr;
    row->sp = sp;
    row->r0 = r0;
    row->r1 = r1;
    row->r2 = r2;
    row->r3 = r3;
    row->r4 = r4;
    row->r9 = r9;
    row->addr = addr;
    row->cfg_off = addr - g_l.cfg_base;
    row->size = (uint32_t)size;
    memcpy(row->bytes, buf, size > 32 ? 32 : (size_t)size);
    hex_n(buf, size > 16 ? 16 : size, hex, sizeof(hex));
    ensure_csvs();
    if (g_l.read_csv) {
        fprintf(g_l.read_csv,
                "%llu,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%s,0,,0,"
                "mem_read\n",
                g_l.run_id, row->seq, pc, lr, sp, r0, r1, r2, r3, r4, r9, addr, row->cfg_off,
                row->size, hex);
        fflush(g_l.read_csv);
    }
}
#endif

static void install_read_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_err err;
    if (!uc || !g_l.cfg_base || !g_l.mem_read) return;
    if (g_l.mem_read_armed && g_l.hooked_base == g_l.cfg_base) return;
    if (g_l.mem_read_armed) {
        uc_hook_del((uc_engine *)uc, g_l.mem_read_hook);
        g_l.mem_read_armed = 0;
    }
    err = uc_hook_add((uc_engine *)uc, &g_l.mem_read_hook, UC_HOOK_MEM_READ, (void *)on_mem_read,
                      NULL, (uint64_t)g_l.cfg_base,
                      (uint64_t)g_l.cfg_base + (uint64_t)g_l.cfg_len - 1ull);
    if (err == UC_ERR_OK) {
        g_l.mem_read_armed = 1;
        g_l.hooked_base = g_l.cfg_base;
        e10a31l_mark_milestone("SMSCFG_READ_HOOK_ARMED", "method0_window");
    } else {
        printf("[JJFB_E10A31L] read_hook_add_failed err=%d cfg_base=0x%X\n", (int)err,
               g_l.cfg_base);
        fflush(stdout);
    }
#else
    (void)uc;
#endif
}

static void disarm_read_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    if (g_l.mem_read_armed && uc) {
        uc_hook_del((uc_engine *)uc, g_l.mem_read_hook);
        g_l.mem_read_armed = 0;
    }
#else
    (void)uc;
#endif
}

static int rhs_is_gwy(const uint8_t *b) {
    return b[0] == 'g' && b[1] == 'w' && b[2] == 'y' && (b[3] == 0 || b[3] == '.');
}

static int rhs_is_gpt(const uint8_t *b) {
    return b[0] == 'G' && b[1] == 'P' && b[2] == 'T' && (b[3] == 0 || b[3] == '.');
}

static void emit_compare(void *uc, uint32_t call_pc, uint32_t enter_pc, uint32_t lr, uint32_t lhs,
                         uint32_t rhs, uint32_t r9, uint32_t sp) {
    CmpRow *c;
    uint8_t lb[64], rb[64];
    char lh[136], rh[136], la[68], ra[68];
    int32_t off = -1, clen = 0;
    E10a31lLhsSource src;
    const char *pkg;
    if (!g_l.compare || g_l.cmp_n >= E10A31L_CMP_MAX) return;
    memset(lb, 0, sizeof(lb));
    memset(rb, 0, sizeof(rb));
    (void)read_guest(uc, lhs, lb, sizeof(lb));
    (void)read_guest(uc, rhs, rb, sizeof(rb));
    src = classify_lhs(lhs, sp, lb, 64, &off, &clen);
    hex_n(lb, 32, lh, sizeof(lh));
    hex_n(rb, 32, rh, sizeof(rh));
    ascii_n(lb, 32, la, sizeof(la));
    ascii_n(rb, 32, ra, sizeof(ra));
    c = &g_l.cmps[g_l.cmp_n++];
    memset(c, 0, sizeof(*c));
    g_l.seq++;
    c->seq = g_l.seq;
    c->call_pc = call_pc;
    c->enter_pc = enter_pc;
    c->lr = lr;
    c->lhs = lhs;
    c->rhs = rhs;
    c->r9 = r9;
    c->sp = sp;
    c->cfg_base = g_l.cfg_base;
    c->lhs_source = src;
    c->cfg_offset = off;
    c->copy_len = clen;
    memcpy(c->lhs_bytes, lb, 64);
    memcpy(c->rhs_bytes, rb, 64);
    snprintf(c->lhs_ascii, sizeof(c->lhs_ascii), "%s", la);
    snprintf(c->rhs_ascii, sizeof(c->rhs_ascii), "%s", ra);
    c->is_gwy = rhs_is_gwy(rb);
    c->is_gpt = rhs_is_gpt(rb);
    pkg = package_scope_active_package();
    snprintf(c->package, sizeof(c->package), "%s", pkg ? pkg : "");
    snprintf(c->module, sizeof(c->module), "gamelist");
    if (c->is_gpt && (lb[0] == 'G' && lb[1] == 'P' && lb[2] == 'T')) g_l.saw_gpt_pass = 1;
    if (c->is_gwy) {
        g_l.saw_gwy_cmp = 1;
        g_l.last_gwy_src = src;
        e10a31l_mark_milestone(gwy_verdict_for(src), la);
        ensure_csvs();
        if (g_l.gwy_csv) {
            fprintf(g_l.gwy_csv,
                    "%llu,%u,0x%X,0x%X,0x%X,0x%X,0x%X,%s,%d,0x%X,0x%X,0x%X,%s,%s,\"%s\",\"%s\","
                    "\"%s\",\"%s\",%s\n",
                    g_l.run_id, c->seq, enter_pc, call_pc, lr, lhs, rhs,
                    e10a31l_lhs_source_name(src), (int)off, g_l.cfg_base, r9, sp, lh, rh, la, ra,
                    c->package, c->module, gwy_verdict_for(src));
            fflush(g_l.gwy_csv);
        }
        printf("[JJFB_E10A31L_GWY] lhs=0x%X rhs=0x%X source=%s cfg_offset=%d lhs_ascii=%s "
               "rhs_ascii=%s verdict=%s evidence=OBSERVED\n",
               lhs, rhs, e10a31l_lhs_source_name(src), (int)off, la, ra, gwy_verdict_for(src));
        fflush(stdout);
    }
    ensure_csvs();
    if (g_l.cmp_csv) {
        fprintf(g_l.cmp_csv,
                "%llu,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s,%d,%d,%s,%s,\"%s\",\"%s\",%d,"
                "%d,0,\"%s\",\"%s\"\n",
                g_l.run_id, c->seq, call_pc, enter_pc, lr, lhs, rhs, r9, sp, g_l.cfg_base,
                e10a31l_lhs_source_name(src), (int)off, (int)clen, lh, rh, la, ra, c->is_gwy,
                c->is_gpt, c->package, c->module);
        fflush(g_l.cmp_csv);
    }
}

static void mark_recent_cmp_causal(void) {
    int i;
    for (i = g_l.cmp_n - 1; i >= 0; i--) {
        if (!g_l.cmps[i].leads_to_neg1) {
            g_l.cmps[i].leads_to_neg1 = 1;
            break;
        }
    }
}

static void write_annotated_chain(void) {
    FILE *f;
    int i;
    const char *path = getenv("JJFB_E10A31L_ANNOTATED");
    if (!path || !path[0]) path = "out/e10a31l/method0_config_gate_chain_annotated.txt";
#ifdef _WIN32
    _mkdir("out");
    _mkdir("out/e10a31l");
#else
    mkdir("out", 0755);
    mkdir("out/e10a31l", 0755);
#endif
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# E10A-3.1l method0 config gate chain (dynamic)\n");
    fprintf(f, "cfg_base=0x%X cfg_len=0x%X gpt_pass=%d gwy_cmp=%d true_fail_pc=0x%X\n\n",
            g_l.cfg_base, g_l.cfg_len, g_l.saw_gpt_pass, g_l.saw_gwy_cmp, g_l.true_fail_pc);
    for (i = 0; i < g_l.cmp_n; i++) {
        CmpRow *c = &g_l.cmps[i];
        fprintf(f,
                "REQ_%d:\n  call_pc=0x%X enter_pc=0x%X\n  lhs=0x%X rhs=0x%X\n  source=%s\n"
                "  cfg_offset=%d copy_len=%d\n  lhs_ascii=%s\n  rhs_ascii=%s\n  is_gpt=%d "
                "is_gwy=%d leads_to_neg1=%d\n\n",
                i + 1, c->call_pc, c->enter_pc, c->lhs, c->rhs,
                e10a31l_lhs_source_name(c->lhs_source), (int)c->cfg_offset, (int)c->copy_len,
                c->lhs_ascii, c->rhs_ascii, c->is_gpt, c->is_gwy, c->leads_to_neg1);
    }
    fprintf(f, "NOTE: literal adjacency gwy\\\\0GPT\\\\0 is NOT sufficient provenance.\n");
    fclose(f);

    f = fopen("out/e10a31l/method0_config_gate_cfg.dot", "w");
    if (f) {
        fprintf(f, "digraph method0_config_gates {\n  rankdir=LR;\n");
        for (i = 0; i < g_l.cmp_n; i++) {
            CmpRow *c = &g_l.cmps[i];
            fprintf(f, "  r%d [label=\"%s\\n%s vs %s\"];\n", i + 1,
                    e10a31l_lhs_source_name(c->lhs_source), c->lhs_ascii, c->rhs_ascii);
            if (i > 0) fprintf(f, "  r%d -> r%d;\n", i, i + 1);
        }
        fprintf(f, "}\n");
        fclose(f);
    }
}

static void write_manifest(void) {
    FILE *f;
    char sha_hex[65];
    uint8_t sha[32];
    int i, req_n = 0;
    const char *path = getenv("JJFB_E10A31L_MANIFEST_JSON");
    const GwySmsCfgState *st = gwy_sms_cfg_state();
    if (!path || !path[0]) path = "reports/e10a31l_required_platform_tags.json";
    memset(sha, 0, sizeof(sha));
    if (st) memcpy(sha, st->cfunction_sha256, 32);
    if (sha[0] == 0 && sha[1] == 0) {
        if (gwy_sha256_file("cfunction.ext", sha) != 1) {
            const char *root = getenv("GWY_RESOURCE_ROOT");
            char p[640];
            if (root && root[0]) {
                snprintf(p, sizeof(p), "%s/cfunction.ext", root);
                (void)gwy_sha256_file(p, sha);
            }
        }
    }
    gwy_sha256_hex(sha, sha_hex);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
            "{\n  \"platform_fingerprint\": {\n    \"cfunction_sha256\": \"%s\",\n"
            "    \"mr_version\": %u,\n    \"sms_cfg_len\": %u\n  },\n"
            "  \"original_default_recovered\": false,\n  \"requirements\": [\n",
            sha_hex, GWY_SMS_CFG_MR_VERSION, GWY_SMS_CFG_LEN);

    /* Always emit proven GPT requirement. */
    fprintf(f,
            "    {\n      \"source\": \"sms_cfg\",\n      \"offset\": %u,\n      \"length\": 3,\n"
            "      \"bytes_hex\": \"475054\",\n      \"ascii\": \"GPT\",\n"
            "      \"evidence_pc\": \"0x%X\",\n      \"required\": true,\n"
            "      \"note\": \"first_gate_proven_e10a31k\"\n    }",
            GWY_SMS_CFG_GPT_OFF, E10A31L_PC_STRCMP_BLX);
    req_n = 1;

    /* Only add gwy if provenance proves SMSCFG with exact offset. */
    for (i = 0; i < g_l.cmp_n; i++) {
        CmpRow *c = &g_l.cmps[i];
        if (!c->is_gwy) continue;
        if ((c->lhs_source == E10A31L_LHS_SMSCFG_FIELD ||
             c->lhs_source == E10A31L_LHS_STACK_FROM_SMSCFG) &&
            c->cfg_offset >= 0) {
            fprintf(f,
                    ",\n    {\n      \"source\": \"sms_cfg\",\n      \"offset\": %d,\n"
                    "      \"length\": 3,\n      \"bytes_hex\": \"677779\",\n"
                    "      \"ascii\": \"gwy\",\n      \"evidence_pc\": \"0x%X\",\n"
                    "      \"required\": true,\n      \"note\": \"proven_sms_cfg_offset\"\n    }",
                    (int)c->cfg_offset, c->enter_pc);
            req_n++;
        } else {
            fprintf(f,
                    ",\n    {\n      \"source\": \"%s\",\n      \"offset\": null,\n"
                    "      \"length\": 3,\n      \"bytes_hex\": \"677779\",\n"
                    "      \"ascii\": \"gwy\",\n      \"evidence_pc\": \"0x%X\",\n"
                    "      \"required\": true,\n"
                    "      \"note\": \"NOT_sms_cfg_do_not_add_to_profile\"\n    }",
                    e10a31l_lhs_source_name(c->lhs_source), c->enter_pc);
        }
        break;
    }

    /* Emit other causal SMSCFG-backed compares (non-GPT/gwy) if any. */
    for (i = 0; i < g_l.cmp_n; i++) {
        CmpRow *c = &g_l.cmps[i];
        char hx[16];
        int j, dup = 0;
        if (c->is_gpt || c->is_gwy) continue;
        if (!c->leads_to_neg1) continue;
        if (c->lhs_source != E10A31L_LHS_SMSCFG_FIELD &&
            c->lhs_source != E10A31L_LHS_STACK_FROM_SMSCFG)
            continue;
        if (c->cfg_offset < 0) continue;
        /* Skip mid-string walks: offset must be a fresh field start, not inside GPT/gwy. */
        if (c->cfg_offset >= (int32_t)GWY_SMS_CFG_GPT_OFF &&
            c->cfg_offset < (int32_t)GWY_SMS_CFG_GPT_OFF + 6)
            continue;
        for (j = 0; j < g_l.cmp_n; j++) {
            if (j == i) continue;
            if ((g_l.cmps[j].is_gpt || g_l.cmps[j].is_gwy) &&
                g_l.cmps[j].cfg_offset >= 0 &&
                c->cfg_offset >= g_l.cmps[j].cfg_offset &&
                c->cfg_offset < g_l.cmps[j].cfg_offset + 3)
                dup = 1;
        }
        if (dup) continue;
        hex_n(c->rhs_bytes, 3, hx, sizeof(hx));
        fprintf(f,
                ",\n    {\n      \"source\": \"sms_cfg\",\n      \"offset\": %d,\n"
                "      \"length\": 3,\n      \"bytes_hex\": \"%s\",\n      \"ascii\": \"%s\",\n"
                "      \"evidence_pc\": \"0x%X\",\n      \"required\": true\n    }",
                (int)c->cfg_offset, hx, c->rhs_ascii, c->enter_pc);
        req_n++;
    }

    fprintf(f, "\n  ],\n  \"requirement_count\": %d,\n  \"gwy_lhs_source\": \"%s\",\n"
               "  \"gwy_verdict\": \"%s\"\n}\n",
            req_n, e10a31l_lhs_source_name(g_l.last_gwy_src), gwy_verdict_for(g_l.last_gwy_src));
    fclose(f);
    e10a31l_mark_milestone("REQUIRED_PLATFORM_TAGS_MANIFEST_WRITTEN", path);
}

void e10a31l_on_method0_enter(void *uc, uint32_t helper) {
    ensure();
    if (!g_l.enabled) return;
    g_l.active = 1;
    g_l.helper = helper;
    g_l.read_n = 0;
    g_l.cmp_n = 0;
    g_l.copy_n = 0;
    g_l.saw_gwy_cmp = 0;
    g_l.saw_gpt_pass = 0;
    g_l.true_fail = 0;
    resolve_cfg(uc);
    ensure_csvs();
    install_read_hooks(uc);
    e10a31l_mark_milestone("METHOD0_CONFIG_MAP_ARMED", "observe_only");
}

void e10a31l_on_method0_return(void *uc, uint32_t helper, int32_t ret) {
    int i;
    (void)helper;
    ensure();
    if (!g_l.enabled || !g_l.active) return;
    if (ret < 0) mark_recent_cmp_causal();
    /* Patch compare CSV causal flags for last failing cmp. */
    for (i = 0; i < g_l.cmp_n; i++) {
        if (!g_l.cmps[i].leads_to_neg1) continue;
        /* already logged; annotated file carries flag */
    }
    write_annotated_chain();
    write_manifest();
    disarm_read_hooks(uc);
    g_l.active = 0;
    e10a31l_mark_milestone("METHOD0_CONFIG_MAP_COMPLETE", ret < 0 ? "ret_neg1" : "ret_ok");
    printf("[JJFB_E10A31L] reads=%d compares=%d gwy_source=%s gpt_pass=%d ret=%d "
           "evidence=OBSERVED\n",
           g_l.read_n, g_l.cmp_n, e10a31l_lhs_source_name(g_l.last_gwy_src), g_l.saw_gpt_pass,
           (int)ret);
    fflush(stdout);
}

void e10a31l_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr,
                             const uint8_t *bytes, uint32_t size) {
    uint32_t sp = 0;
    (void)r3;
    (void)r4;
    (void)cpsr;
    (void)bytes;
    (void)size;
    ensure();
    if (!g_l.enabled || !g_l.active) return;
#ifdef GWY_HAVE_UNICORN
    if (uc) uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &sp);
#endif

    /* smsGetBytes-style: track copies from cfg into dst when at known memcpy site. */
    if (pc == E10A31L_PC_MEMCPY_BLX && g_l.cfg_base && r1 >= g_l.cfg_base &&
        r1 < g_l.cfg_base + g_l.cfg_len && r2 > 0 && r2 <= 0x100u) {
        note_copy(r0, r1 - g_l.cfg_base, r2, pc);
        e10a31l_mark_milestone("SMSCFG_COPY_TO_DST", "memcpy_from_cfg");
    }

    if (pc == E10A31L_PC_STRCMP_BLX) {
        emit_compare(uc, pc, E10A31L_PC_STRCMP_ENTER, lr, r0, r1, r9, sp);
    }
    if (pc == E10A31L_PC_STRCMP_ENTER) {
        /* Skip char-walk steps (pointers advance within the same strings). */
        if (g_l.cmp_n > 0) {
            CmpRow *prev = &g_l.cmps[g_l.cmp_n - 1];
            if (r0 >= prev->lhs && r0 < prev->lhs + 64u && r1 >= prev->rhs &&
                r1 < prev->rhs + 64u && (r0 - prev->lhs) == (r1 - prev->rhs) &&
                (r0 - prev->lhs) > 0u && (r0 - prev->lhs) < 64u)
                return;
            if (r0 == prev->lhs && r1 == prev->rhs) return;
        }
        emit_compare(uc, lr ? (lr > 4u ? lr - 4u : lr) : 0, pc, lr, r0, r1, r9, sp);
    }
    if (pc == E10A31L_PC_STRCMP_TRUE_FAIL) {
        g_l.true_fail = 1;
        g_l.true_fail_pc = pc;
        mark_recent_cmp_causal();
        e10a31l_mark_milestone("TRUE_FAIL_LINKED_TO_COMPARE", "0xAC2E8");
    }
}
