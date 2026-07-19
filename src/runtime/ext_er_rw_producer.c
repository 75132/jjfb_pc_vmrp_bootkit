#include "gwy_launcher/ext_er_rw_producer.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define GWY_ERP_RING 192
#define GWY_ERP_TRACK 8

typedef enum {
    ERP_CODE_IMAGE_EXTRACTED = 1,
    ERP_CODE_IMAGE_MAPPED,
    ERP_LOADCODE_ENTER,
    ERP_LOADCODE_EXIT,
    ERP_DSM_FIRST_MODULE_ENTRY_SELECT,
    ERP_DSM_FIRST_MODULE_ENTRY_CALL,
    ERP_R9_SWITCH_ENTER,
    ERP_R9_SWITCH_BLOCKED,
    ERP_MR_C_FUNCTION_LOAD_ENTER,
    ERP_MR_C_FUNCTION_LOAD_EXIT,
    ERP_ER_RW_ALLOC,
    ERP_ER_RW_INIT,
    ERP_ER_RW_PUBLISH,
    ERP_MR_C_FUNCTION_NEW,
    ERP_MR_HELPER,
    ERP_HELPER_CALL,
    ERP_ENTRY_FAULT
} ErpEventKind;

typedef struct {
    uint32_t seq;
    ErpEventKind kind;
    uint64_t module_id;
    char module[72];
    uint32_t base;
    uint32_t size;
    uint32_t helper;
    uint32_t producer_offset;
} ErpRingSlot;

typedef struct {
    int in_use;
    uint64_t module_id;
    char module[72];
    uint32_t code_base;
    uint32_t code_size;
    uint32_t first_entry_seq;
    uint32_t first_entry_r9;
    uint32_t first_entry_pc;
    uint32_t blocked_seq;
    uint32_t blocked_effective_r9;
    uint32_t caller_r9_at_block;
    int blocked_outcome_emitted;
    int entry_after_block;
    uint32_t er_rw_base;
    uint32_t er_rw_size;
    uint32_t er_rw_publish_seq;
    uint32_t alloc_match_seq;
    uint32_t init_match_seq;
    char producer_kind[24];
    char producer_module[72];
    uint32_t producer_offset;
    int cfn_seen;
    uint32_t cfn_seq;
    int helper_seen;
    uint32_t helper_seq;
    int pre_entry_meta_emitted;
    int identity_emitted;
    int contract_emitted;
    int class_emitted;
    int birth_corr_emitted;
    ErRwEntryIdentity entry_identity;
    ErRwTimingClass timing_class;
    char next_fix[48];
    char gate_evidence[24];
} ErpTrack;

typedef struct {
    void *uc;
    uint32_t seq;
    ErpRingSlot ring[GWY_ERP_RING];
    int ring_count;
    int ring_head;
    ErpTrack tracks[GWY_ERP_TRACK];
    int dsm_entry_select_logged;
    int dsm_loadcode_enter;
    int global_contract_logged;
    ErRwTimingClass last_class;
    char focus_module[72];
    uint64_t focus_mid;
} ErpState;

static ErpState g_erp;

void ext_er_rw_producer_reset(void) { memset(&g_erp, 0, sizeof(g_erp)); }

int ext_er_rw_producer_enabled(void) {
    const char *env = getenv("GWY_ER_RW_PRODUCER");
    return env && env[0] == '1';
}

void ext_er_rw_producer_bind_uc(void *uc) { g_erp.uc = uc; }

const char *ext_er_rw_timing_class_name(ErRwTimingClass c) {
    switch (c) {
        case ER_RW_METADATA_PUBLICATION_LATE: return "ER_RW_METADATA_PUBLICATION_LATE";
        case ER_RW_BOOTSTRAP_ENTRY_PRECEDES: return "BOOTSTRAP_ENTRY_PRECEDES_ER_RW";
        case ER_RW_MODULE_ENTRY_ORDERING_WRONG: return "MODULE_ENTRY_ORDERING_WRONG";
        case ER_RW_DESCRIPTOR_NOT_EXPOSED: return "ER_RW_DESCRIPTOR_NOT_EXPOSED";
        case ER_RW_BOOTSTRAP_ENTRY_MISCLASSIFIED: return "BOOTSTRAP_ENTRY_MISCLASSIFIED";
        default: return "UNKNOWN";
    }
}

ErRwTimingClass ext_er_rw_producer_last_class(void) { return g_erp.last_class; }

static const char *event_name(ErpEventKind k) {
    switch (k) {
        case ERP_CODE_IMAGE_EXTRACTED: return "CODE_IMAGE_EXTRACTED";
        case ERP_CODE_IMAGE_MAPPED: return "CODE_IMAGE_MAPPED";
        case ERP_LOADCODE_ENTER: return "LOADCODE_ENTER";
        case ERP_LOADCODE_EXIT: return "LOADCODE_EXIT";
        case ERP_DSM_FIRST_MODULE_ENTRY_SELECT: return "DSM_FIRST_MODULE_ENTRY_SELECT";
        case ERP_DSM_FIRST_MODULE_ENTRY_CALL: return "DSM_FIRST_MODULE_ENTRY_CALL";
        case ERP_R9_SWITCH_ENTER: return "R9_SWITCH_ENTER";
        case ERP_R9_SWITCH_BLOCKED: return "R9_SWITCH_BLOCKED";
        case ERP_MR_C_FUNCTION_LOAD_ENTER: return "MR_C_FUNCTION_LOAD_ENTER";
        case ERP_MR_C_FUNCTION_LOAD_EXIT: return "MR_C_FUNCTION_LOAD_EXIT";
        case ERP_ER_RW_ALLOC: return "ER_RW_ALLOC";
        case ERP_ER_RW_INIT: return "ER_RW_INIT";
        case ERP_ER_RW_PUBLISH: return "ER_RW_PUBLISH";
        case ERP_MR_C_FUNCTION_NEW: return "MR_C_FUNCTION_NEW";
        case ERP_MR_HELPER: return "MR_HELPER";
        case ERP_HELPER_CALL: return "HELPER_CALL";
        case ERP_ENTRY_FAULT: return "ENTRY_FAULT";
        default: return "UNKNOWN";
    }
}

