#include "gwy_launcher/boot_successor_trace.h"

#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/platform_mrp_resource.h"
#include "gwy_launcher/product_runtime_progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <time.h>
#endif

#define RES_CAP 64u
#define TIMELINE_CAP 4096u
#define PC_HIST_CAP 512u
#define PC_SAMPLE_CAP 20000u
#define BB_CAP 256u
#define STACK_DUMP 0x64u
#define SP_OBS_CAP 64u
#define SP_ROW_CAP 320u

typedef struct {
    uint32_t call_id;
    uint32_t resource_sequence;
    char member_name[128];
    char tag[32];
    uint32_t pc;
    uint32_t sp;
    uint32_t lr;
    uint32_t r0;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r9;
} SpObsRow;

typedef struct {
    uint32_t call_id;
    uint32_t dispatch_enter_sp;
    char member_name[128];
    int has_enter;
    int has_caller;
    int32_t caller_sp_delta;
} SpCallCtx;

typedef struct {
    char member[128];
    uint32_t pc;
    uint32_t lr;
    char package[64];
    uint64_t gen;
    char via[32];
} ResRec;

typedef struct {
    char tag[48];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    char detail[96];
} TimelineRec;

typedef struct {
    uint32_t pc;
    uint32_t count;
} PcHist;

static int g_armed;
static int g_dense;
static int g_en_known;
static int g_en;
static int g_atexit;
static uint32_t g_res_n;
static ResRec g_res[RES_CAP];
static uint32_t g_tl_n;
static TimelineRec g_tl[TIMELINE_CAP];
static uint32_t g_note_pixels_n;
static int g_post5;
static uint32_t g_post5_pc_n;
static uint32_t g_post5_pcs[PC_SAMPLE_CAP];
static uint32_t g_hist_n;
static PcHist g_hist[PC_HIST_CAP];
static uint32_t g_bb_n;
static uint32_t g_bbs[BB_CAP];
static uint32_t g_last_pc;
static uint32_t g_first_post_ui_pc;
static uint32_t g_stable_loop_pc;
static uint32_t g_stable_loop_hits;
static uint64_t g_last_res_ms;
static uint64_t g_post5_ms;
static int g_builder_leave;
static int g_no_new_logged;
static uint32_t g_family_1e209_n;
static uint32_t g_plat_10102_n;
static uint32_t g_plat_10140_n;
static uint32_t g_entry_complete_n;
static uint32_t g_sp_call_id;
static uint32_t g_sp_row_n;
static SpObsRow g_sp_rows[SP_ROW_CAP];
static SpCallCtx g_sp_calls[SP_OBS_CAP];
static uint32_t g_lookup_cont_hit;
static uint32_t g_dispatch_epilogue_hit;
static uint32_t g_caller_cont_hit;
static char g_resume_mode[16];

static FILE *open_report(const char *name, const char *mode);
#ifdef GWY_HAVE_UNICORN
static uc_hook g_pc_hook;
static uc_hook g_sp_hooks[4];
static int g_pc_hook_ok;
static int g_sp_hooks_ok;
static void *g_uc;

static void read_regs(uc_engine *uc, uint32_t *sp, uint32_t *lr, uint32_t *r0, uint32_t *r4,
                    uint32_t *r5, uint32_t *r6, uint32_t *r9) {
    if (!uc) return;
    if (sp) uc_reg_read(uc, UC_ARM_REG_SP, sp);
    if (lr) uc_reg_read(uc, UC_ARM_REG_LR, lr);
    if (r0) uc_reg_read(uc, UC_ARM_REG_R0, r0);
    if (r4) uc_reg_read(uc, UC_ARM_REG_R4, r4);
    if (r5) uc_reg_read(uc, UC_ARM_REG_R5, r5);
    if (r6) uc_reg_read(uc, UC_ARM_REG_R6, r6);
    if (r9) uc_reg_read(uc, UC_ARM_REG_R9, r9);
}

static SpCallCtx *sp_call_for(uint32_t call_id) {
    uint32_t i;
    if (!call_id) return NULL;
    for (i = 0; i < SP_OBS_CAP; i++) {
        if (g_sp_calls[i].call_id == call_id) return &g_sp_calls[i];
    }
    for (i = 0; i < SP_OBS_CAP; i++) {
        if (!g_sp_calls[i].call_id) {
            g_sp_calls[i].call_id = call_id;
            return &g_sp_calls[i];
        }
    }
    return NULL;
}

