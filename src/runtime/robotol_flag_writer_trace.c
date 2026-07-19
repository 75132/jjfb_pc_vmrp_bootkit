#include "gwy_launcher/robotol_flag_writer_trace.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/platform_handler_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define E8F_MAX_BP 48

typedef struct E8fBp {
    int used;
    uint32_t pc; /* even */
    char tag[12];
    uint32_t hit_n;
    uint32_t first_tick;
    uint32_t last_r0;
    uint32_t last_r1;
    uint32_t last_r9;
#ifdef GWY_HAVE_UNICORN
    uc_hook hook;
#endif
} E8fBp;

static struct {
    int known;
    int enabled;
    int writer_bp;
    int sibling_probe;
    int longpath_watch;
    int armed;
    int sibling_done;
    int counterfactual_done;
    int probe10162_done;
    int probe10102_done;
    void *uc;
    uint32_t tick;
    char sibling_mode[16]; /* 10162 / 10102 / BOTH */
    char counterfactual[16]; /* C44 / C9D / CF5 / ALL / empty */
    E8fBp bps[E8F_MAX_BP];
    int nbps;
    uint32_t longpath_hits;
    uint32_t queue_depth_at_probe;
} g_e8f;

static int env1(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

static int parse_hex_list(const char *s, uint32_t *out, int maxn) {
    int n = 0;
    const char *p = s;
    if (!s || !s[0]) return 0;
    while (*p && n < maxn) {
        char *end = NULL;
        unsigned long v;
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;
        v = strtoul(p, &end, 0);
        if (end == p) break;
        out[n++] = (uint32_t)v;
        p = end;
    }
    return n;
}

int robotol_flag_writer_trace_enabled(void) {
    const char *sib;
    const char *cf;
    if (!g_e8f.known) {
        g_e8f.writer_bp = env1("JJFB_E8F_WRITER_BP");
        g_e8f.sibling_probe = env1("JJFB_E8F_SIBLING_PROBE");
        g_e8f.longpath_watch = env1("JJFB_E8F_LONGPATH_WATCH");
        sib = getenv("JJFB_E8F_SIBLING");
        if (sib && sib[0]) {
            strncpy(g_e8f.sibling_mode, sib, sizeof(g_e8f.sibling_mode) - 1);
        } else {
            strncpy(g_e8f.sibling_mode, "BOTH", sizeof(g_e8f.sibling_mode) - 1);
        }
        cf = getenv("JJFB_E8F_COUNTERFACTUAL");
        if (cf && cf[0]) {
            strncpy(g_e8f.counterfactual, cf, sizeof(g_e8f.counterfactual) - 1);
        }
        g_e8f.enabled = g_e8f.writer_bp || g_e8f.sibling_probe || g_e8f.longpath_watch ||
                        (g_e8f.counterfactual[0] != '\0');
        g_e8f.known = 1;
    }
    return g_e8f.enabled;
}

void robotol_flag_writer_trace_reset(void) {
#ifdef GWY_HAVE_UNICORN
    int i;
    if (g_e8f.uc && g_e8f.armed) {
        for (i = 0; i < g_e8f.nbps; i++) {
            if (g_e8f.bps[i].used && g_e8f.bps[i].hook) {
                uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.bps[i].hook);
                g_e8f.bps[i].hook = 0;
            }
        }
    }
#endif
    memset(&g_e8f, 0, sizeof(g_e8f));
}

void robotol_flag_writer_trace_bind_uc(void *uc) { g_e8f.uc = uc; }

void robotol_flag_writer_trace_set_tick(uint32_t tick) { g_e8f.tick = tick; }

static int find_robotol_code(uint32_t *base_out, uint32_t *size_out) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m;
    if (!reg) return 0;
    m = module_registry_find(reg, "robotol.ext");
    if (!m || !m->map.guest_code_base || !m->map.guest_code_size) return 0;
    *base_out = m->map.guest_code_base;
    *size_out = m->map.guest_code_size;
    return 1;
}

