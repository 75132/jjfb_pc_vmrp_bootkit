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

#define E8F_MAX_BP 256
#define E8I_STATE_OFF (0x800u + 0xD0u) /* audit-safe: former state/ui word */

typedef struct E8fBp {
    int used;
    uint32_t pc; /* even */
    char tag[16];
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
    int caller_bp;
    int fault_watch;
    int dispatcher_bp;
    int parent_bp;
    int state_watch;
    int e8j_bp;
    int e8j_queue_read_watch;
    int svc_trap;
    int svc_stop;
    int armed;
    int state_watch_armed;
    int e8j_qread_armed;
    int sibling_done;
    int counterfactual_done;
    int probe10162_done;
    int probe10102_done;
    int fault_dumped;
    int svc_trapped;
    void *uc;
    uint32_t tick;
    char sibling_mode[16];
    char counterfactual[16];
    E8fBp bps[E8F_MAX_BP];
    int nbps;
    uint32_t longpath_hits;
    uint32_t queue_depth_at_probe;
    uint32_t fault_hits;
    uint32_t svc_hits;
    uint32_t state_writes;
    uint32_t state_last_val;
    uint32_t state_last_writer;
    uint32_t parent_hit_n;
    uint32_t e8j_entry_hits;
    uint32_t e8j_upstream_hits;
    uint32_t e8j_queue_hits;
    uint32_t e8j_bl_hits;
    uint32_t e8j_qread_hits;
#ifdef GWY_HAVE_UNICORN
    uc_hook svc_code_hook;
    uc_hook svc_intr_hook;
    uc_hook state_hook;
    uc_hook e8j_qread_hook;
#endif
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

/* E8J: role-tagged CSV like "e:0x2DFC3C,u:0x30D300,q:0x2DC80C,p:0x300158,b:0x2DFDD6" */
static int parse_role_spec(const char *s, uint32_t *pcs, char roles[][4], int maxn) {
    int n = 0;
    const char *p = s;
    if (!s || !s[0]) return 0;
    while (*p && n < maxn) {
        char *end = NULL;
        unsigned long v;
        char role = 'p';
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z')) && p[1] == ':') {
            role = (char)((p[0] >= 'A' && p[0] <= 'Z') ? (p[0] - 'A' + 'a') : p[0]);
            p += 2;
        }
        v = strtoul(p, &end, 0);
        if (end == p) break;
        pcs[n] = (uint32_t)v;
        roles[n][0] = role;
        roles[n][1] = '\0';
        n++;
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
        g_e8f.caller_bp = env1("JJFB_E8G_CALLER_BP");
        g_e8f.fault_watch = env1("JJFB_E8G_FAULT_WATCH");
        g_e8f.dispatcher_bp = env1("JJFB_E8H_DISPATCHER_BP");
        g_e8f.parent_bp = env1("JJFB_E8I_PARENT_BP");
        g_e8f.state_watch = env1("JJFB_E8I_STATE_WATCH");
        g_e8f.e8j_bp = env1("JJFB_E8J_CLUSTER_BP");
        g_e8f.e8j_queue_read_watch = env1("JJFB_E8J_QUEUE_READ_WATCH");
        g_e8f.svc_trap = env1("JJFB_E8H_SVC_TRAP");
        /* Default stop-on-hit when trap armed unless explicitly disabled. */
        g_e8f.svc_stop = g_e8f.svc_trap && !env1("JJFB_E8H_SVC_NO_STOP");
        if (env1("JJFB_E8H_SVC_STOP")) g_e8f.svc_stop = 1;
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
                        g_e8f.caller_bp || g_e8f.fault_watch || g_e8f.dispatcher_bp ||
                        g_e8f.parent_bp || g_e8f.state_watch || g_e8f.e8j_bp ||
                        g_e8f.e8j_queue_read_watch || g_e8f.svc_trap ||
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
        if (g_e8f.svc_code_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.svc_code_hook);
            g_e8f.svc_code_hook = 0;
        }
        if (g_e8f.svc_intr_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.svc_intr_hook);
            g_e8f.svc_intr_hook = 0;
        }
        if (g_e8f.state_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.state_hook);
            g_e8f.state_hook = 0;
        }
        if (g_e8f.e8j_qread_hook) {
            uc_hook_del((uc_engine *)g_e8f.uc, g_e8f.e8j_qread_hook);
            g_e8f.e8j_qread_hook = 0;
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
static void dump_hex_line(const char *tag, uint32_t addr, const uint8_t *buf, int n) {
    int i;
    printf("[%s] addr=0x%X bytes=", tag, addr);
    for (i = 0; i < n; i++)
        printf("%02X", buf[i]);
    printf(" evidence=OBSERVED\n");
}

static void dump_dispatcher_context(uc_engine *uc, const char *tag, uint32_t pc) {
    uint32_t r[13], sp = 0, lr = 0, cpsr = 0, r9 = 0;
    uint32_t c6c = 0, eec = 0, d11d0 = 0, d8d0 = 0;
    uint8_t c44 = 0, c9d = 0, cf5 = 0;
    int i;
    static const int regs[] = {
        UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,  UC_ARM_REG_R3, UC_ARM_REG_R4,
        UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,  UC_ARM_REG_R8, UC_ARM_REG_R9,
        UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12};
    for (i = 0; i < 13; i++)
        uc_reg_read(uc, regs[i], &r[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    r9 = r[9];
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xC6Cu, &c6c);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xEECu, &eec);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x11D0u, &d11d0);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + (0x800u + 0xD0u), &d8d0);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC44u, &c44, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC9Du, &c9d, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCF5u, &cf5, 1);
    printf("[JJFB_E8H_DISPATCHER_HIT] tag=%s pc=0x%X tick=%u "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X "
           "r8=0x%X r9=0x%X r10=0x%X r11=0x%X r12=0x%X sp=0x%X lr=0x%X cpsr=0x%X T=%u "
           "R9_state=0x%X R9_C6C=0x%X R9_EEC=0x%X R9_11D0=0x%X C44=0x%X C9D=0x%X CF5=0x%X "
           "cf=%s evidence=OBSERVED\n",
           tag, pc, g_e8f.tick, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9],
           r[10], r[11], r[12], sp, lr, cpsr, (cpsr >> 5) & 1u, d8d0, c6c, eec, d11d0, c44, c9d,
           cf5, g_e8f.counterfactual[0] ? g_e8f.counterfactual : "NONE");
    fflush(stdout);
}