static void sp_row_add(uint32_t call_id, const char *tag, uint32_t pc, uint32_t sp, uint32_t lr,
                       uint32_t r0, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r9,
                       const char *member_name) {
    SpObsRow *row;
    FILE *fp;
    SpCallCtx *ctx;
    if (g_sp_row_n >= SP_ROW_CAP) return;
    row = &g_sp_rows[g_sp_row_n++];
    memset(row, 0, sizeof(*row));
    row->call_id = call_id;
    row->resource_sequence = call_id;
    row->pc = pc;
    row->sp = sp;
    row->lr = lr;
    row->r0 = r0;
    row->r4 = r4;
    row->r5 = r5;
    row->r6 = r6;
    row->r9 = r9;
    snprintf(row->tag, sizeof(row->tag), "%s", tag ? tag : "?");
    if (member_name && member_name[0]) snprintf(row->member_name, sizeof(row->member_name), "%s", member_name);
    ctx = sp_call_for(call_id);
    if (ctx && member_name && member_name[0])
        snprintf(ctx->member_name, sizeof(ctx->member_name), "%s", member_name);

    fp = open_report("p8_sp_invariant.csv", g_sp_row_n == 1 ? "wb" : "ab");
    if (fp) {
        if (g_sp_row_n == 1)
            fprintf(fp, "call_id,resource_sequence,member_name,tag,pc,sp,lr,r0,r4,r5,r6,r9\n");
        fprintf(fp, "%u,%u,\"%s\",%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n", row->call_id,
                row->resource_sequence, row->member_name, row->tag, row->pc, row->sp, row->lr,
                row->r0, row->r4, row->r5, row->r6, row->r9);
        fclose(fp);
    }
}

static void on_sp_dispatch_enter(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t sp, lr, r0, r4, r5, r6, r9;
    SpCallCtx *ctx;
    (void)size;
    (void)user_data;
    g_sp_call_id++;
    read_regs(uc, &sp, &lr, &r0, &r4, &r5, &r6, &r9);
    ctx = sp_call_for(g_sp_call_id);
    if (ctx) {
        ctx->dispatch_enter_sp = sp;
        ctx->has_enter = 1;
        ctx->member_name[0] = 0;
        ctx->has_caller = 0;
        ctx->caller_sp_delta = 0;
    }
    sp_row_add(g_sp_call_id, "DISPATCH_ENTER", (uint32_t)address, sp, lr, r0, r4, r5, r6, r9, "");
}

static void on_sp_lookup_continuation(uc_engine *uc, uint64_t address, uint32_t size,
                                      void *user_data) {
    uint32_t sp, lr, r0, r4, r5, r6, r9;
    SpCallCtx *ctx;
    (void)size;
    (void)user_data;
    g_lookup_cont_hit++;
    read_regs(uc, &sp, &lr, &r0, &r4, &r5, &r6, &r9);
    ctx = sp_call_for(g_sp_call_id);
    sp_row_add(g_sp_call_id, "LOOKUP_CONTINUATION", (uint32_t)address, sp, lr, r0, r4, r5, r6, r9,
               ctx ? ctx->member_name : "");
}

static void on_sp_dispatch_epilogue(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t sp, lr, r0, r4, r5, r6, r9;
    SpCallCtx *ctx;
    (void)size;
    (void)user_data;
    g_dispatch_epilogue_hit++;
    read_regs(uc, &sp, &lr, &r0, &r4, &r5, &r6, &r9);
    ctx = sp_call_for(g_sp_call_id);
    sp_row_add(g_sp_call_id, "DISPATCH_EPILOGUE", (uint32_t)address, sp, lr, r0, r4, r5, r6, r9,
               ctx ? ctx->member_name : "");
}

static void on_sp_caller_continuation(uc_engine *uc, uint64_t address, uint32_t size,
                                      void *user_data) {
    uint32_t sp, lr, r0, r4, r5, r6, r9;
    SpCallCtx *ctx;
    (void)size;
    (void)user_data;
    g_caller_cont_hit++;
    read_regs(uc, &sp, &lr, &r0, &r4, &r5, &r6, &r9);
    ctx = sp_call_for(g_sp_call_id);
    if (ctx && ctx->has_enter) {
        ctx->has_caller = 1;
        ctx->caller_sp_delta = (int32_t)sp - (int32_t)ctx->dispatch_enter_sp;
    }
    sp_row_add(g_sp_call_id, "CALLER_CONTINUATION", (uint32_t)address, sp, lr, r0, r4, r5, r6, r9,
               ctx ? ctx->member_name : "");
}

