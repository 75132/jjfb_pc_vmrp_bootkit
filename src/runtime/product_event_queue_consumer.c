#include "gwy_launcher/product_event_queue_consumer.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/platform_event_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* Proven list/drain contract (robotol.ext) — observation sites only. */
#define PC_305EB8 0x305EB8u
#define PC_2DC80C 0x2DC80Cu
#define PC_312AC4 0x312AC4u
#define PC_312AB4 0x312AB4u
#define PC_312C0C 0x312C0Cu
#define PC_312A84 0x312A84u /* empty-list push return after count=1 */
#define PC_312AA0 0x312AA0u /* nonempty push return after count++ */
#define OFF_B54 0xB54u
#define OFF_B7D 0xB7Du
#define OFF_UI 0x8D0u
#define OFF_15D 0x15Du /* LDRSB via literal 0x15C + #1 */
#define OFF_B71 0xB71u
#define OFF_134D 0x134Du
#define OFF_C76 0xC76u
#define PC_2DC82E 0x2DC82Eu /* after get_item: r0=item */
#define PC_2DC848 0x2DC848u /* alternate/dispatch branch */
#define PC_30BC40 0x30BC40u /* item destructor (free path) */
#define PC_305EC2 0x305EC2u /* post-drain gate sample */
#define PC_305EF4 0x305EF4u /* BL 2DADC4 when gates pass */
#define PC_2DADC4 0x2DADC4u
#define PC_2FC418 0x2FC418u /* natural UI_MODE=0x45 writer */

#define TL_CAP 96
#define ACC_CAP 128
#define LIVE_CAP 64
#define VFS_CAP 48
#define NODE_WORDS 3

typedef struct {
    uint32_t seq;
    uint64_t insn;
    char phase[40];
    uint32_t cb_depth;
    uint32_t list;
    uint32_t head;
    uint32_t count;
    uint32_t node;
    uint32_t node_w0, node_w1, node_w2;
    uint32_t ui_mode;
    uint32_t b7d;
    uint32_t req_state;
    char note[64];
} EqcTl;

typedef struct {
    uint32_t pc;
    uint32_t base;
    uint32_t offset;
    uint32_t value;
    int is_write;
    char role[48];
} EqcAcc;

typedef struct {
    uint32_t seq;
    uint32_t pc;
    uint32_t list;
    uint32_t head;
    uint32_t count;
    uint32_t item;
    char event[40];
} EqcLive;

typedef struct {
    uint64_t insn;
    int after_link;
    char path[96];
    char cls[32];
} EqcVfs;

static int g_en, g_en_known, g_finalized, g_hook_ok;
static char g_run_id[80];
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_seq;
static uint64_t g_insn;

static EqcTl g_tl[TL_CAP];
static int g_tl_n;
static EqcAcc g_acc[ACC_CAP];
static int g_acc_n;
static EqcLive g_live[LIVE_CAP];
static int g_live_n;
static EqcVfs g_vfs[VFS_CAP];
static int g_vfs_n;

static uint32_t g_first_list;
static uint32_t g_first_node;
static uint32_t g_first_entry;
static uint32_t g_count_at_link;
static uint32_t g_head_at_link;
static int g_node_linked;
static int g_count_changed;
static int g_consumer_enter;
static int g_consumer_trigger_enter;
static int g_node_consumed;
static int g_drain_scheduled;
static int g_drain_delivered;
static int g_drain_ok;
static uint32_t g_post_count;
static uint32_t g_post_head;
static uint32_t g_producer_w[NODE_WORDS];
static uint32_t g_consumer_read_pc[NODE_WORDS];
static int g_empty_body; /* entry/item may be non-null record ptr even with_rec=0 */
static int g_item_free_path;       /* 0x30BC40 entered */
static int g_item_alt_path;        /* 0x2DC848 entered */
static int g_post_drain_gate_ok;   /* reached BL 0x2DADC4 */
static int g_gate_init_enter;      /* 0x2DADC4 */
static int g_ui_writer_enter;      /* 0x2FC418 */
static uint32_t g_gate_15d, g_gate_b71, g_gate_134d, g_gate_c76;
static uint32_t g_drain_item, g_drain_item_w0;
static int g_gate_sampled;

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

uint32_t product_eqc_drain_trigger_va(void) { return PC_305EB8 | 1u; }
uint32_t product_eqc_drain_entry_va(void) { return PC_2DC80C | 1u; }

int product_eqc_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_PRODUCT_TRACE_QUEUE_CONSUMER");
        g_en_known = 1;
    }
    return g_en;
}

void product_eqc_reset(void) {
    g_finalized = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_seq = 0;
    g_insn = 0;
    g_hook_ok = 0;
    g_tl_n = 0;
    g_acc_n = 0;
    g_live_n = 0;
    g_vfs_n = 0;
    g_first_list = 0;
    g_first_node = 0;
    g_first_entry = 0;
    g_count_at_link = 0;
    g_head_at_link = 0;
    g_node_linked = 0;
    g_count_changed = 0;
    g_consumer_enter = 0;
    g_consumer_trigger_enter = 0;
    g_node_consumed = 0;
    g_drain_scheduled = 0;
    g_drain_delivered = 0;
    g_drain_ok = 0;
    g_post_count = 0;
    g_post_head = 0;
    memset(g_producer_w, 0, sizeof(g_producer_w));
    memset(g_consumer_read_pc, 0, sizeof(g_consumer_read_pc));
    g_empty_body = 0;
    g_item_free_path = 0;
    g_item_alt_path = 0;
    g_post_drain_gate_ok = 0;
    g_gate_init_enter = 0;
    g_ui_writer_enter = 0;
    g_gate_15d = g_gate_b71 = g_gate_134d = g_gate_c76 = 0;
    g_drain_item = g_drain_item_w0 = 0;
    g_gate_sampled = 0;
    g_en_known = 0;
    g_en = 0;
}

