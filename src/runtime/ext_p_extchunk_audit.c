#include "gwy_launcher/ext_p_extchunk_audit.h"
#include "gwy_launcher/ext_shell_publication_audit.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/ext_gwy_shell_native_exec.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/module_r9_switch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PXC_DUMP_WORDS 33u /* +0x00 .. +0x80 inclusive */
#define PXC_WATCH_LEN 0x80u
#define PXC_GATE_LINE                                                                             \
    "p_extchunk_gate=%s post_cfn_r9_gate=blocked post_continuation_gate=open "                    \
    "graphics_gate=blocked event_scheduler_gate=blocked nested_r9_scope_gate=open "               \
    "module_r9_switch_gate=open guest_callback_frame_gate=blocked "                               \
    "bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked"
#define PXC_GATE_ARGS (g_pxc.gate_open ? "open" : "blocked")

static struct {
    int enabled;
    int enabled_known;
    void *uc;
    int armed;
    int finalized;
    int gate_open;
    ExtchunkAuditClass last_class;
    char next_fix[80];
    char stop_reason[48];

    uint32_t helper;
    uint32_t p_guest;
    uint32_t p_len;
    uint64_t module_id;
    char module[72];

    int struct_logged;
    int watch_armed;
    uint32_t watch_base;
#ifdef GWY_HAVE_UNICORN
    uc_hook read_hook;
    uc_hook write_hook;
#endif

    uint32_t writes_seen;
    uint32_t reads_seen;
    uint32_t last_write_pc;
    uint32_t last_write_val;
    uint32_t last_read_pc;
    int saw_nonnull_write;
    int null_at_use;
    int owner_logged;

    int phase_alloc;
    int phase_robotol;
    int phase_mrc_init;
    int phase_pre_cont;
    int phase_post_cont;
    int phase_pre_fault;
    int mrc_init_not_observed_emitted;

    uint32_t cont_pc;
    uint32_t fault_pc;
    uint32_t fault_addr;
    uint32_t function_start;
    char address_expr[48];
} g_pxc;

const char *ext_p_extchunk_class_name(ExtchunkAuditClass c) {
    switch (c) {
    case EXTCHUNK_NEVER_WRITTEN: return "EXTCHUNK_NEVER_WRITTEN";
    case EXTCHUNK_WRITE_AFTER_FAULT_WINDOW: return "EXTCHUNK_WRITE_AFTER_FAULT_WINDOW";
    case EXTCHUNK_PROVIDER_PATH_SKIPPED: return "EXTCHUNK_PROVIDER_PATH_SKIPPED";
    case EXTCHUNK_GWY_CONTEXT_HYPOTHESIS: return "EXTCHUNK_GWY_CONTEXT_HYPOTHESIS";
    case EXTCHUNK_FILLED_BUT_NULL_AT_USE: return "EXTCHUNK_FILLED_BUT_NULL_AT_USE";
    default: return "UNKNOWN";
    }
}

ExtchunkAuditClass ext_p_extchunk_audit_last_class(void) { return g_pxc.last_class; }
int ext_p_extchunk_gate_open(void) { return g_pxc.gate_open; }
int ext_p_extchunk_audit_armed(void) { return g_pxc.armed; }

int ext_p_extchunk_audit_enabled(void) {
    const char *e;
    if (g_pxc.enabled_known) return g_pxc.enabled;
    e = getenv("GWY_P_EXTCHUNK_AUDIT");
    if (e && e[0] == '1' && e[1] == '\0') {
        g_pxc.enabled = 1;
    } else {
        const char *pc = getenv("GWY_POST_CONT_AUDIT");
        g_pxc.enabled = (pc && pc[0] == '1' && pc[1] == '\0') ? 1 : 0;
    }
    g_pxc.enabled_known = 1;
    return g_pxc.enabled;
}

void ext_p_extchunk_audit_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_pxc.uc && g_pxc.watch_armed) {
        uc_hook_del((uc_engine *)g_pxc.uc, g_pxc.read_hook);
        uc_hook_del((uc_engine *)g_pxc.uc, g_pxc.write_hook);
    }
#endif
    memset(&g_pxc, 0, sizeof(g_pxc));
    snprintf(g_pxc.next_fix, sizeof(g_pxc.next_fix), "%s", "NONE");
    snprintf(g_pxc.stop_reason, sizeof(g_pxc.stop_reason), "%s", "UNKNOWN");
}

void ext_p_extchunk_audit_bind_uc(void *uc) { g_pxc.uc = uc; }

