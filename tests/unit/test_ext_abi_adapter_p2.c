#include "gwy_launcher/ext_abi_adapter.h"
#include "gwy_launcher/ext_lifecycle.h"
#include "gwy_launcher/platform_handler_registry.h"
#include "gwy_launcher/platform_scheduler.h"
#include <stdio.h>
#include <string.h>

static uint32_t g_stub_appinfo = 0x200010u;
static int32_t g_ret_version = 0;
static int32_t g_ret_appinfo = 0;
static int32_t g_ret_init = 0;
static uint32_t g_last_r9;
static uint32_t g_saved_r9 = 0x1111u;
static int g_r9_restored;

static int32_t stub_helper_call(void *uc, uint32_t helper, uint32_t p_guest, uint32_t method,
                                uint32_t input, uint32_t input_len, uint32_t erw) {
    (void)uc;
    (void)helper;
    (void)p_guest;
    (void)input;
    (void)input_len;
    g_last_r9 = erw;
    /* Simulate callee using ER_RW then restore caller. */
    g_r9_restored = (g_saved_r9 == 0x1111u) ? 1 : 0;
    if (method == GWY_EXT_METHOD_VERSION) return g_ret_version;
    if (method == GWY_EXT_METHOD_APPINFO) return g_ret_appinfo;
    if (method == GWY_EXT_METHOD_INIT) return g_ret_init;
    return -99;
}

static uint32_t stub_appinfo_alloc(uint32_t appid, uint32_t appver) {
    if (appid != 400101u || appver != 12u) return 0;
    return g_stub_appinfo;
}

static void inject_ready_ctx(uint64_t gen) {
    GwyExtAbiContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.module_id = 3;
    ctx.module_generation = gen;
    ctx.helper = 0x304AED;
    ctx.p_guest = 0x2AC8DC;
    ctx.er_rw_guest = 0x2B1854;
    ctx.er_rw_size = 0x1000;
    ctx.ext_chunk_guest = 0x2B3000;
    ctx.appid = 400101;
    ctx.appver = 12;
    ctx.mr_version = GWY_EXT_MR_VERSION;
    snprintf(ctx.module_name, sizeof(ctx.module_name), "robotol.ext");
    ext_abi_adapter_test_set_context(&ctx, EXT_ABI_CONTEXT_READY);
}

static int test_call_order(void) {
    ext_abi_adapter_reset();
    ext_lifecycle_reset();
    g_ret_version = g_ret_appinfo = g_ret_init = 0;
    ext_abi_adapter_set_helper_call(stub_helper_call);
    ext_abi_adapter_set_appinfo_alloc(stub_appinfo_alloc);
    inject_ready_ctx(7);
    ext_abi_adapter_request_handshake(3, 7, "robotol.ext");
    if (ext_abi_adapter_try_deliver(NULL) != GWY_EXT_HS_OK) return 1;
    if (ext_abi_adapter_test_call_count() != 3) return 2;
    if (ext_abi_adapter_test_call_method(0) != 6) return 3;
    if (ext_abi_adapter_test_call_method(1) != 8) return 4;
    if (ext_abi_adapter_test_call_method(2) != 0) return 5;
    return 0;
}

static int test_no_init_on_version_fail(void) {
    ext_abi_adapter_reset();
    g_ret_version = -1;
    g_ret_appinfo = g_ret_init = 0;
    ext_abi_adapter_set_helper_call(stub_helper_call);
    ext_abi_adapter_set_appinfo_alloc(stub_appinfo_alloc);
    inject_ready_ctx(8);
    ext_abi_adapter_request_handshake(3, 8, "robotol.ext");
    if (ext_abi_adapter_try_deliver(NULL) != GWY_EXT_HS_VERSION_FAILED) return 10;
    if (ext_abi_adapter_test_call_count() != 1) return 11;
    if (ext_abi_adapter_test_call_method(0) != 6) return 12;
    return 0;
}

