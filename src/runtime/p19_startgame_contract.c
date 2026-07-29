#include "gwy_launcher/p19_startgame_contract.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/mrp_runtime_stack.h"
#include "gwy_launcher/original_gwy_bootstrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* File offsets in gbrwcore.ext (research breakpoints; not product entries). */
#define OFF_API_BUILDER 0x1B400u
#define OFF_SG_NAME_STORE 0x1B4F4u
#define OFF_SG_FN_STORE 0x1B4FAu
#define OFF_SG_ENTRY 0x1AE74u
#define OFF_SG_PARSER_RET 0x1AE9Au
#define OFF_OPCODE300_BLX 0x1AEB8u

/* Research assertion for current SHA only (Thumb pointer with +1). */
#define RESEARCH_SG_THUMB_PTR 0x306655u

#define P19_API_ROWS 64
#define P19_OPCODE_ROWS 32

typedef struct {
    char name[48];
    uint32_t function_ptr;
    uint32_t table_off;
    char owner[48];
    uint32_t r9;
    uint32_t table_obj;
    uint32_t generation;
} P19ApiRow;

typedef struct {
    uint32_t pc;
    uint32_t r9;
    uint32_t dispatcher;
    uint32_t opcode;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
    uint32_t stack0;
    uint32_t stack1;
    char note[64];
} P19OpcodeRow;

static struct {
    int enabled_known;
    int enabled;
    int finalized;

    uint32_t gbrw_base;
    uint32_t gbrw_size;
    int base_known;

    int hit_api_builder;
    int hit_sg_name_store;
    int hit_sg_fn_store;
    int hit_sg_entry;
    int hit_parser_ret;
    int hit_opcode300;
    int nested_jjfb;
    int child_robotol;

    uint32_t live_r9;
    uint32_t table_obj;
    uint32_t sg_name_ptr;
    uint32_t sg_fn_ptr;
    char sg_name[48];

    uint32_t sg_entry_r0;
    uint32_t sg_arg1;
    uint32_t sg_arg2;
    uint32_t sg_arg3;
    int args_valid;
    char arg1_dump[256];
    char arg2_dump[256];
    char arg3_dump[256];

    uint32_t dispatcher_1488;
    uint32_t dispatcher_writer_pc;
    char dispatcher_owner[48];

    P19ApiRow apis[P19_API_ROWS];
    int api_n;
    P19OpcodeRow ops[P19_OPCODE_ROWS];
    int op_n;

    int table_scanned;
    uint32_t max_off_seen;
    void *uc;
    int hooks_armed;
    int table_poll_done;
} g_p19;

static int env_is_1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

int p19_startgame_contract_enabled(void) {
    if (g_p19.enabled_known) return g_p19.enabled;
    g_p19.enabled = env_is_1("JJFB_P19_STARTGAME_CONTRACT");
    if (!g_p19.enabled && original_gwy_bootstrap_enabled() && env_is_1("JJFB_ORIGINAL_LOAD_GAMELIST"))
        g_p19.enabled = 1;
    g_p19.enabled_known = 1;
    return g_p19.enabled;
}

void p19_startgame_contract_reset(void) { memset(&g_p19, 0, sizeof(g_p19)); }

int p19_startgame_contract_prefer_gamelist_continue(void) {
    return p19_startgame_contract_enabled() || env_is_1("JJFB_ORIGINAL_LOAD_GAMELIST");
}

int p19_gate_api_builder(void) { return g_p19.hit_api_builder; }
int p19_gate_startgame_ptr(void) { return g_p19.sg_fn_ptr != 0; }
int p19_gate_startgame_entry(void) { return g_p19.hit_sg_entry; }
int p19_gate_three_args(void) { return g_p19.args_valid; }
int p19_gate_opcode300(void) { return g_p19.hit_opcode300; }
int p19_gate_nested_jjfb(void) { return g_p19.nested_jjfb; }
int p19_gate_child_robotol(void) { return g_p19.child_robotol; }

