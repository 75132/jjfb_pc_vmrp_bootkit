#include "gwy_launcher/p20_gbrwcore_lifecycle.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_runtime_stack.h"
#include "gwy_launcher/original_gwy_bootstrap.h"
#include "gwy_launcher/p19_startgame_contract.h"
#include "gwy_launcher/platform_handler_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* File offsets — VAs use refined image_base (pad-aware). */
#define OFF_MRPGCMAP_HELPER 0x21284u
#define OFF_DISPATCHER 0x217ECu
#define OFF_CMD0_BRANCH 0x2181Cu
#define OFF_CB_REG_CALL 0x21828u
#define OFF_REG_FN 0x216D4u
#define OFF_PLAT_REG_WRAP 0x214D4u
#define OFF_EVENT_CB 0x1FFE4u
#define OFF_LAZY_INIT 0x20444u
#define OFF_API_BUILDER 0x1B400u
#define OFF_SG_FN_STORE 0x1B4FAu
#define OFF_SG_ENTRY 0x1AE74u
#define OFF_OPCODE300 0x1AEB8u

/* Research asserts for current SHA (Thumb); never product hardcode. */
#define RESEARCH_DISPATCHER_THUMB 0x30CFCDu
#define RESEARCH_CB_THUMB 0x30B7C5u
#define RESEARCH_SG_THUMB 0x306655u

typedef struct {
    char tag[32];
    uint32_t pc;
    uint32_t r0, r1, r2, r3, r9, sp, lr;
    uint32_t module_object;
} P20Hit;

#define P20_HIT_MAX 48
#define P20_API_MAX 64

typedef struct {
    char name[48];
    uint32_t fn;
    uint32_t off;
    uint32_t r9;
    uint32_t table;
} P20ApiRow;

static struct {
    int enabled_known;
    int enabled;
    int finalized;
    int hooks_armed;

    uint32_t map_base;    /* raw CODE_IMAGE / guest_code_base */
    uint32_t image_base;  /* pad-refined PIC base (entries.image_base) */
    uint32_t gbrw_size;
    int base_known;

    uint32_t helper;
    uint32_t module_object; /* P */
    uint32_t live_r9;

    int hit_helper;
    int hit_dispatcher;
    int hit_cmd0;
    int hit_cb_reg_call;
    int hit_reg_fn;
    int hit_plat_wrap;
    int hit_callback;
    int hit_lazy;
    int hit_builder;
    int hit_sg_store;
    int hit_sg_entry;
    int hit_opcode300;
    int nested_jjfb;

    int reg10102_ok;
    uint32_t reg_family;
    uint32_t reg_callback;
    char reg_owner[48];

    uint32_t table_obj;
    uint32_t sg_fn_ptr;
    char sg_name[48];

    uint32_t first_event_code;
    int first_event_seen;
    int skipped_7d7e;

    P20Hit hits[P20_HIT_MAX];
    int hit_n;
    P20ApiRow apis[P20_API_MAX];
    int api_n;

    void *uc;
} g_p20;

static int env_is_1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

int p20_gbrwcore_lifecycle_enabled(void) {
    if (g_p20.enabled_known) return g_p20.enabled;
    g_p20.enabled = env_is_1("JJFB_P20_GBRWCORE_LIFECYCLE");
    if (!g_p20.enabled && original_gwy_bootstrap_enabled() &&
        env_is_1("JJFB_P19_STARTGAME_CONTRACT"))
        g_p20.enabled = 1;
    g_p20.enabled_known = 1;
    return g_p20.enabled;
}

void p20_gbrwcore_lifecycle_reset(void) { memset(&g_p20, 0, sizeof(g_p20)); }

int p20_gate_cmd0(void) { return g_p20.hit_cmd0; }
int p20_gate_reg10102(void) { return g_p20.reg10102_ok; }
int p20_gate_callback_enter(void) { return g_p20.hit_callback; }
int p20_gate_lazy_init(void) { return g_p20.hit_lazy; }
int p20_gate_api_builder(void) { return g_p20.hit_builder; }
int p20_gate_sg_ptr(void) { return g_p20.sg_fn_ptr != 0; }
int p20_gate_startgame(void) { return g_p20.hit_sg_entry; }
int p20_gate_opcode300(void) { return g_p20.hit_opcode300; }
int p20_gate_nested_jjfb(void) { return g_p20.nested_jjfb; }
uint32_t p20_image_base(void) { return g_p20.image_base; }
uint32_t p20_live_r9(void) { return g_p20.live_r9; }
uint32_t p20_sg_fn_ptr(void) { return g_p20.sg_fn_ptr; }

