#include "gwy_launcher/p22_cfg_loader_predicate.h"

#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

/* Key module offsets (relative to gamelist.ext guest_code_base). */
#define OFF_PARAM_SITE 0x77AEu /* historical P21 label; may not match abs PC */
#define OFF_PARAM_REAL 0x1344Au /* abs 0x2E77AE @ base 0x2D4364: ldrh [r4] tag check */
#define OFF_PARAM_FN 0x133E0u
#define OFF_UI_INIT 0x10740u
#define OFF_CFG_LOADER 0x7B6Cu
#define OFF_CFG_DISPATCH 0x7B9Cu
#define OFF_STATE_GET 0x7DB0u
#define OFF_PATH_STATE 0xD768u
#define OFF_LOADER_WRAP 0xD964u
#define OFF_LOADER_WRAP_BL 0xD96Cu
#define OFF_FF00 0xFF00u
#define OFF_FF10 0xFF10u
#define OFF_FF12 0xFF12u
#define OFF_FF30 0xFF30u
#define OFF_FF32 0xFF32u
#define OFF_FF3A 0xFF3Au
#define OFF_FFA4 0xFFA4u
#define OFF_FFA6 0xFFA6u
#define OFF_FFB2 0xFFB2u
#define OFF_EAFA 0xEAFAu
#define OFF_1074C 0x1074Cu
#define OFF_1074E 0x1074Eu
#define OFF_107F6 0x107F6u
#define OFF_107F8 0x107F8u
#define OFF_10814 0x10814u
#define OFF_10A24 0x10A24u
#define OFF_10D36 0x10D36u
#define OFF_DUMP_LO 0x7600u
#define OFF_DUMP_HI 0x7D00u

#define SLICE_CAP 200000
#define BRANCH_CAP 4096
#define XREF_CAP 32
#define PROV_CAP 128
#define MEM_CAP 4096

typedef struct {
    uint32_t caller_off;
    uint32_t bl_off;
    const char *func;
    const char *kind; /* direct */
    int reached;
    int called;
    char classif[40];
} XrefRow;

typedef struct {
    int known;
    int enabled;
    void *uc;
    unsigned long long run_id;
    clock_t t0;

    uint32_t gl_base;
    uint32_t gl_end;
    uint32_t gl_size;
    uint64_t gl_mod_id;
    uint32_t erw;
    uint32_t p_guest;
    uint32_t generation;
    char package_owner[96];

    int dump_done;
    int bytes_ok_77ae;
    int bytes_ok_7b6c;
    char dump_sha[72];

    int slice_armed;
    int slice_done;
    uint32_t slice_seq;
    uint32_t insn_budget;
    uint32_t hit_77ae;
    uint32_t hit_7b6c;
    uint32_t hit_ff00;
    uint32_t hit_7db0;
    uint32_t hit_d964;
    uint32_t hit_d768;
    uint32_t hit_eafa;
    uint32_t hit_10740;
    uint32_t hit_10814;
    uint32_t hit_10a24;
    uint32_t hit_10d36;
    uint32_t hit_133e0;
    uint32_t hit_1344a;
    uint32_t gl_insn_n;
    uint32_t fire_ext_n;
    uint32_t fire_ext_at_arm;
    int idle_stop;
    char stop_reason[80];

    uint32_t last_pc;
    uint32_t last_off;
    uint32_t last_cpsr;
    uint32_t func_ret_lr; /* LR at 77AE hit — treat as param-fn return */

    /* First blocking branch (filled at finalize if possible). */
    int block_found;
    uint32_t block_pc;
    uint32_t block_off;
    char block_insn[48];
    char block_cmp[96];
    char block_actual[64];
    char block_loader_path[64];
    uint32_t block_field_addr;
    int32_t block_field_erw_off;
    uint32_t block_value;
    uint32_t block_expect;
    char block_writer[80];
    char block_producer[80];

    /* 0x10800 */
    int ack_seen;
    uint32_t ack_caller;
    uint32_t ack_app;
    uint32_t ack_ret;
    uint32_t ack_store_va;
    int ack_affects_gate; /* -1 unk, 0 no, 1 yes */

    /* Parser contract snaps */
    int parser_cfn_enter;
    int parser_cfn_leave;
    uint32_t parser_cfn_r0;
    int parser_gl_enter;
    int parser_gl_leave;
    uint32_t parser_gl_r0;

    FILE *slice_csv;
    FILE *xref_csv;
    FILE *prov_csv;
    FILE *branch_md;
    FILE *disasm_txt;

    XrefRow xrefs[XREF_CAP];
    int xref_n;

    uint32_t branch_n;
} P22cState;

static P22cState g;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

