#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/ext_r9_scope_audit.h"
#include "gwy_launcher/ext_callback_frame.h"
#include "gwy_launcher/ext_post_cont_audit.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

typedef struct {
    ModuleR9Frame frames[GWY_MAX_MODULE_CALL_DEPTH];
    uint32_t depth;
    int enter_ok_count;
    int blocked_count;
    int leave_count;
    uint64_t next_frame_id;
    uint64_t next_scope_id;
    uint64_t generation;
    ModuleR9Scope last_enter_scope;
    uint32_t guest_pc;
    char guest_module[72];
    GwyEmuExitReason emu_exit_hint;
} ModuleR9SwitchStack;

/* Side stack for MRP→DSM return sites — does not consume R9 frame depth. */
#define GWY_MAX_DSM_RETURN_DEPTH 32
typedef struct {
    uint64_t caller_module_id;
    uint64_t r9_frame_id;
    uint32_t call_pc;
    uint32_t return_pc;
    int executed;
    int owns_r9_pop; /* 1 = leave R9 frame on this return (outer enter) */
} DsmReturnSite;

typedef struct {
    DsmReturnSite sites[GWY_MAX_DSM_RETURN_DEPTH];
    uint32_t depth;
} DsmReturnStack;

static ModuleR9SwitchStack g_r9s;
static DsmReturnStack g_dsm_ret;
#ifdef GWY_HAVE_UNICORN
static uc_hook g_dsm_r9_hook;
static int g_dsm_r9_hook_installed;
static uint64_t g_dsm_r9_hook_module_id;

static void dsm_r9_guard_on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data);
#endif

void module_r9_switch_note_dsm_code(uint64_t module_id);
int module_r9_switch_ensure_dsm_r9(void *uc, uint32_t guest_pc);
void module_r9_switch_attach_dsm_r9_guard(void *uc, uint64_t dsm_module_id, uint32_t code_base,
                                          uint32_t code_size);

#ifdef GWY_HAVE_UNICORN
static void dsm_r9_guard_on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    (void)size;
    (void)user_data;
    if (g_dsm_r9_hook_module_id) module_r9_switch_note_dsm_code(g_dsm_r9_hook_module_id);
    (void)module_r9_switch_ensure_dsm_r9(uc, (uint32_t)address);
}
#endif

void module_r9_switch_reset(void) {
    memset(&g_r9s, 0, sizeof(g_r9s));
    memset(&g_dsm_ret, 0, sizeof(g_dsm_ret));
#ifdef GWY_HAVE_UNICORN
    g_dsm_r9_hook_installed = 0;
    g_dsm_r9_hook = 0;
    g_dsm_r9_hook_module_id = 0;
#endif
}

int module_r9_switch_enabled(void) {
    const char *env = getenv("GWY_MODULE_R9_SWITCH");
    return env && env[0] == '1';
}

uint32_t module_r9_switch_depth(void) { return g_r9s.depth; }

uint64_t module_r9_switch_event_seq(void) { return guest_memory_r9_write_event_seq(); }

void module_r9_switch_set_guest_context(uint32_t guest_pc, const char *guest_module) {
    g_r9s.guest_pc = guest_pc;
    if (guest_module)
        snprintf(g_r9s.guest_module, sizeof(g_r9s.guest_module), "%s", guest_module);
    else
        g_r9s.guest_module[0] = 0;
}

void module_r9_switch_set_emu_exit_hint(GwyEmuExitReason reason) { g_r9s.emu_exit_hint = reason; }

void module_r9_switch_last_enter_scope(ModuleR9Scope *out) {
    if (!out) return;
    *out = g_r9s.last_enter_scope;
}

int module_r9_switch_peek_top(ModuleR9Frame *out) {
    if (!out || g_r9s.depth == 0) return 0;
    *out = g_r9s.frames[g_r9s.depth - 1u];
    return 1;
}

int module_r9_switch_peek_at(uint32_t depth_index, ModuleR9Frame *out) {
    if (!out || depth_index >= g_r9s.depth) return 0;
    *out = g_r9s.frames[depth_index];
    return 1;
}

const char *gwy_module_call_kind_name(GwyModuleCallKind k) {
    switch (k) {
        case GWY_CALL_BOOTSTRAP_ENTRY: return "BOOTSTRAP_ENTRY";
        case GWY_CALL_RUNTIME_ENTRY: return "RUNTIME_ENTRY";
        case GWY_CALL_MR_HELPER: return "MR_HELPER";
        case GWY_CALL_CALLBACK: return "CALLBACK";
        case GWY_CALL_NESTED_EXT: return "NESTED_EXT";
        default: return "UNKNOWN";
    }
}

const char *gwy_emu_exit_reason_name(GwyEmuExitReason r) {
    switch (r) {
    case GWY_EMU_EXIT_NORMAL_GUEST_RETURN: return "NORMAL_GUEST_RETURN";
    case GWY_EMU_EXIT_HOST_CALLBACK: return "HOST_CALLBACK";
    case GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST: return "YIELD_TO_NESTED_GUEST";
    case GWY_EMU_EXIT_UC_FAULT: return "UC_FAULT";
    case GWY_EMU_EXIT_EXPLICIT_STOP: return "EXPLICIT_STOP";
    default: return "UNKNOWN";
    }
}

const char *gwy_r9_leave_action_name(GwyR9LeaveAction a) {
    switch (a) {
    case GWY_R9_LEAVE_POP_OWNED_FRAME: return "POP_OWNED_FRAME";
    case GWY_R9_LEAVE_REJECT: return "REJECT";
    case GWY_R9_LEAVE_POP_FOREIGN_FRAME: return "POP_FOREIGN_FRAME";
    default: return "NOOP";
    }
}