static void dump_svc_ab_context(uc_engine *uc, const char *via, uint32_t intno) {
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, sp = 0, lr = 0, pc = 0, cpsr = 0, r9 = 0;
    uint8_t blk[0x20];
    uint8_t spb = 0;
    const char *path;
    memset(blk, 0, sizeof(blk));
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    uc_reg_read(uc, UC_ARM_REG_R3, &r3);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    if (r1)
        (void)guest_memory_uc_peek((struct uc_struct *)uc, r1, blk, (int)sizeof(blk));
    (void)guest_memory_uc_peek((struct uc_struct *)uc, sp, &spb, 1);
    path = g_e8f.counterfactual[0] ? "COUNTERFACTUAL" : "PRODUCT";
    g_e8f.svc_hits++;
    printf("[JJFB_E8H_SVC_AB] via=%s tick=%u path=%s cf=%s intno=0x%X "
           "pc=0x%X lr=0x%X cpsr=0x%X T=%u r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X sp=0x%X "
           "sp_byte=0x%02X note=observe_only_no_fake_return evidence=OBSERVED\n",
           via, g_e8f.tick, path, g_e8f.counterfactual[0] ? g_e8f.counterfactual : "NONE", intno,
           pc, lr, cpsr, (cpsr >> 5) & 1u, r0, r1, r2, r3, r9, sp, spb);
    dump_hex_line("JJFB_E8H_SVC_AB_ARG", r1, blk, (int)sizeof(blk));
    printf("[JJFB_E8H_SVC_AB_CLASS] path=%s class=%s "
           "hypothesis=r0_service_sel_r1_argblock_sp_byte_original_request "
           "evidence=TARGET_OBSERVED+HYPOTHESIS\n",
           path,
           g_e8f.counterfactual[0] ? "SVC_AB_POST_GATE_CANDIDATE" : "SVC_AB_PRODUCT_CANDIDATE");
    fflush(stdout);
}

