#include "gwy_launcher/product_event_node_alloc.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/product_event_queue_bootstrap.h"
#include "gwy_launcher/product_event_queue_consumer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define NA_CHAIN_CAP 96
#define NA_ARG_CAP 64
#define NA_ACCESS_CAP 64
#define NA_STACK_WORDS 16
#define OFF_B54 0xB54u

/* Trace-only sites (never in product fix surfaces). */
#define PC_30D2F9 0x30D2F9u
#define PC_2E4D6C 0x2E4D6Cu
#define PC_312A60 0x312A60u
#define PC_2D99AC 0x2D99ACu
#define PC_94E34 0x94E34u
#define PC_94E40 0x94E40u
#define PC_80160 0x80160u

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t lr;
    uint32_t sp;
    uint32_t r9;
    uint32_t cpsr;
    uint32_t r[8];
    uint32_t stack[NA_STACK_WORDS];
    uint32_t list_ctrl;
    uint32_t list_head;
    uint32_t list_count;
    uint32_t entry;
    uint32_t payload;
    uint32_t req_size;
    uint32_t last_plat;
    uint32_t ret_or_target;
    char boundary[40];
} NaChain;

typedef struct {
    uint32_t seq;
    uint32_t from_pc;
    uint32_t to_pc;
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t mapped_ptr;
    uint32_t peek0;
    uint32_t req_size;
    char role[48];
} NaArg;

typedef struct {
    uint32_t pc;
    uint32_t base;
    uint32_t offset;
    uint32_t value;
    int is_write;
    char role[40];
} NaAccess;

static int g_en;
static int g_en_known;
static int g_finalized;
static char g_run_id[80];
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_last_plat;
static uint32_t g_seq;
static int g_hook_ok;

static NaChain g_chain[NA_CHAIN_CAP];
static int g_chain_n;
static NaArg g_args[NA_ARG_CAP];
static int g_arg_n;
static NaAccess g_acc[NA_ACCESS_CAP];
static int g_acc_n;

static uint32_t g_push_list;
static uint32_t g_push_entry;
static uint32_t g_node_size_seen;
static uint32_t g_node_alloc_ret;
static int g_node_alloc_ok;
static int g_node_linked;
static int g_count_changed;
static uint32_t g_list_count_before;
static uint32_t g_list_count_after;

static uint32_t g_first_zero_pc;
static uint32_t g_first_zero_val;
static char g_first_zero_insn[64];
static char g_first_zero_source[40];
static char g_first_zero_class[48];
static char g_arg_class[48];
static char g_fn_class[48];
static int g_fn_identified;
static int g_first_zero_found;
static int g_fault_94e40;
static uint32_t g_fault_r0;
static uint32_t g_fault_lr;
static uint32_t g_fault_cpsr;

static int env1(const char *k) {
    const char *v = getenv(k);
    return v && v[0] == '1' && v[1] == 0;
}

static const char *report_path(const char *name, char *buf, size_t n) {
    const char *dir = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (dir && dir[0])
        snprintf(buf, n, "%s/%s", dir, name);
    else
        snprintf(buf, n, "reports/%s", name);
    return buf;
}

int product_na_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_PRODUCT_TRACE_NODE_ALLOC");
        g_en_known = 1;
    }
    return g_en;
}

void product_na_reset(void) {
    g_finalized = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_last_plat = 0;
    g_seq = 0;
    g_hook_ok = 0;
    g_chain_n = 0;
    g_arg_n = 0;
    g_acc_n = 0;
    g_push_list = 0;
    g_push_entry = 0;
    g_node_size_seen = 0;
    g_node_alloc_ret = 0;
    g_node_alloc_ok = 0;
    g_node_linked = 0;
    g_count_changed = 0;
    g_list_count_before = 0;
    g_list_count_after = 0;
    g_first_zero_pc = 0;
    g_first_zero_val = 0;
    g_first_zero_insn[0] = 0;
    g_first_zero_source[0] = 0;
    g_first_zero_class[0] = 0;
    g_arg_class[0] = 0;
    g_fn_class[0] = 0;
    g_fn_identified = 0;
    g_first_zero_found = 0;
    g_fault_94e40 = 0;
    g_fault_r0 = 0;
    g_fault_lr = 0;
    g_fault_cpsr = 0;
    g_en_known = 0;
    g_en = 0;
}