static int find_robotol_r9(void *uc, uint32_t *r9_out) {
    uint32_t r9 = 0, er = 0, ersz = 0;
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m;
    if (!uc) return 0;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9);
    if (reg) {
        m = module_registry_find(reg, "robotol.ext");
        if (m && m->data.start_of_er_rw) {
            er = m->data.start_of_er_rw;
            ersz = m->data.er_rw_size ? m->data.er_rw_size : 0x2000u;
            if (r9 >= er && r9 < er + ersz) {
                *r9_out = r9;
                return 1;
            }
            *r9_out = er;
            return 1;
        }
    }
    if (r9) {
        *r9_out = r9;
        return 1;
    }
    return 0;
}

#ifdef GWY_HAVE_UNICORN
static void on_e8f_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    E8fBp *bp = (E8fBp *)user_data;
    uint32_t r0 = 0, r1 = 0, r9 = 0, lr = 0;
    (void)address;
    (void)size;
    if (!bp || !bp->used) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    bp->hit_n++;
    if (bp->hit_n == 1) bp->first_tick = g_e8f.tick;
    bp->last_r0 = r0;
    bp->last_r1 = r1;
    bp->last_r9 = r9;
    if (bp->pc == 0x30D28Cu) {
        g_e8f.longpath_hits++;
        printf("[JJFB_E8F_LONGPATH_HIT] pc=0x%X tick=%u r0=0x%X r1=0x%X r9=0x%X lr=0x%X "
               "evidence=OBSERVED\n",
               bp->pc, g_e8f.tick, r0, r1, r9, lr);
    } else {
        printf("[JJFB_E8F_WRITER_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
    }
    fflush(stdout);
}
#endif

static void add_bp(uint32_t pc, const char *tag) {
    E8fBp *bp;
    uint32_t even = pc & ~1u;
    int i;
    if (g_e8f.nbps >= E8F_MAX_BP) return;
    for (i = 0; i < g_e8f.nbps; i++) {
        if (g_e8f.bps[i].pc == even) return;
    }
    bp = &g_e8f.bps[g_e8f.nbps++];
    memset(bp, 0, sizeof(*bp));
    bp->used = 1;
    bp->pc = even;
    strncpy(bp->tag, tag ? tag : "?", sizeof(bp->tag) - 1);
}

void robotol_flag_writer_trace_try_arm(void *uc) {
    uint32_t code_base = 0, code_size = 0;
    uint32_t pcs[E8F_MAX_BP];
    int np = 0, i;
    const char *csv;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif

    if (!robotol_flag_writer_trace_enabled()) return;
    if (!uc) uc = g_e8f.uc;
    if (!uc || g_e8f.armed) return;
    if (!g_e8f.writer_bp && !g_e8f.longpath_watch) return;
    if (!find_robotol_code(&code_base, &code_size)) return;

    /* Built-in top writers (E8D/E8F list). */
    static const uint32_t k_default[] = {
        0x2E87F2u, 0x2F4E82u, 0x2F7F56u, 0x2FB286u, 0x2FC8CAu, 0x2FEDFAu, 0x2FEE4Eu,
        0x30CC72u, 0x311C3Eu, 0x2E3A68u, 0x2F097Au, 0x2F7772u, 0x2FB008u, 0x307796u,
        0x30AA42u, 0x3115B4u, 0x2D9B68u, 0x2D9CE6u, 0x2DBA82u, 0x2E7DBCu, 0x2F7F2Cu,
    };
    csv = getenv("JJFB_E8F_WRITER_PCS");
    if (csv && csv[0])
        np = parse_hex_list(csv, pcs, E8F_MAX_BP);
    else {
        for (i = 0; i < (int)(sizeof(k_default) / sizeof(k_default[0])); i++)
            pcs[np++] = k_default[i];
    }
    g_e8f.nbps = 0;
    if (g_e8f.writer_bp) {
        for (i = 0; i < np; i++)
            add_bp(pcs[i], "writer");
    }
    if (g_e8f.longpath_watch) {
        add_bp(0x30D28Cu, "longpath");
        add_bp(0x30D262u, "fe8_str");
    }

#ifdef GWY_HAVE_UNICORN
    for (i = 0; i < g_e8f.nbps; i++) {
        E8fBp *bp = &g_e8f.bps[i];
        if (bp->pc < code_base || bp->pc >= code_base + code_size) {
            printf("[JJFB_E8F_WRITER_BP] skip=out_of_robotol pc=0x%X evidence=OBSERVED\n", bp->pc);
            continue;
        }
        ue = uc_hook_add((uc_engine *)uc, &bp->hook, UC_HOOK_CODE, (void *)on_e8f_code, bp,
                         (uint64_t)bp->pc, (uint64_t)bp->pc);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8F_WRITER_BP] arm_fail pc=0x%X uc_err=%u evidence=OBSERVED\n", bp->pc,
                   (unsigned)ue);
        }
    }
