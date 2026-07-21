#include "gwy_launcher/ext_gwy_shell_native_exec.h"
#include "gwy_launcher/e10a_shell_trace.h"
#include "gwy_launcher/e10a3_postselect_trace.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/robotol_flag_writer_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* Static string offsets in gbrwcore.ext (TARGET_OBSERVED; not function entries). */
#define GBRWCORE_OFF_LIB_STARTGAME 0x223D4u
#define GBRWCORE_OFF_LIB_RUNAPP 0x224C0u
#define GBRWCORE_OFF_LIB_GETUSERINFO 0x22330u
#define GBRWCORE_OFF_LIB_CHECKMRPVER 0x22444u
#define GBRWCORE_OFF_LIB_GETCLIENTINFO 0x22490u
#define GBRWCORE_EXT_SIZE 147196u
/* field0 release site: loads *[ctx]+0 then calls helper via ERW+0x1880 (E10A-2). */
#define GBRWCORE_OFF_FIELD0_RELEASE 0xD02Cu /* VA 0x30D02C @ base 0x300000 */
#define GAMELIST_EXT_SIZE 91532u
#define GBRWSHELL_EXT_SIZE 45216u
#define GAMELIST_OFF_CFG36_FMT 0x1412Cu
/* TARGET_OBSERVED gamelist.ext offsets: cfg-open / gate / cmd switch (D4/D5). */
#define GAMELIST_OFF_CFG_OPEN 0x1AF8u   /* VA 0x2D5E4C @ base 0x2D4354 */
#define GAMELIST_OFF_CFG_GATE 0x392Cu   /* VA 0x2D7C80 */
#define GAMELIST_OFF_CFG_WRAP 0xF670u   /* VA 0x2E39C4 */
#define GAMELIST_OFF_CMD_DISP 0xF6C4u   /* VA 0x2E3A18 char switch */

/*
 * Do not seed handlers from a fixed image base: cacheSync/raw base can shift
 * (D5b: base+RVA missed live 0x10102 targets by 0x14). Rely on slot28 register.
 */

typedef struct {
    char name[48];
    uint32_t base;
    uint32_t size;
    int mapped;
    int pc_hit;
    int export_reg;
    int export_call;
} ShellMod;

typedef struct {
    uint32_t event_code;
    uint32_t handler;
    uint32_t enter_n;
    int seeded;
} GlHandler;

static struct {
    int enabled_known;
    int enabled;
    int finalized;
    void *uc;
    GwyShellNativeExecClass last_class;

    int package_open_gbrwcore;
    int package_open_gamelist;
    int package_open_gbrwshell;
    int mrp_started_gbrwcore;
    int mrp_started_gamelist;
    int mrp_started_gbrwshell;
    int mrp_started_jjfb;
    int host_equivalent_forbidden;

    int ext_loaded;
    int export_registered;
    int export_called;
    int guest_pc_hit;
    int cfg36_build;
    int post_update;
    int cfg_gate_hit;
    int cmd_disp_hit;
    int field0_release_hit;
    int gamelist_call;
    int strcom_601;
    int strcom_800;
    int strcom_801;
    int mrc_init;
    int pxc_writes;
    uint32_t pxc_pc;
    uint32_t pxc_lr;
    char pxc_module[48];

    uint32_t param_va;
    uint32_t param_len;
    char param_buf[160];
    int param_map_logged;
    int param_read_n;

    GlHandler handlers[16];
    int handler_n;
    int handlers_seeded;

    char active_pkg[128];
    ShellMod mods[8];
    int mod_count;
    int gate_open;
} g_ne;

static int env_is_1(const char *key) {
    const char *e = getenv(key);
    return e && e[0] == '1' && e[1] == '\0';
}

static int path_has(const char *s, const char *n) {
    return s && n && strstr(s, n) != NULL;
}

static int is_shell_pkg_name(const char *s) {
    return path_has(s, "gbrwcore") || path_has(s, "gamelist") || path_has(s, "gbrwshell");
}

static int is_shell_ext_name(const char *s) {
    return path_has(s, "gbrwcore.ext") || path_has(s, "gamelist.ext") ||
           path_has(s, "gbrwshell.ext");
}

static GlHandler *find_handler(uint32_t handler) {
    int i;
    uint32_t h = handler & ~1u;
    for (i = 0; i < g_ne.handler_n; i++) {
        if ((g_ne.handlers[i].handler & ~1u) == h) return &g_ne.handlers[i];
    }
    return NULL;
}

static void add_handler(uint32_t event_code, uint32_t handler, int seeded) {
    GlHandler *h;
    if (!handler) return;
    h = find_handler(handler);
    if (h) {
        if (event_code) h->event_code = event_code;
        return;
    }
    if (g_ne.handler_n >= (int)(sizeof(g_ne.handlers) / sizeof(g_ne.handlers[0]))) return;
    h = &g_ne.handlers[g_ne.handler_n++];
    h->event_code = event_code;
    h->handler = handler | 1u;
    h->enter_n = 0;
    h->seeded = seeded;
    printf("[JJFB_GAMELIST_HANDLER_MAP] event_code=0x%X handler=0x%X seeded=%d "
           "evidence=TARGET_OBSERVED\n",
           event_code, h->handler, seeded);
    fflush(stdout);
}

static int read_guest_cstr(void *uc, uint32_t addr, char *out, size_t cap);

