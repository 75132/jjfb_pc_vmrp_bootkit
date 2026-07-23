#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/ext_entry_observe.h"
#include "gwy_launcher/ext_chunk_observe.h"
#include "gwy_launcher/ext_helper_handoff.h"
#include "gwy_launcher/ext_dsm_record_observe.h"
#include "gwy_launcher/ext_module_entry_abi.h"
#include "gwy_launcher/ext_entry_null_contract.h"
#include "gwy_launcher/ext_module_data_init.h"
#include "gwy_launcher/ext_er_rw_producer.h"
#include "gwy_launcher/ext_bootstrap_abi.h"
#include "gwy_launcher/ext_callback_frame.h"
#include "gwy_launcher/ext_r9_scope_audit.h"
#include "gwy_launcher/ext_post_cont_audit.h"
#include "gwy_launcher/ext_post_cfn_r9_audit.h"
#include "gwy_launcher/ext_p_extchunk_audit.h"
#include "gwy_launcher/ext_shell_publication_audit.h"
#include "gwy_launcher/ext_mrpgcmap_entry_order.h"
#include "gwy_launcher/ext_entry_abi_cluster_audit.h"
#include "gwy_launcher/ext_cfunction_publication_audit.h"
#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_er_rw_bind_restore.h"
#include "gwy_launcher/ext_gwy_shell_shim.h"
#include "gwy_launcher/ext_gwy_shell_native_exec.h"
#include "gwy_launcher/e10a3_postselect_trace.h"
#include "gwy_launcher/e10a31_gamelist_context.h"
#include "gwy_launcher/e10a31a_precont_diag.h"
#include "gwy_launcher/e10a31b_publication.h"
#include "gwy_launcher/e10a31c_dispatch.h"
#include "gwy_launcher/e10a31j_smscfg_long.h"
#include "gwy_launcher/gwy_sms_cfg.h"
#include "gwy_launcher/e10a_shell_trace.h"
#include "gwy_launcher/ext_gwy_startgame_audit.h"
#include "gwy_launcher/module_r9_switch.h"
#include "gwy_launcher/guest_call_observer.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/platform_send_app_event.h"
#include "gwy_launcher/platform_timer.h"
#include "gwy_launcher/platform_handler_registry.h"
#include "gwy_launcher/platform_call_census.h"
#include "gwy_launcher/platform_scheduler.h"
#include "gwy_launcher/ext_abi_adapter.h"
#include "gwy_launcher/ext_lifecycle.h"
#include "gwy_launcher/package_scope.h"
#include "gwy_launcher/product_callback_trace.h"
#include "gwy_launcher/product_p4_progress.h"
#include "gwy_launcher/product_p5_event_advance.h"
#include "gwy_launcher/product_first_frame_push.h"
#include "gwy_launcher/product_event_queue_bootstrap.h"
#include "gwy_launcher/platform_event_service.h"
#include "gwy_launcher/platform_event_queue.h"
#include "gwy_launcher/platform_timer_cadence.h"
#include "gwy_launcher/handler_forensic.h"
#include "gwy_launcher/robotol_idle_watch.h"
#include "gwy_launcher/robotol_flag_writer_trace.h"
#include "gwy_launcher/jjfb_plat_11f00.h"
#include "gwy_launcher/vm_runtime.h"
#include "gwy_launcher/guest_memory.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward decls for product P2 helpers defined later in this file. */
void gwy_ext_obs_set_product_run_id(const char *run_id);
int gwy_ext_obs_try_product_handshake(void *uc);
void gwy_ext_obs_request_product_handshake(void);

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

static ModuleR9Scope g_helper_r9_scope;
static int g_helper_r9_scope_valid;

/* Match third_party/vmrp_upstream/header/gwy_ext_obs_abi.h allocator typedefs. */
typedef void *(*GwyExtObsGuestAllocFn)(uint32_t size);
typedef uint32_t (*GwyExtObsGuestPtrFn)(void *host);

static GwyExtObsGuestAllocFn g_guest_alloc;
static GwyExtObsGuestPtrFn g_guest_to_ptr;
static uint32_t g_userinfo_guest;

typedef int32_t (*GwyExtObsTimerStartFn)(uint16_t ms);
typedef int32_t (*GwyExtObsTimerStopFn)(void);
typedef uint32_t (*GwyExtObsTimerClockFn)(void);
typedef void (*GwyExtObsTimerDeliverFn)(void *uc);
static GwyExtObsTimerStartFn g_timer_start;
static GwyExtObsTimerStopFn g_timer_stop;
static GwyExtObsTimerDeliverFn g_timer_deliver;
static int g_timer_flushing;
static uint32_t g_armed_timer_chunk;
static int g_timer_arm_seen;
static int g_timer_arm_count;
static uint32_t g_timer_last_period_ms;
static uint32_t g_timer_last_id;
static int g_start_dsm_returned;
static int g_arm_absent_emitted;
static uint32_t g_post_loop_iter;
static uint32_t g_draw_count;
static uint32_t g_refresh_count;
/* Host 50ms tick when guest registered 0x10140 but never armed classic timer. */
static int g_lifecycle_host_armed;
static int g_lifecycle_pending;
static int g_lifecycle_delivering;
static uint32_t g_lifecycle_ticks;
static void *g_bound_uc;
static const uint32_t GWY_LIFECYCLE_PERIOD_MS = 50u;
static const uint64_t GWY_LIFECYCLE_INSN_LIMIT = 200000ull;

static void gwy_ext_obs_timer_poll(void *uc);
static void gwy_ext_obs_deferred_timer_pump(void *uc);
static void gwy_ext_obs_lifecycle_deliver(void *uc);
static void gwy_ext_obs_drain_family_events(void *uc);
static int gwy_ext_obs_note_family_event(uint32_t event_code, uint32_t app);

#define GWY_P4_FAMILY_EVENT_Q 16
typedef struct GwyFamilyEvent {
    uint32_t event_code;
    uint32_t app;
    uint32_t handler;
    uint32_t del_r2;
    uint32_t del_r3;
    uint32_t del_stack0;
    uint32_t del_stack1;
    int del_apply_stack;
    /* 1 = 10165 Path-A enqueue ABI: R0=buf10165 R1=buf10162 (not family app/event). */
    int enqueue_mode;
    uint32_t enq_r0;
    uint32_t enq_r1;
    uint64_t owner_module_id;
    uint64_t owner_generation;
    uint64_t request_id;
    char owner_module[64];
    int used;
} GwyFamilyEvent;
static GwyFamilyEvent g_family_eq[GWY_P4_FAMILY_EVENT_Q];
static int g_family_eq_n;
static int g_family_contract_fixed;
static int g_family_draining;

static int env_flag(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1';
}

void gwy_ext_obs_bind_uc(void *uc) {
    const char *rid;
    g_bound_uc = uc;
    rid = getenv("GWY_PRODUCT_RUN_ID");
    if (rid && rid[0]) gwy_ext_obs_set_product_run_id(rid);
    e10a31j_bind_uc(uc);
    gwy_guest_call_observer_bind_uc(uc);
    ext_entry_observe_bind_uc(uc);
    ext_chunk_observe_bind_uc(uc);
    ext_helper_handoff_bind_uc(uc);
    ext_dsm_record_observe_bind_uc(uc);
    ext_module_entry_abi_bind_uc(uc);
    ext_entry_null_contract_bind_uc(uc);
    ext_module_data_init_bind_uc(uc);
    ext_er_rw_producer_bind_uc(uc);
    ext_er_rw_producer_reset();
    ext_bootstrap_abi_bind_uc(uc);
    ext_bootstrap_abi_reset();
    ext_callback_frame_bind_uc(uc);
    ext_callback_frame_reset();
    ext_r9_scope_audit_bind_uc(uc);
    ext_r9_scope_audit_reset();
    ext_post_cont_audit_bind_uc(uc);
    ext_post_cont_audit_reset();
    ext_post_cfn_r9_audit_bind_uc(uc);
    ext_post_cfn_r9_audit_reset();
    ext_p_extchunk_audit_bind_uc(uc);
    ext_p_extchunk_audit_reset();
    ext_shell_publication_audit_bind_uc(uc);
    ext_shell_publication_audit_reset();
    ext_mrpgcmap_entry_order_reset();
    ext_mrpgcmap_entry_order_bind_uc(uc);
    ext_entry_abi_cluster_audit_reset();
    ext_entry_abi_cluster_audit_bind_uc(uc);
    ext_cfunction_publication_audit_reset();
    ext_cfunction_publication_audit_bind_uc(uc);
    ext_chunk_provider_reset();
    ext_chunk_provider_bind_uc(uc);
    ext_er_rw_bind_restore_reset();
    ext_er_rw_bind_restore_bind_uc(uc);
    ext_gwy_startgame_audit_bind_uc(uc);
    ext_gwy_startgame_audit_reset();
    ext_gwy_shell_native_exec_bind_uc(uc);
    ext_gwy_shell_native_exec_reset();
    module_r9_switch_reset();
    guest_memory_r9_write_reset();
    platform_handler_registry_reset();
    platform_timer_reset();
    platform_scheduler_reset();
    platform_timer_cadence_reset();
    product_callback_trace_reset();
    product_p4_reset();
    product_p5_reset();
    product_ffp_reset();
    if (getenv("GWY_PRODUCT_RUN_ID") && getenv("GWY_PRODUCT_RUN_ID")[0]) {
        product_callback_trace_set_run_id(getenv("GWY_PRODUCT_RUN_ID"));
        product_p4_set_run_id(getenv("GWY_PRODUCT_RUN_ID"));
        product_p5_set_run_id(getenv("GWY_PRODUCT_RUN_ID"));
        product_ffp_set_run_id(getenv("GWY_PRODUCT_RUN_ID"));
        platform_timer_cadence_set_run_id(getenv("GWY_PRODUCT_RUN_ID"));
        platform_scheduler_set_run_id(getenv("GWY_PRODUCT_RUN_ID"));
        platform_handler_registry_set_run_id(getenv("GWY_PRODUCT_RUN_ID"));
    }
    ext_lifecycle_reset();
    ext_abi_adapter_reset();
    e10a31_reset();
    e10a31a_reset();
    e10a31b_reset();
    e10a31c_reset();
    e10a31c_set_timer_pump(gwy_ext_obs_deferred_timer_pump);
    g_armed_timer_chunk = 0;
    g_timer_arm_seen = 0;
    g_timer_arm_count = 0;
    g_timer_last_period_ms = 0;
    g_timer_last_id = 0;
    g_start_dsm_returned = 0;
    g_arm_absent_emitted = 0;
    g_post_loop_iter = 0;
    g_draw_count = 0;
    g_refresh_count = 0;
    g_lifecycle_host_armed = 0;
    g_lifecycle_pending = 0;
    g_lifecycle_delivering = 0;
    g_lifecycle_ticks = 0;
    g_helper_r9_scope_valid = 0;
    memset(&g_helper_r9_scope, 0, sizeof(g_helper_r9_scope));
    memset(g_family_eq, 0, sizeof(g_family_eq));
    g_family_eq_n = 0;
    g_family_contract_fixed = 0;
    g_family_draining = 0;
    platform_call_census_reset();
    robotol_idle_watch_reset();
    robotol_idle_watch_bind_uc(uc);
    robotol_flag_writer_trace_reset();
    robotol_flag_writer_trace_bind_uc(uc);
}

void gwy_ext_obs_host_callback_enter(void *uc, uint32_t slot_addr, const char *name) {
    ext_callback_frame_on_host_enter(uc, slot_addr, name);
    e10a31j_on_host_api_enter(uc, slot_addr, name);
    ext_post_cont_audit_on_host_api(uc, slot_addr, name, 1, 0);
    ext_gwy_startgame_audit_on_plat_or_testcom(uc, name, slot_addr);
    if (name && (strstr(name, "TestCom") || strstr(name, "testcom") || strstr(name, "plat"))) {
#ifdef GWY_HAVE_UNICORN
        uint32_t r0 = 0, r1 = 0, r2 = 0;
        if (uc) {
            uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0, &r0);
            uc_reg_read((uc_engine *)uc, UC_ARM_REG_R1, &r1);
            uc_reg_read((uc_engine *)uc, UC_ARM_REG_R2, &r2);
        }
        if (r0 == 601 || r1 == 601 || r2 == 601)
            ext_gwy_shell_native_exec_on_strcom(601, name);
        if (r0 == 800 || r1 == 800 || r2 == 800)
            ext_gwy_shell_native_exec_on_strcom(800, name);
        if (r0 == 801 || r1 == 801 || r2 == 801)
            ext_gwy_shell_native_exec_on_strcom(801, name);
#else
        (void)uc;
#endif
    }
}

void gwy_ext_obs_host_callback_leave(void *uc, uint32_t slot_addr, const char *name) {
    ext_callback_frame_on_host_leave(uc, slot_addr, name);
    e10a31j_on_host_api_leave(uc, slot_addr, name);
    ext_post_cont_audit_on_host_api(uc, slot_addr, name, 0, 0);
}

void gwy_ext_obs_host_callback_resume(void *uc, uint32_t slot_addr, const char *name) {
    ext_callback_frame_on_host_resume(uc, slot_addr, name);
    /* After _mr_c_function_new returns, guest fills P+0/+4; peek once resume arms. */
    if (name && strstr(name, "_mr_c_function_new")) {
        uint32_t pg = ext_chunk_provider_last_p_guest();
        if (pg)
            ext_er_rw_bind_restore_peek_and_bind(pg, "mr_c_function_st_metadata_bind");
        robotol_idle_watch_note_stage(uc, "after_er_rw_bind");
    }
}

/*
 * FixR9 DOCUMENTED: *((void**)(mr_c_function_load)-1) = P  → guest VA image+4.
 * Prefer the MRP module that owns helper; else LR-bearing module.
 */
uint32_t gwy_ext_obs_c_function_p_slot_va(uint32_t helper, uint32_t lr) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *by_h = NULL;
    const GwyLoadedModule *by_lr = NULL;
    if (!reg) return 0;
    if (helper) {
        by_h = module_registry_find_by_helper(reg, helper);
        if (!by_h) by_h = module_registry_find_by_code_addr(reg, helper & ~1u);
    }
    if (lr) by_lr = module_registry_find_by_code_addr(reg, lr & ~1u);
    if (by_h && by_h->origin == MODULE_ORIGIN_MRP_MEMBER && by_h->map.guest_code_base)
        return by_h->map.guest_code_base + 4u;
    if (by_lr && by_lr->origin == MODULE_ORIGIN_MRP_MEMBER && by_lr->map.guest_code_base)
        return by_lr->map.guest_code_base + 4u;
    if (by_h && by_h->map.guest_code_base) return by_h->map.guest_code_base + 4u;
    if (by_lr && by_lr->map.guest_code_base) return by_lr->map.guest_code_base + 4u;
    return 0;
}

void gwy_ext_obs_note_c_function_p_slot(uint32_t p_guest, uint32_t helper, uint32_t slot,
                                        uint32_t lr, int wrote) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = NULL;
    const char *mod = "?";
    const char *pkg = "?";
    if (reg && helper) {
        m = module_registry_find_by_helper(reg, helper);
        if (!m) m = module_registry_find_by_code_addr(reg, helper & ~1u);
    }
    if (!m && reg && lr) m = module_registry_find_by_code_addr(reg, lr & ~1u);
    if (m) {
        mod = m->resolved_name[0] ? m->resolved_name : m->requested_name;
        if (strstr(m->package_path, "jjfb.mrp")) pkg = "gwy/jjfb.mrp";
        else if (strstr(m->package_path, "wxjwq.mrp")) pkg = "gwy/wxjwq.mrp";
        else if (reg->logical_package[0]) pkg = reg->logical_package;
    }
    printf("[JJFB_CFN_P_SLOT] package=%s module=%s helper=0x%X P=0x%X slot=0x%X lr=0x%X "
           "reason=fixr9_c_function_p_image_plus4 evidence=DOCUMENTED wrote=%d\n",
           pkg, mod, helper, p_guest, slot, lr, wrote);
    fflush(stdout);
    printf("[JJFB_GAME_P_TIMELINE] package=%s module=%s event=CFN_P_SLOT_PUBLISH P=0x%X "
           "p0=0 p4=0 registry_base=0x0 registry_len=0 slot=0x%X evidence=DOCUMENTED\n",
           pkg, mod, p_guest, slot);
    fflush(stdout);
}