static uint32_t va(uint32_t off) {
    return (g_p20.image_base ? g_p20.image_base : g_p20.map_base) + off;
}

static void record_hit(const char *tag, uint32_t pc, const uint32_t regs[16]) {
    P20Hit *h;
    if (g_p20.hit_n >= P20_HIT_MAX || !tag) return;
    h = &g_p20.hits[g_p20.hit_n++];
    memset(h, 0, sizeof(*h));
    snprintf(h->tag, sizeof(h->tag), "%s", tag);
    h->pc = pc;
    if (regs) {
        h->r0 = regs[0];
        h->r1 = regs[1];
        h->r2 = regs[2];
        h->r3 = regs[3];
        h->r9 = regs[9];
        h->sp = regs[13];
        h->lr = regs[14];
        h->module_object = regs[0];
        g_p20.live_r9 = regs[9];
    }
    printf("[P20_HIT] tag=%s pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X sp=0x%X lr=0x%X "
           "evidence=OBSERVED\n",
           tag, pc, h->r0, h->r1, h->r2, h->r3, h->r9, h->sp, h->lr);
    fflush(stdout);
}

static void refine_image_base(uint32_t candidate) {
    if (!candidate) return;
    candidate &= ~1u;
    if (g_p20.image_base == candidate) return;
    /* Prefer helper-derived or registry image_base over raw map pad. */
    if (!g_p20.image_base || candidate != g_p20.map_base) {
        uint32_t old = g_p20.image_base;
        g_p20.image_base = candidate;
        printf("[P20_IMAGE_BASE] image_base=0x%X map_base=0x%X pad=0x%X old=0x%X "
               "evidence=OBSERVED\n",
               g_p20.image_base, g_p20.map_base,
               g_p20.map_base ? (g_p20.image_base - g_p20.map_base) : 0u, old);
        fflush(stdout);
        g_p20.hooks_armed = 0; /* re-arm at refined VAs */
        if (g_p20.uc) p20_gbrwcore_lifecycle_bind_uc(g_p20.uc);
    }
}

void p20_gbrwcore_lifecycle_on_module_map(const char *module_name, uint32_t base, uint32_t size) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    if (!p20_gbrwcore_lifecycle_enabled() || !module_name || !base) return;
    if (!strstr(module_name, "gbrwcore")) return;
    g_p20.map_base = base & ~1u;
    g_p20.gbrw_size = size ? size : 147196u;
    g_p20.base_known = 1;
    /* Prefer registry image_base when pad refine already ran. */
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find(reg, "gbrwcore.ext") : NULL;
    if (!m && reg) {
        size_t i;
        for (i = 0; i < reg->count; i++) {
            const char *n = reg->modules[i].resolved_name[0] ? reg->modules[i].resolved_name
                                                            : reg->modules[i].requested_name;
            if (n && strstr(n, "gbrwcore")) {
                m = &reg->modules[i];
                break;
            }
        }
    }
    if (m && m->entries.image_base)
        refine_image_base(m->entries.image_base);
    else if (!g_p20.image_base)
        g_p20.image_base = g_p20.map_base;
    printf("[P20_GBRWCORE_MAP] map_base=0x%X image_base=0x%X size=0x%X dispatcher=0x%X "
           "cmd0=0x%X callback=0x%X builder=0x%X evidence=OBSERVED\n",
           g_p20.map_base, g_p20.image_base, g_p20.gbrw_size, va(OFF_DISPATCHER),
           va(OFF_CMD0_BRANCH), va(OFF_EVENT_CB), va(OFF_API_BUILDER));
    fflush(stdout);
    if (g_p20.uc) p20_gbrwcore_lifecycle_bind_uc(g_p20.uc);
}