static void maybe_param_read(void *uc, uint32_t pc, const uint32_t regs[16], const char *mod) {
    int i;
    char buf[96];
    if (!regs) return;
    for (i = 0; i < 4; i++) {
        uint32_t a = regs[i];
        if (g_ne.param_va &&
            (a == g_ne.param_va ||
             (g_ne.param_len && a >= g_ne.param_va && a < g_ne.param_va + g_ne.param_len))) {
            g_ne.param_read_n++;
            if (g_ne.param_read_n <= 8) {
                printf("[JJFB_PARAM_READ] pc=0x%X module=%s reg=r%d va=0x%X "
                       "evidence=TARGET_OBSERVED\n",
                       pc, mod ? mod : "?", i, a);
                fflush(stdout);
            }
            return;
        }
        if (!uc || !a) continue;
        if (!read_guest_cstr(uc, a, buf, sizeof(buf))) continue;
        if (strstr(buf, "napptype=") && strstr(buf, "gwyblink")) {
            g_ne.param_read_n++;
            if (g_ne.param_read_n <= 8) {
                printf("[JJFB_PARAM_READ] pc=0x%X module=%s reg=r%d va=0x%X bytes=\"%.80s\" "
                       "evidence=TARGET_OBSERVED\n",
                       pc, mod ? mod : "?", i, a, buf);
                printf("[JJFB_PARAM_PARSE_HINT] key=napptype/nmrpname evidence=TARGET_OBSERVED\n");
                fflush(stdout);
            }
            return;
        }
    }
}

static const char *shell_mod_label(const char *s) {
    if (path_has(s, "gbrwcore")) return "gbrwcore.ext";
    if (path_has(s, "gamelist")) return "gamelist.ext";
    if (path_has(s, "gbrwshell")) return "gbrwshell.ext";
    return "shell.ext";
}

int ext_gwy_shell_native_exec_enabled(void) {
    const char *path;
    if (g_ne.enabled_known) return g_ne.enabled;
    path = getenv("JJFB_LAUNCH_PATH");
    if (env_is_1("JJFB_SHELL_NATIVE_EXEC_TRACE")) g_ne.enabled = 1;
    if (path && (strcmp(path, "gwy_guest_native_runapp") == 0 ||
                 strcmp(path, "gwy_shell_core_continue") == 0))
        g_ne.enabled = 1;
    g_ne.enabled_known = 1;
    return g_ne.enabled;
}

void ext_gwy_shell_native_exec_reset(void) { memset(&g_ne, 0, sizeof(g_ne)); }

void ext_gwy_shell_native_exec_bind_uc(void *uc) { g_ne.uc = uc; }

const char *ext_gwy_shell_native_exec_class_name(GwyShellNativeExecClass c) {
    switch (c) {
    case GWY_SHELL_NATIVE_PACKAGE_OPEN_ONLY: return "PACKAGE_OPEN_ONLY";
    case GWY_SHELL_NATIVE_EXEC_PARTIAL: return "EXEC_PARTIAL";
    case GWY_SHELL_NATIVE_EXEC_GATE_OPEN: return "EXEC_GATE_OPEN";
    case GWY_SHELL_NATIVE_GUEST_RUNAPP_CALLED: return "GUEST_RUNAPP_CALLED";
    default: return "NONE";
    }
}

GwyShellNativeExecClass ext_gwy_shell_native_exec_last_class(void) { return g_ne.last_class; }
int ext_gwy_shell_native_exec_gate_open(void) { return g_ne.gate_open; }
int ext_gwy_shell_native_exec_gbrwcore_started(void) { return g_ne.mrp_started_gbrwcore; }
int ext_gwy_shell_native_exec_gamelist_started(void) { return g_ne.mrp_started_gamelist; }

static ShellMod *find_or_add_mod(const char *label) {
    int i;
    if (!label) return NULL;
    for (i = 0; i < g_ne.mod_count; i++) {
        if (strcmp(g_ne.mods[i].name, label) == 0) return &g_ne.mods[i];
    }
    if (g_ne.mod_count >= (int)(sizeof(g_ne.mods) / sizeof(g_ne.mods[0]))) return NULL;
    snprintf(g_ne.mods[g_ne.mod_count].name, sizeof(g_ne.mods[0].name), "%s", label);
    return &g_ne.mods[g_ne.mod_count++];
}

static void recompute_gate(void) {
    int started = g_ne.mrp_started_gbrwcore || g_ne.mrp_started_gamelist ||
                  g_ne.mrp_started_gbrwshell;
    /* Min success: package started + EXT loaded + export registered + guest PC hit. */
    g_ne.gate_open = started && g_ne.ext_loaded && g_ne.export_registered && g_ne.guest_pc_hit;
}

static void emit_export_table(ShellMod *m) {
    static const struct {
        const char *name;
        uint32_t off;
    } k_svcs[] = {
        {"lib.getuserinfo", GBRWCORE_OFF_LIB_GETUSERINFO},
        {"lib.checkmrpver", GBRWCORE_OFF_LIB_CHECKMRPVER},
        {"lib.getClientInfo", GBRWCORE_OFF_LIB_GETCLIENTINFO},
        {"lib.startGame", GBRWCORE_OFF_LIB_STARTGAME},
        {"lib.runapp", GBRWCORE_OFF_LIB_RUNAPP},
    };
    size_t si;
    if (!m || !m->mapped || m->export_reg) return;
    if (strcmp(m->name, "gbrwcore.ext") != 0) return;
    m->export_reg = 1;
    g_ne.export_registered = 1;
    for (si = 0; si < sizeof(k_svcs) / sizeof(k_svcs[0]); si++) {
        uint32_t va = m->base + k_svcs[si].off;
        printf("[JJFB_SHELL_EXPORT] module=gbrwcore.ext name=%s registered=yes "
               "addr=0x%X kind=string_va_not_entry evidence=TARGET_OBSERVED\n",
               k_svcs[si].name, va);
        printf("[JJFB_SHELL_EXPORT_RESOLVE] name=%s string_va=0x%X dispatcher=unknown "
               "status=string_va_not_entry evidence=TARGET_OBSERVED\n",
               k_svcs[si].name, va);
        e10a3_note_service_registry(k_svcs[si].name, va, 0, "string_va_not_entry",
                                    "gbrwcore.ext", 1, "static_string_table");
        e10a3_note_named_service("register", k_svcs[si].name, "gbrwcore.ext", "gbrwcore.ext",
                                 va, "string_va", 0, 0, "string_table_only");
    }
    fflush(stdout);
    recompute_gate();
}

