#include "gwy_launcher/product_event_object_trace.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/product_runtime_progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define EOT_OBJ_CAP 16
#define EOT_STORE_CAP 256
#define EOT_STAGE_CAP 128
#define EOT_CALL_CAP 8

typedef struct EotStore {
    uint32_t object_id;
    uint32_t object_base;
    uint32_t field_offset;
    uint32_t store_pc;
    uint32_t store_size;
    uint32_t old_value;
    uint32_t new_value;
    uint32_t src_reg; /* 0=R0 .. 3=R3, 0xFF=unknown/mem */
    uint32_t src_value;
    uint32_t lr;
    uint32_t sp;
    uint32_t r0, r1, r2, r3, r9;
    char stage[48];
} EotStore;

typedef struct EotStage {
    char stage[48];
    uint32_t pc, lr, sp;
    uint32_t r0, r1, r2, r3, r9;
    uint32_t payload_ptr;
    uint32_t object_ptr;
    uint32_t wrapper_ptr;
    uint32_t node_ptr;
    uint32_t content_ptr;
    uint32_t word0, word4, word8, wordc;
} EotStage;

typedef struct EotObj {
    int used;
    uint32_t object_id;
    uint32_t base;
    uint32_t node;
    uint32_t list;
    uint32_t born_pc;
    uint32_t first_word0;
    uint32_t first_word8;
    int saw_write_plus0;
    int saw_write_plus8;
    uint32_t writer_pc_plus8;
    uint32_t value_plus8;
    char class_guess[48];
} EotObj;

typedef struct EotCall {
    int used;
    uint32_t call_id;
    uint32_t r0;
    uint32_t matched_object_id;
    uint32_t word0, word4, word8, wordc;
    uint32_t lr, sp, r1, r2, r3, r9;
    int is_wrapper_guess;
    char classification[64];
} EotCall;

static char g_run_id[64];
static int g_en_known;
static int g_en;
static int g_finalized;
static void *g_uc;
static uint32_t g_next_oid = 1;
static EotObj g_obj[EOT_OBJ_CAP];
static int g_obj_n;
static EotStore g_store[EOT_STORE_CAP];
static int g_store_n;
static EotStage g_stage[EOT_STAGE_CAP];
static int g_stage_n;
static EotCall g_call[EOT_CALL_CAP];
static int g_call_n;
static char g_cur_stage[48] = "init";
#ifdef GWY_HAVE_UNICORN
static uc_hook g_code_hooks[12];
static int g_hook_ok;
#endif

static int env1(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

static void report_path(char *out, size_t n, const char *name) {
    const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (root && root[0])
        snprintf(out, n, "%s/%s", root, name);
    else
        snprintf(out, n, "reports/%s", name);
}

int product_eot_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_EVENT_OBJECT_TRACE");
        g_en_known = 1;
    }
    return g_en;
}

void product_eot_reset(void) {
    g_finalized = 0;
    g_uc = NULL;
    g_obj_n = 0;
    g_store_n = 0;
    g_stage_n = 0;
    g_call_n = 0;
    g_next_oid = 1;
    g_en_known = 0;
    g_en = 0;
    g_hook_ok = 0;
    snprintf(g_cur_stage, sizeof(g_cur_stage), "%s", "init");
    memset(g_obj, 0, sizeof(g_obj));
    memset(g_store, 0, sizeof(g_store));
    memset(g_stage, 0, sizeof(g_stage));
    memset(g_call, 0, sizeof(g_call));
}

void product_eot_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_eot_run_id(void) {
    return g_run_id[0] ? g_run_id : "unknown";
}

void product_eot_bind_uc(void *uc) { g_uc = uc; }

#ifdef GWY_HAVE_UNICORN
static uint32_t peek_u32(uc_engine *uc, uint32_t addr) {
    uint32_t v = 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &v);
    return v;
}

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

static void set_stage_name(const char *s) {
    if (!s) return;
    snprintf(g_cur_stage, sizeof(g_cur_stage), "%s", s);
}