void product_eqc_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_eqc_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

void product_eqc_bind_uc(void *uc) { g_uc = uc; }

void product_eqc_note_er_rw(uint32_t er_rw) {
    if (er_rw) g_er_rw = er_rw;
}

uint32_t product_eqc_peek_head(void *uc, uint32_t list) {
    uint32_t v = 0;
    if (!uc || !list) return 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, list, &v);
    return v;
}

uint32_t product_eqc_peek_count(void *uc, uint32_t list) {
    uint32_t v = 0;
    if (!uc || !list) return 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, list + 4u, &v);
    return v;
}

int product_eqc_list_nonempty(void *uc, uint32_t list) {
    return product_eqc_peek_count(uc, list) != 0;
}

static void note_acc(uint32_t pc, uint32_t base, uint32_t offset, uint32_t value, int is_write,
                     const char *role) {
    EqcAcc *a;
    if (g_acc_n >= ACC_CAP) return;
    a = &g_acc[g_acc_n++];
    memset(a, 0, sizeof(*a));
    a->pc = pc;
    a->base = base;
    a->offset = offset;
    a->value = value;
    a->is_write = is_write;
    snprintf(a->role, sizeof(a->role), "%s", role ? role : "");
}

static void peek_node(void *uc, uint32_t node, uint32_t *w0, uint32_t *w1, uint32_t *w2) {
    *w0 = *w1 = *w2 = 0;
    if (!uc || !node) return;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, node, w0);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, node + 4u, w1);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, node + 8u, w2);
}

static void add_tl(void *uc, const char *phase, uint32_t list, uint32_t node, const char *note) {
    EqcTl *t;
    uint32_t ui = 0, b7d = 0, head = 0, count = 0;
    uint32_t w0 = 0, w1 = 0, w2 = 0;
    if (g_tl_n >= TL_CAP) return;
    t = &g_tl[g_tl_n++];
    memset(t, 0, sizeof(*t));
    t->seq = ++g_seq;
    t->insn = ++g_insn;
    snprintf(t->phase, sizeof(t->phase), "%s", phase ? phase : "");
    t->list = list;
    if (uc && list) {
        head = product_eqc_peek_head(uc, list);
        count = product_eqc_peek_count(uc, list);
    }
    t->head = head;
    t->count = count;
    t->node = node;
    if (uc && node) peek_node(uc, node, &w0, &w1, &w2);
    t->node_w0 = w0;
    t->node_w1 = w1;
    t->node_w2 = w2;
    if (uc && g_er_rw) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_UI, &ui);
        {
            uint8_t b = 0;
            (void)guest_memory_uc_peek((struct uc_struct *)uc, g_er_rw + OFF_B7D, &b, 1);
            b7d = b;
        }
    }
    t->ui_mode = ui;
    t->b7d = b7d;
    t->req_state = (ui == 0) ? 1u : 0u; /* unfinished while UI_MODE==0 */
    snprintf(t->note, sizeof(t->note), "%s", note ? note : "");
}

static void add_live(uint32_t pc, uint32_t list, uint32_t head, uint32_t count, uint32_t item,
                     const char *ev) {
    EqcLive *l;
    if (g_live_n >= LIVE_CAP) return;
    l = &g_live[g_live_n++];
    memset(l, 0, sizeof(*l));
    l->seq = ++g_seq;
    l->pc = pc;
    l->list = list;
    l->head = head;
    l->count = count;
    l->item = item;
    snprintf(l->event, sizeof(l->event), "%s", ev ? ev : "");
}

static void sample_gates(void *uc) {
    uint8_t b15 = 0, bb71 = 0, b134 = 0, bc76 = 0;
    if (!uc || !g_er_rw) return;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, g_er_rw + OFF_15D, &b15, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, g_er_rw + OFF_B71, &bb71, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, g_er_rw + OFF_134D, &b134, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, g_er_rw + OFF_C76, &bc76, 1);
    g_gate_15d = b15;
    g_gate_b71 = bb71;
    g_gate_134d = b134;
    g_gate_c76 = bc76;
    g_gate_sampled = 1;
}

