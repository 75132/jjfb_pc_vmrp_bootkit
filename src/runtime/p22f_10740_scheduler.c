#include "gwy_launcher/p22f_10740_scheduler.h"

#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* Module offsets relative to refined gamelist.ext guest_code_base. */
#define OFF_10740 0x10740u
#define OFF_1074E 0x1074Eu
#define OFF_107F8 0x107F8u
#define OFF_10814 0x10814u
#define OFF_FF00 0xFF00u
#define OFF_7DB0 0x7DB0u
#define OFF_7B6C 0x7B6Cu
#define OFF_F670 0xF670u
#define OFF_392C 0x392Cu
#define OFF_8CDC 0x8CDCu
#define OFF_8CF2 0x8CF2u
#define OFF_8D0C 0x8D0Cu
#define OFF_8D20 0x8D20u
#define OFF_8D26 0x8D26u
#define OFF_D978 0xD978u
#define OFF_D984 0xD984u
#define OFF_12CF0 0x12CF0u
#define OFF_12D06 0x12D06u
#define OFF_12D0A 0x12D0Au
#define OFF_12D0E 0x12D0Eu
#define OFF_133E0 0x133E0u
#define OFF_1344A 0x1344Au

#define STATIC_CALLER_N 11
#define HIT_CAP 64
#define BRANCH_CAP 8192
#define REG_CAP 256
#define EVT_CAP 4096
#define PROV_CAP 512
#define XREF_SCAN_EXTRA 64
#define DUMP_MAX 0x40000u

static const uint32_t k_static_callers[STATIC_CALLER_N] = {
    0x4076u, 0x4526u, 0x458Au, 0x46C4u, 0x4778u, 0x4C76u,
    0x53A4u, 0x5904u, 0x5918u, 0x8D26u, 0x12D0Eu,
};

typedef struct {
    uint32_t caller_off;
    uint32_t abs_pc;
    uint8_t raw[4];
    char insn[48];
    uint32_t decoded_target;
    uint32_t target_norm;
    int runtime_bytes_match;
    char containing_function[40];
    char kind[24]; /* direct_bl / blx_reg / table / other */
    uint32_t hit_n;
} XrefRow;

typedef struct {
    uint32_t off;
    const char *name;
    uint32_t hit_n;
    uint32_t first_pc;
    uint32_t first_lr;
    uint32_t first_r0;
    uint32_t first_r1;
    uint32_t first_r2;
    uint32_t first_r3;
    uint32_t first_r9;
    uint32_t first_sp;
    uint32_t first_cpsr;
    uint32_t sp_words[16];
    int sp_words_ok;
} HitSite;

typedef struct {
    uint32_t seq;
    uint32_t branch_pc;
    uint32_t branch_off;
    uint32_t comparison_pc;
    uint32_t lhs;
    uint32_t rhs;
    uint32_t cpsr;
    int taken;
    uint32_t not_taken_target;
    uint32_t taken_target;
    int reaches_10740;
    char note[80];
} BranchRow;

typedef struct {
    uint32_t seq;
    uint32_t registration_pc;
    char registration_module[48];
    uint32_t destination_address;
    uint32_t table_slot;
    char platform_service[48];
    uint32_t callback_target;
    char callback_owner[48];
    uint32_t r9;
    uint32_t erw;
    uint32_t registration_args[4];
    uint32_t registration_return;
    int delivered;
    uint32_t delivery_seq;
    uint32_t event_code;
} RegRow;

typedef struct {
    uint32_t seq;
    uint64_t timestamp_ms;
    char producer[40];
    char event_type[32];
    uint32_t event_code;
    int callback_registered;
    uint32_t callback_target;
    uint32_t delivery_target;
    uint32_t r0, r1, r2, r3;
    uint32_t stack_args[4];
    char module_owner[48];
    char generation[32];
    uint32_t r9_before;
    uint32_t r9_after;
    char result[48];
} EvtRow;

typedef struct {
    uint32_t seq;
    char subject[48];
    char classif[40];
    char detail[160];
} ProvRow;

typedef struct {
    int known;
    int enabled;
    int finalized;
    void *uc;
    clock_t t0;

    char run_id[96];
    char source_commit[80];
    char main_exe_sha[72];
    char raw_ext_sha[72];
    char runtime_sha[72];
    char module_id_s[40];
    char p_guest_s[40];
    char generation_s[40];
    char package_owner[96];
    char identity_missing[160];

    uint32_t gl_base;
    uint32_t gl_end;
    uint32_t gl_size;
    uint32_t raw_base_refine_pad;
    uint32_t erw;
    uint32_t p_guest;
    uint64_t generation;
    uint64_t module_id;
    int identity_resolved;

    int dump_done;
    int xref_verified;

    uint32_t gl_insn_n;
    uint32_t fire_ext_n;
    uint32_t hit_10740;
    uint32_t hit_10814;
    uint32_t hit_ff00;
    uint32_t hit_7b6c;
    uint32_t hit_7db0;
    uint32_t snap_r9_3e4;
    uint32_t snap_r9_6c4;
    uint32_t snap_r9_450;
    int snap_ok;
    uint32_t entry_10740_r0;
    uint32_t entry_10740_lr;
    uint32_t entry_10740_r9;

    /* Dense slice: arm when a near-caller is hit without reaching 10740. */
    int slice_armed;
    uint32_t slice_focus_off;
    uint32_t last_cmp_pc;
    uint32_t last_cmp_lhs;
    uint32_t last_cmp_rhs;

    char stop_reason[96];
    char verdict_class[8];
    char nearest_caller[48];
    char first_block_branch[96];
    char first_block_ops[96];
    char actual_path[96];
    char target_path[96];
    char natural_producer[64];
    char missing_contract[128];

    XrefRow xrefs[STATIC_CALLER_N + XREF_SCAN_EXTRA];
    int xref_n;
    int extra_direct_n;
    int extra_indirect_n;

    HitSite hits[HIT_CAP];
    int hit_n;

    BranchRow branches[BRANCH_CAP];
    uint32_t branch_n;

    RegRow regs[REG_CAP];
    uint32_t reg_n;

    EvtRow evts[EVT_CAP];
    uint32_t evt_n;

    ProvRow provs[PROV_CAP];
    uint32_t prov_n;

    FILE *xref_csv;
    FILE *hits_csv;
    FILE *branch_csv;
    FILE *reg_csv;
    FILE *evt_csv;
    FILE *prov_csv;
    FILE *disasm_txt;
    FILE *summary_txt;
    FILE *verdict_md;

#ifdef GWY_HAVE_UNICORN
    uc_hook mem_hook;
    int mem_hook_armed;
#endif
} P22fState;

static P22fState g;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

static const char *env_or(const char *k, const char *fb) {
    const char *e = getenv(k);
    return (e && e[0]) ? e : fb;
}

static int is_gl(const char *m) {
    return m && strstr(m, "gamelist") != NULL;
}

static uint64_t now_ms(void) {
    return (uint64_t)((clock() - g.t0) * 1000 / CLOCKS_PER_SEC);
}

static void set_unknown(char *dst, size_t n, const char *why) {
    snprintf(dst, n, "UNKNOWN_NOT_EXPOSED");
    if (why && why[0] && !strstr(g.identity_missing, why)) {
        size_t L = strlen(g.identity_missing);
        snprintf(g.identity_missing + L, sizeof(g.identity_missing) - L, "%s%s",
                 L ? ";" : "", why);
    }
}

static void identity_header(FILE *f) {
    if (!f) return;
    fprintf(f,
            "# run_id=%s source_commit=%s main_exe_sha256=%s raw_gamelist_ext_sha256=%s "
            "runtime_image_sha256=%s module_id=%s runtime_base=0x%X runtime_end=0x%X "
            "runtime_size=0x%X raw_base_refine_pad=0x%X ERW=0x%X P=%s generation=%s "
            "package_owner=%s identity_gaps=%s\n"
            "# offset_map=runtime_module_offset = guest_pc - runtime_base; "
            "raw_file_offset ≈ runtime_module_offset (after RAW_BASE_REFINE pad)\n",
            g.run_id[0] ? g.run_id : "?", g.source_commit[0] ? g.source_commit : "?",
            g.main_exe_sha[0] ? g.main_exe_sha : "?", g.raw_ext_sha[0] ? g.raw_ext_sha : "?",
            g.runtime_sha[0] ? g.runtime_sha : "PENDING",
            g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED", g.gl_base, g.gl_end,
            g.gl_size, g.raw_base_refine_pad, g.erw,
            g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
            g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
            g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED",
            g.identity_missing[0] ? g.identity_missing : "none");
}

