#ifndef GWY_LAUNCHER_E10A31G_STRCMP_H
#define GWY_LAUNCHER_E10A31G_STRCMP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1g: true-fail strcmp / GPT-tag argument tracing.
 * Env: JJFB_E10A31G_MODE=1
 * Sites:
 *   gamelist 0x2E5396 BL 0x2E3180  (id=0x349, dst=sp+0x30, len=3)
 *   gamelist 0x2E31AE BLX memcpy   (src = base+0x349)
 *   gamelist 0x2E53A6 BLX 0xAC2D0  (strcmp dst vs "GPT")
 *   cfunction 0xAC2D0 / 0xAC2E8    (true fail)
 */

int e10a31g_enabled(void);
void e10a31g_reset(void);
void e10a31g_mark_milestone(const char *name, const char *note);

void e10a31g_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr,
                             const uint8_t *bytes, uint32_t size);

void e10a31g_on_method0_return(void *uc, uint32_t helper, int32_t ret);

#ifdef __cplusplus
}
#endif

#endif
