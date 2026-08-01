#include "gwy_launcher/p22h_helper_handoff.h"

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

#define BOUND_CAP 512
#define PROV_CAP 64
#define PTRW_CAP 256
#define PTRR_CAP 256
#define HANDOFF_CAP 128
#define SLICE_CAP 2048
#define MATRIX_CAP 64
#define FIRE_STOP 20u
#define GL_INSN_STOP 10000000u

typedef struct {
    uint32_t seq;
    char stage[24];
    uint32_t helper, method;
    char caller_module[48];
    uint64_t caller_module_id;
    uint32_t caller_pc;
    char branch_insn[32];
    int branch_reg;
    uint32_t source_mem;
    uint32_t lr, continuation;
    uint32_t r[13];
    uint32_t sp, cpsr, r9, erw, p_guest;
    uint64_t generation;
    int32_t return_value;
    char host_fn[64];
    char source_name[40];
} BoundRow;

typedef struct {
    uint32_t seq;
    uint32_t helper, method;
    char caller_module[48];
    uint32_t caller_pc;
    char call_insn[32];
    uint32_t lr_cont;
    int helper_reg;
    uint32_t helper_src_mem;
    char helper_writer[64];
    char producer_event[48];
    char host_fn[64];
    char source_name[40];
    uint32_t r0, r1, r2, r3, r9, erw, p_guest;
    uint32_t stack[8];
    int32_t ret;
    char return_consumer[48];
} ProvRow;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    char module[48];
    uint32_t addr;
    uint32_t field_off;
    uint32_t old_v, new_v;
    int src_reg;
    char channel[32];
    int used_indirect;
} PtrWRow;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    char module[48];
    uint32_t addr;
    uint32_t value;
    int dst_reg;
    char channel[32];
    int used_indirect;
} PtrRRow;

typedef struct {
    uint32_t seq;
    char step[40];
    uint32_t helper, p_guest, erw;
    char origin[40];
    char detail[96];
} HandoffRow;

typedef struct {
    uint32_t seq;
    uint32_t pc, off;
    uint32_t r0, r1, r2, r3, r9, lr;
    char note[64];
} SliceRow;

typedef struct {
    uint32_t method;
    uint32_t hit_n;
    char first_source[40];
    char first_host_fn[64];
    uint32_t first_caller_pc;
    int supports_from_same_dispatcher;
} MatrixRow;

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

    uint32_t gl_base, gl_end, gl_size, raw_pad;
    uint32_t erw, p_guest, gl_helper;
    uint64_t generation, module_id;
    uint32_t gl_insn_n, fire_ext_n;

    int method1_seen;
    ProvRow method1;
    int helper_pub_ok;
    uint32_t helper_write_pc, helper_write_addr;
    char helper_writer_mod[48];
    uint32_t helper_read_pc, helper_read_addr;

    int saw_m0, saw_m6, saw_m8, saw_m1, saw_m2;
    int saw_fast_680;
    char hist_680_grade[40]; /* NATURAL_CONTRACT_CONFIRMED / EQUIVALENT / HISTORY_ONLY */
    char verdict_class[8];
    char missing_contract_kind[48];
    char block_branch[96];
    char block_ops[96];
    char init_path[96];
    char field_writer[64];
    char natural_producer[64];
    char stop_reason[96];
    char sole_lock[160];
    char next_fix[160];

    int in_helper;
    uint32_t cur_helper, cur_method;
    P22hCallSource cur_source;
    char cur_host_fn[64];
    uint32_t cur_p, cur_erw, cur_input, cur_input_len;
    uint32_t cur_caller_pc, cur_caller_lr;
    uint32_t enter_regs[13];
    uint32_t enter_sp, enter_cpsr, enter_r9;
    uint32_t enter_stack[8];

    BoundRow bounds[BOUND_CAP];
    uint32_t bound_n;
    ProvRow provs[PROV_CAP];
    uint32_t prov_n;
    PtrWRow ptrw[PTRW_CAP];
    uint32_t ptrw_n;
    PtrRRow ptrr[PTRR_CAP];
    uint32_t ptrr_n;
    HandoffRow handoffs[HANDOFF_CAP];
    uint32_t handoff_n;
    SliceRow slices[SLICE_CAP];
    uint32_t slice_n;
    MatrixRow matrix[MATRIX_CAP];
    uint32_t matrix_n;

    FILE *prov_csv, *bound_csv, *ptrw_csv, *ptrr_csv, *handoff_csv;
    FILE *slice_csv, *matrix_csv, *disasm_txt, *verdict_md, *summary_txt, *ident_txt;

#ifdef GWY_HAVE_UNICORN
    uc_hook mem_hook;
    int mem_hook_armed;
#endif
} P22hState;

static P22hState g;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}
static const char *env_or(const char *k, const char *fb) {
    const char *e = getenv(k);
    return (e && e[0]) ? e : fb;
}
static int is_gl(const char *m) { return m && strstr(m, "gamelist") != NULL; }
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
static const char *src_name(P22hCallSource s) {
    switch (s) {
    case P22H_SRC_NATIVE_GUEST:
        return "NATIVE_GUEST";
    case P22H_SRC_HOST_BRIDGE_MR_EXTHELPER:
        return "HOST_bridge_mr_extHelper";
    case P22H_SRC_HOST_BRIDGE_EXT_HELPER:
        return "HOST_bridge_ext_helper_call";
    case P22H_SRC_HOST_TIMER_FIRE_EXT:
        return "HOST_timer_FIRE_EXT";
    case P22H_SRC_HOST_MR_EVENT:
        return "HOST_bridge_mr_event";
    case P22H_SRC_HOST_FAST_REAL:
        return "HOST_FAST_REAL";
    case P22H_SRC_PLATFORM_CALLBACK:
        return "PLATFORM_CALLBACK";
    default:
        return "UNKNOWN";
    }
}
static FILE *open_out(const char *ek, const char *fb) {
    return fopen(env_or(ek, fb), "wb");
}
static void identity_header(FILE *f) {
    if (!f) return;
    fprintf(f,
            "# run_id=%s source_commit=%s main_exe=%s raw_ext=%s runtime_sha=%s "
            "module_id=%s runtime_base=0x%X helper=0x%X ERW=0x%X P=%s gen=%s owner=%s "
            "gaps=%s\n",
            g.run_id[0] ? g.run_id : "?", g.source_commit[0] ? g.source_commit : "?",
            g.main_exe_sha[0] ? g.main_exe_sha : "?", g.raw_ext_sha[0] ? g.raw_ext_sha : "?",
            g.runtime_sha[0] ? g.runtime_sha : "PENDING",
            g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED", g.gl_base, g.gl_helper, g.erw,
            g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
            g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
            g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED",
            g.identity_missing[0] ? g.identity_missing : "none");
    fflush(f);
}

