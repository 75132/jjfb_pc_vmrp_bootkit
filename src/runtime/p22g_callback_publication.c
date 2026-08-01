#include "gwy_launcher/p22g_callback_publication.h"

#include "gwy_launcher/ext_chunk_provider.h"
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

/* Targets relative to refined gamelist.ext guest_code_base. */
#define OFF_F670 0xF670u
#define OFF_8CDC 0x8CDCu
#define OFF_D978 0xD978u
#define OFF_392C 0x392Cu
#define OFF_HEADER_CAND 0x8u

#define WRITE_CAP 512
#define COPY_CAP 256
#define TABLE_CAP 256
#define LIFE_CAP 128
#define TIMELINE_CAP 512
#define HELPER_CAP 512
#define INDIRECT_CAP 256
#define SCAN_REGION_MAX 0x40000u
#define GL_INSN_STOP 10000000u
#define FIRE_STOP 20u
#define IDLE_STABLE_N 3u

typedef enum {
    INIT_NONE = 0,
    INIT_MAPPED,
    INIT_REGISTERED,
    INIT_HEADER_ENTRY_CALLED,
    INIT_HELPER_REGISTERED,
    INIT_HELPER_INVOKED,
    INIT_METHOD_COMPLETE,
    INIT_CALLBACK_PUBLISHED,
    INIT_CALLBACK_INVOKED
} InitState;

typedef struct {
    uint32_t seq;
    uint32_t writer_pc;
    char writer_module[48];
    uint32_t writer_offset;
    uint32_t written_value;
    uint32_t normalized_target;
    int thumb_bit;
    uint32_t destination_address;
    char destination_owner[48];
    uint32_t r9, erw, p_guest;
    uint64_t generation;
    char instruction[40];
    int source_register;
    char channel[32];
} WriteRow;

typedef struct {
    uint32_t seq;
    uint32_t copy_call_pc;
    uint32_t source;
    uint32_t destination;
    uint32_t length;
    uint32_t ptr_offset_in_src;
    uint32_t raw_value;
    uint32_t normalized_target;
    char channel[24];
} CopyRow;

typedef struct {
    uint32_t seq;
    char target[16];
    uint32_t memory_address;
    uint32_t first_seen_seq;
    uint32_t last_seen_seq;
    char containing_region[40];
    uint32_t possible_table_base;
    uint32_t context[16]; /* ±8 words */
    int overwritten;
    char scan_reason[40];
} TableHit;

typedef struct {
    char target[16];
    uint32_t memory_address;
    uint32_t first_seq;
    uint32_t last_seq;
    uint32_t last_value;
    int live;
} LifeRow;

typedef struct {
    uint32_t seq;
    char state_before[32];
    char state_after[32];
    char caller[48];
    char callee[48];
    char module[48];
    uint32_t pc;
    uint32_t r0, r1, r2, r3, lr, r9, erw, p_guest;
    uint64_t generation;
    int32_t return_value;
    char note[96];
} TimelineRow;

typedef struct {
    uint32_t seq;
    char caller_module[48];
    uint32_t caller_pc;
    uint32_t helper_target;
    uint32_t method;
    uint32_t r0, r1, r2, r3;
    uint32_t stack_args[4];
    uint32_t input;
    uint32_t input_len;
    int32_t return_value;
    uint32_t output_hint;
    uint32_t r9;
    char phase[24]; /* enter/return */
} HelperRow;

typedef struct {
    uint32_t seq;
    uint32_t indirect_call_pc;
    int target_register;
    uint32_t raw_target;
    uint32_t normalized_target;
    uint32_t source_mem;
    uint32_t table_base;
    uint32_t table_slot;
    char module_owner[48];
    uint32_t r9;
    uint32_t r0, r1, r2, r3;
    char insn[32];
} IndirRow;

typedef struct {
    int known, enabled, finalized;
    void *uc;
    clock_t t0;
    uint32_t seq;

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

    uint32_t gl_base, gl_end, gl_size, raw_base_refine_pad;
    uint32_t erw, p_guest;
    uint64_t generation, module_id;
    uint32_t gl_helper;
    uint32_t header_cand;

    uint32_t t_a, t_b, t_c; /* normalized absolute targets */

    InitState init_state;
    InitState farthest;
    char missing_transition[48];
    char missing_producer[64];
    char missing_method[48];
    char missing_contract[128];
    char block_branch[96];
    char block_ops[96];
    char verdict_class[8];
    char stop_reason[96];

    uint32_t gl_insn_n;
    uint32_t fire_ext_n;
    uint32_t helper_invoke_n;
    uint32_t method_seen_mask; /* bit0=0 bit1=6 bit2=8 */
    uint32_t first_helper_caller_pc;
    char first_helper_caller_mod[48];
    uint32_t natural_methods[16];
    int natural_method_n;

    int wrote_f670, wrote_8cdc, wrote_d978;
    int indir_f670, indir_8cdc, indir_d978;
    int entered_f670, entered_8cdc, entered_d978;
    uint32_t write_pc_f670, write_pc_8cdc, write_pc_d978;
    uint32_t write_dest_f670, write_dest_8cdc, write_dest_d978;

    int slice_armed;
    uint32_t slice_focus;
    uint32_t last_cmp_pc, last_cmp_lhs, last_cmp_rhs;
    uint32_t last_init_change_seq;
    uint32_t idle_check_n;
    InitState idle_prev_state;

    WriteRow writes[WRITE_CAP];
    uint32_t write_n;
    CopyRow copies[COPY_CAP];
    uint32_t copy_n;
    TableHit tables[TABLE_CAP];
    uint32_t table_n;
    LifeRow lives[LIFE_CAP];
    uint32_t life_n;
    TimelineRow timeline[TIMELINE_CAP];
    uint32_t timeline_n;
    HelperRow helpers[HELPER_CAP];
    uint32_t helper_n;
    IndirRow indirs[INDIRECT_CAP];
    uint32_t indir_n;

    FILE *writes_csv, *copies_csv, *tables_csv, *life_csv, *timeline_csv;
    FILE *helper_csv, *missing_csv, *indir_csv, *verdict_md, *summary_txt;

#ifdef GWY_HAVE_UNICORN
    uc_hook mem_hook;
    int mem_hook_armed;
#endif
} P22gState;

static P22gState g;

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
        snprintf(g.identity_missing + L, sizeof(g.identity_missing) - L, "%s%s", L ? ";" : "",
                 why);
    }
}

static const char *init_name(InitState s) {
    switch (s) {
    case INIT_MAPPED:
        return "MAPPED";
    case INIT_REGISTERED:
        return "REGISTERED";
    case INIT_HEADER_ENTRY_CALLED:
        return "HEADER_ENTRY_CALLED";
    case INIT_HELPER_REGISTERED:
        return "HELPER_REGISTERED";
    case INIT_HELPER_INVOKED:
        return "HELPER_INVOKED";
    case INIT_METHOD_COMPLETE:
        return "INIT_METHOD_COMPLETE";
    case INIT_CALLBACK_PUBLISHED:
        return "CALLBACK_PUBLISHED";
    case INIT_CALLBACK_INVOKED:
        return "CALLBACK_INVOKED";
    default:
        return "NONE";
    }
}

static FILE *open_out(const char *env_key, const char *fallback) {
    const char *p = env_or(env_key, fallback);
    return fopen(p, "wb");
}

static void identity_header(FILE *f) {
    if (!f) return;
    fprintf(f,
            "# run_id=%s source_commit=%s main_exe_sha256=%s raw_gamelist_ext_sha256=%s "
            "runtime_image_sha256=%s module_id=%s runtime_base=0x%X ERW=0x%X P=%s generation=%s "
            "package_owner=%s identity_gaps=%s\n",
            g.run_id[0] ? g.run_id : "?", g.source_commit[0] ? g.source_commit : "?",
            g.main_exe_sha[0] ? g.main_exe_sha : "?", g.raw_ext_sha[0] ? g.raw_ext_sha : "?",
            g.runtime_sha[0] ? g.runtime_sha : "PENDING",
            g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED", g.gl_base, g.erw,
            g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
            g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
            g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED",
            g.identity_missing[0] ? g.identity_missing : "none");
}

