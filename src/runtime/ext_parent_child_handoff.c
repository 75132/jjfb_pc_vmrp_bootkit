#include "gwy_launcher/ext_parent_child_handoff.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/platform_handler_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

typedef struct PostAction {
    int used;
    char actor_module[64];
    uint32_t pc;
    char instruction[24];
    uint32_t target;
    uint32_t r0, r1, r2, r3;
    uint32_t event_id;
    uint32_t handler;
    uint32_t continuation;
    char kind[40];
    char detail[120];
} PostAction;

typedef struct StartGameRow {
    int used;
    char parent_module[64];
    char parent_package[96];
    char api[32];
    char call_kind[24];
    uint32_t call_pc;
    uint32_t continuation;
    uint32_t sp, lr, cpsr, r9;
    uint32_t r0, r1, r2, r3;
    char child_hint[96];
} StartGameRow;

static struct {
    int enabled;
    int enabled_known;
    void *uc;
    char out_dir[260];
    ShellChildLaunchFrame frame;
    int child_started;
    int child_returned;
    int32_t child_ret;
    char child_pkg[96];
    uint32_t handler_10140;
    int ack_10800;
    int first_action_seen;
    PostAction first;
    StartGameRow launches[8];
    int launch_n;
    uint32_t host_ticks_after_child;
    int idle_emitted;
    int startgame_body_seen;
} g_p19;