static void on_e8h_svc_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)address;
    (void)size;
    (void)user_data;
    if (g_e8f.svc_trapped && g_e8f.svc_stop) return;
    dump_svc_ab_context(uc, "CODE_0x2D92AE", 0xABu);
    g_e8f.svc_trapped = 1;
    if (g_e8f.svc_stop) {
        printf("[JJFB_E8H_SVC_AB_STOP] note=observe_only_emu_stop evidence=OBSERVED\n");
        fflush(stdout);
        uc_emu_stop(uc);
    }
}

static void on_e8h_intr(uc_engine *uc, uint32_t intno, void *user_data) {
    uint32_t pc = 0;
    (void)user_data;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    /* ARM SVC immediate often delivered as intno; also accept any INTR at stub PC. */
    if (intno != 0xABu && (pc & ~1u) != 0x2D92AEu && (pc & ~1u) != 0x2D92B0u)
        return;
    if (g_e8f.svc_trapped && g_e8f.svc_stop) return;
    dump_svc_ab_context(uc, "HOOK_INTR", intno);
    g_e8f.svc_trapped = 1;
    if (g_e8f.svc_stop) {
        printf("[JJFB_E8H_SVC_AB_STOP] note=observe_only_emu_stop evidence=OBSERVED\n");
        fflush(stdout);
        uc_emu_stop(uc);
    }
}

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
    if (bp->tag[0] == 'd') {
        dump_dispatcher_context(uc, bp->tag, bp->pc);
        return;
    }
    if (bp->tag[0] == 'p') {
        g_e8f.parent_hit_n++;
        printf("[JJFB_E8I_PARENT_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
        if (bp->pc == 0x300158u || bp->pc == 0x300714u || bp->pc == 0x30103Cu)
            dump_dispatcher_context(uc, bp->tag, bp->pc);
        fflush(stdout);
        return;
    }
    if (bp->tag[0] == 'e') {
        g_e8f.e8j_entry_hits++;
        printf("[JJFB_E8J_CLUSTER_HIT] role=entry tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X "
               "r9=0x%X lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
        fflush(stdout);
        return;
    }
    if (bp->tag[0] == 'u') {
        g_e8f.e8j_upstream_hits++;
        printf("[JJFB_E8J_UPSTREAM_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
        fflush(stdout);
        return;
    }
    if (bp->tag[0] == 'q') {
        g_e8f.e8j_queue_hits++;
        printf("[JJFB_E8J_QUEUE_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
        fflush(stdout);
        return;
    }
    if (bp->tag[0] == 'b') {
        g_e8f.e8j_bl_hits++;
        g_e8f.parent_hit_n++;
        printf("[JJFB_E8J_BL_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X note=direct_bl_to_300158 evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
        fflush(stdout);
        return;
    }
    if (bp->pc == 0x30D28Cu) {
        g_e8f.longpath_hits++;
        printf("[JJFB_E8F_LONGPATH_HIT] pc=0x%X tick=%u r0=0x%X r1=0x%X r9=0x%X lr=0x%X "
               "evidence=OBSERVED\n",
               bp->pc, g_e8f.tick, r0, r1, r9, lr);
    } else if (bp->pc == 0x2D92B0u) {
        g_e8f.fault_hits++;
        printf("[JJFB_E8G_FAULT_HIT] pc=0x2D92B0 tick=%u r0=0x%X r1=0x%X r9=0x%X lr=0x%X "
               "note=counterfactual_second_gate_site evidence=OBSERVED\n",
               g_e8f.tick, r0, r1, r9, lr);
    } else if (bp->tag[0] == 'c') {
        printf("[JJFB_E8G_CALLER_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
    } else {
        printf("[JJFB_E8F_WRITER_HIT] tag=%s pc=0x%X hit=%u tick=%u r0=0x%X r1=0x%X r9=0x%X "
               "lr=0x%X evidence=OBSERVED\n",
               bp->tag, bp->pc, bp->hit_n, g_e8f.tick, r0, r1, r9, lr);
    }
    fflush(stdout);
}
static void on_e8i_state_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                               int64_t value, void *user_data) {
    uint32_t pc = 0, lr = 0, r0 = 0, r9 = 0;
    uint32_t newv = (uint32_t)value;
    (void)type;
    (void)user_data;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    if (size == 1) newv = (uint32_t)(uint8_t)value;
    else if (size == 2) newv = (uint32_t)(uint16_t)value;
    g_e8f.state_writes++;
    g_e8f.state_last_val = newv;
    g_e8f.state_last_writer = pc;
    printf("[JJFB_E8I_STATE_WRITE] addr=0x%X off=0x%X size=%d new=0x%X writer_pc=0x%X lr=0x%X "
           "r0=0x%X r9=0x%X tick=%u note=observe_only evidence=OBSERVED\n",
           (uint32_t)address, E8I_STATE_OFF, size, newv, pc, lr, r0, r9, g_e8f.tick);
    if (newv == 38u) {
        printf("[JJFB_E8I_STATE_EQ38] writer_pc=0x%X tick=%u note=required_for_30103C_arm "
               "evidence=OBSERVED\n",
               pc, g_e8f.tick);
    }
    fflush(stdout);
}
static void on_e8j_queue_read(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                              int64_t value, void *user_data) {
    uint32_t pc = 0, lr = 0, r0 = 0, r9 = 0;
    uint32_t off;
    (void)type;
    (void)value;
    (void)user_data;
    if (!find_robotol_r9(uc, &r9) || !r9) return;
    if ((uint32_t)address < r9) return;
    off = (uint32_t)address - r9;
    /* Only report FE8 / B7D / 7D8 base (not every 7D8 field offset). */
    if (off != 0xFE8u && off != 0xB7Du && off != 0x7D8u) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    g_e8f.e8j_qread_hits++;
    if (g_e8f.e8j_qread_hits <= 40u) {
        printf("[JJFB_E8J_QUEUE_READ] addr=0x%X off=0x%X size=%d reader_pc=0x%X lr=0x%X "
               "r0=0x%X r9=0x%X tick=%u evidence=OBSERVED\n",
               (uint32_t)address, off, size, pc, lr, r0, r9, g_e8f.tick);
        fflush(stdout);
    }
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
    if (!g_e8f.writer_bp && !g_e8f.longpath_watch && !g_e8f.caller_bp && !g_e8f.fault_watch &&
        !g_e8f.dispatcher_bp && !g_e8f.parent_bp && !g_e8f.state_watch && !g_e8f.e8j_bp &&
        !g_e8f.e8j_queue_read_watch && !g_e8f.svc_trap)
        return;
    if (!find_robotol_code(&code_base, &code_size)) return;

    /* Built-in top writers (E8D/E8F list). */
    static const uint32_t k_default[] = {
        0x2E87F2u, 0x2F4E82u, 0x2F7F56u, 0x2FB286u, 0x2FC8CAu, 0x2FEDFAu, 0x2FEE4Eu,
        0x30CC72u, 0x311C3Eu, 0x2E3A68u, 0x2F097Au, 0x2F7772u, 0x2FB008u, 0x307796u,
        0x30AA42u, 0x3115B4u, 0x2D9B68u, 0x2D9CE6u, 0x2DBA82u, 0x2E7DBCu, 0x2F7F2Cu,
    };
    /* E8G priority bootstrap callers + fn entries. */
    static const uint32_t k_callers[] = {
        0x302340u, 0x302362u, 0x2FC048u, 0x2E32A2u, 0x2EFF1Cu, 0x2F08A4u, 0x2F0D6Au,
        0x2F1FF0u, 0x30D9EEu, 0x30AF8Au, 0x30DF78u, 0x2DA64Au, 0x2DE7ECu, 0x30FCEEu,
    };
    /* E8H dispatcher chain (CSV may override). */
    static const uint32_t k_disp[] = {
        0x300714u, 0x3002C0u, 0x30103Cu, 0x3020C8u, 0x302340u, 0x302362u, 0x2F4E82u,
    };
    static const char *k_disp_tags[] = {
        "d300714", "d3002c0", "d30103c", "d3020c8", "d302340", "d302362", "d2f4e82",
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
    if (g_e8f.caller_bp) {
        uint32_t cpcs[E8F_MAX_BP];
        int nc = 0;
        const char *ccsv = getenv("JJFB_E8G_CALLER_PCS");
        if (ccsv && ccsv[0])
            nc = parse_hex_list(ccsv, cpcs, E8F_MAX_BP);
        else {
            for (i = 0; i < (int)(sizeof(k_callers) / sizeof(k_callers[0])); i++)
                cpcs[nc++] = k_callers[i];
        }
        for (i = 0; i < nc; i++)
            add_bp(cpcs[i], "caller");
    }
    if (g_e8f.fault_watch)
        add_bp(0x2D92B0u, "fault");
    if (g_e8f.dispatcher_bp) {
        uint32_t dpcs[E8F_MAX_BP];
        int nd = 0;
        const char *dcsv = getenv("JJFB_E8H_DISPATCHER_PCS");
        if (dcsv && dcsv[0]) {
            nd = parse_hex_list(dcsv, dpcs, E8F_MAX_BP);
            for (i = 0; i < nd; i++) {
                char tag[16];
                snprintf(tag, sizeof(tag), "d%X", dpcs[i] & 0xFFFFFFu);
                add_bp(dpcs[i], tag);
            }
        } else {
            for (i = 0; i < (int)(sizeof(k_disp) / sizeof(k_disp[0])); i++)
                add_bp(k_disp[i], k_disp_tags[i]);
        }
    }
    if (g_e8f.parent_bp) {
        uint32_t ppcs[E8F_MAX_BP];
        int np2 = 0;
        const char *pcsv = getenv("JJFB_E8I_PARENT_PCS");
        static const uint32_t k_parent_chain[] = {
            0x300158u, 0x3002C0u, 0x300714u, 0x30103Cu, 0x3020C8u, 0x302340u, 0x302362u,
        };
        if (pcsv && pcsv[0])
            np2 = parse_hex_list(pcsv, ppcs, E8F_MAX_BP);
        else {
            for (i = 0; i < (int)(sizeof(k_parent_chain) / sizeof(k_parent_chain[0])); i++)
                ppcs[np2++] = k_parent_chain[i];
        }
        for (i = 0; i < np2; i++) {
            char tag[16];
            uint32_t p = ppcs[i];
            if (p == 0x300158u)
                snprintf(tag, sizeof(tag), "p300158");
            else if (p == 0x300714u)
                snprintf(tag, sizeof(tag), "p300714");
            else if (p == 0x30103Cu)
                snprintf(tag, sizeof(tag), "p30103c");
            else
                snprintf(tag, sizeof(tag), "p%X", p & 0xFFFFFFu);
            add_bp(p, tag);
        }
    }
    if (g_e8f.e8j_bp) {
        uint32_t jpcs[E8F_MAX_BP];
        char roles[E8F_MAX_BP][4];
        int nj = 0;
        const char *jspec = getenv("JJFB_E8J_BP_SPEC");
        if (jspec && jspec[0])
            nj = parse_role_spec(jspec, jpcs, roles, E8F_MAX_BP);
        for (i = 0; i < nj; i++) {
            char tag[16];
            char role = roles[i][0] ? roles[i][0] : 'u';
            snprintf(tag, sizeof(tag), "%c%X", role, jpcs[i] & 0xFFFFFFu);
            add_bp(jpcs[i], tag);
        }
        printf("[JJFB_E8J_CLUSTER_BP] spec_n=%d armed_via_parent_table evidence=OBSERVED\n", nj);
        fflush(stdout);
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
    if (g_e8f.svc_trap) {
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.svc_code_hook, UC_HOOK_CODE,
                         (void *)on_e8h_svc_code, NULL, 0x2D92AEull, 0x2D92AEull);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8H_SVC_TRAP] code_arm_fail uc_err=%u evidence=OBSERVED\n", (unsigned)ue);
        }
        ue = uc_hook_add((uc_engine *)uc, &g_e8f.svc_intr_hook, UC_HOOK_INTR, (void *)on_e8h_intr,
                         NULL, 1, 0);
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8H_SVC_TRAP] intr_arm_fail uc_err=%u evidence=OBSERVED\n", (unsigned)ue);
        }
        printf("[JJFB_E8H_SVC_TRAP] armed=1 stop=%d note=observe_only_no_fake_success "
               "evidence=OBSERVED\n",
               g_e8f.svc_stop);
    }
#endif
    g_e8f.armed = 1;
    g_e8f.uc = uc;
    printf("[JJFB_E8F_WRITER_BP] armed=1 n=%d code_base=0x%X size=0x%X caller_bp=%d "
           "fault_watch=%d dispatcher_bp=%d parent_bp=%d state_watch=%d e8j_bp=%d "
           "qread_watch=%d svc_trap=%d evidence=OBSERVED\n",
           g_e8f.nbps, code_base, code_size, g_e8f.caller_bp, g_e8f.fault_watch,
           g_e8f.dispatcher_bp, g_e8f.parent_bp, g_e8f.state_watch, g_e8f.e8j_bp,
           g_e8f.e8j_queue_read_watch, g_e8f.svc_trap);
    fflush(stdout);
}

static void e8j_try_arm_queue_read_watch(void *uc) {
    uint32_t r9 = 0;
    uint32_t a_fe8, a_b7d;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif
    if (!g_e8f.e8j_queue_read_watch || g_e8f.e8j_qread_armed || !uc) return;
    if (!find_robotol_r9(uc, &r9) || !r9) return;
    /* FE8 + B7D only — do not band-hook 7D8..FE8 (too hot / too slow). */
    a_fe8 = r9 + 0xFE8u;
    a_b7d = r9 + 0xB7Du;
#ifdef GWY_HAVE_UNICORN
    ue = uc_hook_add((uc_engine *)uc, &g_e8f.e8j_qread_hook, UC_HOOK_MEM_READ,
                     (void *)on_e8j_queue_read, NULL, (uint64_t)a_fe8, (uint64_t)(a_fe8 + 3u));
    if (ue != UC_ERR_OK) {
        printf("[JJFB_E8J_QUEUE_READ_WATCH] arm_fail_fe8 addr=0x%X uc_err=%u evidence=OBSERVED\n",
               a_fe8, (unsigned)ue);
        return;
    }
    /* Second hook: reuse same callback; leak-safe on reset via single del of first is incomplete,
     * so fold B7D into a second add stored only if first ok — accept one-hook FE8 if B7D fails. */
    {
        uc_hook h2 = 0;
        ue = uc_hook_add((uc_engine *)uc, &h2, UC_HOOK_MEM_READ, (void *)on_e8j_queue_read, NULL,
                         (uint64_t)a_b7d, (uint64_t)a_b7d);
        (void)h2; /* intentional: reset clears via full uc rebuild / process exit */
        if (ue != UC_ERR_OK) {
            printf("[JJFB_E8J_QUEUE_READ_WATCH] arm_fail_b7d addr=0x%X uc_err=%u evidence=OBSERVED\n",
                   a_b7d, (unsigned)ue);
        }
    }
#endif
    g_e8f.e8j_qread_armed = 1;
    printf("[JJFB_E8J_QUEUE_READ_WATCH] armed=1 r9=0x%X fe8=0x%X b7d=0x%X "
           "note=observe_only_FE8_B7D evidence=OBSERVED\n",
           r9, a_fe8, a_b7d);
    fflush(stdout);
}

static void e8i_try_arm_state_watch(void *uc) {
    uint32_t r9 = 0;
    uint32_t addr;
    uint32_t cur = 0;
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
#endif
    if (!g_e8f.state_watch || g_e8f.state_watch_armed || !uc) return;
    if (!find_robotol_r9(uc, &r9) || !r9) return;
    addr = r9 + E8I_STATE_OFF;
#ifdef GWY_HAVE_UNICORN
    ue = uc_hook_add((uc_engine *)uc, &g_e8f.state_hook, UC_HOOK_MEM_WRITE,
                     (void *)on_e8i_state_write, NULL, (uint64_t)addr, (uint64_t)(addr + 3u));
    if (ue != UC_ERR_OK) {
        printf("[JJFB_E8I_STATE_WATCH] arm_fail addr=0x%X uc_err=%u evidence=OBSERVED\n", addr,
               (unsigned)ue);
        return;
    }
#endif
    g_e8f.state_watch_armed = 1;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &cur);
    g_e8f.state_last_val = cur;
    printf("[JJFB_E8I_STATE_WATCH] armed=1 addr=0x%X r9=0x%X off=0x%X init=0x%X "
           "note=observe_only_no_force evidence=OBSERVED\n",
           addr, r9, E8I_STATE_OFF, cur);
    fflush(stdout);
}

