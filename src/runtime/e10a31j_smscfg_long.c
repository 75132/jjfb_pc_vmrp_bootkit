#include "gwy_launcher/e10a31j_smscfg_long.h"

#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/sha256.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { E10A31J_WIN_FULL = 0, E10A31J_WIN_GAMELIST = 1 };

static struct {
    int known, enabled, window, armed_writes, past_gamelist, stop;
    unsigned long long run_id;
    void *uc;
    uint32_t dsm_generation, mr_table, cfg_base, erw_hint, erw_size, seq;
    int cfg_mapped, ptr_changed_ever;
    int any_write, write_349, write_gpt, write_before_gamelist, write_during_gamelist;
    int full_buffer_write, zero_write, setbytes_349, testcom_502, indication_code6;
    int direct_copy_writer, load_called, dsm_cfg_open, dsm_cfg_missing, dsm_cfg_loaded;
    int load_not_called_flag, hash_changed_before_gamelist, hash_never_changed;
    int partially_init, method0_fail_empty, have_sha;
    uint8_t last_sha[32];
    char phase[48], package[48], module[48];
    FILE *ptr_csv, *write_csv, *api_csv, *io_csv, *ckpt_csv;
#ifdef GWY_HAVE_UNICORN
    uc_hook mem_write_hook;
    int mem_write_armed;
    uint32_t hooked_base;
#endif
} g_j;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static unsigned long long run_id_now(void) {
    const char *e = getenv("JJFB_E10A31J_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    e = getenv("JJFB_E10A_RUN_ID");
    if (e && e[0]) return strtoull(e, NULL, 10);
    return (unsigned long long)time(NULL);
}

static void ensure(void) {
    const char *w;
    if (g_j.known) return;
    g_j.known = 1;
    g_j.enabled = env1("JJFB_E10A31J_MODE");
    g_j.run_id = run_id_now();
    g_j.window = E10A31J_WIN_FULL;
    w = getenv("JJFB_E10A31J_WINDOW");
    if (w && (w[0] == 'g' || w[0] == 'G')) g_j.window = E10A31J_WIN_GAMELIST;
    g_j.hash_never_changed = 1;
    snprintf(g_j.phase, sizeof(g_j.phase), "BOOT");
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
    if (!g_j.ptr_csv)
        g_j.ptr_csv = open_csv("JJFB_E10A31J_PTR_CSV", "reports/e10a31j_smscfg_pointer_lifetime.csv",
            "run_id,seq,phase,package,module,dsm_generation,mr_table,slot_1c0,cfg_base,cfg_len,mapped,changed,note\n");
    if (!g_j.write_csv)
        g_j.write_csv = open_csv("JJFB_E10A31J_WRITE_CSV", "reports/e10a31j_smscfg_long_write_trace.csv",
            "run_id,seq,phase,package,module,dispatch,pc,lr,sp,r0,r1,r2,r3,r4,r9,addr,cfg_off,size,old_hex,new_hex,writer,note\n");
    if (!g_j.api_csv)
        g_j.api_csv = open_csv("JJFB_E10A31J_API_CSV", "reports/e10a31j_smscfg_writer_api_trace.csv",
            "run_id,seq,phase,api,slot,pc,lr,r0,r1,r2,r3,r9,extra,note\n");
    if (!g_j.io_csv)
        g_j.io_csv = open_csv("JJFB_E10A31J_IO_CSV", "reports/e10a31j_dsm_cfg_io_trace.csv",
            "run_id,seq,phase,op,module,guest_path,host_path,exists,size,rc,pc,lr,dst,dst_len,overlaps_smscfg,note\n");
    if (!g_j.ckpt_csv)
        g_j.ckpt_csv = open_csv("JJFB_E10A31J_CKPT_CSV", "reports/e10a31j_smscfg_checkpoints.csv",
            "run_id,seq,phase,cfg_base,sha256,nonzero,first_nz,last_nz,hex_000_03f,hex_340_35f,gpt_hex,note\n");
}

static int read_guest(void *uc, uint32_t addr, void *dst, size_t n) {
#ifdef GWY_HAVE_UNICORN
    if (!uc || !dst || !n) return 0;
    return uc_mem_read((uc_engine *)uc, addr, dst, n) == UC_ERR_OK;
#else
    (void)uc; (void)addr; (void)dst; (void)n; return 0;
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

static int path_is_dsm_cfg(const char *p) {
    const char *s;
    if (!p || !p[0]) return 0;
    for (s = p; *s; s++) {
        if ((s[0] == 'd' || s[0] == 'D') && (s[1] == 's' || s[1] == 'S') &&
            (s[2] == 'm' || s[2] == 'M') && s[3] == '.' && (s[4] == 'c' || s[4] == 'C') &&
            (s[5] == 'f' || s[5] == 'F') && (s[6] == 'g' || s[6] == 'G'))
            return 1;
    }
    return 0;
}

static int overlaps_cfg(uint32_t addr, uint32_t len) {
    uint64_t a0, a1, c0, c1;
    if (!g_j.cfg_base || !len) return 0;
    a0 = addr; a1 = (uint64_t)addr + len;
    c0 = g_j.cfg_base; c1 = (uint64_t)g_j.cfg_base + E10A31J_SMS_CFG_LEN;
    return a0 < c1 && a1 > c0;
}

static int covers_349(uint32_t addr, uint32_t len) {
    uint32_t t0, t1, a1;
    if (!g_j.cfg_base || !len) return 0;
    t0 = g_j.cfg_base + E10A31J_GPT_OFF; t1 = t0 + 3u; a1 = addr + len;
    return addr < t1 && a1 > t0;
}

static int monitoring_active(void) {
    if (!g_j.enabled) return 0;
    if (g_j.window == E10A31J_WIN_FULL) return 1;
    return g_j.past_gamelist;
}

void e10a31j_mark_milestone(const char *name, const char *note) {
    ensure();
    if (!g_j.enabled || !name) return;
    printf("[JJFB_E10A31J_MILESTONE] name=%s note=%s phase=%s cfg_base=0x%X evidence=OBSERVED\n",
           name, note ? note : "", g_j.phase, g_j.cfg_base);
    fflush(stdout);
}

int e10a31j_enabled(void) { ensure(); return g_j.enabled; }
const char *e10a31j_window_name(void) {
    ensure();
    return g_j.window == E10A31J_WIN_GAMELIST ? "gamelist" : "full";
}
int e10a31j_stop_requested(void) { ensure(); return g_j.stop; }

void e10a31j_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_j.mem_write_armed && g_j.uc) {
        uc_hook_del((uc_engine *)g_j.uc, g_j.mem_write_hook);
        g_j.mem_write_armed = 0;
    }
#endif
    if (g_j.ptr_csv) fclose(g_j.ptr_csv);
    if (g_j.write_csv) fclose(g_j.write_csv);
    if (g_j.api_csv) fclose(g_j.api_csv);
    if (g_j.io_csv) fclose(g_j.io_csv);
    if (g_j.ckpt_csv) fclose(g_j.ckpt_csv);
    memset(&g_j, 0, sizeof(g_j));
}