void gwy_ext_obs_c_function_new_ex(uint32_t helper,
                                   uint32_t p_len,
                                   uint32_t p_guest_addr,
                                   uint32_t rw_base,
                                   uint32_t rw_size,
                                   uint32_t stack_base,
                                   const char *origin) {
    ExtLoader *L = gwy_ext_loader_ensure();
    uint32_t regs[16];
    memset(regs, 0, sizeof(regs));
    regs[0] = helper;
    regs[1] = p_len;
    ext_helper_handoff_cfunction_enter(origin ? origin : "HOST_BRIDGE", helper, p_len, p_guest_addr,
                                       regs, 0, 0, NULL, 0, NULL);
    ext_dsm_record_note_cfunction_side_effects(helper, p_guest_addr, p_len,
                                               origin ? origin : "HOST_BRIDGE");
    ext_module_entry_abi_on_cfunction_p(helper, p_guest_addr, p_len,
                                        origin ? origin : "HOST_BRIDGE");
    ext_entry_null_contract_on_cfunction_p(helper, p_guest_addr, p_len);
    ext_module_data_init_on_cfunction_p(helper, p_guest_addr, p_len, rw_base, rw_size);
    ext_er_rw_producer_on_cfunction_p(helper, p_guest_addr, p_len, rw_base, rw_size);
    ext_bootstrap_abi_on_cfunction_p(helper, p_guest_addr, p_len, rw_base, rw_size);
    ext_callback_frame_on_cfunction_observe(NULL, helper, p_len, origin ? origin : "HOST_BRIDGE");
    ext_post_cont_audit_on_cfunction_p(helper, p_guest_addr, p_len, rw_base, rw_size);
    ext_post_cfn_r9_audit_on_cfunction_p(helper, p_guest_addr, p_len, rw_base, rw_size);
    ext_p_extchunk_audit_on_cfunction_p(helper, p_guest_addr, p_len, rw_base, rw_size);
    ext_gwy_startgame_audit_on_cfunction_p(helper, p_guest_addr, p_len);
    ext_loader_on_c_function_new(L, helper, p_len, p_guest_addr, rw_base, rw_size, stack_base);
    ext_entry_observe_bootstrap_event("C_FUNCTION_NEW");
    ext_entry_observe_on_p_candidate(p_guest_addr, p_len, NULL);
    if (p_guest_addr)
        ext_entry_abi_cluster_audit_on_p_candidate(
            p_guest_addr, (origin && strstr(origin, "GUEST_NESTED")) ? "nested_cfn" :
                          (origin && strstr(origin, "LOG_PARSE"))     ? "log_parse" :
                                                                       "host_bridge");
    ext_cfunction_publication_audit_on_cfunction_new(helper, p_len, p_guest_addr,
                                                     origin ? origin : "HOST_BRIDGE");
    /* Phase 6K: after nested register, run documented entry before guest continues. */
    if (origin && (strstr(origin, "GUEST_NESTED") || strstr(origin, "LOG_PARSE"))) {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *gm = reg ? module_registry_find_by_helper(reg, helper) : NULL;
        const char *mn = "gbrwcore.ext";
        if (gm) {
            mn = gm->resolved_name[0] ? gm->resolved_name : gm->requested_name;
            ext_mrpgcmap_entry_order_on_module_registered(mn, gm->map.guest_code_base, helper);
        }
        if (mn && p_guest_addr &&
            (strstr(mn, "gamelist") || strstr(mn, "gbrwcore") || strstr(mn, "gbrwshell"))) {
            static uint32_t s_last_shell_p;
            if (strstr(mn, "gamelist")) {
                int reused = (s_last_shell_p != 0 && s_last_shell_p == p_guest_addr);
                e10a31b_note_cfn(mn, helper, p_guest_addr, p_len, reused, s_last_shell_p);
            }
            s_last_shell_p = p_guest_addr;
        }
        ext_mrpgcmap_entry_order_before_continuation(NULL, mn, 0);
        /* Phase 6N: entry emu / cfunction zero-init may clear P+0xC — restore platform publish. */
        ext_chunk_provider_after_entry_order(helper);
        /* Phase 6O: if P+0/+4 already filled, bind registry ER_RW + extChunk var. */
        {
            uint32_t pg = ext_chunk_provider_last_p_guest();
            if (pg)
                ext_er_rw_bind_restore_peek_and_bind(pg, "platform_er_rw_publication_restore");
            robotol_idle_watch_note_stage(NULL, "after_platform_er_rw");
        }
    }
}

void gwy_ext_obs_extchunk_set_sendappevent(uint32_t guest_addr) {
    ext_chunk_provider_set_sendappevent_guest(guest_addr);
}

void gwy_ext_obs_extchunk_set_mr_table(uint32_t guest_addr) {
    ext_chunk_provider_set_mr_table_guest(guest_addr);
    e10a31j_on_mr_table(guest_addr);
}

int gwy_ext_obs_extchunk_want(uint32_t helper) {
    return ext_chunk_provider_want(helper);
}

int gwy_ext_obs_extchunk_try_reuse(void *uc, uint32_t helper, uint32_t p_guest, void *p_host) {
    return ext_chunk_provider_try_reuse(uc, helper, p_guest, p_host);
}

int gwy_ext_obs_extchunk_on_c_function_new(void *uc, uint32_t helper, uint32_t p_guest, void *p_host,
                                          void *chunk_host, uint32_t chunk_guest) {
    return ext_chunk_provider_on_c_function_new(uc, helper, p_guest, p_host, chunk_host, chunk_guest);
}

void gwy_ext_obs_set_guest_allocator(GwyExtObsGuestAllocFn alloc, GwyExtObsGuestPtrFn to_guest) {
    g_guest_alloc = alloc;
    g_guest_to_ptr = to_guest;
}

uint32_t gwy_ext_obs_guest_malloc0(uint32_t size) {
    void *host;
    if (!g_guest_alloc || !g_guest_to_ptr || !size) return 0;
    host = g_guest_alloc(size);
    if (!host) return 0;
    memset(host, 0, size);
    return g_guest_to_ptr(host);
}

void gwy_ext_obs_set_timer_fns(GwyExtObsTimerStartFn start, GwyExtObsTimerStopFn stop) {
    g_timer_start = start;
    g_timer_stop = stop;
}

void gwy_ext_obs_set_timer_deliver(GwyExtObsTimerDeliverFn deliver) {
    g_timer_deliver = deliver;
}

void gwy_ext_obs_set_timer_clock(GwyExtObsTimerClockFn clock_ms) {
    platform_timer_set_clock(clock_ms);
}

void gwy_ext_obs_timer_poll_uc(void *uc) { gwy_ext_obs_timer_poll(uc); }

void gwy_ext_obs_timer_signal_due(void) { platform_timer_signal_due(); }

int gwy_ext_obs_timer_take_due(void) { return platform_timer_take_due(); }

int gwy_ext_obs_timer_is_due(void) { return platform_timer_is_due(); }

int gwy_ext_obs_timer_should_defer(void) {
    return e10a31c_enabled() && e10a31c_should_defer_timer();
}

void gwy_ext_obs_timer_note_defer(void *uc) {
    /* helper resolved lazily inside poll/pump paths; host_loop may pass NULL uc. */
    e10a31c_note_timer_defer(uc, 0);
}

int gwy_ext_obs_timer_running(void) { return platform_timer_running(); }

uint32_t gwy_ext_obs_timer_armed_chunk(void) {
    uint32_t c = e10a31_timer_armed_chunk();
    return c ? c : g_armed_timer_chunk;
}

int gwy_ext_obs_timer_ext_target(uint32_t *out_helper, uint32_t *out_p_guest,
                                 uint32_t *out_erw) {
    if (e10a31_timer_armed_chunk()) {
        return e10a31_timer_fire_resolve(g_bound_uc, out_helper, out_p_guest, out_erw);
    }
    if (!g_armed_timer_chunk) return 0;
    return ext_chunk_provider_timer_dispatch_target(g_armed_timer_chunk, out_helper, out_p_guest,
                                                    out_erw);
}

int gwy_ext_obs_timer_arm_seen(void) { return g_timer_arm_seen; }

int gwy_ext_obs_e10a31_timer_arm_observed(void) { return e10a31_timer_arm_observed(); }

int gwy_ext_obs_e10a31_timer_fire_observed(void) { return e10a31_timer_fire_observed(); }

int gwy_ext_obs_e10a31_timer_fire_count(void) { return e10a31_timer_fire_count(); }

void gwy_ext_obs_timer_host_arm(uint32_t period_ms, const char *route, uint32_t pc) {
    if (env_flag("JJFB_TIMER_ARM_TRACE")) {
        printf("[JJFB_TIMER_ARM_ATTEMPT] module=? pc=0x%X delay_ms=%u period_ms=%u "
               "route=%s evidence=DOCUMENTED\n",
               pc, period_ms, period_ms, route ? route : "?");
        fflush(stdout);
    }
    if (period_ms < 1u || period_ms > 60000u) {
        if (env_flag("JJFB_TIMER_ARM_TRACE")) {
            printf("[JJFB_TIMER_ARM_ABSENT] window=arm_attempt reason=period_out_of_range "
                   "period_ms=%u evidence=OBSERVED\n",
                   period_ms);
            fflush(stdout);
        }
        return;
    }
    platform_timer_start(period_ms);
    g_timer_arm_seen = 1;
    g_timer_arm_count++;
    g_timer_last_period_ms = period_ms;
    g_timer_last_id = (uint32_t)g_timer_arm_count;
    if (g_timer_start) (void)g_timer_start((uint16_t)period_ms);
    printf("[PLATFORM_TIMER] op=START chunk=0x0 period_ms=%u id=%u name=%s "
           "evidence=DOCUMENTED\n",
           period_ms, g_timer_last_id, route ? route : "?");
    if (env_flag("JJFB_TIMER_ARM_TRACE")) {
        printf("[JJFB_TIMER_ARM] timer_id=%u owner_package=? owner_module=? delay_ms=%u "
               "period_ms=%u source_pc=0x%X route=%s evidence=DOCUMENTED\n",
               g_timer_last_id, period_ms, period_ms, pc, route ? route : "?");
        fflush(stdout);
    }
    fflush(stdout);
}

void gwy_ext_obs_timer_host_disarm(const char *route, uint32_t pc) {
    platform_timer_stop();
    g_armed_timer_chunk = 0;
    e10a31_timer_disarm();
    if (g_timer_stop) (void)g_timer_stop();
    if (env_flag("JJFB_TIMER_ARM_TRACE")) {
        printf("[JJFB_TIMER_CANCEL] timer_id=%u owner_module=? reason=%s pc=0x%X "
               "evidence=DOCUMENTED\n",
               g_timer_last_id, route ? route : "timerStop", pc);
        fflush(stdout);
    }
    printf("[PLATFORM_TIMER] op=STOP chunk=0x0 name=%s evidence=DOCUMENTED\n",
           route ? route : "timerStop");
    fflush(stdout);
}

void gwy_ext_obs_on_start_dsm_return(const char *filename, int32_t ret) {
    g_start_dsm_returned = 1;
    (void)filename;
    (void)ret;
    (void)gwy_ext_obs_try_product_handshake(g_bound_uc);
    if (!g_arm_absent_emitted && !g_timer_arm_seen) {
        g_arm_absent_emitted = 1;
        printf("[JJFB_TIMER_ARM_ABSENT] window=mrc_init_to_start_dsm_return "
               "reason=no_timer_api_call_seen arm_count=0 evidence=TARGET_OBSERVED\n");
        fflush(stdout);
    }
    /*
     * CROSS_TARGET: 0x10140 is the period/main handler. When guest never calls
     * classic timerStart, host may arm a tick for liveness — tagged forced so it
     * cannot satisfy SCHEDULER_NATURAL_CALLBACK.
     */
    if (!g_lifecycle_host_armed && !g_timer_arm_seen &&
        platform_handler_registry_has(0x10140u)) {
        uint32_t h = platform_handler_registry_get(0x10140u);
        g_lifecycle_host_armed = 1;
        gwy_ext_obs_timer_host_arm(GWY_LIFECYCLE_PERIOD_MS, "lifecycle_10140_forced_host", 0);
        printf("[JJFB_LIFECYCLE] op=ARM period_ms=%u handler=0x%X family=0x%X "
               "reason=10140_registered_no_classic_timer forced=yes "
               "evidence=CROSS_TARGET+docs/06\n",
               GWY_LIFECYCLE_PERIOD_MS, h, platform_handler_registry_family(0x10140u));
        fflush(stdout);
        {
            GwyScheduledWork w;
            memset(&w, 0, sizeof(w));
            w.source = GWY_SCHED_SRC_PLATFORM_TIMER;
            w.handler_or_helper = h;
            w.forced = 1; /* fabricated host arm — not natural */
            snprintf(w.owner_module, sizeof(w.owner_module), "robotol.ext");
            (void)platform_scheduler_enqueue(&w, module_r9_switch_depth());
        }
    }
}

static int gwy_ext_obs_note_family_event(uint32_t event_code, uint32_t app) {
    const GwyPlatformHandlerRecord *fam;
    const GwyPlatformHandlerRecord *enq;
    const GwyPlatformHandlerRecord *target;
    GwyFamilyEvent *slot;
    int i;
    uint32_t r[8];
    uint32_t pc = 0, lr = 0, r9 = 0, arg2 = 0, arg3 = 0, arg4 = 0;
    void *uc = g_bound_uc;

    fam = platform_handler_registry_find_family_event(event_code);
    enq = platform_handler_registry_enqueue_handler();
    /* Family-band sendAppEvent (e.g. 0x1E209 under 0x1E200) goes to the 10102
     * family switch. 10165 is the queue drain path, not the direct event ABI. */
    target = fam ? fam : enq;
    if (!target || !target->handler) return 0;

#ifdef GWY_HAVE_UNICORN
    if (uc) {
        int ri;
        for (ri = 0; ri < 8; ri++) uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0 + ri, &r[ri]);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &lr);
        (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9);
        arg2 = r[2];
        arg3 = r[3];
    } else
#endif
    {
        memset(r, 0, sizeof(r));
        r[0] = event_code;
        r[1] = app;
    }

    for (i = 0; i < g_family_eq_n; i++) {
        if (g_family_eq[i].used && g_family_eq[i].event_code == event_code &&
            g_family_eq[i].app == app && g_family_eq[i].handler == target->handler) {
            /* Still pending in host queue — guest reissued before delivery. */
            if (product_p5_enabled() || product_ffp_enabled()) {
                printf("[PLATFORM_FAMILY_EVENT] op=ALREADY_QUEUED event=0x%X app=0x%X "
                       "note=guest_reissue_while_pending evidence=OBSERVED\n",
                       event_code, app);
                fflush(stdout);
            }
            return 1;
        }
    }

    {
        uint32_t erw = r9;
        uint32_t sp0 = 0, sp1 = 0;
        uint64_t rid = 0;
#ifdef GWY_HAVE_UNICORN
        if (uc) {
            uint32_t sp = 0;
            uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &sp);
            if (sp) {
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp, &sp0);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 4u, &sp1);
                if (!arg4) arg4 = sp0;
            }
        }
