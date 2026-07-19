#include "gwy_launcher/ext_cfunction_publication_audit.h"
#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_er_rw_bind_restore.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define GWY_6M_TL_MAX 96
#define GWY_6M_CFN_VA_89 0x89CF4u
#define GWY_6M_CFN_VA_94 0x94F04u
#define GWY_6M_CFN_BASE 0x80000u
#define GWY_6M_CHECK_MAGIC 0x7FD854EBu

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t lr;
    uint32_t p;
    uint32_t off;
    uint32_t old_v;
    uint32_t new_v;
    char module[40];
    char klass[16];
} TlEv;

static struct {
    int mode_known;
    int enabled;
    int timeline;
    int chunk04;
    void *uc;
    int finalized;
    uint32_t tl_seq;
    TlEv tl[GWY_6M_TL_MAX];
    int tl_n;
    int saw_94f04_zero;
    int saw_pxc_zero_write;
    int saw_pxc_nz;
    int dumped_89;
    int dumped_94;
    uint32_t last_nested_p;
    uint32_t last_helper;
    int host_cfn_new_seen;
    int chunk04_set_nonzero;
    int chunk04_missing;
    char missing_reason[48];
    int cand_count;
} g_6m;

static int env_is_1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static void parse_mode(void) {
    if (g_6m.mode_known) return;
    g_6m.enabled = env_is_1("JJFB_CFUNCTION_PUBLICATION_AUDIT");
    g_6m.timeline = env_is_1("JJFB_P_TIMELINE_TRACE") || g_6m.enabled;
    g_6m.chunk04 = env_is_1("JJFB_CHUNK_FIELD04_AUDIT") || g_6m.enabled;
    g_6m.mode_known = 1;
}

void ext_cfunction_publication_audit_reset(void) {
    void *uc = g_6m.uc;
    memset(&g_6m, 0, sizeof(g_6m));
    g_6m.uc = uc;
}

void ext_cfunction_publication_audit_bind_uc(void *uc) { g_6m.uc = uc; }

int ext_cfunction_publication_audit_enabled(void) {
    parse_mode();
    return g_6m.enabled;
}

static const char *classify_write(uint32_t off, uint32_t old_v, uint32_t new_v, uint32_t pc) {
    if (new_v == 0 && (pc == GWY_6M_CFN_VA_94 || (pc & ~1u) == GWY_6M_CFN_VA_94))
        return "zero_init";
    if (new_v == 0 && old_v != 0) return "zero_init";
    if (off == 0x0Cu && new_v != 0) return "publication";
    if (off == 0x00u || off == 0x04u || off == 0x08u) return "metadata";
    return "other";
}

static void emit_bytes(void *uc, uint32_t base, uint32_t len) {
    uint8_t buf[256];
    uint32_t n = len;
    uint32_t i;
    if (n > sizeof(buf)) n = (uint32_t)sizeof(buf);
    if (!uc || !guest_memory_uc_peek((struct uc_struct *)uc, base, buf, n)) {
        printf("[JJFB_CFN_BYTES] base=0x%X len=0x%X ok=0 evidence=TARGET_OBSERVED\n", base, len);
        fflush(stdout);
        return;
    }
    printf("[JJFB_CFN_BYTES] base=0x%X len=0x%X ok=1 hex=", base, n);
    for (i = 0; i < n; i++) printf("%02X", buf[i]);
    printf(" evidence=TARGET_OBSERVED\n");
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static void read_regs(void *uc, uint32_t *r0, uint32_t *r1, uint32_t *r2, uint32_t *r3,
                      uint32_t *lr) {
    uc_engine *u = (uc_engine *)uc;
    if (!uc) return;
    if (r0) uc_reg_read(u, UC_ARM_REG_R0, r0);
    if (r1) uc_reg_read(u, UC_ARM_REG_R1, r1);
    if (r2) uc_reg_read(u, UC_ARM_REG_R2, r2);
    if (r3) uc_reg_read(u, UC_ARM_REG_R3, r3);
    if (lr) uc_reg_read(u, UC_ARM_REG_LR, lr);
}
#else
static void read_regs(void *uc, uint32_t *r0, uint32_t *r1, uint32_t *r2, uint32_t *r3,
                      uint32_t *lr) {
    (void)uc;
    if (r0) *r0 = 0;
    if (r1) *r1 = 0;
    if (r2) *r2 = 0;
    if (r3) *r3 = 0;
    if (lr) *lr = 0;
}
#endif

static void try_chunk_magic_scan(void *uc, uint32_t hint) {
    uint32_t word = 0;
    if (!uc || !hint) return;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, hint & ~3u, &word)) return;
    if (word == GWY_6M_CHECK_MAGIC) {
        printf("[JJFB_EXTCHUNK_CANDIDATE] addr=0x%X check=0x%X source=peek_hint "
               "evidence=DOCUMENTED\n",
               hint & ~3u, word);
        g_6m.cand_count++;
        fflush(stdout);
    }
}

