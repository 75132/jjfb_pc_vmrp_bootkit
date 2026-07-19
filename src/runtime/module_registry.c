#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/ext_callback_frame.h"
#include "gwy_launcher/ext_cfunction_publication_audit.h"
#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_er_rw_producer.h"
#include <stdio.h>
#include <string.h>

const char *gwy_module_state_name(GwyModuleState s) {
    switch (s) {
    case GWY_MODULE_DISCOVERED: return "DISCOVERED";
    case GWY_MODULE_RESOLVED: return "RESOLVED";
    case GWY_MODULE_EXTRACTED: return "EXTRACTED";
    case GWY_MODULE_MAPPED: return "MAPPED";
    case GWY_MODULE_REGISTERED: return "REGISTERED";
    case GWY_MODULE_ENTRY_CALLED: return "ENTRY_CALLED";
    case GWY_MODULE_FAILED: return "FAILED";
    default: return "UNKNOWN";
    }
}

const char *gwy_module_origin_name(ModuleOrigin o) {
    switch (o) {
    case MODULE_ORIGIN_DSM: return "DSM";
    case MODULE_ORIGIN_MRP_MEMBER: return "MRP_MEMBER";
    default: return "UNKNOWN";
    }
}

const char *gwy_address_evidence_name(GwyAddressEvidence e) {
    switch (e) {
        case GWY_ADDR_EV_DOCUMENTED: return "DOCUMENTED";
        case GWY_ADDR_EV_OBSERVED: return "OBSERVED";
        case GWY_ADDR_EV_CROSS_TARGET: return "CROSS_TARGET";
        default: return "UNKNOWN";
    }
}

const char *gwy_er_rw_source_name(GwyErRwSource s) {
    switch (s) {
        case GWY_ER_RW_SRC_LOADER_DOCUMENTED: return "LOADER_DOCUMENTED";
        case GWY_ER_RW_SRC_MR_C_FUNCTION_LOAD: return "MR_C_FUNCTION_LOAD";
        case GWY_ER_RW_SRC_EXT_RUNTIME_DESCRIPTOR: return "EXT_RUNTIME_DESCRIPTOR";
        case GWY_ER_RW_SRC_CROSS_TARGET: return "CROSS_TARGET";
        default: return "UNKNOWN";
    }
}

const char *gwy_entry_relation_name(GwyEntryRelation r) {
    switch (r) {
    case GWY_ENTRY_REL_DIRECT_MATCH: return "DIRECT_MATCH";
    case GWY_ENTRY_REL_THUMB_NORMALIZED_MATCH: return "THUMB_NORMALIZED_MATCH";
    case GWY_ENTRY_REL_TRAMPOLINE_TO_ENTRY: return "TRAMPOLINE_TO_ENTRY";
    case GWY_ENTRY_REL_TRAMPOLINE_TO_HELPER: return "TRAMPOLINE_TO_HELPER";
    case GWY_ENTRY_REL_HEADER_ENTRY_WRONG: return "HEADER_ENTRY_WRONG";
    case GWY_ENTRY_REL_CHUNK_ENTRY_CORRUPT: return "CHUNK_ENTRY_CORRUPT";
    case GWY_ENTRY_REL_DIRECT_TARGET_IS_HELPER: return "DIRECT_TARGET_IS_HELPER";
    case GWY_ENTRY_REL_DIRECT_TARGET_IS_INTERNAL_ENTRY: return "DIRECT_TARGET_IS_INTERNAL_ENTRY";
    case GWY_ENTRY_REL_DIRECT_TARGET_IS_TRAMPOLINE: return "DIRECT_TARGET_IS_TRAMPOLINE";
    case GWY_ENTRY_REL_DIRECT_TARGET_UNKNOWN: return "DIRECT_TARGET_UNKNOWN";
    case GWY_ENTRY_REL_CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW:
        return "CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW";
    default: return "UNKNOWN";
    }
}

static void apply_header_entry_from_image(GwyLoadedModule *m) {
    if (!m || m->map.guest_code_base == 0) return;
    m->entries.image_base = m->map.guest_code_base;
    /* DOCUMENTED candidate only: image+8 (fixR9 / TestCom 800). Not assumed DSM target. */
    if (m->entries.header_entry_candidate == 0) {
        m->entries.header_entry_candidate = m->map.guest_code_base + 8u;
        m->entries.header_evidence = GWY_ADDR_EV_DOCUMENTED;
    }
    m->map.entry_address = m->entries.header_entry_candidate;
}

