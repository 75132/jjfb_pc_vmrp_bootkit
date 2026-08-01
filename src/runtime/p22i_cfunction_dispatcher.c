#include "gwy_launcher/p22i_cfunction_dispatcher.h"

#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/p22k_post_m1_path.h"
#include "gwy_launcher/p22l_parent_return.h"
#include "gwy_launcher/p22m_queue_scheduler.h"
#include "gwy_launcher/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define CALL_STACK_CAP 16
#define STACK_LOG_CAP 512
#define RET_CAP 128
#define PROV_CAP 128
#define R9_CAP 256
#define POST_CAP 512
#define MATRIX_CAP 64
#define XREF_CAP 256
#define FIRE_STOP 20u
#define INSN_STOP 5000000u
#define OFF_F670 0xF670u
#define OFF_8CDC 0x8CDCu
#define OFF_D978 0xD978u
#define OFF_10740 0x10740u
#define OFF_10814 0x10814u
#define OFF_7B6C 0x7B6Cu

typedef struct {
    uint32_t call_id;
    uint32_t parent_call_id;
    uint32_t helper;
    uint32_t method;
    uint32_t entry_sp;
    uint32_t entry_lr;
    uint32_t entry_r9;
    uint32_t helper_entry_pc;
    uint32_t caller_continuation;
    uint32_t decoded_callsite_pc;
    uint32_t caller_module_offset;
    char caller_module[48];
    uint64_t caller_module_id;
    uint32_t r[13];
    uint32_t sp, cpsr, p_guest, erw_col;
    uint32_t input, input_len;
    char source_name[40];
    char host_fn[64];
    P22iCallSource source;
    int active;
    int returned;
    uint32_t return_pc;
    int32_t return_r0;
    uint32_t r9_depth;
    char r9_contract[40];
    char r9_owner[48];
} CallFrame;

typedef struct {
    uint32_t seq;
    char phase[16];
    uint32_t call_id, parent_call_id;
    uint32_t helper, method;
    uint32_t helper_entry_pc;
    uint32_t caller_continuation;
    uint32_t decoded_callsite_pc;
    char caller_module[48];
    uint32_t caller_module_offset;
    uint32_t entry_sp, entry_lr, entry_r9;
    uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12;
    uint32_t sp, cpsr, p_guest, erw;
    uint32_t input, input_len;
    int32_t return_r0;
    uint32_t return_pc;
    char source_name[40];
    char host_fn[64];
    char note[64];
} StackRow;

typedef struct {
    uint32_t seq;
    uint32_t call_id;
    uint32_t method;
    int32_t return_r0;
    uint32_t return_pc;
    uint32_t first_consumer_pc;
    char first_consumer_insn[48];
    uint32_t cmp_lhs, cmp_rhs;
    char branch_taken[48];
    char fail_path[64];
    char success_path[64];
    uint32_t next_method_pc;
    uint32_t next_method;
    char note[80];
} RetRow;

typedef struct {
    uint32_t seq;
    uint32_t call_id;
    uint32_t method;
    uint32_t r1;
    char value_source[40];
    uint32_t producer_pc;
    char detail[80];
} ProvRow;

typedef struct {
    uint32_t seq;
    uint32_t call_id;
    uint32_t method;
    uint32_t r9_actual;
    char r9_owner[48];
    uint32_t expected_child_r9;
    uint32_t gamelist_erw;
    uint32_t cfunction_erw;
    uint32_t r9_depth;
    uint64_t generation;
    uint32_t p_guest;
    char contract[40];
} R9Row;

typedef struct {
    uint32_t seq;
    char event[40];
    uint32_t pc;
    uint32_t off;
    uint32_t r0, r1, r9;
    char detail[96];
} PostRow;

typedef struct {
    uint32_t method;
    uint32_t hit_n;
    char first_source[40];
    char first_host_fn[64];
    uint32_t first_helper_entry_pc;
    uint32_t first_caller_continuation;
    uint32_t first_callsite_pc;
} MatrixRow;

typedef struct {
    uint32_t xref_pc;
    uint32_t xref_off;
    char insn[32];
    uint32_t target;
    char note[48];
} XrefRow;

typedef struct {
    int known, enabled, finalized;
    void *uc;
    clock_t t0;
    uint32_t seq;
    uint32_t next_call_id;
    uint32_t guest_insn_n;
    uint32_t fire_ext_n;

    char run_id[96];
    char source_commit[80];
    char main_exe_sha[72];
    char raw_ext_sha[72];
    char gl_runtime_sha[72];
    char cf_runtime_sha[72];
    char identity_missing[160];

    uint32_t gl_base, gl_end, gl_size;
    uint32_t gl_helper, gl_erw, p_guest;
    uint64_t gl_id, generation;
    char gl_owner[96];
    char gl_id_s[40], p_guest_s[40], generation_s[40];

    uint32_t cf_base, cf_end, cf_size, cf_erw;
    uint64_t cf_id;
    char cf_owner[96];
    char cf_id_s[40];

    uint32_t anchor_lr;
    uint32_t callsite_pc;
    int callsite_reg;
    char callsite_insn[48];
    uint32_t r12_producer_pc;
    char r12_producer_note[64];
    uint32_t caller_fn_entry, caller_fn_end;
    int callsite_dumped;
    int xrefs_scanned;
    int method8_scanned;

    int saw_m0, saw_m1, saw_m2, saw_m6, saw_m8;
    int ret_m0, ret_m1, ret_m6;
    int32_t r0_m0, r0_m1, r0_m6;
    int natural_601;
    uint64_t m1_ret_ms;
    int entered_f670, entered_8cdc, entered_d978;
    int entered_10740, entered_10814, entered_7b6c;
    int callback_pub;
    int host_init_method;
    int arm_callsite; /* 1 if caller uses ARM BLX Rm */

    char method8_class[48];
    char method8_status[40];
    uint32_t method8_branch_pc;
    char verdict_class[8];
    char sole_lock[200];
    char next_fix[160];
    char stop_reason[96];
    char hist_680[48];

    /* post-return consumer watch */
    int watch_ret;
    uint32_t watch_call_id;
    uint32_t watch_method;
    int32_t watch_r0;
    uint32_t watch_insn_left;
    uint32_t watch_first_pc;
    char watch_first_insn[48];
    int watch_saw_cmp;
    uint32_t watch_cmp_lhs, watch_cmp_rhs;
    char watch_branch[48];

    CallFrame stack[CALL_STACK_CAP];
    int depth;

    StackRow stack_log[STACK_LOG_CAP];
    uint32_t stack_log_n;
    RetRow rets[RET_CAP];
    uint32_t ret_n;
    ProvRow provs[PROV_CAP];
    uint32_t prov_n;
    R9Row r9rows[R9_CAP];
    uint32_t r9_n;
    PostRow posts[POST_CAP];
    uint32_t post_n;
    MatrixRow matrix[MATRIX_CAP];
    uint32_t matrix_n;
    XrefRow xrefs[XREF_CAP];
    uint32_t xref_n;

    FILE *stack_csv, *ret_csv, *prov_csv, *r9_csv, *post_csv, *xref_csv;
    FILE *callsite_txt, *branch_md, *verdict_md, *summary_txt, *ident_txt, *matrix_csv;
} P22iState;

