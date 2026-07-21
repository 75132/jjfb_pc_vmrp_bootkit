#include "gwy_launcher/e10a31_gamelist_context.h"

#include "gwy_launcher/e10a3_postselect_trace.h"
#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    int known;
    int enabled;
    int timer_trace;
    int param_trace;
    int cfg_gate_trace;
    int start_dsm_abi_trace;
    unsigned long long run_id;
    uint64_t timer_generation;
    GwyTimerBinding binding;
    int binding_valid;
    int timer_rebound;
    int timer_coherent;
    int ext_first_pc;
    int init_complete;
    uint32_t timer_fire_n;
    uint32_t param_read_n;
    uint32_t cfg_gate_n;
    uint32_t start_dsm_n;
    FILE *timer_csv;
    FILE *param_csv;
    FILE *start_dsm_csv;
    FILE *cfg_gate_csv;
    uint32_t param_va;
    uint32_t param_len;
    char param_buf[160];
#ifdef GWY_HAVE_UNICORN
    uc_hook param_read_hook;
    int param_hook_armed;
    void *uc;
#endif
} g_e31;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

static void parse_mode(void) {
    if (g_e31.known) return;
    g_e31.enabled = env1("JJFB_E10A31_MODE") || env1("JJFB_E10A31_TIMER_CONTEXT") ||
                    env1("JJFB_E10A31_PARAM_TRACE") || env1("JJFB_E10A31_CFG_GATE") ||
                    env1("JJFB_E10A31_START_DSM_ABI");
    g_e31.timer_trace =
        env1("JJFB_E10A31_TIMER_CONTEXT") || env1("JJFB_E10A31_MODE");
    g_e31.param_trace = env1("JJFB_E10A31_PARAM_TRACE") || env1("JJFB_E10A31_MODE");
    g_e31.cfg_gate_trace = env1("JJFB_E10A31_CFG_GATE") || env1("JJFB_E10A31_MODE");
    g_e31.start_dsm_abi_trace =
        env1("JJFB_E10A31_START_DSM_ABI") || env1("JJFB_E10A31_MODE");
    {
        const char *rid = getenv("JJFB_E10A31_RUN_ID");
        if (!rid || !rid[0]) rid = getenv("JJFB_E10A_RUN_ID");
        if (rid && rid[0]) g_e31.run_id = strtoull(rid, NULL, 10);
    }
    g_e31.known = 1;
}

int e10a31_enabled(void) {
    parse_mode();
    return g_e31.enabled;
}

int e10a31_timer_trace(void) {
    parse_mode();
    return g_e31.timer_trace;
}

int e10a31_param_trace(void) {
    parse_mode();
    return g_e31.param_trace;
}

int e10a31_cfg_gate_trace(void) {
    parse_mode();
    return g_e31.cfg_gate_trace;
}

int e10a31_start_dsm_abi_trace(void) {
    parse_mode();
    return g_e31.start_dsm_abi_trace;
}

void e10a31_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_e31.param_hook_armed && g_e31.uc) {
        uc_hook_del((uc_engine *)g_e31.uc, g_e31.param_read_hook);
        g_e31.param_hook_armed = 0;
    }
#endif
    if (g_e31.timer_csv) fclose(g_e31.timer_csv);
    if (g_e31.param_csv) fclose(g_e31.param_csv);
    if (g_e31.start_dsm_csv) fclose(g_e31.start_dsm_csv);
    if (g_e31.cfg_gate_csv) fclose(g_e31.cfg_gate_csv);
    memset(&g_e31, 0, sizeof(g_e31));
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

static void ensure_timer_csv(void) {
    if (!g_e31.timer_csv) {
        g_e31.timer_csv = open_csv("JJFB_E10A31_TIMER_CSV",
                                   "reports/e10a31_timer_binding_trace.csv",
                                   "run_id,event,generation,timer_id,period_ms,source_pc,source_lr,"
                                   "source_r9,source_module,chunk_guest,chunk_module,helper,"
                                   "helper_module,p_guest,p_module,erw,erw_module,chunk_p_erw,"
                                   "chunk_var_erw,registry_erw,p_generation,module_generation,"
                                   "classification,note\n");
    }
}

static void ensure_param_csv(void) {
    if (!g_e31.param_csv) {
        g_e31.param_csv = open_csv("JJFB_E10A31_PARAM_CSV",
                                   "reports/e10a31_launch_param_read_trace.csv",
                                   "run_id,seq,pc,lr,module,r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,"
                                   "r11,r12,sp,addr,size,bytes_hex,owner_p,owner_erw,milestone,"
                                   "note\n");
    }
}