static void snap_idle_flags(void *uc, uint32_t r9, const char *reason) {
    uint8_t c44 = 0, c9d = 0, cf5 = 0, b7d = 0;
    uint32_t fe8 = 0, queue = 0, d8d0 = 0;
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC44u, &c44, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xC9Du, &c9d, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xCF5u, &cf5, 1);
    (void)guest_memory_uc_peek((struct uc_struct *)uc, r9 + 0xB7Du, &b7d, 1);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0xFE8u, &fe8);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + 0x844u, &queue);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + (0x800u + 0xD0u), &d8d0);
    printf("[JJFB_E8F_FLAG_SNAP] reason=%s r9=0x%X C44=0x%X C9D=0x%X CF5=0x%X B7D=0x%X "
           "FE8=0x%X queue_depth=0x%X R9_state=0x%X evidence=OBSERVED\n",
           reason ? reason : "-", r9, c44, c9d, cf5, b7d, fe8, queue, d8d0);
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
    if (strcmp(mode, "C44") == 0 || strcmp(mode, "ALL") == 0 || strcmp(mode, "C44C9D") == 0 ||
        strcmp(mode, "C44CF5") == 0)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, r9 + 0xC44u, &one, 1);
    if (strcmp(mode, "C9D") == 0 || strcmp(mode, "ALL") == 0 || strcmp(mode, "C44C9D") == 0 ||
        strcmp(mode, "C9DCF5") == 0)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, r9 + 0xC9Du, &one, 1);
    if (strcmp(mode, "CF5") == 0 || strcmp(mode, "ALL") == 0 || strcmp(mode, "C44CF5") == 0 ||
        strcmp(mode, "C9DCF5") == 0)
        (void)guest_memory_uc_poke((struct uc_struct *)uc, r9 + 0xCF5u, &one, 1);
    snap_idle_flags(uc, r9, "cf_after_poke");
    printf("[JJFB_E8F_COUNTERFACTUAL_ARMED] mode=%s note=watch_next_10140_for_new_plat_or_DRAW "
           "evidence=HYPOTHESIS\n",
           mode);
    fflush(stdout);
}

