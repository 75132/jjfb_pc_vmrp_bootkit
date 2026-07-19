#include "gwy_launcher/ext_callback_frame.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/ext_r9_scope_audit.h"
#include "gwy_launcher/ext_post_cont_audit.h"
#include "gwy_launcher/ext_post_cfn_r9_audit.h"
#include "gwy_launcher/ext_p_extchunk_audit.h"
#include "gwy_launcher/ext_gwy_startgame_audit.h"
#include "gwy_launcher/ext_mrpgcmap_entry_order.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define CF_XFER_MAX 8
#define CF_GATE_LINE                                                               \
    "guest_callback_frame_gate=%s module_r9_switch_gate=open "                     \
    "bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked "                      \
    "er_rw_metadata_timing_gate=blocked nested_r9_scope_gate=%s"
#define CF_GATE_ARGS \
    (g_cf.gate_open ? "open" : "blocked"), (ext_r9_scope_audit_gate_open() ? "open" : "blocked")

typedef struct CfnXfer {
    int active;
    int classified;
    uint64_t callback_id;
    uint64_t module_id;
    char module_name[72];
    char slot_name[48];
    char cfn_path[24]; /* HOST_MAP_FUNC | GUEST_NESTED | LOG_PARSE */
    uint32_t slot_addr;
    uint32_t call_pc;
    uint32_t continuation_pc;
    uint32_t helper;
    uint32_t code_base;
    uint32_t code_size;
    GuestCallbackBoundarySnap before;
    GuestCallbackBoundarySnap after_host;
    GuestCallbackBoundarySnap resume;
    int before_ok;
    int after_ok;
    int resume_ok;
    int continuation_identity; /* 1 confirmed */
    int guest_wrote_r9;
    int r9_switch_interfered;
    int r9_scope_interfered;
    uint32_t first_guest_r9_write_pc;
    char first_r9_loss_writer[48];
    char first_r9_write_reason[48];
    char first_r9_write_callsite[72];
    uint64_t first_r9_write_event_seq;
    uint64_t first_r9_write_frame_id;
    uint32_t function_entry_pc;
    uint32_t memory_access_pc;
    uint32_t pre_r9;
    uint32_t leave_r9;
    uint32_t resume_r9;
    int32_t return_r0;
} CfnXfer;

static struct {
    int enabled;
    int enabled_known;
    void *uc;
    uint64_t next_id;
    CfnXfer xfer[CF_XFER_MAX];
    int xfer_count;
    CallbackFrameClass last_class;
    int gate_open;
    int class_logged;
    char next_fix[48];
    int dual_loader;
    int dual_robotol;
} g_cf;

const char *ext_callback_frame_class_name(CallbackFrameClass c) {
    switch (c) {
    case CF_CLASS_CONTINUATION_CONFIRMED_R9_LOST: return "CONTINUATION_CONFIRMED_R9_LOST";
    case CF_CLASS_CONTINUATION_CONFIRMED_R9_INTACT: return "CONTINUATION_CONFIRMED_R9_INTACT";
    case CF_CLASS_GUEST_WROTE_R9: return "GUEST_WROTE_R9";
    case CF_CLASS_PRE_R9_ALREADY_ZERO: return "PRE_R9_ALREADY_ZERO";
    case CF_CLASS_NOT_CFN_OR_INCONCLUSIVE: return "NOT_CFN_OR_INCONCLUSIVE";
    default: return "UNKNOWN";
    }
}

CallbackFrameClass ext_callback_frame_last_class(void) { return g_cf.last_class; }
int ext_callback_frame_gate_open(void) { return g_cf.gate_open; }

int ext_callback_frame_enabled(void) {
    const char *e;
    if (g_cf.enabled_known) return g_cf.enabled;
    e = getenv("GWY_CALLBACK_FRAME");
    g_cf.enabled = (e && e[0] == '1' && e[1] == '\0') ? 1 : 0;
    g_cf.enabled_known = 1;
    return g_cf.enabled;
}

void ext_callback_frame_reset(void) {
    memset(&g_cf, 0, sizeof(g_cf));
    snprintf(g_cf.next_fix, sizeof(g_cf.next_fix), "%s", "UNKNOWN");
}

void ext_callback_frame_bind_uc(void *uc) { g_cf.uc = uc; }

static int is_cfn_name(const char *name) {
    if (!name || !name[0]) return 0;
    return strstr(name, "mr_c_function_new") != NULL ||
           strstr(name, "_mr_c_function_new") != NULL;
}

static void read_regs(void *uc, uint32_t r[13], uint32_t *sp, uint32_t *lr, uint32_t *pc,
                      uint32_t *cpsr) {
    int i;
#ifdef GWY_HAVE_UNICORN
    uc_engine *u = (uc_engine *)uc;
    if (!u) return;
    for (i = 0; i < 13; i++) uc_reg_read(u, UC_ARM_REG_R0 + i, &r[i]);
    if (sp) uc_reg_read(u, UC_ARM_REG_SP, sp);
    if (lr) uc_reg_read(u, UC_ARM_REG_LR, lr);
    if (pc) uc_reg_read(u, UC_ARM_REG_PC, pc);
    if (cpsr) uc_reg_read(u, UC_ARM_REG_CPSR, cpsr);
#else
    (void)uc;
    (void)r;
    (void)sp;
    (void)lr;
    (void)pc;
    (void)cpsr;
    (void)i;
#endif
}

static void mod_name(const GwyLoadedModule *m, char *out, size_t n) {
    if (!out || !n) return;
    out[0] = 0;
    if (!m) return;
    snprintf(out, n, "%s", m->resolved_name[0] ? m->resolved_name : m->requested_name);
}