GwyModuleCallKind gwy_module_call_kind_for_entry(const GwyLoadedModule *callee) {
    if (callee && callee->data.start_of_er_rw)
        return GWY_CALL_RUNTIME_ENTRY;
    return GWY_CALL_BOOTSTRAP_ENTRY;
}

static const char *mod_name(const GwyLoadedModule *m) {
    if (!m) return "?";
    return m->resolved_name[0] ? m->resolved_name : m->requested_name;
}

static void mark_entry_attempt(uint64_t callee_mid) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    GwyLoadedModule *mut;
    size_t i;
    if (!reg) return;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].module_id == callee_mid) {
            mut = &reg->modules[i];
            mut->data.entry_switch_attempted = 1;
            return;
        }
    }
}

static void fill_scope(ModuleR9Scope *s, uint64_t scope_id, uint64_t frame_id, int owns,
                       int enter_result, int pushed, uint64_t caller_mid, uint64_t callee_mid,
                       GwyModuleCallKind call_kind) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->scope_id = scope_id;
    s->frame_id = frame_id;
    s->generation = g_r9s.generation;
    s->caller_module_id = caller_mid;
    s->callee_module_id = callee_mid;
    s->call_kind = call_kind;
    s->owns_frame = owns;
    s->enter_result = enter_result;
    s->frame_pushed = pushed;
}

static void emit_enter_scope(const char *result, const GwyLoadedModule *caller,
                             const GwyLoadedModule *callee, GwyModuleCallKind call_kind,
                             uint32_t cur_r9, uint32_t target_r9, int frame_pushed,
                             uint32_t depth_before, const ModuleR9Scope *scope,
                             const ModuleR9Frame *top) {
    printf("[R9_SCOPE] stage=ENTER scope_id=%llu frame_id=%llu owns_frame=%s "
           "requested_kind=%s caller=%s callee=%s current_r9=0x%X target_r9=0x%X "
           "result=%s frame_pushed=%s depth_before=%u depth_after=%u "
           "top_frame_id=%llu top_frame_kind=%s generation=%llu "
           "module_r9_switch_gate=open nested_r9_scope_gate=%s "
           "allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY evidence=DOCUMENTED\n",
           (unsigned long long)(scope ? scope->scope_id : 0),
           (unsigned long long)(scope ? scope->frame_id : 0),
           (scope && scope->owns_frame) ? "yes" : "no", gwy_module_call_kind_name(call_kind),
           mod_name(caller), mod_name(callee), cur_r9, target_r9, result,
           frame_pushed ? "yes" : "no", depth_before, g_r9s.depth,
           (unsigned long long)(top ? top->frame_id : 0),
           top ? gwy_module_call_kind_name(top->call_kind) : "NONE",
           (unsigned long long)g_r9s.generation,
           ext_r9_scope_audit_gate_open() ? "open" : "blocked");
    fflush(stdout);
}

static void emit_leave_decision(const char *stage, const ModuleR9Scope *scope, const char *by,
                                GwyR9LeaveAction action, GwyEmuExitReason emu_exit,
                                uint64_t top_frame_id, GwyModuleCallKind top_kind,
                                uint32_t depth_before, uint32_t cur_r9, uint32_t new_r9) {
    printf("[R9_SCOPE] stage=%s scope_id=%llu owns_frame=%s requested_by=%s "
           "requested_frame_id=%llu actual_top_frame_id=%llu top_frame_kind=%s "
           "leave_action=%s emu_exit_reason=%s depth_before=%u depth_after=%u "
           "old_r9=0x%X new_r9=0x%X generation=%llu "
           "module_r9_switch_gate=open nested_r9_scope_gate=%s "
           "allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY evidence=DOCUMENTED\n",
           stage, (unsigned long long)(scope ? scope->scope_id : 0),
           (scope && scope->owns_frame) ? "yes" : "no", by ? by : "?",
           (unsigned long long)(scope ? scope->frame_id : 0), (unsigned long long)top_frame_id,
           top_kind ? gwy_module_call_kind_name(top_kind) : "NONE",
           gwy_r9_leave_action_name(action), gwy_emu_exit_reason_name(emu_exit), depth_before,
           g_r9s.depth, cur_r9, new_r9,
           (unsigned long long)(scope ? scope->generation : 0),
           ext_r9_scope_audit_gate_open() ? "open" : "blocked");
    fflush(stdout);
}

int module_r9_switch_enter(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                           GwyModuleCallKind call_kind) {
    return module_r9_switch_enter_ex(uc, caller_module_id, callee_module_id, call_kind, "enter",
                                     NULL);
}

