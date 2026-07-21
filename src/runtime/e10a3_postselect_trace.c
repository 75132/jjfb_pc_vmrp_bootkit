#include "gwy_launcher/e10a3_postselect_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    int known;
    int enabled;
    unsigned long long run_id;
    uint32_t post_n;
    uint32_t ev_n;
    uint32_t named_n;
    uint32_t reg_n;
    uint32_t wait_n;
    uint32_t timer_tick;
    int gamelist_init_ok;
    int real_cfg_selected;
    int postselect_armed;
    int wait_loop_logged;
    uint32_t last_timer_pc;
    uint32_t same_timer_streak;
    FILE *post_csv;
    FILE *ev_csv;
    FILE *named_csv;
    FILE *reg_csv;
    FILE *wait_csv;
} g_e10a3;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

int e10a3_enabled(void) {
    if (!g_e10a3.known) {
        g_e10a3.enabled = env1("JJFB_E10A3_MODE") || env1("JJFB_E10A3_POSTSELECT") ||
                          env1("JJFB_E10A3_EVENT10180") || env1("JJFB_E10A3_SERVICES") ||
                          env1("JJFB_E10A3_WAITSTATE");
        {
            const char *rid = getenv("JJFB_E10A_RUN_ID");
            if (!rid || !rid[0]) rid = getenv("JJFB_E10A3_RUN_ID");
            if (rid && rid[0]) g_e10a3.run_id = strtoull(rid, NULL, 10);
        }
        g_e10a3.known = 1;
    }
    return g_e10a3.enabled;
}

void e10a3_reset(void) {
    if (g_e10a3.post_csv) fclose(g_e10a3.post_csv);
    if (g_e10a3.ev_csv) fclose(g_e10a3.ev_csv);
    if (g_e10a3.named_csv) fclose(g_e10a3.named_csv);
    if (g_e10a3.reg_csv) fclose(g_e10a3.reg_csv);
    if (g_e10a3.wait_csv) fclose(g_e10a3.wait_csv);
    memset(&g_e10a3, 0, sizeof(g_e10a3));
}

static FILE *open_csv(const char *env_key, const char *fallback, const char *header) {
    const char *p = getenv(env_key);
    FILE *f;
    if (!p || !p[0]) p = fallback;
    f = fopen(p, "wb");
    if (f) {
        fputs(header, f);
        fflush(f);
    }
    return f;
}

static void ensure_post(void) {
    if (!g_e10a3.post_csv) {
        g_e10a3.post_csv =
            open_csv("JJFB_E10A3_POSTSELECT_CSV", "reports/e10a3_cfg36_postselect_trace.csv",
                     "run_id,seq,pc,lr,module,r0,r1,r2,r3,r4,r5,r6,r7,r9,sp,instruction,"
                     "call_target,call_target_module,platform_api,event_code,string_arg,"
                     "state_before,state_after,note\n");
    }
}

static void ensure_ev(void) {
    if (!g_e10a3.ev_csv) {
        g_e10a3.ev_csv =
            open_csv("JJFB_E10A3_EVENT10180_CSV", "reports/e10a3_event_10180_contract.csv",
                     "run_id,seq,side,pc,lr,module,r0,r1,r2,r3,r4,r5,r6,r7,r9,sp,"
                     "payload_ptr,payload_len,payload_hex,decoded,callback_ptr,userdata,"
                     "state_id,dispatcher,provider_exists,provider_module,out_written,"
                     "callback_scheduled,ret,classification,note\n");
    }
}

static void ensure_named(void) {
    if (!g_e10a3.named_csv) {
        g_e10a3.named_csv =
            open_csv("JJFB_E10A3_NAMED_CSV", "reports/e10a3_named_service_trace.csv",
                     "run_id,seq,operation,name,caller_module,provider_module,target_pc,args,"
                     "callback,rc,note\n");
    }
}

static void ensure_reg(void) {
    if (!g_e10a3.reg_csv) {
        g_e10a3.reg_csv =
            open_csv("JJFB_E10A3_REGISTRY_CSV", "reports/e10a3_gbrwcore_service_registry.csv",
                     "run_id,seq,name,string_va,entry_pc,kind,provider_module,registered,note\n");
    }
}

static void ensure_wait(void) {
    if (!g_e10a3.wait_csv) {
        g_e10a3.wait_csv =
            open_csv("JJFB_E10A3_WAIT_CSV", "reports/e10a3_gamelist_wait_state.csv",
                     "run_id,tick_number,callback_pc,state_id,predicate_pc,predicate_value,"
                     "expected_value,responsible_event_or_service,changed_since_cfg36,note\n");
    }
}