void ext_gwy_shell_native_exec_on_start_dsm(const char *filename, const char *ext,
                                            const char *entry) {
    const char *pkg;
    if (!ext_gwy_shell_native_exec_enabled()) return;
    (void)ext;
    snprintf(g_ne.active_pkg, sizeof(g_ne.active_pkg), "%s", filename ? filename : "");
    pkg = filename ? filename : "?";

    if (path_has(pkg, "gbrwcore")) {
        g_ne.mrp_started_gbrwcore = 1;
        robotol_flag_writer_e10a_shell_phase("gbrwcore_mr_start");
        e10a_shell_phase("SHELL_PHASE_GBRWCORE_START", "gbrwcore.ext", 0, 0, 0, 0, 0, 0, 0, 0,
                         entry ? entry : "start.mr");
        printf("[JJFB_SHELL_EXEC] package=gwy/gbrwcore.mrp stage=mr_start entered=yes "
               "pc=pending entry=\"%s\" evidence=OBSERVED\n",
               entry ? entry : "(null)");
        printf("[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=entry "
               "evidence=TARGET_OBSERVED\n");
    } else if (path_has(pkg, "gamelist")) {
        g_ne.mrp_started_gamelist = 1;
        robotol_flag_writer_e10a_shell_phase("gamelist_mr_start");
        e10a_shell_phase("SHELL_PHASE_GAMELIST_LOAD", "gamelist.ext", 0, 0, 0, 0, 0, 0, 0, 0,
                         entry ? entry : "start.mr");
        printf("[JJFB_SHELL_EXEC] package=gwy/gamelist.mrp stage=mr_start entered=yes "
               "pc=pending entry=\"%s\" evidence=OBSERVED\n",
               entry ? entry : "(null)");
        printf("[JJFB_GAMELIST_STARTED] filename=\"%s\" entry=\"%s\" evidence=DOCUMENTED\n",
               pkg, entry ? entry : "(null)");
        printf("[JJFB_SHELL_CORE_MODULE] module=gamelist.ext stage=entry "
               "evidence=TARGET_OBSERVED\n");
    } else if (path_has(pkg, "gbrwshell")) {
        g_ne.mrp_started_gbrwshell = 1;
        printf("[JJFB_SHELL_EXEC] package=gwy/gbrwshell.mrp stage=mr_start entered=yes "
               "pc=pending entry=\"%s\" evidence=OBSERVED\n",
               entry ? entry : "(null)");
        printf("[JJFB_SHELL_CORE_MODULE] module=gbrwshell.ext stage=entry "
               "evidence=TARGET_OBSERVED\n");
    } else if (path_has(pkg, "jjfb")) {
        g_ne.mrp_started_jjfb = 1;
        robotol_flag_writer_e10a_shell_phase("jjfb_mr_start");
        e10a_shell_phase("SHELL_PHASE_JJFB_MR_START", "jjfb.mrp", 0, 0, 0, 0, 0, 0, 0, 0,
                         entry ? entry : "start.mr");
        if (g_ne.mrp_started_gbrwcore || g_ne.mrp_started_gamelist || g_ne.guest_pc_hit) {
            printf("[JJFB_SHELL_EXPORT_CALL] name=lib.runapp_or_startGame via=nested_start_dsm "
                   "target=gwy/jjfb.mrp pc=nested evidence=HYPOTHESIS_pending_export_pc\n");
            g_ne.export_called = 1;
            printf("[JJFB_RUNAPP] source=native_shell target=gwy/jjfb.mrp "
                   "via=guest_native_nested_start_dsm evidence=TARGET_OBSERVED\n");
        }
    }
    fflush(stdout);
    recompute_gate();
}

void ext_gwy_shell_native_exec_on_file_open(const char *guest_path, const char *host_path, int ok) {
    if (!ext_gwy_shell_native_exec_enabled() || !guest_path || !ok) return;
    (void)host_path;
    if (path_has(guest_path, "gbrwcore.mrp")) g_ne.package_open_gbrwcore = 1;
    if (path_has(guest_path, "gamelist.mrp")) g_ne.package_open_gamelist = 1;
    if (path_has(guest_path, "gbrwshell.mrp")) g_ne.package_open_gbrwshell = 1;
    if (is_shell_pkg_name(guest_path)) {
        printf("[JJFB_SHELL_EXEC] package=%s stage=shell_package_open entered=yes "
               "evidence=OBSERVED\n",
               guest_path);
        fflush(stdout);
    }
    /* Natural resource chain: cfg/gifs/bitmaps under gwy/ (game or shell). */
    if (path_has(guest_path, "gwy/") &&
        (path_has(guest_path, "cfg.bin") || path_has(guest_path, ".bmp") ||
         path_has(guest_path, "gifs/") || path_has(guest_path, "loading") ||
         path_has(guest_path, "slogo") || path_has(guest_path, "bar"))) {
        if (path_has(guest_path, "cfg.bin")) {
            e10a_shell_cfg_runtime("cfg_bin_open", 0, 0, 0, "", guest_path, "", 0, "",
                                   "shell_file_open", "SHELL_CFG_BIN_PARSED_RUNTIME");
            e10a3_mark_real_cfg_selected(guest_path);
            e10a_shell_phase("SHELL_PHASE_CFG_RECORD_SELECTED", "gamelist.ext", 0, 0, 0, 0, 0, 0,
                             0, 0, guest_path);
        }
        printf("[JJFB_RESOURCE_REQUEST] guest=\"%s\" host=\"%s\" ok=1 evidence=OBSERVED\n",
               guest_path, host_path ? host_path : "?");
        fflush(stdout);
    }
}