static FILE *open_out(const char *env_key, const char *fallback) {
    const char *p = env_or(env_key, fallback);
    FILE *f = fopen(p, "wb");
    return f;
}

static void ensure_files(void) {
    if (!g.xref_csv) {
        g.xref_csv = open_out("JJFB_P22F_XREF_CSV", "reports/p22f/p22f_10740_all_xrefs.csv");
        if (g.xref_csv) {
            identity_header(g.xref_csv);
            fputs("caller_offset,absolute_pc,raw_bytes,instruction,decoded_target,"
                  "target_normalized,runtime_bytes_match,containing_function,kind,hit_n\n",
                  g.xref_csv);
        }
    }
    if (!g.hits_csv) {
        g.hits_csv = open_out("JJFB_P22F_HITS_CSV", "reports/p22f/p22f_10740_caller_hits.csv");
        if (g.hits_csv) {
            identity_header(g.hits_csv);
            fputs("site_offset,name,hit_n,first_pc,first_lr,r0,r1,r2,r3,r9,sp,cpsr,"
                  "sp_w0,sp_w1,sp_w2,sp_w3,sp_w4,sp_w5,sp_w6,sp_w7\n",
                  g.hits_csv);
        }
    }
    if (!g.branch_csv) {
        g.branch_csv =
            open_out("JJFB_P22F_BRANCH_CSV", "reports/p22f/p22f_caller_branch_slices.csv");
        if (g.branch_csv) {
            identity_header(g.branch_csv);
            fputs("seq,branch_pc,branch_off,comparison_pc,lhs,rhs,cpsr,taken,"
                  "not_taken_target,taken_target,reaches_10740,note\n",
                  g.branch_csv);
        }
    }
    if (!g.reg_csv) {
        g.reg_csv =
            open_out("JJFB_P22F_REG_CSV", "reports/p22f/p22f_callback_registration.csv");
        if (g.reg_csv) {
            identity_header(g.reg_csv);
            fputs("seq,registration_pc,registration_module,destination_address,table_slot,"
                  "platform_service,callback_target,callback_owner,r9,erw,"
                  "arg0,arg1,arg2,arg3,registration_return,delivered,delivery_seq,event_code\n",
                  g.reg_csv);
        }
    }
    if (!g.evt_csv) {
        g.evt_csv =
            open_out("JJFB_P22F_EVT_CSV", "reports/p22f/p22f_event_delivery_timeline.csv");
        if (g.evt_csv) {
            identity_header(g.evt_csv);
            fputs("sequence,timestamp_ms,producer,event_type,event_code,callback_registered,"
                  "callback_target,delivery_target,r0,r1,r2,r3,stack0,stack1,stack2,stack3,"
                  "module_owner,generation,r9_before,r9_after,result\n",
                  g.evt_csv);
        }
    }
    if (!g.prov_csv) {
        g.prov_csv =
            open_out("JJFB_P22F_PROV_CSV", "reports/p22f/p22f_scheduler_provenance.csv");
        if (g.prov_csv) {
            identity_header(g.prov_csv);
            fputs("seq,subject,classification,detail\n", g.prov_csv);
        }
    }
}

static void add_prov(const char *subject, const char *classif, const char *detail) {
    ProvRow *r;
    if (g.prov_n >= PROV_CAP) return;
    ensure_files();
    r = &g.provs[g.prov_n++];
    r->seq = g.prov_n;
    snprintf(r->subject, sizeof(r->subject), "%s", subject ? subject : "?");
    snprintf(r->classif, sizeof(r->classif), "%s", classif ? classif : "?");
    snprintf(r->detail, sizeof(r->detail), "%s", detail ? detail : "");
    if (g.prov_csv) {
        fprintf(g.prov_csv, "%u,%s,%s,\"%s\"\n", r->seq, r->subject, r->classif, r->detail);
        fflush(g.prov_csv);
    }
}

static void add_evt(const char *producer, const char *etype, uint32_t code, int regd,
                    uint32_t cb, uint32_t deliv, uint32_t r0, uint32_t r1, uint32_t r2,
                    uint32_t r3, uint32_t r9b, uint32_t r9a, const char *result) {
    EvtRow *e;
    if (g.evt_n >= EVT_CAP) return;
    ensure_files();
    e = &g.evts[g.evt_n++];
    e->seq = g.evt_n;
    e->timestamp_ms = now_ms();
    snprintf(e->producer, sizeof(e->producer), "%s", producer ? producer : "?");
    snprintf(e->event_type, sizeof(e->event_type), "%s", etype ? etype : "?");
    e->event_code = code;
    e->callback_registered = regd;
    e->callback_target = cb;
    e->delivery_target = deliv;
    e->r0 = r0;
    e->r1 = r1;
    e->r2 = r2;
    e->r3 = r3;
    snprintf(e->module_owner, sizeof(e->module_owner), "%s",
             g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED");
    snprintf(e->generation, sizeof(e->generation), "%s",
             g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED");
    e->r9_before = r9b;
    e->r9_after = r9a;
    snprintf(e->result, sizeof(e->result), "%s", result ? result : "");
    if (g.evt_csv) {
        fprintf(g.evt_csv,
                "%u,%llu,%s,%s,0x%X,%d,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0,0,0,0,%s,%s,0x%X,0x%X,%s\n",
                e->seq, (unsigned long long)e->timestamp_ms, e->producer, e->event_type,
                e->event_code, e->callback_registered, e->callback_target, e->delivery_target,
                e->r0, e->r1, e->r2, e->r3, e->module_owner, e->generation, e->r9_before,
                e->r9_after, e->result);
        fflush(g.evt_csv);
    }
}

static HitSite *find_hit(uint32_t off) {
    int i;
    for (i = 0; i < g.hit_n; i++)
        if (g.hits[i].off == off) return &g.hits[i];
    return NULL;
}

static HitSite *ensure_hit(uint32_t off, const char *name) {
    HitSite *h = find_hit(off);
    if (h) return h;
    if (g.hit_n >= HIT_CAP) return NULL;
    h = &g.hits[g.hit_n++];
    memset(h, 0, sizeof(*h));
    h->off = off;
    h->name = name;
    return h;
}

static void init_hit_sites(void) {
    if (g.hit_n) return;
    ensure_hit(OFF_F670, "F670_wrapper");
    ensure_hit(OFF_392C, "dispatcher_392C");
    ensure_hit(0x4076u, "callsite_4076");
    ensure_hit(0x4526u, "callsite_4526");
    ensure_hit(0x458Au, "callsite_458A");
    ensure_hit(0x46C4u, "callsite_46C4");
    ensure_hit(0x4778u, "callsite_4778");
    ensure_hit(0x4C76u, "callsite_4C76");
    ensure_hit(0x53A4u, "callsite_53A4");
    ensure_hit(0x5904u, "callsite_5904");
    ensure_hit(0x5918u, "callsite_5918");
    ensure_hit(OFF_8CDC, "cb_8CDC");
    ensure_hit(OFF_8CF2, "cb_8CF2");
    ensure_hit(OFF_8D0C, "cb_8D0C");
    ensure_hit(OFF_8D20, "cb_8D20");
    ensure_hit(OFF_8D26, "cb_8D26");
    ensure_hit(OFF_D978, "D978");
    ensure_hit(OFF_D984, "D984");
    ensure_hit(OFF_12CF0, "fn_12CF0");
    ensure_hit(OFF_12D06, "12D06");
    ensure_hit(OFF_12D0A, "12D0A");
    ensure_hit(OFF_12D0E, "callsite_12D0E");
    ensure_hit(OFF_10740, "UI_10740");
    ensure_hit(OFF_1074E, "once_flag_1074E");
    ensure_hit(OFF_107F8, "mode_107F8");
    ensure_hit(OFF_10814, "10814");
    ensure_hit(OFF_FF00, "FF00");
    ensure_hit(OFF_7DB0, "7DB0");
    ensure_hit(OFF_7B6C, "cfg_loader_7B6C");
    ensure_hit(OFF_133E0, "tag_133E0");
    ensure_hit(OFF_1344A, "tag_1344A");
}

static int decode_thumb_bl(const uint8_t *b, uint32_t off, uint32_t *out_tgt, int *is_blx) {
    uint16_t h, h2;
    int s, imm10, j1, j2, imm11, I1, I2, imm;
    if (!b) return 0;
    h = (uint16_t)(b[0] | (b[1] << 8));
    h2 = (uint16_t)(b[2] | (b[3] << 8));
    if ((h & 0xF800) != 0xF000) return 0;
    if ((h2 & 0xD000) == 0xD000) {
        if (is_blx) *is_blx = 0;
    } else if ((h2 & 0xD000) == 0xC000) {
        if (is_blx) *is_blx = 1;
    } else {
        return 0;
    }
    s = (h >> 10) & 1;
    imm10 = h & 0x3FF;
    j1 = (h2 >> 13) & 1;
    j2 = (h2 >> 11) & 1;
    imm11 = h2 & 0x7FF;
    I1 = 1 - (j1 ^ s);
    I2 = 1 - (j2 ^ s);
    imm = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1);
    if (s) imm -= 1 << 25;
    *out_tgt = (off + 4u + (uint32_t)imm) & ~1u;
    return 1;
}

static const char *fn_for_caller(uint32_t off) {
    if (off >= 0x392Cu && off < 0x5A00u) return "dispatcher_392C";
    if (off == OFF_8D26) return "cb_8CDC";
    if (off == OFF_12D0E) return "fn_12CF0";
    return "unknown";
}

static void refresh_identity_from_registry(void) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m = NULL;
    ExtChunkOwnerInfo oi;
    memset(&oi, 0, sizeof(oi));
    reg = gwy_ext_loader_bound_registry();
    if (reg && g.gl_base)
        m = module_registry_find_by_code_addr(reg, g.gl_base);
    if (m) {
        g.module_id = m->module_id;
        snprintf(g.module_id_s, sizeof(g.module_id_s), "0x%llX",
                 (unsigned long long)m->module_id);
        if (m->data.start_of_er_rw) g.erw = m->data.start_of_er_rw;
        if (m->map.guest_code_size && !g.gl_size) {
            g.gl_size = m->map.guest_code_size;
            g.gl_end = g.gl_base + g.gl_size;
        }
        if (m->requested_name[0] || m->resolved_name[0]) {
            const char *n = m->resolved_name[0] ? m->resolved_name : m->requested_name;
            snprintf(g.package_owner, sizeof(g.package_owner), "%s", n);
        }
    } else if (!g.module_id_s[0]) {
        set_unknown(g.module_id_s, sizeof(g.module_id_s), "module_registry_find_by_code_addr");
    }

    if (g.p_guest &&
        ext_chunk_provider_owner_for_p(g.p_guest, &oi)) {
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", g.p_guest);
        if (oi.module_generation) {
            g.generation = oi.module_generation;
            snprintf(g.generation_s, sizeof(g.generation_s), "%u", oi.module_generation);
        }
        if (oi.erw) g.erw = oi.erw;
    }

    if (!g.p_guest_s[0]) {
        uint32_t last_p = ext_chunk_provider_last_p_guest();
        if (last_p) {
            g.p_guest = last_p;
            snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", last_p);
            if (ext_chunk_provider_owner_for_p(last_p, &oi)) {
                if (oi.module_generation) {
                    g.generation = oi.module_generation;
                    snprintf(g.generation_s, sizeof(g.generation_s), "%u",
                             oi.module_generation);
                }
                if (oi.erw) g.erw = oi.erw;
            }
        } else {
            set_unknown(g.p_guest_s, sizeof(g.p_guest_s), "ext_chunk_provider_last_p_guest");
        }
    }
    if (!g.generation_s[0]) {
        uint32_t depth = module_r9_switch_depth();
        if (depth == 0)
            set_unknown(g.generation_s, sizeof(g.generation_s),
                        "module_r9_switch_top_generation");
        else
            set_unknown(g.generation_s, sizeof(g.generation_s),
                        "module_r9_frame_generation_api");
    }
    if (!g.package_owner[0])
        set_unknown(g.package_owner, sizeof(g.package_owner), "module_registry_name");
    g.identity_resolved = 1;
}