#endif
        /* P6+: PlatformEventService owns identity. P5 one-shot is diagnostic-only. */
        if (product_ffp_enabled()) {
            if (!product_ffp_on_family_request(uc, event_code, app, fam ? fam->family : 0,
                                              fam ? fam->handler : 0, enq ? enq->handler : 0,
                                              lr ? lr : pc, lr, r, r9, sp0, sp1, erw,
                                              target->owner_module_id, target->owner_generation,
                                              &rid)) {
                printf("[PLATFORM_FAMILY_EVENT] op=SUPPRESS event=0x%X app=0x%X note=ffp_identity "
                       "evidence=OBSERVED\n",
                       event_code, app);
                fflush(stdout);
                return 0;
            }
        } else if (product_p5_enabled()) {
            if (!product_p5_on_guest_request(uc, event_code, app, arg2, arg3, arg4, lr ? lr : pc, lr,
                                             r, r9, erw, target->owner_module_id,
                                             target->owner_generation, fam ? fam->handler : 0,
                                             enq ? enq->handler : 0)) {
                printf("[PLATFORM_FAMILY_EVENT] op=SUPPRESS event=0x%X app=0x%X note=one_shot_or_dup "
                       "evidence=OBSERVED\n",
                       event_code, app);
                fflush(stdout);
                return 0;
            }
            rid = product_p5_pending_request_id(event_code, app);
        }

        if (g_family_eq_n >= GWY_P4_FAMILY_EVENT_Q) return 0;

        /*
         * Event Completion Contract: before family notification (case 9 → 0x305E08
         * free via 0x10133), publish Path-A via registered 10165 enqueue handler.
         * Proven: UI_MODE@ER_RW+(0x800+0xD0) stays 0 until B54 Path A runs; case 9 does not
         * consume 10165 context. Requires JJFB_PRODUCT_EVENT_CONTRACT=1.
         */
        if (product_ffp_enabled() && env_flag("JJFB_PRODUCT_EVENT_CONTRACT") &&
            event_code == 0x1E209u && app == 9u) {
            uint32_t p65 = 0, p62 = 0, enq_h = 0, store = 0;
            uint32_t b54 = 0, flag15c = 0;
            if (platform_event_service_resolve_completion_objs(&p65, &p62, &enq_h, &store) &&
                enq_h && g_family_eq_n + 1 < GWY_P4_FAMILY_EVENT_Q) {
                /* Path A list insert @0x312A60 requires a non-null queue head (ER_RW+0xB54).
                 * Invoking 30D24C while B54==0 → UC_FAULT @ LDR [r4,#4] (ENTRY_ARGUMENT).
                 * Case E: if guest never ran 0x30CBBC→0x2FE82C→0x312AA4, publish the proven
                 * 8-byte list control via PlatformEventQueue into the live B54 slot. */
                if (uc && erw) {
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, erw + 0xB54u, &b54);
                    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, erw + 0x15Cu, &flag15c);
                    product_eqb_note_er_rw(erw, "path_a");
                    product_eqb_note_owner_store(0x10165u, p65, store);
                    product_eqb_note_owner_store(0x10162u, p62, 0);
                    /* Round B (Validate / ApplyAbi): publish proven list control if missing.
                     * Round A TraceQueueBootstrap alone remains observe-only. */
                    if (!b54 && (product_ffp_apply_abi() ||
                                 product_ffp_phase() == GWY_FFP_PHASE_VALIDATE)) {
                        if (platform_event_queue_ensure_list_head(uc, erw, target->owner_module_id,
                                                                 target->owner_generation)) {
                            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, erw + 0xB54u,
                                                           &b54);
                            printf("[EVENT_PATH_A_LIST_HEAD_RECOVERED] er_rw=0x%X b54=0x%X "
                                   "via=platform_event_queue evidence=OBSERVED\n",
                                   erw, b54);
                            fflush(stdout);
                        }
                    }
                }
                if (!b54) {
                    printf("[EVENT_PATH_A_BLOCKED_NULL_LIST_HEAD] er_rw=0x%X off=0xB54 b54=0 "
                           "flag15c=0x%X enq_handler=0x%X ctx=0x%X sib=0x%X "
                           "note=defer_30D24C_until_B54_ready evidence=OBSERVED\n",
                           erw, flag15c, enq_h, p65, p62);
                    fflush(stdout);
                    product_eqb_on_path_a_blocked(uc, erw, b54, store, p65, p62);
                } else if (g_family_eq_n + 1 < GWY_P4_FAMILY_EVENT_Q) {
                GwyFamilyEvent *es = &g_family_eq[g_family_eq_n++];
                memset(es, 0, sizeof(*es));
                es->used = 1;
                es->event_code = event_code;
                es->app = app;
                es->handler = enq_h;
                es->enqueue_mode = 1;
                es->enq_r0 = p65;
                es->enq_r1 = p62;
                es->owner_module_id = target->owner_module_id;
                es->owner_generation = target->owner_generation;
                es->request_id = rid;
                snprintf(es->owner_module, sizeof(es->owner_module), "%s",
                         target->owner_module[0] ? target->owner_module : "robotol.ext");
                printf("[PROVEN_EVENT_COMPLETION_CONTRACT_FIXED] request_id=%llu object=0x%X "
                       "offset=path_a_via_101ab owner_store=0x%X old=ui_mode_0 new=enqueue_30D24C "
                       "source=0x101AB_path_a owner_module=robotol.ext enq_handler=0x%X "
                       "sib=0x%X b54=0x%X evidence=OBSERVED\n",
                       (unsigned long long)rid, p65, store, enq_h, p62, b54);
                printf("[EVENT_PATH_A_ENQUEUE_BEGIN] enq_handler=0x%X ctx=0x%X sib=0x%X b54=0x%X "
                       "evidence=OBSERVED\n",
                       enq_h, p65, p62, b54);
                printf("[EVENT_PATH_A_ENQUEUE_OK] note=list_head_ready evidence=OBSERVED\n");
                printf("[EVENT_FIRST_MISSING_OUTPUT_PRODUCER_FOUND] producer=0x101AB "
                       "via_handler=0x%X note=case9_305E08_is_10133_free_not_ack "
                       "unfinished=ER_RW+0x800+0xD0 evidence=OBSERVED\n",
                       enq_h);
                fflush(stdout);
                {
                    GwyScheduledWork w2;
                    memset(&w2, 0, sizeof(w2));
                    w2.source = GWY_SCHED_SRC_PLATFORM_EVENT;
                    w2.handler_or_helper = enq_h;
                    w2.method_or_event = event_code;
                    w2.owner_module_id = target->owner_module_id;
                    w2.owner_generation = target->owner_generation;
                    w2.forced = 0;
                    snprintf(w2.owner_module, sizeof(w2.owner_module), "%s", es->owner_module);
                    (void)platform_scheduler_enqueue(&w2, module_r9_switch_depth());
                }
                }
            }
        }

        slot = &g_family_eq[g_family_eq_n++];
        memset(slot, 0, sizeof(*slot));
        slot->used = 1;
        slot->event_code = event_code;
        slot->app = app;
        slot->handler = target->handler;
        slot->owner_module_id = target->owner_module_id;
        slot->owner_generation = target->owner_generation;
        slot->request_id = rid;
        if (product_ffp_enabled() && rid) {
            GwyEventDeliveryAbi dabi;
            product_ffp_prepare_delivery_abi(rid, &dabi);
            slot->del_r2 = dabi.r2;
            slot->del_r3 = dabi.r3;
            slot->del_stack0 = dabi.stack0;
            slot->del_stack1 = dabi.stack1;
            slot->del_apply_stack = dabi.have_stack && product_ffp_apply_abi();
        }
        snprintf(slot->owner_module, sizeof(slot->owner_module), "%s",
                 target->owner_module[0] ? target->owner_module : "robotol.ext");
    }

    {
        GwyScheduledWork w;
        memset(&w, 0, sizeof(w));
        w.source = GWY_SCHED_SRC_PLATFORM_EVENT;
        w.handler_or_helper = target->handler;
        w.method_or_event = event_code;
        w.owner_module_id = target->owner_module_id;
        w.owner_generation = target->owner_generation;
        w.forced = 0;
        snprintf(w.owner_module, sizeof(w.owner_module), "%s", slot->owner_module);
        (void)platform_scheduler_enqueue(&w, module_r9_switch_depth());
    }

    if (product_p4_enabled()) {
        product_p4_note_work("event_completion_generated", target->plat_code, target->family,
                             target->handler, target->owner_module_id, target->owner_generation,
                             target->owner_module, 1, 1, 1, 0, 0);
    }
    if (!g_family_contract_fixed) {
        g_family_contract_fixed = 1;
        printf("[PROVEN_PLATFORM_CONTRACT_FIXED] contract=family_event_dispatch "
               "event=0x%X app=0x%X handler=0x%X via=%s run_id=%s evidence=OBSERVED\n",
               event_code, app, target->handler, fam ? "10102_family" : "10165_enqueue",
               product_callback_trace_run_id());
        fflush(stdout);
    }
    printf("[PLATFORM_FAMILY_EVENT] op=ENQUEUE event=0x%X app=0x%X handler=0x%X "
           "request_id=%llu owner=%s evidence=OBSERVED\n",
           event_code, app, target->handler, (unsigned long long)slot->request_id,
           slot->owner_module);
    fflush(stdout);
    return 1;
}

static void gwy_ext_obs_drain_family_events(void *uc) {
    int i;
    if (!uc || g_family_draining || g_family_eq_n <= 0) return;
    /* Host lifecycle already runs nested under package depth; do not require
     * guest_depth==0 here — that would permanently drop registered completions. */
    g_family_draining = 1;
    for (i = 0; i < g_family_eq_n; i++) {
        GwyFamilyEvent *ev = &g_family_eq[i];
        GwyUcEntryAbi abi;
        GwyUcEntryRunOut out;
        uint32_t stop = GWY_VM_DEFAULT_MEM_BASE;
        uint32_t r9_save = 0, r9_run = 0;
        ModuleRegistry *reg;
        const GwyLoadedModule *owner;
        int ok;
        if (!ev->used || !ev->handler) continue;

        (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
        r9_run = r9_save;
        reg = gwy_ext_loader_bound_registry();
        owner = reg ? module_registry_find_by_code_addr(reg, ev->handler & ~1u) : NULL;
        if (owner && owner->data.start_of_er_rw) r9_run = owner->data.start_of_er_rw;
        if (r9_run) (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_run);

        memset(&abi, 0, sizeof(abi));
        abi.set_r0 = 1;
        abi.set_r1 = 1;
        abi.set_lr = 1;
        abi.lr = stop;
        if (ev->enqueue_mode) {
            /* 10165 enqueue: R0=10165 buf, R1=10162 buf (legacy V64 ABI). */
            abi.r0 = ev->enq_r0;
            abi.r1 = ev->enq_r1;
            printf("[PLATFORM_FAMILY_EVENT] op=ENQUEUE_PATH_A handler=0x%X r0=0x%X r1=0x%X "
                   "request_id=%llu evidence=OBSERVED\n",
                   ev->handler, abi.r0, abi.r1, (unsigned long long)ev->request_id);
            fflush(stdout);
        } else {
            /* Family switch ABI: subcode in R0 (sendAppEvent app), event id in R1. */
            abi.r0 = ev->app;
            abi.r1 = ev->event_code;
            if (ev->del_r2 || product_ffp_apply_abi()) {
                abi.set_r2 = 1;
                abi.r2 = ev->del_r2;
            }
            if (ev->del_r3 || product_ffp_apply_abi()) {
                abi.set_r3 = 1;
                abi.r3 = ev->del_r3;
            }
        }

        /* Round B: place recovered stack args at entry SP[0]/SP[4] (AAPCS). */
        if (ev->del_apply_stack && !ev->enqueue_mode) {
#ifdef GWY_HAVE_UNICORN
            uint32_t sp = 0;
            if (guest_memory_uc_read_sp((struct uc_struct *)uc, &sp) && sp >= 8u) {
                uint32_t sp2 = sp - 8u;
                if (guest_memory_uc_write_sp((struct uc_struct *)uc, sp2)) {
                    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, sp2, ev->del_stack0);
                    (void)guest_memory_uc_poke_u32((struct uc_struct *)uc, sp2 + 4u, ev->del_stack1);
                    printf("[PLATFORM_FAMILY_EVENT] op=STACK_ARGS sp=0x%X stack0=0x%X stack1=0x%X "
                           "evidence=OBSERVED\n",
                           sp2, ev->del_stack0, ev->del_stack1);
                    fflush(stdout);
                }
            }
#endif
        }

        printf("[PLATFORM_FAMILY_EVENT] op=DELIVER event=0x%X app=0x%X handler=0x%X r9=0x%X "
               "r2=0x%X r3=0x%X request_id=%llu evidence=OBSERVED\n",
               ev->event_code, ev->app, ev->handler, r9_run, abi.r2, abi.r3,
               (unsigned long long)ev->request_id);
        fflush(stdout);

        if (product_ffp_enabled()) {
            GwyEventDeliveryAbi dabi;
            memset(&dabi, 0, sizeof(dabi));
            dabi.r0 = abi.r0;
            dabi.r1 = abi.r1;
            dabi.r2 = abi.r2;
            dabi.r3 = abi.r3;
            dabi.stack0 = ev->del_stack0;
            dabi.stack1 = ev->del_stack1;
            dabi.have_stack = ev->del_apply_stack;
            product_ffp_on_handler_enter(uc, ev->request_id, ev->handler, &dabi);
        } else if (product_p5_enabled()) {
            product_p5_on_handler_enter(uc, ev->request_id, ev->handler, abi.r0, abi.r1, abi.r2,
                                        abi.r3, r9_run);
        }

        ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, ev->handler, stop,
                                          GWY_LIFECYCLE_INSN_LIMIT, &abi, &out);
        (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);

        if (product_ffp_enabled()) {
            product_ffp_on_handler_leave(uc, ev->request_id, ok, (int32_t)out.r0_after, r9_run);
            /* Defer FFP finalize to lifecycle cadence so Round A can collect ≥8 samples. */
        } else if (product_p5_enabled()) {
            product_p5_on_handler_leave(uc, ev->request_id, ok, (int32_t)out.r0_after, r9_run);
            /* Flush after ≥2 deliveries so Round A sees guest reissue before kill. */
            if (product_p5_txn_by_id(ev->request_id) &&
                product_p5_txn_by_id(ev->request_id)->request_id >= 2)
                product_p5_finalize();
        }

        printf("[PLATFORM_FAMILY_EVENT] op=DELIVER_DONE ok=%d ret=%d end=%s handler=0x%X "
               "evidence=OBSERVED\n",
               ok, (int)out.r0_after, out.end_reason[0] ? out.end_reason : "?", ev->handler);
        fflush(stdout);

        if (product_p4_enabled()) {
            product_p4_note_work("event_completion_delivered", 0x10102u, ev->event_code, ev->handler,
                                 ev->owner_module_id, ev->owner_generation, ev->owner_module, 1, 1,
                                 1, 1, ok ? 1 : 0);
        }
        {
            GwyScheduledWork delivered;
            memset(&delivered, 0, sizeof(delivered));
            delivered.source = GWY_SCHED_SRC_PLATFORM_EVENT;
            delivered.handler_or_helper = ev->handler;
            delivered.method_or_event = ev->event_code;
            delivered.forced = 0;
            delivered.owner_module_id = ev->owner_module_id;
            delivered.owner_generation = ev->owner_generation;
            snprintf(delivered.owner_module, sizeof(delivered.owner_module), "%s", ev->owner_module);
            platform_scheduler_note_natural_callback(&delivered, (int32_t)out.r0_after, ok ? 1 : 0);
        }
        ev->used = 0;
    }
    g_family_eq_n = 0;
    g_family_draining = 0;
}