void ext_cfunction_publication_audit_on_cfunction_new(uint32_t helper, uint32_t p_len,
                                                     uint32_t p_guest, const char *origin) {
    parse_mode();
    if (!g_6m.enabled) return;
    g_6m.host_cfn_new_seen = 1;
    g_6m.last_helper = helper;
    if (p_guest) g_6m.last_nested_p = p_guest;
    printf("[JJFB_CFN_NEW_CONTRACT] helper=0x%X p_len=%u p_guest=0x%X origin=%s "
           "host_memset=yes ret=MR_SUCCESS note=DOCUMENTED_alloc_zero_P "
           "extChunk_not_allocated_by_host evidence=DOCUMENTED\n",
           helper, p_len, p_guest, origin ? origin : "?");
    printf("[JJFB_P_TIMELINE] seq=%u pc=0x0 module=host:_mr_c_function_new op=alloc_memset "
           "P=0x%X off=0x0 old=0x0 new=0x0 lr=0x0 class=zero_init evidence=DOCUMENTED\n",
           ++g_6m.tl_seq, p_guest);
    fflush(stdout);
}

void ext_cfunction_publication_audit_on_ext_register(const char *module_name, uint64_t module_id,
                                                     uint32_t helper, uint32_t header_entry,
                                                     uint32_t chunk_field_04) {
    parse_mode();
    if (!g_6m.enabled && !g_6m.chunk04) return;
    printf("[JJFB_CHUNK_FIELD04] module=%s module_id=%llu helper=0x%X header=0x%X "
           "value=0x%X source=EXT_REGISTER note=tracks_extChunk_plus4_init_func_NOT_P_plus_C "
           "evidence=DOCUMENTED\n",
           module_name ? module_name : "?", (unsigned long long)module_id, helper, header_entry,
           chunk_field_04);
    if (chunk_field_04 == 0) {
        g_6m.chunk04_missing = 1;
        snprintf(g_6m.missing_reason, sizeof(g_6m.missing_reason), "EXT_REGISTER_ZERO");
        printf("[JJFB_CHUNK_FIELD04_MISSING] module=%s reason=EXT_REGISTER_ZERO "
               "evidence=TARGET_OBSERVED\n",
               module_name ? module_name : "?");
    } else {
        g_6m.chunk04_set_nonzero = 1;
    }
    fflush(stdout);
}

void ext_cfunction_publication_audit_on_chunk_field04(const char *module_name, uint32_t old_v,
                                                      uint32_t new_v, uint32_t pc,
                                                      const char *source) {
    parse_mode();
    if (!g_6m.chunk04 && !g_6m.enabled) return;
    printf("[JJFB_CHUNK_FIELD04_WRITE] module=%s old=0x%X new=0x%X pc=0x%X source=%s "
           "evidence=TARGET_OBSERVED\n",
           module_name ? module_name : "?", old_v, new_v, pc, source ? source : "?");
    if (new_v) g_6m.chunk04_set_nonzero = 1;
    fflush(stdout);
}