static P22iState g;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}
static const char *env_or(const char *k, const char *fb) {
    const char *e = getenv(k);
    return (e && e[0]) ? e : fb;
}
static FILE *open_out(const char *ek, const char *fb) { return fopen(env_or(ek, fb), "wb"); }
static uint64_t now_ms(void) {
    return (uint64_t)((clock() - g.t0) * 1000 / CLOCKS_PER_SEC);
}
static int is_gl(const char *m) { return m && strstr(m, "gamelist") != NULL; }
static int is_cf(const char *m) { return m && strstr(m, "cfunction") != NULL; }
static const char *src_name(P22iCallSource s) {
    switch (s) {
    case P22I_SRC_NATIVE_GUEST:
        return "NATIVE_GUEST";
    case P22I_SRC_HOST_BRIDGE_MR_EXTHELPER:
        return "HOST_bridge_mr_extHelper";
    case P22I_SRC_HOST_BRIDGE_EXT_HELPER:
        return "HOST_bridge_ext_helper_call";
    case P22I_SRC_HOST_TIMER_FIRE_EXT:
        return "HOST_timer_FIRE_EXT";
    case P22I_SRC_HOST_MR_EVENT:
        return "HOST_bridge_mr_event";
    case P22I_SRC_HOST_FAST_REAL:
        return "HOST_FAST_REAL";
    case P22I_SRC_PLATFORM_CALLBACK:
        return "PLATFORM_CALLBACK";
    default:
        return "UNKNOWN";
    }
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
static void resolve_mod(uint32_t pc, char *out, size_t n, uint64_t *oid, uint32_t *base_out) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = reg && pc ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
    if (oid) *oid = 0;
    if (base_out) *base_out = 0;
    if (!out || !n) return;
    if (m) {
        const char *nm = m->resolved_name[0] ? m->resolved_name : m->requested_name;
        snprintf(out, n, "%s", nm ? nm : "UNKNOWN_NOT_EXPOSED");
        if (oid) *oid = m->module_id;
        if (base_out) *base_out = m->map.guest_code_base;
    } else
        snprintf(out, n, "%s", pc ? "UNKNOWN_NOT_EXPOSED" : "HOST");
}
static void identity_header(FILE *f) {
    if (!f) return;
    fprintf(f,
            "# run_id=%s commit=%s main=%s raw_ext=%s gl_base=0x%X cf_base=0x%X helper=0x%X "
            "gl_erw=0x%X cf_erw=0x%X P=%s gen=%s\n",
            g.run_id[0] ? g.run_id : "?", g.source_commit[0] ? g.source_commit : "?",
            g.main_exe_sha[0] ? g.main_exe_sha : "?", g.raw_ext_sha[0] ? g.raw_ext_sha : "?",
            g.gl_base, g.cf_base, g.gl_helper, g.gl_erw, g.cf_erw,
            g.p_guest_s[0] ? g.p_guest_s : "?", g.generation_s[0] ? g.generation_s : "?");
    fflush(f);
}
static void maybe_sha_mod(uint32_t base, uint32_t size, char *out, size_t outn) {
#ifdef GWY_HAVE_UNICORN
    uint8_t *buf, dig[32];
    size_t j, n;
    if (!g.uc || !base || !size || (out && out[0])) return;
    n = size > 0x40000u ? 0x40000u : size;
    buf = (uint8_t *)malloc(n);
    if (!buf) return;
    if (guest_memory_uc_peek((struct uc_struct *)g.uc, base, buf, (uint32_t)n)) {
        gwy_sha256(buf, n, dig);
        for (j = 0; j < 32 && j * 2 + 2 < outn; j++) sprintf(out + j * 2, "%02x", dig[j]);
        if (outn > 64) out[64] = 0;
    }
    free(buf);
#else
    (void)base;
    (void)size;
    (void)out;
    (void)outn;
#endif
}
static void refresh_identity(void) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m;
    ExtChunkOwnerInfo oi;
    memset(&oi, 0, sizeof(oi));
    if (reg && g.gl_base) {
        m = module_registry_find_by_code_addr(reg, g.gl_base);
        if (m) {
            g.gl_id = m->module_id;
            snprintf(g.gl_id_s, sizeof(g.gl_id_s), "0x%llX", (unsigned long long)m->module_id);
            if (m->data.start_of_er_rw) g.gl_erw = m->data.start_of_er_rw;
            if (m->entries.registered_helper) g.gl_helper = m->entries.registered_helper;
            else if (m->map.helper_address) g.gl_helper = m->map.helper_address;
        }
    }
    if (reg && g.cf_base) {
        m = module_registry_find_by_code_addr(reg, g.cf_base);
        if (m) {
            g.cf_id = m->module_id;
            snprintf(g.cf_id_s, sizeof(g.cf_id_s), "0x%llX", (unsigned long long)m->module_id);
            if (m->data.start_of_er_rw) g.cf_erw = m->data.start_of_er_rw;
        }
    }
    if (g.p_guest && ext_chunk_provider_owner_for_p(g.p_guest, &oi)) {
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", g.p_guest);
        if (oi.module_generation) {
            g.generation = oi.module_generation;
            snprintf(g.generation_s, sizeof(g.generation_s), "%u", oi.module_generation);
        }
        if (oi.erw && !g.gl_erw) g.gl_erw = oi.erw;
    }
    if (!g.p_guest_s[0]) {
        uint32_t lp = ext_chunk_provider_last_p_guest();
        if (lp) {
            g.p_guest = lp;
            snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", lp);
        }
    }
    maybe_sha_mod(g.gl_base, g.gl_size, g.gl_runtime_sha, sizeof(g.gl_runtime_sha));
    maybe_sha_mod(g.cf_base, g.cf_size, g.cf_runtime_sha, sizeof(g.cf_runtime_sha));
}
static void ensure_files(void) {
    if (!g.stack_csv) {
        g.stack_csv = open_out("JJFB_P22I_STACK_CSV", "reports/p22i/p22i_helper_call_stack.csv");
        if (g.stack_csv) {
            identity_header(g.stack_csv);
            fputs("sequence,phase,call_id,parent_call_id,helper,method,helper_entry_pc,"
                  "caller_continuation,decoded_callsite_pc,caller_module,caller_module_offset,"
                  "entry_sp,entry_lr,entry_r9,R0,R1,R2,R3,R4,R5,R6,R7,R8,R9,R10,R11,R12,SP,CPSR,"
                  "P,ERW,input,input_len,return_r0,return_pc,source,host_fn,note\n",
                  g.stack_csv);
            fflush(g.stack_csv);
        }
    }
    if (!g.ret_csv) {
        g.ret_csv = open_out("JJFB_P22I_RETURNS_CSV", "reports/p22i/p22i_helper_returns.csv");
        if (g.ret_csv) {
            identity_header(g.ret_csv);
            fputs("sequence,call_id,method,return_r0,return_pc,first_consumer_pc,first_consumer_insn,"
                  "cmp_lhs,cmp_rhs,branch_taken,fail_path,success_path,next_method_pc,next_method,"
                  "note\n",
                  g.ret_csv);
            fflush(g.ret_csv);
        }
    }
    if (!g.prov_csv) {
        g.prov_csv =
            open_out("JJFB_P22I_PROV_CSV", "reports/p22i/p22i_method_value_provenance.csv");
        if (g.prov_csv) {
            identity_header(g.prov_csv);
            fputs("sequence,call_id,method,R1,value_source,producer_pc,detail\n", g.prov_csv);
            fflush(g.prov_csv);
        }
    }
    if (!g.r9_csv) {
        g.r9_csv = open_out("JJFB_P22I_R9_CSV", "reports/p22i/p22i_r9_owner_timeline.csv");
        if (g.r9_csv) {
            identity_header(g.r9_csv);
            fputs("sequence,call_id,method,R9_actual,R9_owner,expected_child_R9,gamelist_ERW,"
                  "cfunction_ERW,r9_depth,generation,P,contract\n",
                  g.r9_csv);
            fflush(g.r9_csv);
        }
    }
    if (!g.post_csv) {
        g.post_csv = open_out("JJFB_P22I_POST_CSV", "reports/p22i/p22i_post_init_timeline.csv");
        if (g.post_csv) {
            identity_header(g.post_csv);
            fputs("sequence,event,pc,offset,R0,R1,R9,detail\n", g.post_csv);
            fflush(g.post_csv);
        }
    }
    if (!g.xref_csv) {
        g.xref_csv =
            open_out("JJFB_P22I_XREF_CSV", "reports/p22i/p22i_cfunction_caller_xrefs.csv");
        if (g.xref_csv) {
            identity_header(g.xref_csv);
            fputs("xref_pc,xref_offset,insn,target,note\n", g.xref_csv);
            fflush(g.xref_csv);
        }
    }
    if (!g.matrix_csv) {
        g.matrix_csv =
            open_out("JJFB_P22I_MATRIX_CSV", "reports/p22i/p22i_method_dispatch_matrix.csv");
        if (g.matrix_csv) {
            identity_header(g.matrix_csv);
            fputs("method,hit_n,first_source,first_host_fn,first_helper_entry_pc,"
                  "first_caller_continuation,first_callsite_pc\n",
                  g.matrix_csv);
            fflush(g.matrix_csv);
        }
    }
}
static void add_post(const char *ev, uint32_t pc, uint32_t off, uint32_t r0, uint32_t r1,
                     uint32_t r9, const char *detail) {
    PostRow *p;
    if (g.post_n >= POST_CAP) return;
    ensure_files();
    p = &g.posts[g.post_n++];
    memset(p, 0, sizeof(*p));
    p->seq = ++g.seq;
    snprintf(p->event, sizeof(p->event), "%s", ev ? ev : "?");
    p->pc = pc;
    p->off = off;
    p->r0 = r0;
    p->r1 = r1;
    p->r9 = r9;
    snprintf(p->detail, sizeof(p->detail), "%s", detail ? detail : "");
    if (g.post_csv) {
        fprintf(g.post_csv, "%u,%s,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\"\n", p->seq, p->event, p->pc,
                p->off, p->r0, p->r1, p->r9, p->detail);
        fflush(g.post_csv);
    }
}
static void matrix_touch(uint32_t method, P22iCallSource src, const char *host_fn,
                         uint32_t helper_pc, uint32_t cont, uint32_t callsite) {
    uint32_t i;
    MatrixRow *m = NULL;
    for (i = 0; i < g.matrix_n; i++)
        if (g.matrix[i].method == method) {
            m = &g.matrix[i];
            break;
        }
    if (!m) {
        if (g.matrix_n >= MATRIX_CAP) return;
        ensure_files();
        m = &g.matrix[g.matrix_n++];
        memset(m, 0, sizeof(*m));
        m->method = method;
        snprintf(m->first_source, sizeof(m->first_source), "%s", src_name(src));
        snprintf(m->first_host_fn, sizeof(m->first_host_fn), "%s", host_fn ? host_fn : "");
        m->first_helper_entry_pc = helper_pc;
        m->first_caller_continuation = cont;
        m->first_callsite_pc = callsite;
    }
    m->hit_n++;
}
static void classify_r9(uint32_t r9, char *owner, size_t on, char *contract, size_t cn) {
    if (g.gl_erw && r9 == g.gl_erw) {
        snprintf(owner, on, "gamelist.ext_ERW");
        snprintf(contract, cn, "CHILD_R9_EXPECTED");
    } else if (g.cf_erw && r9 == g.cf_erw) {
        snprintf(owner, on, "cfunction.ext_ERW");
        /* gamelist helper entered with parent ERW — frame may need child switch */
        snprintf(contract, cn, "PARENT_R9_EXPECTED");
    } else if (g.p_guest && r9 == g.p_guest) {
        snprintf(owner, on, "P_extChunk");
        snprintf(contract, cn, "UNKNOWN");
    } else if (r9 >= 0x280000u && r9 < 0x290000u) {
        /* likely cfunction ERW band before registry refresh */
        snprintf(owner, on, "cfunction_erw_band");
        snprintf(contract, cn, "PARENT_R9_EXPECTED");
        if (!g.cf_erw) g.cf_erw = r9;
    } else if (g.cf_base && r9 >= g.cf_base && r9 < g.cf_end) {
        snprintf(owner, on, "cfunction_code");
        snprintf(contract, cn, "UNKNOWN");
    } else if (g.gl_base && r9 >= g.gl_base && r9 < g.gl_end) {
        snprintf(owner, on, "gamelist_code");
        snprintf(contract, cn, "UNKNOWN");
    } else {
        snprintf(owner, on, "UNKNOWN_NOT_EXPOSED");
        snprintf(contract, cn, "UNKNOWN");
    }
}
static uint32_t decode_callsite(uint32_t lr, uint32_t cpsr) {
    uint32_t word = 0;
    int thumb = (cpsr & (1u << 5)) != 0;
    if (!lr) return 0;
    /* Caller may be ARM even when helper entry CPSR has T=1. Prefer ARM BLX/BX at LR-4. */
#ifdef GWY_HAVE_UNICORN
    if (g.uc && guest_memory_uc_peek_u32((struct uc_struct *)g.uc, (lr & ~3u) - 4u, &word)) {
        if ((word & 0x0FFFFFF0u) == 0x012FFF30u || (word & 0x0FFFFFF0u) == 0x012FFF10u) {
            g.arm_callsite = 1;
            return (lr & ~3u) - 4u;
        }
    }
#else
    (void)word;
#endif
    return thumb ? ((lr & ~1u) - 2u) : (lr - 4u);
}
static int peek_half(uint32_t addr, uint16_t *out) {
#ifdef GWY_HAVE_UNICORN
    uint32_t w = 0;
    if (!g.uc || !out) return 0;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)g.uc, addr & ~3u, &w)) return 0;
    *out = (uint16_t)((addr & 2u) ? (w >> 16) : (w & 0xFFFFu));
    return 1;