void p20_gbrwcore_lifecycle_on_helper_register(uint32_t helper, uint32_t p_guest) {
    uint32_t hn;
    if (!p20_gbrwcore_lifecycle_enabled() || !helper) return;
    g_p20.helper = helper;
    if (p_guest) g_p20.module_object = p_guest;
    hn = helper & ~1u;
    /* Derive image_base from live dispatcher pointer (file +0x217EC). */
    if (hn > OFF_DISPATCHER) refine_image_base(hn - OFF_DISPATCHER);
    printf("[P20_HELPER_REGISTER] helper=0x%X P=0x%X image_base=0x%X "
           "assert_near_dispatcher=%s evidence=OBSERVED\n",
           helper, p_guest, g_p20.image_base,
           ((hn == (RESEARCH_DISPATCHER_THUMB & ~1u)) ||
            (g_p20.image_base && hn == (g_p20.image_base + OFF_DISPATCHER)))
               ? "yes"
               : "differs_use_live");
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static void p20_bp_cb(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    uint32_t regs[16];
    static const int k_regs[16] = {
        UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3, UC_ARM_REG_R4,
        UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,  UC_ARM_REG_R8, UC_ARM_REG_R9,
        UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR,
        UC_ARM_REG_PC};
    int i;
    (void)size;
    (void)user;
    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, k_regs[i], &regs[i]);
    p20_gbrwcore_lifecycle_on_code(uc, 0, "gbrwcore.ext", (uint32_t)address, regs);
}

static void arm_bp(void *uc, uint32_t addr, const char *tag) {
    uc_hook h = 0;
    uint64_t a = (uint64_t)(addr & ~1u);
    if (!uc || !addr) return;
    if (uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)p20_bp_cb, NULL, a, a + 2ull) ==
        UC_ERR_OK) {
        printf("[P20_BP_ARM] tag=%s va=0x%X evidence=DOCUMENTED\n", tag, addr);
        fflush(stdout);
    }
}
#endif

void p20_gbrwcore_lifecycle_bind_uc(void *uc) {
    if (!p20_gbrwcore_lifecycle_enabled() || !uc) return;
    g_p20.uc = uc;
#ifdef GWY_HAVE_UNICORN
    if (!g_p20.base_known || !g_p20.image_base || g_p20.hooks_armed) return;
    g_p20.hooks_armed = 1;
    arm_bp(uc, va(OFF_MRPGCMAP_HELPER), "mrpgcmap_helper");
    arm_bp(uc, va(OFF_DISPATCHER), "module_dispatcher");
    arm_bp(uc, va(OFF_CMD0_BRANCH), "cmd0_branch");
    arm_bp(uc, va(OFF_CB_REG_CALL), "cb_reg_call");
    arm_bp(uc, va(OFF_REG_FN), "reg_fn");
    arm_bp(uc, va(OFF_PLAT_REG_WRAP), "plat_reg_wrap");
    arm_bp(uc, va(OFF_EVENT_CB), "event_callback");
    arm_bp(uc, va(OFF_LAZY_INIT), "lazy_init");
    arm_bp(uc, va(OFF_API_BUILDER), "api_builder");
    arm_bp(uc, va(OFF_SG_FN_STORE), "sg_fn_store");
    arm_bp(uc, va(OFF_SG_ENTRY), "sg_entry");
    arm_bp(uc, va(OFF_OPCODE300), "opcode300");
#endif
}

static int read_cstr(void *uc, uint32_t addr, char *out, size_t cap) {
    size_t i;
    if (!uc || !addr || !out || cap < 2) return 0;
    out[0] = 0;
    for (i = 0; i + 1 < cap; i++) {
        uint8_t b = 0;
        if (!guest_memory_uc_peek((struct uc_struct *)uc, addr + (uint32_t)i, &b, 1)) break;
        out[i] = (char)b;
        if (!b) return 1;
        if (b < 0x20 || b > 0x7e) {
            out[i] = 0;
            return i > 0;
        }
    }
    out[cap - 1] = 0;
    return i > 0;
}

