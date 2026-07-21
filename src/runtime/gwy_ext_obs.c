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
static void gwy_ext_obs_lifecycle_deliver(void *uc);

static int env_flag(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1';
}

void gwy_ext_obs_bind_uc(void *uc) {
    g_bound_uc = uc;
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
    e10a31_reset();
    e10a31a_reset();
    e10a31b_reset();
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
    platform_call_census_reset();
    robotol_idle_watch_reset();
    robotol_idle_watch_bind_uc(uc);
    robotol_flag_writer_trace_reset();
    robotol_flag_writer_trace_bind_uc(uc);
}

void gwy_ext_obs_host_callback_enter(void *uc, uint32_t slot_addr, const char *name) {
    ext_callback_frame_on_host_enter(uc, slot_addr, name);
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
    if (!g_arm_absent_emitted && !g_timer_arm_seen) {
        g_arm_absent_emitted = 1;
        printf("[JJFB_TIMER_ARM_ABSENT] window=mrc_init_to_start_dsm_return "
               "reason=no_timer_api_call_seen arm_count=0 evidence=TARGET_OBSERVED\n");
        fflush(stdout);
    }
    /*
     * CROSS_TARGET: 0x10140 is the period/main handler. When guest never calls
     * classic timerStart, host arms a 50ms tick (matches SDL_WaitEventTimeout)
     * and runs the *registered* handler only — no fixed JJFB PC.
     */
    if (!g_lifecycle_host_armed && !g_timer_arm_seen &&
        platform_handler_registry_has(0x10140u)) {
        uint32_t h = platform_handler_registry_get(0x10140u);
        g_lifecycle_host_armed = 1;
        gwy_ext_obs_timer_host_arm(GWY_LIFECYCLE_PERIOD_MS, "lifecycle_10140", 0);
        printf("[JJFB_LIFECYCLE] op=ARM period_ms=%u handler=0x%X family=0x%X "
               "reason=10140_registered_no_classic_timer evidence=CROSS_TARGET+docs/06\n",
               GWY_LIFECYCLE_PERIOD_MS, h, platform_handler_registry_family(0x10140u));
        fflush(stdout);
    }
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

    ok = guest_memory_uc_run_entry_ex((struct uc_struct *)uc, handler, stop,
                                      GWY_LIFECYCLE_INSN_LIMIT, &abi, &out);
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
    fflush(stdout);
    (void)guest_memory_uc_write_r9((struct uc_struct *)uc, r9_save);
}

int gwy_ext_obs_lifecycle_on_timer_due(void *uc) {
    if (g_armed_timer_chunk) return 0;
    if (!g_lifecycle_host_armed && !platform_handler_registry_has(0x10140u)) return 0;
    if (g_lifecycle_delivering) {
        g_lifecycle_pending = 1;
        return 1;
    }
    g_lifecycle_delivering = 1;
    gwy_ext_obs_lifecycle_deliver(uc);
    if (g_lifecycle_host_armed) {
        platform_timer_start(GWY_LIFECYCLE_PERIOD_MS);
        if (g_timer_start) (void)g_timer_start((uint16_t)GWY_LIFECYCLE_PERIOD_MS);
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
            (void)platform_handler_registry_register(r0, result.reg_family, result.reg_handler);
            robotol_idle_watch_on_handler_register(r0, result.reg_family, result.reg_handler);
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
            (void)platform_handler_registry_register(r0, result.reg_family, result.reg_handler);
            robotol_idle_watch_on_handler_register(r0, result.reg_family, result.reg_handler);
        }
        printf("[PLATFORM_ALLOC] code=0x%X size=0x%X buf=0x%X ret=0x%X tag_u16=%u name=%s "
               "evidence=%s\n",
               r0, result.alloc_size, r2, ret, (unsigned)result.alloc_u16_at0,
               result.name ? result.name : "?", result.evidence ? result.evidence : "?");
        if (env_flag("JJFB_PLAT_RET0_TRACE") || env_flag("JJFB_MRC_INIT_TRACE")) {
            printf("[JJFB_PLAT_CALL] code=0x%X app=0x%X size=0x%X ret=0x%X kind=ALLOC name=%s "
                   "handler=0x%X evidence=%s\n",
                   r0, r1, result.alloc_size, ret, result.name ? result.name : "?",
                   result.reg_handler, result.evidence ? result.evidence : "?");
        }
        fflush(stdout);
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
    } else {
        ret = 0;
        if (env_flag("JJFB_PLAT_RET0_TRACE") || env_flag("JJFB_MRC_INIT_TRACE")) {
            printf("[JJFB_PLAT_CALL] code=0x%X app=0x%X arg2=0x%X ret=%d kind=%d name=%s "
                   "evidence=%s note=fallback\n",
                   r0, r1, r2, (int)ret, (int)result.kind, result.name ? result.name : "?",
                   result.evidence ? result.evidence : "?");
            fflush(stdout);
        }
    }

    ext_chunk_provider_on_slot28_call(pc, r0, r1, r2, r3, r4, ret);
    platform_call_census_note(r0, r1, caller_pc, ret);
    e10a31a_note_platform_api(uc, result.name, caller_pc, 0, r0, ret);
    platform_1e209_trace_call(caller_pc, r0, r1, r2, r3, ret, g_lifecycle_ticks);
    if (result.kind == GWY_PLAT_KIND_GRAPHICS_FP)
        platform_call_census_note_refresh();
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

void gwy_ext_obs_mem_fault(void *uc,
                           uint32_t access_type,
                           uint64_t address,
                           uint32_t size,
                           int64_t value) {
    uint32_t pc = 0;
#ifdef GWY_HAVE_UNICORN
    if (uc) uc_reg_read((uc_engine *)uc, UC_ARM_REG_PC, &pc);
#endif
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
    if (uc_reg_write((uc_engine *)uc, UC_ARM_REG_R9, &g_timer_fire_pin_erw) != UC_ERR_OK) return 0;
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