static void request_stop(const char *why) {
    if (g_j.stop) return;
    g_j.stop = 1;
    e10a31j_mark_milestone("E10A31J_STOP", why ? why : "stop");
}

static void emit_api(const char *api, uint32_t slot, uint32_t pc, uint32_t lr, uint32_t r0,
                     uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9, const char *extra,
                     const char *note) {
    ensure_csvs();
    g_j.seq++;
    if (g_j.api_csv) {
        fprintf(g_j.api_csv,
                "%llu,%u,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\",\"%s\"\n",
                g_j.run_id, g_j.seq, g_j.phase, api ? api : "", slot, pc, lr, r0, r1, r2, r3, r9,
                extra ? extra : "", note ? note : "");
        fflush(g_j.api_csv);
    }
    printf("[JJFB_E10A31J_API] api=%s slot=0x%X pc=0x%X r0=0x%X r1=0x%X r2=0x%X note=%s evidence=OBSERVED\n",
           api ? api : "", slot, pc, r0, r1, r2, note ? note : "");
    fflush(stdout);
}

static void emit_write(uint32_t pc, uint32_t lr, uint32_t sp, uint32_t r0, uint32_t r1, uint32_t r2,
                       uint32_t r3, uint32_t r4, uint32_t r9, uint32_t addr, uint32_t size,
                       const uint8_t *oldb, const uint8_t *newb, const char *writer,
                       const char *note) {
    char oh[80], nh[80];
    uint32_t off = (g_j.cfg_base && addr >= g_j.cfg_base) ? (addr - g_j.cfg_base) : 0xffffffffu;
    int n = (int)((size > 32u) ? 32u : size);
    ensure_csvs();
    hex_n(oldb, n, oh, sizeof(oh));
    hex_n(newb, n, nh, sizeof(nh));
    g_j.seq++;
    if (g_j.write_csv) {
        fprintf(g_j.write_csv,
                "%llu,%u,%s,%s,%s,mem_write,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%s,%s,%s,\"%s\"\n",
                g_j.run_id, g_j.seq, g_j.phase, g_j.package, g_j.module, pc, lr, sp, r0, r1, r2, r3,
                r4, r9, addr, off, size, oh, nh, writer ? writer : "", note ? note : "");
        fflush(g_j.write_csv);
    }
    printf("[JJFB_E10A31J_WRITE] addr=0x%X off=0x%X size=%u old=%s new=%s writer=%s phase=%s evidence=OBSERVED\n",
           addr, off, size, oh, nh, writer ? writer : "", g_j.phase);
    fflush(stdout);
}

