#ifndef GWY_LAUNCHER_EXT_MODULE_DATA_INIT_H
#define GWY_LAUNCHER_EXT_MODULE_DATA_INIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 6B-A10: EXT Module Data Initialization Contract (observe-only). */

typedef enum GwyExtLayoutEvidence {
    GWY_LAYOUT_EV_UNKNOWN = 0,
    GWY_LAYOUT_EV_DOCUMENTED = 1,
    GWY_LAYOUT_EV_FORMAT_CROSS_TARGET = 2,
    GWY_LAYOUT_EV_DISASM_DERIVED = 3,
    GWY_LAYOUT_EV_OBSERVED = 4
} GwyExtLayoutEvidence;

typedef struct ExtImageLayout {
    uint32_t image_size;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t rw_template_offset;
    uint32_t rw_template_size;
    uint32_t bss_size;
    uint32_t relocation_offset;
    uint32_t relocation_count;
    GwyExtLayoutEvidence image_size_ev;
    GwyExtLayoutEvidence code_ev;
    GwyExtLayoutEvidence rw_template_ev;
    GwyExtLayoutEvidence bss_ev;
    GwyExtLayoutEvidence relocation_ev;
} ExtImageLayout;

typedef enum ExtDataInitClass {
    DATA_INIT_UNKNOWN = 0,
    DATA_INIT_MISSING_RW_TEMPLATE_INITIALIZATION = 1,
    DATA_INIT_MISSING_BSS_INITIALIZATION = 2,
    DATA_INIT_MISSING_DATA_RELOCATION = 3,
    DATA_INIT_MISSING_MODULE_R9_SWITCH = 4,
    DATA_INIT_REGISTRY_DATA_METADATA_MISSING = 5,
    DATA_INIT_MISSING_PRE_ENTRY_INITIALIZATION = 6
} ExtDataInitClass;

void ext_module_data_init_reset(void);
int ext_module_data_init_enabled(void);
void ext_module_data_init_bind_uc(void *uc);

const char *ext_data_init_class_name(ExtDataInitClass c);
ExtDataInitClass ext_module_data_init_last_class(void);

void ext_module_data_init_on_code_image(uint32_t guest_addr, uint32_t size);
void ext_module_data_init_on_alloc(uint32_t guest_addr, uint32_t size);
void ext_module_data_init_on_block_copy(uint32_t dst, uint32_t src, uint32_t len);
void ext_module_data_init_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                         uint32_t rw_base, uint32_t rw_size);
void ext_module_data_init_on_registered(uint64_t module_id, uint32_t helper);
void ext_module_data_init_on_mr_helper(uint32_t helper, uint32_t code, uint32_t p_guest,
                                       uint32_t r9);

void ext_module_data_init_on_second_hop(void *uc, uint32_t caller_pc, uint32_t target, uint32_t r0,
                                        uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9,
                                        uint64_t from_mid, uint64_t to_mid,
                                        const char *target_relation);

void ext_module_data_init_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                  uint32_t cpsr);

void ext_module_data_init_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr, uint32_t r0,
                                       uint32_t r9);

void ext_module_data_init_on_entry_begin(uint32_t helper, uint32_t er_rw, uint32_t r9);

#ifdef __cplusplus
}
#endif

#endif