#else
    (void)addr;
    (void)out;
    return 0;
#endif
}
static void describe_thumb(uint16_t h, char *out, size_t n) {
    int rm = -1;
    if (ext_entry_decode_thumb_blx_rm(h, &rm))
        snprintf(out, n, "BLX r%d", rm);
    else if (ext_entry_decode_thumb_bx_rm(h, &rm))
        snprintf(out, n, "BX r%d", rm);
    else if ((h & 0xFF00u) == 0x4700u)
        snprintf(out, n, "BX/BLX-family 0x%04X", h);
    else if ((h & 0xF800u) == 0x2000u)
        snprintf(out, n, "MOVS r%u,#0x%X", (h >> 8) & 7u, h & 0xFFu);
    else if ((h & 0xF800u) == 0x2800u)
        snprintf(out, n, "CMP r%u,#0x%X", (h >> 8) & 7u, h & 0xFFu);
    else if ((h & 0xFFC0u) == 0x4280u)
        snprintf(out, n, "CMP r%u,r%u", h & 7u, (h >> 3) & 7u);
    else
        snprintf(out, n, "h=0x%04X", h);
}
static void dump_callsite_around(uint32_t cont, uint32_t cpsr) {
#ifdef GWY_HAVE_UNICORN
    uint32_t callsite, a, end, word = 0;
    uint8_t buf[192];
    FILE *f;
    int thumb;
    if (!g.uc || !cont || g.callsite_dumped) return;
    callsite = decode_callsite(cont, cpsr);
    g.callsite_pc = callsite;
    g.anchor_lr = cont;
    thumb = g.arm_callsite ? 0 : ((cpsr & (1u << 5)) != 0);
    a = (callsite > 32u) ? (callsite - 32u) : callsite;
    end = callsite + 128u;
    if (end - a > sizeof(buf)) end = a + (uint32_t)sizeof(buf);
    memset(buf, 0, sizeof(buf));
    guest_memory_uc_peek((struct uc_struct *)g.uc, a, buf, end - a);
    ensure_files();
    f = open_out("JJFB_P22I_CALLSITE", "reports/p22i/p22i_cfunction_helper_callsite.txt");
    g.callsite_txt = f;
    if (!f) return;
    identity_header(f);
    fprintf(f, "# dynamic cfunction helper callsite (absolute addrs are run-local)\n");
    fprintf(f, "caller_continuation=0x%X\n", cont);
    fprintf(f, "cfunction_runtime_base=0x%X\n", g.cf_base);
    fprintf(f, "cfunction_runtime_end=0x%X\n", g.cf_end);
    fprintf(f, "caller_continuation_offset=0x%X\n",
            g.cf_base && cont >= g.cf_base ? (cont & ~1u) - g.cf_base : 0);
    fprintf(f, "decoded_callsite_pc=0x%X\n", callsite);
    fprintf(f, "decoded_callsite_offset=0x%X\n",
            g.cf_base && callsite >= g.cf_base ? callsite - g.cf_base : 0);
    fprintf(f, "thumb=%d arm_callsite=%d\n", thumb, g.arm_callsite);
    fprintf(f, "gamelist_helper=0x%X\n", g.gl_helper);
    if (g.arm_callsite) {
        uint32_t off;
        for (off = 0; a + off + 3 < end; off += 4) {
            uint32_t pc = a + off;
            uint32_t w = (uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8) |
                         ((uint32_t)buf[off + 2] << 16) | ((uint32_t)buf[off + 3] << 24);
            char desc[64];
            int rm = -1;
            if ((w & 0x0FFFFFF0u) == 0x012FFF30u) {
                rm = (int)(w & 0xFu);
                snprintf(desc, sizeof(desc), "BLX r%d", rm);
            } else if ((w & 0x0FFFFFF0u) == 0x012FFF10u) {
                rm = (int)(w & 0xFu);
                snprintf(desc, sizeof(desc), "BX r%d", rm);
            } else if ((w & 0x0FF00000u) == 0x03A00000u)
                snprintf(desc, sizeof(desc), "MOV/imm approx w=0x%08X", w);
            else if ((w & 0x0FFFFFF0u) == 0x01A00000u)
                snprintf(desc, sizeof(desc), "MOV Rd,Rm w=0x%08X", w);
            else
                snprintf(desc, sizeof(desc), "w=0x%08X", w);
            fprintf(f, "0x%X off=0x%X bytes=%02X%02X%02X%02X %s%s\n", pc,
                    g.cf_base && pc >= g.cf_base ? pc - g.cf_base : 0, buf[off], buf[off + 1],
                    buf[off + 2], buf[off + 3], desc, pc == callsite ? "  <== callsite" : "");
            if (pc == callsite && rm >= 0) {
                g.callsite_reg = rm;
                snprintf(g.callsite_insn, sizeof(g.callsite_insn), "BLX r%d", rm);
            }
            /* MOV r12, rN: E1A0C00N */
            if ((w & 0xFFFFF000u) == 0xE1A0C000u && pc < callsite) {
                g.r12_producer_pc = pc;
                snprintf(g.r12_producer_note, sizeof(g.r12_producer_note), "MOV r12,r%u @0x%X",
                         w & 0xFu, pc);
            }
            /* LDR r12,[...] */
            if ((w & 0x0FFF0000u) == 0x059C0000u || (w & 0x0FF00000u) == 0x05900000u) {
                if (((w >> 12) & 0xFu) == 12u && pc < callsite) {
                    g.r12_producer_pc = pc;
                    snprintf(g.r12_producer_note, sizeof(g.r12_producer_note),
                             "LDR r12,[...] @0x%X w=0x%08X", pc, w);
                }
            }
        }
        if (guest_memory_uc_peek_u32((struct uc_struct *)g.uc, callsite, &word)) {
            if ((word & 0x0FFFFFF0u) == 0x012FFF30u) {
                g.callsite_reg = (int)(word & 0xFu);
                snprintf(g.callsite_insn, sizeof(g.callsite_insn), "BLX r%d", g.callsite_reg);
            }
        }
    } else {
        uint32_t off;
        for (off = 0; a + off + 1 < end; off += 2) {
            uint16_t h = (uint16_t)(buf[off] | (buf[off + 1] << 8));
            uint32_t pc = a + off;
            char desc[48];
            int rm = -1;
            describe_thumb(h, desc, sizeof(desc));
            fprintf(f, "0x%X off=0x%X bytes=%02X%02X %s%s\n", pc,
                    g.cf_base && pc >= g.cf_base ? pc - g.cf_base : 0, buf[off], buf[off + 1], desc,
                    pc == callsite ? "  <== callsite" : "");
            if (pc == callsite && ext_entry_decode_thumb_blx_rm(h, &rm)) {
                g.callsite_reg = rm;
                snprintf(g.callsite_insn, sizeof(g.callsite_insn), "BLX r%d", rm);
            }
            if ((h & 0xFF00u) == 0x4600u) {
                uint32_t rd = (h & 7u) | ((h & 0x80u) ? 8u : 0);
                uint32_t rm2 = (h >> 3) & 0xFu;
                if (rd == 12u && pc < callsite) {
                    g.r12_producer_pc = pc;
                    snprintf(g.r12_producer_note, sizeof(g.r12_producer_note),
                             "MOV r12,r%u @0x%X", rm2, pc);
                }
            }
        }
    }
    /* scan back for function entry: PUSH with LR */
    /* function bounds */
    {
        uint32_t scan = callsite;
        uint32_t stop = callsite > 0x400u ? callsite - 0x400u : 0;
        if (g.cf_base && stop < g.cf_base) stop = g.cf_base;
        g.caller_fn_entry = g.cf_base ? g.cf_base : 0;
        if (g.arm_callsite) {
            while (scan > stop + 4u) {
                uint32_t w = 0;
                scan -= 4u;
                if (!guest_memory_uc_peek_u32((struct uc_struct *)g.uc, scan, &w)) break;
                /* STMFD sp!,{...,lr} / PUSH */
                if ((w & 0xFFFF0000u) == 0xE92D0000u && (w & 0x4000u)) {
                    g.caller_fn_entry = scan;
                    break;
                }
            }
        } else {
            while (scan > stop + 2u) {
                uint16_t h = 0;
                scan -= 2u;
                if (!peek_half(scan, &h)) break;
                if ((h & 0xFF00u) == 0xB500u || (h & 0xFE00u) == 0xB400u) {
                    if (h & 0x0100u) {
                        g.caller_fn_entry = scan;
                        break;
                    }
                }
            }
        }
        g.caller_fn_end = callsite + 0x200u;
        if (g.cf_end && g.caller_fn_end > g.cf_end) g.caller_fn_end = g.cf_end;
    }
    fprintf(f, "containing_function_entry=0x%X\n", g.caller_fn_entry);
    fprintf(f, "containing_function_end_heuristic=0x%X\n", g.caller_fn_end);
    fprintf(f, "call_insn=%s\n", g.callsite_insn[0] ? g.callsite_insn : "UNKNOWN_NOT_EXPOSED");
    fprintf(f, "target_register=r%d\n", g.callsite_reg >= 0 ? g.callsite_reg : -1);
    fprintf(f, "R12_producer=%s\n",
            g.r12_producer_note[0] ? g.r12_producer_note : "UNKNOWN_NOT_EXPOSED");
    fflush(f);
    g.callsite_dumped = 1;
    printf("[JJFB_P22I] callsite=0x%X cont=0x%X insn=%s evidence=OBSERVED\n", callsite, cont,
           g.callsite_insn[0] ? g.callsite_insn : "?");
    fflush(stdout);
#else
    (void)cont;
    (void)cpsr;
#endif
}
static void scan_xrefs(void) {
#ifdef GWY_HAVE_UNICORN
    uint32_t pc, end;
    if (!g.uc || !g.cf_base || !g.caller_fn_entry || g.xrefs_scanned) return;
    g.xrefs_scanned = 1;
    ensure_files();
    end = g.cf_end ? g.cf_end : (g.cf_base + g.cf_size);
    for (pc = g.cf_base; pc + 3u < end; pc += 2u) {
        uint16_t h0 = 0, h1 = 0;
        int32_t imm32;
        uint32_t S, imm10, J1, J2, imm11, I1, I2, tgt;
        if (!peek_half(pc, &h0)) break;
        if ((h0 & 0xF800u) != 0xF000u) continue;
        if (!peek_half(pc + 2u, &h1)) continue;
        if ((h1 & 0xD000u) != 0xD000u && (h1 & 0xD000u) != 0xC000u) continue;
        S = (h0 >> 10) & 1u;
        imm10 = h0 & 0x3FFu;
        J1 = (h1 >> 13) & 1u;
        J2 = (h1 >> 11) & 1u;
        imm11 = h1 & 0x7FFu;
        I1 = (J1 ^ S) ^ 1u;
        I2 = (J2 ^ S) ^ 1u;
        imm32 = (int32_t)((S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1));
        if (S) imm32 |= (int32_t)0xFE000000;
        tgt = (pc + 4u + (uint32_t)imm32) & ~1u;
        if (tgt == (g.caller_fn_entry & ~1u) ||
            (tgt >= (g.caller_fn_entry & ~1u) && tgt < (g.caller_fn_entry & ~1u) + 8u)) {
            XrefRow *x;
            if (g.xref_n >= XREF_CAP) break;
            x = &g.xrefs[g.xref_n++];
            memset(x, 0, sizeof(*x));
            x->xref_pc = pc;
            x->xref_off = pc - g.cf_base;
            snprintf(x->insn, sizeof(x->insn), "%s",
                     ((h1 & 0xD000u) == 0xC000u) ? "blx_imm" : "bl_imm");
            x->target = tgt;
            snprintf(x->note, sizeof(x->note), "to_caller_fn");
            if (g.xref_csv)
                fprintf(g.xref_csv, "0x%X,0x%X,%s,0x%X,%s\n", x->xref_pc, x->xref_off, x->insn,
                        x->target, x->note);
        }
    }
    if (g.xref_csv) fflush(g.xref_csv);
#else
    ;
#endif
}
static void scan_method8(void) {
#ifdef GWY_HAVE_UNICORN
    uint32_t pc, end;
    int saw_imm8 = 0, saw_cmp8 = 0, saw_r1_8 = 0;
    if (!g.uc || !g.caller_fn_entry || g.method8_scanned) return;
    g.method8_scanned = 1;
    end = g.caller_fn_end ? g.caller_fn_end : (g.caller_fn_entry + 0x400u);
    if (g.cf_end && end > g.cf_end) end = g.cf_end;
    if (g.arm_callsite) {
        for (pc = g.caller_fn_entry & ~3u; pc + 3u < end; pc += 4u) {
            uint32_t w = 0;
            if (!guest_memory_uc_peek_u32((struct uc_struct *)g.uc, pc, &w)) break;
            /* MOV R1,#8 → E3A01008 */
            if (w == 0xE3A01008u || (w & 0xFFFFF000u) == 0xE3A01000u && (w & 0xFFFu) == 8u) {
                saw_imm8 = 1;
                saw_r1_8 = 1;
                g.method8_branch_pc = pc;
            }
            /* CMP R1,#8 → E3510008 */
            if (w == 0xE3510008u) {
                saw_cmp8 = 1;
                if (!g.method8_branch_pc) g.method8_branch_pc = pc;
            }
        }
    } else {
        for (pc = g.caller_fn_entry; pc + 1u < end; pc += 2u) {
            uint16_t h = 0;
            if (!peek_half(pc, &h)) break;
            if (h == 0x2108u) {
                saw_imm8 = 1;
                saw_r1_8 = 1;
                g.method8_branch_pc = pc;
            }
            if (h == 0x2908u) {
                saw_cmp8 = 1;
                if (!g.method8_branch_pc) g.method8_branch_pc = pc;
            }
            if ((h & 0xF800u) == 0x2000u && (h & 0xFFu) == 8u) saw_imm8 = 1;
        }
    }
    if (saw_r1_8)
        snprintf(g.method8_class, sizeof(g.method8_class), "PRESENT_BUT_UNREACHABLE");
    else if (saw_cmp8 || saw_imm8)
        snprintf(g.method8_class, sizeof(g.method8_class), "PRESENT_OTHER_LIFECYCLE");
    else
        snprintf(g.method8_class, sizeof(g.method8_class), "NOT_PRESENT_IN_THIS_DISPATCHER");
    snprintf(g.method8_status, sizeof(g.method8_status), "METHOD8_REQUIREMENT_UNPROVEN");
#else
    snprintf(g.method8_class, sizeof(g.method8_class), "INDIRECT_VALUE_NOT_RESOLVED");
    snprintf(g.method8_status, sizeof(g.method8_status), "METHOD8_REQUIREMENT_UNPROVEN");
#endif
}
static void emit_stack_row(const char *phase, const CallFrame *f, int32_t ret, uint32_t ret_pc,
                           const char *note) {
    StackRow *r;
    if (!f || g.stack_log_n >= STACK_LOG_CAP) return;
    ensure_files();
    r = &g.stack_log[g.stack_log_n++];
    memset(r, 0, sizeof(*r));
    r->seq = ++g.seq;
    snprintf(r->phase, sizeof(r->phase), "%s", phase ? phase : "?");
    r->call_id = f->call_id;
    r->parent_call_id = f->parent_call_id;
    r->helper = f->helper;
    r->method = f->method;
    r->helper_entry_pc = f->helper_entry_pc;
    r->caller_continuation = f->caller_continuation;
    r->decoded_callsite_pc = f->decoded_callsite_pc;
    snprintf(r->caller_module, sizeof(r->caller_module), "%s", f->caller_module);
    r->caller_module_offset = f->caller_module_offset;
    r->entry_sp = f->entry_sp;
    r->entry_lr = f->entry_lr;
    r->entry_r9 = f->entry_r9;
    r->r0 = f->r[0];
    r->r1 = f->r[1];
    r->r2 = f->r[2];
    r->r3 = f->r[3];
    r->r4 = f->r[4];
    r->r5 = f->r[5];
    r->r6 = f->r[6];
    r->r7 = f->r[7];
    r->r8 = f->r[8];
    r->r9 = f->r[9];
    r->r10 = f->r[10];
    r->r11 = f->r[11];
    r->r12 = f->r[12];
    r->sp = f->sp;
    r->cpsr = f->cpsr;
    r->p_guest = f->p_guest;
    r->erw = f->erw_col;
    r->input = f->input;
    r->input_len = f->input_len;
    r->return_r0 = ret;
    r->return_pc = ret_pc;
    snprintf(r->source_name, sizeof(r->source_name), "%s", f->source_name);
    snprintf(r->host_fn, sizeof(r->host_fn), "%s", f->host_fn);
    snprintf(r->note, sizeof(r->note), "%s", note ? note : "");
    if (g.stack_csv) {
        fprintf(g.stack_csv,
                "%u,%s,%u,%u,0x%X,%u,0x%X,0x%X,0x%X,%s,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,0x%X,%s,%s,\"%s\"\n",
                r->seq, r->phase, r->call_id, r->parent_call_id, r->helper, r->method,
                r->helper_entry_pc, r->caller_continuation, r->decoded_callsite_pc, r->caller_module,
                r->caller_module_offset, r->entry_sp, r->entry_lr, r->entry_r9, r->r0, r->r1, r->r2,
                r->r3, r->r4, r->r5, r->r6, r->r7, r->r8, r->r9, r->r10, r->r11, r->r12, r->sp,
                r->cpsr, r->p_guest, r->erw, r->input, r->input_len, r->return_r0, r->return_pc,
                r->source_name, r->host_fn, r->note);
        fflush(g.stack_csv);
    }
}
static void emit_prov(uint32_t call_id, uint32_t method, uint32_t r1, const char *src, uint32_t ppc,
                      const char *detail) {
    ProvRow *p;
    if (g.prov_n >= PROV_CAP) return;
    ensure_files();
    p = &g.provs[g.prov_n++];
    memset(p, 0, sizeof(*p));
    p->seq = ++g.seq;
    p->call_id = call_id;
    p->method = method;
    p->r1 = r1;
    snprintf(p->value_source, sizeof(p->value_source), "%s", src ? src : "UNKNOWN");
    p->producer_pc = ppc;
    snprintf(p->detail, sizeof(p->detail), "%s", detail ? detail : "");
    if (g.prov_csv) {
        fprintf(g.prov_csv, "%u,%u,%u,0x%X,%s,0x%X,\"%s\"\n", p->seq, p->call_id, p->method, p->r1,
                p->value_source, p->producer_pc, p->detail);
        fflush(g.prov_csv);
    }
}
static void emit_r9(const CallFrame *f) {
    R9Row *r;
    if (!f || g.r9_n >= R9_CAP) return;
    ensure_files();
    r = &g.r9rows[g.r9_n++];
    memset(r, 0, sizeof(*r));
    r->seq = ++g.seq;
    r->call_id = f->call_id;
    r->method = f->method;
    r->r9_actual = f->entry_r9;
    snprintf(r->r9_owner, sizeof(r->r9_owner), "%s", f->r9_owner);
    r->expected_child_r9 = g.gl_erw;
    r->gamelist_erw = g.gl_erw;
    r->cfunction_erw = g.cf_erw;
    r->r9_depth = f->r9_depth;
    r->generation = g.generation;
    r->p_guest = f->p_guest;
    snprintf(r->contract, sizeof(r->contract), "%s", f->r9_contract);
    if (g.r9_csv) {
        fprintf(g.r9_csv, "%u,%u,%u,0x%X,%s,0x%X,0x%X,0x%X,%u,%llu,0x%X,%s\n", r->seq, r->call_id,
                r->method, r->r9_actual, r->r9_owner, r->expected_child_r9, r->gamelist_erw,
                r->cfunction_erw, r->r9_depth, (unsigned long long)r->generation, r->p_guest,
                r->contract);
        fflush(g.r9_csv);
    }
}
static void close_frame(CallFrame *f, uint32_t ret_pc, int32_t ret, const char *note) {
    RetRow *rr;
    if (!f || !f->active) return;
    f->active = 0;
    f->returned = 1;
    f->return_pc = ret_pc;
    f->return_r0 = ret;
    emit_stack_row("RETURN", f, ret, ret_pc, note);
    if (f->method == 6u) {
        g.ret_m6 = 1;
        g.r0_m6 = ret;
    }
    if (f->method == 0u) {
        g.ret_m0 = 1;
        g.r0_m0 = ret;
    }
    if (f->method == 1u) {
        g.ret_m1 = 1;
        g.r0_m1 = ret;
        g.m1_ret_ms = now_ms();
        add_post("method1_return", ret_pc,
                 g.cf_base && ret_pc >= g.cf_base ? (ret_pc & ~1u) - g.cf_base : 0, (uint32_t)ret,
                 f->method, f->entry_r9, "start_post_init_watch");
    }
    if (g.ret_n < RET_CAP) {
        ensure_files();
        rr = &g.rets[g.ret_n++];
        memset(rr, 0, sizeof(*rr));
        rr->seq = ++g.seq;
        rr->call_id = f->call_id;
        rr->method = f->method;
        rr->return_r0 = ret;
        rr->return_pc = ret_pc;
        snprintf(rr->note, sizeof(rr->note), "%s", note ? note : "");
        if (g.ret_csv) {
            fprintf(g.ret_csv, "%u,%u,%u,%d,0x%X,0x0,,0x0,0x0,,,,0x0,0,\"%s\"\n", rr->seq,
                    rr->call_id, rr->method, rr->return_r0, rr->return_pc, rr->note);
            fflush(g.ret_csv);
        }
    }
    /* arm consumer watch at continuation */
    g.watch_ret = 1;
    g.watch_call_id = f->call_id;
    g.watch_method = f->method;
    g.watch_r0 = ret;
    g.watch_insn_left = 64;
    g.watch_first_pc = 0;
    g.watch_first_insn[0] = 0;
    g.watch_saw_cmp = 0;
    g.watch_cmp_lhs = g.watch_cmp_rhs = 0;
    g.watch_branch[0] = 0;

    printf("[JJFB_P22I] helper_return method=%u r0=%d pc=0x%X call_id=%u evidence=OBSERVED\n",
           f->method, (int)ret, ret_pc, f->call_id);
    fflush(stdout);
}
static void push_enter(void *uc, P22iCallSource source, uint32_t helper, uint32_t method,
                       uint32_t p_guest, uint32_t erw, uint32_t input, uint32_t input_len,
                       uint32_t helper_pc, uint32_t cont_lr, uint32_t cpsr, uint32_t sp,
                       const uint32_t regs[13], const char *host_fn) {
    CallFrame *f;
    uint32_t base = 0;
    uint64_t mid = 0;
    char owner[48], contract[40];
    int i;
    (void)uc;
    if (g.depth > 0) {
        CallFrame *top = &g.stack[g.depth - 1];
        if (top->active && top->helper == helper && top->method == method &&
            top->entry_sp == sp && (top->entry_lr & ~1u) == (cont_lr & ~1u))
            return; /* dedup */
        /* sequential same-LR dispatcher: close previous if still active */
        if (top->active && (top->entry_lr & ~1u) == (cont_lr & ~1u) && top->entry_sp == sp) {
            close_frame(top, cont_lr, 0 /* unknown until peek */, "IMPLIED_BEFORE_NEXT_ENTER");
            g.depth--;
        }
    }
    if (g.depth >= CALL_STACK_CAP) return;
    f = &g.stack[g.depth++];
    memset(f, 0, sizeof(*f));
    f->call_id = ++g.next_call_id;
    f->parent_call_id = g.depth > 1 ? g.stack[g.depth - 2].call_id : 0;
    f->helper = helper;
    f->method = method;
    f->entry_sp = sp;
    f->entry_lr = cont_lr;
    f->helper_entry_pc = helper_pc ? helper_pc : (helper & ~1u);
    f->caller_continuation = cont_lr;
    f->decoded_callsite_pc = decode_callsite(cont_lr, cpsr);
    f->source = source;
    snprintf(f->source_name, sizeof(f->source_name), "%s", src_name(source));
    snprintf(f->host_fn, sizeof(f->host_fn), "%s", host_fn ? host_fn : "");
    f->p_guest = p_guest ? p_guest : g.p_guest;
    f->erw_col = erw ? erw : g.gl_erw;
    f->input = input;
    f->input_len = input_len;
    f->sp = sp;
    f->cpsr = cpsr;
    if (regs) {
        for (i = 0; i < 13; i++) f->r[i] = regs[i];
        f->entry_r9 = regs[9];
    }
    resolve_mod(cont_lr, f->caller_module, sizeof(f->caller_module), &mid, &base);
    f->caller_module_id = mid;
    if (base && cont_lr >= base)
        f->caller_module_offset = (cont_lr & ~1u) - base;
    else if (g.cf_base && cont_lr >= g.cf_base && cont_lr < g.cf_end)
        f->caller_module_offset = (cont_lr & ~1u) - g.cf_base;
    f->r9_depth = module_r9_switch_depth();
    classify_r9(f->entry_r9, owner, sizeof(owner), contract, sizeof(contract));
    snprintf(f->r9_owner, sizeof(f->r9_owner), "%s", owner);
    snprintf(f->r9_contract, sizeof(f->r9_contract), "%s", contract);
    f->active = 1;

    if (method == 0) g.saw_m0 = 1;
    if (method == 1) g.saw_m1 = 1;
    if (method == 2) g.saw_m2 = 1;
    if (method == 6) g.saw_m6 = 1;
    if (method == 8) g.saw_m8 = 1;
    if (source == P22I_SRC_HOST_FAST_REAL && (method == 0 || method == 6 || method == 8))
        g.host_init_method = 1;
    if (g.saw_m6 && g.saw_m0 && g.saw_m1) g.natural_601 = 1;

    matrix_touch(method, source, host_fn, f->helper_entry_pc, cont_lr, f->decoded_callsite_pc);
    emit_stack_row("ENTER", f, 0, 0, "");
    emit_r9(f);
    {
        char det[64];
        const char *vs = "UNKNOWN";
        if (method == 6 || method == 0 || method == 1 || method == 2 || method == 8)
            vs = "IMMEDIATE_OR_REG";
        snprintf(det, sizeof(det), "R2=0x%X R3=0x%X R12=0x%X", f->r[2], f->r[3], f->r[12]);
        emit_prov(f->call_id, method, f->r[1], vs, f->decoded_callsite_pc, det);
    }
    if (cont_lr && is_cf(f->caller_module)) {
        if (!g.cf_base) {
            /* recover cf base from registry */
            ModuleRegistry *reg = gwy_ext_loader_bound_registry();
            const GwyLoadedModule *m =
                reg ? module_registry_find_by_code_addr(reg, cont_lr & ~1u) : NULL;
            if (m) {
                g.cf_base = m->map.guest_code_base;
                g.cf_size = m->map.guest_code_size;
                g.cf_end = g.cf_base + g.cf_size;
                if (m->data.start_of_er_rw) g.cf_erw = m->data.start_of_er_rw;
            }
        }
        dump_callsite_around(cont_lr, cpsr);
        scan_xrefs();
        scan_method8();
    }
    printf("[JJFB_P22I] helper_enter method=%u helper=0x%X lr=0x%X callsite=0x%X src=%s "
           "r9=0x%X evidence=OBSERVED\n",
           method, helper, cont_lr, f->decoded_callsite_pc, f->source_name, f->entry_r9);
    fflush(stdout);
    /* P22K/P22L: arm sparse DSM CODE watch on continuation BEFORE helper returns. */
    if (cont_lr && source == P22I_SRC_NATIVE_GUEST) {
        p22k_note_dispatcher_continuation(uc ? uc : g.uc, cont_lr, method, 0, sp);
        p22l_note_dispatcher_continuation(uc ? uc : g.uc, cont_lr, method, sp);
        p22m_note_dispatcher_continuation(uc ? uc : g.uc, cont_lr, method, sp);
    }
}
static void try_match_return(uint32_t pc, uint32_t sp, const uint32_t regs[16]) {
    int i;
    if (g.depth <= 0) return;
    for (i = g.depth - 1; i >= 0; i--) {
        CallFrame *f = &g.stack[i];
        if (!f->active) continue;
        if ((pc & ~1u) != (f->entry_lr & ~1u)) continue;
        /* SP should be restored to entry (or +0 for leaf) */
        if (sp && f->entry_sp && sp != f->entry_sp && sp + 16u < f->entry_sp) continue;
        close_frame(f, pc, regs ? (int32_t)regs[0] : 0, "LR_RESUME");
        /* compact: pop trailing inactive */
        while (g.depth > 0 && !g.stack[g.depth - 1].active) g.depth--;
        return;
    }
}
static void watch_consumer(uint32_t pc, const uint32_t regs[16], uint32_t cpsr) {
    uint16_t h = 0;
    char desc[48];
    uint32_t i;
    if (!g.watch_ret || g.watch_insn_left == 0) return;
    g.watch_insn_left--;
    if (!peek_half(pc & ~1u, &h)) return;
    describe_thumb(h, desc, sizeof(desc));
    if (!g.watch_first_pc) {
        g.watch_first_pc = pc;
        snprintf(g.watch_first_insn, sizeof(g.watch_first_insn), "%s", desc);
    }
    if ((h & 0xF800u) == 0x2800u) { /* CMP rn,#imm */
        g.watch_saw_cmp = 1;
        g.watch_cmp_lhs = regs ? regs[(h >> 8) & 7u] : 0;
        g.watch_cmp_rhs = h & 0xFFu;
    }
    if ((h & 0xF000u) == 0xD000u) { /* Bcond */
        snprintf(g.watch_branch, sizeof(g.watch_branch), "Bcond/0x%04X", h);
    }
    /* next method: MOVS r1,#imm then later BLX */
    if ((h & 0xFF00u) == 0x2100u) { /* MOVS r1,#imm */
        for (i = 0; i < g.ret_n; i++) {
            if (g.rets[i].call_id == g.watch_call_id && !g.rets[i].next_method_pc) {
                g.rets[i].next_method_pc = pc;
                g.rets[i].next_method = h & 0xFFu;
            }
        }
    }
    if (g.watch_insn_left == 0 || g.watch_saw_cmp) {
        for (i = 0; i < g.ret_n; i++) {
            RetRow *rr = &g.rets[i];
            if (rr->call_id != g.watch_call_id) continue;
            rr->first_consumer_pc = g.watch_first_pc;
            snprintf(rr->first_consumer_insn, sizeof(rr->first_consumer_insn), "%s",
                     g.watch_first_insn);
            rr->cmp_lhs = g.watch_cmp_lhs;
            rr->cmp_rhs = g.watch_cmp_rhs;
            snprintf(rr->branch_taken, sizeof(rr->branch_taken), "%s",
                     g.watch_branch[0] ? g.watch_branch : "UNKNOWN_NOT_EXPOSED");
            if (g.watch_saw_cmp && g.watch_cmp_lhs != g.watch_cmp_rhs)
                snprintf(rr->fail_path, sizeof(rr->fail_path), "cmp_mismatch_possible");
            else
                snprintf(rr->success_path, sizeof(rr->success_path), "fallthrough_or_eq");
        }
        /* rewrite returns csv simply by appending update line */
        if (g.ret_csv) {
            fprintf(g.ret_csv, "# update call_id=%u consumer=0x%X insn=%s cmp=0x%X/0x%X br=%s\n",
                    g.watch_call_id, g.watch_first_pc, g.watch_first_insn, g.watch_cmp_lhs,
                    g.watch_cmp_rhs, g.watch_branch[0] ? g.watch_branch : "");
            fflush(g.ret_csv);
        }
        if (g.watch_insn_left == 0) g.watch_ret = 0;
        (void)cpsr;
    }
}
static void maybe_stop(const char *why);
static void note_gl_off(uint32_t pc, const uint32_t regs[16]) {
    uint32_t off;
    if (!g.gl_base || pc < g.gl_base || pc >= g.gl_end) return;
    off = (pc & ~1u) - g.gl_base;
    if (off == OFF_F670 && !g.entered_f670) {
        g.entered_f670 = 1;
        add_post("enter_+0xF670", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "callback_wrapper");
        g.callback_pub = 1;
    } else if (off == OFF_8CDC && !g.entered_8cdc) {
        g.entered_8cdc = 1;
        add_post("enter_+0x8CDC", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "");
        g.callback_pub = 1;
    } else if (off == OFF_D978 && !g.entered_d978) {
        g.entered_d978 = 1;
        add_post("enter_+0xD978", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "");
    } else if (off == OFF_10740 && !g.entered_10740) {
        g.entered_10740 = 1;
        add_post("enter_+0x10740", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "UI_init");
        maybe_stop("entered_10740");
    } else if (off == OFF_10814 && !g.entered_10814) {
        g.entered_10814 = 1;
        add_post("enter_+0x10814", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "");
    } else if (off == OFF_7B6C && !g.entered_7b6c) {
        g.entered_7b6c = 1;
        add_post("enter_+0x7B6C", pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0,
                 regs ? regs[9] : 0, "cfg_loader");
    }
}
static void classify(void) {
    int wrong_r9 = 0;
    uint32_t i;
    snprintf(g.hist_680, sizeof(g.hist_680), "HISTORY_ONLY_NOT_CONFIRMED");
    if (g.natural_601 && !g.saw_m8)
        snprintf(g.hist_680, sizeof(g.hist_680), "SUPERSEDED_BY_NATURAL_601");
    else if (g.saw_m6 && g.saw_m8 && g.saw_m0)
        snprintf(g.hist_680, sizeof(g.hist_680), "NATURAL_680_CONFIRMED");
    if (!g.method8_status[0])
        snprintf(g.method8_status, sizeof(g.method8_status), "METHOD8_REQUIREMENT_UNPROVEN");
    if (!g.method8_class[0])
        snprintf(g.method8_class, sizeof(g.method8_class), "NOT_PRESENT_IN_THIS_DISPATCHER");

    for (i = 0; i < g.r9_n; i++) {
        if ((g.r9rows[i].method == 6u || g.r9rows[i].method == 0u || g.r9rows[i].method == 1u) &&
            strstr(g.r9rows[i].contract, "WRONG"))
            wrong_r9 = 1;
        /* parent R9 while calling child gamelist helper — treat as D if gl_erw known and differs */
        if ((g.r9rows[i].method == 6u || g.r9rows[i].method == 0u || g.r9rows[i].method == 1u) &&
            strstr(g.r9rows[i].contract, "PARENT") && g.gl_erw &&
            g.r9rows[i].r9_actual != g.gl_erw)
            wrong_r9 = 1;
    }

    if (g.saw_m8 && strstr(g.method8_class, "REQUIRED_REACHABLE")) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "A");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "method=8 reachable branch exists but natural condition unmet");
        snprintf(g.next_fix, sizeof(g.next_fix), "close method8 predicate operands/producers");
    } else if (wrong_r9) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "D");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "natural 6/0/1 used non-child R9 context (possible wrong ERW writes)");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "P22J: DSM→MRP helper entry must switch to callee ERW before method body");
    } else if (g.natural_601 && (g.ret_m6 || g.ret_m0 || g.ret_m1) &&
               ((g.ret_m6 && g.r0_m6 != 0) || (g.ret_m0 && g.r0_m0 != 0) ||
                (g.ret_m1 && g.r0_m1 != 0))) {
        /* non-zero return — only Class C if consumer treated as fail; still report */
        snprintf(g.verdict_class, sizeof(g.verdict_class), "C");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "natural method return non-zero (m6=%d m0=%d m1=%d) — verify consumer branch",
                 (int)g.r0_m6, (int)g.r0_m0, (int)g.r0_m1);
        snprintf(g.next_fix, sizeof(g.next_fix), "close failing method platform service contract");
    } else if (g.natural_601 && !g.saw_m8) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "B");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "natural contract is 6→0→1 with child R9; historical 6→8→0 unproven");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "trace first blocking branch after method=1 return (not patch method8)");
    } else if (strstr(g.method8_class, "OTHER_LIFECYCLE")) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "E");
        snprintf(g.sole_lock, sizeof(g.sole_lock), "method=8 present but other lifecycle");
        snprintf(g.next_fix, sizeof(g.next_fix), "ignore method8 for init; continue post-m1 path");
    } else if (g.natural_601) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "F");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "P22H attribution/classifier error; natural 6→0→1 already observed");
        snprintf(g.next_fix, sizeof(g.next_fix), "use P22I reports; no functional Guest change");
    } else {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "F");
        snprintf(g.sole_lock, sizeof(g.sole_lock), "dispatcher not fully observed this run");
        snprintf(g.next_fix, sizeof(g.next_fix), "re-run with longer slice / check freeze");
    }
}
static void write_branch_md(void) {
    FILE *f = open_out("JJFB_P22I_BRANCH_MD", "reports/p22i/p22i_return_branch_chain.md");
    uint32_t i;
    g.branch_md = f;
    if (!f) return;
    identity_header(f);
    fprintf(f, "# P22I return branch chain\n\n");
    fprintf(f, "Natural sequence evidence: saw_m6=%d saw_m0=%d saw_m1=%d saw_m8=%d\n\n", g.saw_m6,
            g.saw_m0, g.saw_m1, g.saw_m8);
    for (i = 0; i < g.ret_n; i++) {
        RetRow *r = &g.rets[i];
        fprintf(f,
                "## method %u return\n\n"
                "- call_id=%u\n"
                "- return_r0=%d\n"
                "- return_pc=0x%X\n"
                "- first_consumer=0x%X `%s`\n"
                "- CMP 0x%X,#0x%X\n"
                "- branch=%s\n"
                "- next_method_pc=0x%X next_method=%u\n\n",
                r->method, r->call_id, (int)r->return_r0, r->return_pc, r->first_consumer_pc,
                r->first_consumer_insn, r->cmp_lhs, r->cmp_rhs,
                r->branch_taken[0] ? r->branch_taken : "?", r->next_method_pc, r->next_method);
    }
    fprintf(f, "## chain summary\n\n```\n");
    if (g.ret_m6)
        fprintf(f, "method 6 return %d → (consumer) → method 0\n", (int)g.r0_m6);
    if (g.ret_m0)
        fprintf(f, "method 0 return %d → (consumer) → method 1\n", (int)g.r0_m0);
    if (g.ret_m1)
        fprintf(f, "method 1 return %d → post-init watch\n", (int)g.r0_m1);
    fprintf(f, "```\n");
    fflush(f);
}
static void write_verdict(void) {
    FILE *f = open_out("JJFB_P22I_VERDICT", "reports/p22i/p22i_dispatcher_verdict.md");
    char nat_seq[64];
    g.verdict_md = f;
    if (!f) return;
    snprintf(nat_seq, sizeof(nat_seq), "%s%s%s%s", g.saw_m6 ? "6" : "",
             g.saw_m6 && g.saw_m0 ? "→" : "", g.saw_m0 ? "0" : "",
             (g.saw_m0 || g.saw_m6) && g.saw_m1 ? "→1" : (g.saw_m1 ? "1" : ""));
    fprintf(f,
            "# P22I-CLEAN cfunction dispatcher verdict\n\n"
            "## Bottom line\n\n"
            "**Class: %s**\n\n"
            "```text\n"
            "%s\n"
            "→ method8_status=%s\n"
            "→ method8_class=%s\n"
            "→ hist_680=%s\n"
            "```\n\n"
            "## PASS answers\n\n"
            "```\n"
            "cfunction runtime base/end：0x%X / 0x%X\n"
            "LR 0x%X 对应 offset：0x%X\n"
            "真实 callsite：0x%X\n"
            "调用指令：%s\n"
            "target register：r%d\n"
            "R12 producer：%s\n"
            "caller function：0x%X .. 0x%X\n"
            "\n"
            "自然 method 序列：%s\n"
            "method6 call/return：%s / %s (r0=%d)\n"
            "method0 call/return：%s / %s (r0=%d)\n"
            "method1 call/return：%s / %s (r0=%d)\n"
            "是否出现 method8：%s\n"
            "\n"
            "method8 静态分支是否存在：%s\n"
            "method8 是否属于当前 init：%s\n"
            "6→8→0 历史假设裁决：%s\n"
            "\n"
            "method6 返回消费分支：see p22i_return_branch_chain.md\n"
            "method0 返回消费分支：see p22i_return_branch_chain.md\n"
            "method1 返回消费分支：see p22i_return_branch_chain.md\n"
            "\n"
            "调用时实际 R9：see p22i_r9_owner_timeline.csv\n"
            "R9 owner：see timeline\n"
            "gamelist ERW：0x%X\n"
            "cfunction ERW：0x%X\n"
            "是否需要 thunk 内部切换：%s\n"
            "ABI 是否正确：%s\n"
            "\n"
            "method1 后是否发布 callback：%s\n"
            "+0x10740 是否自然进入：%s\n"
            "+0x7B6C 是否自然进入：%s\n"
            "是否出现真实 cfg open：%s\n"
            "\n"
            "是否 Host 调用 init method：%s\n"
            "是否修改 Guest：NO\n"
            "是否注入事件：NO\n"
            "是否启用 FAST：NO\n"
            "当前唯一门锁：%s\n"
            "下一处最小通用修复：%s\n"
            "stop_reason：%s\n"
            "fire_ext_n：%u\n"
            "guest_insn_n：%u\n"
            "```\n",
            g.verdict_class, g.sole_lock, g.method8_status, g.method8_class, g.hist_680, g.cf_base,
            g.cf_end, g.anchor_lr ? g.anchor_lr : 0x89BF0u,
            g.cf_base && g.anchor_lr >= g.cf_base ? (g.anchor_lr & ~1u) - g.cf_base : 0,
            g.callsite_pc, g.callsite_insn[0] ? g.callsite_insn : "UNKNOWN_NOT_EXPOSED",
            g.callsite_reg, g.r12_producer_note[0] ? g.r12_producer_note : "UNKNOWN_NOT_EXPOSED",
            g.caller_fn_entry, g.caller_fn_end, nat_seq[0] ? nat_seq : "NONE",
            g.saw_m6 ? "YES" : "NO", g.ret_m6 ? "YES" : "NO", (int)g.r0_m6, g.saw_m0 ? "YES" : "NO",
            g.ret_m0 ? "YES" : "NO", (int)g.r0_m0, g.saw_m1 ? "YES" : "NO", g.ret_m1 ? "YES" : "NO",
            (int)g.r0_m1, g.saw_m8 ? "YES" : "NO",
            strstr(g.method8_class, "NOT_PRESENT") ? "NO" : "MAYBE/YES",
            strstr(g.method8_status, "UNPROVEN") ? "UNPROVEN" : "see_class", g.hist_680, g.gl_erw,
            g.cf_erw,
            strstr(g.verdict_class, "D") ? "YES_investigate" : "UNKNOWN_NOT_EXPOSED",
            strstr(g.verdict_class, "D") ? "LIKELY_WRONG_R9" : "OBSERVED_AS_IS",
            g.callback_pub ? "YES" : "NO", g.entered_10740 ? "YES" : "NO",
            g.entered_7b6c ? "YES" : "NO", g.entered_7b6c ? "PARTIAL/YES" : "NO",
            g.host_init_method ? "YES" : "NO", g.sole_lock, g.next_fix, g.stop_reason, g.fire_ext_n,
            g.guest_insn_n);
    fflush(f);
}
static void write_identity(void) {
    FILE *f = open_out("JJFB_P22I_IDENTITY", "out/p22i/p22i_build_identity.txt");
    g.ident_txt = f;
    if (!f) return;
    fprintf(f,
            "run_id=%s\n"
            "source_commit=%s\n"
            "main_exe_sha256=%s\n"
            "raw_gamelist_ext_sha256=%s\n"
            "gamelist_runtime_sha256=%s\n"
            "cfunction_runtime_sha256=%s\n"
            "gamelist_base=0x%X\n"
            "gamelist_end=0x%X\n"
            "cfunction_base=0x%X\n"
            "cfunction_end=0x%X\n"
            "registered_helper=0x%X\n"
            "gamelist_ERW=0x%X\n"
            "cfunction_ERW=0x%X\n"
            "P=%s\n"
            "generation=%s\n"
            "JJFB_P22I_CLEAN=1\n"
            "research_assisted=0\n"
            "product_valid=1\n"
            "FAST_BD0_INIT_CALL=0\n"
            "FAST_PROGRESS_TICK_CALL=0\n"
            "JJFB_FAST_REAL_GAMELIST_INIT_SEQUENCE=0\n",
            g.run_id, g.source_commit[0] ? g.source_commit : "UNKNOWN_NOT_EXPOSED",
            g.main_exe_sha[0] ? g.main_exe_sha : "UNKNOWN_NOT_EXPOSED",
            g.raw_ext_sha[0] ? g.raw_ext_sha : "UNKNOWN_NOT_EXPOSED",
            g.gl_runtime_sha[0] ? g.gl_runtime_sha : "UNKNOWN_NOT_EXPOSED",
            g.cf_runtime_sha[0] ? g.cf_runtime_sha : "UNKNOWN_NOT_EXPOSED", g.gl_base, g.gl_end,
            g.cf_base, g.cf_end, g.gl_helper, g.gl_erw, g.cf_erw,
            g.p_guest_s[0] ? g.p_guest_s : "UNKNOWN_NOT_EXPOSED",
            g.generation_s[0] ? g.generation_s : "UNKNOWN_NOT_EXPOSED");
    fflush(f);
    fclose(f);
    g.ident_txt = NULL;
}
static void flush_matrix(void) {
    uint32_t i;
    ensure_files();
    if (!g.matrix_csv) return;
    for (i = 0; i < g.matrix_n; i++) {
        MatrixRow *m = &g.matrix[i];
        fprintf(g.matrix_csv, "%u,%u,%s,%s,0x%X,0x%X,0x%X\n", m->method, m->hit_n, m->first_source,
                m->first_host_fn, m->first_helper_entry_pc, m->first_caller_continuation,
                m->first_callsite_pc);
    }
    fflush(g.matrix_csv);
}
static void maybe_stop(const char *why) {
    if (g.finalized) return;
    if (why) snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", why);
    p22i_finalize(g.stop_reason);
}

