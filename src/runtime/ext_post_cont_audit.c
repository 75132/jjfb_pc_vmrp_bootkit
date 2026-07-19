#include "gwy_launcher/ext_post_cont_audit.h"
#include "gwy_launcher/ext_er_rw_bind_restore.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PC_GATE_LINE                                                                              \
    "post_continuation_gate=%s graphics_gate=%s event_scheduler_gate=%s "                         \
    "nested_r9_scope_gate=open module_r9_switch_gate=open guest_callback_frame_gate=blocked "      \
    "bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked"
#define PC_GATE_ARGS                                                                              \
    (g_pc.post_cont_gate_open ? "open" : "blocked"),                                              \
        (g_pc.graphics_gate_open ? "open" : "blocked"),                                           \
        (g_pc.event_sched_gate_open ? "open" : "blocked")

typedef enum ApiCat {
    API_CAT_NONE = 0,
    API_CAT_FILE,
    API_CAT_TIMER,
    API_CAT_EVENT,
    API_CAT_CALLBACK,
    API_CAT_DRAW,
    API_CAT_BITMAP,
    API_CAT_TEXT,
    API_CAT_REFRESH,
    API_CAT_SOUND,
    API_CAT_NETWORK,
    API_CAT_STORAGE,
    API_CAT_SKIP
} ApiCat;

typedef struct FirstApi {
    int seen;
    char name[48];
    char category[24];
    char caller_module[72];
    uint32_t caller_offset;
    uint32_t r0, r1, r2, r3;
    uint32_t result;
    uint32_t seq;
} FirstApi;

static struct {
    int enabled;
    int enabled_known;
    void *uc;
    int armed;
    int finalized;
    PostContClass last_class;
    int post_cont_gate_open;
    int graphics_gate_open;
    int event_sched_gate_open;
    char next_fix[64];
    char stop_reason[48];
    char first_boundary[48];

    uint64_t module_id;
    char module[72];
    uint32_t cont_pc;
    uint32_t call_pc;
    uint32_t cont_r0, cont_r1, cont_r2, cont_r3, cont_r9;
    uint32_t cont_sp, cont_lr;
    uint32_t cont_scope_depth;

    uint64_t instr_count;
    uint32_t event_seq;
    int boundary_hit;

    uint32_t helper;
    uint32_t p_guest;
    uint32_t p_len;
    uint32_t p_er_rw;
    uint32_t p_er_len;
    uint32_t registry_er_rw;
    uint32_t registry_er_size;
    int load_enter_seen;
    int load_exit_seen;
    int runtime_ready;
    int helper_abi_ok;
    int helper_abi_checked;

    int normal_return_seen;
    uint32_t return_pc;
    uint32_t return_value;
    char return_module[72];
    int outer_frame_popped;
    uint32_t return_scope_depth;

    FirstApi first_file, first_timer, first_event, first_callback;
    FirstApi first_draw, first_bitmap, first_text, first_refresh;
    FirstApi first_sound, first_network, first_storage;
    FirstApi first_platform;
    int draw_seen;
    int refresh_seen;
    int timer_seen;
    int event_seen;
    int unimplemented_api;

    int fault_seen;
    uint32_t fault_pc;
    uint32_t fault_addr;
    uint32_t fault_r9;
    uint32_t fault_scope_depth;
    char fault_module[72];
    uint32_t fault_offset;

    uint32_t last_pc;
    char last_module[72];
    uint32_t last_offset;
    uint32_t last_r0, last_r1, last_r2, last_r3, last_r9;
    uint32_t last_sp, last_lr;
    uint32_t last_scope_depth;
} g_pc;

const char *ext_post_cont_class_name(PostContClass c) {
    switch (c) {
    case PC_CLASS_RUNTIME_PROGRESS_TO_PLATFORM_API: return "RUNTIME_PROGRESS_TO_PLATFORM_API";
    case PC_CLASS_MODULE_ER_RW_RUNTIME_GAP: return "MODULE_ER_RW_RUNTIME_GAP";
    case PC_CLASS_MISSING_EVENT_SCHEDULING: return "MISSING_EVENT_SCHEDULING";
    case PC_CLASS_MISSING_GRAPHICS_REFRESH: return "MISSING_GRAPHICS_REFRESH";
    case PC_CLASS_GRAPHICS_PIPELINE_ACTIVE: return "GRAPHICS_PIPELINE_ACTIVE";
    case PC_CLASS_NORMAL_IDLE: return "NORMAL_IDLE";
    case PC_CLASS_NEW_ABI_FAULT: return "NEW_ABI_FAULT";
    case PC_CLASS_HARNESS_WINDOW_TOO_SHORT: return "HARNESS_WINDOW_TOO_SHORT";
    default: return "UNKNOWN";
    }
}

