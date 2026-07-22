#ifndef GWY_LAUNCHER_E10A31H_SMSCFG_H
#define GWY_LAUNCHER_E10A31H_SMSCFG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1h: GPT fail attributed to mr_sms_cfg_buf / smsGetBytes.
 * Env: JJFB_E10A31H_MODE=1
 *
 * Identity proof:
 *   gamelist 0x2E3180 range_limit = 0x10E0 = MR_SMS_CFG_BUF_LEN (120*36)
 *   *(mr_table + 0x1C0) = mr_sms_cfg_buf  (bridge MAP_DATA offset)
 *   *(mr_table + 0xC)   = memcpy           (live BLX target 0x94E94)
 */

int e10a31h_enabled(void);
void e10a31h_reset(void);
void e10a31h_mark_milestone(const char *name, const char *note);

void e10a31h_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr,
                             const uint8_t *bytes, uint32_t size);

void e10a31h_on_method0_return(void *uc, uint32_t helper, int32_t ret);

#ifdef __cplusplus
}
#endif

#endif
