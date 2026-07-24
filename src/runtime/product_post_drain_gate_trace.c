#include "gwy_launcher/product_post_drain_gate_trace.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* Candidate natural writers + drain/gate anchors — observation only. */
#define PC_30CBBC 0x30CBBCu
#define PC_30CCF4 0x30CCF4u /* strb 15D=1 inside 30CBBC (legacy) */
#define PC_2E2520 0x2E2520u
#define PC_2DC4D8 0x2DC4D8u
#define PC_2DC572 0x2DC572u /* strb B71=1 in 2DC4D8 (legacy) */
#define PC_2DC80C 0x2DC80Cu
#define PC_312C0C 0x312C0Cu
#define PC_305EC2 0x305EC2u
#define OFF_15D 0x15Du
#define OFF_B71 0xB71u
#define OFF_134D 0x134Du
#define OFF_C76 0xC76u
#define OFF_B54 0xB54u

#define WATCH_CAP 256
#define ANCHOR_CAP 32

typedef struct {
    uint32_t seq;
    uint64_t insn;
    char kind[28];
    char tag[40];
    uint32_t pc, lr, sp;
    uint32_t r0, r1, r2, r3, r9;
    uint32_t addr;
    uint32_t before, after;
    uint32_t size;
    uint32_t c76, off134d;
    uint32_t item;
    uint32_t list;
    uint32_t cb_depth;
    int after_2dc80c;
    int after_312c0c;
    int after_305ec2;
} PdgtWatch;

typedef struct {
    char name[32];
    uint32_t pc;
    uint64_t insn;
    uint32_t seq;
} PdgtAnchor;

static int g_en, g_en_known, g_finalized, g_hook_ok;
static char g_run_id[80];
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_seq;
static uint64_t g_insn;
static uint32_t g_cb_depth;

static PdgtWatch g_w[WATCH_CAP];
static int g_w_n;
static PdgtAnchor g_a[ANCHOR_CAP];
static int g_a_n;

static int g_seen_2dc80c, g_seen_312c0c, g_seen_305ec2;
static int g_enter_30cbbc, g_enter_2e2520, g_enter_2dc4d8;
static int g_store_15d, g_store_b71;
static uint32_t g_last_item, g_last_list;
static int g_atexit_ok;

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

static void write_reports(void); /* forward */

static void pdgt_atexit_flush(void) {
    if (product_pdgt_enabled())
        product_pdgt_finalize();
}

int product_pdgt_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_POST_DRAIN_GATE_TRACE");
        g_en_known = 1;
    }
    return g_en;
}

void product_pdgt_reset(void) {
    g_finalized = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_seq = 0;
    g_insn = 0;
    g_cb_depth = 0;
    g_hook_ok = 0;
    g_w_n = 0;
    g_a_n = 0;
    g_seen_2dc80c = g_seen_312c0c = g_seen_305ec2 = 0;
    g_enter_30cbbc = g_enter_2e2520 = g_enter_2dc4d8 = 0;
    g_store_15d = g_store_b71 = 0;
    g_last_item = g_last_list = 0;
    g_en_known = 0;
    g_en = 0;
    g_atexit_ok = 0;
    memset(g_w, 0, sizeof(g_w));
    memset(g_a, 0, sizeof(g_a));
}

void product_pdgt_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_pdgt_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

void product_pdgt_bind_uc(void *uc) { g_uc = uc; }

void product_pdgt_note_er_rw(uint32_t er_rw) {
    if (er_rw) g_er_rw = er_rw;
}

void product_pdgt_note_anchor(const char *name, uint32_t pc, uint64_t insn) {
    PdgtAnchor *a;
    if (!product_pdgt_enabled() || !name) return;
    if (g_a_n >= ANCHOR_CAP) return;
    a = &g_a[g_a_n++];
    memset(a, 0, sizeof(*a));
    snprintf(a->name, sizeof(a->name), "%s", name);
    a->pc = pc;
    a->insn = insn;
    a->seq = ++g_seq;
    if (pc == PC_2DC80C) g_seen_2dc80c = 1;
    if (pc == PC_312C0C) g_seen_312c0c = 1;
    if (pc == PC_305EC2) g_seen_305ec2 = 1;
}