static void ensure_start_dsm_csv(void) {
    if (!g_e31.start_dsm_csv) {
        g_e31.start_dsm_csv = open_csv("JJFB_E10A31_START_DSM_CSV",
                                       "reports/e10a31_start_dsm_param_abi.csv",
                                       "run_id,seq,start_t_guest,filename_ptr,filename,ext_ptr,ext,"
                                       "entry_ptr,entry,raw_hex,verdict,note\n");
    }
}

static void ensure_cfg_gate_csv(void) {
    if (!g_e31.cfg_gate_csv) {
        g_e31.cfg_gate_csv = open_csv("JJFB_E10A31_CFG_GATE_CSV",
                                     "reports/e10a31_cfg_gate_predicates.csv",
                                     "run_id,seq,site,pc,off,module,r9,erw,reached,note\n");
    }
}

static const GwyLoadedModule *mod_by_helper(uint32_t helper) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *gm;
    if (!reg || !helper) return NULL;
    gm = module_registry_find_by_helper(reg, helper);
    if (!gm) gm = module_registry_find_by_code_addr(reg, helper & ~1u);
    return gm;
}

static const GwyLoadedModule *mod_by_p(uint32_t p_guest) {
    ExtChunkOwnerInfo info;
    if (!ext_chunk_provider_owner_for_p(p_guest, &info)) return NULL;
    if (!info.module_id) return mod_by_helper(info.helper);
    {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        return reg ? module_registry_find_by_id(reg, info.module_id) : NULL;
    }
}

static const GwyLoadedModule *mod_by_erw(uint32_t erw) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    size_t i;
    if (!reg || !erw) return NULL;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].data.start_of_er_rw == erw) return &reg->modules[i];
    }
    return NULL;
}

static const char *mod_name(const GwyLoadedModule *m) {
    if (!m) return "?";
    return m->resolved_name[0] ? m->resolved_name : m->requested_name;
}

static int owner_known(uint64_t mid) { return mid != 0; }

static int owners_equal(uint64_t a, uint64_t b) {
    return owner_known(a) && owner_known(b) && a == b;
}

/* True when helper/chunk/P share one owner but ERW differs (safe single-field heal). */
static int erw_only_mismatch(const GwyTimerBinding *b) {
    if (!b || !owner_known(b->helper_module_id)) return 0;
    if (!owners_equal(b->helper_module_id, b->chunk_module_id)) return 0;
    if (!owners_equal(b->helper_module_id, b->p_module_id)) return 0;
    if (!owner_known(b->erw_module_id)) return 0;
    return b->helper_module_id != b->erw_module_id;
}

static int multi_owner_mismatch(const GwyTimerBinding *b) {
    int bad = 0;
    if (!b || !owner_known(b->helper_module_id)) return 0;
    if (owner_known(b->chunk_module_id) && b->chunk_module_id != b->helper_module_id) bad = 1;
    if (owner_known(b->p_module_id) && b->p_module_id != b->helper_module_id) bad = 1;
    return bad;
}

static const char *classify_timer_context(const GwyTimerBinding *b) {
    if (!b->helper || !b->chunk_guest) return "TIMER_BINDING_INCOMPLETE";
    if (multi_owner_mismatch(b)) return "TIMER_CONTEXT_MIXED_MULTI_OWNER";
    if (b->chunk_module_id && b->helper_module_id &&
        b->chunk_module_id != b->helper_module_id)
        return "TIMER_CHUNK_OWNER_MISMATCH";
    if (b->p_module_id && b->helper_module_id && b->p_module_id != b->helper_module_id)
        return "TIMER_P_OWNER_MISMATCH";
    if (erw_only_mismatch(b)) return "TIMER_ERW_OWNER_MISMATCH";
    if (b->helper_module_id && b->erw_module_id && b->helper_module_id != b->erw_module_id)
        return "TIMER_CONTEXT_MIXED";
    if (b->helper_module_id && b->module_id && b->helper_module_id != b->module_id)
        return "TIMER_HELPER_OWNER_MISMATCH";
    return "TIMER_CONTEXT_COHERENT";
}

