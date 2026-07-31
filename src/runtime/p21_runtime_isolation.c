#include "gwy_launcher/p21_runtime_isolation.h"
#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/mrp_runtime_stack.h"
#include "gwy_launcher/original_gwy_bootstrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define FAULT_FN 0x30D5B0u
#define FAULT_ADD_R9 0x30D5C2u
#define FAULT_LOAD_1914 0x30D5CAu
#define FAULT_DEREF 0x30D5D2u
#define SLOT_1914_OFF 0x1914u

static struct {
    int enabled_known;
    int enabled;
    int finalized;
    int hooks_armed;
    void *uc;
    uint32_t image_base;

    uint32_t gbrw_p;
    uint32_t gamelist_p;
    uint32_t gbrw_erw;
    uint32_t gamelist_erw;

    int hit_fault_fn;
    int hit_load_1914;
    int hit_deref;
    int fault_seen;
    uint32_t fault_r9;
    uint32_t fault_r0;
    uint32_t slot_1914_val;
    char fault_r9_owner[48];
    uint32_t slot_writer_pc;
    char slot_writer_mod[48];

    int callback_r9_parent;
    int startgame_enter;
    uint32_t parent_sha_before;
    uint32_t parent_sha_after;

    int slot_watch_armed;
    uint32_t slot_watch_addr;
    uint32_t slot_write_n;
    uint32_t slot_first_val;
    int slot_first_seen;
} g_p21;

static int env_is_1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

int p21_runtime_isolation_enabled(void) {
    if (!g_p21.enabled_known) {
        g_p21.enabled = env_is_1("JJFB_P21_RUNTIME_ISOLATION") ||
                        (env_is_1("JJFB_P20_GBRWCORE_LIFECYCLE") &&
                         original_gwy_bootstrap_enabled());
        g_p21.enabled_known = 1;
    }
    return g_p21.enabled;
}

void p21_runtime_isolation_reset(void) { memset(&g_p21, 0, sizeof(g_p21)); }

int p21_gate_p_isolated(void) {
    return g_p21.gbrw_p && g_p21.gamelist_p && g_p21.gbrw_p != g_p21.gamelist_p;
}
int p21_gate_parent_p_intact(void) {
    return g_p21.parent_sha_before && g_p21.parent_sha_before == g_p21.parent_sha_after;
}
int p21_gate_callback_r9_parent(void) { return g_p21.callback_r9_parent; }
int p21_gate_no_30d5d2_fault(void) { return g_p21.hit_fault_fn && !g_p21.fault_seen; }
int p21_gate_startgame_enter(void) { return g_p21.startgame_enter; }
const char *p21_fault_r9_owner(void) {
    return g_p21.fault_r9_owner[0] ? g_p21.fault_r9_owner : "UNKNOWN";
}
uint32_t p21_slot_1914_last_writer_pc(void) { return g_p21.slot_writer_pc; }

static const char *classify_r9(uint32_t r9) {
    if (g_p21.gbrw_erw && r9 == g_p21.gbrw_erw) return "GBRWCORE";
    if (g_p21.gamelist_erw && r9 == g_p21.gamelist_erw) return "GAMELIST";
    if (r9 == 0x2B0D18u) return "GBRWCORE";
    if (r9 == 0x280400u) return "DSM_OR_SHARED";
    return "UNKNOWN";
}

void p21_runtime_isolation_on_module_map(const char *module_name, uint32_t base, uint32_t size) {
    (void)size;
    if (!p21_runtime_isolation_enabled() || !module_name || !base) return;
    if (strstr(module_name, "gbrwcore")) {
        g_p21.image_base = base;
        if (base == 0x2EB7E0u) g_p21.image_base = 0x2EB7FCu;
    }
}

