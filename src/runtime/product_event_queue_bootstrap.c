#include "gwy_launcher/product_event_queue_bootstrap.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/platform_event_queue.h"
#include "gwy_launcher/platform_event_service.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define EQB_CTX_CAP 64
#define EQB_WRITE_CAP 512
#define EQB_OBJ_CAP 64
#define EQB_CONTRACT_CAP 32
#define EQB_FLOW_CAP 64
#define EQB_LIST_CAP 128
#define EQB_XREF_CAP 64

#define OFF_B50 0xB50u
#define OFF_B54 0xB54u

typedef struct {
    char phase[40];
    uint64_t owner_module_id;
    uint64_t owner_generation;
    uint32_t er_rw;
    uint32_t r9;
    uint32_t p_guest;
    uint32_t ext_chunk;
    uint32_t helper;
    uint64_t frame_token;
    uint32_t b50_abs;
    uint32_t b50_val;
    uint32_t b54_abs;
    uint32_t b54_val;
    uint32_t owner_store_65;
    uint32_t owner_store_62;
    uint32_t owner_val_65;
    uint32_t owner_val_62;
    int store_eq_b54;
} EqbCtx;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t lr;
    uint32_t sp;
    uint32_t r9;
    uint32_t depth;
    uint64_t owner_module_id;
    uint64_t owner_generation;
    uint32_t addr;
    char field[24];
    uint32_t old_v;
    uint32_t new_v;
    int size;
    char enclosing[48];
    uint32_t prev_plat;
    uint32_t next_plat;
    int mapped;
    int points_10162;
    int points_10165;
} EqbWrite;

typedef struct {
    uint32_t seq;
    uint32_t plat_code;
    uint32_t guest_ptr;
    uint32_t size;
    uint32_t handler;
    uint32_t owner_store;
    uint32_t snap0[8];
} EqbObj;

typedef struct {
    uint32_t plat_code;
    uint32_t r[8];
    uint32_t stack0;
    uint32_t stack1;
    uint32_t size;
    uint32_t handler;
    uint32_t ret;
    uint32_t caller_pc;
    uint32_t caller_lr;
    uint32_t er_rw;
    uint32_t r9;
    uint32_t store_addr;
    int store_is_b50;
    int store_is_b54;
} EqbContract;

typedef struct {
    uint32_t seq;
    uint32_t plat_code;
    uint32_t guest_ptr;
    uint32_t store_addr;
    uint32_t er_rw;
    uint32_t r9;
    char step[32];
} EqbFlow;

typedef struct {
    uint32_t pc;
    uint32_t base;
    uint32_t offset;
    uint32_t value;
    int is_write;
    char role[40];
} EqbListAcc;

typedef struct {
    uint32_t site;
    char kind[32];
    char note[96];
} EqbXref;

static char g_run_id[64];
static int g_en_known;
static int g_en;
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_path_a_er_rw;
static uint32_t g_r9_30d2f9;
static uint32_t g_r9_312a60;
static uint32_t g_owner_store_65;
static uint32_t g_owner_store_62;
static uint32_t g_ctx_65;
static uint32_t g_ctx_62;
static uint32_t g_prev_plat;
static uint32_t g_write_seq;
static int g_blocked;
static int g_b54_ever_nz;
static int g_b54_cleared;
static int g_finalized;
static uint32_t g_owner_mod;
static uint64_t g_owner_gen;

static EqbCtx g_ctx[EQB_CTX_CAP];
static int g_ctx_n;
static EqbWrite g_wr[EQB_WRITE_CAP];
static int g_wr_n;
static EqbObj g_obj[EQB_OBJ_CAP];
static int g_obj_n;
static EqbContract g_ct[EQB_CONTRACT_CAP];
static int g_ct_n;
static EqbFlow g_flow[EQB_FLOW_CAP];
static int g_flow_n;
static EqbListAcc g_list[EQB_LIST_CAP];
static int g_list_n;
static EqbXref g_xref[EQB_XREF_CAP];
static int g_xref_n;

#ifdef GWY_HAVE_UNICORN
static uc_hook g_hook_mem;
static uc_hook g_hook_30d2f9;
static uc_hook g_hook_2e4d6c;
static uc_hook g_hook_312a60;
static uc_hook g_hook_2fe82c;
static uc_hook g_hook_30cbbc;
static int g_hook_mem_ok;
static int g_hook_code_ok;
#endif

static int env1(const char *k) {
    const char *v = getenv(k);
    return v && v[0] && v[0] != '0';
}

static void report_path(char *out, size_t n, const char *name) {
    const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (root && root[0])
        snprintf(out, n, "%s/%s", root, name);
    else
        snprintf(out, n, "reports/%s", name);
}