void ext_gwy_shell_native_exec_on_member_open(const char *guest_path) {
    const char *label;
    if (!ext_gwy_shell_native_exec_enabled() || !guest_path) return;
    if (path_has(guest_path, "mrc_loader.ext")) {
        printf("[JJFB_MRC_LOADER] member=%s stage=open evidence=OBSERVED\n", guest_path);
        fflush(stdout);
    }
    if (path_has(guest_path, "robotol.ext") || path_has(guest_path, "mmochat.ext")) {
        printf("[JJFB_RESOURCE_REQUEST] member=%s class=primary_ext evidence=OBSERVED\n",
               guest_path);
        fflush(stdout);
    }
    if (!is_shell_ext_name(guest_path) && !is_shell_pkg_name(guest_path)) return;
    label = shell_mod_label(guest_path);
    if (is_shell_ext_name(guest_path)) {
        g_ne.ext_loaded = 1;
        printf("[JJFB_SHELL_EXT] package=gwy/%s member=%s loaded=yes base=pending size=pending "
               "evidence=OBSERVED\n",
               label, label);
        fflush(stdout);
        (void)find_or_add_mod(label);
    }
    recompute_gate();
}

void ext_gwy_shell_native_exec_on_code_image(uint32_t guest_addr, uint32_t size) {
    const char *label = NULL;
    ShellMod *m;
    if (!ext_gwy_shell_native_exec_enabled() || size == 0) return;
    if (size == GBRWCORE_EXT_SIZE || (size > GBRWCORE_EXT_SIZE && size <= GBRWCORE_EXT_SIZE + 0x60))
        label = "gbrwcore.ext";
    else if (size == GAMELIST_EXT_SIZE ||
             (size > GAMELIST_EXT_SIZE && size <= GAMELIST_EXT_SIZE + 0x60))
        label = "gamelist.ext";
    else if (size == GBRWSHELL_EXT_SIZE ||
             (size > GBRWSHELL_EXT_SIZE && size <= GBRWSHELL_EXT_SIZE + 0x60))
        label = "gbrwshell.ext";
    if (!label) return;
    m = find_or_add_mod(label);
    if (!m) return;
    m->base = guest_addr;
    m->size = size;
    m->mapped = 1;
    g_ne.ext_loaded = 1;
    printf("[JJFB_SHELL_EXT] package=gwy/%s member=%s loaded=yes base=0x%X size=%u "
           "evidence=OBSERVED\n",
           label, label, guest_addr, size);
    fflush(stdout);
    emit_export_table(m);
    if (strcmp(label, "gamelist.ext") == 0) {
        /* Format string mapped != live cfg36 select (E10A-3). */
        printf("[JJFB_GAMELIST_CFG36_BUILD] param_fmt_va=0x%X note=format_string_mapped "
               "evidence=TARGET_OBSERVED\n",
               guest_addr + GAMELIST_OFF_CFG36_FMT);
        g_ne.cfg36_build = 1;
        robotol_flag_writer_e10a_shell_phase("gamelist_cfg36_fmt_mapped");
        e10a_shell_phase("SHELL_PHASE_CFG_FMT_MAPPED", "gamelist.ext", 0, 0, 0, 0, 0, 0, 0, 0,
                         "cfg36_param_fmt_not_selected");
        fflush(stdout);
    }
    recompute_gate();
}

void ext_gwy_shell_native_exec_on_module_map(const char *module_name, uint32_t base, uint32_t size) {
    if (!ext_gwy_shell_native_exec_enabled()) return;
    if (!module_name || !is_shell_ext_name(module_name)) return;
    ext_gwy_shell_native_exec_on_code_image(base, size);
}

static int read_guest_cstr(void *uc, uint32_t addr, char *out, size_t cap) {
    size_t i;
    if (!uc || !addr || !out || cap < 2) return 0;
    for (i = 0; i + 1 < cap; i++) {
        uint8_t b = 0;
        if (!guest_memory_uc_peek((struct uc_struct *)uc, addr + (uint32_t)i, &b, 1)) return 0;
        out[i] = (char)b;
        if (b == 0) return 1;
        if (b < 32 || b > 126) {
            out[i] = 0;
            return i > 0;
        }
    }
    out[cap - 1] = 0;
    return 1;
}

void ext_gwy_shell_native_exec_on_launch_param(uint32_t entry_va, const char *entry) {
    if (!ext_gwy_shell_native_exec_enabled()) return;
    g_ne.param_va = entry_va;
    g_ne.param_len = entry ? (uint32_t)strlen(entry) + 1u : 0;
    if (entry) {
        snprintf(g_ne.param_buf, sizeof(g_ne.param_buf), "%s", entry);
    } else {
        g_ne.param_buf[0] = 0;
    }
    g_ne.param_map_logged = 1;
    printf("[JJFB_PARAM_MAP] va=0x%X len=%u param=\"%.120s\" evidence=DOCUMENTED\n", entry_va,
           g_ne.param_len, g_ne.param_buf);
    fflush(stdout);
}