static void log_module(const GwyLoadedModule *m) {
    const char *pkg;
    const char *mod;
    if (!m) return;
    pkg = m->package_path[0] ? m->package_path : "?";
    if (strstr(pkg, "jjfb.mrp")) pkg = "gwy/jjfb.mrp";
    else if (strstr(pkg, "wxjwq.mrp")) pkg = "gwy/wxjwq.mrp";
    else if (strstr(pkg, "gbrwcore.mrp")) pkg = "gwy/gbrwcore.mrp";
    else if (strstr(pkg, "gamelist.mrp")) pkg = "gwy/gamelist.mrp";
    mod = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    printf("[MODULE_REGISTRY] package=%s module=%s module_id=%llu requested=%s resolved=%s "
           "origin=%s strategy=%s state=%s stored_size=%u unpacked_size=%u extracted_size=%u "
           "helper=0x%X\n",
           pkg, mod, (unsigned long long)m->module_id, m->requested_name, m->resolved_name,
           gwy_module_origin_name(m->origin), ext_resolve_strategy_name(m->strategy),
           gwy_module_state_name(m->state), m->stored_size, m->unpacked_size, m->extracted_size,
           m->map.helper_address);
    fflush(stdout);
}

static int state_can_advance(GwyModuleState cur, GwyModuleState next) {
    if (next == GWY_MODULE_FAILED) return 1;
    if (cur == GWY_MODULE_FAILED) return 0;
    if (next == cur) return 1;
    switch (cur) {
    case GWY_MODULE_DISCOVERED:
        /* Allow skip RESOLVED when decode is available in the same step. */
        return next == GWY_MODULE_RESOLVED || next == GWY_MODULE_EXTRACTED ||
               next == GWY_MODULE_FAILED;
    case GWY_MODULE_RESOLVED:
        return next == GWY_MODULE_EXTRACTED || next == GWY_MODULE_FAILED;
    case GWY_MODULE_EXTRACTED:
        return next == GWY_MODULE_MAPPED || next == GWY_MODULE_FAILED;
    case GWY_MODULE_MAPPED:
        return next == GWY_MODULE_REGISTERED || next == GWY_MODULE_FAILED;
    case GWY_MODULE_REGISTERED:
        return next == GWY_MODULE_ENTRY_CALLED || next == GWY_MODULE_FAILED;
    case GWY_MODULE_ENTRY_CALLED:
        return 0;
    default:
        return 0;
    }
}

static GwyLoadedModule *module_mut(ModuleRegistry *reg, uint64_t module_id) {
    size_t i;
    if (!reg) return NULL;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].module_id == module_id) return &reg->modules[i];
    }
    return NULL;
}

void module_registry_init(ModuleRegistry *reg,
                          const char *package_path,
                          const char *package_sha256_hex) {
    if (!reg) return;
    memset(reg, 0, sizeof(*reg));
    reg->next_module_id = 1;
    if (package_path) {
        snprintf(reg->package_path, sizeof(reg->package_path), "%s", package_path);
        snprintf(reg->logical_package, sizeof(reg->logical_package), "%s", package_path);
    }
    if (package_sha256_hex) {
        snprintf(reg->package_sha256_hex, sizeof(reg->package_sha256_hex), "%s",
                 package_sha256_hex);
    }
}

void module_registry_reset(ModuleRegistry *reg) {
    char path[1024];
    char sha[65];
    char logical[1024];
    char backend[64];
    char reason[64];
    if (!reg) return;
    snprintf(path, sizeof(path), "%s", reg->package_path);
    snprintf(sha, sizeof(sha), "%s", reg->package_sha256_hex);
    snprintf(logical, sizeof(logical), "%s", reg->logical_package);
    snprintf(backend, sizeof(backend), "%s", reg->view_backend);
    snprintf(reason, sizeof(reason), "%s", reg->view_reason);
    module_registry_init(reg, path, sha);
    snprintf(reg->logical_package, sizeof(reg->logical_package), "%s", logical);
    snprintf(reg->view_backend, sizeof(reg->view_backend), "%s", backend);
    snprintf(reg->view_reason, sizeof(reg->view_reason), "%s", reason);
}