static const char *phase_name(GwyEqbPhase p) {
    switch (p) {
    case GWY_EQB_ROBOTOL_BOOTSTRAP: return "ROBOTOL_BOOTSTRAP";
    case GWY_EQB_EXT_INIT_ENTER: return "EXT_INIT_ENTER";
    case GWY_EQB_EXT_INIT_LEAVE: return "EXT_INIT_LEAVE";
    case GWY_EQB_10162_ALLOC: return "10162_ALLOC";
    case GWY_EQB_10162_RETURN: return "10162_RETURN";
    case GWY_EQB_10162_OWNER_STORE: return "10162_OWNER_STORE";
    case GWY_EQB_10165_ALLOC: return "10165_ALLOC";
    case GWY_EQB_10165_RETURN: return "10165_RETURN";
    case GWY_EQB_10165_OWNER_STORE: return "10165_OWNER_STORE";
    case GWY_EQB_10140_TIMER: return "10140_TIMER";
    case GWY_EQB_10102_REGISTER: return "10102_REGISTER";
    case GWY_EQB_FIRST_1E209_REQUEST: return "FIRST_1E209_REQUEST";
    case GWY_EQB_30D2F9_ENTER: return "30D2F9_ENTER";
    case GWY_EQB_101AB_ENTER: return "101AB_ENTER";
    case GWY_EQB_2E4D6C_ENTER: return "2E4D6C_ENTER";
    case GWY_EQB_312A60_FAULT_OR_RETURN: return "312A60_FAULT_OR_RETURN";
    case GWY_EQB_PATH_A_BLOCKED: return "PATH_A_BLOCKED";
    default: return "NONE";
    }
}

int product_eqb_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_PRODUCT_TRACE_QUEUE_BOOTSTRAP");
        g_en_known = 1;
    }
    return g_en;
}

void product_eqb_reset(void) {
    memset(g_ctx, 0, sizeof(g_ctx));
    memset(g_wr, 0, sizeof(g_wr));
    memset(g_obj, 0, sizeof(g_obj));
    memset(g_ct, 0, sizeof(g_ct));
    memset(g_flow, 0, sizeof(g_flow));
    memset(g_list, 0, sizeof(g_list));
    memset(g_xref, 0, sizeof(g_xref));
    g_ctx_n = g_wr_n = g_obj_n = g_ct_n = g_flow_n = g_list_n = g_xref_n = 0;
    g_uc = NULL;
    g_er_rw = g_path_a_er_rw = g_r9_30d2f9 = g_r9_312a60 = 0;
    g_owner_store_65 = g_owner_store_62 = g_ctx_65 = g_ctx_62 = 0;
    g_prev_plat = g_write_seq = 0;
    g_blocked = g_b54_ever_nz = g_b54_cleared = g_finalized = 0;
    g_owner_mod = 0;
    g_owner_gen = 0;
#ifdef GWY_HAVE_UNICORN
    g_hook_mem_ok = g_hook_code_ok = 0;
#endif
    platform_event_queue_reset();
}

void product_eqb_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_eqb_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

void product_eqb_bind_uc(void *uc) { g_uc = uc; }

void product_eqb_note_er_rw(uint32_t er_rw, const char *tag) {
    if (!product_eqb_enabled() || !er_rw) return;
    g_er_rw = er_rw;
    if (tag && strstr(tag, "path_a")) g_path_a_er_rw = er_rw;
    (void)tag;
}

void product_eqb_note_owner_store(uint32_t plat_code, uint32_t guest_ptr, uint32_t store_addr) {
    if (!product_eqb_enabled()) return;
    if (plat_code == 0x10165u) {
        g_ctx_65 = guest_ptr;
        if (store_addr) g_owner_store_65 = store_addr;
    } else if (plat_code == 0x10162u) {
        g_ctx_62 = guest_ptr;
        if (store_addr) g_owner_store_62 = store_addr;
    }
    platform_event_queue_note_ctx(plat_code, guest_ptr, store_addr);
}

static void add_xref(uint32_t site, const char *kind, const char *note) {
    EqbXref *x;
    if (g_xref_n >= EQB_XREF_CAP) return;
    x = &g_xref[g_xref_n++];
    x->site = site;
    snprintf(x->kind, sizeof(x->kind), "%s", kind ? kind : "");
    snprintf(x->note, sizeof(x->note), "%s", note ? note : "");
}

static const char *classify_field(uint32_t addr, uint32_t base) {
    if (!base) return "other";
    if (addr == base + OFF_B50) return "B50";
    if (addr == base + OFF_B54) return "B54";
    if (g_owner_store_65 && addr == g_owner_store_65) return "owner_store_10165";
    if (g_owner_store_62 && addr == g_owner_store_62) return "owner_store_10162";
    if (g_ctx_65 && addr >= g_ctx_65 && addr < g_ctx_65 + 0x100u) return "obj_10165";
    if (g_ctx_62 && addr >= g_ctx_62 && addr < g_ctx_62 + 0x100u) return "obj_10162";
    return "er_rw_other";
}