static int env1(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

int ext_parent_child_handoff_enabled(void) {
    if (g_p19.enabled_known) return g_p19.enabled;
    g_p19.enabled = env1("JJFB_P19_HANDOFF") || env1("GWY_P19_PARENT_CHILD_HANDOFF");
    g_p19.enabled_known = 1;
    return g_p19.enabled;
}

void ext_parent_child_handoff_reset(void) {
    memset(&g_p19, 0, sizeof(g_p19));
}

void ext_parent_child_handoff_bind_uc(void *uc) { g_p19.uc = uc; }

void ext_parent_child_handoff_set_out_dir(const char *dir) {
    snprintf(g_p19.out_dir, sizeof(g_p19.out_dir), "%s", dir ? dir : "");
}

const ShellChildLaunchFrame *ext_parent_child_handoff_frame(void) { return &g_p19.frame; }
int ext_parent_child_handoff_child_returned(void) { return g_p19.child_returned; }
int ext_parent_child_handoff_first_action_seen(void) { return g_p19.first_action_seen; }

static int path_has(const char *p, const char *needle) {
    return p && needle && strstr(p, needle) != NULL;
}

static void read_live_regs(void *uc, uint32_t regs[16], uint32_t *cpsr, uint32_t *sp, uint32_t *lr,
                           uint32_t *r9) {
    int i;
#ifdef GWY_HAVE_UNICORN
    static const int k[16] = {
        UC_ARM_REG_R0,  UC_ARM_REG_R1,  UC_ARM_REG_R2,  UC_ARM_REG_R3, UC_ARM_REG_R4,
        UC_ARM_REG_R5,  UC_ARM_REG_R6,  UC_ARM_REG_R7,  UC_ARM_REG_R8, UC_ARM_REG_R9,
        UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR,
        UC_ARM_REG_PC};
    if (!uc) return;
    for (i = 0; i < 16; i++) {
        regs[i] = 0;
        uc_reg_read((uc_engine *)uc, k[i], &regs[i]);
    }
    if (cpsr) uc_reg_read((uc_engine *)uc, UC_ARM_REG_CPSR, cpsr);
#else
    (void)uc;
    for (i = 0; i < 16; i++) regs[i] = 0;
    if (cpsr) *cpsr = 0;
#endif
    if (sp) *sp = regs[13];
    if (lr) *lr = regs[14];
    if (r9) *r9 = regs[9];
}

static void note_first_action(const char *kind, const char *actor, uint32_t pc, const char *insn,
                              uint32_t target, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                              uint32_t event_id, uint32_t handler, uint32_t cont,
                              const char *detail) {
    if (!ext_parent_child_handoff_enabled() || !g_p19.child_returned || g_p19.first_action_seen)
        return;
    g_p19.first_action_seen = 1;
    g_p19.first.used = 1;
    snprintf(g_p19.first.kind, sizeof(g_p19.first.kind), "%s", kind ? kind : "?");
    snprintf(g_p19.first.actor_module, sizeof(g_p19.first.actor_module), "%s", actor ? actor : "?");
    g_p19.first.pc = pc;
    snprintf(g_p19.first.instruction, sizeof(g_p19.first.instruction), "%s", insn ? insn : "-");
    g_p19.first.target = target;
    g_p19.first.r0 = r0;
    g_p19.first.r1 = r1;
    g_p19.first.r2 = r2;
    g_p19.first.r3 = r3;
    g_p19.first.event_id = event_id;
    g_p19.first.handler = handler;
    g_p19.first.continuation = cont;
    snprintf(g_p19.first.detail, sizeof(g_p19.first.detail), "%s", detail ? detail : "");
    printf("[POST_CHILD_FIRST_ACTION] kind=%s actor=%s pc=0x%X insn=%s target=0x%X "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X event=0x%X handler=0x%X cont=0x%X detail=%s "
           "evidence=OBSERVED\n",
           g_p19.first.kind, g_p19.first.actor_module, pc, g_p19.first.instruction, target, r0, r1,
           r2, r3, event_id, handler, cont, g_p19.first.detail);
    fflush(stdout);
}

void ext_parent_child_handoff_on_parent_launch(void *uc, const char *api_name, uint32_t call_pc,
                                              const uint32_t regs[16], uint32_t cpsr,
                                              const char *module_name, uint64_t module_id,
                                              const char *call_kind) {
    uint32_t live[16];
    uint32_t sp = 0, lr = 0, r9 = 0, cpsr_live = cpsr;
    StartGameRow *row;
    int i;
    if (!ext_parent_child_handoff_enabled()) return;
    if (regs) {
        memcpy(live, regs, sizeof(live));
    } else {
        memset(live, 0, sizeof(live));
        read_live_regs(uc ? uc : g_p19.uc, live, &cpsr_live, &sp, &lr, &r9);
    }
    sp = live[13];
    lr = live[14];
    r9 = live[9];

    if (g_p19.launch_n < (int)(sizeof(g_p19.launches) / sizeof(g_p19.launches[0]))) {
        row = &g_p19.launches[g_p19.launch_n++];
        memset(row, 0, sizeof(*row));
        row->used = 1;
        snprintf(row->parent_module, sizeof(row->parent_module), "%s",
                 module_name ? module_name : "?");
        snprintf(row->api, sizeof(row->api), "%s", api_name ? api_name : "?");
        snprintf(row->call_kind, sizeof(row->call_kind), "%s", call_kind ? call_kind : "?");
        row->call_pc = call_pc ? call_pc : live[15];
        row->continuation = lr;
        row->sp = sp;
        row->lr = lr;
        row->cpsr = cpsr_live;
        row->r9 = r9;
        row->r0 = live[0];
        row->r1 = live[1];
        row->r2 = live[2];
        row->r3 = live[3];
    }

    if (!g_p19.frame.valid || call_kind) {
        memset(&g_p19.frame, 0, sizeof(g_p19.frame));
        g_p19.frame.valid = 1;
        g_p19.frame.parent_pc = call_pc ? call_pc : live[15];
        g_p19.frame.parent_continuation = lr;
        g_p19.frame.parent_sp = sp;
        g_p19.frame.parent_lr = lr;
        g_p19.frame.parent_cpsr = cpsr_live;
        g_p19.frame.parent_r9 = r9;
        for (i = 0; i < 13; i++) g_p19.frame.parent_regs[i] = live[i];
        g_p19.frame.parent_module_id = module_id;
        snprintf(g_p19.frame.parent_module, sizeof(g_p19.frame.parent_module), "%s",
                 module_name ? module_name : "?");
        snprintf(g_p19.frame.launch_api, sizeof(g_p19.frame.launch_api), "%s",
                 api_name ? api_name : "?");
        snprintf(g_p19.frame.call_kind, sizeof(g_p19.frame.call_kind), "%s",
                 call_kind ? call_kind : "?");
    }

    printf("[PARENT_LAUNCH_ENTER] api=%s call_kind=%s module=%s module_id=%llu "
           "call_pc=0x%X cont=0x%X sp=0x%X lr=0x%X cpsr=0x%X r9=0x%X "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X evidence=OBSERVED\n",
           api_name ? api_name : "?", call_kind ? call_kind : "?", module_name ? module_name : "?",
           (unsigned long long)module_id, g_p19.frame.parent_pc, lr, sp, lr, cpsr_live, r9, live[0],
           live[1], live[2], live[3]);
    /* P20-CLEAN: stack window for live capsule (observe-only). */
    if (env1("JJFB_P20_CLEAN") && uc && sp) {
        uint8_t win[0xC0];
        uint32_t base = sp - 0x40u;
        int bi;
        memset(win, 0, sizeof(win));
#ifdef GWY_HAVE_UNICORN
        if (guest_memory_uc_peek((struct uc_struct *)uc, base, win, sizeof(win))) {
            printf("[PARENT_PRE_STARTGAME] stack_base=0x%X window=0xC0 hex=", base);
            for (bi = 0; bi < (int)sizeof(win); bi++) printf("%02X", win[bi]);
            printf(" evidence=OBSERVED\n");
        }
#else
        (void)base;
        (void)bi;
        (void)win;
#endif
    }
    fflush(stdout);
}

void ext_parent_child_handoff_on_start_dsm(const char *filename, const char *entry) {
    if (!ext_parent_child_handoff_enabled()) return;
    /* Filename decides package identity. Entry may be cfg36 param containing
     * "jjfb" while the package is still gbrwcore/gamelist — do NOT treat that
     * as child enter. */
    if (path_has(filename, "gbrwcore") || path_has(filename, "gamelist") ||
        path_has(filename, "gbrwshell")) {
        printf("[P19_START_DSM] package=%s entry=%s phase=shell_parent evidence=OBSERVED\n",
               filename ? filename : "?", entry ? entry : "?");
        fflush(stdout);
        return;
    }
    if (!path_has(filename, "jjfb")) {
        printf("[P19_START_DSM] package=%s entry=%s phase=non_jjfb evidence=OBSERVED\n",
               filename ? filename : "?", entry ? entry : "?");
        fflush(stdout);
        return;
    }
    g_p19.child_started = 1;
    g_p19.child_returned = 0;
    g_p19.first_action_seen = 0;
    memset(&g_p19.first, 0, sizeof(g_p19.first));
    snprintf(g_p19.child_pkg, sizeof(g_p19.child_pkg), "%s", filename ? filename : "gwy/jjfb.mrp");
    snprintf(g_p19.frame.child_package, sizeof(g_p19.frame.child_package), "%s", g_p19.child_pkg);
    printf("[P19_START_DSM] package=%s entry=%s phase=jjfb_child_enter evidence=OBSERVED\n",
           g_p19.child_pkg, entry ? entry : "?");
    fflush(stdout);
}

void ext_parent_child_handoff_on_child_init_return(void *uc, const char *filename, int32_t ret) {
    uint32_t regs[16];
    uint32_t cpsr = 0, sp = 0, lr = 0, r9 = 0;
    uint32_t h10140 = 0;
    if (!ext_parent_child_handoff_enabled()) return;
    /* Only JJFB package return is the child-init boundary. */
    if (!path_has(filename, "jjfb")) {
        printf("[P19_SHELL_PARENT_RETURN] package=%s ret=%d note=not_child_boundary "
               "evidence=OBSERVED\n",
               filename ? filename : "?", (int)ret);
        fflush(stdout);
        return;
    }

    g_p19.child_returned = 1;
    g_p19.child_ret = ret;
    g_p19.host_ticks_after_child = 0;
    g_p19.idle_emitted = 0;
    if (filename && filename[0])
        snprintf(g_p19.child_pkg, sizeof(g_p19.child_pkg), "%s", filename);

    memset(regs, 0, sizeof(regs));
    read_live_regs(uc ? uc : g_p19.uc, regs, &cpsr, &sp, &lr, &r9);
    h10140 = platform_handler_registry_get(0x10140u);
    g_p19.handler_10140 = h10140;
    g_p19.frame.child_er_rw = r9;
    g_p19.frame.child_entry = regs[15];

    printf("[CHILD_INIT_RETURN] package=%s ret=%d handler_10140=0x%X ack_10800=%d "
           "pc=0x%X lr=0x%X sp=0x%X r9=0x%X parent_cont=0x%X parent_module=%s "
           "frame_valid=%d evidence=OBSERVED\n",
           g_p19.child_pkg[0] ? g_p19.child_pkg : (filename ? filename : "?"), (int)ret, h10140,
           g_p19.ack_10800, regs[15], lr, sp, r9, g_p19.frame.parent_continuation,
           g_p19.frame.parent_module[0] ? g_p19.frame.parent_module : "?", g_p19.frame.valid);
    fflush(stdout);
    /* Partial CSV so kill-before-emu_exit still leaves an artifact. */
    ext_parent_child_handoff_flush();
}

static void try_decode_call_kind_from_lr(void *uc, uint32_t lr, char *out, size_t out_n) {
    uint32_t word = 0;
    uint16_t half = 0;
    if (!out || out_n < 4) return;
    out[0] = 0;
    if (!uc || !lr) return;
#ifdef GWY_HAVE_UNICORN
    /* Thumb: BL/BLX is 4 bytes ending at LR. */
    if ((lr & 1u) || (lr & 2u)) {
        uint32_t site = (lr & ~1u) - 4u;
        if (guest_memory_uc_peek((struct uc_struct *)uc, site, (uint8_t *)&half, 2) &&
            (half & 0xF800u) == 0xF000u) {
            snprintf(out, out_n, "BL");
            return;
        }
    }
    if (guest_memory_uc_peek_u32((struct uc_struct *)uc, (lr & ~3u) - 4u, &word)) {
        if ((word & 0x0F000000u) == 0x0B000000u) {
            snprintf(out, out_n, "BL");
            return;
        }
        if ((word & 0xFE000000u) == 0xFA000000u) {
            snprintf(out, out_n, "BLX");
            return;
        }
        if ((word & 0x0FFFFFF0u) == 0x012FFF10u) {
            snprintf(out, out_n, "BX");
            return;
        }
        if ((word & 0x0FFFFFF0u) == 0x012FFF30u) {
            snprintf(out, out_n, "BLX");
            return;
        }
    }
#else
    (void)uc;
    (void)lr;
    (void)word;
    (void)half;
#endif
}

void ext_parent_child_handoff_on_guest_code(void *uc, uint32_t pc, const uint32_t regs[16],
                                           uint32_t cpsr, const char *module_name,
                                           uint64_t module_id) {
    uint32_t h;
    char call_kind[24];
    if (!ext_parent_child_handoff_enabled() || !regs) return;

    /* Research-observed startGame body enter (TARGET_OBSERVED 0x2AAD84) — parent launch. */
    if (!g_p19.child_returned && !g_p19.startgame_body_seen && (pc & ~1u) == 0x2AAD84u) {
        g_p19.startgame_body_seen = 1;
        snprintf(call_kind, sizeof(call_kind), "%s", "ENTER_BODY");
        try_decode_call_kind_from_lr(uc, regs[14], call_kind, sizeof(call_kind));
        if (!call_kind[0]) snprintf(call_kind, sizeof(call_kind), "ENTER_BODY");
        ext_parent_child_handoff_on_parent_launch(uc, "lib.startGame", pc, regs, cpsr, module_name,
                                                 module_id, call_kind);
        return;
    }

    if (!g_p19.child_returned || g_p19.first_action_seen) return;

    h = g_p19.handler_10140 ? g_p19.handler_10140 : platform_handler_registry_get(0x10140u);
    if (h && ((pc & ~1u) == (h & ~1u))) {
        note_first_action("DIRECT_10140_ENTER", module_name, pc, "ENTER_HANDLER", h, regs[0],
                          regs[1], regs[2], regs[3], 0, h, regs[14], "guest_entered_10140_handler");
        return;
    }
    /* Parent continuation exact hit. */
    if (g_p19.frame.valid && g_p19.frame.parent_continuation &&
        (pc & ~1u) == (g_p19.frame.parent_continuation & ~1u)) {
        note_first_action("PARENT_CONTINUATION_RESUME", module_name, pc, "RESUME", 0, regs[0],
                          regs[1], regs[2], regs[3], 0, 0, g_p19.frame.parent_continuation,
                          "exact_parent_lr_after_child");
        return;
    }
    /* Parent module resume heuristic: known shell names executing after child return. */
    if (module_name && (strstr(module_name, "gamelist") || strstr(module_name, "gbrwcore") ||
                        strstr(module_name, "gbrwshell"))) {
        note_first_action("PARENT_MODULE_RESUME", module_name, pc, "CODE", 0, regs[0], regs[1],
                          regs[2], regs[3], 0, 0, g_p19.frame.parent_continuation,
                          "parent_or_shell_code_after_child");
        return;
    }
}

void ext_parent_child_handoff_on_plat(void *uc, uint32_t code, uint32_t app, uint32_t arg2,
                                     uint32_t arg3, uint32_t ret, uint32_t pc, uint32_t lr) {
    (void)uc;
    (void)ret;
    if (!ext_parent_child_handoff_enabled()) return;
    if (code == 0x10800u) g_p19.ack_10800 = 1;
    if (!g_p19.child_returned || g_p19.first_action_seen) return;

    if (code == 0x10140u) {
        note_first_action("PLAT_10140_REGISTER_AGAIN", "?", pc, "PLAT", 0x10140u, code, app, arg2,
                          arg3, 0, arg3 ? arg3 : arg2, lr, "re_register_after_child");
        return;
    }
    /* Family / activate-ish events after child return. */
    if (code == 0x1E209u || app == 0x1E209u || (code >= 0x1E200u && code <= 0x1E2FFu)) {
        note_first_action("PLATFORM_FAMILY_OR_ACTIVATE_EVENT", "?", pc, "PLAT", code, code, app,
                          arg2, arg3, code, platform_handler_registry_get(0x10102u), lr,
                          "post_child_plat_event");
        return;
    }
    if (code == 0x10102u || code == 0x10165u) {
        note_first_action("PLATFORM_HANDLER_REGISTER_POST_CHILD", "?", pc, "PLAT", code, code, app,
                          arg2, arg3, 0, arg2, lr, "post_child_register");
        return;
    }
}

void ext_parent_child_handoff_on_host_loop_tick(uint32_t t_ms) {
    (void)t_ms;
    if (!ext_parent_child_handoff_enabled() || !g_p19.child_returned || g_p19.first_action_seen)
        return;
    g_p19.host_ticks_after_child++;
    /* After ~2s of host loop with no Guest successor, emit idle gap once. */
    if (!g_p19.idle_emitted && g_p19.host_ticks_after_child >= 40u) {
        g_p19.idle_emitted = 1;
        note_first_action("NO_GUEST_SUCCESSOR_HOST_IDLE", "host_sdl_loop", 0, "HOST_TICK", 0, 0, 0,
                          0, 0, 0, g_p19.handler_10140, g_p19.frame.parent_continuation,
                          "start_dsm_return_treated_as_end_or_missing_activate");
        ext_parent_child_handoff_flush();
    }
}

void ext_parent_child_handoff_flush(void) {
    char path[320];
    FILE *f;
    int i;
    const char *dir = g_p19.out_dir[0] ? g_p19.out_dir : "out/p19";
    if (!ext_parent_child_handoff_enabled()) return;

    snprintf(path, sizeof(path), "%s/p19_startgame_call_trace.csv", dir);
    f = fopen(path, "wb");
    if (f) {
        fputs("seq,api,call_kind,parent_module,call_pc,continuation,sp,lr,cpsr,r9,r0,r1,r2,r3\n",
              f);
        for (i = 0; i < g_p19.launch_n; i++) {
            StartGameRow *r = &g_p19.launches[i];
            if (!r->used) continue;
            fprintf(f, "%d,%s,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n", i + 1,
                    r->api, r->call_kind, r->parent_module, r->call_pc, r->continuation, r->sp,
                    r->lr, r->cpsr, r->r9, r->r0, r->r1, r->r2, r->r3);
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/../reports/p19_startgame_call_trace.csv", dir);
    /* also write reports/ via absolute-ish fallback from runner */
    f = fopen("reports/p19_startgame_call_trace.csv", "wb");
    if (f) {
        fputs("seq,api,call_kind,parent_module,call_pc,continuation,sp,lr,cpsr,r9,r0,r1,r2,r3\n",
              f);
        for (i = 0; i < g_p19.launch_n; i++) {
            StartGameRow *r = &g_p19.launches[i];
            if (!r->used) continue;
            fprintf(f, "%d,%s,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n", i + 1,
                    r->api, r->call_kind, r->parent_module, r->call_pc, r->continuation, r->sp,
                    r->lr, r->cpsr, r->r9, r->r0, r->r1, r->r2, r->r3);
        }
        fclose(f);
    }

    f = fopen("reports/p19_post_child_first_action.csv", "wb");
    if (f) {
        fputs("seen,kind,actor_module,pc,instruction,target,r0,r1,r2,r3,event_id,handler,"
              "continuation,detail,child_ret,handler_10140,parent_cont,host_ticks\n",
              f);
        fprintf(f,
                "%d,%s,%s,0x%X,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\",%d,0x%X,0x%X,%u\n",
                g_p19.first_action_seen, g_p19.first.kind[0] ? g_p19.first.kind : "NONE",
                g_p19.first.actor_module[0] ? g_p19.first.actor_module : "-", g_p19.first.pc,
                g_p19.first.instruction[0] ? g_p19.first.instruction : "-", g_p19.first.target,
                g_p19.first.r0, g_p19.first.r1, g_p19.first.r2, g_p19.first.r3, g_p19.first.event_id,
                g_p19.first.handler, g_p19.first.continuation, g_p19.first.detail, (int)g_p19.child_ret,
                g_p19.handler_10140, g_p19.frame.parent_continuation, g_p19.host_ticks_after_child);
        fclose(f);
    }

    printf("[P19_HANDOFF_FLUSH] launches=%d child_returned=%d first_action=%d kind=%s "
           "evidence=OBSERVED\n",
           g_p19.launch_n, g_p19.child_returned, g_p19.first_action_seen,
           g_p19.first.kind[0] ? g_p19.first.kind : "NONE");
    fflush(stdout);
}
