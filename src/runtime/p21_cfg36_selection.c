#include "gwy_launcher/p21_cfg36_selection.h"

#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define CFG_RECORD_BASE 1024u
#define CFG_RECORD_SIZE 272u
#define CFG36_INDEX 36u
#define CFG36_FILE_OFF (CFG_RECORD_BASE + CFG36_INDEX * CFG_RECORD_SIZE)

#define OFF_SELECT_1 0x089CCu
#define OFF_SELECT_2 0x10136u
#define OFF_SELECT_3 0x10186u
#define OFF_SELECT_4 0x1024Eu
#define OFF_CFG_LOADER 0x07B6Cu
#define OFF_CFG_DISPATCH 0x07B9Cu
#define OFF_DESC_BUILDER 0x13A34u
#define STATE_OFF 0x6EEu

#define IO_CAP 2048
#define REC_CAP 64
#define PARAM_CAP 256
#define BR_CAP 256
#define TIMER_CAP 64
#define RING_BEFORE 64
#define RING_AFTER 64

static const char *EXPECTED_MRP = "gwy/jjfb.mrp";

typedef struct P21Field {
    int seen_raw;
    int parsed;
    int written;
    int gamelist_read;
    uint32_t raw_off;
    uint32_t parse_pc;
    char parse_mod[48];
    int32_t value_i;
    char value_s[96];
    uint32_t write_va;
    char write_owner[48];
    uint32_t write_pc;
} P21Field;

typedef struct {
    int known;
    int enabled;
    void *uc;
    unsigned long long run_id;
    clock_t t0;
    int gamelist_started;
    uint32_t gl_base;
    uint32_t gl_size;

    int gate_fmt;
    int gate_open;
    int gate_record_read;
    int gate_cfg36_present;
    int gate_cfg36_selected;

    char cfg_source[320];
    char cfg_source_ns[32];
    uint32_t cfg_buf_guest;
    uint32_t cfg_buf_len;
    int cfg_record_count;

    uint32_t cfg36_guest;
    uint32_t cfg36_source_off;
    uint32_t cfg36_index;
    char cfg36_sha[72];

    FILE *io_csv;
    FILE *rec_csv;
    FILE *param_csv;
    FILE *sel_csv;
    FILE *timer_csv;

    uint32_t io_seq;
    uint32_t param_seq;
    uint32_t br_seq;
    uint32_t timer_seq;
    uint32_t timer_gen;

    P21Field f_napptype, f_nextid, f_ncode, f_narg, f_narg1, f_nmrpname, f_gwyblink;

    uint32_t param_va;
    uint32_t param_len;
    char param_raw[256];

    /* Selection watch: after CFG36_RECORD_PRESENT, arm mem-read on record. */
    int cfg36_read_hook_armed;
#ifdef GWY_HAVE_UNICORN
    uc_hook cfg36_read_hook;
    uc_hook mem_write_hook;
    int mem_write_hook_armed;
#endif
    int cfg36_first_read_logged;
    int capture_insn;
    int capture_left;
    uint32_t capture_focus;

    /* Selected-state candidates (R9+0x6EE and nearby pointers). */
    uint32_t last_state_base;
    uint8_t last_state[64];
    int last_state_valid;

    /* Timer pre/post snapshots. */
    uint8_t timer_pre_state[64];
    int timer_pre_valid;
    uint32_t timer_pre_file_seq;
    uint32_t timer_pre_cfg_writes;
    int timer_idle_streak;
    uint32_t cfg_buffer_writes;

    uint32_t pc_ring[RING_BEFORE];
    size_t pc_ring_n;
    size_t pc_ring_next;
} P21State;

static P21State g_p21;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

static int path_has(const char *s, const char *n) {
    return s && n && strstr(s, n) != NULL;
}

static int is_cfg_path(const char *p) {
    if (!p || !p[0]) return 0;
    if (path_has(p, "cfg.bin") || path_has(p, "dsm.cfg") || path_has(p, "gamelist.cfg"))
        return 1;
    if (path_has(p, "_cfg") || path_has(p, "cfg.td")) return 1;
    return 0;
}

static int is_gamelist_mod(const char *m) {
    return m && path_has(m, "gamelist");
}

static uint32_t be24(const uint8_t *p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

static const char *mod_at_pc(uint32_t pc) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *gm =
        reg ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
    if (!gm) return "?";
    return gm->resolved_name[0] ? gm->resolved_name : gm->requested_name;
}

static void hex64(const uint8_t *b, size_t n, char *out, size_t out_sz) {
    size_t i, lim = n < 64 ? n : 64;
    size_t o = 0;
    out[0] = 0;
    for (i = 0; i < lim && o + 3 < out_sz; i++) {
        int w = snprintf(out + o, out_sz - o, "%02X", b[i]);
        if (w < 0) break;
        o += (size_t)w;
    }
}

static FILE *open_csv(const char *env_key, const char *fallback, const char *header) {
    const char *p = getenv(env_key);
    FILE *f;
    if (!p || !p[0]) p = fallback;
    f = fopen(p, "wb");
    if (f) {
        fputs(header, f);
        fflush(f);
    }
    return f;
}

