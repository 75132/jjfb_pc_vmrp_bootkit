#ifndef GWY_LAUNCHER_GUEST_CALL_OBSERVER_H
#define GWY_LAUNCHER_GUEST_CALL_OBSERVER_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/ext_loader.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t load_id;
    uint64_t caller_module_id;
    uint32_t function_table_base;
    uint32_t slot;
    uint32_t target;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t result;
} GuestIndirectCallEvent;

typedef struct GuestCallObserver GuestCallObserver;

GuestCallObserver *guest_call_observer_create(void);
void guest_call_observer_destroy(GuestCallObserver *obs);

void guest_call_observer_bind(GuestCallObserver *obs, ExtLoader *loader, void *uc_engine);

/* Watch ModuleRegistry mapping region for module_id (observe-only). */
LauncherStatus guest_call_observer_attach_module(GuestCallObserver *obs,
                                                 uint64_t module_id,
                                                 LauncherError *err);

/* Process-local singleton for Gwy bridge. */
GuestCallObserver *gwy_guest_call_observer_ensure(void);
void gwy_guest_call_observer_bind_uc(void *uc_engine);
void *gwy_guest_call_observer_uc(void);

#ifdef __cplusplus
}
#endif

#endif
