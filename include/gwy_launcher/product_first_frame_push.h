#ifndef GWY_LAUNCHER_PRODUCT_FIRST_FRAME_PUSH_H
#define GWY_LAUNCHER_PRODUCT_FIRST_FRAME_PUSH_H

#include "gwy_launcher/platform_event_service.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Product First-Frame Push (P6–P8 orchestration).
 * Modes via JJFB_PRODUCT_FFP_MODE=1 and JJFB_PRODUCT_FFP_PHASE=event|resource|validate
 */

typedef enum GwyFfpPhase {
    GWY_FFP_PHASE_NONE = 0,
    GWY_FFP_PHASE_EVENT = 1,
    GWY_FFP_PHASE_RESOURCE = 2,
    GWY_FFP_PHASE_VALIDATE = 3
} GwyFfpPhase;

int product_ffp_enabled(void);
GwyFfpPhase product_ffp_phase(void);
int product_ffp_apply_abi(void); /* Round B: apply recovered delivery ABI */

void product_ffp_reset(void);
void product_ffp_set_run_id(const char *run_id);
const char *product_ffp_run_id(void);

/* Bind / note platform lifecycle. */
void product_ffp_on_alloc(void *uc, uint32_t plat_code, uint32_t size, uint32_t guest_ptr,
                          uint32_t handler, uint32_t er_rw, uint64_t owner_module_id);
void product_ffp_on_guest_sendappevent(void *uc, uint32_t code, uint32_t app, const uint32_t r[8],
                                       uint32_t stack0, uint32_t stack1, uint32_t caller_pc,
                                       uint32_t caller_lr, uint32_t er_rw);

/*
 * Family event path: returns 1 to enqueue, 0 to suppress.
 * When enabled, owns identity / queue policy (P5 one-shot is diagnostic only).
 */
int product_ffp_on_family_request(void *uc, uint32_t event_code, uint32_t app, uint32_t family,
                                  uint32_t handler, uint32_t secondary_handler, uint32_t caller_pc,
                                  uint32_t caller_lr, const uint32_t r[8], uint32_t r9,
                                  uint32_t stack0, uint32_t stack1, uint32_t er_rw,
                                  uint64_t owner_module_id, uint64_t owner_generation,
                                  uint64_t *out_request_id);

void product_ffp_prepare_delivery_abi(uint64_t request_id, GwyEventDeliveryAbi *out);
void product_ffp_on_handler_enter(void *uc, uint64_t request_id, uint32_t handler,
                                  const GwyEventDeliveryAbi *abi);
void product_ffp_on_handler_leave(void *uc, uint64_t request_id, int ok, int32_t ret,
                                  uint32_t er_rw);
void product_ffp_on_next_timer(void *uc, uint32_t er_rw);

/* Auto phase transitions / milestone markers. */
void product_ffp_note_resource_open(const char *path);
void product_ffp_note_resource_read(uint32_t bytes);
void product_ffp_note_framebuffer_write(uint32_t bytes, uint32_t hash);
void product_ffp_note_disp_up_ex(void);

void product_ffp_finalize(void);

int product_ffp_state_advanced(void);
int product_ffp_abi_confirmed(void);

#ifdef __cplusplus
}
#endif

#endif
