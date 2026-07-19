#ifndef GWY_LAUNCHER_MODULE_REGISTRY_H
#define GWY_LAUNCHER_MODULE_REGISTRY_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/ext_resolver.h"
#include "gwy_launcher/mrp_archive.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GWY_MODULE_MAX 64

typedef enum GwyModuleState {
    GWY_MODULE_DISCOVERED = 1,
    GWY_MODULE_RESOLVED = 2,
    GWY_MODULE_EXTRACTED = 3,
    GWY_MODULE_MAPPED = 4,
    GWY_MODULE_REGISTERED = 5,
    /* First observed helper method dispatch — not mrc_init success. */
    GWY_MODULE_ENTRY_CALLED = 6,
    GWY_MODULE_FAILED = 7
} GwyModuleState;

typedef enum ModuleOrigin {
    MODULE_ORIGIN_UNKNOWN = 0,
    MODULE_ORIGIN_DSM = 1,
    MODULE_ORIGIN_MRP_MEMBER = 2
} ModuleOrigin;

typedef enum GwyAddressEvidence {
    GWY_ADDR_EV_UNKNOWN = 0,
    GWY_ADDR_EV_DOCUMENTED = 1,
    GWY_ADDR_EV_OBSERVED = 2,
    GWY_ADDR_EV_CROSS_TARGET = 3
} GwyAddressEvidence;

typedef enum GwyEntryRelation {
    GWY_ENTRY_REL_UNKNOWN = 0,
    GWY_ENTRY_REL_DIRECT_MATCH = 1,
    GWY_ENTRY_REL_THUMB_NORMALIZED_MATCH = 2,
    GWY_ENTRY_REL_TRAMPOLINE_TO_ENTRY = 3,
    GWY_ENTRY_REL_TRAMPOLINE_TO_HELPER = 4,
    GWY_ENTRY_REL_HEADER_ENTRY_WRONG = 5,
    GWY_ENTRY_REL_CHUNK_ENTRY_CORRUPT = 6,
    /* A5: dsm_direct_target vs helper/observed_first (observe-only). */
    GWY_ENTRY_REL_DIRECT_TARGET_IS_HELPER = 7,
    GWY_ENTRY_REL_DIRECT_TARGET_IS_INTERNAL_ENTRY = 8,
    GWY_ENTRY_REL_DIRECT_TARGET_IS_TRAMPOLINE = 9,
    GWY_ENTRY_REL_DIRECT_TARGET_UNKNOWN = 10,
    /* 6C-D1: PC is guest continuation after host _mr_c_function_new (not a module entry). */
    GWY_ENTRY_REL_CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW = 11
} GwyEntryRelation;

/*
 * EXT address concepts — never collapse helper == header/chunk field.
 * map.helper_address mirrors registered_helper.
 * map.entry_address mirrors header_entry_candidate for snapshot/compare only —
 * guest execution continues via DSM BLX / registered_helper / bridge CODE_ADDRESS+8
 * (DSM load path), not via registry-driven execute of header_entry_candidate.
 */
typedef struct ExtEntryPoints {
    uint32_t image_base;
    /* Observe/compare only — never promote to uc_emu_start / runCode PC. */
    uint32_t header_entry_candidate; /* image+8 provisional (DOCUMENTED name only) */
    uint32_t chunk_field_04;         /* raw last-seen value at [chunk+0x4] */
    uint32_t registered_helper;      /* from _mr_c_function_new */
    uint32_t observed_first_pc;      /* first guest PC after REGISTERED */
    /* Observe: last DSM BLX target that landed in this module's code. */
    uint32_t dsm_direct_target;
    GwyEntryRelation relation;
    GwyEntryRelation direct_target_relation;
    GwyAddressEvidence header_evidence;
    GwyAddressEvidence helper_evidence;
    GwyAddressEvidence chunk_field_evidence;
} ExtEntryPoints;

