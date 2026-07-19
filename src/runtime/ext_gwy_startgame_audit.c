#include "gwy_launcher/ext_gwy_startgame_audit.h"
#include "gwy_launcher/ext_p_extchunk_audit.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define SG_GATE_LINE                                                                              \
    "gwy_startgame_context_gate=%s p_extchunk_gate=blocked post_cfn_r9_gate=blocked "             \
    "post_continuation_gate=open graphics_gate=blocked event_scheduler_gate=blocked "             \
    "nested_r9_scope_gate=open module_r9_switch_gate=open guest_callback_frame_gate=blocked "     \
    "bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked"
#define SG_GATE_ARGS (g_sg.gate_open ? "open" : "blocked")

static struct {
    int enabled;
    int enabled_known;
    void *uc;
    int armed;
    int finalized;
    int gate_open;
    int launch_emitted;
    GwyStartgameClass last_class;
    char next_fix[88];
    char stop_reason[48];

    char resource_root[260];
    char target[96];
    char param[256];
    char profile[260];
    char overlay[260];

    int cfg36_ok;
    int start_dsm_seen;
    uint32_t start_dsm_count;
    int gbrwcore_opened;
    int gamelist_opened;
    int gbrwshell_opened;
    int vdload_opened;
    int fileopen_miss_critical;
    int strcom_601;
    int strcom_800;
    int strcom_801;
    int plat_seen;
    int testcom_seen;
    int mrc_init_seen;
    uint32_t p_guest;
    uint32_t p_plus_c_at_fault;
    int pxc_writes_seen;
    uint32_t fault_pc;
} g_sg;

const char *ext_gwy_startgame_class_name(GwyStartgameClass c) {
    switch (c) {
    case GWY_SG_SHELL_BYPASSED_DIRECT_JJFB: return "SHELL_BYPASSED_DIRECT_JJFB";
    case GWY_SG_SHELL_LOADED_BUT_NO_EXTCHUNK: return "SHELL_LOADED_BUT_NO_EXTCHUNK";
    case GWY_SG_PARAM_MISMATCH: return "PARAM_MISMATCH";
    case GWY_SG_RESOURCE_MISS_BLOCKS_CONTEXT: return "RESOURCE_MISS_BLOCKS_CONTEXT";
    default: return "UNKNOWN";
    }
}

GwyStartgameClass ext_gwy_startgame_audit_last_class(void) { return g_sg.last_class; }
int ext_gwy_startgame_context_gate_open(void) { return g_sg.gate_open; }

int ext_gwy_startgame_audit_enabled(void) {
    const char *e;
    if (g_sg.enabled_known) return g_sg.enabled;
    e = getenv("GWY_GWY_STARTGAME_AUDIT");
    if (e && e[0] == '1' && e[1] == '\0') {
        g_sg.enabled = 1;
    } else {
        const char *px = getenv("GWY_P_EXTCHUNK_AUDIT");
        g_sg.enabled = (px && px[0] == '1' && px[1] == '\0') ? 1 : 0;
    }
    g_sg.enabled_known = 1;
    return g_sg.enabled;
}

void ext_gwy_startgame_audit_reset(void) {
    memset(&g_sg, 0, sizeof(g_sg));
    snprintf(g_sg.next_fix, sizeof(g_sg.next_fix), "%s", "NONE");
    snprintf(g_sg.stop_reason, sizeof(g_sg.stop_reason), "%s", "UNKNOWN");
}

void ext_gwy_startgame_audit_bind_uc(void *uc) { g_sg.uc = uc; }

static void env_copy(char *dst, size_t n, const char *key) {
    const char *v = getenv(key);
    if (!v) v = "";
    snprintf(dst, n, "%s", v);
}

static int parse_cfg36_fields(const char *param, int *napptype, int *nextid, int *ncode, int *narg,
                              int *narg1, char *nmrp, size_t nmrp_n, int *gwyblink) {
    const char *p;
    if (!param) return 0;
    *napptype = *nextid = *ncode = *narg = *narg1 = 0;
    *gwyblink = 0;
    if (nmrp && nmrp_n) nmrp[0] = '\0';
    p = strstr(param, "napptype=");
    if (p) *napptype = atoi(p + 9);
    p = strstr(param, "nextid=");
    if (p) *nextid = atoi(p + 7);
    p = strstr(param, "ncode=");
    if (p) *ncode = atoi(p + 6);
    p = strstr(param, "narg=");
    if (p) *narg = atoi(p + 5);
    p = strstr(param, "narg1=");
    if (p) *narg1 = atoi(p + 6);
    p = strstr(param, "nmrpname=");
    if (p && nmrp && nmrp_n) {
        size_t i = 0;
        p += 9;
        while (p[i] && p[i] != '_' && i + 1 < nmrp_n) {
            nmrp[i] = p[i];
            i++;
        }
        nmrp[i] = '\0';
    }
    if (strstr(param, "gwyblink")) *gwyblink = 1;
    return 1;
}