void ext_cfunction_publication_audit_on_entry_reconcile(const char *module_name,
                                                        uint32_t chunk_field_04,
                                                        const char *writer_tag) {
    parse_mode();
    if (!g_6m.enabled && !g_6m.chunk04) return;
    printf("[JJFB_CHUNK_FIELD04] module=%s value=0x%X source=ENTRY_RECONCILE writer=%s "
           "evidence=TARGET_OBSERVED\n",
           module_name ? module_name : "?", chunk_field_04, writer_tag ? writer_tag : "?");
    if (chunk_field_04 == 0 && writer_tag && strstr(writer_tag, "NONE")) {
        g_6m.chunk04_missing = 1;
        snprintf(g_6m.missing_reason, sizeof(g_6m.missing_reason), "%s", writer_tag);
        printf("[JJFB_CHUNK_FIELD04_MISSING] module=%s reason=%s evidence=TARGET_OBSERVED\n",
               module_name ? module_name : "?", writer_tag);
    }
    fflush(stdout);
}

void ext_cfunction_publication_audit_on_p_write(uint32_t p_guest, uint32_t off, uint32_t old_v,
                                                uint32_t new_v, uint32_t pc, uint32_t lr,
                                                const char *module) {
    const char *klass;
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, rlr = lr;
    parse_mode();
    if (!g_6m.enabled) return;
    if (off != 0x00u && off != 0x04u && off != 0x08u && off != 0x0Cu && off != 0x10u) return;

    klass = classify_write(off, old_v, new_v, pc);
    if (p_guest) g_6m.last_nested_p = p_guest;

    if (g_6m.timeline) {
        printf("[JJFB_P_TIMELINE] seq=%u pc=0x%X module=%s op=P_WRITE P=0x%X off=0x%02X "
               "old=0x%X new=0x%X lr=0x%X class=%s evidence=TARGET_OBSERVED\n",
               ++g_6m.tl_seq, pc, module && module[0] ? module : "?", p_guest, off, old_v, new_v,
               lr, klass);
        if (g_6m.tl_n < GWY_6M_TL_MAX) {
            TlEv *e = &g_6m.tl[g_6m.tl_n++];
            e->seq = g_6m.tl_seq;
            e->pc = pc;
            e->lr = lr;
            e->p = p_guest;
            e->off = off;
            e->old_v = old_v;
            e->new_v = new_v;
            snprintf(e->module, sizeof(e->module), "%s", module && module[0] ? module : "?");
            snprintf(e->klass, sizeof(e->klass), "%s", klass);
        }
    }

    if (off == 0x0Cu) {
        if (new_v) g_6m.saw_pxc_nz = 1;
        if (new_v == 0) g_6m.saw_pxc_zero_write = 1;
        if ((pc & ~1u) == GWY_6M_CFN_VA_94 || pc == GWY_6M_CFN_VA_94) {
            g_6m.saw_94f04_zero = 1;
            read_regs(g_6m.uc, &r0, &r1, &r2, &r3, &rlr);
            printf("[JJFB_CFN_PXC_SOURCE] pc=0x%X src_reg=imm_or_rN src_value=0x%X "
                   "r0=0x%X r1=0x%X r2=0x%X r3=0x%X lr=0x%X reason=store_zero_to_P_plus_C "
                   "class=zero_init evidence=TARGET_OBSERVED\n",
                   pc, new_v, r0, r1, r2, r3, rlr ? rlr : lr);
            printf("[JJFB_CFN_DISASM] func=0x%X kind=zero_init evidence=TARGET_OBSERVED\n",
                   GWY_6M_CFN_VA_94);
        }
        /* Phase 6N: platform restore after guest/host zero of P+0xC (not a game force). */
        if (new_v == 0 && old_v != 0)
            ext_chunk_provider_on_pxc_cleared(p_guest, pc);
    }
    /* Phase 6O: also bind when CFN audit sees P+0/+4 (idempotent if shell already bound). */
    if (off == 0x00u || off == 0x04u)
        ext_er_rw_bind_restore_on_p_write(p_guest, off, new_v, module);
    fflush(stdout);
}