void p19_startgame_contract_on_module_map(const char *module_name, uint32_t base, uint32_t size) {
    if (!p19_startgame_contract_enabled() || !module_name || !base) return;
    if (!strstr(module_name, "gbrwcore")) return;
    g_p19.gbrw_base = base & ~1u;
    g_p19.gbrw_size = size ? size : 147196u;
    g_p19.base_known = 1;
    printf("[P19_GBRWCORE_MAP] base=0x%X size=0x%X api_builder=0x%X sg_entry=0x%X "
           "sg_fn_store=0x%X evidence=OBSERVED\n",
           g_p19.gbrw_base, g_p19.gbrw_size, g_p19.gbrw_base + OFF_API_BUILDER,
           g_p19.gbrw_base + OFF_SG_ENTRY, g_p19.gbrw_base + OFF_SG_FN_STORE);
    fflush(stdout);
    if (g_p19.uc) p19_startgame_contract_bind_uc(g_p19.uc);
}

#ifdef GWY_HAVE_UNICORN
static void p19_bp_cb(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    uint32_t regs[16];
    uint32_t pc = (uint32_t)address;
    static const int k_regs[16] = {
        UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3, UC_ARM_REG_R4,
        UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,  UC_ARM_REG_R8, UC_ARM_REG_R9,
        UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR,
        UC_ARM_REG_PC};
    int i;
    (void)size;
    (void)user;
    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++)
        uc_reg_read(uc, k_regs[i], &regs[i]);
    p19_startgame_contract_on_code(uc, 0, "gbrwcore.ext", pc, regs);
}

static void arm_bp(void *uc, uint32_t va, const char *tag) {
    uc_hook h = 0;
    uint64_t a = (uint64_t)(va & ~1u);
    if (!uc || !va) return;
    if (uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)p19_bp_cb, NULL, a, a + 2ull) ==
        UC_ERR_OK) {
        printf("[P19_BP_ARM] tag=%s va=0x%X evidence=DOCUMENTED\n", tag, va);
        fflush(stdout);
    }
}
#endif

void p19_startgame_contract_bind_uc(void *uc) {
    if (!p19_startgame_contract_enabled() || !uc) return;
    g_p19.uc = uc;
#ifdef GWY_HAVE_UNICORN
    if (!g_p19.base_known || g_p19.hooks_armed) return;
    g_p19.hooks_armed = 1;
    arm_bp(uc, g_p19.gbrw_base + OFF_API_BUILDER, "api_builder");
    arm_bp(uc, g_p19.gbrw_base + OFF_SG_NAME_STORE, "sg_name_store");
    arm_bp(uc, g_p19.gbrw_base + OFF_SG_FN_STORE, "sg_fn_store");
    arm_bp(uc, g_p19.gbrw_base + OFF_SG_ENTRY, "sg_entry");
    arm_bp(uc, g_p19.gbrw_base + OFF_SG_PARSER_RET, "sg_parser_ret");
    arm_bp(uc, g_p19.gbrw_base + OFF_OPCODE300_BLX, "opcode300_blx");
    /* Also cover a few bytes into the builder body (entry may be Thumb+1). */
    arm_bp(uc, g_p19.gbrw_base + OFF_API_BUILDER + 2u, "api_builder_plus2");
    arm_bp(uc, (g_p19.gbrw_base + OFF_SG_ENTRY) | 1u, "sg_entry_thumb");
#endif
}

static int read_cstr(void *uc, uint32_t addr, char *out, size_t cap);
static void add_api_row(const char *name, uint32_t fn, uint32_t off, uint32_t r9, uint32_t table,
                        const char *owner);
static void scan_api_table(void *uc, uint32_t table, uint32_t r9);

