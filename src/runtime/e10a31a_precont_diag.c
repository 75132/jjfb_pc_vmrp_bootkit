#include "gwy_launcher/e10a31a_precont_diag.h"

#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_gwy_shell_native_exec.h"
#include "gwy_launcher/ext_gwy_shell_shim.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct {
    int known;
    int enabled;
    int exit_trace;
    int tail_trace;
    unsigned long long run_id;
    uint32_t continue_n;
    uint32_t stop_n;
    uint32_t tail_n;
    int font_armed;
    int br_exit_seen;
    int first_stop_logged;
    char first_stop[64];
    char final_stop[64];
    FILE *continue_csv;
    FILE *stop_csv;
    FILE *tail_csv;
} g_a;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

static void parse(void) {
    if (g_a.known) return;
    g_a.enabled = env1("JJFB_E10A31A_MODE") || env1("JJFB_E10A31A_EXIT_TRACE") ||
                  env1("JJFB_E10A31A_TAIL_TRACE") || env1("JJFB_E10A31A_AB");
    g_a.exit_trace = env1("JJFB_E10A31A_EXIT_TRACE") || env1("JJFB_E10A31A_MODE");
    g_a.tail_trace = env1("JJFB_E10A31A_TAIL_TRACE") || env1("JJFB_E10A31A_MODE");
    {
        const char *rid = getenv("JJFB_E10A31A_RUN_ID");
        if (!rid || !rid[0]) rid = getenv("JJFB_E10A_RUN_ID");
        if (rid && rid[0]) g_a.run_id = strtoull(rid, NULL, 10);
    }
    g_a.known = 1;
}

int e10a31a_enabled(void) {
    parse();
    return g_a.enabled;
}

void e10a31a_reset(void) {
    if (g_a.continue_csv) fclose(g_a.continue_csv);
    if (g_a.stop_csv) fclose(g_a.stop_csv);
    if (g_a.tail_csv) fclose(g_a.tail_csv);
    memset(&g_a, 0, sizeof(g_a));
}

const char *e10a31a_continue_decision_name(GwyContinueDecision d) {
    switch (d) {
    case GWY_CONTINUE_READY: return "GWY_CONTINUE_READY";
    case GWY_CONTINUE_MODE_DISABLED: return "GWY_CONTINUE_MODE_DISABLED";
    case GWY_CONTINUE_ALREADY_CONSUMED: return "GWY_CONTINUE_ALREADY_CONSUMED";
    case GWY_CONTINUE_NOT_ARMED: return "GWY_CONTINUE_NOT_ARMED";
    case GWY_CONTINUE_TARGET_NOT_OBSERVED: return "GWY_CONTINUE_TARGET_NOT_OBSERVED";
    case GWY_CONTINUE_TARGET_MISSING: return "GWY_CONTINUE_TARGET_MISSING";
    case GWY_CONTINUE_PARAM_MISSING: return "GWY_CONTINUE_PARAM_MISSING";
    case GWY_CONTINUE_WRONG_ACTIVE_PACKAGE: return "GWY_CONTINUE_WRONG_ACTIVE_PACKAGE";
    case GWY_CONTINUE_GBRWCORE_NOT_READY: return "GWY_CONTINUE_GBRWCORE_NOT_READY";
    case GWY_CONTINUE_EXIT_OWNER_MISMATCH: return "GWY_CONTINUE_EXIT_OWNER_MISMATCH";
    case GWY_CONTINUE_STATE_RESET: return "GWY_CONTINUE_STATE_RESET";
    case GWY_CONTINUE_GAMELIST_ALREADY: return "GWY_CONTINUE_GAMELIST_ALREADY";
    default: return "GWY_CONTINUE_UNKNOWN_BLOCKER";
    }
}

static FILE *open_csv(const char *env_key, const char *fallback, const char *header) {
    const char *p = getenv(env_key);
    FILE *f;
    if (!p || !p[0]) p = fallback;
    f = fopen(p, "wb");
    if (f) {
        fputs(header, f);
        fflush(f);
    }
    return f;
}