static int name_is_robotol(const char *nm) {
    return nm && strstr(nm, "robotol.ext") != NULL;
}

static uint32_t peek_u32(void *uc, uint32_t addr) {
    uint32_t v = 0;
    if (!uc || !addr) return 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, addr, &v);
    return v;
}

static void emit_phase(const char *phase) {
    uint32_t chunk = 0;
    if (!g_pxc.p_guest) {
        printf("[JJFB_EXTCHUNK_PHASE] phase=%s P=0x0 P+0xC=0x0 " PXC_GATE_LINE
               " evidence=OBSERVED\n",
               phase ? phase : "?", PXC_GATE_ARGS);
        fflush(stdout);
        return;
    }
    if (g_pxc.uc) chunk = peek_u32(g_pxc.uc, g_pxc.p_guest + 0x0Cu);
    printf("[JJFB_EXTCHUNK_PHASE] phase=%s P=0x%X P+0xC=0x%X " PXC_GATE_LINE
           " evidence=OBSERVED\n",
           phase ? phase : "?", g_pxc.p_guest, chunk, PXC_GATE_ARGS);
    fflush(stdout);
}

static void emit_p_struct_once(void) {
    if (g_pxc.struct_logged || !g_pxc.p_guest) return;
    g_pxc.struct_logged = 1;
    printf("[JJFB_P_STRUCT] base=0x%X range=+0x00..+0x80 len=%u "
           "names=start_of_ER_RW,ER_RW_Length,ext_type,mrc_extChunk,stack "
           "evidence=DOCUMENTED source=mr_helper.h " PXC_GATE_LINE "\n",
           g_pxc.p_guest, g_pxc.p_len ? g_pxc.p_len : 20u, PXC_GATE_ARGS);
    fflush(stdout);
}

static void dump_p_words(const char *stage) {
    uint32_t i;
    if (!g_pxc.uc || !g_pxc.p_guest) return;
    printf("[JJFB_P_DUMP] stage=%s p=0x%X", stage ? stage : "?", g_pxc.p_guest);
    for (i = 0; i < PXC_DUMP_WORDS; i++) {
        uint32_t off = i * 4u;
        uint32_t v = peek_u32(g_pxc.uc, g_pxc.p_guest + off);
        printf(" +0x%02X=0x%X", off, v);
    }
    printf(" " PXC_GATE_LINE " evidence=OBSERVED\n", PXC_GATE_ARGS);
    fflush(stdout);
}

static uint32_t find_function_start(void *uc, uint32_t fault_pc) {
    uint32_t pc = fault_pc & ~1u;
    int steps;
    if (!uc) return 0;
    for (steps = 0; steps < 64; steps++) {
        uint32_t w = 0;
        uint32_t addr = pc - (uint32_t)(steps * 2);
        if (addr < 0x1000u) break;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, addr & ~1u, &w)) break;
        {
            uint16_t h = (uint16_t)(w & 0xFFFFu);
            if ((h & 0xFF00u) == 0xB500u) return addr & ~1u;
        }
    }
    return 0;
}