#ifdef GWY_HAVE_UNICORN
static void read_regs(uc_engine *uc, uint32_t *pc, uint32_t *lr, uint32_t *sp, uint32_t *r0,
                      uint32_t *r1, uint32_t *r2, uint32_t *r3, uint32_t *r9) {
    uc_reg_read(uc, UC_ARM_REG_PC, pc);
    uc_reg_read(uc, UC_ARM_REG_LR, lr);
    uc_reg_read(uc, UC_ARM_REG_SP, sp);
    uc_reg_read(uc, UC_ARM_REG_R0, r0);
    uc_reg_read(uc, UC_ARM_REG_R1, r1);
    uc_reg_read(uc, UC_ARM_REG_R2, r2);
    uc_reg_read(uc, UC_ARM_REG_R3, r3);
    uc_reg_read(uc, UC_ARM_REG_R9, r9);
}

static void peek_aux(uc_engine *uc, uint32_t *c76, uint32_t *o134, uint32_t *list) {
    uint8_t b = 0;
    *c76 = *o134 = *list = 0;
    if (!g_er_rw) return;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, g_er_rw + OFF_C76, &b, 1);
    *c76 = b;
    b = 0;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, g_er_rw + OFF_134D, &b, 1);
    *o134 = b;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, g_er_rw + OFF_B54, list);
}

static void add_watch(uc_engine *uc, const char *kind, const char *tag, uint32_t addr,
                      uint32_t before, uint32_t after, uint32_t size) {
    PdgtWatch *w;
    uint32_t pc = 0, lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    uint32_t c76 = 0, o134 = 0, list = 0;
    if (g_w_n >= WATCH_CAP) return;
    read_regs(uc, &pc, &lr, &sp, &r0, &r1, &r2, &r3, &r9);
    peek_aux(uc, &c76, &o134, &list);
    w = &g_w[g_w_n++];
    memset(w, 0, sizeof(*w));
    w->seq = ++g_seq;
    w->insn = g_insn;
    snprintf(w->kind, sizeof(w->kind), "%s", kind ? kind : "");
    snprintf(w->tag, sizeof(w->tag), "%s", tag ? tag : "");
    w->pc = pc;
    w->lr = lr;
    w->sp = sp;
    w->r0 = r0;
    w->r1 = r1;
    w->r2 = r2;
    w->r3 = r3;
    w->r9 = r9;
    w->addr = addr;
    w->before = before;
    w->after = after;
    w->size = size;
    w->c76 = c76;
    w->off134d = o134;
    w->item = g_last_item;
    w->list = list ? list : g_last_list;
    w->cb_depth = g_cb_depth;
    w->after_2dc80c = g_seen_2dc80c;
    w->after_312c0c = g_seen_312c0c;
    w->after_305ec2 = g_seen_305ec2;
}