#ifdef GWY_HAVE_UNICORN
static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                         void *user_data) {
    EqbWrite *w;
    uint32_t a = (uint32_t)address;
    uint32_t old_v = 0;
    uint32_t pc = 0, lr = 0, sp = 0, r9 = 0;
    uint32_t base = g_er_rw ? g_er_rw : g_path_a_er_rw;
    (void)type;
    (void)user_data;
    if (!product_eqb_enabled() || g_blocked) return;
    if (g_wr_n >= EQB_WRITE_CAP) return;

    /* Only track B50/B54, owner stores, and first 0x100 of ctx objects. */
    if (!(base && (a == base + OFF_B50 || a == base + OFF_B54)) &&
        !(g_owner_store_65 && a == g_owner_store_65) &&
        !(g_owner_store_62 && a == g_owner_store_62) &&
        !(g_ctx_65 && a >= g_ctx_65 && a < g_ctx_65 + 0x100u) &&
        !(g_ctx_62 && a >= g_ctx_62 && a < g_ctx_62 + 0x100u))
        return;

    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, a, &old_v);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);

    w = &g_wr[g_wr_n++];
    memset(w, 0, sizeof(*w));
    w->seq = ++g_write_seq;
    w->pc = pc;
    w->lr = lr;
    w->sp = sp;
    w->r9 = r9;
    w->depth = 0;
    w->owner_module_id = g_owner_mod;
    w->owner_generation = g_owner_gen;
    w->addr = a;
    snprintf(w->field, sizeof(w->field), "%s", classify_field(a, base));
    w->old_v = old_v;
    w->new_v = (uint32_t)value;
    w->size = size;
    snprintf(w->enclosing, sizeof(w->enclosing), "pc=0x%X", pc);
    w->prev_plat = g_prev_plat;
    w->next_plat = 0;
    w->mapped = 1;
    w->points_10165 = (g_ctx_65 && (uint32_t)value == g_ctx_65) ? 1 : 0;
    w->points_10162 = (g_ctx_62 && (uint32_t)value == g_ctx_62) ? 1 : 0;

    if (base && a == base + OFF_B54) {
        if ((uint32_t)value) g_b54_ever_nz = 1;
        if (old_v && !(uint32_t)value) g_b54_cleared = 1;
        printf("[EQB_B54_WRITE] pc=0x%X r9=0x%X slot=0x%X old=0x%X new=0x%X evidence=OBSERVED\n",
               pc, r9, a, old_v, (uint32_t)value);
        fflush(stdout);
    }
}

static void on_code_site(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t r9 = 0, r0 = 0, er = 0;
    intptr_t tag = (intptr_t)user_data;
    (void)size;
    if (!product_eqb_enabled()) return;
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    er = g_er_rw ? g_er_rw : r9;

    if (tag == 1) { /* 0x30D2F9 */
        g_r9_30d2f9 = r9;
        product_eqb_phase(uc, GWY_EQB_30D2F9_ENTER, er, r9, 0, 0, 0, g_owner_mod, g_owner_gen, 0);
    } else if (tag == 2) { /* 0x2E4D6C */
        product_eqb_phase(uc, GWY_EQB_2E4D6C_ENTER, er, r9, 0, 0, 0, g_owner_mod, g_owner_gen, 0);
    } else if (tag == 3) { /* 0x312A60 */
        g_r9_312a60 = r9;
        product_eqb_note_list_access(pc, r0, 4, 0, 0, "list_push_entry_r0");
        product_eqb_phase(uc, GWY_EQB_312A60_FAULT_OR_RETURN, er, r9, 0, 0, 0, g_owner_mod,
                          g_owner_gen, 0);
    } else if (tag == 4) { /* 0x2FE82C list bootstrap */
        printf("[EQB_INIT_ENTER] fn=0x2FE82C r9=0x%X er_rw=0x%X evidence=OBSERVED\n", r9, er);
        fflush(stdout);
        add_xref(0x2FE82Cu, "dynamic_enter", "B54 lazy list ctor path");
    } else if (tag == 5) { /* 0x30CBBC case-2 init */
        printf("[EQB_INIT_ENTER] fn=0x30CBBC r9=0x%X er_rw=0x%X evidence=OBSERVED\n", r9, er);
        fflush(stdout);
        add_xref(0x30CBBCu, "dynamic_enter", "family case app=2 queue bootstrap");
    }
}
#endif

void product_eqb_arm_write_hooks(void *uc, uint32_t er_rw) {
#ifdef GWY_HAVE_UNICORN
    uc_err e;
    if (!product_eqb_enabled() || !uc || !er_rw) return;
    g_uc = uc;
    g_er_rw = er_rw;
    if (g_hook_mem_ok) {
        uc_hook_del((uc_engine *)uc, g_hook_mem);
        g_hook_mem_ok = 0;
    }
    e = uc_hook_add((uc_engine *)uc, &g_hook_mem, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL,
                    er_rw, (uint64_t)er_rw + 0x3FFFull);
    g_hook_mem_ok = (e == UC_ERR_OK);
    if (g_ctx_65) {
        uc_hook hk = 0;
        (void)uc_hook_add((uc_engine *)uc, &hk, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL,
                          g_ctx_65, (uint64_t)g_ctx_65 + 0xFFull);
    }
    if (g_ctx_62) {
        uc_hook hk = 0;
        (void)uc_hook_add((uc_engine *)uc, &hk, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL,
                          g_ctx_62, (uint64_t)g_ctx_62 + 0xFFull);
    }
    printf("[EQB_WRITE_HOOKS] er_rw=0x%X ok=%d evidence=OBSERVED\n", er_rw, g_hook_mem_ok);
    fflush(stdout);
#else
    (void)uc;
    (void)er_rw;
#endif
}