static void try_poll_api_table(void *uc, uint32_t r9) {
    uint32_t table = 0, fn = 0, name_p = 0;
    char name[48];
    if (!uc || !r9 || g_p19.table_poll_done) return;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x08u, &table) || !table) return;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, table + 0x78u, &fn) || !fn) return;
    g_p19.table_poll_done = 1;
    g_p19.live_r9 = r9;
    g_p19.table_obj = table;
    g_p19.sg_fn_ptr = fn;
    guest_memory_uc_peek_u32((struct uc_struct *)uc, table + 0x74u, &name_p);
    g_p19.sg_name_ptr = name_p;
    name[0] = 0;
    if (name_p) read_cstr(uc, name_p, name, sizeof(name));
    snprintf(g_p19.sg_name, sizeof(g_p19.sg_name), "%s", name[0] ? name : "lib.startGame");
    printf("[P19_API_TABLE_POLL] R9=0x%X table=0x%X function_ptr=0x%X name=\"%s\" "
           "evidence=OBSERVED note=post_er_rw_poll\n",
           r9, table, fn, g_p19.sg_name);
    fflush(stdout);
    add_api_row(g_p19.sg_name, fn, 0x78u, r9, table, "gbrwcore.ext");
    scan_api_table(uc, table, r9);
}

static int read_cstr(void *uc, uint32_t addr, char *out, size_t cap) {
    size_t i;
    if (!uc || !addr || !out || cap < 2) return 0;
    out[0] = 0;
    for (i = 0; i + 1 < cap && i < 96; i++) {
        uint8_t b = 0;
        if (!guest_memory_uc_peek((struct uc_struct *)uc, addr + (uint32_t)i, &b, 1)) return 0;
        out[i] = (char)b;
        if (!b) return 1;
        if (b < 32 || b > 126) {
            out[i] = 0;
            return i > 0;
        }
    }
    out[cap - 1] = 0;
    return 1;
}

static void hex_preview(void *uc, uint32_t addr, char *out, size_t cap, size_t nbytes) {
    size_t i;
    size_t n = nbytes;
    if (!out || cap < 8) return;
    out[0] = 0;
    if (!uc || !addr) {
        snprintf(out, cap, "(null)");
        return;
    }
    if (n > 32) n = 32;
    for (i = 0; i < n && (i * 2 + 3) < cap; i++) {
        uint8_t b = 0;
        char t[4];
        if (!guest_memory_uc_peek((struct uc_struct *)uc, addr + (uint32_t)i, &b, 1)) break;
        snprintf(t, sizeof(t), "%02X", b);
        strncat(out, t, cap - strlen(out) - 1);
    }
}

static void describe_arg(void *uc, uint32_t v, char *out, size_t cap) {
    char s[96];
    char hx[80];
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!v) {
        snprintf(out, cap, "value=0x0 kind=null");
        return;
    }
    if (read_cstr(uc, v, s, sizeof(s)) && s[0] && strlen(s) >= 3) {
        snprintf(out, cap, "value=0x%X kind=string str=\"%.80s\"", v, s);
        return;
    }
    hex_preview(uc, v, hx, sizeof(hx), 32);
    snprintf(out, cap, "value=0x%X kind=ptr_or_object bytes=%s", v, hx);
}

static void add_api_row(const char *name, uint32_t fn, uint32_t off, uint32_t r9, uint32_t table,
                        const char *owner) {
    P19ApiRow *r;
    int i;
    if (!name || !name[0] || g_p19.api_n >= P19_API_ROWS) return;
    for (i = 0; i < g_p19.api_n; i++) {
        if (strcmp(g_p19.apis[i].name, name) == 0) {
            g_p19.apis[i].function_ptr = fn;
            g_p19.apis[i].table_off = off;
            g_p19.apis[i].r9 = r9;
            g_p19.apis[i].table_obj = table;
            return;
        }
    }
    r = &g_p19.apis[g_p19.api_n++];
    memset(r, 0, sizeof(*r));
    snprintf(r->name, sizeof(r->name), "%s", name);
    r->function_ptr = fn;
    r->table_off = off;
    r->r9 = r9;
    r->table_obj = table;
    snprintf(r->owner, sizeof(r->owner), "%s", owner ? owner : "gbrwcore.ext");
    original_gwy_api_register(name, fn, 0, 0, table, owner ? owner : "gbrwcore.ext", r9, 1,
                              fn ? "api_table_entry" : "api_table_name_only");
}

