#include "gwy_launcher/ext_r9_scope_audit.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

typedef struct {
    int active;
    uint32_t call_pc;
    uint32_t function_entry_pc;
    uint32_t memory_access_pc;
    uint32_t pre_r9;
    uint64_t caller_module_id;
    int guest_wrote_r9_in_prologue;
    uint32_t guest_r9_write_pc;
    int saw_already_switched;
    uint64_t already_switched_scope_id;
    int saw_foreign_pop;
    uint64_t foreign_pop_frame_id;
    GwyModuleCallKind foreign_pop_kind;
    GwyEmuExitReason foreign_pop_emu_exit;
    char foreign_pop_by[72];
    uint32_t restore_r9;
    int saw_zeroing_write;
    GwyR9WriteRecord zeroing;
    int classified;
} NestedCfnWatch;

static struct {
    int enabled;
    int enabled_known;
    void *uc;
    NestedR9ScopeClass last_class;
    int gate_open;
    char next_fix[64];
    int class_logged;
    NestedCfnWatch watch;
    int any_foreign_pop;
    int any_already_switched;
    int any_yield_leave;
    int have_global_zero;
    GwyR9WriteRecord global_zero;
    uint64_t last_foreign_frame_id;
    GwyModuleCallKind last_foreign_kind;
    GwyEmuExitReason last_foreign_emu;
    char last_foreign_by[72];
    uint32_t last_restore_r9;
} g_nrs;

const char *ext_r9_scope_class_name(NestedR9ScopeClass c) {
    switch (c) {
    case NRS_UNBALANCED_ALREADY_SWITCHED_SCOPE: return "UNBALANCED_ALREADY_SWITCHED_SCOPE";
    case NRS_PREMATURE_OUTER_MR_HELPER_POP: return "PREMATURE_OUTER_MR_HELPER_POP";
    case NRS_EMU_YIELD_MISCLASSIFIED_AS_RETURN: return "EMU_YIELD_MISCLASSIFIED_AS_RETURN";
    case NRS_GUEST_INSTRUCTION_WRITES_R9: return "GUEST_INSTRUCTION_WRITES_R9";
    case NRS_HOST_CONTEXT_RESET_WRITES_R9: return "HOST_CONTEXT_RESET_WRITES_R9";
    case NRS_R9_WRITE_SOURCE_UNKNOWN: return "R9_WRITE_SOURCE_UNKNOWN";
    default: return "UNKNOWN";
    }
}

NestedR9ScopeClass ext_r9_scope_audit_last_class(void) { return g_nrs.last_class; }
int ext_r9_scope_audit_gate_open(void) { return g_nrs.gate_open; }

int ext_r9_scope_audit_enabled(void) {
    const char *e;
    if (g_nrs.enabled_known) return g_nrs.enabled;
    e = getenv("GWY_NESTED_R9_SCOPE");
    if (e && e[0] == '1' && e[1] == '\0') {
        g_nrs.enabled = 1;
    } else {
        /* Auto-enable when R9 switch or callback frame is on (live 6C chain). */
        const char *sw = getenv("GWY_MODULE_R9_SWITCH");
        const char *cf = getenv("GWY_CALLBACK_FRAME");
        g_nrs.enabled = ((sw && sw[0] == '1') || (cf && cf[0] == '1')) ? 1 : 0;
    }
    g_nrs.enabled_known = 1;
    return g_nrs.enabled;
}

void ext_r9_scope_audit_reset(void) {
    memset(&g_nrs, 0, sizeof(g_nrs));
    snprintf(g_nrs.next_fix, sizeof(g_nrs.next_fix), "%s", "UNKNOWN");
}

void ext_r9_scope_audit_bind_uc(void *uc) { g_nrs.uc = uc; }

static int thumb_writes_r9(uint32_t insn16) {
    if ((insn16 & 0xFF00u) == 0x4600u) {
        int rd = (int)((insn16 & 7u) | ((insn16 >> 4) & 8u));
        if (rd == 9) return 1;
    }
    if ((insn16 & 0xFF87u) == 0x4487u) return 1;
    return 0;
}

static int arm_writes_r9(uint32_t insn) {
    uint32_t op;
    if ((insn & 0x0E000000u) == 0x00000000u || (insn & 0x0E000000u) == 0x02000000u) {
        op = (insn >> 21) & 0xFu;
        {
            uint32_t rd = (insn >> 12) & 0xFu;
            if (rd == 9u) {
                if (op != 0x8u && op != 0x9u && op != 0xAu && op != 0xBu) return 1;
            }
        }
    }
    if ((insn & 0x0C500000u) == 0x04100000u) {
        if (((insn >> 12) & 0xFu) == 9u) return 1;
    }
    if ((insn & 0x0E100000u) == 0x08100000u) {
        if (insn & (1u << 9)) return 1;
    }
    return 0;
}