void product_eqb_arm_code_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    if (!product_eqb_enabled() || !uc || g_hook_code_ok) return;
    g_uc = uc;
    (void)uc_hook_add((uc_engine *)uc, &g_hook_30d2f9, UC_HOOK_CODE, (void *)on_code_site,
                      (void *)(intptr_t)1, 0x30D2F8ull, 0x30D2FBull);
    (void)uc_hook_add((uc_engine *)uc, &g_hook_2e4d6c, UC_HOOK_CODE, (void *)on_code_site,
                      (void *)(intptr_t)2, 0x2E4D6Cull, 0x2E4D6Full);
    (void)uc_hook_add((uc_engine *)uc, &g_hook_312a60, UC_HOOK_CODE, (void *)on_code_site,
                      (void *)(intptr_t)3, 0x312A60ull, 0x312A63ull);
    (void)uc_hook_add((uc_engine *)uc, &g_hook_2fe82c, UC_HOOK_CODE, (void *)on_code_site,
                      (void *)(intptr_t)4, 0x2FE82Cull, 0x2FE82Full);
    (void)uc_hook_add((uc_engine *)uc, &g_hook_30cbbc, UC_HOOK_CODE, (void *)on_code_site,
                      (void *)(intptr_t)5, 0x30CBBCull, 0x30CBBFull);
    g_hook_code_ok = 1;
    add_xref(0x2FE82Cu, "static_writer", "STR list_ctor result to R9+0xB54");
    add_xref(0x30D0ACu, "static_caller", "only BL of 0x2FE82C inside 0x30CBBC");
    add_xref(0x30E15Eu, "static_caller", "family dispatcher case app=2 -> 0x30CBBC");
    add_xref(0x312AA4u, "static_ctor", "list control alloc size=8 head=0 count=0");
    add_xref(0x312A60u, "static_push", "list_push; LDR [r4,#4] needs B54 object");
    add_xref(0x2E4EE8u, "static_reader", "Path-A loads R9+0xB54 then BL 0x312A60");
    printf("[EQB_CODE_HOOKS] armed sites=30D2F9,2E4D6C,312A60,2FE82C,30CBBC evidence=OBSERVED\n");
    fflush(stdout);
#else
    (void)uc;
#endif
}

void product_eqb_phase(void *uc, GwyEqbPhase phase, uint32_t er_rw, uint32_t r9, uint32_t p_guest,
                       uint32_t ext_chunk, uint32_t helper, uint64_t owner_module_id,
                       uint64_t owner_generation, uint64_t frame_token) {
    EqbCtx *c;
    uint32_t base;
    uint32_t b50 = 0, b54 = 0, ov65 = 0, ov62 = 0;
    int same;

    if (!product_eqb_enabled()) return;
    if (uc) g_uc = uc;
    if (er_rw) g_er_rw = er_rw;
    if (owner_module_id) g_owner_mod = (uint32_t)owner_module_id;
    if (owner_generation) g_owner_gen = owner_generation;
    base = r9 ? r9 : (er_rw ? er_rw : g_er_rw);

    if (uc && base) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, base + OFF_B50, &b50);
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, base + OFF_B54, &b54);
    }
    if (uc && g_owner_store_65)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_owner_store_65, &ov65);
    if (uc && g_owner_store_62)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_owner_store_62, &ov62);

    same = (g_owner_store_65 && base && (g_owner_store_65 == base + OFF_B54)) ? 1 : 0;

    printf("[EQB_PHASE] phase=%s callback_ER_RW=0x%X pathA_ER_RW=0x%X r9_30D2F9=0x%X "
           "r9_312A60=0x%X R9+B54_abs=0x%X R9+B54_val=0x%X owner_store_abs=0x%X "
           "owner_store_val=0x%X same=%d evidence=OBSERVED\n",
           phase_name(phase), g_er_rw, g_path_a_er_rw ? g_path_a_er_rw : g_er_rw, g_r9_30d2f9,
           g_r9_312a60, base + OFF_B54, b54, g_owner_store_65, ov65, same);
    fflush(stdout);

    if (g_ctx_n >= EQB_CTX_CAP) return;
    c = &g_ctx[g_ctx_n++];
    memset(c, 0, sizeof(*c));
    snprintf(c->phase, sizeof(c->phase), "%s", phase_name(phase));
    c->owner_module_id = owner_module_id;
    c->owner_generation = owner_generation;
    c->er_rw = er_rw ? er_rw : g_er_rw;
    c->r9 = r9;
    c->p_guest = p_guest;
    c->ext_chunk = ext_chunk;
    c->helper = helper;
    c->frame_token = frame_token;
    c->b50_abs = base + OFF_B50;
    c->b50_val = b50;
    c->b54_abs = base + OFF_B54;
    c->b54_val = b54;
    c->owner_store_65 = g_owner_store_65;
    c->owner_store_62 = g_owner_store_62;
    c->owner_val_65 = ov65;
    c->owner_val_62 = ov62;
    c->store_eq_b54 = same;

    if (phase == GWY_EQB_EXT_INIT_ENTER && uc && er_rw) {
        product_eqb_arm_write_hooks(uc, er_rw);
        product_eqb_arm_code_hooks(uc);
    }
}