static void record_reg(uint32_t pc, const char *mod, uint32_t dest, uint32_t value,
                       const char *svc) {
    RegRow *r;
    uint32_t tgt = value & ~1u;
    if (g.reg_n >= REG_CAP) return;
    ensure_files();
    r = &g.regs[g.reg_n++];
    memset(r, 0, sizeof(*r));
    r->seq = g.reg_n;
    r->registration_pc = pc;
    snprintf(r->registration_module, sizeof(r->registration_module), "%s", mod ? mod : "?");
    r->destination_address = dest;
    r->table_slot = dest;
    snprintf(r->platform_service, sizeof(r->platform_service), "%s", svc ? svc : "mem_write");
    r->callback_target = value;
    if (g.gl_base && tgt == (g.gl_base + OFF_8CDC))
        snprintf(r->callback_owner, sizeof(r->callback_owner), "gamelist+0x8CDC");
    else if (g.gl_base && tgt == (g.gl_base + OFF_F670))
        snprintf(r->callback_owner, sizeof(r->callback_owner), "gamelist+0xF670");
    else if (g.gl_base && tgt == (g.gl_base + OFF_D978))
        snprintf(r->callback_owner, sizeof(r->callback_owner), "gamelist+0xD978");
    else if (g.gl_base && tgt == (g.gl_base + OFF_392C))
        snprintf(r->callback_owner, sizeof(r->callback_owner), "gamelist+0x392C");
    else
        snprintf(r->callback_owner, sizeof(r->callback_owner), "other");
    r->r9 = 0;
    r->erw = g.erw;
    if (g.reg_csv) {
        fprintf(g.reg_csv,
                "%u,0x%X,%s,0x%X,0x%X,%s,0x%X,%s,0x%X,0x%X,0,0,0,0,0,%d,0,0\n", r->seq,
                r->registration_pc, r->registration_module, r->destination_address, r->table_slot,
                r->platform_service, r->callback_target, r->callback_owner, r->r9, r->erw,
                r->delivered);
        fflush(g.reg_csv);
    }
    {
        char d[128];
        snprintf(d, sizeof(d), "pc=0x%X dest=0x%X value=0x%X owner=%s", pc, dest, value,
                 r->callback_owner);
        add_prov(r->callback_owner, "REGISTERED", d);
    }
}

#ifdef GWY_HAVE_UNICORN
/* Optional targeted write observer kept for future ERW-scoped hooks. */
static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                         int64_t value, void *user) {
    uint32_t v, tgt, pc = 0;
    (void)type;
    (void)user;
    if (!g.enabled || !g.gl_base || size < 4) return;
    v = (uint32_t)value;
    tgt = v & ~1u;
    if (tgt != (g.gl_base + OFF_8CDC) && tgt != (g.gl_base + OFF_F670) &&
        tgt != (g.gl_base + OFF_D978) && tgt != (g.gl_base + OFF_392C) &&
        tgt != (g.gl_base + OFF_12CF0))
        return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    record_reg(pc, "guest_mem", (uint32_t)address, v, "UC_MEM_WRITE_callback_ptr");
}
#endif

static void arm_mem_hook(void) {
    /* No global UC_HOOK_MEM_WRITE — full-range hooks distort timing (P21 lesson).
     * Registration inferred via platform args + entry hits; p22f_note_mem_write OK. */
#ifdef GWY_HAVE_UNICORN
    (void)on_mem_write;
    (void)g.mem_hook;
    (void)g.mem_hook_armed;
#endif
}