void module_registry_set_provenance(ModuleRegistry *reg,
                                    const char *logical_package,
                                    const char *view_backend,
                                    const char *view_reason) {
    if (!reg) return;
    if (logical_package) {
        snprintf(reg->logical_package, sizeof(reg->logical_package), "%s", logical_package);
    }
    if (view_backend) {
        snprintf(reg->view_backend, sizeof(reg->view_backend), "%s", view_backend);
    }
    if (view_reason) {
        snprintf(reg->view_reason, sizeof(reg->view_reason), "%s", view_reason);
    }
}

LauncherStatus module_registry_discover(ModuleRegistry *reg,
                                        const char *requested_name,
                                        LauncherError *err) {
    GwyLoadedModule *m;
    launcher_error_clear(err);
    if (!reg || !requested_name || !requested_name[0]) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (module_registry_find(reg, requested_name)) {
        return L_OK;
    }
    if (reg->count >= GWY_MODULE_MAX) {
        launcher_error_set(err, L_ERR_BOUNDS, "module_registry", "registry full", requested_name);
        return L_ERR_BOUNDS;
    }
    m = &reg->modules[reg->count++];
    memset(m, 0, sizeof(*m));
    m->module_id = reg->next_module_id++;
    snprintf(m->package_path, sizeof(m->package_path), "%s", reg->package_path);
    snprintf(m->requested_name, sizeof(m->requested_name), "%s", requested_name);
    m->origin = MODULE_ORIGIN_MRP_MEMBER;
    m->state = GWY_MODULE_DISCOVERED;
    log_module(m);
    return L_OK;
}

LauncherStatus module_registry_register_resolved(ModuleRegistry *reg,
                                                 const ExtResolvedMember *resolved,
                                                 const ByteBuffer *decoded_opt,
                                                 LauncherError *err) {
    GwyLoadedModule *m;
    const GwyLoadedModule *existing;
    launcher_error_clear(err);
    if (!reg || !resolved || !resolved->member) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }

    existing = module_registry_find(reg, resolved->requested_name);
    if (existing) {
        size_t i;
        for (i = 0; i < reg->count; i++) {
            if (reg->modules[i].module_id == existing->module_id) {
                m = &reg->modules[i];
                snprintf(m->resolved_name, sizeof(m->resolved_name), "%s", resolved->resolved_name);
                m->strategy = resolved->strategy;
                if (m->origin == MODULE_ORIGIN_UNKNOWN) {
                    m->origin = MODULE_ORIGIN_MRP_MEMBER;
                }
                m->stored_size = resolved->member->stored_size;
                m->unpacked_size = decoded_opt ? (uint32_t)decoded_opt->size
                                               : (resolved->member->unpacked_known
                                                      ? resolved->member->unpacked_size
                                                      : 0);
                m->extracted_size = decoded_opt ? (uint32_t)decoded_opt->size : 0;
                m->compression = resolved->member->compression;
                if (decoded_opt) {
                    if (!state_can_advance(m->state, GWY_MODULE_EXTRACTED) &&
                        m->state != GWY_MODULE_EXTRACTED) {
                        launcher_error_set(err, L_ERR_STATE, "module_registry",
                                           "illegal state advance", m->requested_name);
                        return L_ERR_STATE;
                    }
                    m->state = GWY_MODULE_EXTRACTED;
                } else {
                    if (!state_can_advance(m->state, GWY_MODULE_RESOLVED) &&
                        m->state != GWY_MODULE_RESOLVED &&
                        m->state != GWY_MODULE_DISCOVERED) {
                        launcher_error_set(err, L_ERR_STATE, "module_registry",
                                           "illegal state advance", m->requested_name);
                        return L_ERR_STATE;
                    }
                    m->state = GWY_MODULE_RESOLVED;
                }
                log_module(m);
                return L_OK;
            }
        }
    }

    if (reg->count >= GWY_MODULE_MAX) {
        launcher_error_set(err, L_ERR_BOUNDS, "module_registry", "registry full",
                           resolved->resolved_name);
        return L_ERR_BOUNDS;
    }
    m = &reg->modules[reg->count];
    memset(m, 0, sizeof(*m));
    m->module_id = reg->next_module_id++;
    snprintf(m->package_path, sizeof(m->package_path), "%s",
             resolved->package_guest_path[0] ? resolved->package_guest_path : reg->package_path);
    snprintf(m->requested_name, sizeof(m->requested_name), "%s", resolved->requested_name);
    snprintf(m->resolved_name, sizeof(m->resolved_name), "%s", resolved->resolved_name);
    m->strategy = resolved->strategy;
    m->origin = MODULE_ORIGIN_MRP_MEMBER;
    m->stored_size = resolved->member->stored_size;
    m->unpacked_size = decoded_opt ? (uint32_t)decoded_opt->size
                                   : (resolved->member->unpacked_known ? resolved->member->unpacked_size
                                                                      : 0);
    m->extracted_size = decoded_opt ? (uint32_t)decoded_opt->size : 0;
    m->compression = resolved->member->compression;
    m->state = decoded_opt ? GWY_MODULE_EXTRACTED : GWY_MODULE_RESOLVED;
    reg->count++;
    log_module(m);
    return L_OK;
}