void ext_cfunction_publication_audit_on_code(void *uc, const char *module_name, uint32_t pc,
                                             const uint32_t regs[16], uint32_t cpsr) {
    uint32_t pn = pc & ~1u;
    (void)cpsr;
    parse_mode();
    if (!g_6m.enabled) return;
    if (!uc) uc = g_6m.uc;

    if (pn == GWY_6M_CFN_VA_89 && !g_6m.dumped_89) {
        g_6m.dumped_89 = 1;
        printf("[JJFB_CFN_ENTER] func=0x%X module=%s r0=0x%X r1=0x%X r2=0x%X r3=0x%X lr=0x%X "
               "evidence=TARGET_OBSERVED\n",
               GWY_6M_CFN_VA_89, module_name ? module_name : "?", regs ? regs[0] : 0,
               regs ? regs[1] : 0, regs ? regs[2] : 0, regs ? regs[3] : 0, regs ? regs[14] : 0);
        emit_bytes(uc, 0x89CF0u, 0xA0u);
        printf("[JJFB_CFN_DISASM] func=0x%X kind=cfunction_new_path evidence=TARGET_OBSERVED\n",
               GWY_6M_CFN_VA_89);
        fflush(stdout);
    }
    if (pn == GWY_6M_CFN_VA_94 && !g_6m.dumped_94) {
        g_6m.dumped_94 = 1;
        printf("[JJFB_CFN_ENTER] func=0x%X module=%s r0=0x%X r1=0x%X r2=0x%X r3=0x%X lr=0x%X "
               "evidence=TARGET_OBSERVED\n",
               GWY_6M_CFN_VA_94, module_name ? module_name : "?", regs ? regs[0] : 0,
               regs ? regs[1] : 0, regs ? regs[2] : 0, regs ? regs[3] : 0, regs ? regs[14] : 0);
        emit_bytes(uc, 0x94E94u, 0xD0u);
        /* Scan r0 as potential dest (P) and nearby for magic chunk header. */
        if (regs) {
            try_chunk_magic_scan(uc, regs[0]);
            try_chunk_magic_scan(uc, g_6m.last_nested_p);
            if (g_6m.last_nested_p)
                try_chunk_magic_scan(uc, g_6m.last_nested_p + 0x0Cu); /* if ptr already set */
        }
        fflush(stdout);
    }
}

void ext_cfunction_publication_audit_on_cross_module(uint32_t from_pc, uint32_t target,
                                                     uint32_t r0, uint32_t r1, uint32_t lr,
                                                     const char *from_mod, const char *to_mod) {
    uint32_t tn = target & ~1u;
    parse_mode();
    if (!g_6m.enabled) return;
    if (tn != GWY_6M_CFN_VA_89 && tn != GWY_6M_CFN_VA_94) return;
    printf("[JJFB_CFN_XFER] from_pc=0x%X target=0x%X r0=0x%X r1=0x%X lr=0x%X from=%s to=%s "
           "evidence=TARGET_OBSERVED\n",
           from_pc, target, r0, r1, lr, from_mod ? from_mod : "?", to_mod ? to_mod : "?");
    fflush(stdout);
}

void ext_cfunction_publication_audit_finalize(const char *stop_reason) {
    parse_mode();
    if (!g_6m.enabled || g_6m.finalized) return;
    g_6m.finalized = 1;
    printf("[JJFB_6M_SUMMARY] host_cfn_new=%s saw_94f04_zero=%s pxc_zero_write=%s pxc_nz=%s "
           "chunk04_nz=%s chunk04_missing=%s missing_reason=%s cand=%d stop=%s "
           "class=CFUNCTION_PUBLICATION_SOURCE_ZERO evidence=TARGET_OBSERVED\n",
           g_6m.host_cfn_new_seen ? "yes" : "no", g_6m.saw_94f04_zero ? "yes" : "no",
           g_6m.saw_pxc_zero_write ? "yes" : "no", g_6m.saw_pxc_nz ? "yes" : "no",
           g_6m.chunk04_set_nonzero ? "yes" : "no", g_6m.chunk04_missing ? "yes" : "no",
           g_6m.missing_reason[0] ? g_6m.missing_reason : "n/a", g_6m.cand_count,
           stop_reason ? stop_reason : "?");
    fflush(stdout);
}
