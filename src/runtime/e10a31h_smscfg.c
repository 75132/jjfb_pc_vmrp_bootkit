#include "gwy_launcher/e10a31h_smscfg.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define E10A31H_PC_MEMCPY_BLX 0x2E31AEu
#define E10A31H_PC_READ_BL 0x2E5396u
#define E10A31H_SMS_CFG_LEN 0x10E0u
#define E10A31H_GPT_OFF 0x349u
#define E10A31H_MR_TABLE_MEMCPY_OFF 0xCu
#define E10A31H_MR_TABLE_SMSCFG_OFF 0x1C0u

static struct {
    int known;
    int enabled;
    unsigned long long run_id;
    FILE *csv;
    int saw_read;
    int saw_table;
    uint32_t mr_table;
    uint32_t slot_memcpy;
    uint32_t slot_smscfg;
    uint32_t cfg_base;
    uint8_t cfg_at_349[8];
    int cfg_ok;
} g_h;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31H_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A31G_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
}

static void ensure(void) {
    if (g_h.known) return;
    g_h.known = 1;
    g_h.enabled = env1("JJFB_E10A31H_MODE");
    g_h.run_id = run_id_now();
}

static FILE *csv(void) {
    const char *p;
    if (g_h.csv) return g_h.csv;
    p = getenv("JJFB_E10A31H_CSV");
    if (!p || !p[0]) p = "reports/e10a31h_smscfg_trace.csv";
    g_h.csv = fopen(p, "w");
    if (!g_h.csv) return NULL;
    fputs("run_id,event,pc,r0,r1,r2,r3,r4,r9,ptr,hex,note\n", g_h.csv);
    fflush(g_h.csv);
    return g_h.csv;
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

static void emit(const char *event, uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2,
                 uint32_t r3, uint32_t r4, uint32_t r9, uint32_t ptr, const uint8_t *bytes,
                 int n, const char *note) {
    FILE *f = csv();
    char hx[40];
    hx[0] = 0;
    if (bytes && n > 0) hex_n(bytes, n, hx, sizeof(hx));
    if (f) {
        fprintf(f, "%llu,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s,\"%s\"\n", g_h.run_id,
                event ? event : "", pc, r0, r1, r2, r3, r4, r9, ptr, hx, note ? note : "");
        fflush(f);
    }
    printf("[JJFB_E10A31H] event=%s pc=0x%X r0=0x%X r1=0x%X r4=0x%X r9=0x%X ptr=0x%X hex=%s "
           "note=%s evidence=OBSERVED\n",
           event ? event : "", pc, r0, r1, r4, r9, ptr, hx, note ? note : "");
    fflush(stdout);
}

int e10a31h_enabled(void) {
    ensure();
    return g_h.enabled;
}

void e10a31h_reset(void) {
    if (g_h.csv) fclose(g_h.csv);
    memset(&g_h, 0, sizeof(g_h));
}

void e10a31h_mark_milestone(const char *name, const char *note) {
    ensure();
    if (!g_h.enabled || !name) return;
    printf("[JJFB_E10A31H_MILESTONE] name=%s note=%s evidence=OBSERVED\n", name,
           note ? note : "");
    fflush(stdout);
}

void e10a31h_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr,
                             const uint8_t *bytes, uint32_t size) {
    (void)lr;
    (void)cpsr;
    (void)bytes;
    (void)size;
    ensure();
    if (!g_h.enabled) return;

    if (pc == E10A31H_PC_READ_BL && r0 == E10A31H_GPT_OFF && r2 == 3) {
        g_h.saw_read = 1;
        e10a31h_mark_milestone("SMSGETBYTES_CALL_OFF_349_LEN_3", "gamelist_0x2E3180");
        emit("SMSGETBYTES_CALL", pc, r0, r1, r2, r3, r4, r9, r1, NULL, 0, "pos_dst_len");
    }

    if (pc == E10A31H_PC_MEMCPY_BLX && r4) {
        uint32_t slot0 = 0, slot_c = 0, slot_1c0 = 0;
        uint8_t at349[8];
        memset(at349, 0, sizeof(at349));
        g_h.mr_table = r4;
        g_h.saw_table = 1;
        read_guest(uc, r4 + 0x0u, &slot0, 4);
        read_guest(uc, r4 + E10A31H_MR_TABLE_MEMCPY_OFF, &slot_c, 4);
        read_guest(uc, r4 + E10A31H_MR_TABLE_SMSCFG_OFF, &slot_1c0, 4);
        g_h.slot_memcpy = slot_c;
        g_h.slot_smscfg = slot_1c0;
        g_h.cfg_base = (r1 >= E10A31H_GPT_OFF) ? (r1 - E10A31H_GPT_OFF) : slot_1c0;
        emit("MR_TABLE", pc, slot0, slot_c, slot_1c0, r3, r4, r9, r4, NULL, 0,
             "slot0_slotC_slot1C0");
        if (slot_c == r3)
            e10a31h_mark_milestone("MR_TABLE_SLOT_C_IS_MEMCPY", "matches_r3");
        if (slot_1c0 == g_h.cfg_base)
            e10a31h_mark_milestone("MR_TABLE_SLOT_1C0_IS_SMS_CFG_BUF", "mr_sms_cfg_buf");
        if (r2 == 3 && r0 && (r1 - g_h.cfg_base) == E10A31H_GPT_OFF)
            e10a31h_mark_milestone("FIELD_READ_IS_SMSGETBYTES", "limit_0x10E0_doc");

        g_h.cfg_ok = read_guest(uc, g_h.cfg_base + E10A31H_GPT_OFF, at349, 8);
        if (g_h.cfg_ok) {
            memcpy(g_h.cfg_at_349, at349, 8);
            emit("SMS_CFG_AT_349", pc, g_h.cfg_base, E10A31H_GPT_OFF, 8, 0, r4, r9,
                 g_h.cfg_base + E10A31H_GPT_OFF, at349, 8, "expect_GPT");
            if (at349[0] == 0 && at349[1] == 0 && at349[2] == 0)
                e10a31h_mark_milestone("SMS_CFG_349_IS_NUL", "no_GPT_tag");
            else if (at349[0] == 'G' && at349[1] == 'P' && at349[2] == 'T')
                e10a31h_mark_milestone("SMS_CFG_349_IS_GPT", "tag_present");
            else
                e10a31h_mark_milestone("SMS_CFG_349_UNEXPECTED", "not_GPT");
        }
        if (r9 && g_h.cfg_base == r9 + 0x8D4u)
            e10a31h_mark_milestone("SMS_CFG_BUF_IN_CFUNCTION_ERW_8D4", "guest_local_buf");
        e10a31h_mark_milestone("BRIDGE_MAP_DATA_SMS_CFG_NO_INITFN", "hooks_init_skips_MAP_DATA");
        e10a31h_mark_milestone("DSM_CFG_FILE_ABSENT_IN_TREE", "dsm.cfg_not_shipped");
        printf("[JJFB_E10A31H] mr_table=0x%X memcpy_slot=0x%X smscfg_slot=0x%X cfg_base=0x%X "
               "SMS_CFG_LEN=0x%X evidence=DOCUMENTED\n",
               g_h.mr_table, g_h.slot_memcpy, g_h.slot_smscfg, g_h.cfg_base, E10A31H_SMS_CFG_LEN);
        fflush(stdout);
    }
}

void e10a31h_on_method0_return(void *uc, uint32_t helper, int32_t ret) {
    (void)uc;
    (void)helper;
    ensure();
    if (!g_h.enabled) return;
    printf("[JJFB_E10A31H] method0_return ret=%d saw_read=%d saw_table=%d cfg_base=0x%X "
           "cfg349=%02X%02X%02X evidence=OBSERVED\n",
           (int)ret, g_h.saw_read, g_h.saw_table, g_h.cfg_base, g_h.cfg_at_349[0],
           g_h.cfg_at_349[1], g_h.cfg_at_349[2]);
    fflush(stdout);
    if (g_h.saw_table && g_h.cfg_ok && g_h.cfg_at_349[0] == 0)
        e10a31h_mark_milestone("METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG", "not_filebuf_primary");
}