static void add_stage(uc_engine *uc, const char *stage, uint32_t payload, uint32_t object,
                      uint32_t wrapper, uint32_t node, uint32_t content) {
    EotStage *st;
    uint32_t pc = 0, lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    uint32_t base;
    if (!product_eot_enabled() || g_stage_n >= EOT_STAGE_CAP) return;
    read_regs(uc, &pc, &lr, &sp, &r0, &r1, &r2, &r3, &r9);
    st = &g_stage[g_stage_n++];
    memset(st, 0, sizeof(*st));
    snprintf(st->stage, sizeof(st->stage), "%s", stage ? stage : "");
    st->pc = pc;
    st->lr = lr;
    st->sp = sp;
    st->r0 = r0;
    st->r1 = r1;
    st->r2 = r2;
    st->r3 = r3;
    st->r9 = r9;
    st->payload_ptr = payload;
    st->object_ptr = object;
    st->wrapper_ptr = wrapper;
    st->node_ptr = node;
    st->content_ptr = content;
    base = content ? content : (object ? object : payload);
    if (base) {
        st->word0 = peek_u32(uc, base);
        st->word4 = peek_u32(uc, base + 4u);
        st->word8 = peek_u32(uc, base + 8u);
        st->wordc = peek_u32(uc, base + 12u);
    }
    printf("[EOT_STAGE] stage=%s pc=0x%X lr=0x%X r0=0x%X payload=0x%X object=0x%X node=0x%X "
           "content=0x%X words=[0]=0x%X [4]=0x%X [8]=0x%X [C]=0x%X evidence=OBSERVED\n",
           st->stage, st->pc, st->lr, st->r0, st->payload_ptr, st->object_ptr, st->node_ptr,
           st->content_ptr, st->word0, st->word4, st->word8, st->wordc);
    fflush(stdout);
}

static EotObj *find_obj(uint32_t base) {
    int i;
    if (!base) return NULL;
    for (i = 0; i < g_obj_n; i++) {
        if (g_obj[i].used && g_obj[i].base == base) return &g_obj[i];
    }
    return NULL;
}

static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                         int64_t value, void *user_data);

static void arm_obj_write_watch(uc_engine *uc, uint32_t base) {
    uc_hook h = 0;
    if (!uc || !base) return;
    /* Narrow 16-byte window only — never full-address-space mem hooks. */
    (void)uc_hook_add(uc, &h, UC_HOOK_MEM_WRITE, on_mem_write, NULL, (uint64_t)base,
                      (uint64_t)base + 15ull);
}

static EotObj *track_obj(uc_engine *uc, uint32_t base, uint32_t born_pc, const char *cls) {
    EotObj *o = find_obj(base);
    if (o) return o;
    if (!base || g_obj_n >= EOT_OBJ_CAP) return NULL;
    o = &g_obj[g_obj_n++];
    memset(o, 0, sizeof(*o));
    o->used = 1;
    o->object_id = g_next_oid++;
    o->base = base;
    o->born_pc = born_pc;
    o->first_word0 = peek_u32(uc, base);
    o->first_word8 = peek_u32(uc, base + 8u);
    snprintf(o->class_guess, sizeof(o->class_guess), "%s", cls ? cls : "unknown");
    arm_obj_write_watch(uc, base);
    printf("[EOT_TRACK] object_id=%u base=0x%X born_pc=0x%X class=%s first=[0]=0x%X [8]=0x%X "
           "evidence=OBSERVED\n",
           o->object_id, o->base, o->born_pc, o->class_guess, o->first_word0, o->first_word8);
    fflush(stdout);
    return o;
}