LauncherStatus module_registry_set_origin(ModuleRegistry *reg,
                                          uint64_t module_id,
                                          ModuleOrigin origin,
                                          LauncherError *err) {
    GwyLoadedModule *m;
    launcher_error_clear(err);
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    m->origin = origin;
    log_module(m);
    return L_OK;
}

LauncherStatus module_registry_set_state(ModuleRegistry *reg,
                                         uint64_t module_id,
                                         GwyModuleState next,
                                         int error_code,
                                         LauncherError *err) {
    GwyLoadedModule *m;
    launcher_error_clear(err);
    if (!reg) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    if (!state_can_advance(m->state, next)) {
        printf("[MODULE_REGISTRY] module_id=%llu state=FAILED stage=%s error=illegal_transition→%s\n",
               (unsigned long long)m->module_id, gwy_module_state_name(m->state),
               gwy_module_state_name(next));
        fflush(stdout);
        launcher_error_set(err, L_ERR_STATE, "module_registry", "illegal state transition",
                           m->requested_name);
        return L_ERR_STATE;
    }
    m->state = next;
    if (next == GWY_MODULE_FAILED) m->error_code = error_code;
    log_module(m);
    return L_OK;
}

LauncherStatus module_registry_set_mapping(ModuleRegistry *reg,
                                           uint64_t module_id,
                                           const GwyModuleMapping *mapping,
                                           LauncherError *err) {
    GwyLoadedModule *m;
    launcher_error_clear(err);
    if (!reg || !mapping) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    if (m->state != GWY_MODULE_EXTRACTED && m->state != GWY_MODULE_MAPPED) {
        printf("[MODULE_REGISTRY] module_id=%llu state=FAILED stage=%s error=set_mapping_requires_EXTRACTED\n",
               (unsigned long long)m->module_id, gwy_module_state_name(m->state));
        fflush(stdout);
        launcher_error_set(err, L_ERR_STATE, "module_registry", "set_mapping requires EXTRACTED",
                           m->requested_name);
        return L_ERR_STATE;
    }
    m->map = *mapping;
    apply_header_entry_from_image(m);
    if (m->state == GWY_MODULE_EXTRACTED) {
        m->state = GWY_MODULE_MAPPED;
    }
    printf("[EXT_MAP] module_id=%llu code_base=0x%X code_size=%u rw_base=0x%X rw_size=%u "
           "stack_base=0x%X stack_size=%u header_entry_candidate=0x%X result=OK\n",
           (unsigned long long)m->module_id, m->map.guest_code_base, m->map.guest_code_size,
           m->map.guest_rw_base, m->map.guest_rw_size, m->map.guest_stack_base,
           m->map.guest_stack_size, m->entries.header_entry_candidate);
    fflush(stdout);
    log_module(m);
    return L_OK;
}

LauncherStatus module_registry_set_er_rw(ModuleRegistry *reg,
                                         uint64_t module_id,
                                         uint32_t start_of_er_rw,
                                         uint32_t er_rw_size,
                                         GwyErRwSource source,
                                         GwyAddressEvidence evidence,
                                         LauncherError *err) {
    GwyLoadedModule *m;
    const char *mod;
    launcher_error_clear(err);
    if (!reg) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (!start_of_er_rw) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "zero start_of_er_rw",
                           NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    m->data.start_of_er_rw = start_of_er_rw;
    m->data.er_rw_size = er_rw_size;
    m->data.er_rw_source = source;
    m->data.er_rw_evidence = evidence;
    if (!m->data.entry_switch_attempted) m->data.available_before_entry = 1;
    /* Keep legacy map in sync for observers that still read guest_rw_*. */
    m->map.guest_rw_base = start_of_er_rw;
    m->map.guest_rw_size = er_rw_size;
    mod = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    printf("[MODULE_ER_RW] module=%s module_id=%llu base=0x%X size=%u source=%s evidence=%s "
           "available_before_entry=%s note=no_hardcoded_address\n",
           mod, (unsigned long long)m->module_id, start_of_er_rw, er_rw_size,
           gwy_er_rw_source_name(source), gwy_address_evidence_name(evidence),
           m->data.available_before_entry ? "yes" : "no");
    fflush(stdout);
    ext_er_rw_producer_on_er_rw_publish(m->module_id, start_of_er_rw, er_rw_size, source);
    return L_OK;
}

