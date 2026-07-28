#ifndef GWY_LAUNCHER_BOOT_SUCCESSOR_TRACE_H
#define GWY_LAUNCHER_BOOT_SUCCESSOR_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P6–P7: post-initial-UI successor observation (product-safe, default ON light;
 * dense PC capture when JJFB_BOOT_SUCCESSOR_TRACE=1).
 */

void boot_successor_trace_reset(void);
void boot_successor_trace_arm(void *uc);
void boot_successor_trace_flush(const char *reason);

/* Resource pipeline markers. */
void boot_successor_on_resource_complete(const char *member_name, uint32_t caller_pc,
                                         uint32_t caller_lr, const char *active_package,
                                         uint64_t scope_generation, const char *via);
void boot_successor_on_resource_request(const char *member_name, uint32_t caller_pc,
                                        uint32_t caller_lr);

/* Guest runtime markers. */
void boot_successor_on_pc(void *uc, uint32_t pc);
void boot_successor_on_platform(uint32_t code, uint32_t app, uint32_t r2, uint32_t r3,
                                uint32_t caller_pc, uint32_t lr, uint32_t sp, uint32_t r9);
void boot_successor_on_family_handler_enter(void *uc, uint32_t event_code, uint32_t app,
                                            uint32_t handler, uint32_t r0, uint32_t r1, uint32_t r2,
                                            uint32_t r3, uint32_t sp, uint32_t lr, uint32_t r9);
void boot_successor_on_timer(uint32_t code, uint32_t period_ms);

uint32_t boot_successor_resource_count(void);
uint32_t boot_successor_note_pixels_calls(void);
void boot_successor_note_pixels_legacy_call(void);

/* P6-1: record entry-complete skip of native body. */
void boot_successor_on_304bf0_entry_complete(const char *member_name, uint32_t out_va,
                                            uint32_t lr, int skipped_native);

#ifdef __cplusplus
}
#endif

#endif