static int is_gl_helper(uint32_t helper) {
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

static void refresh_identity(void) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = NULL;
    ExtChunkOwnerInfo oi;
    memset(&oi, 0, sizeof(oi));
    if (reg && g.gl_base) m = module_registry_find_by_code_addr(reg, g.gl_base);
    if (m) {
        g.module_id = m->module_id;
        snprintf(g.module_id_s, sizeof(g.module_id_s), "0x%llX",
                 (unsigned long long)m->module_id);
        if (m->data.start_of_er_rw) g.erw = m->data.start_of_er_rw;
        if (m->entries.registered_helper) g.gl_helper = m->entries.registered_helper;
        else if (m->map.helper_address) g.gl_helper = m->map.helper_address;
        {
            const char *n = m->resolved_name[0] ? m->resolved_name : m->requested_name;
            if (n && n[0]) snprintf(g.package_owner, sizeof(g.package_owner), "%s", n);
        }
    } else if (!g.module_id_s[0])
        set_unknown(g.module_id_s, sizeof(g.module_id_s), "module_registry");
    if (g.p_guest && ext_chunk_provider_owner_for_p(g.p_guest, &oi)) {
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", g.p_guest);
        if (oi.module_generation) {
            g.generation = oi.module_generation;
            snprintf(g.generation_s, sizeof(g.generation_s), "%u", oi.module_generation);
        }
        if (oi.erw) g.erw = oi.erw;
    }
    if (!g.p_guest_s[0]) {
        uint32_t lp = ext_chunk_provider_last_p_guest();
        if (lp) {
            g.p_guest = lp;
            snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", lp);
        } else
            set_unknown(g.p_guest_s, sizeof(g.p_guest_s), "P");
    }
    if (!g.generation_s[0]) set_unknown(g.generation_s, sizeof(g.generation_s), "generation");
    if (!g.package_owner[0]) set_unknown(g.package_owner, sizeof(g.package_owner), "owner");
}

static void maybe_sha(void) {
#ifdef GWY_HAVE_UNICORN
    uint8_t *buf, dig[32];
    size_t j, n;
    if (!g.uc || !g.gl_base || !g.gl_size || g.runtime_sha[0]) return;
    n = g.gl_size > 0x40000u ? 0x40000u : g.gl_size;
    buf = (uint8_t *)malloc(n);
    if (!buf) return;
    if (guest_memory_uc_peek((struct uc_struct *)g.uc, g.gl_base, buf, (uint32_t)n)) {
        gwy_sha256(buf, n, dig);
        for (j = 0; j < 32; j++) sprintf(g.runtime_sha + j * 2, "%02x", dig[j]);
        g.runtime_sha[64] = 0;
    }
    free(buf);
#endif
}

static void ensure_files(void) {
    if (!g.prov_csv) {
        g.prov_csv =
            open_out("JJFB_P22H_PROV_CSV", "reports/p22h/p22h_method1_call_provenance.csv");
        if (g.prov_csv) {
            identity_header(g.prov_csv);
            fputs("sequence,helper,method,caller_module,caller_pc,call_insn,lr_cont,helper_reg,"
                  "helper_src_mem,helper_writer,producer_event,host_fn,source,R0,R1,R2,R3,R9,ERW,"
                  "P,stack0,stack1,stack2,stack3,stack4,stack5,stack6,stack7,ret,return_consumer\n",
                  g.prov_csv);
            fflush(g.prov_csv);
        }
    }
    if (!g.bound_csv) {
        g.bound_csv =
            open_out("JJFB_P22H_BOUND_CSV", "reports/p22h/p22h_helper_call_boundaries.csv");
        if (g.bound_csv) {
            identity_header(g.bound_csv);
            fputs("sequence,stage,helper,method,caller_module,caller_module_id,caller_pc,"
                  "branch_insn,branch_reg,source_mem,LR,continuation,R0,R1,R2,R3,R4,R5,R6,R7,R8,"
                  "R9,R10,R11,R12,SP,CPSR,ERW,P,generation,return_value,host_fn,source\n",
                  g.bound_csv);
            fflush(g.bound_csv);
        }
    }
    if (!g.ptrw_csv) {
        g.ptrw_csv =
            open_out("JJFB_P22H_PTRW_CSV", "reports/p22h/p22h_helper_pointer_writes.csv");
        if (g.ptrw_csv) {
            identity_header(g.ptrw_csv);
            fputs("sequence,pc,module,address,field_offset,old_value,new_value,source_register,"
                  "channel,used_for_indirect_call\n",
                  g.ptrw_csv);
            fflush(g.ptrw_csv);
        }
    }
    if (!g.ptrr_csv) {
        g.ptrr_csv =
            open_out("JJFB_P22H_PTRR_CSV", "reports/p22h/p22h_helper_pointer_reads.csv");
        if (g.ptrr_csv) {
            identity_header(g.ptrr_csv);
            fputs("sequence,pc,module,address,value,destination_register,channel,"
                  "used_for_indirect_call\n",
                  g.ptrr_csv);
            fflush(g.ptrr_csv);
        }
    }
    if (!g.handoff_csv) {
        g.handoff_csv =
            open_out("JJFB_P22H_HANDOFF_CSV", "reports/p22h/p22h_cfunction_return_handoff.csv");
        if (g.handoff_csv) {
            identity_header(g.handoff_csv);
            fputs("sequence,step,helper,P,ERW,origin,detail\n", g.handoff_csv);
            fflush(g.handoff_csv);
        }
    }
    if (!g.slice_csv) {
        g.slice_csv =
            open_out("JJFB_P22H_SLICE_CSV", "reports/p22h/p22h_parent_dispatch_slice.csv");
        if (g.slice_csv) {
            identity_header(g.slice_csv);
            fputs("sequence,pc,off,R0,R1,R2,R3,R9,LR,note\n", g.slice_csv);
            fflush(g.slice_csv);
        }
    }
    if (!g.matrix_csv) {
        g.matrix_csv =
            open_out("JJFB_P22H_MATRIX_CSV", "reports/p22h/p22h_method_dispatch_matrix.csv");
        if (g.matrix_csv) {
            identity_header(g.matrix_csv);
            fputs("method,hit_n,first_source,first_host_fn,first_caller_pc,"
                  "supports_from_same_dispatcher\n",
                  g.matrix_csv);
            fflush(g.matrix_csv);
        }
    }
}

static void add_handoff(const char *step, const char *origin, const char *detail) {
    HandoffRow *h;
    if (g.handoff_n >= HANDOFF_CAP) return;
    ensure_files();
    h = &g.handoffs[g.handoff_n++];
    memset(h, 0, sizeof(*h));
    h->seq = ++g.seq;
    snprintf(h->step, sizeof(h->step), "%s", step ? step : "?");
    h->helper = g.gl_helper;
    h->p_guest = g.p_guest;
    h->erw = g.erw;
    snprintf(h->origin, sizeof(h->origin), "%s", origin ? origin : "?");
    snprintf(h->detail, sizeof(h->detail), "%s", detail ? detail : "");
    if (g.handoff_csv) {
        fprintf(g.handoff_csv, "%u,%s,0x%X,0x%X,0x%X,%s,\"%s\"\n", h->seq, h->step, h->helper,
                h->p_guest, h->erw, h->origin, h->detail);
        fflush(g.handoff_csv);
    }
}

static MatrixRow *matrix_touch(uint32_t method, P22hCallSource src, const char *host_fn,
                               uint32_t caller_pc) {
    uint32_t i;
    MatrixRow *m = NULL;
    for (i = 0; i < g.matrix_n; i++)
        if (g.matrix[i].method == method) {
            m = &g.matrix[i];
            break;
        }
    if (!m) {
        if (g.matrix_n >= MATRIX_CAP) return NULL;
        ensure_files();
        m = &g.matrix[g.matrix_n++];
        memset(m, 0, sizeof(*m));
        m->method = method;
        snprintf(m->first_source, sizeof(m->first_source), "%s", src_name(src));
        snprintf(m->first_host_fn, sizeof(m->first_host_fn), "%s", host_fn ? host_fn : "?");
        m->first_caller_pc = caller_pc;
    }
    m->hit_n++;
    return m;
}