#ifdef GWY_HAVE_UNICORN
static void p21_bp_cb(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    uint32_t regs[16];
    int i;
    static const int k_regs[16] = {
        UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3, UC_ARM_REG_R4,
        UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,  UC_ARM_REG_R8, UC_ARM_REG_R9,
        UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR,
        UC_ARM_REG_PC};
    (void)size;
    (void)user;
    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, k_regs[i], &regs[i]);
    p21_runtime_isolation_on_code(uc, (uint32_t)address, regs);
}

static const char *module_at_pc(uint32_t pc) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *gm =
        reg ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
    if (!gm) return "?";
    return gm->resolved_name[0] ? gm->resolved_name : gm->requested_name;
}

static void p21_slot_write_cb(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                              int64_t value, void *user) {
    uint32_t pc = 0, r9 = 0;
    uint32_t written = (uint32_t)(uint64_t)value;
    (void)type;
    (void)user;
    (void)size;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    g_p21.slot_write_n++;
    g_p21.slot_writer_pc = pc;
    snprintf(g_p21.slot_writer_mod, sizeof(g_p21.slot_writer_mod), "%s", module_at_pc(pc));
    if (!g_p21.slot_first_seen) {
        g_p21.slot_first_seen = 1;
        g_p21.slot_first_val = written;
    }
    printf("[P21_SLOT_1914_WRITE] n=%u addr=0x%X val=0x%X pc=0x%X r9=0x%X module=%s "
           "evidence=OBSERVED\n",
           g_p21.slot_write_n, (uint32_t)address, written, pc, r9, g_p21.slot_writer_mod);
    fflush(stdout);
}

static void arm_slot_watch(void *uc, uint32_t erw) {
    uc_hook h = 0;
    uint64_t a;
    if (!uc || !erw || g_p21.slot_watch_armed) return;
    a = (uint64_t)(erw + SLOT_1914_OFF);
    if (uc_hook_add((uc_engine *)uc, &h, UC_HOOK_MEM_WRITE, (void *)p21_slot_write_cb, NULL, a,
                    a + 3ull) == UC_ERR_OK) {
        g_p21.slot_watch_armed = 1;
        g_p21.slot_watch_addr = (uint32_t)a;
        g_p21.gbrw_erw = erw;
        printf("[P21_SLOT_WATCH] addr=0x%X erw=0x%X evidence=DOCUMENTED\n", (uint32_t)a, erw);
        fflush(stdout);
    }
}

static void arm_bp(void *uc, uint32_t addr, const char *tag) {
    uc_hook h = 0;
    uint64_t a = (uint64_t)(addr & ~1u);
    if (!uc || !addr) return;
    if (uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)p21_bp_cb, NULL, a, a + 2ull) ==
        UC_ERR_OK) {
        printf("[P21_BP_ARM] tag=%s va=0x%X evidence=DOCUMENTED\n", tag, addr);
        fflush(stdout);
    }
}
#endif

void p21_runtime_isolation_bind_uc(void *uc) {
    if (!p21_runtime_isolation_enabled() || !uc) return;
    g_p21.uc = uc;
#ifdef GWY_HAVE_UNICORN
    if (g_p21.hooks_armed) return;
    g_p21.hooks_armed = 1;
    arm_bp(uc, FAULT_FN, "fault_fn");
    arm_bp(uc, FAULT_ADD_R9, "r9_plus_190c");
    arm_bp(uc, FAULT_LOAD_1914, "load_1914");
    arm_bp(uc, FAULT_DEREF, "deref_r0_plus8");
    arm_bp(uc, 0x2AAD84u, "startgame_live");
    /* Known gbrwcore ER_RW from P20; also re-armed dynamically when R9 settles. */
    arm_slot_watch(uc, 0x2B0D18u);
#endif
}