static void gwy_ext_obs_lifecycle_deliver(void *uc) {
    uint32_t handler;
    GwyUcEntryAbi abi;
    GwyUcEntryRunOut out;
    uint32_t stop = GWY_VM_DEFAULT_MEM_BASE;
    uint32_t r9_save = 0, r9_run = 0;
    ModuleRegistry *reg;
    const GwyLoadedModule *owner;
    int ok;
    int drain = robotol_idle_watch_drain_order();

    if (!uc) return;
    g_lifecycle_pending = 0;

    /* Capture post-completion digest on subsequent timer ticks (before drain). */
    if (product_ffp_enabled() && g_lifecycle_ticks > 0u) {
        uint32_t erw = 0;
        ModuleRegistry *reg0 = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *own0 =
            reg0 ? module_registry_find_by_code_addr(
                       reg0, platform_handler_registry_get(0x10140u) & ~1u)
                 : NULL;
        if (own0 && own0->data.start_of_er_rw)
            erw = own0->data.start_of_er_rw;
        else
            (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &erw);
        product_ffp_on_next_timer(uc, erw);
    } else if (product_p5_enabled() && g_lifecycle_ticks > 0u) {
        uint32_t erw = 0;
        ModuleRegistry *reg0 = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *own0 =
            reg0 ? module_registry_find_by_code_addr(
                       reg0, platform_handler_registry_get(0x10140u) & ~1u)
                 : NULL;
        if (own0 && own0->data.start_of_er_rw)
            erw = own0->data.start_of_er_rw;
        else
            (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &erw);
        product_p5_on_next_timer(uc, erw);
    }

    /* Drain guest-registered family/enqueue completions before the timer poller. */
    gwy_ext_obs_drain_family_events(uc);

    handler = platform_handler_registry_get(0x10140u);
    if (!handler) return;

    /* E8E drain-order C: one 10165 at app-start (before first 10140), then only 10140. */
    if (drain == 'C' && g_lifecycle_ticks == 0u) {
        printf("[JJFB_E8E_DRAIN_ORDER] order=C phase=app_start_10165_then_10140_only "
               "evidence=HYPOTHESIS\n");
        fflush(stdout);
        robotol_idle_watch_try_10165_probe(uc);
    }
    /* E8E drain-order A: 10165 then 10140 on first tick. */
    if (drain == 'A' && g_lifecycle_ticks == 0u) {
        printf("[JJFB_E8E_DRAIN_ORDER] order=A phase=10165_before_10140 evidence=HYPOTHESIS\n");
        fflush(stdout);
        robotol_idle_watch_try_10165_probe(uc);
    }

    /* CROSS_TARGET: handler runs with owning module ER_RW in R9 (legacy tick path). */
    (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9_save);
    r9_run = r9_save;
    reg = gwy_ext_loader_bound_registry();
    owner = reg ? module_registry_find_by_code_addr(reg, handler & ~1u) : NULL;
    if (owner && owner->data.start_of_er_rw) r9_run = owner->data.start_of_er_rw;
    if (r9_run) (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_run);

    g_lifecycle_ticks++;
    platform_call_census_set_tick(g_lifecycle_ticks);
    memset(&abi, 0, sizeof(abi));
    abi.set_r0 = 1;
    abi.r0 = 0;
    abi.set_r1 = 1;
    abi.r1 = 0;
    abi.set_lr = 1;
    /* Match bridge runCode stop: LR=CODE_ADDRESS (ARM). Thumb LR|1 mis-executes DSM. */
    abi.lr = stop;

    printf("[JJFB_LIFECYCLE] op=FIRE tick=%u handler=0x%X r0=0 r1=0 r9=0x%X owner=%s "
           "evidence=CROSS_TARGET+docs/06\n",
           g_lifecycle_ticks, handler, r9_run,
           owner ? (owner->resolved_name[0] ? owner->resolved_name : owner->requested_name) : "?");
    fflush(stdout);

    /* Observe-only forensic on first FIRE. Do not poke UC regs here (audit:
     * guest_memory is the only write surface); ENTRY snap is pre-emu, ring[0]
     * captures post-canonicalization T/regs. */
    if (handler_forensic_enabled() && g_lifecycle_ticks == 1u) {
        (void)handler_forensic_begin(
            uc, handler, owner ? owner->map.guest_code_base : 0,
            owner ? owner->map.guest_code_size : 0,
            owner ? (owner->resolved_name[0] ? owner->resolved_name : owner->requested_name) : "?",
            r9_run);
    }

    {
        GwyP3CallbackSnap snap;
        uint32_t seq;
        uint64_t occ = (uint64_t)g_lifecycle_ticks;
        int forced_tick = (g_lifecycle_host_armed && !g_timer_arm_seen) ? 1 : 0;
        memset(&snap, 0, sizeof(snap));
        snap.timer_or_event_id = occ;
        snap.owner_module_id = owner ? owner->module_id : 0;
        snap.owner_generation = owner ? owner->module_id : 0;
        snap.source = (uint32_t)GWY_SCHED_SRC_PLATFORM_TIMER;
        snap.handler = handler;
        snap.pc = handler;
        snap.lr = stop;
        snap.r9 = r9_run;
        snap.guest_depth = module_r9_switch_depth();
        snap.forced = forced_tick;
        if (owner)
            snprintf(snap.owner_module, sizeof(snap.owner_module), "%s",
                     owner->resolved_name[0] ? owner->resolved_name : owner->requested_name);
        else
            snprintf(snap.owner_module, sizeof(snap.owner_module), "robotol.ext");

        platform_timer_cadence_note_arm(1u, snap.owner_generation, GWY_LIFECYCLE_PERIOD_MS, 1,
                                        forced_tick ? "host_lifecycle" : "platform_timer");
        if (!platform_timer_cadence_try_queue_occurrence(1u, occ)) {
            /* duplicate occurrence while in-flight — skip deliver */
            (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
            return;
        }
        platform_timer_cadence_note_dequeue(1u, occ);
        platform_timer_cadence_note_inflight_begin(1u, occ);

        seq = product_callback_trace_on_enter(&snap);
        snap.seq = seq;
        product_callback_trace_frame_enter(seq, (uint64_t)seq << 32 | handler, handler, r9_run,
                                           stop);

        /*
         * P4 observes host-lifecycle and classic-timer fires alike. P3 may mark
         * host_lifecycle as forced_tick for scheduler enqueue provenance, but the
         * guest entry itself is still the natural Robotol handler under test.
         * Instrument whenever P4 mode is on and the guest actually enters.
         */
        if (product_p4_enabled() && seq) {
            uint32_t mt = ext_chunk_provider_mr_table_guest();
            uint32_t chunk = ext_chunk_provider_last_chunk_guest();
            uint32_t pg = ext_chunk_provider_last_p_guest();
            uint32_t draw_fp = 0, disp_fp = 0;
            if (uc && mt) {
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, mt + 0x1E0u, &draw_fp);
                (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, mt + 0x1E4u, &disp_fp);
            }
            if (draw_fp) product_p4_note_display_target("draw_blit", draw_fp);
            if (disp_fp) product_p4_note_display_target("_DispUpEx", disp_fp);
            product_p4_callback_begin(uc, &snap, owner ? owner->map.guest_code_base : 0,
                                      owner ? owner->map.guest_code_size : 0, r9_run, pg, chunk);
        }

        ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, handler, stop,
                                          GWY_LIFECYCLE_INSN_LIMIT, &abi, &out);

        if (product_p4_enabled() && seq)
            product_p4_callback_end(uc, seq, ok, out.uc_err,
                                    out.end_reason[0] ? out.end_reason : "?",
                                    (int32_t)out.r0_after);

        product_callback_trace_handler_return(seq, out.pc_after, (int32_t)out.r0_after);
        product_callback_trace_frame_restore_begin(seq, r9_save);
        (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
        product_callback_trace_frame_restore_complete(seq, r9_save, 1);
        product_callback_trace_on_leave(seq, ok, out.uc_err, out.end_reason[0] ? out.end_reason : "?",
                                        out.pc_after, out.lr_after, out.sp_after, out.r9_after,
                                        (int32_t)out.r0_after, r9_save);
        platform_timer_cadence_note_inflight_end(1u, occ, ok);

        {
            GwyScheduledWork delivered;
            int have = platform_scheduler_try_dequeue(0, &delivered);
            if (!have) {
                memset(&delivered, 0, sizeof(delivered));
                delivered.source = GWY_SCHED_SRC_PLATFORM_TIMER;
                delivered.handler_or_helper = handler;
                delivered.forced = forced_tick;
                delivered.owner_module_id = owner ? owner->module_id : 0;
                snprintf(delivered.owner_module, sizeof(delivered.owner_module), "%s",
                         snap.owner_module);
            }
            if (!delivered.forced)
                platform_scheduler_note_natural_callback(&delivered, (int32_t)out.r0_after, 1);
            if (platform_scheduler_natural_callback_observed()) {
                char path[512];
                FILE *csv;
                const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
                if (root && root[0])
                    snprintf(path, sizeof(path), "%s/product_scheduler_natural_callback.csv", root);
                else
                    snprintf(path, sizeof(path), "reports/product_scheduler_natural_callback.csv");
                csv = fopen(path, "a");
                if (!csv) {
                    csv = fopen(path, "w");
                    if (csv) fprintf(csv, "run_id,source,handler,ret,forced\n");
                }
                if (csv) {
                    fprintf(csv, "%s,%s,0x%X,%d,%d\n", platform_scheduler_run_id(),
                            gwy_scheduler_source_name(delivered.source), delivered.handler_or_helper,
                            (int)out.r0_after, delivered.forced);
                    fclose(csv);
                }
            }
        }
    }
    if (handler_forensic_enabled() && g_lifecycle_ticks == 1u) {
        handler_forensic_end(uc, ok, out.uc_err, out.end_reason[0] ? out.end_reason : "?",
                             out.pc_after, out.r0_after, out.r9_after, out.sp_after, out.lr_after,
                             out.cpsr_after);
    }
    printf("[JJFB_LIFECYCLE] op=FIRE_DONE tick=%u ok=%d end=%s pc_after=0x%X r0_after=0x%X "
           "r9_after=0x%X sp_after=0x%X uc_err=%u detail=%s evidence=OBSERVED\n",
           g_lifecycle_ticks, ok, out.end_reason[0] ? out.end_reason : "?", out.pc_after,
           out.r0_after, out.r9_after, out.sp_after, out.uc_err,
           out.err_detail[0] ? out.err_detail : "-");
    if (!ok) {
        robotol_flag_writer_trace_on_lifecycle_fault(uc, g_lifecycle_ticks, ok, out.uc_err,
                                                     out.pc_after, out.r0_after, out.r9_after,
                                                     out.sp_after, out.lr_after);
    }
    if (!ok && out.pc_after) {
        uint8_t peek[8];
        if (guest_memory_uc_peek((struct uc_struct *)uc, out.pc_after & ~1u, peek, sizeof(peek))) {
            printf("[JJFB_LIFECYCLE] op=FAULT_BYTES pc=0x%X bytes=%02X%02X%02X%02X%02X%02X%02X%02X "
                   "evidence=OBSERVED\n",
                   out.pc_after & ~1u, peek[0], peek[1], peek[2], peek[3], peek[4], peek[5],
                   peek[6], peek[7]);
        }
    }
    if (platform_call_census_enabled() &&
        (g_lifecycle_ticks == 1u || (g_lifecycle_ticks % 25u) == 0u)) {
        platform_call_census_dump("lifecycle_tick");
    }
    if ((g_lifecycle_ticks % 10u) == 0u) platform_timer_cadence_flush_csv();
    robotol_idle_watch_set_tick(g_lifecycle_ticks);
    robotol_flag_writer_trace_set_tick(g_lifecycle_ticks);
    robotol_flag_writer_trace_try_arm(uc);
    if (g_lifecycle_ticks == 1u)
        robotol_idle_watch_note_stage(uc, "first_10140_tick");
    else if (g_lifecycle_ticks == 40u)
        robotol_idle_watch_note_stage(uc, "tick_40");
    else
        robotol_idle_watch_snap(uc, "lifecycle_fire_done");
    /*
     * Drain-order B (default): 10140 then one-shot 10165 probe after first tick.
     * Order A/C already fired 10165 (probe_done suppresses second fire).
     */
    if (g_lifecycle_ticks == 1u && drain != 'A' && drain != 'C') {
        if (drain == 'B') {
            printf("[JJFB_E8E_DRAIN_ORDER] order=B phase=10140_then_10165 evidence=HYPOTHESIS\n");
            fflush(stdout);
        }
        robotol_idle_watch_try_10165_probe(uc);
    }
    /* E8F: writer BP summary + sibling/counterfactual after tick returns (depth 0). */
    robotol_flag_writer_trace_on_lifecycle(uc, g_lifecycle_ticks);
    /* Family events posted during this 10140 tick become deliverable now. */
    gwy_ext_obs_drain_family_events(uc);
    if (product_ffp_enabled() && g_lifecycle_ticks >= 2u && (g_lifecycle_ticks % 4u) == 0u)
        product_ffp_finalize();
    if (product_p5_enabled() && g_lifecycle_ticks >= 4u && (g_lifecycle_ticks % 8u) == 0u)
        product_p5_finalize();
    if (product_p4_enabled() && product_p4_callback_count() >= 8u &&
        (product_p4_callback_count() >= GWY_P4_MAX_CALLBACKS || (g_lifecycle_ticks % 16u) == 0u))
        product_p4_finalize();
    fflush(stdout);
    /* R9 already restored in traced path above. */
}

int gwy_ext_obs_lifecycle_on_timer_due(void *uc) {
    if (product_callback_trace_halted()) {
        platform_timer_cadence_flush_csv();
        return 0;
    }
    if (g_armed_timer_chunk) return 0;
    if (!g_lifecycle_host_armed && !platform_handler_registry_has(0x10140u)) return 0;
    if (g_lifecycle_delivering) {
        g_lifecycle_pending = 1;
        return 1;
    }
    g_lifecycle_delivering = 1;
    gwy_ext_obs_lifecycle_deliver(uc);
    if (g_lifecycle_host_armed && !product_callback_trace_halted()) {
        platform_timer_start(GWY_LIFECYCLE_PERIOD_MS);
        if (g_timer_start) (void)g_timer_start((uint16_t)GWY_LIFECYCLE_PERIOD_MS);
        platform_timer_cadence_note_arm(1u, 0, GWY_LIFECYCLE_PERIOD_MS, 1, "lifecycle_rearm");
    }
    g_lifecycle_delivering = 0;
    return 1;
}