static void note_store(uc_engine *uc, EotObj *o, uint32_t offset, uint32_t size, uint32_t oldv,
                       uint32_t newv) {
    EotStore *s;
    uint32_t pc = 0, lr = 0, sp = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0;
    uint32_t src_reg = 0xFFu, src_val = newv;
    if (!o || g_store_n >= EOT_STORE_CAP) return;
    read_regs(uc, &pc, &lr, &sp, &r0, &r1, &r2, &r3, &r9);
    if (newv == r0) {
        src_reg = 0;
        src_val = r0;
    } else if (newv == r1) {
        src_reg = 1;
        src_val = r1;
    } else if (newv == r2) {
        src_reg = 2;
        src_val = r2;
    } else if (newv == r3) {
        src_reg = 3;
        src_val = r3;
    }
    s = &g_store[g_store_n++];
    memset(s, 0, sizeof(*s));
    s->object_id = o->object_id;
    s->object_base = o->base;
    s->field_offset = offset;
    s->store_pc = pc;
    s->store_size = size;
    s->old_value = oldv;
    s->new_value = newv;
    s->src_reg = src_reg;
    s->src_value = src_val;
    s->lr = lr;
    s->sp = sp;
    s->r0 = r0;
    s->r1 = r1;
    s->r2 = r2;
    s->r3 = r3;
    s->r9 = r9;
    snprintf(s->stage, sizeof(s->stage), "%s", g_cur_stage);
    if (offset == 0) o->saw_write_plus0 = 1;
    if (offset == 8) {
        o->saw_write_plus8 = 1;
        o->writer_pc_plus8 = pc;
        o->value_plus8 = newv;
    }
    printf("[EOT_STORE] object_id=%u base=0x%X +0x%X size=%u old=0x%X new=0x%X pc=0x%X lr=0x%X "
           "src_reg=%u stage=%s evidence=OBSERVED\n",
           o->object_id, o->base, offset, size, oldv, newv, pc, lr, src_reg, g_cur_stage);
    fflush(stdout);
    {
        static int hdr_done;
        char path[512];
        FILE *fp;
        report_path(path, sizeof(path), "product_event_object_stores.csv");
        fp = fopen(path, hdr_done ? "ab" : "wb");
        if (fp) {
            if (!hdr_done) {
                fprintf(fp, "run_id,object_id,object_base,field_offset,store_pc,store_size,"
                            "old_value,new_value,src_reg,src_value,lr,sp,r0,r1,r2,r3,r9,stage\n");
                hdr_done = 1;
            }
            fprintf(fp,
                    "%s,%u,0x%X,0x%X,0x%X,%u,0x%X,0x%X,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,%s\n",
                    product_eot_run_id(), s->object_id, s->object_base, s->field_offset,
                    s->store_pc, s->store_size, s->old_value, s->new_value, s->src_reg,
                    s->src_value, s->lr, s->sp, s->r0, s->r1, s->r2, s->r3, s->r9, s->stage);
            fclose(fp);
        }
    }
}

static void on_mem_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                         int64_t value, void *user_data) {
    uint32_t addr = (uint32_t)address;
    int i;
    (void)type;
    (void)user_data;
    if (!product_eot_enabled()) return;
    for (i = 0; i < g_obj_n; i++) {
        EotObj *o = &g_obj[i];
        uint32_t off;
        uint32_t oldv;
        if (!o->used) continue;
        if (addr < o->base || addr >= o->base + 16u) continue;
        off = addr - o->base;
        if (off != 0 && off != 4 && off != 8 && off != 12) continue;
        oldv = peek_u32(uc, o->base + off);
        /* value is the store data; for size<4 only low bytes matter. */
        note_store(uc, o, off, (uint32_t)size, oldv, (uint32_t)(uint64_t)value);
        return;
    }
}