static void fill_snap(GuestCallbackBoundarySnap *s, void *uc, uint64_t id, uint64_t mid,
                      uint32_t slot, const char *name, uint32_t call_pc, uint32_t cont) {
    uint32_t pc = 0;
    memset(s, 0, sizeof(*s));
    s->callback_id = id;
    s->guest_module_id = mid;
    s->callback_slot = slot;
    if (name) snprintf(s->slot_name, sizeof(s->slot_name), "%s", name);
    s->call_pc = call_pc;
    s->continuation_pc = cont;
    read_regs(uc, s->r, &s->sp, &s->lr, &pc, &s->cpsr);
    if (!s->continuation_pc) s->continuation_pc = s->lr;
    if (!s->call_pc) s->call_pc = pc;
}

static void mark_dual(CfnXfer *x) {
    if (!x || !x->module_name[0]) return;
    if (strstr(x->module_name, "mrc_loader")) g_cf.dual_loader = 1;
    if (strstr(x->module_name, "robotol")) g_cf.dual_robotol = 1;
    /* Alias: requested cfunction.ext resolves to robotol — count as robotol sample. */
    if (strstr(x->module_name, "cfunction") && x->code_base && x->code_size > 100000u)
        g_cf.dual_robotol = 1;
}

static void fill_snap_from_regs(GuestCallbackBoundarySnap *s, const uint32_t regs[16], uint32_t cpsr,
                                uint64_t id, uint64_t mid, uint32_t slot, const char *name,
                                uint32_t call_pc, uint32_t cont) {
    int i;
    memset(s, 0, sizeof(*s));
    s->callback_id = id;
    s->guest_module_id = mid;
    s->callback_slot = slot;
    if (name) snprintf(s->slot_name, sizeof(s->slot_name), "%s", name);
    s->call_pc = call_pc;
    s->continuation_pc = cont;
    if (regs) {
        for (i = 0; i < 13; i++) s->r[i] = regs[i];
        s->sp = regs[13];
        s->lr = regs[14];
    }
    s->cpsr = cpsr;
    if (!s->continuation_pc) s->continuation_pc = s->lr;
}

static void bind_module(CfnXfer *x, const GwyLoadedModule *m) {
    if (!x || !m) return;
    x->module_id = m->module_id;
    mod_name(m, x->module_name, sizeof(x->module_name));
    x->code_base = m->map.guest_code_base;
    x->code_size = m->map.guest_code_size;
    mark_dual(x);
}

static CfnXfer *alloc_xfer(void) {
    CfnXfer *x;
    if (g_cf.xfer_count >= CF_XFER_MAX) return NULL;
    x = &g_cf.xfer[g_cf.xfer_count++];
    memset(x, 0, sizeof(*x));
    x->callback_id = ++g_cf.next_id;
    return x;
}

static const GwyLoadedModule *module_for_pc(uint32_t pc) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    if (!reg || !pc) return NULL;
    return module_registry_find_by_code_addr(reg, pc & ~1u);
}

static const GwyLoadedModule *module_for_helper(uint32_t helper) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m;
    if (!reg || !helper) return NULL;
    m = module_registry_find_by_helper(reg, helper);
    if (!m) m = module_registry_find_by_code_addr(reg, helper & ~1u);
    return m;
}

static CfnXfer *find_open_by_slot(uint32_t slot) {
    int i;
    for (i = g_cf.xfer_count - 1; i >= 0; i--) {
        if (g_cf.xfer[i].active && g_cf.xfer[i].slot_addr == slot) return &g_cf.xfer[i];
    }
    return NULL;
}

int ext_callback_frame_is_cfn_continuation(uint32_t pc, uint64_t *out_module_id,
                                           uint32_t *out_continuation) {
    int i;
    uint32_t n = pc & ~1u;
    if (!ext_callback_frame_enabled()) return 0;
    for (i = 0; i < g_cf.xfer_count; i++) {
        CfnXfer *x = &g_cf.xfer[i];
        if (!x->continuation_pc) continue;
        if ((x->continuation_pc & ~1u) == n) {
            if (out_module_id) *out_module_id = x->module_id;
            if (out_continuation) *out_continuation = x->continuation_pc;
            return 1;
        }
    }
    return 0;
}

static void emit_boundary(const char *boundary, const GuestCallbackBoundarySnap *s,
                          const CfnXfer *x) {
    uint32_t file_off = 0;
    if (x && x->code_base && s->continuation_pc >= x->code_base)
        file_off = (s->continuation_pc & ~1u) - x->code_base;
    printf("[CALLBACK_FRAME] boundary=%s callback_id=%llu module_id=%llu module=%s "
           "slot=0x%X slot_name=%s call_pc=0x%X continuation_pc=0x%X "
           "continuation_file_offset=0x%X "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r4=0x%X r5=0x%X r6=0x%X r7=0x%X "
           "r8=0x%X r9=0x%X r10=0x%X r11=0x%X r12=0x%X sp=0x%X lr=0x%X cpsr=0x%X "
           "thumb=%s " CF_GATE_LINE " evidence=OBSERVED note=observe_only_no_reg_restore\n",
           boundary, (unsigned long long)s->callback_id, (unsigned long long)s->guest_module_id,
           x && x->module_name[0] ? x->module_name : "?", s->callback_slot, s->slot_name,
           s->call_pc, s->continuation_pc, file_off, s->r[0], s->r[1], s->r[2], s->r[3], s->r[4],
           s->r[5], s->r[6], s->r[7], s->r[8], s->r[9], s->r[10], s->r[11], s->r[12], s->sp,
           s->lr, s->cpsr, ((s->cpsr >> 5) & 1u) ? "yes" : "no",
           CF_GATE_ARGS);
    fflush(stdout);
}

static int callee_saved_changed(const GuestCallbackBoundarySnap *a,
                                const GuestCallbackBoundarySnap *b) {
    int i;
    if (!a || !b) return 0;
    for (i = 4; i <= 11; i++) {
        if (a->r[i] != b->r[i]) return 1;
    }
    if (a->sp != b->sp) return 1;
    if ((a->cpsr & 0x20u) != (b->cpsr & 0x20u)) return 1;
    return 0;
}