static void verify_xrefs_and_dump(void *uc) {
    uint8_t *buf = NULL;
    size_t n;
    int i;
    uint32_t off;
    if (g.dump_done || !g.gl_base || !g.gl_size) return;
#ifdef GWY_HAVE_UNICORN
    if (!uc) return;
    n = g.gl_size;
    if (n > DUMP_MAX) n = DUMP_MAX;
    buf = (uint8_t *)malloc(n);
    if (!buf) return;
    if (!guest_memory_uc_peek((struct uc_struct *)uc, g.gl_base, buf, (uint32_t)n)) {
        free(buf);
        return;
    }
    {
        uint8_t digest[32];
        size_t j;
        gwy_sha256(buf, n, digest);
        for (j = 0; j < 32; j++) sprintf(g.runtime_sha + j * 2, "%02x", digest[j]);
        g.runtime_sha[64] = 0;
    }

    g.xref_n = 0;
    for (i = 0; i < STATIC_CALLER_N; i++) {
        XrefRow *x;
        uint32_t tgt = 0;
        int is_blx = 0;
        off = k_static_callers[i];
        if (off + 4 > n) continue;
        x = &g.xrefs[g.xref_n++];
        memset(x, 0, sizeof(*x));
        x->caller_off = off;
        x->abs_pc = g.gl_base + off;
        memcpy(x->raw, buf + off, 4);
        if (decode_thumb_bl(buf + off, off, &tgt, &is_blx)) {
            x->decoded_target = tgt;
            x->target_norm = tgt & ~1u;
            x->runtime_bytes_match = (x->target_norm == OFF_10740) ? 1 : 0;
            snprintf(x->insn, sizeof(x->insn), "%s #0x%X", is_blx ? "blx" : "bl", tgt);
            snprintf(x->kind, sizeof(x->kind), "%s", is_blx ? "direct_blx" : "direct_bl");
        } else {
            snprintf(x->insn, sizeof(x->insn), "NOT_BL raw=%02x%02x%02x%02x", x->raw[0],
                     x->raw[1], x->raw[2], x->raw[3]);
            snprintf(x->kind, sizeof(x->kind), "mismatch");
            x->runtime_bytes_match = 0;
        }
        snprintf(x->containing_function, sizeof(x->containing_function), "%s",
                 fn_for_caller(off));
    }

    /* Full-range scan for additional direct BL to +0x10740 and BLX imm. */
    for (off = 0; off + 4 < n; off += 2) {
        uint32_t tgt = 0;
        int is_blx = 0;
        int known = 0;
        if (!decode_thumb_bl(buf + off, off, &tgt, &is_blx)) continue;
        if ((tgt & ~1u) != OFF_10740) continue;
        for (i = 0; i < g.xref_n; i++)
            if (g.xrefs[i].caller_off == off) {
                known = 1;
                break;
            }
        if (known) continue;
        if (g.xref_n >= (int)(sizeof(g.xrefs) / sizeof(g.xrefs[0]))) break;
        {
            XrefRow *x = &g.xrefs[g.xref_n++];
            memset(x, 0, sizeof(*x));
            x->caller_off = off;
            x->abs_pc = g.gl_base + off;
            memcpy(x->raw, buf + off, 4);
            x->decoded_target = tgt;
            x->target_norm = tgt & ~1u;
            x->runtime_bytes_match = 1;
            snprintf(x->insn, sizeof(x->insn), "%s #0x%X", is_blx ? "blx" : "bl", tgt);
            snprintf(x->kind, sizeof(x->kind), "extra_direct");
            snprintf(x->containing_function, sizeof(x->containing_function), "scan");
            g.extra_direct_n++;
        }
    }

    /* Scan for pointer literals to (base+0x10740)|1 or thumb entry. */
    {
        uint32_t want1 = (g.gl_base + OFF_10740) | 1u;
        uint32_t want0 = g.gl_base + OFF_10740;
        for (off = 0; off + 4 <= n; off += 4) {
            uint32_t w = (uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8) |
                         ((uint32_t)buf[off + 2] << 16) | ((uint32_t)buf[off + 3] << 24);
            if (w != want1 && w != want0) continue;
            if (g.xref_n >= (int)(sizeof(g.xrefs) / sizeof(g.xrefs[0]))) break;
            {
                XrefRow *x = &g.xrefs[g.xref_n++];
                memset(x, 0, sizeof(*x));
                x->caller_off = off;
                x->abs_pc = g.gl_base + off;
                memcpy(x->raw, buf + off, 4);
                x->decoded_target = OFF_10740;
                x->target_norm = OFF_10740;
                x->runtime_bytes_match = 1;
                snprintf(x->insn, sizeof(x->insn), ".word 0x%X", w);
                snprintf(x->kind, sizeof(x->kind), "literal_ptr");
                snprintf(x->containing_function, sizeof(x->containing_function), "data");
                g.extra_indirect_n++;
            }
        }
    }

    ensure_files();
    if (g.xref_csv) {
        for (i = 0; i < g.xref_n; i++) {
            XrefRow *x = &g.xrefs[i];
            fprintf(g.xref_csv,
                    "0x%X,0x%X,%02x%02x%02x%02x,%s,0x%X,0x%X,%d,%s,%s,%u\n", x->caller_off,
                    x->abs_pc, x->raw[0], x->raw[1], x->raw[2], x->raw[3], x->insn,
                    x->decoded_target, x->target_norm, x->runtime_bytes_match,
                    x->containing_function, x->kind, x->hit_n);
        }
        fflush(g.xref_csv);
    }

    g.disasm_txt = open_out("JJFB_P22F_DISASM", "reports/p22f/p22f_10740_runtime_disasm.txt");
    if (g.disasm_txt) {
        identity_header(g.disasm_txt);
        fprintf(g.disasm_txt,
                "# P22F runtime verification of +0x10740 callers\n"
                "# runtime_base=0x%X size=0x%X sha=%s extra_direct=%d literal_ptrs=%d\n\n",
                g.gl_base, g.gl_size, g.runtime_sha, g.extra_direct_n, g.extra_indirect_n);
        for (i = 0; i < g.xref_n; i++) {
            XrefRow *x = &g.xrefs[i];
            fprintf(g.disasm_txt,
                    "caller=+0x%X abs=0x%X bytes=%02x%02x%02x%02x insn=%s tgt=0x%X "
                    "norm=0x%X match=%d fn=%s kind=%s\n",
                    x->caller_off, x->abs_pc, x->raw[0], x->raw[1], x->raw[2], x->raw[3],
                    x->insn, x->decoded_target, x->target_norm, x->runtime_bytes_match,
                    x->containing_function, x->kind);
        }
        /* Dump key function prologues. */
        {
            static const uint32_t dens[] = {OFF_F670, OFF_392C, OFF_8CDC, OFF_D978, OFF_12CF0,
                                            OFF_10740};
            size_t di;
            fprintf(g.disasm_txt, "\n# Key entry windows (raw halfwords)\n");
            for (di = 0; di < sizeof(dens) / sizeof(dens[0]); di++) {
                uint32_t o, end;
                o = dens[di];
                end = o + 0x40u;
                if (end > n) end = (uint32_t)n;
                fprintf(g.disasm_txt, "\n## +0x%X\n", o);
                for (; o + 1 < end; o += 2) {
                    uint32_t tgt = 0;
                    int is_blx = 0;
                    if (o + 3 < end && decode_thumb_bl(buf + o, o, &tgt, &is_blx)) {
                        fprintf(g.disasm_txt, "0x%04X: %02x%02x%02x%02x  %s #0x%X\n", o,
                                buf[o], buf[o + 1], buf[o + 2], buf[o + 3],
                                is_blx ? "blx" : "bl", tgt);
                        o += 2;
                    } else {
                        fprintf(g.disasm_txt, "0x%04X: %02x%02x\n", o, buf[o], buf[o + 1]);
                    }
                }
            }
        }
        fflush(g.disasm_txt);
    }

    {
        char d[128];
        int matched = 0;
        for (i = 0; i < STATIC_CALLER_N && i < g.xref_n; i++)
            if (g.xrefs[i].runtime_bytes_match) matched++;
        snprintf(d, sizeof(d), "static11_match=%d/%d extra_direct=%d literals=%d", matched,
                 STATIC_CALLER_N, g.extra_direct_n, g.extra_indirect_n);
        add_prov("+0x10740_xrefs", matched == STATIC_CALLER_N ? "VERIFIED" : "PARTIAL", d);
    }

    free(buf);
    g.dump_done = 1;
    g.xref_verified = 1;
    printf("[JJFB_P22F] dump=1 base=0x%X end=0x%X size=0x%X sha=%s xrefs=%d evidence=OBSERVED\n",
           g.gl_base, g.gl_end, g.gl_size, g.runtime_sha, g.xref_n);
    fflush(stdout);
#else
    (void)uc;
#endif
}

static void snap_r9_fields(void *uc, uint32_t r9) {
#ifdef GWY_HAVE_UNICORN
    uint32_t v;
    if (!uc || !r9) return;
    if (guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0x3E4u, &v, 4)) g.snap_r9_3e4 = v;
    if (guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0x6C4u, &v, 4)) g.snap_r9_6c4 = v;
    if (guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0x450u, &v, 4)) g.snap_r9_450 = v;
    /* D978 uses *( *(R9+0x3E4) + 0x6C ) — also try ERW-relative if r9 is ERW. */
    g.snap_ok = 1;
#else
    (void)uc;
    (void)r9;
#endif
}

