#ifndef GWY_LAUNCHER_PLATFORM_TEXT_API_H
#define GWY_LAUNCHER_PLATFORM_TEXT_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product 0x11F00 / 0x12340 using authentic gb12.uc2 (no block-glyph fallback).
 * Default ON. Opt-out: JJFB_PLATFORM_TEXT_UC2=0
 */

void platform_text_api_reset(void);
void platform_text_api_arm(void);
int platform_text_api_enabled(void);
int platform_text_api_font_ready(void);
int platform_text_api_font_fallback(void); /* always 0 on product success path */

/* Returns 1 if handled (draw/measure completed with UC2). */
int platform_text_api_handle_11f00(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0,
                                   uint32_t caller_pc, uint32_t caller_lr);
int platform_text_api_handle_12340(void *uc, uint32_t app, uint32_t code_obj, uint32_t param0,
                                   uint32_t caller_pc, uint32_t caller_lr, uint32_t sp);

#ifdef __cplusplus
}
#endif

#endif