static void log_timer_row(const char *event, const GwyTimerBinding *b, const char *classification,
                          const char *note) {
    if (!e10a31_timer_trace()) return;
    ensure_timer_csv();
    if (!g_e31.timer_csv || !b) return;
    fprintf(g_e31.timer_csv,
            "%llu,%s,%llu,%u,%u,0x%X,0x%X,0x%X,%s,0x%X,%llu,0x%X,%llu,0x%X,%llu,0x%X,%llu,"
            "0x%X,0x%X,0x%X,%u,%u,\"%s\",\"%s\"\n",
            g_e31.run_id, event ? event : "?", (unsigned long long)b->timer_generation,
            b->timer_id, b->period_ms, b->source_pc, b->source_lr, b->source_r9, b->module_name,
            b->chunk_guest, (unsigned long long)b->chunk_module_id, b->helper,
            (unsigned long long)b->helper_module_id, b->p_guest, (unsigned long long)b->p_module_id,
            b->er_rw, (unsigned long long)b->erw_module_id, b->chunk_p_erw, b->chunk_var_erw,
            b->registry_erw, b->p_generation, b->module_generation,
            classification ? classification : "?", note ? note : "");
    fflush(g_e31.timer_csv);
}

static void fill_binding_from_chunk(GwyTimerBinding *b, uint32_t chunk_guest) {
    ExtChunkOwnerInfo info;
    const GwyLoadedModule *hm;
    const GwyLoadedModule *pm;
    const GwyLoadedModule *em;
    memset(b, 0, sizeof(*b));
    if (!ext_chunk_provider_owner_for_chunk(chunk_guest, &info)) return;
    b->chunk_guest = chunk_guest;
    b->helper = info.helper;
    b->p_guest = info.p_guest;
    b->er_rw = info.erw ? info.erw : info.p_erw;
    b->chunk_p_erw = info.p_erw;
    b->chunk_var_erw = info.chunk_erw;
    b->module_id = info.module_id;
    b->chunk_module_id = info.module_id;
    b->p_generation = info.p_generation;
    b->module_generation = info.module_generation;
    snprintf(b->module_name, sizeof(b->module_name), "%.63s", info.module_name);
    snprintf(b->package_name, sizeof(b->package_name), "%.127s", info.package_name);

    hm = mod_by_helper(b->helper);
    pm = mod_by_p(b->p_guest);
    em = mod_by_erw(b->er_rw);
    if (hm) {
        b->helper_module_id = hm->module_id;
        if (!b->module_id) b->module_id = hm->module_id;
        if (!b->module_name[0])
            snprintf(b->module_name, sizeof(b->module_name), "%.63s", mod_name(hm));
        if (!b->package_name[0])
            snprintf(b->package_name, sizeof(b->package_name), "%.127s", hm->package_path);
    }
    if (pm) b->p_module_id = pm->module_id;
    if (em) b->erw_module_id = em->module_id;

    if (hm && hm->data.start_of_er_rw) b->registry_erw = hm->data.start_of_er_rw;
}

void e10a31_mark_milestone(const char *name, const char *note) {
    if (!name || !name[0]) return;
    printf("[JJFB_E10A31] milestone=%s note=%s evidence=OBSERVED\n", name,
           note ? note : "");
    fflush(stdout);
}

void e10a31_mark_ext_first_pc(void) {
    if (g_e31.ext_first_pc) return;
    g_e31.ext_first_pc = 1;
    e10a31_mark_milestone("GAMELIST_EXT_FIRST_PC", "first_guest_pc_not_init_complete");
    e10a3_mark_gamelist_init_ok();
}

void e10a31_timer_arm(void *uc, uint32_t source_pc, uint32_t source_lr, uint32_t source_r9,
                      uint32_t timer_chunk, uint32_t timer_id, uint32_t period_ms) {
    GwyTimerBinding b;
    const char *cls;
    (void)uc;
    if (!timer_chunk) return;
    g_e31.timer_generation++;
    fill_binding_from_chunk(&b, timer_chunk);
    b.timer_generation = g_e31.timer_generation;
    b.timer_id = timer_id;
    b.period_ms = period_ms;
    b.source_pc = source_pc;
    b.source_lr = source_lr;
    b.source_r9 = source_r9;
    b.package_id = b.module_id;
    g_e31.binding = b;
    g_e31.binding_valid = 1;
    g_e31.timer_rebound = 0;
    g_e31.timer_coherent = 0;
    cls = classify_timer_context(&b);
    log_timer_row("TIMER_ARM", &b, cls, "arm_from_sendAppEvent");
    if (strcmp(cls, "TIMER_CONTEXT_COHERENT") == 0) {
        g_e31.timer_coherent = 1;
        e10a31_mark_milestone("GAMELIST_TIMER_REGISTERED", b.module_name);
    } else {
        printf("[JJFB_E10A31_TIMER_ARM] chunk=0x%X helper=0x%X P=0x%X erw=0x%X "
               "helper_mod=%llu erw_mod=%llu class=%s evidence=OBSERVED\n",
               timer_chunk, b.helper, b.p_guest, b.er_rw,
               (unsigned long long)b.helper_module_id, (unsigned long long)b.erw_module_id,
               cls);
        fflush(stdout);
    }
}