LauncherStatus module_registry_set_registered(ModuleRegistry *reg,
                                              uint64_t module_id,
                                              uint32_t helper_address,
                                              uint32_t entry_address,
                                              LauncherError *err) {
    GwyLoadedModule *m;
    launcher_error_clear(err);
    if (!reg) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    if (m->state != GWY_MODULE_MAPPED && m->state != GWY_MODULE_REGISTERED) {
        printf("[MODULE_REGISTRY] module_id=%llu state=FAILED stage=%s error=set_registered_requires_MAPPED\n",
               (unsigned long long)m->module_id, gwy_module_state_name(m->state));
        fflush(stdout);
        launcher_error_set(err, L_ERR_STATE, "module_registry", "set_registered requires MAPPED",
                           m->requested_name);
        return L_ERR_STATE;
    }
    m->map.helper_address = helper_address;
    m->entries.registered_helper = helper_address;
    m->entries.helper_evidence = GWY_ADDR_EV_OBSERVED;
    /* Keep header_entry_candidate from image+8; do not collapse to helper. */
    if (m->entries.header_entry_candidate == 0 && m->map.guest_code_base) {
        apply_header_entry_from_image(m);
    }
    /* Legacy entry_address arg ignored when it equals helper (old collapse pattern). */
    if (entry_address && entry_address != helper_address && m->entries.header_entry_candidate == 0) {
        m->entries.header_entry_candidate = entry_address;
        m->entries.header_evidence = GWY_ADDR_EV_OBSERVED;
    }
    m->map.entry_address = m->entries.header_entry_candidate;
    if (m->state == GWY_MODULE_MAPPED) {
        m->state = GWY_MODULE_REGISTERED;
    }
    printf("[EXT_REGISTER] module_id=%llu helper=0x%X header_entry_candidate=0x%X "
           "chunk_field_04=0x%X result=OK\n",
           (unsigned long long)m->module_id, m->entries.registered_helper,
           m->entries.header_entry_candidate, m->entries.chunk_field_04);
    fflush(stdout);
    {
        const char *mn =
            m->resolved_name[0] ? m->resolved_name : (m->requested_name[0] ? m->requested_name : "?");
        ext_cfunction_publication_audit_on_ext_register(
            mn, m->module_id, m->entries.registered_helper, m->entries.header_entry_candidate,
            m->entries.chunk_field_04);
        ext_chunk_provider_on_module_registered(mn, m->module_id, m->entries.registered_helper,
                                                m->map.guest_code_base, m->map.guest_code_size, 0,
                                                NULL);
    }
    log_module(m);
    return L_OK;
}

LauncherStatus module_registry_set_chunk_field_04(ModuleRegistry *reg,
                                                  uint64_t module_id,
                                                  uint32_t chunk_field_04,
                                                  LauncherError *err) {
    GwyLoadedModule *m;
    launcher_error_clear(err);
    if (!reg) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    {
        uint32_t old_v = m->entries.chunk_field_04;
        const char *mn =
            m->resolved_name[0] ? m->resolved_name : (m->requested_name[0] ? m->requested_name : "?");
        m->entries.chunk_field_04 = chunk_field_04;
        m->entries.chunk_field_evidence = GWY_ADDR_EV_OBSERVED;
        ext_cfunction_publication_audit_on_chunk_field04(mn, old_v, chunk_field_04, 0,
                                                         "module_registry_set");
    }
    return L_OK;
}