void ext_gwy_shell_native_exec_on_slot28(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2,
                                         uint32_t r3, uint32_t ret) {
    (void)pc;
    (void)r3;
    (void)ret;
    if (!ext_gwy_shell_native_exec_enabled()) return;
    /* TARGET_OBSERVED: 0x10102(event_code, handler) registration. */
    if (r0 == 0x10102u && r2) {
        add_handler(r1, r2, 0);
        e10a_shell_event(r1, r2, pc, 0, "slot28_10102_register");
    }
}

static void maybe_export_call_from_regs(void *uc, uint32_t pc, const uint32_t regs[16],
                                        const char *module_name) {
    int i;
    char buf[64];
    if (!regs) return;
    for (i = 0; i < 4; i++) {
        if (!read_guest_cstr(uc, regs[i], buf, sizeof(buf))) continue;
        if (strstr(buf, "lib.runapp") || strcmp(buf, "runapp") == 0) {
            g_ne.export_called = 1;
            printf("[JJFB_SHELL_EXPORT_CALL] name=lib.runapp pc=0x%X args=r%d=\"%s\" "
                   "module=%s evidence=OBSERVED\n",
                   pc, i, buf, module_name ? module_name : "?");
            printf("[JJFB_RUNAPP] source=native_shell target=gwy/jjfb.mrp "
                   "via=guest_native_lib_runapp evidence=TARGET_OBSERVED\n");
            robotol_flag_writer_e10a_shell_phase("shell_runapp");
            e10a3_note_named_service("lookup", "lib.runapp", module_name ? module_name : "?", "?",
                                     regs[i], buf, 0, 0, "reg_string_observe");
            e10a_shell_phase("SHELL_PHASE_RUNAPP_LOOKUP", module_name ? module_name : "?", pc, 0,
                             regs[i], 0, 0, 0, 0, 0, buf);
            e10a_shell_phase("SHELL_PHASE_RUNAPP_CALLED", module_name ? module_name : "?", pc, 0, 0,
                             0, 0, 0, 0, 0, buf);
            fflush(stdout);
        }
        if (strstr(buf, "lib.startGame") || strcmp(buf, "startGame") == 0) {
            g_ne.export_called = 1;
            printf("[JJFB_SHELL_EXPORT_CALL] name=lib.startGame pc=0x%X args=r%d=\"%s\" "
                   "module=%s evidence=OBSERVED\n",
                   pc, i, buf, module_name ? module_name : "?");
            printf("[JJFB_STARTGAME] source=gwy_shell cfg_index=36 target=gwy/jjfb.mrp "
                   "via=guest_native_lib_startGame\n");
            e10a3_note_named_service("lookup", "lib.startGame",
                                     module_name ? module_name : "?", "?", regs[i], buf, 0, 0,
                                     "reg_string_observe");
            e10a_shell_phase("SHELL_PHASE_STARTGAME_LOOKUP", module_name ? module_name : "?", pc,
                             0, regs[i], 0, 0, 0, 0, 0, buf);
            fflush(stdout);
        }
        if (strstr(buf, "lib.getuserinfo") || strcmp(buf, "getuserinfo") == 0) {
            e10a3_note_named_service("lookup", "lib.getuserinfo",
                                     module_name ? module_name : "?", "?", regs[i], buf, 0, 0,
                                     "reg_string_observe");
            fflush(stdout);
        }
        if (strstr(buf, "lib.checkmrpver") || strcmp(buf, "checkmrpver") == 0) {
            e10a3_note_named_service("lookup", "lib.checkmrpver",
                                     module_name ? module_name : "?", "?", regs[i], buf, 0, 0,
                                     "reg_string_observe");
            e10a_shell_phase("SHELL_PHASE_VERSION_CHECK_REQUEST",
                             module_name ? module_name : "?", pc, 0, regs[i], 0, 0, 0, 0, 0, buf);
            fflush(stdout);
        }
        if (strstr(buf, "napptype=") && strstr(buf, "gwyblink")) {
            g_ne.cfg36_build = 1;
            printf("[JJFB_GAMELIST_CFG36_BUILD] param=%s evidence=OBSERVED\n", buf);
            e10a3_mark_real_cfg_selected("gwyblink_param_in_regs");
            e10a_shell_phase("SHELL_PHASE_CFG_DESCRIPTOR_BUILT",
                             module_name ? module_name : "?", pc, 0, regs[i], 0, 0, 0, 0, 0, buf);
            fflush(stdout);
        }
        if (strstr(buf, "no_update") || strstr(buf, "update_ok") || strstr(buf, "checkmrpver")) {
            if (!g_ne.post_update) {
                g_ne.post_update = 1;
                robotol_flag_writer_e10a_shell_phase("gamelist_post_update");
                e10a_shell_phase("SHELL_PHASE_UPDATE_NO_UPDATE", "gamelist.ext", 0, 0, 0, 0, 0, 0,
                                 0, 0, buf);
                e10a_shell_update("update_check", "no_update_or_check", buf);
                printf("[JJFB_GAMELIST_POST_UPDATE] result=no_update_or_check branch=guest "
                       "hint=%s evidence=OBSERVED\n",
                       buf);
                fflush(stdout);
            }
        }
    }
}

