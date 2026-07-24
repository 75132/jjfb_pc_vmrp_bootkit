#ifndef GWY_LAUNCHER_PRODUCT_PATH_A_HANDLER_TRACE_H
#define GWY_LAUNCHER_PRODUCT_PATH_A_HANDLER_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Path-A valid handler (0x2E4040) outcome trace — observe-only.
 *
 * Env: JJFB_PATH_A_HANDLER_TRACE=1  (default Launcher OFF; Diagnostic may enable)
 *
 * Follows 0x2E2520 → 0x2E4040 call tree until return to the handler's LR
 * (or insn budget / async boundary). Never writes B71/15D/UI_MODE and never
 * forces callees or fake platform success.
 */

int product_pah_enabled(void);
void product_pah_reset(void);
void product_pah_set_run_id(const char *run_id);
const char *product_pah_run_id(void);

void product_pah_bind_uc(void *uc);
void product_pah_note_er_rw(uint32_t er_rw);
void product_pah_note_module_range(uint32_t code_base, uint32_t code_size);
void product_pah_arm_hooks(void *uc);

/* Lifecycle / platform side-effect observers (no guest mutation). */
void product_pah_on_10140_tick(void *uc, uint32_t er_rw);
void product_pah_note_resource_request(const char *path);
void product_pah_note_disp_up(void);
void product_pah_note_platform_api(const char *api, uint32_t a0, uint32_t a1);

/* 1 while dense-following 0x2E4040 tree (for 0x10138 scope). */
int product_pah_in_handler(void);
uint32_t product_pah_handler_call_id(void);

void product_pah_finalize(void);

#ifdef __cplusplus
}
#endif

#endif