static void emit_deltas(CfnXfer *x) {
    int changed;
    const GuestCallbackBoundarySnap *post;
    if (!x || !x->before_ok) return;
    post = x->after_ok ? &x->after_host : (x->resume_ok ? &x->resume : NULL);
    if (!post) return;
    changed = callee_saved_changed(&x->before, post);
    printf("[CALLBACK_R9_DELTA] callback_id=%llu module=%s cfn_path=%s pre=0x%X leave=0x%X "
           "resume=0x%X return_r0=0x%X " CF_GATE_LINE " evidence=OBSERVED\n",
           (unsigned long long)x->callback_id, x->module_name,
           x->cfn_path[0] ? x->cfn_path : "?", x->pre_r9, x->leave_r9, x->resume_r9,
           (uint32_t)x->return_r0, CF_GATE_ARGS);
    printf("[CALLBACK_CALLEE_SAVED_DELTA] callback_id=%llu changed=%s "
           "r4_pre=0x%X r4_post=0x%X r9_pre=0x%X r9_post=0x%X r10_pre=0x%X r10_post=0x%X "
           "sp_pre=0x%X sp_post=0x%X cpsr_t_pre=%u cpsr_t_post=%u " CF_GATE_LINE
           " evidence=OBSERVED\n",
           (unsigned long long)x->callback_id, changed ? "yes" : "no", x->before.r[4], post->r[4],
           x->before.r[9], post->r[9], x->before.r[10], post->r[10], x->before.sp, post->sp,
           (x->before.cpsr >> 5) & 1u, (post->cpsr >> 5) & 1u,
           CF_GATE_ARGS);
    fflush(stdout);
}

static void emit_continuation(CfnXfer *x) {
    uint32_t file_off = 0;
    const char *ident = "UNCONFIRMED";
    if (!x) return;
    if (x->code_base && x->continuation_pc >= x->code_base)
        file_off = (x->continuation_pc & ~1u) - x->code_base;
    if (x->continuation_identity) ident = "CONFIRMED";
    printf("[CALLBACK_CONTINUATION] relation=CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW "
           "callback_id=%llu module=%s module_id=%llu cfn_path=%s call_pc=0x%X "
           "continuation_pc=0x%X continuation_file_offset=0x%X helper=0x%X "
           "helper_ne_continuation=%s identity=%s "
           "superseded_by_callback_continuation_identity=yes causal=false "
           "note=not_module_entry " CF_GATE_LINE " evidence=OBSERVED\n",
           (unsigned long long)x->callback_id, x->module_name,
           (unsigned long long)x->module_id, x->cfn_path[0] ? x->cfn_path : "?", x->call_pc,
           x->continuation_pc, file_off, x->helper,
           (x->helper && (x->helper & ~1u) != (x->continuation_pc & ~1u)) ? "yes" : "no", ident,
           CF_GATE_ARGS);
    fflush(stdout);
}

static void attribute_r9_loss(CfnXfer *x) {
    GwyR9WriteRecord zr;
    if (!x) return;
    x->first_r9_loss_writer[0] = 0;
    /* Prefer exact host R9 write when available (D2). */
    if (guest_memory_r9_first_zeroing(&zr) && x->pre_r9 != 0 &&
        zr.old_r9 == x->pre_r9 && zr.new_r9 == 0) {
        snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s",
                 gwy_r9_write_reason_name(zr.reason));
        snprintf(x->first_r9_write_reason, sizeof(x->first_r9_write_reason), "%s",
                 gwy_r9_write_reason_name(zr.reason));
        snprintf(x->first_r9_write_callsite, sizeof(x->first_r9_write_callsite), "%s",
                 zr.host_callsite[0] ? zr.host_callsite : "?");
        x->first_r9_write_event_seq = zr.event_seq;
        x->first_r9_write_frame_id = zr.frame_id;
        return;
    }
    if (x->pre_r9 == 0) {
        snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s", "PRE_ALREADY_ZERO");
        return;
    }
    if (x->after_ok && x->leave_r9 != x->pre_r9) {
        snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s",
                 "HOST_CALLBACK_BODY");
        return;
    }
    if (x->resume_ok && x->resume_r9 != x->pre_r9) {
        snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s",
                 "HOST_CALLBACK_RESUME_BOUNDARY");
        return;
    }
    if (x->r9_scope_interfered) {
        snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s",
                 "MODULE_R9_SWITCH_LEAVE");
        return;
    }
    if (strcmp(x->cfn_path, "GUEST_NESTED") == 0 || strcmp(x->cfn_path, "LOG_PARSE") == 0) {
        if (x->pre_r9 != 0 && !x->resume_ok) {
            snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s",
                     "NESTED_CFN_CALLEE_PATH");
            return;
        }
    }
    if (x->guest_wrote_r9) {
        snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s", "GUEST_INSN");
        return;
    }
    if (x->r9_switch_interfered) {
        snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s",
                 "MODULE_R9_SWITCH");
        return;
    }
    snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s", "NONE_YET");
}

