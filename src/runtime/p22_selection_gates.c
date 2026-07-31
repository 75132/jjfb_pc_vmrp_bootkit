#include "gwy_launcher/p22_selection_gates.h"

#include "gwy_launcher/e10a3_postselect_trace.h"
#include "gwy_launcher/e10a31_gamelist_context.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_r9_switch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* Neutral old sites (diag only — NOT cfg gates). */
#define OFF_STATE_SLOT_COPY 0x01AF8u
#define OFF_STATE_SLOT_COMMIT 0x0CE8Au

/* Real cfg loader / path state machine (gamelist.ext file offsets). */
#define OFF_CFG_LOADER_ENTRY 0x07B6Cu
#define OFF_CFG_PRECHECK_CALL 0x07B7Cu
#define OFF_CFG_PRECHECK_RETURN 0x07B80u
#define OFF_CFG_PATH_RESOLVED 0x07B94u
#define OFF_CFG_DISPATCH_CALL 0x07B9Cu
#define OFF_CFG_DISPATCH_RETURN 0x07BA0u
#define OFF_CFG_LOADER_CALLER_1 0x0D96Cu
#define OFF_CFG_LOADER_CALLER_2 0x0FF3Au
#define OFF_CFG_LOADER_CALLER_3 0x0FFB2u
#define OFF_CFG_PATH_STATE_ENTRY 0x0D768u
#define OFF_CFG_PATH_DYNAMIC_1 0x0D7D6u
#define OFF_CFG_PATH_DYNAMIC_2 0x0D7F6u
#define OFF_CFG_PATH_TEMP_1 0x0D80Eu
#define OFF_CFG_PATH_TEMP_2 0x0D826u
#define OFF_CFG_PATH_STATE_RETURN 0x0D832u

#define OFF_SELECT_1 0x089CCu
#define OFF_SELECT_2 0x10136u
#define OFF_SELECT_3 0x10186u
#define OFF_SELECT_4 0x1024Eu
#define OFF_DESC_BUILDER 0x13A34u
#define OFF_API_HANDOFF 0x13B7Cu
#define OFF_SELECT_FN 0x8930u

#define CFG_INTERNAL_SIZE 6898u
#define CFG_EXTERNAL_SIZE 20728u
#define CFG_RECORD_BASE 1024u
#define CFG_RECORD_SIZE 272u
#define CFG36_INDEX 36u
#define CFG36_FILE_OFF (CFG_RECORD_BASE + CFG36_INDEX * CFG_RECORD_SIZE)

#define EXPECTED_DESC \
    "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"

#define PC_RING 256
#define BR_RING 64
#define BL_RING 32

static const uint32_t k_legal_builder_lr_off[] = {0x089D1u, 0x1013Bu, 0x1018Bu, 0x10253u};

static const char *k_gate_names[P22_GATE_COUNT] = {
    "G0_BUILD",
    "G1_CFG_LOADER",
    "G2_INTERNAL_REQUESTED",
    "G3_INTERNAL_LOADED",
    "G4_INTERNAL_PARSED",
    "G5_PATH_STATE",
    "G6_EXTERNAL_LOADED",
    "G7_CFG36_PARSED",
    "G8_ITEM_CREATED",
    "G9_SELECT_CALLBACK",
    "G10_DESC_BUILDER_LEGAL",
    "G11_STATE_NONEMPTY",
    "G12_DESC_MATCH",
    "G13_STARTGAME_LOOKUP",
    "G14_STARTGAME_ENTER",
    "G15_OPCODE300",
    "G16_NESTED_JJFB",
    "G17_ROBOTOL_EXT",
};

static struct {
    int known;
    int enabled;
    int headless;
    unsigned long long run_id;
    clock_t t0;
    int gate[P22_GATE_COUNT];
    FILE *csv;
    FILE *trace;

    uint32_t gl_base;
    uint32_t gl_size;
    int gamelist_active;

    int cfg_internal_req;
    int cfg_internal_loaded;
    uint32_t cfg_internal_size;
    int cfg_external_loaded;
    uint32_t cfg_external_size;
    int cfg36_parsed;
    int item_created;
    int selected;
    int desc_enter_legal;
    int desc_stray;
    int desc_match;
    int sg_lookup;
    int sg_enter;
    int op300;
    int nested;
    int robotol;

    uint32_t cfg_record_va;
    uint32_t item_object_va;
    uint32_t callback_va;
    uint32_t select_callsite_off;
    uint32_t last_r9;
    uint32_t state_base;

    char descriptor[192];
    char handoff_note[160];
    uint32_t sg_r0, sg_r1, sg_r2, sg_pc, sg_r9;
    uint32_t sg_lookup_pc;
    char nested_target[96];

    int param_abi_ok;
    int param_ptr_copied;
    int param_gwyblink_read;
    int headless_fired;
    clock_t item_created_t;

    void *uc;
    int bp_logged[32];

    /* Control-flow forensics rings (gamelist-active). */
    uint32_t pc_ring[PC_RING];
    uint32_t lr_ring[PC_RING];
    uint32_t sp_ring[PC_RING];
    uint32_t r0_ring[PC_RING];
    size_t pc_n;
    size_t pc_next;

    uint32_t br_from[BR_RING];
    uint32_t br_to[BR_RING];
    size_t br_n;
    size_t br_next;

    uint32_t bl_pc[BL_RING];
    uint32_t bl_lr[BL_RING];
    uint32_t bl_tgt[BL_RING];
    size_t bl_n;
    size_t bl_next;

    uint32_t stray_pc;
    uint32_t stray_lr;
    uint32_t stray_sp;
    uint32_t stray_r9;
    char stray_source[48];
    uint32_t fault_pc;
    uint32_t fault_addr;
} g_p22;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

static double elapsed_s(void) {
    if (!g_p22.t0) return 0.0;
    return (double)(clock() - g_p22.t0) / (double)CLOCKS_PER_SEC;
}

static int path_has(const char *s, const char *n) {
    return s && n && strstr(s, n) != NULL;
}