static void classify_write(uint32_t addr, uint32_t size, const uint8_t *newb) {
    int i, all_zero = 1;
    g_j.any_write = 1;
    e10a31j_mark_milestone("SMSCFG_ANY_WRITE", g_j.phase);
    if (g_j.past_gamelist) {
        g_j.write_during_gamelist = 1;
        e10a31j_mark_milestone("SMSCFG_WRITE_DURING_GAMELIST", g_j.phase);
    } else {
        g_j.write_before_gamelist = 1;
        e10a31j_mark_milestone("SMSCFG_WRITE_BEFORE_GAMELIST", g_j.phase);
    }
    if (size >= E10A31J_SMS_CFG_LEN / 2u) {
        g_j.full_buffer_write = 1;
        e10a31j_mark_milestone("SMSCFG_FULL_BUFFER_WRITE", g_j.phase);
    }
    for (i = 0; i < (int)size && i < 64; i++) if (newb[i]) all_zero = 0;
    if (all_zero) {
        g_j.zero_write = 1;
        e10a31j_mark_milestone("SMSCFG_ZERO_WRITE", g_j.phase);
    }
    if (covers_349(addr, size)) {
        uint32_t rel = (addr <= g_j.cfg_base + E10A31J_GPT_OFF)
                           ? (g_j.cfg_base + E10A31J_GPT_OFF - addr) : 0;
        g_j.write_349 = 1;
        e10a31j_mark_milestone("SMSCFG_349_WRITE", g_j.phase);
        if (rel + 2 < size && newb[rel] == 'G' && newb[rel + 1] == 'P' && newb[rel + 2] == 'T') {
            g_j.write_gpt = 1;
            e10a31j_mark_milestone("SMSCFG_GPT_WRITE", g_j.phase);
            request_stop("GPT_written_at_349");
        }
    }
}

#ifdef GWY_HAVE_UNICORN
static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                         int64_t value, void *user_data) {
    uint32_t pc = 0, lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, r9 = 0;
    uint8_t oldb[64], newb[64];
    uint32_t addr = (uint32_t)address;
    int n = size;
    (void)type; (void)user_data;
    if (!g_j.enabled || g_j.stop || !monitoring_active()) return;
    if (!overlaps_cfg(addr, (uint32_t)((size > 0) ? size : 1))) return;
    if (n < 1) n = 1;
    if (n > 64) n = 64;
    memset(oldb, 0, sizeof(oldb));
    memset(newb, 0, sizeof(newb));
    if (size > 0 && size <= 8) {
        uint64_t v = (uint64_t)value;
        memcpy(newb, &v, (size_t)size);
        read_guest(uc, addr, oldb, (size_t)size);
    } else {
        read_guest(uc, addr, newb, (size_t)n);
        memcpy(oldb, newb, (size_t)n);
    }
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    emit_write(pc, lr, sp, r0, r1, r2, r3, r4, r9, addr, (uint32_t)size, oldb, newb, "UC_MEM_WRITE", g_j.phase);
    classify_write(addr, (uint32_t)size, newb);
}
#endif

static void install_write_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_err err;
    if (!uc || !g_j.cfg_base || !monitoring_active()) return;
    if (g_j.mem_write_armed && g_j.hooked_base == g_j.cfg_base) return;
    if (g_j.mem_write_armed) {
        uc_hook_del((uc_engine *)uc, g_j.mem_write_hook);
        g_j.mem_write_armed = 0;
    }
    err = uc_hook_add((uc_engine *)uc, &g_j.mem_write_hook, UC_HOOK_MEM_WRITE, (void *)on_mem_write,
                      NULL, (uint64_t)g_j.cfg_base,
                      (uint64_t)g_j.cfg_base + (uint64_t)E10A31J_SMS_CFG_LEN - 1ull);
    if (err == UC_ERR_OK) {
        g_j.mem_write_armed = 1;
        g_j.hooked_base = g_j.cfg_base;
        g_j.armed_writes = 1;
        e10a31j_mark_milestone("SMSCFG_WRITE_HOOK_ARMED", "full_0x10E0");
    } else {
        printf("[JJFB_E10A31J] write_hook_add_failed err=%d cfg_base=0x%X\n", (int)err, g_j.cfg_base);
        fflush(stdout);
    }