void e10a3_mark_gamelist_init_ok(void) {
    if (!e10a3_enabled()) return;
    g_e10a3.gamelist_init_ok = 1;
    g_e10a3.postselect_armed = 1;
    printf("[JJFB_E10A3] mark=GAMELIST_EXT_FIRST_PC postselect_armed=1 "
           "note=legacy_alias_not_init_complete evidence=OBSERVED\n");
    fflush(stdout);
}

void e10a3_mark_real_cfg_selected(const char *note) {
    if (!e10a3_enabled()) return;
    if (g_e10a3.real_cfg_selected) return;
    g_e10a3.real_cfg_selected = 1;
    g_e10a3.postselect_armed = 1;
    printf("[JJFB_E10A3] mark=real_cfg_selected note=%s evidence=OBSERVED\n",
           note ? note : "");
    fflush(stdout);
}

int e10a3_postselect_armed(void) {
    return e10a3_enabled() && g_e10a3.postselect_armed;
}

void e10a3_note_postselect(uint32_t pc, uint32_t lr, const uint32_t r[8], uint32_t r9,
                           uint32_t sp, const char *module, const char *instruction,
                           uint32_t call_target, const char *call_target_module,
                           const char *platform_api, uint32_t event_code,
                           const char *string_arg, const char *state_before,
                           const char *state_after, const char *note) {
    if (!e10a3_enabled() || !g_e10a3.postselect_armed) return;
    if (g_e10a3.post_n >= 2000u) return;
    ensure_post();
    if (!g_e10a3.post_csv) return;
    g_e10a3.post_n++;
    fprintf(g_e10a3.post_csv,
            "%llu,%u,0x%X,0x%X,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
            "\"%s\",0x%X,\"%s\",\"%s\",0x%X,\"%s\",\"%s\",\"%s\",\"%s\"\n",
            g_e10a3.run_id, g_e10a3.post_n, pc, lr, module ? module : "?",
            r ? r[0] : 0, r ? r[1] : 0, r ? r[2] : 0, r ? r[3] : 0, r ? r[4] : 0,
            r ? r[5] : 0, r ? r[6] : 0, r ? r[7] : 0, r9, sp,
            instruction ? instruction : "", call_target,
            call_target_module ? call_target_module : "",
            platform_api ? platform_api : "", event_code, string_arg ? string_arg : "",
            state_before ? state_before : "", state_after ? state_after : "",
            note ? note : "");
    fflush(g_e10a3.post_csv);
}

void e10a3_note_event_10180(const char *side, uint32_t pc, uint32_t lr, const char *module,
                            uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r4,
                            uint32_t r5, uint32_t r6, uint32_t r7, uint32_t r9, uint32_t sp,
                            uint32_t payload_ptr, uint32_t payload_len, const char *payload_hex,
                            const char *decoded, uint32_t callback_ptr, uint32_t userdata,
                            uint32_t state_id, const char *dispatcher, int provider_exists,
                            const char *provider_module, int out_written, int callback_scheduled,
                            uint32_t ret, const char *classification, const char *note) {
    if (!e10a3_enabled()) return;
    ensure_ev();
    if (!g_e10a3.ev_csv) return;
    g_e10a3.ev_n++;
    fprintf(g_e10a3.ev_csv,
            "%llu,%u,%s,0x%X,0x%X,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
            "0x%X,%u,\"%s\",\"%s\",0x%X,0x%X,0x%X,\"%s\",%d,\"%s\",%d,%d,0x%X,\"%s\",\"%s\"\n",
            g_e10a3.run_id, g_e10a3.ev_n, side ? side : "?", pc, lr, module ? module : "?",
            r0, r1, r2, r3, r4, r5, r6, r7, r9, sp, payload_ptr, payload_len,
            payload_hex ? payload_hex : "", decoded ? decoded : "", callback_ptr, userdata,
            state_id, dispatcher ? dispatcher : "", provider_exists,
            provider_module ? provider_module : "", out_written, callback_scheduled, ret,
            classification ? classification : "unknown", note ? note : "");
    fflush(g_e10a3.ev_csv);
    printf("[JJFB_E10A3_10180] side=%s app=0x%X ret=0x%X class=%s provider=%s "
           "callback_sched=%d evidence=OBSERVED\n",
           side ? side : "?", r1, ret, classification ? classification : "?",
           provider_module ? provider_module : "?", callback_scheduled);
    fflush(stdout);
}