static void ensure_continue_csv(void) {
    if (!g_a.continue_csv) {
        g_a.continue_csv =
            open_csv("JJFB_E10A31A_CONTINUE_CSV", "reports/e10a31a_continue_gate_trace.csv",
                     "run_id,seq,phase,decision,shell_mode,armed,target_observed,"
                     "gbrwcore_started,gbrwcore_first_pc,gbrwcore_exit_seen,already_continued,"
                     "gamelist_started,pc,lr,sp,r9,active_p,active_chunk,active_erw,"
                     "active_helper,package_id,module_id,active_package,active_module,"
                     "target,param,note\n");
    }
}

static void ensure_stop_csv(void) {
    if (!g_a.stop_csv) {
        g_a.stop_csv =
            open_csv("JJFB_E10A31A_STOP_CSV", "reports/e10a31a_runtime_stop_trace.csv",
                     "run_id,seq,timestamp_ms,source,reason,pc,lr,sp,r0,r1,r2,r3,r4,r5,r6,r7,"
                     "r8,r9,r10,r11,r12,package,module,helper,P,chunk,ERW,return_code,"
                     "first_cause,final_cause,detail\n");
    }
}

static void ensure_tail_csv(void) {
    if (!g_a.tail_csv) {
        g_a.tail_csv =
            open_csv("JJFB_E10A31A_TAIL_CSV", "reports/e10a31a_gbrwcore_tail_trace.csv",
                     "run_id,seq,pc,lr,sp,r0,r1,r2,r3,r9,instruction,call_target,target_owner,"
                     "platform_api,return_value,active_package,active_module,note\n");
    }
}

static void read_regs(void *uc, GwyContinueSnapshot *out) {
    int i;
    memset(out->r0_r12, 0, sizeof(out->r0_r12));
    out->pc = out->lr = out->sp = out->r9 = 0;
#ifdef GWY_HAVE_UNICORN
    if (!uc) return;
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &out->pc);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &out->lr);
    uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &out->sp);
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &out->r9);
    for (i = 0; i < 13; i++)
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0 + i, &out->r0_r12[i]);
#else
    (void)uc;
    (void)i;
#endif
}