static void ensure_csvs(void) {
    if (!g_p21.io_csv)
        g_p21.io_csv = open_csv(
            "JJFB_P21_CFG_IO_CSV", "reports/p21_cfg_file_io.csv",
            "sequence,caller_pc,caller_module,branch_instruction,api,path,mode,offset,"
            "requested_size,returned_size,return_value,buffer_guest,buffer_sha256,"
            "buffer_first_64_hex,owner_package,timer_generation\n");
    if (!g_p21.rec_csv)
        g_p21.rec_csv = open_csv(
            "JJFB_P21_CFG_REC_CSV", "reports/p21_cfg_record_inventory.csv",
            "record_guest,record_size,record_index,source_file,source_offset,field_offsets,"
            "raw_hex,decoded_fields,record_sha256,producer_pc,producer_module,full_cfg36\n");
    if (!g_p21.param_csv)
        g_p21.param_csv = open_csv(
            "JJFB_P21_PARAM_CSV", "reports/p21_launch_param_provenance.csv",
            "field,raw_string_offset,parse_pc,parse_module,parsed_value,write_va,write_owner,"
            "write_pc,gamelist_read,note\n");
    if (!g_p21.sel_csv)
        g_p21.sel_csv = open_csv(
            "JJFB_P21_SEL_CSV", "reports/p21_cfg_selection_branches.csv",
            "seq,event,pc,off,module,r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,sp,lr,"
            "cpsr,insn_hex,disasm_hint,mem_rw,cmp_tst,branch_cond,branch_taken,note\n");
    if (!g_p21.timer_csv)
        g_p21.timer_csv = open_csv(
            "JJFB_P21_TIMER_CSV", "reports/p21_timer_state_diff.csv",
            "timer_generation,period_ms,helper,method,return,erw_changed,file_calls,"
            "network_calls,cfg_buffer_writes,list_count_delta,selected_state_changed,"
            "classification,note\n");
}

int p21_enabled(void) {
    if (g_p21.known) return g_p21.enabled;
    g_p21.enabled = env1("JJFB_P21_MODE");
    {
        const char *rid = getenv("JJFB_P21_RUN_ID");
        if (!rid || !rid[0]) rid = getenv("JJFB_E10A_RUN_ID");
        if (rid && rid[0]) g_p21.run_id = strtoull(rid, NULL, 10);
    }
    g_p21.known = 1;
    if (g_p21.enabled) {
        g_p21.t0 = clock();
        ensure_csvs();
        printf("[JJFB_P21] armed=1 NATURAL_ONLY=1 no_headless=1 run_id=%llu evidence=OBSERVED\n",
               g_p21.run_id);
        fflush(stdout);
    }
    return g_p21.enabled;
}

void p21_reset(void) {
    if (g_p21.io_csv) fclose(g_p21.io_csv);
    if (g_p21.rec_csv) fclose(g_p21.rec_csv);
    if (g_p21.param_csv) fclose(g_p21.param_csv);
    if (g_p21.sel_csv) fclose(g_p21.sel_csv);
    if (g_p21.timer_csv) fclose(g_p21.timer_csv);
#ifdef GWY_HAVE_UNICORN
    if (g_p21.cfg36_read_hook_armed && g_p21.uc) {
        uc_hook_del((uc_engine *)g_p21.uc, g_p21.cfg36_read_hook);
    }
    if (g_p21.mem_write_hook_armed && g_p21.uc) {
        uc_hook_del((uc_engine *)g_p21.uc, g_p21.mem_write_hook);
    }
#endif
    memset(&g_p21, 0, sizeof(g_p21));
}

void p21_bind_uc(void *uc) {
    if (!p21_enabled()) {
        g_p21.uc = uc;
        return;
    }
    g_p21.uc = uc;
}

void p21_note_gamelist_started(void) {
    if (!p21_enabled()) return;
    g_p21.gamelist_started = 1;
    printf("[JJFB_P21] gate=GAMELIST_STARTED evidence=OBSERVED\n");
    fflush(stdout);
}

void p21_note_cfg_fmt_mapped(uint32_t fmt_va, const char *note) {
    if (!p21_enabled()) return;
    if (g_p21.gate_fmt) return;
    g_p21.gate_fmt = 1;
    printf("[JJFB_P21] gate=CFG_FMT_MAPPED va=0x%X note=%s "
           "NOT_cfg36_selected evidence=OBSERVED\n",
           fmt_va, note ? note : "");
    fflush(stdout);
}

static int caller_interesting(uint32_t pc, const char *path) {
    const char *m = mod_at_pc(pc);
    if (is_gamelist_mod(m)) return 1;
    if (is_cfg_path(path)) return 1;
    if (g_p21.gamelist_started && path && path[0]) return 1;
    return 0;
}