LauncherStatus module_registry_set_dsm_direct_target(ModuleRegistry *reg,
                                                    uint64_t module_id,
                                                    uint32_t target,
                                                    LauncherError *err) {
    GwyLoadedModule *m;
    uint32_t t = gwy_guest_pc_norm(target);
    GwyEntryRelation rel = GWY_ENTRY_REL_DIRECT_TARGET_UNKNOWN;
    launcher_error_clear(err);
    if (!reg) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    m->entries.dsm_direct_target = target;
    if (m->entries.registered_helper && t == gwy_guest_pc_norm(m->entries.registered_helper))
        rel = GWY_ENTRY_REL_DIRECT_TARGET_IS_HELPER;
    else if (ext_callback_frame_is_cfn_continuation(t, NULL, NULL))
        rel = GWY_ENTRY_REL_CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW;
    else if (m->entries.observed_first_pc && t == gwy_guest_pc_norm(m->entries.observed_first_pc))
        rel = GWY_ENTRY_REL_DIRECT_TARGET_IS_INTERNAL_ENTRY;
    else if (t && m->map.guest_code_base && t >= m->map.guest_code_base &&
             t < m->map.guest_code_base + m->map.guest_code_size)
        rel = GWY_ENTRY_REL_DIRECT_TARGET_IS_INTERNAL_ENTRY;
    /* 6C-D1: helper≠target inside module code after CFN → prefer continuation over internal_entry. */
    if (rel == GWY_ENTRY_REL_DIRECT_TARGET_IS_INTERNAL_ENTRY &&
        m->entries.registered_helper &&
        t != gwy_guest_pc_norm(m->entries.registered_helper) &&
        ext_callback_frame_enabled()) {
        rel = GWY_ENTRY_REL_CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW;
    }
    m->entries.direct_target_relation = rel;
    printf("[DSM_DIRECT_TARGET] module_id=%llu module=%s target=0x%X relation=%s "
           "helper=0x%X observed_first=0x%X evidence=OBSERVED",
           (unsigned long long)m->module_id,
           m->resolved_name[0] ? m->resolved_name : m->requested_name, target,
           gwy_entry_relation_name(rel), m->entries.registered_helper,
           m->entries.observed_first_pc);
    if (rel == GWY_ENTRY_REL_CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW) {
        printf(" superseded_by_callback_continuation_identity=yes causal=false"
               " note=guest_callback_continuation_not_module_entry");
    }
    printf("\n");
    fflush(stdout);
    return L_OK;
}

LauncherStatus module_registry_set_observed_first_pc(ModuleRegistry *reg,
                                                     uint64_t module_id,
                                                     uint32_t first_pc,
                                                     LauncherError *err) {
    GwyLoadedModule *m;
    uint32_t pc = gwy_guest_pc_norm(first_pc);
    const char *cls = "neither";
    launcher_error_clear(err);
    if (!reg) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    if (m->entries.observed_first_pc == 0) {
        m->entries.observed_first_pc = pc;
    }
    if (m->entries.header_entry_candidate &&
        pc == gwy_guest_pc_norm(m->entries.header_entry_candidate))
        cls = "header_entry_candidate";
    else if (m->entries.chunk_field_04 && pc == gwy_guest_pc_norm(m->entries.chunk_field_04))
        cls = "chunk_field_04";
    else if (m->entries.registered_helper && pc == gwy_guest_pc_norm(m->entries.registered_helper))
        cls = "registered_helper";
    printf("[EXT_ENTRY_POINTS] module_id=%llu image_base=0x%X header_entry_candidate=0x%X "
           "chunk_field_04=0x%X registered_helper=0x%X observed_first_pc=0x%X "
           "match=%s header_ev=%s helper_ev=%s chunk_ev=%s relation=%s\n",
           (unsigned long long)m->module_id, m->entries.image_base,
           m->entries.header_entry_candidate, m->entries.chunk_field_04,
           m->entries.registered_helper, m->entries.observed_first_pc, cls,
           gwy_address_evidence_name(m->entries.header_evidence),
           gwy_address_evidence_name(m->entries.helper_evidence),
           gwy_address_evidence_name(m->entries.chunk_field_evidence),
           gwy_entry_relation_name(m->entries.relation));
    fflush(stdout);
    return L_OK;
}

