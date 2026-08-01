#include "gwy_launcher/p22k_post_m1_path.h"

#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define P22K_INSN_CAP 256
#define P22K_WINDOW 0x200u /* cover 0x89BF0 .. ~0x89DEC */

typedef struct {
    uint32_t seq;
    uint32_t method;
    uint32_t pc;
    uint32_t insn;
    uint32_t r0, r1, r5, r8, r9, sp;
    uint32_t sp64, sp68;
    char desc[48];
    char note[64];
} P22kInsn;

static struct {
    int armed;
    int finalized;
    int hook_installed;
    void *uc;
    uint32_t cont_pc;
    uint32_t window_end;
    uint32_t last_method;
    int32_t last_ret_r0;
    uint32_t m1_sp64, m1_sp68;
    int saw_cont;
    int saw_beq_taken;   /* BEQ → 0x89C14 path */
    int saw_fallthrough; /* non-zero sp64 → BL A5690 path */
    int saw_mov_r0_2;
    int saw_ldmfd;
    uint32_t ldmfd_return_pc;
    uint32_t bl_a5690_n, bl_a5724_n, bl_a5704_n;
    uint32_t insn_n;
    P22kInsn rows[P22K_INSN_CAP];
    char run_id[64];
    char sole_lock[160];
    char next_fix[160];
    char verdict_class[12];
    char stop_reason[80];
#ifdef GWY_HAVE_UNICORN
    uc_hook hook;
#endif
} g;

static const char *env_or(const char *k, const char *d) {
    const char *v = getenv(k);
    return (v && v[0]) ? v : d;
}

int p22k_enabled(void) {
    const char *e = getenv("JJFB_P22K_CLEAN");
    return e && e[0] == '1';
}

void p22k_reset(void) {
#ifdef GWY_HAVE_UNICORN
    uc_hook old = g.hook;
    void *uc = g.uc;
    int had = g.hook_installed;
#endif
    memset(&g, 0, sizeof(g));
#ifdef GWY_HAVE_UNICORN
    if (had && uc && old) (void)uc_hook_del((uc_engine *)uc, old);
#endif
}

static FILE *open_out(const char *envk, const char *defpath) {
    const char *p = env_or(envk, defpath);
    FILE *f = fopen(p, "wb");
    return f;
}

static void describe_arm(uint32_t w, uint32_t pc, char *out, size_t n) {
    int32_t imm;
    uint32_t tgt;
    if ((w & 0x0FFFFFF0u) == 0x012FFF30u) {
        snprintf(out, n, "BLX r%u", w & 0xFu);
        return;
    }
    if ((w & 0x0F000000u) == 0x0A000000u) {
        imm = (int32_t)(w & 0x00FFFFFFu);
        if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
        tgt = (uint32_t)((int32_t)pc + 8 + (imm << 2));
        snprintf(out, n, "B%s 0x%X", (w & 0xF0000000u) == 0x0A000000u ? "EQ" :
                                      (w & 0xF0000000u) == 0x1A000000u ? "NE" : "cond",
                 tgt);
        return;
    }
    if ((w & 0x0F000000u) == 0x0B000000u) {
        imm = (int32_t)(w & 0x00FFFFFFu);
        if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
        tgt = (uint32_t)((int32_t)pc + 8 + (imm << 2));
        snprintf(out, n, "BL 0x%X", tgt);
        return;
    }
    if ((w & 0x0FFFFFF0u) == 0x012FFF10u) {
        snprintf(out, n, "BX r%u", w & 0xFu);
        return;
    }
    if (w == 0xE1A05000u) {
        snprintf(out, n, "MOV r5,r0");
        return;
    }
    if (w == 0xE3A00002u) {
        snprintf(out, n, "MOV r0,#2");
        return;
    }
    if (w == 0xE8BD8DF0u) {
        snprintf(out, n, "LDMFD sp!,{r4-r11,pc}");
        return;
    }
    if ((w & 0xFFF00000u) == 0xE59D0000u) {
        snprintf(out, n, "LDR r%u,[sp,#0x%X]", (w >> 12) & 0xFu, w & 0xFFFu);
        return;
    }
    if ((w & 0xFFF00000u) == 0xE3510000u) {
        snprintf(out, n, "CMP r1,#0x%X", w & 0xFFu);
        return;
    }
    snprintf(out, n, "w=0x%08X", w);
}

