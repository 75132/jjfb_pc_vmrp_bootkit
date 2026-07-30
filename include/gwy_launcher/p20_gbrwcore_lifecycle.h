#ifndef GWY_LAUNCHER_P20_GBRWCORE_LIFECYCLE_H
#define GWY_LAUNCHER_P20_GBRWCORE_LIFECYCLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P20: gbrwcore module lifecycle command=0 → 0x10102 callback → lazy init → API builder.
 *
 * Env: JJFB_P20_GBRWCORE_LIFECYCLE=1
 *
 * File offsets (gbrwcore.ext) — VAs = image_base + offset (NOT raw map pad base).
 *   MRPGCMAP helper      0x21284
 *   module dispatcher    0x217EC
 *   command=0 branch     0x2181C
 *   cb-reg call          0x21828
 *   registration fn      0x216D4
 *   platform-reg wrap    0x214D4
 *   event callback       0x1FFE4
 *   lazy init            0x20444
 *   API table builder    0x1B400
 *   startGame fn store   0x1B4FA
 *
 * Static addresses are research breakpoints/asserts only — never product jump targets.
 */

int p20_gbrwcore_lifecycle_enabled(void);
void p20_gbrwcore_lifecycle_reset(void);

void p20_gbrwcore_lifecycle_on_module_map(const char *module_name, uint32_t base, uint32_t size);
void p20_gbrwcore_lifecycle_on_helper_register(uint32_t helper, uint32_t p_guest);
void p20_gbrwcore_lifecycle_bind_uc(void *uc);
void p20_gbrwcore_lifecycle_on_code(void *uc, uint64_t module_id, const char *module_name,
                                    uint32_t pc, const uint32_t regs[16]);
void p20_gbrwcore_lifecycle_on_plat_10102(uint32_t family, uint32_t callback, uint32_t r9,
                                          const char *owner);
void p20_gbrwcore_lifecycle_finalize(const char *stop_reason);

/* Gates 1–9 (1 = hit). */
int p20_gate_cmd0(void);
int p20_gate_reg10102(void);
int p20_gate_callback_enter(void);
int p20_gate_lazy_init(void);
int p20_gate_api_builder(void);
int p20_gate_sg_ptr(void);
int p20_gate_startgame(void);
int p20_gate_opcode300(void);
int p20_gate_nested_jjfb(void);

uint32_t p20_image_base(void);
uint32_t p20_live_r9(void);
uint32_t p20_sg_fn_ptr(void);

#ifdef __cplusplus
}
#endif

#endif