#else
    (void)uc;
#endif
}

static void emit_pointer_row(uint32_t slot, uint32_t cfg, int mapped, int changed, const char *note) {
    ensure_csvs();
    g_j.seq++;
    if (g_j.ptr_csv) {
        fprintf(g_j.ptr_csv, "%llu,%u,%s,%s,%s,%u,0x%X,0x%X,0x%X,0x%X,%d,%d,\"%s\"\n",
                g_j.run_id, g_j.seq, g_j.phase, g_j.package, g_j.module, g_j.dsm_generation,
                g_j.mr_table, slot, cfg, E10A31J_SMS_CFG_LEN, mapped, changed, note ? note : "");
        fflush(g_j.ptr_csv);
    }
}

static void apply_cfg_base(void *uc, uint32_t cfg, const char *note);
static void checkpoint(void *uc, const char *phase);

static void apply_cfg_base(void *uc, uint32_t cfg, const char *note) {
    int changed = 0, mapped = 0;
    uint8_t probe[4];
    if (!cfg) {
        emit_pointer_row(0, 0, 0, 0, note ? note : "cfg_null");
        if (!g_j.cfg_base)
            e10a31j_mark_milestone("SMSCFG_POINTER_PUBLICATION_MISSING", note ? note : "");
        return;
    }
    mapped = read_guest(uc, cfg, probe, 4);
    if (g_j.cfg_base && g_j.cfg_base != cfg) {
        changed = 1;
        g_j.ptr_changed_ever = 1;
        e10a31j_mark_milestone("SMSCFG_POINTER_RELOCATED", note ? note : "");
    } else if (g_j.cfg_base == cfg) {
        e10a31j_mark_milestone("SMSCFG_POINTER_STABLE", note ? note : "");
    }
    g_j.cfg_base = cfg;
    g_j.cfg_mapped = mapped;
    emit_pointer_row(cfg, cfg, mapped, changed, note);
    install_write_hooks(uc);
}


void e10a31j_on_erw(void *uc, uint32_t erw_base, uint32_t erw_size) {
    ensure();
    if (!g_j.enabled || !erw_base) return;
    if (uc) g_j.uc = uc;
    if (erw_base != g_j.erw_hint) {
        g_j.erw_hint = erw_base;
        g_j.erw_size = erw_size;
        e10a31j_mark_milestone("SMSCFG_ERW_HINT", "dsm_cfunction_erw");
    }
    /* Prefer live slot if published; else ERW+0x8D4 (proven in E10A31H). */
    e10a31j_poll_pointer(g_j.uc, g_j.phase[0] ? g_j.phase : "ERW_PUBLISH", g_j.package, g_j.module);
    if (!g_j.cfg_base && erw_size >= (0x8D4u + E10A31J_SMS_CFG_LEN)) {
        apply_cfg_base(g_j.uc, erw_base + 0x8D4u, "erw+0x8D4_on_publish");
        checkpoint(g_j.uc, "DSM_MAP_DATA");
    } else if (g_j.cfg_base) {
        checkpoint(g_j.uc, g_j.phase[0] ? g_j.phase : "ERW_PUBLISH");
    }
}
void e10a31j_on_mr_table(uint32_t mr_table_guest) {
    ensure();
    if (!g_j.enabled) return;
    if (mr_table_guest && mr_table_guest != g_j.mr_table) {
        g_j.mr_table = mr_table_guest;
        g_j.dsm_generation++;
        e10a31j_mark_milestone("SMSCFG_MR_TABLE_SET", "extchunk_or_bridge");
    }
}