static int arm_sp_obs_hooks(uc_engine *uc) {
    uint32_t i;
    struct {
        uint32_t pc;
        void *cb;
    } sites[4] = {
        {PLATFORM_MRP_DISPATCH_ENTRY_PC, (void *)on_sp_dispatch_enter},
        {PLATFORM_MRP_LOOKUP_CONTINUATION_PC, (void *)on_sp_lookup_continuation},
        {PLATFORM_MRP_DISPATCH_EPILOGUE_PC, (void *)on_sp_dispatch_epilogue},
        {PLATFORM_MRP_CALLER_CONTINUATION_PC, (void *)on_sp_caller_continuation},
    };
    if (!uc || g_sp_hooks_ok) return g_sp_hooks_ok;
    for (i = 0; i < 4; i++) {
        if (uc_hook_add(uc, &g_sp_hooks[i], UC_HOOK_CODE, sites[i].cb, NULL, sites[i].pc,
                        sites[i].pc) != UC_ERR_OK)
            return 0;
    }
    g_sp_hooks_ok = 1;
    printf("[P8_SP_OBS] armed dispatch=0x%X cont=0x%X epilogue=0x%X caller=0x%X evidence=OBSERVED\n",
           PLATFORM_MRP_DISPATCH_ENTRY_PC, PLATFORM_MRP_LOOKUP_CONTINUATION_PC,
           PLATFORM_MRP_DISPATCH_EPILOGUE_PC, PLATFORM_MRP_CALLER_CONTINUATION_PC);
    fflush(stdout);
    return 1;
}

static void on_code_hook(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)size;
    (void)user_data;
    boot_successor_on_pc(uc, (uint32_t)address);
}
#endif

static uint64_t now_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
#endif
}

static int env1(const char *k) {
    const char *v = getenv(k);
    return v && v[0] == '1' && v[1] == '\0';
}

static int enabled(void) {
    if (!g_en_known) {
        /* Light markers always on unless explicitly disabled. */
        if (getenv("JJFB_BOOT_SUCCESSOR_TRACE") && getenv("JJFB_BOOT_SUCCESSOR_TRACE")[0] == '0')
            g_en = 0;
        else
            g_en = 1;
        g_dense = env1("JJFB_BOOT_SUCCESSOR_TRACE");
        g_en_known = 1;
    }
    return g_en;
}

static void ensure_reports(void) {
#ifdef _WIN32
    CreateDirectoryA("reports", NULL);
    CreateDirectoryA("../../reports", NULL);
#else
    mkdir("reports", 0755);
    mkdir("../../reports", 0755);
#endif
}

static FILE *open_report(const char *name, const char *mode) {
    char path[512];
    FILE *fp;
    ensure_reports();
    snprintf(path, sizeof(path), "../../reports/%s", name);
    fp = fopen(path, mode);
    if (fp) return fp;
    snprintf(path, sizeof(path), "reports/%s", name);
    return fopen(path, mode);
}

static void tl_add(const char *tag, uint32_t a, uint32_t b, uint32_t c, const char *detail) {
    TimelineRec *t;
    FILE *fp;
    if (g_tl_n >= TIMELINE_CAP) return;
    t = &g_tl[g_tl_n++];
    memset(t, 0, sizeof(*t));
    snprintf(t->tag, sizeof(t->tag), "%s", tag ? tag : "?");
    t->a = a;
    t->b = b;
    t->c = c;
    snprintf(t->detail, sizeof(t->detail), "%s", detail ? detail : "");
    printf("[BOOT_%s] a=0x%X b=0x%X c=0x%X detail=%s evidence=OBSERVED\n", t->tag, a, b, c,
           t->detail);
    fflush(stdout);
    product_runtime_progress_emit("boot_successor", t->tag, t->detail);
    /* Kill-safe incremental CSV (TerminateProcess skips atexit). */
    fp = open_report("p6_post_resource5_timeline.csv", g_tl_n == 1 ? "wb" : "ab");
    if (fp) {
        if (g_tl_n == 1) fprintf(fp, "idx,tag,a,b,c,detail\n");
        fprintf(fp, "%u,%s,0x%X,0x%X,0x%X,\"%s\"\n", g_tl_n - 1u, t->tag, t->a, t->b, t->c,
                t->detail);
        fclose(fp);
    }
    if ((g_tl_n % 16u) == 0u || g_res_n == 5u) boot_successor_trace_flush("incremental");
}

static void hist_add(uint32_t pc) {
    uint32_t i;
    for (i = 0; i < g_hist_n; i++) {
        if (g_hist[i].pc == pc) {
            g_hist[i].count++;
            if (g_hist[i].count > g_stable_loop_hits) {
                g_stable_loop_hits = g_hist[i].count;
                g_stable_loop_pc = pc;
            }
            return;
        }
    }
    if (g_hist_n >= PC_HIST_CAP) return;
    g_hist[g_hist_n].pc = pc;
    g_hist[g_hist_n].count = 1;
    g_hist_n++;
}