static void emit_func_bytes(void *uc, uint32_t start, uint32_t end) {
    uint32_t a;
    if (!uc || !start || end <= start || (end - start) > 0x100u) return;
    printf("[JJFB_P_FUNC_BYTES] start=0x%X end=0x%X bytes=", start, end);
    for (a = start; a < end; a += 2u) {
        uint32_t w = 0;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, a & ~1u, &w)) break;
        printf("%04X", (unsigned)(w & 0xFFFFu));
        if (a + 2u < end) printf(":");
    }
    printf(" " PXC_GATE_LINE " evidence=OBSERVED\n", PXC_GATE_ARGS);
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static void on_p_mem_access(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                            int64_t value, void *user_data) {
    uint32_t addr = (uint32_t)address;
    uint32_t off;
    uint32_t pc = 0, lr = 0;
    int is_write;
    (void)size;
    (void)user_data;
    if (!g_pxc.p_guest || addr < g_pxc.p_guest || addr >= g_pxc.p_guest + PXC_WATCH_LEN) return;
    off = addr - g_pxc.p_guest;
    is_write = (type == UC_MEM_WRITE || type == UC_MEM_WRITE_UNMAPPED ||
                type == UC_MEM_WRITE_PROT);
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    {
        /* DSM has no full GCO watch; P access from DSM PC proves helper ran. */
        const GwyLoadedModule *pm =
            module_registry_find_by_code_addr(gwy_ext_loader_bound_registry(), pc);
        if (pm && pm->origin == MODULE_ORIGIN_DSM) {
            module_r9_switch_note_dsm_code(pm->module_id);
            module_r9_switch_ensure_dsm_r9(uc, pc);
        }
    }

    if (is_write) {
        uint32_t oldv = peek_u32(uc, addr);
        uint32_t newv = (uint32_t)value;
        printf("[JJFB_P_WRITE] off=0x%X old=0x%X new=0x%X pc=0x%X lr=0x%X " PXC_GATE_LINE
               " evidence=OBSERVED\n",
               off, oldv, newv, pc, lr, PXC_GATE_ARGS);
        {
            const GwyLoadedModule *wm =
                module_registry_find_by_code_addr(gwy_ext_loader_bound_registry(), pc);
            ext_shell_publication_audit_on_p_write(
                g_pxc.p_guest, off, oldv, newv, pc, lr, wm ? wm->requested_name : "unknown");
        }
        if (off == 0x0Cu) {
            g_pxc.writes_seen++;
            g_pxc.last_write_pc = pc;
            g_pxc.last_write_val = newv;
            if (newv) g_pxc.saw_nonnull_write = 1;
            printf("[JJFB_EXTCHUNK_WRITE] old=0x%X new=0x%X pc=0x%X lr=0x%X phase=live "
                   "addr=P+0xC " PXC_GATE_LINE " evidence=OBSERVED\n",
                   oldv, newv, pc, lr, PXC_GATE_ARGS);
            printf("[JJFB_EXTCHUNK_WATCH] addr=P+0xC value=0x%X " PXC_GATE_LINE
                   " evidence=OBSERVED\n",
                   newv, PXC_GATE_ARGS);
            {
                const GwyLoadedModule *wm =
                    module_registry_find_by_code_addr(gwy_ext_loader_bound_registry(), pc);
                ext_gwy_shell_native_exec_on_pxc_write(
                    oldv, newv, pc, lr, wm ? wm->requested_name : "unknown");
            }
        }
        fflush(stdout);
        return;
    }

    {
        uint32_t val = peek_u32(uc, addr);
        /* Unicorn may deliver value on read; prefer memory peek for consistency. */
        if (value != 0 && (uint32_t)value != val) val = (uint32_t)value;
        printf("[JJFB_P_READ] off=0x%X val=0x%X pc=0x%X lr=0x%X " PXC_GATE_LINE
               " evidence=OBSERVED\n",
               off, val, pc, lr, PXC_GATE_ARGS);
        if (off == 0x0Cu) {
            g_pxc.reads_seen++;
            g_pxc.last_read_pc = pc;
            printf("[JJFB_EXTCHUNK_READ] value=0x%X pc=0x%X func=0x%X " PXC_GATE_LINE
                   " evidence=OBSERVED\n",
                   val, pc, g_pxc.function_start ? g_pxc.function_start : (pc & ~0xFFu),
                   PXC_GATE_ARGS);
            printf("[JJFB_EXTCHUNK_WATCH] addr=P+0xC value=0x%X " PXC_GATE_LINE
                   " evidence=OBSERVED\n",
                   val, PXC_GATE_ARGS);
        }
        fflush(stdout);
    }
}

static void arm_p_watch(void *uc) {
    uc_err ue1, ue2;
    uint64_t begin, end;
    if (!uc || !g_pxc.p_guest) return;
    /* Re-arm when nested _mr_c_function_new installs a new P (Phase 6J). */
    if (g_pxc.watch_armed) {
        if (g_pxc.watch_base == g_pxc.p_guest) return;
        uc_hook_del((uc_engine *)uc, g_pxc.read_hook);
        uc_hook_del((uc_engine *)uc, g_pxc.write_hook);
        g_pxc.watch_armed = 0;
        g_pxc.read_hook = 0;
        g_pxc.write_hook = 0;
    }
    begin = (uint64_t)g_pxc.p_guest;
    end = begin + (uint64_t)PXC_WATCH_LEN - 1ull;
    ue1 = uc_hook_add((uc_engine *)uc, &g_pxc.read_hook, UC_HOOK_MEM_READ, (void *)on_p_mem_access,
                      NULL, begin, end);
    ue2 = uc_hook_add((uc_engine *)uc, &g_pxc.write_hook, UC_HOOK_MEM_WRITE, (void *)on_p_mem_access,
                      NULL, begin, end);
    if (ue1 == UC_ERR_OK && ue2 == UC_ERR_OK) {
        g_pxc.watch_armed = 1;
        g_pxc.watch_base = g_pxc.p_guest;
        g_pxc.uc = uc;
        printf("[JJFB_EXTCHUNK_WATCH] armed base=0x%X len=0x%X " PXC_GATE_LINE
               " evidence=OBSERVED\n",
               g_pxc.p_guest, PXC_WATCH_LEN, PXC_GATE_ARGS);
        fflush(stdout);
    }
}
#else
static void arm_p_watch(void *uc) { (void)uc; }
#endif