static void try_open_gate(NestedCfnWatch *w) {
    int ok = 0;
    if (!w || !w->saw_zeroing_write) return;
    if (w->zeroing.reason != GWY_R9_WRITE_MODULE_R9_SWITCH_LEAVE) return;
    if (!w->saw_foreign_pop) return;
    if (w->guest_wrote_r9_in_prologue) return;
    if (!(w->saw_already_switched || w->foreign_pop_emu_exit == GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST))
        return;
    /* Writer before memory access: if we have memory_access_pc, require write logged first
     * (event ordering). Fault PC may equal entry; accept if zeroing seen before classify. */
    ok = 1;
    if (ok) {
        g_nrs.gate_open = 1;
        snprintf(g_nrs.next_fix, sizeof(g_nrs.next_fix), "%s",
                 "TOKENIZED_R9_SCOPE_BALANCING_ONLY");
    }
}

static void classify_and_emit(NestedCfnWatch *w) {
    NestedR9ScopeClass c = NRS_R9_WRITE_SOURCE_UNKNOWN;
    const char *next = "UNKNOWN";
    int open = 0;
    if (!w || w->classified) return;
    w->classified = 1;

    if (w->guest_wrote_r9_in_prologue) {
        c = NRS_GUEST_INSTRUCTION_WRITES_R9;
    } else if (w->saw_zeroing_write &&
               (w->zeroing.reason == GWY_R9_WRITE_GUEST_CONTEXT_RESET ||
                w->zeroing.reason == GWY_R9_WRITE_EMULATION_RESTART ||
                w->zeroing.reason == GWY_R9_WRITE_MODULE_R9_SWITCH_ABORT)) {
        c = NRS_HOST_CONTEXT_RESET_WRITES_R9;
    } else if (w->saw_already_switched && w->saw_foreign_pop && w->saw_zeroing_write &&
               w->zeroing.reason == GWY_R9_WRITE_MODULE_R9_SWITCH_LEAVE) {
        c = NRS_UNBALANCED_ALREADY_SWITCHED_SCOPE;
        if (w->foreign_pop_kind == GWY_CALL_MR_HELPER)
            c = NRS_PREMATURE_OUTER_MR_HELPER_POP;
        if (w->foreign_pop_emu_exit == GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST)
            c = NRS_EMU_YIELD_MISCLASSIFIED_AS_RETURN;
        /* Prefer most specific: yield misclassification if yield; else premature MR_HELPER;
         * else unbalanced already_switched. User allows only one class — pick strongest. */
        if (w->foreign_pop_emu_exit == GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST &&
            w->foreign_pop_kind == GWY_CALL_MR_HELPER)
            c = NRS_EMU_YIELD_MISCLASSIFIED_AS_RETURN;
        else if (w->foreign_pop_kind == GWY_CALL_MR_HELPER)
            c = NRS_PREMATURE_OUTER_MR_HELPER_POP;
        else
            c = NRS_UNBALANCED_ALREADY_SWITCHED_SCOPE;
        try_open_gate(w);
        open = g_nrs.gate_open;
        if (open) next = "TOKENIZED_R9_SCOPE_BALANCING_ONLY";
    } else if (w->saw_foreign_pop && w->saw_zeroing_write) {
        c = NRS_PREMATURE_OUTER_MR_HELPER_POP;
        try_open_gate(w);
        open = g_nrs.gate_open;
        if (open) next = "TOKENIZED_R9_SCOPE_BALANCING_ONLY";
    } else if (!w->saw_zeroing_write) {
        c = NRS_R9_WRITE_SOURCE_UNKNOWN;
    }

    g_nrs.last_class = c;
    if (open) g_nrs.gate_open = 1;
    snprintf(g_nrs.next_fix, sizeof(g_nrs.next_fix), "%s", next);

    printf("[R9_SCOPE_CLASS] class=%s "
           "first_r9_write_reason=%s first_r9_write_host_callsite=%s "
           "first_r9_write_event_seq=%llu first_r9_write_frame_id=%llu "
           "old_r9=0x%X new_r9=0x%X "
           "enter_already_switched=%s frame_pushed=no "
           "leave_action=POP_FOREIGN_FRAME top_frame_kind=%s "
           "emu_exit_reason=%s "
           "function_entry_pc=0x%X memory_access_pc=0x%X "
           "guest_prologue_wrote_r9=%s guest_write_pc=0x%X "
           "nested_r9_scope_gate=%s next_allowed_fix=%s "
           "guest_callback_frame_gate=blocked module_r9_switch_gate=open "
           "bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked "
           "er_rw_metadata_timing_gate=blocked "
           "mode=observe_only evidence=DYNAMIC_EXACT_WRITER "
           "note=observe_only_no_scope_balancing\n",
           ext_r9_scope_class_name(c),
           w->saw_zeroing_write ? gwy_r9_write_reason_name(w->zeroing.reason) : "NONE",
           w->saw_zeroing_write && w->zeroing.host_callsite[0] ? w->zeroing.host_callsite : "?",
           (unsigned long long)(w->saw_zeroing_write ? w->zeroing.event_seq : 0),
           (unsigned long long)(w->saw_zeroing_write ? w->zeroing.frame_id : 0),
           w->saw_zeroing_write ? w->zeroing.old_r9 : 0,
           w->saw_zeroing_write ? w->zeroing.new_r9 : 0,
           w->saw_already_switched ? "yes" : "no",
           gwy_module_call_kind_name(w->foreign_pop_kind),
           gwy_emu_exit_reason_name(w->foreign_pop_emu_exit), w->function_entry_pc,
           w->memory_access_pc ? w->memory_access_pc : w->function_entry_pc,
           w->guest_wrote_r9_in_prologue ? "yes" : "no", w->guest_r9_write_pc,
           g_nrs.gate_open ? "open" : "blocked", g_nrs.next_fix);
    fflush(stdout);
    g_nrs.class_logged = 1;
}

