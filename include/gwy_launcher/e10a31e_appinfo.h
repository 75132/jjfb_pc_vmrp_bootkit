#ifndef GWY_LAUNCHER_E10A31E_APPINFO_H
#define GWY_LAUNCHER_E10A31E_APPINFO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1e: package-scoped appInfo metadata binding instrumentation.
 * Env: JJFB_E10A31E_MODE=1
 *      JJFB_E10A31E_METADATA | BINDING | AB_COMPARE | METHOD0 | READ_PROOF
 */

int e10a31e_enabled(void);
void e10a31e_reset(void);
void e10a31e_mark_milestone(const char *name, const char *note);

/* After metadata activate / resolve. */
void e10a31e_note_metadata(const char *phase);

/* After [GWY_APPINFO_BIND] / binding set. */
void e10a31e_note_binding(uint32_t guest_ptr, uint32_t appid, uint32_t appver, int source);

/* Immediately before code8 (owner validation). */
void e10a31e_before_code8(void *uc, uint32_t helper, uint32_t erw, uint32_t appinfo);

/* After code6 / code8 / method0 returns. */
void e10a31e_after_code6(void *uc, uint32_t erw, uint32_t input_len, int32_t ret);
void e10a31e_after_code8(void *uc, uint32_t erw, uint32_t appinfo, int32_t ret);
void e10a31e_after_method0(void *uc, uint32_t helper, int32_t ret);

/* Narrow read-proof around R9+0x920 and appInfo fields during method0. */
void e10a31e_read_proof_arm(void *uc, uint32_t erw, uint32_t appinfo);
void e10a31e_read_proof_disarm(void *uc);

/* A/B compare row (legacy env vs package metadata). */
void e10a31e_note_ab_case(const char *case_name, int32_t ret6, int32_t ret8, int32_t ret0,
                          uint32_t appid, uint32_t appver, uint32_t fail_pc,
                          const char *fail_class);

#ifdef __cplusplus
}
#endif

#endif