static void scan_api_table(void *uc, uint32_t table, uint32_t r9) {
    uint32_t off;
    if (!uc || !table || g_p19.table_scanned) return;
    g_p19.table_scanned = 1;
    /* Pairs of (name_ptr, fn_ptr) — startGame at +0x74/+0x78; scan 0..0x200. */
    for (off = 0; off + 8 <= 0x200u; off += 8u) {
        uint32_t name_p = 0, fn_p = 0;
        char name[48];
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, table + off, &name_p)) continue;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, table + off + 4u, &fn_p)) continue;
        if (!name_p || !fn_p) continue;
        if (!read_cstr(uc, name_p, name, sizeof(name))) continue;
        if (strncmp(name, "lib.", 4) != 0 && strcmp(name, "startGame") != 0 &&
            strcmp(name, "runapp") != 0)
            continue;
        add_api_row(name, fn_p, off, r9, table, "gbrwcore.ext");
        printf("[P19_API_TABLE_ROW] name=%s function_ptr=0x%X table_off=0x%X table=0x%X R9=0x%X "
               "owner=gbrwcore.ext evidence=OBSERVED\n",
               name, fn_p, off, table, r9);
    }
    fflush(stdout);
}

static void note_opcode(uint32_t pc, uint32_t r9, uint32_t disp, uint32_t opcode, uint32_t a1,
                        uint32_t a2, uint32_t a3, uint32_t s0, uint32_t s1, const char *note) {
    P19OpcodeRow *r;
    if (g_p19.op_n >= P19_OPCODE_ROWS) return;
    r = &g_p19.ops[g_p19.op_n++];
    memset(r, 0, sizeof(*r));
    r->pc = pc;
    r->r9 = r9;
    r->dispatcher = disp;
    r->opcode = opcode;
    r->arg1 = a1;
    r->arg2 = a2;
    r->arg3 = a3;
    r->stack0 = s0;
    r->stack1 = s1;
    snprintf(r->note, sizeof(r->note), "%s", note ? note : "");
}

static void on_sg_fn_store(void *uc, uint32_t pc, const uint32_t regs[16]) {
    uint32_t r9 = regs ? regs[9] : 0;
    uint32_t table = 0;
    uint32_t name_p = 0;
    uint32_t fn_p = 0;
    char name[48];
    (void)pc;
    g_p19.hit_sg_fn_store = 1;
    g_p19.live_r9 = r9;
    if (uc && r9)
        guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x08u, &table);
    g_p19.table_obj = table;
    if (uc && table) {
        guest_memory_uc_peek_u32((struct uc_struct *)uc, table + 0x74u, &name_p);
        guest_memory_uc_peek_u32((struct uc_struct *)uc, table + 0x78u, &fn_p);
    }
    /* Prefer just-written values from regs if table not yet visible. */
    if (!fn_p && regs) {
        /* STR sequence: often Rt holds pointer being stored — scan r0-r3. */
        int i;
        for (i = 0; i < 4; i++) {
            uint32_t c = regs[i];
            if ((c & ~1u) == (g_p19.gbrw_base + OFF_SG_ENTRY) || c == RESEARCH_SG_THUMB_PTR ||
                ((c & ~1u) >= g_p19.gbrw_base && (c & ~1u) < g_p19.gbrw_base + g_p19.gbrw_size &&
                 ((c & ~1u) - g_p19.gbrw_base) == OFF_SG_ENTRY)) {
                fn_p = c | 1u;
                break;
            }
        }
    }
    g_p19.sg_name_ptr = name_p;
    g_p19.sg_fn_ptr = fn_p;
    name[0] = 0;
    if (name_p) read_cstr(uc, name_p, name, sizeof(name));
    snprintf(g_p19.sg_name, sizeof(g_p19.sg_name), "%s", name[0] ? name : "lib.startGame");
    printf("[P19_STARTGAME_PTR_STORE] pc=0x%X R9=0x%X table=0x%X name_ptr=0x%X "
           "function_ptr=0x%X name=\"%s\" research_assert_ptr=0x%X match=%s evidence=OBSERVED\n",
           pc, r9, table, name_p, fn_p, g_p19.sg_name, RESEARCH_SG_THUMB_PTR,
           (fn_p == RESEARCH_SG_THUMB_PTR || (fn_p & ~1u) == (RESEARCH_SG_THUMB_PTR & ~1u))
               ? "yes"
               : "use_runtime_value");
    fflush(stdout);
    add_api_row(g_p19.sg_name[0] ? g_p19.sg_name : "lib.startGame", fn_p, 0x78u, r9, table,
                "gbrwcore.ext");
    if (table) scan_api_table(uc, table, r9);
    mrp_runtime_stack_update_top(mrp_runtime_stack_global(), r9, 0, table, 0, 0, 0);
}