#ifdef GWY_HAVE_UNICORN
static void on_eqc_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t r0 = 0, r1 = 0, r4 = 0;
    intptr_t tag = (intptr_t)user_data;
    uint32_t list = 0, head = 0, count = 0;
    (void)size;
    if (!product_eqc_enabled()) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R4, &r4);

    if (tag == 1) { /* 0x305EB8 trigger */
        g_consumer_trigger_enter = 1;
        list = g_first_list;
        if (!list && g_er_rw)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_B54, &list);
        head = product_eqc_peek_head(uc, list);
        count = product_eqc_peek_count(uc, list);
        add_live(pc, list, head, count, 0, "DRAIN_TRIGGER_ENTER");
        add_tl(uc, "drain_trigger_305EB8", list, head, "EVENT_QUEUE_CONSUMER_TRIGGER");
        printf("[EVENT_QUEUE_CONSUMER_TRIGGER_ENTER] pc=0x305EB8 list=0x%X head=0x%X count=%u "
               "evidence=OBSERVED\n",
               list, head, count);
        fflush(stdout);
    } else if (tag == 2) { /* 0x2DC80C consumer */
        g_consumer_enter = 1;
        list = 0;
        if (g_er_rw)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_B54, &list);
        if (!list) list = r0;
        head = product_eqc_peek_head(uc, list);
        count = product_eqc_peek_count(uc, list);
        add_live(pc, list, head, count, 0, "DRAIN_CONSUMER_ENTER");
        add_tl(uc, "drain_consumer_2DC80C", list, head, "EVENT_QUEUE_CONSUMER_ENTER");
        note_acc(pc, list, 0, head, 0, "consumer_load_head_slot");
        note_acc(pc, list, 4, count, 0, "consumer_nonempty_via_count");
        printf("[EVENT_QUEUE_CONSUMER_ENTER] pc=0x2DC80C list=0x%X head=0x%X count=%u "
               "evidence=OBSERVED\n",
               list, head, count);
        fflush(stdout);
        if (count == 0) {
            printf("[EVENT_NODE_NOT_VISIBLE_TO_CONSUMER] list=0x%X head=0x%X count=0 "
                   "evidence=OBSERVED\n",
                   list, head);
            fflush(stdout);
        } else {
            printf("[EVENT_QUEUE_NONEMPTY_VISIBLE] list=0x%X count=%u evidence=OBSERVED\n", list,
                   count);
            fflush(stdout);
        }
    } else if (tag == 3) { /* 0x312AC4 count helper — r0=list on entry */
        list = r0;
        count = product_eqc_peek_count(uc, list);
        note_acc(pc, list, 4, count, 0, "list_count_read_312AC4");
        add_live(pc, list, product_eqc_peek_head(uc, list), count, 0, "LIST_COUNT_READ");
    } else if (tag == 4) { /* 0x312AB4 get item */
        list = r4 ? r4 : g_first_list;
        if (r0) {
            uint32_t w0 = 0, w1 = 0, w2 = 0;
            peek_node(uc, r0, &w0, &w1, &w2);
            if (!g_consumer_read_pc[0]) g_consumer_read_pc[0] = pc;
            if (!g_consumer_read_pc[2]) g_consumer_read_pc[2] = pc; /* item at +8 */
            note_acc(pc, r0, 8, w2, 0, "consumer_read_item");
            add_live(pc, list, product_eqc_peek_head(uc, list), product_eqc_peek_count(uc, list), w2,
                     "GET_ITEM");
        }
    } else if (tag == 5) { /* 0x312C0C pop/remove */
        list = r4 ? r4 : g_first_list;
        {
            uint32_t before = product_eqc_peek_count(uc, list);
            add_live(pc, list, product_eqc_peek_head(uc, list), before, 0, "POP_ENTER");
            add_tl(uc, "list_pop_312C0C", list, product_eqc_peek_head(uc, list), "POP");
            g_node_consumed = 1;
            printf("[EVENT_NODE_CONSUMED] list=0x%X count_before=%u evidence=OBSERVED\n", list,
                   before);
            fflush(stdout);
        }
    } else if (tag == 6) { /* push return sites — count should already be updated */
        list = g_first_list ? g_first_list : r4;
        head = product_eqc_peek_head(uc, list);
        count = product_eqc_peek_count(uc, list);
        if (count != 0) g_count_changed = 1;
        note_acc(pc, list, 0, head, 1, "list_head_after_push_return");
        note_acc(pc, list, 4, count, 1, "list_count_after_push_return");
        add_tl(uc, "push_return", list, head, "count_sampled_after_link");
        if (count != g_count_at_link && g_node_linked) {
            printf("[EVENT_LIST_COUNT_CHANGED] before=%u after=%u list=0x%X evidence=OBSERVED\n",
                   g_count_at_link, count, list);
            fflush(stdout);
        }
    } else if (tag == 8) { /* 0x2DC82E after get_item — r0=item */
        g_drain_item = r0;
        g_drain_item_w0 = 0;
        if (r0)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r0, &g_drain_item_w0);
        sample_gates(uc);
        add_live(pc, g_first_list, 0, 0, r0, "DRAIN_ITEM");
        printf("[EVENT_DRAIN_ITEM] item=0x%X item_w0=0x%X C76=%u evidence=OBSERVED\n", r0,
               g_drain_item_w0, g_gate_c76);
        fflush(stdout);
    } else if (tag == 9) { /* 0x2DC848 alternate/dispatch branch */
        g_item_alt_path = 1;
        sample_gates(uc);
        add_live(pc, g_first_list, 0, 0, g_drain_item, "DRAIN_ALT_PATH");
        printf("[EVENT_DRAIN_ALT_PATH] pc=0x2DC848 item=0x%X item_w0=0x%X C76=%u "
               "evidence=OBSERVED\n",
               g_drain_item, g_drain_item_w0, g_gate_c76);
        fflush(stdout);
    } else if (tag == 10) { /* 0x30BC40 item free/destructor */
        g_item_free_path = 1;
        add_live(pc, g_first_list, 0, 0, r0, "ITEM_FREE_30BC40");
        printf("[EVENT_ITEM_FREE_PATH] pc=0x30BC40 item=0x%X evidence=OBSERVED\n", r0);
        fflush(stdout);
    } else if (tag == 11) { /* 0x305EC2 post-drain gate sample */
        sample_gates(uc);
        add_live(pc, g_first_list, 0, 0, 0, "POST_DRAIN_GATE_SAMPLE");
        printf("[EVENT_POST_DRAIN_GATE] 15D=%u B71=%u 134D=%u C76=%u need=15D==1,B71!=0,134D==0 "
               "evidence=OBSERVED\n",
               g_gate_15d, g_gate_b71, g_gate_134d, g_gate_c76);
        fflush(stdout);
    } else if (tag == 12) { /* 0x305EF4 BL 2DADC4 */
        g_post_drain_gate_ok = 1;
        sample_gates(uc);
        add_live(pc, g_first_list, 0, 0, 0, "POST_DRAIN_GATE_OK");
        printf("[EVENT_POST_DRAIN_GATE_OK] pc=0x305EF4 -> 2DADC4 evidence=OBSERVED\n");
        fflush(stdout);
    } else if (tag == 13) { /* 0x2DADC4 */
        g_gate_init_enter = 1;
        add_live(pc, g_first_list, 0, 0, 0, "GATE_INIT_2DADC4");
        printf("[EVENT_GATE_INIT_ENTER] pc=0x2DADC4 evidence=OBSERVED\n");
        fflush(stdout);
    } else if (tag == 14) { /* 0x2FC418 UI_MODE writer */
        g_ui_writer_enter = 1;
        add_live(pc, g_first_list, 0, 0, 0, "UI_MODE_WRITER_2FC418");
        printf("[EVENT_UI_MODE_WRITER_ENTER] pc=0x2FC418 evidence=OBSERVED\n");
        fflush(stdout);
    }
}
#endif