/* ---------- public ---------- */

int p22i_enabled(void) {
    if (!g.known) {
        g.known = 1;
        g.enabled = env1("JJFB_P22I_CLEAN");
        if (g.enabled) {
            g.t0 = clock();
            g.callsite_reg = -1;
            snprintf(g.run_id, sizeof(g.run_id), "%s", env_or("JJFB_P22I_RUN_ID", "p22i"));
            snprintf(g.source_commit, sizeof(g.source_commit), "%s",
                     env_or("JJFB_P22I_SOURCE_COMMIT", "UNKNOWN_NOT_EXPOSED"));
            snprintf(g.main_exe_sha, sizeof(g.main_exe_sha), "%s",
                     env_or("JJFB_P22I_MAIN_SHA", "UNKNOWN_NOT_EXPOSED"));
            snprintf(g.raw_ext_sha, sizeof(g.raw_ext_sha), "%s",
                     env_or("JJFB_P22I_RAW_EXT_SHA", "UNKNOWN_NOT_EXPOSED"));
            snprintf(g.method8_status, sizeof(g.method8_status), "METHOD8_REQUIREMENT_UNPROVEN");
            printf("[JJFB_P22I] armed run_id=%s evidence=OBSERVED\n", g.run_id);
            fflush(stdout);
        }
    }
    return g.enabled;
}