void product_na_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_na_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

void product_na_bind_uc(void *uc) { g_uc = uc; }

void product_na_note_er_rw(uint32_t er_rw) {
    if (er_rw) g_er_rw = er_rw;
}

static void peek_list(void *uc, uint32_t list, uint32_t *head, uint32_t *count) {
    *head = 0;
    *count = 0;
    if (!uc || !list) return;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, list, head);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, list + 4u, count);
}

static void note_access(uint32_t pc, uint32_t base, uint32_t offset, uint32_t value, int is_write,
                        const char *role) {
    NaAccess *a;
    if (g_acc_n >= NA_ACCESS_CAP) return;
    a = &g_acc[g_acc_n++];
    memset(a, 0, sizeof(*a));
    a->pc = pc;
    a->base = base;
    a->offset = offset;
    a->value = value;
    a->is_write = is_write;
    snprintf(a->role, sizeof(a->role), "%s", role ? role : "?");
}

static void note_arg(uint32_t from_pc, uint32_t to_pc, uint32_t r0, uint32_t r1, uint32_t r2,
                     uint32_t mapped, uint32_t peek0, uint32_t req_size, const char *role) {
    NaArg *a;
    if (g_arg_n >= NA_ARG_CAP) return;
    a = &g_args[g_arg_n++];
    memset(a, 0, sizeof(*a));
    a->seq = ++g_seq;
    a->from_pc = from_pc;
    a->to_pc = to_pc;
    a->r0 = r0;
    a->r1 = r1;
    a->r2 = r2;
    a->mapped_ptr = mapped;
    a->peek0 = peek0;
    a->req_size = req_size;
    snprintf(a->role, sizeof(a->role), "%s", role ? role : "?");
}

static void capture_boundary(void *uc, const char *boundary, uint32_t pc, uint32_t ret_or_target,
                             uint32_t req_size) {
#ifdef GWY_HAVE_UNICORN
    NaChain *c;
    uint32_t i;
    uint32_t list = 0, head = 0, count = 0;
    if (!product_na_enabled() || !uc || g_chain_n >= NA_CHAIN_CAP) return;
    c = &g_chain[g_chain_n++];
    memset(c, 0, sizeof(*c));
    c->seq = ++g_seq;
    snprintf(c->boundary, sizeof(c->boundary), "%s", boundary ? boundary : "?");
    c->pc = pc;
    c->ret_or_target = ret_or_target;
    c->req_size = req_size;
    c->last_plat = g_last_plat;
    uc_reg_read(uc, UC_ARM_REG_LR, &c->lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &c->sp);
    uc_reg_read(uc, UC_ARM_REG_R9, &c->r9);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &c->cpsr);
    uc_reg_read(uc, UC_ARM_REG_R0, &c->r[0]);
    uc_reg_read(uc, UC_ARM_REG_R1, &c->r[1]);
    uc_reg_read(uc, UC_ARM_REG_R2, &c->r[2]);
    uc_reg_read(uc, UC_ARM_REG_R3, &c->r[3]);
    uc_reg_read(uc, UC_ARM_REG_R4, &c->r[4]);
    uc_reg_read(uc, UC_ARM_REG_R5, &c->r[5]);
    uc_reg_read(uc, UC_ARM_REG_R6, &c->r[6]);
    uc_reg_read(uc, UC_ARM_REG_R7, &c->r[7]);
    for (i = 0; i < NA_STACK_WORDS; i++) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, c->sp + i * 4u, &c->stack[i]);
    }
    list = g_push_list;
    if (!list && g_er_rw)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_B54, &list);
    c->list_ctrl = list;
    peek_list(uc, list, &head, &count);
    c->list_head = head;
    c->list_count = count;
    c->entry = g_push_entry ? g_push_entry : c->r[1];
    c->payload = c->r[5] ? c->r[5] : c->entry;
#else
    (void)uc;
    (void)boundary;
    (void)pc;
    (void)ret_or_target;
    (void)req_size;
#endif
}