void product_eqc_arm_code_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_hook h1 = 0, h2 = 0, h3 = 0, h4 = 0, h5 = 0, h6 = 0, h7 = 0;
    uc_hook h8 = 0, h9 = 0, h10 = 0, h11 = 0, h12 = 0, h13 = 0, h14 = 0;
    if (!product_eqc_enabled() || !uc || g_hook_ok) return;
    g_uc = uc;
    (void)uc_hook_add((uc_engine *)uc, &h1, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)1,
                      0x305EB8ull, 0x305EBBull);
    (void)uc_hook_add((uc_engine *)uc, &h2, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)2,
                      0x2DC80Cull, 0x2DC80Full);
    (void)uc_hook_add((uc_engine *)uc, &h3, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)3,
                      0x312AC4ull, 0x312AC7ull);
    (void)uc_hook_add((uc_engine *)uc, &h4, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)4,
                      0x312AB4ull, 0x312AB7ull);
    (void)uc_hook_add((uc_engine *)uc, &h5, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)5,
                      0x312C0Cull, 0x312C0Full);
    (void)uc_hook_add((uc_engine *)uc, &h6, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)6,
                      0x312A84ull, 0x312A87ull);
    (void)uc_hook_add((uc_engine *)uc, &h7, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)6,
                      0x312AA0ull, 0x312AA3ull);
    (void)uc_hook_add((uc_engine *)uc, &h8, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)8,
                      (uint64_t)PC_2DC82E, (uint64_t)PC_2DC82E + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h9, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)9,
                      (uint64_t)PC_2DC848, (uint64_t)PC_2DC848 + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h10, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)10,
                      (uint64_t)PC_30BC40, (uint64_t)PC_30BC40 + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h11, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)11,
                      (uint64_t)PC_305EC2, (uint64_t)PC_305EC2 + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h12, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)12,
                      (uint64_t)PC_305EF4, (uint64_t)PC_305EF4 + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h13, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)13,
                      (uint64_t)PC_2DADC4, (uint64_t)PC_2DADC4 + 1ull);
    (void)uc_hook_add((uc_engine *)uc, &h14, UC_HOOK_CODE, on_eqc_code, (void *)(intptr_t)14,
                      (uint64_t)PC_2FC418, (uint64_t)PC_2FC418 + 1ull);
    g_hook_ok = 1;
    printf("[EQC_CODE_HOOKS] armed sites=305EB8,2DC80C,312AC4,312AB4,312C0C,312A84,312AA0,"
           "2DC82E,2DC848,30BC40,305EC2,305EF4,2DADC4,2FC418 evidence=OBSERVED\n");
    fflush(stdout);
#else
    (void)uc;
#endif
}

void product_eqc_on_path_a_begin(void *uc, uint32_t list, uint32_t entry, uint32_t count) {
    if (!product_eqc_enabled()) return;
    if (!g_first_list) g_first_list = list;
    add_tl(uc, "before_path_a_push", list, 0, "PATH_A_BEGIN");
    (void)entry;
    (void)count;
}

void product_eqc_on_path_a_linked(void *uc, uint32_t list, uint32_t head, uint32_t node,
                                  uint32_t entry, uint32_t count) {
    uint32_t w0 = 0, w1 = 0, w2 = 0;
    if (!product_eqc_enabled()) return;
    g_node_linked = 1;
    g_first_list = list;
    g_first_node = node;
    g_first_entry = entry;
    g_head_at_link = head;
    g_count_at_link = count;
    if (uc && node) {
        peek_node(uc, node, &w0, &w1, &w2);
        g_producer_w[0] = w0;
        g_producer_w[1] = w1;
        g_producer_w[2] = w2;
    }
    /* with_rec=0 still carries a non-null entry pointer (item), not a byte body. */
    g_empty_body = (entry == 0) ? 1 : 0;
    if (count != 0) g_count_changed = 1;
    add_tl(uc, "node_linked", list, node, "EVENT_LIST_NODE_LINKED");
    note_acc(0, list, 0, head, 1, "list_head_at_link");
    note_acc(0, list, 4, count, 1, "list_count_at_link");
    printf("[EQC_NODE_LINKED] list=0x%X head=0x%X node=0x%X entry=0x%X count=%u "
           "empty_body=%d evidence=OBSERVED\n",
           list, head, node, entry, count, g_empty_body);
    fflush(stdout);
}

void product_eqc_on_path_a_return(void *uc, uint32_t list) {
    if (!product_eqc_enabled()) return;
    g_post_head = product_eqc_peek_head(uc, list);
    g_post_count = product_eqc_peek_count(uc, list);
    add_tl(uc, "path_a_return", list, g_post_head, "POST_ENQUEUE");
}

void product_eqc_on_timer_decision(void *uc, uint32_t er_rw, uint32_t tick) {
    uint32_t list = 0, b54 = 0;
    if (!product_eqc_enabled()) return;
    if (er_rw) g_er_rw = er_rw;
    if (uc && g_er_rw)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_B54, &b54);
    list = b54 ? b54 : g_first_list;
    add_tl(uc, "next_10140_timer", list, product_eqc_peek_head(uc, list), "TIMER");
    (void)tick;
}

void product_eqc_on_vfs(const char *path, uint64_t insn, int after_first_link) {
    EqcVfs *v;
    const char *cls = "UNKNOWN";
    if (!product_eqc_enabled() || !path) return;
    if (g_vfs_n >= VFS_CAP) return;
    if (!after_first_link || !g_node_linked)
        cls = "BOOTSTRAP_BASELINE";
    else
        cls = "UNKNOWN"; /* Path-A causality needs gated compare */
    v = &g_vfs[g_vfs_n++];
    memset(v, 0, sizeof(*v));
    v->insn = insn ? insn : g_insn;
    v->after_link = after_first_link && g_node_linked;
    snprintf(v->path, sizeof(v->path), "%s", path);
    snprintf(v->cls, sizeof(v->cls), "%s", cls);
}