void e10a31_timer_disarm(void) {
    if (g_e31.binding_valid && e10a31_timer_trace())
        log_timer_row("TIMER_DISARM", &g_e31.binding, "TIMER_DISARMED", "timer_stop");
    memset(&g_e31.binding, 0, sizeof(g_e31.binding));
    g_e31.binding_valid = 0;
}

uint32_t e10a31_timer_armed_chunk(void) {
    return g_e31.binding_valid ? g_e31.binding.chunk_guest : 0;
}

int e10a31_timer_arm_observed(void) {
    return g_e31.binding_valid && g_e31.binding.helper && g_e31.binding.p_guest;
}

int e10a31_timer_fire_observed(void) { return g_e31.timer_fire_n > 0; }

int e10a31_timer_fire_resolve(void *uc, uint32_t *out_helper, uint32_t *out_p_guest,
                              uint32_t *out_erw) {
    GwyTimerBinding fire;
    const GwyLoadedModule *hm;
    const char *cls;
    uint32_t helper = 0, p = 0, erw = 0;
    int rebound = 0;
    (void)uc;

    if (!g_e31.binding_valid || !g_e31.binding.chunk_guest) {
        if (out_helper) *out_helper = 0;
        if (out_p_guest) *out_p_guest = 0;
        if (out_erw) *out_erw = 0;
        return 0;
    }

    fill_binding_from_chunk(&fire, g_e31.binding.chunk_guest);
    fire.timer_generation = g_e31.binding.timer_generation;
    fire.timer_id = g_e31.binding.timer_id;
    fire.period_ms = g_e31.binding.period_ms;
    fire.source_pc = g_e31.binding.source_pc;
    fire.source_lr = g_e31.binding.source_lr;
    fire.source_r9 = g_e31.binding.source_r9;
    snprintf(fire.module_name, sizeof(fire.module_name), "%s", g_e31.binding.module_name);
    snprintf(fire.package_name, sizeof(fire.package_name), "%s", g_e31.binding.package_name);

    helper = fire.helper ? fire.helper : g_e31.binding.helper;
    p = fire.p_guest ? fire.p_guest : g_e31.binding.p_guest;
    erw = fire.er_rw ? fire.er_rw : g_e31.binding.er_rw;

    hm = mod_by_helper(helper);
    /* Refresh owner fields from live chunk before classify. */
    fire.helper = helper;
    fire.p_guest = p;
    fire.er_rw = erw;
    {
        const GwyLoadedModule *pm = mod_by_p(p);
        const GwyLoadedModule *em = mod_by_erw(erw);
        if (hm) fire.helper_module_id = hm->module_id;
        if (pm) fire.p_module_id = pm->module_id;
        if (em) fire.erw_module_id = em->module_id;
    }
    cls = classify_timer_context(&fire);

    /*
     * Auto-heal ERW only when helper/chunk/P owners agree and only ERW is wrong.
     * Do not hide stale P/chunk by rewriting R9 alone.
     */
    if (hm && hm->data.start_of_er_rw && erw_only_mismatch(&fire)) {
        uint32_t owned = hm->data.start_of_er_rw;
        if (owned && owned != erw) {
            if (ext_chunk_provider_rebind_chunk_erw(fire.chunk_guest, owned, p)) {
                erw = owned;
                fire.er_rw = owned;
                fire.erw_module_id = hm->module_id;
                rebound = 1;
                g_e31.timer_rebound = 1;
                cls = "GAMELIST_TIMER_CONTEXT_REBOUND";
                e10a31_mark_milestone("GAMELIST_TIMER_CONTEXT_REBOUND", fire.module_name);
            }
        }
    } else if (multi_owner_mismatch(&fire)) {
        cls = "TIMER_CONTEXT_MIXED_MULTI_OWNER";
        printf("[JJFB_E10A31_TIMER_NO_HEAL] helper_mod=%llu chunk_mod=%llu p_mod=%llu "
               "erw_mod=%llu class=%s note=refuse_erw_only_heal evidence=OBSERVED\n",
               (unsigned long long)fire.helper_module_id,
               (unsigned long long)fire.chunk_module_id, (unsigned long long)fire.p_module_id,
               (unsigned long long)fire.erw_module_id, cls);
        fflush(stdout);
    }

    if (rebound)
        e10a31_mark_milestone("GAMELIST_TIMER_FIRES_WITH_OWN_ERW", "after_safe_erw_rebind");
    else if (strcmp(cls, "TIMER_CONTEXT_COHERENT") == 0)
        e10a31_mark_milestone("GAMELIST_TIMER_FIRES_WITH_OWN_ERW", fire.module_name);

    log_timer_row("TIMER_FIRE", &fire, cls,
                  rebound ? "safe_erw_rebind" : (multi_owner_mismatch(&fire) ? "no_heal_multi_owner"
                                                                             : "fire_resolve"));

    if (out_helper) *out_helper = helper;
    if (out_p_guest) *out_p_guest = p;
    if (out_erw) *out_erw = erw;
    return helper && p;
}