void p21_runtime_isolation_on_code(void *uc, uint32_t pc, const uint32_t regs[16]) {
    uint32_t pn = pc & ~1u;
    if (!p21_runtime_isolation_enabled() || !regs) return;

    if (pn == (FAULT_FN & ~1u) || pn == (FAULT_ADD_R9 & ~1u) || pn == (FAULT_LOAD_1914 & ~1u) ||
        pn == (FAULT_DEREF & ~1u)) {
        uint32_t r9 = regs[9];
        uint32_t slot = 0;
        ExtChunkOwnerInfo oi;
        MrpRuntimeStack *st = mrp_runtime_stack_global();
        const char *owner = classify_r9(r9);
        if (pn == (FAULT_FN & ~1u)) g_p21.hit_fault_fn = 1;
        g_p21.fault_r9 = r9;
        g_p21.fault_r0 = regs[0];
        snprintf(g_p21.fault_r9_owner, sizeof(g_p21.fault_r9_owner), "%s", owner);
        if (strcmp(owner, "GBRWCORE") == 0) g_p21.callback_r9_parent = 1;
        if (r9 && uc)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, r9 + SLOT_1914_OFF, &slot);
        g_p21.slot_1914_val = slot;
        printf("[P21_FAULT_TRACE] pc=0x%X r0=0x%X r1=0x%X r6=0x%X r9=0x%X "
               "slot1914=0x%X FAULT_R9_OWNER=%s frame_depth=%d evidence=OBSERVED\n",
               pn, regs[0], regs[1], regs[6], r9, slot, owner, st ? st->depth : -1);
        fflush(stdout);
        if (pn == (FAULT_LOAD_1914 & ~1u)) {
            g_p21.hit_load_1914 = 1;
        }
        if (pn == (FAULT_DEREF & ~1u)) {
            g_p21.hit_deref = 1;
            if (regs[0] > 0xC0000000u || regs[0] < 0x1000u) g_p21.fault_seen = 1;
        }
        if (ext_chunk_provider_owner_for_helper(0x30CFE9u, &oi)) {
            g_p21.gbrw_p = oi.p_guest;
            g_p21.gbrw_erw = oi.registry_erw ? oi.registry_erw : oi.erw;
        }
        if (!g_p21.gbrw_erw && r9 == 0x2B0D18u) g_p21.gbrw_erw = r9;
#ifdef GWY_HAVE_UNICORN
        if (uc && g_p21.gbrw_erw) arm_slot_watch(uc, g_p21.gbrw_erw);
#endif
        (void)ext_chunk_provider_parent_p_sha(&g_p21.parent_sha_before, &g_p21.parent_sha_after);
        printf("[P21_FAULT_SLOT] visit_pc=0x%X slot1914=0x%X writer_pc=0x%X writer_mod=%s "
               "writes=%u evidence=OBSERVED\n",
               pn, slot, g_p21.slot_writer_pc,
               g_p21.slot_writer_mod[0] ? g_p21.slot_writer_mod : "NONE_YET", g_p21.slot_write_n);
        fflush(stdout);
    }

    /* Live startGame table entry from P20. */
    if (pn == 0x2AAD84u) {
        g_p21.startgame_enter = 1;
        printf("[P21_STARTGAME_ENTER] pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X "
               "evidence=OBSERVED\n",
               pn, regs[0], regs[1], regs[2], regs[3], regs[9]);
        fflush(stdout);
    }
}