static void on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    intptr_t tag = (intptr_t)user_data;
    uint32_t pc = (uint32_t)address;
    uint32_t r0 = 0, r1 = 0;
    (void)size;
    if (!product_eot_enabled()) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    if (tag == 1) { /* 0x30D2F9 Path-A handler enter */
        set_stage_name("path_a_payload_received");
        add_stage(uc, "path_a_payload_received", r0, r1, 0, 0, r1);
        if (r1) track_obj(uc, r1, pc, "path_a_r1_candidate");
        if (r0 && r0 != r1) track_obj(uc, r0, pc, "path_a_r0_candidate");
    } else if (tag == 2) { /* 0x2E4D6C */
        set_stage_name("event_payload_preparation");
        add_stage(uc, "event_payload_preparation", r0, r1, 0, 0, r1);
        if (r1) track_obj(uc, r1, pc, "prep_r1");
    } else if (tag == 3) { /* 0x312A60 push enter: r0=list r1=entry */
        set_stage_name("object_queue_push");
        if (r1) track_obj(uc, r1, pc, "queue_entry");
        add_stage(uc, "queue_push_enter", r1, r1, 0, 0, r1);
    } else if (tag == 4) { /* 0x312A72 after node init — r0=node */
        set_stage_name("node_constructor");
        if (r0) {
            uint32_t item = peek_u32(uc, r0 + 8u);
            EotObj *no = track_obj(uc, r0, pc, "queue_node");
            if (no) no->node = r0;
            if (item) {
                EotObj *eo = track_obj(uc, item, pc, "node_content");
                if (eo) eo->node = r0;
            }
            add_stage(uc, "node_constructor", item, item, r0, r0, item);
        }
    } else if (tag == 5) { /* 0x2DC82E after get_item */
        set_stage_name("consumer_pop_item");
        add_stage(uc, "consumer_get_item_r0", 0, r0, 0, 0, r0);
        if (r0) track_obj(uc, r0, pc, "drain_item");
    } else if (tag == 6) { /* 0x2DC8D4 — BL site toward 0x2E2520 family */
        set_stage_name("0x2E2520_caller");
        add_stage(uc, "0x2E2520_caller", 0, r0, 0, 0, r0);
    } else if (tag == 7) { /* 0x2E4EAE — heap helper arg0 = framing size (r6-2) */
        set_stage_name("framing_heap_size");
        printf("[EOT_FRAMING] pc=0x2E4EAE heap_arg0=0x%X (size=r6-2) evidence=OBSERVED\n", r0);
        fflush(stdout);
        add_stage(uc, "framing_heap_size", 0, 0, 0, 0, 0);
    } else if (tag == 8) { /* 0x2E4EBA — entry = malloc(0xC) return in r0 before ADDS r5,r0 */
        set_stage_name("framing_entry_alloc");
        if (r0) track_obj(uc, r0, pc, "framing_entry");
        add_stage(uc, "framing_entry_alloc", r0, r0, 0, 0, r0);
    } else if (tag == 9) { /* 0x2E4ED8 — STR r1,[r5] writes entry+0 (event_code) */
        uint32_t r5 = 0;
        set_stage_name("framing_store_plus0");
        uc_reg_read(uc, UC_ARM_REG_R5, &r5);
        if (r5) track_obj(uc, r5, pc, "framing_entry");
        add_stage(uc, "framing_store_plus0", r1, r5, 0, 0, r5);
        printf("[EOT_FRAMING] pc=0x2E4ED8 STR [r5=0x%X,#0] r1=0x%X evidence=OBSERVED\n", r5,
               r1);
        fflush(stdout);
    }
}
#endif