const char *ext_post_cont_event_name(PostContEventType t) {
    switch (t) {
    case PC_EVT_ROBOTOL_CONTINUATION: return "ROBOTOL_CONTINUATION";
    case PC_EVT_NORMAL_RETURN: return "NORMAL_RETURN";
    case PC_EVT_YIELD: return "YIELD";
    case PC_EVT_EVENT_WAIT: return "EVENT_WAIT";
    case PC_EVT_PLATFORM_API: return "PLATFORM_API";
    case PC_EVT_DRAW_CALL: return "DRAW_CALL";
    case PC_EVT_REFRESH_CALL: return "REFRESH_CALL";
    case PC_EVT_FILE_REQUEST: return "FILE_REQUEST";
    case PC_EVT_TIMER_REQUEST: return "TIMER_REQUEST";
    case PC_EVT_NETWORK_REQUEST: return "NETWORK_REQUEST";
    case PC_EVT_NEW_FAULT: return "NEW_FAULT";
    case PC_EVT_PROGRESS: return "PROGRESS";
    case PC_EVT_HELPER_CALL: return "HELPER_CALL";
    case PC_EVT_RUNTIME_READY: return "RUNTIME_READY";
    default: return "NONE";
    }
}

PostContClass ext_post_cont_audit_last_class(void) { return g_pc.last_class; }
int ext_post_cont_audit_armed(void) { return g_pc.armed; }
uint64_t ext_post_cont_audit_instruction_count(void) { return g_pc.instr_count; }
int ext_post_cont_gate_open(void) { return g_pc.post_cont_gate_open; }
int ext_post_cont_graphics_gate_open(void) { return g_pc.graphics_gate_open; }
int ext_post_cont_event_scheduler_gate_open(void) { return g_pc.event_sched_gate_open; }

int ext_post_cont_audit_enabled(void) {
    const char *e;
    if (g_pc.enabled_known) return g_pc.enabled;
    e = getenv("GWY_POST_CONT_AUDIT");
    if (e && e[0] == '1' && e[1] == '\0') {
        g_pc.enabled = 1;
    } else {
        /* Auto-enable with the live 6C/6D observation chain. */
        const char *cf = getenv("GWY_CALLBACK_FRAME");
        const char *sw = getenv("GWY_MODULE_R9_SWITCH");
        g_pc.enabled = ((cf && cf[0] == '1') || (sw && sw[0] == '1')) ? 1 : 0;
    }
    g_pc.enabled_known = 1;
    return g_pc.enabled;
}

void ext_post_cont_audit_reset(void) {
    memset(&g_pc, 0, sizeof(g_pc));
    snprintf(g_pc.next_fix, sizeof(g_pc.next_fix), "%s", "NONE");
    snprintf(g_pc.stop_reason, sizeof(g_pc.stop_reason), "%s", "UNKNOWN");
}

void ext_post_cont_audit_bind_uc(void *uc) { g_pc.uc = uc; }

static int name_is_robotol(const char *nm) {
    if (!nm || !nm[0]) return 0;
    return strstr(nm, "robotol.ext") != NULL;
}

static const char *api_cat_name(ApiCat c) {
    switch (c) {
    case API_CAT_FILE: return "FILE";
    case API_CAT_TIMER: return "TIMER";
    case API_CAT_EVENT: return "EVENT";
    case API_CAT_CALLBACK: return "CALLBACK";
    case API_CAT_DRAW: return "DRAW";
    case API_CAT_BITMAP: return "BITMAP";
    case API_CAT_TEXT: return "TEXT";
    case API_CAT_REFRESH: return "REFRESH";
    case API_CAT_SOUND: return "SOUND";
    case API_CAT_NETWORK: return "NETWORK";
    case API_CAT_STORAGE: return "STORAGE";
    default: return "OTHER";
    }
}

static ApiCat classify_api(const char *name) {
    if (!name || !name[0]) return API_CAT_SKIP;
    /* Skip loader / libc / CFN — not post-continuation progress signals. */
    if (strcmp(name, "_mr_c_function_new") == 0) return API_CAT_SKIP;
    if (strncmp(name, "mr_malloc", 9) == 0 || strncmp(name, "mr_free", 7) == 0 ||
        strncmp(name, "mr_realloc", 10) == 0)
        return API_CAT_SKIP;
    if (strcmp(name, "memcpy") == 0 || strcmp(name, "memset") == 0 || strcmp(name, "memmove") == 0 ||
        strcmp(name, "strcpy") == 0 || strcmp(name, "strlen") == 0 || strcmp(name, "sprintf") == 0 ||
        strcmp(name, "strcmp") == 0 || strcmp(name, "strncmp") == 0)
        return API_CAT_SKIP;
    if (strcmp(name, "mr_printf") == 0 || strcmp(name, "mr_mem_get") == 0 ||
        strcmp(name, "mr_mem_free") == 0)
        return API_CAT_SKIP;

    if (strcmp(name, "mr_timerStart") == 0 || strcmp(name, "mr_timerStop") == 0 ||
        strcmp(name, "timerStart") == 0 || strcmp(name, "timerStop") == 0)
        return API_CAT_TIMER;
    if (strcmp(name, "mr_event") == 0 || strstr(name, "Event") != NULL) return API_CAT_EVENT;
    if (strcmp(name, "mr_drawBitmap") == 0 || strcmp(name, "drawBitmap") == 0)
        return API_CAT_BITMAP;
    if (strstr(name, "draw") != NULL || strstr(name, "Draw") != NULL) return API_CAT_DRAW;
    if (strstr(name, "Text") != NULL || strstr(name, "text") != NULL) return API_CAT_TEXT;
    if (strcmp(name, "mr_refresh") == 0 || strstr(name, "DispUp") != NULL ||
        strstr(name, "Refresh") != NULL)
        return API_CAT_REFRESH;
    if (strstr(name, "Sound") != NULL || strstr(name, "sound") != NULL) return API_CAT_SOUND;
    if (strstr(name, "Socket") != NULL || strstr(name, "Network") != NULL ||
        strstr(name, "send") != NULL || strstr(name, "recv") != NULL ||
        strstr(name, "connect") != NULL || strstr(name, "HostByName") != NULL)
        return API_CAT_NETWORK;
    if (strcmp(name, "mr_open") == 0 || strcmp(name, "mr_close") == 0 ||
        strcmp(name, "mr_read") == 0 || strcmp(name, "mr_write") == 0 ||
        strcmp(name, "mr_seek") == 0 || strcmp(name, "mr_getLen") == 0 ||
        strcmp(name, "mr_remove") == 0 || strcmp(name, "mr_rename") == 0 ||
        strcmp(name, "mr_mkDir") == 0 || strcmp(name, "mr_rmDir") == 0)
        return API_CAT_FILE;
    if (strstr(name, "ferr") != NULL || strstr(name, "Storage") != NULL) return API_CAT_STORAGE;
    if (strcmp(name, "mr_plat") == 0 || strcmp(name, "mr_platEx") == 0) return API_CAT_CALLBACK;
    return API_CAT_SKIP;
}