static void note_hit(void *uc, uint32_t off, const uint32_t regs[16], uint32_t lr, uint32_t sp,
                     uint32_t cpsr) {
    HitSite *h = find_hit(off);
    int i;
    if (!h) return;
    h->hit_n++;
    if (h->hit_n == 1) {
        h->first_pc = g.gl_base + off;
        h->first_lr = lr;
        h->first_r0 = regs[0];
        h->first_r1 = regs[1];
        h->first_r2 = regs[2];
        h->first_r3 = regs[3];
        h->first_r9 = regs[9];
        h->first_sp = sp;
        h->first_cpsr = cpsr;
#ifdef GWY_HAVE_UNICORN
        if (uc && sp) {
            for (i = 0; i < 16; i++) {
                uint32_t w = 0;
                if (guest_memory_uc_peek((struct uc_struct *)uc, sp + (uint32_t)i * 4u, &w, 4))
                    h->sp_words[i] = w;
            }
            h->sp_words_ok = 1;
        }
#else
        (void)uc;
        (void)i;
#endif
        if (off == OFF_392C || off == OFF_F670 || off == OFF_8CDC || off == OFF_D978 ||
            off == OFF_12CF0) {
            char d[160];
            snprintf(d, sizeof(d),
                     "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X lr=0x%X sp=0x%X "
                     "sp0=0x%X sp1=0x%X sp4=0x%X",
                     regs[0], regs[1], regs[2], regs[3], regs[9], lr, sp, h->sp_words[0],
                     h->sp_words[1], h->sp_words[4]);
            add_prov(h->name, "ENTERED", d);
            add_evt("guest_code", "fn_enter", off, 0, g.gl_base + off, g.gl_base + off, regs[0],
                    regs[1], regs[2], regs[3], regs[9], regs[9], "entered");
        }
    }

    /* Mark xref hits for callsites. */
    for (i = 0; i < g.xref_n; i++) {
        if (g.xrefs[i].caller_off == off) g.xrefs[i].hit_n++;
    }

    if (off == OFF_10740 && h->hit_n == 1) {
        g.hit_10740++;
        g.entry_10740_r0 = regs[0];
        g.entry_10740_lr = lr;
        g.entry_10740_r9 = regs[9];
        snap_r9_fields(uc, regs[9]);
        add_prov("UI_10740", "ENTERED", "natural entry");
    }
    if (off == OFF_10814) g.hit_10814++;
    if (off == OFF_FF00) g.hit_ff00++;
    if (off == OFF_7B6C) g.hit_7b6c++;
    if (off == OFF_7DB0) g.hit_7db0++;

    /* Arm dense branch slice when A/B/C entered but 10740 not yet. */
    if (!g.hit_10740 && !g.slice_armed) {
        if (off == OFF_392C || off == OFF_8CDC || off == OFF_D978 || off == OFF_12CF0 ||
            off == OFF_F670) {
            g.slice_armed = 1;
            g.slice_focus_off = off;
        }
    }
}

static int cond_pass(uint32_t cpsr, unsigned cond) {
    unsigned N = (cpsr >> 31) & 1;
    unsigned Z = (cpsr >> 30) & 1;
    unsigned C = (cpsr >> 29) & 1;
    unsigned V = (cpsr >> 28) & 1;
    switch (cond) {
    case 0:
        return Z;
    case 1:
        return !Z;
    case 2:
        return C;
    case 3:
        return !C;
    case 4:
        return N;
    case 5:
        return !N;
    case 6:
        return V;
    case 7:
        return !V;
    case 8:
        return C && !Z;
    case 9:
        return !C || Z;
    case 10:
        return N == V;
    case 11:
        return N != V;
    case 12:
        return !Z && N == V;
    case 13:
        return Z || N != V;
    default:
        return 1;
    }
}

static void maybe_slice_branch(void *uc, uint32_t off, const uint32_t regs[16], uint32_t cpsr) {
    uint8_t b[4];
    uint16_t h;
    BranchRow *br;
    int taken;
    uint32_t tgt, fall;
    (void)uc;
    if (!g.slice_armed || g.branch_n >= BRANCH_CAP || !g.gl_base) return;
#ifdef GWY_HAVE_UNICORN
    if (!g.uc) return;
    if (!guest_memory_uc_peek((struct uc_struct *)g.uc, g.gl_base + off, b, 4)) return;
#else
    return;
#endif
    h = (uint16_t)(b[0] | (b[1] << 8));

    /* cmp rn, #imm */
    if ((h & 0xF800) == 0x2800) {
        unsigned rn = (h >> 8) & 7;
        g.last_cmp_pc = g.gl_base + off;
        g.last_cmp_lhs = regs[rn];
        g.last_cmp_rhs = h & 0xFF;
        return;
    }
    /* cmp rn, rm */
    if ((h & 0xFFC0) == 0x4280) {
        unsigned rn = h & 7;
        unsigned rm = (h >> 3) & 7;
        g.last_cmp_pc = g.gl_base + off;
        g.last_cmp_lhs = regs[rn];
        g.last_cmp_rhs = regs[rm];
        return;
    }
    /* b.cond */
    if ((h & 0xF000) == 0xD000 && ((h >> 8) & 0xF) != 0xF) {
        unsigned cond = (h >> 8) & 0xF;
        int imm8 = (int)(int8_t)(h & 0xFF);
        tgt = (off + 4u + (uint32_t)(imm8 << 1)) & ~1u;
        fall = (off + 2u) & ~1u;
        taken = cond_pass(cpsr, cond);
        ensure_files();
        br = &g.branches[g.branch_n++];
        memset(br, 0, sizeof(*br));
        br->seq = g.branch_n;
        br->branch_pc = g.gl_base + off;
        br->branch_off = off;
        br->comparison_pc = g.last_cmp_pc;
        br->lhs = g.last_cmp_lhs;
        br->rhs = g.last_cmp_rhs;
        br->cpsr = cpsr;
        br->taken = taken;
        br->taken_target = taken ? tgt : fall;
        br->not_taken_target = taken ? fall : tgt;
        br->reaches_10740 = 0;
        /* Heuristic: targets that land on known callsites. */
        {
            uint32_t nt = br->not_taken_target;
            uint32_t tt = br->taken_target;
            int k;
            for (k = 0; k < STATIC_CALLER_N; k++) {
                if (nt == k_static_callers[k] || tt == k_static_callers[k] ||
                    nt == OFF_10740 || tt == OFF_10740)
                    br->reaches_10740 = 1;
            }
        }
        snprintf(br->note, sizeof(br->note), "focus=+0x%X cond=%u", g.slice_focus_off, cond);
        if (g.branch_csv) {
            fprintf(g.branch_csv,
                    "%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,0x%X,0x%X,%d,\"%s\"\n", br->seq,
                    br->branch_pc, br->branch_off, br->comparison_pc, br->lhs, br->rhs, br->cpsr,
                    br->taken, br->not_taken_target, br->taken_target, br->reaches_10740,
                    br->note);
            fflush(g.branch_csv);
        }
        if (br->reaches_10740 && !g.first_block_branch[0] && !taken) {
            snprintf(g.first_block_branch, sizeof(g.first_block_branch),
                     "+0x%X b.cond taken=%d tgt=0x%X fall=0x%X", off, taken, tgt, fall);
            snprintf(g.first_block_ops, sizeof(g.first_block_ops), "lhs=0x%X rhs=0x%X cmp_pc=0x%X",
                     br->lhs, br->rhs, br->comparison_pc);
        }
    }
}

static void flush_hits_csv(void) {
    int i;
    ensure_files();
    if (!g.hits_csv) return;
    /* rewrite */
    fclose(g.hits_csv);
    g.hits_csv = open_out("JJFB_P22F_HITS_CSV", "reports/p22f/p22f_10740_caller_hits.csv");
    if (!g.hits_csv) return;
    identity_header(g.hits_csv);
    fputs("site_offset,name,hit_n,first_pc,first_lr,r0,r1,r2,r3,r9,sp,cpsr,"
          "sp_w0,sp_w1,sp_w2,sp_w3,sp_w4,sp_w5,sp_w6,sp_w7\n",
          g.hits_csv);
    for (i = 0; i < g.hit_n; i++) {
        HitSite *h = &g.hits[i];
        fprintf(g.hits_csv,
                "0x%X,%s,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n",
                h->off, h->name ? h->name : "?", h->hit_n, h->first_pc, h->first_lr, h->first_r0,
                h->first_r1, h->first_r2, h->first_r3, h->first_r9, h->first_sp, h->first_cpsr,
                h->sp_words[0], h->sp_words[1], h->sp_words[2], h->sp_words[3], h->sp_words[4],
                h->sp_words[5], h->sp_words[6], h->sp_words[7]);
    }
    fflush(g.hits_csv);
}