void ext_gwy_startgame_audit_emit_launch_context(void) {
    int napptype = 0, nextid = 0, ncode = 0, narg = 0, narg1 = 0, gwyblink = 0;
    char nmrp[96];
    if (!ext_gwy_startgame_audit_enabled() || g_sg.launch_emitted) return;
    g_sg.launch_emitted = 1;
    env_copy(g_sg.resource_root, sizeof(g_sg.resource_root), "GWY_RESOURCE_ROOT");
    env_copy(g_sg.target, sizeof(g_sg.target), "GWY_LAUNCH_TARGET");
    env_copy(g_sg.param, sizeof(g_sg.param), "GWY_LAUNCH_PARAM");
    env_copy(g_sg.profile, sizeof(g_sg.profile), "GWY_PROFILE");
    env_copy(g_sg.overlay, sizeof(g_sg.overlay), "GWY_OVERLAY_ROOT");

    parse_cfg36_fields(g_sg.param, &napptype, &nextid, &ncode, &narg, &narg1, nmrp, sizeof(nmrp),
                       &gwyblink);
    g_sg.cfg36_ok = (napptype == 12 && nextid == 482 && ncode == 512 && narg == 0 && narg1 == 1 &&
                     gwyblink && (strstr(nmrp, "jjfb.mrp") != NULL));

    printf("[JJFB_LAUNCH_CONTEXT] resource_root=\"%s\" target=\"%s\" param=\"%s\" "
           "profile=\"%s\" overlay=\"%s\" shell_packages_loaded=pending " SG_GATE_LINE
           " evidence=OBSERVED\n",
           g_sg.resource_root, g_sg.target, g_sg.param, g_sg.profile, g_sg.overlay, SG_GATE_ARGS);
    printf("[JJFB_CFG36_CONTRACT] expected=napptype=12,nextid=482,ncode=512,narg=0,narg1=1,"
           "nmrpname=gwy/jjfb.mrp,gwyblink "
           "live=napptype=%d,nextid=%d,ncode=%d,narg=%d,narg1=%d,nmrpname=%s,gwyblink=%s "
           "match=%s consumer=LaunchDescriptor_env_to_bridge_dsm_mr_start_dsm " SG_GATE_LINE
           " evidence=DOCUMENTED\n",
           napptype, nextid, ncode, narg, narg1, nmrp[0] ? nmrp : "?", gwyblink ? "yes" : "no",
           g_sg.cfg36_ok ? "yes" : "no", SG_GATE_ARGS);
    fflush(stdout);
}

void ext_gwy_startgame_audit_on_start_dsm(const char *filename, const char *ext,
                                          const char *entry) {
    if (!ext_gwy_startgame_audit_enabled()) return;
    ext_gwy_startgame_audit_emit_launch_context();
    g_sg.start_dsm_seen = 1;
    g_sg.start_dsm_count++;
    g_sg.armed = 1;
    printf("[JJFB_STARTGAME_EQUIV] layer=bridge_dsm_mr_start_dsm filename=\"%s\" ext=\"%s\" "
           "entry=\"%s\" count=%u evidence=DOCUMENTED_lowest_layer_not_full_shell " SG_GATE_LINE
           "\n",
           filename ? filename : "?", ext ? ext : "?", entry ? entry : "(null)",
           g_sg.start_dsm_count, SG_GATE_ARGS);
    fflush(stdout);
}

static int path_has(const char *guest, const char *needle) {
    return guest && needle && strstr(guest, needle) != NULL;
}