static int match_cfg36_fields(const uint8_t *rec, char *decoded, size_t decoded_sz) {
    uint32_t napptype = rec[0x57];
    uint32_t nextid = be24(rec + 0x72);
    uint32_t ncode = be24(rec + 0x78);
    uint32_t narg = be24(rec + 0x7B);
    uint32_t narg1 = rec[0x7E];
    char mrp[96];
    int has_mrp = 0;
    int has_blink = 0;
    size_t i;
    mrp[0] = 0;
    for (i = 0; i + 12 < CFG_RECORD_SIZE; i++) {
        if (memcmp(rec + i, "gwy/", 4) == 0) {
            size_t j = 0;
            while (i + j < CFG_RECORD_SIZE && j + 1 < sizeof(mrp) && rec[i + j] &&
                   rec[i + j] >= 32 && rec[i + j] < 127) {
                mrp[j] = (char)rec[i + j];
                j++;
            }
            mrp[j] = 0;
            if (strstr(mrp, ".mrp")) {
                has_mrp = 1;
                break;
            }
        }
    }
    for (i = 0; i + 8 < CFG_RECORD_SIZE; i++) {
        if (memcmp(rec + i, "gwyblink", 8) == 0 ||
            (rec[i] == '_' && i + 9 < CFG_RECORD_SIZE &&
             memcmp(rec + i + 1, "gwyblink", 8) == 0)) {
            has_blink = 1;
            break;
        }
    }
    /* Direct-launch flag may be a bit rather than the literal string. Accept
     * either literal or narg1==1 with jjfb target (historical cfg36 shape). */
    if (!has_blink && narg1 == 1 && has_mrp && strstr(mrp, "jjfb")) has_blink = 1;

    snprintf(decoded, decoded_sz,
             "napptype=%u;nextid=%u;ncode=%u;narg=%u;narg1=%u;nmrpname=%s;gwyblink=%d",
             napptype, nextid, ncode, narg, narg1, has_mrp ? mrp : "?", has_blink);

    if (napptype != 12) return 0;
    if (nextid != 482) return 0;
    if (ncode != 512) return 0;
    if (narg != 0) return 0;
    if (narg1 != 1) return 0;
    if (!has_mrp || strcmp(mrp, EXPECTED_MRP) != 0) return 0;
    if (!has_blink) return 0;
    return 1;
}

static void inventory_record(uint32_t guest, uint32_t index, uint32_t source_off,
                             const uint8_t *rec, uint32_t producer_pc, const char *src_file,
                             int full) {
    uint8_t dig[32];
    char sha[72];
    char raw_hex[560];
    char decoded[192];
    char fields[96];
    ensure_csvs();
    gwy_sha256(rec, CFG_RECORD_SIZE, dig);
    gwy_sha256_hex(dig, sha);
    hex64(rec, CFG_RECORD_SIZE, raw_hex, sizeof(raw_hex));
    (void)match_cfg36_fields(rec, decoded, sizeof(decoded));
    snprintf(fields, sizeof(fields), "0x57,0x72,0x78,0x7B,0x7E,mrp_scan");
    if (g_p21.rec_csv) {
        fprintf(g_p21.rec_csv,
                "0x%X,%u,%u,\"%s\",%u,\"%s\",\"%s\",\"%s\",%s,0x%X,\"%s\",%d\n", guest,
                CFG_RECORD_SIZE, index, src_file ? src_file : "", source_off, fields, raw_hex,
                decoded, sha, producer_pc, mod_at_pc(producer_pc), full);
        fflush(g_p21.rec_csv);
    }
    if (full && !g_p21.gate_cfg36_present) {
        g_p21.gate_cfg36_present = 1;
        g_p21.cfg36_guest = guest;
        g_p21.cfg36_source_off = source_off;
        g_p21.cfg36_index = index;
        snprintf(g_p21.cfg36_sha, sizeof(g_p21.cfg36_sha), "%s", sha);
        printf("[JJFB_P21] gate=CFG36_RECORD_PRESENT guest=0x%X index=%u src_off=%u "
               "sha=%s evidence=OBSERVED\n",
               guest, index, source_off, sha);
        fflush(stdout);
    }
}