void e10a31j_poll_pointer(void *uc, const char *phase, const char *package, const char *module) {
    uint32_t slot = 0, mt;
    ensure();
    if (!g_j.enabled) return;
    if (uc) g_j.uc = uc;
    if (phase && phase[0]) snprintf(g_j.phase, sizeof(g_j.phase), "%s", phase);
    if (package && package[0]) snprintf(g_j.package, sizeof(g_j.package), "%s", package);
    if (module && module[0]) snprintf(g_j.module, sizeof(g_j.module), "%s", module);
    mt = g_j.mr_table ? g_j.mr_table : ext_chunk_provider_mr_table_guest();
    if (mt) g_j.mr_table = mt;
    if (!mt || !g_j.uc) {
        emit_pointer_row(0, g_j.cfg_base, g_j.cfg_mapped, 0, "no_mr_table");
        return;
    }
    if (!read_guest(g_j.uc, mt + E10A31J_MR_TABLE_SMSCFG_OFF, &slot, 4)) {
        emit_pointer_row(0, g_j.cfg_base, 0, 0, "slot_read_fail");
        if (g_j.erw_hint) {
            apply_cfg_base(g_j.uc, g_j.erw_hint + 0x8D4u, "erw+0x8D4_fallback_unreadable_slot");
        } else if (!g_j.cfg_base) {
            e10a31j_mark_milestone("SMSCFG_POINTER_PUBLICATION_MISSING", "slot_1c0_unreadable");
        }
        return;
    }
    if (slot) {
        apply_cfg_base(g_j.uc, slot, "mr_table+0x1C0");
    } else if (g_j.erw_hint) {
        /* DOCUMENTED live identity: sms_cfg lives at DSM cfunction ERW+0x8D4 */
        apply_cfg_base(g_j.uc, g_j.erw_hint + 0x8D4u, "erw+0x8D4_while_slot_1c0_zero");
    } else {
        emit_pointer_row(0, 0, 0, 0, "slot_1c0_zero");
        if (!g_j.cfg_base)
            e10a31j_mark_milestone("SMSCFG_POINTER_PUBLICATION_MISSING", "mr_table+0x1C0");
    }
}

static void checkpoint(void *uc, const char *phase) {
    uint8_t buf[E10A31J_SMS_CFG_LEN];
    uint8_t sha[32];
    char sha_hex[65], h0[132], h340[132], gpt[16];
    uint32_t i, nz = 0, first = 0xffffffffu, last = 0;
    ensure();
    if (!g_j.enabled) return;
    if (phase && phase[0]) snprintf(g_j.phase, sizeof(g_j.phase), "%s", phase);
    ensure_csvs();
    if (!g_j.cfg_base || !uc) {
        g_j.seq++;
        if (g_j.ckpt_csv) {
            fprintf(g_j.ckpt_csv, "%llu,%u,%s,0x0,,,,,,,,no_cfg_base\n", g_j.run_id, g_j.seq, g_j.phase);
            fflush(g_j.ckpt_csv);
        }
        return;
    }
    memset(buf, 0, sizeof(buf));
    if (!read_guest(uc, g_j.cfg_base, buf, sizeof(buf))) {
        g_j.seq++;
        if (g_j.ckpt_csv) {
            fprintf(g_j.ckpt_csv, "%llu,%u,%s,0x%X,,,,,,,,read_fail\n", g_j.run_id, g_j.seq, g_j.phase, g_j.cfg_base);
            fflush(g_j.ckpt_csv);
        }
        return;
    }
    for (i = 0; i < E10A31J_SMS_CFG_LEN; i++) {
        if (buf[i]) { nz++; if (first == 0xffffffffu) first = i; last = i; }
    }
    gwy_sha256(buf, E10A31J_SMS_CFG_LEN, sha);
    hex_n(sha, 32, sha_hex, sizeof(sha_hex));
    hex_n(buf, 64, h0, sizeof(h0));
    hex_n(buf + 0x340, 32, h340, sizeof(h340));
    hex_n(buf + E10A31J_GPT_OFF, 3, gpt, sizeof(gpt));
    if (g_j.have_sha && memcmp(g_j.last_sha, sha, 32) != 0) {
        g_j.hash_never_changed = 0;
        if (!g_j.past_gamelist) {
            g_j.hash_changed_before_gamelist = 1;
            e10a31j_mark_milestone("SMSCFG_INITIALIZED_UPSTREAM", phase);
        }
    }
    memcpy(g_j.last_sha, sha, 32);
    g_j.have_sha = 1;
    if (nz && buf[E10A31J_GPT_OFF] == 0 && buf[E10A31J_GPT_OFF + 1] == 0 && buf[E10A31J_GPT_OFF + 2] == 0) {
        g_j.partially_init = 1;
        e10a31j_mark_milestone("SMSCFG_PARTIALLY_INITIALIZED_GPT_MISSING", phase);
    }
    g_j.seq++;
    if (g_j.ckpt_csv) {
        fprintf(g_j.ckpt_csv, "%llu,%u,%s,0x%X,%s,%u,0x%X,0x%X,%s,%s,%s,\"%s\"\n",
                g_j.run_id, g_j.seq, g_j.phase, g_j.cfg_base, sha_hex, nz,
                (first == 0xffffffffu) ? 0 : first, last, h0, h340, gpt, "");
        fflush(g_j.ckpt_csv);
    }
    printf("[JJFB_E10A31J_CKPT] phase=%s cfg=0x%X sha=%s nz=%u gpt=%s evidence=OBSERVED\n",
           g_j.phase, g_j.cfg_base, sha_hex, nz, gpt);
    fflush(stdout);
}