void e10a31a_continue_snapshot_fill(void *uc, GwyContinueSnapshot *out) {
    const char *param;
    const char *tgt = "gwy/gamelist.mrp";
    ExtChunkOwnerInfo oi;
    ModuleRegistry *reg;
    const GwyLoadedModule *gm = NULL;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    read_regs(uc, out);

    out->shell_mode = ext_gwy_shell_shim_shell_core_continue_mode();
    out->gbrwcore_started = ext_gwy_shell_shim_gbrwcore_started_flag();
    if (!out->gbrwcore_started && ext_gwy_shell_native_exec_enabled())
        out->gbrwcore_started = ext_gwy_shell_native_exec_gbrwcore_started();
    out->gamelist_started = ext_gwy_shell_shim_gamelist_started_flag();
    if (!out->gamelist_started && ext_gwy_shell_native_exec_enabled())
        out->gamelist_started = ext_gwy_shell_native_exec_gamelist_started();
    out->already_continued = ext_gwy_shell_shim_continued_flag();
    out->gbrwcore_first_pc =
        ext_gwy_shell_native_exec_enabled() ? ext_gwy_shell_native_exec_gbrwcore_pc_hit() : 0;
    out->gbrwcore_exit_seen = g_a.br_exit_seen;
    out->armed = out->shell_mode && out->gbrwcore_started && !out->already_continued &&
                 !out->gamelist_started;
    out->target_observed = out->gbrwcore_started;

    snprintf(out->target, sizeof(out->target), "%s", tgt);
    param = ext_gwy_shell_shim_jjfb_param();
    snprintf(out->param, sizeof(out->param), "%s", param ? param : "");
    snprintf(out->active_package, sizeof(out->active_package), "%s",
             ext_gwy_shell_shim_active_package());

    out->active_p = ext_chunk_provider_last_p_guest();
    out->active_chunk = ext_chunk_provider_last_chunk_guest();
    if (out->active_chunk && ext_chunk_provider_owner_for_chunk(out->active_chunk, &oi)) {
        out->active_helper = oi.helper;
        out->active_erw = oi.erw;
        out->module_id = oi.module_id;
        snprintf(out->active_module, sizeof(out->active_module), "%.63s", oi.module_name);
    }
    if (!out->active_module[0]) {
        snprintf(out->active_module, sizeof(out->active_module), "%.63s",
                 ext_chunk_provider_last_module());
    }

    reg = gwy_ext_loader_bound_registry();
    if (reg && out->pc)
        gm = module_registry_find_by_code_addr(reg, out->pc & ~1u);
    if (gm) {
        out->package_id = gm->module_id;
        if (!out->module_id) out->module_id = gm->module_id;
        if (!out->active_module[0])
            snprintf(out->active_module, sizeof(out->active_module), "%.63s",
                     gm->resolved_name[0] ? gm->resolved_name : gm->requested_name);
    }

    /* Decision priority — diagnose, do not force. */
    if (!ext_gwy_shell_shim_enabled()) {
        out->decision = GWY_CONTINUE_MODE_DISABLED;
        snprintf(out->note, sizeof(out->note), "%s", "shim_disabled");
    } else if (!out->shell_mode) {
        out->decision = GWY_CONTINUE_MODE_DISABLED;
        snprintf(out->note, sizeof(out->note), "%s", "shell_core_continue_off");
    } else if (out->already_continued) {
        out->decision = GWY_CONTINUE_ALREADY_CONSUMED;
        snprintf(out->note, sizeof(out->note), "%s", "already_continued");
    } else if (out->gamelist_started) {
        out->decision = GWY_CONTINUE_GAMELIST_ALREADY;
        snprintf(out->note, sizeof(out->note), "%s", "gamelist_already");
    } else if (!out->gbrwcore_started) {
        out->decision = GWY_CONTINUE_GBRWCORE_NOT_READY;
        snprintf(out->note, sizeof(out->note), "%s", "gbrwcore_start_dsm_missing");
    } else if (!out->target[0]) {
        out->decision = GWY_CONTINUE_TARGET_MISSING;
        snprintf(out->note, sizeof(out->note), "%s", "target_empty");
    } else if (!out->param[0]) {
        out->decision = GWY_CONTINUE_PARAM_MISSING;
        snprintf(out->note, sizeof(out->note), "%s", "param_empty");
    } else if (!out->armed) {
        out->decision = GWY_CONTINUE_NOT_ARMED;
        snprintf(out->note, sizeof(out->note), "%s", "armed_predicate_false");
    } else {
        out->decision = GWY_CONTINUE_READY;
        snprintf(out->note, sizeof(out->note), "%s", "ready_to_continue");
    }
}

void e10a31a_log_continue_decision(const char *phase, const GwyContinueSnapshot *snap) {
    const char *dn;
    if (!snap) return;
    dn = e10a31a_continue_decision_name(snap->decision);
    printf("[GWY_CONTINUE_DECISION] phase=%s decision=%s shell_mode=%d armed=%d "
           "gbrwcore_started=%d gamelist_started=%d already=%d target=%s note=%s "
           "evidence=OBSERVED\n",
           phase ? phase : "?", dn, snap->shell_mode, snap->armed, snap->gbrwcore_started,
           snap->gamelist_started, snap->already_continued, snap->target, snap->note);
    fflush(stdout);
    if (!e10a31a_enabled() || !g_a.exit_trace) return;
    ensure_continue_csv();
    if (!g_a.continue_csv) return;
    g_a.continue_n++;
    fprintf(g_a.continue_csv,
            "%llu,%u,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
            "%llu,%llu,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
            g_a.run_id, g_a.continue_n, phase ? phase : "?", dn, snap->shell_mode, snap->armed,
            snap->target_observed, snap->gbrwcore_started, snap->gbrwcore_first_pc,
            snap->gbrwcore_exit_seen, snap->already_continued, snap->gamelist_started, snap->pc,
            snap->lr, snap->sp, snap->r9, snap->active_p, snap->active_chunk, snap->active_erw,
            snap->active_helper, (unsigned long long)snap->package_id,
            (unsigned long long)snap->module_id, snap->active_package, snap->active_module,
            snap->target, snap->param, snap->note);
    fflush(g_a.continue_csv);
}