int module_r9_switch_enter_ex(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                              GwyModuleCallKind call_kind, const char *pushed_by,
                              ModuleR9Scope *out_scope) {
    ModuleRegistry *reg;
    const GwyLoadedModule *caller;
    const GwyLoadedModule *callee;
    ModuleR9Frame *fr;
    ModuleR9Frame top;
    int have_top;
    uint32_t cur_r9 = 0;
    uint32_t new_r9;
    uint32_t depth_before;
    uint64_t scope_id;
    ModuleR9Scope scope;
    GwyR9WriteAudit audit;
    const char *by = pushed_by && pushed_by[0] ? pushed_by : "enter";

    depth_before = g_r9s.depth;
    have_top = module_r9_switch_peek_top(&top);
    scope_id = ++g_r9s.next_scope_id;
    memset(&scope, 0, sizeof(scope));

    if (!module_r9_switch_enabled()) {
        fill_scope(&scope, scope_id, 0, 0, 0, 0, caller_module_id, callee_module_id, call_kind);
        g_r9s.last_enter_scope = scope;
        if (out_scope) *out_scope = scope;
        return 0;
    }

    reg = gwy_ext_loader_bound_registry();
    caller = reg ? module_registry_find_by_id(reg, caller_module_id) : NULL;
    callee = reg ? module_registry_find_by_id(reg, callee_module_id) : NULL;

    if (caller_module_id && callee_module_id && caller_module_id == callee_module_id) {
        fill_scope(&scope, scope_id, have_top ? top.frame_id : 0, 0, 0, 0, caller_module_id,
                   callee_module_id, call_kind);
        g_r9s.last_enter_scope = scope;
        if (out_scope) *out_scope = scope;
        printf("[R9_SWITCH] stage=ENTER same_module=1 module=%s call_kind=%s depth=%u "
               "evidence=DOCUMENTED note=preserve_r9\n",
               mod_name(callee), gwy_module_call_kind_name(call_kind), g_r9s.depth);
        emit_enter_scope("SAME_MODULE", caller, callee, call_kind, cur_r9, cur_r9, 0, depth_before,
                         &scope, have_top ? &top : NULL);
        fflush(stdout);
        ext_r9_scope_audit_on_enter(&scope, call_kind, 0, cur_r9, cur_r9);
        return 0;
    }

    if (!uc) {
        fill_scope(&scope, scope_id, 0, 0, -1, 0, caller_module_id, callee_module_id, call_kind);
        g_r9s.last_enter_scope = scope;
        if (out_scope) *out_scope = scope;
        return -1;
    }

    if (call_kind == GWY_CALL_BOOTSTRAP_ENTRY || call_kind == GWY_CALL_RUNTIME_ENTRY)
        mark_entry_attempt(callee_module_id);

    if (!callee || !callee->data.start_of_er_rw) {
        g_r9s.blocked_count++;
        fill_scope(&scope, scope_id, 0, 0, -1, 0, caller_module_id, callee_module_id, call_kind);
        g_r9s.last_enter_scope = scope;
        if (out_scope) *out_scope = scope;
        printf("[R9_SWITCH_BLOCKED] reason=CALLEE_ER_RW_NOT_AVAILABLE module=%s "
               "call_kind=%s caller=%s callee_id=%llu depth=%u "
               "module_r9_switch_gate=open bootstrap_entry_r9_gate=blocked "
               "r9_policy=OBSERVE_ONLY evidence=DOCUMENTED note=no_dsm_r9_fallback\n",
               mod_name(callee), gwy_module_call_kind_name(call_kind), mod_name(caller),
               (unsigned long long)callee_module_id, g_r9s.depth);
        emit_enter_scope("BLOCKED_NO_ER_RW", caller, callee, call_kind, 0, 0, 0, depth_before,
                         &scope, have_top ? &top : NULL);
        fflush(stdout);
        return -1;
    }

    if (g_r9s.depth >= GWY_MAX_MODULE_CALL_DEPTH) {
        fill_scope(&scope, scope_id, 0, 0, -1, 0, caller_module_id, callee_module_id, call_kind);
        g_r9s.last_enter_scope = scope;
        if (out_scope) *out_scope = scope;
        printf("[R9_SWITCH_BLOCKED] reason=STACK_OVERFLOW module=%s depth=%u "
               "module_r9_switch_gate=open evidence=DOCUMENTED\n",
               mod_name(callee), g_r9s.depth);
        fflush(stdout);
        return -1;
    }

    new_r9 = callee->data.start_of_er_rw;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &cur_r9);
    if (cur_r9 == new_r9 ||
        (have_top && top.pushed && top.caller_module_id == caller_module_id &&
         top.callee_module_id == callee_module_id && callee->origin == MODULE_ORIGIN_DSM)) {
        /* Nested MRP→DSM: reuse outer frame (no push). If nested return restored MRP R9,
         * switch back to DSM for this helper without growing depth. */
        if (cur_r9 != new_r9) {
            GwyR9WriteAudit audit;
            memset(&audit, 0, sizeof(audit));
            audit.reason = GWY_R9_WRITE_MODULE_R9_SWITCH_ENTER;
            audit.frame_id = top.frame_id;
            audit.host_callsite = "enter_reuse_dsm_frame";
            audit.guest_pc = g_r9s.guest_pc;
            audit.guest_module = mod_name(callee);
            if (!guest_memory_uc_write_r9_ex((struct uc_struct *)uc, new_r9, &audit)) {
                fill_scope(&scope, scope_id, 0, 0, -1, 0, caller_module_id, callee_module_id,
                           call_kind);
                g_r9s.last_enter_scope = scope;
                if (out_scope) *out_scope = scope;
                return -1;
            }
        }
        fill_scope(&scope, scope_id, have_top ? top.frame_id : 0, 0, 0, 0, caller_module_id,
                   callee_module_id, call_kind);
        g_r9s.last_enter_scope = scope;
        if (out_scope) *out_scope = scope;
        printf("[R9_SWITCH] stage=ENTER already_switched=1 from=%s to=%s r9=0x%X call_kind=%s "
               "depth=%u evidence=DOCUMENTED note=dsm_return_side_stack\n",
               mod_name(caller), mod_name(callee), new_r9, gwy_module_call_kind_name(call_kind),
               g_r9s.depth);
        emit_enter_scope("ALREADY_SWITCHED", caller, callee, call_kind, cur_r9, new_r9, 0,
                         depth_before, &scope, have_top ? &top : NULL);
        fflush(stdout);
        ext_r9_scope_audit_on_enter(&scope, call_kind, 1, cur_r9, new_r9);
        if (callee->origin == MODULE_ORIGIN_DSM && callee->map.guest_code_base &&
            callee->map.guest_code_size > 1u)
            module_r9_switch_attach_dsm_r9_guard(uc, callee_module_id, callee->map.guest_code_base,
                                                 callee->map.guest_code_size);
        return 0;
    }

    fr = &g_r9s.frames[g_r9s.depth];
    memset(fr, 0, sizeof(*fr));
    fr->frame_id = ++g_r9s.next_frame_id;
    fr->generation = ++g_r9s.generation;
    fr->owner_call_id = scope_id;
    fr->owner_scope_id = scope_id;
    fr->caller_module_id = caller_module_id;
    fr->callee_module_id = callee_module_id;
    fr->saved_r9 = cur_r9;
    fr->callee_r9 = new_r9;
    fr->call_kind = call_kind;
    fr->owner_kind = call_kind;
    fr->depth = g_r9s.depth + 1u;
    fr->pushed = 1;
    fr->active = 1;
    snprintf(fr->pushed_by, sizeof(fr->pushed_by), "%s", by);
    snprintf(fr->expected_leave_by, sizeof(fr->expected_leave_by), "%s", by);

    memset(&audit, 0, sizeof(audit));
    audit.reason = GWY_R9_WRITE_MODULE_R9_SWITCH_ENTER;
    audit.frame_id = fr->frame_id;
    audit.scope_id = scope_id;
    audit.host_callsite = by;
    audit.guest_pc = g_r9s.guest_pc;
    audit.guest_module = g_r9s.guest_module[0] ? g_r9s.guest_module : mod_name(callee);
    audit.depth_before = depth_before;
    audit.depth_after = depth_before + 1u;

    if (!guest_memory_uc_write_r9_ex((struct uc_struct *)uc, new_r9, &audit)) {
        printf("[R9_SWITCH_BLOCKED] reason=UC_REG_WRITE_FAIL module=%s "
               "module_r9_switch_gate=open evidence=OBSERVED\n",
               mod_name(callee));
        fflush(stdout);
        fill_scope(&scope, scope_id, 0, 0, -1, 0, caller_module_id, callee_module_id, call_kind);
        g_r9s.last_enter_scope = scope;
        if (out_scope) *out_scope = scope;
        return -1;
    }

    g_r9s.depth++;
    g_r9s.enter_ok_count++;
    fill_scope(&scope, scope_id, fr->frame_id, 1, 1, 1, caller_module_id, callee_module_id,
               call_kind);
    scope.generation = fr->generation;
    g_r9s.last_enter_scope = scope;
    if (out_scope) *out_scope = scope;

    printf("[R9_SWITCH] stage=ENTER from=%s to=%s old_r9=0x%X new_r9=0x%X call_kind=%s "
           "depth=%u er_rw_size=%u evidence=DOCUMENTED "
           "module_r9_switch_gate=open allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY "
           "phase6b_b_gate=blocked note=r9_only\n",
           mod_name(caller), mod_name(callee), cur_r9, new_r9, gwy_module_call_kind_name(call_kind),
           g_r9s.depth, callee->data.er_rw_size);
    emit_enter_scope("SWITCHED", caller, callee, call_kind, cur_r9, new_r9, 1, depth_before, &scope,
                     fr);
    fflush(stdout);
    {
        GwyR9WriteRecord wr;
        if (guest_memory_r9_last_write(&wr)) ext_r9_scope_audit_on_r9_write(&wr);
    }
    ext_r9_scope_audit_on_enter(&scope, call_kind, 0, cur_r9, new_r9);
    if (callee->origin == MODULE_ORIGIN_DSM && callee->map.guest_code_base &&
        callee->map.guest_code_size > 1u)
        module_r9_switch_attach_dsm_r9_guard(uc, callee_module_id, callee->map.guest_code_base,
                                             callee->map.guest_code_size);
    return 1;
}