static const char *er_rw_source_name(GwyErRwSource s) {
    switch (s) {
        case GWY_ER_RW_SRC_LOADER_DOCUMENTED: return "LOADER_DOCUMENTED";
        case GWY_ER_RW_SRC_MR_C_FUNCTION_LOAD: return "MR_C_FUNCTION_LOAD";
        case GWY_ER_RW_SRC_EXT_RUNTIME_DESCRIPTOR: return "EXT_RUNTIME_DESCRIPTOR";
        case GWY_ER_RW_SRC_CROSS_TARGET: return "CROSS_TARGET";
        default: return "UNKNOWN";
    }
}

static uint32_t next_seq(void) { return ++g_erp.seq; }

static int name_is_dsm(const GwyLoadedModule *m) {
    if (!m) return 0;
    if (m->origin == MODULE_ORIGIN_DSM) return 1;
    if (m->requested_name[0] && strstr(m->requested_name, "dsm:")) return 1;
    return 0;
}

static int name_is_nested_focus(const GwyLoadedModule *m) {
    const char *n;
    if (!m || name_is_dsm(m)) return 0;
    n = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    if (!n || !n[0]) return 0;
    if (strstr(n, "mrc_loader")) return 0;
    if (strstr(n, "robotol")) return 1;
    if (m->origin == MODULE_ORIGIN_MRP_MEMBER && strstr(n, ".ext")) return 1;
    return 0;
}

static void mod_name(const GwyLoadedModule *m, char *out, size_t n) {
    const char *s;
    if (!out || n == 0) return;
    out[0] = 0;
    if (!m) return;
    s = m->resolved_name[0] ? m->resolved_name : m->requested_name;
    if (s && s[0]) snprintf(out, n, "%s", s);
}

static ErpTrack *track_find(uint64_t mid) {
    int i;
    for (i = 0; i < GWY_ERP_TRACK; i++) {
        if (g_erp.tracks[i].in_use && g_erp.tracks[i].module_id == mid) return &g_erp.tracks[i];
    }
    return NULL;
}

static ErpTrack *track_ensure(uint64_t mid, const char *name) {
    ErpTrack *t = track_find(mid);
    int i;
    if (t) {
        if (name && name[0] && !t->module[0]) snprintf(t->module, sizeof(t->module), "%s", name);
        return t;
    }
    for (i = 0; i < GWY_ERP_TRACK; i++) {
        if (!g_erp.tracks[i].in_use) {
            t = &g_erp.tracks[i];
            memset(t, 0, sizeof(*t));
            t->in_use = 1;
            t->module_id = mid;
            if (name && name[0]) snprintf(t->module, sizeof(t->module), "%s", name);
            snprintf(t->next_fix, sizeof(t->next_fix), "%s", "UNKNOWN");
            snprintf(t->gate_evidence, sizeof(t->gate_evidence), "%s", "OBSERVED");
            snprintf(t->producer_kind, sizeof(t->producer_kind), "%s", "UNKNOWN");
            return t;
        }
    }
    return NULL;
}

static void emit_timeline(const ErpRingSlot *s) {
    printf("[ER_RW_TIMELINE] seq=%u module=%s module_id=%llu event=%s base=0x%X size=%u "
           "helper=0x%X producer_offset=0x%X er_rw_metadata_timing_gate=blocked "
           "phase6b_b_gate=blocked evidence=OBSERVED\n",
           s->seq, s->module[0] ? s->module : "?", (unsigned long long)s->module_id,
           event_name(s->kind), s->base, s->size, s->helper, s->producer_offset);
    fflush(stdout);
}

static uint32_t push_event(ErpEventKind kind, uint64_t mid, const char *mod, uint32_t base,
                           uint32_t size, uint32_t helper, uint32_t off) {
    ErpRingSlot *s;
    uint32_t seq;
    if (!ext_er_rw_producer_enabled()) return 0;
    seq = next_seq();
    if (g_erp.ring_count < GWY_ERP_RING) {
        s = &g_erp.ring[g_erp.ring_count++];
    } else {
        s = &g_erp.ring[g_erp.ring_head];
        g_erp.ring_head = (g_erp.ring_head + 1) % GWY_ERP_RING;
    }
    memset(s, 0, sizeof(*s));
    s->seq = seq;
    s->kind = kind;
    s->module_id = mid;
    if (mod && mod[0]) snprintf(s->module, sizeof(s->module), "%s", mod);
    s->base = base;
    s->size = size;
    s->helper = helper;
    s->producer_offset = off;
    emit_timeline(s);
    return seq;
}