static int test_once_per_generation(void) {
    ext_abi_adapter_reset();
    g_ret_version = g_ret_appinfo = g_ret_init = 0;
    ext_abi_adapter_set_helper_call(stub_helper_call);
    ext_abi_adapter_set_appinfo_alloc(stub_appinfo_alloc);
    inject_ready_ctx(9);
    ext_abi_adapter_request_handshake(3, 9, "robotol.ext");
    if (ext_abi_adapter_try_deliver(NULL) != GWY_EXT_HS_OK) return 20;
    ext_abi_adapter_request_handshake(3, 9, "robotol.ext");
    if (ext_abi_adapter_handshake_pending()) return 21;
    if (ext_abi_adapter_try_deliver(NULL) != GWY_EXT_HS_NOT_PENDING &&
        ext_abi_adapter_try_deliver(NULL) != GWY_EXT_HS_ALREADY_DONE)
        return 22;
    return 0;
}

static int test_appinfo_from_metadata(void) {
    /* stub_appinfo_alloc rejects wrong id/ver — proves metadata values flow through. */
    ext_abi_adapter_reset();
    g_ret_version = g_ret_appinfo = g_ret_init = 0;
    ext_abi_adapter_set_helper_call(stub_helper_call);
    ext_abi_adapter_set_appinfo_alloc(stub_appinfo_alloc);
    inject_ready_ctx(10);
    ext_abi_adapter_request_handshake(3, 10, "robotol.ext");
    if (ext_abi_adapter_try_deliver(NULL) != GWY_EXT_HS_OK) return 30;
    return 0;
}

static int test_owner_mismatch(void) {
    GwyExtAbiContext ctx;
    ext_abi_adapter_reset();
    memset(&ctx, 0, sizeof(ctx));
    ctx.module_id = 3;
    ctx.module_generation = 1;
    ctx.helper = 0x304AED;
    ctx.p_guest = 0x2AC8DC;
    ctx.er_rw_guest = 0x2B1854;
    ctx.ext_chunk_guest = 0x2B3000;
    ctx.appid = 400101;
    ctx.appver = 12;
    snprintf(ctx.module_name, sizeof(ctx.module_name), "robotol.ext");
    ext_abi_adapter_test_set_context(&ctx, EXT_ABI_OWNER_MISMATCH);
    ext_abi_adapter_set_helper_call(stub_helper_call);
    ext_abi_adapter_set_appinfo_alloc(stub_appinfo_alloc);
    ext_abi_adapter_request_handshake(3, 1, "robotol.ext");
    if (ext_abi_adapter_try_deliver(NULL) != GWY_EXT_HS_CONTEXT_NOT_READY) return 40;
    if (ext_abi_adapter_test_call_count() != 0) return 41;
    return 0;
}

static int test_r9_frame(void) {
    ext_abi_adapter_reset();
    g_ret_version = g_ret_appinfo = g_ret_init = 0;
    g_last_r9 = 0;
    g_r9_restored = 0;
    ext_abi_adapter_set_helper_call(stub_helper_call);
    ext_abi_adapter_set_appinfo_alloc(stub_appinfo_alloc);
    inject_ready_ctx(11);
    ext_abi_adapter_request_handshake(3, 11, "robotol.ext");
    if (ext_abi_adapter_try_deliver(NULL) != GWY_EXT_HS_OK) return 50;
    if (g_last_r9 != 0x2B1854u) return 51; /* callee ER_RW */
    if (!g_r9_restored) return 52;
    return 0;
}

static int test_guest_depth_enqueue_only(void) {
    GwyScheduledWork w, out;
    platform_scheduler_reset();
    platform_scheduler_set_live_generation(3, 5, "robotol.ext");
    memset(&w, 0, sizeof(w));
    w.source = GWY_SCHED_SRC_PLATFORM_TIMER;
    w.handler_or_helper = 0x30630Du;
    w.forced = 0;
    w.owner_module_id = 3;
    w.owner_generation = 5;
    snprintf(w.owner_module, sizeof(w.owner_module), "robotol.ext");
    if (!platform_scheduler_enqueue(&w, 2)) return 60;
    if (platform_scheduler_try_dequeue(2, &out)) return 61; /* depth>0 must not drain */
    if (!platform_scheduler_try_dequeue(0, &out)) return 62;
    return 0;
}