static void on_sg_entry(void *uc, uint32_t pc, const uint32_t regs[16]) {
    g_p19.hit_sg_entry = 1;
    g_p19.sg_entry_r0 = regs ? regs[0] : 0;
    g_p19.live_r9 = regs ? regs[9] : g_p19.live_r9;
    printf("[P19_STARTGAME_ENTER] pc=0x%X R0=0x%X (VM_call_object) R1=0x%X R2=0x%X R3=0x%X "
           "R9=0x%X SP=0x%X LR=0x%X evidence=OBSERVED\n",
           pc, regs ? regs[0] : 0, regs ? regs[1] : 0, regs ? regs[2] : 0, regs ? regs[3] : 0,
           regs ? regs[9] : 0, regs ? regs[13] : 0, regs ? regs[14] : 0);
    if (uc && regs && regs[0]) {
        char hx[160];
        hex_preview(uc, regs[0], hx, sizeof(hx), 64);
        printf("[P19_STARTGAME_VM_OBJECT] R0=0x%X dump64=%s evidence=OBSERVED\n", regs[0], hx);
    }
    fflush(stdout);
}

static void on_parser_ret(void *uc, uint32_t pc, const uint32_t regs[16]) {
    uint32_t sp = regs ? regs[13] : 0;
    uint32_t a1 = 0, a2 = 0, a3 = 0;
    (void)pc;
    g_p19.hit_parser_ret = 1;
    if (uc && sp) {
        guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x14u, &a1);
        guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x10u, &a2);
        guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x0Cu, &a3);
    }
    g_p19.sg_arg1 = a1;
    g_p19.sg_arg2 = a2;
    g_p19.sg_arg3 = a3;
    g_p19.args_valid = 1;
    describe_arg(uc, a1, g_p19.arg1_dump, sizeof(g_p19.arg1_dump));
    describe_arg(uc, a2, g_p19.arg2_dump, sizeof(g_p19.arg2_dump));
    describe_arg(uc, a3, g_p19.arg3_dump, sizeof(g_p19.arg3_dump));
    printf("[P19_STARTGAME_ARGS] arg1=%s\n", g_p19.arg1_dump);
    printf("[P19_STARTGAME_ARGS] arg2=%s\n", g_p19.arg2_dump);
    printf("[P19_STARTGAME_ARGS] arg3=%s\n", g_p19.arg3_dump);
    printf("[P19_STARTGAME_ARGS] summary arg1=0x%X arg2=0x%X arg3=0x%X SP=0x%X "
           "evidence=OBSERVED\n",
           a1, a2, a3, sp);
    fflush(stdout);
}

static void on_opcode300(void *uc, uint32_t pc, const uint32_t regs[16]) {
    uint32_t r9 = regs ? regs[9] : 0;
    uint32_t disp = 0;
    uint32_t sp = regs ? regs[13] : 0;
    uint32_t s0 = 0, s1 = 0;
    g_p19.hit_opcode300 = 1;
    g_p19.live_r9 = r9;
    if (uc && r9)
        guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x1488u, &disp);
    g_p19.dispatcher_1488 = disp;
    if (uc && sp) {
        guest_memory_uc_peek_u32((struct uc_struct *)uc, sp, &s0);
        guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 4u, &s1);
    }
    printf("[P19_OPCODE300] pc=0x%X dispatcher=[R9+0x1488]=0x%X R0=0x%X R1=0x%X R2=0x%X R3=0x%X "
           "stack0=0x%X stack1=0x%X R9=0x%X evidence=OBSERVED\n",
           pc, disp, regs ? regs[0] : 0, regs ? regs[1] : 0, regs ? regs[2] : 0,
           regs ? regs[3] : 0, s0, s1, r9);
    fflush(stdout);
    note_opcode(pc, r9, disp, regs ? regs[0] : 0, regs ? regs[1] : 0, regs ? regs[2] : 0,
                regs ? regs[3] : 0, s0, s1, disp ? "dispatcher_nonzero" : "dispatcher_empty");
    mrp_runtime_stack_update_top(mrp_runtime_stack_global(), r9, 0, 0, disp, 0, 0);
}