static void scan_table(void *uc, uint32_t table, uint32_t r9) {
    uint32_t off;
    if (!uc || !table || g_p20.api_n > 0) return;
    for (off = 0; off < 0x200u && g_p20.api_n < P20_API_MAX; off += 8u) {
        uint32_t name_p = 0, fn = 0;
        char name[48];
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, table + off, &name_p)) break;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, table + off + 4u, &fn)) break;
        if (!name_p || !fn) continue;
        if (!read_cstr(uc, name_p, name, sizeof(name))) continue;
        if (strncmp(name, "lib.", 4) != 0 && strncmp(name, "mr_", 3) != 0) continue;
        snprintf(g_p20.apis[g_p20.api_n].name, sizeof(g_p20.apis[g_p20.api_n].name), "%s", name);
        g_p20.apis[g_p20.api_n].fn = fn;
        g_p20.apis[g_p20.api_n].off = off;
        g_p20.apis[g_p20.api_n].r9 = r9;
        g_p20.apis[g_p20.api_n].table = table;
        g_p20.api_n++;
        printf("[P20_API_ROW] name=%s fn=0x%X table_off=0x%X R9=0x%X evidence=OBSERVED\n", name,
               fn, off, r9);
        fflush(stdout);
    }
}

static void try_read_sg(void *uc, uint32_t r9) {
    uint32_t table = 0, fn = 0, name_p = 0;
    char name[48];
    if (!uc || !r9) return;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x08u, &table) || !table) return;
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, table + 0x78u, &fn) || !fn) return;
    g_p20.table_obj = table;
    g_p20.sg_fn_ptr = fn;
    g_p20.live_r9 = r9;
    guest_memory_uc_peek_u32((struct uc_struct *)uc, table + 0x74u, &name_p);
    if (name_p && read_cstr(uc, name_p, name, sizeof(name)))
        snprintf(g_p20.sg_name, sizeof(g_p20.sg_name), "%s", name);
    printf("[P20_SG_PTR] R9=0x%X table=0x%X name=%s fn=0x%X assert=%s evidence=OBSERVED\n", r9,
           table, g_p20.sg_name[0] ? g_p20.sg_name : "?", fn,
           (fn == RESEARCH_SG_THUMB || (fn & ~1u) == (RESEARCH_SG_THUMB & ~1u)) ? "match_sha"
                                                                                : "live_differs");
    fflush(stdout);
    scan_table(uc, table, r9);
#ifdef GWY_HAVE_UNICORN
    /* Live table entry may differ from static OFF_SG_ENTRY — arm that site for Gate 7. */
    if (g_p20.uc && fn && (fn & ~1u) != va(OFF_SG_ENTRY))
        arm_bp(g_p20.uc, fn & ~1u, "sg_entry_live");
#endif
}

void p20_gbrwcore_lifecycle_on_plat_10102(uint32_t family, uint32_t callback, uint32_t r9,
                                          const char *owner) {
    if (!p20_gbrwcore_lifecycle_enabled()) return;
    g_p20.reg_family = family;
    g_p20.reg_callback = callback;
    if (owner) snprintf(g_p20.reg_owner, sizeof(g_p20.reg_owner), "%s", owner);
    if (r9) g_p20.live_r9 = r9;
    if (family == 0x11100u && callback) {
        g_p20.reg10102_ok = 1;
        printf("[P20_REGISTER_10102_OK] family=0x%X callback=0x%X owner=%s R9=0x%X "
               "assert_cb=%s evidence=OBSERVED\n",
               family, callback, g_p20.reg_owner[0] ? g_p20.reg_owner : "?", r9,
               ((callback & ~1u) == (RESEARCH_CB_THUMB & ~1u) ||
                (g_p20.image_base && (callback & ~1u) == va(OFF_EVENT_CB)))
                   ? "match_or_image"
                   : "live");
        fflush(stdout);
    }
}