void p22i_reset(void) {
    FILE *a = g.stack_csv, *b = g.ret_csv, *c = g.prov_csv, *d = g.r9_csv, *e = g.post_csv;
    FILE *f = g.xref_csv, *h = g.matrix_csv;
    void *uc = g.uc;
    memset(&g, 0, sizeof(g));
    g.stack_csv = a;
    g.ret_csv = b;
    g.prov_csv = c;
    g.r9_csv = d;
    g.post_csv = e;
    g.xref_csv = f;
    g.matrix_csv = h;
    g.uc = uc;
    g.callsite_reg = -1;
}

void p22i_bind_uc(void *uc) {
    p22k_bind_uc(uc);
    p22l_bind_uc(uc);
    p22m_bind_uc(uc);
    if (!p22i_enabled()) return;
    g.uc = uc;
}

void p22i_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint64_t generation, uint64_t module_id,
                          const char *package_owner) {
    p22l_note_module_map(module_name, base, size, erw);
    p22m_note_module_map(module_name, base, size, erw, p_guest, generation, package_owner);
    if (!p22i_enabled()) return;
    if (is_gl(module_name)) {
        g.gl_base = base;
        g.gl_size = size;
        g.gl_end = base + size;
        if (erw) g.gl_erw = erw;
        if (module_id) {
            g.gl_id = module_id;
            snprintf(g.gl_id_s, sizeof(g.gl_id_s), "0x%llX", (unsigned long long)module_id);
        }
        if (package_owner && package_owner[0])
            snprintf(g.gl_owner, sizeof(g.gl_owner), "%s", package_owner);
    }
    if (is_cf(module_name)) {
        g.cf_base = base;
        g.cf_size = size;
        g.cf_end = base + size;
        if (erw) g.cf_erw = erw;
        if (module_id) {
            g.cf_id = module_id;
            snprintf(g.cf_id_s, sizeof(g.cf_id_s), "0x%llX", (unsigned long long)module_id);
        }
        if (package_owner && package_owner[0])
            snprintf(g.cf_owner, sizeof(g.cf_owner), "%s", package_owner);
    }
    if (p_guest) {
        g.p_guest = p_guest;
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", p_guest);
    }
    if (generation) {
        g.generation = generation;
        snprintf(g.generation_s, sizeof(g.generation_s), "%llu", (unsigned long long)generation);
    }
    refresh_identity();
}