static void emit_contract_once(void) {
    if (g_erp.global_contract_logged) return;
    g_erp.global_contract_logged = 1;
    /* DOCUMENTED: FixR9 / 反汇编研究 — guest mr_c_function_load(code==0) allocates,
     * copies RW template, zeros BSS, writes P->start_of_ER_RW. Host publishes via P read. */
    printf("[CFUNCTION_LOAD_CONTRACT] allocates_er_rw=yes initializes_er_rw=yes "
           "publishes_er_rw=yes requires_module_entry_first=unknown "
           "evidence=DOCUMENTED note=guest_mrc_malloc_then_P_fill "
           "er_rw_metadata_timing_gate=blocked\n");
    fflush(stdout);
}

static void emit_pre_entry_meta(ErpTrack *t, const GwyLoadedModule *m, uint32_t entry_r9) {
    uint32_t reg_er = 0;
    uint32_t map_rw = 0;
    int present = 0;
    if (!t || t->pre_entry_meta_emitted) return;
    t->pre_entry_meta_emitted = 1;
    if (m) {
        reg_er = m->data.start_of_er_rw;
        map_rw = m->map.guest_rw_base;
    }
    present = (reg_er != 0) || (map_rw != 0);
    printf("[ER_RW_PRE_ENTRY_META] module=%s module_id=%llu present=%s "
           "field=registry_start_of_er_rw value=0x%X map_guest_rw=0x%X entry_r9=0x%X "
           "evidence=OBSERVED note=caller_dsm_P_not_callee\n",
           t->module[0] ? t->module : "?", (unsigned long long)t->module_id,
           present ? "yes" : "no", reg_er, map_rw, entry_r9);
    fflush(stdout);
}

static void correlate_birth(ErpTrack *t) {
    int i, n, idx;
    uint32_t best_seq = 0;
    ErpEventKind best_kind = 0;
    const char *rel = "NEVER";
    if (!t || !t->er_rw_base || t->birth_corr_emitted) return;
    t->birth_corr_emitted = 1;
    n = g_erp.ring_count < GWY_ERP_RING ? g_erp.ring_count : GWY_ERP_RING;
    for (i = 0; i < n; i++) {
        idx = (g_erp.ring_count < GWY_ERP_RING) ? i : ((g_erp.ring_head + i) % GWY_ERP_RING);
        {
            const ErpRingSlot *s = &g_erp.ring[idx];
            if (s->base == t->er_rw_base &&
                (s->kind == ERP_ER_RW_ALLOC || s->kind == ERP_ER_RW_INIT ||
                 s->kind == ERP_MR_C_FUNCTION_NEW || s->kind == ERP_ER_RW_PUBLISH)) {
                if (!best_seq || s->seq < best_seq) {
                    best_seq = s->seq;
                    best_kind = s->kind;
                }
            }
            /* mrc_malloc often returns payload after a small header; allow +4/+8. */
            if ((s->kind == ERP_ER_RW_ALLOC || s->kind == ERP_ER_RW_INIT) && s->base &&
                (s->base == t->er_rw_base || s->base + 4u == t->er_rw_base ||
                 s->base + 8u == t->er_rw_base)) {
                if (!best_seq || s->seq < best_seq) {
                    best_seq = s->seq;
                    best_kind = s->kind;
                    t->alloc_match_seq = s->seq;
                    if (s->kind == ERP_ER_RW_ALLOC)
                        snprintf(t->producer_kind, sizeof(t->producer_kind), "%s", "ALLOC");
                    else if (s->kind == ERP_ER_RW_INIT)
                        snprintf(t->producer_kind, sizeof(t->producer_kind), "%s", "INIT");
                    if (s->module[0])
                        snprintf(t->producer_module, sizeof(t->producer_module), "%s", s->module);
                    t->producer_offset = s->producer_offset;
                }
            }
        }
    }
    if (!t->alloc_match_seq && t->er_rw_publish_seq) {
        snprintf(t->producer_kind, sizeof(t->producer_kind), "%s", "PUBLISH");
        best_seq = t->er_rw_publish_seq;
        best_kind = ERP_ER_RW_PUBLISH;
    }
    if (t->first_entry_seq && best_seq) {
        rel = (best_seq < t->first_entry_seq) ? "BEFORE_ENTRY" : "AFTER_ENTRY";
    } else if (best_seq && !t->first_entry_seq) {
        rel = "BEFORE_ENTRY";
    }
    printf("[ER_RW_PRODUCER] module=%s module_id=%llu base=0x%X size=%u producer_kind=%s "
           "producer_module=%s producer_offset=0x%X seq=%u evidence=OBSERVED "
           "note=no_hardcoded_address\n",
           t->module[0] ? t->module : "?", (unsigned long long)t->module_id, t->er_rw_base,
           t->er_rw_size, t->producer_kind,
           t->producer_module[0] ? t->producer_module : "?", t->producer_offset,
           best_seq ? best_seq : t->er_rw_publish_seq);
    printf("[ER_RW_BIRTH_CORR] er_rw_base=0x%X size=%u match_ord=%u match_kind=%s "
           "entry_ord=%u blocked_ord=%u relation=%s evidence=OBSERVED "
           "note=no_hardcoded_address\n",
           t->er_rw_base, t->er_rw_size, best_seq, best_seq ? event_name(best_kind) : "NONE",
           t->first_entry_seq, t->blocked_seq, rel);
    fflush(stdout);
}