void p20_gbrwcore_lifecycle_on_code(void *uc, uint64_t module_id, const char *module_name,
                                    uint32_t pc, const uint32_t regs[16]) {
    uint32_t pn, base, off;
    (void)module_id;
    (void)module_name;
    if (!p20_gbrwcore_lifecycle_enabled() || !g_p20.base_known) return;

    /* Late refine: registry may gain helper/image_base after first CODE_IMAGE map. */
    if (!g_p20.hooks_armed || !g_p20.helper) {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        size_t i;
        if (reg) {
            for (i = 0; i < reg->count; i++) {
                const GwyLoadedModule *m = &reg->modules[i];
                const char *n =
                    m->resolved_name[0] ? m->resolved_name : m->requested_name;
                if (!n || !strstr(n, "gbrwcore")) continue;
                if (m->map.helper_address)
                    p20_gbrwcore_lifecycle_on_helper_register(m->map.helper_address,
                                                             g_p20.module_object);
                else if (m->entries.image_base)
                    refine_image_base(m->entries.image_base);
                break;
            }
        }
    }

    pn = pc & ~1u;
    base = g_p20.image_base ? g_p20.image_base : g_p20.map_base;
    if (!base || pn < base || pn >= base + (g_p20.gbrw_size ? g_p20.gbrw_size : 0x30000u))
        return;
    off = pn - base;

    if (off == OFF_MRPGCMAP_HELPER || (off >= OFF_MRPGCMAP_HELPER && off < OFF_MRPGCMAP_HELPER + 4u)) {
        if (!g_p20.hit_helper) {
            g_p20.hit_helper = 1;
            record_hit("mrpgcmap_helper", pn, regs);
        }
    }
    if (off == OFF_DISPATCHER || (regs && regs[1] == 0u && off >= OFF_DISPATCHER &&
                                  off < OFF_DISPATCHER + 0x40u && !g_p20.hit_dispatcher)) {
        /* Enter dispatcher: R1=command. */
        if (regs && regs[1] == 0u && !g_p20.hit_dispatcher) {
            g_p20.hit_dispatcher = 1;
            record_hit("MODULE_COMMAND_0_ENTER", pn, regs);
            if (regs[0]) g_p20.module_object = regs[0];
            printf("[P20_MODULE_COMMAND_0_ENTER] dispatcher=0x%X module_object=0x%X R9=0x%X "
                   "evidence=OBSERVED\n",
                   pn, regs[0], regs[9]);
            fflush(stdout);
        }
    }
    if (off == OFF_CMD0_BRANCH && !g_p20.hit_cmd0) {
        g_p20.hit_cmd0 = 1;
        record_hit("cmd0_branch", pn, regs);
    }
    if (off == OFF_CB_REG_CALL && !g_p20.hit_cb_reg_call) {
        g_p20.hit_cb_reg_call = 1;
        record_hit("cb_reg_call", pn, regs);
    }
    if (off == OFF_REG_FN && !g_p20.hit_reg_fn) {
        g_p20.hit_reg_fn = 1;
        record_hit("reg_fn", pn, regs);
    }
    if (off == OFF_PLAT_REG_WRAP && !g_p20.hit_plat_wrap) {
        g_p20.hit_plat_wrap = 1;
        record_hit("plat_reg_wrap", pn, regs);
        if (regs)
            printf("[P20_PLAT_REG_ARGS] r0=0x%X r1=0x%X r2=0x%X r3=0x%X evidence=OBSERVED\n",
                   regs[0], regs[1], regs[2], regs[3]);
    }
    if (off == OFF_EVENT_CB && !g_p20.hit_callback) {
        g_p20.hit_callback = 1;
        record_hit("event_callback", pn, regs);
        if (regs) {
            g_p20.first_event_code = regs[0];
            g_p20.first_event_seen = 1;
            if (regs[0] == 0x7Du || regs[0] == 0x7Eu) {
                g_p20.skipped_7d7e = 1;
                g_p20.hit_callback = 0; /* wait for non-skip event */
                printf("[P20_EVENT_SKIP] code=0x%X note=7D_7E_skip_init evidence=OBSERVED\n",
                       regs[0]);
            } else {
                printf("[P20_FIRST_NATURAL_EVENT] code=0x%X r1=0x%X r2=0x%X r3=0x%X "
                       "callback=0x%X evidence=OBSERVED\n",
                       regs[0], regs[1], regs[2], regs[3], pn);
            }
            fflush(stdout);
        }
    }
    if (off == OFF_LAZY_INIT && !g_p20.hit_lazy) {
        g_p20.hit_lazy = 1;
        record_hit("lazy_init", pn, regs);
    }
    if (off == OFF_API_BUILDER && !g_p20.hit_builder) {
        g_p20.hit_builder = 1;
        record_hit("api_builder", pn, regs);
    }
    if (off == OFF_SG_FN_STORE && !g_p20.hit_sg_store) {
        g_p20.hit_sg_store = 1;
        record_hit("sg_fn_store", pn, regs);
        if (regs) try_read_sg(uc, regs[9]);
    }
    if (off == OFF_SG_ENTRY && !g_p20.hit_sg_entry) {
        g_p20.hit_sg_entry = 1;
        record_hit("sg_entry", pn, regs);
    }
    if (!g_p20.hit_sg_entry && g_p20.sg_fn_ptr && (pn & ~1u) == (g_p20.sg_fn_ptr & ~1u)) {
        g_p20.hit_sg_entry = 1;
        record_hit("sg_entry", pn, regs);
    }
    if (off == OFF_OPCODE300 && !g_p20.hit_opcode300) {
        g_p20.hit_opcode300 = 1;
        record_hit("opcode300", pn, regs);
    }

    /* Detect nested jjfb via runtime stack (not mere depth≥2 from gamelist continue). */
    if (!g_p20.nested_jjfb && mrp_runtime_stack_global()) {
        const MrpRuntimeStack *st = mrp_runtime_stack_global();
        if (st->nested_jjfb_intercepted) g_p20.nested_jjfb = 1;
    }

    /* Poll API table once R9 looks like gbrwcore ER_RW. */
    if (regs && regs[9] && regs[9] != 0x280400u && !g_p20.sg_fn_ptr)
        try_read_sg(uc, regs[9]);
}