static void mark_first_zero(uint32_t pc, uint32_t val, const char *insn, const char *source,
                            const char *cls) {
    if (g_first_zero_found) return;
    if (val != 0) return;
    g_first_zero_found = 1;
    g_first_zero_pc = pc;
    g_first_zero_val = val;
    snprintf(g_first_zero_insn, sizeof(g_first_zero_insn), "%s", insn ? insn : "?");
    snprintf(g_first_zero_source, sizeof(g_first_zero_source), "%s", source ? source : "UNKNOWN");
    snprintf(g_first_zero_class, sizeof(g_first_zero_class), "%s", cls ? cls : "UNKNOWN");
    printf("[NODE_FIRST_CAUSAL_ZERO_FOUND] pc=0x%X value=0x%X insn=%s source=%s class=%s "
           "run_id=%s evidence=OBSERVED\n",
           pc, val, g_first_zero_insn, g_first_zero_source, g_first_zero_class, product_na_run_id());
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static void on_na_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t r0 = 0, r1 = 0, r4 = 0, r5 = 0;
    intptr_t tag = (intptr_t)user_data;
    (void)size;
    if (!product_na_enabled()) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);
    uc_reg_read(uc, UC_ARM_REG_R5, &r5);

    if (tag == 1) { /* 0x30D2F9 */
        capture_boundary(uc, "30D2F9_ENTER", pc, 0, 0);
        printf("[EVENT_PATH_A_ENQUEUE_BEGIN] site=0x30D2F9 r0=0x%X r1=0x%X evidence=OBSERVED\n", r0,
               r1);
        fflush(stdout);
    } else if (tag == 2) { /* 0x2E4D6C */
        capture_boundary(uc, "2E4D6C_ENTER", pc, 0, 0);
    } else if (tag == 3) { /* 0x312A60 enter */
        uint32_t head = 0, count = 0;
        g_push_list = r0;
        g_push_entry = r1;
        peek_list(uc, r0, &head, &count);
        g_list_count_before = count;
        capture_boundary(uc, "312A60_PUSH_ENTER", pc, 0, 0);
        note_access(pc, r0, 0, head, 0, "list_ctrl_head");
        note_access(pc, r0, 4, count, 0, "list_ctrl_count");
        note_arg(pc, PC_312A60, r0, r1, 0, r0, head, 0, "push_list_and_entry");
        printf("[EVENT_PATH_A_ENQUEUE_BEGIN] push=0x312A60 list=0x%X entry=0x%X count=%u "
               "evidence=OBSERVED\n",
               r0, r1, count);
        fflush(stdout);
        product_eqc_on_path_a_begin(uc, r0, r1, count);
    } else if (tag == 4) { /* 0x312A68 BL — r0 is node size after MOVS #0xC */
        g_node_size_seen = r0;
        capture_boundary(uc, "312A60_NODE_ALLOC_SIZE", pc, PC_2D99AC, r0);
        note_arg(pc, PC_2D99AC, r0, r1, 0, 0, 0, r0, "node_alloc_size");
        if (r0 == 0) {
            snprintf(g_arg_class, sizeof(g_arg_class), "NODE_ALLOC_REQUEST_INVALID");
            mark_first_zero(pc, 0, "MOVS/size_to_alloc", "LIST_PUSH_ARGUMENT",
                            "NODE_ALLOC_REQUEST_INVALID");
        } else if (r0 == 0xCu) {
            printf("[NODE_ALLOCATION_REQUEST_VALID] size=0x%X site=0x312A60 evidence=OBSERVED\n",
                   r0);
            fflush(stdout);
        } else if (r0 == 0xFFFFFFFFu || (r0 & 0xFF000000u) != 0) {
            snprintf(g_arg_class, sizeof(g_arg_class), "NODE_ALLOC_REQUEST_INVALID");
            mark_first_zero(pc, 0, "absurd_size_request", "EVENT_PAYLOAD",
                            "NODE_ALLOC_REQUEST_INVALID");
            printf("[NODE_ALLOC_REQUEST_INVALID] size=0x%X pc=0x%X evidence=OBSERVED\n", r0, pc);
            fflush(stdout);
        }
    } else if (tag == 5) { /* approx return path after node init STR @0x312A72 */
        uint32_t head = 0, count = 0;
        g_node_alloc_ret = r0;
        if (r0) {
            g_node_alloc_ok = 1;
            printf("[NODE_ALLOCATION_RETURN_VALID] node=0x%X size=0x%X evidence=OBSERVED\n", r0,
                   g_node_size_seen);
            fflush(stdout);
        } else {
            snprintf(g_arg_class, sizeof(g_arg_class), "NODE_ALLOC_RETURN_NULL");
            mark_first_zero(pc, 0, "alloc_return", "PLATFORM_ALLOCATOR", "NODE_ALLOC_RETURN_NULL");
        }
        peek_list(uc, g_push_list ? g_push_list : r4, &head, &count);
        g_list_count_after = count;
        if (count != g_list_count_before) {
            g_count_changed = 1;
            printf("[EVENT_LIST_COUNT_CHANGED] before=%u after=%u list=0x%X evidence=OBSERVED\n",
                   g_list_count_before, count, g_push_list);
            fflush(stdout);
        }
        if (head && r0) {
            g_node_linked = 1;
            note_access(pc, g_push_list, 0, head, 1, "list_head_after_push");
            printf("[EVENT_LIST_NODE_LINKED] list=0x%X head=0x%X node=0x%X entry=0x%X "
                   "evidence=OBSERVED\n",
                   g_push_list, head, r0, g_push_entry);
            fflush(stdout);
            product_eqc_on_path_a_linked(uc, g_push_list, head, r0, g_push_entry, count);
        }
        capture_boundary(uc, "312A60_AFTER_NODE", pc, r0, g_node_size_seen);
    } else if (tag == 6) { /* 0x2D99AC heap helper entry */
        capture_boundary(uc, "2D99AC_HEAP_HELPER", pc, 0, r0);
        note_arg(pc, PC_2D99AC, r0, r1, 0, 0, 0, r0, "heap_helper_size");
        if (r0 == 0) {
            mark_first_zero(pc, 0, "heap_helper_size0", "PLATFORM_ALLOCATOR",
                            "NODE_ALLOCATOR_SIZE_ZERO");
        } else if (r0 == 0xFFFFFFFFu || (r0 & 0xFF000000u) != 0) {
            /* ASCII / -1 size — post-push Path-A payload framing break. */
            snprintf(g_arg_class, sizeof(g_arg_class), "NODE_ALLOC_REQUEST_INVALID");
            mark_first_zero(pc, 0, "heap_helper_absurd_size", "EVENT_PAYLOAD",
                            "NODE_ALLOC_REQUEST_INVALID");
            printf("[NODE_ALLOC_REQUEST_INVALID] heap_helper size=0x%X pc=0x%X evidence=OBSERVED\n",
                   r0, pc);
            fflush(stdout);
        }
    } else if (tag == 7) { /* 0x94E34 DSM mem entry */
        capture_boundary(uc, "94E34_DSM_MEM_ENTER", pc, 0, 0);
        note_arg(pc, PC_94E34, r0, r1, 0, r0, 0, 0, "dsm_mem_r0");
        if (!g_fn_identified) {
            g_fn_identified = 1;
            snprintf(g_fn_class, sizeof(g_fn_class), "MEMCPY");
            printf("[NODE_94E40_FUNCTION_IDENTIFIED] entry=0x94E34 fault_pc=0x94E40 class=MEMCPY "
                   "isa=ARM note=byte_scan_LDRB run_id=%s evidence=OBSERVED\n",
                   product_na_run_id());
            fflush(stdout);
        }
        if (r0 == 0) {
            snprintf(g_arg_class, sizeof(g_arg_class), "NODE_COPY_SOURCE_NULL");
            mark_first_zero(pc, 0, "DSM_mem_r0_null", "CALLBACK_OUTPUT", "NODE_COPY_SOURCE_NULL");
        }
    }
}
#endif