static int is_gl(const char *m) {
    return m && strstr(m, "gamelist") != NULL;
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

static void ensure_files(void) {
    if (!g.slice_csv)
        g.slice_csv = open_csv(
            "JJFB_P22C_SLICE_CSV", "reports/p22_param_to_cfg_dynamic_slice.csv",
            "sequence,absolute_pc,module_base,module_offset,raw_bytes,disassembly,"
            "r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,sp,lr,cpsr,"
            "basic_block,branch_condition,branch_taken,memory_read,memory_write,note\n");
    if (!g.xref_csv)
        g.xref_csv = open_csv(
            "JJFB_P22C_XREF_CSV", "reports/p22_cfg_loader_xrefs.csv",
            "caller_offset,caller_absolute_pc,branch_instruction,direct_indirect,"
            "r0,r1,r2,r3,lr_continuation,owner_function,upper_caller,classification,note\n");
    if (!g.prov_csv)
        g.prov_csv = open_csv(
            "JJFB_P22C_PROV_CSV", "reports/p22_cfg_entry_predicate_provenance.csv",
            "field,address,erw_or_p_offset,current_value,expected_value,last_writer_pc,"
            "last_writer_module,write_trigger,natural_producer,category,note\n");
}

static void init_xrefs(void) {
    if (g.xref_n) return;
    g.xrefs[g.xref_n++] =
        (XrefRow){0xD96Cu, 0, "loader_wrap_d964", "direct", 0, 0, "CALLER_NOT_REACHED"};
    g.xrefs[g.xref_n++] =
        (XrefRow){0xFF3Au, 0, "ff00_case_cfg0", "direct", 0, 0, "CALLER_NOT_REACHED"};
    g.xrefs[g.xref_n++] =
        (XrefRow){0xFFB2u, 0, "ff00_case_cfg1", "direct", 0, 0, "CALLER_NOT_REACHED"};
}

static const char *thumb_hint(const uint8_t *b, uint32_t off) {
    uint16_t h = (uint16_t)(b[0] | (b[1] << 8));
    if ((h & 0xF800) == 0xE000) return "b";
    if ((h & 0xF000) == 0xD000) return "b.cond";
    if ((h & 0xFF00) == 0x4700) return (h & 0x80) ? "blx_reg" : "bx";
    if ((h & 0xF800) == 0x4800) return "ldr_pc";
    if ((h & 0xF800) == 0x6800) return "ldr";
    if ((h & 0xF800) == 0x6000) return "str";
    if ((h & 0xF800) == 0x2800) return "cmp_imm";
    if ((h & 0xFFC0) == 0x4280) return "cmp_reg";
    if ((h & 0xFF00) == 0xB500 || (h & 0xFE00) == 0xB400) return "push";
    if ((h & 0xFE00) == 0xBC00) return "pop";
    if (b[1] == 0xF0 || b[1] == 0xF7 || b[1] == 0xF1 || b[1] == 0xF3 || b[1] == 0xF5) {
        uint16_t h2 = (uint16_t)(b[2] | (b[3] << 8));
        if ((h & 0xF800) == 0xF000 && (h2 & 0xD000) == 0xD000) return "bl";
        if ((h & 0xF800) == 0xF000 && (h2 & 0xD000) == 0xC000) return "blx";
    }
    if (off == OFF_CFG_LOADER) return "cfg_loader_entry";
    if (off == OFF_PARAM_SITE) return "param_site_77AE";
    if (off == OFF_FF00) return "ff00_dispatch";
    if (off == OFF_FF12) return "ff00_early_exit_bhs";
    if (off == OFF_STATE_GET) return "state_get_7DB0";
    return "op";
}

static int is_cond_branch(const uint8_t *b) {
    uint16_t h = (uint16_t)(b[0] | (b[1] << 8));
    return ((h & 0xF000) == 0xD000 && ((h >> 8) & 0xF) != 0xF);
}

static void mark_xref_hit(uint32_t off, int called) {
    int i;
    for (i = 0; i < g.xref_n; i++) {
        if (g.xrefs[i].caller_off == off ||
            (off == OFF_CFG_LOADER && g.xrefs[i].called)) {
            g.xrefs[i].reached = 1;
            if (called) {
                g.xrefs[i].called = 1;
                snprintf(g.xrefs[i].classif, sizeof(g.xrefs[i].classif), "REACHED_AND_CALLED");
            }
        }
    }
}

static void dump_runtime_code(void *uc) {
    uint8_t *buf;
    size_t n;
    FILE *f;
    const char *out_bin;
    const char *out_sha;
    uint8_t digest[32];
    char sha[72];
    size_t i;
    if (g.dump_done || !g.gl_base || !g.gl_size) return;
#ifdef GWY_HAVE_UNICORN
    if (!uc) return;
    n = g.gl_size;
    if (n > 0x40000) n = 0x40000;
    buf = (uint8_t *)malloc(n);
    if (!buf) return;
    if (!guest_memory_uc_peek((struct uc_struct *)uc, g.gl_base, buf, (uint32_t)n)) {
        free(buf);
        return;
    }
    out_bin = getenv("JJFB_P22C_DUMP_BIN");
    if (!out_bin || !out_bin[0]) out_bin = "out/p22/gamelist_cfg_path_runtime.bin";
    out_sha = getenv("JJFB_P22C_DUMP_SHA");
    if (!out_sha || !out_sha[0]) out_sha = "out/p22/gamelist_cfg_path_runtime.sha256";
    f = fopen(out_bin, "wb");
    if (f) {
        fwrite(buf, 1, n, f);
        fclose(f);
    }
    /* Prefer window dump also written as full module; sha of full. */
    gwy_sha256(buf, n, digest);
    for (i = 0; i < 32; i++) sprintf(sha + i * 2, "%02x", digest[i]);
    sha[64] = 0;
    snprintf(g.dump_sha, sizeof(g.dump_sha), "%s", sha);
    f = fopen(out_sha, "wb");
    if (f) {
        fprintf(f, "%s\n", sha);
        fclose(f);
    }
    /* Verify expected opcodes at offsets. */
    if (n > OFF_PARAM_SITE + 4) {
        /* 0x77AE: adds r0,r1,r0 = 0x1808 in LE thumb? static was 0818 */
        g.bytes_ok_77ae = (buf[OFF_PARAM_SITE] == 0x08 && buf[OFF_PARAM_SITE + 1] == 0x18);
    }
    if (n > OFF_CFG_LOADER + 2) {
        /* push {r4-r7,lr} = f0 b5 */
        g.bytes_ok_7b6c = (buf[OFF_CFG_LOADER] == 0xf0 && buf[OFF_CFG_LOADER + 1] == 0xb5);
    }
    /* Write annotated window disasm skeleton. */
    if (!g.disasm_txt) {
        const char *dp = getenv("JJFB_P22C_DISASM");
        if (!dp || !dp[0]) dp = "reports/p22_gamelist_cfg_path_disasm.txt";
        g.disasm_txt = fopen(dp, "wb");
    }
    if (g.disasm_txt) {
        uint32_t off;
        fprintf(g.disasm_txt,
                "# P22-CLEAN gamelist cfg-path runtime dump\n"
                "# module_base=0x%X size=0x%X sha256=%s\n"
                "# bytes_ok_77AE=%d (expect thumb adds @+0x77AE)\n"
                "# bytes_ok_7B6C=%d (expect push {r4-r7,lr} = function ENTRY)\n"
                "# static_xrefs_to_7B6C: +0xD96C, +0xFF3A, +0xFFB2 (direct BL)\n"
                "# window module_offset 0x%X .. 0x%X\n\n",
                g.gl_base, g.gl_size, g.dump_sha, g.bytes_ok_77ae, g.bytes_ok_7b6c, OFF_DUMP_LO,
                OFF_DUMP_HI);
        for (off = OFF_DUMP_LO; off + 1 < OFF_DUMP_HI && off + 1 < n; off += 2) {
            uint8_t b0 = buf[off], b1 = buf[off + 1];
            uint16_t h = (uint16_t)(b0 | (b1 << 8));
            const char *hint = "";
            char extra[64];
            extra[0] = 0;
            if (off == OFF_PARAM_SITE) hint = "  ; PARAM_SITE (P21 label; static=adds)";
            if (off == OFF_CFG_LOADER) hint = "  ; CFG_LOADER ENTRY";
            if (off == OFF_CFG_DISPATCH) hint = "  ; near 10112 dispatch helper call";
            if ((h & 0xF800) == 0xF000 && off + 3 < n) {
                uint16_t h2 = (uint16_t)(buf[off + 2] | (buf[off + 3] << 8));
                if ((h2 & 0xD000) == 0xD000 || (h2 & 0xD000) == 0xC000) {
                    int s = (h >> 10) & 1;
                    int imm10 = h & 0x3FF;
                    int j1 = (h2 >> 13) & 1;
                    int j2 = (h2 >> 11) & 1;
                    int imm11 = h2 & 0x7FF;
                    int I1 = 1 - (j1 ^ s);
                    int I2 = 1 - (j2 ^ s);
                    int imm = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1);
                    uint32_t tgt;
                    if (s) imm -= 1 << 25;
                    tgt = (off + 4 + (uint32_t)imm) & ~1u;
                    snprintf(extra, sizeof(extra), "  ; %s 0x%X%s",
                             ((h2 & 0xD000) == 0xD000) ? "bl" : "blx", tgt,
                             tgt == OFF_CFG_LOADER ? " CFG_LOADER" : "");
                    fprintf(g.disasm_txt, "0x%04X: %02x%02x%02x%02x  %s%s%s\n", off, b0, b1,
                            buf[off + 2], buf[off + 3], thumb_hint(buf + off, off), extra, hint);
                    off += 2;
                    continue;
                }
            }
            fprintf(g.disasm_txt, "0x%04X: %02x%02x        hint=%s%s\n", off, b0, b1,
                    thumb_hint(buf + off, off), hint);
        }
        fprintf(g.disasm_txt,
                "\n# Also see full-module dump for xrefs outside window:\n"
                "#   loader_wrap +0xD964: bl +0xD768; movs r0,#0; bl +0x7B6C; bl +0xFF00\n"
                "#   ff00_dispatch +0xFF00: bl +0x7DB0; adds r0,#5; cmp r0,#5; bhs early_exit\n"
                "#   case paths: +0xFF3A bl cfg_loader(r0=0); +0xFFB2 bl cfg_loader(r0=1)\n");
        fflush(g.disasm_txt);
    }
    free(buf);
    g.dump_done = 1;
    printf("[JJFB_P22C] dump=1 base=0x%X size=0x%X sha=%s ok77AE=%d ok7B6C=%d "
           "evidence=OBSERVED\n",
           g.gl_base, g.gl_size, g.dump_sha, g.bytes_ok_77ae, g.bytes_ok_7b6c);
    fflush(stdout);
#else
    (void)uc;
#endif
}

