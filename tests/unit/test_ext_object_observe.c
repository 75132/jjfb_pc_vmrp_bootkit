#include "gwy_launcher/ext_object_observe.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    ExtAddrClassResult r;
    const ExtCallTransfer *xfer;

    ext_object_observe_reset();
    if (strcmp(ext_object_kind_name(EXT_OBJECT_MRC_EXT_CHUNK), "MRC_EXT_CHUNK") != 0) return 1;
    if (strcmp(guest_address_class_name(ADDR_MODULE_CODE), "MODULE_CODE") != 0) return 1;
    if (strcmp(ext_identity_confidence_name(EXT_IDENTITY_CANDIDATE), "CANDIDATE") != 0) return 1;
    if (strcmp(ext_transfer_hop_kind_name(EXT_HOP_DSM_TO_LOADER), "DSM_TO_LOADER") != 0) return 1;
    if (strcmp(ext_arg_role_name(EXT_ARG_COMMAND), "COMMAND") != 0) return 1;
    if (strcmp(gwy_entry_relation_name(GWY_ENTRY_REL_DIRECT_TARGET_IS_HELPER),
               "DIRECT_TARGET_IS_HELPER") != 0)
        return 1;

    ext_object_classify_address(0, 0, &r);
    if (r.addr_class != ADDR_UNKNOWN) return 1;

    /* transfer_id smoke: records even without a bound ModuleRegistry. */
    ext_object_note_cross_module_call(1, 0x100, 2, 0x200, 0, 0, 0);
    xfer = ext_object_last_transfer();
    if (!xfer || !xfer->valid || xfer->transfer_id != 1) return 1;
    if (xfer->caller_module_id != 1 || xfer->callee_module_id != 2) return 1;

    if (ext_object_dispatch_trace_enabled()) return 1; /* env unset in unit */

    printf("test_ext_object_observe OK\n");
    return 0;
}