static void maybe_bb(uint32_t pc) {
    uint32_t i;
    if ((pc & ~1u) == (g_last_pc & ~1u)) return;
    /* Treat large PC jump as new basic-block seed. */
    if (g_last_pc && ((pc > g_last_pc + 16u) || (pc + 16u < g_last_pc))) {
        for (i = 0; i < g_bb_n; i++)
            if (g_bbs[i] == (pc & ~1u)) return;
        if (g_bb_n < BB_CAP) g_bbs[g_bb_n++] = pc & ~1u;
    }
}

void boot_successor_trace_reset(void) {
    g_armed = 0;
    g_en_known = 0;
    g_en = 0;
    g_dense = 0;
    g_res_n = 0;
    g_tl_n = 0;
    g_note_pixels_n = 0;
    g_post5 = 0;
    g_post5_pc_n = 0;
    g_hist_n = 0;
    g_bb_n = 0;
    g_last_pc = 0;
    g_first_post_ui_pc = 0;
    g_stable_loop_pc = 0;
    g_stable_loop_hits = 0;
    g_last_res_ms = 0;
    g_post5_ms = 0;
    g_builder_leave = 0;
    g_no_new_logged = 0;
    g_family_1e209_n = 0;
    g_plat_10102_n = 0;
    g_plat_10140_n = 0;
    g_entry_complete_n = 0;
    g_sp_call_id = 0;
    g_sp_row_n = 0;
    g_lookup_cont_hit = 0;
    g_dispatch_epilogue_hit = 0;
    g_caller_cont_hit = 0;
    g_resume_mode[0] = 0;
    memset(g_sp_rows, 0, sizeof(g_sp_rows));
    memset(g_sp_calls, 0, sizeof(g_sp_calls));
    memset(g_res, 0, sizeof(g_res));
    memset(g_tl, 0, sizeof(g_tl));
    memset(g_hist, 0, sizeof(g_hist));
}

static void boot_atexit(void) { boot_successor_trace_flush("atexit"); }

void boot_successor_trace_arm(void *uc) {
    if (!enabled()) return;
    g_armed = 1;
#ifdef GWY_HAVE_UNICORN
    g_uc = uc;
    if (uc) (void)arm_sp_obs_hooks((uc_engine *)uc);
    if (g_dense && uc && !g_pc_hook_ok) {
        if (uc_hook_add((uc_engine *)uc, &g_pc_hook, UC_HOOK_CODE, (void *)on_code_hook, NULL, 1,
                        0) == UC_ERR_OK)
            g_pc_hook_ok = 1;
    }
#else
    (void)uc;
#endif
    if (!g_atexit) {
        atexit(boot_atexit);
        g_atexit = 1;
    }
    printf("[BOOT_SUCCESSOR] armed dense=%d evidence=OBSERVED\n", g_dense);
    fflush(stdout);
}

void boot_successor_on_mrp_dispatch_enter(void *uc) {
    (void)uc;
    /* Recorded by on_sp_dispatch_enter hook. */
}

void boot_successor_on_mrp_lookup_callsite(void *uc, const char *member_name) {
    uint32_t sp, lr, r0, r4, r5, r6, r9;
    SpCallCtx *ctx;
    static int s_dumped_prologue;
    if (!enabled()) return;
#ifdef GWY_HAVE_UNICORN
    /* DISPATCH_ENTER (0x30450C) may not fire if 0x304BF0 is a direct BL target.
     * Assign call_id at LOOKUP so SP rows correlate across one resource attempt. */
    if (!g_sp_call_id || (ctx = sp_call_for(g_sp_call_id)) == NULL || ctx->has_caller ||
        (ctx->member_name[0] && member_name && member_name[0] &&
         strcmp(ctx->member_name, member_name) != 0)) {
        g_sp_call_id++;
        ctx = sp_call_for(g_sp_call_id);
    } else {
        ctx = sp_call_for(g_sp_call_id);
    }
    read_regs((uc_engine *)uc, &sp, &lr, &r0, &r4, &r5, &r6, &r9);
    if (ctx) {
        if (!ctx->has_enter) {
            ctx->dispatch_enter_sp = sp;
            ctx->has_enter = 1;
        }
        if (member_name && member_name[0])
            snprintf(ctx->member_name, sizeof(ctx->member_name), "%s", member_name);
    }
    if (!s_dumped_prologue && uc) {
        uint8_t code[16];
        memset(code, 0, sizeof(code));
        if (guest_memory_uc_peek((struct uc_struct *)uc, PLATFORM_MRP_LOOKUP_CALLSITE_PC, code,
                                 sizeof(code))) {
            printf("[P8_PROLOGUE_DUMP] pc=0x%X lr=0x%X sp=0x%X bytes=",
                   PLATFORM_MRP_LOOKUP_CALLSITE_PC, lr, sp);
            {
                int i;
                char hex[48];
                for (i = 0; i < 16; i++) {
                    printf("%02X", code[i]);
                    snprintf(hex + i * 2, 3, "%02X", code[i]);
                }
                hex[32] = 0;
                product_runtime_progress_emit("p8_prologue_dump", "0x304BF0", hex);
            }
            printf(" note=lr_is_builder_not_304BF4_implies_callee_entry evidence=OBSERVED\n");
            fflush(stdout);
            s_dumped_prologue = 1;
        }
    }
    sp_row_add(g_sp_call_id ? g_sp_call_id : 1u, "LOOKUP_CALLSITE",
               PLATFORM_MRP_LOOKUP_CALLSITE_PC, sp, lr, r0, r4, r5, r6, r9,
               member_name ? member_name : "");
#else
    (void)uc;
    (void)member_name;
    (void)ctx;
#endif
}