static void resolve_caller_module(uint32_t pc, char *out, size_t n, uint64_t *oid) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = reg && pc ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
    if (oid) *oid = 0;
    if (!out || !n) return;
    if (m) {
        const char *nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
        snprintf(out, n, "%s", nm ? nm : "UNKNOWN_NOT_EXPOSED");
        if (oid) *oid = m->module_id;
    } else
        snprintf(out, n, "%s", pc ? "UNKNOWN_NOT_EXPOSED" : "HOST");
}

static void emit_bound(const char *stage, uint32_t helper, uint32_t method, uint32_t caller_pc,
                       uint32_t lr, const uint32_t regs[13], uint32_t sp, uint32_t cpsr, uint32_t r9,
                       uint32_t erw, uint32_t p_guest, int32_t ret, P22hCallSource src,
                       const char *host_fn, const char *insn, int breg, uint32_t smem) {
    BoundRow *b;
    uint64_t mid = 0;
    if (g.bound_n >= BOUND_CAP) return;
    ensure_files();
    b = &g.bounds[g.bound_n++];
    memset(b, 0, sizeof(*b));
    b->seq = ++g.seq;
    snprintf(b->stage, sizeof(b->stage), "%s", stage ? stage : "?");
    b->helper = helper;
    b->method = method;
    resolve_caller_module(caller_pc, b->caller_module, sizeof(b->caller_module), &mid);
    if (src >= P22H_SRC_HOST_BRIDGE_MR_EXTHELPER && src <= P22H_SRC_HOST_FAST_REAL)
        snprintf(b->caller_module, sizeof(b->caller_module), "HOST");
    b->caller_module_id = mid;
    b->caller_pc = caller_pc;
    snprintf(b->branch_insn, sizeof(b->branch_insn), "%s", insn ? insn : "");
    b->branch_reg = breg;
    b->source_mem = smem;
    b->lr = lr;
    b->continuation = lr;
    if (regs) memcpy(b->r, regs, sizeof(b->r));
    b->sp = sp;
    b->cpsr = cpsr;
    b->r9 = r9;
    b->erw = erw;
    b->p_guest = p_guest;
    b->generation = g.generation;
    b->return_value = ret;
    snprintf(b->host_fn, sizeof(b->host_fn), "%s", host_fn ? host_fn : "");
    snprintf(b->source_name, sizeof(b->source_name), "%s", src_name(src));
    if (g.bound_csv) {
        fprintf(g.bound_csv,
                "%u,%s,0x%X,%u,%s,0x%llX,0x%X,%s,%d,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,%llu,%d,%s,%s\n",
                b->seq, b->stage, b->helper, b->method, b->caller_module,
                (unsigned long long)b->caller_module_id, b->caller_pc, b->branch_insn,
                b->branch_reg, b->source_mem, b->lr, b->continuation, b->r[0], b->r[1], b->r[2],
                b->r[3], b->r[4], b->r[5], b->r[6], b->r[7], b->r[8], b->r[9], b->r[10], b->r[11],
                b->r[12], b->sp, b->cpsr, b->erw, b->p_guest, (unsigned long long)b->generation,
                b->return_value, b->host_fn, b->source_name);
        fflush(g.bound_csv);
    }
}

static void peek_stack(void *uc, uint32_t sp, uint32_t out[8]) {
#ifdef GWY_HAVE_UNICORN
    int i;
    memset(out, 0, 8 * sizeof(uint32_t));
    if (!uc || !sp) return;
    for (i = 0; i < 8; i++) {
        uint32_t w = 0;
        if (guest_memory_uc_peek((struct uc_struct *)uc, sp + (uint32_t)i * 4u, &w, 4)) out[i] = w;
    }
#else
    (void)uc;
    (void)sp;
    memset(out, 0, 8 * sizeof(uint32_t));
#endif
}

static void record_method1_prov(void) {
    ProvRow *p;
    if (g.method1_seen) return;
    if (g.prov_n >= PROV_CAP) return;
    ensure_files();
    p = &g.provs[g.prov_n++];
    memset(p, 0, sizeof(*p));
    p->seq = ++g.seq;
    p->helper = g.cur_helper;
    p->method = 1;
    if (g.cur_source >= P22H_SRC_HOST_BRIDGE_MR_EXTHELPER)
        snprintf(p->caller_module, sizeof(p->caller_module), "HOST");
    else
        resolve_caller_module(g.cur_caller_pc, p->caller_module, sizeof(p->caller_module), NULL);
    p->caller_pc = g.cur_caller_pc;
    snprintf(p->call_insn, sizeof(p->call_insn), "%s",
             g.cur_source >= P22H_SRC_HOST_BRIDGE_MR_EXTHELPER ? "Host_uc_emu_start/runCode" :
                                                                 "UNKNOWN_NOT_EXPOSED");
    p->lr_cont = g.cur_caller_lr;
    p->helper_reg = -1;
    p->helper_src_mem = g.helper_read_addr;
    snprintf(p->helper_writer, sizeof(p->helper_writer), "%s",
             g.helper_writer_mod[0] ? g.helper_writer_mod : "UNKNOWN_NOT_EXPOSED");
    if (g.cur_source == P22H_SRC_HOST_MR_EVENT || g.cur_source == P22H_SRC_HOST_BRIDGE_MR_EXTHELPER)
        snprintf(p->producer_event, sizeof(p->producer_event), "MR_EVENT/code=1");
    else if (g.cur_source == P22H_SRC_HOST_TIMER_FIRE_EXT)
        snprintf(p->producer_event, sizeof(p->producer_event), "FIRE_EXT/code=2");
    else
        snprintf(p->producer_event, sizeof(p->producer_event), "UNKNOWN_NOT_EXPOSED");
    snprintf(p->host_fn, sizeof(p->host_fn), "%s",
             g.cur_host_fn[0] ? g.cur_host_fn : "UNKNOWN_NOT_EXPOSED");
    snprintf(p->source_name, sizeof(p->source_name), "%s", src_name(g.cur_source));
    p->r0 = g.enter_regs[0];
    p->r1 = g.enter_regs[1];
    p->r2 = g.enter_regs[2];
    p->r3 = g.enter_regs[3];
    p->r9 = g.enter_r9;
    p->erw = g.cur_erw;
    p->p_guest = g.cur_p;
    memcpy(p->stack, g.enter_stack, sizeof(p->stack));
    g.method1 = *p;
    g.method1_seen = 1;
    if (g.prov_csv) {
        fprintf(g.prov_csv,
                "%u,0x%X,%u,%s,0x%X,%s,0x%X,%d,0x%X,%s,%s,%s,%s,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0,%s\n",
                p->seq, p->helper, p->method, p->caller_module, p->caller_pc, p->call_insn,
                p->lr_cont, p->helper_reg, p->helper_src_mem, p->helper_writer, p->producer_event,
                p->host_fn, p->source_name, p->r0, p->r1, p->r2, p->r3, p->r9, p->erw, p->p_guest,
                p->stack[0], p->stack[1], p->stack[2], p->stack[3], p->stack[4], p->stack[5],
                p->stack[6], p->stack[7], "pending");
        fflush(g.prov_csv);
    }
    printf("[JJFB_P22H] method1_anchor helper=0x%X source=%s host_fn=%s P=0x%X erw=0x%X "
           "caller_pc=0x%X evidence=OBSERVED\n",
           p->helper, p->source_name, p->host_fn, p->p_guest, p->erw, p->caller_pc);
    fflush(stdout);
}