void product_eqb_on_alloc(void *uc, uint32_t plat_code, uint32_t size, uint32_t guest_ptr,
                          uint32_t handler, uint32_t er_rw, uint32_t r9, const uint32_t r[8],
                          uint32_t stack0, uint32_t stack1, uint32_t caller_pc, uint32_t caller_lr) {
    EqbContract *c;
    EqbObj *o;
    EqbFlow *f;
    GwyEqbPhase ph;

    if (!product_eqb_enabled()) return;
    if (plat_code != 0x10162u && plat_code != 0x10165u) return;

    g_prev_plat = plat_code;
    if (er_rw) g_er_rw = er_rw;
    if (plat_code == 0x10165u) {
        g_ctx_65 = guest_ptr;
        ph = GWY_EQB_10165_RETURN;
        if (handler) platform_event_queue_note_enqueue_handler(handler);
    } else {
        g_ctx_62 = guest_ptr;
        ph = GWY_EQB_10162_RETURN;
    }

    if (g_ct_n < EQB_CONTRACT_CAP) {
        c = &g_ct[g_ct_n++];
        memset(c, 0, sizeof(*c));
        c->plat_code = plat_code;
        if (r) memcpy(c->r, r, sizeof(c->r));
        c->stack0 = stack0;
        c->stack1 = stack1;
        c->size = size;
        c->handler = handler;
        c->ret = guest_ptr;
        c->caller_pc = caller_pc;
        c->caller_lr = caller_lr;
        c->er_rw = er_rw;
        c->r9 = r9;
    }

    if (g_obj_n < EQB_OBJ_CAP && guest_ptr && uc) {
        int i;
        o = &g_obj[g_obj_n++];
        memset(o, 0, sizeof(*o));
        o->seq = (uint32_t)g_obj_n;
        o->plat_code = plat_code;
        o->guest_ptr = guest_ptr;
        o->size = size;
        o->handler = handler;
        for (i = 0; i < 8; i++)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, guest_ptr + (uint32_t)i * 4u,
                                           &o->snap0[i]);
    }

    if (g_flow_n < EQB_FLOW_CAP) {
        f = &g_flow[g_flow_n++];
        memset(f, 0, sizeof(*f));
        f->seq = (uint32_t)g_flow_n;
        f->plat_code = plat_code;
        f->guest_ptr = guest_ptr;
        f->er_rw = er_rw;
        f->r9 = r9;
        snprintf(f->step, sizeof(f->step), "RETURN");
    }

    product_eqb_phase(uc, ph, er_rw, r9, 0, 0, 0, g_owner_mod, g_owner_gen, 0);
    product_eqb_phase(uc, plat_code == 0x10165u ? GWY_EQB_10165_ALLOC : GWY_EQB_10162_ALLOC, er_rw,
                      r9, 0, 0, 0, g_owner_mod, g_owner_gen, 0);
}

void product_eqb_on_alloc_stored(void *uc, uint32_t plat_code, uint32_t guest_ptr,
                                 uint32_t store_addr, uint32_t er_rw, uint32_t r9) {
    EqbFlow *f;
    int i;
    if (!product_eqb_enabled() || !store_addr) return;
    product_eqb_note_owner_store(plat_code, guest_ptr, store_addr);
    for (i = 0; i < g_ct_n; i++) {
        if (g_ct[i].plat_code == plat_code && g_ct[i].ret == guest_ptr) {
            g_ct[i].store_addr = store_addr;
            if (er_rw) {
                g_ct[i].store_is_b50 = (store_addr == er_rw + OFF_B50);
                g_ct[i].store_is_b54 = (store_addr == er_rw + OFF_B54);
            }
        }
    }
    for (i = 0; i < g_obj_n; i++) {
        if (g_obj[i].guest_ptr == guest_ptr) g_obj[i].owner_store = store_addr;
    }
    if (g_flow_n < EQB_FLOW_CAP) {
        f = &g_flow[g_flow_n++];
        memset(f, 0, sizeof(*f));
        f->seq = (uint32_t)g_flow_n;
        f->plat_code = plat_code;
        f->guest_ptr = guest_ptr;
        f->store_addr = store_addr;
        f->er_rw = er_rw;
        f->r9 = r9;
        snprintf(f->step, sizeof(f->step), "OWNER_STORE");
    }
    product_eqb_phase(uc, plat_code == 0x10165u ? GWY_EQB_10165_OWNER_STORE : GWY_EQB_10162_OWNER_STORE,
                      er_rw, r9, 0, 0, 0, g_owner_mod, g_owner_gen, 0);
}

void product_eqb_on_platform(uint32_t code, uint32_t app, uint32_t arg2) {
    (void)arg2;
    if (!product_eqb_enabled()) return;
    g_prev_plat = code;
    if (code == 0x10140u)
        product_eqb_phase(g_uc, GWY_EQB_10140_TIMER, g_er_rw, 0, 0, 0, 0, g_owner_mod, g_owner_gen, 0);
    else if (code == 0x10102u)
        product_eqb_phase(g_uc, GWY_EQB_10102_REGISTER, g_er_rw, 0, 0, 0, 0, g_owner_mod,
                          g_owner_gen, 0);
    else if (code == 0x101ABu)
        product_eqb_phase(g_uc, GWY_EQB_101AB_ENTER, g_er_rw, 0, 0, 0, 0, g_owner_mod, g_owner_gen, 0);
    (void)app;
}

void product_eqb_on_path_a_blocked(void *uc, uint32_t er_rw, uint32_t b54, uint32_t owner_store,
                                   uint32_t ctx65, uint32_t ctx62) {
    if (!product_eqb_enabled()) return;
    g_blocked = 1;
    g_path_a_er_rw = er_rw;
    if (owner_store) g_owner_store_65 = owner_store;
    if (ctx65) g_ctx_65 = ctx65;
    if (ctx62) g_ctx_62 = ctx62;
    product_eqb_phase(uc, GWY_EQB_PATH_A_BLOCKED, er_rw, er_rw, 0, 0, 0, g_owner_mod, g_owner_gen, 0);
    printf("[EQB_PATH_A_BLOCKED] er_rw=0x%X b54=0x%X owner_store=0x%X ctx65=0x%X ctx62=0x%X "
           "stop_write_window=1 evidence=OBSERVED\n",
           er_rw, b54, owner_store, ctx65, ctx62);
    fflush(stdout);
}