typedef struct GwyModuleMapping {
    uint32_t guest_code_base;
    uint32_t guest_code_size;
    uint32_t guest_rw_base;
    uint32_t guest_rw_size;
    uint32_t guest_stack_base;
    uint32_t guest_stack_size;
    /* Snapshot mirror of header_entry_candidate only — not an execute entry. */
    uint32_t entry_address;
    uint32_t helper_address; /* = registered_helper */
    int relocation_applied; /* bool */
} GwyModuleMapping;

/* Phase 6C-A: authoritative module SB / ER_RW (not RW template file offset). */
typedef enum GwyErRwSource {
    GWY_ER_RW_SRC_UNKNOWN = 0,
    GWY_ER_RW_SRC_LOADER_DOCUMENTED = 1,
    GWY_ER_RW_SRC_MR_C_FUNCTION_LOAD = 2,
    GWY_ER_RW_SRC_EXT_RUNTIME_DESCRIPTOR = 3,
    GWY_ER_RW_SRC_CROSS_TARGET = 4
} GwyErRwSource;

typedef struct ExtModuleDataContext {
    uint32_t start_of_er_rw;
    uint32_t er_rw_size;
    GwyAddressEvidence er_rw_evidence;
    GwyErRwSource er_rw_source;
    /* 1 if start_of_er_rw was registered before first MODULE_ENTRY switch attempt. */
    int available_before_entry;
    int entry_switch_attempted;
} ExtModuleDataContext;

typedef struct GwyModuleFailInfo {
    char fail_stage[32];
    char fail_access[32];
    uint32_t fault_pc;
    uint32_t module_offset;
    uint32_t invalid_address;
    uint32_t access_size;
    uint32_t method;
    uint32_t caller_pc;
    int base_reg_index;
    uint32_t base_reg_value;
} GwyModuleFailInfo;

typedef struct GwyLoadedModule {
    uint64_t module_id;
    char package_path[1024];
    char requested_name[MRP_MEMBER_NAME_MAX];
    char resolved_name[MRP_MEMBER_NAME_MAX];
    ExtResolveStrategy strategy;
    ModuleOrigin origin;
    uint32_t stored_size;
    uint32_t unpacked_size;
    uint32_t extracted_size;
    MrpCompression compression;
    GwyModuleState state;
    int error_code;
    GwyModuleMapping map;
    ExtEntryPoints entries;
    ExtModuleDataContext data;
    /* Populated on ENTRY_EXECUTION failure (observe-only). */
    char fail_stage[32];
    char fail_access[32];
    uint32_t fault_pc;
    uint32_t fault_module_offset;
    uint32_t fault_invalid_address;
    uint32_t fault_access_size;
    uint32_t fault_method;
    uint32_t fault_caller_pc;
    int fault_base_reg_index;
    uint32_t fault_base_reg_value;
} GwyLoadedModule;

typedef struct ModuleRegistry {
    GwyLoadedModule modules[GWY_MODULE_MAX];
    size_t count;
    uint64_t next_module_id;
    char package_path[1024];
    char package_sha256_hex[65]; /* original package identity */
    char logical_package[1024];
    char view_backend[64];
    char view_reason[64];
} ModuleRegistry;

void module_registry_init(ModuleRegistry *reg,
                          const char *package_path,
                          const char *package_sha256_hex);

void module_registry_reset(ModuleRegistry *reg);

void module_registry_set_provenance(ModuleRegistry *reg,
                                    const char *logical_package,
                                    const char *view_backend,
                                    const char *view_reason);

LauncherStatus module_registry_discover(ModuleRegistry *reg,
                                        const char *requested_name,
                                        LauncherError *err);

LauncherStatus module_registry_register_resolved(ModuleRegistry *reg,
                                                 const ExtResolvedMember *resolved,
                                                 const ByteBuffer *decoded_opt,
                                                 LauncherError *err);

LauncherStatus module_registry_set_origin(ModuleRegistry *reg,
                                          uint64_t module_id,
                                          ModuleOrigin origin,
                                          LauncherError *err);