static void maybe_note_robotol_phase(void) {
    ModuleRegistry *reg;
    size_t i;
    if (g_pxc.phase_robotol) return;
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return;
    for (i = 0; i < reg->count; i++) {
        if (name_is_robotol(reg->modules[i].resolved_name) &&
            reg->modules[i].state >= GWY_MODULE_MAPPED) {
            g_pxc.phase_robotol = 1;
            if (!g_pxc.module_id) g_pxc.module_id = reg->modules[i].module_id;
            snprintf(g_pxc.module, sizeof(g_pxc.module), "%s", reg->modules[i].resolved_name);
            emit_phase("after_robotol_load");
            return;
        }
    }
}

static void classify_and_summary(const char *stop) {
    ExtchunkAuditClass c = EXTCHUNK_CLASS_UNKNOWN;
    const char *fix = "NONE";
    if (g_pxc.finalized) return;
    g_pxc.finalized = 1;
    if (stop && stop[0]) snprintf(g_pxc.stop_reason, sizeof(g_pxc.stop_reason), "%s", stop);

    if (!g_pxc.phase_mrc_init && !g_pxc.mrc_init_not_observed_emitted) {
        g_pxc.mrc_init_not_observed_emitted = 1;
        emit_phase("mrc_init_not_observed");
    }

    if (g_pxc.null_at_use && g_pxc.saw_nonnull_write) {
        c = EXTCHUNK_FILLED_BUT_NULL_AT_USE;
        fix = "EXTCHUNK_LIFETIME_AUDIT";
    } else if (g_pxc.writes_seen > 0 && g_pxc.null_at_use && g_pxc.last_write_pc &&
               g_pxc.fault_pc && g_pxc.last_write_pc > g_pxc.fault_pc) {
        c = EXTCHUNK_WRITE_AFTER_FAULT_WINDOW;
        fix = "PROVIDER_ORDERING_AUDIT";
    } else if (g_pxc.writes_seen == 0 && (g_pxc.reads_seen > 0 || g_pxc.null_at_use)) {
        /* NULL consumed at use with zero natural providers — GWY shell context hypothesis. */
        c = EXTCHUNK_GWY_CONTEXT_HYPOTHESIS;
        fix = "GWY_STARTGAME_RUNAPP_CONTEXT_AUDIT";
    } else if (g_pxc.writes_seen == 0 && g_pxc.phase_robotol) {
        c = EXTCHUNK_PROVIDER_PATH_SKIPPED;
        fix = "RESTORE_PLUGIN_OR_GWY_LOAD_PATH";
    } else if (g_pxc.writes_seen == 0) {
        c = EXTCHUNK_NEVER_WRITTEN;
        fix = "MISSING_EXTCHUNK_PROVIDER_PATH";
    } else {
        c = EXTCHUNK_CLASS_UNKNOWN;
        fix = "NONE";
    }

    g_pxc.last_class = c;
    g_pxc.gate_open = 0;
    snprintf(g_pxc.next_fix, sizeof(g_pxc.next_fix), "%s", fix);

    printf("[JJFB_EXTCHUNK_SUMMARY] extchunk_class=%s writes_seen=%u reads_seen=%u "
           "last_write_pc=0x%X last_write_val=0x%X null_at_use=%s "
           "function_start=0x%X memory_access_pc=0x%X fault_expr=%s "
           "next_allowed_fix=%s stop_reason=%s P=0x%X " PXC_GATE_LINE
           " evidence=OBSERVED note=observe_only\n",
           ext_p_extchunk_class_name(c), g_pxc.writes_seen, g_pxc.reads_seen, g_pxc.last_write_pc,
           g_pxc.last_write_val, g_pxc.null_at_use ? "yes" : "no", g_pxc.function_start,
           g_pxc.fault_pc, g_pxc.address_expr[0] ? g_pxc.address_expr : "none", g_pxc.next_fix,
           g_pxc.stop_reason, g_pxc.p_guest, PXC_GATE_ARGS);
    fflush(stdout);
}