void e10a31a_runtime_set_stop(const char *source, const char *reason, void *uc, uint32_t helper,
                              uint32_t p, uint32_t chunk, uint32_t erw, int32_t return_code,
                              const char *detail) {
    GwyContinueSnapshot snap;
    uint32_t now_ms;
    const char *pkg;
    const char *mod;
    parse();
    if (!e10a31a_enabled()) return;
    read_regs(uc, &snap);
    now_ms = (uint32_t)(clock() * 1000 / (CLOCKS_PER_SEC ? CLOCKS_PER_SEC : 1));
    pkg = ext_gwy_shell_shim_active_package();
    mod = snap.active_module[0] ? snap.active_module : ext_chunk_provider_last_module();

    if (!g_a.first_stop_logged) {
        g_a.first_stop_logged = 1;
        snprintf(g_a.first_stop, sizeof(g_a.first_stop), "%.63s", reason ? reason : "?");
    }
    snprintf(g_a.final_stop, sizeof(g_a.final_stop), "%.63s", reason ? reason : "?");

    printf("[JJFB_E10A31A_STOP] source=%s reason=%s pc=0x%X lr=0x%X first=%s final=%s "
           "detail=%s evidence=OBSERVED\n",
           source ? source : "?", reason ? reason : "?", snap.pc, snap.lr, g_a.first_stop,
           g_a.final_stop, detail ? detail : "");
    fflush(stdout);

    ensure_stop_csv();
    if (!g_a.stop_csv) return;
    g_a.stop_n++;
    fprintf(g_a.stop_csv,
            "%llu,%u,%u,%s,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
            "0x%X,0x%X,0x%X,0x%X,\"%s\",\"%s\",0x%X,0x%X,0x%X,0x%X,%d,\"%s\",\"%s\",\"%s\"\n",
            g_a.run_id, g_a.stop_n, now_ms, source ? source : "?", reason ? reason : "?", snap.pc,
            snap.lr, snap.sp, snap.r0_r12[0], snap.r0_r12[1], snap.r0_r12[2], snap.r0_r12[3],
            snap.r0_r12[4], snap.r0_r12[5], snap.r0_r12[6], snap.r0_r12[7], snap.r0_r12[8],
            snap.r9, snap.r0_r12[10], snap.r0_r12[11], snap.r0_r12[12], pkg ? pkg : "?",
            mod ? mod : "?", helper, p, chunk, erw, (int)return_code, g_a.first_stop,
            g_a.final_stop, detail ? detail : "");
    fflush(g_a.stop_csv);
}

void e10a31a_note_font_load_suc(void *uc) {
    (void)uc;
    if (!e10a31a_enabled()) return;
    g_a.font_armed = 1;
    printf("[JJFB_E10A31A] milestone=GBRWCORE_FONT_RETURN_REACHED evidence=OBSERVED\n");
    fflush(stdout);
    if (g_a.tail_trace) {
        ensure_tail_csv();
        if (g_a.tail_csv) {
            g_a.tail_n++;
            fprintf(g_a.tail_csv,
                    "%llu,%u,0,0,0,0,0,0,0,0,\"\",\"\",\"\",\"font_load_suc\",0,\"%s\",\"\","
                    "\"arm_tail\"\n",
                    g_a.run_id, g_a.tail_n, ext_gwy_shell_shim_active_package());
            fflush(g_a.tail_csv);
        }
    }
}