LauncherStatus module_registry_set_state(ModuleRegistry *reg,
                                         uint64_t module_id,
                                         GwyModuleState next,
                                         int error_code,
                                         LauncherError *err);

/* EXTRACTED → MAPPED only. */
LauncherStatus module_registry_set_mapping(ModuleRegistry *reg,
                                           uint64_t module_id,
                                           const GwyModuleMapping *mapping,
                                           LauncherError *err);

/* MAPPED → REGISTERED only. Sets registered_helper; does not overwrite header_entry. */
LauncherStatus module_registry_set_registered(ModuleRegistry *reg,
                                              uint64_t module_id,
                                              uint32_t helper_address,
                                              uint32_t entry_address,
                                              LauncherError *err);

/*
 * Record module start_of_ER_RW (DOCUMENTED guest mr_c_function_load / host P fill).
 * Fail closed on zero base. Does not invent addresses.
 */
LauncherStatus module_registry_set_er_rw(ModuleRegistry *reg,
                                         uint64_t module_id,
                                         uint32_t start_of_er_rw,
                                         uint32_t er_rw_size,
                                         GwyErRwSource source,
                                         GwyAddressEvidence evidence,
                                         LauncherError *err);

/* Record first observed guest PC (observe-only). */
LauncherStatus module_registry_set_observed_first_pc(ModuleRegistry *reg,
                                                     uint64_t module_id,
                                                     uint32_t first_pc,
                                                     LauncherError *err);

/* Observe-only: record raw [chunk+0x4] value for this module. */
LauncherStatus module_registry_set_chunk_field_04(ModuleRegistry *reg,
                                                  uint64_t module_id,
                                                  uint32_t chunk_field_04,
                                                  LauncherError *err);

/* Observe-only: last DSM BLX target into this module + relation vs helper/first_pc. */
LauncherStatus module_registry_set_dsm_direct_target(ModuleRegistry *reg,
                                                    uint64_t module_id,
                                                    uint32_t target,
                                                    LauncherError *err);

/* Clear Thumb bit for guest PC / function-pointer compare. */
static inline uint32_t gwy_guest_pc_norm(uint32_t addr) {
    return addr & ~1u;
}

/* REGISTERED → ENTRY_CALLED only. */
LauncherStatus module_registry_set_entry_called(ModuleRegistry *reg,
                                                uint64_t module_id,
                                                uint32_t method,
                                                int32_t ret_value,
                                                LauncherError *err);

/*
 * ENTRY_CALLED (or REGISTERED) → FAILED with entry-execution fault metadata.
 * fail_info may be NULL (then only error_code is set).
 */
LauncherStatus module_registry_set_entry_failed(ModuleRegistry *reg,
                                                uint64_t module_id,
                                                const GwyModuleFailInfo *fail_info,
                                                LauncherError *err);

const GwyLoadedModule *module_registry_find(const ModuleRegistry *reg,
                                            const char *resolved_or_requested_name);

const GwyLoadedModule *module_registry_find_by_id(const ModuleRegistry *reg, uint64_t module_id);

const GwyLoadedModule *module_registry_find_by_helper(const ModuleRegistry *reg,
                                                      uint32_t helper_address);

const GwyLoadedModule *module_registry_find_by_code_addr(const ModuleRegistry *reg,
                                                         uint32_t guest_addr);

LauncherStatus module_registry_write_snapshot(const ModuleRegistry *reg,
                                              const char *path,
                                              LauncherError *err);

const char *gwy_module_state_name(GwyModuleState s);
const char *gwy_module_origin_name(ModuleOrigin o);
const char *gwy_address_evidence_name(GwyAddressEvidence e);
const char *gwy_entry_relation_name(GwyEntryRelation r);
const char *gwy_er_rw_source_name(GwyErRwSource s);

#ifdef __cplusplus
}
#endif

#endif