void ext_p_extchunk_audit_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                         uint32_t rw_base, uint32_t rw_size) {
    (void)rw_base;
    (void)rw_size;
    if (!ext_p_extchunk_audit_enabled()) return;
    if (helper) g_pxc.helper = helper;
    if (!p_guest) return;
    g_pxc.p_guest = p_guest;
    g_pxc.p_len = p_len ? p_len : 20u;
    ext_shell_publication_audit_on_p_candidate(p_guest);
    emit_p_struct_once();
    if (g_pxc.uc) arm_p_watch(g_pxc.uc);
    if (!g_pxc.phase_alloc) {
        g_pxc.phase_alloc = 1;
        emit_phase("after_cfunction_p_alloc");
        dump_p_words("after_cfunction_p_alloc");
    }
    maybe_note_robotol_phase();
}

void ext_p_extchunk_audit_on_module_event(uint64_t module_id, const char *module_name,
                                          const char *phase_tag) {
    if (!ext_p_extchunk_audit_enabled()) return;
    if (module_id) g_pxc.module_id = module_id;
    if (module_name && module_name[0])
        snprintf(g_pxc.module, sizeof(g_pxc.module), "%s", module_name);
    if (phase_tag && strcmp(phase_tag, "after_mrc_init") == 0) {
        g_pxc.phase_mrc_init = 1;
        emit_phase("after_mrc_init");
        return;
    }
    if (name_is_robotol(module_name) || (phase_tag && strstr(phase_tag, "robotol"))) {
        if (!g_pxc.phase_robotol) {
            g_pxc.phase_robotol = 1;
            emit_phase(phase_tag && phase_tag[0] ? phase_tag : "after_robotol_load");
        }
    } else if (phase_tag && phase_tag[0]) {
        emit_phase(phase_tag);
    }
}

void ext_p_extchunk_audit_on_continuation_resume(void *uc, uint64_t module_id, const char *module,
                                                 uint32_t call_pc, uint32_t continuation_pc,
                                                 const uint32_t regs[16], uint32_t sp, uint32_t lr,
                                                 uint32_t cpsr) {
    (void)call_pc;
    (void)sp;
    (void)lr;
    (void)cpsr;
    if (!ext_p_extchunk_audit_enabled()) return;
    if (module && !name_is_robotol(module) && module_id == 0) {
        /* Still allow if P already bound. */
        if (!g_pxc.p_guest) return;
    }
    g_pxc.armed = 1;
    g_pxc.uc = uc ? uc : g_pxc.uc;
    if (module_id) g_pxc.module_id = module_id;
    if (module && module[0]) snprintf(g_pxc.module, sizeof(g_pxc.module), "%s", module);
    g_pxc.cont_pc = continuation_pc;

    if (g_pxc.p_guest && g_pxc.uc) arm_p_watch(g_pxc.uc);
    maybe_note_robotol_phase();

    if (!g_pxc.phase_pre_cont) {
        g_pxc.phase_pre_cont = 1;
        emit_phase("pre_continuation");
        dump_p_words("pre_continuation");
    }
    if (!g_pxc.phase_post_cont) {
        g_pxc.phase_post_cont = 1;
        emit_phase("post_continuation");
        dump_p_words("post_continuation");
    }
    if (regs && g_pxc.p_guest && regs[3] == g_pxc.p_guest && !g_pxc.owner_logged) {
        g_pxc.owner_logged = 1;
        printf("[JJFB_P_OWNER] p=0x%X source_reg=r3 pc=0x%X " PXC_GATE_LINE
               " evidence=OBSERVED\n",
               g_pxc.p_guest, continuation_pc, PXC_GATE_ARGS);
        fflush(stdout);
    }
    printf("[JJFB_EXTCHUNK_ARM] module=%s cont_pc=0x%X P=0x%X helper=0x%X " PXC_GATE_LINE
           " evidence=OBSERVED\n",
           g_pxc.module[0] ? g_pxc.module : "?", continuation_pc, g_pxc.p_guest, g_pxc.helper,
           PXC_GATE_ARGS);
    fflush(stdout);
}