static int track_is_focus(const ErpTrack *t) {
    if (!t || !t->in_use) return 0;
    if (strstr(t->module, "robotol")) return 1;
    if (g_erp.focus_mid && t->module_id == g_erp.focus_mid) return 1;
    /* Nested game EXT (not loader): name contains .ext and not mrc_loader. */
    if (t->module[0] && strstr(t->module, ".ext") && !strstr(t->module, "mrc_loader") &&
        !strstr(t->module, "cfunction"))
        return 1;
    return 0;
}

static void maybe_emit_identity(ErpTrack *t) {
    const char *id;
    const char *ev = "OBSERVED";
    ErRwEntryIdentity prev;
    if (!t || !t->first_entry_seq || !track_is_focus(t)) return;
    prev = t->entry_identity;
    /* Entry before CFN/ER_RW publish → bootstrap; else runtime if helper already seen. */
    if (t->er_rw_publish_seq && t->first_entry_seq < t->er_rw_publish_seq) {
        t->entry_identity = ER_RW_ENTRY_BOOTSTRAP;
    } else if (t->cfn_seen && t->cfn_seq && t->first_entry_seq < t->cfn_seq) {
        t->entry_identity = ER_RW_ENTRY_BOOTSTRAP;
    } else if (t->blocked_seq && !t->er_rw_publish_seq && !t->cfn_seen) {
        /* First CODE_IMAGE entry blocked with no callee ER_RW yet → bootstrap lean. */
        t->entry_identity = ER_RW_ENTRY_BOOTSTRAP;
    } else if (t->helper_seen && t->helper_seq && t->first_entry_seq > t->helper_seq &&
               t->er_rw_publish_seq && t->first_entry_seq > t->er_rw_publish_seq) {
        t->entry_identity = ER_RW_ENTRY_MODULE_RUNTIME;
    } else if (!t->identity_emitted) {
        t->entry_identity = ER_RW_ENTRY_UNKNOWN;
    }
    if (t->identity_emitted && prev == t->entry_identity) return;
    t->identity_emitted = 1;
    id = (t->entry_identity == ER_RW_ENTRY_BOOTSTRAP)     ? "BOOTSTRAP_ENTRY"
         : (t->entry_identity == ER_RW_ENTRY_MODULE_RUNTIME) ? "MODULE_RUNTIME_ENTRY"
                                                            : "UNKNOWN";
    printf("[ENTRY_IDENTITY] module=%s module_id=%llu identity=%s first_entry_seq=%u "
           "cfn_seq=%u er_rw_publish_seq=%u helper_seq=%u evidence=%s "
           "er_rw_metadata_timing_gate=blocked\n",
           t->module[0] ? t->module : "?", (unsigned long long)t->module_id, id,
           t->first_entry_seq, t->cfn_seq, t->er_rw_publish_seq, t->helper_seq, ev);
    fflush(stdout);
}