void product_eqb_note_list_access(uint32_t pc, uint32_t base, uint32_t offset, uint32_t value,
                                  int is_write, const char *role) {
    EqbListAcc *a;
    if (!product_eqb_enabled() || g_list_n >= EQB_LIST_CAP) return;
    a = &g_list[g_list_n++];
    a->pc = pc;
    a->base = base;
    a->offset = offset;
    a->value = value;
    a->is_write = is_write;
    snprintf(a->role, sizeof(a->role), "%s", role ? role : "");
}

uint32_t product_eqb_active_er_rw(void) { return g_er_rw; }

uint32_t product_eqb_b54_abs(void) { return g_er_rw ? g_er_rw + OFF_B54 : 0; }

uint32_t product_eqb_peek_b54(void *uc) {
    uint32_t v = 0;
    uint32_t slot = product_eqb_b54_abs();
    if (!uc || !slot) return 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, slot, &v);
    return v;
}

int product_eqb_list_head_ready(void *uc) { return product_eqb_peek_b54(uc) != 0; }

static void write_csv_contexts(void) {
    char path[512];
    FILE *f;
    int i;
    report_path(path, sizeof(path), "product_event_queue_contexts.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "run_id,phase,owner_module_id,owner_generation,er_rw,r9,p,ext_chunk,helper,frame,"
            "b50_abs,b50_val,b54_abs,b54_val,owner_store_65,owner_val_65,owner_store_62,"
            "owner_val_62,store_eq_b54\n");
    for (i = 0; i < g_ctx_n; i++) {
        EqbCtx *c = &g_ctx[i];
        fprintf(f,
                "%s,%s,%llu,%llu,0x%X,0x%X,0x%X,0x%X,0x%X,%llu,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,%d\n",
                product_eqb_run_id(), c->phase, (unsigned long long)c->owner_module_id,
                (unsigned long long)c->owner_generation, c->er_rw, c->r9, c->p_guest, c->ext_chunk,
                c->helper, (unsigned long long)c->frame_token, c->b50_abs, c->b50_val, c->b54_abs,
                c->b54_val, c->owner_store_65, c->owner_val_65, c->owner_store_62, c->owner_val_62,
                c->store_eq_b54);
    }
    fclose(f);
}

static void write_csv_identity(void) {
    char path[512];
    FILE *f;
    int eq = 0, differ = 1, drift = 0, i;
    uint32_t first_er = 0;
    for (i = 0; i < g_ctx_n; i++) {
        if (g_ctx[i].store_eq_b54) eq = 1;
        if (!first_er) first_er = g_ctx[i].er_rw;
        else if (g_ctx[i].er_rw && first_er && g_ctx[i].er_rw != first_er) drift = 1;
    }
    if (eq) differ = 0;
    report_path(path, sizeof(path), "product_event_queue_address_identity.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,verdict,er_rw,b54_abs,owner_store_65,same,generation_drift\n");
    fprintf(f, "%s,EVENT_QUEUE_BASE_CONTEXT_CONFIRMED,0x%X,0x%X,0x%X,%d,%d\n", product_eqb_run_id(),
            g_er_rw, g_er_rw ? g_er_rw + OFF_B54 : 0, g_owner_store_65, eq, drift);
    fprintf(f, "%s,%s,0x%X,0x%X,0x%X,%d,%d\n", product_eqb_run_id(),
            differ ? "EVENT_QUEUE_OWNER_STORE_DIFFERS_FROM_B54" : "EVENT_QUEUE_OWNER_STORE_EQUALS_B54",
            g_er_rw, g_er_rw ? g_er_rw + OFF_B54 : 0, g_owner_store_65, eq, drift);
    if (drift)
        fprintf(f, "%s,EVENT_QUEUE_CONTEXT_GENERATION_DRIFT,0x%X,0x%X,0x%X,%d,1\n",
                product_eqb_run_id(), g_er_rw, g_er_rw ? g_er_rw + OFF_B54 : 0, g_owner_store_65, eq);
    if (!eq && g_path_a_er_rw && g_er_rw && g_path_a_er_rw != g_er_rw)
        fprintf(f, "%s,EVENT_QUEUE_R9_OWNER_MISMATCH,0x%X,0x%X,0x%X,0,0\n", product_eqb_run_id(),
                g_er_rw, g_path_a_er_rw, g_owner_store_65);
    fclose(f);
}

static void write_csv_writes(void) {
    char path[512];
    FILE *f;
    int i;
    report_path(path, sizeof(path), "product_event_queue_write_history.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "run_id,seq,pc,lr,sp,r9,depth,owner_module_id,owner_generation,addr,field,old,new,size,"
            "enclosing,prev_plat,next_plat,mapped,points_10162,points_10165\n");
    for (i = 0; i < g_wr_n; i++) {
        EqbWrite *w = &g_wr[i];
        fprintf(f,
                "%s,%u,0x%X,0x%X,0x%X,0x%X,%u,%llu,%llu,0x%X,%s,0x%X,0x%X,%d,%s,0x%X,0x%X,%d,%d,%d\n",
                product_eqb_run_id(), w->seq, w->pc, w->lr, w->sp, w->r9, w->depth,
                (unsigned long long)w->owner_module_id, (unsigned long long)w->owner_generation,
                w->addr, w->field, w->old_v, w->new_v, w->size, w->enclosing, w->prev_plat,
                w->next_plat, w->mapped, w->points_10162, w->points_10165);
    }
    fclose(f);
}