static void resolve_gl_from_pc(uint32_t pc) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *gm;
    uint32_t base, size;
    if (!reg) return;
    gm = module_registry_find_by_code_addr(reg, pc & ~1u);
    if (!gm || !is_gl(gm->resolved_name[0] ? gm->resolved_name : gm->requested_name)) return;
    base = gm->map.guest_code_base;
    size = gm->map.guest_code_size;
    if (!base) return;
    /* Pick up RAW_BASE_REFINE if we still hold the aligned cacheSync base. */
    if (g.gl_base && base != g.gl_base) {
        uint32_t delta = (base > g.gl_base) ? (base - g.gl_base) : (g.gl_base - base);
        if (delta < 0x20u) {
            printf("[JJFB_P22C] module_base_refine_via_pc prev=0x%X new=0x%X pad=0x%X "
                   "evidence=DOCUMENTED\n",
                   g.gl_base, base, delta);
            fflush(stdout);
            g.dump_done = 0;
        }
    }
    g.gl_base = base;
    g.gl_size = size;
    g.gl_end = base + size;
    g.gl_mod_id = gm->module_id;
    if (gm->data.start_of_er_rw) g.erw = gm->data.start_of_er_rw;
    if (gm->package_path[0])
        snprintf(g.package_owner, sizeof(g.package_owner), "%s", gm->package_path);
}

static void arm_slice(uint32_t off, const char *why) {
    if (g.slice_armed) return;
    g.slice_armed = 1;
    g.fire_ext_at_arm = g.fire_ext_n;
    g.insn_budget = 500000;
    {
        const char *e = getenv("JJFB_P22C_INSN_BUDGET");
        if (e && e[0]) g.insn_budget = (uint32_t)strtoul(e, NULL, 10);
    }
    printf("[JJFB_P22C] slice_arm off=0x%X why=%s budget=%u evidence=OBSERVED\n", off, why,
           g.insn_budget);
    fflush(stdout);
}

static void note_branch_chain(uint32_t pc, uint32_t off, const uint8_t *insn, uint32_t r0,
                              uint32_t cpsr, int taken, const char *note) {
    if (!g.branch_md) {
        const char *p = getenv("JJFB_P22C_BRANCH_MD");
        if (!p || !p[0]) p = "reports/p22_param_to_cfg_branch_chain.md";
        g.branch_md = fopen(p, "wb");
        if (g.branch_md) {
            fputs("# P22-CLEAN param→cfg branch chain\n\n", g.branch_md);
            fflush(g.branch_md);
        }
    }
    if (!g.branch_md) return;
    fprintf(g.branch_md,
            "- seq=%u pc=0x%X off=0x%X insn=%02X%02X%02X%02X r0=0x%X cpsr=0x%X taken=%d "
            "note=%s\n",
            g.slice_seq, pc, off, insn[0], insn[1], insn[2], insn[3], r0, cpsr, taken,
            note ? note : "");
    fflush(g.branch_md);
    g.branch_n++;
}