static PostContEventType cat_to_boundary(ApiCat c) {
    switch (c) {
    case API_CAT_FILE: return PC_EVT_FILE_REQUEST;
    case API_CAT_TIMER: return PC_EVT_TIMER_REQUEST;
    case API_CAT_NETWORK: return PC_EVT_NETWORK_REQUEST;
    case API_CAT_DRAW:
    case API_CAT_BITMAP: return PC_EVT_DRAW_CALL;
    case API_CAT_REFRESH: return PC_EVT_REFRESH_CALL;
    case API_CAT_EVENT: return PC_EVT_EVENT_WAIT;
    default: return PC_EVT_PLATFORM_API;
    }
}

static void resolve_module(uint64_t module_id, uint32_t pc, char *out, size_t out_sz,
                           uint32_t *out_off) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = NULL;
    if (out && out_sz) out[0] = '\0';
    if (out_off) *out_off = 0;
    if (!reg) return;
    if (module_id) m = module_registry_find_by_id(reg, module_id);
    if (!m && pc) m = module_registry_find_by_code_addr(reg, pc & ~1u);
    if (!m) return;
    snprintf(out, out_sz, "%s", m->resolved_name[0] ? m->resolved_name : m->requested_name);
    if (out_off && m->map.guest_code_base && pc >= m->map.guest_code_base &&
        pc < m->map.guest_code_base + m->map.guest_code_size) {
        *out_off = (pc & ~1u) - m->map.guest_code_base;
    }
}

static void refresh_registry_er_rw(void) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *m = NULL;
    size_t i;
    if (!reg) return;
    if (g_pc.module_id) m = module_registry_find_by_id(reg, g_pc.module_id);
    if (!m) {
        for (i = 0; i < reg->count; i++) {
            if (name_is_robotol(reg->modules[i].resolved_name) ||
                name_is_robotol(reg->modules[i].requested_name)) {
                m = &reg->modules[i];
                break;
            }
        }
    }
    if (!m) return;
    g_pc.registry_er_rw = m->data.start_of_er_rw;
    g_pc.registry_er_size = m->data.er_rw_size;
    if (!g_pc.helper && m->entries.registered_helper)
        g_pc.helper = m->entries.registered_helper;
    if (!g_pc.module[0])
        snprintf(g_pc.module, sizeof(g_pc.module), "%s",
                 m->resolved_name[0] ? m->resolved_name : m->requested_name);
    if (!g_pc.module_id) g_pc.module_id = m->module_id;
}

static void peek_p_fields(void *uc) {
    uint32_t er = 0, len = 0;
    if (!uc || !g_pc.p_guest) return;
    if (guest_memory_uc_peek_u32((struct uc_struct *)uc, g_pc.p_guest, &er) && er) {
        g_pc.p_er_rw = er;
        g_pc.load_exit_seen = 1;
    }
    if (guest_memory_uc_peek_u32((struct uc_struct *)uc, g_pc.p_guest + 4u, &len) && len)
        g_pc.p_er_len = len;
}