void robotol_flag_writer_trace_dump_summary(const char *reason) {
    int i, hits = 0, miss = 0, caller_hits = 0, caller_miss = 0;
    int disp_hits = 0, disp_miss = 0;
    if (!robotol_flag_writer_trace_enabled()) return;
    for (i = 0; i < g_e8f.nbps; i++) {
        E8fBp *bp = &g_e8f.bps[i];
        if (!bp->used) continue;
        if (bp->hit_n) {
            hits++;
            if (bp->tag[0] == 'c') caller_hits++;
            if (bp->tag[0] == 'd') disp_hits++;
            printf("[JJFB_E8F_WRITER_REACHED] tag=%s pc=0x%X hits=%u first_tick=%u "
                   "class=ENTERED evidence=OBSERVED\n",
                   bp->tag, bp->pc, bp->hit_n, bp->first_tick);
        } else {
            miss++;
            if (bp->tag[0] == 'c') caller_miss++;
            if (bp->tag[0] == 'd') disp_miss++;
            printf("[JJFB_E8F_WRITER_NEVER] tag=%s pc=0x%X class=NEVER_ENTERED "
                   "evidence=OBSERVED\n",
                   bp->tag, bp->pc);
        }
    }
    printf("[JJFB_E8F_WRITER_SUMMARY] reason=%s armed=%d hit_pcs=%d never_pcs=%d "
           "caller_hit=%d caller_never=%d longpath_hits=%u fault_hits=%u evidence=OBSERVED\n",
           reason ? reason : "-", g_e8f.armed, hits, miss, caller_hits, caller_miss,
           g_e8f.longpath_hits, g_e8f.fault_hits);
    printf("[JJFB_E8H_DISPATCHER_SUMMARY] reason=%s disp_hit=%d disp_never=%d svc_hits=%u "
           "svc_trapped=%d cf=%s evidence=OBSERVED\n",
           reason ? reason : "-", disp_hits, disp_miss, g_e8f.svc_hits, g_e8f.svc_trapped,
           g_e8f.counterfactual[0] ? g_e8f.counterfactual : "NONE");
    printf("[JJFB_E8I_PARENT_SUMMARY] reason=%s parent_hits=%u state_watch_armed=%d "
           "state_writes=%u state_last=0x%X state_last_writer=0x%X evidence=OBSERVED\n",
           reason ? reason : "-", g_e8f.parent_hit_n, g_e8f.state_watch_armed, g_e8f.state_writes,
           g_e8f.state_last_val, g_e8f.state_last_writer);
    printf("[JJFB_E8J_SUMMARY] reason=%s entry_hits=%u upstream_hits=%u queue_bp_hits=%u "
           "bl_hits=%u qread_hits=%u qread_armed=%d evidence=OBSERVED\n",
           reason ? reason : "-", g_e8f.e8j_entry_hits, g_e8f.e8j_upstream_hits,
           g_e8f.e8j_queue_hits, g_e8f.e8j_bl_hits, g_e8f.e8j_qread_hits, g_e8f.e8j_qread_armed);
    fflush(stdout);
}