static void consider_first_block(uint32_t pc, uint32_t off, const uint8_t *insn, uint32_t r0,
                                 uint32_t r9, int taken) {
    /* First discriminating gate on path toward cfg loader. */
    if (g.block_found) return;

    /* Gate UI: +0x10740 once-flag — ldrb [R9+lit]; cmp #1; beq early_ret (skips path to FF00). */
    if (off == OFF_1074E && taken) {
        g.block_found = 1;
        g.block_pc = pc;
        g.block_off = off;
        snprintf(g.block_insn, sizeof(g.block_insn), "beq early_ret_skip_ff00_path");
        snprintf(g.block_cmp, sizeof(g.block_cmp), "ldrb r0,[R9+#lit]; cmp r0,#1");
        snprintf(g.block_actual, sizeof(g.block_actual), "TAKEN → +0x107AA return");
        snprintf(g.block_loader_path, sizeof(g.block_loader_path),
                 "NOT_TAKEN → init → +0x10814 bl +0xFF00");
        g.block_value = r0;
        g.block_expect = 0;
        g.block_field_addr = 0;
        g.block_field_erw_off = -1;
        snprintf(g.block_writer, sizeof(g.block_writer), "strb #1 at +0x1076C (same flag)");
        snprintf(g.block_producer, sizeof(g.block_producer),
                 "prior +0x10740 completion / parent UI init");
        note_branch_chain(pc, off, insn, r0, g.last_cpsr, taken, "FIRST_BLOCK_1074E_once_flag");
        return;
    }

    /* Gate mode: cmp [R9+..], #0xF; beq +0x10814 (bl FF00). */
    if (off == OFF_107F8 && !taken) {
        g.block_found = 1;
        g.block_pc = pc;
        g.block_off = off;
        snprintf(g.block_insn, sizeof(g.block_insn), "beq +0x10814 (not taken)");
        snprintf(g.block_cmp, sizeof(g.block_cmp), "cmp r0,#0xF (mode/state field)");
        snprintf(g.block_actual, sizeof(g.block_actual), "NOT_TAKEN → alt path / return");
        snprintf(g.block_loader_path, sizeof(g.block_loader_path), "TAKEN → bl +0xFF00");
        g.block_value = r0;
        g.block_expect = 0xFu;
        snprintf(g.block_writer, sizeof(g.block_writer), "R9-relative mode field");
        snprintf(g.block_producer, sizeof(g.block_producer), "UI/list mode publisher");
        note_branch_chain(pc, off, insn, r0, g.last_cpsr, taken, "FIRST_BLOCK_107F8_mode");
        return;
    }

    /* Gate A: ff00 early exit — 7DB0 returned >=0 so (r0+5) >= 5 / HS. */
    if (off == OFF_FF12 && taken) {
        g.block_found = 1;
        g.block_pc = pc;
        g.block_off = off;
        snprintf(g.block_insn, sizeof(g.block_insn), "bhs early_exit");
        snprintf(g.block_cmp, sizeof(g.block_cmp),
                 "after bl 0x7DB0: adds r0,#5; cmp r0,#5; (pre-add r0 was >=0)");
        snprintf(g.block_actual, sizeof(g.block_actual), "TAKEN → 0xFFC0 return 0");
        snprintf(g.block_loader_path, sizeof(g.block_loader_path), "NOT_TAKEN continue switch");
        g.block_value = r0;
        g.block_expect = 0;
        g.block_field_addr = 0;
        g.block_field_erw_off = -1;
        snprintf(g.block_writer, sizeof(g.block_writer), "return of +0x7DB0");
        snprintf(g.block_producer, sizeof(g.block_producer),
                 "gamelist.ext +0x7DB0 (state/list readiness getter)");
        note_branch_chain(pc, off, insn, r0, g.last_cpsr, taken, "FIRST_BLOCK_ff00_early_exit");
        return;
    }

    /* Gate B: inside case0 — R9 field nonzero skips cfg_loader */
    if (off == OFF_FF32 && taken) {
        g.block_found = 1;
        g.block_pc = pc;
        g.block_off = off;
        snprintf(g.block_insn, sizeof(g.block_insn), "bne skip_cfg_loader");
        snprintf(g.block_cmp, sizeof(g.block_cmp), "cmp r0,#0 (R9-relative flag)");
        snprintf(g.block_actual, sizeof(g.block_actual), "TAKEN skip loader");
        snprintf(g.block_loader_path, sizeof(g.block_loader_path), "NOT_TAKEN → bl 0xD768; bl 0x7B6C");
        g.block_value = r0;
        g.block_expect = 0;
        g.block_field_addr = 0;
        snprintf(g.block_writer, sizeof(g.block_writer), "R9-relative load @+0xFF2A..FF2E");
        snprintf(g.block_producer, sizeof(g.block_producer), "unknown_pending_provenance");
        note_branch_chain(pc, off, insn, r0, g.last_cpsr, taken, "FIRST_BLOCK_ff32_skip");
        return;
    }

    /* Gate C: case1 path — e0a4 ret != 1 (not a hard block; loader still called). */
    if (off == OFF_FFA6 && taken) {
        (void)r9;
        (void)insn;
        return;
    }

    (void)r9;
    (void)insn;
}

int p22c_enabled(void) {
    if (g.known) return g.enabled;
    g.enabled = env1("JJFB_P22_CLEAN");
    {
        const char *rid = getenv("JJFB_P22C_RUN_ID");
        if (!rid || !rid[0]) rid = getenv("JJFB_E10A_RUN_ID");
        if (rid && rid[0]) g.run_id = strtoull(rid, NULL, 10);
    }
    g.known = 1;
    if (g.enabled) {
        /* Hard refuse old headless forge env if accidentally set — warn only. */
        if (getenv("JJFB_P22_MODE") || getenv("JJFB_P22_HEADLESS_SELECT")) {
            printf("[JJFB_P22C] WARN old JJFB_P22_MODE/HEADLESS present — observe continues but "
                   "forge must stay off\n");
            fflush(stdout);
        }
        g.t0 = clock();
        ensure_files();
        init_xrefs();
        printf("[JJFB_P22C] armed=1 CLEAN=1 no_headless=1 no_forge=1 run_id=%llu "
               "evidence=OBSERVED\n",
               g.run_id);
        fflush(stdout);
    }
    return g.enabled;
}

void p22c_reset(void) {
    if (g.slice_csv) fclose(g.slice_csv);
    if (g.xref_csv) fclose(g.xref_csv);
    if (g.prov_csv) fclose(g.prov_csv);
    if (g.branch_md) fclose(g.branch_md);
    if (g.disasm_txt) fclose(g.disasm_txt);
    memset(&g, 0, sizeof(g));
}

void p22c_bind_uc(void *uc) {
    if (!p22c_enabled()) return;
    g.uc = uc;
}