void boot_successor_on_mrp_lookup_continuation(void *uc) { (void)uc; }
void boot_successor_on_mrp_dispatch_epilogue(void *uc) { (void)uc; }
void boot_successor_on_mrp_caller_continuation(void *uc) { (void)uc; }

void boot_successor_on_mrp_resume(const char *mode, const char *via, const char *member_name) {
    if (!enabled()) return;
    snprintf(g_resume_mode, sizeof(g_resume_mode), "%s", mode ? mode : "direct_lr");
    tl_add("MRP_RESUME", 0, 0, 0, mode ? mode : "direct_lr");
    printf("[P8_MRP_RESUME] mode=%s via=%s name=\"%s\" evidence=OBSERVED\n", mode ? mode : "?",
           via ? via : "?", member_name ? member_name : "?");
    fflush(stdout);
}

void boot_successor_note_pixels_legacy_call(void) {
    g_note_pixels_n++;
    printf("[NOTE_PIXELS_LEGACY] call_n=%u note=deprecated_no_enqueue evidence=OBSERVED\n",
           g_note_pixels_n);
    fflush(stdout);
}

uint32_t boot_successor_note_pixels_calls(void) { return g_note_pixels_n; }
uint32_t boot_successor_resource_count(void) { return g_res_n; }

void boot_successor_on_resource_request(const char *member_name, uint32_t caller_pc,
                                        uint32_t caller_lr) {
    if (!enabled() || !g_armed) return;
    if (g_res_n == 0)
        tl_add("UI_BUILDER_ENTER", caller_pc, caller_lr, 0, member_name ? member_name : "");
}

void boot_successor_on_resource_complete(const char *member_name, uint32_t caller_pc,
                                         uint32_t caller_lr, const char *active_package,
                                         uint64_t scope_generation, const char *via) {
    char tag[48];
    ResRec *r;
    if (!enabled()) return;
    if (!g_armed) boot_successor_trace_arm(NULL);
    if (g_res_n == 0)
        tl_add("UI_BUILDER_ENTER", caller_pc, caller_lr, 0, member_name ? member_name : "");
    if (g_res_n < RES_CAP) {
        r = &g_res[g_res_n];
        memset(r, 0, sizeof(*r));
        snprintf(r->member, sizeof(r->member), "%s", member_name ? member_name : "");
        r->pc = caller_pc;
        r->lr = caller_lr;
        snprintf(r->package, sizeof(r->package), "%s", active_package ? active_package : "");
        r->gen = scope_generation;
        snprintf(r->via, sizeof(r->via), "%s", via ? via : "");
    }
    g_res_n++;
    g_last_res_ms = now_ms();
    snprintf(tag, sizeof(tag), "RESOURCE_%u", g_res_n);
    tl_add(tag, caller_pc, caller_lr, (uint32_t)scope_generation, member_name ? member_name : "");

    if (g_res_n == 5u && !g_post5) {
        g_post5 = 1;
        g_post5_ms = now_ms();
        tl_add("LAST_RESOURCE_COMPLETE", caller_pc, caller_lr, 5u, member_name ? member_name : "");
        /* Heuristic: same LR for all five → builder still in one function until it returns. */
        tl_add("UI_BUILDER_LEAVE_CANDIDATE", caller_pc, caller_lr, 0, "await_pc_leave_builder");
    }
    if (g_res_n >= 6u) {
        tl_add("SUCCESSOR_RESOURCE_6", caller_pc, caller_lr, g_res_n, member_name ? member_name : "");
        if (caller_lr != 0x2D93D1u)
            tl_add("SUCCESSOR_NEW_CALLER_LR", caller_lr, caller_pc, 0, member_name ? member_name : "");
        if (active_package && active_package[0])
            tl_add("SUCCESSOR_ACTIVE_PACKAGE", 0, 0, 0, active_package);
    }
}