static void maybe_emit_ready(void) {
    refresh_registry_er_rw();
    if (g_pc.runtime_ready) return;
    if (!g_pc.helper || !g_pc.p_guest) return;
    peek_p_fields(g_pc.uc);
    /* Require P->start_of_ER_RW filled by guest load (DOCUMENTED). */
    if (!g_pc.p_er_rw) return;
    /* At resume instant, P may still hold caller/DSM SB — wait for guest progress. */
    if (g_pc.instr_count == 0 && g_pc.cont_r9 && g_pc.p_er_rw == g_pc.cont_r9) return;
    g_pc.runtime_ready = 1;
    g_pc.load_exit_seen = 1;
    g_pc.event_seq++;
    printf("[ROBOTOL_RUNTIME_READY] p=0x%X er_rw=0x%X er_rw_size=%u helper=0x%X "
           "registry=%s " PC_GATE_LINE " evidence=OBSERVED\n",
           g_pc.p_guest, g_pc.p_er_rw, g_pc.p_er_len ? g_pc.p_er_len : g_pc.registry_er_size,
           g_pc.helper, g_pc.registry_er_rw ? "published" : "pending", PC_GATE_ARGS);
    /* Stage E2: bind as soon as guest P ER_RW is observed (write-watch may miss). */
    if (g_pc.p_guest && g_pc.p_er_rw && g_pc.p_er_len)
        ext_er_rw_bind_restore_peek_and_bind(g_pc.p_guest, "mr_c_function_st_metadata_bind");
    printf("[POST_CONT] seq=%u event_type=RUNTIME_READY guest_pc=0x%X module=%s "
           "module_offset=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X scope_depth=%u "
           "lr=0x%X sp=0x%X instr=%llu " PC_GATE_LINE " evidence=OBSERVED\n",
           g_pc.event_seq, g_pc.last_pc, g_pc.last_module[0] ? g_pc.last_module : g_pc.module,
           g_pc.last_offset, g_pc.last_r0, g_pc.last_r1, g_pc.last_r2, g_pc.last_r3, g_pc.last_r9,
           g_pc.last_scope_depth, g_pc.last_lr, g_pc.last_sp,
           (unsigned long long)g_pc.instr_count, PC_GATE_ARGS);
    fflush(stdout);
}

static void emit_post_cont(PostContEventType t, uint32_t pc, const char *module, uint32_t off,
                           uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9,
                           uint32_t sp, uint32_t lr, uint32_t scope_depth) {
    g_pc.event_seq++;
    printf("[POST_CONT] seq=%u event_type=%s guest_pc=0x%X module=%s module_offset=0x%X "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X scope_depth=%u lr=0x%X sp=0x%X "
           "instr=%llu " PC_GATE_LINE " evidence=OBSERVED\n",
           g_pc.event_seq, ext_post_cont_event_name(t), pc, module ? module : "?", off, r0, r1, r2,
           r3, r9, scope_depth, lr, sp, (unsigned long long)g_pc.instr_count, PC_GATE_ARGS);
    fflush(stdout);
}

static void set_first_api(FirstApi *slot, ApiCat cat, const char *api, uint32_t r0, uint32_t r1,
                          uint32_t r2, uint32_t r3, uint32_t result) {
    if (slot->seen) return;
    slot->seen = 1;
    snprintf(slot->name, sizeof(slot->name), "%s", api ? api : "?");
    snprintf(slot->category, sizeof(slot->category), "%s", api_cat_name(cat));
    snprintf(slot->caller_module, sizeof(slot->caller_module), "%s",
             g_pc.last_module[0] ? g_pc.last_module : g_pc.module);
    slot->caller_offset = g_pc.last_offset;
    slot->r0 = r0;
    slot->r1 = r1;
    slot->r2 = r2;
    slot->r3 = r3;
    slot->result = result;
    slot->seq = g_pc.event_seq + 1u;
    printf("[POST_CONT_PLATFORM_API] seq=%u category=%s api=%s caller_module=%s "
           "caller_offset=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X result=0x%X " PC_GATE_LINE
           " evidence=OBSERVED\n",
           slot->seq, slot->category, slot->name, slot->caller_module, slot->caller_offset, r0, r1,
           r2, r3, result, PC_GATE_ARGS);
    fflush(stdout);
}

static void open_single_fix(const char *fix, int open_graphics, int open_event) {
    snprintf(g_pc.next_fix, sizeof(g_pc.next_fix), "%s", fix ? fix : "NONE");
    g_pc.post_cont_gate_open = 1;
    g_pc.graphics_gate_open = open_graphics ? 1 : 0;
    g_pc.event_sched_gate_open = open_event ? 1 : 0;
}