static int phase_is(const char *phase, const char *key) {
    return phase && key && strstr(phase, key) != NULL;
}

void e10a31j_on_phase(void *uc, const char *phase, const char *module, const char *package) {
    ensure();
    if (!g_j.enabled) return;
    if (uc) g_j.uc = uc;
    if (phase && phase[0]) snprintf(g_j.phase, sizeof(g_j.phase), "%s", phase);
    if (module && module[0]) snprintf(g_j.module, sizeof(g_j.module), "%s", module);
    if (package && package[0]) snprintf(g_j.package, sizeof(g_j.package), "%s", package);
    e10a31j_poll_pointer(g_j.uc, g_j.phase, g_j.package, g_j.module);
    if (phase_is(phase, "GBRWCORE") || phase_is(phase, "DSM") || phase_is(phase, "CFUNCTION") ||
        phase_is(phase, "GAMELIST") || phase_is(phase, "METHOD") || phase_is(phase, "BOOT"))
        checkpoint(g_j.uc, g_j.phase);
}

void e10a31j_bind_uc(void *uc) {
    ensure();
    if (!g_j.enabled) return;
    g_j.uc = uc;
    snprintf(g_j.phase, sizeof(g_j.phase), "DSM_BOOT_ENTER");
    e10a31j_on_mr_table(ext_chunk_provider_mr_table_guest());
    e10a31j_poll_pointer(uc, "DSM_BOOT_ENTER", "dsm", "bridge");
    checkpoint(uc, "DSM_BOOT_ENTER");
    e10a31j_mark_milestone("E10A31J_ARMED", e10a31j_window_name());
}

void e10a31j_on_block_copy(void *uc, uint32_t dst, uint32_t src, uint32_t len) {
    uint8_t sample[64];
    ensure();
    if (!g_j.enabled || g_j.stop || !monitoring_active()) return;
    if (!overlaps_cfg(dst, len)) return;
    memset(sample, 0, sizeof(sample));
    if (uc) read_guest(uc, src, sample, (len > 64u) ? 64u : len);
    g_j.direct_copy_writer = 1;
    e10a31j_mark_milestone("SMSCFG_DIRECT_COPY_WRITER_FOUND", "memcpy");
    emit_api("memcpy_to_smscfg", 0, 0, 0, dst, src, len, 0, 0, "", "block_copy");
    classify_write(dst, len, sample);
    emit_write(0, 0, 0, dst, src, len, 0, 0, 0, dst, len, sample, sample, "memcpy", "block_copy");
    if (covers_349(dst, len)) request_stop("memcpy_covers_349");
}

void e10a31j_on_memset(void *uc, uint32_t dst, uint32_t value, uint32_t len) {
    uint8_t sample[8];
    ensure();
    if (!g_j.enabled || g_j.stop || !monitoring_active()) return;
    if (!overlaps_cfg(dst, len)) return;
    memset(sample, (int)(value & 0xff), sizeof(sample));
    emit_api("memset_to_smscfg", 0, 0, 0, dst, value, len, 0, 0, "", "memset");
    classify_write(dst, len, sample);
    (void)uc;
}

void e10a31j_on_file_read(void *uc, uint32_t dst, uint32_t len, int32_t ret, const char *api) {
    ensure();
    if (!g_j.enabled || g_j.stop) return;
    if (!overlaps_cfg(dst, len)) return;
    emit_api(api ? api : "file_read", 0, 0, 0, dst, len, (uint32_t)ret, 0, 0, "", "read_overlaps_smscfg");
    g_j.direct_copy_writer = 1;
    e10a31j_mark_milestone("SMSCFG_DIRECT_COPY_WRITER_FOUND", "file_read");
    if (len >= E10A31J_SMS_CFG_LEN || (uint32_t)ret >= E10A31J_SMS_CFG_LEN) {
        g_j.dsm_cfg_loaded = 1;
        e10a31j_mark_milestone("DSM_CFG_LOADED_TO_SMSCFG", api ? api : "read");
    }
    (void)uc;
}