int module_r9_switch_leave_scope(void *uc, const ModuleR9Scope *scope, const char *requested_by,
                                 GwyEmuExitReason emu_exit) {
    ModuleR9Frame *fr;
    ModuleRegistry *reg;
    const GwyLoadedModule *caller;
    const GwyLoadedModule *callee;
    uint32_t cur_r9 = 0;
    uint32_t after = 0;
    uint32_t depth_before;
    GwyR9LeaveAction action = GWY_R9_LEAVE_NOOP;
    GwyR9WriteAudit audit;
    const char *by = requested_by && requested_by[0] ? requested_by : "leave";
    uint64_t top_frame_id = 0;
    GwyModuleCallKind top_kind = (GwyModuleCallKind)0;
    ModuleR9Scope local;

    if (!module_r9_switch_enabled()) return 0;
    depth_before = g_r9s.depth;
    if (uc) (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &cur_r9);

    if (!scope) {
        emit_leave_decision("NOOP", NULL, by, GWY_R9_LEAVE_NOOP, emu_exit, 0, (GwyModuleCallKind)0,
                            depth_before, cur_r9, cur_r9);
        return 0;
    }
    local = *scope;

    if (g_r9s.depth > 0) {
        fr = &g_r9s.frames[g_r9s.depth - 1u];
        top_frame_id = fr->frame_id;
        top_kind = fr->call_kind;
    } else {
        fr = NULL;
    }

    /* D3 rules: yield / unowned → NOOP (never pop foreign). */
    if (emu_exit == GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST || !local.owns_frame) {
        action = GWY_R9_LEAVE_NOOP;
        emit_leave_decision("NOOP", &local, by, action, emu_exit, top_frame_id, top_kind,
                            depth_before, cur_r9, cur_r9);
        ext_r9_scope_audit_on_leave(by, local.scope_id, local.owns_frame, action, top_frame_id,
                                    top_kind, emu_exit, cur_r9);
        ext_post_cont_audit_on_r9_leave(by, action, emu_exit, top_frame_id, top_kind, cur_r9);
        return 0;
    }

    /* Owned leave requires NORMAL or HOST_CALLBACK return. */
    if (emu_exit != GWY_EMU_EXIT_NORMAL_GUEST_RETURN && emu_exit != GWY_EMU_EXIT_HOST_CALLBACK &&
        emu_exit != GWY_EMU_EXIT_UNKNOWN) {
        action = GWY_R9_LEAVE_NOOP;
        emit_leave_decision("NOOP", &local, by, action, emu_exit, top_frame_id, top_kind,
                            depth_before, cur_r9, cur_r9);
        ext_r9_scope_audit_on_leave(by, local.scope_id, local.owns_frame, action, top_frame_id,
                                    top_kind, emu_exit, cur_r9);
        ext_post_cont_audit_on_r9_leave(by, action, emu_exit, top_frame_id, top_kind, cur_r9);
        return 0;
    }

    if (!uc || !fr || local.frame_id == 0 || fr->frame_id != local.frame_id ||
        fr->owner_scope_id != local.scope_id ||
        (local.generation && fr->generation != local.generation)) {
        action = GWY_R9_LEAVE_REJECT;
        emit_leave_decision("REJECT", &local, by, action, emu_exit, top_frame_id, top_kind,
                            depth_before, cur_r9, cur_r9);
        ext_r9_scope_audit_on_leave(by, local.scope_id, local.owns_frame, action, top_frame_id,
                                    top_kind, emu_exit, cur_r9);
        ext_post_cont_audit_on_r9_leave(by, action, emu_exit, top_frame_id, top_kind, cur_r9);
        return -1;
    }

    action = GWY_R9_LEAVE_POP_OWNED_FRAME;
    reg = gwy_ext_loader_bound_registry();
    caller = reg ? module_registry_find_by_id(reg, fr->caller_module_id) : NULL;
    callee = reg ? module_registry_find_by_id(reg, fr->callee_module_id) : NULL;

    memset(&audit, 0, sizeof(audit));
    audit.reason = GWY_R9_WRITE_MODULE_R9_SWITCH_LEAVE;
    audit.frame_id = fr->frame_id;
    audit.scope_id = local.scope_id;
    audit.host_callsite = by;
    audit.guest_pc = g_r9s.guest_pc;
    audit.guest_module = g_r9s.guest_module[0] ? g_r9s.guest_module : mod_name(callee);
    audit.depth_before = depth_before;
    audit.depth_after = depth_before > 0 ? depth_before - 1u : 0;

    if (!guest_memory_uc_write_r9_ex((struct uc_struct *)uc, fr->saved_r9, &audit)) {
        printf("[R9_SWITCH] stage=LEAVE restore_fail=1 from=%s to=%s saved_r9=0x%X "
               "evidence=OBSERVED\n",
               mod_name(callee), mod_name(caller), fr->saved_r9);
        fflush(stdout);
        return -1;
    }
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &after);
    g_r9s.depth--;
    g_r9s.leave_count++;
    g_r9s.emu_exit_hint = GWY_EMU_EXIT_UNKNOWN;

    emit_leave_decision("LEAVE", &local, by, action, emu_exit, top_frame_id, top_kind, depth_before,
                        cur_r9, after);
    printf("[R9_SWITCH] stage=LEAVE from=%s to=%s restore_r9=0x%X depth=%u call_kind=%s "
           "leave_action=%s foreign=no evidence=DOCUMENTED module_r9_switch_gate=open "
           "phase6b_b_gate=blocked nested_r9_scope_gate=%s "
           "allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY\n",
           mod_name(callee), mod_name(caller), after, g_r9s.depth,
           gwy_module_call_kind_name(fr->call_kind), gwy_r9_leave_action_name(action),
           ext_r9_scope_audit_gate_open() ? "open" : "blocked");
    fflush(stdout);

    {
        GwyR9WriteRecord wr;
        if (guest_memory_r9_last_write(&wr)) ext_r9_scope_audit_on_r9_write(&wr);
    }
    ext_r9_scope_audit_on_leave(by, local.scope_id, local.owns_frame, action, top_frame_id, top_kind,
                                emu_exit, after);
    ext_callback_frame_on_r9_scope_leave(by, action, top_frame_id, top_kind);
    ext_post_cont_audit_on_r9_leave(by, action, emu_exit, top_frame_id, top_kind, after);
    return 1;
}