static void classify_and_verdict(void) {
    HitSite *h392c = find_hit(OFF_392C);
    HitSite *hf670 = find_hit(OFF_F670);
    HitSite *h8cdc = find_hit(OFF_8CDC);
    HitSite *hd978 = find_hit(OFF_D978);
    HitSite *h12 = find_hit(OFF_12CF0);
    HitSite *h8d26 = find_hit(OFF_8D26);
    HitSite *h12d0e = find_hit(OFF_12D0E);
    int reg_8cdc = 0;
    uint32_t i;

    for (i = 0; i < g.reg_n; i++) {
        if (strstr(g.regs[i].callback_owner, "8CDC")) reg_8cdc = 1;
    }

    if (g.hit_10740) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "F");
        snprintf(g.nearest_caller, sizeof(g.nearest_caller), "+0x10740 ENTERED");
        snprintf(g.natural_producer, sizeof(g.natural_producer), "Guest (entered)");
        snprintf(g.missing_contract, sizeof(g.missing_contract),
                 "internal once-flag/mode — see R9+0x3E4 / R9+0x6C4");
        snprintf(g.actual_path, sizeof(g.actual_path), "entered +0x10740");
        snprintf(g.target_path, sizeof(g.target_path), "+0x10814/+0xFF00/+0x7B6C");
        return;
    }

    /* Prefer entered paths. */
    if (h392c && h392c->hit_n) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "A");
        snprintf(g.nearest_caller, sizeof(g.nearest_caller), "dispatcher +0x392C entered");
        snprintf(g.natural_producer, sizeof(g.natural_producer), "callback/UI dispatcher");
        snprintf(g.actual_path, sizeof(g.actual_path), "+0xF670? -> +0x392C (no BL 10740)");
        snprintf(g.target_path, sizeof(g.target_path), "+0x392C case -> BL +0x10740");
        if (g.first_block_branch[0]) {
            snprintf(g.missing_contract, sizeof(g.missing_contract),
                     "dispatcher branch skipped 10740: %s ops %s", g.first_block_branch,
                     g.first_block_ops);
        } else {
            snprintf(g.missing_contract, sizeof(g.missing_contract),
                     "dispatcher entered but no case reached a 10740 callsite "
                     "(opcode/event may not select cfg-loader case)");
        }
        return;
    }

    if (h8cdc && h8cdc->hit_n) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "A");
        snprintf(g.nearest_caller, sizeof(g.nearest_caller), "+0x8CDC entered");
        if (h8d26 && h8d26->hit_n == 0) {
            snprintf(g.first_block_branch, sizeof(g.first_block_branch),
                     "+0x8CDC body returned before +0x8D26");
            snprintf(g.missing_contract, sizeof(g.missing_contract),
                     "8CDC entered but BL +0x8D26 not reached");
        }
        return;
    }

    if (hd978 && hd978->hit_n) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "A");
        snprintf(g.nearest_caller, sizeof(g.nearest_caller), "+0xD978 entered");
        snprintf(g.actual_path, sizeof(g.actual_path), "+0xD978 path");
        snprintf(g.target_path, sizeof(g.target_path), "+0x12CF0 -> +0x12D0E BL 10740");
        snprintf(g.missing_contract, sizeof(g.missing_contract),
                 "D978 entered; check [R9+0x450]/0x3E4+0x6C] and 0x10204 return");
        return;
    }

    if (h12 && h12->hit_n) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "A");
        snprintf(g.nearest_caller, sizeof(g.nearest_caller), "+0x12CF0 entered");
        if (!h12d0e || !h12d0e->hit_n) {
            snprintf(g.missing_contract, sizeof(g.missing_contract),
                     "12CF0 entered but +0x12D0E not reached (0x10204/cmp gate)");
        }
        return;
    }

    if (reg_8cdc) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "B");
        snprintf(g.nearest_caller, sizeof(g.nearest_caller), "+0x8CDC REGISTERED_NOT_DELIVERED");
        snprintf(g.natural_producer, sizeof(g.natural_producer), "platform event/timer");
        snprintf(g.missing_contract, sizeof(g.missing_contract),
                 "callback +0x8CDC registered via mem write but never entered — platform "
                 "delivery contract missing");
        snprintf(g.actual_path, sizeof(g.actual_path), "registered, not delivered");
        snprintf(g.target_path, sizeof(g.target_path), "deliver -> +0x8CDC -> +0x8D26");
        return;
    }

    if (hf670 && hf670->hit_n == 0 && (!h392c || !h392c->hit_n) && (!h8cdc || !h8cdc->hit_n) &&
        (!hd978 || !hd978->hit_n)) {
        /* Check if F670 was registered */
        int reg_f670 = 0;
        for (i = 0; i < g.reg_n; i++)
            if (strstr(g.regs[i].callback_owner, "F670")) reg_f670 = 1;
        if (reg_f670) {
            snprintf(g.verdict_class, sizeof(g.verdict_class), "B");
            snprintf(g.nearest_caller, sizeof(g.nearest_caller), "+0xF670 REGISTERED_NOT_DELIVERED");
            snprintf(g.natural_producer, sizeof(g.natural_producer), "platform event/timer");
            snprintf(g.missing_contract, sizeof(g.missing_contract),
                     "+0xF670 wrapper registered but never entered");
        } else {
            snprintf(g.verdict_class, sizeof(g.verdict_class), "C");
            snprintf(g.nearest_caller, sizeof(g.nearest_caller), "NONE of A/B/C entered");
            snprintf(g.natural_producer, sizeof(g.natural_producer), "unknown");
            snprintf(g.missing_contract, sizeof(g.missing_contract),
                     "callback itself never registered — registration-producing Guest init "
                     "not observed; +0xF670/+0x8CDC/+0xD978 never entered");
            snprintf(g.actual_path, sizeof(g.actual_path), "NEVER_REGISTERED / never entered");
            snprintf(g.target_path, sizeof(g.target_path),
                     "register+deliver F670/8CDC/D978 producer");
        }
        return;
    }

    snprintf(g.verdict_class, sizeof(g.verdict_class), "G");
    snprintf(g.missing_contract, sizeof(g.missing_contract), "incomplete classification");
}

static void write_verdict_md(void) {
    HitSite *h;
    int i;
    g.verdict_md = open_out("JJFB_P22F_VERDICT", "reports/p22f/p22f_10740_scheduler_verdict.md");
    if (!g.verdict_md) return;
    fprintf(g.verdict_md,
            "# P22F-CLEAN +0x10740 scheduler provenance verdict\n\n"
            "## Bottom line\n\n"
            "**Class: %s**\n\n"
            "%s\n\n"
            "## Identity\n\n"
            "```\n"
            "source commit: %s\n"
            "main.exe SHA: %s\n"
            "raw gamelist.ext SHA: %s\n"
            "runtime image SHA: %s\n"
            "runtime base/end: 0x%X / 0x%X\n"
            "runtime size: 0x%X\n"
            "raw_base_refine_pad: 0x%X\n"
            "module id: %s\n"
            "ERW: 0x%X\n"
            "P: %s\n"
            "generation: %s\n"
            "package owner: %s\n"
            "identity_gaps: %s\n"
            "```\n\n"
            "## Runtime callers\n\n"
            "```\n"
            "xref_n=%d static11_verified=%s extra_direct=%d literal_ptrs=%d\n"
            "```\n\n",
            g.verdict_class, g.missing_contract, g.source_commit, g.main_exe_sha, g.raw_ext_sha,
            g.runtime_sha[0] ? g.runtime_sha : "UNKNOWN_NOT_EXPOSED", g.gl_base, g.gl_end,
            g.gl_size, g.raw_base_refine_pad,
            g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED", g.erw,
            g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
            g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
            g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED",
            g.identity_missing[0] ? g.identity_missing : "none", g.xref_n,
            g.xref_verified ? "yes" : "no", g.extra_direct_n, g.extra_indirect_n);

    fprintf(g.verdict_md, "### A group (+0x392C)\n\n```\n");
    h = find_hit(OFF_F670);
    fprintf(g.verdict_md, "+0xF670 hit=%u lr=0x%X\n", h ? h->hit_n : 0, h ? h->first_lr : 0);
    h = find_hit(OFF_392C);
    fprintf(g.verdict_md, "+0x392C hit=%u r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X\n",
            h ? h->hit_n : 0, h ? h->first_r0 : 0, h ? h->first_r1 : 0, h ? h->first_r2 : 0,
            h ? h->first_r3 : 0, h ? h->first_r9 : 0);
    for (i = 0; i < 9; i++) {
        static const uint32_t cs[] = {0x4076, 0x4526, 0x458A, 0x46C4, 0x4778,
                                      0x4C76, 0x53A4, 0x5904, 0x5918};
        h = find_hit(cs[i]);
        fprintf(g.verdict_md, "  callsite +0x%X hit=%u\n", cs[i], h ? h->hit_n : 0);
    }
    fprintf(g.verdict_md, "```\n\n### B group (+0x8CDC)\n\n```\n");
    h = find_hit(OFF_8CDC);
    fprintf(g.verdict_md, "+0x8CDC hit=%u\n", h ? h->hit_n : 0);
    h = find_hit(OFF_8D26);
    fprintf(g.verdict_md, "+0x8D26 hit=%u\n", h ? h->hit_n : 0);
    fprintf(g.verdict_md, "registrations=%u\n```\n\n### C group (+0xD978)\n\n```\n", g.reg_n);
    h = find_hit(OFF_D978);
    fprintf(g.verdict_md, "+0xD978 hit=%u\n", h ? h->hit_n : 0);
    h = find_hit(OFF_12CF0);
    fprintf(g.verdict_md, "+0x12CF0 hit=%u\n", h ? h->hit_n : 0);
    h = find_hit(OFF_12D0E);
    fprintf(g.verdict_md, "+0x12D0E hit=%u\n", h ? h->hit_n : 0);
    fprintf(g.verdict_md, "[R9+0x450] snap=0x%X (prep only if 10740 not entered)\n",
            g.snap_r9_450);
    fprintf(g.verdict_md,
            "```\n\n## PASS answers\n\n```\n"
            "nearest +0x10740 caller path: %s\n"
            "first blocking branch: %s\n"
            "comparison operands: %s\n"
            "actual path: %s\n"
            "target path: %s\n"
            "natural producer: %s\n"
            "+0x10740 natural enter: %u\n"
            "[R9+0x3E4]: 0x%X\n"
            "[R9+0x6C4]: 0x%X\n"
            "+0x10814: %u\n"
            "+0xFF00: %u\n"
            "+0x7B6C: %u\n"
            "Guest state written: NO\n"
            "events injected: NO\n"
            "headless: NO\n"
            "current sole lock: %s\n"
            "next minimal fix: restore natural producer / platform delivery contract "
            "(observe-only this round)\n"
            "stop_reason: %s\n"
            "fire_ext_n: %u\n"
            "gl_insn_n: %u\n"
            "```\n",
            g.nearest_caller, g.first_block_branch[0] ? g.first_block_branch : "n/a",
            g.first_block_ops[0] ? g.first_block_ops : "n/a", g.actual_path, g.target_path,
            g.natural_producer, g.hit_10740, g.snap_r9_3e4, g.snap_r9_6c4, g.hit_10814,
            g.hit_ff00, g.hit_7b6c, g.missing_contract, g.stop_reason, g.fire_ext_n,
            g.gl_insn_n);
    fflush(g.verdict_md);
}