static void ensure_files(void) {
    if (!g.writes_csv) {
        g.writes_csv =
            open_out("JJFB_P22G_WRITES_CSV", "reports/p22g/p22g_target_pointer_writes.csv");
        if (g.writes_csv) {
            identity_header(g.writes_csv);
            fputs("sequence,writer_pc,writer_module,writer_offset,written_value,normalized_target,"
                  "thumb_bit,destination_address,destination_owner,R9,ERW,P,generation,"
                  "instruction,source_register,channel\n",
                  g.writes_csv);
            fflush(g.writes_csv);
        }
    }
    if (!g.copies_csv) {
        g.copies_csv =
            open_out("JJFB_P22G_COPIES_CSV", "reports/p22g/p22g_target_pointer_copies.csv");
        if (g.copies_csv) {
            identity_header(g.copies_csv);
            fputs("sequence,copy_call_pc,source,destination,length,ptr_offset_in_src,raw_value,"
                  "normalized_target,channel\n",
                  g.copies_csv);
            fflush(g.copies_csv);
        }
    }
    if (!g.tables_csv) {
        g.tables_csv =
            open_out("JJFB_P22G_TABLES_CSV", "reports/p22g/p22g_guest_function_tables.csv");
        if (g.tables_csv) {
            identity_header(g.tables_csv);
            fputs("sequence,target,memory_address,first_seen_sequence,last_seen_sequence,"
                  "containing_region,possible_table_base,ctx_m8,ctx_m7,ctx_m6,ctx_m5,ctx_m4,"
                  "ctx_m3,ctx_m2,ctx_m1,ctx_0,ctx_p1,ctx_p2,ctx_p3,ctx_p4,ctx_p5,ctx_p6,ctx_p7,"
                  "overwritten,scan_reason\n",
                  g.tables_csv);
            fflush(g.tables_csv);
        }
    }
    if (!g.life_csv) {
        g.life_csv = open_out("JJFB_P22G_LIFE_CSV", "reports/p22g/p22g_pointer_lifetime.csv");
        if (g.life_csv) {
            identity_header(g.life_csv);
            fputs("target,memory_address,first_seen_sequence,last_seen_sequence,last_value,live\n",
                  g.life_csv);
            fflush(g.life_csv);
        }
    }
    if (!g.timeline_csv) {
        g.timeline_csv =
            open_out("JJFB_P22G_TIMELINE_CSV", "reports/p22g/p22g_module_init_timeline.csv");
        if (g.timeline_csv) {
            identity_header(g.timeline_csv);
            fputs("sequence,state_before,state_after,caller,callee,module,PC,R0,R1,R2,R3,LR,R9,"
                  "ERW,P,generation,return_value,note\n",
                  g.timeline_csv);
            fflush(g.timeline_csv);
        }
    }
    if (!g.helper_csv) {
        g.helper_csv =
            open_out("JJFB_P22G_HELPER_CSV", "reports/p22g/p22g_gamelist_helper_invocations.csv");
        if (g.helper_csv) {
            identity_header(g.helper_csv);
            fputs("sequence,caller_module,caller_pc,helper_target,method,R0,R1,R2,R3,"
                  "stack0,stack1,stack2,stack3,input,input_len,return_value,output_hint,R9,"
                  "phase\n",
                  g.helper_csv);
            fflush(g.helper_csv);
        }
    }
    if (!g.missing_csv) {
        g.missing_csv =
            open_out("JJFB_P22G_MISSING_CSV", "reports/p22g/p22g_missing_init_transition.csv");
        if (g.missing_csv) {
            identity_header(g.missing_csv);
            fputs("farthest_state,missing_transition,expected_producer,expected_method_or_event,"
                  "block_branch,block_ops,natural_method_sequence,note\n",
                  g.missing_csv);
            fflush(g.missing_csv);
        }
    }
    if (!g.indir_csv) {
        g.indir_csv = open_out("JJFB_P22G_INDIR_CSV", "reports/p22g/p22g_indirect_calls.csv");
        if (g.indir_csv) {
            identity_header(g.indir_csv);
            fputs("sequence,indirect_call_pc,target_register,raw_target,normalized_target,"
                  "source_mem,table_base,table_slot,module_owner,R9,R0,R1,R2,R3,instruction\n",
                  g.indir_csv);
            fflush(g.indir_csv);
        }
    }
}

static int match_target(uint32_t raw, uint32_t *norm_out, const char **name_out) {
    uint32_t n = raw & ~1u;
    if (!g.gl_base) return 0;
    if (n == g.t_a) {
        if (norm_out) *norm_out = n;
        if (name_out) *name_out = "+0xF670";
        return 1;
    }
    if (n == g.t_b) {
        if (norm_out) *norm_out = n;
        if (name_out) *name_out = "+0x8CDC";
        return 2;
    }
    if (n == g.t_c) {
        if (norm_out) *norm_out = n;
        if (name_out) *name_out = "+0xD978";
        return 3;
    }
    return 0;
}

static void refresh_targets(void) {
    if (!g.gl_base) return;
    g.t_a = g.gl_base + OFF_F670;
    g.t_b = g.gl_base + OFF_8CDC;
    g.t_c = g.gl_base + OFF_D978;
    g.header_cand = g.gl_base + OFF_HEADER_CAND;
}

static void refresh_identity(void) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m = NULL;
    ExtChunkOwnerInfo oi;
    memset(&oi, 0, sizeof(oi));
    reg = gwy_ext_loader_bound_registry();
    if (reg && g.gl_base) m = module_registry_find_by_code_addr(reg, g.gl_base);
    if (m) {
        g.module_id = m->module_id;
        snprintf(g.module_id_s, sizeof(g.module_id_s), "0x%llX",
                 (unsigned long long)m->module_id);
        if (m->data.start_of_er_rw) g.erw = m->data.start_of_er_rw;
        if (m->map.helper_address) g.gl_helper = m->map.helper_address;
        if (m->entries.registered_helper) g.gl_helper = m->entries.registered_helper;
        if (m->requested_name[0] || m->resolved_name[0]) {
            const char *n = m->resolved_name[0] ? m->resolved_name : m->requested_name;
            snprintf(g.package_owner, sizeof(g.package_owner), "%s", n);
        }
    } else if (!g.module_id_s[0]) {
        set_unknown(g.module_id_s, sizeof(g.module_id_s), "module_registry_find_by_code_addr");
    }
    if (g.p_guest && ext_chunk_provider_owner_for_p(g.p_guest, &oi)) {
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
        } else {
            set_unknown(g.p_guest_s, sizeof(g.p_guest_s), "ext_chunk_provider_last_p_guest");
        }
    }
    if (!g.generation_s[0])
        set_unknown(g.generation_s, sizeof(g.generation_s), "module_generation");
    if (!g.package_owner[0])
        set_unknown(g.package_owner, sizeof(g.package_owner), "module_registry_name");
}

static void bump_init(InitState next, const char *caller, const char *callee, const char *module,
                      uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t lr,
                      uint32_t r9, int32_t ret, const char *note) {
    TimelineRow *t;
    InitState before = g.init_state;
    if ((int)next < (int)g.init_state && next != INIT_NONE) {
        /* allow non-monotonic notes only as timeline, not state regression except publish */
    }
    if ((int)next > (int)g.init_state) g.init_state = next;
    if ((int)g.init_state > (int)g.farthest) {
        g.farthest = g.init_state;
        g.last_init_change_seq = g.seq;
    }
    if (g.timeline_n >= TIMELINE_CAP) return;
    ensure_files();
    t = &g.timeline[g.timeline_n++];
    memset(t, 0, sizeof(*t));
    t->seq = ++g.seq;
    snprintf(t->state_before, sizeof(t->state_before), "%s", init_name(before));
    snprintf(t->state_after, sizeof(t->state_after), "%s", init_name(g.init_state));
    snprintf(t->caller, sizeof(t->caller), "%s", caller ? caller : "?");
    snprintf(t->callee, sizeof(t->callee), "%s", callee ? callee : "?");
    snprintf(t->module, sizeof(t->module), "%s", module ? module : "?");
    t->pc = pc;
    t->r0 = r0;
    t->r1 = r1;
    t->r2 = r2;
    t->r3 = r3;
    t->lr = lr;
    t->r9 = r9;
    t->erw = g.erw;
    t->p_guest = g.p_guest;
    t->generation = g.generation;
    t->return_value = ret;
    snprintf(t->note, sizeof(t->note), "%s", note ? note : "");
    if (g.timeline_csv) {
        fprintf(g.timeline_csv,
                "%u,%s,%s,%s,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%llu,%d,\"%s\"\n",
                t->seq, t->state_before, t->state_after, t->caller, t->callee, t->module, t->pc,
                t->r0, t->r1, t->r2, t->r3, t->lr, t->r9, t->erw, t->p_guest,
                (unsigned long long)t->generation, t->return_value, t->note);
        fflush(g.timeline_csv);
    }
}