void e10a31a_on_guest_code(void *uc, uint32_t pc, uint32_t lr, const uint32_t regs[16],
                           const char *module) {
    if (!e10a31a_enabled() || !g_a.tail_trace || !g_a.font_armed) return;
    if (g_a.tail_n >= 1000u) return;
    ensure_tail_csv();
    if (!g_a.tail_csv) return;
    g_a.tail_n++;
    fprintf(g_a.tail_csv,
            "%llu,%u,0x%X,0x%X,0,0x%X,0x%X,0x%X,0x%X,0x%X,\"guest\",\"\",\"\",\"\",0,\"%s\","
            "\"%s\",\"post_font\"\n",
            g_a.run_id, g_a.tail_n, pc, lr, regs ? regs[0] : 0, regs ? regs[1] : 0,
            regs ? regs[2] : 0, regs ? regs[3] : 0, regs ? regs[9] : 0,
            ext_gwy_shell_shim_active_package(), module ? module : "?");
    fflush(g_a.tail_csv);
    (void)uc;
}

void e10a31a_note_platform_api(void *uc, const char *api, uint32_t pc, uint32_t lr, uint32_t r0,
                               int32_t ret) {
    if (!e10a31a_enabled() || !g_a.tail_trace || !g_a.font_armed) return;
    if (g_a.tail_n >= 1000u) return;
    ensure_tail_csv();
    if (!g_a.tail_csv) return;
    g_a.tail_n++;
    fprintf(g_a.tail_csv,
            "%llu,%u,0x%X,0x%X,0,0x%X,0,0,0,0,\"\",\"\",\"\",\"%s\",%d,\"%s\",\"\","
            "\"platform_api\"\n",
            g_a.run_id, g_a.tail_n, pc, lr, r0, api ? api : "?", (int)ret,
            ext_gwy_shell_shim_active_package());
    fflush(g_a.tail_csv);
    (void)uc;
}

void e10a31a_note_br_exit_enter(void *uc) {
    GwyContinueSnapshot snap;
    g_a.br_exit_seen = 1;
    printf("[GWY_BR_EXIT_ENTER] evidence=OBSERVED\n");
    fflush(stdout);
    e10a31a_continue_snapshot_fill(uc, &snap);
    e10a31a_log_continue_decision("br_exit_enter", &snap);
    printf("[JJFB_E10A31A] milestone=GBRWCORE_BR_EXIT_REACHED evidence=OBSERVED\n");
    fflush(stdout);
}

void e10a31a_note_br_exit_fallback(void *uc, const GwyContinueSnapshot *snap) {
    printf("[GWY_BR_EXIT_FALLBACK] decision=%s note=%s evidence=OBSERVED\n",
           snap ? e10a31a_continue_decision_name(snap->decision) : "?",
           snap ? snap->note : "");
    fflush(stdout);
    e10a31a_runtime_set_stop("br_exit", "GBRWCORE_MR_EXIT_FALLBACK", uc, 0, 0, 0, 0, 0,
                             snap ? snap->note : "fallback_exit");
    printf("[JJFB_E10A31A] milestone=GBRWCORE_EXIT_WITHOUT_CONTINUATION evidence=OBSERVED\n");
    fflush(stdout);
}

void e10a31a_note_br_exit_process_exit(int code) {
    printf("[GWY_BR_EXIT_PROCESS_EXIT] code=%d evidence=OBSERVED\n", code);
    fflush(stdout);
    e10a31a_runtime_set_stop("br_exit_process_exit", "GBRWCORE_MR_EXIT_FALLBACK", NULL, 0, 0, 0,
                             0, code, "exit_call");
}