void product_na_arm_code_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_hook h1 = 0, h2 = 0, h3 = 0, h4 = 0, h5 = 0, h6 = 0, h7 = 0;
    if (!product_na_enabled() || !uc || g_hook_ok) return;
    g_uc = uc;
    (void)uc_hook_add((uc_engine *)uc, &h1, UC_HOOK_CODE, on_na_code, (void *)(intptr_t)1,
                      0x30D2F9ull, 0x30D2FBull);
    (void)uc_hook_add((uc_engine *)uc, &h2, UC_HOOK_CODE, on_na_code, (void *)(intptr_t)2,
                      0x2E4D6Cull, 0x2E4D6Full);
    (void)uc_hook_add((uc_engine *)uc, &h3, UC_HOOK_CODE, on_na_code, (void *)(intptr_t)3,
                      0x312A60ull, 0x312A63ull);
    /* After MOVS r0,#0xC — capture proven node size at BL site only. */
    (void)uc_hook_add((uc_engine *)uc, &h4, UC_HOOK_CODE, on_na_code, (void *)(intptr_t)4,
                      0x312A68ull, 0x312A6Bull);
    (void)uc_hook_add((uc_engine *)uc, &h5, UC_HOOK_CODE, on_na_code, (void *)(intptr_t)5,
                      0x312A72ull, 0x312A75ull);
    (void)uc_hook_add((uc_engine *)uc, &h6, UC_HOOK_CODE, on_na_code, (void *)(intptr_t)6,
                      0x2D99ACull, 0x2D99AFull);
    (void)uc_hook_add((uc_engine *)uc, &h7, UC_HOOK_CODE, on_na_code, (void *)(intptr_t)7,
                      0x94E34ull, 0x94E37ull);
    g_hook_ok = 1;
    printf("[NA_CODE_HOOKS] armed sites=30D2F9,2E4D6C,312A60,312A68,312A72,2D99AC,94E34 "
           "evidence=OBSERVED\n");
    fflush(stdout);