void gwy_ext_obs_post_start_loop_tick(uint32_t t_ms) {
    static uint32_t last_sum;
    uint32_t due_in = 0;
    int active;
    const char *reason;
    if (!env_flag("JJFB_POST_START_SCHEDULER_TRACE")) return;
    if (!g_start_dsm_returned) {
        if ((g_post_loop_iter++ % 40u) == 1u) {
            printf("[JJFB_SCHEDULER_NOT_POLLED] reason=start_dsm_not_returned_yet "
                   "evidence=OBSERVED\n");
            fflush(stdout);
        }
        return;
    }
    g_post_loop_iter++;
    if (last_sum && (t_ms - last_sum) < 1000u) return;
    last_sum = t_ms ? t_ms : 1u;
    active = platform_timer_running();
    if (!active)
        reason = "no_active";
    else if ((int32_t)(t_ms - platform_timer_deadline_ms()) < 0)
        reason = "not_due";
    else
        reason = "due_pending_take";
    if (active && platform_timer_deadline_ms() > t_ms)
        due_in = platform_timer_deadline_ms() - t_ms;
    printf("[JJFB_POST_START_LOOP] t_ms=%u loop_iter=%u timers_active=%d next_due_ms=%u "
           "events_pending=0 last_module=? draw_count=%u refresh_count=%u "
           "arm_seen=%d evidence=OBSERVED\n",
           t_ms, g_post_loop_iter, active, due_in, g_draw_count, g_refresh_count,
           g_timer_arm_seen);
    if (env_flag("JJFB_TIMER_POLL_TRACE")) {
        printf("[JJFB_TIMER_POLL] t_ms=%u active=%d due=%d fired=0 reason=%s period_ms=%u "
               "evidence=OBSERVED\n",
               t_ms, active,
               active && (int32_t)(t_ms - platform_timer_deadline_ms()) >= 0 ? 1 : 0, reason,
               platform_timer_period_ms());
    }
    fflush(stdout);
}

static void gwy_ext_obs_deferred_timer_pump(void *uc) {
    int due;
    if (g_timer_flushing) return;
    due = platform_timer_take_due();
    if (!due) {
        /* Defer recorded a due that guest may have re-armed over. Still owe one FIRE. */
        platform_timer_signal_due();
        due = platform_timer_take_due();
    }
    if (!due) {
        printf("[PLATFORM_TIMER] op=DEFERRED_PUMP_SKIP reason=no_due evidence=OBSERVED\n");
        fflush(stdout);
        return;
    }
    g_timer_flushing = 1;
    printf("[PLATFORM_TIMER] op=FIRE_DUE via=deferred_pump evidence=DOCUMENTED\n");
    fflush(stdout);
    e10a31c_mark_milestone("DEFERRED_TIMER_FIRED_AFTER_INIT",
                           e10a31c_init_sequence_complete() ? "after_init_complete"
                                                            : "after_init_tx_end");
    if (g_timer_deliver)
        g_timer_deliver(uc);
    else if (!gwy_ext_obs_lifecycle_on_timer_due(uc) && g_timer_stop)
        (void)g_timer_stop();
    g_timer_flushing = 0;
    if (env_flag("JJFB_PLATFORM_TIMER_DISPATCH") && g_timer_last_period_ms > 0u &&
        g_timer_last_period_ms <= 60000u) {
        platform_timer_start(g_timer_last_period_ms);
        printf("[JJFB_PLATFORM_TIMER_DISPATCH] op=REARM period_ms=%u id=%u "
               "note=deferred_pump_periodic evidence=OBSERVED\n",
               g_timer_last_period_ms, g_timer_last_id ? g_timer_last_id : 1u);
        fflush(stdout);
    }
    printf("[PLATFORM_TIMER] op=FIRE_DONE via=deferred_pump evidence=DOCUMENTED\n");
    fflush(stdout);
}

static void gwy_ext_obs_timer_poll(void *uc) {
    static uint32_t wait_hb;
    int due;
    if (g_timer_flushing) return;
    if (g_lifecycle_pending && !g_lifecycle_delivering) {
        (void)gwy_ext_obs_lifecycle_on_timer_due(uc);
    }
    if (platform_timer_running()) {
        wait_hb++;
        if ((wait_hb % 200u) == 1u) {
            printf("[PLATFORM_TIMER] op=POLL_WAIT period_ms=%u evidence=DOCUMENTED\n",
                   platform_timer_period_ms());
            fflush(stdout);
        }
    }
    /* E10A-3.1c: never FIRE while guest helper/init/dispatch is active. */
    if (e10a31c_enabled() && e10a31c_should_defer_timer() && platform_timer_is_due()) {
        uint32_t helper = 0, p_guest = 0, erw = 0;
        (void)gwy_ext_obs_timer_ext_target(&helper, &p_guest, &erw);
        e10a31c_note_timer_defer(uc, helper);
        return;
    }
    due = platform_timer_take_due();
    if (!due) return;
    g_timer_flushing = 1;
    printf("[PLATFORM_TIMER] op=FIRE_DUE via=emu_slice_poll evidence=DOCUMENTED\n");
    if (env_flag("JJFB_TIMER_DELIVER_TRACE")) {
        printf("[JJFB_TIMER_FIRE] timer_id=%u t_ms=0 route=emu_slice_poll "
               "evidence=DOCUMENTED\n",
               g_timer_last_id ? g_timer_last_id : 1u);
        fflush(stdout);
    }
    fflush(stdout);
    /* Prefer bridge deliver (EXT chunk or lifecycle via gwy_ext_obs_lifecycle_on_timer_due). */
    if (g_timer_deliver)
        g_timer_deliver(uc);
    else if (!gwy_ext_obs_lifecycle_on_timer_due(uc) && g_timer_stop)
        (void)g_timer_stop();
    g_timer_flushing = 0;
    /* E9V: Maopao splash progress expects periodic timer callbacks.
     * platform_timer_take_due is one-shot; re-arm when dispatch compat is on. */
    if (env_flag("JJFB_PLATFORM_TIMER_DISPATCH") && g_timer_last_period_ms > 0u &&
        g_timer_last_period_ms <= 60000u) {
        platform_timer_start(g_timer_last_period_ms);
        printf("[JJFB_PLATFORM_TIMER_DISPATCH] op=REARM period_ms=%u id=%u "
               "note=periodic_compat NOT_PRODUCT evidence=OBSERVED\n",
               g_timer_last_period_ms, g_timer_last_id ? g_timer_last_id : 1u);
        fflush(stdout);
    }
    printf("[PLATFORM_TIMER] op=FIRE_DONE via=emu_slice_poll evidence=DOCUMENTED\n");
    fflush(stdout);
}

uint32_t gwy_ext_obs_sendappevent_dispatch(void *uc) {
    uint32_t pc = 0, lr = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, r4 = 0, sp = 0, r9 = 0;
    uint32_t ret = 0;
    uint32_t arg4 = 0;
    uint32_t caller_pc = 0;
    GwyPlatCall call;
    GwyPlatCallResult result;

#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &lr);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0, &r0);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R1, &r1);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R2, &r2);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R3, &r3);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R4, &r4);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &sp);
        (void)guest_memory_uc_read_r9((struct uc_struct *)uc, &r9);
        /* DOCUMENTED 5th sendAppEvent arg (period) lives at SP[0]. */
        if (sp && !guest_memory_uc_peek_u32((struct uc_struct *)uc, sp, &arg4))
            arg4 = 0;
    }
#else
    (void)uc;