static void scan_cfg_buffer(uint32_t buf, uint32_t len, uint32_t producer_pc, const char *src,
                            const uint8_t *host, uint32_t host_len) {
    uint32_t max_idx;
    uint32_t i;
    const uint8_t *data = host;
    uint8_t *tmp = NULL;
    if (!buf || len < CFG_RECORD_BASE + CFG_RECORD_SIZE) return;
    if (!data || host_len < len) {
#ifdef GWY_HAVE_UNICORN
        if (g_p21.uc && len <= 256u * 1024u) {
            tmp = (uint8_t *)malloc(len);
            if (tmp &&
                guest_memory_uc_peek((struct uc_struct *)g_p21.uc, buf, tmp, len)) {
                data = tmp;
                host_len = len;
            } else {
                free(tmp);
                tmp = NULL;
                data = NULL;
            }
        }
#endif
    }
    if (!data) return;
    max_idx = (len - CFG_RECORD_BASE) / CFG_RECORD_SIZE;
    g_p21.cfg_record_count = (int)max_idx;
    g_p21.cfg_buf_guest = buf;
    g_p21.cfg_buf_len = len;
    snprintf(g_p21.cfg_source, sizeof(g_p21.cfg_source), "%s", src ? src : "");
    g_p21.gate_record_read = 1;
    printf("[JJFB_P21] gate=CFG_RECORD_READ buf=0x%X len=%u records=%u evidence=OBSERVED\n",
           buf, len, max_idx);
    fflush(stdout);

    /* Always inventory index 36 if in range; also scan all for full signature. */
    for (i = 0; i < max_idx && i < 512u; i++) {
        uint32_t off = CFG_RECORD_BASE + i * CFG_RECORD_SIZE;
        const uint8_t *rec = data + off;
        char decoded[192];
        int full = match_cfg36_fields(rec, decoded, sizeof(decoded));
        if (i == CFG36_INDEX || full) {
            inventory_record(buf + off, i, off, rec, producer_pc, src, full);
        }
        (void)decoded;
    }
    free(tmp);
}

#ifdef GWY_HAVE_UNICORN
static void cfg36_mem_read_cb(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                              int64_t value, void *user_data) {
    uint32_t pc = 0, lr = 0, sp = 0, cpsr = 0;
    uint32_t regs[16];
    int i;
    (void)type;
    (void)value;
    (void)user_data;
    if (!g_p21.gate_cfg36_present || !g_p21.cfg36_guest) return;
    if (address < g_p21.cfg36_guest ||
        address >= (uint64_t)g_p21.cfg36_guest + CFG_RECORD_SIZE)
        return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    for (i = 0; i < 13; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    regs[13] = sp;
    regs[14] = lr;
    regs[15] = pc;
    ensure_csvs();
    if (g_p21.sel_csv) {
        fprintf(g_p21.sel_csv,
                "%u,CFG36_RECORD_READ,0x%X,0x%X,\"%s\",0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,,MEM_READ_addr=0x%llX_sz=%d,"
                ",,,,\"first_or_related\"\n",
                ++g_p21.br_seq, pc,
                g_p21.gl_base ? ((pc & ~1u) - g_p21.gl_base) : 0, mod_at_pc(pc), regs[0],
                regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7], regs[8],
                regs[9], regs[10], regs[11], regs[12], sp, lr, cpsr,
                (unsigned long long)address, size);
        fflush(g_p21.sel_csv);
    }
    if (!g_p21.cfg36_first_read_logged) {
        g_p21.cfg36_first_read_logged = 1;
        g_p21.capture_insn = 1;
        g_p21.capture_left = RING_AFTER;
        g_p21.capture_focus = pc;
        printf("[JJFB_P21] CFG36_RECORD_FIRST_READ pc=0x%X addr=0x%llX size=%d "
               "evidence=OBSERVED\n",
               pc, (unsigned long long)address, size);
        fflush(stdout);
    }
}

static void snapshot_selected_state(void *uc, uint32_t pc, uint32_t r9, const char *why) {
    uint8_t cur[64];
    uint32_t base;
    int nonzero = 0;
    int changed = 0;
    size_t i;
    if (!uc || !r9) return;
    base = r9 + STATE_OFF;
    if (!guest_memory_uc_peek((struct uc_struct *)uc, base, cur, sizeof(cur))) return;
    for (i = 0; i < sizeof(cur); i++) {
        if (cur[i]) nonzero = 1;
        if (g_p21.last_state_valid && cur[i] != g_p21.last_state[i]) changed = 1;
    }
    if (nonzero && (!g_p21.last_state_valid || changed)) {
        const char *own = mod_at_pc(pc);
        ensure_csvs();
        if (g_p21.sel_csv) {
            fprintf(g_p21.sel_csv,
                    "%u,SELECTED_STATE_SNAP,0x%X,0x%X,\"%s\",,,,,,,,,,,,,0x%X,,,,,,,,"
                    "\"%s\"\n",
                    ++g_p21.br_seq, pc, g_p21.gl_base ? ((pc & ~1u) - g_p21.gl_base) : 0, own,
                    r9, why ? why : "snap");
            fflush(g_p21.sel_csv);
        }
        if (g_p21.gate_cfg36_present && is_gamelist_mod(own) && !g_p21.gate_cfg36_selected) {
            g_p21.gate_cfg36_selected = 1;
            printf("[JJFB_P21] gate=CFG36_SELECTED write_pc=0x%X state=0x%X why=%s "
                   "evidence=OBSERVED\n",
                   pc, base, why ? why : "?");
            fflush(stdout);
        }
    }
    memcpy(g_p21.last_state, cur, sizeof(cur));
    g_p21.last_state_valid = 1;
    g_p21.last_state_base = base;
}