void e10a31j_on_host_api_enter(void *uc, uint32_t slot, const char *name) {
    uint32_t pc = 0, lr = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    ensure();
    if (!g_j.enabled || !name) return;
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &lr);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0, &r0);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R1, &r1);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R2, &r2);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R3, &r3);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &r9);
    }
#else
    (void)uc;
#endif
    if (strcmp(name, "memcpy") == 0) { e10a31j_on_block_copy(uc, r0, r1, r2); return; }
    if (strcmp(name, "memset") == 0) { e10a31j_on_memset(uc, r0, r1, r2); return; }
    if (strcmp(name, "mr_read") == 0 || strcmp(name, "read") == 0) {
        emit_api(name, slot, pc, lr, r0, r1, r2, r3, r9, "", "enter");
        if (overlaps_cfg(r1, r2)) e10a31j_on_file_read(uc, r1, r2, 0, name);
        return;
    }
    if (strstr(name, "TestCom")) {
        char extra[64];
        snprintf(extra, sizeof(extra), "code=%u", r0);
        emit_api(name, slot, pc, lr, r0, r1, r2, r3, r9, extra, "enter");
        if (r0 == 502u || r1 == 502u) {
            g_j.testcom_502 = 1;
            e10a31j_mark_milestone("SMSCFG_TESTCOM_502_CALLED", name);
        }
        return;
    }
    if (strstr(name, "load_sms_cfg") || strstr(name, "sms_cfg")) {
        g_j.load_called = 1;
        e10a31j_mark_milestone("SMSCFG_LOAD_CALLED", name);
        emit_api(name, slot, pc, lr, r0, r1, r2, r3, r9, "", "enter");
        return;
    }
    if (strstr(name, "smsSetBytes") || strstr(name, "sms_set")) {
        char extra[80];
        snprintf(extra, sizeof(extra), "pos=0x%X len=%u", r0, r2);
        emit_api(name, slot, pc, lr, r0, r1, r2, r3, r9, extra, "enter");
        if (r0 == E10A31J_GPT_OFF) {
            g_j.setbytes_349 = 1;
            e10a31j_mark_milestone("SMSCFG_SETBYTES_349_CALLED", extra);
            request_stop("smsSetBytes_pos_349");
        }
        return;
    }
    if (strstr(name, "smsIndiaction") || strstr(name, "Indiaction")) {
        emit_api(name, slot, pc, lr, r0, r1, r2, r3, r9, "", "enter");
        if (uc && r0) {
            uint8_t chunk[8];
            if (read_guest(uc, r0, chunk, 8) && chunk[0] == 6) {
                g_j.indication_code6 = 1;
                e10a31j_mark_milestone("SMSCFG_INDICATION_CODE6_CALLED", name);
            }
        }
    }
}

void e10a31j_on_host_api_leave(void *uc, uint32_t slot, const char *name) {
    (void)uc; (void)slot; (void)name;
}

void e10a31j_on_unimpl_api(void *uc, uint32_t slot, const char *name) {
    ensure();
    if (!g_j.enabled || !name) return;
    e10a31j_on_host_api_enter(uc, slot, name);
    emit_api(name, slot, 0, 0, 0, 0, 0, 0, 0, "", "unimpl");
}

void e10a31j_on_vfs(void *uc, const char *op, const char *module, const char *guest_path,
                    const char *host_path, int exists, int rc, uint32_t pc, uint32_t lr) {
    ensure();
    if (!g_j.enabled) return;
    if (!path_is_dsm_cfg(guest_path) && !path_is_dsm_cfg(host_path)) return;
    ensure_csvs();
    g_j.seq++;
    g_j.dsm_cfg_open = 1;
    e10a31j_mark_milestone("DSM_CFG_OPEN_ATTEMPTED", guest_path ? guest_path : "");
    if (!exists) {
        g_j.dsm_cfg_missing = 1;
        e10a31j_mark_milestone("DSM_CFG_MISSING", guest_path ? guest_path : "");
    }
    if (g_j.io_csv) {
        fprintf(g_j.io_csv, "%llu,%u,%s,%s,%s,\"%s\",\"%s\",%d,0,%d,0x%X,0x%X,0x0,0,0,\"%s\"\n",
                g_j.run_id, g_j.seq, g_j.phase, op ? op : "", module ? module : "",
                guest_path ? guest_path : "", host_path ? host_path : "", exists, rc, pc, lr,
                exists ? "present" : "missing");
        fflush(g_j.io_csv);
    }
    printf("[JJFB_E10A31J_IO] op=%s guest=\"%s\" exists=%d rc=%d evidence=OBSERVED\n",
           op ? op : "", guest_path ? guest_path : "", exists, rc);
    fflush(stdout);
    (void)uc;
}