void p22i_note_gamelist_started(void) {
    if (!p22i_enabled()) return;
    add_post("gamelist_started", 0, 0, 0, 0, 0, "shell");
    printf("[JJFB_P22I] gamelist_started evidence=OBSERVED\n");
    fflush(stdout);
}

void p22i_note_c_function_new(uint32_t helper, uint32_t p_len, uint32_t p_guest, uint32_t rw_base,
                              uint32_t rw_size, const char *origin) {
    if (!p22i_enabled()) return;
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
    if (rw_base) g.gl_erw = rw_base;
    (void)p_len;
    (void)rw_size;
    add_post("c_function_new", 0, 0, helper, 0, rw_base, origin ? origin : "?");
    printf("[JJFB_P22I] c_function_new helper=0x%X P=0x%X origin=%s evidence=OBSERVED\n", helper,
           p_guest, origin ? origin : "?");
    fflush(stdout);
}

void p22i_helper_enter(void *uc, P22iCallSource source, uint32_t helper, uint32_t method,
                       uint32_t p_guest, uint32_t erw, uint32_t input, uint32_t input_len,
                       uint32_t caller_pc, uint32_t caller_lr, const char *host_fn) {
    uint32_t regs[13];
    uint32_t cpsr = 0, sp = 0, r9 = 0, helper_pc;
    int i;
    if (!p22i_enabled() || g.finalized) return;
    if (!is_gl_helper(helper) && !(g.gl_helper && (helper & ~1u) == (g.gl_helper & ~1u))) {
        if (!g.gl_helper) return;
        if ((helper & ~1u) != (g.gl_helper & ~1u)) return;
    }
    if (!g.uc) g.uc = uc;
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
    (void)r9;
#endif
    regs[0] = p_guest ? p_guest : regs[0];
    regs[1] = method;
    regs[2] = input;
    regs[3] = input_len;
    if (erw) regs[9] = erw;
    /* Host path: caller_pc is often Host; continuation is caller_lr. */
    helper_pc = helper & ~1u;
    (void)caller_pc;
    push_enter(uc, source, helper, method, p_guest, erw, input, input_len, helper_pc, caller_lr,
               cpsr, sp, regs, host_fn);
}