void e10a31_on_timer_fire(void *uc, uint32_t helper, uint32_t method, uint32_t p_guest,
                          uint32_t erw, int32_t ret) {
    (void)uc;
    g_e31.timer_fire_n++;
    if (e10a31_timer_trace() && g_e31.binding_valid) {
        const char *cls = g_e31.timer_rebound ? "GAMELIST_TIMER_CONTEXT_REBOUND"
                                            : classify_timer_context(&g_e31.binding);
        log_timer_row("TIMER_FIRE_RET", &g_e31.binding, cls, "after_helper_call");
    }
    e10a3_on_timer_fire(helper, method, p_guest, erw, ret);
}

#ifdef GWY_HAVE_UNICORN
static void param_mem_read_cb(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                              int64_t value, void *user_data) {
    uint32_t pc = 0, lr = 0, sp = 0, r9 = 0;
    uint32_t regs[13];
    uint8_t bytes[16];
    char hex[64];
    const char *mod = "?";
    int i;
    const char *milestone = "SHELL_PARAM_ENTRY_FIELD_READ";
    (void)type;
    (void)value;
    (void)user_data;
    if (!e10a31_param_trace()) return;
    if (!g_e31.param_va || !g_e31.param_len) return;
    if (address < g_e31.param_va || address >= (uint64_t)g_e31.param_va + g_e31.param_len)
        return;
    if (g_e31.param_read_n >= 500u) return;

    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    for (i = 0; i < 13; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &regs[i]);

    {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *gm =
            reg ? module_registry_find_by_code_addr(reg, pc & ~1u) : NULL;
        if (gm) mod = mod_name(gm);
    }

    hex[0] = 0;
    if (size > 0 && size <= 16 &&
        guest_memory_uc_peek((struct uc_struct *)uc, (uint32_t)address, bytes, (size_t)size)) {
        for (i = 0; i < size; i++) {
            char t[4];
            snprintf(t, sizeof(t), "%02X", bytes[i]);
            strncat(hex, t, sizeof(hex) - strlen(hex) - 1);
        }
    }
    if (address == g_e31.param_va) milestone = "SHELL_PARAM_START_STRUCT_READ";
    if (strstr(g_e31.param_buf, "gwyblink") && strstr(g_e31.param_buf, "napptype="))
        milestone = "SHELL_PARAM_GWYBLINK_PARSED";

    g_e31.param_read_n++;
    ensure_param_csv();
    if (g_e31.param_csv) {
        fprintf(g_e31.param_csv,
                "%llu,%u,0x%X,0x%X,%s,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                "0x%X,0x%X,0x%llX,%u,\"%s\",0,0x%X,\"%s\",\"mem_read_hook\"\n",
                g_e31.run_id, g_e31.param_read_n, pc, lr, mod, regs[0], regs[1], regs[2],
                regs[3], regs[4], regs[5], regs[6], regs[7], regs[8], regs[9], regs[10], regs[11],
                regs[12], sp, (unsigned long long)address, (unsigned)(size > 0 ? size : 0), hex,
                r9, milestone);
        fflush(g_e31.param_csv);
    }
    if (g_e31.param_read_n <= 8) {
        printf("[JJFB_E10A31_PARAM_READ] pc=0x%X module=%s addr=0x%llX size=%d bytes=%s "
               "milestone=%s evidence=OBSERVED\n",
               pc, mod, (unsigned long long)address, size, hex, milestone);
        fflush(stdout);
    }
}