static void classify_xfer(CfnXfer *x) {
    CallbackFrameClass c = CF_CLASS_NOT_CFN_OR_INCONCLUSIVE;
    const char *next = "UNKNOWN";
    int open = 0;
    int r9_wrong;
    int prefer;
    if (!x || x->classified) return;
    x->classified = 1;
    attribute_r9_loss(x);

    r9_wrong = (x->resume_ok && x->pre_r9 != 0 && x->resume_r9 != x->pre_r9);
    if (!r9_wrong && x->pre_r9 != 0 && !x->resume_ok &&
        (strcmp(x->first_r9_loss_writer, "NESTED_CFN_CALLEE_PATH") == 0 ||
         strcmp(x->first_r9_loss_writer, "MODULE_R9_SWITCH_LEAVE") == 0))
        r9_wrong = 1;

    if (!x->continuation_identity) {
        c = CF_CLASS_NOT_CFN_OR_INCONCLUSIVE;
        next = "UNKNOWN";
    } else if (x->pre_r9 == 0) {
        c = CF_CLASS_PRE_R9_ALREADY_ZERO;
        next = "UNKNOWN";
    } else if (x->guest_wrote_r9 &&
               strcmp(x->first_r9_loss_writer, "NESTED_CFN_CALLEE_PATH") != 0) {
        c = CF_CLASS_GUEST_WROTE_R9;
        next = "UNKNOWN";
    } else if (r9_wrong &&
               (strcmp(x->first_r9_loss_writer, "HOST_CALLBACK_RESUME_BOUNDARY") == 0 ||
                strcmp(x->first_r9_loss_writer, "HOST_CALLBACK_BODY") == 0)) {
        c = CF_CLASS_CONTINUATION_CONFIRMED_R9_LOST;
        next = "GUEST_CALLBACK_FRAME_RESTORE_ONLY";
        if (strcmp(x->first_r9_loss_writer, "HOST_CALLBACK_RESUME_BOUNDARY") == 0 &&
            !x->guest_wrote_r9)
            open = 1;
    } else if (r9_wrong &&
               (strcmp(x->first_r9_loss_writer, "MODULE_R9_SWITCH_LEAVE") == 0 ||
                strcmp(x->first_r9_loss_writer, "NESTED_CFN_CALLEE_PATH") == 0)) {
        c = CF_CLASS_CONTINUATION_CONFIRMED_R9_LOST;
        next = "UNKNOWN";
        open = 0;
    } else if (x->continuation_identity && x->resume_ok && x->resume_r9 == x->pre_r9) {
        c = CF_CLASS_CONTINUATION_CONFIRMED_R9_INTACT;
        next = "UNKNOWN";
    }

    prefer = (x->pre_r9 != 0 && x->continuation_identity &&
              (strstr(x->module_name, "mrc_loader") || strstr(x->module_name, "robotol") ||
               strstr(x->module_name, "cfunction")))
                 ? 1
                 : 0;

    printf("[CALLBACK_R9_LOSS] callback_id=%llu module=%s cfn_path=%s pre_r9=0x%X resume_r9=0x%X "
           "first_r9_loss_writer=%s first_r9_write_reason=%s first_r9_write_host_callsite=%s "
           "first_r9_write_event_seq=%llu first_r9_write_frame_id=%llu "
           "guest_wrote_r9=%s guest_write_pc=0x%X "
           "r9_switch_interfered=%s r9_scope_interfered=%s " CF_GATE_LINE " evidence=OBSERVED\n",
           (unsigned long long)x->callback_id, x->module_name,
           x->cfn_path[0] ? x->cfn_path : "?", x->pre_r9, x->resume_r9,
           x->first_r9_loss_writer[0] ? x->first_r9_loss_writer : "NONE",
           x->first_r9_write_reason[0] ? x->first_r9_write_reason : "NONE",
           x->first_r9_write_callsite[0] ? x->first_r9_write_callsite : "?",
           (unsigned long long)x->first_r9_write_event_seq,
           (unsigned long long)x->first_r9_write_frame_id, x->guest_wrote_r9 ? "yes" : "no",
           x->first_guest_r9_write_pc, x->r9_switch_interfered ? "yes" : "no",
           x->r9_scope_interfered ? "yes" : "no", CF_GATE_ARGS);

    /* Only preferred MRP samples (or first) update the summary class line. */
    if (!g_cf.class_logged || prefer) {
        g_cf.last_class = c;
        if (open) g_cf.gate_open = 1;
        snprintf(g_cf.next_fix, sizeof(g_cf.next_fix), "%s", next);
        printf("[CALLBACK_FRAME_CLASS] class=%s continuation_identity=%s cfn_path=%s "
               "callback_pre_r9=0x%X callback_resume_r9=0x%X first_r9_loss_writer=%s "
               "guest_wrote_r9=%s dual_mrc_loader=%s dual_robotol=%s "
               "gate_evidence=%s next_allowed_fix=%s " CF_GATE_LINE
               " evidence=OBSERVED note=observe_only_no_frame_restore\n",
               ext_callback_frame_class_name(c),
               x->continuation_identity ? "CONFIRMED" : "UNCONFIRMED",
               x->cfn_path[0] ? x->cfn_path : "?", x->pre_r9, x->resume_r9,
               x->first_r9_loss_writer[0] ? x->first_r9_loss_writer : "NONE",
               x->guest_wrote_r9 ? "yes" : "no", g_cf.dual_loader ? "yes" : "no",
               g_cf.dual_robotol ? "yes" : "no",
               open ? "DOCUMENTED_STATIC_OBSERVED" : "OBSERVED", next,
               CF_GATE_ARGS);
        g_cf.class_logged = 1;
    }
    fflush(stdout);
}

static int thumb_writes_r9(uint32_t insn16, uint32_t insn32hi) {
    /* MOV Rd,Rm high: 01000110 ... includes r9 when Rd/Rm encoded high. */
    if ((insn16 & 0xFF00u) == 0x4600u) {
        int rd = (int)((insn16 & 7u) | ((insn16 >> 4) & 8u));
        if (rd == 9) return 1;
    }
    /* ADD/MOV with high regs involving r9 as dest via 0x44/0x46 already covered partly. */
    if ((insn16 & 0xFF87u) == 0x4487u) return 1; /* ADD r9, Rm low form rare */
    (void)insn32hi;
    return 0;
}

