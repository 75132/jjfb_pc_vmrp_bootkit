#ifndef GWY_LAUNCHER_P22_SELECTION_GATES_H
#define GWY_LAUNCHER_P22_SELECTION_GATES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P22–P27 selection / cfg state-machine gates.
 * Env: JJFB_P22_MODE / JJFB_P25_MODE (research / original_headless).
 *
 * P25 corrects false cfg breakpoints:
 *   base+0x01AF8 = STATE_SLOT_COPY_438_TO_430 (diag only)
 *   base+0x0CE8A = CONDITIONAL_STATE_SLOT_COMMIT (diag only)
 * Real cfg loader: base+0x07B6C → 0x10112 at base+0x07B9C.
 */

typedef enum P22Gate {
    P22_G0_BUILD = 0,
    P22_G1_CFG_LOADER,          /* base+0x7B6C */
    P22_G2_INTERNAL_REQUESTED,  /* 0x10112 path=cfg.bin */
    P22_G3_INTERNAL_LOADED,     /* decoded 6898 */
    P22_G4_INTERNAL_PARSED,
    P22_G5_PATH_STATE,          /* base+0xD768 */
    P22_G6_EXTERNAL_LOADED,     /* gwy/cfg.bin 20728 */
    P22_G7_CFG36_PARSED,
    P22_G8_ITEM_CREATED,
    P22_G9_SELECT_CALLBACK,
    P22_G10_DESC_BUILDER_LEGAL, /* legal LR only */
    P22_G11_STATE_NONEMPTY,
    P22_G12_DESC_MATCH,
    P22_G13_STARTGAME_LOOKUP,
    P22_G14_STARTGAME_ENTER,
    P22_G15_OPCODE300,
    P22_G16_NESTED_JJFB,
    P22_G17_ROBOTOL_EXT,
    P22_GATE_COUNT
} P22Gate;

int p22_enabled(void);
void p22_reset(void);

void p22_note_start_dsm(const char *filename, const char *entry, uint32_t param_va);
void p22_note_param_read(const char *milestone);
void p22_note_file_open(const char *guest_path, const char *host_path, int ok, uint32_t size);
void p22_note_file_io(const char *op, const char *guest_path, uint32_t offset, uint32_t length,
                      int32_t ret);
void p22_note_plat_10112(const char *path, const char *ns, const char *host, uint32_t buf,
                         uint32_t len, int loaded, int ret);
void p22_note_module_map(const char *module_name, uint32_t base, uint32_t size);
/* Host uc_mem_write / poke into gamelist code — catches AFF4→B008 patches that
 * bypass UC_HOOK_MEM_WRITE. */
void p22_note_host_mem_write(uint32_t guest_addr, uint32_t size, const void *buf);
void p22_note_platform_memcpy(uint32_t dst, uint32_t src, uint32_t n);
void p22_on_code(void *uc, const char *module_name, uint32_t pc, const uint32_t regs[16],
                 uint32_t lr, uint32_t sp, uint32_t cpsr);
void p22_note_startgame_lookup(uint32_t target_pc, const char *name);
void p22_note_startgame_enter(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r9);
void p22_note_opcode300(uint32_t pc, uint32_t a0, uint32_t a1, uint32_t a2, const char *note);
void p22_note_nested_mrp(const char *target, const char *entry);
void p22_note_robotol_ext(const char *member);
void p22_note_fetch_fault(uint32_t fault_pc, uint32_t fetch_addr, uint32_t lr, uint32_t sp,
                          uint32_t r9);

void p22_report_runtime_stack(const char *why);
int p22_gate_done(P22Gate g);
const char *p22_gate_name(P22Gate g);
void p22_finalize(const char *stop_reason);

#ifdef __cplusplus
}
#endif

#endif