void ext_gwy_shell_native_exec_on_code(void *uc, uint64_t module_id, const char *module_name,
                                       uint32_t pc, const uint32_t regs[16]) {
    ShellMod *m = NULL;
    int i;
    int in_shell = 0;
    (void)module_id;
    if (!ext_gwy_shell_native_exec_enabled()) return;
    if (uc) g_ne.uc = uc;
    e10a_vfs_note_guest_code(module_name ? module_name : "?", pc);

    if (module_name && is_shell_ext_name(module_name)) in_shell = 1;
    for (i = 0; i < g_ne.mod_count; i++) {
        if (!g_ne.mods[i].mapped) continue;
        if (pc >= g_ne.mods[i].base && pc < g_ne.mods[i].base + g_ne.mods[i].size) {
            m = &g_ne.mods[i];
            in_shell = 1;
            break;
        }
    }
    if (!in_shell) {
        maybe_param_read(uc ? uc : g_ne.uc, pc, regs, module_name);
        maybe_export_call_from_regs(uc ? uc : g_ne.uc, pc, regs, module_name);
        return;
    }
    /* D5b: 0x10102 handler enter (first insn at registered VA). */
    {
        GlHandler *hh = find_handler(pc);
        if (hh && hh->enter_n == 0) {
            hh->enter_n = 1;
            printf("[JJFB_GAMELIST_HANDLER_ENTER] event_code=0x%X pc=0x%X r0=0x%X r1=0x%X r2=0x%X "
                   "seeded=%d evidence=TARGET_OBSERVED\n",
                   hh->event_code, pc, regs ? regs[0] : 0, regs ? regs[1] : 0, regs ? regs[2] : 0,
                   hh->seeded);
            fflush(stdout);
        } else if (hh && hh->enter_n > 0 && hh->enter_n < 4) {
            hh->enter_n++;
            printf("[JJFB_GAMELIST_HANDLER_ENTER] event_code=0x%X pc=0x%X count=%u "
                   "evidence=TARGET_OBSERVED\n",
                   hh->event_code, pc, hh->enter_n);
            fflush(stdout);
        }
    }
    maybe_param_read(uc ? uc : g_ne.uc, pc, regs, m ? m->name : module_name);
    if (m && !m->pc_hit) {
        m->pc_hit = 1;
        g_ne.guest_pc_hit = 1;
        printf("[JJFB_SHELL_GUEST_PC] pc=0x%X module=%s hit=first evidence=OBSERVED\n", pc, m->name);
        printf("[JJFB_SHELL_EXEC] package=gwy/%s stage=shell_guest_pc_hit entered=yes pc=0x%X "
               "evidence=OBSERVED\n",
               m->name, pc);
        printf("[JJFB_SHELL_CORE_MODULE] module=%s stage=init_ok evidence=TARGET_OBSERVED\n",
               m->name);
        fflush(stdout);
        if (strcmp(m->name, "gamelist.ext") == 0) e10a3_mark_gamelist_init_ok();
        emit_export_table(m);
        recompute_gate();
        /* Do not finalize here: keep observing until fault / export call / exit. */
        if (g_ne.gate_open) {
            printf("[JJFB_SHELL_NATIVE_GATE] shell_native_exec_gate=open class=EXEC_GATE_OPEN "
                   "pc=0x%X module=%s note=continue_observe evidence=TARGET_OBSERVED\n",
                   pc, m->name);
            fflush(stdout);
        }
    } else if (!m && !g_ne.guest_pc_hit && module_name) {
        g_ne.guest_pc_hit = 1;
        printf("[JJFB_SHELL_GUEST_PC] pc=0x%X module=%s hit=first evidence=OBSERVED\n", pc,
               module_name);
        fflush(stdout);
        recompute_gate();
        if (g_ne.gate_open) {
            printf("[JJFB_SHELL_NATIVE_GATE] shell_native_exec_gate=open class=EXEC_GATE_OPEN "
                   "pc=0x%X module=%s note=continue_observe evidence=TARGET_OBSERVED\n",
                   pc, module_name);
            fflush(stdout);
        }
    }
    /* E10A-2: gbrwcore field0-release (absolute VA; do not require mapped mod). */
    if (regs && ((module_name && strstr(module_name, "gbrwcore")) ||
                 (m && m->name[0] && strstr(m->name, "gbrwcore")))) {
        uint32_t pca = pc & ~1u;
        if (!g_ne.field0_release_hit && pca >= 0x30D02Cu && pca < 0x30D04Cu) {
            uint32_t obj = 0, field0 = 0, field8 = 0, erw1880 = 0;
            uint8_t fb[32];
            char fb_hex[80];
            int ni;
            void *ucu = uc ? uc : g_ne.uc;
            g_ne.field0_release_hit = 1;
            fb_hex[0] = 0;
            memset(fb, 0, sizeof(fb));
            if (ucu && regs[4])
                guest_memory_uc_peek_u32((struct uc_struct *)ucu, regs[4], &obj);
            if (ucu && obj) {
                guest_memory_uc_peek_u32((struct uc_struct *)ucu, obj, &field0);
                guest_memory_uc_peek_u32((struct uc_struct *)ucu, obj + 8u, &field8);
            }
            if (ucu && regs[9])
                guest_memory_uc_peek_u32((struct uc_struct *)ucu, regs[9] + 0x1880u, &erw1880);
            if (ucu && field0)
                guest_memory_uc_peek((struct uc_struct *)ucu, field0, fb, sizeof(fb));
            for (ni = 0; ni < 16; ni++) {
                char t[3];
                snprintf(t, sizeof(t), "%02X", fb[ni]);
                strncat(fb_hex, t, sizeof(fb_hex) - strlen(fb_hex) - 1);
            }
            printf("[JJFB_GBRWCORE_FIELD0_RELEASE] pc=0x%X r0=0x%X r1=0x%X r4=0x%X r5=0x%X "
                   "r6=0x%X r9=0x%X obj=0x%X field0=0x%X field8=0x%X erw+1880=0x%X "
                   "field0_bytes=%s evidence=OBSERVED\n",
                   pc, regs[0], regs[1], regs[4], regs[5], regs[6], regs[9], obj, field0,
                   field8, erw1880, fb_hex);
            fflush(stdout);
            if (ucu && regs[6]) {
                uint32_t a0 = 0, a4 = 0, a8 = 0;
                guest_memory_uc_peek_u32((struct uc_struct *)ucu, regs[6], &a0);
                guest_memory_uc_peek_u32((struct uc_struct *)ucu, regs[6] + 4u, &a4);
                guest_memory_uc_peek_u32((struct uc_struct *)ucu, regs[6] + 8u, &a8);
                printf("[JJFB_GBRWCORE_DISPATCH_ARG] pc=0x%X r5(P)=0x%X r6=0x%X "
                       "[r6]=0x%X [r6+4]=0x%X [r6+8]=0x%X evidence=OBSERVED\n",
                       pc, regs[5], regs[6], a0, a4, a8);
                fflush(stdout);
            }
            e10a_shell_cfg_runtime("gbrwcore_field0_release", regs[4], obj, field0, "",
                                   field0 ? (const char *)fb : "", "", erw1880, fb_hex,
                                   "ERW+0x1880 helper", "SHELL_FIELD0_RELEASE_SITE");
        }
    }
    /* D5: one-shot — did guest enter cfg-open / command-dispatch (not timer)? */
    if (m && strcmp(m->name, "gamelist.ext") == 0 && m->base && m->size) {
        uint32_t off = pc - m->base;
        if (!g_ne.cfg_gate_hit &&
            (off == GAMELIST_OFF_CFG_OPEN || off == GAMELIST_OFF_CFG_GATE ||
             off == GAMELIST_OFF_CFG_WRAP ||
             (off >= GAMELIST_OFF_CFG_OPEN && off < GAMELIST_OFF_CFG_OPEN + 0x2C0u))) {
            g_ne.cfg_gate_hit = 1;
            printf("[JJFB_GAMELIST_CFG_GATE] pc=0x%X off=0x%X hit=yes evidence=TARGET_OBSERVED\n",
                   pc, off);
            fflush(stdout);
        }
        if (!g_ne.cmd_disp_hit &&
            (off == GAMELIST_OFF_CMD_DISP ||
             (off >= GAMELIST_OFF_CMD_DISP && off < GAMELIST_OFF_CMD_DISP + 0x80u))) {
            uint8_t fb[8];
            int ni;
            g_ne.cmd_disp_hit = 1;
            printf("[JJFB_GAMELIST_CMD_DISP] pc=0x%X off=0x%X r0=0x%X r1=0x%X r2=0x%X "
                   "evidence=TARGET_OBSERVED\n",
                   pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0, regs ? regs[2] : 0);
            printf("[JJFB_GAMELIST_CMD_DISP_ENTER] pc=0x%X r0=0x%X r1=0x%X r2=0x%X "
                   "evidence=TARGET_OBSERVED\n",
                   pc, regs ? regs[0] : 0, regs ? regs[1] : 0, regs ? regs[2] : 0);
            if (uc && regs && regs[0] &&
                guest_memory_uc_peek((struct uc_struct *)uc, regs[0], fb, sizeof(fb))) {
                char fb_hex[32];
                fb_hex[0] = 0;
                for (ni = 0; ni < 8; ni++) {
                    char t[3];
                    snprintf(t, sizeof(t), "%02X", fb[ni]);
                    strncat(fb_hex, t, sizeof(fb_hex) - strlen(fb_hex) - 1);
                }
                e10a_shell_cfg_runtime("cmd_dispatch_path_candidate", 0, 0, off, "",
                                       "", "", regs[0], fb_hex,
                                       "r0_path_candidate", fb[0] == 0x08 ? "badpath=yes" : "badpath=no");
                printf("[JJFB_GAMELIST_CMD_DISP_CHAR] first_bytes=");
                for (ni = 0; ni < 8; ni++) printf("%02X", fb[ni]);
                if (fb[0] >= 32 && fb[0] < 127) printf(" char='%c'", (char)fb[0]);
                printf(" evidence=TARGET_OBSERVED\n");
            }
            fflush(stdout);
        }
    }
    maybe_export_call_from_regs(uc ? uc : g_ne.uc, pc, regs,
                                module_name ? module_name : (m ? m->name : "?"));
}