static void write_reports(void) {
    const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
    char path[512];
    FILE *f;
    int i;

    if (root && root[0])
        snprintf(path, sizeof(path), "%s/P20_CALLBACK_REGISTRATION.csv", root);
    else
        snprintf(path, sizeof(path), "reports/P20_CALLBACK_REGISTRATION.csv");
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "platform_code,family,callback,owner,R9,ok,generation\n");
        fprintf(f, "0x10102,0x%X,0x%X,%s,0x%X,%d,%u\n", g_p20.reg_family, g_p20.reg_callback,
                g_p20.reg_owner[0] ? g_p20.reg_owner : "?", g_p20.live_r9, g_p20.reg10102_ok,
                g_p20.reg10102_ok ? 1u : 0u);
        fclose(f);
    }

    if (root && root[0])
        snprintf(path, sizeof(path), "%s/P20_API_BUILDER_TRACE.csv", root);
    else
        snprintf(path, sizeof(path), "reports/P20_API_BUILDER_TRACE.csv");
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "tag,pc,r0,r1,r2,r3,r9,sp,lr\n");
        for (i = 0; i < g_p20.hit_n; i++) {
            P20Hit *h = &g_p20.hits[i];
            fprintf(f, "%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n", h->tag, h->pc, h->r0, h->r1,
                    h->r2, h->r3, h->r9, h->sp, h->lr);
        }
        fprintf(f, "api_name,fn,table_off,r9,table\n");
        for (i = 0; i < g_p20.api_n; i++) {
            fprintf(f, "%s,0x%X,0x%X,0x%X,0x%X\n", g_p20.apis[i].name, g_p20.apis[i].fn,
                    g_p20.apis[i].off, g_p20.apis[i].r9, g_p20.apis[i].table);
        }
        fclose(f);
    }

    if (root && root[0])
        snprintf(path, sizeof(path), "%s/P20_GBRWCORE_LIFECYCLE.md", root);
    else
        snprintf(path, sizeof(path), "reports/P20_GBRWCORE_LIFECYCLE.md");
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "# P20 gbrwcore Module Lifecycle\n\n");
        fprintf(f, "## Gates\n\n");
        fprintf(f, "| Gate | Hit |\n|---|---|\n");
        fprintf(f, "| 1 command=0 | %s |\n", g_p20.hit_cmd0 ? "YES" : "NO");
        fprintf(f, "| 2 0x10102 register | %s |\n", g_p20.reg10102_ok ? "YES" : "NO");
        fprintf(f, "| 3 callback 0x30B7C4 | %s |\n", g_p20.hit_callback ? "YES" : "NO");
        fprintf(f, "| 4 lazy init | %s |\n", g_p20.hit_lazy ? "YES" : "NO");
        fprintf(f, "| 5 API builder | %s |\n", g_p20.hit_builder ? "YES" : "NO");
        fprintf(f, "| 6 startGame ptr | %s (0x%X) |\n", g_p20.sg_fn_ptr ? "YES" : "NO",
                g_p20.sg_fn_ptr);
        fprintf(f, "| 7 startGame entry | %s |\n", g_p20.hit_sg_entry ? "YES" : "NO");
        fprintf(f, "| 8 opcode 300 | %s |\n", g_p20.hit_opcode300 ? "YES" : "NO");
        fprintf(f, "| 9 nested jjfb | %s |\n\n", g_p20.nested_jjfb ? "YES" : "NO");
        fprintf(f, "## Identity\n\n");
        fprintf(f, "- map_base: `0x%X`\n", g_p20.map_base);
        fprintf(f, "- image_base: `0x%X` (pad `0x%X`)\n", g_p20.image_base,
                g_p20.map_base && g_p20.image_base >= g_p20.map_base
                    ? (g_p20.image_base - g_p20.map_base)
                    : 0u);
        fprintf(f, "- helper: `0x%X` P: `0x%X` live R9: `0x%X`\n", g_p20.helper,
                g_p20.module_object, g_p20.live_r9);
        fprintf(f, "- 10102 family=`0x%X` callback=`0x%X` owner=`%s`\n", g_p20.reg_family,
                g_p20.reg_callback, g_p20.reg_owner);
        fprintf(f, "- first event: `0x%X` skipped_7d7e=%d\n", g_p20.first_event_code,
                g_p20.skipped_7d7e);
        fprintf(f, "- startGame name=`%s` fn=`0x%X` table=`0x%X`\n",
                g_p20.sg_name[0] ? g_p20.sg_name : "?", g_p20.sg_fn_ptr, g_p20.table_obj);
        fprintf(f, "\nPolicy: no forced PC/R9, no host callback write, no forged events, "
                   "no code15/E6C.\n");
        fclose(f);
    }

    if (root && root[0])
        snprintf(path, sizeof(path), "%s/P20_NESTED_JJFB_RESULT.md", root);
    else
        snprintf(path, sizeof(path), "reports/P20_NESTED_JJFB_RESULT.md");
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "# P20 Nested JJFB Result\n\n");
        fprintf(f, "- nested_jjfb: **%s**\n", g_p20.nested_jjfb ? "YES" : "NO");
        fprintf(f, "- startGame fn: `0x%X`\n", g_p20.sg_fn_ptr);
        fprintf(f, "- opcode300: %s\n", g_p20.hit_opcode300 ? "YES" : "NO");
        fprintf(f, "- parent code15: not evaluated (gate9 incomplete or no long hold)\n");
        fprintf(f, "- first screen vs direct_boot: N/A until gate9\n");
        fclose(f);
    }
}