LauncherStatus module_registry_set_entry_called(ModuleRegistry *reg,
                                                uint64_t module_id,
                                                uint32_t method,
                                                int32_t ret_value,
                                                LauncherError *err) {
    GwyLoadedModule *m;
    launcher_error_clear(err);
    if (!reg) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    if (m->state != GWY_MODULE_REGISTERED && m->state != GWY_MODULE_ENTRY_CALLED) {
        printf("[MODULE_REGISTRY] module_id=%llu state=FAILED stage=%s error=entry_requires_REGISTERED\n",
               (unsigned long long)m->module_id, gwy_module_state_name(m->state));
        fflush(stdout);
        launcher_error_set(err, L_ERR_STATE, "module_registry", "entry_called requires REGISTERED",
                           m->requested_name);
        return L_ERR_STATE;
    }
    if (m->state == GWY_MODULE_REGISTERED) {
        m->state = GWY_MODULE_ENTRY_CALLED;
    }
    printf("[EXT_ENTRY] module_id=%llu method=%u return=%d\n",
           (unsigned long long)m->module_id, (unsigned)method, (int)ret_value);
    fflush(stdout);
    log_module(m);
    return L_OK;
}

LauncherStatus module_registry_set_entry_failed(ModuleRegistry *reg,
                                                uint64_t module_id,
                                                const GwyModuleFailInfo *fail_info,
                                                LauncherError *err) {
    GwyLoadedModule *m;
    launcher_error_clear(err);
    if (!reg) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    m = module_mut(reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "module_registry", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    if (m->state != GWY_MODULE_ENTRY_CALLED && m->state != GWY_MODULE_REGISTERED &&
        m->state != GWY_MODULE_FAILED) {
        launcher_error_set(err, L_ERR_STATE, "module_registry",
                           "entry_failed requires ENTRY_CALLED or REGISTERED", m->requested_name);
        return L_ERR_STATE;
    }
    if (m->state != GWY_MODULE_FAILED) {
        m->state = GWY_MODULE_FAILED;
    }
    m->error_code = (int)L_ERR_GUEST_FAULT;
    if (fail_info) {
        snprintf(m->fail_stage, sizeof(m->fail_stage), "%s", fail_info->fail_stage);
        snprintf(m->fail_access, sizeof(m->fail_access), "%s", fail_info->fail_access);
        m->fault_pc = fail_info->fault_pc;
        m->fault_module_offset = fail_info->module_offset;
        m->fault_invalid_address = fail_info->invalid_address;
        m->fault_access_size = fail_info->access_size;
        m->fault_method = fail_info->method;
        m->fault_caller_pc = fail_info->caller_pc;
        m->fault_base_reg_index = fail_info->base_reg_index;
        m->fault_base_reg_value = fail_info->base_reg_value;
    }
    printf("[EXT_FAIL] module_id=%llu stage=%s access=%s fault_pc=0x%X module_offset=0x%X "
           "invalid_address=0x%X access_size=%u method=%u caller_pc=0x%X base_reg=r%d "
           "base_value=0x%X\n",
           (unsigned long long)m->module_id, m->fail_stage[0] ? m->fail_stage : "?",
           m->fail_access[0] ? m->fail_access : "?", m->fault_pc, m->fault_module_offset,
           m->fault_invalid_address, m->fault_access_size, m->fault_method, m->fault_caller_pc,
           m->fault_base_reg_index, m->fault_base_reg_value);
    fflush(stdout);
    log_module(m);
    return L_OK;
}

const GwyLoadedModule *module_registry_find(const ModuleRegistry *reg,
                                            const char *resolved_or_requested_name) {
    size_t i;
    if (!reg || !resolved_or_requested_name) return NULL;
    for (i = 0; i < reg->count; i++) {
        if (strcmp(reg->modules[i].resolved_name, resolved_or_requested_name) == 0 ||
            strcmp(reg->modules[i].requested_name, resolved_or_requested_name) == 0) {
            return &reg->modules[i];
        }
    }
    return NULL;
}

const GwyLoadedModule *module_registry_find_by_id(const ModuleRegistry *reg, uint64_t module_id) {
    size_t i;
    if (!reg) return NULL;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].module_id == module_id) return &reg->modules[i];
    }
    return NULL;
}

const GwyLoadedModule *module_registry_find_by_helper(const ModuleRegistry *reg,
                                                      uint32_t helper_address) {
    size_t i;
    if (!reg || helper_address == 0) return NULL;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].map.helper_address == helper_address) return &reg->modules[i];
    }
    return NULL;
}