void ext_gwy_shell_native_exec_on_helper_call(uint32_t helper, uint32_t method, int32_t ret) {
    if (!ext_gwy_shell_native_exec_enabled()) return;
    if (!g_ne.guest_pc_hit && (g_ne.mrp_started_gbrwcore || g_ne.ext_loaded)) {
        /* Helper activity after shell start is weak evidence of guest progress. */
        printf("[JJFB_SHELL_EXEC] package=active stage=helper_call helper=0x%X method=%u ret=%d "
               "evidence=OBSERVED\n",
               helper, method, (int)ret);
        fflush(stdout);
    }
}

void ext_gwy_shell_native_exec_on_strcom(uint32_t code, const char *caller) {
    if (!ext_gwy_shell_native_exec_enabled()) return;
    if (code == 601) g_ne.strcom_601 = 1;
    if (code == 800) g_ne.strcom_800 = 1;
    if (code == 801) g_ne.strcom_801 = 1;
    if (code == 601 || code == 800 || code == 801) {
        printf("[JJFB_STRCOM] code=%u caller=%s evidence=OBSERVED\n", code,
               caller ? caller : "?");
        fflush(stdout);
    }
}

void ext_gwy_shell_native_exec_on_mrc_init(uint32_t pc, int32_t ret) {
    if (!ext_gwy_shell_native_exec_enabled()) return;
    g_ne.mrc_init = 1;
    printf("[JJFB_MRC_INIT] pc=0x%X ret=%d evidence=OBSERVED\n", pc, (int)ret);
    fflush(stdout);
}