#endif
    /* Prefer LR (guest return site) over stub PC for census/1E209 observe. */
    caller_pc = lr ? lr : pc;

    memset(&call, 0, sizeof(call));
    call.code = r0;
    call.app = r1;
    call.arg2 = r2;
    call.arg3 = r3;
    call.arg4 = arg4;
    /* Prefer exact published chunk match (r0 or r1) over last-only. */
    if (ext_chunk_provider_lookup_chunk(r0, NULL))
        call.known_chunk = r0;
    else if (ext_chunk_provider_lookup_chunk(r1, NULL))
        call.known_chunk = r1;
    else
        call.known_chunk = ext_chunk_provider_last_chunk_guest();
    platform_send_app_event_classify(&call, &result);
    robotol_idle_watch_helper_fx_begin(r0, r1);
    robotol_idle_watch_try_arm(uc);

    /* E9O: formal platform drawText 0x11F00 (via 2F2360 → 304558 → slot+0x28).
     * Prefer JJFB_PLATFORM_TEXT_API_11F00 over E9N diagnostic textshim. */
    if (r0 == 0x11F00u) {
        if (jjfb_plat_11f00_handle(uc, r1, r2, r3, pc, lr)) {
            ret = 0; /* MR_SUCCESS */
            ext_chunk_provider_on_slot28_call(pc, r0, r1, r2, r3, r4, ret);
            return ret;
        }
        if (jjfb_e9n_try_plat_11f00_text_draw(uc, r1, r2, r3)) {
            ret = 0;
            ext_chunk_provider_on_slot28_call(pc, r0, r1, r2, r3, r4, ret);
            return ret;
        }
        if ((getenv("JJFB_E9N_MODE") && getenv("JJFB_E9N_MODE")[0] == '1') ||
            (getenv("JJFB_E9O_MODE") && getenv("JJFB_E9O_MODE")[0] == '1')) {
            printf("[JJFB_E9N_CLASS] class=TEXT_305C3C_BLOCKED_BY_PLATFORM_TEXT_API "
                   "note=0x11F00_unhandled app=0x%X code=0x%X p0=0x%X "
                   "evidence=OBSERVED\n",
                   r1, r2, r3);
            fflush(stdout);
        }
    }

    /* E9Q: formal platform text-measure 0x12340. Outs flushed @ 0x305EA0 via R4/R7. */
    if (r0 == 0x12340u) {
        if (jjfb_plat_12340_handle(uc, r1, r2, r3, pc, lr, sp)) {
            ret = 0; /* MR_SUCCESS */
            ext_chunk_provider_on_slot28_call(pc, r0, r1, r2, r3, r4, ret);
            return ret;
        }
        if (getenv("JJFB_E9Q_MODE") && getenv("JJFB_E9Q_MODE")[0] == '1') {
            printf("[JJFB_E9Q_CLASS] class=PLATFORM_TEXT_MEASURE_12340_ABI_WRONG "
                   "note=unhandled app=0x%X code=0x%X evidence=OBSERVED\n",
                   r1, r2);
            fflush(stdout);
        }
    }

    if (env_flag("JJFB_TIMER_ARM_TRACE") &&
        (result.kind == GWY_PLAT_KIND_TIMER_START || result.kind == GWY_PLAT_KIND_TIMER_STOP ||
         (result.kind == GWY_PLAT_KIND_STATUS && (r0 == 0u || (r1 >= 1u && r1 <= 60000u))))) {
        printf("[JJFB_TIMER_ARM_ATTEMPT] module=? pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
               "r4=0x%X delay_ms=%u period_ms=%u route=sendAppEvent kind=%d name=%s "
               "evidence=OBSERVED\n",
               pc, r0, r1, r2, r3, r4, r1, r3 ? r3 : r1, (int)result.kind,
               result.name ? result.name : "?");
        fflush(stdout);
    }

    if (result.kind == GWY_PLAT_KIND_GRAPHICS_FP) {
        uint32_t mt = ext_chunk_provider_mr_table_guest();
        uint32_t fp = 0;
        int wrote = 0;
        /* DOCUMENTED: mr_table._DrawBitmap at +0x1E0 (mr_helper / mrc port table). */
        if (uc && mt && (result.graphics_id == 0x11F02u || result.graphics_id == 0x11F03u ||
                         result.graphics_id == 0x11F04u)) {
            if (guest_memory_uc_peek_u32((struct uc_struct *)uc, mt + 0x1E0u, &fp) && fp &&
                result.graphics_out &&
                guest_memory_uc_poke_u32((struct uc_struct *)uc, result.graphics_out, fp))
                wrote = 1;
        }
        ret = result.status_ret;
        if (env_flag("JJFB_PLAT_RET0_TRACE") || env_flag("JJFB_MRC_INIT_TRACE")) {
            printf("[JJFB_PLAT_CALL] code=0x%X app=0x%X arg2=0x%X out=0x%X fp=0x%X wrote=%d "
                   "ret=%d kind=GRAPHICS_FP name=%s evidence=%s\n",
                   r0, r1, r2, result.graphics_out, fp, wrote, (int)ret,
                   result.name ? result.name : "?", result.evidence ? result.evidence : "?");
            fflush(stdout);
        }
    } else if (result.kind == GWY_PLAT_KIND_REGISTER) {
        ret = result.status_ret;
        if (result.reg_handler) {
            ModuleRegistry *reg = gwy_ext_loader_bound_registry();
            const GwyLoadedModule *owner =
                reg ? module_registry_find_by_code_addr(reg, result.reg_handler & ~1u) : NULL;
            const char *oname = owner
                                    ? (owner->resolved_name[0] ? owner->resolved_name
                                                               : owner->requested_name)
                                    : NULL;
            ExtChunkOwnerInfo oi;
            uint64_t gen = 0;
            GwyHandlerIsa isa =
                (result.reg_handler & 1u) ? GWY_HANDLER_ISA_THUMB : GWY_HANDLER_ISA_ARM;
            memset(&oi, 0, sizeof(oi));
            if (owner && owner->map.helper_address &&
                ext_chunk_provider_owner_for_helper(owner->map.helper_address, &oi))
                gen = oi.module_generation;
            if (!gen && owner) gen = owner->module_id;
            if (owner && oname) {
                (void)platform_handler_registry_register_owned(
                    r0, result.reg_family, result.reg_handler, owner->module_id, gen, oname, isa,
                    result.name ? result.name : "sendAppEvent");
            } else {
                (void)platform_handler_registry_register(r0, result.reg_family, result.reg_handler);
            }
            robotol_idle_watch_on_handler_register(r0, result.reg_family, result.reg_handler);
            if (owner && oname && strstr(oname, "robotol"))
                platform_scheduler_note_work_source(GWY_SCHED_SRC_PLATFORM_EVENT);
        }
        if (env_flag("JJFB_PLAT_RET0_TRACE") || env_flag("JJFB_MRC_INIT_TRACE")) {
            printf("[JJFB_PLAT_CALL] code=0x%X family=0x%X handler=0x%X ret=%d kind=REGISTER "
                   "name=%s evidence=%s\n",
                   r0, result.reg_family, result.reg_handler, (int)ret,
                   result.name ? result.name : "?", result.evidence ? result.evidence : "?");
            fflush(stdout);
        }
    } else if (result.kind == GWY_PLAT_KIND_USERINFO_BLOB) {
        if (e10a3_enabled()) {
            e10a3_note_event_10180("request", caller_pc, lr, "sendAppEvent", r0, r1, r2, r3, r4, 0,
                                   0, 0, 0, 0, 0, 0, NULL, NULL, 0, 0, 0,
                                   "platform_send_app_event", 1, "platform_userinfo", 0, 0, 0,
                                   "synchronous_query", "sendAppEvent_entry");
            e10a_shell_phase("SHELL_PHASE_USERINFO_REQUEST", "gamelist.ext", caller_pc, lr, r0, r1,
                             r2, r3, 0, 0, "app=subcode");
        }
        if (!g_userinfo_guest && g_guest_alloc && g_guest_to_ptr) {
            void *host = g_guest_alloc(GWY_USERINFO_BLOB_BYTES);
            if (host) {
                memcpy(host, result.userinfo.bytes, GWY_USERINFO_BLOB_BYTES);
                g_userinfo_guest = g_guest_to_ptr(host);
                printf("[PLATFORM_10180] blob=0x%X size=0x%X layout=mr_userinfo "
                       "ver=0x%X evidence=%s\n",
                       g_userinfo_guest, (unsigned)GWY_USERINFO_BLOB_BYTES, result.userinfo.ver,
                       result.evidence ? result.evidence : "?");
                fflush(stdout);
            }
        }
        ret = g_userinfo_guest;
        if (!ret) {
            printf("[PLATFORM_10180] blob_alloc_failed evidence=%s\n",
                   result.evidence ? result.evidence : "?");
            fflush(stdout);
        } else if (e10a3_enabled()) {
            char blob_hex[129];
            char decoded[96];
            size_t bi;
            blob_hex[0] = decoded[0] = 0;
            if (uc && guest_memory_uc_peek((struct uc_struct *)uc, ret, (uint8_t *)decoded, 63)) {
                for (bi = 0; bi < 32u; bi++) {
                    uint8_t b = 0;
                    char t[3];
                    if (!guest_memory_uc_peek((struct uc_struct *)uc, ret + (uint32_t)bi, &b, 1))
                        break;
                    snprintf(t, sizeof(t), "%02X", (unsigned)b);
                    strncat(blob_hex, t, sizeof(blob_hex) - strlen(blob_hex) - 1);
                }
            }
            e10a3_note_event_10180("return", caller_pc, lr, "platform_userinfo", r0, r1, r2, r3,
                                   r4, 0, 0, 0, 0, 0, ret, GWY_USERINFO_BLOB_BYTES, blob_hex,
                                   decoded, 0, 0, r1, "platform_userinfo_fill", 1,
                                   "platform_userinfo", 1, 0, ret, "synchronous_query",
                                   "blob_returned");
            e10a_shell_phase("SHELL_PHASE_USERINFO_RESPONSE", "gamelist.ext", caller_pc, lr, ret,
                             r1, 0, 0, 0, 0, "blob_sync");
        }
    } else if (result.kind == GWY_PLAT_KIND_ALLOC) {
        if (g_guest_alloc && g_guest_to_ptr && result.alloc_size) {
            void *host = g_guest_alloc(result.alloc_size);
            if (host) {
                memset(host, 0, result.alloc_size);
                if (result.alloc_u16_at0) {
                    uint16_t tag = result.alloc_u16_at0;
                    memcpy(host, &tag, sizeof(tag));
                }
                ret = g_guest_to_ptr(host);
            }
        }
        /* 0x10165/10162: alloc size in R1 + optional enqueue handler in R2. */
        if (result.reg_handler) {
            ModuleRegistry *reg = gwy_ext_loader_bound_registry();
            const GwyLoadedModule *owner =
                reg ? module_registry_find_by_code_addr(reg, result.reg_handler & ~1u) : NULL;
            const char *oname = owner
                                    ? (owner->resolved_name[0] ? owner->resolved_name
                                                               : owner->requested_name)
                                    : NULL;
            ExtChunkOwnerInfo oi;
            uint64_t gen = 0;
            GwyHandlerIsa isa =
                (result.reg_handler & 1u) ? GWY_HANDLER_ISA_THUMB : GWY_HANDLER_ISA_ARM;
            memset(&oi, 0, sizeof(oi));
            if (owner && owner->map.helper_address &&
                ext_chunk_provider_owner_for_helper(owner->map.helper_address, &oi))
                gen = oi.module_generation;
            if (!gen && owner) gen = owner->module_id;
            if (owner && oname) {
                (void)platform_handler_registry_register_owned(
                    r0, result.reg_family, result.reg_handler, owner->module_id, gen, oname, isa,
                    result.name ? result.name : "sendAppEvent_alloc");
            } else {
                (void)platform_handler_registry_register(r0, result.reg_family, result.reg_handler);
            }
            robotol_idle_watch_on_handler_register(r0, result.reg_family, result.reg_handler);
            if (owner && oname && strstr(oname, "robotol"))
                platform_scheduler_note_work_source(GWY_SCHED_SRC_PLATFORM_EVENT);
        }
        printf("[PLATFORM_ALLOC] code=0x%X size=0x%X buf=0x%X ret=0x%X tag_u16=%u name=%s "
               "evidence=%s\n",
               r0, result.alloc_size, r2, ret, (unsigned)result.alloc_u16_at0,
               result.name ? result.name : "?", result.evidence ? result.evidence : "?");
        if (product_ffp_enabled() && ret) {
            product_ffp_on_alloc(uc, r0, result.alloc_size, ret, result.reg_handler, r9,
                                0 /* owner filled via registry path above */);
        }
        if (product_eqb_enabled() && ret && (r0 == 0x10162u || r0 == 0x10165u)) {
            uint32_t rr[8];
            memset(rr, 0, sizeof(rr));
            rr[0] = r0;
            rr[1] = r1;
            rr[2] = r2;
            rr[3] = r3;
            product_eqb_on_alloc(uc, r0, result.alloc_size, ret, result.reg_handler, r9, r9, rr, 0, 0,
                                 caller_pc, lr);
            product_eqb_on_platform(r0, r1, r2);
        }
        if (env_flag("JJFB_PLAT_RET0_TRACE") || env_flag("JJFB_MRC_INIT_TRACE")) {
            printf("[JJFB_PLAT_CALL] code=0x%X app=0x%X size=0x%X ret=0x%X kind=ALLOC name=%s "
                   "handler=0x%X evidence=%s\n",
                   r0, r1, result.alloc_size, ret, result.name ? result.name : "?",
                   result.reg_handler, result.evidence ? result.evidence : "?");
        }
        fflush(stdout);
    } else if (result.kind == GWY_PLAT_KIND_BUFFER_FILL) {
        static int g_101ab_with_rec = 1;
        uint8_t tmp[192];
        uint32_t n = 0;
        ret = result.status_ret;
        if (uc && result.fill_buf) {
            n = platform_101ab_fill_path_a(tmp, (uint32_t)sizeof(tmp), g_101ab_with_rec);
            if (n && guest_memory_uc_poke((struct uc_struct *)uc, result.fill_buf, tmp, n)) {
                if (g_101ab_with_rec) g_101ab_with_rec = 0;
                printf("[PLATFORM_BUFFER_FILL] code=0x101AB buf=0x%X type=%u bytes=%u "
                       "with_rec=%d name=%s evidence=%s\n",
                       result.fill_buf, result.fill_type, n, g_101ab_with_rec ? 0 : 1,
                       result.name ? result.name : "?", result.evidence ? result.evidence : "?");
                fflush(stdout);
            } else {
                printf("[PLATFORM_BUFFER_FILL] code=0x101AB buf=0x%X FAILED n=%u evidence=OBSERVED\n",
                       result.fill_buf, n);
                fflush(stdout);
            }
        }
        if (env_flag("JJFB_PLAT_RET0_TRACE") || env_flag("JJFB_MRC_INIT_TRACE")) {
            printf("[JJFB_PLAT_CALL] code=0x%X app=0x%X arg2=0x%X arg3=0x%X ret=%d "
                   "kind=BUFFER_FILL name=%s evidence=%s\n",
                   r0, r1, r2, r3, (int)ret, result.name ? result.name : "?",
                   result.evidence ? result.evidence : "?");
            fflush(stdout);
        }
    } else if (result.kind == GWY_PLAT_KIND_TIMER_START) {
        platform_timer_start(result.timer_period_ms);
        g_armed_timer_chunk = result.timer_chunk;
        e10a31_timer_arm(uc, caller_pc, lr, r9, result.timer_chunk,
                         result.timer_id ? result.timer_id : (uint32_t)(g_timer_arm_count + 1),
                         result.timer_period_ms);
        g_timer_arm_seen = 1;
        g_timer_arm_count++;
        g_timer_last_period_ms = result.timer_period_ms;
        g_timer_last_id = result.timer_id ? result.timer_id : (uint32_t)g_timer_arm_count;
        platform_scheduler_note_work_source(GWY_SCHED_SRC_PLATFORM_TIMER);
        {
            GwyScheduledWork w;
            uint32_t depth = module_r9_switch_depth();
            memset(&w, 0, sizeof(w));
            w.source = GWY_SCHED_SRC_PLATFORM_TIMER;
            w.handler_or_helper = platform_handler_registry_get(0x10140u);
            w.method_or_event = 2u;
            w.due_time = 0;
            w.forced = 0;
            snprintf(w.owner_module, sizeof(w.owner_module), "robotol.ext");
            (void)platform_scheduler_enqueue(&w, depth);
        }
        if (g_timer_start && result.timer_period_ms) {
            (void)g_timer_start((uint16_t)result.timer_period_ms);
        }
        if (result.timer_chunk)
            (void)ext_chunk_provider_set_timer_field(result.timer_chunk, result.timer_id);
        ret = result.status_ret;
        printf("[PLATFORM_TIMER] op=START chunk=0x%X period_ms=%u id=0x%X name=%s "
               "evidence=%s\n",
               result.timer_chunk, result.timer_period_ms, result.timer_id,
               result.name ? result.name : "?", result.evidence ? result.evidence : "?");
        if (env_flag("JJFB_TIMER_ARM_TRACE")) {
            printf("[JJFB_TIMER_ARM] timer_id=%u owner_package=? owner_module=? delay_ms=%u "
                   "period_ms=%u source_pc=0x%X route=sendAppEvent/%s evidence=%s\n",
                   g_timer_last_id, result.timer_period_ms, result.timer_period_ms, pc,
                   result.name ? result.name : "?", result.evidence ? result.evidence : "?");
        }
        fflush(stdout);
    } else if (result.kind == GWY_PLAT_KIND_TIMER_STOP) {
        platform_timer_stop();
        g_armed_timer_chunk = 0;
        e10a31_timer_disarm();
        if (g_timer_stop) (void)g_timer_stop();
        ret = result.status_ret;
        printf("[PLATFORM_TIMER] op=STOP chunk=0x%X name=%s evidence=%s\n", result.timer_chunk,
               result.name ? result.name : "?", result.evidence ? result.evidence : "?");
        if (env_flag("JJFB_TIMER_ARM_TRACE")) {
            printf("[JJFB_TIMER_CANCEL] timer_id=%u owner_module=? reason=sendAppEvent/%s "
                   "pc=0x%X evidence=%s\n",
                   g_timer_last_id, result.name ? result.name : "?", pc,
                   result.evidence ? result.evidence : "?");
        }
        fflush(stdout);
    } else if (result.kind == GWY_PLAT_KIND_STATUS) {
        ret = result.status_ret;
        if ((env_flag("JJFB_PLAT_RET0_TRACE") || env_flag("JJFB_MRC_INIT_TRACE")) &&
            (r0 == 0x10102u || r0 == 0x10113u || r0 == 0x10120u || r0 == 0x10800u || r0 == 1u)) {
            printf("[JJFB_PLAT_CALL] code=0x%X app=0x%X arg2=0x%X arg3=0x%X ret=%d kind=STATUS "
                   "name=%s evidence=%s\n",
                   r0, r1, r2, r3, (int)ret, result.name ? result.name : "?",
                   result.evidence ? result.evidence : "?");
            fflush(stdout);
        }
        /* Proven P4 gap: guest posts family ops (e.g. 0x1E209) after 10102 register;
         * returning STATUS alone never delivered the registered handler. */
        if (platform_handler_registry_find_family_event(r0))
            (void)gwy_ext_obs_note_family_event(r0, r1);
    } else {
        ret = 0;
        if (env_flag("JJFB_PLAT_RET0_TRACE") || env_flag("JJFB_MRC_INIT_TRACE")) {
            printf("[JJFB_PLAT_CALL] code=0x%X app=0x%X arg2=0x%X ret=%d kind=%d name=%s "
                   "evidence=%s note=fallback\n",
                   r0, r1, r2, (int)ret, (int)result.kind, result.name ? result.name : "?",
                   result.evidence ? result.evidence : "?");
            fflush(stdout);
        }
        if (platform_handler_registry_find_family_event(r0))
            (void)gwy_ext_obs_note_family_event(r0, r1);
    }

    ext_chunk_provider_on_slot28_call(pc, r0, r1, r2, r3, r4, ret);
    platform_call_census_note(r0, r1, caller_pc, ret);
    e10a31a_note_platform_api(uc, result.name, caller_pc, 0, r0, ret);
    platform_1e209_trace_call(caller_pc, r0, r1, r2, r3, ret, g_lifecycle_ticks);
    if (result.kind == GWY_PLAT_KIND_GRAPHICS_FP)
        platform_call_census_note_refresh();
    if (product_p4_enabled()) {
        int sync = 1;
        int need_comp = 0;
        int out_init = 1;
        if (result.kind == GWY_PLAT_KIND_REGISTER || result.kind == GWY_PLAT_KIND_ALLOC) {
            /* Handler registration / enqueue alloc: completion is separate work. */
            if (r0 == 0x10102u || r0 == 0x10120u || r0 == 0x10165u) need_comp = 1;
        }
        if (result.kind == GWY_PLAT_KIND_GRAPHICS_FP)
            out_init = (ret == 0);
        if (result.kind == GWY_PLAT_KIND_USERINFO_BLOB) out_init = (ret != 0);
        product_p4_note_platform_call(r0, r1, r2, r3, arg4, ret, caller_pc,
                                      result.name ? result.name : "sendAppEvent", sync, need_comp,
                                      out_init);
        if (need_comp && (r0 == 0x10102u || r0 == 0x10120u || r0 == 0x10165u)) {
            product_p4_note_work("event_handler_registration", r0, r1,
                                 result.reg_handler ? result.reg_handler : r2, 0, 0, "guest", 1, 1,
                                 0, 0, 0);
        }
        if (result.kind == GWY_PLAT_KIND_TIMER_START) {
            product_p4_note_work("timer_registration", r0, r1, r2, 0, 0, "guest", 1, 0, 0, 0, 0);
        }
    }
    robotol_idle_watch_helper_fx_end(r0, r1, ret);
    /* Poll after every plat call — SDL timers do not run during nested emu. */
    gwy_ext_obs_timer_poll(uc);
    return ret;
}

void gwy_ext_obs_extchunk_slot28_call(void *uc) {
    (void)gwy_ext_obs_sendappevent_dispatch(uc);
}

void gwy_ext_obs_c_function_new(uint32_t helper,
                                uint32_t p_len,
                                uint32_t p_guest_addr,
                                uint32_t rw_base,
                                uint32_t rw_size,
                                uint32_t stack_base) {
    gwy_ext_obs_c_function_new_ex(helper, p_len, p_guest_addr, rw_base, rw_size, stack_base,
                                  "HOST_BRIDGE");
}

void gwy_ext_obs_p_update(uint32_t helper,
                          uint32_t rw_base,
                          uint32_t rw_size,
                          uint32_t stack_base) {
    ExtLoader *L = gwy_ext_loader_ensure();
    if (!helper) return;
    if (rw_base) e10a31j_on_erw(g_bound_uc, rw_base, rw_size);
    if (rw_base) gwy_sms_cfg_on_erw(g_bound_uc, helper, rw_base, rw_size);
    ext_module_data_init_on_cfunction_p(helper, 0, 0, rw_base, rw_size);
    ext_er_rw_producer_on_cfunction_p(helper, 0, 0, rw_base, rw_size);
    ext_bootstrap_abi_on_cfunction_p(helper, 0, 0, rw_base, rw_size);
    ext_post_cont_audit_on_cfunction_p(helper, 0, 0, rw_base, rw_size);
    ext_post_cfn_r9_audit_on_cfunction_p(helper, 0, 0, rw_base, rw_size);
    ext_p_extchunk_audit_on_cfunction_p(helper, 0, 0, rw_base, rw_size);
    ext_loader_on_c_function_new(L, helper, 0, 0, rw_base, rw_size, stack_base);
}

void gwy_ext_obs_helper_call(uint32_t helper, uint32_t method, int32_t ret_value) {
    ExtLoader *L = gwy_ext_loader_ensure();
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    const char *mn;
    const char *trace;
    ext_er_rw_producer_on_helper_call(helper, method);
    ext_loader_on_helper_call(L, helper, method, ret_value);
    ext_gwy_shell_native_exec_on_helper_call(helper, method, ret_value);

    /* Product track: DOCUMENTED mythroad case_801 code=0 → mrc_init (not shell-gated). */
    trace = getenv("JJFB_MRC_INIT_TRACE");
    if (trace && trace[0] == '1' && method == 0u && helper) {
        reg = gwy_ext_loader_bound_registry();
        m = reg ? module_registry_find_by_helper(reg, helper) : NULL;
        if (!m && reg) m = module_registry_find_by_code_addr(reg, helper);
        mn = m ? (m->resolved_name[0] ? m->resolved_name : m->requested_name) : NULL;
        if (mn && (strcmp(mn, "robotol.ext") == 0 || strcmp(mn, "mmochat.ext") == 0)) {
            printf("[JJFB_MRC_INIT] module=%s helper=0x%X method=0 ret=%d "
                   "route=mr_extHelper evidence=DOCUMENTED source=mythroad.c:case_801\n",
                   mn, helper, (int)ret_value);
            fflush(stdout);
            /* E8D: ER_RW snapshot after mrc_init; 10165 probe deferred to timer poll. */
            robotol_idle_watch_note_stage(NULL, "after_mrc_init");
        }
    }
}