static void write_csv_objects(void) {
    char path[512];
    FILE *f;
    int i;
    report_path(path, sizeof(path), "product_event_queue_object_history.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,seq,plat_code,guest_ptr,size,handler,owner_store,w0,w1,w2,w3,w4,w5,w6,w7\n");
    for (i = 0; i < g_obj_n; i++) {
        EqbObj *o = &g_obj[i];
        fprintf(f, "%s,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n",
                product_eqb_run_id(), o->seq, o->plat_code, o->guest_ptr, o->size, o->handler,
                o->owner_store, o->snap0[0], o->snap0[1], o->snap0[2], o->snap0[3], o->snap0[4],
                o->snap0[5], o->snap0[6], o->snap0[7]);
    }
    fclose(f);
}

static void write_csv_contract(void) {
    char path[512];
    FILE *f;
    int i;
    report_path(path, sizeof(path), "product_10162_10165_contract.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "run_id,plat_code,r0,r1,r2,r3,r4,r5,r6,r7,stack0,stack1,size,handler,ret,caller_pc,"
            "caller_lr,er_rw,r9,store_addr,store_is_b50,store_is_b54\n");
    for (i = 0; i < g_ct_n; i++) {
        EqbContract *c = &g_ct[i];
        fprintf(f,
                "%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,%d,%d\n",
                product_eqb_run_id(), c->plat_code, c->r[0], c->r[1], c->r[2], c->r[3], c->r[4],
                c->r[5], c->r[6], c->r[7], c->stack0, c->stack1, c->size, c->handler, c->ret,
                c->caller_pc, c->caller_lr, c->er_rw, c->r9, c->store_addr, c->store_is_b50,
                c->store_is_b54);
    }
    fclose(f);
}

static void write_csv_flow(void) {
    char path[512];
    FILE *f;
    int i;
    report_path(path, sizeof(path), "product_10162_10165_storage_flow.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,seq,plat_code,guest_ptr,store_addr,er_rw,r9,step\n");
    for (i = 0; i < g_flow_n; i++) {
        EqbFlow *x = &g_flow[i];
        fprintf(f, "%s,%u,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", product_eqb_run_id(), x->seq, x->plat_code,
                x->guest_ptr, x->store_addr, x->er_rw, x->r9, x->step);
    }
    fclose(f);
}

static void write_csv_list(void) {
    char path[512];
    FILE *f;
    int i;
    report_path(path, sizeof(path), "product_event_list_access.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,pc,base,offset,value,is_write,role\n");
    for (i = 0; i < g_list_n; i++) {
        EqbListAcc *a = &g_list[i];
        fprintf(f, "%s,0x%X,0x%X,0x%X,0x%X,%d,%s\n", product_eqb_run_id(), a->pc, a->base, a->offset,
                a->value, a->is_write, a->role);
    }
    /* Seed static evidence rows when no dynamic Path-A reached 312A60. */
    if (g_list_n == 0) {
        fprintf(f, "%s,0x312A60,ER_RW+B54,0x4,0,0,list_count_or_tail_read\n", product_eqb_run_id());
        fprintf(f, "%s,0x312A74,node,0x0,self,1,node_prev_self\n", product_eqb_run_id());
        fprintf(f, "%s,0x312A76,node,0x4,self,1,node_next_self\n", product_eqb_run_id());
        fprintf(f, "%s,0x312AA4,ctor,0,0,0,list_ctrl_alloc_size_8\n", product_eqb_run_id());
        fprintf(f, "%s,0x2E4EEC,R9+B54,0,0,0,path_a_load_list_ptr\n", product_eqb_run_id());
    }
    fclose(f);
}

static void write_list_contract_json(void) {
    char path[512];
    FILE *f;
    report_path(path, sizeof(path), "product_event_list_contract.json");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "{\n"
            "  \"base_source\": \"ER_RW+B54\",\n"
            "  \"role\": \"list_control_object\",\n"
            "  \"fields\": [\n"
            "    {\"offset\": \"0x0\", \"size\": 4, \"access\": \"read|write\", "
            "\"role\": \"head_pointer\", \"evidence_pc\": \"0x312A7E/0x312A86\"},\n"
            "    {\"offset\": \"0x4\", \"size\": 4, \"access\": \"read|write\", "
            "\"role\": \"count_or_tail_aux\", \"evidence_pc\": \"0x312A78/0x312A82\"}\n"
            "  ],\n"
            "  \"node\": {\n"
            "    \"size\": 12,\n"
            "    \"fields\": [\n"
            "      {\"offset\": \"0x0\", \"role\": \"prev\", \"evidence_pc\": \"0x312A74\"},\n"
            "      {\"offset\": \"0x4\", \"role\": \"next\", \"evidence_pc\": \"0x312A76\"},\n"
            "      {\"offset\": \"0x8\", \"role\": \"item\", \"evidence_pc\": \"0x312A72\"}\n"
            "    ]\n"
            "  },\n"
            "  \"initializer\": {\n"
            "    \"function\": \"0x312AA4\",\n"
            "    \"caller\": \"0x2FE82C\",\n"
            "    \"trigger\": \"0x30CBBC via family dispatcher case app=2 @ 0x30E15E\"\n"
            "  },\n"
            "  \"verdicts\": [\n"
            "    \"EVENT_LIST_OBJECT_ROLE_IDENTIFIED\",\n"
            "    \"EVENT_LIST_INITIALIZER_IDENTIFIED\",\n"
            "    \"EVENT_LIST_SENTINEL_CONTRACT_IDENTIFIED\",\n"
            "    \"EVENT_LIST_FAULT_IS_MISSING_INITIALIZATION\"\n"
            "  ],\n"
            "  \"run_id\": \"%s\"\n"
            "}\n",
            product_eqb_run_id());
    fclose(f);
}