int module_r9_switch_leave(void *uc) {
    ModuleR9Scope last;
    module_r9_switch_last_enter_scope(&last);
    return module_r9_switch_leave_scope(uc, &last, "leave",
                                        g_r9s.emu_exit_hint != GWY_EMU_EXIT_UNKNOWN
                                            ? g_r9s.emu_exit_hint
                                            : GWY_EMU_EXIT_UNKNOWN);
}

void module_r9_switch_note_dsm_code(uint64_t module_id) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    DsmReturnSite *site;
    if (!module_r9_switch_enabled() || !module_id || g_dsm_ret.depth == 0) return;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    if (!m || m->origin != MODULE_ORIGIN_DSM) return;
    site = &g_dsm_ret.sites[g_dsm_ret.depth - 1u];
    site->executed = 1;
    if (g_r9s.depth > 0) {
        ModuleR9Frame *fr = &g_r9s.frames[g_r9s.depth - 1u];
        if (fr->pushed && fr->frame_id == site->r9_frame_id) fr->dsm_helper_executed = 1;
    }
}

/* If DSM code is running with an MRP module R9, force DSM ER_RW (observe+correct). */
int module_r9_switch_ensure_dsm_r9(void *uc, uint32_t guest_pc) {
    ModuleRegistry *reg;
    const GwyLoadedModule *dsm;
    const GwyLoadedModule *pc_mod;
    uint32_t cur_r9 = 0;
    uint32_t dsm_r9;
    GwyR9WriteAudit audit;
    size_t i;

    if (!module_r9_switch_enabled() || !uc) return 0;
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return 0;
    pc_mod = module_registry_find_by_code_addr(reg, guest_pc & ~1u);
    if (!pc_mod || pc_mod->origin != MODULE_ORIGIN_DSM) return 0;

    dsm = NULL;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].origin == MODULE_ORIGIN_DSM && reg->modules[i].data.start_of_er_rw) {
            dsm = &reg->modules[i];
            break;
        }
    }
    if (!dsm) return 0;
    dsm_r9 = dsm->data.start_of_er_rw;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &cur_r9);
    if (cur_r9 == dsm_r9) return 0;

    memset(&audit, 0, sizeof(audit));
    audit.reason = GWY_R9_WRITE_MODULE_R9_SWITCH_ENTER;
    audit.host_callsite = "ensure_dsm_r9";
    audit.guest_pc = guest_pc;
    audit.guest_module = "dsm:cfunction.ext";
    if (!guest_memory_uc_write_r9_ex((struct uc_struct *)uc, dsm_r9, &audit)) return 0;
    printf("[R9_SWITCH] stage=ENSURE_DSM_R9 pc=0x%X old_r9=0x%X new_r9=0x%X "
           "evidence=DOCUMENTED note=dsm_code_with_mrp_r9\n",
           guest_pc, cur_r9, dsm_r9);
    fflush(stdout);
    if (g_dsm_ret.depth > 0) g_dsm_ret.sites[g_dsm_ret.depth - 1u].executed = 1;
    return 1;
}