static void emit_insn(uint32_t method, uint32_t pc, uint32_t insn, const uint32_t regs[16],
                      uint32_t sp, uint32_t sp64, uint32_t sp68, const char *note) {
    P22kInsn *r;
    if (g.insn_n >= P22K_INSN_CAP) return;
    r = &g.rows[g.insn_n++];
    memset(r, 0, sizeof(*r));
    r->seq = g.insn_n;
    r->method = method;
    r->pc = pc;
    r->insn = insn;
    r->r0 = regs[0];
    r->r1 = regs[1];
    r->r5 = regs[5];
    r->r8 = regs[8];
    r->r9 = regs[9];
    r->sp = sp;
    r->sp64 = sp64;
    r->sp68 = sp68;
    describe_arm(insn, pc, r->desc, sizeof(r->desc));
    snprintf(r->note, sizeof(r->note), "%s", note ? note : "");
    printf("[JJFB_P22K] pc=0x%X %s r0=0x%X r1=0x%X r5=0x%X sp64=0x%X sp68=0x%X method=%u "
           "note=%s evidence=OBSERVED\n",
           pc, r->desc, r->r0, r->r1, r->r5, sp64, sp68, method, r->note);
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static void p22k_on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t pc = (uint32_t)address;
    uint32_t regs[16];
    uint32_t cpsr = 0, sp = 0, insn = 0;
    uint32_t sp64 = 0, sp68 = 0;
    int i;
    (void)size;
    (void)user_data;
    if (!p22k_enabled() || g.finalized || !g.armed) return;
    if (pc < g.cont_pc || pc >= g.window_end) return;

    memset(regs, 0, sizeof(regs));
    for (i = 0; i < 16; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, pc, &insn);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x64u, &sp64);
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x68u, &sp68);

    if (pc == g.cont_pc) {
        g.saw_cont = 1;
        if (g.last_method == 1u) {
            g.m1_sp64 = sp64;
            g.m1_sp68 = sp68;
        }
        emit_insn(g.last_method, pc, insn, regs, sp, sp64, sp68, "continuation");
    } else if (insn == 0xE1A05000u) {
        emit_insn(g.last_method, pc, insn, regs, sp, sp64, sp68, "mov_r5_r0");
    } else if ((insn & 0x0FFFFFFF) == 0x0A000002u && (pc & ~3u) == ((g.cont_pc + 0x14u) & ~3u)) {
        /* BEQ +2 @ typically 0x89C04 */
        if (regs[1] == 0) g.saw_beq_taken = 1;
        else g.saw_fallthrough = 1;
        emit_insn(g.last_method, pc, insn, regs, sp, sp64, sp68,
                  regs[1] == 0 ? "beq_taken_zero_sp64" : "beq_not_taken");
    } else if (insn == 0xE3A00002u) {
        g.saw_mov_r0_2 = 1;
        emit_insn(g.last_method, pc, insn, regs, sp, sp64, sp68, "epilogue_r0_2");
    } else if (insn == 0xE8BD8DF0u) {
        uint32_t retpc = 0;
        g.saw_ldmfd = 1;
        /* After ADD sp,#0x70, return PC is last of {r4-r11,pc} at [sp+32]. */
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 32u, &retpc);
        g.ldmfd_return_pc = retpc;
        emit_insn(g.last_method, pc, insn, regs, sp, sp64, sp68, "function_epilogue");
        printf("[JJFB_P22K] ldmfd_return_pc=0x%X evidence=OBSERVED\n", retpc);
        fflush(stdout);
    } else if ((insn & 0xFF000000u) == 0xEB000000u) {
        int32_t imm = (int32_t)(insn & 0x00FFFFFFu);
        uint32_t tgt;
        if (imm & 0x00800000) imm |= (int32_t)0xFF000000u;
        tgt = (uint32_t)((int32_t)pc + 8 + (imm << 2));
        if (tgt == 0xA5690u || (g.cont_pc && tgt == g.cont_pc + 0x1BA7Cu)) g.bl_a5690_n++;
        if ((tgt & ~0xFFu) == 0xA5700u) g.bl_a5704_n++;
        if ((tgt & ~0xFFu) == 0xA5720u) g.bl_a5724_n++;
        emit_insn(g.last_method, pc, insn, regs, sp, sp64, sp68, "bl");
    } else if ((insn & 0x0F000000u) == 0x0A000000u || (insn & 0x0F000000u) == 0x0B000000u ||
               (insn & 0xFFF00000u) == 0xE59D0000u || (insn & 0xFFF0FFF0u) == 0xE3500000u ||
               (insn & 0x0FFFFFF0u) == 0x012FFF30u) {
        emit_insn(g.last_method, pc, insn, regs, sp, sp64, sp68, "ctrl");
    } else {
        /* rate-limit: only log first 8 misc + key window */
        if (g.insn_n < 48u) emit_insn(g.last_method, pc, insn, regs, sp, sp64, sp68, "step");
    }
    (void)cpsr;
}
#endif