static void on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    intptr_t tag = (intptr_t)user_data;
    uint32_t pc = (uint32_t)address;
    (void)size;
    if (!product_pdgt_enabled()) return;
    g_insn++;
    if (tag == 1) { /* 30CBBC */
        g_enter_30cbbc = 1;
        add_watch(uc, "CODE_ENTER", "cand_15D_writer_30CBBC", pc, 0, 0, 0);
        printf("[PDGT_ENTER] site=0x30CBBC role=cand_15D_writer evidence=OBSERVED\n");
        fflush(stdout);
        write_reports();
    } else if (tag == 2) { /* 30CCF4 */
        add_watch(uc, "CODE_SITE", "strb_15D_site_30CCF4", pc, 0, 0, 0);
        printf("[PDGT_SITE] pc=0x30CCF4 tag=strb_15D=1_in_30CBBC evidence=OBSERVED\n");
        fflush(stdout);
    } else if (tag == 3) { /* 2E2520 */
        int first = !g_enter_2e2520;
        g_enter_2e2520 = 1;
        add_watch(uc, "CODE_ENTER", "b71_upstream_2E2520", pc, 0, 0, 0);
        printf("[PDGT_ENTER] site=0x2E2520 role=b71_upstream_dispatcher evidence=OBSERVED\n");
        fflush(stdout);
        if (first || (g_w_n % 16) == 0)
            write_reports();
    } else if (tag == 4) { /* 2DC4D8 */
        g_enter_2dc4d8 = 1;
        add_watch(uc, "CODE_ENTER", "b71_writer_chain_2DC4D8", pc, 0, 0, 0);
        printf("[PDGT_ENTER] site=0x2DC4D8 role=b71_writer_chain evidence=OBSERVED\n");
        fflush(stdout);
        write_reports();
    } else if (tag == 5) { /* 2DC572 */
        add_watch(uc, "CODE_SITE", "strb_B71_site_2DC572", pc, 0, 0, 0);
        printf("[PDGT_SITE] pc=0x2DC572 tag=strb_B71=1_in_2DC4D8 evidence=OBSERVED\n");
        fflush(stdout);
        write_reports();
    } else if (tag == 10) { /* 2DC80C */
        g_seen_2dc80c = 1;
        product_pdgt_note_anchor("drain_2DC80C", PC_2DC80C, g_insn);
        add_watch(uc, "ANCHOR", "drain_2DC80C", pc, 0, 0, 0);
    } else if (tag == 11) { /* 312C0C */
        uint32_t r0 = 0;
        g_seen_312c0c = 1;
        uc_reg_read(uc, UC_ARM_REG_R0, &r0);
        g_last_item = r0;
        product_pdgt_note_anchor("pop_312C0C", PC_312C0C, g_insn);
        add_watch(uc, "ANCHOR", "pop_312C0C", pc, 0, 0, 0);
    } else if (tag == 12) { /* 305EC2 */
        g_seen_305ec2 = 1;
        product_pdgt_note_anchor("gate_305EC2", PC_305EC2, g_insn);
        add_watch(uc, "ANCHOR", "gate_305EC2", pc, 0, 0, 0);
        write_reports();
    }
}

static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                         void *user_data) {
    uint32_t addr = (uint32_t)address;
    uint32_t before = 0;
    uint8_t b = 0;
    (void)type;
    (void)user_data;
    if (!product_pdgt_enabled() || !g_er_rw) return;
    g_insn++;
    if (addr == g_er_rw + OFF_15D) {
        (void)guest_memory_uc_peek((struct uc_struct *)uc, addr, &b, 1);
        before = b;
        g_store_15d = 1;
        add_watch(uc, "ER_RW_WRITE", "ER_RW+15D", addr, before, (uint32_t)(value & 0xFFu),
                  (uint32_t)size);
        printf("[PDGT_ER_RW_WRITE] off=15D before=%u after=%u size=%d pc_site=watch "
               "evidence=OBSERVED\n",
               before, (unsigned)(value & 0xFFu), size);
        fflush(stdout);
        write_reports();
    } else if (addr == g_er_rw + OFF_B71) {
        (void)guest_memory_uc_peek((struct uc_struct *)uc, addr, &b, 1);
        before = b;
        g_store_b71 = 1;
        add_watch(uc, "ER_RW_WRITE", "ER_RW+B71", addr, before, (uint32_t)(value & 0xFFu),
                  (uint32_t)size);
        printf("[PDGT_ER_RW_WRITE] off=B71 before=%u after=%u size=%d evidence=OBSERVED\n", before,
               (unsigned)(value & 0xFFu), size);
        fflush(stdout);
        write_reports();
    }
}
#endif