void module_r9_switch_attach_dsm_r9_guard(void *uc, uint64_t dsm_module_id, uint32_t code_base,
                                          uint32_t code_size) {
#ifdef GWY_HAVE_UNICORN
    uc_err ue;
    if (!module_r9_switch_enabled() || !uc || !code_base || code_size < 4u) return;
    if (g_dsm_r9_hook_installed) return;
    g_dsm_r9_hook_module_id = dsm_module_id;
    ue = uc_hook_add((uc_engine *)uc, &g_dsm_r9_hook, UC_HOOK_BLOCK, (void *)dsm_r9_guard_on_code,
                     NULL, (uint64_t)code_base, (uint64_t)code_base + (uint64_t)code_size - 1ull);
    if (ue == UC_ERR_OK) {
        g_dsm_r9_hook_installed = 1;
        printf("[R9_SWITCH] stage=DSM_R9_GUARD_ATTACH code=[0x%X,0x%X) module_id=%llu "
               "evidence=DOCUMENTED note=block_hook_r9_only\n",
               code_base, code_base + code_size, (unsigned long long)dsm_module_id);
        fflush(stdout);
    }
#else
    (void)uc;
    (void)dsm_module_id;
    (void)code_base;
    (void)code_size;
#endif
}

static uint32_t dsm_helper_return_pc_from_call(void *uc, uint32_t call_pc, uint32_t cpsr) {
    int thumb = (cpsr & (1u << 5)) != 0;
    uint32_t word = 0;
    int rm = -1;

    if (!uc || !call_pc) return 0;
    if (thumb) {
        uint16_t half;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, call_pc & ~3u, &word))
            return (call_pc + 2u) & ~1u;
        half = (uint16_t)((call_pc & 2u) ? (word >> 16) : (word & 0xFFFFu));
        /* BLX Rm: 16-bit → return pc+2 */
        if (ext_entry_decode_thumb_blx_rm(half, &rm)) return (call_pc + 2u) & ~1u;
        /* Thumb BL/BLX imm: 32-bit → return pc+4 */
        if ((half & 0xF800u) == 0xF000u) return (call_pc + 4u) & ~1u;
        return (call_pc + 2u) & ~1u;
    }
    if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, call_pc, &word))
        return call_pc + 4u;
    (void)word;
    return call_pc + 4u;
}

void module_r9_switch_arm_dsm_helper_return(void *uc, uint64_t caller_module_id, uint32_t call_pc,
                                           uint32_t cpsr) {
    ModuleR9Frame *fr = NULL;
    ModuleRegistry *reg;
    const GwyLoadedModule *callee = NULL;
    DsmReturnSite *site;
    uint32_t ret;
    uint32_t call = call_pc & ~1u;
    int owns_pop = 0;
    uint64_t frame_id = 0;

    if (!module_r9_switch_enabled() || !caller_module_id || !call_pc) return;
    ret = dsm_helper_return_pc_from_call(uc, call_pc, cpsr);
    if (!ret) return;

    if (g_r9s.depth > 0) {
        fr = &g_r9s.frames[g_r9s.depth - 1u];
        if (fr->pushed && fr->caller_module_id == caller_module_id) {
            reg = gwy_ext_loader_bound_registry();
            callee = reg ? module_registry_find_by_id(reg, fr->callee_module_id) : NULL;
            if (callee && callee->origin == MODULE_ORIGIN_DSM) {
                frame_id = fr->frame_id;
                /* First return site for this R9 frame owns the R9 pop. */
                if (!fr->dsm_return_pc) {
                    fr->dsm_call_pc = call;
                    fr->dsm_return_pc = ret & ~1u;
                    fr->dsm_helper_executed = 0;
                    owns_pop = 1;
                }
            }
        }
    }

    if (g_dsm_ret.depth >= GWY_MAX_DSM_RETURN_DEPTH) {
        printf("[R9_SWITCH] stage=ARM_DSM_RETURN_OVERFLOW caller_id=%llu call_pc=0x%X "
               "depth=%u evidence=OBSERVED note=side_stack_full\n",
               (unsigned long long)caller_module_id, call, g_dsm_ret.depth);
        fflush(stdout);
        return;
    }
    site = &g_dsm_ret.sites[g_dsm_ret.depth++];
    memset(site, 0, sizeof(*site));
    site->caller_module_id = caller_module_id;
    site->r9_frame_id = frame_id;
    site->call_pc = call;
    site->return_pc = ret & ~1u;
    /* executed stays 0 until DSM code is observed (note_dsm_code). Arm-time
     * executed=1 caused false RESTORE on Thumb second-half / pre-BLX fallthrough
     * (return_pc hit before helper ran → MRP R9 while DSM still active). */
    site->executed = 0;
    site->owns_r9_pop = owns_pop;
    printf("[R9_SWITCH] stage=ARM_DSM_RETURN caller_id=%llu call_pc=0x%X return_pc=0x%X "
           "frame_id=%llu owns_r9_pop=%d side_depth=%u evidence=DOCUMENTED "
           "note=mrp_dsm_helper_blx\n",
           (unsigned long long)caller_module_id, call, site->return_pc,
           (unsigned long long)frame_id, owns_pop, g_dsm_ret.depth);
    fflush(stdout);
}