void p22c_note_module_map(const char *module_name, uint32_t base, uint32_t size, uint32_t erw,
                          uint32_t p_guest, uint32_t generation, const char *package_owner) {
    if (!p22c_enabled()) return;
    if (!is_gl(module_name)) return;
    /* Allow RAW_BASE_REFINE (pad < 0x20) to replace aligned cacheSync base. */
    if (g.gl_base && base != g.gl_base) {
        uint32_t delta = (base > g.gl_base) ? (base - g.gl_base) : (g.gl_base - base);
        if (delta >= 0x20u) {
            printf("[JJFB_P22C] module_map_ignore base=0x%X prev=0x%X delta=0x%X "
                   "evidence=OBSERVED\n",
                   base, g.gl_base, delta);
            fflush(stdout);
            return;
        }
        printf("[JJFB_P22C] module_base_refine prev=0x%X new=0x%X pad=0x%X size=%u "
               "evidence=DOCUMENTED\n",
               g.gl_base, base, delta, size);
        fflush(stdout);
        g.dump_done = 0; /* re-dump against refined raw MRPG base */
    }
    g.gl_base = base;
    g.gl_size = size;
    g.gl_end = base + size;
    if (erw) g.erw = erw;
    if (p_guest) g.p_guest = p_guest;
    g.generation = generation;
    if (package_owner && package_owner[0])
        snprintf(g.package_owner, sizeof(g.package_owner), "%s", package_owner);
    printf("[JJFB_P22C] module_map name=%s base=0x%X end=0x%X size=0x%X erw=0x%X P=0x%X "
           "gen=%u owner=%s CFG_LOADER=0x%X PARAM_SITE=0x%X evidence=OBSERVED\n",
           module_name ? module_name : "?", base, g.gl_end, size, g.erw, g.p_guest, g.generation,
           g.package_owner[0] ? g.package_owner : "-", base + OFF_CFG_LOADER, base + OFF_PARAM_SITE);
    fflush(stdout);
    dump_runtime_code(g.uc);
}

void p22c_note_gamelist_started(void) {
    if (!p22c_enabled()) return;
    printf("[JJFB_P22C] gamelist_started evidence=OBSERVED\n");
    fflush(stdout);
}

void p22c_note_plat_10800(uint32_t caller_pc, uint32_t app, uint32_t arg2, uint32_t arg3,
                          uint32_t status_ret, uint32_t r9) {
    if (!p22c_enabled()) return;
    g.ack_seen = 1;
    g.ack_caller = caller_pc;
    g.ack_app = app;
    g.ack_ret = status_ret;
    (void)arg2;
    (void)arg3;
    (void)r9;
    ensure_files();
    if (g.prov_csv) {
        fprintf(g.prov_csv,
                "ack_10800_ret,0x%X,-,0x%X,0x1,0x%X,platform,sendAppEvent,"
                "platform_send_app_event,platform_return_contract,"
                "\"app=0x%X observed; gate-link pending slice\"\n",
                caller_pc, status_ret, caller_pc, app);
        fflush(g.prov_csv);
    }
    printf("[JJFB_P22C] plat_10800 caller=0x%X app=0x%X ret=0x%X evidence=OBSERVED\n", caller_pc,
           app, status_ret);
    fflush(stdout);
}

void p22c_note_plat_10112(const char *path, uint32_t caller_pc, int ret) {
    if (!p22c_enabled()) return;
    printf("[JJFB_P22C] plat_10112 path=%s caller=0x%X ret=%d evidence=OBSERVED\n",
           path ? path : "?", caller_pc, ret);
    fflush(stdout);
}

void p22c_note_param_byte_read(uint32_t pc, const char *module, uint32_t addr, uint32_t size,
                               const uint8_t *bytes, const uint32_t regs[13], uint32_t lr,
                               uint32_t r9) {
    uint32_t off;
    if (!p22c_enabled()) return;
    (void)bytes;
    (void)regs;
    (void)r9;
    if (!g.gl_base) resolve_gl_from_pc(pc);
    if (!g.gl_base || !is_gl(module)) {
        if (module && strstr(module, "cfunction")) {
            if (!g.parser_cfn_enter) g.parser_cfn_enter = 1;
            g.parser_cfn_r0 = regs ? regs[0] : 0;
        }
        return;
    }
    off = (pc & ~1u) - g.gl_base;
    /* Any gamelist read into mapped launch-param VA — record true module_offset. */
    if (!g.parser_gl_enter) {
        g.parser_gl_enter = 1;
        arm_slice(off, "param_mem_read_any");
    }
    ensure_files();
    if (g.prov_csv) {
        fprintf(g.prov_csv,
                "param_mem_read,0x%X,pc_off=0x%X,addr=0x%X,size=%u,0x%X,%s,mem_read,"
                "guest_code,launch_descriptor_or_alias,\"lr=0x%X note=not_assume_77AE\"\n",
                pc, off, addr, size, pc, module ? module : "?", lr);
        fflush(g.prov_csv);
    }
}

void p22c_note_timer_fire(uint32_t helper, uint32_t method, int end) {
    if (!p22c_enabled()) return;
    (void)helper;
    (void)method;
    if (end) {
        g.fire_ext_n++;
        if (g.slice_armed && !g.slice_done && g.fire_ext_n > g.fire_ext_at_arm) {
            snprintf(g.stop_reason, sizeof(g.stop_reason), "next_FIRE_EXT");
            g.idle_stop = 1;
            g.slice_done = 1;
            printf("[JJFB_P22C] slice_stop reason=next_FIRE_EXT n=%u evidence=OBSERVED\n",
                   g.fire_ext_n);
            fflush(stdout);
        }
        /* Runner kills process without emu_exit — finalize after 3 natural fires. */
        if (g.fire_ext_n >= 3u) {
            if (!g.stop_reason[0])
                snprintf(g.stop_reason, sizeof(g.stop_reason), "timer_fire_n3");
            p22c_finalize(g.stop_reason);
        }
    }
}