#endif
    g_e8f.armed = 1;
    g_e8f.uc = uc;
    printf("[JJFB_E8F_WRITER_BP] armed=1 n=%d code_base=0x%X size=0x%X evidence=OBSERVED\n",
           g_e8f.nbps, code_base, code_size);
    fflush(stdout);
}

static void snap_idle_flags(void *uc, uint32_t r9, const char *reason) {
    uint8_t c44 = 0, c9d = 0, cf5 = 0, b7d = 0;
    uint32_t fe8 = 0, queue = 0;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC44u, &c44, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC9Du, &c9d, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCF5u, &cf5, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xB7Du, &b7d, 1);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xFE8u, &fe8);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x844u, &queue);
    printf("[JJFB_E8F_FLAG_SNAP] reason=%s r9=0x%X C44=0x%X C9D=0x%X CF5=0x%X B7D=0x%X "
           "FE8=0x%X queue_depth=0x%X evidence=OBSERVED\n",
           reason ? reason : "-", r9, c44, c9d, cf5, b7d, fe8, queue);
    fflush(stdout);
}

static void fire_handler(void *uc, uint32_t code, const char *label) {
    uint32_t handler, stop = 0x80000u;
    uint32_t r9_save = 0, r9_run = 0;
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    int ok;
    handler = platform_handler_registry_get(code);
    if (!handler) {
        printf("[JJFB_E8F_SIBLING_FIRE] code=0x%X label=%s skip=no_handler evidence=OBSERVED\n",
               code, label);
        return;
    }
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    if (!find_robotol_r9(uc, &r9_run)) r9_run = r9_save;
    if (r9_run) (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_run);
    snap_idle_flags(uc, r9_run, "before_sibling");
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = 0;
    abi.set_r1 = 1;
    abi.r1 = 0;
    abi.set_lr = 1;
    abi.lr = stop;
    printf("[JJFB_E8F_SIBLING_FIRE] code=0x%X label=%s handler=0x%X r9=0x%X "
           "note=observe_only_ZERO_ARGS evidence=HYPOTHESIS\n",
           code, label, handler, r9_run);
    fflush(stdout);
    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, handler, stop, 200000ull, &abi, &out);
    snap_idle_flags(uc, r9_run, "after_sibling");
    printf("[JJFB_E8F_SIBLING_FIRE_DONE] code=0x%X ok=%d end=%s pc_after=0x%X r0_after=0x%X "
           "evidence=OBSERVED\n",
           code, ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after, out.r0_after);
    fflush(stdout);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
}

static void run_counterfactual(void *uc) {
    uint32_t r9 = 0;
    uint8_t one = 1;
    const char *mode = g_e8f.counterfactual;
    if (!mode[0] || g_e8f.counterfactual_done) return;
    if (!find_robotol_r9(uc, &r9) || !r9) return;
    g_e8f.counterfactual_done = 1;
    printf("[JJFB_E8F_COUNTERFACTUAL] mode=%s r9=0x%X note=COUNTERFACTUAL_ONLY_not_product "
           "evidence=HYPOTHESIS\n",
           mode, r9);
    fflush(stdout);
    snap_idle_flags(uc, r9, "cf_before");
    if (strcmp(mode, "C44") == 0 || strcmp(mode, "ALL") == 0)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, r9 + 0xC44u, &one, 1);
    if (strcmp(mode, "C9D") == 0 || strcmp(mode, "ALL") == 0)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, r9 + 0xC9Du, &one, 1);
    if (strcmp(mode, "CF5") == 0 || strcmp(mode, "ALL") == 0)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, r9 + 0xCF5u, &one, 1);
    snap_idle_flags(uc, r9, "cf_after_poke");
    printf("[JJFB_E8F_COUNTERFACTUAL_ARMED] mode=%s note=watch_next_10140_for_new_plat_or_DRAW "
           "evidence=HYPOTHESIS\n",
           mode);
    fflush(stdout);
}