static void maybe_emit_class(ErpTrack *t) {
    ErRwTimingClass c = ER_RW_TIMING_UNKNOWN;
    const char *next = "UNKNOWN";
    const char *gev = "OBSERVED";
    int ready = 0;
    if (!t || !t->first_entry_seq || !track_is_focus(t)) return;

    /* Finalize only with discriminating evidence — not on first blocked alone for loader noise. */
    if (t->er_rw_publish_seq)
        ready = 1;
    else if (t->cfn_seen && t->cfn_seq && t->first_entry_seq && t->first_entry_seq < t->cfn_seq)
        ready = 1;
    else if (t->blocked_seq && t->entry_after_block)
        ready = 1; /* enough to lean Case B: entry ran without callee ER_RW */
    else
        return;

    if (t->er_rw_publish_seq && t->er_rw_publish_seq < t->first_entry_seq && t->blocked_seq) {
        /* Metadata existed before entry but switch blocked → late registry (needs XT to open fix). */
        c = ER_RW_METADATA_PUBLICATION_LATE;
        next = "UNKNOWN";
        gev = "TARGET_OBSERVED";
    } else if ((t->er_rw_publish_seq && t->first_entry_seq < t->er_rw_publish_seq) ||
               (t->blocked_seq && (!t->er_rw_publish_seq || t->first_entry_seq < t->er_rw_publish_seq))) {
        c = ER_RW_BOOTSTRAP_ENTRY_PRECEDES;
        /* DOCUMENTED: mr_c_function_load allocates ER_RW; OBSERVED: first entry precedes it. */
        next = "BOOTSTRAP_ENTRY_R9_CONTRACT";
        gev = "DOCUMENTED";
        t->entry_identity = ER_RW_ENTRY_BOOTSTRAP;
    } else if (t->entry_identity == ER_RW_ENTRY_MODULE_RUNTIME && t->blocked_seq) {
        c = ER_RW_BOOTSTRAP_ENTRY_MISCLASSIFIED;
        next = "ENTRY_IDENTITY_CORRECTION";
        gev = "OBSERVED";
    } else {
        c = ER_RW_TIMING_UNKNOWN;
        next = "UNKNOWN";
        gev = "OBSERVED";
    }

    if (t->class_emitted && t->timing_class == c) return;

    if (t->er_rw_publish_seq && t->first_entry_seq && t->first_entry_seq < t->er_rw_publish_seq) {
        printf("[CFUNCTION_LOAD_CONTRACT] allocates_er_rw=yes initializes_er_rw=yes "
               "publishes_er_rw=yes requires_module_entry_first=yes "
               "evidence=OBSERVED note=first_entry_seq_lt_publish_seq "
               "er_rw_metadata_timing_gate=blocked\n");
        fflush(stdout);
    } else if (t->blocked_seq && !t->er_rw_publish_seq) {
        printf("[CFUNCTION_LOAD_CONTRACT] allocates_er_rw=yes initializes_er_rw=yes "
               "publishes_er_rw=yes requires_module_entry_first=yes "
               "evidence=OBSERVED note=entry_blocked_before_callee_er_rw "
               "er_rw_metadata_timing_gate=blocked\n");
        fflush(stdout);
    }

    t->timing_class = c;
    t->class_emitted = 1;
    g_erp.last_class = c;
    snprintf(t->next_fix, sizeof(t->next_fix), "%s", next);
    snprintf(t->gate_evidence, sizeof(t->gate_evidence), "%s", gev);

    printf("[ER_RW_TIMING_CLASS] class=%s module=%s module_id=%llu "
           "first_entry_seq=%u er_rw_publish_seq=%u alloc_match_seq=%u "
           "blocked_outcome=%s entry_identity=%s "
           "er_rw_metadata_timing_gate=blocked module_r9_switch_gate=open "
           "phase6b_b_gate=blocked gate_evidence=%s next_allowed_fix=%s "
           "evidence=%s note=observe_only_no_order_change\n",
           ext_er_rw_timing_class_name(c), t->module[0] ? t->module : "?",
           (unsigned long long)t->module_id, t->first_entry_seq, t->er_rw_publish_seq,
           t->alloc_match_seq,
           t->blocked_outcome_emitted ? "SWITCH_BLOCKED_ENTRY_EXECUTED_WITH_CALLER_R9" : "none",
           (t->entry_identity == ER_RW_ENTRY_BOOTSTRAP)         ? "BOOTSTRAP_ENTRY"
           : (t->entry_identity == ER_RW_ENTRY_MODULE_RUNTIME) ? "MODULE_RUNTIME_ENTRY"
                                                                : "UNKNOWN",
           gev, next, gev);
    fflush(stdout);

    {
        const char *pat = "UNKNOWN";
        if (c == ER_RW_BOOTSTRAP_ENTRY_PRECEDES) pat = "B";
        else if (c == ER_RW_METADATA_PUBLICATION_LATE) pat = "A";
        else if (c == ER_RW_MODULE_ENTRY_ORDERING_WRONG) pat = "C";
        else if (t->first_entry_seq || t->er_rw_publish_seq) pat = "PARTIAL";
        printf("[XT_ER_RW_ORDER] target=jjfb pattern=%s first_entry_seq=%u er_rw_seq=%u "
               "blocked_seq=%u evidence=OBSERVED\n",
               pat, t->first_entry_seq, t->er_rw_publish_seq, t->blocked_seq);
        fflush(stdout);
    }
    (void)ready;
}

void ext_er_rw_producer_on_member_open(const char *guest_path) {
    ModuleRegistry *reg;
    size_t i;
    if (!ext_er_rw_producer_enabled()) return;
    emit_contract_once();
    reg = gwy_ext_loader_bound_registry();
    if (!reg || !guest_path) return;
    for (i = 0; i < reg->count; i++) {
        const GwyLoadedModule *m = &reg->modules[i];
        char nm[72];
        if (!name_is_nested_focus(m)) continue;
        if (strstr(m->requested_name, guest_path) || strstr(m->resolved_name, guest_path) ||
            (guest_path && strstr(guest_path, m->resolved_name))) {
            mod_name(m, nm, sizeof(nm));
            track_ensure(m->module_id, nm);
            push_event(ERP_CODE_IMAGE_EXTRACTED, m->module_id, nm, 0, m->extracted_size, 0, 0);
            if (name_is_nested_focus(m) && strstr(nm, "robotol")) {
                g_erp.focus_mid = m->module_id;
                snprintf(g_erp.focus_module, sizeof(g_erp.focus_module), "%s", nm);
            }
        }
    }
}

void ext_er_rw_producer_on_code_image(uint32_t guest_addr, uint32_t size) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    char nm[72];
    ErpTrack *t;
    if (!ext_er_rw_producer_enabled()) return;
    emit_contract_once();
    if (!g_erp.dsm_loadcode_enter) {
        g_erp.dsm_loadcode_enter = 1;
        push_event(ERP_LOADCODE_ENTER, 0, "dsm", guest_addr, size, 0, 0);
        push_event(ERP_LOADCODE_EXIT, 0, "dsm", guest_addr, size, 0, 0);
    }
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_code_addr(reg, guest_addr) : NULL;
    if (!m) {
        /* size-match extracted MRP members */
        size_t i;
        if (reg) {
            for (i = 0; i < reg->count; i++) {
                const GwyLoadedModule *x = &reg->modules[i];
                if (x->extracted_size && x->extracted_size == size && name_is_nested_focus(x)) {
                    m = x;
                    break;
                }
            }
        }
    }
    if (!m) {
        push_event(ERP_CODE_IMAGE_MAPPED, 0, "?", guest_addr, size, 0, 0);
        return;
    }
    mod_name(m, nm, sizeof(nm));
    t = track_ensure(m->module_id, nm);
    if (t) {
        t->code_base = guest_addr ? guest_addr : m->map.guest_code_base;
        t->code_size = size ? size : m->map.guest_code_size;
        if (name_is_nested_focus(m) && (!g_erp.focus_mid || strstr(nm, "robotol"))) {
            g_erp.focus_mid = m->module_id;
            snprintf(g_erp.focus_module, sizeof(g_erp.focus_module), "%s", nm);
        }
    }
    push_event(ERP_CODE_IMAGE_MAPPED, m->module_id, nm, guest_addr, size, 0, 0);
}