int module_r9_switch_cancel_dsm_helper_blx(void *uc, uint32_t call_pc, uint32_t restore_r9) {
    uint32_t call = call_pc & ~1u;
    uint32_t cur_r9 = 0;
    int cancelled = 0;
    uint32_t i;
    if (!call) return 0;
    /* Remove matching sites from the top down (most recent BLX first). */
    while (g_dsm_ret.depth > 0) {
        DsmReturnSite *site = &g_dsm_ret.sites[g_dsm_ret.depth - 1u];
        if ((site->call_pc & ~1u) != call) break;
        g_dsm_ret.depth--;
        cancelled = 1;
        printf("[R9_SWITCH] stage=CANCEL_DSM_BLX call_pc=0x%X return_pc=0x%X "
               "side_depth=%u evidence=DOCUMENTED note=host_skipped_blx\n",
               call, site->return_pc, g_dsm_ret.depth);
        fflush(stdout);
    }
    /* Also scrub older orphans for this call_pc (index-scan loop). */
    for (i = 0; i < g_dsm_ret.depth;) {
        if ((g_dsm_ret.sites[i].call_pc & ~1u) == call) {
            memmove(&g_dsm_ret.sites[i], &g_dsm_ret.sites[i + 1u],
                    (g_dsm_ret.depth - i - 1u) * sizeof(g_dsm_ret.sites[0]));
            g_dsm_ret.depth--;
            cancelled = 1;
        } else {
            i++;
        }
    }
    if (uc && restore_r9) {
        (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &cur_r9);
        if (cur_r9 != restore_r9) {
            GwyR9WriteAudit audit;
            memset(&audit, 0, sizeof(audit));
            audit.reason = GWY_R9_WRITE_MODULE_R9_SWITCH_LEAVE;
            audit.host_callsite = "cancel_dsm_helper_blx";
            audit.guest_pc = call;
            audit.guest_module = "host_shim_skip_blx";
            if (guest_memory_uc_write_r9_ex((struct uc_struct *)uc, restore_r9, &audit)) {
                printf("[R9_SWITCH] stage=RESTORE_AFTER_CANCEL old_r9=0x%X new_r9=0x%X "
                       "call_pc=0x%X evidence=DOCUMENTED note=host_shim_r9\n",
                       cur_r9, restore_r9, call);
                fflush(stdout);
            }
        }
    }
    return cancelled;
}

void module_r9_switch_clear_dsm_return_side_stack(void) {
    if (g_dsm_ret.depth == 0) return;
    printf("[R9_SWITCH] stage=CLEAR_DSM_SIDE_STACK depth_before=%u evidence=DOCUMENTED "
           "note=postmatch_or_host_complete\n",
           g_dsm_ret.depth);
    fflush(stdout);
    memset(&g_dsm_ret, 0, sizeof(g_dsm_ret));
}