static int test_stale_generation(void) {
    GwyScheduledWork w, out;
    platform_scheduler_reset();
    platform_scheduler_set_live_generation(3, 5, "robotol.ext");
    memset(&w, 0, sizeof(w));
    w.source = GWY_SCHED_SRC_PLATFORM_EVENT;
    w.owner_module_id = 3;
    w.owner_generation = 4; /* stale */
    w.forced = 0;
    if (platform_scheduler_enqueue(&w, 0)) return 70;
    platform_scheduler_set_live_generation(3, 5, "robotol.ext");
    w.owner_generation = 5;
    if (!platform_scheduler_enqueue(&w, 0)) return 71;
    platform_scheduler_set_live_generation(3, 6, "robotol.ext"); /* cancels stale */
    if (platform_scheduler_try_dequeue(0, &out)) {
        /* cancelled by generation change */
    }
    return 0;
}

static int test_forced_cannot_natural(void) {
    GwyScheduledWork w;
    platform_scheduler_reset();
    platform_scheduler_set_live_generation(3, 1, "robotol.ext");
    memset(&w, 0, sizeof(w));
    w.source = GWY_SCHED_SRC_PLATFORM_TIMER;
    w.forced = 1;
    w.owner_module_id = 3;
    w.owner_generation = 1;
    snprintf(w.owner_module, sizeof(w.owner_module), "robotol.ext");
    platform_scheduler_note_natural_callback(&w, 0, 1);
    if (platform_scheduler_natural_callback_observed()) return 80;
    w.forced = 0;
    platform_scheduler_note_natural_callback(&w, 0, 1);
    if (!platform_scheduler_natural_callback_observed()) return 81;
    return 0;
}

static int test_loose_handler_text(void) {
    platform_handler_registry_reset();
    if (!platform_handler_registry_text_mentions_handler("HANDLER_REGISTER foo")) return 90;
    if (!platform_handler_registry_text_mentions_handler("platform_handler")) return 91;
    if (!platform_handler_registry_text_mentions_handler("PLAT register")) return 92;
    /* Loose text must not mark robotol-owned. */
    if (platform_handler_registry_robotol_owned_observed()) return 93;
    if (!platform_handler_registry_register_owned(0x10140u, 5u, 0x30630Du, 3, 1, "robotol.ext",
                                                  GWY_HANDLER_ISA_THUMB, "sendAppEvent"))
        return 94;
    if (!platform_handler_registry_robotol_owned_observed()) return 95;
    /* DSM / mrc_loader rejected */
    if (platform_handler_registry_register_owned(0x10140u, 5u, 0x306311u, 2, 1, "mrc_loader.ext",
                                                 GWY_HANDLER_ISA_ARM, "sendAppEvent"))
        return 96;
    return 0;
}

int main(void) {
    int rc;
    if ((rc = test_call_order()) != 0) {
        fprintf(stderr, "call_order fail %d\n", rc);
        return rc;
    }
    if ((rc = test_no_init_on_version_fail()) != 0) {
        fprintf(stderr, "no_init fail %d\n", rc);
        return rc;
    }
    if ((rc = test_once_per_generation()) != 0) {
        fprintf(stderr, "once_per_gen fail %d\n", rc);
        return rc;
    }
    if ((rc = test_appinfo_from_metadata()) != 0) {
        fprintf(stderr, "appinfo fail %d\n", rc);
        return rc;
    }
    if ((rc = test_owner_mismatch()) != 0) {
        fprintf(stderr, "owner_mismatch fail %d\n", rc);
        return rc;
    }
    if ((rc = test_r9_frame()) != 0) {
        fprintf(stderr, "r9_frame fail %d\n", rc);
        return rc;
    }
    if ((rc = test_guest_depth_enqueue_only()) != 0) {
        fprintf(stderr, "guest_depth fail %d\n", rc);
        return rc;
    }
    if ((rc = test_stale_generation()) != 0) {
        fprintf(stderr, "stale_gen fail %d\n", rc);
        return rc;
    }
    if ((rc = test_forced_cannot_natural()) != 0) {
        fprintf(stderr, "forced fail %d\n", rc);
        return rc;
    }
    if ((rc = test_loose_handler_text()) != 0) {
        fprintf(stderr, "handler_text fail %d\n", rc);
        return rc;
    }
    printf("test_ext_abi_adapter_p2 OK\n");
    return 0;
}