void ext_p_extchunk_audit_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                  uint32_t cpsr) {
    (void)module_id;
    (void)cpsr;
    if (!ext_p_extchunk_audit_enabled() || g_pxc.finalized || !regs) return;
    g_pxc.uc = uc ? uc : g_pxc.uc;
    if (g_pxc.p_guest && g_pxc.uc && !g_pxc.watch_armed) arm_p_watch(g_pxc.uc);
    maybe_note_robotol_phase();
    if (g_pxc.p_guest && regs[3] == g_pxc.p_guest && !g_pxc.owner_logged) {
        g_pxc.owner_logged = 1;
        printf("[JJFB_P_OWNER] p=0x%X source_reg=r3 pc=0x%X " PXC_GATE_LINE
               " evidence=OBSERVED\n",
               g_pxc.p_guest, pc, PXC_GATE_ARGS);
        fflush(stdout);
    }
}

void ext_p_extchunk_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                       const uint32_t regs[16], uint32_t cpsr) {
    int thumb;
    uint32_t insn = 0;
    int rt = 0, rn = 0, is_load = 0;
    uint32_t imm = 0;
    uint32_t chunk = 0;
    if (!ext_p_extchunk_audit_enabled() || g_pxc.finalized) return;
    g_pxc.uc = uc ? uc : g_pxc.uc;
    g_pxc.armed = 1;
    g_pxc.fault_pc = fault_pc;
    g_pxc.fault_addr = fault_addr;
    thumb = regs ? ((cpsr >> 5) & 1) : 1;

    if (!g_pxc.phase_pre_fault) {
        g_pxc.phase_pre_fault = 1;
        emit_phase("pre_fault");
        dump_p_words("pre_fault");
    }

    if (g_pxc.p_guest && g_pxc.uc) chunk = peek_u32(g_pxc.uc, g_pxc.p_guest + 0x0Cu);
    if (chunk == 0) g_pxc.null_at_use = 1;

    if (uc) {
        if (thumb)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, fault_pc & ~1u, &insn);
        else
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, fault_pc & ~3u, &insn);
    }
    if (ext_entry_decode_ldr_imm(insn, thumb, &rt, &rn, &imm, &is_load)) {
        snprintf(g_pxc.address_expr, sizeof(g_pxc.address_expr), "r%d+0x%X", rn, imm);
    } else {
        snprintf(g_pxc.address_expr, sizeof(g_pxc.address_expr), "unknown");
    }

    g_pxc.function_start = find_function_start(uc, fault_pc);
    if (g_pxc.function_start)
        emit_func_bytes(uc, g_pxc.function_start, g_pxc.function_start + 0x68u);

    /* Attribute fault-window P+0xC consumption when mem-hook missed earlier LDR. */
    if (regs && g_pxc.p_guest && regs[3] == g_pxc.p_guest && chunk == 0) {
        if (g_pxc.reads_seen == 0) {
            g_pxc.reads_seen = 1;
            g_pxc.last_read_pc = fault_pc;
            printf("[JJFB_EXTCHUNK_READ] value=0x0 pc=0x%X func=0x%X note=fault_window_infer "
                   PXC_GATE_LINE " evidence=OBSERVED\n",
                   fault_pc, g_pxc.function_start, PXC_GATE_ARGS);
        }
    }

    if (regs && g_pxc.p_guest && regs[3] == g_pxc.p_guest && !g_pxc.owner_logged) {
        g_pxc.owner_logged = 1;
        printf("[JJFB_P_OWNER] p=0x%X source_reg=r3 pc=0x%X " PXC_GATE_LINE
               " evidence=OBSERVED\n",
               g_pxc.p_guest, fault_pc, PXC_GATE_ARGS);
    }

    printf("[JJFB_EXTCHUNK_FAULT] function_start=0x%X memory_access_pc=0x%X expr=%s "
           "r0=0x%X r3=0x%X P=0x%X P+0xC=0x%X fault_addr=0x%X writes_seen=%u " PXC_GATE_LINE
           " evidence=OBSERVED\n",
           g_pxc.function_start, fault_pc, g_pxc.address_expr, regs ? regs[0] : 0,
           regs ? regs[3] : 0, g_pxc.p_guest, chunk, fault_addr, g_pxc.writes_seen, PXC_GATE_ARGS);
    fflush(stdout);

    ext_shell_publication_audit_on_mem_fault(fault_pc, fault_addr, g_pxc.p_guest, chunk);
    classify_and_summary("NEW_FAULT");
}

void ext_p_extchunk_audit_finalize(const char *stop_reason) {
    if (!ext_p_extchunk_audit_enabled()) return;
    if (g_pxc.finalized) return;
    if (g_pxc.p_guest && g_pxc.uc) dump_p_words("FINALIZE");
    classify_and_summary(stop_reason ? stop_reason : "EXPLICIT_STOP");
}