static void maybe_watch_r9_1488_write(void *uc, uint32_t pc, const uint32_t regs[16],
                                      const char *module) {
    uint32_t r9, slot, cur = 0;
    if (!uc || !regs) return;
    r9 = regs[9];
    if (!r9) return;
    slot = r9 + 0x1488u;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, slot, &cur)) return;
    if (cur && cur != g_p19.dispatcher_1488) {
        g_p19.dispatcher_1488 = cur;
        g_p19.dispatcher_writer_pc = pc;
        snprintf(g_p19.dispatcher_owner, sizeof(g_p19.dispatcher_owner), "%s",
                 module ? module : "?");
        printf("[P19_DISPATCHER_1488] value=0x%X observed_at_pc=0x%X R9=0x%X owner=%s "
               "evidence=OBSERVED note=slot_nonzero\n",
               cur, pc, r9, g_p19.dispatcher_owner);
        fflush(stdout);
    }
}

void p19_startgame_contract_on_code(void *uc, uint64_t module_id, const char *module_name,
                                    uint32_t pc, const uint32_t regs[16]) {
    uint32_t pca;
    uint32_t off;
    (void)module_id;
    if (!p19_startgame_contract_enabled()) return;
    if (!g_p19.base_known && module_name && strstr(module_name, "gbrwcore") && regs) {
        /* Infer base from PC if map callback missed. */
        if (pc > OFF_API_BUILDER) {
            /* weak: leave unknown until map */
        }
    }
    if (module_name && strstr(module_name, "jjfb")) {
        if (!g_p19.nested_jjfb) {
            g_p19.nested_jjfb = 1;
            original_gwy_bootstrap_on_nested_jjfb("gwy/jjfb.mrp", original_gwy_cfg36_param(),
                                                  regs ? regs[9] : 0);
            printf("[P19_NESTED_JJFB] package=gwy/jjfb.mrp evidence=OBSERVED\n");
            fflush(stdout);
        }
    }
    if (module_name && strstr(module_name, "robotol") && !g_p19.child_robotol) {
        g_p19.child_robotol = 1;
        printf("[P19_CHILD_ROBOTOL] module=robotol.ext pc=0x%X evidence=OBSERVED\n", pc);
        fflush(stdout);
    }

    if (!g_p19.base_known) return;
    pca = pc & ~1u;
    if (pca < g_p19.gbrw_base || pca >= g_p19.gbrw_base + g_p19.gbrw_size) {
        maybe_watch_r9_1488_write(uc, pc, regs, module_name);
        return;
    }
    off = pca - g_p19.gbrw_base;
    if (off > g_p19.max_off_seen) {
        g_p19.max_off_seen = off;
        if (g_p19.max_off_seen == off && (off == OFF_API_BUILDER || off > 0x1000u) &&
            (off & 0xFFFu) == 0) {
            printf("[P19_GBRWCORE_PC_WATERMARK] max_off=0x%X pc=0x%X evidence=OBSERVED\n", off,
                   pc);
            fflush(stdout);
        }
    }

    if (!g_p19.hit_api_builder && off >= OFF_API_BUILDER && off < OFF_API_BUILDER + 0x100u) {
        g_p19.hit_api_builder = 1;
        printf("[P19_API_BUILDER] pc=0x%X off=0x%X R9=0x%X evidence=OBSERVED\n", pc, off,
               regs ? regs[9] : 0);
        fflush(stdout);
    }
    if (!g_p19.hit_sg_name_store && off >= OFF_SG_NAME_STORE && off < OFF_SG_NAME_STORE + 4u) {
        g_p19.hit_sg_name_store = 1;
        printf("[P19_STARTGAME_NAME_STORE] pc=0x%X off=0x%X evidence=OBSERVED\n", pc, off);
        fflush(stdout);
    }
    if (!g_p19.hit_sg_fn_store && off >= OFF_SG_FN_STORE && off < OFF_SG_FN_STORE + 4u) {
        on_sg_fn_store(uc, pc, regs);
    }
    if (!g_p19.hit_sg_entry && off >= OFF_SG_ENTRY && off < OFF_SG_ENTRY + 4u) {
        on_sg_entry(uc, pc, regs);
    }
    if (!g_p19.hit_parser_ret && off >= OFF_SG_PARSER_RET && off < OFF_SG_PARSER_RET + 4u) {
        on_parser_ret(uc, pc, regs);
    }
    if (!g_p19.hit_opcode300 && off >= OFF_OPCODE300_BLX && off < OFF_OPCODE300_BLX + 4u) {
        on_opcode300(uc, pc, regs);
    }
    if (regs && regs[9] && regs[9] != 0x280400u)
        try_poll_api_table(uc, regs[9]);
    maybe_watch_r9_1488_write(uc, pc, regs, module_name);
}