void ext_er_rw_producer_on_mapped(uint64_t module_id, uint32_t code_base, uint32_t code_size) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    char nm[72];
    ErpTrack *t;
    if (!ext_er_rw_producer_enabled() || !module_id) return;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    mod_name(m, nm, sizeof(nm));
    t = track_ensure(module_id, nm);
    if (t) {
        t->code_base = code_base;
        t->code_size = code_size;
    }
    push_event(ERP_CODE_IMAGE_MAPPED, module_id, nm, code_base, code_size, 0, 0);
}

void ext_er_rw_producer_on_alloc(uint32_t guest_addr, uint32_t size) {
    char nm[72];
    ErpTrack *t = NULL;
    ModuleRegistry *reg;
    /* Heuristic nested ER_RW sizes (documented RW+ZI range); never JJFB-literal. */
    int likely_er_rw = (size >= 0x100u && size <= 0x20000u && (size % 4u) == 0);
    if (!ext_er_rw_producer_enabled() || !guest_addr) return;
    reg = gwy_ext_loader_bound_registry();
    if (reg && g_erp.focus_mid) {
        const GwyLoadedModule *m = module_registry_find_by_id(reg, g_erp.focus_mid);
        mod_name(m, nm, sizeof(nm));
        t = track_ensure(g_erp.focus_mid, nm);
    }
    if (likely_er_rw) {
        push_event(ERP_ER_RW_ALLOC, t ? t->module_id : 0, t ? t->module : "?", guest_addr, size, 0,
                   0);
    }
}

void ext_er_rw_producer_on_block_copy(uint32_t dst, uint32_t src, uint32_t len) {
    ErpTrack *t = NULL;
    char nm[72];
    if (!ext_er_rw_producer_enabled() || !dst || !len) return;
    (void)src;
    if (len >= 0x100u && len <= 0x20000u) {
        if (g_erp.focus_mid) {
            ModuleRegistry *reg = gwy_ext_loader_bound_registry();
            const GwyLoadedModule *m =
                reg ? module_registry_find_by_id(reg, g_erp.focus_mid) : NULL;
            mod_name(m, nm, sizeof(nm));
            t = track_ensure(g_erp.focus_mid, nm);
        }
        push_event(ERP_ER_RW_INIT, t ? t->module_id : 0, t ? t->module : "?", dst, len, 0, 0);
        if (t) t->init_match_seq = g_erp.seq;
    }
}

void ext_er_rw_producer_on_cfunction_p(uint32_t helper, uint32_t p_guest, uint32_t p_len,
                                       uint32_t rw_base, uint32_t rw_size) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m = NULL;
    char nm[72];
    ErpTrack *t;
    uint32_t seq;
    if (!ext_er_rw_producer_enabled()) return;
    (void)p_guest;
    (void)p_len;
    reg = gwy_ext_loader_bound_registry();
    if (reg && helper) {
        m = module_registry_find_by_helper(reg, helper);
        if (!m) m = module_registry_find_by_code_addr(reg, helper & ~1u);
    }
    mod_name(m, nm, sizeof(nm));
    t = m ? track_ensure(m->module_id, nm) : NULL;
    seq = push_event(ERP_MR_C_FUNCTION_NEW, m ? m->module_id : 0, nm, rw_base, rw_size, helper, 0);
    if (t) {
        t->cfn_seen = 1;
        if (!t->cfn_seq) t->cfn_seq = seq;
        if (t->first_entry_seq && seq > t->first_entry_seq) {
            push_event(ERP_MR_C_FUNCTION_LOAD_ENTER, t->module_id, t->module, rw_base, rw_size,
                       helper, 0);
            if (rw_base) {
                push_event(ERP_MR_C_FUNCTION_LOAD_EXIT, t->module_id, t->module, rw_base, rw_size,
                           helper, 0);
            }
        }
        if (rw_base && track_is_focus(t)) {
            if (!t->er_rw_base) {
                t->er_rw_base = rw_base;
                t->er_rw_size = rw_size;
            }
            /* OBSERVED P fill may arrive before ModuleRegistry publish for nested EXT. */
            if (!t->er_rw_publish_seq) {
                t->er_rw_publish_seq = push_event(ERP_ER_RW_PUBLISH, t->module_id, t->module, rw_base,
                                                  rw_size, helper, 0);
                snprintf(t->producer_kind, sizeof(t->producer_kind), "%s", "PUBLISH");
                correlate_birth(t);
            }
            maybe_emit_identity(t);
            maybe_emit_class(t);
        } else if (rw_base && !t->er_rw_base) {
            t->er_rw_base = rw_base;
            t->er_rw_size = rw_size;
        }
        maybe_emit_identity(t);
    }
}