void product_pdgt_arm_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uc_hook h = 0;
    uc_err e;
    if (!product_pdgt_enabled() || !uc || g_hook_ok) return;
    g_uc = uc;
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)1,
                      (uint64_t)PC_30CBBC, (uint64_t)PC_30CBBC + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)2,
                      (uint64_t)PC_30CCF4, (uint64_t)PC_30CCF4 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)3,
                      (uint64_t)PC_2E2520, (uint64_t)PC_2E2520 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)4,
                      (uint64_t)PC_2DC4D8, (uint64_t)PC_2DC4D8 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)5,
                      (uint64_t)PC_2DC572, (uint64_t)PC_2DC572 + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)10,
                      (uint64_t)PC_2DC80C, (uint64_t)PC_2DC80C + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)11,
                      (uint64_t)PC_312C0C, (uint64_t)PC_312C0C + 3ull);
    (void)uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_code, (void *)(intptr_t)12,
                      (uint64_t)PC_305EC2, (uint64_t)PC_305EC2 + 3ull);
    if (g_er_rw) {
        e = uc_hook_add((uc_engine *)uc, &h, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL,
                        (uint64_t)(g_er_rw + OFF_15D), (uint64_t)(g_er_rw + OFF_15D));
        (void)e;
        e = uc_hook_add((uc_engine *)uc, &h, UC_HOOK_MEM_WRITE, (void *)on_mem_write, NULL,
                        (uint64_t)(g_er_rw + OFF_B71), (uint64_t)(g_er_rw + OFF_B71));
        (void)e;
    }
    g_hook_ok = 1;
    if (!g_atexit_ok) {
        atexit(pdgt_atexit_flush);
        g_atexit_ok = 1;
    }
    printf("[PDGT_HOOKS] armed 30CBBC/30CCF4/2E2520/2DC4D8/2DC572 + anchors + ER_RW 15D/B71 "
           "er_rw=0x%X evidence=OBSERVED\n",
           g_er_rw);
    fflush(stdout);
#else
    (void)uc;
#endif
}