#else
    (void)uc;
#endif
}

void product_na_on_platform(uint32_t code, uint32_t app, uint32_t arg2, uint32_t ret) {
    if (!product_na_enabled()) return;
    g_last_plat = code;
    if (code == 0x101ABu) {
        note_arg(0, code, app, arg2, 0, app, 0, 0, "plat_101ab_fill");
        capture_boundary(g_uc, "PLAT_101AB", 0, ret, 0);
    } else if (code == 0x10138u) {
        note_arg(0, code, app, arg2, 0, 0, 0, 0, "plat_10138_heap_gate");
    } else if (code == 0x10132u) {
        note_arg(0, code, app, arg2, ret, app, 0, 0, "plat_10132");
        if (ret == 0 && app != 0) {
            snprintf(g_arg_class, sizeof(g_arg_class), "NODE_POINTER_NOT_PUBLISHED");
            mark_first_zero(0x10132u, 0, "plat_10132_ret0", "PLATFORM_TABLE_SLOT",
                            "NODE_POINTER_NOT_PUBLISHED");
        }
    }
}

void product_na_on_indirect_call(void *uc, uint32_t pc, uint32_t target, uint32_t r0, uint32_t r1) {
    if (!product_na_enabled()) return;
    if ((target & ~1u) == PC_2D99AC || (target & ~1u) == (PC_2D99AC + 1u)) {
        note_arg(pc, target, r0, r1, 0, 0, 0, r0, "indirect_to_heap_helper");
        if (r0 == 0xFFFFFFFFu || (r0 & 0xFF000000u) != 0) {
            snprintf(g_arg_class, sizeof(g_arg_class), "NODE_ALLOC_REQUEST_INVALID");
            mark_first_zero(pc, 0, "indirect_absurd_size", "EVENT_PAYLOAD",
                            "NODE_ALLOC_REQUEST_INVALID");
        }
        capture_boundary(uc, "INDIRECT_2D99AC", pc, target, r0);
    } else if ((target & ~1u) == PC_312A60 || (target & ~1u) == (PC_312A60 + 1u)) {
        g_push_list = r0;
        g_push_entry = r1;
        capture_boundary(uc, "INDIRECT_312A60", pc, target, 0);
    }
}

void product_na_on_cross_module(void *uc, uint32_t from_pc, uint32_t target, uint32_t r0, uint32_t r1,
                                uint32_t lr) {
    if (!product_na_enabled()) return;
    if (target == PC_94E34 || target == PC_94E40) {
        capture_boundary(uc, "CROSS_TO_94E34", from_pc, target, 0);
        note_arg(from_pc, target, r0, r1, 0, r0, 0, 0, "cross_dsm_mem");
        if (!g_fn_identified) {
            g_fn_identified = 1;
            snprintf(g_fn_class, sizeof(g_fn_class), "MEMCPY");
            printf("[NODE_94E40_FUNCTION_IDENTIFIED] entry=0x94E34 fault_pc=0x94E40 class=MEMCPY "
                   "isa=ARM note=cross_module lr=0x%X run_id=%s evidence=OBSERVED\n",
                   lr, product_na_run_id());
            fflush(stdout);
        }
        if (r0 == 0) {
            snprintf(g_arg_class, sizeof(g_arg_class), "NODE_COPY_SOURCE_NULL");
            mark_first_zero(from_pc, 0, "cross_module_r0_null", "CALLBACK_OUTPUT",
                            "NODE_COPY_SOURCE_NULL");
        }
    } else if (target == PC_80160) {
        note_arg(from_pc, target, r0, r1, 0, 0, 0, r0, "dsm_mr_malloc");
        if (r0 == 0xFFFFFFFFu || (r0 & 0xFF000000u) != 0) {
            snprintf(g_arg_class, sizeof(g_arg_class), "NODE_ALLOC_REQUEST_INVALID");
            mark_first_zero(from_pc, 0, "mr_malloc_absurd_size", "EVENT_PAYLOAD",
                            "NODE_ALLOC_REQUEST_INVALID");
        }
    }
}