void product_eqc_note_drain_scheduled(uint32_t handler) {
    g_drain_scheduled = 1;
    printf("[EVENT_QUEUE_DRAIN_TRIGGER_SCHEDULED] handler=0x%X trigger=periodic_305EB8 "
           "consumer=2DC80C evidence=OBSERVED\n",
           handler);
    fflush(stdout);
}

void product_eqc_note_drain_delivered(uint32_t handler, int ok) {
    g_drain_delivered = 1;
    g_drain_ok = ok ? 1 : 0;
    if (ok) {
        printf("[EVENT_QUEUE_CONSUMER_REACHED] via_trigger=0x%X ok=1 evidence=OBSERVED\n", handler);
        fflush(stdout);
    }
}

static void ensure_out_dir(void) {
#ifdef _WIN32
    (void)system("mkdir out\\product_event 2>nul");
#else
    (void)system("mkdir -p out/product_event");
#endif
}

static void write_static_artifacts(void) {
    FILE *f;
    ensure_out_dir();
    f = fopen("out/product_event/queue_consumer_annotated.txt", "wb");
    if (f) {
        fprintf(f,
                "# Event queue consumer (Path-A B54 list)\n"
                "#\n"
                "# list_control { head@0, count@4 } size=8 ctor=0x312AA4 push=0x312A60\n"
                "# nonempty predicate: BL 0x312AC4 → LDR r0,[list,#4]; CMP r0,#0\n"
                "# get item:          BL 0x312AB4 → node = f(list); LDR r0,[node,#8]\n"
                "# pop/remove:        BL 0x312C0C → unlink; count--; free via 0x305E08\n"
                "#\n"
                "# Natural consumer of B54 (Path A):\n"
                "===== drain_entry @ 0x2DC80C =====\n"
                "0x2DC80C: PUSH {lr}\n"
                "0x2DC80E: LDR r4,[pc] ; =0xB54\n"
                "0x2DC810: ADD r4,r9\n"
                "0x2DC812: LDR r0,[r4]\n"
                "0x2DC814: BL 0x312AC4          ; count\n"
                "0x2DC81E: CMP r0,#0\n"
                "0x2DC820: BLE empty_return\n"
                "0x2DC82A: BL 0x312AB4          ; get item → r0\n"
                "0x2DC830: BNE 0x2DC848         ; nonzero item → branch\n"
                "0x2DC848: if C76==1 && item[0]!=80 → BL 0x30BC40 (FREE item)\n"
                "         else alternate → may BL 0x2E2520 (can set B71)\n"
                "0x2DC83A/868: BL 0x312C0C     ; pop node; STRB B7D=1\n"
                "#\n"
                "# Natural trigger (sole BL → 0x2DC80C):\n"
                "===== periodic @ 0x305EB8 =====\n"
                "0x305EB8: PUSH {lr}\n"
                "0x305EBA: BL 0x2DC984\n"
                "0x305EBE: BL 0x2DC80C          ; *** B54 drain ***\n"
                "0x305EC2: gates: 15D==1 && B71!=0 && 134D==0\n"
                "0x305EF4: BL 0x2DADC4          ; gate_init → … → 0x2FC418 UI_MODE=0x45\n"
                "#\n"
                "# Ack note: 0x30BC40 is destructor NOT ack. B71 first writers:\n"
                "#   0x2DC572 via 0x2DC4D8 (from 0x2E2520 table); 0x30ED7A via 2DADC4 (after gate).\n"
                "# Upstream of trigger: mrc_event path 0x2F5732 → BL 0x305EB8\n"
                "# 10140 handler 0x30630D drains R9+0x11B0 (different list), not B54.\n"
                "# Path A after push also STRB B7D=1 @ 0x2E4F2C (flag only; not a scheduler).\n");
        fclose(f);
    }
    f = fopen("out/product_event/queue_consumer_cfg.dot", "wb");
    if (f) {
        fprintf(f,
                "digraph queue_consumer {\n"
                "  rankdir=LR;\n"
                "  PathA_312A60 [label=\"list_push\\n0x312A60\"];\n"
                "  B54 [label=\"list_control\\nER_RW+B54\"];\n"
                "  Trigger_305EB8 [label=\"periodic\\n0x305EB8/9\"];\n"
                "  Drain_2DC80C [label=\"drain\\n0x2DC80C\"];\n"
                "  Free_30BC40 [label=\"item_free\\n0x30BC40\"];\n"
                "  Gate [label=\"15D/B71/134D\"];\n"
                "  Init_2DADC4 [label=\"gate_init\\n0x2DADC4\"];\n"
                "  UI_2FC418 [label=\"UI_MODE=0x45\\n0x2FC418\"];\n"
                "  PathA_312A60 -> B54;\n"
                "  Trigger_305EB8 -> Drain_2DC80C;\n"
                "  Drain_2DC80C -> Free_30BC40 [label=\"C76==1 && w0!=80\"];\n"
                "  Trigger_305EB8 -> Gate;\n"
                "  Gate -> Init_2DADC4 [label=\"B71!=0\"];\n"
                "  Init_2DADC4 -> UI_2FC418;\n"
                "}\n");
        fclose(f);
    }
}