void ext_gwy_startgame_audit_on_file_open(const char *guest_path, int ok) {
    if (!ext_gwy_startgame_audit_enabled()) return;
    ext_gwy_startgame_audit_emit_launch_context();
    if (!guest_path) return;
    if (path_has(guest_path, "gbrwcore")) {
        if (ok) g_sg.gbrwcore_opened = 1;
        else g_sg.fileopen_miss_critical = 1;
    }
    if (path_has(guest_path, "gamelist")) {
        if (ok) g_sg.gamelist_opened = 1;
        else g_sg.fileopen_miss_critical = 1;
    }
    if (path_has(guest_path, "gbrwshell")) {
        if (ok) g_sg.gbrwshell_opened = 1;
    }
    if (path_has(guest_path, "vdload")) {
        if (ok) g_sg.vdload_opened = 1;
    }
    if (!ok && path_has(guest_path, "cfg.bin"))
        g_sg.fileopen_miss_critical = 1;
}

void ext_gwy_startgame_audit_on_plat_or_testcom(void *uc, const char *api_name, uint32_t slot) {
    uint32_t r0 = 0, r1 = 0, r2 = 0;
    if (!ext_gwy_startgame_audit_enabled()) return;
    (void)slot;
#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0, &r0);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R1, &r1);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R2, &r2);
    }
#else
    (void)uc;
#endif
    if (api_name && (strstr(api_name, "TestCom") || strstr(api_name, "testcom"))) {
        g_sg.testcom_seen = 1;
        if (r1 == 601 || r2 == 601 || r0 == 601) g_sg.strcom_601 = 1;
        if (r1 == 800 || r2 == 800 || r0 == 800) g_sg.strcom_800 = 1;
        if (r1 == 801 || r2 == 801 || r0 == 801) g_sg.strcom_801 = 1;
        printf("[JJFB_STRCOM] api=%s r0=%u r1=%u r2=%u " SG_GATE_LINE " evidence=OBSERVED\n",
               api_name, r0, r1, r2, SG_GATE_ARGS);
    } else if (api_name && (strstr(api_name, "plat") || strstr(api_name, "Plat"))) {
        g_sg.plat_seen = 1;
        if (r0 == 601 || r1 == 601) g_sg.strcom_601 = 1;
        if (r0 == 800 || r1 == 800) g_sg.strcom_800 = 1;
        if (r0 == 801 || r1 == 801) g_sg.strcom_801 = 1;
        printf("[JJFB_MR_PLAT] api=%s code=%u arg=%u " SG_GATE_LINE " evidence=OBSERVED\n",
               api_name, r0, r1, SG_GATE_ARGS);
    }
    fflush(stdout);
}

void ext_gwy_startgame_audit_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len) {
    (void)helper;
    (void)p_len;
    if (!ext_gwy_startgame_audit_enabled()) return;
    ext_gwy_startgame_audit_emit_launch_context();
    if (p_guest) g_sg.p_guest = p_guest;
}

void ext_gwy_startgame_audit_on_continuation_resume(void *uc, uint64_t module_id, const char *module,
                                                    uint32_t continuation_pc, const uint32_t regs[16],
                                                    uint32_t cpsr) {
    (void)module_id;
    (void)regs;
    (void)cpsr;
    if (!ext_gwy_startgame_audit_enabled()) return;
    g_sg.uc = uc ? uc : g_sg.uc;
    g_sg.armed = 1;
    printf("[JJFB_GWY_CONTEXT_CONT] module=%s cont_pc=0x%X " SG_GATE_LINE " evidence=OBSERVED\n",
           module ? module : "?", continuation_pc, SG_GATE_ARGS);
    fflush(stdout);
}