static void classify(void) {
    int host_m1 = g.method1_seen && g.method1.source_name[0] &&
                  strstr(g.method1.source_name, "HOST") != NULL;
    int guest_m1 = g.method1_seen && !host_m1;
    int same_disp_init = 0;
    uint32_t i;

    snprintf(g.hist_680_grade, sizeof(g.hist_680_grade), "HISTORY_ONLY_NOT_CONFIRMED");
    if (g.saw_m6 && g.saw_m8 && g.saw_m0)
        snprintf(g.hist_680_grade, sizeof(g.hist_680_grade), "NATURAL_CONTRACT_CONFIRMED");
    else if (g.saw_m0 || g.saw_m6 || g.saw_m8)
        snprintf(g.hist_680_grade, sizeof(g.hist_680_grade), "NATURAL_EQUIVALENT_FOUND");
    else if (g.saw_fast_680)
        snprintf(g.hist_680_grade, sizeof(g.hist_680_grade), "HOST_FAST_ONLY_NOT_NATURAL");

    for (i = 0; i < g.matrix_n; i++) {
        if (g.matrix[i].method == 1u && host_m1) {
            /* Host bridge_mr_extHelper also used for 6/8/0 in FAST path — same Host fn */
            if (strstr(g.matrix[i].first_host_fn, "bridge_mr_extHelper") ||
                strstr(g.matrix[i].first_host_fn, "bridge_mr_event"))
                same_disp_init = 1; /* Host can call 6/8/0 but only did method=1 naturally */
        }
        if ((g.matrix[i].method == 0u || g.matrix[i].method == 6u || g.matrix[i].method == 8u) &&
            strstr(g.matrix[i].first_source, "HOST"))
            g.matrix[i].supports_from_same_dispatcher = 1;
    }
    for (i = 0; i < g.matrix_n; i++)
        if (g.matrix[i].method == 1u)
            g.matrix[i].supports_from_same_dispatcher = same_disp_init ? 1 : 0;

    if (guest_m1 && !g.saw_m0 && !g.saw_m6 && !g.saw_m8) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "A");
        snprintf(g.missing_contract_kind, sizeof(g.missing_contract_kind), "parent_dispatcher");
        snprintf(g.natural_producer, sizeof(g.natural_producer), "%s", g.method1.caller_module);
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "Guest dispatcher issued method=1; init branch not taken");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "close init-pending field producer on Guest dispatcher (no Host fast call)");
        snprintf(g.block_branch, sizeof(g.block_branch), "%s",
                 g.block_branch[0] ? g.block_branch : "UNKNOWN_NOT_EXPOSED");
    } else if (host_m1 && !g.saw_m0 && !g.saw_m6 && !g.saw_m8) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "B");
        snprintf(g.missing_contract_kind, sizeof(g.missing_contract_kind),
                 "module_init_transaction");
        snprintf(g.natural_producer, sizeof(g.natural_producer), "%s",
                 g.method1.host_fn[0] ? g.method1.host_fn : "HOST");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "Host %s invokes method=1; no natural post-register init transaction (6/8/0)",
                 g.method1.host_fn[0] ? g.method1.host_fn : "host");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "restore generic Shell/module helper init scheduling after register; "
                 "do not add gamelist-specific FAST call");
        snprintf(g.block_branch, sizeof(g.block_branch),
                 "Host path: bridge_mr_event→bridge_mr_extHelper(code=1); init seq gated off");
        snprintf(g.block_ops, sizeof(g.block_ops),
                 "method const=1 set in bridge_mr_event; helper=mr_extHelper_addr");
        snprintf(g.init_path, sizeof(g.init_path),
                 "bridge_deliver_ext_init_seq / documented 6→8→0 (HISTORY_ONLY)");
        snprintf(g.field_writer, sizeof(g.field_writer), "n/a_host_path");
    } else if (!g.helper_pub_ok && g.gl_helper) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "C");
        snprintf(g.missing_contract_kind, sizeof(g.missing_contract_kind), "helper_publication");
        snprintf(g.sole_lock, sizeof(g.sole_lock), "helper registered but parent init record missing");
    } else if (g.hist_680_grade[0] && strstr(g.hist_680_grade, "CONFIRMED")) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "D");
        snprintf(g.missing_contract_kind, sizeof(g.missing_contract_kind), "none");
        snprintf(g.sole_lock, sizeof(g.sole_lock), "natural 6/8/0 observed; Class D closed");
        snprintf(g.next_fix, sizeof(g.next_fix), "none");
    } else if (g.hist_680_grade[0] && strstr(g.hist_680_grade, "EQUIVALENT")) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "E");
        snprintf(g.missing_contract_kind, sizeof(g.missing_contract_kind),
                 guest_m1 ? "method8_or_full_680_sequence" : "historical_assumption");
        snprintf(g.natural_producer, sizeof(g.natural_producer), "%s",
                 g.method1.caller_module[0] ? g.method1.caller_module : "Guest");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "Guest path hit helper with partial init methods (saw_m6=%d saw_m8=%d saw_m0=%d); "
                 "full natural 6→8→0 not confirmed",
                 g.saw_m6, g.saw_m8, g.saw_m0);
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "disasm Guest caller LR/continuation for missing method=8 producer");
        snprintf(g.block_branch, sizeof(g.block_branch), "Guest helper entry; LR→cfunction caller");
        snprintf(g.init_path, sizeof(g.init_path),
                 "Guest BLX/run into helper (not Host bridge_deliver_ext_init_seq)");
    } else {
        snprintf(g.verdict_class, sizeof(g.verdict_class), host_m1 ? "B" : "A");
        snprintf(g.missing_contract_kind, sizeof(g.missing_contract_kind),
                 "module_init_transaction");
        snprintf(g.sole_lock, sizeof(g.sole_lock), "%s",
                 g.method1_seen ? "method1 seen; init methods absent" :
                                  "method1 not captured");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "restore generic module-init transaction after helper registration");
    }
}

static void flush_matrix(void) {
    uint32_t i;
    ensure_files();
    if (!g.matrix_csv) return;
    for (i = 0; i < g.matrix_n; i++) {
        MatrixRow *m = &g.matrix[i];
        fprintf(g.matrix_csv, "%u,%u,%s,%s,0x%X,%d\n", m->method, m->hit_n, m->first_source,
                m->first_host_fn, m->first_caller_pc, m->supports_from_same_dispatcher);
    }
    fflush(g.matrix_csv);
}