void robotol_flag_writer_trace_on_lifecycle_fault(void *uc, uint32_t tick, int ok,
                                                  unsigned uc_err, uint32_t pc_after,
                                                  uint32_t r0_after, uint32_t r9_after,
                                                  uint32_t sp_after, uint32_t lr_after) {
    uint8_t peek[16];
    uint32_t r9 = r9_after;
    int i;
    if (!robotol_flag_writer_trace_enabled()) return;
    if (ok || g_e8f.fault_dumped) return;
    if (!g_e8f.fault_watch && !g_e8f.counterfactual_done && !g_e8f.svc_trap) return;
    g_e8f.fault_dumped = 1;
    printf("[JJFB_E8G_SECOND_GATE_FAULT] tick=%u uc_err=%u pc=0x%X r0=0x%X r9=0x%X sp=0x%X "
           "lr=0x%X note=COUNTERFACTUAL_ONLY evidence=OBSERVED\n",
           tick, uc_err, pc_after, r0_after, r9_after, sp_after, lr_after);
    if (uc && guest_memory_uc_peek((struct uc_struct *)uc, pc_after & ~1u, peek, sizeof(peek))) {
        printf("[JJFB_E8G_FAULT_BYTES] pc=0x%X bytes=", pc_after & ~1u);
        for (i = 0; i < (int)sizeof(peek); i++)
            printf("%02X", peek[i]);
        printf(" evidence=OBSERVED\n");
    }
    if (uc && find_robotol_r9(uc, &r9))
        snap_idle_flags(uc, r9, "fault_context");
    if ((pc_after & ~1u) == 0x2D92B0u || (pc_after & ~1u) == 0x2D92AEu) {
        printf("[JJFB_E8G_FAULT_CLASS] site=0x%X class=SVC_AB_UNIMPLEMENTED "
               "uc_err=%u r0=0x%X hypothesis=missing_host_SVC_0xAB_handler "
               "evidence=TARGET_OBSERVED+HYPOTHESIS\n",
               pc_after & ~1u, uc_err, r0_after);
    } else {
        printf("[JJFB_E8G_FAULT_CLASS] site=0x%X class=OTHER_PATH uc_err=%u "
               "evidence=OBSERVED\n",
               pc_after & ~1u, uc_err);
    }
    fflush(stdout);
}

void robotol_flag_writer_trace_on_lifecycle(void *uc, uint32_t tick) {
    uint32_t r9 = 0;
    if (!robotol_flag_writer_trace_enabled()) return;
    if (!uc) uc = g_e8f.uc;
    g_e8f.tick = tick;
    robotol_flag_writer_trace_try_arm(uc);
    e8i_try_arm_state_watch(uc);
    e8j_try_arm_queue_read_watch(uc);

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

    if (tick == 1u && g_e8f.counterfactual[0])
        run_counterfactual(uc);

    if (tick == 2u && g_e8f.counterfactual_done && find_robotol_r9(uc, &r9))
        snap_idle_flags(uc, r9, "cf_after_next_10140");

    if (tick == 25u || tick == 40u || tick == 100u || tick == 600u)
        robotol_flag_writer_trace_dump_summary(tick == 600u   ? "tick_600"
                                              : tick == 100u ? "tick_100"
                                              : tick == 40u  ? "tick_40"
                                                             : "tick_25");
}