static void life_touch(const char *target, uint32_t addr, uint32_t value, int present) {
    uint32_t i;
    LifeRow *L = NULL;
    for (i = 0; i < g.life_n; i++) {
        if (g.lives[i].memory_address == addr && strcmp(g.lives[i].target, target) == 0) {
            L = &g.lives[i];
            break;
        }
    }
    if (!L) {
        if (!present || g.life_n >= LIFE_CAP) return;
        L = &g.lives[g.life_n++];
        memset(L, 0, sizeof(*L));
        snprintf(L->target, sizeof(L->target), "%s", target);
        L->memory_address = addr;
        L->first_seq = g.seq;
        L->live = 1;
    }
    L->last_seq = g.seq;
    L->last_value = value;
    if (!present && L->live) L->live = 0;
}

static void record_write(uint32_t pc, const char *mod, uint32_t dest, uint32_t value,
                         const char *insn, int src_reg, const char *channel) {
    WriteRow *w;
    uint32_t norm = 0;
    const char *tname = NULL;
    int which;
    if (!(which = match_target(value, &norm, &tname))) return;
    if (g.write_n >= WRITE_CAP) return;
    ensure_files();
    w = &g.writes[g.write_n++];
    memset(w, 0, sizeof(*w));
    w->seq = ++g.seq;
    w->writer_pc = pc;
    snprintf(w->writer_module, sizeof(w->writer_module), "%s", mod ? mod : "?");
    if (g.gl_base && (pc & ~1u) >= g.gl_base && (pc & ~1u) < g.gl_end)
        w->writer_offset = (pc & ~1u) - g.gl_base;
    w->written_value = value;
    w->normalized_target = norm;
    w->thumb_bit = (int)(value & 1u);
    w->destination_address = dest;
    snprintf(w->destination_owner, sizeof(w->destination_owner), "%s", tname);
    w->r9 = 0;
    w->erw = g.erw;
    w->p_guest = g.p_guest;
    w->generation = g.generation;
    snprintf(w->instruction, sizeof(w->instruction), "%s", insn ? insn : "?");
    w->source_register = src_reg;
    snprintf(w->channel, sizeof(w->channel), "%s", channel ? channel : "?");
    if (g.writes_csv) {
        fprintf(g.writes_csv,
                "%u,0x%X,%s,0x%X,0x%X,0x%X,%d,0x%X,%s,0x%X,0x%X,0x%X,%llu,%s,%d,%s\n", w->seq,
                w->writer_pc, w->writer_module, w->writer_offset, w->written_value,
                w->normalized_target, w->thumb_bit, w->destination_address, w->destination_owner,
                w->r9, w->erw, w->p_guest, (unsigned long long)w->generation, w->instruction,
                w->source_register, w->channel);
        fflush(g.writes_csv);
    }
    if (which == 1) {
        g.wrote_f670 = 1;
        if (!g.write_pc_f670) {
            g.write_pc_f670 = pc;
            g.write_dest_f670 = dest;
        }
    } else if (which == 2) {
        g.wrote_8cdc = 1;
        if (!g.write_pc_8cdc) {
            g.write_pc_8cdc = pc;
            g.write_dest_8cdc = dest;
        }
    } else if (which == 3) {
        g.wrote_d978 = 1;
        if (!g.write_pc_d978) {
            g.write_pc_d978 = pc;
            g.write_dest_d978 = dest;
        }
    }
    life_touch(tname, dest, value, 1);
    bump_init(INIT_CALLBACK_PUBLISHED, mod, "publish_callback", "gamelist", pc, value, dest, 0, 0,
              0, 0, 0, channel);
    printf("[JJFB_P22G] ptr_write target=%s val=0x%X dest=0x%X pc=0x%X ch=%s evidence=OBSERVED\n",
           tname, value, dest, pc, channel ? channel : "?");
    fflush(stdout);
}

static void record_copy(uint32_t pc, uint32_t src, uint32_t dst, uint32_t len, uint32_t off,
                        uint32_t raw, const char *channel) {
    CopyRow *c;
    uint32_t norm = 0;
    if (!match_target(raw, &norm, NULL)) return;
    if (g.copy_n >= COPY_CAP) return;
    ensure_files();
    c = &g.copies[g.copy_n++];
    memset(c, 0, sizeof(*c));
    c->seq = ++g.seq;
    c->copy_call_pc = pc;
    c->source = src;
    c->destination = dst;
    c->length = len;
    c->ptr_offset_in_src = off;
    c->raw_value = raw;
    c->normalized_target = norm;
    snprintf(c->channel, sizeof(c->channel), "%s", channel ? channel : "memcpy");
    if (g.copies_csv) {
        fprintf(g.copies_csv, "%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", c->seq, c->copy_call_pc,
                c->source, c->destination, c->length, c->ptr_offset_in_src, c->raw_value,
                c->normalized_target, c->channel);
        fflush(g.copies_csv);
    }
    record_write(pc, "memcpy", dst + off, raw, "memcpy", -1, channel);
}

static void scan_buffer_for_targets(const uint8_t *buf, uint32_t base, uint32_t n,
                                    const char *region, const char *reason) {
    uint32_t off;
    if (!buf || n < 4) return;
    for (off = 0; off + 4 <= n; off += 4) {
        uint32_t w = (uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8) |
                     ((uint32_t)buf[off + 2] << 16) | ((uint32_t)buf[off + 3] << 24);
        uint32_t norm = 0;
        const char *tname = NULL;
        TableHit *th;
        uint32_t i, addr;
        int found = 0;
        if (!match_target(w, &norm, &tname)) continue;
        addr = base + off;
        for (i = 0; i < g.table_n; i++) {
            if (g.tables[i].memory_address == addr && strcmp(g.tables[i].target, tname) == 0) {
                g.tables[i].last_seen_seq = g.seq;
                found = 1;
                th = &g.tables[i];
                break;
            }
        }
        if (!found) {
            if (g.table_n >= TABLE_CAP) continue;
            ensure_files();
            th = &g.tables[g.table_n++];
            memset(th, 0, sizeof(*th));
            th->seq = ++g.seq;
            snprintf(th->target, sizeof(th->target), "%s", tname);
            th->memory_address = addr;
            th->first_seen_seq = th->seq;
            th->last_seen_seq = th->seq;
            snprintf(th->containing_region, sizeof(th->containing_region), "%s",
                     region ? region : "?");
            th->possible_table_base = addr & ~0x3Fu;
            snprintf(th->scan_reason, sizeof(th->scan_reason), "%s", reason ? reason : "scan");
            {
                int k;
                for (k = -8; k < 8; k++) {
                    uint32_t a = (uint32_t)((int32_t)addr + k * 4);
                    uint32_t wv = 0;
                    if (a >= base && a + 4 <= base + n) {
                        uint32_t o = a - base;
                        wv = (uint32_t)buf[o] | ((uint32_t)buf[o + 1] << 8) |
                             ((uint32_t)buf[o + 2] << 16) | ((uint32_t)buf[o + 3] << 24);
                    }
                    th->context[k + 8] = wv;
                }
            }
            if (g.tables_csv) {
                fprintf(g.tables_csv,
                        "%u,%s,0x%X,%u,%u,%s,0x%X,"
                        "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                        "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                        "%d,%s\n",
                        th->seq, th->target, th->memory_address, th->first_seen_seq,
                        th->last_seen_seq, th->containing_region, th->possible_table_base,
                        th->context[0], th->context[1], th->context[2], th->context[3],
                        th->context[4], th->context[5], th->context[6], th->context[7],
                        th->context[8], th->context[9], th->context[10], th->context[11],
                        th->context[12], th->context[13], th->context[14], th->context[15],
                        th->overwritten, th->scan_reason);
                fflush(g.tables_csv);
            }
            record_write(0, "scan", addr, w, "scan_hit", -1, reason);
        }
        life_touch(tname, addr, w, 1);
    }
}