static void arm_hooks(void *uc) {
    uc_err ue = UC_ERR_OK;
    if (!uc || !p21_enabled()) return;
    if (g_p21.gate_cfg36_present && g_p21.cfg36_guest && !g_p21.cfg36_read_hook_armed) {
        ue = uc_hook_add((uc_engine *)uc, &g_p21.cfg36_read_hook, UC_HOOK_MEM_READ,
                         (void *)cfg36_mem_read_cb, NULL, g_p21.cfg36_guest,
                         (uint64_t)g_p21.cfg36_guest + CFG_RECORD_SIZE - 1u);
        if (ue == UC_ERR_OK) {
            g_p21.cfg36_read_hook_armed = 1;
            printf("[JJFB_P21] CFG36_READ_HOOK_ARMED va=0x%X evidence=OBSERVED\n",
                   g_p21.cfg36_guest);
            fflush(stdout);
        }
    }
    /* No broad UC_HOOK_MEM_WRITE — too slow for product-length runs. */
    (void)ue;
}
#else
static void snapshot_selected_state(void *uc, uint32_t pc, uint32_t r9, const char *why) {
    (void)uc;
    (void)pc;
    (void)r9;
    (void)why;
}
static void arm_hooks(void *uc) { (void)uc; }
#endif

void p21_note_file_io(const char *api, const char *path, const char *mode, int32_t offset,
                      uint32_t requested_size, int32_t returned_size, int32_t return_value,
                      uint32_t buffer_guest, const void *host_buf, uint32_t host_buf_len) {
    uint32_t pc = 0, lr = 0;
    uint8_t dig[32];
    char sha[72];
    char first64[140];
    const char *mod;
    if (!p21_enabled()) return;
#ifdef GWY_HAVE_UNICORN
    if (g_p21.uc) {
        uc_reg_read((uc_engine *)g_p21.uc, UC_ARM_REG_PC, &pc);
        uc_reg_read((uc_engine *)g_p21.uc, UC_ARM_REG_LR, &lr);
    }
#endif
    if (!caller_interesting(pc, path) && !caller_interesting(lr, path)) return;
    if (g_p21.io_seq >= IO_CAP) return;
    ensure_csvs();
    mod = mod_at_pc(pc);
    sha[0] = 0;
    first64[0] = 0;
    if (host_buf && host_buf_len) {
        gwy_sha256(host_buf, host_buf_len > 4096 ? 4096 : host_buf_len, dig);
        gwy_sha256_hex(dig, sha);
        hex64((const uint8_t *)host_buf, host_buf_len, first64, sizeof(first64));
    }
    g_p21.io_seq++;
    if (g_p21.io_csv) {
        fprintf(g_p21.io_csv,
                "%u,0x%X,\"%s\",0x%X,\"%s\",\"%s\",\"%s\",%d,%u,%d,%d,0x%X,%s,%s,\"%s\",%u\n",
                g_p21.io_seq, pc, mod, lr, api ? api : "?", path ? path : "",
                mode ? mode : "", (int)offset, requested_size, (int)returned_size,
                (int)return_value, buffer_guest, sha[0] ? sha : "-",
                first64[0] ? first64 : "-", mod, g_p21.timer_gen);
        fflush(g_p21.io_csv);
    }
    if (is_cfg_path(path) && api && strstr(api, "open")) {
        if (!g_p21.gate_open) {
            g_p21.gate_open = 1;
            printf("[JJFB_P21] gate=CFG_FILE_OPENED path=\"%s\" pc=0x%X evidence=OBSERVED\n",
                   path ? path : "?", pc);
            fflush(stdout);
        }
    }
    if (is_cfg_path(path) && api && (strstr(api, "read") || strstr(api, "10112")) &&
        buffer_guest && returned_size > 0) {
        scan_cfg_buffer(buffer_guest, (uint32_t)returned_size, pc, path,
                        (const uint8_t *)host_buf, host_buf_len);
#ifdef GWY_HAVE_UNICORN
        arm_hooks(g_p21.uc);
#endif
    }
}

void p21_note_plat_10112(const char *path, const char *ns, const char *host, uint32_t buf,
                         uint32_t len, int loaded, int ret, uint32_t caller_pc) {
    uint8_t *tmp = NULL;
    if (!p21_enabled()) return;
    (void)ns;
    (void)host;
    p21_note_file_io("plat_10112", path, "read_all", 0, len, loaded ? (int32_t)len : -1, ret,
                     buf, NULL, 0);
    if (!loaded || !buf || !len) return;
    if (!is_cfg_path(path) && !(path && strcmp(path, "cfg.bin") == 0)) return;
    if (!g_p21.gate_open) {
        g_p21.gate_open = 1;
        printf("[JJFB_P21] gate=CFG_FILE_OPENED path=\"%s\" via=10112 evidence=OBSERVED\n",
               path ? path : "?");
        fflush(stdout);
    }
    snprintf(g_p21.cfg_source_ns, sizeof(g_p21.cfg_source_ns), "%s", ns ? ns : "");
#ifdef GWY_HAVE_UNICORN
    if (g_p21.uc && len <= 256u * 1024u) {
        tmp = (uint8_t *)malloc(len);
        if (tmp && guest_memory_uc_peek((struct uc_struct *)g_p21.uc, buf, tmp, len)) {
            scan_cfg_buffer(buf, len, caller_pc, path, tmp, len);
            arm_hooks(g_p21.uc);
        }
        free(tmp);
    }
#else
    (void)caller_pc;
#endif
}