static void classify_and_summary(const char *stop) {
    GwyStartgameClass c = GWY_SG_CLASS_UNKNOWN;
    const char *fix = "NONE";
    int shell_any;
    if (g_sg.finalized) return;
    g_sg.finalized = 1;
    if (stop && stop[0]) snprintf(g_sg.stop_reason, sizeof(g_sg.stop_reason), "%s", stop);

    shell_any = g_sg.gbrwcore_opened || g_sg.gamelist_opened || g_sg.gbrwshell_opened;

    printf("[JJFB_SHELL_BYPASS] gbrwcore_opened=%s gamelist_opened=%s gbrwshell_opened=%s "
           "vdload_opened=%s startGame_string_exec=no start_dsm_seen=%s " SG_GATE_LINE
           " evidence=OBSERVED\n",
           g_sg.gbrwcore_opened ? "yes" : "no", g_sg.gamelist_opened ? "yes" : "no",
           g_sg.gbrwshell_opened ? "yes" : "no", g_sg.vdload_opened ? "yes" : "no",
           g_sg.start_dsm_seen ? "yes" : "no", SG_GATE_ARGS);

    if (!g_sg.strcom_601 && !g_sg.strcom_800 && !g_sg.strcom_801) {
        printf("[JJFB_STRCOM] strcom_601_800_801=not_observed testcom_seen=%s plat_seen=%s "
               SG_GATE_LINE " evidence=OBSERVED\n",
               g_sg.testcom_seen ? "yes" : "no", g_sg.plat_seen ? "yes" : "no", SG_GATE_ARGS);
    }

    printf("[JJFB_MRC_INIT_GAP] mrc_init_seen=%s note=%s " SG_GATE_LINE " evidence=TARGET_OBSERVED\n",
           g_sg.mrc_init_seen ? "yes" : "no",
           g_sg.mrc_init_seen
               ? "observed"
               : "fault_or_stop_before_mrc_init_or_path_never_entered",
           SG_GATE_ARGS);

    if (!g_sg.cfg36_ok) {
        c = GWY_SG_PARAM_MISMATCH;
        fix = "FIX_CFG36_PARAM_PIPELINE";
    } else if (g_sg.fileopen_miss_critical && !shell_any) {
        c = GWY_SG_RESOURCE_MISS_BLOCKS_CONTEXT;
        fix = "FIX_RESOURCE_ROOT_OR_PATH";
    } else if (shell_any && g_sg.pxc_writes_seen == 0) {
        c = GWY_SG_SHELL_LOADED_BUT_NO_EXTCHUNK;
        fix = "SHELL_PUBLICATION_ROUTINE_AUDIT";
    } else if (!shell_any && g_sg.start_dsm_seen && g_sg.pxc_writes_seen == 0) {
        c = GWY_SG_SHELL_BYPASSED_DIRECT_JJFB;
        fix = "RESTORE_GWY_STARTGAME_RUNAPP_CONTEXT";
    } else if (!shell_any && g_sg.start_dsm_seen) {
        c = GWY_SG_SHELL_BYPASSED_DIRECT_JJFB;
        fix = "RESTORE_GWY_STARTGAME_RUNAPP_CONTEXT";
    } else {
        c = GWY_SG_CLASS_UNKNOWN;
        fix = "NONE";
    }

    g_sg.last_class = c;
    g_sg.gate_open = 0;
    snprintf(g_sg.next_fix, sizeof(g_sg.next_fix), "%s", fix);

    printf("[JJFB_GWY_CONTEXT_SUMMARY] gwy_context_class=%s shell_bypassed=%s "
           "start_dsm_count=%u pxc_writes_seen=%u p_plus_c_at_fault=0x%X cfg36_match=%s "
           "fileopen_miss_critical=%s next_allowed_fix=%s stop_reason=%s fault_pc=0x%X "
           SG_GATE_LINE " evidence=OBSERVED note=observe_only\n",
           ext_gwy_startgame_class_name(c), shell_any ? "no" : "yes", g_sg.start_dsm_count,
           g_sg.pxc_writes_seen, g_sg.p_plus_c_at_fault, g_sg.cfg36_ok ? "yes" : "no",
           g_sg.fileopen_miss_critical ? "yes" : "no", g_sg.next_fix, g_sg.stop_reason,
           g_sg.fault_pc, SG_GATE_ARGS);
    fflush(stdout);
}

void ext_gwy_startgame_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                          const uint32_t regs[16], uint32_t cpsr) {
    uint32_t chunk = 0;
    (void)fault_addr;
    (void)regs;
    (void)cpsr;
    if (!ext_gwy_startgame_audit_enabled() || g_sg.finalized) return;
    g_sg.uc = uc ? uc : g_sg.uc;
    g_sg.armed = 1;
    g_sg.fault_pc = fault_pc;
    if (g_sg.p_guest && g_sg.uc) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)g_sg.uc, g_sg.p_guest + 0x0Cu, &chunk);
        g_sg.p_plus_c_at_fault = chunk;
    }
    /* Prefer live EXTCHUNK summary if available via class; writes stay 0 unless we saw shell. */
    if (ext_p_extchunk_audit_last_class() == EXTCHUNK_NEVER_WRITTEN ||
        ext_p_extchunk_audit_last_class() == EXTCHUNK_GWY_CONTEXT_HYPOTHESIS)
        g_sg.pxc_writes_seen = 0;
    classify_and_summary("NEW_FAULT");
}

void ext_gwy_startgame_audit_finalize(const char *stop_reason) {
    if (!ext_gwy_startgame_audit_enabled()) return;
    if (g_sg.finalized) return;
    classify_and_summary(stop_reason ? stop_reason : "EXPLICIT_STOP");
}