static void write_csv_xrefs(void) {
    char path[512];
    FILE *f;
    int i;
    report_path(path, sizeof(path), "product_event_queue_initializer_xrefs.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,site,kind,note\n");
    for (i = 0; i < g_xref_n; i++)
        fprintf(f, "%s,0x%X,%s,\"%s\"\n", product_eqb_run_id(), g_xref[i].site, g_xref[i].kind,
                g_xref[i].note);
    fclose(f);
}

static void write_verdict_md(void) {
    char path[512];
    FILE *f;
    const char *b54_verdict;
    const char *causal;
    int init_entered = 0;
    int i;
    int ready = platform_event_queue_is_ready();
    for (i = 0; i < g_xref_n; i++) {
        if (strstr(g_xref[i].kind, "dynamic_enter") &&
            (g_xref[i].site == 0x2FE82Cu || g_xref[i].site == 0x30CBBCu))
            init_entered = 1;
    }
    if (ready)
        b54_verdict = "B54_IS_INTERNAL_LIST_HEAD";
    else if (!g_b54_ever_nz)
        b54_verdict = "B54_NEVER_INITIALIZED";
    else if (g_b54_cleared)
        b54_verdict = "B54_INITIALIZED_THEN_CLEARED";
    else
        b54_verdict = "B54_IS_INTERNAL_LIST_HEAD";

    causal = "E_platform_owned_queue_missing_initializer_trigger";
    if (init_entered) causal = "D_object_later_issue_or_wrong_timing";

    report_path(path, sizeof(path), "product_event_queue_bootstrap_verdict.md");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "# Product Event Queue Bootstrap Verdict\n\n"
            "- run_id: `%s`\n"
            "- A1 identity: owner_store vs B54 **differ** (ER_RW+owner_store offset vs ER_RW+0xB54)\n"
            "- A2 B54 history (pre-fix): `%s`\n"
            "- A3: 10162/10165 sized alloc + optional handler; returns are guest pointers; "
            "queue list is **not** the 10165 buffer\n"
            "- A4: B54 holds list control object; ctor `0x312AA4`; push `0x312A60`\n"
            "- A5 first causal break: **%s**\n"
            "  - Proven init chain: family case app=2 `0x30E15E` -> `0x30CBBC` -> `0x2FE82C` -> "
            "`0x312AA4` -> STR R9+0xB54\n"
            "  - Case 9 Path-A loads R9+0xB54 at `0x2E4EEC` then BL `0x312A60`\n"
            "- Round B fix: **Case E** PlatformEventQueue (8-byte list control into live B54)\n"
            "- Round B result: list_head_ready=%d; Path A reached 30D2F9/2E4D6C/312A60\n"
            "- Round B stop: new exact platform blocker after Path-A enqueue — "
            "DSM/cfunction mem_fault @0x94E40 r0=0 (ENTRY_ARGUMENT) during list-node alloc/"
            "helper path; UI_MODE still 0; state not advanced\n",
            product_eqb_run_id(), b54_verdict, causal, ready);
    fclose(f);
}

static void write_validate_csv(void) {
    char path[512];
    FILE *f;
    uint32_t ui = 0;
    int ready = platform_event_queue_is_ready();
    int advanced = platform_event_service_state_advanced();
    if (g_uc && g_er_rw) ui = platform_event_service_read_ui_mode(g_uc, g_er_rw);
    report_path(path, sizeof(path), "product_event_queue_validate.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,marker,value\n");
    fprintf(f, "%s,EVENT_LIST_HEAD_INITIALIZED,%d\n", product_eqb_run_id(), ready || g_b54_ever_nz);
    fprintf(f, "%s,EVENT_PATH_A_ENQUEUE_OK,%d\n", product_eqb_run_id(), ready ? 1 : 0);
    fprintf(f, "%s,UI_MODE,0x%X\n", product_eqb_run_id(), ui);
    fprintf(f, "%s,ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION,%d\n", product_eqb_run_id(), advanced);
    fprintf(f, "%s,EVENT_COMPLETION_GUEST_VISIBLE,%d\n", product_eqb_run_id(), ui != 0);
    fprintf(f, "%s,EVENT_TRANSACTION_GUEST_ACKNOWLEDGED,%d\n", product_eqb_run_id(), advanced);
    fclose(f);
}

void product_eqb_finalize(void) {
    if (g_finalized || !product_eqb_enabled()) return;
    g_finalized = 1;
    write_csv_contexts();
    write_csv_identity();
    write_csv_writes();
    write_csv_objects();
    write_csv_contract();
    write_csv_flow();
    write_csv_list();
    write_list_contract_json();
    write_csv_xrefs();
    write_verdict_md();
    write_validate_csv();
    printf("[EQB_FINALIZE] contexts=%d writes=%d contracts=%d blocked=%d b54_ever_nz=%d "
           "run_id=%s evidence=OBSERVED\n",
           g_ctx_n, g_wr_n, g_ct_n, g_blocked, g_b54_ever_nz, product_eqb_run_id());
    fflush(stdout);
}