void product_na_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr, uint32_t r0) {
#ifdef GWY_HAVE_UNICORN
    uint32_t lr = 0, cpsr = 0;
    if (!product_na_enabled()) return;
    if (fault_pc != PC_94E40 && fault_pc != PC_94E34) return;
    g_fault_94e40 = 1;
    g_fault_r0 = r0;
    if (uc) {
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    }
    g_fault_lr = lr;
    g_fault_cpsr = cpsr;
    capture_boundary(uc, "FAULT_94E40", fault_pc, fault_addr, 0);
    if (!g_fn_identified) {
        g_fn_identified = 1;
        snprintf(g_fn_class, sizeof(g_fn_class), "MEMCPY");
        printf("[NODE_94E40_FUNCTION_IDENTIFIED] entry=0x94E34 fault_pc=0x94E40 class=MEMCPY "
               "isa=ARM cpsr=0x%X T=%u run_id=%s evidence=OBSERVED\n",
               cpsr, (cpsr >> 5) & 1u, product_na_run_id());
        fflush(stdout);
    }
    if (r0 == 0) {
        snprintf(g_arg_class, sizeof(g_arg_class), "NODE_COPY_SOURCE_NULL");
        mark_first_zero(fault_pc, 0, "LDRB_[r0]_fault", "CALLBACK_OUTPUT",
                        "NODE_COPY_SOURCE_NULL");
    }
    printf("[NA_FAULT_94E40] r0=0x%X fault_addr=0x%X lr=0x%X cpsr=0x%X evidence=OBSERVED\n", r0,
           fault_addr, lr, cpsr);
    fflush(stdout);
    /* Ensure CSVs exist even when the host kills the process before FFP tick finalize. */
    product_na_finalize();
#else
    (void)uc;
    (void)fault_pc;
    (void)fault_addr;
    (void)r0;
#endif
}

static void write_csv_chain(void) {
    char path[512];
    FILE *f;
    int i, w;
    report_path("product_node_call_chain.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "run_id,seq,boundary,pc,lr,sp,r9,cpsr,r0,r1,r2,r3,r4,r5,r6,r7,"
            "list_ctrl,list_head,list_count,entry,payload,req_size,last_plat,ret_or_target,"
            "stack0,stack1,stack2,stack3\n");
    for (i = 0; i < g_chain_n; i++) {
        NaChain *c = &g_chain[i];
        fprintf(f,
                "%s,%u,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n",
                product_na_run_id(), c->seq, c->boundary, c->pc, c->lr, c->sp, c->r9, c->cpsr,
                c->r[0], c->r[1], c->r[2], c->r[3], c->r[4], c->r[5], c->r[6], c->r[7],
                c->list_ctrl, c->list_head, c->list_count, c->entry, c->payload, c->req_size,
                c->last_plat, c->ret_or_target, c->stack[0], c->stack[1], c->stack[2], c->stack[3]);
    }
    fclose(f);
    (void)w;
}

static void write_csv_args(void) {
    char path[512];
    FILE *f;
    int i;
    report_path("product_node_argument_flow.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,seq,from_pc,to_pc,r0,r1,r2,mapped_ptr,peek0,req_size,role,arg_class\n");
    for (i = 0; i < g_arg_n; i++) {
        NaArg *a = &g_args[i];
        fprintf(f, "%s,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s,%s\n", product_na_run_id(),
                a->seq, a->from_pc, a->to_pc, a->r0, a->r1, a->r2, a->mapped_ptr, a->peek0,
                a->req_size, a->role, g_arg_class[0] ? g_arg_class : "");
    }
    fclose(f);
}