static void note_param_token(const char *field, uint32_t raw_off, uint32_t pc, const char *mod,
                             const char *val_s, int32_t val_i, int is_int) {
    P21Field *f = NULL;
    if (strcmp(field, "napptype") == 0) f = &g_p21.f_napptype;
    else if (strcmp(field, "nextid") == 0) f = &g_p21.f_nextid;
    else if (strcmp(field, "ncode") == 0) f = &g_p21.f_ncode;
    else if (strcmp(field, "narg") == 0) f = &g_p21.f_narg;
    else if (strcmp(field, "narg1") == 0) f = &g_p21.f_narg1;
    else if (strcmp(field, "nmrpname") == 0) f = &g_p21.f_nmrpname;
    else if (strcmp(field, "gwyblink") == 0) f = &g_p21.f_gwyblink;
    if (!f) return;
    if (!f->seen_raw) {
        f->seen_raw = 1;
        f->raw_off = raw_off;
        f->parse_pc = pc;
        snprintf(f->parse_mod, sizeof(f->parse_mod), "%s", mod ? mod : "?");
        if (is_int) {
            f->parsed = 1;
            f->value_i = val_i;
        } else {
            f->parsed = 1;
            snprintf(f->value_s, sizeof(f->value_s), "%s", val_s ? val_s : "");
        }
        if (is_gamelist_mod(mod)) f->gamelist_read = 1;
        ensure_csvs();
        if (g_p21.param_csv) {
            if (is_int)
                fprintf(g_p21.param_csv,
                        "%s,%u,0x%X,\"%s\",%d,0x%X,\"%s\",0x%X,%d,\"token_read\"\n", field,
                        raw_off, pc, mod ? mod : "?", (int)val_i, f->write_va,
                        f->write_owner[0] ? f->write_owner : "-", f->write_pc,
                        f->gamelist_read);
            else
                fprintf(g_p21.param_csv,
                        "%s,%u,0x%X,\"%s\",\"%s\",0x%X,\"%s\",0x%X,%d,\"token_read\"\n", field,
                        raw_off, pc, mod ? mod : "?", val_s ? val_s : "", f->write_va,
                        f->write_owner[0] ? f->write_owner : "-", f->write_pc,
                        f->gamelist_read);
            fflush(g_p21.param_csv);
        }
    } else if (is_gamelist_mod(mod) && !f->gamelist_read) {
        f->gamelist_read = 1;
        ensure_csvs();
        if (g_p21.param_csv) {
            fprintf(g_p21.param_csv, "%s,%u,0x%X,\"%s\",(reread),0x%X,\"%s\",0x%X,1,"
                                     "\"gamelist_consumer_read\"\n",
                    field, raw_off, pc, mod ? mod : "?", f->write_va,
                    f->write_owner[0] ? f->write_owner : "-", f->write_pc);
            fflush(g_p21.param_csv);
        }
    }
}

void p21_note_param_byte_read(uint32_t pc, const char *module, uint32_t addr, uint32_t size,
                              const uint8_t *bytes, const uint32_t regs[13], uint32_t lr,
                              uint32_t sp, uint32_t r9) {
    uint32_t off;
    (void)size;
    (void)bytes;
    (void)regs;
    (void)lr;
    (void)sp;
    (void)r9;
    if (!p21_enabled()) return;
    if (!g_p21.param_va || addr < g_p21.param_va ||
        addr >= g_p21.param_va + g_p21.param_len)
        return;
    if (g_p21.param_seq >= PARAM_CAP) return;
    g_p21.param_seq++;
    off = addr - g_p21.param_va;
    if (g_p21.param_raw[0]) {
        /* Map byte offset to nearest field token. */
        static const struct {
            const char *key;
            const char *field;
            int is_int;
            int32_t ival;
            const char *sval;
        } toks[] = {
            {"napptype=", "napptype", 1, 12, NULL},
            {"nextid=", "nextid", 1, 482, NULL},
            {"ncode=", "ncode", 1, 512, NULL},
            {"narg=", "narg", 1, 0, NULL},
            {"narg1=", "narg1", 1, 1, NULL},
            {"nmrpname=", "nmrpname", 0, 0, "gwy/jjfb.mrp"},
            {"gwyblink", "gwyblink", 0, 0, "present"},
        };
        size_t t;
        for (t = 0; t < sizeof(toks) / sizeof(toks[0]); t++) {
            const char *p = strstr(g_p21.param_raw, toks[t].key);
            if (!p) continue;
            {
                uint32_t toff = (uint32_t)(p - g_p21.param_raw);
                uint32_t tlen = (uint32_t)strlen(toks[t].key) + 8u;
                if (off >= toff && off < toff + tlen) {
                    note_param_token(toks[t].field, toff, pc, module,
                                     toks[t].sval, toks[t].ival, toks[t].is_int);
                }
            }
        }
    }
#ifdef GWY_HAVE_UNICORN
    arm_hooks(g_p21.uc);
#endif
}