void p20_gbrwcore_lifecycle_finalize(const char *stop_reason) {
    if (!p20_gbrwcore_lifecycle_enabled() || g_p20.finalized) return;
    g_p20.finalized = 1;
    /* Re-check registry for 10102 if guest registered before our hook. */
    if (!g_p20.reg10102_ok) {
        uint32_t fam = platform_handler_registry_family(0x10102u);
        uint32_t h = platform_handler_registry_get(0x10102u);
        const GwyPlatformHandlerRecord *rec =
            platform_handler_registry_find_family_event(fam ? fam : 0x11100u);
        if (rec && rec->handler) {
            p20_gbrwcore_lifecycle_on_plat_10102(rec->family, rec->handler, g_p20.live_r9,
                                                 rec->owner_module);
        } else if (fam == 0x11100u && h) {
            p20_gbrwcore_lifecycle_on_plat_10102(fam, h, g_p20.live_r9, "gbrwcore.ext");
        }
    }
    if (g_p20.uc && g_p20.live_r9) try_read_sg(g_p20.uc, g_p20.live_r9);
    write_reports();
    printf("[P20_FINALIZE] stop=%s cmd0=%d reg10102=%d cb=%d lazy=%d builder=%d sg=0x%X "
           "op300=%d nested=%d evidence=OBSERVED\n",
           stop_reason ? stop_reason : "?", g_p20.hit_cmd0, g_p20.reg10102_ok, g_p20.hit_callback,
           g_p20.hit_lazy, g_p20.hit_builder, g_p20.sg_fn_ptr, g_p20.hit_opcode300,
           g_p20.nested_jjfb);
    fflush(stdout);
}