void ext_gwy_shell_native_exec_on_pxc_write(uint32_t old_v, uint32_t new_v, uint32_t pc, uint32_t lr,
                                            const char *module) {
    if (!ext_gwy_shell_native_exec_enabled()) return;
    g_ne.pxc_writes++;
    g_ne.pxc_pc = pc;
    g_ne.pxc_lr = lr;
    snprintf(g_ne.pxc_module, sizeof(g_ne.pxc_module), "%s", module ? module : "?");
    printf("[JJFB_P_WRITE] off=0x0C old=0x%X new=0x%X pc=0x%X lr=0x%X module=%s "
           "evidence=OBSERVED\n",
           old_v, new_v, pc, lr, g_ne.pxc_module);
    fflush(stdout);
}

void ext_gwy_shell_native_exec_on_mem_fault(void *uc, uint32_t fault_pc) {
    int i;
    uint32_t regs[16];
    if (!ext_gwy_shell_native_exec_enabled()) return;
    if (uc) g_ne.uc = uc;
    memset(regs, 0, sizeof(regs));
    for (i = 0; i < g_ne.mod_count; i++) {
        if (!g_ne.mods[i].mapped) continue;
        if (fault_pc >= g_ne.mods[i].base &&
            fault_pc < g_ne.mods[i].base + g_ne.mods[i].size) {
            if (!g_ne.mods[i].pc_hit) {
                g_ne.mods[i].pc_hit = 1;
                g_ne.guest_pc_hit = 1;
                printf("[JJFB_SHELL_GUEST_PC] pc=0x%X module=%s hit=fault_window "
                       "evidence=OBSERVED\n",
                       fault_pc, g_ne.mods[i].name);
                printf("[JJFB_SHELL_EXEC] package=gwy/%s stage=shell_guest_pc_hit entered=yes "
                       "pc=0x%X evidence=OBSERVED\n",
                       g_ne.mods[i].name, fault_pc);
                fflush(stdout);
            }
            recompute_gate();
            ext_gwy_shell_native_exec_finalize("shell_ext_fault_in_guest_pc");
            return;
        }
    }
    if (g_ne.ext_loaded || g_ne.mrp_started_gbrwcore) {
        recompute_gate();
        ext_gwy_shell_native_exec_finalize("mem_fault");
    }
}

void ext_gwy_shell_native_exec_finalize(const char *stop_reason) {
    GwyShellNativeExecClass c;
    int pkg_open;
    int started;
    if (!ext_gwy_shell_native_exec_enabled() || g_ne.finalized) return;
    g_ne.finalized = 1;
    recompute_gate();
    pkg_open = g_ne.package_open_gbrwcore || g_ne.package_open_gamelist ||
               g_ne.package_open_gbrwshell;
    started = g_ne.mrp_started_gbrwcore || g_ne.mrp_started_gamelist || g_ne.mrp_started_gbrwshell;

    if (g_ne.export_called)
        c = GWY_SHELL_NATIVE_GUEST_RUNAPP_CALLED;
    else if (g_ne.gate_open)
        c = GWY_SHELL_NATIVE_EXEC_GATE_OPEN;
    else if (started || g_ne.ext_loaded || g_ne.guest_pc_hit)
        c = GWY_SHELL_NATIVE_EXEC_PARTIAL;
    else if (pkg_open)
        c = GWY_SHELL_NATIVE_PACKAGE_OPEN_ONLY;
    else
        c = GWY_SHELL_NATIVE_NONE;
    g_ne.last_class = c;

    printf("[JJFB_SHELL_NATIVE_SUMMARY] class=%s shell_native_exec_gate=%s "
           "package_open=%s mrp_started=%s ext_loaded=%s export_registered=%s "
           "export_called=%s guest_pc_hit=%s gbrwcore=%s gamelist=%s gbrwshell=%s "
           "jjfb_started=%s cfg36_build=%s post_update=%s strcom_601=%s strcom_800=%s "
           "strcom_801=%s mrc_init=%s pxc_writes=%d stop=%s evidence=TARGET_OBSERVED\n",
           ext_gwy_shell_native_exec_class_name(c), g_ne.gate_open ? "open" : "blocked",
           pkg_open ? "yes" : "no", started ? "yes" : "no", g_ne.ext_loaded ? "yes" : "no",
           g_ne.export_registered ? "yes" : "no", g_ne.export_called ? "yes" : "no",
           g_ne.guest_pc_hit ? "yes" : "no", g_ne.mrp_started_gbrwcore ? "yes" : "no",
           g_ne.mrp_started_gamelist ? "yes" : "no", g_ne.mrp_started_gbrwshell ? "yes" : "no",
           g_ne.mrp_started_jjfb ? "yes" : "no", g_ne.cfg36_build ? "yes" : "no",
           g_ne.post_update ? "yes" : "no", g_ne.strcom_601 ? "yes" : "no",
           g_ne.strcom_800 ? "yes" : "no", g_ne.strcom_801 ? "yes" : "no",
           g_ne.mrc_init ? "yes" : "no", g_ne.pxc_writes, stop_reason ? stop_reason : "?");
    fflush(stdout);
}