static void classify_and_emit_summary(const char *stop_reason) {
    PostContClass c = PC_CLASS_UNKNOWN;
    const char *fix = "NONE";
    int open_g = 0, open_e = 0;

    if (g_pc.finalized) return;
    g_pc.finalized = 1;
    if (stop_reason && stop_reason[0])
        snprintf(g_pc.stop_reason, sizeof(g_pc.stop_reason), "%s", stop_reason);

    refresh_registry_er_rw();
    peek_p_fields(g_pc.uc);

    if (g_pc.fault_seen) {
        c = PC_CLASS_NEW_ABI_FAULT;
        fix = "NEW_FAULT_ANALYSIS_ONLY";
    } else if (g_pc.draw_seen && g_pc.refresh_seen) {
        c = PC_CLASS_GRAPHICS_PIPELINE_ACTIVE;
        fix = "NONE";
        open_g = 1;
    } else if (g_pc.draw_seen && !g_pc.refresh_seen) {
        c = PC_CLASS_MISSING_GRAPHICS_REFRESH;
        fix = "GRAPHICS_REFRESH_ONLY";
        open_g = 1;
    } else if (g_pc.first_platform.seen) {
        c = PC_CLASS_RUNTIME_PROGRESS_TO_PLATFORM_API;
        if (g_pc.unimplemented_api)
            fix = "FIRST_MISSING_PLATFORM_API_ONLY";
        else if (g_pc.timer_seen || g_pc.event_seen)
            fix = "EVENT_SCHEDULER_ONLY";
        else
            fix = "FIRST_MISSING_PLATFORM_API_ONLY";
        if (strcmp(fix, "EVENT_SCHEDULER_ONLY") == 0) open_e = 1;
    } else if (g_pc.normal_return_seen && !g_pc.timer_seen && !g_pc.event_seen) {
        c = PC_CLASS_MISSING_EVENT_SCHEDULING;
        fix = "EVENT_SCHEDULER_ONLY";
        open_e = 1;
    } else if (g_pc.normal_return_seen) {
        c = PC_CLASS_NORMAL_IDLE;
        fix = "NONE";
    } else if (g_pc.armed && !g_pc.runtime_ready && g_pc.instr_count >= 64u) {
        c = PC_CLASS_MODULE_ER_RW_RUNTIME_GAP;
        fix = "MODULE_ER_RW_RUNTIME_ONLY";
    } else if (g_pc.armed && g_pc.instr_count < 64u &&
               strcmp(g_pc.stop_reason, "HARNESS_TIMEOUT") == 0) {
        c = PC_CLASS_HARNESS_WINDOW_TOO_SHORT;
        fix = "NONE";
    } else if (g_pc.armed && strcmp(g_pc.stop_reason, "HARNESS_TIMEOUT") == 0) {
        if (!g_pc.runtime_ready) {
            c = PC_CLASS_MODULE_ER_RW_RUNTIME_GAP;
            fix = "MODULE_ER_RW_RUNTIME_ONLY";
        } else if (!g_pc.timer_seen && !g_pc.event_seen) {
            c = PC_CLASS_MISSING_EVENT_SCHEDULING;
            fix = "EVENT_SCHEDULER_ONLY";
            open_e = 1;
        } else {
            c = PC_CLASS_HARNESS_WINDOW_TOO_SHORT;
            fix = "NONE";
        }
    }

    g_pc.last_class = c;
    open_single_fix(fix, open_g, open_e);

    printf("[POST_CONT_SUMMARY] post_continuation_class=%s continuation_instruction_count=%llu "
           "robotol_runtime_ready=%s first_platform_api=%s draw_seen=%s refresh_seen=%s "
           "stop_reason=%s next_allowed_fix=%s first_boundary=%s "
           "load_exit=%s p=0x%X er_rw=0x%X registry_er_rw=0x%X helper=0x%X "
           "normal_return=%s outer_frame_popped=%s helper_abi_ok=%s " PC_GATE_LINE
           " evidence=OBSERVED note=observe_only\n",
           ext_post_cont_class_name(c), (unsigned long long)g_pc.instr_count,
           g_pc.runtime_ready ? "yes" : "no",
           g_pc.first_platform.seen ? g_pc.first_platform.name : "none",
           g_pc.draw_seen ? "yes" : "no", g_pc.refresh_seen ? "yes" : "no", g_pc.stop_reason,
           g_pc.next_fix, g_pc.first_boundary[0] ? g_pc.first_boundary : "none",
           g_pc.load_exit_seen ? "yes" : "no", g_pc.p_guest,
           g_pc.p_er_rw ? g_pc.p_er_rw : g_pc.registry_er_rw, g_pc.registry_er_rw, g_pc.helper,
           g_pc.normal_return_seen ? "yes" : "no", g_pc.outer_frame_popped ? "yes" : "no",
           g_pc.helper_abi_checked ? (g_pc.helper_abi_ok ? "yes" : "no") : "unchecked",
           PC_GATE_ARGS);
    fflush(stdout);
}

static void hit_boundary(PostContEventType t, const char *stop) {
    if (g_pc.boundary_hit) return;
    peek_p_fields(g_pc.uc);
    refresh_registry_er_rw();
    maybe_emit_ready();
    g_pc.boundary_hit = 1;
    snprintf(g_pc.first_boundary, sizeof(g_pc.first_boundary), "%s", ext_post_cont_event_name(t));
    snprintf(g_pc.stop_reason, sizeof(g_pc.stop_reason), "%s", stop ? stop : ext_post_cont_event_name(t));
    classify_and_emit_summary(g_pc.stop_reason);
}