/* Called from launch_param_mapped path via gwy_ext_obs — set raw buffer. */
void p21_set_launch_param(uint32_t va, uint32_t len, const char *entry) {
    if (!p21_enabled()) return;
    g_p21.param_va = va;
    g_p21.param_len = len;
    if (entry) snprintf(g_p21.param_raw, sizeof(g_p21.param_raw), "%s", entry);
}

void p21_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                 uint32_t lr, uint32_t sp, uint32_t cpsr) {
    uint32_t off;
    uint32_t norm;
    int watch = 0;
    if (!p21_enabled()) return;
    if (uc) g_p21.uc = uc;
    if (module_name && is_gamelist_mod(module_name)) {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *gm =
            reg ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
        if (gm && gm->map.guest_code_base) {
            g_p21.gl_base = gm->map.guest_code_base;
            g_p21.gl_size = gm->map.guest_code_size;
        }
    }
    if (!g_p21.gl_base) return;
    norm = pc & ~1u;
    if (norm < g_p21.gl_base || norm >= g_p21.gl_base + g_p21.gl_size) return;
    off = norm - g_p21.gl_base;

    g_p21.pc_ring[g_p21.pc_ring_next % RING_BEFORE] = pc;
    g_p21.pc_ring_next++;
    if (g_p21.pc_ring_n < RING_BEFORE) g_p21.pc_ring_n++;

    if (off == OFF_SELECT_1 || off == OFF_SELECT_2 || off == OFF_SELECT_3 ||
        off == OFF_SELECT_4 || off == OFF_CFG_LOADER || off == OFF_CFG_DISPATCH ||
        off == OFF_DESC_BUILDER)
        watch = 1;
    if (g_p21.capture_insn && g_p21.capture_left > 0) watch = 1;

    if (!watch) return;
    if (g_p21.br_seq >= BR_CAP && !g_p21.capture_insn) return;
    ensure_csvs();
    if (g_p21.sel_csv && regs) {
        uint8_t insn[4] = {0};
        char insn_hex[16];
        const char *hint = "?";
#ifdef GWY_HAVE_UNICORN
        if (uc) guest_memory_uc_peek((struct uc_struct *)uc, norm, insn, 4);
#endif
        snprintf(insn_hex, sizeof(insn_hex), "%02X%02X%02X%02X", insn[0], insn[1], insn[2],
                 insn[3]);
        if (off == OFF_SELECT_1 || off == OFF_SELECT_2 || off == OFF_SELECT_3 ||
            off == OFF_SELECT_4)
            hint = "select_site";
        else if (off == OFF_CFG_LOADER)
            hint = "cfg_loader";
        else if (off == OFF_CFG_DISPATCH)
            hint = "cfg_dispatch_10112";
        else if (off == OFF_DESC_BUILDER)
            hint = "desc_builder";
        else if (g_p21.capture_insn)
            hint = "post_cfg36_read_window";
        fprintf(g_p21.sel_csv,
                "%u,CODE,0x%X,0x%X,\"%s\",0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s,\"%s\",,,,,\"observe\"\n",
                ++g_p21.br_seq, pc, off, module_name ? module_name : "gamelist.ext", regs[0],
                regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7], regs[8],
                regs[9], regs[10], regs[11], regs[12], sp, lr, cpsr, insn_hex, hint);
        fflush(g_p21.sel_csv);
    }
#ifdef GWY_HAVE_UNICORN
    if (off == OFF_SELECT_1 || off == OFF_SELECT_2 || off == OFF_SELECT_3 ||
        off == OFF_SELECT_4) {
        snapshot_selected_state(uc, pc, regs ? regs[9] : 0, "select_site");
    }
    arm_hooks(uc);
#endif
    if (g_p21.capture_insn && g_p21.capture_left > 0) {
        g_p21.capture_left--;
        if (g_p21.capture_left == 0) g_p21.capture_insn = 0;
    }
}

void p21_on_timer_fire_begin(void *uc, uint32_t helper, uint32_t p_guest, uint32_t erw,
                             uint32_t period_ms, uint64_t generation) {
    (void)helper;
    (void)p_guest;
    (void)period_ms;
    if (!p21_enabled()) return;
    if (uc) g_p21.uc = uc;
    g_p21.timer_gen = (uint32_t)generation;
    g_p21.timer_pre_file_seq = g_p21.io_seq;
    g_p21.timer_pre_cfg_writes = g_p21.cfg_buffer_writes;
    g_p21.timer_pre_valid = 0;
#ifdef GWY_HAVE_UNICORN
    if (uc && erw) {
        uint32_t base = erw + STATE_OFF;
        if (guest_memory_uc_peek((struct uc_struct *)uc, base, g_p21.timer_pre_state,
                                 sizeof(g_p21.timer_pre_state))) {
            g_p21.timer_pre_valid = 1;
        }
    }
#else
    (void)erw;
#endif
}