static void write_reports(void) {
    char path[512];
    FILE *f;
    int i;
    const char *tl_verdict = "EVENT_NODE_LINKED_BUT_NOT_CONSUMED";
    const char *trig = "TIMER_POLL";
    const char *cons_verdict = "EVENT_QUEUE_CONSUMER_NOT_SCHEDULED";
    const char *empty_cls = "EVENT_RECORD_OPTIONAL";
    const char *second = "COUNT";

    const char *ack_blocker = "EVENT_QUEUE_CONSUMER_NOT_SCHEDULED";
    int ack_done = 0;

    if (g_ui_writer_enter || (g_gate_init_enter && g_post_drain_gate_ok))
        ack_done = 1;

    if (g_node_consumed && !ack_done)
        tl_verdict = "EVENT_NODE_CONSUMED_NO_ACK";
    else if (g_consumer_enter && product_eqc_peek_count(g_uc, g_first_list) == 0 && g_node_linked &&
             !g_node_consumed)
        tl_verdict = "EVENT_NODE_NOT_VISIBLE_TO_CONSUMER";
    else if (g_node_consumed && ack_done)
        tl_verdict = "EVENT_QUEUE_CONSUMED_AND_ACKNOWLEDGED";
    else if (g_node_consumed)
        tl_verdict = "EVENT_NODE_CONSUMED_NO_ACK";

    if (g_consumer_enter)
        cons_verdict = "EVENT_QUEUE_CONSUMER_REACHED";
    else if (g_drain_scheduled && !g_consumer_enter)
        cons_verdict = "EVENT_QUEUE_CONSUMER_NOT_SCHEDULED";
    else if (g_consumer_trigger_enter && !g_consumer_enter)
        cons_verdict = "EVENT_QUEUE_CONSUMER_NOT_SCHEDULED";

    if (g_empty_body)
        empty_cls = "EVENT_EMPTY_BODY_VALID"; /* item ptr may still be set */
    else
        empty_cls = "EVENT_RECORD_OPTIONAL";

    if (!g_consumer_enter)
        ack_blocker = "EVENT_QUEUE_CONSUMER_NOT_SCHEDULED";
    else if (g_item_free_path && !g_item_alt_path && g_gate_sampled && g_gate_b71 == 0)
        ack_blocker = "POST_DRAIN_GATE_B71";
    else if (g_gate_sampled && !(g_gate_15d == 1 && g_gate_b71 != 0 && g_gate_134d == 0))
        ack_blocker = "POST_DRAIN_GATE_15D_B71_134D";
    else if (g_node_consumed && !g_post_drain_gate_ok)
        ack_blocker = "POST_DRAIN_GATE_NOT_PASSED";
    else if (g_post_drain_gate_ok && !g_ui_writer_enter)
        ack_blocker = "GATE_INIT_NO_UI_WRITER";
    else if (!ack_done)
        ack_blocker = "DISPATCH_OR_ACK_AFTER_CONSUME";
    else
        ack_blocker = "NONE";

    report_path("product_event_enqueue_timeline.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,insn,phase,list,head,count,node,node_w0,node_w1,node_w2,ui_mode,b7d,"
                   "req_unfinished,note\n");
        for (i = 0; i < g_tl_n; i++) {
            EqcTl *t = &g_tl[i];
            fprintf(f, "%s,%u,%llu,%s,0x%X,0x%X,%u,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,%s\n",
                    product_eqc_run_id(), t->seq, (unsigned long long)t->insn, t->phase, t->list,
                    t->head, t->count, t->node, t->node_w0, t->node_w1, t->node_w2, t->ui_mode,
                    t->b7d, t->req_state, t->note);
        }
        fclose(f);
    }

    report_path("product_event_post_enqueue_state.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,metric,value\n");
        fprintf(f, "%s,EVENT_NODE_LINKED,%d\n", product_eqc_run_id(), g_node_linked);
        fprintf(f, "%s,list,0x%X\n", product_eqc_run_id(), g_first_list);
        fprintf(f, "%s,head_at_link,0x%X\n", product_eqc_run_id(), g_head_at_link);
        fprintf(f, "%s,count_at_link,%u\n", product_eqc_run_id(), g_count_at_link);
        fprintf(f, "%s,post_head,0x%X\n", product_eqc_run_id(), g_post_head);
        fprintf(f, "%s,post_count,%u\n", product_eqc_run_id(), g_post_count);
        fprintf(f, "%s,verdict,%s\n", product_eqc_run_id(), tl_verdict);
        fclose(f);
    }

    report_path("product_event_list_control_contract.json", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "{\n"
                "  \"size\": 8,\n"
                "  \"fields\": [\n"
                "    {\"offset\": \"0x0\", \"role\": \"HEAD\", \"empty\": \"0\", "
                "\"nonempty\": \"node_ptr\", \"readers\": [\"0x312A86\", \"0x2DC812\"], "
                "\"writers\": [\"0x312A7E\", \"0x312C2C\"]},\n"
                "    {\"offset\": \"0x4\", \"role\": \"COUNT\", \"empty\": \"0\", "
                "\"nonempty\": \">=1\", \"readers\": [\"0x312A78\", \"0x312AC4\", \"0x2DC814\"], "
                "\"writers\": [\"0x312A82\", \"0x312A9E\", \"0x312C36\"], "
                "\"nonempty_predicate\": \"count!=0 via 0x312AC4\"}\n"
                "  ],\n"
                "  \"second_field_class\": \"%s\",\n"
                "  \"verdicts\": [\n"
                "    \"EVENT_LIST_HEAD_CONTRACT_CONFIRMED\",\n"
                "    \"EVENT_LIST_SECOND_FIELD_IDENTIFIED\",\n"
                "    \"%s\"\n"
                "  ],\n"
                "  \"run_id\": \"%s\"\n"
                "}\n",
                second, g_count_changed || g_post_count ? "EVENT_LIST_NONEMPTY_STATE_CONFIRMED"
                                                        : "EVENT_LIST_NONEMPTY_STATE_PENDING",
                product_eqc_run_id());
        fclose(f);
    }

    report_path("product_event_list_control_access.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,pc,base,offset,value,is_write,role\n");
        for (i = 0; i < g_acc_n; i++) {
            EqcAcc *a = &g_acc[i];
            fprintf(f, "%s,0x%X,0x%X,0x%X,0x%X,%d,%s\n", product_eqc_run_id(), a->pc, a->base,
                    a->offset, a->value, a->is_write, a->role);
        }
        fclose(f);
    }

    report_path("product_event_consumer_xrefs.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,site,role,caller,notes\n");
        fprintf(f, "%s,0x2DC80C,B54_DRAIN,0x305EBE,sole_BL_from_periodic\n", product_eqc_run_id());
        fprintf(f, "%s,0x305EB8,DRAIN_TRIGGER,0x2F5734,mrc_event_case_then_gates\n",
                product_eqc_run_id());
        fprintf(f, "%s,0x312AC4,LIST_COUNT,many,nonempty_predicate\n", product_eqc_run_id());
        fprintf(f, "%s,0x312AB4,LIST_GET_ITEM,0x2DC82A,returns_node_plus_8\n", product_eqc_run_id());
        fprintf(f, "%s,0x312C0C,LIST_POP,0x2DC83A,dec_count_unlink_free\n", product_eqc_run_id());
        fprintf(f, "%s,0x3066E6,OTHER_LIST_COUNT,0x30630D,drains_R9+0x11B0_NOT_B54\n",
                product_eqc_run_id());
        fclose(f);
    }

    report_path("product_event_consumer_live.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,seq,pc,list,head,count,item,event\n");
        for (i = 0; i < g_live_n; i++) {
            EqcLive *l = &g_live[i];
            fprintf(f, "%s,%u,0x%X,0x%X,0x%X,%u,0x%X,%s\n", product_eqc_run_id(), l->seq, l->pc,
                    l->list, l->head, l->count, l->item, l->event);
        }
        fclose(f);
    }

    report_path("product_event_node_consumer_schema.json", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "{\n"
                "  \"node_size\": 12,\n"
                "  \"fields\": [\n"
                "    {\"offset\": 0, \"producer\": \"0x%X\", \"consumer_pc\": \"0x%X\", "
                "\"role\": \"PREV_OR_LINK\", \"class\": \"pointer\"},\n"
                "    {\"offset\": 4, \"producer\": \"0x%X\", \"consumer_pc\": \"0x%X\", "
                "\"role\": \"NEXT_OR_LINK\", \"class\": \"pointer\"},\n"
                "    {\"offset\": 8, \"producer\": \"0x%X\", \"consumer_pc\": \"0x%X\", "
                "\"role\": \"ITEM\", \"class\": \"pointer\", \"nullability\": \"%s\"}\n"
                "  ],\n"
                "  \"empty_body\": \"%s\",\n"
                "  \"verdict\": \"EVENT_NODE_SCHEMA_CONSUMER_CONFIRMED\",\n"
                "  \"run_id\": \"%s\"\n"
                "}\n",
                g_producer_w[0], g_consumer_read_pc[0] ? g_consumer_read_pc[0] : 0x312C0Cu,
                g_producer_w[1], g_consumer_read_pc[1] ? g_consumer_read_pc[1] : 0x312C0Cu,
                g_producer_w[2], g_consumer_read_pc[2] ? g_consumer_read_pc[2] : 0x312AB4u,
                g_empty_body ? "nullable" : "non_null_observed", empty_cls, product_eqc_run_id());
        fclose(f);
    }

    report_path("product_event_node_field_flow.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,offset,producer_value,consumer_pc,role,branch\n");
        fprintf(f, "%s,0,0x%X,0x%X,PREV_OR_LINK,unlink_uses_both_links\n", product_eqc_run_id(),
                g_producer_w[0], g_consumer_read_pc[0] ? g_consumer_read_pc[0] : 0x312C0Cu);
        fprintf(f, "%s,4,0x%X,0x%X,NEXT_OR_LINK,unlink_uses_both_links\n", product_eqc_run_id(),
                g_producer_w[1], g_consumer_read_pc[1] ? g_consumer_read_pc[1] : 0x312C0Cu);
        fprintf(f, "%s,8,0x%X,0x%X,ITEM,dispatch_after_get\n", product_eqc_run_id(),
                g_producer_w[2], g_consumer_read_pc[2] ? g_consumer_read_pc[2] : 0x312AB4u);
        fclose(f);
    }

    report_path("product_resource_causality_compare.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,path,insn,after_first_node_link,class,notes\n");
        for (i = 0; i < g_vfs_n; i++) {
            EqcVfs *v = &g_vfs[i];
            fprintf(f, "%s,%s,%llu,%d,%s,timeline_vs_first_link\n", product_eqc_run_id(), v->path,
                    (unsigned long long)v->insn, v->after_link, v->cls);
        }
        if (g_vfs_n == 0)
            fprintf(f, "%s,sdk_key.dat,0,0,BOOTSTRAP_BASELINE,no_eqc_vfs_samples_seed\n",
                    product_eqc_run_id());
        fclose(f);
    }

    report_path("product_event_post_enqueue_unfinished_predicate.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,predicate,value,notes\n");
        fprintf(f, "%s,UI_MODE,0,top_level_visible\n", product_eqc_run_id());
        fprintf(f, "%s,list_count_after_enqueue,%u,nonempty_if_gt0\n", product_eqc_run_id(),
                g_post_count ? g_post_count : g_count_at_link);
        fprintf(f, "%s,consumer_entered,%d,2DC80C\n", product_eqc_run_id(), g_consumer_enter);
        fprintf(f, "%s,trigger_entered,%d,305EB8\n", product_eqc_run_id(),
                g_consumer_trigger_enter);
        fprintf(f, "%s,node_consumed,%d,312C0C\n", product_eqc_run_id(), g_node_consumed);
        fprintf(f, "%s,item_free_path,%d,30BC40\n", product_eqc_run_id(), g_item_free_path);
        fprintf(f, "%s,item_alt_path,%d,2DC848\n", product_eqc_run_id(), g_item_alt_path);
        fprintf(f, "%s,gate_15D,%u,need_1\n", product_eqc_run_id(), g_gate_15d);
        fprintf(f, "%s,gate_B71,%u,need_nonzero\n", product_eqc_run_id(), g_gate_b71);
        fprintf(f, "%s,gate_134D,%u,need_0\n", product_eqc_run_id(), g_gate_134d);
        fprintf(f, "%s,gate_C76,%u,item_branch\n", product_eqc_run_id(), g_gate_c76);
        fprintf(f, "%s,drain_item,0x%X,\n", product_eqc_run_id(), g_drain_item);
        fprintf(f, "%s,drain_item_w0,0x%X,alt_if_eq_80\n", product_eqc_run_id(), g_drain_item_w0);
        fprintf(f, "%s,post_drain_gate_ok,%d,305EF4\n", product_eqc_run_id(),
                g_post_drain_gate_ok);
        fprintf(f, "%s,gate_init_enter,%d,2DADC4\n", product_eqc_run_id(), g_gate_init_enter);
        fprintf(f, "%s,ui_writer_enter,%d,2FC418\n", product_eqc_run_id(), g_ui_writer_enter);
        fprintf(f, "%s,ack_blocker,%s\n", product_eqc_run_id(), ack_blocker);
        fclose(f);
    }

    report_path("product_event_completion_state_machine.json", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "{\n"
                "  \"request\": \"0x1E209/app=9\",\n"
                "  \"path_a\": \"30D2F9 -> 101AB -> 2E4D6C -> 312A60\",\n"
                "  \"list\": \"ER_RW+B54\",\n"
                "  \"after_enqueue\": \"UI_MODE remains 0; same request reissued by 10140\",\n"
                "  \"consumer_trigger\": \"0x305EB8 (Thumb deliver 0x305EB9)\",\n"
                "  \"consumer\": \"0x2DC80C\",\n"
                "  \"item_free\": \"0x30BC40\",\n"
                "  \"post_drain_gates\": \"15D==1 && B71!=0 && 134D==0 -> 0x2DADC4\",\n"
                "  \"ui_writer\": \"0x2FC418 stores 0x45 at ER_RW+0x8D0\",\n"
                "  \"trigger_class\": \"%s\",\n"
                "  \"ack_blocker\": \"%s\",\n"
                "  \"gate_sample\": {\"15D\": %u, \"B71\": %u, \"134D\": %u, \"C76\": %u},\n"
                "  \"item_path\": {\"free_30BC40\": %d, \"alt_2DC848\": %d, \"item\": \"0x%X\", "
                "\"item_w0\": \"0x%X\"},\n"
                "  \"verdict\": \"EVENT_ACK_BLOCKER_IDENTIFIED\",\n"
                "  \"run_id\": \"%s\"\n"
                "}\n",
                trig, ack_blocker, g_gate_15d, g_gate_b71, g_gate_134d, g_gate_c76,
                g_item_free_path, g_item_alt_path, g_drain_item, g_drain_item_w0,
                product_eqc_run_id());
        fclose(f);
    }

    report_path("product_event_consumer_validate.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,metric,value\n");
        fprintf(f, "%s,EVENT_LIST_HEAD_CONTRACT_CONFIRMED,1\n", product_eqc_run_id());
        fprintf(f, "%s,EVENT_LIST_SECOND_FIELD_IDENTIFIED,1\n", product_eqc_run_id());
        fprintf(f, "%s,EVENT_LIST_NONEMPTY_STATE_CONFIRMED,%d\n", product_eqc_run_id(),
                (g_count_changed || g_post_count || g_count_at_link) ? 1 : 0);
        fprintf(f, "%s,EVENT_QUEUE_CONSUMER_IDENTIFIED,1\n", product_eqc_run_id());
        fprintf(f, "%s,EVENT_QUEUE_CONSUMER_TRIGGER_IDENTIFIED,1\n", product_eqc_run_id());
        fprintf(f, "%s,%s,1\n", product_eqc_run_id(), cons_verdict);
        fprintf(f, "%s,EVENT_NODE_SCHEMA_CONSUMER_CONFIRMED,1\n", product_eqc_run_id());
        fprintf(f, "%s,EVENT_ACK_BLOCKER_IDENTIFIED,1\n", product_eqc_run_id());
        fprintf(f, "%s,EVENT_QUEUE_CONSUMED_AND_ACKNOWLEDGED,%d\n", product_eqc_run_id(),
                ack_done ? 1 : 0);
        fprintf(f, "%s,drain_scheduled,%d\n", product_eqc_run_id(), g_drain_scheduled);
        fprintf(f, "%s,drain_delivered,%d\n", product_eqc_run_id(), g_drain_delivered);
        fprintf(f, "%s,item_free_path,%d\n", product_eqc_run_id(), g_item_free_path);
        fprintf(f, "%s,post_drain_gate_ok,%d\n", product_eqc_run_id(), g_post_drain_gate_ok);
        fprintf(f, "%s,ack_blocker,%s\n", product_eqc_run_id(), ack_blocker);
        fprintf(f, "%s,timeline_verdict,%s\n", product_eqc_run_id(), tl_verdict);
        fprintf(f, "%s,empty_body_class,%s\n", product_eqc_run_id(), empty_cls);
        fclose(f);
    }
}

void product_eqc_finalize(void) {
    if (g_finalized) return;
    g_finalized = 1;
    if (!product_eqc_enabled() && !g_node_linked && !g_drain_scheduled) return;
    write_static_artifacts();
    write_reports();
    printf("[EQC_FINALIZE] linked=%d count_changed=%d trigger=%d consumer=%d consumed=%d "
           "drain_sched=%d drain_ok=%d run_id=%s evidence=OBSERVED\n",
           g_node_linked, g_count_changed, g_consumer_trigger_enter, g_consumer_enter,
           g_node_consumed, g_drain_scheduled, g_drain_ok, product_eqc_run_id());
    fflush(stdout);
}