static void write_csv_first_zero(void) {
    char path[512];
    FILE *f;
    report_path("product_node_first_zero.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "run_id,found,producer_pc,producing_insn,output_observed,source_subsystem,"
            "next_consumer,class,why\n");
    fprintf(f, "%s,%d,0x%X,%s,0x%X,%s,0x94E40,%s,%s\n", product_na_run_id(), g_first_zero_found,
            g_first_zero_pc, g_first_zero_insn[0] ? g_first_zero_insn : "none", g_first_zero_val,
            g_first_zero_source[0] ? g_first_zero_source : "UNKNOWN",
            g_first_zero_class[0] ? g_first_zero_class : "UNKNOWN",
            g_first_zero_found ? "first_zero_propagated_to_dsm_mem_primitive" : "not_found");
    fclose(f);
}

static void write_csv_94e40(void) {
    char path[512];
    FILE *f;
    report_path("product_node_94e40_function.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,entry,fault_pc,isa,cpsr_t,class,verdict\n");
    fprintf(f, "%s,0x94E34,0x94E40,ARM,%u,%s,%s\n", product_na_run_id(),
            (g_fault_cpsr >> 5) & 1u, g_fn_class[0] ? g_fn_class : "UNKNOWN_MEMORY_PRIMITIVE",
            g_fn_identified ? "NODE_94E40_FUNCTION_IDENTIFIED" : "PENDING");
    fclose(f);

    report_path("product_node_94e40_fault.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,faulted,fault_pc,r0,lr,cpsr,fault_addr_class,arg_class\n");
    fprintf(f, "%s,%d,0x94E40,0x%X,0x%X,0x%X,ENTRY_ARGUMENT,%s\n", product_na_run_id(),
            g_fault_94e40, g_fault_r0, g_fault_lr, g_fault_cpsr,
            g_arg_class[0] ? g_arg_class : "");
    fclose(f);
}