void robotol_flag_writer_trace_dump_summary(const char *reason) {
    int i, hits = 0, miss = 0;
    if (!robotol_flag_writer_trace_enabled()) return;
    for (i = 0; i < g_e8f.nbps; i++) {
        E8fBp *bp = &g_e8f.bps[i];
        if (!bp->used) continue;
        if (bp->hit_n) {
            hits++;
            printf("[JJFB_E8F_WRITER_REACHED] tag=%s pc=0x%X hits=%u first_tick=%u "
                   "class=ENTERED evidence=OBSERVED\n",
                   bp->tag, bp->pc, bp->hit_n, bp->first_tick);
        } else {
            miss++;
            printf("[JJFB_E8F_WRITER_NEVER] tag=%s pc=0x%X class=NEVER_ENTERED "
                   "evidence=OBSERVED\n",
                   bp->tag, bp->pc);
        }
    }
    printf("[JJFB_E8F_WRITER_SUMMARY] reason=%s armed=%d hit_pcs=%d never_pcs=%d "
           "longpath_hits=%u evidence=OBSERVED\n",
           reason ? reason : "-", g_e8f.armed, hits, miss, g_e8f.longpath_hits);
    fflush(stdout);
}

void robotol_flag_writer_trace_on_lifecycle(void *uc, uint32_t tick) {
    uint32_t r9 = 0;
    if (!robotol_flag_writer_trace_enabled()) return;
    if (!uc) uc = g_e8f.uc;
    g_e8f.tick = tick;
    robotol_flag_writer_trace_try_arm(uc);

    if (tick == 1u && find_robotol_r9(uc, &r9)) {
        uint32_t depth = 0;
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x844u, &depth);
        g_e8f.queue_depth_at_probe = depth;
        printf("[JJFB_E8F_QUEUE_DEPTH] r9=0x%X off=0x844 depth=0x%X "
               "note=long_path_if_depth_le_0 evidence=TARGET_OBSERVED\n",
               r9, depth);
        snap_idle_flags(uc, r9, "tick1");
        fflush(stdout);
    }

    /* Sibling probes after first tick returns (depth==0). */
    if (tick == 1u && g_e8f.sibling_probe && !g_e8f.sibling_done) {
        g_e8f.sibling_done = 1;
        if (strcmp(g_e8f.sibling_mode, "10162") == 0 || strcmp(g_e8f.sibling_mode, "BOTH") == 0) {
            if (!g_e8f.probe10162_done) {
                g_e8f.probe10162_done = 1;
                fire_handler(uc, 0x10162u, "10162");
            }
        }
        if (strcmp(g_e8f.sibling_mode, "10102") == 0 || strcmp(g_e8f.sibling_mode, "BOTH") == 0) {
            if (!g_e8f.probe10102_done) {
                g_e8f.probe10102_done = 1;
                fire_handler(uc, 0x10102u, "10102");
            }
        }
    }

    /* Counterfactual after tick1 sibling/idle snap — before subsequent ticks. */
    if (tick == 1u && g_e8f.counterfactual[0])
        run_counterfactual(uc);

    if (tick == 2u && g_e8f.counterfactual_done && find_robotol_r9(uc, &r9))
        snap_idle_flags(uc, r9, "cf_after_next_10140");

    if (tick == 25u || tick == 40u)
        robotol_flag_writer_trace_dump_summary(tick == 40u ? "tick_40" : "tick_25");
}
