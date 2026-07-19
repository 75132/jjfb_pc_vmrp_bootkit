#ifndef GWY_LAUNCHER_PLATFORM_CALL_CENSUS_H
#define GWY_LAUNCHER_PLATFORM_CALL_CENSUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stage E8B: observe-only unique platform call census (JJFB_PLAT_CENSUS=1). */

int platform_call_census_enabled(void);
void platform_call_census_reset(void);
void platform_call_census_set_tick(uint32_t tick);

/* Record one call. caller_pc may be 0 if unknown. */
void platform_call_census_note(uint32_t code, uint32_t app_or_family, uint32_t caller_pc,
                               uint32_t ret);

void platform_call_census_note_draw(void);
void platform_call_census_note_refresh(void);
void platform_call_census_note_file_miss(const char *guest_path);
void platform_call_census_note_networkish(const char *tag);

/* Compact dump (call on finalize / long-run summary). */
void platform_call_census_dump(const char *reason);

/* Stage E8B: observe-only 0x1E209/0x9 (JJFB_PLAT_1E209_TRACE=1). Never changes ret. */
int platform_1e209_trace_enabled(void);
void platform_1e209_trace_call(uint32_t caller_pc, uint32_t r0, uint32_t r1, uint32_t r2,
                               uint32_t r3, uint32_t ret, uint32_t tick);

#ifdef __cplusplus
}
#endif

#endif