void ext_er_rw_producer_on_helper_call(uint32_t helper, uint32_t method) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    char nm[72];
    ErpTrack *t;
    uint32_t seq;
    if (!ext_er_rw_producer_enabled() || !helper) return;
    (void)method;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_helper(reg, helper) : NULL;
    if (!m && reg) m = module_registry_find_by_code_addr(reg, helper & ~1u);
    mod_name(m, nm, sizeof(nm));
    t = m ? track_ensure(m->module_id, nm) : NULL;
    seq = push_event(ERP_HELPER_CALL, m ? m->module_id : 0, nm, 0, 0, helper, 0);
    if (t) {
        t->helper_seen = 1;
        if (!t->helper_seq) t->helper_seq = seq;
        push_event(ERP_MR_HELPER, t->module_id, t->module, 0, 0, helper, 0);
    }
}

void ext_er_rw_producer_on_er_rw_publish(uint64_t module_id, uint32_t base, uint32_t size,
                                         GwyErRwSource source) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    char nm[72];
    ErpTrack *t;
    uint32_t seq;
    if (!ext_er_rw_producer_enabled() || !module_id || !base) return;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    mod_name(m, nm, sizeof(nm));
    t = track_ensure(module_id, nm);
    seq = push_event(ERP_ER_RW_PUBLISH, module_id, nm, base, size, 0, 0);
    if (!t) return;
    t->er_rw_base = base;
    t->er_rw_size = size;
    if (!t->er_rw_publish_seq) t->er_rw_publish_seq = seq;
    if (!t->producer_kind[0] || strcmp(t->producer_kind, "UNKNOWN") == 0)
        snprintf(t->producer_kind, sizeof(t->producer_kind), "%s", "PUBLISH");
    printf("[ER_RW_TIMELINE] seq=%u module=%s module_id=%llu event=ER_RW_PUBLISH_SOURCE "
           "source=%s base=0x%X size=%u er_rw_metadata_timing_gate=blocked evidence=OBSERVED\n",
           t->er_rw_publish_seq, nm[0] ? nm : "?", (unsigned long long)module_id,
           er_rw_source_name(source), base, size);
    fflush(stdout);
    correlate_birth(t);
    maybe_emit_identity(t);
    maybe_emit_class(t);
}

void ext_er_rw_producer_on_r9_switch(void *uc, uint64_t caller_module_id, uint64_t callee_module_id,
                                     GwyModuleCallKind call_kind, int enter_rc) {
    ModuleRegistry *reg;
    const GwyLoadedModule *callee;
    const GwyLoadedModule *caller;
    char nm[72];
    char cnm[72];
    ErpTrack *t;
    uint32_t effective_r9 = 0;
    uint32_t caller_r9 = 0;
    uint32_t seq;
    if (!ext_er_rw_producer_enabled() || !callee_module_id) return;
    reg = gwy_ext_loader_bound_registry();
    callee = reg ? module_registry_find_by_id(reg, callee_module_id) : NULL;
    caller = reg ? module_registry_find_by_id(reg, caller_module_id) : NULL;
    mod_name(callee, nm, sizeof(nm));
    mod_name(caller, cnm, sizeof(cnm));
    t = track_ensure(callee_module_id, nm);
    if (uc) guest_memory_uc_read_r9((struct uc_struct *)uc, &effective_r9);
    caller_r9 = effective_r9;

    if (call_kind == GWY_CALL_BOOTSTRAP_ENTRY || call_kind == GWY_CALL_RUNTIME_ENTRY) {
        if (!g_erp.dsm_entry_select_logged && caller && name_is_dsm(caller)) {
            g_erp.dsm_entry_select_logged = 1;
            push_event(ERP_DSM_FIRST_MODULE_ENTRY_SELECT, callee_module_id, nm, 0, 0, 0, 0);
        }
    }

    if (enter_rc < 0) {
        seq = push_event(ERP_R9_SWITCH_BLOCKED, callee_module_id, nm, 0, 0, 0, 0);
        if (t && (call_kind == GWY_CALL_BOOTSTRAP_ENTRY || call_kind == GWY_CALL_RUNTIME_ENTRY)) {
            if (!t->first_entry_seq) {
                t->first_entry_seq = seq;
                t->first_entry_r9 = effective_r9;
                push_event(ERP_DSM_FIRST_MODULE_ENTRY_CALL, callee_module_id, nm, 0, 0, 0, 0);
            }
            t->blocked_seq = seq;
            t->blocked_effective_r9 = effective_r9;
            t->caller_r9_at_block = caller_r9;
            emit_pre_entry_meta(t, callee, effective_r9);
            /* Switch fail-closed: GCO continues; PC already in callee → entry executed. */
            if (!t->blocked_outcome_emitted) {
                t->blocked_outcome_emitted = 1;
                t->entry_after_block = 1;
                printf("[R9_SWITCH_BLOCKED_OUTCOME] "
                       "outcome=SWITCH_BLOCKED_ENTRY_EXECUTED_WITH_CALLER_R9 "
                       "entry_deferred=no entry_executed=yes effective_r9=0x%X caller_r9=0x%X "
                       "callee_er_rw=0x%X module=%s module_id=%llu call_kind=%s "
                       "seq=%u evidence=OBSERVED note=switch_fail_closed_not_entry_fail_closed "
                       "er_rw_metadata_timing_gate=blocked module_r9_switch_gate=open "
                       "bootstrap_entry_r9_gate=blocked\n",
                       effective_r9, caller_r9, callee ? callee->data.start_of_er_rw : 0,
                       nm[0] ? nm : "?", (unsigned long long)callee_module_id,
                       gwy_module_call_kind_name(call_kind), seq);
                fflush(stdout);
            }
            maybe_emit_identity(t);
            maybe_emit_class(t);
        }
        return;
    }

    if (enter_rc > 0) {
        push_event(ERP_R9_SWITCH_ENTER, callee_module_id, nm, effective_r9,
                   callee ? callee->data.er_rw_size : 0, 0, 0);
        if (t && (call_kind == GWY_CALL_BOOTSTRAP_ENTRY || call_kind == GWY_CALL_RUNTIME_ENTRY) &&
            !t->first_entry_seq) {
            t->first_entry_seq = g_erp.seq;
            t->first_entry_r9 = effective_r9;
            push_event(ERP_DSM_FIRST_MODULE_ENTRY_CALL, callee_module_id, nm, 0, 0, 0, 0);
            emit_pre_entry_meta(t, callee, effective_r9);
        }
    }
}