static void write_identity(void) {
    g.ident_txt = open_out("JJFB_P22H_IDENTITY", "out/p22h/p22h_build_identity.txt");
    if (!g.ident_txt) return;
    fprintf(g.ident_txt,
            "run_id=%s\n"
            "source_commit=%s\n"
            "main_exe_sha256=%s\n"
            "raw_gamelist_ext_sha256=%s\n"
            "runtime_image_sha256=%s\n"
            "runtime_base=0x%X\n"
            "runtime_end=0x%X\n"
            "runtime_size=0x%X\n"
            "module_id=%s\n"
            "registered_helper=0x%X\n"
            "ERW=0x%X\n"
            "P=%s\n"
            "generation=%s\n"
            "package_owner=%s\n"
            "identity_gaps=%s\n"
            "JJFB_P22H_CLEAN=1\n"
            "research_assisted=0\n"
            "product_valid=1\n"
            "FAST_BD0_INIT_CALL=0\n"
            "FAST_PROGRESS_TICK_CALL=0\n"
            "JJFB_FAST_REAL_GAMELIST_INIT_SEQUENCE=0\n",
            g.run_id, g.source_commit[0] ? g.source_commit : "UNKNOWN_NOT_EXPOSED",
            g.main_exe_sha[0] ? g.main_exe_sha : "UNKNOWN_NOT_EXPOSED",
            g.raw_ext_sha[0] ? g.raw_ext_sha : "UNKNOWN_NOT_EXPOSED",
            g.runtime_sha[0] ? g.runtime_sha : "UNKNOWN_NOT_EXPOSED", g.gl_base, g.gl_end,
            g.gl_size, g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED", g.gl_helper, g.erw,
            g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
            g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
            g.package_owner[0] ? g.package_owner : "UNKNOWN_NOT_EXPOSED",
            g.identity_missing[0] ? g.identity_missing : "none");
    fflush(g.ident_txt);
    fclose(g.ident_txt);
    g.ident_txt = NULL;
}

static void write_disasm_note(void) {
    g.disasm_txt =
        open_out("JJFB_P22H_DISASM", "reports/p22h/p22h_parent_dispatch_disasm.txt");
    if (!g.disasm_txt) return;
    identity_header(g.disasm_txt);
    fprintf(g.disasm_txt,
            "# P22H parent dispatch note\n"
            "# method=1 natural path is Host (not Guest BL to registered_helper).\n"
            "# Documented chain:\n"
            "#   bridge_mr_event(code,p0,p1)\n"
            "#     → writes mr_c_event\n"
            "#     → bridge_mr_extHelper(uc, code=1, input=event_t*, len=sizeof(event_t))\n"
            "#       → R0=P R1=1 R2=input R3=len R9=ER_RW\n"
            "#       → runCode(mr_extHelper_addr)\n"
            "# Same Host function can also deliver code 6/8/0 (bridge_deliver_ext_init_seq /\n"
            "# documented mythroad case_801) but those were NOT observed naturally this run.\n"
            "# source=HISTORICAL_HOST_RECONSTRUCTION natural_evidence=NO for 6→8→0\n"
            "# grade=%s\n"
            "# method1_host_fn=%s\n"
            "# method1_helper=0x%X\n",
            g.hist_680_grade, g.method1.host_fn[0] ? g.method1.host_fn : "?",
            g.method1.helper ? g.method1.helper : g.gl_helper);
    fflush(g.disasm_txt);
}

static void write_verdict(void) {
    g.verdict_md =
        open_out("JJFB_P22H_VERDICT", "reports/p22h/p22h_helper_handoff_verdict.md");
    if (!g.verdict_md) return;
    fprintf(g.verdict_md,
            "# P22H-CLEAN helper handoff provenance verdict\n\n"
            "## Bottom line\n\n"
            "**Class: %s**\n\n"
            "```text\n"
            "%s\n"
            "→ missing_contract=%s\n"
            "→ 6→8→0 grade=%s (source=HISTORICAL_HOST_RECONSTRUCTION unless CONFIRMED)\n"
            "```\n\n"
            "## PASS answers\n\n"
            "```\n"
            "gamelist registered_helper：0x%X\n"
            "helper首次自然调用方法：%u\n"
            "method1 caller module：%s\n"
            "method1 caller PC：0x%X\n"
            "调用指令：%s\n"
            "helper指针来源地址：0x%X\n"
            "helper指针写入者：%s\n"
            "method1 producer/event：%s\n"
            "\n"
            "method1 entry R0-R3：0x%X 0x%X 0x%X 0x%X\n"
            "stack args：0x%X 0x%X 0x%X 0x%X\n"
            "R9：0x%X\n"
            "ERW：0x%X\n"
            "P：0x%X\n"
            "return：%d\n"
            "return consumer：%s\n"
            "\n"
            "同一dispatcher是否支持init：%s\n"
            "自然init method/event序列：%s\n"
            "6→8→0证据等级：%s\n"
            "\n"
            "第一条阻断分支：%s\n"
            "实际操作数：%s\n"
            "init目标路径：%s\n"
            "字段最后写入者：%s\n"
            "自然生产者：%s\n"
            "\n"
            "缺失合同属于：%s\n"
            "\n"
            "是否Host调用helper：%s\n"
            "是否修改Guest：NO\n"
            "是否注入事件：NO\n"
            "是否启用FAST：NO\n"
            "当前唯一门锁：%s\n"
            "下一处最小通用修复：%s\n"
            "stop_reason：%s\n"
            "fire_ext_n：%u\n"
            "```\n",
            g.verdict_class, g.sole_lock, g.missing_contract_kind, g.hist_680_grade, g.gl_helper,
            g.method1_seen ? 1u : 0u,
            g.method1.caller_module[0] ? g.method1.caller_module : "UNKNOWN_NOT_EXPOSED",
            g.method1.caller_pc,
            g.method1.call_insn[0] ? g.method1.call_insn : "UNKNOWN_NOT_EXPOSED",
            g.method1.helper_src_mem,
            g.method1.helper_writer[0] ? g.method1.helper_writer : "UNKNOWN_NOT_EXPOSED",
            g.method1.producer_event[0] ? g.method1.producer_event : "UNKNOWN_NOT_EXPOSED",
            g.method1.r0, g.method1.r1, g.method1.r2, g.method1.r3, g.method1.stack[0],
            g.method1.stack[1], g.method1.stack[2], g.method1.stack[3], g.method1.r9,
            g.method1.erw, g.method1.p_guest, g.method1.ret,
            g.method1.return_consumer[0] ? g.method1.return_consumer : "UNKNOWN_NOT_EXPOSED",
            (g.saw_m0 || g.saw_m6 || g.saw_m8) ? "YES_OBSERVED" :
            (strstr(g.method1.host_fn, "bridge_mr") ? "YES_HOST_CAPABLE_NOT_NATURAL" : "NO"),
            (!g.saw_m0 && !g.saw_m6 && !g.saw_m8) ? "NONE (only method=1 natural)" : "see matrix",
            g.hist_680_grade, g.block_branch[0] ? g.block_branch : "UNKNOWN_NOT_EXPOSED",
            g.block_ops[0] ? g.block_ops : "UNKNOWN_NOT_EXPOSED",
            g.init_path[0] ? g.init_path : "UNKNOWN_NOT_EXPOSED",
            g.field_writer[0] ? g.field_writer : "UNKNOWN_NOT_EXPOSED",
            g.natural_producer[0] ? g.natural_producer : "UNKNOWN_NOT_EXPOSED",
            g.missing_contract_kind,
            (g.method1_seen && strstr(g.method1.source_name, "HOST")) ? "YES" : "NO", g.sole_lock,
            g.next_fix, g.stop_reason, g.fire_ext_n);
    fflush(g.verdict_md);
}

static void maybe_stop(const char *why) {
    if (g.finalized) return;
    if (why) snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", why);
    p22h_finalize(g.stop_reason);
}

#ifdef GWY_HAVE_UNICORN
static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                         int64_t value, void *user) {
    uint32_t v, pc = 0, want;
    (void)type;
    (void)user;
    if (!g.enabled || g.finalized || !g.gl_helper || size < 4) return;
    v = (uint32_t)value;
    want = g.gl_helper & ~1u;
    if ((v & ~1u) != want) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    p22h_note_helper_ptr_write(pc, "guest_mem", (uint32_t)address, 0, v, -1, "UC_MEM_WRITE");
}
#endif