static void write_reports(void) {
    char path[512];
    FILE *f;
    int i;
    const char *grade_15d = "candidate_unproven";
    const char *grade_b71 = "candidate_unproven";
    const char *blocker = "POST_DRAIN_SUCCESSOR_BLOCKED";

    if (g_store_15d && g_enter_30cbbc)
        grade_15d = "proven_natural_writer";
    else if (g_enter_30cbbc)
        grade_15d = "candidate_entered_no_store";
    else if (g_store_15d)
        grade_15d = "store_seen_writer_pc_elsewhere";

    if (g_store_b71 && (g_enter_2dc4d8 || g_enter_2e2520))
        grade_b71 = "proven_natural_writer";
    else if (g_enter_2e2520 || g_enter_2dc4d8)
        grade_b71 = "candidate_entered_no_store";
    else if (g_store_b71)
        grade_b71 = "store_seen_writer_pc_elsewhere";

    if (g_store_15d && g_store_b71)
        blocker = "POST_DRAIN_SUCCESSOR_REACHED_OR_PENDING_GATE_RECHECK";

    report_path("product_post_drain_gate_watch.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "run_id,seq,insn,kind,tag,pc,lr,sp,r0,r1,r2,r3,r9,addr,before,after,size,"
                "c76,134d,item,list,cb_depth,after_2dc80c,after_312c0c,after_305ec2\n");
        for (i = 0; i < g_w_n; i++) {
            PdgtWatch *w = &g_w[i];
            fprintf(f,
                    "%s,%u,%llu,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%u,%u,%u,"
                    "%u,%u,0x%X,0x%X,%u,%d,%d,%d\n",
                    product_pdgt_run_id(), w->seq, (unsigned long long)w->insn, w->kind, w->tag,
                    w->pc, w->lr, w->sp, w->r0, w->r1, w->r2, w->r3, w->r9, w->addr, w->before,
                    w->after, w->size, w->c76, w->off134d, w->item, w->list, w->cb_depth,
                    w->after_2dc80c, w->after_312c0c, w->after_305ec2);
        }
        fclose(f);
    }

    report_path("product_post_drain_gate_timeline.md", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "# Post-Drain Gate Timeline\n\n");
        fprintf(f, "- **run_id:** %s\n", product_pdgt_run_id());
        fprintf(f, "- **er_rw:** 0x%X\n", g_er_rw);
        fprintf(f, "- **enter_30CBBC:** %d\n", g_enter_30cbbc);
        fprintf(f, "- **enter_2E2520:** %d\n", g_enter_2e2520);
        fprintf(f, "- **enter_2DC4D8:** %d\n", g_enter_2dc4d8);
        fprintf(f, "- **store_15D:** %d\n", g_store_15d);
        fprintf(f, "- **store_B71:** %d\n", g_store_b71);
        fprintf(f, "- **15D_writer_grade:** %s\n", grade_15d);
        fprintf(f, "- **B71_writer_grade:** %s\n", grade_b71);
        fprintf(f, "- **successor_status:** %s\n\n", blocker);
        fprintf(f, "## Anchors\n\n");
        fprintf(f, "| seq | name | pc | insn |\n|-----|------|----|------|\n");
        for (i = 0; i < g_a_n; i++) {
            fprintf(f, "| %u | %s | 0x%X | %llu |\n", g_a[i].seq, g_a[i].name, g_a[i].pc,
                    (unsigned long long)g_a[i].insn);
        }
        fprintf(f, "\n## Watch events (first %d)\n\n", g_w_n);
        fprintf(f, "| seq | kind | tag | pc | before | after |\n");
        fprintf(f, "|-----|------|-----|----|--------|-------|\n");
        for (i = 0; i < g_w_n; i++) {
            fprintf(f, "| %u | %s | %s | 0x%X | %u | %u |\n", g_w[i].seq, g_w[i].kind, g_w[i].tag,
                    g_w[i].pc, g_w[i].before, g_w[i].after);
        }
        fprintf(f, "\n## Discipline\n\n");
        fprintf(f, "- Observe-only: no writes to 15D/B71/UI_MODE.\n");
        fprintf(f, "- Proven natural writer requires CODE_ENTER + ER_RW store to needed value.\n");
        fclose(f);
    }

    report_path("product_post_drain_gate_summary.csv", path, sizeof(path));
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,metric,value\n");
        fprintf(f, "%s,enter_30CBBC,%d\n", product_pdgt_run_id(), g_enter_30cbbc);
        fprintf(f, "%s,enter_2E2520,%d\n", product_pdgt_run_id(), g_enter_2e2520);
        fprintf(f, "%s,enter_2DC4D8,%d\n", product_pdgt_run_id(), g_enter_2dc4d8);
        fprintf(f, "%s,store_15D,%d\n", product_pdgt_run_id(), g_store_15d);
        fprintf(f, "%s,store_B71,%d\n", product_pdgt_run_id(), g_store_b71);
        fprintf(f, "%s,15D_writer_grade,%s\n", product_pdgt_run_id(), grade_15d);
        fprintf(f, "%s,B71_writer_grade,%s\n", product_pdgt_run_id(), grade_b71);
        fprintf(f, "%s,watch_events,%d\n", product_pdgt_run_id(), g_w_n);
        fprintf(f, "%s,successor_status,%s\n", product_pdgt_run_id(), blocker);
        fclose(f);
    }
}

void product_pdgt_finalize(void) {
    if (g_finalized) return;
    g_finalized = 1;
    if (!product_pdgt_enabled()) return;
    write_reports();
    printf("[PDGT_FINALIZE] enter_30CBBC=%d enter_2E2520=%d enter_2DC4D8=%d store_15D=%d "
           "store_B71=%d watches=%d run_id=%s evidence=OBSERVED\n",
           g_enter_30cbbc, g_enter_2e2520, g_enter_2dc4d8, g_store_15d, g_store_b71, g_w_n,
           product_pdgt_run_id());
    fflush(stdout);
}
