#ifndef GWY_LAUNCHER_E10A31D_PROVENANCE_H
#define GWY_LAUNCHER_E10A31D_PROVENANCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1d: gamelist method-0 / mrc_init failure provenance.
 * Env: JJFB_E10A31D_MODE=1
 *      JJFB_E10A31D_HISTORY | JJFB_E10A31D_METHOD0_TRACE | JJFB_E10A31D_APPINFO
 *
 * Do not force method0 return, patch failure branches, or touch cfg/AC8/timer.
 */

typedef enum E10a31dSource {
    E10A31D_SRC_NATIVE_GUEST = 0,
    E10A31D_SRC_HOST_FAST_REAL = 1,
    E10A31D_SRC_TIMER = 2,
    E10A31D_SRC_PLATFORM_CALLBACK = 3
} E10a31dSource;

int e10a31d_enabled(void);
int e10a31d_history_enabled(void);
int e10a31d_method0_trace_enabled(void);
int e10a31d_appinfo_enabled(void);
void e10a31d_reset(void);

/* Lane A: start recording after GAMELIST_EXT_FIRST_PC. */
void e10a31d_on_ext_first_pc(void);

/* Lane A/C/D: helper enter/return around runCode. */
void e10a31d_helper_enter(void *uc, E10a31dSource source, uint32_t helper, uint32_t method,
                          uint32_t p_guest, uint32_t erw, uint32_t input, uint32_t input_len,
                          uint32_t caller_pc, uint32_t caller_lr);
void e10a31d_helper_return(void *uc, uint32_t helper, uint32_t method, int32_t ret);

/* Lane C: arm/disarm bounded CODE hook for method 0 only. */
void e10a31d_method0_trace_arm(void *uc, uint32_t helper);
void e10a31d_method0_trace_disarm(void *uc);

/* Lane C/I: platform API on causal method0 path. */
void e10a31d_note_platform_api(void *uc, const char *api, uint32_t slot, int32_t r0);

/* Lane D: dump ERW/appInfo immediately after code6/code8. */
void e10a31d_after_code6(void *uc, uint32_t erw, uint32_t input_len, int32_t ret);
void e10a31d_after_code8(void *uc, uint32_t erw, uint32_t appinfo, uint32_t input_len,
                         int32_t ret);

void e10a31d_mark_milestone(const char *name, const char *note);

#ifdef __cplusplus
}
#endif

#endif