void p21_runtime_isolation_finalize(const char *stop_reason) {
    FILE *f;
    ExtChunkOwnerInfo oi;
    if (!p21_runtime_isolation_enabled() || g_p21.finalized) return;
    g_p21.finalized = 1;

    if (ext_chunk_provider_owner_for_helper(0x30CFE9u, &oi)) {
        g_p21.gbrw_p = oi.p_guest;
        g_p21.gbrw_erw = oi.registry_erw ? oi.registry_erw : oi.erw;
    }
    {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        size_t i;
        if (reg) {
            for (i = 0; i < reg->count; i++) {
                const char *n = reg->modules[i].resolved_name[0] ? reg->modules[i].resolved_name
                                                                : reg->modules[i].requested_name;
                if (n && strstr(n, "gamelist")) {
                    ExtChunkOwnerInfo gi;
                    if (ext_chunk_provider_owner_for_helper(reg->modules[i].map.helper_address,
                                                           &gi)) {
                        g_p21.gamelist_p = gi.p_guest;
                        g_p21.gamelist_erw = gi.registry_erw ? gi.registry_erw : gi.erw;
                    }
                }
            }
        }
    }
    (void)ext_chunk_provider_parent_p_sha(&g_p21.parent_sha_before, &g_p21.parent_sha_after);

    f = fopen("reports/P21_RUNTIME_FRAME_ISOLATION.md", "w");
    if (f) {
        fprintf(f, "# P21 Runtime Frame Isolation\n\n");
        fprintf(f, "| Gate | Hit |\n|---|---|\n");
        fprintf(f, "| 1 gbrwcore P != gamelist P | %s (gbrw=0x%X gl=0x%X) |\n",
                p21_gate_p_isolated() ? "YES" : "NO", g_p21.gbrw_p, g_p21.gamelist_p);
        fprintf(f, "| 2 parent P intact | %s (sha before=0x%X after=0x%X) |\n",
                p21_gate_parent_p_intact() ? "YES" : "NO", g_p21.parent_sha_before,
                g_p21.parent_sha_after);
        fprintf(f, "| 3 callback R9 = parent | %s (owner=%s r9=0x%X) |\n",
                p21_gate_callback_r9_parent() ? "YES" : "NO", p21_fault_r9_owner(),
                g_p21.fault_r9);
        fprintf(f, "| 4 no 0x30D5D2 fault | %s |\n", p21_gate_no_30d5d2_fault() ? "YES" : "NO");
        fprintf(f, "| 5 startGame enter | %s |\n", p21_gate_startgame_enter() ? "YES" : "NO");
        fprintf(f, "\n## Fault dataflow\n\n");
        fprintf(f, "- FAULT_R9_OWNER = **%s**\n", p21_fault_r9_owner());
        fprintf(f, "- [R9+0x1914] = 0x%X\n", g_p21.slot_1914_val);
        fprintf(f, "- FAULT_SLOT_1914_LAST_WRITER = pc=0x%X module=%s writes=%u first_val=0x%X\n",
                g_p21.slot_writer_pc, g_p21.slot_writer_mod[0] ? g_p21.slot_writer_mod : "NONE",
                g_p21.slot_write_n, g_p21.slot_first_val);
        fprintf(f, "- stop=%s\n", stop_reason ? stop_reason : "?");
        fclose(f);
    }

    f = fopen("reports/P21_CALLBACK_SCOPE_TRACE.csv", "w");
    if (f) {
        fprintf(f, "event,pc,r9,r0,slot1914,owner,note\n");
        fprintf(f, "fault_trace,0x%X,0x%X,0x%X,0x%X,%s,observed\n", FAULT_DEREF, g_p21.fault_r9,
                g_p21.fault_r0, g_p21.slot_1914_val, p21_fault_r9_owner());
        fprintf(f, "slot_writer,0x%X,0x%X,0x%X,0x%X,%s,writes=%u\n", g_p21.slot_writer_pc,
                g_p21.fault_r9, g_p21.slot_first_val, g_p21.slot_1914_val,
                g_p21.slot_writer_mod[0] ? g_p21.slot_writer_mod : "NONE", g_p21.slot_write_n);
        fprintf(f, "parent_p,0,0x%X,0,0,gbrwcore,isolated=%d\n", g_p21.gbrw_p,
                p21_gate_p_isolated());
        fprintf(f, "child_p,0,0x%X,0,0,gamelist,isolated=%d\n", g_p21.gamelist_p,
                p21_gate_p_isolated());
        fclose(f);
    }

    printf("[P21_FINALIZE] stop=%s p_iso=%d parent_intact=%d cb_r9=%s no_fault=%d sg=%d "
           "evidence=OBSERVED\n",
           stop_reason ? stop_reason : "?", p21_gate_p_isolated(), p21_gate_parent_p_intact(),
           p21_fault_r9_owner(), p21_gate_no_30d5d2_fault(), p21_gate_startgame_enter());
    fflush(stdout);
}