void product_eot_arm_hooks(void *uc) {
#ifdef GWY_HAVE_UNICORN
    if (!product_eot_enabled() || !uc || g_hook_ok) return;
    g_uc = uc;
    (void)uc_hook_add((uc_engine *)uc, &g_code_hooks[0], UC_HOOK_CODE, on_code, (void *)(intptr_t)1,
                      0x30D2F9ull, 0x30D2FBull);
    (void)uc_hook_add((uc_engine *)uc, &g_code_hooks[1], UC_HOOK_CODE, on_code, (void *)(intptr_t)2,
                      0x2E4D6Cull, 0x2E4D6Full);
    (void)uc_hook_add((uc_engine *)uc, &g_code_hooks[2], UC_HOOK_CODE, on_code, (void *)(intptr_t)3,
                      0x312A60ull, 0x312A63ull);
    (void)uc_hook_add((uc_engine *)uc, &g_code_hooks[3], UC_HOOK_CODE, on_code, (void *)(intptr_t)4,
                      0x312A72ull, 0x312A75ull);
    (void)uc_hook_add((uc_engine *)uc, &g_code_hooks[4], UC_HOOK_CODE, on_code, (void *)(intptr_t)5,
                      0x2DC82Eull, 0x2DC82Full);
    (void)uc_hook_add((uc_engine *)uc, &g_code_hooks[5], UC_HOOK_CODE, on_code, (void *)(intptr_t)6,
                      0x2DC8D4ull, 0x2DC8D7ull);
    (void)uc_hook_add((uc_engine *)uc, &g_code_hooks[6], UC_HOOK_CODE, on_code, (void *)(intptr_t)7,
                      0x2E4EAEull, 0x2E4EAFull);
    (void)uc_hook_add((uc_engine *)uc, &g_code_hooks[7], UC_HOOK_CODE, on_code, (void *)(intptr_t)8,
                      0x2E4EBAull, 0x2E4EBBull);
    (void)uc_hook_add((uc_engine *)uc, &g_code_hooks[8], UC_HOOK_CODE, on_code, (void *)(intptr_t)9,
                      0x2E4ED8ull, 0x2E4ED9ull);
    g_hook_ok = 1;
    atexit(product_eot_finalize);
    printf("[EOT_HOOKS] armed mem_write+code sites evidence=OBSERVED\n");
    fflush(stdout);
#else
    (void)uc;
#endif
}

void product_eot_on_path_a_begin(void *uc, uint32_t list, uint32_t entry, uint32_t count) {
#ifdef GWY_HAVE_UNICORN
    if (!product_eot_enabled()) return;
    set_stage_name("path_a_begin");
    if (entry) track_obj((uc_engine *)uc, entry, 0x312A60u, "path_a_entry");
    add_stage((uc_engine *)uc, "path_a_begin", entry, entry, 0, 0, entry);
    product_runtime_progress_emit("event_path_a_seen", "eot", "path_a_begin");
#else
    (void)uc;
    (void)entry;
#endif
    (void)list;
    (void)count;
}

void product_eot_on_path_a_linked(void *uc, uint32_t list, uint32_t head, uint32_t node,
                                  uint32_t entry, uint32_t count) {
#ifdef GWY_HAVE_UNICORN
    EotObj *eo;
    EotObj *no;
    if (!product_eot_enabled()) return;
    set_stage_name("node_content_assignment");
    no = node ? track_obj((uc_engine *)uc, node, 0x312A72u, "linked_node") : NULL;
    eo = entry ? track_obj((uc_engine *)uc, entry, 0x312A60u, "linked_entry") : NULL;
    if (no) {
        no->node = node;
        no->list = list;
    }
    if (eo) {
        eo->node = node;
        eo->list = list;
    }
    add_stage((uc_engine *)uc, "node_linked", entry, entry, node, node, entry);
    product_runtime_progress_emit("event_node_linked", "eot", "node_linked");
#else
    (void)uc;
    (void)node;
    (void)entry;
#endif
    (void)list;
    (void)head;
    (void)count;
}

void product_eot_on_get_item(void *uc, uint32_t node, uint32_t item) {
#ifdef GWY_HAVE_UNICORN
    if (!product_eot_enabled()) return;
    set_stage_name("consumer_get");
    if (item) track_obj((uc_engine *)uc, item, 0x312AB4u, "get_item");
    add_stage((uc_engine *)uc, "consumer_get", item, item, node, node, item);
#else
    (void)uc;
    (void)item;
#endif
    (void)node;
}

void product_eot_on_drain_item(void *uc, uint32_t item) {
#ifdef GWY_HAVE_UNICORN
    if (!product_eot_enabled()) return;
    set_stage_name("consumer_pop");
    if (item) track_obj((uc_engine *)uc, item, 0x2DC82Eu, "drain_item");
    add_stage((uc_engine *)uc, "consumer_pop", item, item, 0, 0, item);
#else
    (void)uc;
    (void)item;
#endif
}