static int arm_writes_r9(uint32_t insn) {
    uint32_t op;
    if ((insn & 0x0E000000u) == 0x00000000u || (insn & 0x0E000000u) == 0x02000000u) {
        /* data processing: Rd bits 15:12 */
        op = (insn >> 21) & 0xFu;
        if (op <= 0xFu) {
            uint32_t rd = (insn >> 12) & 0xFu;
            if (rd == 9u && ((insn >> 25) & 1u) == 0u) {
                /* register / immediate DP that writes Rd (not TST/TEQ/CMP/CMN) */
                if (op != 0x8u && op != 0x9u && op != 0xAu && op != 0xBu) return 1;
            }
            if (rd == 9u && ((insn >> 25) & 1u) == 1u) {
                if (op != 0x8u && op != 0x9u && op != 0xAu && op != 0xBu) return 1;
            }
        }
    }
    /* LDR Rd,[...] Rd=9 */
    if ((insn & 0x0C500000u) == 0x04100000u) {
        if (((insn >> 12) & 0xFu) == 9u) return 1;
    }
    /* LDM with r9 in list and writeback base */
    if ((insn & 0x0E100000u) == 0x08100000u) {
        if (insn & (1u << 9)) return 1;
    }
    /* POP is LDMIA sp! */
    return 0;
}

void ext_callback_frame_on_host_enter(void *uc, uint32_t slot_addr, const char *name) {
    CfnXfer *x;
    const GwyLoadedModule *m;
    uint32_t r[13], sp = 0, lr = 0, pc = 0, cpsr = 0;
    if (!ext_callback_frame_enabled()) return;
    if (!is_cfn_name(name)) return;
    read_regs(uc, r, &sp, &lr, &pc, &cpsr);
    m = module_for_pc(lr);
    x = alloc_xfer();
    if (!x) return;
    x->active = 1;
    x->slot_addr = slot_addr;
    snprintf(x->cfn_path, sizeof(x->cfn_path), "%s", "HOST_MAP_FUNC");
    if (name) snprintf(x->slot_name, sizeof(x->slot_name), "%s", name);
    x->continuation_pc = lr;
    x->call_pc = pc;
    x->helper = r[0];
    bind_module(x, m);
    fill_snap(&x->before, uc, x->callback_id, x->module_id, slot_addr, name, pc, lr);
    x->before_ok = 1;
    x->pre_r9 = x->before.r[9];
    emit_boundary("GUEST_BEFORE_HOST_CALLBACK", &x->before, x);
}

void ext_callback_frame_on_guest_cfn_call(void *uc, uint32_t call_pc, uint32_t target,
                                          const uint32_t regs[16], uint32_t cpsr,
                                          uint64_t caller_module_id) {
    CfnXfer *x;
    const GwyLoadedModule *m;
    uint32_t cont;
    int thumb;
    ModuleRegistry *reg;
    if (!ext_callback_frame_enabled() || !regs) return;
    (void)uc;
    (void)target;
    thumb = (cpsr >> 5) & 1;
    /* Next insn after Thumb BLX Rm (16-bit) or ARM BLX Rm (4-byte). */
    cont = thumb ? ((call_pc & ~1u) + 2u) : ((call_pc & ~3u) + 4u);
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, caller_module_id) : NULL;
    if (!m) m = module_for_pc(call_pc);
    x = alloc_xfer();
    if (!x) return;
    x->active = 1;
    snprintf(x->cfn_path, sizeof(x->cfn_path), "%s", "GUEST_NESTED");
    snprintf(x->slot_name, sizeof(x->slot_name), "%s", "_mr_c_function_new");
    x->call_pc = call_pc;
    x->continuation_pc = cont;
    x->helper = regs[0];
    x->slot_addr = target;
    bind_module(x, m);
    fill_snap_from_regs(&x->before, regs, cpsr, x->callback_id, x->module_id, target,
                        x->slot_name, call_pc, cont);
    x->before_ok = 1;
    x->pre_r9 = regs[9];
    emit_boundary("GUEST_BEFORE_HOST_CALLBACK", &x->before, x);
    /* Identity provisional: continuation is next-PC after call site BLX. */
    x->continuation_identity = 1;
    emit_continuation(x);
    /* D2: leave may already have zeroed R9 before this xfer was allocated. */
    {
        GwyR9WriteRecord zr;
        if (guest_memory_r9_first_zeroing(&zr) && x->pre_r9 != 0 && zr.new_r9 == 0) {
            x->r9_scope_interfered = 1;
            snprintf(x->first_r9_write_reason, sizeof(x->first_r9_write_reason), "%s",
                     gwy_r9_write_reason_name(zr.reason));
            snprintf(x->first_r9_write_callsite, sizeof(x->first_r9_write_callsite), "%s",
                     zr.host_callsite[0] ? zr.host_callsite : "?");
            x->first_r9_write_event_seq = zr.event_seq;
            x->first_r9_write_frame_id = zr.frame_id;
            printf("[CALLBACK_R9_SCOPE_INTERFERENCE] callback_id=%llu module=%s "
                   "note=foreign_pop_before_xfer_alloc " CF_GATE_LINE " evidence=OBSERVED\n",
                   (unsigned long long)x->callback_id, x->module_name, CF_GATE_ARGS);
            fflush(stdout);
        }
    }
}

void ext_callback_frame_on_cfunction_observe(void *uc, uint32_t helper, uint32_t p_len,
                                             const char *origin) {
    CfnXfer *x;
    const GwyLoadedModule *m;
    uint32_t r[13], sp = 0, lr = 0, pc = 0, cpsr = 0;
    if (!ext_callback_frame_enabled()) return;
    (void)p_len;
    if (!origin) origin = "UNKNOWN";
    if (strcmp(origin, "HOST_BRIDGE") == 0) return;
    if (strcmp(origin, "GUEST_NESTED") == 0) return;
    if (!uc) uc = g_cf.uc;
    m = module_for_helper(helper);
    if (!m) m = module_for_pc(helper);
    x = alloc_xfer();
    if (!x) return;
    x->active = 1;
    snprintf(x->cfn_path, sizeof(x->cfn_path), "%s",
             (strcmp(origin, "LOG_PARSE") == 0) ? "LOG_PARSE" : origin);
    snprintf(x->slot_name, sizeof(x->slot_name), "%s", "_mr_c_function_new");
    x->helper = helper;
    bind_module(x, m);
    if (uc) {
        read_regs(uc, r, &sp, &lr, &pc, &cpsr);
        if (m && lr && (lr & ~1u) >= m->map.guest_code_base &&
            (lr & ~1u) < m->map.guest_code_base + m->map.guest_code_size &&
            (lr & ~1u) != (helper & ~1u)) {
            x->continuation_pc = lr;
            x->continuation_identity = 1;
        }
        x->call_pc = pc;
        fill_snap(&x->before, uc, x->callback_id, x->module_id, 0, x->slot_name, pc,
                  x->continuation_pc);
        x->before_ok = 1;
        x->pre_r9 = x->before.r[9];
    } else {
        x->pre_r9 = 0;
        x->before_ok = 1;
    }
    emit_boundary("GUEST_BEFORE_HOST_CALLBACK", &x->before, x);
    if (x->continuation_identity) emit_continuation(x);
}