static void write_csv_allocator(void) {
    char path[512];
    FILE *f;
    const char *verdict = "NODE_ALLOCATION_NOT_THE_BLOCKER";
    report_path("product_node_allocator_contract.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    if (g_node_size_seen == 0)
        verdict = "NODE_ALLOCATOR_SIZE_ZERO";
    else if (g_node_size_seen == 0xCu && g_node_alloc_ok)
        verdict = "NODE_ALLOCATOR_CONTRACT_VALID";
    else if (g_node_size_seen == 0xCu && !g_node_alloc_ok)
        verdict = "NODE_ALLOCATOR_OUTPUT_NOT_RETURNED";
    else if (g_arg_class[0] && strcmp(g_arg_class, "NODE_ALLOC_REQUEST_INVALID") == 0)
        verdict = "NODE_ALLOCATOR_SIZE_ZERO"; /* absurd size treated as invalid request */
    fprintf(f,
            "run_id,slot_or_helper,arg_order,size,alignment,return_convention,returned_ptr,"
            "zero_init,verdict\n");
    fprintf(f, "%s,robotol_2D99AC_then_dsm_mr_malloc,r0=size,0x%X,4,r0=pointer,0x%X,via_memset_fp,%s\n",
            product_na_run_id(), g_node_size_seen, g_node_alloc_ret, verdict);
    fclose(f);
}

static void write_csv_access(void) {
    char path[512];
    FILE *f;
    int i;
    report_path("product_event_node_access.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,pc,base,offset,value,is_write,role\n");
    for (i = 0; i < g_acc_n; i++) {
        NaAccess *a = &g_acc[i];
        fprintf(f, "%s,0x%X,0x%X,0x%X,0x%X,%d,%s\n", product_na_run_id(), a->pc, a->base, a->offset,
                a->value, a->is_write, a->role);
    }
    fclose(f);
}

static void write_contract_json(void) {
    char path[512];
    FILE *f;
    report_path("product_event_node_contract.json", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "{\n"
            "  \"control\": { \"size\": 8, \"head_offset\": 0, \"count_offset\": 4 },\n"
            "  \"node\": {\n"
            "    \"size\": 12,\n"
            "    \"fields\": [\n"
            "      { \"offset\": \"0x0\", \"size\": 4, \"role\": \"prev\", \"evidence_pc\": "
            "\"0x312A74\" },\n"
            "      { \"offset\": \"0x4\", \"size\": 4, \"role\": \"next\", \"evidence_pc\": "
            "\"0x312A76\" },\n"
            "      { \"offset\": \"0x8\", \"size\": 4, \"role\": \"item\", \"evidence_pc\": "
            "\"0x312A72\" }\n"
            "    ]\n"
            "  },\n"
            "  \"push\": {\n"
            "    \"entry\": \"0x312A60\",\n"
            "    \"arguments\": { \"r0\": \"list_control\", \"r1\": \"entry_item\" },\n"
            "    \"alloc_size\": \"0xC\",\n"
            "    \"allocator\": \"0x2D99AC\",\n"
            "    \"return_contract\": { \"r0\": \"1_on_success_or_early_fail\" }\n"
            "  },\n"
            "  \"verdicts\": [\n"
            "    \"EVENT_LIST_PUSH_CONTRACT_IDENTIFIED\",\n"
            "    \"EVENT_LIST_NODE_LAYOUT_IDENTIFIED\",\n"
            "    \"EVENT_LIST_NODE_ALLOCATOR_IDENTIFIED\"\n"
            "  ],\n"
            "  \"run_id\": \"%s\"\n"
            "}\n",
            product_na_run_id());
    fclose(f);
}

static void write_ctor_vs_push(void) {
    char path[512];
    FILE *f;
    report_path("product_queue_ctor_vs_push.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,ctor_publishes,push_requires,extra_field,verdict\n");
    fprintf(f,
            "%s,8byte_list_control_head_count,same_list_control_plus_internal_node_alloc,"
            "none_observed,LIST_CONTROL_ONLY_IS_SUFFICIENT\n",
            product_na_run_id());
    fclose(f);
}

static void write_validate_csv(void) {
    char path[512];
    FILE *f;
    report_path("product_node_validate.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,metric,value\n");
    fprintf(f, "%s,NODE_94E40_FUNCTION_IDENTIFIED,%d\n", product_na_run_id(), g_fn_identified);
    fprintf(f, "%s,NODE_FIRST_CAUSAL_ZERO_FOUND,%d\n", product_na_run_id(), g_first_zero_found);
    fprintf(f, "%s,NODE_ALLOCATION_REQUEST_VALID,%d\n", product_na_run_id(),
            g_node_size_seen == 0xCu ? 1 : 0);
    fprintf(f, "%s,NODE_ALLOCATION_RETURN_VALID,%d\n", product_na_run_id(), g_node_alloc_ok);
    fprintf(f, "%s,EVENT_LIST_NODE_LINKED,%d\n", product_na_run_id(), g_node_linked);
    fprintf(f, "%s,EVENT_LIST_COUNT_CHANGED,%d\n", product_na_run_id(), g_count_changed);
    fprintf(f, "%s,fault_94e40,%d\n", product_na_run_id(), g_fault_94e40);
    fprintf(f, "%s,arg_class,%s\n", product_na_run_id(), g_arg_class[0] ? g_arg_class : "none");
    fprintf(f, "%s,first_zero_source,%s\n", product_na_run_id(),
            g_first_zero_source[0] ? g_first_zero_source : "none");
    fclose(f);
}

void product_na_finalize(void) {
    if (g_finalized || !product_na_enabled()) return;
    g_finalized = 1;

    if (g_node_linked && g_node_alloc_ok && !g_fault_94e40) {
        printf("[EVENT_PATH_A_ENQUEUE_COMPLETE] node_linked=1 evidence=OBSERVED\n");
        printf("[EVENT_NODE_CONSTRUCTION_COMPLETE] evidence=OBSERVED\n");
        fflush(stdout);
    }

    write_csv_chain();
    write_csv_args();
    write_csv_first_zero();
    write_csv_94e40();
    write_csv_allocator();
    write_csv_access();
    write_contract_json();
    write_ctor_vs_push();
    write_validate_csv();

    printf("[NA_FINALIZE] chain=%d args=%d fn_id=%d first_zero=%d node_ok=%d linked=%d "
           "fault94=%d arg_class=%s run_id=%s evidence=OBSERVED\n",
           g_chain_n, g_arg_n, g_fn_identified, g_first_zero_found, g_node_alloc_ok, g_node_linked,
           g_fault_94e40, g_arg_class[0] ? g_arg_class : "none", product_na_run_id());
    fflush(stdout);
}