static void scan_guest_regions(const char *reason) {
#ifdef GWY_HAVE_UNICORN
    uc_mem_region *regions = NULL;
    uint32_t count = 0;
    uint32_t i;
    if (!g.uc || !g.gl_base) return;
    if (uc_mem_regions((uc_engine *)g.uc, &regions, &count) != UC_ERR_OK) return;
    for (i = 0; i < count; i++) {
        uint64_t begin = regions[i].begin;
        uint64_t end = regions[i].end;
        uint32_t perms = regions[i].perms;
        uint32_t sz;
        uint8_t *buf;
        char regname[40];
        if (!(perms & UC_PROT_READ)) continue;
        if (!(perms & UC_PROT_WRITE) && begin != g.gl_base) {
            /* still scan RW and known ERW; skip pure RX code except for self-check */
            if (g.erw == 0 || begin != g.erw) continue;
        }
        if (end <= begin) continue;
        sz = (uint32_t)(end - begin + 1);
        if (sz > SCAN_REGION_MAX) sz = SCAN_REGION_MAX;
        /* Prefer ERW / heap-like; skip huge code image except first page scan of gl for literals */
        if (begin == g.gl_base) {
            /* skip code image body — targets live as data pointers elsewhere */
            continue;
        }
        buf = (uint8_t *)malloc(sz);
        if (!buf) continue;
        if (guest_memory_uc_peek((struct uc_struct *)g.uc, (uint32_t)begin, buf, sz)) {
            if (g.erw && begin == g.erw)
                snprintf(regname, sizeof(regname), "ERW");
            else
                snprintf(regname, sizeof(regname), "rw_0x%X", (uint32_t)begin);
            scan_buffer_for_targets(buf, (uint32_t)begin, sz, regname, reason);
        }
        free(buf);
    }
    uc_free(regions);
#else
    (void)reason;
#endif
    /* Also scan ERW explicitly if known. */
#ifdef GWY_HAVE_UNICORN
    if (g.erw && g.uc) {
        uint8_t *buf;
        uint32_t sz = 0x10000u;
        buf = (uint8_t *)malloc(sz);
        if (buf) {
            if (guest_memory_uc_peek((struct uc_struct *)g.uc, g.erw, buf, sz))
                scan_buffer_for_targets(buf, g.erw, sz, "ERW", reason);
            free(buf);
        }
    }
#endif
}

static void maybe_sha_runtime(void) {
#ifdef GWY_HAVE_UNICORN
    uint8_t *buf;
    uint8_t digest[32];
    size_t j, n;
    if (!g.uc || !g.gl_base || !g.gl_size || g.runtime_sha[0]) return;
    n = g.gl_size;
    if (n > 0x40000u) n = 0x40000u;
    buf = (uint8_t *)malloc(n);
    if (!buf) return;
    if (guest_memory_uc_peek((struct uc_struct *)g.uc, g.gl_base, buf, (uint32_t)n)) {
        gwy_sha256(buf, n, digest);
        for (j = 0; j < 32; j++) sprintf(g.runtime_sha + j * 2, "%02x", digest[j]);
        g.runtime_sha[64] = 0;
    }
    free(buf);
#else
    ;
#endif
}

#ifdef GWY_HAVE_UNICORN
static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                         void *user) {
    uint32_t v, pc = 0;
    (void)type;
    (void)user;
    if (!g.enabled || g.finalized || !g.gl_base || size < 4) return;
    v = (uint32_t)value;
    if (!match_target(v, NULL, NULL)) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    record_write(pc, "guest_mem", (uint32_t)address, v, "UC_MEM_WRITE", -1, "mem_write_hook");
}
#endif

static void arm_mem_hook(void) {
#ifdef GWY_HAVE_UNICORN
    if (!g.uc || g.mem_hook_armed || !g.gl_base) return;
    /* Filtered value-match only — P21 warned against unfiltered full-range logging. */
    if (uc_hook_add((uc_engine *)g.uc, &g.mem_hook, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL, 1,
                    0) == UC_ERR_OK)
        g.mem_hook_armed = 1;
#endif
}