static void arm_param_read_hook(void *uc) {
    uc_err ue;
    if (!uc || !g_e31.param_va || !g_e31.param_len || g_e31.param_hook_armed) return;
    g_e31.uc = uc;
    ue = uc_hook_add((uc_engine *)uc, &g_e31.param_read_hook, UC_HOOK_MEM_READ,
                     (void *)param_mem_read_cb, NULL, g_e31.param_va,
                     (uint64_t)g_e31.param_va + g_e31.param_len - 1u);
    if (ue == UC_ERR_OK) g_e31.param_hook_armed = 1;
}
#else
static void arm_param_read_hook(void *uc) { (void)uc; }
#endif

void e10a31_launch_param_mapped(void *uc, uint32_t param_va, uint32_t param_len,
                                const char *entry) {
    g_e31.param_va = param_va;
    g_e31.param_len = param_len;
    if (entry) snprintf(g_e31.param_buf, sizeof(g_e31.param_buf), "%s", entry);
    if (e10a31_param_trace()) arm_param_read_hook(uc);
    if (param_va && param_len)
        e10a31_mark_milestone("SHELL_PARAM_POINTER_COPIED", "mr_start_dsm_entry_mapped");
}

void e10a31_note_start_dsm(void *uc, uint32_t start_t_guest, const char *filename, const char *ext,
                           const char *entry) {
    char raw_hex[128];
    const char *verdict = "START_DSM_PARAM_ABI_CONFIRMED";
    (void)uc;
    if (!e10a31_start_dsm_abi_trace()) return;
    g_e31.start_dsm_n++;
    ensure_start_dsm_csv();
    raw_hex[0] = 0;
#ifdef GWY_HAVE_UNICORN
    if (start_t_guest && uc) {
        uint8_t raw[12];
        if (guest_memory_uc_peek((struct uc_struct *)uc, start_t_guest, raw, sizeof(raw))) {
            int i;
            for (i = 0; i < 12; i++) {
                char t[4];
                snprintf(t, sizeof(t), "%02X", raw[i]);
                strncat(raw_hex, t, sizeof(raw_hex) - strlen(raw_hex) - 1);
            }
        }
    }
#endif
    if (!entry || !entry[0]) verdict = "START_DSM_ENTRY_FIELD_NOT_LAUNCH_PARAM";
    if (g_e31.start_dsm_csv) {
        fprintf(g_e31.start_dsm_csv,
                "%llu,%u,0x%X,0, \"%s\",0,\"%s\",0,\"%s\",\"%s\",\"%s\",\"start_t_fields\"\n",
                g_e31.run_id, g_e31.start_dsm_n, start_t_guest, filename ? filename : "",
                ext ? ext : "", entry ? entry : "", raw_hex, verdict);
        fflush(g_e31.start_dsm_csv);
    }
    printf("[JJFB_E10A31_START_DSM] start_t=0x%X filename=\"%s\" ext=\"%s\" entry=\"%s\" "
           "verdict=%s evidence=OBSERVED\n",
           start_t_guest, filename ? filename : "", ext ? ext : "", entry ? entry : "", verdict);
    fflush(stdout);
    e10a31_mark_milestone("SHELL_PARAM_START_STRUCT_READ", "start_dsm_abi_trace");
}

void e10a31_note_cfg_site(void *uc, uint32_t pc, uint32_t base, uint32_t off, const char *site,
                          const char *module, uint32_t r9, uint32_t erw) {
    (void)uc;
    if (!e10a31_cfg_gate_trace()) return;
    g_e31.cfg_gate_n++;
    ensure_cfg_gate_csv();
    if (g_e31.cfg_gate_csv) {
        fprintf(g_e31.cfg_gate_csv, "%llu,%u,\"%s\",0x%X,0x%X,%s,0x%X,0x%X,1,\"guest_pc_hit\"\n",
                g_e31.run_id, g_e31.cfg_gate_n, site ? site : "?", pc, off, module ? module : "?",
                r9, erw);
        fflush(g_e31.cfg_gate_csv);
    }
    e10a31_mark_milestone("GAMELIST_CFG_GATE_REACHED", site ? site : "?");
    if (off == 0x1AF8u)
        e10a31_mark_milestone("GAMELIST_EXTERNAL_CFG_OPEN", site ? site : "cfg_open");
}