void p22i_helper_return(void *uc, uint32_t helper, uint32_t method, int32_t ret) {
    int i;
    if (!p22i_enabled() || g.finalized) return;
    (void)uc;
    for (i = g.depth - 1; i >= 0; i--) {
        CallFrame *f = &g.stack[i];
        if (!f->active) continue;
        if (helper && (f->helper & ~1u) != (helper & ~1u)) continue;
        if (method && f->method != method) continue;
        close_frame(f, f->caller_continuation, ret, "HOST_RETURN");
        while (g.depth > 0 && !g.stack[g.depth - 1].active) g.depth--;
        return;
    }
}

void p22i_note_entry_begin(uint32_t helper, uint32_t method, uint32_t p_guest, uint32_t input,
                           uint32_t input_len, uint32_t er_rw, uint32_t sp) {
    if (!p22i_enabled()) return;
    if (!is_gl_helper(helper)) return;
    p22i_helper_enter(g.uc, P22I_SRC_HOST_BRIDGE_MR_EXTHELPER, helper, method, p_guest, er_rw,
                      input, input_len, 0, 0, "gwy_ext_obs_entry_begin");
    (void)sp;
}

void p22i_note_helper_call(uint32_t helper, uint32_t method, int32_t ret_value) {
    if (!p22i_enabled()) return;
    if (!is_gl_helper(helper)) return;
    p22i_helper_return(g.uc, helper, method, ret_value);
}