static int cond_pass(uint32_t cpsr, unsigned cond) {
    unsigned N = (cpsr >> 31) & 1, Z = (cpsr >> 30) & 1, C = (cpsr >> 29) & 1, V = (cpsr >> 28) & 1;
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

static void observe_str_and_indirect(void *uc, uint32_t pc, const uint32_t regs[16], uint32_t lr,
                                     uint32_t sp, uint32_t cpsr, const char *module) {
#ifdef GWY_HAVE_UNICORN
    uint8_t b[4];
    uint16_t h, h2;
    uint32_t norm = pc & ~1u;
    int i;
    if (!uc || !guest_memory_uc_peek((struct uc_struct *)uc, norm, b, 4)) return;
    h = (uint16_t)(b[0] | (b[1] << 8));
    h2 = (uint16_t)(b[2] | (b[3] << 8));

    /* Thumb str rt,[rn,#imm] */
    if ((h & 0xF800) == 0x6000) {
        unsigned rt = h & 7, rn = (h >> 3) & 7, imm = ((h >> 6) & 0x1F) << 2;
        if (match_target(regs[rt], NULL, NULL)) {
            char insn[32];
            snprintf(insn, sizeof(insn), "str r%u,[r%u,#%u]", rt, rn, imm);
            record_write(pc, module, regs[rn] + imm, regs[rt], insn, (int)rt, "code_str");
        }
    }
    /* str rt,[sp,#imm] */
    if ((h & 0xF800) == 0x9000) {
        unsigned rt = (h >> 8) & 7, imm = (h & 0xFF) << 2;
        if (match_target(regs[rt], NULL, NULL)) {
            char insn[32];
            snprintf(insn, sizeof(insn), "str r%u,[sp,#%u]", rt, imm);
            record_write(pc, module, sp + imm, regs[rt], insn, (int)rt, "code_str_sp");
        }
    }
    /* stmia rn!, {reglist} — approximate first matching reg store */
    if ((h & 0xF800) == 0xC000) {
        unsigned rn = (h >> 8) & 7;
        unsigned list = h & 0xFF;
        uint32_t addr = regs[rn];
        for (i = 0; i < 8; i++) {
            if (!(list & (1u << i))) continue;
            if (match_target(regs[i], NULL, NULL)) {
                char insn[32];
                snprintf(insn, sizeof(insn), "stmia r%u!,{r%d}", rn, i);
                record_write(pc, module, addr, regs[i], insn, i, "code_stmia");
            }
            addr += 4;
        }
    }

    /* bx rm / blx rm */
    if ((h & 0xFF87) == 0x4700 || (h & 0xFF87) == 0x4780) {
        unsigned rm = (h >> 3) & 0xF;
        uint32_t raw = regs[rm];
        uint32_t nt = 0;
        const char *tname = NULL;
        if (match_target(raw, &nt, &tname) && g.indir_n < INDIRECT_CAP) {
            IndirRow *r;
            ensure_files();
            r = &g.indirs[g.indir_n++];
            memset(r, 0, sizeof(*r));
            r->seq = ++g.seq;
            r->indirect_call_pc = pc;
            r->target_register = (int)rm;
            r->raw_target = raw;
            r->normalized_target = nt;
            snprintf(r->module_owner, sizeof(r->module_owner), "%s", module ? module : "?");
            r->r9 = regs[9];
            r->r0 = regs[0];
            r->r1 = regs[1];
            r->r2 = regs[2];
            r->r3 = regs[3];
            snprintf(r->insn, sizeof(r->insn), "%s r%u", ((h & 0xFF87) == 0x4780) ? "blx" : "bx",
                     rm);
            if (g.indir_csv) {
                fprintf(g.indir_csv,
                        "%u,0x%X,%d,0x%X,0x%X,0x%X,0x%X,0x%X,%s,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n",
                        r->seq, r->indirect_call_pc, r->target_register, r->raw_target,
                        r->normalized_target, r->source_mem, r->table_base, r->table_slot,
                        r->module_owner, r->r9, r->r0, r->r1, r->r2, r->r3, r->insn);
                fflush(g.indir_csv);
            }
            if (nt == g.t_a) g.indir_f670 = 1;
            if (nt == g.t_b) g.indir_8cdc = 1;
            if (nt == g.t_c) g.indir_d978 = 1;
            bump_init(INIT_CALLBACK_INVOKED, module, tname, "gamelist", pc, regs[0], regs[1],
                      regs[2], regs[3], lr, regs[9], 0, r->insn);
        }
    }

    /* Dense slice: cmp + b.cond when init helper candidate armed */
    if (g.slice_armed) {
        if ((h & 0xF800) == 0x2800) {
            unsigned rn = (h >> 8) & 7;
            g.last_cmp_pc = pc;
            g.last_cmp_lhs = regs[rn];
            g.last_cmp_rhs = h & 0xFF;
        } else if ((h & 0xFFC0) == 0x4280) {
            g.last_cmp_pc = pc;
            g.last_cmp_lhs = regs[h & 7];
            g.last_cmp_rhs = regs[(h >> 3) & 7];
        } else if ((h & 0xF000) == 0xD000 && ((h >> 8) & 0xF) != 0xF) {
            unsigned cond = (h >> 8) & 0xF;
            int imm8 = (int)(int8_t)(h & 0xFF);
            int taken = cond_pass(cpsr, cond);
            if (!taken && !g.block_branch[0]) {
                snprintf(g.block_branch, sizeof(g.block_branch),
                         "pc=0x%X b.cond cond=%u taken=0 fallthru", pc, cond);
                snprintf(g.block_ops, sizeof(g.block_ops), "lhs=0x%X rhs=0x%X cmp_pc=0x%X",
                         g.last_cmp_lhs, g.last_cmp_rhs, g.last_cmp_pc);
            }
            (void)imm8;
            (void)h2;
        }
    }
#else
    (void)uc;
    (void)pc;
    (void)regs;
    (void)lr;
    (void)sp;
    (void)cpsr;
    (void)module;
#endif
}

static void flush_life_csv(void) {
    uint32_t i;
    ensure_files();
    if (!g.life_csv) return;
    for (i = 0; i < g.life_n; i++) {
        LifeRow *L = &g.lives[i];
        fprintf(g.life_csv, "%s,0x%X,%u,%u,0x%X,%d\n", L->target, L->memory_address, L->first_seq,
                L->last_seq, L->last_value, L->live);
    }
    fflush(g.life_csv);
}

static void classify(void) {
    int pub = g.wrote_f670 || g.wrote_8cdc || g.wrote_d978 || g.table_n > 0;
    int inv = g.entered_f670 || g.entered_8cdc || g.entered_d978 || g.indir_f670 || g.indir_8cdc ||
              g.indir_d978;
    int helper_nat = g.helper_invoke_n > 0;
    int got_init_methods = (g.method_seen_mask & 7u) != 0;

    if (pub && !inv) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "A");
        snprintf(g.missing_transition, sizeof(g.missing_transition), "CALLBACK_INVOKED");
        snprintf(g.missing_producer, sizeof(g.missing_producer), "platform_event_or_UI_dispatcher");
        snprintf(g.missing_method, sizeof(g.missing_method), "event_delivery");
        snprintf(g.missing_contract, sizeof(g.missing_contract),
                 "callback published in Guest table but never naturally invoked");
    } else if (!helper_nat && g.farthest <= INIT_HELPER_REGISTERED) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "B");
        snprintf(g.missing_transition, sizeof(g.missing_transition), "HELPER_INVOKED");
        snprintf(g.missing_producer, sizeof(g.missing_producer), "parent_Shell_handoff");
        snprintf(g.missing_method, sizeof(g.missing_method), "method_6_8_0_or_equiv");
        snprintf(g.missing_contract, sizeof(g.missing_contract),
                 "Shell→gamelist helper init handoff missing (no natural method 6/8/0)");
    } else if (helper_nat && !pub) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "C");
        snprintf(g.missing_transition, sizeof(g.missing_transition), "CALLBACK_PUBLISHED");
        snprintf(g.missing_producer, sizeof(g.missing_producer), "gamelist_init_method_body");
        snprintf(g.missing_method, sizeof(g.missing_method),
                 got_init_methods ? "post_helper_publish" : "init_method_incomplete");
        snprintf(g.missing_contract, sizeof(g.missing_contract),
                 "helper entered but returned before publishing F670/8CDC/D978");
    } else if (pub && g.life_n) {
        uint32_t i;
        int cleared = 0;
        for (i = 0; i < g.life_n; i++)
            if (!g.lives[i].live) cleared = 1;
        if (cleared) {
            snprintf(g.verdict_class, sizeof(g.verdict_class), "D");
            snprintf(g.missing_transition, sizeof(g.missing_transition), "CALLBACK_STABLE");
            snprintf(g.missing_contract, sizeof(g.missing_contract),
                     "callback pointer published then overwritten/cleared");
        }
    }

    if (!g.verdict_class[0]) {
        if (!pub && !helper_nat) {
            snprintf(g.verdict_class, sizeof(g.verdict_class), "F");
            if (g.farthest < INIT_HELPER_REGISTERED)
                snprintf(g.missing_transition, sizeof(g.missing_transition), "HELPER_REGISTERED");
            else if (g.farthest < INIT_HELPER_INVOKED)
                snprintf(g.missing_transition, sizeof(g.missing_transition), "HELPER_INVOKED");
            else
                snprintf(g.missing_transition, sizeof(g.missing_transition), "CALLBACK_PUBLISHED");
            snprintf(g.missing_producer, sizeof(g.missing_producer), "parent_Shell_or_module_entry");
            snprintf(g.missing_method, sizeof(g.missing_method), "method_6_8_0_or_equiv");
            snprintf(g.missing_contract, sizeof(g.missing_contract),
                     "no callback publication; natural init stalled at %s (FIRE_EXT=%u only)",
                     init_name(g.farthest), g.fire_ext_n);
        } else {
            snprintf(g.verdict_class, sizeof(g.verdict_class), "C");
            snprintf(g.missing_transition, sizeof(g.missing_transition), "CALLBACK_PUBLISHED");
            snprintf(g.missing_contract, sizeof(g.missing_contract),
                     "incomplete — see timeline/helper CSVs");
        }
    }
}

static void write_missing_row(void) {
    char methseq[96];
    int i, n = 0;
    methseq[0] = 0;
    for (i = 0; i < g.natural_method_n && n < (int)sizeof(methseq) - 8; i++)
        n += snprintf(methseq + n, sizeof(methseq) - (size_t)n, "%s%u", i ? "->" : "",
                      g.natural_methods[i]);
    if (!methseq[0]) snprintf(methseq, sizeof(methseq), "NONE");
    ensure_files();
    if (g.missing_csv) {
        fprintf(g.missing_csv, "%s,%s,%s,%s,\"%s\",\"%s\",%s,\"%s\"\n", init_name(g.farthest),
                g.missing_transition[0] ? g.missing_transition : "?",
                g.missing_producer[0] ? g.missing_producer : "?",
                g.missing_method[0] ? g.missing_method : "?",
                g.block_branch[0] ? g.block_branch : "n/a", g.block_ops[0] ? g.block_ops : "n/a",
                methseq, g.missing_contract);
        fflush(g.missing_csv);
    }
}