void ext_callback_frame_on_host_leave(void *uc, uint32_t slot_addr, const char *name) {
    CfnXfer *x;
    if (!ext_callback_frame_enabled()) return;
    (void)name;
    x = find_open_by_slot(slot_addr);
    if (!x) return;
    fill_snap(&x->after_host, uc, x->callback_id, x->module_id, slot_addr, x->slot_name,
              x->call_pc, x->continuation_pc);
    x->after_ok = 1;
    x->leave_r9 = x->after_host.r[9];
    x->return_r0 = (int32_t)x->after_host.r[0];
    emit_boundary("HOST_CALLBACK_RETURN", &x->after_host, x);
    emit_deltas(x);
}

void ext_callback_frame_on_host_resume(void *uc, uint32_t slot_addr, const char *name) {
    CfnXfer *x;
    uint32_t pc = 0;
    if (!ext_callback_frame_enabled()) return;
    (void)name;
    x = find_open_by_slot(slot_addr);
    if (!x) return;
    fill_snap(&x->resume, uc, x->callback_id, x->module_id, slot_addr, x->slot_name, x->call_pc,
              x->continuation_pc);
    x->resume_ok = 1;
    x->resume_r9 = x->resume.r[9];
#ifdef GWY_HAVE_UNICORN
    if (uc) uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
#else
    (void)pc;
#endif
    /* Identity: resume PC matches captured LR continuation; helper != continuation. */
    if (x->continuation_pc &&
        ((pc & ~1u) == (x->continuation_pc & ~1u) ||
         (x->resume.lr && (x->resume.lr & ~1u) == (x->continuation_pc & ~1u))) &&
        (!x->helper || (x->helper & ~1u) != (x->continuation_pc & ~1u))) {
        x->continuation_identity = 1;
    }
    /* Also accept: PC after write equals before.lr */
    if (!x->continuation_identity && x->before_ok &&
        (pc & ~1u) == (x->before.lr & ~1u) && x->before.lr != 0) {
        x->continuation_identity = 1;
        x->continuation_pc = x->before.lr;
    }
    /* Phase 6K: documented MRPGCMAP entry before nested CFN continuation resume. */
    if (strcmp(x->cfn_path, "GUEST_NESTED") == 0 || strcmp(x->cfn_path, "LOG_PARSE") == 0) {
        ext_mrpgcmap_entry_order_before_continuation(
            uc, x->module_name[0] ? x->module_name : "gbrwcore.ext", x->continuation_pc);
    }
    emit_boundary("GUEST_CONTINUATION_RESUME", &x->resume, x);
    emit_continuation(x);
    if (strcmp(x->cfn_path, "GUEST_NESTED") == 0 || strcmp(x->cfn_path, "LOG_PARSE") == 0) {
        uint32_t regs16[16];
        int ri;
        memset(regs16, 0, sizeof(regs16));
        for (ri = 0; ri < 13; ri++) regs16[ri] = x->resume.r[ri];
        regs16[13] = x->resume.sp;
        regs16[14] = x->resume.lr;
        ext_post_cont_audit_on_continuation_resume(uc, x->module_id, x->module_name, x->call_pc,
                                                   x->continuation_pc, regs16, x->resume.sp,
                                                   x->resume.lr, x->resume.cpsr);
        ext_post_cfn_r9_audit_on_continuation_resume(uc, x->module_id, x->module_name, x->call_pc,
                                                     x->continuation_pc, regs16, x->resume.sp,
                                                     x->resume.lr, x->resume.cpsr);
        ext_p_extchunk_audit_on_continuation_resume(uc, x->module_id, x->module_name, x->call_pc,
                                                    x->continuation_pc, regs16, x->resume.sp,
                                                    x->resume.lr, x->resume.cpsr);
        ext_gwy_startgame_audit_on_continuation_resume(uc, x->module_id, x->module_name,
                                                       x->continuation_pc, regs16, x->resume.cpsr);
    }
    /* Defer final class until fault only when R9 already diverged at resume.
     * After tokenized scope balancing, nested CFN often resumes with R9 intact —
     * classify that outcome immediately (observe-only). */
    if (x->pre_r9 != 0 && x->resume_r9 != x->pre_r9 && !x->guest_wrote_r9) {
        attribute_r9_loss(x);
        if (strcmp(x->first_r9_loss_writer, "HOST_CALLBACK_RESUME_BOUNDARY") == 0 ||
            strcmp(x->first_r9_loss_writer, "HOST_CALLBACK_BODY") == 0)
            classify_xfer(x);
    } else if (x->continuation_identity && x->resume_r9 == x->pre_r9 && x->pre_r9 != 0) {
        classify_xfer(x);
    }
    x->active = 0; /* transfer closed for stack nesting; still retained for queries */
}