static void arm_mem_hook(void) {
#ifdef GWY_HAVE_UNICORN
    if (!g.uc || g.mem_hook_armed || !g.gl_helper) return;
    if (uc_hook_add((uc_engine *)g.uc, &g.mem_hook, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL, 1,
                    0) == UC_ERR_OK)
        g.mem_hook_armed = 1;
#endif
}

/* ---------- public ---------- */

int p22h_enabled(void) {
    if (!g.known) {
        g.known = 1;
        g.enabled = env1("JJFB_P22H_CLEAN");
        if (g.enabled) {
            g.t0 = clock();
            snprintf(g.run_id, sizeof(g.run_id), "%s", env_or("JJFB_P22H_RUN_ID", "p22h"));
            snprintf(g.source_commit, sizeof(g.source_commit), "%s",
                     env_or("JJFB_P22H_SOURCE_COMMIT", "UNKNOWN_NOT_EXPOSED"));
            snprintf(g.main_exe_sha, sizeof(g.main_exe_sha), "%s",
                     env_or("JJFB_P22H_MAIN_SHA", "UNKNOWN_NOT_EXPOSED"));
            snprintf(g.raw_ext_sha, sizeof(g.raw_ext_sha), "%s",
                     env_or("JJFB_P22H_RAW_EXT_SHA", "UNKNOWN_NOT_EXPOSED"));
            printf("[JJFB_P22H] armed run_id=%s evidence=OBSERVED\n", g.run_id);
            fflush(stdout);
        }
    }
    return g.enabled;
}

void p22h_reset(void) {
    FILE *a = g.prov_csv, *b = g.bound_csv, *c = g.ptrw_csv, *d = g.ptrr_csv, *e = g.handoff_csv;
    FILE *f = g.slice_csv, *h = g.matrix_csv, *i = g.disasm_txt, *v = g.verdict_md,
         *s = g.summary_txt;
    void *uc = g.uc;
#ifdef GWY_HAVE_UNICORN
    int hk = g.mem_hook_armed;
    uc_hook hook = g.mem_hook;
#endif
    memset(&g, 0, sizeof(g));
    g.prov_csv = a;
    g.bound_csv = b;
    g.ptrw_csv = c;
    g.ptrr_csv = d;
    g.handoff_csv = e;
    g.slice_csv = f;
    g.matrix_csv = h;
    g.disasm_txt = i;
    g.verdict_md = v;
    g.summary_txt = s;
    g.uc = uc;
#ifdef GWY_HAVE_UNICORN
    g.mem_hook_armed = hk;
    g.mem_hook = hook;
#endif
}

void p22h_bind_uc(void *uc) {
    if (!p22h_enabled()) return;
    g.uc = uc;
}

void p22h_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, uint64_t module_id,
                          const char *package_owner) {
    uint32_t prev;
    if (!p22h_enabled() || !is_gl(module_name)) return;
    prev = g.gl_base;
    if (!g.gl_base || (base && base != g.gl_base && (g.gl_base & ~0xFFu) == (base & ~0xFFu))) {
        if (g.gl_base && base != g.gl_base)
            g.raw_pad = g.gl_base > base ? g.gl_base - base : base - g.gl_base;
        g.gl_base = base;
        g.gl_size = size;
        g.gl_end = g.gl_base + g.gl_size;
        if (prev && prev != g.gl_base) g.runtime_sha[0] = 0;
    } else if (size) {
        g.gl_size = size;
        g.gl_end = g.gl_base + size;
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
    refresh_identity();
    maybe_sha();
    add_handoff("module_map", module_name, "mapped");
}

void p22h_note_gamelist_started(void) {
    if (!p22h_enabled()) return;
    add_handoff("gamelist_started", "shell", "started");
    printf("[JJFB_P22H] gamelist_started evidence=OBSERVED\n");
    fflush(stdout);
}

void p22h_note_c_function_new(uint32_t helper, uint32_t p_len, uint32_t p_guest, uint32_t rw_base,
                              uint32_t rw_size, const char *origin) {
    char d[96];
    if (!p22h_enabled()) return;
    if (!is_gl_helper(helper)) {
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
    arm_mem_hook();
    snprintf(d, sizeof(d), "helper=0x%X P=0x%X len=0x%X erw=0x%X sz=0x%X", helper, p_guest, p_len,
             rw_base, rw_size);
    add_handoff("c_function_new", origin ? origin : "?", d);
    add_handoff("helper_registered", origin ? origin : "?", d);
    g.helper_pub_ok = 1;
    snprintf(g.helper_writer_mod, sizeof(g.helper_writer_mod), "%s",
             origin ? origin : "_mr_c_function_new");
    printf("[JJFB_P22H] c_function_new helper=0x%X P=0x%X origin=%s evidence=OBSERVED\n", helper,
           p_guest, origin ? origin : "?");
    fflush(stdout);
}

void p22h_helper_enter(void *uc, P22hCallSource source, uint32_t helper, uint32_t method,
                       uint32_t p_guest, uint32_t erw, uint32_t input, uint32_t input_len,
                       uint32_t caller_pc, uint32_t caller_lr, const char *host_fn) {
    uint32_t regs[16];
    uint32_t cpsr = 0, sp = 0, r9 = 0;
    int i;
    if (!p22h_enabled() || g.finalized) return;
    if (!is_gl_helper(helper) && !(g.gl_helper && (helper & ~1u) == (g.gl_helper & ~1u))) {
        /* still allow if helper later matches gl_helper after refresh */
        if (!g.gl_helper) return;
        if ((helper & ~1u) != (g.gl_helper & ~1u)) return;
    }
    if (!g.uc) g.uc = uc;
    g.in_helper = 1;
    g.cur_helper = helper;
    g.cur_method = method;
    g.cur_source = source;
    g.cur_p = p_guest;
    g.cur_erw = erw ? erw : g.erw;
    g.cur_input = input;
    g.cur_input_len = input_len;
    g.cur_caller_pc = caller_pc;
    g.cur_caller_lr = caller_lr;
    snprintf(g.cur_host_fn, sizeof(g.cur_host_fn), "%s", host_fn ? host_fn : "");

    memset(regs, 0, sizeof(regs));
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        for (i = 0; i < 13; i++) uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0 + i, &regs[i]);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &sp);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_CPSR, &cpsr);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &r9);
    }
#else
    (void)i;
#endif
    /* Prefer ABI args Host just wrote */
    regs[0] = p_guest ? p_guest : regs[0];
    regs[1] = method;
    regs[2] = input;
    regs[3] = input_len;
    if (erw) r9 = erw;
    memcpy(g.enter_regs, regs, sizeof(g.enter_regs));
    g.enter_sp = sp;
    g.enter_cpsr = cpsr;
    g.enter_r9 = r9;
    peek_stack(uc, sp, g.enter_stack);

    if (method == 1) g.saw_m1 = 1;
    if (method == 2) g.saw_m2 = 1;
    /* HOST_FAST_REAL is Host 6→8→0 deliver — not a natural Guest/module init transaction. */
    if (source != P22H_SRC_HOST_FAST_REAL) {
        if (method == 0) g.saw_m0 = 1;
        if (method == 6) g.saw_m6 = 1;
        if (method == 8) g.saw_m8 = 1;
    } else {
        if (method == 0 || method == 6 || method == 8) g.saw_fast_680 = 1;
    }

    matrix_touch(method, source, host_fn, caller_pc);
    emit_bound("THUNK_ENTER", helper, method, caller_pc, caller_lr, g.enter_regs, sp, cpsr, r9,
               g.cur_erw, p_guest, 0, source, host_fn,
               source >= P22H_SRC_HOST_BRIDGE_MR_EXTHELPER ? "Host_runCode" : "guest", -1, 0);

    if (method == 1u) record_method1_prov();

    {
        char d[80];
        snprintf(d, sizeof(d), "method=%u source=%s host=%s", method, src_name(source),
                 host_fn ? host_fn : "");
        add_handoff("helper_enter", src_name(source), d);
    }
}