void p22c_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                  uint32_t lr, uint32_t sp, uint32_t cpsr) {
    uint32_t norm, off;
    uint8_t insn[4] = {0};
    int watch = 0;
    int take = -1;
    const char *hint;
    char raw[12];
    char note[96];

    if (!p22c_enabled()) return;
    if (uc) g.uc = uc;
    if (!is_gl(module_name) && !(g.gl_base && (pc & ~1u) >= g.gl_base && (pc & ~1u) < g.gl_end))
        return;
    /* Always refresh base so RAW_BASE_REFINE is visible even if we mapped early. */
    resolve_gl_from_pc(pc);
    if (!g.gl_base) return;
    if (!g.dump_done) dump_runtime_code(uc);

    norm = pc & ~1u;
    if (norm < g.gl_base || norm >= g.gl_end) return;
    off = norm - g.gl_base;
    g.gl_insn_n++;

    if (off == OFF_PARAM_SITE) {
        g.hit_77ae++;
        if (!g.slice_armed) {
            g.func_ret_lr = lr;
            arm_slice(off, "hit_77AE_hist");
        }
    }
    if (off == OFF_PARAM_REAL || off == OFF_PARAM_FN) {
        if (off == OFF_PARAM_REAL) g.hit_1344a++;
        if (off == OFF_PARAM_FN) g.hit_133e0++;
        if (!g.slice_armed) {
            g.func_ret_lr = lr;
            arm_slice(off, "hit_param_real_1344A");
        }
    }
    if (off == OFF_UI_INIT || off == OFF_1074C || off == OFF_1074E || off == OFF_107F6 ||
        off == OFF_107F8) {
        if (off == OFF_UI_INIT) g.hit_10740++;
        if (!g.slice_armed) arm_slice(off, "hit_ui_init_10740");
    }
    if (off == OFF_CFG_LOADER) {
        g.hit_7b6c++;
        mark_xref_hit(OFF_CFG_LOADER, 1);
        /* Attribute which BL site via LR */
        {
            uint32_t lroff = (lr & ~1u) > g.gl_base ? (lr & ~1u) - g.gl_base : 0;
            if (lroff == OFF_LOADER_WRAP_BL + 4 || lroff == 0xD970u)
                mark_xref_hit(0xD96Cu, 1);
            if (lroff == 0xFF3Eu) mark_xref_hit(0xFF3Au, 1);
            if (lroff == 0xFFB6u) mark_xref_hit(0xFFB2u, 1);
        }
        if (g.slice_armed && !g.slice_done) {
            snprintf(g.stop_reason, sizeof(g.stop_reason), "reached_7B6C");
            g.slice_done = 1;
        }
    }
    if (off == OFF_FF00) g.hit_ff00++;
    if (off == OFF_STATE_GET) g.hit_7db0++;
    if (off == OFF_LOADER_WRAP || off == OFF_LOADER_WRAP_BL) {
        g.hit_d964++;
        mark_xref_hit(0xD96Cu, 0);
        g.xrefs[0].reached = 1;
        if (strcmp(g.xrefs[0].classif, "REACHED_AND_CALLED") != 0)
            snprintf(g.xrefs[0].classif, sizeof(g.xrefs[0].classif), "REACHED_BRANCH_NOT_TAKEN");
    }
    if (off == OFF_PATH_STATE) g.hit_d768++;
    if (off == OFF_EAFA) g.hit_eafa++;
    if (off == OFF_10814) g.hit_10814++;
    if (off == OFF_10A24) g.hit_10a24++;
    if (off == OFF_10D36) g.hit_10d36++;
    if (off == OFF_FF3A) {
        g.xrefs[1].reached = 1;
        if (strcmp(g.xrefs[1].classif, "REACHED_AND_CALLED") != 0)
            snprintf(g.xrefs[1].classif, sizeof(g.xrefs[1].classif), "REACHED_BRANCH_NOT_TAKEN");
    }
    if (off == OFF_FFB2) {
        g.xrefs[2].reached = 1;
        if (strcmp(g.xrefs[2].classif, "REACHED_AND_CALLED") != 0)
            snprintf(g.xrefs[2].classif, sizeof(g.xrefs[2].classif), "REACHED_BRANCH_NOT_TAKEN");
    }

    /* Also arm slice if we hit cfg-path sites. */
    if (!g.slice_armed && (off == OFF_FF00 || off == OFF_STATE_GET || off == OFF_LOADER_WRAP ||
                           off == OFF_EAFA || off == OFF_10814 || off == OFF_10A24 ||
                           off == OFF_10D36 || off == OFF_UI_INIT || off == OFF_PARAM_REAL))
        arm_slice(off, "cfg_path_site");

    watch = (off == OFF_PARAM_SITE || off == OFF_PARAM_REAL || off == OFF_PARAM_FN ||
             off == OFF_UI_INIT || off == OFF_1074C || off == OFF_1074E || off == OFF_107F6 ||
             off == OFF_107F8 || off == OFF_CFG_LOADER || off == OFF_CFG_DISPATCH ||
             off == OFF_STATE_GET || off == OFF_FF00 || off == OFF_FF10 || off == OFF_FF12 ||
             off == OFF_FF30 || off == OFF_FF32 || off == OFF_FF3A || off == OFF_FFA4 ||
             off == OFF_FFA6 || off == OFF_FFB2 || off == OFF_LOADER_WRAP ||
             off == OFF_LOADER_WRAP_BL || off == OFF_PATH_STATE || off == OFF_EAFA ||
             off == OFF_10814 || off == OFF_10A24 || off == OFF_10D36);

    if (g.slice_armed && !g.slice_done) watch = 1;

    if (!watch) {
        g.last_pc = pc;
        g.last_off = off;
        g.last_cpsr = cpsr;
        return;
    }

#ifdef GWY_HAVE_UNICORN
    if (uc) guest_memory_uc_peek((struct uc_struct *)uc, norm, insn, 4);
#endif
    hint = thumb_hint(insn, off);
    snprintf(raw, sizeof(raw), "%02X%02X%02X%02X", insn[0], insn[1], insn[2], insn[3]);

    if (g.slice_armed && !g.slice_done) {
        /* Detect return of param function: PC == LR saved at 77AE. */
        if (g.func_ret_lr && (pc & ~1u) == (g.func_ret_lr & ~1u) && g.hit_77ae) {
            snprintf(g.stop_reason, sizeof(g.stop_reason), "param_fn_return");
            g.slice_done = 1;
            g.parser_gl_leave = 1;
            g.parser_gl_r0 = regs ? regs[0] : 0;
        }
        if (g.insn_budget) {
            g.insn_budget--;
            if (g.insn_budget == 0) {
                snprintf(g.stop_reason, sizeof(g.stop_reason), "insn_budget");
                g.slice_done = 1;
            }
        }
    }

    /* Branch taken heuristic vs previous PC. */
    if (is_cond_branch(insn) && g.last_pc) {
        /* Will know next insn; approximate: if next logged PC != fallthrough */
        take = -1;
    }
    if (off == OFF_FF12 || off == OFF_FF32 || off == OFF_FFA6 || off == OFF_1074E ||
        off == OFF_107F8) {
        if (off == OFF_FF12) {
            take = (cpsr & (1u << 29)) ? 1 : 0;
        } else if (off == OFF_FF32) {
            take = (regs && regs[0] != 0) ? 1 : 0;
        } else if (off == OFF_FFA6) {
            take = (regs && regs[0] != 1) ? 1 : 0;
        } else if (off == OFF_1074E) {
            take = (regs && regs[0] == 1) ? 1 : 0;
        } else if (off == OFF_107F8) {
            take = (regs && regs[0] == 0xFu) ? 1 : 0;
        }
        consider_first_block(pc, off, insn, regs ? regs[0] : 0, regs ? regs[9] : 0, take);
        note_branch_chain(pc, off, insn, regs ? regs[0] : 0, cpsr, take, hint);
    }

    note[0] = 0;
    if (off == OFF_PARAM_SITE) snprintf(note, sizeof(note), "PARAM_SITE_HIST_77AE");
    else if (off == OFF_PARAM_REAL) snprintf(note, sizeof(note), "PARAM_REAL_1344A_tag");
    else if (off == OFF_UI_INIT) snprintf(note, sizeof(note), "UI_INIT_10740");
    else if (off == OFF_1074E) snprintf(note, sizeof(note), "UI_ONCE_FLAG_GATE");
    else if (off == OFF_107F8) snprintf(note, sizeof(note), "UI_MODE_0xF_GATE");
    else if (off == OFF_CFG_LOADER) snprintf(note, sizeof(note), "CFG_LOADER");
    else if (off == OFF_FF12) snprintf(note, sizeof(note), "FF00_EARLY_EXIT_GATE");
    else if (off == OFF_STATE_GET) snprintf(note, sizeof(note), "STATE_GET_7DB0");
    else if (g.slice_armed) snprintf(note, sizeof(note), "slice");

    /* Dense slice: log every watched insn; for full slice log up to SLICE_CAP. */
    if (g.slice_csv && regs && g.slice_seq < SLICE_CAP &&
        (g.slice_armed || watch)) {
        int log_it = 1;
        /* If dense and not a key/branch, sample every 8th to keep file usable — but
         * first 4000 always full. */
        if (g.slice_armed && g.slice_seq > 4000 && !is_cond_branch(insn) &&
            !(off == OFF_CFG_LOADER || off == OFF_FF00 || off == OFF_STATE_GET ||
              off == OFF_FF12 || off == OFF_FF3A || off == OFF_FFB2 || off == OFF_PARAM_SITE)) {
            if ((g.slice_seq & 7u) != 0) log_it = 0;
        }
        if (log_it) {
            fprintf(g.slice_csv,
                    "%u,0x%X,0x%X,0x%X,%s,\"%s\",0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,bb_0x%X,\"%s\",%d,,,\"%s\"\n",
                    ++g.slice_seq, pc, g.gl_base, off, raw, hint, regs[0], regs[1], regs[2],
                    regs[3], regs[4], regs[5], regs[6], regs[7], regs[8], regs[9], regs[10],
                    regs[11], regs[12], sp, lr, cpsr, off,
                    (take >= 0) ? (take ? "taken" : "not") : "", take, note);
            if ((g.slice_seq & 0x3Fu) == 0) fflush(g.slice_csv);
        } else {
            g.slice_seq++;
        }
    }

    /* Capture 7DB0 return at next site after leave — when we see FF10. */
    if (off == OFF_FF10 && regs) {
        ensure_files();
        if (g.prov_csv) {
            /* r0 here is already +5 from 7DB0 return */
            uint32_t post = regs[0];
            int32_t orig = (int32_t)post - 5;
            fprintf(g.prov_csv,
                    "ff00_state_ret_plus5,0x%X,+0xFF10,0x%X,(need_orig_in_-5..-1),0x%X,"
                    "gamelist.ext,bl_7DB0_return,gamelist+0x7DB0,list_ready_or_ui_state,"
                    "\"orig_r0≈%d; bhs_if_orig>=0\"\n",
                    pc, post, pc, (int)orig);
            fflush(g.prov_csv);
        }
        /* If orig >= 0, this is the gate that will block at FF12. */
        if (!g.block_found && (int32_t)regs[0] - 5 >= 0) {
            /* Will confirm at FF12. */
        }
    }

    g.last_pc = pc;
    g.last_off = off;
    g.last_cpsr = cpsr;
}