const GwyLoadedModule *module_registry_find_by_code_addr(const ModuleRegistry *reg,
                                                         uint32_t guest_addr) {
    size_t i;
    if (!reg || guest_addr == 0) return NULL;
    for (i = 0; i < reg->count; i++) {
        const GwyLoadedModule *m = &reg->modules[i];
        uint32_t base = m->map.guest_code_base;
        uint32_t size = m->map.guest_code_size;
        if (base == 0 || size == 0) continue;
        if (guest_addr >= base && guest_addr < base + size) return m;
    }
    return NULL;
}

LauncherStatus module_registry_write_snapshot(const ModuleRegistry *reg,
                                              const char *path,
                                              LauncherError *err) {
    FILE *fp;
    size_t i;
    launcher_error_clear(err);
    if (!reg || !path) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "module_registry", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        launcher_error_set(err, L_ERR_IO, "module_registry", "open failed", path);
        return L_ERR_IO;
    }
    fprintf(fp,
            "{\n  \"package\": \"%s\",\n  \"logical_package\": \"%s\",\n"
            "  \"original_sha256\": \"%s\",\n  \"view_backend\": \"%s\",\n"
            "  \"view_reason\": \"%s\",\n  \"modules\": [\n",
            reg->package_path, reg->logical_package, reg->package_sha256_hex, reg->view_backend,
            reg->view_reason);
    for (i = 0; i < reg->count; i++) {
        const GwyLoadedModule *m = &reg->modules[i];
        fprintf(fp,
                "    {\"module_id\":%llu,\"requested\":\"%s\",\"resolved\":\"%s\","
                "\"origin\":\"%s\",\"strategy\":\"%s\",\"stored_size\":%u,\"unpacked_size\":%u,"
                "\"extracted_size\":%u,\"compression\":\"%s\",\"state\":\"%s\","
                "\"guest_code_base\":%u,\"guest_code_size\":%u,"
                "\"guest_rw_base\":%u,\"guest_rw_size\":%u,"
                "\"start_of_er_rw\":%u,\"er_rw_size\":%u,"
                "\"er_rw_source\":\"%s\",\"er_rw_evidence\":\"%s\","
                "\"available_before_entry\":%d,"
                "\"helper_address\":%u,\"entry_address\":%u,"
                "\"header_entry_candidate\":%u,\"chunk_field_04\":%u,\"registered_helper\":%u,"
                "\"observed_first_pc\":%u,\"dsm_direct_target\":%u,"
                "\"header_evidence\":\"%s\","
                "\"helper_evidence\":\"%s\",\"chunk_field_evidence\":\"%s\","
                "\"relation\":\"%s\",\"direct_target_relation\":\"%s\",\"error_code\":%d,"
                "\"fail_stage\":\"%s\",\"fail_access\":\"%s\","
                "\"fault_pc\":%u,\"fault_module_offset\":%u,"
                "\"fault_invalid_address\":%u,\"fault_access_size\":%u,"
                "\"fault_method\":%u,\"fault_caller_pc\":%u,"
                "\"fault_base_reg_index\":%d,\"fault_base_reg_value\":%u}%s\n",
                (unsigned long long)m->module_id, m->requested_name, m->resolved_name,
                gwy_module_origin_name(m->origin), ext_resolve_strategy_name(m->strategy),
                m->stored_size, m->unpacked_size, m->extracted_size,
                mrp_compression_name(m->compression), gwy_module_state_name(m->state),
                m->map.guest_code_base, m->map.guest_code_size, m->map.guest_rw_base,
                m->map.guest_rw_size, m->data.start_of_er_rw, m->data.er_rw_size,
                gwy_er_rw_source_name(m->data.er_rw_source),
                gwy_address_evidence_name(m->data.er_rw_evidence), m->data.available_before_entry,
                m->map.helper_address, m->map.entry_address,
                m->entries.header_entry_candidate, m->entries.chunk_field_04,
                m->entries.registered_helper, m->entries.observed_first_pc,
                m->entries.dsm_direct_target,
                gwy_address_evidence_name(m->entries.header_evidence),
                gwy_address_evidence_name(m->entries.helper_evidence),
                gwy_address_evidence_name(m->entries.chunk_field_evidence),
                gwy_entry_relation_name(m->entries.relation),
                gwy_entry_relation_name(m->entries.direct_target_relation), m->error_code,
                m->fail_stage,
                m->fail_access, m->fault_pc, m->fault_module_offset, m->fault_invalid_address,
                m->fault_access_size, m->fault_method, m->fault_caller_pc,
                m->fault_base_reg_index, m->fault_base_reg_value,
                (i + 1 < reg->count) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    return L_OK;
}