int module_r9_switch_restore_after_dsm_helper(void *uc, uint64_t caller_module_id,
                                               uint32_t guest_pc) {
    DsmReturnSite site;
    ModuleR9Frame *fr;
    ModuleR9Scope sc;
    ModuleRegistry *reg;
    const GwyLoadedModule *callee;
    uint32_t cur_r9 = 0;
    uint32_t lr = 0;
    uint32_t pc = guest_pc & ~1u;
    int rc = 0;
    int lr_matches_return = 0;

    if (!module_r9_switch_enabled() || !uc || !caller_module_id || g_dsm_ret.depth == 0) return 0;
    site = g_dsm_ret.sites[g_dsm_ret.depth - 1u];
    if (site.caller_module_id != caller_module_id) return 0;
    if (pc != (site.return_pc & ~1u)) return 0;

    /* Reject Thumb second-half / pre-BLX fallthrough: return_pc can be observed before the
     * helper runs. Real return leaves LR at the return site until the next call. */
#ifdef GWY_HAVE_UNICORN
    {
        uc_engine *u = (uc_engine *)uc;
        (void)uc_reg_read(u, UC_ARM_REG_LR, &lr);
    }
#endif
    lr_matches_return = ((lr & ~1u) == (site.return_pc & ~1u));
    /* Accept when DSM was observed, or LR still names the return site (leaf helpers
     * like memcpy/memset at 0x94E94/0x94F04 never touch watched P). */
    if (!site.executed && !lr_matches_return) return 0;

    g_dsm_ret.depth--;
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &cur_r9);

    printf("[R9_SWITCH] stage=RESTORE_AFTER_DSM to_caller_id=%llu pc=0x%X return_pc=0x%X "
           "call_pc=0x%X cur_r9=0x%X lr=0x%X executed=%d owns_r9_pop=%d side_depth=%u "
           "frame_id=%llu evidence=DOCUMENTED note=mrp_dsm_helper_return\n",
           (unsigned long long)caller_module_id, pc, site.return_pc, site.call_pc, cur_r9, lr,
           site.executed, site.owns_r9_pop, g_dsm_ret.depth,
           (unsigned long long)site.r9_frame_id);
    fflush(stdout);

    /* Nested already_switched: keep outer R9 frame, but MRP code after the helper must
     * see caller ER_RW (gamelist uses add rd,r9 for allocator slots). */
    if (!site.owns_r9_pop) {
        const GwyLoadedModule *caller;
        uint32_t restore_r9 = 0;
        reg = gwy_ext_loader_bound_registry();
        if (g_r9s.depth > 0) {
            fr = &g_r9s.frames[g_r9s.depth - 1u];
            if (fr->pushed && fr->frame_id == site.r9_frame_id && fr->saved_r9)
                restore_r9 = fr->saved_r9;
        }
        if (!restore_r9 && reg) {
            caller = module_registry_find_by_id(reg, caller_module_id);
            if (caller && caller->data.start_of_er_rw) restore_r9 = caller->data.start_of_er_rw;
        }
        if (restore_r9 && cur_r9 != restore_r9) {
            GwyR9WriteAudit audit;
            memset(&audit, 0, sizeof(audit));
            audit.reason = GWY_R9_WRITE_MODULE_R9_SWITCH_LEAVE;
            audit.frame_id = site.r9_frame_id;
            audit.host_callsite = "mrp_return_from_dsm_helper_nested";
            audit.guest_pc = pc;
            audit.guest_module = "mrp_nested_dsm_return";
            if (guest_memory_uc_write_r9_ex((struct uc_struct *)uc, restore_r9, &audit)) {
                printf("[R9_SWITCH] stage=RESTORE_NESTED_MRP_R9 to_caller_id=%llu "
                       "old_r9=0x%X new_r9=0x%X pc=0x%X frame_id=%llu evidence=DOCUMENTED "
                       "note=keep_outer_frame\n",
                       (unsigned long long)caller_module_id, cur_r9, restore_r9, pc,
                       (unsigned long long)site.r9_frame_id);
                fflush(stdout);
            }
        }
        return 1;
    }

    if (g_r9s.depth == 0) return 1;
    fr = &g_r9s.frames[g_r9s.depth - 1u];
    if (!fr->pushed || fr->frame_id != site.r9_frame_id) return 1;
    reg = gwy_ext_loader_bound_registry();
    callee = reg ? module_registry_find_by_id(reg, fr->callee_module_id) : NULL;
    if (!callee || callee->origin != MODULE_ORIGIN_DSM) return 1;

    memset(&sc, 0, sizeof(sc));
    sc.scope_id = fr->owner_scope_id;
    sc.frame_id = fr->frame_id;
    sc.generation = fr->generation;
    sc.caller_module_id = fr->caller_module_id;
    sc.callee_module_id = fr->callee_module_id;
    sc.call_kind = fr->call_kind;
    sc.owns_frame = 1;
    sc.frame_pushed = 1;
    sc.enter_result = 1;
    rc = module_r9_switch_leave_scope(uc, &sc, "mrp_return_from_dsm_helper",
                                      GWY_EMU_EXIT_NORMAL_GUEST_RETURN);
    return rc;
}

int module_r9_switch_leave_ex(void *uc, const char *requested_by, uint64_t requesting_scope_id,
                              int owns_frame, GwyEmuExitReason emu_exit) {
    ModuleR9Scope sc;
    ModuleR9Frame top;
    memset(&sc, 0, sizeof(sc));
    sc.scope_id = requesting_scope_id;
    sc.owns_frame = owns_frame;
    if (owns_frame && module_r9_switch_peek_top(&top) && top.owner_scope_id == requesting_scope_id) {
        sc.frame_id = top.frame_id;
        sc.generation = top.generation;
        sc.caller_module_id = top.caller_module_id;
        sc.callee_module_id = top.callee_module_id;
        sc.call_kind = top.call_kind;
        sc.frame_pushed = 1;
        sc.enter_result = 1;
    } else {
        module_r9_switch_last_enter_scope(&sc);
        if (requesting_scope_id) sc.scope_id = requesting_scope_id;
        sc.owns_frame = owns_frame;
    }
    return module_r9_switch_leave_scope(uc, &sc, requested_by, emu_exit);
}

void module_r9_switch_abort(void *uc, const char *reason) {
    uint32_t guest_r9 = 0;
    uint32_t saved = 0;
    const char *why = reason ? reason : "UNKNOWN";
    GwyR9WriteAudit audit;
    uint32_t depth_before;
    if (!module_r9_switch_enabled()) {
        module_r9_switch_reset();
        return;
    }
    depth_before = g_r9s.depth;
    if (uc) (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &guest_r9);
    if (g_r9s.depth > 0) saved = g_r9s.frames[0].saved_r9;
    printf("[R9_SCOPE] stage=UNWIND active_depth=%u guest_r9=0x%X restore_r9=0x%X reason=%s "
           "module_r9_switch_gate=open nested_r9_scope_gate=%s "
           "allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY evidence=OBSERVED "
           "note=clear_all_frames_on_uc_fault\n",
           depth_before, guest_r9, saved,
           why, ext_r9_scope_audit_gate_open() ? "open" : "blocked");
    printf("[R9_SWITCH] stage=ABORT active_depth=%u guest_r9=0x%X saved_r9=0x%X reason=%s "
           "evidence=OBSERVED note=clear_host_frames\n",
           g_r9s.depth, guest_r9, saved, why);
    fflush(stdout);
    if (uc && g_r9s.depth > 0) {
        memset(&audit, 0, sizeof(audit));
        audit.reason = GWY_R9_WRITE_MODULE_R9_SWITCH_ABORT;
        audit.frame_id = g_r9s.frames[0].frame_id;
        audit.scope_id = g_r9s.frames[0].owner_scope_id;
        audit.host_callsite = why;
        audit.guest_pc = g_r9s.guest_pc;
        audit.guest_module = g_r9s.guest_module;
        audit.depth_before = g_r9s.depth;
        audit.depth_after = 0;
        (void)guest_memory_uc_write_r9_ex((struct uc_struct *)uc, g_r9s.frames[0].saved_r9, &audit);
        {
            GwyR9WriteRecord wr;
            if (guest_memory_r9_last_write(&wr)) ext_r9_scope_audit_on_r9_write(&wr);
        }
    }
    module_r9_switch_reset();
}