void product_eot_on_dispatch_enter(void *uc, uint32_t call_id, uint32_t r0, uint32_t lr,
                                   uint32_t sp, uint32_t r1, uint32_t r2, uint32_t r3,
                                   uint32_t r9) {
#ifdef GWY_HAVE_UNICORN
    EotCall *c;
    EotObj *o;
    char det[128];
    if (!product_eot_enabled() || g_call_n >= EOT_CALL_CAP) return;
    set_stage_name("0x2E2520_entry");
    c = &g_call[g_call_n++];
    memset(c, 0, sizeof(*c));
    c->used = 1;
    c->call_id = call_id ? call_id : (uint32_t)g_call_n;
    c->r0 = r0;
    c->lr = lr;
    c->sp = sp;
    c->r1 = r1;
    c->r2 = r2;
    c->r3 = r3;
    c->r9 = r9;
    if (uc && r0) {
        c->word0 = peek_u32((uc_engine *)uc, r0);
        c->word4 = peek_u32((uc_engine *)uc, r0 + 4u);
        c->word8 = peek_u32((uc_engine *)uc, r0 + 8u);
        c->wordc = peek_u32((uc_engine *)uc, r0 + 12u);
    }
    o = find_obj(r0);
    if (o) {
        c->matched_object_id = o->object_id;
        snprintf(c->classification, sizeof(c->classification), "%s", o->class_guess);
    } else {
        /* R0 may be wrapper: check if any tracked object equals inner +4. */
        if (c->word4) {
            EotObj *inner = find_obj(c->word4);
            if (inner) {
                c->matched_object_id = inner->object_id;
                c->is_wrapper_guess = 1;
                snprintf(c->classification, sizeof(c->classification), "wrapper_over_%s",
                         inner->class_guess);
            } else {
                snprintf(c->classification, sizeof(c->classification), "untracked_at_entry");
            }
        } else {
            snprintf(c->classification, sizeof(c->classification), "untracked_at_entry");
        }
        if (uc && r0) track_obj((uc_engine *)uc, r0, 0x2E2520u, "dispatch_r0");
    }
    add_stage((uc_engine *)uc, "0x2E2520_entry", r0, r0, c->is_wrapper_guess ? r0 : 0, 0, r0);
    printf("[EOT_DISPATCH] event_object_call_id=%u r0=0x%X words=[0]=0x%X [4]=0x%X [8]=0x%X "
           "[C]=0x%X matched_oid=%u class=%s wrapper_guess=%d lr=0x%X evidence=OBSERVED\n",
           c->call_id, c->r0, c->word0, c->word4, c->word8, c->wordc, c->matched_object_id,
           c->classification, c->is_wrapper_guess, c->lr);
    fflush(stdout);
    /* Incremental persist — kill-safe (TerminateProcess may skip atexit). */
    {
        char path[512];
        FILE *fp;
        report_path(path, sizeof(path), "product_event_object_calls.csv");
        fp = fopen(path, g_call_n == 1 ? "wb" : "ab");
        if (fp) {
            if (g_call_n == 1)
                fprintf(fp, "run_id,call_id,r0,matched_oid,word0,word4,word8,wordc,lr,r1,"
                            "wrapper_guess,classification\n");
            fprintf(fp, "%s,%u,0x%X,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%s\n",
                    product_eot_run_id(), c->call_id, c->r0, c->matched_object_id, c->word0,
                    c->word4, c->word8, c->wordc, c->lr, c->r1, c->is_wrapper_guess,
                    c->classification);
            fclose(fp);
        }
    }
    snprintf(det, sizeof(det), "call_id=%u r0=0x%X word0=0x%X word8=0x%X", c->call_id, c->r0,
             c->word0, c->word8);
    product_runtime_progress_emit("event_dispatch_2e2520", "eot", det);
#else
    (void)uc;
    (void)call_id;
    (void)r0;
    (void)lr;
    (void)sp;
    (void)r1;
    (void)r2;
    (void)r3;
    (void)r9;
#endif
}

