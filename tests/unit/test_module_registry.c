#include "gwy_launcher/ext_resolver.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_archive.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    ModuleRegistry reg;
    ExtResolvedMember resolved;
    MrpMember member;
    LauncherError err;
    const GwyLoadedModule *m;
    GwyModuleMapping map;

    module_registry_init(&reg, "gwy/jjfb.mrp",
                         "52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036");
    module_registry_set_provenance(&reg, "gwy/jjfb.mrp", "generated_member_view",
                                   "scoped_member_alias");

    if (module_registry_discover(&reg, "cfunction.ext", &err) != L_OK) return 1;
    m = module_registry_find(&reg, "cfunction.ext");
    if (!m || m->state != GWY_MODULE_DISCOVERED || m->map.helper_address != 0) return 1;
    if (m->origin != MODULE_ORIGIN_MRP_MEMBER) return 1;

    memset(&member, 0, sizeof(member));
    snprintf(member.name, sizeof(member.name), "%s", "robotol.ext");
    member.stored_size = 161178;
    member.unpacked_size = 253420;
    member.unpacked_known = 1;
    member.compression = MRP_COMP_GZIP;

    memset(&resolved, 0, sizeof(resolved));
    snprintf(resolved.requested_name, sizeof(resolved.requested_name), "%s", "cfunction.ext");
    snprintf(resolved.resolved_name, sizeof(resolved.resolved_name), "%s", "robotol.ext");
    resolved.strategy = EXT_RESOLVE_PROFILE_ALIAS;
    resolved.member = &member;
    snprintf(resolved.package_guest_path, sizeof(resolved.package_guest_path), "%s", "gwy/jjfb.mrp");

    if (module_registry_register_resolved(&reg, &resolved, NULL, &err) != L_OK) return 1;
    m = module_registry_find(&reg, "cfunction.ext");
    if (!m || m->state != GWY_MODULE_RESOLVED) return 1;
    if (strcmp(m->resolved_name, "robotol.ext") != 0) return 1;
    if (reg.count != 1) return 1;

    /* Illegal skip EXTRACTED → ENTRY_CALLED */
    if (module_registry_set_state(&reg, m->module_id, GWY_MODULE_ENTRY_CALLED, 0, &err) == L_OK)
        return 1;

    if (module_registry_set_state(&reg, m->module_id, GWY_MODULE_EXTRACTED, 0, &err) != L_OK)
        return 1;
    m = module_registry_find_by_id(&reg, m->module_id);
    if (!m || m->state != GWY_MODULE_EXTRACTED) return 1;

    /* Illegal EXTRACTED → REGISTERED */
    if (module_registry_set_registered(&reg, m->module_id, 0x1000, 0x1008, &err) == L_OK)
        return 1;

    memset(&map, 0, sizeof(map));
    map.guest_code_base = 0x200000;
    map.guest_code_size = 253420;
    map.guest_rw_base = 0x300000;
    map.guest_rw_size = 4096;
    if (module_registry_set_mapping(&reg, m->module_id, &map, &err) != L_OK) return 1;
    m = module_registry_find_by_id(&reg, m->module_id);
    if (!m || m->state != GWY_MODULE_MAPPED) return 1;
    if (module_registry_find_by_code_addr(&reg, 0x200010) != m) return 1;

    if (module_registry_set_registered(&reg, m->module_id, 0x200100, 0x200100, &err) != L_OK)
        return 1;
    m = module_registry_find_by_helper(&reg, 0x200100);
    if (!m || m->state != GWY_MODULE_REGISTERED) return 1;
    /* header_entry_candidate must remain image+8, not collapse to helper */
    if (m->entries.header_entry_candidate != 0x200008) {
        fprintf(stderr, "header_entry_candidate=0x%X expected 0x200008\n",
                m->entries.header_entry_candidate);
        return 1;
    }
    if (m->entries.registered_helper != 0x200100) return 1;
    if (m->map.entry_address != m->entries.header_entry_candidate) return 1;
    if (m->map.helper_address != m->entries.registered_helper) return 1;
    if (strcmp(gwy_address_evidence_name(m->entries.header_evidence), "DOCUMENTED") != 0)
        return 1;
    if (gwy_guest_pc_norm(0x200101) != 0x200100) return 1;

    if (module_registry_set_chunk_field_04(&reg, m->module_id, 0x200009, &err) != L_OK)
        return 1;
    m = module_registry_find_by_id(&reg, m->module_id);
    if (!m || gwy_guest_pc_norm(m->entries.chunk_field_04) !=
                  gwy_guest_pc_norm(m->entries.header_entry_candidate))
        return 1;

    if (module_registry_set_dsm_direct_target(&reg, m->module_id, 0x200100, &err) != L_OK)
        return 1;
    m = module_registry_find_by_id(&reg, m->module_id);
    if (!m || m->entries.dsm_direct_target != 0x200100) return 1;
    if (m->entries.direct_target_relation != GWY_ENTRY_REL_DIRECT_TARGET_IS_HELPER) return 1;

    if (module_registry_set_observed_first_pc(&reg, m->module_id, 0x200200, &err) != L_OK)
        return 1;
    m = module_registry_find_by_id(&reg, m->module_id);
    if (!m || m->entries.observed_first_pc != 0x200200) return 1;

    if (module_registry_set_entry_called(&reg, m->module_id, 0, 1, &err) != L_OK) return 1;
    m = module_registry_find_by_id(&reg, m->module_id);
    if (!m || m->state != GWY_MODULE_ENTRY_CALLED) return 1;

    {
        GwyModuleFailInfo fi;
        memset(&fi, 0, sizeof(fi));
        snprintf(fi.fail_stage, sizeof(fi.fail_stage), "%s", "ENTRY_EXECUTION");
        snprintf(fi.fail_access, sizeof(fi.fail_access), "%s", "UC_MEM_READ_UNMAPPED");
        fi.fault_pc = 0x200200;
        fi.module_offset = 0x200;
        fi.invalid_address = 0x28;
        fi.access_size = 4;
        fi.method = 0;
        fi.caller_pc = 0x100;
        fi.base_reg_index = 0;
        fi.base_reg_value = 0;
        if (module_registry_set_entry_failed(&reg, m->module_id, &fi, &err) != L_OK) return 1;
        m = module_registry_find_by_id(&reg, m->module_id);
        if (!m || m->state != GWY_MODULE_FAILED) return 1;
        if (m->fault_invalid_address != 0x28 || m->fault_module_offset != 0x200) return 1;
        if (strcmp(m->fail_stage, "ENTRY_EXECUTION") != 0) return 1;
    }

    /* Reset for FAILED path */
    module_registry_reset(&reg);
    if (reg.count != 0) return 1;
    if (strcmp(reg.view_backend, "generated_member_view") != 0) return 1;
    if (strcmp(reg.view_reason, "scoped_member_alias") != 0) return 1;

    if (module_registry_discover(&reg, "x.ext", &err) != L_OK) return 1;
    m = module_registry_find(&reg, "x.ext");
    if (module_registry_set_state(&reg, m->module_id, GWY_MODULE_FAILED, 42, &err) != L_OK)
        return 1;
    m = module_registry_find_by_id(&reg, m->module_id);
    if (!m || m->state != GWY_MODULE_FAILED || m->error_code != 42) return 1;
    if (module_registry_set_state(&reg, m->module_id, GWY_MODULE_RESOLVED, 0, &err) == L_OK)
        return 1;

    puts("test_module_registry OK");
    return 0;
}