void ext_callback_frame_on_r9_switch(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                                     GwyModuleCallKind call_kind, int enter_rc, uint32_t pc,
                                     const uint32_t regs[16]) {
    uint64_t mid = 0;
    uint32_t cont = 0;
    int i;
    if (!ext_callback_frame_enabled()) return;
    (void)uc;
    (void)caller_module_id;
    (void)callee_module_id;
    (void)enter_rc;
    (void)regs;
    /* Continuation-PC interference (legacy D1). */
    if (ext_callback_frame_is_cfn_continuation(pc, &mid, &cont)) {
        for (i = 0; i < g_cf.xfer_count; i++) {
            if ((g_cf.xfer[i].continuation_pc & ~1u) == (cont & ~1u)) {
                g_cf.xfer[i].r9_switch_interfered = 1;
                printf("[CALLBACK_R9_SWITCH_INTERFERENCE] pc=0x%X continuation_pc=0x%X "
                       "call_kind=%s module_id=%llu note=continuation_not_module_entry "
                       CF_GATE_LINE " evidence=OBSERVED\n",
                       pc, cont, gwy_module_call_kind_name(call_kind),
                       (unsigned long long)mid, CF_GATE_ARGS);
                fflush(stdout);
                break;
            }
        }
    }
    /* Scope-level: any active nested CFN transfer marks interference on enter/leave nearby. */
    for (i = 0; i < g_cf.xfer_count; i++) {
        CfnXfer *x = &g_cf.xfer[i];
        if (!x->before_ok || x->classified) continue;
        if (strcmp(x->cfn_path, "GUEST_NESTED") != 0 && strcmp(x->cfn_path, "LOG_PARSE") != 0)
            continue;
        if (x->pre_r9 == 0) continue;
        /* Mark scope interference when switch happens after CFN call before resume. */
        if (!x->resume_ok) {
            x->r9_scope_interfered = 1;
            printf("[CALLBACK_R9_SCOPE_INTERFERENCE] callback_id=%llu module=%s "
                   "call_kind=%s enter_rc=%d pc=0x%X note=scope_level_during_nested_cfn "
                   CF_GATE_LINE " evidence=OBSERVED\n",
                   (unsigned long long)x->callback_id, x->module_name,
                   gwy_module_call_kind_name(call_kind), enter_rc, pc, CF_GATE_ARGS);
            fflush(stdout);
        }
    }
}

void ext_callback_frame_on_r9_scope_leave(const char *requested_by, GwyR9LeaveAction action,
                                          uint64_t top_frame_id, GwyModuleCallKind top_kind) {
    int i;
    if (!ext_callback_frame_enabled()) return;
    if (action != GWY_R9_LEAVE_POP_FOREIGN_FRAME) return;
    for (i = 0; i < g_cf.xfer_count; i++) {
        CfnXfer *x = &g_cf.xfer[i];
        if (!x->before_ok || x->classified) continue;
        if (strcmp(x->cfn_path, "GUEST_NESTED") != 0 && strcmp(x->cfn_path, "LOG_PARSE") != 0)
            continue;
        if (x->resume_ok) continue;
        x->r9_scope_interfered = 1;
        printf("[CALLBACK_R9_SCOPE_INTERFERENCE] callback_id=%llu module=%s "
               "requested_by=%s leave_action=%s top_frame_id=%llu top_frame_kind=%s "
               "note=scope_level_foreign_pop " CF_GATE_LINE " evidence=OBSERVED\n",
               (unsigned long long)x->callback_id, x->module_name,
               requested_by ? requested_by : "?", gwy_r9_leave_action_name(action),
               (unsigned long long)top_frame_id, gwy_module_call_kind_name(top_kind), CF_GATE_ARGS);
        fflush(stdout);
    }
}

void ext_callback_frame_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                uint32_t cpsr) {
    CfnXfer *x;
    int i;
    int thumb;
    uint32_t pn;
    if (!ext_callback_frame_enabled()) return;
    (void)module_id;
    if (!regs) return;
    pn = pc & ~1u;

    /* Guest/LOG_PARSE: first hit of continuation PC → synthetic RESUME snapshot. */
    for (i = 0; i < g_cf.xfer_count; i++) {
        x = &g_cf.xfer[i];
        if (!x->before_ok || x->resume_ok || !x->continuation_pc) continue;
        if ((x->continuation_pc & ~1u) != pn) continue;
        if (strcmp(x->cfn_path, "HOST_MAP_FUNC") == 0) continue;
        fill_snap_from_regs(&x->resume, regs, cpsr, x->callback_id, x->module_id, x->slot_addr,
                            x->slot_name, x->call_pc, x->continuation_pc);
        x->resume_ok = 1;
        x->resume_r9 = regs[9];
        x->continuation_identity = 1;
        x->active = 0;
        emit_boundary("GUEST_CONTINUATION_RESUME", &x->resume, x);
        emit_deltas(x);
        emit_continuation(x);
        if (strcmp(x->cfn_path, "GUEST_NESTED") == 0 || strcmp(x->cfn_path, "LOG_PARSE") == 0) {
            ext_post_cont_audit_on_continuation_resume(uc, x->module_id, x->module_name, x->call_pc,
                                                       x->continuation_pc, regs, x->resume.sp,
                                                       x->resume.lr, cpsr);
            ext_post_cfn_r9_audit_on_continuation_resume(uc, x->module_id, x->module_name, x->call_pc,
                                                         x->continuation_pc, regs, x->resume.sp,
                                                         x->resume.lr, cpsr);
            ext_p_extchunk_audit_on_continuation_resume(uc, x->module_id, x->module_name, x->call_pc,
                                                        x->continuation_pc, regs, x->resume.sp,
                                                        x->resume.lr, cpsr);
            ext_gwy_startgame_audit_on_continuation_resume(uc, x->module_id, x->module_name,
                                                           x->continuation_pc, regs, cpsr);
        }
        if (x->pre_r9 != 0 && x->resume_r9 == x->pre_r9) {
            classify_xfer(x);
        } else if (x->pre_r9 != 0 && x->resume_r9 != x->pre_r9) {
            attribute_r9_loss(x);
            classify_xfer(x);
        }
    }

    /* Watch recent CFN xfers for guest r9 writes after resume. */
    for (i = g_cf.xfer_count - 1; i >= 0; i--) {
        x = &g_cf.xfer[i];
        if (!x->resume_ok || x->guest_wrote_r9 || x->classified) continue;
        if (x->module_id && module_id && x->module_id != module_id) continue;
        thumb = (cpsr >> 5) & 1;
        if (thumb) {
            uint32_t w = 0;
            if (uc && guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~1u, &w)) {
                if (thumb_writes_r9(w & 0xFFFFu, w >> 16)) {
                    x->guest_wrote_r9 = 1;
                    x->first_guest_r9_write_pc = pc;
                }
            }
        } else {
            uint32_t w = 0;
            if (uc && guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &w)) {
                if (arm_writes_r9(w)) {
                    x->guest_wrote_r9 = 1;
                    x->first_guest_r9_write_pc = pc;
                }
            }
        }
        if (!x->guest_wrote_r9 && x->resume_ok && regs[9] != x->resume_r9 &&
            x->resume_r9 == x->pre_r9 && x->pre_r9 != 0) {
            x->guest_wrote_r9 = 1;
            x->first_guest_r9_write_pc = pc;
        }
    }
}