void ext_er_rw_producer_on_code(void *uc, uint64_t module_id, uint32_t pc, const uint32_t regs[16],
                                uint32_t cpsr) {
    ErpTrack *t;
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    char nm[72];
    if (!ext_er_rw_producer_enabled() || !module_id) return;
    (void)uc;
    (void)cpsr;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_id(reg, module_id) : NULL;
    if (!m || !name_is_nested_focus(m)) return;
    mod_name(m, nm, sizeof(nm));
    t = track_ensure(module_id, nm);
    if (!t) return;
    if (!t->code_base && m->map.guest_code_base) {
        t->code_base = m->map.guest_code_base;
        t->code_size = m->map.guest_code_size;
    }
    if (t->blocked_seq && !t->first_entry_pc) {
        t->first_entry_pc = pc;
        t->entry_after_block = 1;
    }
    /* Heuristic: first nested PC after map with r0==0 often is CODE_IMAGE entry. */
    if (!t->first_entry_seq && regs && regs[0] == 0) {
        uint32_t seq = push_event(ERP_DSM_FIRST_MODULE_ENTRY_CALL, module_id, nm, 0, 0, 0, 0);
        t->first_entry_seq = seq;
        t->first_entry_r9 = regs[9];
        t->first_entry_pc = pc;
        emit_pre_entry_meta(t, m, regs[9]);
        maybe_emit_identity(t);
    }
}

void ext_er_rw_producer_on_mem_fault(uint32_t fault_pc, uint32_t fault_addr, uint32_t r0,
                                     uint32_t r9) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m;
    ErpTrack *t = NULL;
    char nm[72];
    const char *path = "UNKNOWN";
    uint32_t off = 0;
    uint32_t depth = 0;
    const char *stage = "UNKNOWN";
    if (!ext_er_rw_producer_enabled()) return;
    (void)r0;
    reg = gwy_ext_loader_bound_registry();
    m = reg ? module_registry_find_by_code_addr(reg, fault_pc & ~1u) : NULL;
    if (m) {
        mod_name(m, nm, sizeof(nm));
        t = track_ensure(m->module_id, nm);
        if (m->map.guest_code_base && fault_pc >= m->map.guest_code_base)
            off = (fault_pc & ~1u) - m->map.guest_code_base;
    } else {
        snprintf(nm, sizeof(nm), "%s", "?");
    }
    push_event(ERP_ENTRY_FAULT, m ? m->module_id : 0, nm, fault_addr, 0, 0, off);

    if (t) {
        if (t->entry_identity == ER_RW_ENTRY_BOOTSTRAP ||
            (t->first_entry_seq && (!t->er_rw_publish_seq || t->first_entry_seq < t->er_rw_publish_seq)))
            path = "BOOTSTRAP_CODE";
        else if (t->helper_seen)
            path = "HELPER_CODE";
        else if (t->er_rw_publish_seq && t->first_entry_seq &&
                 t->first_entry_seq > t->er_rw_publish_seq)
            path = "MODULE_RUNTIME_CODE";
        stage = t->er_rw_publish_seq ? "POST_ER_RW" : "PRE_ER_RW";
        maybe_emit_identity(t);
        maybe_emit_class(t);
    }
    /* DSM PC fault after nested CODE_IMAGE entry blocked → still bootstrap path. */
    if ((!t || path[0] == 'U') && g_erp.focus_mid) {
        ErpTrack *ft = track_find(g_erp.focus_mid);
        if (ft && ft->blocked_seq && ft->entry_after_block &&
            (!ft->er_rw_publish_seq || ft->first_entry_seq < ft->er_rw_publish_seq)) {
            path = "BOOTSTRAP_CODE";
            stage = "PRE_ER_RW";
        }
    }
    depth = module_r9_switch_depth();
    printf("[ER_RW_FAULT_PATH] path=%s module=%s module_id=%llu fault_pc=0x%X "
           "module_offset=0x%X fault_addr=0x%X r9=0x%X depth=%u lifecycle=%s "
           "evidence=OBSERVED note=not_progress_until_correct_r9 "
           "er_rw_metadata_timing_gate=blocked\n",
           path, nm[0] ? nm : "?", (unsigned long long)(m ? m->module_id : 0), fault_pc, off,
           fault_addr, r9, depth, stage);
    fflush(stdout);
}