static int g_pending_ext_init_seq;
static int g_ext_init_seq_delivered;

void gwy_ext_obs_request_ext_init_seq(void) {
    const char *e = getenv("JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE");
    if (!e || e[0] != '1') return;
    if (g_ext_init_seq_delivered || g_pending_ext_init_seq) return;
    g_pending_ext_init_seq = 1;
    printf("[JJFB_INIT_SEQ] queued=1 reason=retry_after_context_restore "
           "evidence=DOCUMENTED source=mythroad.c:mr_doExt/start.mr\n");
    fflush(stdout);
}

int gwy_ext_obs_take_ext_init_seq(void) {
    if (!g_pending_ext_init_seq) return 0;
    g_pending_ext_init_seq = 0;
    g_ext_init_seq_delivered = 1;
    return 1;
}

void gwy_ext_obs_set_product_helper_call(GwyExtHelperCallFn fn) {
    ext_abi_adapter_set_helper_call(fn);
}

void gwy_ext_obs_set_product_appinfo_alloc(GwyExtAppInfoAllocFn fn) {
    ext_abi_adapter_set_appinfo_alloc(fn);
}

void gwy_ext_obs_set_product_run_id(const char *run_id) {
    ext_abi_adapter_set_run_id(run_id);
    ext_lifecycle_set_run_id(run_id);
    platform_scheduler_set_run_id(run_id);
    platform_handler_registry_set_run_id(run_id);
    product_callback_trace_set_run_id(run_id);
    platform_timer_cadence_set_run_id(run_id);
}

void gwy_ext_obs_request_product_handshake(void) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = NULL;
    ExtChunkOwnerInfo owner;
    const char *primary;
    uint64_t gen = 0;
    size_t i;

    if (!reg) return;
    primary = package_scope_active_primary();
    if (primary) m = module_registry_find(reg, primary);
    if (!m) {
        for (i = 0; i < reg->count; i++) {
            const GwyLoadedModule *cand = &reg->modules[i];
            const char *n = cand->resolved_name[0] ? cand->resolved_name : cand->requested_name;
            if (cand->origin == MODULE_ORIGIN_MRP_MEMBER && n && strstr(n, "robotol")) {
                m = cand;
                break;
            }
        }
    }
    if (!m) return;

    memset(&owner, 0, sizeof(owner));
    if (m->map.helper_address &&
        ext_chunk_provider_owner_for_helper(m->map.helper_address, &owner))
        gen = owner.module_generation;
    if (!gen) gen = m->module_id;

    ext_lifecycle_ensure(m->module_id, gen,
                         m->resolved_name[0] ? m->resolved_name : m->requested_name);
    if (!ext_lifecycle_find(m->module_id) ||
        ext_lifecycle_find(m->module_id)->state < EXT_LIFECYCLE_BOOTSTRAP_RETURNED) {
        ext_lifecycle_note_bootstrap_return(m->module_id, 0);
    }
    platform_scheduler_set_live_generation(
        m->module_id, gen, m->resolved_name[0] ? m->resolved_name : m->requested_name);
    ext_abi_adapter_request_handshake(
        m->module_id, gen, m->resolved_name[0] ? m->resolved_name : m->requested_name);
}

int gwy_ext_obs_try_product_handshake(void *uc) {
    GwyExtHandshakeResult r;
    if (!ext_abi_adapter_handshake_pending()) return 0;
    r = ext_abi_adapter_try_deliver(uc);
    if (r == GWY_EXT_HS_CONTEXT_NOT_READY) return 0;
    if (r == GWY_EXT_HS_OK) {
        /* After init=0, classify handler gap if none registered yet. */
        if (!platform_handler_registry_robotol_owned_observed()) {
            printf("[ROBOTOL_INIT_ZERO_NO_HANDLER_REGISTRATION] run_id=%s evidence=OBSERVED\n",
                   ext_abi_adapter_run_id());
            fflush(stdout);
        } else {
            char path_csv[512];
            FILE *csv;
            const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
            if (root && root[0])
                snprintf(path_csv, sizeof(path_csv), "%s/product_robotol_init_trace.csv", root);
            else
                snprintf(path_csv, sizeof(path_csv), "reports/product_robotol_init_trace.csv");
            csv = fopen(path_csv, "w");
            if (csv) {
                fprintf(csv, "run_id,verdict,ret6,ret8,ret0\n");
                fprintf(csv, "%s,ROBOTOL_INIT_RETURN_ZERO,%d,%d,%d\n", ext_abi_adapter_run_id(),
                        (int)ext_abi_adapter_last_version_ret(),
                        (int)ext_abi_adapter_last_appinfo_ret(),
                        (int)ext_abi_adapter_last_init_ret());
                fclose(csv);
            }
        }
        return 1;
    }
    /* Bounded product failure provenance (Lane E). */
    {
        char path_md[512], path_csv[512];
        FILE *f;
        const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
        const char *verdict = "ROBOTOL_INIT_NOT_DISPATCHED";
        if (r == GWY_EXT_HS_VERSION_FAILED) verdict = "ROBOTOL_VERSION_CALL_FAILED";
        else if (r == GWY_EXT_HS_APPINFO_FAILED) verdict = "ROBOTOL_APPINFO_CALL_FAILED";
        else if (r == GWY_EXT_HS_INIT_FAILED) verdict = "ROBOTOL_INIT_PLATFORM_API_FAILED";
        else if (r == GWY_EXT_HS_CONTEXT_NOT_READY) verdict = "ROBOTOL_INIT_CONTEXT_INVALID";
        if (root && root[0]) {
            snprintf(path_md, sizeof(path_md), "%s/product_robotol_init_failure.md", root);
            snprintf(path_csv, sizeof(path_csv), "%s/product_robotol_init_trace.csv", root);
        } else {
            snprintf(path_md, sizeof(path_md), "reports/product_robotol_init_failure.md");
            snprintf(path_csv, sizeof(path_csv), "reports/product_robotol_init_trace.csv");
        }
        f = fopen(path_md, "w");
        if (f) {
            fprintf(f,
                    "# Product Robotol Init Failure\n\n"
                    "- **run_id:** %s\n"
                    "- **verdict:** %s\n"
                    "- **ret6:** %d\n"
                    "- **ret8:** %d\n"
                    "- **ret0:** %d\n"
                    "- **note:** bounded product trace; no E10A SMSCFG assumptions\n",
                    ext_abi_adapter_run_id(), verdict, (int)ext_abi_adapter_last_version_ret(),
                    (int)ext_abi_adapter_last_appinfo_ret(), (int)ext_abi_adapter_last_init_ret());
            fclose(f);
        }
        {
            FILE *csv = fopen(path_csv, "w");
            if (csv) {
                fprintf(csv, "run_id,verdict,ret6,ret8,ret0\n");
                fprintf(csv, "%s,%s,%d,%d,%d\n", ext_abi_adapter_run_id(), verdict,
                        (int)ext_abi_adapter_last_version_ret(),
                        (int)ext_abi_adapter_last_appinfo_ret(),
                        (int)ext_abi_adapter_last_init_ret());
                fclose(csv);
            }
        }
        printf("[ROBOTOL_INIT_FAILURE] verdict=%s ret6=%d ret8=%d ret0=%d run_id=%s "
               "evidence=OBSERVED\n",
               verdict, (int)ext_abi_adapter_last_version_ret(),
               (int)ext_abi_adapter_last_appinfo_ret(), (int)ext_abi_adapter_last_init_ret(),
               ext_abi_adapter_run_id());
        fflush(stdout);
    }
    return 1;
}

void gwy_ext_obs_code_image(uint32_t guest_addr, uint32_t size) {
    ExtLoader *L = gwy_ext_loader_ensure();
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    ext_loader_on_code_image(L, guest_addr, size);
    ext_entry_observe_bootstrap_event("CODE_IMAGE");
    ext_module_entry_abi_on_code_image(guest_addr, size);
    ext_entry_null_contract_on_code_image(guest_addr, size);
    ext_module_data_init_on_code_image(guest_addr, size);
    ext_er_rw_producer_on_code_image(guest_addr, size);
    ext_bootstrap_abi_on_code_image(guest_addr, size);
    ext_gwy_shell_native_exec_on_code_image(guest_addr, size);
    reg = gwy_ext_loader_bound_registry();
    /* Prefer refined raw MRPG base when pad refine already ran. */
    m = reg ? module_registry_find_by_code_addr(reg, guest_addr) : NULL;
    if (!m && reg && guest_addr) {
        size_t i;
        for (i = 0; i < reg->count; i++) {
            const GwyLoadedModule *cand = &reg->modules[i];
            if (cand->origin != MODULE_ORIGIN_MRP_MEMBER) continue;
            if (cand->map.guest_code_base >= guest_addr &&
                cand->map.guest_code_base < guest_addr + 0x20u) {
                m = cand;
                break;
            }
        }
    }
    if (m) {
        const char *mn = m->resolved_name[0] ? m->resolved_name : m->requested_name;
        uint32_t base = m->map.guest_code_base ? m->map.guest_code_base : guest_addr;
        uint32_t sz = m->map.guest_code_size ? m->map.guest_code_size : size;
        ext_mrpgcmap_entry_order_on_module_mapped(mn, base, sz);
    }
}

void gwy_ext_obs_ext_image_raw(uint32_t raw_base) {
    ExtLoader *L = gwy_ext_loader_ensure();
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    ext_loader_on_ext_image_raw(L, raw_base);
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_code_addr(reg, raw_base) : NULL;
    if (m) {
        const char *mn = m->resolved_name[0] ? m->resolved_name : m->requested_name;
        ext_mrpgcmap_entry_order_on_module_mapped(mn, m->map.guest_code_base,
                                                  m->map.guest_code_size);
        printf("[JJFB_SHELL_EXT] package=%s member=%s loaded=yes base=0x%X size=%u "
               "evidence=DOCUMENTED note=raw_base_refine\n",
               mn, mn, m->map.guest_code_base, m->map.guest_code_size);
        fflush(stdout);
    }
}

void gwy_ext_obs_alloc(uint32_t guest_addr, uint32_t size) {
    ExtLoader *L = gwy_ext_loader_ensure();
    ext_loader_on_alloc(L, guest_addr, size);
    if (size >= 0x2Cu) {
        printf("[CONTEXT_ALLOC_CANDIDATE] addr=0x%X size=%u evidence=OBSERVED\n", guest_addr, size);
        fflush(stdout);
    }
    ext_chunk_observe_on_alloc(guest_addr, size);
    ext_module_data_init_on_alloc(guest_addr, size);
    ext_er_rw_producer_on_alloc(guest_addr, size);
}

void gwy_ext_obs_block_copy(uint32_t dst, uint32_t src, uint32_t len) {
    e10a31j_on_block_copy(g_bound_uc, dst, src, len);
    ext_chunk_observe_block_copy(dst, src, len);
    ext_helper_handoff_on_block_copy(dst, src, len);
    ext_dsm_record_on_block_copy(dst, src, len);
    ext_module_data_init_on_block_copy(dst, src, len);
    ext_er_rw_producer_on_block_copy(dst, src, len);
}

void gwy_ext_obs_member_open(const char *guest_path) {
    ExtLoader *L = gwy_ext_loader_ensure();
    ext_loader_on_member_open(L, guest_path);
    ext_er_rw_producer_on_member_open(guest_path);
    ext_gwy_shell_native_exec_on_member_open(guest_path);
}

void gwy_ext_obs_entry_begin(uint32_t helper,
                             uint32_t method,
                             uint32_t p_guest,
                             uint32_t input,
                             uint32_t input_len,
                             uint32_t er_rw,
                             uint32_t sp) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = NULL;
    uint64_t mid = 0;
    if (!reg || !helper) return;
    m = module_registry_find_by_helper(reg, helper);
    if (!m) m = module_registry_find_by_code_addr(reg, helper & ~1u);
    if (!m) return;
    mid = m->module_id;
    /* Host ABI: r0=P, r1=method, r2=input, r3=input_len, r9=ER_RW */
    ext_entry_observe_on_entry_begin(mid, helper & ~1u, method, p_guest, method, input, input_len,
                                     er_rw, sp, 0, 0);
    ext_module_data_init_on_entry_begin(helper, er_rw, er_rw);
    ext_post_cont_audit_on_helper_call(helper, method, p_guest, er_rw);
}

void gwy_ext_obs_note_product_draw(const char *api) {
    (void)api;
    g_draw_count++;
    platform_call_census_note_draw();
    product_callback_trace_note_draw();
    if (product_p4_enabled()) product_p4_note_draw_or_refresh(api ? api : "draw", 0);
}

void gwy_ext_obs_note_product_refresh(const char *api) {
    (void)api;
    g_refresh_count++;
    platform_call_census_note_refresh();
    product_callback_trace_note_refresh();
    if (product_p4_enabled()) product_p4_note_draw_or_refresh(api ? api : "_DispUpEx", 1);
}

void gwy_ext_obs_note_product_framebuffer(const char *api, const char *sha256_hex, int32_t x,
                                          int32_t y, int32_t w, int32_t h, uint32_t nbytes,
                                          int nonempty, int hwnd_visible, int captured) {
    if (sha256_hex && sha256_hex[0]) product_callback_trace_set_fb_sha256(sha256_hex);
    product_callback_trace_note_visual_row(api, x, y, w, h, sha256_hex, nonempty, hwnd_visible,
                                           captured);
    if (sha256_hex && sha256_hex[0])
        product_callback_trace_append_fb_hash(api, sha256_hex, nbytes);
    if (nonempty)
        printf("[FRAMEBUFFER_NONEMPTY] run_id=%s sha256=%s evidence=OBSERVED\n",
               product_callback_trace_run_id(), sha256_hex);
    if (hwnd_visible)
        printf("[HWND_VISIBLE] run_id=%s evidence=OBSERVED\n", product_callback_trace_run_id());
    if (captured)
        printf("[FIRST_NATURAL_FRAME_CAPTURED] run_id=%s evidence=OBSERVED\n",
               product_callback_trace_run_id());
    fflush(stdout);
}

void gwy_ext_obs_mem_fault(void *uc,
                           uint32_t access_type,
                           uint64_t address,
                           uint32_t size,
                           int64_t value) {
    uint32_t pc = 0;
#ifdef GWY_HAVE_UNICORN
    if (uc) uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
#endif
    product_callback_trace_on_mem_fault(pc, (uint32_t)address, size,
                                        (access_type & 2) ? 1 : 0, (access_type & 4) ? 1 : 0);
    e10a31a_runtime_set_stop("mem_fault", "UNICORN_FAULT_BEFORE_CONTINUATION", uc, 0, 0, 0, 0, 0,
                             "guest_mem_fault");
    /* Finalize shell-native gate before heavy fault dumps (process may be killed). */
    ext_mrpgcmap_entry_order_on_mem_fault(pc, (uint32_t)address);
    ext_gwy_shell_native_exec_on_mem_fault(uc, pc);
    ext_mrpgcmap_entry_order_finalize("mem_fault");
    ext_entry_abi_cluster_audit_finalize("mem_fault");
    ext_cfunction_publication_audit_finalize("mem_fault");
    ext_chunk_provider_finalize("mem_fault");
    ext_er_rw_bind_restore_finalize("mem_fault");
    ext_gwy_shell_shim_finalize("mem_fault");
    ext_entry_observe_on_mem_fault(uc, access_type, address, size, value);
    module_r9_switch_abort(uc, "UC_FAULT");
}