void ext_post_cont_audit_on_continuation_resume(void *uc, uint64_t module_id, const char *module,
                                                uint32_t call_pc, uint32_t continuation_pc,
                                                const uint32_t regs[16], uint32_t sp, uint32_t lr,
                                                uint32_t cpsr) {
    char mod[72];
    uint32_t off = 0;
    if (!ext_post_cont_audit_enabled() || g_pc.armed) return;
    (void)cpsr;
    resolve_module(module_id, continuation_pc, mod, sizeof(mod), &off);
    if (!name_is_robotol(module) && !name_is_robotol(mod)) return;

    g_pc.armed = 1;
    g_pc.uc = uc ? uc : g_pc.uc;
    g_pc.module_id = module_id;
    snprintf(g_pc.module, sizeof(g_pc.module), "%s",
             (module && module[0]) ? module : (mod[0] ? mod : "robotol.ext"));
    g_pc.cont_pc = continuation_pc;
    g_pc.call_pc = call_pc;
    g_pc.cont_r0 = regs ? regs[0] : 0;
    g_pc.cont_r1 = regs ? regs[1] : 0;
    g_pc.cont_r2 = regs ? regs[2] : 0;
    g_pc.cont_r3 = regs ? regs[3] : 0;
    g_pc.cont_r9 = regs ? regs[9] : 0;
    g_pc.cont_sp = sp;
    g_pc.cont_lr = lr;
    g_pc.cont_scope_depth = module_r9_switch_depth();
    g_pc.last_pc = continuation_pc;
    snprintf(g_pc.last_module, sizeof(g_pc.last_module), "%s", g_pc.module);
    g_pc.last_offset = off;
    g_pc.last_r0 = g_pc.cont_r0;
    g_pc.last_r1 = g_pc.cont_r1;
    g_pc.last_r2 = g_pc.cont_r2;
    g_pc.last_r3 = g_pc.cont_r3;
    g_pc.last_r9 = g_pc.cont_r9;
    g_pc.last_sp = sp;
    g_pc.last_lr = lr;
    g_pc.last_scope_depth = g_pc.cont_scope_depth;
    g_pc.load_enter_seen = 1; /* CFN path already entered load sequence before resume */

    refresh_registry_er_rw();
    peek_p_fields(g_pc.uc);

    emit_post_cont(PC_EVT_ROBOTOL_CONTINUATION, continuation_pc, g_pc.module, off, g_pc.cont_r0,
                   g_pc.cont_r1, g_pc.cont_r2, g_pc.cont_r3, g_pc.cont_r9, sp, lr,
                   g_pc.cont_scope_depth);
    printf("[POST_CONT_ARM] module=%s module_id=%llu continuation_pc=0x%X call_pc=0x%X "
           "helper=0x%X p=0x%X " PC_GATE_LINE " evidence=OBSERVED note=post_cont_timeline_start\n",
           g_pc.module, (unsigned long long)g_pc.module_id, continuation_pc, call_pc, g_pc.helper,
           g_pc.p_guest, PC_GATE_ARGS);
    fflush(stdout);
    maybe_emit_ready();
}

void ext_post_cont_audit_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                 uint32_t cpsr) {
    char mod[72];
    uint32_t off = 0;
    if (!ext_post_cont_audit_enabled() || !g_pc.armed || g_pc.finalized || !regs) return;
    (void)cpsr;
    g_pc.uc = uc ? uc : g_pc.uc;
    g_pc.instr_count++;
    resolve_module(module_id, pc, mod, sizeof(mod), &off);
    g_pc.last_pc = pc;
    if (mod[0]) snprintf(g_pc.last_module, sizeof(g_pc.last_module), "%s", mod);
    g_pc.last_offset = off;
    g_pc.last_r0 = regs[0];
    g_pc.last_r1 = regs[1];
    g_pc.last_r2 = regs[2];
    g_pc.last_r3 = regs[3];
    g_pc.last_r9 = regs[9];
    g_pc.last_sp = regs[13];
    g_pc.last_lr = regs[14];
    g_pc.last_scope_depth = module_r9_switch_depth();

    if ((g_pc.instr_count % 4096ull) == 0ull) {
        peek_p_fields(g_pc.uc);
        refresh_registry_er_rw();
        maybe_emit_ready();
        emit_post_cont(PC_EVT_PROGRESS, pc, g_pc.last_module, off, regs[0], regs[1], regs[2],
                       regs[3], regs[9], regs[13], regs[14], g_pc.last_scope_depth);
    } else if ((g_pc.instr_count % 32ull) == 0ull) {
        peek_p_fields(g_pc.uc);
        refresh_registry_er_rw();
        maybe_emit_ready();
    }
}

void ext_post_cont_audit_on_host_api(void *uc, uint32_t slot_addr, const char *api_name,
                                     int is_enter, uint32_t result_r0) {
    ApiCat cat;
    PostContEventType evt;
    uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0, r9 = 0, sp = 0, lr = 0;
    FirstApi *slot = NULL;
    if (!ext_post_cont_audit_enabled() || !g_pc.armed || g_pc.finalized) return;
    (void)slot_addr;
    cat = classify_api(api_name);
    if (cat == API_CAT_SKIP) return;

#ifdef GWY_HAVE_UNICORN
    if (uc) {
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R0, &r0);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R1, &r1);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R2, &r2);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R3, &r3);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_R9, &r9);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_SP, &sp);
        uc_reg_read((uc_engine *)uc, UC_ARM_REG_LR, &lr);
    }
#else
    (void)uc;