static void install_hook(void *uc, uint32_t cont) {
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
    uint32_t end;
    if (!uc || !cont || g.hook_installed) return;
    end = cont + P22K_WINDOW;
    g.cont_pc = cont & ~3u;
    g.window_end = end;
    ue = uc_hook_add((uc_engine *)uc, &g.hook, UC_HOOK_CODE, (void *)p22k_on_code, NULL,
                     (uint64_t)g.cont_pc, (uint64_t)(end - 1u));
    if (ue == UC_ERR_OK) {
        g.hook_installed = 1;
        g.uc = uc;
        printf("[JJFB_P22K] sparse_hook=[0x%X,0x%X) evidence=DOCUMENTED note=post_m1_arm\n",
               g.cont_pc, end);
        fflush(stdout);
    }
#else
    (void)uc;
    (void)cont;
#endif
}

void p22k_bind_uc(void *uc) {
    if (!p22k_enabled()) return;
    g.uc = uc;
    {
        const char *rid = getenv("JJFB_P22K_RUN_ID");
        if (!rid || !rid[0]) rid = getenv("JJFB_P22I_RUN_ID");
        if (rid && rid[0]) snprintf(g.run_id, sizeof(g.run_id), "%s", rid);
    }
}

void p22k_note_dispatcher_continuation(void *uc, uint32_t continuation_pc, uint32_t method,
                                       int32_t return_r0, uint32_t sp) {
    if (!p22k_enabled() || g.finalized || !continuation_pc) return;
    if (!g.uc) g.uc = uc;
    g.armed = 1;
    g.last_method = method;
    g.last_ret_r0 = return_r0;
    install_hook(g.uc ? g.uc : uc, continuation_pc);
    printf("[JJFB_P22K] arm_continuation=0x%X method=%u r0=%d sp=0x%X evidence=OBSERVED\n",
           continuation_pc, method, (int)return_r0, sp);
    fflush(stdout);
    (void)sp;
}

void p22k_note_emu_leave(uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r9, const char *phase) {
    if (!p22k_enabled() || g.finalized || !g.armed) return;
    printf("[JJFB_P22K] emu_leave pc=0x%X lr=0x%X r0=0x%X r9=0x%X phase=%s evidence=OBSERVED\n",
           pc, lr, r0, r9, phase ? phase : "?");
    fflush(stdout);
    /* After m1, if we already saw epilogue or left without post-m1 hits, finalize soon. */
    if (g.last_method == 1u && (g.saw_ldmfd || g.saw_mov_r0_2 || g.saw_cont)) {
        /* defer finalize to explicit call from runner/timer; just note */
    }
}

static void classify(void) {
    if (g.saw_mov_r0_2 && g.saw_ldmfd) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "K1");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "dispatcher function epilogues with r0=2 after m1; no further init in this fn");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "trace parent resume after LDMFD return_pc=0x%X (who consumes r0=2)",
                 g.ldmfd_return_pc);
    } else if (g.saw_cont && !g.saw_mov_r0_2) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "K2");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "post-m1 continuation observed but epilogue r0=2 not hit (blocked mid-path)");
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "inspect last P22K insn before stall; check BL targets / platform returns");
    } else if (!g.saw_cont) {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "K3");
        snprintf(g.sole_lock, sizeof(g.sole_lock),
                 "post-m1 continuation 0x%X never executed under sparse hook (emu left earlier?)",
                 g.cont_pc);
        snprintf(g.next_fix, sizeof(g.next_fix),
                 "correlate UC_EMU_START_RETURN with implied return; widen hook or hook earlier");
    } else {
        snprintf(g.verdict_class, sizeof(g.verdict_class), "K0");
        snprintf(g.sole_lock, sizeof(g.sole_lock), "partial post-m1 observation");
        snprintf(g.next_fix, sizeof(g.next_fix), "re-run with longer slice");
    }
}