int gwy_ext_obs_r9_switch_enter(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                                int call_kind) {
    ModuleR9Scope scope;
    int rc = module_r9_switch_enter_ex(uc, caller_module_id, callee_module_id,
                                       (GwyModuleCallKind)call_kind, "gwy_ext_obs_r9_switch_enter",
                                       &scope);
    (void)scope;
    ext_er_rw_producer_on_r9_switch(uc, caller_module_id, callee_module_id,
                                    (GwyModuleCallKind)call_kind, rc);
    return rc;
}

int gwy_ext_obs_r9_switch_enter_helper(void *uc, uint32_t helper_address, int call_kind) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m;
    uint64_t caller = 0;
    ModuleR9Scope scope;
    int rc;
    if (!reg || !helper_address) return -1;
    m = module_registry_find_by_helper(reg, helper_address);
    if (!m) m = module_registry_find_by_code_addr(reg, helper_address & ~1u);
    if (!m) return -1;
    /* Host path: caller unknown (mythroad); pass 0. */
    rc = module_r9_switch_enter_ex(uc, caller, m->module_id, (GwyModuleCallKind)call_kind,
                                   "bridge_mr_extHelper", &scope);
    if (scope.owns_frame) {
        g_helper_r9_scope = scope;
        g_helper_r9_scope_valid = 1;
    }
    ext_er_rw_producer_on_r9_switch(uc, caller, m->module_id, (GwyModuleCallKind)call_kind, rc);
    return rc;
}

int gwy_ext_obs_r9_switch_leave(void *uc) {
    ModuleR9Scope scope;
    if (g_helper_r9_scope_valid) {
        scope = g_helper_r9_scope;
        g_helper_r9_scope_valid = 0;
        return module_r9_switch_leave_scope(uc, &scope, "bridge_mr_extHelper",
                                            GWY_EMU_EXIT_NORMAL_GUEST_RETURN);
    }
    /* No owned helper scope: leave last enter (will NOOP if unowned). */
    module_r9_switch_last_enter_scope(&scope);
    return module_r9_switch_leave_scope(uc, &scope, "bridge_mr_extHelper",
                                        GWY_EMU_EXIT_NORMAL_GUEST_RETURN);
}

void gwy_ext_obs_r9_switch_abort(void *uc, const char *reason) {
    module_r9_switch_abort(uc, reason);
}

void gwy_ext_obs_r9_write_raw(void *uc, uint32_t new_r9, const char *callsite) {
    GwyR9WriteAudit audit;
    memset(&audit, 0, sizeof(audit));
    audit.reason = GWY_R9_WRITE_BRIDGE_RAW_UC;
    audit.host_callsite = callsite ? callsite : "bridge_raw";
    audit.depth_before = module_r9_switch_depth();
    audit.depth_after = audit.depth_before;
    (void)guest_memory_uc_write_r9_ex((struct uc_struct *)uc, new_r9, &audit);
    {
        GwyR9WriteRecord wr;
        if (guest_memory_r9_last_write(&wr)) ext_r9_scope_audit_on_r9_write(&wr);
    }
}

int gwy_ext_obs_ensure_dsm_r9(void *uc, uint32_t guest_pc_hint) {
    ModuleRegistry *reg;
    const GwyLoadedModule *dsm = NULL;
    uint32_t pc = guest_pc_hint;
    uint32_t cur_r9 = 0;
    uint32_t dsm_r9 = 0;
    size_t i;

    if (!uc) return 0;
    if (!pc) pc = 0x80008u; /* CODE_ADDRESS+8: known DSM-mapped VA for lookup */

    /* Prefer switch path (pc-gated ENSURE_DSM_R9 log). */
    if (module_r9_switch_ensure_dsm_r9(uc, pc)) return 1;

    /*
     * Fallback for shell continue / sticky MRP R9: registry DSM ER_RW even when
     * the pc-gate rejects (e.g. hint not mapped as DSM yet) or switch already
     * skipped because of an edge case. Never trust live mr_c_function_P.
     */
    reg = gwy_ext_loader_bound_registry();
    if (!reg) return 0;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].origin == MODULE_ORIGIN_DSM &&
            reg->modules[i].data.start_of_er_rw) {
            dsm = &reg->modules[i];
            break;
        }
    }
    if (!dsm) return 0;
    dsm_r9 = dsm->data.start_of_er_rw;
    if (!guest_memory_uc_read_r9((struct uc_struct *)uc, &cur_r9)) return 0;
    if (cur_r9 == dsm_r9) return 0;
    gwy_ext_obs_r9_write_raw(uc, dsm_r9, "ensure_dsm_r9_force");
    printf("[R9_SWITCH] stage=ENSURE_DSM_R9_FORCE pc=0x%X old_r9=0x%X new_r9=0x%X "
           "evidence=TARGET_OBSERVED note=registry_dsm_er_rw\n",
           pc, cur_r9, dsm_r9);
    fflush(stdout);
    return 1;
}

void gwy_ext_obs_emu_exit(int reason) {
    char detail[48];
    snprintf(detail, sizeof(detail), "emu_exit_reason=%d", reason);
    e10a31a_runtime_set_stop("emu_exit", "RUNCODE_RETURNED_BEFORE_CONTINUATION", NULL, 0, 0, 0, 0,
                             reason, detail);
    module_r9_switch_set_emu_exit_hint((GwyEmuExitReason)reason);
    ext_mrpgcmap_entry_order_finalize("emu_exit");
    ext_entry_abi_cluster_audit_finalize("emu_exit");
    ext_cfunction_publication_audit_finalize("emu_exit");
    ext_chunk_provider_finalize("emu_exit");
    ext_er_rw_bind_restore_finalize("emu_exit");
    ext_gwy_shell_native_exec_finalize("emu_exit");
    ext_gwy_shell_shim_finalize("emu_exit");
    ext_gwy_startgame_audit_finalize("emu_exit");
}

void gwy_ext_obs_mr_exit(void *uc) {
    e10a31a_runtime_set_stop("mr_exit_api", "GUEST_EXIT_BEFORE_CONTINUATION", uc, 0, 0, 0, 0, 0,
                             "mr_exit_api");
    ext_gwy_shell_shim_emit_exit_source(uc, "mr_exit_api");
    ext_entry_abi_cluster_audit_on_mr_exit(uc);
    ext_entry_abi_cluster_audit_finalize("mr_exit");
    ext_cfunction_publication_audit_finalize("mr_exit");
    ext_chunk_provider_finalize("mr_exit");
    ext_er_rw_bind_restore_finalize("mr_exit");
    ext_gwy_shell_native_exec_finalize("mr_exit");
    ext_gwy_shell_shim_finalize("mr_exit");
    platform_call_census_dump("mr_exit");
}

static char g_continue_target[160];
static char g_continue_param[320];

int gwy_shell_shim_try_continue_after_mr_exit(void *uc) {
    g_continue_target[0] = 0;
    g_continue_param[0] = 0;
    if (!ext_gwy_shell_shim_try_continue_after_mr_exit(uc, g_continue_target,
                                                       sizeof(g_continue_target), g_continue_param,
                                                       sizeof(g_continue_param)))
        return 0;
    if (!g_continue_target[0])
        snprintf(g_continue_target, sizeof(g_continue_target), "%s", "gwy/gamelist.mrp");
    if (!g_continue_param[0]) {
        const char *p = ext_gwy_shell_shim_jjfb_param();
        snprintf(g_continue_param, sizeof(g_continue_param), "%s", p ? p : "");
    }
    return 1;
}

void gwy_ext_obs_e10a31a_br_exit_enter(void *uc) { e10a31a_note_br_exit_enter(uc); }

void gwy_ext_obs_e10a31a_br_exit_fallback(void *uc) {
    GwyContinueSnapshot snap;
    e10a31a_continue_snapshot_fill(uc, &snap);
    e10a31a_log_continue_decision("br_exit_fallback", &snap);
    e10a31a_note_br_exit_fallback(uc, &snap);
}

void gwy_ext_obs_e10a31a_process_exit(int code) { e10a31a_note_br_exit_process_exit(code); }

void gwy_ext_obs_e10a31a_note_font_load(void *uc) { e10a31a_note_font_load_suc(uc); }

void gwy_ext_obs_e10a31a_runtime_stop(const char *source, const char *reason, void *uc,
                                      int32_t return_code, const char *detail) {
    e10a31a_runtime_set_stop(source, reason, uc, 0, 0, 0, 0, return_code, detail);
}

const char *gwy_shell_shim_continue_target(void) {
    return g_continue_target[0] ? g_continue_target : "gwy/gamelist.mrp";
}

const char *gwy_shell_shim_continue_param(void) {
    if (g_continue_param[0]) return g_continue_param;
    return ext_gwy_shell_shim_jjfb_param();
}

void gwy_ext_obs_unimplemented_api(void *uc, uint32_t slot_addr, const char *name) {
    e10a31j_on_unimpl_api(uc, slot_addr, name);
    ext_gwy_startgame_audit_on_plat_or_testcom(uc, name, slot_addr);
    ext_post_cont_audit_note_unimplemented(name);
    printf("[POST_CONT_UNIMPLEMENTED_API] api=%s slot=0x%X evidence=OBSERVED "
           "note=observe_before_exit\n",
           name ? name : "?", slot_addr);
    fflush(stdout);
    (void)uc;
}

void gwy_ext_obs_start_dsm(const char *filename, const char *ext, const char *entry) {
    ext_gwy_startgame_audit_on_start_dsm(filename, ext, entry);
    ext_gwy_shell_shim_on_start_dsm(filename, ext, entry);
    ext_gwy_shell_native_exec_on_start_dsm(filename, ext, entry);
}

void gwy_ext_obs_launch_param_mapped(uint32_t entry_va, const char *entry) {
    ext_gwy_shell_native_exec_on_launch_param(entry_va, entry);
    e10a31_launch_param_mapped(g_bound_uc, entry_va, entry ? (uint32_t)strlen(entry) + 1u : 0,
                               entry);
}

void gwy_ext_obs_note_start_dsm_abi(void *uc, uint32_t start_t_guest, const char *filename,
                                    const char *ext, const char *entry) {
    e10a31_note_start_dsm(uc ? uc : g_bound_uc, start_t_guest, filename, ext, entry);
}

void gwy_ext_obs_file_open(const char *guest_path, int ok) {
    ext_gwy_startgame_audit_on_file_open(guest_path, ok);
}

void gwy_ext_obs_file_open_ex(const char *guest_path, const char *host_path, int ok) {
    ext_gwy_startgame_audit_on_file_open(guest_path, ok);
    ext_gwy_shell_shim_on_file_open(guest_path, host_path, ok);
    ext_gwy_shell_native_exec_on_file_open(guest_path, host_path, ok);
}

int gwy_shell_shim_try_init_network(void *uc, uint32_t cb, const char *mode, uint32_t userData,
                                    int32_t *out_ret) {
    return ext_gwy_shell_shim_try_init_network(uc, cb, mode, userData, out_ret);
}

int gwy_shell_shim_prepare_jjfb_launch(char *out_target, size_t target_cap, char *out_param,
                                       size_t param_cap) {
    return ext_gwy_shell_shim_prepare_jjfb_launch(out_target, target_cap, out_param, param_cap);
}

int gwy_shell_shim_should_chain_jjfb(void) { return ext_gwy_shell_shim_should_chain_jjfb(); }

const char *gwy_shell_shim_jjfb_target(void) { return ext_gwy_shell_shim_jjfb_target(); }

const char *gwy_shell_shim_jjfb_param(void) { return ext_gwy_shell_shim_jjfb_param(); }

void gwy_shell_shim_emit_runapp_chain(void) { ext_gwy_shell_shim_emit_runapp_chain(); }

void gwy_shell_shim_finalize(const char *stop_reason) {
    ext_gwy_shell_native_exec_finalize(stop_reason);
    ext_gwy_shell_shim_finalize(stop_reason);
}

void gwy_ext_obs_on_timer_fire_ext(uint32_t helper, uint32_t p_guest, uint32_t erw, int32_t ret) {
    e10a31_on_timer_fire(g_bound_uc, helper, 2u, p_guest, erw, ret);
}

static uint32_t g_timer_fire_pin_erw;
static uint32_t g_timer_fire_forbid_erw;
static uint32_t g_timer_fire_pin_hits;

void gwy_ext_obs_timer_fire_r9_pin(uint32_t pin_erw, uint32_t forbid_erw) {
    g_timer_fire_pin_erw = pin_erw;
    g_timer_fire_forbid_erw = forbid_erw;
    g_timer_fire_pin_hits = 0;
    if (pin_erw) {
        printf("[JJFB_E10A31_TIMER_R9_PIN] begin pin=0x%X forbid=0x%X evidence=DOCUMENTED\n",
               pin_erw, forbid_erw);
        fflush(stdout);
    }
}

void gwy_ext_obs_timer_fire_r9_unpin(void) {
    if (g_timer_fire_pin_erw) {
        printf("[JJFB_E10A31_TIMER_R9_PIN] end pin=0x%X hits=%u evidence=OBSERVED\n",
               g_timer_fire_pin_erw, g_timer_fire_pin_hits);
        fflush(stdout);
    }
    g_timer_fire_pin_erw = 0;
    g_timer_fire_forbid_erw = 0;
}

uint32_t gwy_ext_obs_timer_fire_r9_pinned(void) { return g_timer_fire_pin_erw; }

uint32_t gwy_ext_obs_module_erw_by_name(const char *needle) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    size_t i;
    if (!reg || !needle || !needle[0]) return 0;
    for (i = 0; i < reg->count; i++) {
        const char *a = reg->modules[i].resolved_name;
        const char *b = reg->modules[i].requested_name;
        if ((a && strstr(a, needle)) || (b && strstr(b, needle)))
            return reg->modules[i].data.start_of_er_rw;
    }
    return 0;
}

uint32_t gwy_ext_obs_module_helper_by_name(const char *needle) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    size_t i;
    if (!reg || !needle || !needle[0]) return 0;
    for (i = 0; i < reg->count; i++) {
        const char *a = reg->modules[i].resolved_name;
        const char *b = reg->modules[i].requested_name;
        if ((a && strstr(a, needle)) || (b && strstr(b, needle)))
            return reg->modules[i].entries.registered_helper;
    }
    return 0;
}

/* Called from CODE hook during FIRE: force R9 off forbidden foreign ERW. */
int gwy_ext_obs_timer_fire_r9_guard(void *uc) {
#ifdef GWY_HAVE_UNICORN
    uint32_t r9 = 0;
    if (!uc || !g_timer_fire_pin_erw || !g_timer_fire_forbid_erw) return 0;
    if (uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &r9) != UC_ERR_OK) return 0;
    if (r9 != g_timer_fire_forbid_erw) return 0;
    if (guest_memory_uc_write_r9((uc_engine *)uc, g_timer_fire_pin_erw) != 0) return 0;
    g_timer_fire_pin_hits++;
    if (g_timer_fire_pin_hits <= 8u) {
        printf("[JJFB_E10A31_TIMER_R9_PIN] refuse=0x%X force=0x%X hit=%u evidence=OBSERVED\n",
               r9, g_timer_fire_pin_erw, g_timer_fire_pin_hits);
        fflush(stdout);
    }
    return 1;
#else
    (void)uc;
    return 0;
#endif
}