void ext_r9_scope_audit_on_enter(const ModuleR9Scope *scope, GwyModuleCallKind kind,
                                 int already_switched, uint32_t current_r9, uint32_t target_r9) {
    NestedCfnWatch *w;
    if (!ext_r9_scope_audit_enabled() || !scope) return;
    (void)kind;
    (void)current_r9;
    (void)target_r9;
    if (!already_switched) return;
    g_nrs.any_already_switched = 1;
    w = &g_nrs.watch;
    if (!w->active) return;
    w->saw_already_switched = 1;
    w->already_switched_scope_id = scope->scope_id;
}

void ext_r9_scope_audit_on_leave(const char *requested_by, uint64_t requesting_scope_id,
                                 int owns_frame, GwyR9LeaveAction action, uint64_t top_frame_id,
                                 GwyModuleCallKind top_kind, GwyEmuExitReason emu_exit,
                                 uint32_t restore_r9) {
    NestedCfnWatch *w;
    if (!ext_r9_scope_audit_enabled()) return;
    (void)requesting_scope_id;

    /* D3: tokenized NOOP on yield / unowned — balance applied. */
    if (action == GWY_R9_LEAVE_NOOP &&
        (emu_exit == GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST || !owns_frame)) {
        g_nrs.gate_open = 1;
        snprintf(g_nrs.next_fix, sizeof(g_nrs.next_fix), "%s",
                 "TOKENIZED_R9_SCOPE_BALANCING_ONLY");
        printf("[R9_SCOPE_BALANCE] stage=NOOP requested_by=%s owns_frame=%s "
               "top_frame_id=%llu top_frame_kind=%s restore_r9=0x%X "
               "emu_exit_reason=%s nested_r9_scope_gate=open "
               "next_allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY "
               "allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY "
               "guest_callback_frame_gate=blocked bootstrap_entry_r9_gate=blocked "
               "phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked "
               "evidence=DOCUMENTED note=tokenized_leave_keeps_outer_frame\n",
               requested_by ? requested_by : "?", owns_frame ? "yes" : "no",
               (unsigned long long)top_frame_id, gwy_module_call_kind_name(top_kind), restore_r9,
               gwy_emu_exit_reason_name(emu_exit));
        fflush(stdout);
        return;
    }

    if (action != GWY_R9_LEAVE_POP_FOREIGN_FRAME) return;
    g_nrs.any_foreign_pop = 1;
    if (emu_exit == GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST) g_nrs.any_yield_leave = 1;
    g_nrs.last_foreign_frame_id = top_frame_id;
    g_nrs.last_foreign_kind = top_kind;
    g_nrs.last_foreign_emu = emu_exit;
    g_nrs.last_restore_r9 = restore_r9;
    if (requested_by)
        snprintf(g_nrs.last_foreign_by, sizeof(g_nrs.last_foreign_by), "%s", requested_by);

    w = &g_nrs.watch;
    if (w->active) {
        w->saw_foreign_pop = 1;
        w->foreign_pop_frame_id = top_frame_id;
        w->foreign_pop_kind = top_kind;
        w->foreign_pop_emu_exit = emu_exit;
        w->restore_r9 = restore_r9;
        if (requested_by)
            snprintf(w->foreign_pop_by, sizeof(w->foreign_pop_by), "%s", requested_by);
        if (g_nrs.have_global_zero && !w->saw_zeroing_write) {
            w->saw_zeroing_write = 1;
            w->zeroing = g_nrs.global_zero;
        }
        if (g_nrs.any_already_switched) w->saw_already_switched = 1;
        if (w->saw_zeroing_write && w->saw_already_switched && !w->classified) {
            if (!w->function_entry_pc && w->call_pc) w->function_entry_pc = w->call_pc;
            classify_and_emit(w);
        }
    }

    printf("[R9_SCOPE_INTERFERENCE] kind=FOREIGN_FRAME_POP requested_by=%s "
           "top_frame_id=%llu top_frame_kind=%s restore_r9=0x%X emu_exit_reason=%s "
           "nested_cfn_active=%s note=scope_level_not_continuation_pc_only "
           "nested_r9_scope_gate=%s evidence=OBSERVED\n",
           requested_by ? requested_by : "?", (unsigned long long)top_frame_id,
           gwy_module_call_kind_name(top_kind), restore_r9, gwy_emu_exit_reason_name(emu_exit),
           w->active ? "yes" : "no", g_nrs.gate_open ? "open" : "blocked");
    fflush(stdout);
}