void boot_successor_on_304bf0_entry_complete(const char *member_name, uint32_t out_va, uint32_t lr,
                                            int skipped_native) {
    if (!enabled()) return;
    g_entry_complete_n++;
    if (skipped_native) {
        char det[96];
        snprintf(det, sizeof(det), "%s", member_name ? member_name : "");
        tl_add("ENTRY_COMPLETE_SKIPPED_NATIVE", out_va, lr, g_entry_complete_n, det);
        printf("[ENTRY_COMPLETE_MISSING_SIDE_EFFECT] name=\"%s\" out=0x%X lr=0x%X "
               "note=native_304BF0_body_not_executed candidates=refcount,object_registry,"
               "completion_callback,package_context,state_fields,ui_schedule evidence=OBSERVED\n",
               member_name ? member_name : "?", out_va, lr);
        fflush(stdout);
    }
}

void boot_successor_on_pc(void *uc, uint32_t pc) {
    (void)uc;
    if (!enabled() || !g_post5) {
        g_last_pc = pc;
        return;
    }
    if (!g_first_post_ui_pc) {
        g_first_post_ui_pc = pc;
        tl_add("FIRST_POST_UI_PC", pc, 0, 0, "");
    }
    /* Builder leave: PC leaves the 0x2D92E4 resource helper region. */
    if (!g_builder_leave && (pc < 0x2D9000u || pc > 0x2D9800u) && g_res_n >= 5u) {
        g_builder_leave = 1;
        tl_add("UI_BUILDER_LEAVE", pc, 0x2D93D1u, 0, "left_2D92E4_band");
    }
    if (g_post5_pc_n < PC_SAMPLE_CAP) g_post5_pcs[g_post5_pc_n++] = pc;
    hist_add(pc & ~1u);
    maybe_bb(pc);
    g_last_pc = pc;
    if (!g_no_new_logged && g_post5_ms && (now_ms() - g_last_res_ms) > 5000ull) {
        g_no_new_logged = 1;
        tl_add("NO_NEW_RESOURCE_FOR_MS", (uint32_t)(now_ms() - g_last_res_ms), g_stable_loop_pc,
               g_res_n, "stable_after_res5");
        if (g_stable_loop_pc)
            tl_add("STABLE_LOOP_ENTER", g_stable_loop_pc, g_stable_loop_hits, 0, "");
    }
}

void boot_successor_on_platform(uint32_t code, uint32_t app, uint32_t r2, uint32_t r3,
                                uint32_t caller_pc, uint32_t lr, uint32_t sp, uint32_t r9) {
    char det[96];
    if (!enabled()) return;
    if (code == 0x10102u) g_plat_10102_n++;
    if (code == 0x10140u) g_plat_10140_n++;
    if (code == 0x1E209u) g_family_1e209_n++;
    if (g_post5 || code == 0x1E209u || code == 0x10102u || code == 0x10140u) {
        snprintf(det, sizeof(det), "app=0x%X r2=0x%X r3=0x%X sp=0x%X r9=0x%X", app, r2, r3, sp, r9);
        tl_add(code == 0x1E209u   ? "FAMILY_1E209"
               : code == 0x10102u ? "PLAT_10102"
               : code == 0x10140u ? "PLAT_10140"
                                  : "PLAT_OTHER",
               code, caller_pc, lr, det);
    }
}

void boot_successor_on_family_handler_enter(void *uc, uint32_t event_code, uint32_t app,
                                            uint32_t handler, uint32_t r0, uint32_t r1, uint32_t r2,
                                            uint32_t r3, uint32_t sp, uint32_t lr, uint32_t r9) {
    FILE *fp;
    uint32_t stack[24];
    uint32_t i;
    char det[96];
    if (!enabled()) return;
    memset(stack, 0, sizeof(stack));
#ifdef GWY_HAVE_UNICORN
    if (uc && sp) {
        for (i = 0; i < 24; i++)
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + i * 4u, &stack[i]);
    }
#else
    (void)uc;