static void write_api_csv(const char *path) {
    FILE *fp;
    int i;
    fp = fopen(path, "wb");
    if (!fp) return;
    fprintf(fp, "api_name,function_pointer,table_offset,owner_module,R9,table_object,generation,"
                "kind\n");
    for (i = 0; i < g_p19.api_n; i++) {
        const P19ApiRow *r = &g_p19.apis[i];
        fprintf(fp, "%s,0x%X,0x%X,%s,0x%X,0x%X,%u,%s\n", r->name, r->function_ptr, r->table_off,
                r->owner, r->r9, r->table_obj, r->generation ? r->generation : 1u,
                r->function_ptr ? "api_table_entry" : "name_only");
    }
    if (g_p19.sg_fn_ptr && g_p19.api_n == 0) {
        fprintf(fp, "lib.startGame,0x%X,0x78,gbrwcore.ext,0x%X,0x%X,1,api_table_entry\n",
                g_p19.sg_fn_ptr, g_p19.live_r9, g_p19.table_obj);
    }
    fclose(fp);
}

static void write_opcode_csv(const char *path) {
    FILE *fp;
    int i;
    fp = fopen(path, "wb");
    if (!fp) return;
    fprintf(fp, "pc,R9,dispatcher,opcode,arg1,arg2,arg3,stack0,stack1,note\n");
    for (i = 0; i < g_p19.op_n; i++) {
        const P19OpcodeRow *r = &g_p19.ops[i];
        fprintf(fp, "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%s\n", r->pc, r->r9,
                r->dispatcher, r->opcode, r->arg1, r->arg2, r->arg3, r->stack0, r->stack1, r->note);
    }
    fclose(fp);
}

