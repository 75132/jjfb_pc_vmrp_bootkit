#ifndef GWY_LAUNCHER_E10A31N_POST_RANGE_H
#define GWY_LAUNCHER_E10A31N_POST_RANGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1n: batch provenance of post-SMSCFG-range path from 0x2E5410 -> 0x2E3FBA.
 *
 * Env:
 *   JJFB_E10A31N_MODE=1
 *   JJFB_E10A31N_RUN_ID
 *   JJFB_E10A31N_CALL_CSV
 *   JJFB_E10A31N_PROV_CSV
 *   JJFB_E10A31N_PRED_CSV
 *   JJFB_E10A31N_SMSCFG_CSV
 *   JJFB_E10A31N_COMPARE_CSV
 *   JJFB_E10A31N_MANIFEST_JSON
 *   JJFB_E10A31N_APPLY_INT16=0x355:1
 *     (default 0x355:1 when MODE=1; set to 0 to disable; DIAGNOSTIC_ONLY at method0 enter)
 *   JJFB_E10A31O_APPLY_NAPPTYPE=1
 *     method0-enter DIAGNOSTIC: write len@0x355 + string at @0x377 for strstr.
 *     Not product bootstrap.
 *   JJFB_E10A31O_NAPPTYPE_STR=...  optional override (default=GWY_LAUNCH_PARAM)
 *   JJFB_E10A31P_APPLY=1
 *     like O, but uses napptype=1 + launch n* fields (switch cases 1/2/3 consume n* keys;
 *     launch uses n* keys; value 12 takes b* path). Also writes int32@0x35F=1 to skip
 *     optional entry string parse (guest BNE bypass). original_default unknown.
 *   JJFB_E10A31Q_APPLY=1
 *     like P, plus b* keys mirrored from launch n* values (next gate after nmrpname PASS
 *     was bapptype). DIAGNOSTIC only; original_default unknown.
 */

#define E10A31N_PC_POST_GATE_BL 0x2E5410u
#define E10A31N_PC_SMS_377 0x2E5440u
#define E10A31N_PC_FAILFN_BL 0x2E5486u
#define E10A31N_PC_CMP_BLX 0x2E3FB4u
#define E10A31N_PC_BNE 0x2E3FB8u
#define E10A31N_PC_MVNS 0x2E3FBAu
#define E10A31N_PC_CALLEE 0x2E2FEDu
#define E10A31N_SMS_OFF_377 0x377u

int e10a31n_enabled(void);
void e10a31n_reset(void);
void e10a31n_on_method0_enter(void *uc, uint32_t helper);
void e10a31n_on_method0_return(void *uc, uint32_t helper, int32_t ret);
void e10a31n_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t sp, uint32_t r0,
                             uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r5,
                             uint32_t r9, uint32_t cpsr, const uint8_t *bytes, uint32_t size);
void e10a31n_mark_milestone(const char *name, const char *note);

/* Narrow SMSCFG lifecycle timing (bootstrap vs method0-enter). */
void e10a31n_on_smscfg_bootstrap_applied(uint32_t cfg_guest, uint32_t dsm_generation,
                                         const char *source);
void e10a31n_on_smscfg_buffer_ready(uint32_t cfg_guest, uint32_t erw_base,
                                    uint32_t dsm_generation);

#ifdef __cplusplus
}
#endif

#endif