#endif

    if (!is_enter) {
        /* Capture result on leave for first-api record. */
        if (g_pc.first_platform.seen && strcmp(g_pc.first_platform.name, api_name ? api_name : "") == 0 &&
            g_pc.first_platform.result == 0)
            g_pc.first_platform.result = result_r0;
        return;
    }

    switch (cat) {
    case API_CAT_FILE: slot = &g_pc.first_file; break;
    case API_CAT_TIMER:
        slot = &g_pc.first_timer;
        g_pc.timer_seen = 1;
        break;
    case API_CAT_EVENT:
        slot = &g_pc.first_event;
        g_pc.event_seen = 1;
        break;
    case API_CAT_CALLBACK: slot = &g_pc.first_callback; break;
    case API_CAT_DRAW:
        slot = &g_pc.first_draw;
        g_pc.draw_seen = 1;
        break;
    case API_CAT_BITMAP:
        slot = &g_pc.first_bitmap;
        g_pc.draw_seen = 1;
        break;
    case API_CAT_TEXT: slot = &g_pc.first_text; break;
    case API_CAT_REFRESH:
        slot = &g_pc.first_refresh;
        g_pc.refresh_seen = 1;
        break;
    case API_CAT_SOUND: slot = &g_pc.first_sound; break;
    case API_CAT_NETWORK: slot = &g_pc.first_network; break;
    case API_CAT_STORAGE: slot = &g_pc.first_storage; break;
    default: break;
    }

    evt = cat_to_boundary(cat);
    if (slot) set_first_api(slot, cat, api_name, r0, r1, r2, r3, 0);
    if (!g_pc.first_platform.seen) {
        set_first_api(&g_pc.first_platform, cat, api_name, r0, r1, r2, r3, 0);
    }
    emit_post_cont(evt, g_pc.last_pc, g_pc.last_module, g_pc.last_offset, r0, r1, r2, r3, r9, sp, lr,
                   module_r9_switch_depth());
    hit_boundary(evt, ext_post_cont_event_name(evt));
}

void ext_post_cont_audit_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                        uint32_t rw_base, uint32_t rw_size) {
    if (!ext_post_cont_audit_enabled()) return;
    /* Track robotol P even if armed later (CFN often precedes resume log). */
    if (helper) g_pc.helper = helper;
    if (p_guest) {
        g_pc.p_guest = p_guest;
        g_pc.p_len = p_len;
        g_pc.load_enter_seen = 1;
    }
    if (rw_base) {
        g_pc.p_er_rw = rw_base;
        g_pc.p_er_len = rw_size;
        g_pc.load_exit_seen = 1;
    }
    if (g_pc.armed) {
        peek_p_fields(g_pc.uc);
        refresh_registry_er_rw();
        maybe_emit_ready();
    }
}

void ext_post_cont_audit_on_helper_call(uint32_t helper, uint32_t method, uint32_t r0, uint32_t r9) {
    uint32_t expect_er;
    if (!ext_post_cont_audit_enabled() || !g_pc.armed || g_pc.finalized) return;
    if (!helper || (g_pc.helper && (helper & ~1u) != (g_pc.helper & ~1u))) return;
    g_pc.helper_abi_checked = 1;
    expect_er = g_pc.p_er_rw ? g_pc.p_er_rw : g_pc.registry_er_rw;
    g_pc.helper_abi_ok = (expect_er && r9 == expect_er && (!g_pc.p_guest || r0 == g_pc.p_guest)) ? 1
                                                                                                  : 0;
    emit_post_cont(PC_EVT_HELPER_CALL, g_pc.last_pc, g_pc.module, g_pc.last_offset, r0, method, 0, 0,
                   r9, g_pc.last_sp, g_pc.last_lr, module_r9_switch_depth());
    printf("[POST_CONT_HELPER_ABI] helper=0x%X method=0x%X r0=0x%X expect_p=0x%X r9=0x%X "
           "expect_er_rw=0x%X ok=%s " PC_GATE_LINE " evidence=OBSERVED\n",
           helper, method, r0, g_pc.p_guest, r9, expect_er, g_pc.helper_abi_ok ? "yes" : "no",
           PC_GATE_ARGS);
    fflush(stdout);
}

void ext_post_cont_audit_on_r9_leave(const char *requested_by, GwyR9LeaveAction action,
                                     GwyEmuExitReason emu_exit, uint64_t top_frame_id,
                                     GwyModuleCallKind top_kind, uint32_t restore_r9) {
    if (!ext_post_cont_audit_enabled() || !g_pc.armed || g_pc.finalized) return;
    (void)requested_by;
    (void)top_frame_id;
    (void)restore_r9;

    if (emu_exit == GWY_EMU_EXIT_YIELD_TO_NESTED_GUEST) {
        emit_post_cont(PC_EVT_YIELD, g_pc.last_pc, g_pc.last_module, g_pc.last_offset, g_pc.last_r0,
                       g_pc.last_r1, g_pc.last_r2, g_pc.last_r3, g_pc.last_r9, g_pc.last_sp,
                       g_pc.last_lr, module_r9_switch_depth());
        /* Log only; tokenized NOOP yield is not a progress boundary by itself. */
        return;
    }

    if (action == GWY_R9_LEAVE_POP_OWNED_FRAME &&
        emu_exit == GWY_EMU_EXIT_NORMAL_GUEST_RETURN) {
        g_pc.normal_return_seen = 1;
        g_pc.outer_frame_popped = (top_kind == GWY_CALL_MR_HELPER) ? 1 : 1;
        g_pc.return_pc = g_pc.last_pc;
        g_pc.return_value = g_pc.last_r0;
        g_pc.return_scope_depth = module_r9_switch_depth();
        snprintf(g_pc.return_module, sizeof(g_pc.return_module), "%s",
                 g_pc.last_module[0] ? g_pc.last_module : "?");
        emit_post_cont(PC_EVT_NORMAL_RETURN, g_pc.return_pc, g_pc.return_module, g_pc.last_offset,
                       g_pc.return_value, g_pc.last_r1, g_pc.last_r2, g_pc.last_r3, g_pc.last_r9,
                       g_pc.last_sp, g_pc.last_lr, g_pc.return_scope_depth);
        printf("[POST_CONT_RETURN] entry_pc=0x%X return_pc=0x%X lr=0x%X return_value=0x%X "
               "return_module=%s scope_depth=%u outer_mr_helper_pop=%s " PC_GATE_LINE
               " evidence=OBSERVED\n",
               g_pc.cont_pc, g_pc.return_pc, g_pc.last_lr, g_pc.return_value, g_pc.return_module,
               g_pc.return_scope_depth, g_pc.outer_frame_popped ? "yes" : "no", PC_GATE_ARGS);
        fflush(stdout);
        hit_boundary(PC_EVT_NORMAL_RETURN, "NORMAL_RETURN");
    }
}