void p21_on_timer_fire_end(void *uc, uint32_t helper, uint32_t method, uint32_t p_guest,
                           uint32_t erw, int32_t ret) {
    uint32_t file_delta;
    uint32_t cfg_w_delta;
    int state_changed = 0;
    int erw_changed = 0;
    const char *cls = "unknown";
    if (!p21_enabled()) return;
    (void)p_guest;
    if (g_p21.timer_seq >= TIMER_CAP) return;
    ensure_csvs();
    file_delta = g_p21.io_seq - g_p21.timer_pre_file_seq;
    cfg_w_delta = g_p21.cfg_buffer_writes - g_p21.timer_pre_cfg_writes;
#ifdef GWY_HAVE_UNICORN
    if (uc && erw && g_p21.timer_pre_valid) {
        uint8_t cur[64];
        if (guest_memory_uc_peek((struct uc_struct *)uc, erw + STATE_OFF, cur, sizeof(cur))) {
            if (memcmp(cur, g_p21.timer_pre_state, sizeof(cur)) != 0) {
                state_changed = 1;
                erw_changed = 1;
            }
        }
        snapshot_selected_state(uc, 0, erw, "timer_fire_end");
    }
#else
    (void)uc;
    (void)erw;
#endif
    if (file_delta == 0 && cfg_w_delta == 0 && !state_changed) {
        g_p21.timer_idle_streak++;
        cls = (g_p21.timer_idle_streak >= 3) ? "idle_no_io_no_state" : "idle_or_ui_refresh";
    } else {
        g_p21.timer_idle_streak = 0;
        if (file_delta > 0 && is_cfg_path(g_p21.cfg_source))
            cls = "cfg_list_poll";
        else if (file_delta > 0)
            cls = "file_activity";
        else if (state_changed)
            cls = "state_update";
        else
            cls = "other";
    }
    g_p21.timer_seq++;
    if (g_p21.timer_csv) {
        fprintf(g_p21.timer_csv, "%u,%u,0x%X,%u,%d,%d,%u,0,%u,0,%d,\"%s\",\"fire_end\"\n",
                g_p21.timer_gen, 0u, helper, method, (int)ret, erw_changed, file_delta,
                cfg_w_delta, state_changed, cls);
        fflush(g_p21.timer_csv);
    }
    printf("[JJFB_P21_TIMER_DIFF] gen=%u helper=0x%X file_delta=%u cfg_writes=%u "
           "state_changed=%d class=%s evidence=OBSERVED\n",
           g_p21.timer_gen, helper, file_delta, cfg_w_delta, state_changed, cls);
    fflush(stdout);
}

void p21_finalize(const char *stop_reason) {
    if (!p21_enabled()) return;
    ensure_csvs();
    /* Flush final param rows for fields never written. */
    if (g_p21.param_csv) {
        struct {
            const char *n;
            P21Field *f;
        } rows[] = {
            {"napptype", &g_p21.f_napptype},
            {"nextid", &g_p21.f_nextid},
            {"ncode", &g_p21.f_ncode},
            {"narg", &g_p21.f_narg},
            {"narg1", &g_p21.f_narg1},
            {"nmrpname", &g_p21.f_nmrpname},
            {"gwyblink", &g_p21.f_gwyblink},
        };
        size_t i;
        for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
            if (!rows[i].f->seen_raw) {
                fprintf(g_p21.param_csv, "%s,,,\"\",\"\",,,,0,\"never_observed\"\n",
                        rows[i].n);
            }
        }
        fflush(g_p21.param_csv);
    }
    printf("[JJFB_P21_FINAL] stop=%s fmt=%d open=%d record_read=%d cfg36=%d selected=%d "
           "records=%d cfg36_va=0x%X src_off=%u idle_timer_streak=%d evidence=OBSERVED\n",
           stop_reason ? stop_reason : "?", g_p21.gate_fmt, g_p21.gate_open,
           g_p21.gate_record_read, g_p21.gate_cfg36_present, g_p21.gate_cfg36_selected,
           g_p21.cfg_record_count, g_p21.cfg36_guest, g_p21.cfg36_source_off,
           g_p21.timer_idle_streak);
    fflush(stdout);
}

int p21_gate_fmt_mapped(void) { return g_p21.gate_fmt; }
int p21_gate_file_opened(void) { return g_p21.gate_open; }
int p21_gate_record_read(void) { return g_p21.gate_record_read; }
int p21_gate_cfg36_present(void) { return g_p21.gate_cfg36_present; }
int p21_gate_cfg36_selected(void) { return g_p21.gate_cfg36_selected; }
uint32_t p21_cfg36_guest_va(void) { return g_p21.cfg36_guest; }
uint32_t p21_cfg36_source_offset(void) { return g_p21.cfg36_source_off; }
int p21_cfg_list_record_count(void) { return g_p21.cfg_record_count; }
const char *p21_cfg_source_path(void) { return g_p21.cfg_source; }