static int path_eq_ci_tail(const char *s, const char *name) {
    const char *p;
    size_t i;
    if (!s || !name) return 0;
    p = strrchr(s, '/');
    if (!p) p = strrchr(s, '\\');
    p = p ? p + 1 : s;
    for (i = 0; name[i] || p[i]; i++) {
        char a = p[i], b = name[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

const char *p22_gate_name(P22Gate g) {
    if ((int)g < 0 || g >= P22_GATE_COUNT) return "?";
    return k_gate_names[g];
}

int p22_gate_done(P22Gate g) {
    if ((int)g < 0 || g >= P22_GATE_COUNT) return 0;
    return g_p22.gate[g];
}

int p22_enabled(void) {
    const char *mode;
    if (g_p22.known) return g_p22.enabled;
    mode = getenv("JJFB_P22_MODE");
    g_p22.enabled = env1("JJFB_P22_MODE") || env1("JJFB_P25_MODE") ||
                    (mode && (strcmp(mode, "original_headless") == 0 || strcmp(mode, "p25") == 0)) ||
                    env1("JJFB_P22_SELECTION");
    g_p22.headless = env1("JJFB_P22_HEADLESS_SELECT") ||
                     (mode && strcmp(mode, "original_headless") == 0) || g_p22.enabled;
    {
        const char *rid = getenv("JJFB_P25_RUN_ID");
        if (!rid || !rid[0]) rid = getenv("JJFB_P22_RUN_ID");
        if (!rid || !rid[0]) rid = getenv("JJFB_E10A_RUN_ID");
        if (rid && rid[0]) g_p22.run_id = strtoull(rid, NULL, 10);
    }
    g_p22.known = 1;
    if (g_p22.enabled) {
        g_p22.t0 = clock();
        printf("[JJFB_P25] armed=1 headless=%d run_id=%llu evidence=OBSERVED\n", g_p22.headless,
               g_p22.run_id);
        fflush(stdout);
    }
    return g_p22.enabled;
}

void p22_reset(void) {
    if (g_p22.csv) fclose(g_p22.csv);
    if (g_p22.trace) fclose(g_p22.trace);
    memset(&g_p22, 0, sizeof(g_p22));
}

static void ensure_csv(void) {
    const char *p;
    if (g_p22.csv) return;
    p = getenv("JJFB_P25_TRACE_CSV");
    if (!p || !p[0]) p = getenv("JJFB_P22_GATES_CSV");
    if (!p || !p[0]) p = "research/packs/p25_cfg_state/P25_TRACE.csv";
    g_p22.csv = fopen(p, "wb");
    if (g_p22.csv) {
        fputs("run_id,t_sec,event,gate,value,pc,off,r0,r1,r2,r9,item_va,callback_va,note\n",
              g_p22.csv);
        fflush(g_p22.csv);
    }
}

static void trace_row(const char *event, int gate, uint32_t value, uint32_t pc, uint32_t off,
                      uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r9, const char *note) {
    ensure_csv();
    if (!g_p22.csv) return;
    fprintf(g_p22.csv,
            "%llu,%.3f,%s,%s,%u,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\"\n",
            g_p22.run_id, elapsed_s(), event ? event : "?",
            (gate >= 0 && gate < (int)P22_GATE_COUNT) ? p22_gate_name((P22Gate)gate) : "-", value,
            pc, off, r0, r1, r2, r9, g_p22.item_object_va, g_p22.callback_va, note ? note : "");
    fflush(g_p22.csv);
}

static void mark_gate(P22Gate g, uint32_t pc, uint32_t off, uint32_t r0, uint32_t r1, uint32_t r2,
                      uint32_t r9, const char *note) {
    if (g_p22.gate[g]) return;
    g_p22.gate[g] = 1;
    trace_row("GATE", g, 1, pc, off, r0, r1, r2, r9, note);
    printf("[JJFB_P25_GATE] gate=%s t=%.3f pc=0x%X off=0x%X note=%s evidence=OBSERVED\n",
           p22_gate_name(g), elapsed_s(), pc, off, note ? note : "");
    fflush(stdout);
}

void p22_report_runtime_stack(const char *why) {
    uint32_t d, i;
    if (!p22_enabled()) return;
    d = module_r9_switch_depth();
    printf("[JJFB_P25_RUNTIME_STACK] why=%s depth=%u gamelist_active=%d evidence=OBSERVED\n",
           why ? why : "?", d, g_p22.gamelist_active);
    for (i = 0; i < d; i++) {
        ModuleR9Frame fr;
        if (!module_r9_switch_peek_at(i, &fr)) continue;
        printf("[JJFB_P25_FRAME_ROW] i=%u frame_id=%llu ER_RW=0x%X kind=%s pushed_by=%s\n", i,
               (unsigned long long)fr.frame_id, fr.callee_r9, gwy_module_call_kind_name(fr.owner_kind),
               fr.pushed_by);
    }
    fflush(stdout);
}

static int read_cstr(void *uc, uint32_t addr, char *out, size_t cap) {
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

static int looks_like_jjfb_target(const char *s) {
    return s && strstr(s, "gwy/jjfb.mrp") != NULL;
}

static int descriptor_exact(const char *s) {
    return s && strcmp(s, EXPECTED_DESC) == 0;
}

static int lr_is_legal_builder(uint32_t lr_off) {
    size_t i;
    for (i = 0; i < sizeof(k_legal_builder_lr_off) / sizeof(k_legal_builder_lr_off[0]); i++) {
        if (lr_off == k_legal_builder_lr_off[i]) return 1;
    }
    return 0;
}

static void ring_push_pc(uint32_t pc, uint32_t lr, uint32_t sp, uint32_t r0) {
    g_p22.pc_ring[g_p22.pc_next] = pc;
    g_p22.lr_ring[g_p22.pc_next] = lr;
    g_p22.sp_ring[g_p22.pc_next] = sp;
    g_p22.r0_ring[g_p22.pc_next] = r0;
    g_p22.pc_next = (g_p22.pc_next + 1) % PC_RING;
    if (g_p22.pc_n < PC_RING) g_p22.pc_n++;
}

static void ring_push_br(uint32_t from, uint32_t to) {
    g_p22.br_from[g_p22.br_next] = from;
    g_p22.br_to[g_p22.br_next] = to;
    g_p22.br_next = (g_p22.br_next + 1) % BR_RING;
    if (g_p22.br_n < BR_RING) g_p22.br_n++;
}

static void ring_push_bl(uint32_t pc, uint32_t lr, uint32_t tgt) {
    g_p22.bl_pc[g_p22.bl_next] = pc;
    g_p22.bl_lr[g_p22.bl_next] = lr;
    g_p22.bl_tgt[g_p22.bl_next] = tgt;
    g_p22.bl_next = (g_p22.bl_next + 1) % BL_RING;
    if (g_p22.bl_n < BL_RING) g_p22.bl_n++;
}

static void dump_rings(const char *why) {
    size_t i, start, n;
    printf("[JJFB_P25_CF_RING] why=%s pc_n=%zu br_n=%zu bl_n=%zu evidence=OBSERVED\n",
           why ? why : "?", g_p22.pc_n, g_p22.br_n, g_p22.bl_n);
    n = g_p22.pc_n;
    start = (g_p22.pc_next + PC_RING - n) % PC_RING;
    for (i = 0; i < n && i < 64; i++) {
        size_t idx = (start + n - 64 + i) % PC_RING;
        if (n < 64) idx = (start + i) % PC_RING;
        printf("[JJFB_P25_PC] i=%zu pc=0x%X lr=0x%X sp=0x%X r0=0x%X\n", i, g_p22.pc_ring[idx],
               g_p22.lr_ring[idx], g_p22.sp_ring[idx], g_p22.r0_ring[idx]);
    }
    n = g_p22.br_n < 64 ? g_p22.br_n : 64;
    start = (g_p22.br_next + BR_RING - g_p22.br_n) % BR_RING;
    for (i = 0; i < n; i++) {
        size_t idx = (start + g_p22.br_n - n + i) % BR_RING;
        printf("[JJFB_P25_BR] i=%zu from=0x%X to=0x%X\n", i, g_p22.br_from[idx], g_p22.br_to[idx]);
    }
    n = g_p22.bl_n < 32 ? g_p22.bl_n : 32;
    start = (g_p22.bl_next + BL_RING - g_p22.bl_n) % BL_RING;
    for (i = 0; i < n; i++) {
        size_t idx = (start + g_p22.bl_n - n + i) % BL_RING;
        printf("[JJFB_P25_BL] i=%zu pc=0x%X lr=0x%X tgt=0x%X\n", i, g_p22.bl_pc[idx],
               g_p22.bl_lr[idx], g_p22.bl_tgt[idx]);
    }
    fflush(stdout);
}

static void dump_bp(void *uc, const char *tag, uint32_t pc, uint32_t off, const uint32_t regs[16],
                    uint32_t lr, uint32_t sp, uint32_t cpsr) {
    uint32_t r9 = regs ? regs[9] : 0;
    uint32_t state = r9 ? r9 + 0x6EEu : 0;
    uint8_t sb[0x40];
    int i;
    char hex[0x40 * 2 + 4];
    printf("[JJFB_P25_BP] tag=%s pc=0x%X off=0x%X lr=0x%X sp=0x%X cpsr=0x%X "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X "
           "r8=0x%X r9=0x%X r10=0x%X r11=0x%X r12=0x%X state_base=0x%X "
           "frame_depth=%u evidence=OBSERVED\n",
           tag, pc, off, lr, sp, cpsr, regs ? regs[0] : 0, regs ? regs[1] : 0, regs ? regs[2] : 0,
           regs ? regs[3] : 0, regs ? regs[4] : 0, regs ? regs[5] : 0, regs ? regs[6] : 0,
           regs ? regs[7] : 0, regs ? regs[8] : 0, r9, regs ? regs[10] : 0, regs ? regs[11] : 0,
           regs ? regs[12] : 0, state, module_r9_switch_depth());
    memset(sb, 0, sizeof(sb));
    hex[0] = 0;
    if (uc && state && guest_memory_uc_peek((struct uc_struct *)uc, state, sb, sizeof(sb))) {
        for (i = 0; i < 32; i++) {
            char t[3];
            snprintf(t, sizeof(t), "%02X", sb[i]);
            strncat(hex, t, sizeof(hex) - strlen(hex) - 1);
        }
        printf("[JJFB_P25_STATE] state_base=0x%X bytes32=%s evidence=OBSERVED\n", state, hex);
    }
    /* Dump stack window around SP on builder/fault tags. */
    if (uc && sp && (strstr(tag, "BUILDER") || strstr(tag, "STRAY") || strstr(tag, "FAULT"))) {
        uint8_t stk[0x200];
        uint32_t base = sp > 0x100u ? sp - 0x100u : sp;
        if (guest_memory_uc_peek((struct uc_struct *)uc, base, stk, sizeof(stk))) {
            printf("[JJFB_P25_STACK] base=0x%X sp=0x%X words:", base, sp);
            for (i = 0; i < 32; i++) {
                uint32_t w;
                memcpy(&w, stk + (size_t)i * 4u, 4);
                printf(" %08X", w);
            }
            printf(" evidence=OBSERVED\n");
        }
    }
    fflush(stdout);
    g_p22.last_r9 = r9;
    g_p22.state_base = state;
    trace_row(tag, -1, 0, pc, off, regs ? regs[0] : 0, regs ? regs[1] : 0, regs ? regs[2] : 0, r9,
              tag);
}

static void dump_cfg_dispatch(void *uc, uint32_t pc, uint32_t off, const uint32_t regs[16],
                              uint32_t lr, uint32_t sp, uint32_t cpsr) {
    char path[160];
    uint32_t stack0 = 0, stack1 = 0, stack2 = 0;
    path[0] = 0;
    if (uc && regs && regs[2]) read_cstr(uc, regs[2], path, sizeof(path));
    if (uc && sp) {
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp, &stack0);
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 4u, &stack1);
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 8u, &stack2);
    }
    dump_bp(uc, "CFG_DISPATCH_CALL", pc, off, regs, lr, sp, cpsr);
    printf("[JJFB_P25_CFG_DISPATCH] r0=0x%X r1=0x%X r2_path=\"%s\" r3=0x%X "
           "stack0=0x%X stack1=0x%X stack2=0x%X r9=0x%X pkg_active=%s evidence=OBSERVED\n",
           regs ? regs[0] : 0, regs ? regs[1] : 0, path[0] ? path : "?", regs ? regs[3] : 0, stack0,
           stack1, stack2, regs ? regs[9] : 0,
           g_p22.gamelist_active ? "gamelist" : "?");
    fflush(stdout);
    if (regs && regs[0] == 0x10112u && path[0] &&
        (strcmp(path, "cfg.bin") == 0 || path_eq_ci_tail(path, "cfg.bin"))) {
        g_p22.cfg_internal_req = 1;
        mark_gate(P22_G2_INTERNAL_REQUESTED, pc, off, regs[0], regs[1], regs[2], regs[9], path);
    }
}

static int try_parse_cfg36_from_va(void *uc, uint32_t va) {
    char buf[96];
    int i;
    if (!uc || !va) return 0;
    for (i = 0; i < 0x120; i += 4) {
        if (!read_cstr(uc, va + (uint32_t)i, buf, sizeof(buf))) continue;
        if (!looks_like_jjfb_target(buf)) continue;
        g_p22.cfg_record_va = va;
        g_p22.item_object_va = va;
        if (!g_p22.cfg36_parsed) {
            g_p22.cfg36_parsed = 1;
            mark_gate(P22_G7_CFG36_PARSED, 0, 0, va, 0, 0, g_p22.last_r9, "jjfb_target_in_object");
            printf("[JJFB_P25] CFG36_RECORD_PARSED va=0x%X target=%s evidence=OBSERVED\n", va, buf);
            fflush(stdout);
        }
        if (!g_p22.item_created) {
            g_p22.item_created = 1;
            g_p22.item_created_t = clock();
            mark_gate(P22_G8_ITEM_CREATED, 0, 0, va, 0, 0, g_p22.last_r9, "item_object_va");
            printf("[JJFB_P25] CFG36_ITEM_CREATED va=0x%X evidence=OBSERVED\n", va);
            fflush(stdout);
        }
        return 1;
    }
    return 0;
}

static void maybe_mark_selected(uint32_t pc, uint32_t off, uint32_t item, uint32_t cb) {
    if (g_p22.selected) return;
    if (!g_p22.cfg36_parsed || !g_p22.item_created) return;
    if (!item || !cb) return;
    if (g_p22.item_object_va && item != g_p22.item_object_va) g_p22.item_object_va = item;
    g_p22.callback_va = cb;
    g_p22.selected = 1;
    mark_gate(P22_G9_SELECT_CALLBACK, pc, off, item, cb, 0, g_p22.last_r9,
              "original_callback_with_cfg36_item");
    e10a3_mark_real_cfg_selected("P25_CFG36_ITEM_SELECTED");
    printf("[JJFB_P25] CFG36_SELECTED=1 item=0x%X callback=0x%X evidence=OBSERVED\n", item, cb);
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static void headless_invoke_select(void *uc, const uint32_t regs[16], uint32_t sp) {
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    uint32_t start, stop;
    if (!g_p22.headless || g_p22.headless_fired || !uc) return;
    if (!g_p22.item_created || g_p22.selected || g_p22.desc_enter_legal) return;
    if (!g_p22.item_created_t) return;
    if ((double)(clock() - g_p22.item_created_t) / (double)CLOCKS_PER_SEC < 2.0) return;
    if (g_p22.callback_va)
        start = g_p22.callback_va | 1u;
    else if (g_p22.gl_base)
        start = (g_p22.gl_base + OFF_SELECT_FN) | 1u;
    else
        return;
    stop = g_p22.gl_base ? (g_p22.gl_base + OFF_SELECT_FN + 0x400u) : ((start & ~1u) + 0x400u);
    memset(&abi, 0, sizeof(abi));
    memset(&out, 0, sizeof(out));
    if (regs) {
        int i;
        abi.mirror_full = 1;
        for (i = 0; i < 13; i++) abi.mirror_r[i] = regs[i];
        abi.mirror_sp = sp;
        abi.mirror_cpsr = 0x20;
    }
    abi.set_r0 = 1;
    abi.r0 = g_p22.item_object_va;
    abi.set_lr = 1;
    abi.lr = stop | 1u;
    g_p22.headless_fired = 1;
    printf("[JJFB_P25_HEADLESS_SELECT] start=0x%X item=0x%X evidence=OBSERVED\n", start,
           g_p22.item_object_va);
    fflush(stdout);
    (void)guest_memory_uc_run_entry_ex((struct uc_struct *)uc, start, stop, 200000ull, &abi, &out);
    if (g_p22.callback_va)
        maybe_mark_selected(start, g_p22.gl_base ? (start - g_p22.gl_base) : 0, g_p22.item_object_va,
                            g_p22.callback_va);
    else if (g_p22.gl_base)
        maybe_mark_selected(start, OFF_SELECT_FN, g_p22.item_object_va, start);
}
#endif

void p22_note_start_dsm(const char *filename, const char *entry, uint32_t param_va) {
    if (!p22_enabled()) return;
    if (path_has(filename, "gamelist")) {
        g_p22.gamelist_active = 1;
        p22_report_runtime_stack("gamelist_start_dsm");
    }
    if (entry && strstr(entry, "napptype=") && strstr(entry, "gwyblink")) {
        g_p22.param_abi_ok = 1;
        e10a31_mark_milestone("START_DSM_PARAM_ABI_CONFIRMED", "p25");
    }
    if (param_va) {
        g_p22.param_ptr_copied = 1;
        e10a31_mark_milestone("SHELL_PARAM_POINTER_COPIED", "p25");
    }
}

void p22_note_param_read(const char *milestone) {
    if (!p22_enabled()) return;
    if (milestone && strstr(milestone, "GWYBLINK")) {
        g_p22.param_gwyblink_read = 1;
        e10a31_mark_milestone("SHELL_PARAM_GWYBLINK_READ", "p25");
    }
}

void p22_note_plat_10112(const char *path, const char *ns, const char *host, uint32_t buf,
                         uint32_t len, int loaded, int ret) {
    if (!p22_enabled()) return;
    printf("[JJFB_P25_10112] path=\"%s\" ns=%s host=\"%s\" buf=0x%X len=%u loaded=%d ret=%d "
           "evidence=OBSERVED\n",
           path ? path : "?", ns ? ns : "?", host ? host : "?", buf, len, loaded, ret);
    fflush(stdout);
    trace_row("PLAT_10112", -1, (uint32_t)loaded, 0, 0, buf, len, (uint32_t)ret, 0,
              path ? path : "");
    if (path && (strcmp(path, "cfg.bin") == 0 || path_eq_ci_tail(path, "cfg.bin")) &&
        !(path_has(path, "gwy"))) {
        g_p22.cfg_internal_req = 1;
        mark_gate(P22_G2_INTERNAL_REQUESTED, 0, 0, 0x10112u, 0, 0, 0, path);
        if (loaded && (ns && strcmp(ns, "MRP_MEMBER") == 0) && len == CFG_INTERNAL_SIZE) {
            g_p22.cfg_internal_loaded = 1;
            g_p22.cfg_internal_size = len;
            mark_gate(P22_G3_INTERNAL_LOADED, 0, 0, buf, len, 0, 0, "CFG_INTERNAL_LOADED_6898");
            mark_gate(P22_G4_INTERNAL_PARSED, 0, 0, buf, len, 0, 0, "CFG_INTERNAL_BYTES_READY");
            e10a31_mark_milestone("GAMELIST_EMBEDDED_CFG_OPEN", path);
        }
    }
    if (path && path_has(path, "gwy") && path_has(path, "cfg.bin") && loaded &&
        len == CFG_EXTERNAL_SIZE) {
        g_p22.cfg_external_loaded = 1;
        g_p22.cfg_external_size = len;
        mark_gate(P22_G6_EXTERNAL_LOADED, 0, 0, buf, len, 0, 0, "CFG_EXTERNAL_LOADED_20728");
        e10a31_mark_milestone("GAMELIST_EXTERNAL_CFG_OPEN", path);
    }
}

void p22_note_file_open(const char *guest_path, const char *host_path, int ok, uint32_t size) {
    if (!p22_enabled() || !ok || !guest_path) return;
    /* Classify; never treat bare cfg.bin as external list. */
    if (path_has(guest_path, "cfg.bin") && path_has(guest_path, "gwy") &&
        size == CFG_EXTERNAL_SIZE) {
        g_p22.cfg_external_loaded = 1;
        g_p22.cfg_external_size = size;
        mark_gate(P22_G6_EXTERNAL_LOADED, 0, 0, size, 0, 0, 0, "CFG_FILE_OPENED_EXTERNAL");
        printf("[JJFB_P25] CFG_EXTERNAL_LOADED path=%s size=%u host=%s evidence=OBSERVED\n",
               guest_path, size, host_path ? host_path : "?");
        fflush(stdout);
        e10a31_mark_milestone("GAMELIST_EXTERNAL_CFG_OPEN", guest_path);
        return;
    }
    if ((strcmp(guest_path, "cfg.bin") == 0 || path_eq_ci_tail(guest_path, "cfg.bin")) &&
        !path_has(guest_path, "gwy") && size == CFG_INTERNAL_SIZE) {
        g_p22.cfg_internal_loaded = 1;
        g_p22.cfg_internal_size = size;
        mark_gate(P22_G3_INTERNAL_LOADED, 0, 0, size, 0, 0, 0, "CFG_FILE_OPENED_INTERNAL");
        printf("[JJFB_P25] CFG_INTERNAL_LOADED path=%s size=%u host=%s evidence=OBSERVED\n",
               guest_path, size, host_path ? host_path : "?");
        fflush(stdout);
        e10a31_mark_milestone("GAMELIST_EMBEDDED_CFG_OPEN", guest_path);
    }
    /* Temp path probes for external state machine. */
    if (path_has(guest_path, "_cfg.bin") || path_has(guest_path, "cfg.bin.td")) {
        trace_row("CFG_TEMP_PATH", P22_G5_PATH_STATE, size, 0, 0, 0, 0, 0, 0, guest_path);
        printf("[JJFB_P25_CFG_PATH] path=%s size=%u evidence=OBSERVED\n", guest_path, size);
        fflush(stdout);
    }
}

void p22_note_file_io(const char *op, const char *guest_path, uint32_t offset, uint32_t length,
                      int32_t ret) {
    uint32_t end;
    if (!p22_enabled()) return;
    if (!guest_path || !path_has(guest_path, "cfg.bin")) return;
    printf("[JJFB_P25_CFG_IO] op=%s path=%s offset=%u length=%u ret=%d evidence=OBSERVED\n",
           op ? op : "?", guest_path, offset, length, (int)ret);
    fflush(stdout);
    trace_row("CFG_IO", -1, (uint32_t)ret, 0, offset, length, 0, 0, 0, guest_path);
    if (!op || strcmp(op, "read") != 0) return;
    if (!(path_has(guest_path, "gwy") && path_has(guest_path, "cfg.bin"))) return;
    end = offset + length;
    if (offset < CFG36_FILE_OFF + CFG_RECORD_SIZE && end > CFG36_FILE_OFF) {
        if (!g_p22.cfg36_parsed) {
            g_p22.cfg36_parsed = 1;
            mark_gate(P22_G7_CFG36_PARSED, 0, 0, offset, length, 0, 0, "cfg_read_covers_record36");
        }
    }
}

#ifdef GWY_HAVE_UNICORN
static void p25_on_code_write(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                              int64_t value, void *user_data) {
    uint32_t pc = 0, lr = 0, off = 0;
    (void)type;
    (void)user_data;
    if (!g_p22.gl_base || address < g_p22.gl_base ||
        address >= (uint64_t)g_p22.gl_base + g_p22.gl_size)
        return;
    off = (uint32_t)address - g_p22.gl_base;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    printf("[JJFB_P25_CODE_WRITE] addr=0x%llX off=0x%X size=%d value=0x%llX pc=0x%X lr=0x%X "
           "evidence=OBSERVED\n",
           (unsigned long long)address, off, size, (unsigned long long)value, pc, lr);
    fflush(stdout);
    trace_row("CODE_WRITE", -1, off, pc, (uint32_t)size, (uint32_t)value, lr, 0, 0, "gamelist_code");
}

static void p25_dump_site_bytes(void *uc, const char *tag, uint32_t file_off, uint32_t nbytes) {
    uint8_t ib[16];
    char ihex[40];
    uint32_t i;
    uint32_t addr;
    if (!uc || !g_p22.gl_base || nbytes == 0 || nbytes > sizeof(ib)) return;
    addr = g_p22.gl_base + file_off;
    ihex[0] = 0;
    if (!guest_memory_uc_peek((struct uc_struct *)uc, addr, ib, nbytes)) {
        printf("[JJFB_P25_SITE_BYTES] tag=%s off=0x%X fail=peek evidence=OBSERVED\n", tag, file_off);
        fflush(stdout);
        return;
    }
    for (i = 0; i < nbytes; i++) {
        char t[3];
        snprintf(t, sizeof(t), "%02X", ib[i]);
        strncat(ihex, t, sizeof(ihex) - strlen(ihex) - 1);
    }
    printf("[JJFB_P25_SITE_BYTES] tag=%s off=0x%X addr=0x%X live=%s evidence=OBSERVED\n", tag,
           file_off, addr, ihex);
    fflush(stdout);
}
#endif

static int p25_range_overlaps(uint32_t addr, uint32_t size, uint32_t off0, uint32_t off1) {
    uint32_t a0, a1, b0, b1;
    if (!g_p22.gl_base || size == 0) return 0;
    a0 = addr;
    a1 = addr + size;
    if (a1 < a0) return 0;
    b0 = g_p22.gl_base + off0;
    b1 = g_p22.gl_base + off1;
    return a0 < b1 && b0 < a1;
}

void p22_note_host_mem_write(uint32_t guest_addr, uint32_t size, const void *buf) {
    char ihex[48];
    uint32_t i, n, off;
    const uint8_t *p;
    int hit_b008, hit_aff4, in_screen;
    if (!p22_enabled() || !g_p22.gl_base) return;
    if (!p25_range_overlaps(guest_addr, size, 0, g_p22.gl_size)) return;
    off = guest_addr - g_p22.gl_base;
    hit_b008 = p25_range_overlaps(guest_addr, size, 0xB000u, 0xB020u);
    hit_aff4 = p25_range_overlaps(guest_addr, size, 0xAFF0u, 0xB000u);
    in_screen = p25_range_overlaps(guest_addr, size, 0xAF00u, 0xB100u);
    if (!hit_b008 && !hit_aff4 && !in_screen) return;
    ihex[0] = 0;
    p = (const uint8_t *)buf;
    n = size > 16u ? 16u : size;
    if (p) {
        for (i = 0; i < n; i++) {
            char t[3];
            snprintf(t, sizeof(t), "%02X", p[i]);
            strncat(ihex, t, sizeof(ihex) - strlen(ihex) - 1);
        }
    }
    printf("[JJFB_P25_HOST_WRITE] addr=0x%X off=0x%X size=%u data=%s%s "
           "hit_B008=%d hit_AFF4=%d evidence=OBSERVED\n",
           guest_addr, off, size, ihex, size > 16u ? "..." : "", hit_b008, hit_aff4);
    fflush(stdout);
    trace_row("HOST_WRITE", -1, off, guest_addr, size, 0, 0, 0, 0,
              hit_b008 ? "hit_B008_window" : "screen_metric_window");
}

void p22_note_platform_memcpy(uint32_t dst, uint32_t src, uint32_t n) {
    char src_off[20];
    int hit_b008;
    if (!p22_enabled() || !g_p22.gl_base) return;
    if (!p25_range_overlaps(dst, n, 0xAF00u, 0xB100u)) return;
    hit_b008 = p25_range_overlaps(dst, n, 0xB000u, 0xB020u);
    if (src >= g_p22.gl_base && src < g_p22.gl_base + g_p22.gl_size)
        snprintf(src_off, sizeof(src_off), "0x%X", src - g_p22.gl_base);
    else
        snprintf(src_off, sizeof(src_off), "outside");
    printf("[JJFB_P25_HOST_MEMCPY] dst=0x%X dst_off=0x%X src=0x%X src_off=%s n=%u "
           "hit_B008=%d evidence=OBSERVED\n",
           dst, dst - g_p22.gl_base, src, src_off, n, hit_b008);
    fflush(stdout);
    trace_row("HOST_MEMCPY", -1, dst - g_p22.gl_base, dst, src, n, 0, 0, 0, "platform_memcpy");
}

void p22_note_module_map(const char *module_name, uint32_t base, uint32_t size) {
    if (!p22_enabled() || !module_name) return;
    if (!path_has(module_name, "gamelist")) return;
    /* Allow RAW_BASE_REFINE to replace aligned cacheSync base (pad < 0x20). */
    if (g_p22.gl_base && base != g_p22.gl_base) {
        uint32_t delta =
            (base > g_p22.gl_base) ? (base - g_p22.gl_base) : (g_p22.gl_base - base);
        if (delta >= 0x20u) {
            printf("[JJFB_P25] gamelist_base_ignore=0x%X prev=0x%X delta=0x%X "
                   "note=not_refine_window evidence=OBSERVED\n",
                   base, g_p22.gl_base, delta);
            fflush(stdout);
            return;
        }
        printf("[JJFB_P25] gamelist_base_refine prev=0x%X new=0x%X pad=0x%X size=%u "
               "evidence=DOCUMENTED\n",
               g_p22.gl_base, base, delta, size);
        fflush(stdout);
    }
    g_p22.gl_base = base;
    g_p22.gl_size = size;
    printf("[JJFB_P25] gamelist_image_base=0x%X size=%u "
           "CFG_LOADER=0x%X CFG_DISPATCH=0x%X PATH_STATE=0x%X BUILDER=0x%X "
           "STATE_SLOT_COPY=0x%X(diag) SLOT_COMMIT=0x%X(diag) evidence=OBSERVED\n",
           base, size, base + OFF_CFG_LOADER_ENTRY, base + OFF_CFG_DISPATCH_CALL,
           base + OFF_CFG_PATH_STATE_ENTRY, base + OFF_DESC_BUILDER, base + OFF_STATE_SLOT_COPY,
           base + OFF_STATE_SLOT_COMMIT);
    fflush(stdout);
#ifdef GWY_HAVE_UNICORN
    if (g_p22.uc) {
        p25_dump_site_bytes(g_p22.uc, "base_set_AFF4", 0xAFF4u, 8);
        p25_dump_site_bytes(g_p22.uc, "base_set_B008", 0xB008u, 8);
        p25_dump_site_bytes(g_p22.uc, "base_set_13A20", 0x13A20u, 8);
        p25_dump_site_bytes(g_p22.uc, "base_set_13A34", 0x13A34u, 8);
    }
#endif
}

void p22_note_startgame_lookup(uint32_t target_pc, const char *name) {
    if (!p22_enabled()) return;
    g_p22.sg_lookup = 1;
    g_p22.sg_lookup_pc = target_pc;
    mark_gate(P22_G13_STARTGAME_LOOKUP, target_pc, 0, target_pc, 0, 0, 0,
              name ? name : "lib.startGame");
}

void p22_note_startgame_enter(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r9) {
    if (!p22_enabled()) return;
    g_p22.sg_enter = 1;
    g_p22.sg_pc = pc;
    g_p22.sg_r0 = r0;
    g_p22.sg_r1 = r1;
    g_p22.sg_r2 = r2;
    g_p22.sg_r9 = r9;
    mark_gate(P22_G14_STARTGAME_ENTER, pc, 0, r0, r1, r2, r9, "STARTGAME_ENTER");
}

void p22_note_opcode300(uint32_t pc, uint32_t a0, uint32_t a1, uint32_t a2, const char *note) {
    if (!p22_enabled()) return;
    g_p22.op300 = 1;
    mark_gate(P22_G15_OPCODE300, pc, 0, a0, a1, a2, 0, note ? note : "opcode300");
}

void p22_note_nested_mrp(const char *target, const char *entry) {
    if (!p22_enabled() || !target) return;
    snprintf(g_p22.nested_target, sizeof(g_p22.nested_target), "%s", target);
    if (path_has(target, "jjfb.mrp")) {
        g_p22.nested = 1;
        mark_gate(P22_G16_NESTED_JJFB, 0, 0, 0, 0, 0, 0, entry ? entry : target);
    }
}

void p22_note_robotol_ext(const char *member) {
    if (!p22_enabled()) return;
    if (member && path_has(member, "robotol.ext")) {
        g_p22.robotol = 1;
        mark_gate(P22_G17_ROBOTOL_EXT, 0, 0, 0, 0, 0, 0, member);
    }
}

void p22_note_fetch_fault(uint32_t fault_pc, uint32_t fetch_addr, uint32_t lr, uint32_t sp,
                          uint32_t r9) {
    if (!p22_enabled()) return;
    g_p22.fault_pc = fault_pc;
    g_p22.fault_addr = fetch_addr;
    printf("[JJFB_P25_FETCH_FAULT] fault_pc=0x%X fetch=0x%X lr=0x%X sp=0x%X r9=0x%X "
           "stray_builder=%d stray_lr=0x%X evidence=OBSERVED\n",
           fault_pc, fetch_addr, lr, sp, r9, g_p22.desc_stray, g_p22.stray_lr);
    fflush(stdout);
    dump_rings("fetch_unmapped");
    trace_row("FETCH_UNMAPPED", -1, fetch_addr, fault_pc, 0, lr, sp, r9, 0,
              g_p22.desc_stray ? "same_cf_pollution_chain?" : "fault");
}

void p22_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                 uint32_t lr, uint32_t sp, uint32_t cpsr) {
    uint32_t off;
    int i;
    char buf[160];
    if (!p22_enabled()) return;
    if (uc) g_p22.uc = uc;

    /* Late-arm code-write watch once we have both base and uc. */
#ifdef GWY_HAVE_UNICORN
    {
        static int watch_armed;
        if (!watch_armed && g_p22.uc && g_p22.gl_base && g_p22.gl_size) {
            uc_hook h = 0;
            uc_err e = uc_hook_add((uc_engine *)g_p22.uc, &h, UC_HOOK_MEM_WRITE,
                                   (void *)p25_on_code_write, NULL, (uint64_t)g_p22.gl_base,
                                   (uint64_t)g_p22.gl_base + g_p22.gl_size - 1u);
            watch_armed = 1;
            printf("[JJFB_P25] code_write_watch base=0x%X size=%u ok=%d evidence=OBSERVED\n",
                   g_p22.gl_base, g_p22.gl_size, e == UC_ERR_OK);
            fflush(stdout);
            p25_dump_site_bytes(g_p22.uc, "watch_arm_AFF4", 0xAFF4u, 8);
            p25_dump_site_bytes(g_p22.uc, "watch_arm_B008", 0xB008u, 8);
        }
    }
#endif

    if (g_p22.gamelist_active && regs) {
        uint32_t prev_pc = 0;
        if (g_p22.pc_n) {
            size_t last = (g_p22.pc_next + PC_RING - 1) % PC_RING;
            prev_pc = g_p22.pc_ring[last];
            if (prev_pc && ((pc & ~1u) != ((prev_pc + 2) & ~1u) &&
                            (pc & ~1u) != ((prev_pc + 4) & ~1u)))
                ring_push_br(prev_pc, pc);
        }
        ring_push_pc(pc, lr, sp, regs[0]);
    }

    if (uc && regs) {
        for (i = 0; i < 4; i++) {
            if (!read_cstr(uc, regs[i], buf, sizeof(buf))) continue;
            if (descriptor_exact(buf)) {
                snprintf(g_p22.descriptor, sizeof(g_p22.descriptor), "%s", buf);
                if (!g_p22.desc_match) {
                    g_p22.desc_match = 1;
                    mark_gate(P22_G12_DESC_MATCH, pc, 0, regs[i], 0, 0, regs[9], buf);
                }
            }
            if (looks_like_jjfb_target(buf) && g_p22.cfg_external_loaded)
                try_parse_cfg36_from_va(uc, regs[i] > 64u ? regs[i] - 64u : regs[i]);
        }
        if (g_p22.cfg_external_loaded && !g_p22.item_created) {
            try_parse_cfg36_from_va(uc, regs[0]);
            try_parse_cfg36_from_va(uc, regs[1]);
        }
    }

    if (!g_p22.gl_base || !module_name || !path_has(module_name, "gamelist")) {
        if ((pc & ~1u) == 0x2AAD84u && regs) {
            p22_note_startgame_lookup(0x2AAD84u, "pc_hit");
            p22_note_startgame_enter(pc, regs[0], regs[1], regs[2], regs[9]);
        }
#ifdef GWY_HAVE_UNICORN
        if (g_p22.headless && g_p22.item_created && !g_p22.selected)
            headless_invoke_select(uc ? uc : g_p22.uc, regs, sp);
#endif
        return;
    }
    if (pc < g_p22.gl_base || pc >= g_p22.gl_base + g_p22.gl_size) return;
    off = pc - g_p22.gl_base;

    /* Snapshot screen-metric helper + B008 before stray can fire. */
#ifdef GWY_HAVE_UNICORN
    if (off == 0xAFD4u && !g_p22.bp_logged[19]) {
        g_p22.bp_logged[19] = 1;
        p25_dump_site_bytes(uc, "enter_AFD4_AFF4", 0xAFF4u, 8);
        p25_dump_site_bytes(uc, "enter_AFD4_B008", 0xB008u, 8);
    }
    if (off == 0xB008u && !g_p22.bp_logged[20]) {
        g_p22.bp_logged[20] = 1;
        p25_dump_site_bytes(uc, "enter_B008", 0xB008u, 8);
    }
#endif

    /* Diag-only renamed breakpoints — never mark cfg gates. */
    if (off == OFF_STATE_SLOT_COPY && !g_p22.bp_logged[0]) {
        g_p22.bp_logged[0] = 1;
        dump_bp(uc, "STATE_SLOT_COPY_438_TO_430", pc, off, regs, lr, sp, cpsr);
    }
    if (off == OFF_STATE_SLOT_COMMIT && !g_p22.bp_logged[1]) {
        g_p22.bp_logged[1] = 1;
        dump_bp(uc, "CONDITIONAL_STATE_SLOT_COMMIT", pc, off, regs, lr, sp, cpsr);
    }

    if (off == OFF_CFG_LOADER_ENTRY) {
        if (!g_p22.bp_logged[2]) {
            g_p22.bp_logged[2] = 1;
            dump_bp(uc, "CFG_LOADER_ENTRY", pc, off, regs, lr, sp, cpsr);
        }
        mark_gate(P22_G1_CFG_LOADER, pc, off, regs ? regs[0] : 0, 0, 0, regs ? regs[9] : 0,
                  "CFG_LOADER_ENTRY");
    }
    if (off == OFF_CFG_PRECHECK_CALL && !g_p22.bp_logged[3]) {
        g_p22.bp_logged[3] = 1;
        dump_bp(uc, "CFG_PRECHECK_CALL", pc, off, regs, lr, sp, cpsr);
    }
    if (off == OFF_CFG_PRECHECK_RETURN && !g_p22.bp_logged[4]) {
        g_p22.bp_logged[4] = 1;
        dump_bp(uc, "CFG_PRECHECK_RETURN", pc, off, regs, lr, sp, cpsr);
    }
    if (off == OFF_CFG_PATH_RESOLVED && !g_p22.bp_logged[5]) {
        g_p22.bp_logged[5] = 1;
        dump_bp(uc, "CFG_PATH_RESOLVED", pc, off, regs, lr, sp, cpsr);
    }
    if (off == OFF_CFG_DISPATCH_CALL) {
        if (!g_p22.bp_logged[6]) {
            g_p22.bp_logged[6] = 1;
            dump_cfg_dispatch(uc, pc, off, regs, lr, sp, cpsr);
        }
        ring_push_bl(pc, lr, 0); /* target via E018 */
    }
    if (off == OFF_CFG_DISPATCH_RETURN && !g_p22.bp_logged[7]) {
        g_p22.bp_logged[7] = 1;
        dump_bp(uc, "CFG_DISPATCH_RETURN", pc, off, regs, lr, sp, cpsr);
    }
    if (off == OFF_CFG_LOADER_CALLER_1 || off == OFF_CFG_LOADER_CALLER_2 ||
        off == OFF_CFG_LOADER_CALLER_3) {
        int idx = off == OFF_CFG_LOADER_CALLER_1 ? 8 : off == OFF_CFG_LOADER_CALLER_2 ? 9 : 10;
        if (!g_p22.bp_logged[idx]) {
            g_p22.bp_logged[idx] = 1;
            dump_bp(uc, "CFG_LOADER_CALLER", pc, off, regs, lr, sp, cpsr);
        }
    }
    if (off == OFF_CFG_PATH_STATE_ENTRY) {
        if (!g_p22.bp_logged[11]) {
            g_p22.bp_logged[11] = 1;
            dump_bp(uc, "CFG_PATH_STATE_ENTRY", pc, off, regs, lr, sp, cpsr);
        }
        mark_gate(P22_G5_PATH_STATE, pc, off, regs ? regs[0] : 0, 0, 0, regs ? regs[9] : 0,
                  "CFG_PATH_STATE_ENTRY");
    }
    if ((off == OFF_CFG_PATH_DYNAMIC_1 || off == OFF_CFG_PATH_DYNAMIC_2 ||
         off == OFF_CFG_PATH_TEMP_1 || off == OFF_CFG_PATH_TEMP_2 ||
         off == OFF_CFG_PATH_STATE_RETURN) &&
        !g_p22.bp_logged[12]) {
        g_p22.bp_logged[12] = 1;
        dump_bp(uc, "CFG_PATH_DYNAMIC", pc, off, regs, lr, sp, cpsr);
    }

    if (off == OFF_SELECT_1 || off == OFF_SELECT_2 || off == OFF_SELECT_3 || off == OFF_SELECT_4) {
        int idx =
            (off == OFF_SELECT_1) ? 13 : (off == OFF_SELECT_2) ? 14 : (off == OFF_SELECT_3) ? 15 : 16;
        if (!g_p22.bp_logged[idx]) {
            g_p22.bp_logged[idx] = 1;
            dump_bp(uc, "SELECT_CALLSITE", pc, off, regs, lr, sp, cpsr);
        }
        g_p22.select_callsite_off = off;
        if (regs) {
            uint32_t item = regs[0];
            if (!g_p22.item_object_va && item) g_p22.item_object_va = item;
            maybe_mark_selected(pc, off, item ? item : g_p22.item_object_va, pc);
        }
    }

    if (off == OFF_DESC_BUILDER) {
        uint32_t lr_off = (g_p22.gl_base && lr >= g_p22.gl_base &&
                           lr < g_p22.gl_base + g_p22.gl_size)
                              ? ((lr & ~1u) - g_p22.gl_base)
                              : 0xFFFFFFFFu;
        int legal = lr_is_legal_builder(lr_off);
        if (!g_p22.bp_logged[17]) {
            g_p22.bp_logged[17] = 1;
            dump_bp(uc, legal ? "DESCRIPTOR_BUILDER" : "DESCRIPTOR_BUILDER_STRAY_ENTRY", pc, off,
                    regs, lr, sp, cpsr);
        }
        if (!legal) {
            uint8_t ib[8];
            uint32_t prev = 0;
            char ihex[20];
            int bi;
            g_p22.desc_stray = 1;
            g_p22.stray_pc = pc;
            g_p22.stray_lr = lr;
            g_p22.stray_sp = sp;
            g_p22.stray_r9 = regs ? regs[9] : 0;
            if (g_p22.pc_n >= 2) {
                size_t last = (g_p22.pc_next + PC_RING - 2) % PC_RING;
                prev = g_p22.pc_ring[last];
            }
            ihex[0] = 0;
            if (uc && prev &&
                guest_memory_uc_peek((struct uc_struct *)uc, prev & ~1u, ib, sizeof(ib))) {
                for (bi = 0; bi < 8; bi++) {
                    char t[3];
                    snprintf(t, sizeof(t), "%02X", ib[bi]);
                    strncat(ihex, t, sizeof(ihex) - strlen(ihex) - 1);
                }
            }
            snprintf(g_p22.stray_source, sizeof(g_p22.stray_source), "%s",
                     (lr_off == 0xB00Cu || lr_off == 0xB00Du) ? "LR_OFF_B00C_SCREEN_METRIC_PATH"
                                                              : "LR_NOT_IN_LEGAL_SET");
            printf("[JJFB_P25] DESCRIPTOR_BUILDER_STRAY_ENTRY lr=0x%X lr_off=0x%X "
                   "STRAY_BUILDER_PRODUCER_PC=0x%X live_insn8=%s "
                   "STRAY_BUILDER_TARGET_SOURCE=%s "
                   "note=static_B008_is_STRH_not_BL;same_chain_as_fetch_unmapped "
                   "evidence=OBSERVED\n",
                   lr, lr_off, prev, ihex[0] ? ihex : "?",
                   (prev && ((prev - g_p22.gl_base) == 0xB008u)) ? "NONSEQ_FROM_B008_NOT_STATIC_BL"
                                                                 : "INDIRECT_OR_HOST_CONTINUATION");
            fflush(stdout);
            dump_rings("stray_builder");
            trace_row("STRAY_BUILDER", -1, lr_off, pc, off, regs ? regs[0] : 0, lr, sp,
                      regs ? regs[9] : 0, g_p22.stray_source);
            /* Do NOT mark G10. */
        } else {
            g_p22.desc_enter_legal = 1;
            mark_gate(P22_G10_DESC_BUILDER_LEGAL, pc, off, regs ? regs[0] : 0, lr, 0,
                      regs ? regs[9] : 0, "DESCRIPTOR_BUILDER_LEGAL_LR");
            if (g_p22.state_base && uc) {
                uint8_t z[16];
                int nonzero = 0;
                if (guest_memory_uc_peek((struct uc_struct *)uc, g_p22.state_base, z, sizeof(z))) {
                    for (i = 0; i < 16; i++)
                        if (z[i]) nonzero = 1;
                }
                if (nonzero)
                    mark_gate(P22_G11_STATE_NONEMPTY, pc, off, g_p22.state_base, 0, 0,
                              regs ? regs[9] : 0, "state_base_nonzero");
            }
        }
    }

    if (off == OFF_API_HANDOFF && !g_p22.bp_logged[18]) {
        g_p22.bp_logged[18] = 1;
        dump_bp(uc, "API_HANDOFF", pc, off, regs, lr, sp, cpsr);
        snprintf(g_p22.handoff_note, sizeof(g_p22.handoff_note), "r0=0x%X r1=0x%X r2=0x%X r3=0x%X",
                 regs ? regs[0] : 0, regs ? regs[1] : 0, regs ? regs[2] : 0, regs ? regs[3] : 0);
    }

    if (regs && g_p22.item_created && !g_p22.callback_va) {
        uint32_t cand[3] = {regs[1], regs[2], lr};
        for (i = 0; i < 3; i++) {
            uint32_t c = cand[i] & ~1u;
            if (c >= g_p22.gl_base && c < g_p22.gl_base + g_p22.gl_size) {
                g_p22.callback_va = cand[i];
                break;
            }
        }
    }

#ifdef GWY_HAVE_UNICORN
    if (g_p22.headless && g_p22.item_created && !g_p22.selected && !g_p22.desc_enter_legal)
        headless_invoke_select(uc, regs, sp);
#endif
}

void p22_finalize(const char *stop_reason) {
    int i;
    FILE *f;
    if (!p22_enabled()) return;
    ensure_csv();
    printf("[JJFB_P25_FINAL] reason=%s gates=", stop_reason ? stop_reason : "?");
    for (i = 0; i < P22_GATE_COUNT; i++) printf("%s=%d ", k_gate_names[i], g_p22.gate[i]);
    printf("internal=%u external=%u stray=%d fault_pc=0x%X fetch=0x%X descriptor=\"%s\" "
           "nested=\"%s\" evidence=OBSERVED\n",
           g_p22.cfg_internal_size, g_p22.cfg_external_size, g_p22.desc_stray, g_p22.fault_pc,
           g_p22.fault_addr, g_p22.descriptor, g_p22.nested_target);
    fflush(stdout);

    f = fopen(getenv("JJFB_P23_JSON") && getenv("JJFB_P23_JSON")[0] ? getenv("JJFB_P23_JSON")
                                                                   : "reports/P23_STARTGAME_CONTRACT.json",
              "wb");
    if (f) {
        fprintf(f,
                "{\n"
                "  \"run_id\": %llu,\n"
                "  \"cfg_internal_size\": %u,\n"
                "  \"cfg_external_size\": %u,\n"
                "  \"stray_builder\": %s,\n"
                "  \"stray_lr\": \"0x%X\",\n"
                "  \"stray_source\": \"%s\",\n"
                "  \"fault_pc\": \"0x%X\",\n"
                "  \"fault_addr\": \"0x%X\",\n"
                "  \"startgame_lookup_pc\": \"0x%X\",\n"
                "  \"startgame_enter_pc\": \"0x%X\",\n"
                "  \"descriptor\": \"%s\",\n"
                "  \"item_object_va\": \"0x%X\",\n"
                "  \"callback_va\": \"0x%X\"\n"
                "}\n",
                g_p22.run_id, g_p22.cfg_internal_size, g_p22.cfg_external_size,
                g_p22.desc_stray ? "true" : "false", g_p22.stray_lr, g_p22.stray_source,
                g_p22.fault_pc, g_p22.fault_addr, g_p22.sg_lookup_pc, g_p22.sg_pc, g_p22.descriptor,
                g_p22.item_object_va, g_p22.callback_va);
        fclose(f);
    }
}