void p22k_finalize(const char *stop_reason) {
    FILE *csv;
    FILE *vd;
    uint32_t i;
    if (!p22k_enabled() || g.finalized) return;
    g.finalized = 1;
    if (stop_reason && stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s", stop_reason);
    else
        snprintf(g.stop_reason, sizeof(g.stop_reason), "finalize");
    classify();

    csv = open_out("JJFB_P22K_INSN_CSV", "reports/p22k/p22k_post_m1_insn.csv");
    if (csv) {
        fprintf(csv,
                "seq,method,pc,insn,desc,r0,r1,r5,r8,r9,sp,sp64,sp68,note\n");
        for (i = 0; i < g.insn_n; i++) {
            P22kInsn *r = &g.rows[i];
            fprintf(csv, "%u,%u,0x%X,0x%08X,\"%s\",0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\"\n",
                    r->seq, r->method, r->pc, r->insn, r->desc, r->r0, r->r1, r->r5, r->r8, r->r9,
                    r->sp, r->sp64, r->sp68, r->note);
        }
        fflush(csv);
        fclose(csv);
    }

    vd = open_out("JJFB_P22K_VERDICT", "reports/p22k/p22k_post_m1_verdict.md");
    if (vd) {
        fprintf(vd,
                "# P22K post-m1 path verdict\n\n"
                "## Bottom line\n\n**Class: %s**\n\n%s\n\n"
                "## Evidence\n\n"
                "- continuation=0x%X\n"
                "- saw_continuation=%d\n"
                "- m1_sp+0x64=0x%X m1_sp+0x68=0x%X\n"
                "- beq_taken(sp64==0)=%d fallthrough=%d\n"
                "- saw_mov_r0_2=%d saw_ldmfd=%d ldmfd_return_pc=0x%X\n"
                "- bl_hits A5690=%u A5724=%u A5704=%u\n"
                "- insn_rows=%u\n"
                "- stop_reason=%s\n\n"
                "## Next fix\n\n%s\n",
                g.verdict_class, g.sole_lock, g.cont_pc, g.saw_cont, g.m1_sp64, g.m1_sp68,
                g.saw_beq_taken, g.saw_fallthrough, g.saw_mov_r0_2, g.saw_ldmfd, g.ldmfd_return_pc,
                g.bl_a5690_n, g.bl_a5724_n, g.bl_a5704_n, g.insn_n, g.stop_reason, g.next_fix);
        fflush(vd);
        fclose(vd);
    }

    {
        FILE *sum = open_out("JJFB_P22K_SUMMARY", "out/p22k/p22k_runtime_summary.txt");
        if (sum) {
            fprintf(sum,
                    "run_id=%s\nclass=%s\ncont=0x%X\nsaw_cont=%d\nsp64=0x%X\nsp68=0x%X\n"
                    "beq_taken=%d\nfallthrough=%d\nmov_r0_2=%d\nldmfd=%d\nreturn_pc=0x%X\n"
                    "sole_lock=%s\nnext_fix=%s\nstop_reason=%s\ninsn_n=%u\n",
                    g.run_id, g.verdict_class, g.cont_pc, g.saw_cont, g.m1_sp64, g.m1_sp68,
                    g.saw_beq_taken, g.saw_fallthrough, g.saw_mov_r0_2, g.saw_ldmfd,
                    g.ldmfd_return_pc, g.sole_lock, g.next_fix, g.stop_reason, g.insn_n);
            fflush(sum);
            fclose(sum);
        }
    }

    printf("[JJFB_P22K_FINAL] class=%s cont=0x%X sp64=0x%X mov_r0_2=%d ldmfd=%d ret_pc=0x%X "
           "evidence=OBSERVED\n",
           g.verdict_class, g.cont_pc, g.m1_sp64, g.saw_mov_r0_2, g.saw_ldmfd, g.ldmfd_return_pc);
    fflush(stdout);
}