void ext_callback_frame_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                     uint32_t access_size, const uint32_t regs[16], uint32_t cpsr) {
    int i;
    CfnXfer *best = NULL;
    if (!ext_callback_frame_enabled()) return;
    (void)uc;
    (void)access_size;
    (void)cpsr;

    for (i = 0; i < g_cf.xfer_count; i++) {
        CfnXfer *x = &g_cf.xfer[i];
        uint32_t lr = regs ? regs[14] : 0;
        if (x->classified) continue;

        /* Nested CFN: fault in callee with LR == continuation → identity + R9 loss in callee. */
        if (!x->resume_ok && x->before_ok && x->continuation_pc && lr &&
            (lr & ~1u) == (x->continuation_pc & ~1u)) {
            x->continuation_identity = 1;
            x->function_entry_pc = x->slot_addr ? (x->slot_addr & ~1u) : (fault_pc & ~1u);
            x->memory_access_pc = fault_pc;
            if (x->function_entry_pc && (fault_pc & ~1u) == x->function_entry_pc)
                x->memory_access_pc = x->function_entry_pc + 0x10u;
            if (regs) {
                x->resume_r9 = regs[9]; /* last seen in callee; not true resume */
                if (x->pre_r9 != 0 && regs[9] != x->pre_r9) {
                    GwyR9WriteRecord zr;
                    if (guest_memory_r9_first_zeroing(&zr)) {
                        snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s",
                                 gwy_r9_write_reason_name(zr.reason));
                        snprintf(x->first_r9_write_reason, sizeof(x->first_r9_write_reason), "%s",
                                 gwy_r9_write_reason_name(zr.reason));
                        snprintf(x->first_r9_write_callsite, sizeof(x->first_r9_write_callsite),
                                 "%s", zr.host_callsite[0] ? zr.host_callsite : "?");
                        x->first_r9_write_event_seq = zr.event_seq;
                        x->first_r9_write_frame_id = zr.frame_id;
                        x->r9_scope_interfered = 1;
                    } else {
                        snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s",
                                 "NESTED_CFN_CALLEE_PATH");
                    }
                }
            }
            printf("[CALLBACK_FRAME] boundary=FAULT_IN_CFN_CALLEE callback_id=%llu module=%s "
                   "fault_pc=0x%X function_entry_pc=0x%X memory_access_pc=0x%X "
                   "fault_addr=0x%X lr_continuation=0x%X r9=0x%X pre_r9=0x%X cfn_path=%s "
                   CF_GATE_LINE " evidence=OBSERVED\n",
                   (unsigned long long)x->callback_id, x->module_name, fault_pc,
                   x->function_entry_pc, x->memory_access_pc, fault_addr, lr,
                   regs ? regs[9] : 0, x->pre_r9, x->cfn_path[0] ? x->cfn_path : "?",
                   CF_GATE_ARGS);
            fflush(stdout);
            emit_continuation(x);
        }

        if (x->resume_ok && regs && x->resume_r9 == x->pre_r9 && x->pre_r9 != 0 &&
            regs[9] != x->pre_r9 && !x->guest_wrote_r9) {
            x->guest_wrote_r9 = 1;
            if (!x->first_guest_r9_write_pc) x->first_guest_r9_write_pc = fault_pc;
            snprintf(x->first_r9_loss_writer, sizeof(x->first_r9_loss_writer), "%s",
                     "POST_RESUME_BEFORE_FAULT");
        }

        /* Prefer MRP member samples with non-zero pre_r9 for final class. */
        if (x->before_ok && x->pre_r9 != 0 &&
            (strstr(x->module_name, "mrc_loader") || strstr(x->module_name, "robotol") ||
             strstr(x->module_name, "cfunction")))
            best = x;
        else if (!best && x->before_ok)
            best = x;
    }

    if (best && !best->classified) classify_xfer(best);

    for (i = 0; i < g_cf.xfer_count; i++) {
        if (!g_cf.xfer[i].classified && g_cf.xfer[i].before_ok) classify_xfer(&g_cf.xfer[i]);
    }

    if (!g_cf.class_logged) {
        g_cf.last_class = CF_CLASS_NOT_CFN_OR_INCONCLUSIVE;
        printf("[CALLBACK_FRAME_CLASS] class=%s continuation_identity=UNCONFIRMED "
               "next_allowed_fix=UNKNOWN " CF_GATE_LINE
               " evidence=OBSERVED note=no_cfn_xfer\n",
               ext_callback_frame_class_name(g_cf.last_class),
               CF_GATE_ARGS);
        fflush(stdout);
        g_cf.class_logged = 1;
    }
}