/* ---------- public API ---------- */

int p22f_enabled(void) {
    if (!g.known) {
        g.known = 1;
        g.enabled = env1("JJFB_P22F_CLEAN");
        if (g.enabled) {
            g.t0 = clock();
            snprintf(g.run_id, sizeof(g.run_id), "%s", env_or("JJFB_P22F_RUN_ID", "p22f"));
            snprintf(g.source_commit, sizeof(g.source_commit), "%s",
                     env_or("JJFB_P22F_SOURCE_COMMIT", "UNKNOWN_NOT_EXPOSED"));
            snprintf(g.main_exe_sha, sizeof(g.main_exe_sha), "%s",
                     env_or("JJFB_P22F_MAIN_SHA", "UNKNOWN_NOT_EXPOSED"));
            snprintf(g.raw_ext_sha, sizeof(g.raw_ext_sha), "%s",
                     env_or("JJFB_P22F_RAW_EXT_SHA", "UNKNOWN_NOT_EXPOSED"));
            if (!strcmp(g.source_commit, "UNKNOWN_NOT_EXPOSED"))
                set_unknown(g.source_commit, sizeof(g.source_commit), "JJFB_P22F_SOURCE_COMMIT");
            init_hit_sites();
            printf("[JJFB_P22F] armed run_id=%s evidence=OBSERVED\n", g.run_id);
            fflush(stdout);
        }
    }
    return g.enabled;
}

void p22f_reset(void) {
    FILE *xref_csv = g.xref_csv, *hits = g.hits_csv, *br = g.branch_csv, *reg = g.reg_csv,
         *evt = g.evt_csv, *prov = g.prov_csv, *dis = g.disasm_txt, *sum = g.summary_txt,
         *ver = g.verdict_md;
    void *uc = g.uc;
#ifdef GWY_HAVE_UNICORN
    int hooked = g.mem_hook_armed;
    uc_hook hk = g.mem_hook;
#endif
    memset(&g, 0, sizeof(g));
    g.xref_csv = xref_csv;
    g.hits_csv = hits;
    g.branch_csv = br;
    g.reg_csv = reg;
    g.evt_csv = evt;
    g.prov_csv = prov;
    g.disasm_txt = dis;
    g.summary_txt = sum;
    g.verdict_md = ver;
    g.uc = uc;
#ifdef GWY_HAVE_UNICORN
    g.mem_hook_armed = hooked;
    g.mem_hook = hk;
#endif
}

void p22f_bind_uc(void *uc) {
    if (!p22f_enabled()) return;
    g.uc = uc;
    arm_mem_hook();
}

void p22f_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, uint64_t module_id,
                          const char *package_owner) {
    uint32_t aligned;
    uint32_t prev_base;
    int base_changed = 0;
    if (!p22f_enabled()) return;
    if (!is_gl(module_name)) return;
    aligned = base & ~1u;
    prev_base = g.gl_base;
    /* Prefer refined raw base when it replaces a previous aligned-only map. */
    if (!g.gl_base || base < g.gl_base || (g.gl_base && base != g.gl_base && size > 0)) {
        if (g.gl_base && base != g.gl_base && (g.gl_base & ~0xFFu) == (base & ~0xFFu)) {
            /* refine pad: |aligned - refined| */
            g.raw_base_refine_pad =
                (g.gl_base > base) ? (g.gl_base - base) : (base - g.gl_base);
        }
        g.gl_base = base ? base : aligned;
        g.gl_size = size;
        g.gl_end = g.gl_base + g.gl_size;
        if (prev_base && prev_base != g.gl_base) base_changed = 1;
    } else if (size && size != g.gl_size) {
        g.gl_size = size;
        g.gl_end = g.gl_base + g.gl_size;
        base_changed = 1;
    }
    if (erw) g.erw = erw;
    if (p_guest) {
        g.p_guest = p_guest;
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", p_guest);
    }
    if (generation) {
        g.generation = generation;
        snprintf(g.generation_s, sizeof(g.generation_s), "%llu",
                 (unsigned long long)generation);
    }
    if (module_id) {
        g.module_id = module_id;
        snprintf(g.module_id_s, sizeof(g.module_id_s), "0x%llX",
                 (unsigned long long)module_id);
    }
    if (package_owner && package_owner[0])
        snprintf(g.package_owner, sizeof(g.package_owner), "%s", package_owner);
    refresh_identity_from_registry();
    printf("[JJFB_P22F] module_map base=0x%X size=0x%X erw=0x%X P=%s gen=%s mid=%s "
           "pad=0x%X evidence=OBSERVED\n",
           g.gl_base, g.gl_size, g.erw, g.p_guest_s, g.generation_s, g.module_id_s,
           g.raw_base_refine_pad);
    fflush(stdout);
    /* RAW_BASE_REFINE must invalidate early dump taken at cacheSync-aligned base. */
    if (base_changed) {
        g.dump_done = 0;
        g.xref_verified = 0;
        g.xref_n = 0;
        g.extra_direct_n = 0;
        g.extra_indirect_n = 0;
        if (g.xref_csv) {
            fclose(g.xref_csv);
            g.xref_csv = NULL;
        }
        if (g.disasm_txt) {
            fclose(g.disasm_txt);
            g.disasm_txt = NULL;
        }
        printf("[JJFB_P22F] dump_invalidate prev_base=0x%X new_base=0x%X pad=0x%X "
               "evidence=OBSERVED\n",
               prev_base, g.gl_base, g.raw_base_refine_pad);
        fflush(stdout);
    }
    verify_xrefs_and_dump(g.uc);
}

void p22f_note_gamelist_started(void) {
    if (!p22f_enabled()) return;
    add_evt("shell", "GAMELIST_STARTED", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "started");
    printf("[JJFB_P22F] gamelist_started evidence=OBSERVED\n");
    fflush(stdout);
}