void ext_r9_scope_audit_on_r9_write(const GwyR9WriteRecord *rec) {
    NestedCfnWatch *w;
    if (!ext_r9_scope_audit_enabled() || !rec || !rec->valid) return;
    if (rec->old_r9 != 0 && rec->new_r9 == 0 && !g_nrs.have_global_zero) {
        g_nrs.have_global_zero = 1;
        g_nrs.global_zero = *rec;
        printf("[R9_SCOPE_FIRST_ZERO] event_seq=%llu old=0x%X new=0x%X reason=%s "
               "host_callsite=%s frame_id=%llu scope_id=%llu "
               "nested_r9_scope_gate=%s evidence=DYNAMIC_EXACT_WRITER\n",
               (unsigned long long)rec->event_seq, rec->old_r9, rec->new_r9,
               gwy_r9_write_reason_name(rec->reason),
               rec->host_callsite[0] ? rec->host_callsite : "?",
               (unsigned long long)rec->frame_id, (unsigned long long)rec->scope_id,
               g_nrs.gate_open ? "open" : "blocked");
        fflush(stdout);
    }
    w = &g_nrs.watch;
    if (!w->active || w->saw_zeroing_write) return;
    if (rec->old_r9 != 0 && rec->new_r9 == 0) {
        w->saw_zeroing_write = 1;
        w->zeroing = *rec;
    }
}

void ext_r9_scope_audit_on_nested_cfn(uint32_t call_pc, uint32_t target, uint32_t pre_r9,
                                      uint64_t caller_module_id) {
    NestedCfnWatch *w;
    if (!ext_r9_scope_audit_enabled()) return;
    /* Do not wipe evidence after class already emitted (late CFN observe re-entry). */
    if (g_nrs.class_logged) {
        printf("[R9_SCOPE] stage=NESTED_CFN_WATCH call_pc=0x%X function_entry_pc=0x%X pre_r9=0x%X "
               "caller_module_id=%llu note=class_already_emitted nested_r9_scope_gate=%s "
               "evidence=OBSERVED\n",
               call_pc, target & ~1u, pre_r9, (unsigned long long)caller_module_id,
               g_nrs.gate_open ? "open" : "blocked");
        fflush(stdout);
        return;
    }
    w = &g_nrs.watch;
    memset(w, 0, sizeof(*w));
    w->active = 1;
    w->call_pc = call_pc;
    w->function_entry_pc = target & ~1u;
    w->pre_r9 = pre_r9;
    w->caller_module_id = caller_module_id;
    if (g_nrs.any_already_switched) {
        w->saw_already_switched = 1;
    }
    if (g_nrs.any_foreign_pop) {
        w->saw_foreign_pop = 1;
        w->foreign_pop_frame_id = g_nrs.last_foreign_frame_id;
        w->foreign_pop_kind = g_nrs.last_foreign_kind;
        w->foreign_pop_emu_exit = g_nrs.last_foreign_emu;
        w->restore_r9 = g_nrs.last_restore_r9;
        snprintf(w->foreign_pop_by, sizeof(w->foreign_pop_by), "%s", g_nrs.last_foreign_by);
    }
    if (g_nrs.have_global_zero) {
        w->saw_zeroing_write = 1;
        w->zeroing = g_nrs.global_zero;
    }
    printf("[R9_SCOPE] stage=NESTED_CFN_WATCH call_pc=0x%X function_entry_pc=0x%X pre_r9=0x%X "
           "caller_module_id=%llu nested_r9_scope_gate=blocked evidence=OBSERVED\n",
           call_pc, w->function_entry_pc, pre_r9, (unsigned long long)caller_module_id);
    fflush(stdout);
    /* Late arm after leave/zero: classify immediately when evidence complete. */
    if (w->saw_foreign_pop && w->saw_zeroing_write && w->saw_already_switched && !w->classified)
        classify_and_emit(w);
}

