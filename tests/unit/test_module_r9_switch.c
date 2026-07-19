#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/ext_r9_scope_audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void set_env_on(void) {
#ifdef _WIN32
    _putenv("GWY_MODULE_R9_SWITCH=1");
    _putenv("GWY_NESTED_R9_SCOPE=1");
#else
    setenv("GWY_MODULE_R9_SWITCH", "1", 1);
    setenv("GWY_NESTED_R9_SCOPE", "1", 1);
#endif
}

static void set_env_off(void) {
#ifdef _WIN32
    _putenv("GWY_MODULE_R9_SWITCH=");
    _putenv("GWY_NESTED_R9_SCOPE=");
#else
    unsetenv("GWY_MODULE_R9_SWITCH");
    unsetenv("GWY_NESTED_R9_SCOPE");
#endif
}

int main(void) {
    LauncherError err;
    ModuleRegistry reg;
    ModuleR9Scope scope;
    ModuleR9Scope bad;
    ModuleR9Frame top;

    module_r9_switch_reset();
    guest_memory_r9_write_reset();
    ext_r9_scope_audit_reset();
    set_env_off();
    if (module_r9_switch_enabled()) return 1;
    if (strcmp(gwy_module_call_kind_name(GWY_CALL_BOOTSTRAP_ENTRY), "BOOTSTRAP_ENTRY") != 0)
        return 2;
    if (strcmp(gwy_module_call_kind_name(GWY_CALL_MR_HELPER), "MR_HELPER") != 0) return 3;
    if (strcmp(gwy_emu_exit_reason_name(GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST),
               "YIELD_TO_NESTED_GUEST") != 0)
        return 20;
    if (strcmp(gwy_r9_leave_action_name(GWY_R9_LEAVE_REJECT), "REJECT") != 0) return 21;
    if (strcmp(gwy_r9_leave_action_name(GWY_R9_LEAVE_NOOP), "NOOP") != 0) return 29;

    if (module_r9_switch_enter(NULL, 1, 2, GWY_CALL_BOOTSTRAP_ENTRY) != 0) return 4;

    set_env_on();
    module_r9_switch_reset();
    guest_memory_r9_write_reset();
    ext_r9_scope_audit_reset();
    if (!module_r9_switch_enabled()) return 5;

    /* same-module → owns_frame=no */
    if (module_r9_switch_enter(NULL, 1, 1, GWY_CALL_BOOTSTRAP_ENTRY) != 0) return 6;
    module_r9_switch_last_enter_scope(&scope);
    if (scope.owns_frame != 0 || scope.frame_pushed != 0) return 22;
    /* D3: unowned leave must NOOP */
    if (module_r9_switch_leave_scope(NULL, &scope, "unit_unowned",
                                     GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST) != 0)
        return 30;
    if (module_r9_switch_depth() != 0) return 31;

    module_registry_init(&reg, "test.mrp", "00");
    if (module_registry_discover(&reg, "mod.ext", &err) != L_OK) return 7;
    {
        const GwyLoadedModule *m = module_registry_find(&reg, "mod.ext");
        if (!m) return 8;
        if (gwy_module_call_kind_for_entry(m) != GWY_CALL_BOOTSTRAP_ENTRY) return 15;
        if (module_registry_set_er_rw(&reg, m->module_id, 0, 0, GWY_ER_RW_SRC_MR_C_FUNCTION_LOAD,
                                      GWY_ADDR_EV_DOCUMENTED, &err) == L_OK)
            return 9;
        if (module_registry_set_er_rw(&reg, m->module_id, 0x1000, 64, GWY_ER_RW_SRC_MR_C_FUNCTION_LOAD,
                                      GWY_ADDR_EV_DOCUMENTED, &err) != L_OK)
            return 10;
        m = module_registry_find_by_id(&reg, m->module_id);
        if (!m || m->data.start_of_er_rw != 0x1000 || m->data.er_rw_size != 64) return 11;
        if (gwy_module_call_kind_for_entry(m) != GWY_CALL_RUNTIME_ENTRY) return 16;
    }

    /* Leave with no depth → NOOP. */
    if (module_r9_switch_leave_ex(NULL, "unit", 0, 0, GWY_EMU_EXIT_UNKNOWN) != 0) return 23;

    module_r9_switch_last_enter_scope(&scope);
    if (scope.scope_id == 0) return 24;
    if (module_r9_switch_peek_top(&top) != 0) return 25;

    /* REJECT: owns_frame with bogus frame_id and no stack */
    memset(&bad, 0, sizeof(bad));
    bad.scope_id = 99;
    bad.frame_id = 42;
    bad.generation = 1;
    bad.owns_frame = 1;
    bad.enter_result = 1;
    bad.frame_pushed = 1;
    if (module_r9_switch_leave_scope(NULL, &bad, "unit_reject",
                                     GWY_EMU_EXIT_NORMAL_GUEST_RETURN) != -1)
        return 32;

    module_r9_switch_abort(NULL, "UNIT_TEST");
    if (module_r9_switch_depth() != 0) return 13;

    if (strcmp(gwy_r9_write_reason_name(GWY_R9_WRITE_MODULE_R9_SWITCH_LEAVE),
               "MODULE_R9_SWITCH_LEAVE") != 0)
        return 26;
    if (ext_r9_scope_audit_gate_open() == 0) {
        /* Gate may open after NOOP balance path above. */
    }

    set_env_off();
    printf("test_module_r9_switch OK\n");
    return 0;
}