static void write_verdict(void) {
    char methseq[96];
    int i, n = 0;
    methseq[0] = 0;
    for (i = 0; i < g.natural_method_n && n < (int)sizeof(methseq) - 8; i++)
        n += snprintf(methseq + n, sizeof(methseq) - (size_t)n, "%s%u", i ? "->" : "",
                      g.natural_methods[i]);
    if (!methseq[0]) snprintf(methseq, sizeof(methseq), "NONE");

    g.verdict_md =
        open_out("JJFB_P22G_VERDICT", "reports/p22g/p22g_callback_publication_verdict.md");
    if (!g.verdict_md) return;
    fprintf(g.verdict_md,
            "# P22G-CLEAN callback publication verdict\n\n"
            "## Bottom line\n\n"
            "**Class: %s**\n\n"
            "```text\n"
            "farthest=%s\n"
            "missing_transition=%s\n"
            "→ expected producer=%s\n"
            "→ via method/event=%s\n"
            "→ missing host contract=%s\n"
            "```\n\n"
            "## Identity\n\n"
            "```\n"
            "source commit：%s\n"
            "main.exe SHA：%s\n"
            "gamelist.ext SHA：%s\n"
            "runtime image SHA：%s\n"
            "runtime base：0x%X\n"
            "module id：%s\n"
            "ERW：0x%X\n"
            "P：%s\n"
            "generation：%s\n"
            "package owner：%s\n"
            "```\n\n"
            "## PASS answers\n\n"
            "```\n"
            "source commit：%s\n"
            "main.exe SHA：%s\n"
            "gamelist.ext SHA：%s\n"
            "runtime base：0x%X\n"
            "module id：%s\n"
            "ERW：0x%X\n"
            "P：%s\n"
            "generation：%s\n"
            "package owner：%s\n"
            "\n"
            "module map 是否完成：%s\n"
            "module header entry 是否调用：%s\n"
            "helper 是否注册：%s\n"
            "helper 首次自然调用者：%s pc=0x%X\n"
            "自然 method/opcode 序列：%s\n"
            "\n"
            "+0xF670 是否被写入 Guest：%s\n"
            "写入 PC：0x%X\n"
            "目的地址/表槽：0x%X\n"
            "是否被间接调用：%s\n"
            "\n"
            "+0x8CDC 是否被写入 Guest：%s\n"
            "写入 PC：0x%X\n"
            "目的地址/表槽：0x%X\n"
            "是否被间接调用：%s\n"
            "\n"
            "+0xD978 是否被写入 Guest：%s\n"
            "写入 PC：0x%X\n"
            "目的地址/表槽：0x%X\n"
            "是否被间接调用：%s\n"
            "\n"
            "初始化生命周期最远状态：%s\n"
            "第一个缺失的状态转换：%s\n"
            "负责该转换的 Guest 函数：%s\n"
            "阻断分支：%s\n"
            "实际操作数：%s\n"
            "自然生产者：%s\n"
            "\n"
            "是否修改 Guest：NO\n"
            "是否注入事件：NO\n"
            "是否 Host 调用 callback：NO\n"
            "是否启用旧 headless/FAST：NO\n"
            "当前唯一门锁：Class %s — %s\n"
            "下一处最小通用修复：定位父级 Shell 向 gamelist helper 发起 method 6→8→0（或等价）"
            "的自然 handoff 合同；禁止 Host 直调 F670/8CDC/D978/10740\n"
            "stop_reason：%s\n"
            "fire_ext_n：%u\n"
            "gl_insn_n：%u\n"
            "```\n",
            g.verdict_class, init_name(g.farthest),
            g.missing_transition[0] ? g.missing_transition : "?",
            g.missing_producer[0] ? g.missing_producer : "?",
            g.missing_method[0] ? g.missing_method : "?",
            g.missing_contract[0] ? g.missing_contract : "?", g.source_commit, g.main_exe_sha,
            g.raw_ext_sha, g.runtime_sha[0] ? g.runtime_sha : "UNKNOWN_NOT_EXPOSED", g.gl_base,
            g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED", g.erw,
            g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
            g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
            g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED", g.source_commit,
            g.main_exe_sha, g.raw_ext_sha, g.gl_base,
            g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED", g.erw,
            g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
            g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
            g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED",
            g.farthest >= INIT_MAPPED ? "YES" : "NO",
            g.farthest >= INIT_HEADER_ENTRY_CALLED ? "YES" : "NO_OR_UNPROVEN",
            g.farthest >= INIT_HELPER_REGISTERED ? "YES" : "NO",
            g.first_helper_caller_mod[0] ? g.first_helper_caller_mod : "NONE",
            g.first_helper_caller_pc, methseq, g.wrote_f670 ? "YES" : "NO", g.write_pc_f670,
            g.write_dest_f670, (g.indir_f670 || g.entered_f670) ? "YES" : "NO",
            g.wrote_8cdc ? "YES" : "NO", g.write_pc_8cdc, g.write_dest_8cdc,
            (g.indir_8cdc || g.entered_8cdc) ? "YES" : "NO", g.wrote_d978 ? "YES" : "NO",
            g.write_pc_d978, g.write_dest_d978, (g.indir_d978 || g.entered_d978) ? "YES" : "NO",
            init_name(g.farthest), g.missing_transition[0] ? g.missing_transition : "?",
            g.missing_producer[0] ? g.missing_producer : "?",
            g.block_branch[0] ? g.block_branch : "n/a", g.block_ops[0] ? g.block_ops : "n/a",
            g.missing_producer[0] ? g.missing_producer : "unknown", g.verdict_class,
            g.missing_contract, g.stop_reason, g.fire_ext_n, g.gl_insn_n);
    fflush(g.verdict_md);
}

static void maybe_stop(const char *why) {
    if (g.finalized) return;
    if (why && why[0]) snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", why);
    p22g_finalize(g.stop_reason);
}

static int helper_is_gamelist(uint32_t helper) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    const char *mn;
    if (!helper) return 0;
    if (g.gl_helper && (helper & ~1u) == (g.gl_helper & ~1u)) return 1;
    if (g.gl_base && (helper & ~1u) >= g.gl_base && (helper & ~1u) < g.gl_end) return 1;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_helper(reg, helper) : NULL;
    if (!m && reg) m = module_registry_find_by_code_addr(reg, helper & ~1u);
    if (!m) return 0;
    mn = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    return is_gl(mn);
}

/* ---------- public API ---------- */

int p22g_enabled(void) {
    if (!g.known) {
        g.known = 1;
        g.enabled = env1("JJFB_P22G_CLEAN");
        if (g.enabled) {
            g.t0 = clock();
            snprintf(g.run_id, sizeof(g.run_id), "%s", env_or("JJFB_P22G_RUN_ID", "p22g"));
            snprintf(g.source_commit, sizeof(g.source_commit), "%s",
                     env_or("JJFB_P22G_SOURCE_COMMIT", "UNKNOWN_NOT_EXPOSED"));
            snprintf(g.main_exe_sha, sizeof(g.main_exe_sha), "%s",
                     env_or("JJFB_P22G_MAIN_SHA", "UNKNOWN_NOT_EXPOSED"));
            snprintf(g.raw_ext_sha, sizeof(g.raw_ext_sha), "%s",
                     env_or("JJFB_P22G_RAW_EXT_SHA", "UNKNOWN_NOT_EXPOSED"));
            if (!strcmp(g.source_commit, "UNKNOWN_NOT_EXPOSED"))
                set_unknown(g.source_commit, sizeof(g.source_commit), "JJFB_P22G_SOURCE_COMMIT");
            printf("[JJFB_P22G] armed run_id=%s evidence=OBSERVED\n", g.run_id);
            fflush(stdout);
        }
    }
    return g.enabled;
}

void p22g_reset(void) {
    FILE *a = g.writes_csv, *b = g.copies_csv, *c = g.tables_csv, *d = g.life_csv;
    FILE *e = g.timeline_csv, *f = g.helper_csv, *h = g.missing_csv, *i = g.indir_csv;
    FILE *v = g.verdict_md, *s = g.summary_txt;
    void *uc = g.uc;
#ifdef GWY_HAVE_UNICORN
    int hooked = g.mem_hook_armed;
    uc_hook hk = g.mem_hook;
#endif
    memset(&g, 0, sizeof(g));
    g.writes_csv = a;
    g.copies_csv = b;
    g.tables_csv = c;
    g.life_csv = d;
    g.timeline_csv = e;
    g.helper_csv = f;
    g.missing_csv = h;
    g.indir_csv = i;
    g.verdict_md = v;
    g.summary_txt = s;
    g.uc = uc;
#ifdef GWY_HAVE_UNICORN
    g.mem_hook_armed = hooked;
    g.mem_hook = hk;
#endif
}

void p22g_bind_uc(void *uc) {
    if (!p22g_enabled()) return;
    g.uc = uc;
}

void p22g_note_member_open(const char *guest_path) {
    if (!p22g_enabled() || !guest_path) return;
    if (!is_gl(guest_path) && !strstr(guest_path, "gamelist")) return;
    bump_init(INIT_NONE, "vfs", "member_open", guest_path, 0, 0, 0, 0, 0, 0, 0, 0, "member_open");
    printf("[JJFB_P22G] member_open path=%s evidence=OBSERVED\n", guest_path);
    fflush(stdout);
}