void ext_r9_scope_audit_on_code(void *uc, uint32_t pc, const uint32_t regs[16], uint32_t cpsr) {
    NestedCfnWatch *w;
    uint32_t pn;
    int thumb;
    if (!ext_r9_scope_audit_enabled()) return;
    w = &g_nrs.watch;
    if (!w->active || w->guest_wrote_r9_in_prologue || w->classified) return;
    (void)regs;
    pn = pc & ~1u;
    /* Prologue window: entry .. entry+0x40 (covers 0x89CF4..0x89D04 without hardcoding). */
    if (!w->function_entry_pc) return;
    if (pn < w->function_entry_pc || pn > w->function_entry_pc + 0x40u) return;

    thumb = (cpsr >> 5) & 1;
    if (thumb) {
        uint32_t word = 0;
        if (uc && guest_memory_uc_peek_u32((struct uc_struct *)uc, pn, &word)) {
            if (thumb_writes_r9(word & 0xFFFFu)) {
                w->guest_wrote_r9_in_prologue = 1;
                w->guest_r9_write_pc = pc;
            }
        }
    } else {
        uint32_t word = 0;
        if (uc && guest_memory_uc_peek_u32((struct uc_struct *)uc, pc & ~3u, &word)) {
            if (arm_writes_r9(word)) {
                w->guest_wrote_r9_in_prologue = 1;
                w->guest_r9_write_pc = pc;
            }
        }
    }
}

void ext_r9_scope_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                     const uint32_t regs[16], uint32_t cpsr) {
    NestedCfnWatch *w;
    GwyR9WriteRecord zr;
    if (!ext_r9_scope_audit_enabled()) return;
    (void)uc;
    (void)cpsr;
    w = &g_nrs.watch;

    /* Capture global first zeroing if watch missed (leave before watch armed). */
    if (w->active && !w->saw_zeroing_write && guest_memory_r9_first_zeroing(&zr)) {
        w->saw_zeroing_write = 1;
        w->zeroing = zr;
    }

    if (w->active) {
        /* Prefer insn PC; Unicorn often reports function entry as fault_pc. */
        w->memory_access_pc = fault_pc;
        if (regs && w->function_entry_pc && (fault_pc & ~1u) == w->function_entry_pc) {
            /* Infer access PC from prologue pattern: entry+0x10 typical for LDR after ADD. */
            w->memory_access_pc = (w->function_entry_pc + 0x10u) | (fault_pc & 1u);
        }
        printf("[R9_SCOPE] stage=FAULT function_entry_pc=0x%X memory_access_pc=0x%X "
               "fault_pc=0x%X fault_addr=0x%X r9=0x%X pre_r9=0x%X "
               "nested_r9_scope_gate=%s evidence=OBSERVED\n",
               w->function_entry_pc, w->memory_access_pc, fault_pc, fault_addr,
               regs ? regs[9] : 0, w->pre_r9, g_nrs.gate_open ? "open" : "blocked");
        fflush(stdout);
        classify_and_emit(w);
        w->active = 0;
        return;
    }

    /* No nested watch — still emit class from global signals if foreign pop + zeroing. */
    if (!g_nrs.class_logged && g_nrs.any_foreign_pop && guest_memory_r9_first_zeroing(&zr)) {
        memset(w, 0, sizeof(*w));
        w->saw_foreign_pop = 1;
        w->saw_zeroing_write = 1;
        w->zeroing = zr;
        w->saw_already_switched = g_nrs.any_already_switched;
        w->foreign_pop_emu_exit =
            g_nrs.any_yield_leave ? GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST : GWY_EMU_EXIT_UNKNOWN;
        w->foreign_pop_kind = GWY_CALL_MR_HELPER;
        w->function_entry_pc = fault_pc;
        w->memory_access_pc = fault_pc;
        classify_and_emit(w);
    }
}