void p22h_helper_return(void *uc, uint32_t helper, uint32_t method, int32_t ret) {
    if (!p22h_enabled() || g.finalized) return;
    if (!g.in_helper && !is_gl_helper(helper)) return;
    (void)uc;
    /* Mid-call retarget can attribute return to gamelist helper without a matching enter. */
    if (method == 1u && !g.method1_seen && is_gl_helper(helper ? helper : g.gl_helper)) {
        g.cur_helper = helper ? helper : g.gl_helper;
        g.cur_method = 1;
        g.cur_source = P22H_SRC_HOST_MR_EVENT;
        snprintf(g.cur_host_fn, sizeof(g.cur_host_fn), "%s", "bridge_mr_extHelper");
        g.enter_regs[0] = g.p_guest;
        g.enter_regs[1] = 1u;
        g.enter_r9 = g.erw;
        g.cur_p = g.p_guest;
        g.cur_erw = g.erw;
        record_method1_prov();
        add_handoff("helper_return_synth_m1", "HOST_bridge_mr_event",
                    "enter_missed_possible_retarget");
    }
    emit_bound("THUNK_RETURN", helper ? helper : g.cur_helper, method, g.cur_caller_pc,
               g.cur_caller_lr, g.enter_regs, g.enter_sp, g.enter_cpsr, g.enter_r9, g.cur_erw,
               g.cur_p, ret, g.cur_source, g.cur_host_fn, "return", -1, 0);
    if (g.method1_seen && method == 1u && g.method1.helper == (helper ? helper : g.cur_helper)) {
        g.method1.ret = ret;
        snprintf(g.method1.return_consumer, sizeof(g.method1.return_consumer), "%s",
                 g.cur_host_fn[0] ? g.cur_host_fn : "HOST_return");
    }
    emit_bound("CALLER_RESUME", helper ? helper : g.cur_helper, method, g.cur_caller_lr,
               g.cur_caller_lr, g.enter_regs, g.enter_sp, g.enter_cpsr, g.enter_r9, g.cur_erw,
               g.cur_p, ret, g.cur_source, g.cur_host_fn, "resume", -1, 0);
    g.in_helper = 0;
}

void p22h_note_entry_begin(uint32_t helper, uint32_t method, uint32_t p_guest, uint32_t input,
                           uint32_t input_len, uint32_t er_rw, uint32_t sp) {
    /* Prefer p22h_helper_enter from e10a31d/bridge; this fills gaps if Host skipped that. */
    if (!p22h_enabled()) return;
    if (!is_gl_helper(helper)) return;
    if (g.in_helper && g.cur_helper == helper && g.cur_method == method) return;
    p22h_helper_enter(g.uc, P22H_SRC_HOST_BRIDGE_MR_EXTHELPER, helper, method, p_guest, er_rw,
                      input, input_len, 0, 0, "gwy_ext_obs_entry_begin");
    (void)sp;
}

void p22h_note_helper_call(uint32_t helper, uint32_t method, int32_t ret_value) {
    if (!p22h_enabled()) return;
    if (!is_gl_helper(helper)) return;
    p22h_helper_return(g.uc, helper, method, ret_value);
}

void p22h_note_timer_fire(uint32_t helper, uint32_t p_guest, uint32_t erw, int end) {
    if (!p22h_enabled()) return;
    if (p_guest && !g.p_guest) {
        g.p_guest = p_guest;
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", p_guest);
    }
    if (erw) g.erw = erw;
    if (!end) return;
    g.fire_ext_n++;
    add_handoff("FIRE_EXT", "platform_timer", "code=2");
    if (g.fire_ext_n >= FIRE_STOP) maybe_stop("fire_ext_n20");
    /* Without Host INIT_SEQ, natural FIRE may stall after the first; finalize once anchored. */
    if (g.method1_seen && g.fire_ext_n >= 1u) maybe_stop("method1_and_fire");
    if (g.fire_ext_n >= 8u) maybe_stop("fire_ext_n8");
    (void)helper;
}

void p22h_note_mr_event(int32_t event_code, int32_t p0, int32_t p1) {
    char d[64];
    if (!p22h_enabled()) return;
    snprintf(d, sizeof(d), "event=%d p0=%d p1=%d → method=1", (int)event_code, (int)p0, (int)p1);
    add_handoff("mr_event", "bridge_mr_event", d);
}

void p22h_note_guest_boundary(const char *stage, uint32_t helper, uint32_t method, uint32_t pc,
                              uint32_t lr, const uint32_t regs[16], uint32_t cpsr,
                              const char *module, uint64_t module_id, const char *insn,
                              int branch_reg, uint32_t source_mem) {
    uint32_t r13[13];
    int i;
    if (!p22h_enabled()) return;
    if (helper && !is_gl_helper(helper) && g.gl_helper &&
        (helper & ~1u) != (g.gl_helper & ~1u))
        return;
    for (i = 0; i < 13; i++) r13[i] = regs ? regs[i] : 0;
    emit_bound(stage, helper ? helper : g.gl_helper, method, pc, lr, r13, regs ? regs[13] : 0, cpsr,
               regs ? regs[9] : 0, g.erw, g.p_guest, 0, P22H_SRC_NATIVE_GUEST, "", insn, branch_reg,
               source_mem);
    if (method == 1u && !g.method1_seen) {
        g.cur_helper = helper ? helper : g.gl_helper;
        g.cur_method = 1;
        g.cur_source = P22H_SRC_NATIVE_GUEST;
        g.cur_caller_pc = pc;
        g.cur_caller_lr = lr;
        g.cur_host_fn[0] = 0;
        g.enter_regs[0] = regs ? regs[0] : g.p_guest;
        g.enter_regs[1] = 1u;
        g.enter_regs[2] = regs ? regs[2] : 0;
        g.enter_regs[3] = regs ? regs[3] : 0;
        g.enter_r9 = regs ? regs[9] : g.erw;
        g.enter_sp = regs ? regs[13] : 0;
        g.enter_cpsr = cpsr;
        g.cur_p = g.p_guest;
        g.cur_erw = g.erw;
        record_method1_prov();
    }
    if (method == 0) g.saw_m0 = 1;
    if (method == 6) g.saw_m6 = 1;
    if (method == 8) g.saw_m8 = 1;
    if (method == 1) g.saw_m1 = 1;
    if (method == 2) g.saw_m2 = 1;
    (void)module;
    (void)module_id;
}