void p22g_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, uint64_t module_id,
                          const char *package_owner) {
    uint32_t prev;
    if (!p22g_enabled() || !is_gl(module_name)) return;
    prev = g.gl_base;
    if (!g.gl_base || base < g.gl_base ||
        (g.gl_base && base != g.gl_base && (g.gl_base & ~0xFFu) == (base & ~0xFFu))) {
        if (g.gl_base && base != g.gl_base)
            g.raw_base_refine_pad = (g.gl_base > base) ? (g.gl_base - base) : (base - g.gl_base);
        g.gl_base = base;
        g.gl_size = size;
        g.gl_end = g.gl_base + g.gl_size;
    } else if (size) {
        g.gl_size = size;
        g.gl_end = g.gl_base + g.gl_size;
    }
    if (erw) g.erw = erw;
    if (p_guest) {
        g.p_guest = p_guest;
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", p_guest);
    }
    if (generation) {
        g.generation = generation;
        snprintf(g.generation_s, sizeof(g.generation_s), "%llu", (unsigned long long)generation);
    }
    if (module_id) {
        g.module_id = module_id;
        snprintf(g.module_id_s, sizeof(g.module_id_s), "0x%llX", (unsigned long long)module_id);
    }
    if (package_owner && package_owner[0])
        snprintf(g.package_owner, sizeof(g.package_owner), "%s", package_owner);
    refresh_targets();
    refresh_identity();
    if (prev && prev != g.gl_base) g.runtime_sha[0] = 0;
    maybe_sha_runtime();
    arm_mem_hook();
    bump_init(INIT_MAPPED, "loader", "module_map", module_name, g.gl_base, g.gl_base, size, erw,
              p_guest, 0, 0, 0, "mapped");
    bump_init(INIT_REGISTERED, "ModuleRegistry", "register", module_name, g.gl_base, 0, 0, 0, 0, 0,
              0, 0, "registered");
    printf("[JJFB_P22G] module_map base=0x%X size=0x%X T_A=0x%X T_B=0x%X T_C=0x%X pad=0x%X "
           "prev=0x%X evidence=OBSERVED\n",
           g.gl_base, g.gl_size, g.t_a, g.t_b, g.t_c, g.raw_base_refine_pad, prev);
    fflush(stdout);
    scan_guest_regions("module_mapped");
}

void p22g_note_gamelist_started(void) {
    if (!p22g_enabled()) return;
    bump_init(g.init_state, "shell", "gamelist_started", "gamelist", 0, 0, 0, 0, 0, 0, 0, 0,
              "started");
    printf("[JJFB_P22G] gamelist_started evidence=OBSERVED\n");
    fflush(stdout);
}

void p22g_note_c_function_new(uint32_t helper, uint32_t p_len, uint32_t p_guest, uint32_t rw_base,
                              uint32_t rw_size, const char *origin) {
    if (!p22g_enabled()) return;
    if (!helper_is_gamelist(helper)) {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *m = reg ? module_registry_find_by_helper(reg, helper) : NULL;
        const char *mn = m ? (m->resolved_name[0] ? m->resolved_name : m->requested_name) : NULL;
        if (!is_gl(mn)) return;
    }
    g.gl_helper = helper;
    if (p_guest) {
        g.p_guest = p_guest;
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", p_guest);
    }
    if (rw_base) g.erw = rw_base;
    bump_init(INIT_HELPER_REGISTERED, origin ? origin : "host", "_mr_c_function_new", "gamelist",
              helper, helper, p_len, p_guest, rw_base, 0, 0, 0, "helper_registered");
    printf("[JJFB_P22G] c_function_new helper=0x%X P=0x%X erw=0x%X sz=0x%X origin=%s "
           "evidence=OBSERVED\n",
           helper, p_guest, rw_base, rw_size, origin ? origin : "?");
    fflush(stdout);
    scan_guest_regions("c_function_new");
}

void p22g_note_entry_begin(uint32_t helper, uint32_t method, uint32_t p_guest, uint32_t input,
                           uint32_t input_len, uint32_t er_rw, uint32_t sp) {
    HelperRow *h;
    if (!p22g_enabled() || !helper_is_gamelist(helper)) return;
    if (g.helper_n >= HELPER_CAP) return;
    ensure_files();
    h = &g.helpers[g.helper_n++];
    memset(h, 0, sizeof(*h));
    h->seq = ++g.seq;
    snprintf(h->caller_module, sizeof(h->caller_module), "%s", "entry_begin");
    h->helper_target = helper;
    h->method = method;
    h->r0 = p_guest;
    h->r1 = method;
    h->r2 = input;
    h->r3 = input_len;
    h->input = input;
    h->input_len = input_len;
    h->r9 = er_rw;
    snprintf(h->phase, sizeof(h->phase), "enter");
    (void)sp;
    if (g.helper_csv) {
        fprintf(g.helper_csv,
                "%u,%s,0x%X,0x%X,%u,0x%X,0x%X,0x%X,0x%X,0,0,0,0,0x%X,0x%X,0,0,0x%X,%s\n", h->seq,
                h->caller_module, h->caller_pc, h->helper_target, h->method, h->r0, h->r1, h->r2,
                h->r3, h->input, h->input_len, h->r9, h->phase);
        fflush(g.helper_csv);
    }
    if (!g.helper_invoke_n) {
        g.first_helper_caller_pc = 0;
        snprintf(g.first_helper_caller_mod, sizeof(g.first_helper_caller_mod), "entry_begin");
    }
    g.helper_invoke_n++;
    if (g.natural_method_n < 16) g.natural_methods[g.natural_method_n++] = method;
    if (method == 0) g.method_seen_mask |= 1u;
    if (method == 6) g.method_seen_mask |= 2u;
    if (method == 8) g.method_seen_mask |= 4u;
    bump_init(INIT_HELPER_INVOKED, "platform", "helper_enter", "gamelist", helper, p_guest, method,
              input, input_len, 0, er_rw, 0, "helper_enter");
    if (method == 0 || method == 6 || method == 8)
        bump_init(INIT_METHOD_COMPLETE, "platform", "helper_method", "gamelist", helper, p_guest,
                  method, input, input_len, 0, er_rw, 0, "method_seen");
    g.slice_armed = 1;
    g.slice_focus = helper;
    scan_guest_regions("helper_enter");
}

void p22g_note_helper_call(uint32_t helper, uint32_t method, int32_t ret_value) {
    HelperRow *h;
    int initish;
    if (!p22g_enabled() || !helper_is_gamelist(helper)) return;
    if (g.helper_n >= HELPER_CAP) return;
    ensure_files();
    h = &g.helpers[g.helper_n++];
    memset(h, 0, sizeof(*h));
    h->seq = ++g.seq;
    snprintf(h->caller_module, sizeof(h->caller_module), "%s", "helper_return");
    h->helper_target = helper;
    h->method = method;
    h->return_value = ret_value;
    snprintf(h->phase, sizeof(h->phase), "return");
    if (g.helper_csv) {
        fprintf(g.helper_csv,
                "%u,%s,0x%X,0x%X,%u,0,0,0,0,0,0,0,0,0,0,%d,0,0,%s\n", h->seq, h->caller_module,
                h->caller_pc, h->helper_target, h->method, h->return_value, h->phase);
        fflush(g.helper_csv);
    }
    /* Timer FIRE_EXT often returns via helper method 1/2 — not the 6→8→0 init sequence. */
    initish = (method == 0u || method == 6u || method == 8u);
    if (initish) {
        if (g.natural_method_n < 16) g.natural_methods[g.natural_method_n++] = method;
        if (method == 0) g.method_seen_mask |= 1u;
        if (method == 6) g.method_seen_mask |= 2u;
        if (method == 8) g.method_seen_mask |= 4u;
        g.helper_invoke_n++;
        bump_init(INIT_HELPER_INVOKED, "platform", "helper_return", "gamelist", helper, 0, method, 0,
                  0, 0, 0, ret_value, "helper_return_initish");
        bump_init(INIT_METHOD_COMPLETE, "platform", "helper_method", "gamelist", helper, 0, method,
                  0, 0, 0, 0, ret_value, "method_seen");
        scan_guest_regions("helper_return_initish");
    } else {
        bump_init(g.init_state, "platform", "helper_return", "gamelist", helper, 0, method, 0, 0, 0,
                  0, ret_value, "helper_return_non_init");
    }
}