void p22c_finalize(const char *stop_reason) {
    int i;
    const char *cls = "F";
    static int finalized;
    if (!p22c_enabled()) return;
    if (finalized) return;
    finalized = 1;
    ensure_files();
    init_xrefs();

    /* If never reached any caller → F; refine which upper layer is missing. */
    if (!g.hit_7b6c && !g.hit_ff00 && !g.hit_d964) {
        cls = "F";
        if (!g.block_found) {
            g.block_found = 1;
            g.block_pc = 0;
            if (!g.hit_10740 && !g.hit_10814) {
                g.block_off = OFF_UI_INIT;
                snprintf(g.block_insn, sizeof(g.block_insn), "CALLER_NOT_REACHED +0x10740");
                snprintf(g.block_cmp, sizeof(g.block_cmp),
                         "no enter of UI/init that owns bl +0x10814→FF00");
                snprintf(g.block_actual, sizeof(g.block_actual), "+0x10740 never executed");
                snprintf(g.block_loader_path, sizeof(g.block_loader_path),
                         "+0x10740 → +0x10814 → +0xFF00 → +0x7B6C");
                snprintf(g.block_writer, sizeof(g.block_writer), "n/a");
                snprintf(g.block_producer, sizeof(g.block_producer),
                         "callers of +0x10740 (e.g. +0x4076/+0x12D0E event path)");
            } else if (g.hit_10740 && !g.hit_10814) {
                g.block_off = OFF_1074E;
                snprintf(g.block_insn, sizeof(g.block_insn), "inside +0x10740 before +0x10814");
                snprintf(g.block_cmp, sizeof(g.block_cmp),
                         "once-flag(+0x1074E) or mode==0xF(+0x107F8)");
                snprintf(g.block_actual, sizeof(g.block_actual), "+0x10814 not reached");
                snprintf(g.block_loader_path, sizeof(g.block_loader_path), "+0x10814 bl +0xFF00");
                snprintf(g.block_producer, sizeof(g.block_producer),
                         "R9 once-flag / mode field natural writers");
            } else {
                g.block_off = OFF_FF00;
                snprintf(g.block_insn, sizeof(g.block_insn), "N/A (FF00 not entered)");
                snprintf(g.block_cmp, sizeof(g.block_cmp), "no execution of +0xFF00 / +0xD964");
                snprintf(g.block_actual, sizeof(g.block_actual), "cfg loader callers never entered");
                snprintf(g.block_loader_path, sizeof(g.block_loader_path),
                         "+0xFF00 cases / +0xD964 wrap");
                snprintf(g.block_producer, sizeof(g.block_producer),
                         "upper caller of +0xFF00 (+0x10814/+0x10A24/+0x10D36)");
            }
        }
    } else if (g.block_off == OFF_FF12 || g.block_off == OFF_1074E) {
        cls = (g.block_off == OFF_FF12) ? "D" : "B";
    } else if (g.block_off == OFF_FF32 || g.block_off == OFF_107F8) {
        cls = "B";
    }

    /* Xref CSV rows */
    if (g.xref_csv) {
        for (i = 0; i < g.xref_n; i++) {
            uint32_t abs = g.gl_base ? g.gl_base + g.xrefs[i].caller_off : 0;
            if (!g.xrefs[i].reached)
                snprintf(g.xrefs[i].classif, sizeof(g.xrefs[i].classif), "CALLER_NOT_REACHED");
            fprintf(g.xref_csv, "0x%X,0x%X,bl_+0x7B6C,direct,,,,,0x%X,%s,see_static,%s,"
                                "\"runtime_hit_loader=%u\"\n",
                    g.xrefs[i].caller_off, abs, abs + 4u, g.xrefs[i].func, g.xrefs[i].classif,
                    g.hit_7b6c);
        }
        fflush(g.xref_csv);
    }

    /* Ack vs gate */
    g.ack_affects_gate = 0;
    if (g.ack_seen && g.block_off == OFF_FF12) {
        /* Only mark yes if provenance shows 7DB0 reads ack — unknown → 0 */
        g.ack_affects_gate = 0;
    }
    if (g.prov_csv) {
        fprintf(g.prov_csv,
                "summary_block,0x%X,0x%X,0x%X,0x%X,0x%X,-,-,%s,%s,\"class=%s\"\n", g.block_pc,
                g.block_off, g.block_value, g.block_expect, g.block_pc, g.block_producer,
                g.block_writer, cls);
        fflush(g.prov_csv);
    }

    if (!g.stop_reason[0])
        snprintf(g.stop_reason, sizeof(g.stop_reason), "%s",
                 stop_reason ? stop_reason : "finalize");

    printf("[JJFB_P22C_FINAL] stop=%s base=0x%X hit77AE=%u hit1344A=%u hit10740=%u hit7B6C=%u "
           "hitFF00=%u hit7DB0=%u hitD964=%u hit10814=%u gl_insn=%u slice=%u block_off=0x%X "
           "class=%s ack10800=%d cfg_open=0 forge=0 evidence=OBSERVED\n",
           g.stop_reason, g.gl_base, g.hit_77ae, g.hit_1344a, g.hit_10740, g.hit_7b6c, g.hit_ff00,
           g.hit_7db0, g.hit_d964, g.hit_10814, g.gl_insn_n, g.slice_seq, g.block_off, cls,
           g.ack_seen);
    fflush(stdout);

    /* Write a compact machine-readable side file for the runner. */
    {
        const char *sp = getenv("JJFB_P22C_SUMMARY");
        FILE *sf;
        if (!sp || !sp[0]) sp = "out/p22/p22_runtime_summary.txt";
        sf = fopen(sp, "wb");
        if (sf) {
            fprintf(sf,
                    "gl_base=0x%X\ngl_size=0x%X\nerw=0x%X\np_guest=0x%X\ngeneration=%u\n"
                    "package=%s\nsha=%s\nok77AE=%d\nok7B6C=%d\n"
                    "hit77AE=%u\nhit1344A=%u\nhit133E0=%u\nhit10740=%u\nhit7B6C=%u\nhitFF00=%u\n"
                    "hit7DB0=%u\nhitD964=%u\nhitD768=%u\nhitEAFA=%u\nhit10814=%u\nhit10A24=%u\n"
                    "hit10D36=%u\ngl_insn=%u\n"
                    "slice_seq=%u\nstop=%s\nblock_found=%d\nblock_pc=0x%X\nblock_off=0x%X\n"
                    "block_insn=%s\nblock_cmp=%s\nblock_actual=%s\nblock_loader_path=%s\n"
                    "block_value=0x%X\nblock_expect=0x%X\nblock_writer=%s\nblock_producer=%s\n"
                    "ack_seen=%d\nack_ret=0x%X\nack_affects_gate=%d\nclass=%s\n"
                    "parser_cfn_enter=%d\nparser_gl_enter=%d\n",
                    g.gl_base, g.gl_size, g.erw, g.p_guest, g.generation,
                    g.package_owner[0] ? g.package_owner : "-", g.dump_sha, g.bytes_ok_77ae,
                    g.bytes_ok_7b6c, g.hit_77ae, g.hit_1344a, g.hit_133e0, g.hit_10740, g.hit_7b6c,
                    g.hit_ff00, g.hit_7db0, g.hit_d964, g.hit_d768, g.hit_eafa, g.hit_10814,
                    g.hit_10a24, g.hit_10d36, g.gl_insn_n, g.slice_seq, g.stop_reason,
                    g.block_found, g.block_pc, g.block_off, g.block_insn, g.block_cmp,
                    g.block_actual, g.block_loader_path, g.block_value, g.block_expect,
                    g.block_writer, g.block_producer, g.ack_seen, g.ack_ret, g.ack_affects_gate, cls,
                    g.parser_cfn_enter, g.parser_gl_enter);
            fclose(sf);
        }
    }
}