#endif
    snprintf(det, sizeof(det), "handler=0x%X r2=0x%X r3=0x%X", handler, r2, r3);
    tl_add("FAMILY_HANDLER_ENTER", event_code, app, handler, det);

    fp = open_report("p7_family_event_abi.csv", "ab");
    if (fp) {
        long pos = ftell(fp);
        if (pos == 0)
            fprintf(fp, "event,app,handler,r0,r1,r2,r3,sp,lr,r9,"
                        "sp0,sp4,sp8,sp12,sp16,sp20,sp24,sp28,sp32,sp36,sp40,sp44\n");
        fprintf(fp,
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n",
                event_code, app, handler, r0, r1, r2, r3, sp, lr, r9, stack[0], stack[1], stack[2],
                stack[3], stack[4], stack[5], stack[6], stack[7], stack[8], stack[9], stack[10],
                stack[11]);
        fclose(fp);
    }
    printf("[P7_FAMILY_ABI] event=0x%X app=0x%X handler=0x%X r0-r3=0x%X,0x%X,0x%X,0x%X "
           "sp+32=0x%X sp+36=0x%X lr=0x%X r9=0x%X evidence=OBSERVED\n",
           event_code, app, handler, r0, r1, r2, r3, stack[8], stack[9], lr, r9);
    fflush(stdout);
}

void boot_successor_on_timer(uint32_t code, uint32_t period_ms) {
    if (!enabled()) return;
    tl_add("TIMER", code, period_ms, 0, "");
}