void product_eot_finalize(void) {
    char path[512];
    FILE *fp;
    int i;
    if (g_finalized || !product_eot_enabled()) return;
    g_finalized = 1;
    report_path(path, sizeof(path), "product_event_object_stores.csv");
    fp = fopen(path, "wb");
    if (fp) {
        fprintf(fp, "run_id,object_id,object_base,field_offset,store_pc,store_size,old_value,"
                    "new_value,src_reg,src_value,lr,sp,r0,r1,r2,r3,r9,stage\n");
        for (i = 0; i < g_store_n; i++) {
            EotStore *s = &g_store[i];
            fprintf(fp,
                    "%s,%u,0x%X,0x%X,0x%X,%u,0x%X,0x%X,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,%s\n",
                    product_eot_run_id(), s->object_id, s->object_base, s->field_offset,
                    s->store_pc, s->store_size, s->old_value, s->new_value, s->src_reg,
                    s->src_value, s->lr, s->sp, s->r0, s->r1, s->r2, s->r3, s->r9, s->stage);
        }
        fclose(fp);
    }
    report_path(path, sizeof(path), "product_event_object_stages.csv");
    fp = fopen(path, "wb");
    if (fp) {
        fprintf(fp, "run_id,stage,pc,lr,sp,r0,r1,r2,r3,r9,payload,object,wrapper,node,content,"
                    "word0,word4,word8,wordc\n");
        for (i = 0; i < g_stage_n; i++) {
            EotStage *s = &g_stage[i];
            fprintf(fp,
                    "%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X\n",
                    product_eot_run_id(), s->stage, s->pc, s->lr, s->sp, s->r0, s->r1, s->r2,
                    s->r3, s->r9, s->payload_ptr, s->object_ptr, s->wrapper_ptr, s->node_ptr,
                    s->content_ptr, s->word0, s->word4, s->word8, s->wordc);
        }
        fclose(fp);
    }
    report_path(path, sizeof(path), "product_event_object_calls.csv");
    fp = fopen(path, "wb");
    if (fp) {
        fprintf(fp, "run_id,call_id,r0,matched_oid,word0,word4,word8,wordc,lr,r1,wrapper_guess,"
                    "classification\n");
        for (i = 0; i < g_call_n; i++) {
            EotCall *c = &g_call[i];
            fprintf(fp, "%s,%u,0x%X,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%s\n",
                    product_eot_run_id(), c->call_id, c->r0, c->matched_object_id, c->word0,
                    c->word4, c->word8, c->wordc, c->lr, c->r1, c->is_wrapper_guess,
                    c->classification);
        }
        fclose(fp);
    }
    report_path(path, sizeof(path), "product_event_object_objects.csv");
    fp = fopen(path, "wb");
    if (fp) {
        fprintf(fp, "run_id,object_id,base,node,list,born_pc,first_w0,first_w8,saw_w0,saw_w8,"
                    "writer_pc_w8,value_w8,class\n");
        for (i = 0; i < g_obj_n; i++) {
            EotObj *o = &g_obj[i];
            fprintf(fp, "%s,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%d,0x%X,0x%X,%s\n",
                    product_eot_run_id(), o->object_id, o->base, o->node, o->list, o->born_pc,
                    o->first_word0, o->first_word8, o->saw_write_plus0, o->saw_write_plus8,
                    o->writer_pc_plus8, o->value_plus8, o->class_guess);
        }
        fclose(fp);
    }
    printf("[EOT_FINALIZE] objects=%d stores=%d stages=%d calls=%d run_id=%s evidence=OBSERVED\n",
           g_obj_n, g_store_n, g_stage_n, g_call_n, product_eot_run_id());
    fflush(stdout);
}