void e10a31j_on_ext_first_pc(void) {
    ensure();
    if (!g_j.enabled) return;
    g_j.past_gamelist = 1;
    snprintf(g_j.phase, sizeof(g_j.phase), "GAMELIST_EXT_FIRST_PC");
    e10a31j_poll_pointer(g_j.uc, "GAMELIST_EXT_FIRST_PC", "gamelist", "gamelist.ext");
    checkpoint(g_j.uc, "GAMELIST_EXT_FIRST_PC");
    install_write_hooks(g_j.uc);
    e10a31j_mark_milestone("GAMELIST_WINDOW_OPEN", e10a31j_window_name());
}

void e10a31j_on_method0_enter(void *uc, uint32_t helper) {
    ensure();
    if (!g_j.enabled) return;
    if (uc) g_j.uc = uc;
    snprintf(g_j.phase, sizeof(g_j.phase), "METHOD0_ENTER");
    e10a31j_poll_pointer(uc, "METHOD0_ENTER", "gamelist", "method0");
    checkpoint(uc, "METHOD0_ENTER");
    (void)helper;
}

void e10a31j_on_method0_return(void *uc, uint32_t helper, int32_t ret) {
    uint8_t gpt[4];
    ensure();
    if (!g_j.enabled) return;
    snprintf(g_j.phase, sizeof(g_j.phase), "METHOD0_FAIL");
    memset(gpt, 0, sizeof(gpt));
    if (uc && g_j.cfg_base) read_guest(uc, g_j.cfg_base + E10A31J_GPT_OFF, gpt, 3);
    checkpoint(uc, "METHOD0_FAIL");
    if (ret < 0 && gpt[0] == 0 && gpt[1] == 0 && gpt[2] == 0) {
        g_j.method0_fail_empty = 1;
        e10a31j_mark_milestone("METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG", "ret<0");
    }
    if (!g_j.any_write && !g_j.direct_copy_writer && !g_j.setbytes_349)
        e10a31j_mark_milestone("SMSCFG_NO_WRITER_API_OBSERVED", "boot_to_method0");
    if (!g_j.load_called) {
        g_j.load_not_called_flag = 1;
        e10a31j_mark_milestone("DSM_CFG_LOAD_NOT_CALLED", "no_load_sms_cfg");
    }
    if (g_j.hash_never_changed) e10a31j_mark_milestone("SMSCFG_NEVER_INITIALIZED", "hash_stable");
    if (g_j.dsm_cfg_missing && !g_j.any_write)
        e10a31j_mark_milestone("DSM_CFG_MISSING_NO_DEFAULT_INIT", "no_writer");
    if (g_j.write_before_gamelist)
        e10a31j_mark_milestone("SMSCFG_WRITER_PRECEDES_GAMELIST", "full_window");
    else if (g_j.write_during_gamelist)
        e10a31j_mark_milestone("SMSCFG_WRITER_DURING_GAMELIST", "gamelist");
    else
        e10a31j_mark_milestone("SMSCFG_NO_BOOTSTRAP_WRITER_OBSERVED", e10a31j_window_name());
    request_stop("method0_return");
    (void)helper;
}

void e10a31j_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9) {
    ensure();
    if (!g_j.enabled) return;
    if (pc == 0x2E31AEu && r4) {
        uint32_t slot = 0;
        g_j.mr_table = r4;
        if (read_guest(uc, r4 + E10A31J_MR_TABLE_SMSCFG_OFF, &slot, 4) && slot)
            apply_cfg_base(uc, slot, "live_r4_mr_table");
    }
    if (pc == 0x2E5396u && r0 == E10A31J_GPT_OFF && r2 == 3u) {
        e10a31j_mark_milestone("SMSGETBYTES_CALL_OFF_349_LEN_3", "method0");
        emit_api("smsGetBytes", 0, pc, lr, r0, r1, r2, r3, r9, "pos_349", "read");
    }
    (void)r1;
}