void p22h_note_helper_ptr_write(uint32_t pc, const char *module, uint32_t addr, uint32_t old_v,
                                uint32_t new_v, int src_reg, const char *channel) {
    PtrWRow *w;
    if (!p22h_enabled() || !g.gl_helper) return;
    if ((new_v & ~1u) != (g.gl_helper & ~1u)) return;
    if (g.ptrw_n >= PTRW_CAP) return;
    ensure_files();
    w = &g.ptrw[g.ptrw_n++];
    memset(w, 0, sizeof(*w));
    w->seq = ++g.seq;
    w->pc = pc;
    snprintf(w->module, sizeof(w->module), "%s", module ? module : "?");
    w->addr = addr;
    w->field_off = g.p_guest && addr >= g.p_guest && addr < g.p_guest + 0x40u ? addr - g.p_guest : 0;
    w->old_v = old_v;
    w->new_v = new_v;
    w->src_reg = src_reg;
    snprintf(w->channel, sizeof(w->channel), "%s", channel ? channel : "?");
    if (!g.helper_write_pc) {
        g.helper_write_pc = pc;
        g.helper_write_addr = addr;
        snprintf(g.helper_writer_mod, sizeof(g.helper_writer_mod), "%s", w->module);
    }
    if (g.ptrw_csv) {
        fprintf(g.ptrw_csv, "%u,0x%X,%s,0x%X,0x%X,0x%X,0x%X,%d,%s,%d\n", w->seq, w->pc, w->module,
                w->addr, w->field_off, w->old_v, w->new_v, w->src_reg, w->channel, w->used_indirect);
        fflush(g.ptrw_csv);
    }
}

void p22h_note_helper_ptr_read(uint32_t pc, const char *module, uint32_t addr, uint32_t value,
                               int dst_reg, const char *channel) {
    PtrRRow *r;
    if (!p22h_enabled() || !g.gl_helper) return;
    if ((value & ~1u) != (g.gl_helper & ~1u)) return;
    if (g.ptrr_n >= PTRR_CAP) return;
    ensure_files();
    r = &g.ptrr[g.ptrr_n++];
    memset(r, 0, sizeof(*r));
    r->seq = ++g.seq;
    r->pc = pc;
    snprintf(r->module, sizeof(r->module), "%s", module ? module : "?");
    r->addr = addr;
    r->value = value;
    r->dst_reg = dst_reg;
    snprintf(r->channel, sizeof(r->channel), "%s", channel ? channel : "?");
    if (!g.helper_read_pc) {
        g.helper_read_pc = pc;
        g.helper_read_addr = addr;
    }
    if (g.ptrr_csv) {
        fprintf(g.ptrr_csv, "%u,0x%X,%s,0x%X,0x%X,%d,%s,%d\n", r->seq, r->pc, r->module, r->addr,
                r->value, r->dst_reg, r->channel, r->used_indirect);
        fflush(g.ptrr_csv);
    }
}

void p22h_note_memcpy(uint32_t dst, uint32_t src, uint32_t n, uint32_t caller_pc) {
#ifdef GWY_HAVE_UNICORN
    uint8_t *buf;
    uint32_t off, take;
    if (!p22h_enabled() || !g.gl_helper || !g.uc || !n) return;
    take = n > 0x2000u ? 0x2000u : n;
    buf = (uint8_t *)malloc(take);
    if (!buf) return;
    if (guest_memory_uc_peek((struct uc_struct *)g.uc, src, buf, take)) {
        for (off = 0; off + 4 <= take; off += 4) {
            uint32_t w = (uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8) |
                         ((uint32_t)buf[off + 2] << 16) | ((uint32_t)buf[off + 3] << 24);
            if ((w & ~1u) == (g.gl_helper & ~1u))
                p22h_note_helper_ptr_write(caller_pc, "memcpy", dst + off, 0, w, -1, "memcpy");
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

void p22h_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr) {
    uint32_t norm, off;
    if (!p22h_enabled() || g.finalized) return;
    if (!g.uc) g.uc = uc;
    if (!is_gl(module_name)) {
        /* detect LDR of helper into reg then BLX */
        if (g.gl_helper && regs) {
            int ri;
            for (ri = 0; ri < 13; ri++) {
                if ((regs[ri] & ~1u) == (g.gl_helper & ~1u))
                    p22h_note_helper_ptr_read(pc, module_name ? module_name : "?", 0, regs[ri], ri,
                                              "reg_hold");
            }
        }
        return;
    }
    if (!g.gl_base) return;
    norm = pc & ~1u;
    if (norm < g.gl_base || norm >= g.gl_end) return;
    off = norm - g.gl_base;
    g.gl_insn_n++;
    if (g.gl_helper && (norm == (g.gl_helper & ~1u))) {
        p22h_note_guest_boundary("THUNK_ENTER", g.gl_helper, regs ? regs[1] : 0, pc, lr, regs, cpsr,
                                 module_name, g.module_id, "helper_pc", -1, 0);
    }
    (void)sp;
    (void)off;
    if (g.gl_insn_n >= GL_INSN_STOP) maybe_stop("gl_insn_10M");
    if (now_ms() >= 240000ull) maybe_stop("timeout_240s");
    /* After method1 + helper registered, don't wait forever if FIRE stalls. */
    if (g.method1_seen && g.helper_pub_ok && now_ms() >= 90000ull && g.fire_ext_n == 0u)
        maybe_stop("method1_no_fire_90s");
}

void p22h_finalize(const char *stop_reason) {
    const char *sum;
    if (!p22h_enabled() || g.finalized) return;
    g.finalized = 1;
    if (stop_reason && stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", stop_reason);
    else if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "finalize");

    refresh_identity();
    maybe_sha();
    classify();
    flush_matrix();
    write_disasm_note();
    write_identity();
    write_verdict();

    sum = env_or("JJFB_P22H_SUMMARY", "out/p22h/p22h_runtime_summary.txt");
    g.summary_txt = fopen(sum, "wb");
    if (g.summary_txt) {
        fprintf(g.summary_txt,
                "run_id=%s\nclass=%s\nhelper=0x%X\nmethod1_seen=%d\nsaw_m0=%d\nsaw_m1=%d\n"
                "saw_m2=%d\nsaw_m6=%d\nsaw_m8=%d\nhist_680=%s\nmissing=%s\nsole_lock=%s\n"
                "next_fix=%s\nfire_ext_n=%u\ngl_insn_n=%u\nbound_n=%u\nstop_reason=%s\n"
                "guest_state_written=0\nevents_injected=0\nheadless=0\nfast_init=0\n"
                "gl_base=0x%X\nERW=0x%X\nP=%s\ngeneration=%s\nmodule_id=%s\n"
                "runtime_image_sha256=%s\n",
                g.run_id, g.verdict_class, g.gl_helper, g.method1_seen, g.saw_m0, g.saw_m1, g.saw_m2,
                g.saw_m6, g.saw_m8, g.hist_680_grade, g.missing_contract_kind, g.sole_lock,
                g.next_fix, g.fire_ext_n, g.gl_insn_n, g.bound_n, g.stop_reason, g.gl_base, g.erw,
                g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
                g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED",
                g.module_id_s[0] ? g.module_id_s : "UNKNOWN_NOT_EXPOSED",
                g.runtime_sha[0] ? g.runtime_sha : "UNKNOWN_NOT_EXPOSED");
        fflush(g.summary_txt);
        fclose(g.summary_txt);
        g.summary_txt = NULL;
    }
    printf("[JJFB_P22H_FINAL] class=%s helper=0x%X m1=%d m680=%d/%d/%d source=%s fire=%u "
           "evidence=OBSERVED\n",
           g.verdict_class, g.gl_helper, g.method1_seen, g.saw_m6, g.saw_m8, g.saw_m0,
           g.method1.source_name[0] ? g.method1.source_name : "?", g.fire_ext_n);
    fflush(stdout);
}