void e10a3_note_named_service(const char *operation, const char *name, const char *caller_module,
                              const char *provider_module, uint32_t target_pc, const char *args,
                              uint32_t callback, int rc, const char *note) {
    if (!e10a3_enabled()) return;
    ensure_named();
    if (!g_e10a3.named_csv) return;
    g_e10a3.named_n++;
    fprintf(g_e10a3.named_csv, "%llu,%u,%s,\"%s\",%s,%s,0x%X,\"%s\",0x%X,%d,\"%s\"\n",
            g_e10a3.run_id, g_e10a3.named_n, operation ? operation : "?", name ? name : "",
            caller_module ? caller_module : "?", provider_module ? provider_module : "?",
            target_pc, args ? args : "", callback, rc, note ? note : "");
    fflush(g_e10a3.named_csv);
    printf("[GWY_NAMED_SERVICE] operation=%s name=%s caller_module=%s provider_module=%s "
           "target_pc=0x%X args=%s callback=0x%X rc=%d evidence=OBSERVED\n",
           operation ? operation : "?", name ? name : "?", caller_module ? caller_module : "?",
           provider_module ? provider_module : "?", target_pc, args ? args : "", callback, rc);
    fflush(stdout);
}

void e10a3_note_service_registry(const char *name, uint32_t string_va, uint32_t entry_pc,
                                 const char *kind, const char *provider_module, int registered,
                                 const char *note) {
    if (!e10a3_enabled()) return;
    ensure_reg();
    if (!g_e10a3.reg_csv) return;
    g_e10a3.reg_n++;
    fprintf(g_e10a3.reg_csv, "%llu,%u,\"%s\",0x%X,0x%X,%s,%s,%d,\"%s\"\n", g_e10a3.run_id,
            g_e10a3.reg_n, name ? name : "", string_va, entry_pc, kind ? kind : "",
            provider_module ? provider_module : "", registered, note ? note : "");
    fflush(g_e10a3.reg_csv);
}

void e10a3_note_wait_state(uint32_t tick, uint32_t callback_pc, uint32_t state_id,
                           uint32_t predicate_pc, uint32_t predicate_value,
                           uint32_t expected_value, const char *responsible,
                           int changed_since_cfg36, const char *note) {
    if (!e10a3_enabled()) return;
    ensure_wait();
    if (!g_e10a3.wait_csv) return;
    g_e10a3.wait_n++;
    fprintf(g_e10a3.wait_csv, "%llu,%u,0x%X,0x%X,0x%X,0x%X,0x%X,\"%s\",%d,\"%s\"\n",
            g_e10a3.run_id, tick, callback_pc, state_id, predicate_pc, predicate_value,
            expected_value, responsible ? responsible : "", changed_since_cfg36,
            note ? note : "");
    fflush(g_e10a3.wait_csv);
}

void e10a3_on_timer_fire(uint32_t helper, uint32_t method, uint32_t p, uint32_t erw,
                         int32_t ret) {
    uint32_t pc;
    if (!e10a3_enabled() || !g_e10a3.postselect_armed) return;
    if (helper != 0x2E3089u && method != 2u) {
        /* Still record gamelist helper fires even if address shifts. */
    }
    g_e10a3.timer_tick++;
    pc = helper;
    if (pc == g_e10a3.last_timer_pc)
        g_e10a3.same_timer_streak++;
    else {
        g_e10a3.same_timer_streak = 1;
        g_e10a3.last_timer_pc = pc;
    }
    e10a3_note_wait_state(g_e10a3.timer_tick, helper, method, 0, (uint32_t)ret, 0,
                          g_e10a3.real_cfg_selected ? "post_cfg_timer"
                                                   : "pre_cfg_idle_timer",
                          g_e10a3.real_cfg_selected,
                          "helper_timer_fire method_is_helper_code_not_state_id");
    e10a3_note_postselect(helper, 0, NULL, erw, 0, "gamelist.ext", "timer_fire", 0, "",
                          "PLATFORM_TIMER", 2, "", "idle_or_wait", "timer_ret",
                          "FIRE_EXT");
    if (!g_e10a3.wait_loop_logged && g_e10a3.same_timer_streak >= 3u) {
        g_e10a3.wait_loop_logged = 1;
        printf("[JJFB_E10A3_WAIT_LOOP] helper=0x%X method=%u P=0x%X erw=0x%X ret=%d "
               "streak=%u real_cfg_selected=%d note=stable_repeated_timer "
               "predicate=timer_ret_unchanged_cfg_not_opened evidence=OBSERVED\n",
               helper, method, p, erw, (int)ret, g_e10a3.same_timer_streak,
               g_e10a3.real_cfg_selected);
        fflush(stdout);
    }
}