void p22f_note_timer_fire(uint32_t helper, uint32_t p_guest, uint32_t erw, int end) {
    if (!p22f_enabled()) return;
    if (p_guest && !g.p_guest) {
        g.p_guest = p_guest;
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", p_guest);
    }
    if (erw) g.erw = erw;
    if (end) {
        g.fire_ext_n++;
        add_evt("platform_timer", "FIRE_EXT", helper, 0, 0, 0, helper, p_guest, erw, 0, 0, 0,
                "delivered");
        refresh_identity_from_registry();
        /* Match WAIT_FIRE_N (P22F cap 16); also finalize early enough if pump ends sooner. */
        {
            uint32_t need = 12u;
            const char *wn = getenv("JJFB_E10A31_WAIT_FIRE_N");
            if (wn && wn[0]) {
                long v = strtol(wn, NULL, 10);
                if (v >= 1 && v <= 16) need = (uint32_t)v;
            }
            if (g.fire_ext_n >= need) {
                if (!g.stop_reason[0])
                    snprintf(g.stop_reason, sizeof(g.stop_reason), "timer_fire_n%u",
                             g.fire_ext_n);
                p22f_finalize(g.stop_reason);
            }
        }
    }
}

void p22f_note_plat(uint32_t code, uint32_t app, uint32_t arg2, uint32_t arg3, uint32_t ret,
                    uint32_t caller_pc, uint32_t r9) {
    char res[48];
    uint32_t cands[4];
    int i;
    if (!p22f_enabled()) return;
    snprintf(res, sizeof(res), "ret=0x%X", ret);
    add_evt("platform_send_app_event", "plat", code, 0, 0, caller_pc, app, arg2, arg3, ret, r9,
            r9, res);
    /* Detect callback pointer args (Thumb |1) targeting key gamelist entries. */
    if (g.gl_base) {
        cands[0] = app;
        cands[1] = arg2;
        cands[2] = arg3;
        cands[3] = ret;
        for (i = 0; i < 4; i++) {
            uint32_t tgt = cands[i] & ~1u;
            if (tgt == (g.gl_base + OFF_8CDC) || tgt == (g.gl_base + OFF_F670) ||
                tgt == (g.gl_base + OFF_D978) || tgt == (g.gl_base + OFF_392C) ||
                tgt == (g.gl_base + OFF_12CF0)) {
                char svc[48];
                snprintf(svc, sizeof(svc), "plat_0x%X_arg%d", code, i);
                record_reg(caller_pc, "platform", 0, cands[i], svc);
            }
        }
    }
}

void p22f_note_mem_write(uint32_t pc, uint32_t addr, uint32_t size, uint32_t value,
                         const char *module) {
    uint32_t tgt;
    if (!p22f_enabled() || !g.gl_base) return;
    if (size < 4) return;
    tgt = value & ~1u;
    if (tgt == (g.gl_base + OFF_8CDC) || tgt == (g.gl_base + OFF_F670) ||
        tgt == (g.gl_base + OFF_D978) || tgt == (g.gl_base + OFF_392C))
        record_reg(pc, module, addr, value, "note_mem_write");
}

void p22f_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr) {
    uint32_t norm, off;
    if (!p22f_enabled() || g.finalized) return;
    if (!is_gl(module_name)) return;
    if (!g.uc) g.uc = uc;
    if (!g.gl_base) {
        /* Try resolve from pc via registry. */
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *m =
            reg ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
        if (m && m->map.guest_code_base) {
            p22f_note_module_map(module_name, m->map.guest_code_base, m->map.guest_code_size,
                                 m->data.start_of_er_rw, 0, 0, m->module_id, "gwy/gamelist.mrp");
        }
    }
    if (!g.gl_base) return;
    norm = pc & ~1u;
    if (norm < g.gl_base || norm >= g.gl_end) return;
    off = norm - g.gl_base;
    g.gl_insn_n++;

    if (!g.dump_done) verify_xrefs_and_dump(uc);

    /* Level-1 hit counts on watch sites. */
    if (find_hit(off)) note_hit(uc, off, regs, lr, sp, cpsr);

    /* Level-2: dense branch slice inside focus function windows. */
    if (g.slice_armed) {
        int in_focus = 0;
        if (g.slice_focus_off == OFF_392C && off >= OFF_392C && off < 0x5A00u) in_focus = 1;
        if (g.slice_focus_off == OFF_8CDC && off >= OFF_8CDC && off < 0x8D40u) in_focus = 1;
        if (g.slice_focus_off == OFF_D978 && off >= OFF_D978 && off < 0xD9C0u) in_focus = 1;
        if (g.slice_focus_off == OFF_12CF0 && off >= OFF_12CF0 && off < 0x12D40u) in_focus = 1;
        if (g.slice_focus_off == OFF_F670 && off >= OFF_F670 && off < 0xF680u) in_focus = 1;
        if (in_focus) maybe_slice_branch(uc, off, regs, cpsr);
    }

    /* Prep snap of R9 fields occasionally (not called lock until 10740). */
    if ((g.gl_insn_n == 100u || g.gl_insn_n == 1000u || g.gl_insn_n == 10000u) && regs[9])
        snap_r9_fields(uc, regs[9]);

    if (g.hit_10740 && off == OFF_10740) {
        /* continue tracking internals via note_hit */
    }

    if (g.gl_insn_n >= 5000000u) {
        snprintf(g.stop_reason, sizeof(g.stop_reason), "gl_insn_5M");
        p22f_finalize(g.stop_reason);
    }
}

void p22f_finalize(const char *stop_reason) {
    const char *sum_path;
    if (!p22f_enabled() || g.finalized) return;
    g.finalized = 1;
    if (stop_reason && stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", stop_reason);
    else if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "finalize");

    refresh_identity_from_registry();
    if (!g.dump_done) verify_xrefs_and_dump(g.uc);
    flush_hits_csv();
    classify_and_verdict();
    write_verdict_md();

    sum_path = env_or("JJFB_P22F_SUMMARY", "out/p22f/p22f_runtime_summary.txt");
    g.summary_txt = fopen(sum_path, "wb");
    if (g.summary_txt) {
        fprintf(g.summary_txt,
                "run_id=%s\n"
                "source_commit=%s\n"
                "main_exe_sha256=%s\n"
                "raw_gamelist_ext_sha256=%s\n"
                "runtime_image_sha256=%s\n"
                "gl_base=0x%X\n"
                "gl_end=0x%X\n"
                "gl_size=0x%X\n"
                "raw_base_refine_pad=0x%X\n"
                "module_id=%s\n"
                "ERW=0x%X\n"
                "P=%s\n"
                "generation=%s\n"
                "package_owner=%s\n"
                "identity_gaps=%s\n"
                "class=%s\n"
                "nearest_caller=%s\n"
                "first_block_branch=%s\n"
                "first_block_ops=%s\n"
                "actual_path=%s\n"
                "target_path=%s\n"
                "natural_producer=%s\n"
                "missing_contract=%s\n"
                "hit_10740=%u\n"
                "hit_10814=%u\n"
                "hit_FF00=%u\n"
                "hit_7B6C=%u\n"
                "hit_7DB0=%u\n"
                "snap_r9_3e4=0x%X\n"
                "snap_r9_6c4=0x%X\n"
                "snap_r9_450=0x%X\n"
                "fire_ext_n=%u\n"
                "gl_insn_n=%u\n"
                "reg_n=%u\n"
                "xref_n=%d\n"
                "extra_direct=%d\n"
                "extra_indirect=%d\n"
                "stop_reason=%s\n"
                "guest_state_written=0\n"
                "events_injected=0\n"
                "headless=0\n",
                g.run_id, g.source_commit, g.main_exe_sha, g.raw_ext_sha,
                g.runtime_sha[0] ? g.runtime_sha : "UNKNOWN_NOT_EXPOSED", g.gl_base, g.gl_end,
                g.gl_size, g.raw_base_refine_pad,
                g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED", g.erw,
                g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
                g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
                g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED",
                g.identity_missing[0] ? g.identity_missing : "none", g.verdict_class,
                g.nearest_caller, g.first_block_branch, g.first_block_ops, g.actual_path,
                g.target_path, g.natural_producer, g.missing_contract, g.hit_10740, g.hit_10814,
                g.hit_ff00, g.hit_7b6c, g.hit_7db0, g.snap_r9_3e4, g.snap_r9_6c4, g.snap_r9_450,
                g.fire_ext_n, g.gl_insn_n, g.reg_n, g.xref_n, g.extra_direct_n,
                g.extra_indirect_n, g.stop_reason);
        fflush(g.summary_txt);
        fclose(g.summary_txt);
        g.summary_txt = NULL;
    }

    printf("[JJFB_P22F_FINAL] class=%s nearest=%s contract=%s fire=%u insn=%u "
           "hit10740=%u evidence=OBSERVED\n",
           g.verdict_class, g.nearest_caller, g.missing_contract, g.fire_ext_n, g.gl_insn_n,
           g.hit_10740);
    fflush(stdout);
}