static void write_contract_md(const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return;
    fprintf(fp, "# P19 startGame Runtime Contract\n\n");
    fprintf(fp, "## Gates\n\n");
    fprintf(fp, "| Gate | Hit |\n|---|---|\n");
    fprintf(fp, "| 1 API builder 0x1B400 | %s |\n", g_p19.hit_api_builder ? "YES" : "NO");
    fprintf(fp, "| 2 startGame function_ptr nonzero | %s (0x%X) |\n",
            g_p19.sg_fn_ptr ? "YES" : "NO", g_p19.sg_fn_ptr);
    fprintf(fp, "| 3 startGame entry | %s |\n", g_p19.hit_sg_entry ? "YES" : "NO");
    fprintf(fp, "| 4 three args parsed | %s |\n", g_p19.args_valid ? "YES" : "NO");
    fprintf(fp, "| 5 opcode 300 | %s |\n", g_p19.hit_opcode300 ? "YES" : "NO");
    fprintf(fp, "| 6 nested jjfb | %s |\n", g_p19.nested_jjfb ? "YES" : "NO");
    fprintf(fp, "| 7 child robotol | %s |\n\n", g_p19.child_robotol ? "YES" : "NO");

    fprintf(fp, "## startGame pointer\n\n");
    fprintf(fp, "- gbrwcore base: `0x%X`\n", g_p19.gbrw_base);
    fprintf(fp, "- live R9: `0x%X`\n", g_p19.live_r9);
    fprintf(fp, "- table `[R9+8]`: `0x%X`\n", g_p19.table_obj);
    fprintf(fp, "- name `@table+0x74`: `0x%X` (\"%s\")\n", g_p19.sg_name_ptr, g_p19.sg_name);
    fprintf(fp, "- function_ptr `@table+0x78`: `0x%X`\n", g_p19.sg_fn_ptr);
    fprintf(fp, "- research assert Thumb ptr: `0x%X` (not a product hardcode)\n\n",
            RESEARCH_SG_THUMB_PTR);

    fprintf(fp, "## Three arguments\n\n");
    if (g_p19.args_valid) {
        fprintf(fp, "- arg1: %s\n", g_p19.arg1_dump);
        fprintf(fp, "- arg2: %s\n", g_p19.arg2_dump);
        fprintf(fp, "- arg3: %s\n\n", g_p19.arg3_dump);
    } else {
        fprintf(fp, "- not captured (parser return not hit)\n\n");
    }

    fprintf(fp, "## Opcode 300 dispatcher\n\n");
    fprintf(fp, "- `[R9+0x1488]`: `0x%X`\n", g_p19.dispatcher_1488);
    fprintf(fp, "- observed owner/module: `%s`\n",
            g_p19.dispatcher_owner[0] ? g_p19.dispatcher_owner : "(unknown)");
    fprintf(fp, "- writer pc hint: `0x%X`\n\n", g_p19.dispatcher_writer_pc);

    fprintf(fp, "## Nested JJFB / first screen\n\n");
    fprintf(fp, "- nested jjfb: %s\n", g_p19.nested_jjfb ? "YES" : "NO");
    fprintf(fp, "- child robotol: %s\n", g_p19.child_robotol ? "YES" : "NO");
    fprintf(fp, "- gbrwcore max PC offset seen: `0x%X` (api_builder needs `0x%X`)\n",
            g_p19.max_off_seen, OFF_API_BUILDER);
    fprintf(fp, "- parent code15: not claimed here (see matrix)\n");
    fprintf(fp, "- first-screen vs direct_boot: see `P19_NESTED_JJFB_MATRIX.csv`\n\n");

    fprintf(fp, "## Policy\n\n");
    fprintf(fp, "- No descriptor-string call into startGame\n");
    fprintf(fp, "- No static `0x306655` as product entry\n");
    fprintf(fp, "- No synthetic code15 / forced E6C\n");
    fprintf(fp, "- cfg36 napptype=12 from live cfg.bin\n");
    fclose(fp);
}

void p19_startgame_contract_finalize(const char *stop_reason) {
    const char *reports;
    char api_path[512], op_path[512], md_path[512], stack_path[512];
    if (!p19_startgame_contract_enabled() || g_p19.finalized) return;
    g_p19.finalized = 1;
    reports = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (!reports || !reports[0]) reports = "reports";
    snprintf(api_path, sizeof(api_path), "%s/P19_API_TABLE.csv", reports);
    snprintf(op_path, sizeof(op_path), "%s/P19_OPCODE300_TRACE.csv", reports);
    snprintf(md_path, sizeof(md_path), "%s/P19_STARTGAME_RUNTIME_CONTRACT.md", reports);
    snprintf(stack_path, sizeof(stack_path), "%s/ORIGINAL_GWY_RUNTIME_STACK.json", reports);
    write_api_csv(api_path);
    write_opcode_csv(op_path);
    write_contract_md(md_path);
    (void)mrp_runtime_stack_write_json(mrp_runtime_stack_global(), stack_path);
    printf("[P19_FINALIZE] stop=%s api_builder=%d sg_ptr=0x%X sg_enter=%d args=%d op300=%d "
           "nested=%d robotol=%d max_off=0x%X api_csv=%s evidence=OBSERVED\n",
           stop_reason ? stop_reason : "?", g_p19.hit_api_builder, g_p19.sg_fn_ptr,
           g_p19.hit_sg_entry, g_p19.args_valid, g_p19.hit_opcode300, g_p19.nested_jjfb,
           g_p19.child_robotol, g_p19.max_off_seen, api_path);
    fflush(stdout);
}
