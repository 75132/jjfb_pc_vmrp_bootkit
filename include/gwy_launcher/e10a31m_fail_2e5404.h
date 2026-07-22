#ifndef GWY_LAUNCHER_E10A31M_FAIL_2E5404_H
#define GWY_LAUNCHER_E10A31M_FAIL_2E5404_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1m: exact provenance of method0 failure at 0x2E5404.
 *
 * Env:
 *   JJFB_E10A31M_MODE=1
 *   JJFB_E10A31M_RUN_ID
 *   JJFB_E10A31M_BRANCH_CSV
 *   JJFB_E10A31M_SMSCFG_CSV
 *   JJFB_E10A31M_REQ_JSON
 *   JJFB_E10A31M_APPLY_INT16=0x355:1   (optional controlled apply, Case B)
 */

#define E10A31M_PC_GWY_STRCMP_BLX 0x2E53DAu
#define E10A31M_PC_COPY355_BL 0x2E53EAu
#define E10A31M_PC_LDRSH 0x2E53F2u
#define E10A31M_PC_PRED_A_CMP 0x2E53F4u
#define E10A31M_PC_PRED_A_BLE 0x2E53F6u
#define E10A31M_PC_PRED_B_CMP 0x2E5400u
#define E10A31M_PC_PRED_B_BLE 0x2E5402u
#define E10A31M_PC_FAIL 0x2E5404u
#define E10A31M_SMSCFG_OFF 0x355u
#define E10A31M_SMSCFG_MAX 0x1B2u

int e10a31m_enabled(void);
void e10a31m_reset(void);
void e10a31m_on_method0_enter(void *uc, uint32_t helper);
void e10a31m_on_method0_return(void *uc, uint32_t helper, int32_t ret);
void e10a31m_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t sp, uint32_t r0,
                             uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5,
                             uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size);
void e10a31m_on_mem_read(void *uc, uint32_t pc, uint32_t addr, uint32_t size, const void *data);
void e10a31m_mark_milestone(const char *name, const char *note);

#ifdef __cplusplus
}
#endif

#endif