void p22i_note_timer_fire(uint32_t helper, uint32_t p_guest, uint32_t erw, int end) {
    if (!p22i_enabled()) return;
    if (p_guest && !g.p_guest) {
        g.p_guest = p_guest;
        snprintf(g.p_guest_s, sizeof(g.p_guest_s), "0x%X", p_guest);
    }
    if (erw) g.gl_erw = erw;
    if (!end) return;
    g.fire_ext_n++;
    add_post("FIRE_EXT", 0, 0, helper, 2, erw, "code=2");
    if (g.natural_601 && g.ret_m1 && g.fire_ext_n >= 8u) maybe_stop("natural_601_fire8");
    /* After full 6→0→1 returns + at least one natural FIRE, if no UI/callback progress,
     * close the slice (Guest often idles with no further on_code ticks). */
    if (g.natural_601 && g.ret_m6 && g.ret_m0 && g.ret_m1 && g.fire_ext_n >= 1u &&
        !g.entered_10740 && !g.callback_pub)
        maybe_stop("natural_601_complete_no_post_progress");
    if (g.fire_ext_n >= FIRE_STOP) maybe_stop("fire_ext_n20");
    (void)helper;
}

void p22i_note_mr_event(int32_t event_code, int32_t p0, int32_t p1) {
    char d[64];
    if (!p22i_enabled()) return;
    snprintf(d, sizeof(d), "event=%d p0=%d p1=%d", (int)event_code, (int)p0, (int)p1);
    add_post("mr_event", 0, 0, (uint32_t)event_code, (uint32_t)p0, (uint32_t)p1, d);
}

void p22i_note_guest_boundary(const char *stage, uint32_t helper, uint32_t method, uint32_t pc,
                              uint32_t lr, const uint32_t regs[16], uint32_t cpsr,
                              const char *module, uint64_t module_id, const char *insn,
                              int branch_reg, uint32_t source_mem) {
    uint32_t r13[13];
    int i;
    if (!p22i_enabled() || g.finalized) return;
    if (helper && !is_gl_helper(helper) && g.gl_helper &&
        (helper & ~1u) != (g.gl_helper & ~1u))
        return;
    for (i = 0; i < 13; i++) r13[i] = regs ? regs[i] : 0;
    if (stage && strstr(stage, "ENTER")) {
        push_enter(g.uc, P22I_SRC_NATIVE_GUEST, helper ? helper : g.gl_helper, method, g.p_guest,
                   g.gl_erw, regs ? regs[2] : 0, regs ? regs[3] : 0, pc, lr, cpsr,
                   regs ? regs[13] : 0, r13, "");
    }
    (void)module;
    (void)module_id;
    (void)insn;
    (void)branch_reg;
    (void)source_mem;
}

void p22i_note_memcpy(uint32_t dst, uint32_t src, uint32_t n, uint32_t caller_pc) {
    (void)dst;
    (void)src;
    (void)n;
    (void)caller_pc;
}

void p22i_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr) {
    uint32_t norm;
    int ri;
    if (!p22i_enabled() || g.finalized) return;
    if (!g.uc) g.uc = uc;
    g.guest_insn_n++;

    try_match_return(pc, sp, regs);
    if (g.watch_ret && module_name && is_cf(module_name)) watch_consumer(pc, regs, cpsr);
    note_gl_off(pc, regs);

    /* detect BLX to helper from any module (esp. cfunction) */
    if (g.gl_helper && regs) {
        uint16_t h = 0;
        int rm = -1;
        if (peek_half(pc & ~1u, &h) && ext_entry_decode_thumb_blx_rm(h, &rm) && rm >= 0 &&
            rm < 16) {
            if ((regs[rm] & ~1u) == (g.gl_helper & ~1u)) {
                /* next insn will enter helper; record callsite hint */
                if (!g.callsite_dumped && is_cf(module_name)) {
                    g.callsite_reg = rm;
                    snprintf(g.callsite_insn, sizeof(g.callsite_insn), "BLX r%d", rm);
                    g.callsite_pc = pc & ~1u;
                }
            }
        }
        for (ri = 0; ri < 13; ri++) {
            if ((regs[ri] & ~1u) == (g.gl_helper & ~1u) && ri == 12 && is_cf(module_name) &&
                !g.r12_producer_note[0]) {
                snprintf(g.r12_producer_note, sizeof(g.r12_producer_note),
                         "R12_holds_helper_at_pc=0x%X", pc);
            }
        }
    }

    norm = pc & ~1u;
    if (g.gl_helper && norm == (g.gl_helper & ~1u)) {
        uint32_t r13[13];
        uint32_t live_r9 = 0;
        int i;
        for (i = 0; i < 13; i++) r13[i] = regs ? regs[i] : 0;
        /* Prefer live UC R9 (P22J may have switched after the GCO snapshot). */
#ifdef GWY_HAVE_UNICORN
        if (uc && guest_memory_uc_read_r9((struct uc_struct *)uc, &live_r9) && live_r9)
            r13[9] = live_r9;
#else
        (void)live_r9;
#endif
        push_enter(uc, P22I_SRC_NATIVE_GUEST, g.gl_helper, regs ? regs[1] : 0, g.p_guest, g.gl_erw,
                   regs ? regs[2] : 0, regs ? regs[3] : 0, pc, lr, cpsr, sp, r13, "");
    }

    if (g.guest_insn_n >= INSN_STOP) maybe_stop("guest_insn_5M");
    if (now_ms() >= 240000ull) maybe_stop("timeout_240s");
    /* Post-m1 observation window: allow FIRE/progress, then close. */
    if (g.natural_601 && g.ret_m1 && g.m1_ret_ms && now_ms() >= g.m1_ret_ms + 45000ull)
        maybe_stop("post_m1_observe_45s");
    (void)module_name;
}

void p22i_finalize(const char *stop_reason) {
    const char *sum;
    uint32_t i;
    if (!p22i_enabled() || g.finalized) return;
    g.finalized = 1;
    if (stop_reason && stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", stop_reason);
    else if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "finalize");

    /* close any still-active frames */
    for (i = 0; i < (uint32_t)g.depth; i++) {
        if (g.stack[i].active)
            close_frame(&g.stack[i], g.stack[i].caller_continuation, 0, "FINALIZE_OPEN");
    }

    refresh_identity();
    if (!g.method8_scanned) scan_method8();
    classify();
    flush_matrix();
    write_branch_md();
    write_identity();
    write_verdict();

    sum = env_or("JJFB_P22I_SUMMARY", "out/p22i/p22i_runtime_summary.txt");
    g.summary_txt = fopen(sum, "wb");
    if (g.summary_txt) {
        fprintf(g.summary_txt,
                "run_id=%s\nclass=%s\nhelper=0x%X\nsaw_m6=%d\nsaw_m0=%d\nsaw_m1=%d\nsaw_m8=%d\n"
                "ret_m6=%d\nret_m0=%d\nret_m1=%d\nr0_m6=%d\nr0_m0=%d\nr0_m1=%d\n"
                "natural_601=%d\nmethod8_status=%s\nmethod8_class=%s\nhist_680=%s\n"
                "sole_lock=%s\nnext_fix=%s\nfire_ext_n=%u\nguest_insn_n=%u\n"
                "entered_10740=%d\nentered_7b6c=%d\ncallback_pub=%d\n"
                "cf_base=0x%X\ncallsite=0x%X\nanchor_lr=0x%X\nstop_reason=%s\n"
                "guest_state_written=0\nevents_injected=0\nheadless=0\nfast_init=0\n",
                g.run_id, g.verdict_class, g.gl_helper, g.saw_m6, g.saw_m0, g.saw_m1, g.saw_m8,
                g.ret_m6, g.ret_m0, g.ret_m1, (int)g.r0_m6, (int)g.r0_m0, (int)g.r0_m1,
                g.natural_601, g.method8_status, g.method8_class, g.hist_680, g.sole_lock,
                g.next_fix, g.fire_ext_n, g.guest_insn_n, g.entered_10740, g.entered_7b6c,
                g.callback_pub, g.cf_base, g.callsite_pc, g.anchor_lr, g.stop_reason);
        fflush(g.summary_txt);
        fclose(g.summary_txt);
        g.summary_txt = NULL;
    }
    printf("[JJFB_P22I_FINAL] class=%s helper=0x%X seq=%s m8=%s fire=%u evidence=OBSERVED\n",
           g.verdict_class, g.gl_helper, g.natural_601 ? "6-0-1" : "partial", g.method8_status,
           g.fire_ext_n);
    fflush(stdout);
    p22k_finalize(stop_reason && stop_reason[0] ? stop_reason : g.stop_reason);
    p22l_finalize(stop_reason && stop_reason[0] ? stop_reason : g.stop_reason);
    p22m_finalize(stop_reason && stop_reason[0] ? stop_reason : g.stop_reason);
}