void ext_post_cont_audit_on_mem_fault(void *uc, uint32_t fault_pc, uint32_t fault_addr,
                                      const uint32_t regs[16], uint32_t cpsr) {
    char mod[72];
    uint32_t off = 0;
    if (!ext_post_cont_audit_enabled() || !g_pc.armed || g_pc.finalized) return;
    (void)uc;
    (void)cpsr;
    /* Ignore legacy 0x274 only if somehow still present; still record as fault. */
    g_pc.fault_seen = 1;
    g_pc.fault_pc = fault_pc;
    g_pc.fault_addr = fault_addr;
    g_pc.fault_r9 = regs ? regs[9] : 0;
    g_pc.fault_scope_depth = module_r9_switch_depth();
    resolve_module(g_pc.module_id, fault_pc, mod, sizeof(mod), &off);
    snprintf(g_pc.fault_module, sizeof(g_pc.fault_module), "%s", mod[0] ? mod : g_pc.module);
    g_pc.fault_offset = off;
    emit_post_cont(PC_EVT_NEW_FAULT, fault_pc, g_pc.fault_module, off, regs ? regs[0] : 0,
                   regs ? regs[1] : 0, regs ? regs[2] : 0, regs ? regs[3] : 0, g_pc.fault_r9,
                   regs ? regs[13] : 0, regs ? regs[14] : 0, g_pc.fault_scope_depth);
    printf("[POST_CONT_FAULT] module=%s module_offset=0x%X fault_pc=0x%X fault_addr=0x%X "
           "r0=0x%X r1=0x%X r2=0x%X r3=0x%X r9=0x%X scope_depth=%u lr=0x%X sp=0x%X " PC_GATE_LINE
           " evidence=OBSERVED\n",
           g_pc.fault_module, off, fault_pc, fault_addr, regs ? regs[0] : 0, regs ? regs[1] : 0,
           regs ? regs[2] : 0, regs ? regs[3] : 0, g_pc.fault_r9, g_pc.fault_scope_depth,
           regs ? regs[14] : 0, regs ? regs[13] : 0, PC_GATE_ARGS);
    fflush(stdout);
    hit_boundary(PC_EVT_NEW_FAULT, "NEW_FAULT");
}

void ext_post_cont_audit_on_host_event(const char *event_name, int32_t code) {
    if (!ext_post_cont_audit_enabled() || !g_pc.armed || g_pc.finalized) return;
    g_pc.event_seen = 1;
    if (!g_pc.first_event.seen) {
        set_first_api(&g_pc.first_event, API_CAT_EVENT, event_name ? event_name : "host_event",
                      (uint32_t)code, 0, 0, 0, 0);
        if (!g_pc.first_platform.seen)
            set_first_api(&g_pc.first_platform, API_CAT_EVENT,
                          event_name ? event_name : "host_event", (uint32_t)code, 0, 0, 0, 0);
    }
    emit_post_cont(PC_EVT_EVENT_WAIT, g_pc.last_pc, g_pc.last_module, g_pc.last_offset,
                   (uint32_t)code, 0, 0, 0, g_pc.last_r9, g_pc.last_sp, g_pc.last_lr,
                   module_r9_switch_depth());
    hit_boundary(PC_EVT_EVENT_WAIT, "EVENT_WAIT");
}

void ext_post_cont_audit_finalize(const char *stop_reason) {
    if (!ext_post_cont_audit_enabled() || !g_pc.armed) return;
    if (g_pc.finalized) return;
    classify_and_emit_summary(stop_reason ? stop_reason : "EXPLICIT_STOP");
}

void ext_post_cont_audit_note_unimplemented(const char *api_name) {
    if (!ext_post_cont_audit_enabled() || !g_pc.armed || g_pc.finalized) return;
    g_pc.unimplemented_api = 1;
    if (!g_pc.first_platform.seen) {
        ApiCat cat = classify_api(api_name);
        if (cat == API_CAT_SKIP) cat = API_CAT_CALLBACK;
        set_first_api(&g_pc.first_platform, cat, api_name, 0, 0, 0, 0, 0xFFFFFFFFu);
    }
    emit_post_cont(PC_EVT_PLATFORM_API, g_pc.last_pc, g_pc.last_module, g_pc.last_offset, 0, 0, 0, 0,
                   g_pc.last_r9, g_pc.last_sp, g_pc.last_lr, module_r9_switch_depth());
    hit_boundary(PC_EVT_PLATFORM_API, "PLATFORM_API");
}