void p22g_note_timer_fire(uint32_t helper, uint32_t p_guest, uint32_t erw, int end) {
    if (!p22g_enabled()) return;
    if (p_guest && !g.p_guest) {
        g.p_guest = p_guest;
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", p_guest);
    }
    if (erw) g.erw = erw;
    if (!end) return;
    g.fire_ext_n++;
    scan_guest_regions("FIRE_EXT");
    /* idle detection */
    if (g.idle_prev_state == g.farthest)
        g.idle_check_n++;
    else {
        g.idle_check_n = 0;
        g.idle_prev_state = g.farthest;
    }
    if (g.fire_ext_n >= FIRE_STOP) {
        maybe_stop("fire_ext_n20");
        return;
    }
    if (g.idle_check_n >= IDLE_STABLE_N && g.fire_ext_n >= 8u) maybe_stop("stable_idle");
    (void)helper;
}

void p22g_note_plat(uint32_t code, uint32_t app, uint32_t arg2, uint32_t arg3, uint32_t ret,
                    uint32_t caller_pc, uint32_t r9) {
    uint32_t cands[4];
    int i;
    if (!p22g_enabled() || !g.gl_base) return;
    cands[0] = app;
    cands[1] = arg2;
    cands[2] = arg3;
    cands[3] = ret;
    for (i = 0; i < 4; i++) {
        if (match_target(cands[i], NULL, NULL)) {
            char ch[32];
            snprintf(ch, sizeof(ch), "plat_0x%X_arg%d", code, i);
            record_write(caller_pc, "platform", 0, cands[i], ch, i, ch);
            /* Class E candidate */
            if (!g.verdict_class[0]) {
                snprintf(g.verdict_class, sizeof(g.verdict_class), "E");
                snprintf(g.missing_contract, sizeof(g.missing_contract),
                         "platform API 0x%X arg%d carried callback — prior probe may have missed",
                         code, i);
            }
        }
    }
    scan_guest_regions("plat_reg_return");
    (void)r9;
}

void p22g_note_memcpy(uint32_t dst, uint32_t src, uint32_t n, uint32_t caller_pc) {
#ifdef GWY_HAVE_UNICORN
    uint8_t *buf;
    uint32_t off, take;
    if (!p22g_enabled() || !g.gl_base || !g.uc || !n) return;
    take = n;
    if (take > 0x4000u) take = 0x4000u;
    buf = (uint8_t *)malloc(take);
    if (!buf) return;
    if (guest_memory_uc_peek((struct uc_struct *)g.uc, src, buf, take)) {
        for (off = 0; off + 4 <= take; off += 4) {
            uint32_t w = (uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8) |
                         ((uint32_t)buf[off + 2] << 16) | ((uint32_t)buf[off + 3] << 24);
            if (match_target(w, NULL, NULL)) record_copy(caller_pc, src, dst, n, off, w, "memcpy");
        }
    }
    free(buf);
#else
    (void)dst;
    (void)src;
    (void)n;
    (void)caller_pc;
#endif
}

void p22g_note_mem_write(uint32_t pc, uint32_t addr, uint32_t size, uint32_t value,
                         const char *module) {
    if (!p22g_enabled() || size < 4) return;
    record_write(pc, module, addr, value, "note_mem_write", -1, "host_or_obs");
}

void p22g_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr) {
    uint32_t norm, off;
    if (!p22g_enabled() || g.finalized) return;
    if (!g.uc) g.uc = uc;
    if (!is_gl(module_name)) {
        /* still watch for stores of target pointers from any module */
        if (g.gl_base && regs) {
            int ri;
            for (ri = 0; ri < 13; ri++) {
                if (match_target(regs[ri], NULL, NULL)) {
                    observe_str_and_indirect(uc, pc, regs, lr, sp, cpsr,
                                             module_name ? module_name : "?");
                    break;
                }
            }
        }
        return;
    }
    if (!g.gl_base) {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *m = reg ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
        if (m && m->map.guest_code_base)
            p22g_note_module_map(module_name, m->map.guest_code_base, m->map.guest_code_size,
                                 m->data.start_of_er_rw, 0, 0, m->module_id, "gwy/gamelist.mrp");
    }
    if (!g.gl_base) return;
    norm = pc & ~1u;
    if (norm < g.gl_base || norm >= g.gl_end) return;
    off = norm - g.gl_base;
    g.gl_insn_n++;

    if (off == OFF_F670) {
        g.entered_f670 = 1;
        bump_init(INIT_CALLBACK_INVOKED, "guest", "+0xF670", "gamelist", pc, regs[0], regs[1],
                  regs[2], regs[3], lr, regs[9], 0, "entered");
    } else if (off == OFF_8CDC) {
        g.entered_8cdc = 1;
        bump_init(INIT_CALLBACK_INVOKED, "guest", "+0x8CDC", "gamelist", pc, regs[0], regs[1],
                  regs[2], regs[3], lr, regs[9], 0, "entered");
    } else if (off == OFF_D978) {
        g.entered_d978 = 1;
        bump_init(INIT_CALLBACK_INVOKED, "guest", "+0xD978", "gamelist", pc, regs[0], regs[1],
                  regs[2], regs[3], lr, regs[9], 0, "entered");
    } else if (off == OFF_HEADER_CAND || norm == g.header_cand) {
        bump_init(INIT_HEADER_ENTRY_CALLED, "guest", "header_entry_cand", "gamelist", pc, regs[0],
                  regs[1], regs[2], regs[3], lr, regs[9], 0, "header_cand_pc");
    }

    observe_str_and_indirect(uc, pc, regs, lr, sp, cpsr, module_name);

    if ((g.gl_insn_n % 500000u) == 0u) scan_guest_regions("periodic");

    if (g.gl_insn_n >= GL_INSN_STOP) maybe_stop("gl_insn_10M");
    if (now_ms() >= 240000ull) maybe_stop("timeout_240s");
}

void p22g_finalize(const char *stop_reason) {
    const char *sum_path;
    if (!p22g_enabled() || g.finalized) return;
    g.finalized = 1;
    if (stop_reason && stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", stop_reason);
    else if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "finalize");

    refresh_identity();
    maybe_sha_runtime();
    scan_guest_regions("finalize");
    classify();
    write_missing_row();
    flush_life_csv();
    write_verdict();

    sum_path = env_or("JJFB_P22G_SUMMARY", "out/p22g/p22g_runtime_summary.txt");
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
                "ERW=0x%X\n"
                "P=%s\n"
                "generation=%s\n"
                "module_id=%s\n"
                "package_owner=%s\n"
                "class=%s\n"
                "farthest=%s\n"
                "missing_transition=%s\n"
                "missing_producer=%s\n"
                "missing_contract=%s\n"
                "wrote_f670=%d\n"
                "wrote_8cdc=%d\n"
                "wrote_d978=%d\n"
                "indir_f670=%d\n"
                "indir_8cdc=%d\n"
                "indir_d978=%d\n"
                "helper_invoke_n=%u\n"
                "fire_ext_n=%u\n"
                "gl_insn_n=%u\n"
                "write_n=%u\n"
                "table_n=%u\n"
                "stop_reason=%s\n"
                "guest_state_written=0\n"
                "events_injected=0\n"
                "headless=0\n"
                "fast_init=0\n",
                g.run_id, g.source_commit, g.main_exe_sha, g.raw_ext_sha,
                g.runtime_sha[0] ? g.runtime_sha : "UNKNOWN_NOT_EXPOSED", g.gl_base, g.gl_end,
                g.gl_size, g.erw, g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
                g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
                g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED",
                g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED", g.verdict_class,
                init_name(g.farthest), g.missing_transition, g.missing_producer, g.missing_contract,
                g.wrote_f670, g.wrote_8cdc, g.wrote_d978, g.indir_f670, g.indir_8cdc, g.indir_d978,
                g.helper_invoke_n, g.fire_ext_n, g.gl_insn_n, g.write_n, g.table_n, g.stop_reason);
        fflush(g.summary_txt);
        fclose(g.summary_txt);
        g.summary_txt = NULL;
    }

    printf("[JJFB_P22G_FINAL] class=%s farthest=%s missing=%s fire=%u insn=%u writes=%u "
           "helpers=%u evidence=OBSERVED\n",
           g.verdict_class, init_name(g.farthest), g.missing_transition, g.fire_ext_n, g.gl_insn_n,
           g.write_n, g.helper_invoke_n);
    fflush(stdout);
}