void boot_successor_trace_flush(const char *reason) {
    FILE *fp;
    uint32_t i;
    if (!enabled() && g_tl_n == 0 && g_res_n == 0) return;

    fp = open_report("p6_post_resource5_timeline.csv", "wb");
    if (fp) {
        fprintf(fp, "idx,tag,a,b,c,detail\n");
        for (i = 0; i < g_tl_n; i++)
            fprintf(fp, "%u,%s,0x%X,0x%X,0x%X,\"%s\"\n", i, g_tl[i].tag, g_tl[i].a, g_tl[i].b,
                    g_tl[i].c, g_tl[i].detail);
        fclose(fp);
    }

    fp = open_report("p6_post_resource5_pc_histogram.csv", "wb");
    if (fp) {
        fprintf(fp, "pc,count\n");
        for (i = 0; i < g_hist_n; i++)
            fprintf(fp, "0x%X,%u\n", g_hist[i].pc, g_hist[i].count);
        fprintf(fp, "# samples=%u bbs=%u\n", g_post5_pc_n, g_bb_n);
        for (i = 0; i < g_bb_n && i < 64; i++) fprintf(fp, "# bb=0x%X\n", g_bbs[i]);
        fclose(fp);
    }

    fp = open_report("p6_post_resource5_verdict.md", "wb");
    if (fp) {
        fprintf(fp,
                "# P6 post-resource5 verdict\n\n"
                "reason: %s\n\n"
                "## Answers\n\n"
                "1. Initial UI builder return: %s (marker UI_BUILDER_LEAVE%s)\n"
                "2. First stable loop PC: 0x%X (hits=%u)\n"
                "3. Resources completed: %u; 6th natural: %s\n"
                "4. FIRST_POST_UI_PC: 0x%X\n"
                "5. NOTE_PIXELS_LEGACY_CALLS: %u\n"
                "6. entry_complete skips: %u\n"
                "7. family 0x1E209 notes: %u; 0x10102: %u; 0x10140: %u\n\n"
                "## RAW_BLOB status\n\n"
                "RAW_BLOB_CONTRACT = HOST_PROBE_CANDIDATE\n"
                "RAW_BLOB_NATURAL_FLOW_VALIDATED = NO\n"
                "(ani/txt natural requests still 0 in prior census)\n\n"
                "## Hypothesis\n\n"
                "All early resources share caller_lr=0x2D93D1. After resource 5, no new "
                "0x304BF0 request implies boot successor (family event ABI and/or native "
                "304BF0 side effects) is incomplete — not missing ANI whitelist.\n",
                reason ? reason : "?", g_builder_leave ? "YES/CANDIDATE" : "UNKNOWN",
                g_builder_leave ? "" : " absent", g_stable_loop_pc, g_stable_loop_hits, g_res_n,
                g_res_n >= 6u ? "YES" : "NO", g_first_post_ui_pc, g_note_pixels_n,
                g_entry_complete_n, g_family_1e209_n, g_plat_10102_n, g_plat_10140_n);
        fclose(fp);
    }

    fp = open_report("p8_sp_invariant.csv", "wb");
    if (fp) {
        fprintf(fp, "call_id,resource_sequence,member_name,tag,pc,sp,lr,r0,r4,r5,r6,r9\n");
        for (i = 0; i < g_sp_row_n; i++)
            fprintf(fp, "%u,%u,\"%s\",%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\n",
                    g_sp_rows[i].call_id, g_sp_rows[i].resource_sequence, g_sp_rows[i].member_name,
                    g_sp_rows[i].tag, g_sp_rows[i].pc, g_sp_rows[i].sp, g_sp_rows[i].lr,
                    g_sp_rows[i].r0, g_sp_rows[i].r4, g_sp_rows[i].r5, g_sp_rows[i].r6,
                    g_sp_rows[i].r9);
        fclose(fp);
    }

    fp = open_report("p8_callsite_stack_contract_runtime.md", "wb");
    if (fp) {
        uint32_t sp_ok = 0;
        uint32_t sp_bad = 0;
        for (i = 0; i < SP_OBS_CAP; i++) {
            if (!g_sp_calls[i].call_id || !g_sp_calls[i].has_caller) continue;
            if (g_sp_calls[i].caller_sp_delta == 0)
                sp_ok++;
            else
                sp_bad++;
        }
        fprintf(fp,
                "# P8 callsite stack contract\n\n"
                "reason: %s\n\n"
                "## Resume mode\n\n"
                "JJFB_304BF0_RESUME_MODE = %s\n\n"
                "## Hit counts\n\n"
                "| marker | count |\n"
                "|--------|-------|\n"
                "| LOOKUP_CONTINUATION_HIT | %u |\n"
                "| DISPATCH_EPILOGUE_HIT | %u |\n"
                "| CALLER_CONTINUATION_HIT | %u |\n"
                "| resources completed | %u |\n"
                "| 6th natural resource | %s |\n\n"
                "## SP invariant (DISPATCH_ENTER vs CALLER_CONTINUATION)\n\n"
                "| call_id | member | enter_sp | caller_delta | ok |\n",
                reason ? reason : "?", g_resume_mode[0] ? g_resume_mode : "direct_lr",
                g_lookup_cont_hit, g_dispatch_epilogue_hit, g_caller_cont_hit, g_res_n,
                g_res_n >= 6u ? "YES" : "NO");
        for (i = 0; i < SP_OBS_CAP; i++) {
            if (!g_sp_calls[i].call_id) continue;
            fprintf(fp, "| %u | %s | 0x%X | %d | %s |\n", g_sp_calls[i].call_id,
                    g_sp_calls[i].member_name, g_sp_calls[i].dispatch_enter_sp,
                    g_sp_calls[i].caller_sp_delta,
                    g_sp_calls[i].has_caller && g_sp_calls[i].caller_sp_delta == 0 ? "YES"
                    : (g_sp_calls[i].has_caller ? "NO" : "pending"));
        }
        fprintf(fp,
                "\nCALLER_SP_DELTA_OK=%u CALLER_SP_DELTA_BAD=%u\n\n"
                "## Acceptance (callsite mode)\n\n"
                "LOOKUP_CONTINUATION_HIT=5, DISPATCH_EPILOGUE_HIT=5, "
                "CALLER_CONTINUATION_HIT=5, CALLER_SP_DELTA=0 for all five.\n\n"
                "## Verdict\n\n",
                sp_ok, sp_bad);
        if (strcmp(g_resume_mode, "callsite") == 0) {
            if (g_lookup_cont_hit >= 5u && g_dispatch_epilogue_hit >= 5u &&
                g_caller_cont_hit >= 5u && sp_bad == 0u && sp_ok >= 5u)
                fprintf(fp, "P8_STACK_CONTRACT = PASS (callsite continuation + SP invariant)\n");
            else if (g_res_n >= 6u)
                fprintf(fp, "P8_SUCCESSOR_GATE = PASS (6th natural resource)\n");
            else
                fprintf(fp,
                        "P8_STACK_CONTRACT = PARTIAL (callsite hits or SP delta incomplete)\n");
        } else {
            fprintf(fp,
                    "P8_BASELINE = direct_lr (continuation hits expected 0; compare with "
                    "callsite A/B)\n");
        }
        fclose(fp);
    }

    printf("[BOOT_SUCCESSOR] flush reason=%s res=%u timeline=%u hist=%u note_pixels=%u "
           "p8_cont=%u p8_epilogue=%u p8_caller=%u stable_pc=0x%X evidence=OBSERVED\n",
           reason ? reason : "?", g_res_n, g_tl_n, g_hist_n, g_note_pixels_n, g_lookup_cont_hit,
           g_dispatch_epilogue_hit, g_caller_cont_hit, g_stable_loop_pc);
    fflush(stdout);
}