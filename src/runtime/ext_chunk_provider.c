#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_er_rw_bind_restore.h"
#include "gwy_launcher/ext_gwy_shell_native_exec.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_runtime_stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Provided by gwy_ext_obs (bridge-registered allocator). */
extern uint32_t gwy_ext_obs_guest_malloc0_ex(uint32_t size, void **out_host);
extern uint32_t gwy_ext_obs_c_function_p_slot_va(uint32_t helper, uint32_t lr);
extern void gwy_ext_obs_note_c_function_p_slot(uint32_t p_guest, uint32_t helper, uint32_t slot,
                                               uint32_t lr, int wrote);

typedef struct {
    uint32_t helper;
    uint64_t module_id;
    char name[48];
    char package_name[128];
    uint32_t code_base;
    uint32_t code_size;
    uint32_t p_guest;
    void *p_host;
    uint32_t chunk_guest;
    void *chunk_host;
    uint32_t init_func;
    int published;
    uint32_t module_generation;
    uint32_t p_generation;
} ChunkRec;

static struct {
    int mode_known;
    GwyExtChunkProviderMode mode;
    int slot_trace;
    void *uc;
    uint32_t sendappevent_guest;
    uint32_t mr_table_guest;
    int finalized;
    int any_published;
    uint32_t last_chunk_guest;
    char last_module[48];
    ChunkRec recs[8];
    int rec_n;
    int contract_emitted;
    uint32_t global_module_generation;
    uint32_t global_p_generation;
    uint32_t last_published_p;
    void *last_published_p_host;
    uint32_t parent_p_sha_before;
    uint32_t parent_p_sha_after;
} g_prov;

static int env_is_1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static void parse_mode(void) {
    const char *e;
    if (g_prov.mode_known) return;
    g_prov.mode = GWY_EXTCHUNK_OFF;
    e = getenv("JJFB_EXTCHUNK_PROVIDER");
    if (e && e[0]) {
        if (strcmp(e, "gbrwcore_only") == 0) g_prov.mode = GWY_EXTCHUNK_GBRWCORE_ONLY;
        else if (strcmp(e, "gbrwcore_wxjwq") == 0) g_prov.mode = GWY_EXTCHUNK_GBRWCORE_WXJWQ;
        else if (strcmp(e, "gwy_shell") == 0 || strcmp(e, "shell_core") == 0)
            g_prov.mode = GWY_EXTCHUNK_GWY_SHELL;
        else if (strcmp(e, "shell_and_game") == 0)
            g_prov.mode = GWY_EXTCHUNK_SHELL_AND_GAME;
        else if (strcmp(e, "game_package") == 0)
            g_prov.mode = GWY_EXTCHUNK_GAME_PACKAGE;
        else if (e[0] == '1') g_prov.mode = GWY_EXTCHUNK_GBRWCORE_ONLY;
    }
    g_prov.slot_trace = env_is_1("JJFB_EXTCHUNK_SLOT_TRACE") || (g_prov.mode != GWY_EXTCHUNK_OFF);
    g_prov.mode_known = 1;
}

const char *ext_chunk_provider_mode_name(GwyExtChunkProviderMode m) {
    switch (m) {
    case GWY_EXTCHUNK_GBRWCORE_ONLY: return "gbrwcore_only";
    case GWY_EXTCHUNK_GBRWCORE_WXJWQ: return "gbrwcore_wxjwq";
    case GWY_EXTCHUNK_GWY_SHELL: return "gwy_shell";
    case GWY_EXTCHUNK_SHELL_AND_GAME: return "shell_and_game";
    case GWY_EXTCHUNK_GAME_PACKAGE: return "game_package";
    default: return "off";
    }
}

void ext_chunk_provider_reset(void) {
    void *uc = g_prov.uc;
    uint32_t sa = g_prov.sendappevent_guest;
    uint32_t mt = g_prov.mr_table_guest;
    memset(&g_prov, 0, sizeof(g_prov));
    g_prov.uc = uc;
    g_prov.sendappevent_guest = sa;
    g_prov.mr_table_guest = mt;
}

void ext_chunk_provider_bind_uc(void *uc) { g_prov.uc = uc; }

void ext_chunk_provider_set_sendappevent_guest(uint32_t guest_addr) {
    g_prov.sendappevent_guest = guest_addr;
}

void ext_chunk_provider_set_mr_table_guest(uint32_t guest_addr) {
    g_prov.mr_table_guest = guest_addr;
}

uint32_t ext_chunk_provider_mr_table_guest(void) { return g_prov.mr_table_guest; }

int ext_chunk_provider_enabled(void) {
    parse_mode();
    return g_prov.mode != GWY_EXTCHUNK_OFF;
}

GwyExtChunkProviderMode ext_chunk_provider_mode(void) {
    parse_mode();
    return g_prov.mode;
}

int ext_chunk_provider_published(void) { return g_prov.any_published; }
uint32_t ext_chunk_provider_last_chunk_guest(void) { return g_prov.last_chunk_guest; }

static int name_has(const char *s, const char *n) {
    return s && n && strstr(s, n) != NULL;
}

static int mode_applies_name(const char *name) {
    parse_mode();
    if (g_prov.mode == GWY_EXTCHUNK_OFF) return 0;
    if (g_prov.mode == GWY_EXTCHUNK_GBRWCORE_ONLY || g_prov.mode == GWY_EXTCHUNK_GBRWCORE_WXJWQ)
        return name_has(name, "gbrwcore");
    if (g_prov.mode == GWY_EXTCHUNK_GWY_SHELL)
        return name_has(name, "gbrwcore") || name_has(name, "gamelist") || name_has(name, "gbrwshell");
    if (g_prov.mode == GWY_EXTCHUNK_SHELL_AND_GAME)
        return name_has(name, "gbrwcore") || name_has(name, "gamelist") ||
               name_has(name, "gbrwshell") || name_has(name, "mrc_loader") ||
               name_has(name, "robotol") || name_has(name, "mmochat");
    if (g_prov.mode == GWY_EXTCHUNK_GAME_PACKAGE)
        return name_has(name, "mrc_loader") || name_has(name, "robotol") ||
               name_has(name, "mmochat");
    return 0;
}

static int helper_in_gbrwcore(uint32_t helper) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    const GwyLoadedModule *gm;
    if (!reg || !helper) return 0;
    gm = module_registry_find_by_helper(reg, helper);
    if (!gm) gm = module_registry_find_by_code_addr(reg, helper);
    if (!gm) return 0;
    return name_has(gm->requested_name, "gbrwcore") || name_has(gm->resolved_name, "gbrwcore");
}

static int mode_applies_helper(uint32_t helper) {
    parse_mode();
    if (g_prov.mode == GWY_EXTCHUNK_OFF) return 0;
    if (g_prov.mode == GWY_EXTCHUNK_GBRWCORE_ONLY || g_prov.mode == GWY_EXTCHUNK_GBRWCORE_WXJWQ)
        return helper_in_gbrwcore(helper);
    if (g_prov.mode == GWY_EXTCHUNK_GWY_SHELL || g_prov.mode == GWY_EXTCHUNK_SHELL_AND_GAME ||
        g_prov.mode == GWY_EXTCHUNK_GAME_PACKAGE) {
        ModuleRegistry *reg = gwy_ext_loader_bound_registry();
        const GwyLoadedModule *gm;
        if (!reg) return 0;
        gm = module_registry_find_by_helper(reg, helper);
        if (!gm) gm = module_registry_find_by_code_addr(reg, helper);
        if (!gm) return 0;
        return mode_applies_name(gm->resolved_name[0] ? gm->resolved_name : gm->requested_name);
    }
    return 0;
}

int ext_chunk_provider_want(uint32_t helper) {
    parse_mode();
    if (g_prov.mode == GWY_EXTCHUNK_OFF || !helper) return 0;
    return mode_applies_helper(helper);
}

static void emit_contract_once(void) {
    if (g_prov.contract_emitted) return;
    g_prov.contract_emitted = 1;
    printf("[JJFB_EXTCHUNK_CONTRACT] struct=mrc_extChunk_st check_off=0x0 init_func_off=0x4 "
           "event_off=0x8 sendAppEvent_off=0x28 check_magic=0x%X size=0x%X "
           "source=mr_helper.h+doc/ext_important evidence=DOCUMENTED\n",
           GWY_MRC_EXTCHUNK_CHECK, (unsigned)GWY_MRC_EXTCHUNK_BYTES);
    fflush(stdout);
}

static uint32_t fingerprint_p_words(const void *p_host, uint32_t nbytes) {
    const uint8_t *b = (const uint8_t *)p_host;
    uint32_t h = 2166136261u;
    uint32_t i;
    if (!b || !nbytes) return 0;
    for (i = 0; i < nbytes; i++) {
        h ^= b[i];
        h *= 16777619u;
    }
    return h;
}

static int count_by_p(uint32_t p_guest) {
    int i, n = 0;
    if (!p_guest) return 0;
    for (i = 0; i < g_prov.rec_n; i++) {
        if (g_prov.recs[i].p_guest == p_guest) n++;
    }
    return n;
}

/*
 * Diagnostic-only: prefer latest ChunkRec for a P VA.
 * Execution paths must use helper/chunk/generation keys — not this.
 */
static ChunkRec *find_by_p_latest(uint32_t p_guest) {
    int i;
    ChunkRec *best = NULL;
    if (!p_guest) return NULL;
    for (i = 0; i < g_prov.rec_n; i++) {
        if (g_prov.recs[i].p_guest == p_guest) best = &g_prov.recs[i];
    }
    return best;
}

/* Unique-P only. Returns NULL if 0 or >1 owners (ambiguous). */
static ChunkRec *find_by_p_unique(uint32_t p_guest) {
    int n = count_by_p(p_guest);
    if (n == 1) return find_by_p_latest(p_guest);
    if (n > 1) {
        int i;
        printf("[P_LOOKUP_AMBIGUOUS] p=0x%X owners=%d note=refuse_latest_for_exec "
               "evidence=OBSERVED\n",
               p_guest, n);
        for (i = 0; i < g_prov.rec_n; i++) {
            if (g_prov.recs[i].p_guest != p_guest) continue;
            printf("[P_LOOKUP_CANDIDATE] p=0x%X helper=0x%X chunk=0x%X module=%s gen=%u "
                   "evidence=OBSERVED\n",
                   p_guest, g_prov.recs[i].helper, g_prov.recs[i].chunk_guest,
                   g_prov.recs[i].name[0] ? g_prov.recs[i].name : "?",
                   g_prov.recs[i].module_generation);
        }
        fflush(stdout);
    }
    return NULL;
}

/* Collision probe only — latest is OK to detect "someone already owns this P". */
static ChunkRec *find_by_p(uint32_t p_guest) { return find_by_p_latest(p_guest); }

static ChunkRec *find_by_helper(uint32_t helper) {
    int i;
    if (!helper) return NULL;
    for (i = 0; i < g_prov.rec_n; i++) {
        if ((g_prov.recs[i].helper & ~1u) == (helper & ~1u)) return &g_prov.recs[i];
    }
    return NULL;
}

static ChunkRec *find_by_chunk(uint32_t chunk_guest);

static ChunkRec *add_rec(void) {
    if (g_prov.rec_n >= 8) return NULL;
    return &g_prov.recs[g_prov.rec_n++];
}

static void fill_chunk(GwyMrcExtChunk *ch, uint32_t helper, uint32_t code_base, uint32_t code_size,
                       uint32_t p_guest, uint32_t p_len) {
    memset(ch, 0, sizeof(*ch));
    ch->check = GWY_MRC_EXTCHUNK_CHECK;
    ch->init_func = code_base ? (code_base + 8u) : 0;
    ch->event = helper;
    ch->code_buf = code_base;
    ch->code_len = code_size;
    ch->global_p_buf = p_guest;
    ch->global_p_len = p_len ? p_len : 20u;
    ch->send_app_event = g_prov.sendappevent_guest;
    ch->ext_mr_table = g_prov.mr_table_guest;
}

static void publish_to_p(void *p_host, uint32_t p_guest, uint32_t chunk_guest, const char *reason,
                         const char *module) {
    uint32_t old_v = 0;
    if (!p_host || !chunk_guest) return;
    /* Host view of mr_c_function_st — same MRP memory as guest. */
    {
        uint32_t *words = (uint32_t *)p_host;
        old_v = words[3]; /* +0x0C */
        words[3] = chunk_guest;
    }
    g_prov.any_published = 1;
    g_prov.last_chunk_guest = chunk_guest;
    if (module && module[0])
        snprintf(g_prov.last_module, sizeof(g_prov.last_module), "%.47s", module);
    printf("[JJFB_EXTCHUNK_PUBLISH] module=%s P=0x%X off=0x0C old=0x%X new=0x%X reason=%s "
           "evidence=DOCUMENTED\n",
           module ? module : "?", p_guest, old_v, chunk_guest, reason ? reason : "?");
    fflush(stdout);
}

static void emit_slots(const char *module, const GwyMrcExtChunk *ch) {
    printf("[JJFB_EXTCHUNK_SLOT] module=%s off=0x04 value=0x%X meaning=init_func "
           "evidence=DOCUMENTED\n",
           module ? module : "?", ch->init_func);
    printf("[JJFB_EXTCHUNK_SLOT] module=%s off=0x08 value=0x%X meaning=event_helper "
           "evidence=DOCUMENTED\n",
           module ? module : "?", ch->event);
    printf("[JJFB_EXTCHUNK_SLOT] module=%s off=0x28 value=0x%X meaning=sendAppEvent "
           "evidence=DOCUMENTED\n",
           module ? module : "?", ch->send_app_event);
    fflush(stdout);
}

static int ensure_and_publish(void *uc, uint32_t helper, uint32_t p_guest, void *p_host,
                              void *chunk_host, uint32_t chunk_guest, const char *reason) {
    ModuleRegistry *reg;
    const GwyLoadedModule *gm;
    ChunkRec *rec;
    GwyMrcExtChunk *ch;
    uint32_t code_base = 0, code_size = 0;
    uint64_t mid = 0;
    const char *mn = "gbrwcore.ext";
    LauncherError err;
    (void)uc;

    parse_mode();
    if (!ext_chunk_provider_enabled()) return 0;
    if (!mode_applies_helper(helper) && !helper_in_gbrwcore(helper)) return 0;
    if (!p_guest || !p_host || !chunk_host || !chunk_guest) return 0;

    emit_contract_once();

    reg = gwy_ext_loader_bound_registry();
    gm = reg ? module_registry_find_by_helper(reg, helper) : NULL;
    if (!gm && reg) gm = module_registry_find_by_code_addr(reg, helper);
    if (gm) {
        mid = gm->module_id;
        code_base = gm->map.guest_code_base;
        code_size = gm->map.guest_code_size;
        mn = gm->resolved_name[0] ? gm->resolved_name : gm->requested_name;
        if (!mode_applies_name(mn)) return 0;
    } else if (!helper_in_gbrwcore(helper) && g_prov.mode != GWY_EXTCHUNK_GWY_SHELL) {
        /* Allow gbrwcore helper before registry bind if PC in typical shell range — skip if unknown. */
        if (!mode_applies_helper(helper)) return 0;
    }

    rec = find_by_helper(helper);
    if (!rec) {
        ChunkRec *by_p = find_by_p(p_guest);
        if (by_p && by_p->helper && by_p->helper != helper) {
            /*
             * P21: guest _mr_c_function_new often reuses the same P VA without a
             * host-visible mr_free. Snapshot parent P to a fresh VA, retarget the
             * parent ChunkRec + CFN slot, then let the child keep the requested VA.
             */
            uint32_t parent_p = by_p->p_guest;
            void *parent_host = by_p->p_host;
            void *snap_host = NULL;
            uint32_t snap_p = 0;
            uint32_t sha_before = fingerprint_p_words(parent_host, 20u);
            g_prov.parent_p_sha_before = sha_before;
            printf("[JJFB_E10A31B] milestone=GAMELIST_P_COLLISION_ISOLATE_NEW_P parent_p=0x%X "
                   "old_helper=0x%X new_helper=0x%X old_module=%s parent_sha=0x%X "
                   "note=snapshot_parent_keep_child_va evidence=OBSERVED\n",
                   parent_p, by_p->helper, helper, by_p->name[0] ? by_p->name : "?", sha_before);
            fflush(stdout);
            snap_p = gwy_ext_obs_guest_malloc0_ex(20u, &snap_host);
            if (snap_p && snap_host && parent_host) {
                uint32_t slot = 0;
                memcpy(snap_host, parent_host, 20u);
                by_p->p_guest = snap_p;
                by_p->p_host = snap_host;
                printf("[JJFB_RUNTIME_FRAME] isolate_p parent_p=0x%X parent_snap=0x%X "
                       "child_p=0x%X old_helper=0x%X new_helper=0x%X evidence=OBSERVED\n",
                       parent_p, snap_p, p_guest, by_p->helper, helper);
                fflush(stdout);
                if (uc) {
                    /* Parent module CFN slot → snapshot (preserve parent ER_RW words). */
                    slot = gwy_ext_obs_c_function_p_slot_va(by_p->helper, 0);
                    if (slot &&
                        guest_memory_uc_poke_u32((struct uc_struct *)uc, slot, snap_p)) {
                        gwy_ext_obs_note_c_function_p_slot(snap_p, by_p->helper, slot, 0, 1);
                    }
                    /* Child keeps requested VA; clear so child publish starts clean. */
                    memset(parent_host, 0, 20u);
                }
                g_prov.parent_p_sha_after = fingerprint_p_words(snap_host, 20u);
                printf("[PARENT_P_SHA_BEFORE_CHILD] p=0x%X sha=0x%X evidence=OBSERVED\n", snap_p,
                       g_prov.parent_p_sha_before);
                printf("[PARENT_P_SHA_AFTER_CHILD] p=0x%X sha=0x%X match=%s evidence=OBSERVED\n",
                       snap_p, g_prov.parent_p_sha_after,
                       g_prov.parent_p_sha_before == g_prov.parent_p_sha_after ? "yes" : "no");
                fflush(stdout);
            } else {
                printf("[JJFB_RUNTIME_FRAME] isolate_p_failed parent_p=0x%X note=alloc_failed "
                       "evidence=OBSERVED\n",
                       parent_p);
                fflush(stdout);
            }
            rec = add_rec();
            if (!rec) return 0;
            memset(rec, 0, sizeof(*rec));
        } else {
            rec = by_p;
        }
    }
    if (!rec) {
        rec = add_rec();
        if (!rec) return 0;
        memset(rec, 0, sizeof(*rec));
    }
    {
        uint64_t old_mid = rec->module_id;
        uint32_t old_p = rec->p_guest;
        uint32_t old_helper = rec->helper;
        int fresh = (old_mid == 0);
        rec->helper = helper;
        if (mid && old_mid && old_mid != mid) {
            printf("[JJFB_E10A31B] milestone=GAMELIST_CHUNK_RETARGETED chunk=0x%X P=0x%X "
                   "old_module_id=%llu new_module_id=%llu old_helper=0x%X new_helper=0x%X "
                   "module=%s note=reuse_prior_package_chunk evidence=OBSERVED\n",
                   chunk_guest, p_guest, (unsigned long long)old_mid, (unsigned long long)mid,
                   old_helper, helper, mn);
            fflush(stdout);
            rec->module_generation = ++g_prov.global_module_generation;
        } else if (mid && old_mid != mid) {
            rec->module_generation = ++g_prov.global_module_generation;
        } else if (mid && !rec->module_generation)
            rec->module_generation = ++g_prov.global_module_generation;
        rec->module_id = mid;
        if (p_guest && old_p != p_guest) rec->p_generation = ++g_prov.global_p_generation;
        else if (p_guest && !rec->p_generation)
            rec->p_generation = ++g_prov.global_p_generation;

        if (fresh || (reason && strstr(reason, "mr_c_function_new"))) {
            printf("[JJFB_E10A31B] milestone=%s chunk=0x%X P=0x%X helper=0x%X module=%s "
                   "module_id=%llu reason=%s evidence=OBSERVED\n",
                   fresh ? "GAMELIST_CHUNK_CREATED" : "GAMELIST_CHUNK_REGISTERED", chunk_guest,
                   p_guest, helper, mn, (unsigned long long)mid, reason ? reason : "?");
            fflush(stdout);
        }
    }
    snprintf(rec->name, sizeof(rec->name), "%.47s", mn);
    rec->code_base = code_base;
    rec->code_size = code_size;
    rec->p_guest = p_guest;
    rec->p_host = p_host;
    rec->chunk_guest = chunk_guest;
    rec->chunk_host = chunk_host;

    ch = (GwyMrcExtChunk *)chunk_host;
    fill_chunk(ch, helper, code_base, code_size, p_guest, 20u);
    rec->init_func = ch->init_func;

    printf("[JJFB_EXTCHUNK_ALLOC] module=%s module_id=%llu guest=0x%X size=0x%X helper=0x%X "
           "evidence=DOCUMENTED\n",
           mn, (unsigned long long)mid, chunk_guest, (unsigned)GWY_MRC_EXTCHUNK_BYTES, helper);
    emit_slots(mn, ch);
    publish_to_p(p_host, p_guest, chunk_guest, reason, mn);
    rec->published = 1;
    g_prov.last_published_p = p_guest;
    g_prov.last_published_p_host = p_host;
    g_prov.last_chunk_guest = chunk_guest;
    snprintf(g_prov.last_module, sizeof(g_prov.last_module), "%.47s", mn);

    /* E10A-3.1b: after isolating a new gamelist ChunkRec, host-publish own ERW. */
    if (mn && strstr(mn, "gamelist")) {
        (void)ext_er_rw_bind_restore_ensure_isolated_erw(p_guest, mn);
    }

    {
        MrpRuntimeStack *st = mrp_runtime_stack_global();
        if (st && mrp_runtime_stack_top(st)) {
            (void)mrp_runtime_stack_bind_top(st, mid, helper, p_guest, chunk_guest);
            (void)mrp_runtime_stack_update_top(st, 0, 0, 0, 0, 0, helper);
        }
    }

    if (reg && mid && ch->init_func) {
        launcher_error_clear(&err);
        module_registry_set_chunk_field_04(reg, mid, ch->init_func, &err);
    }
    return 1;
}

int ext_chunk_provider_try_reuse(void *uc, uint32_t helper, uint32_t p_guest, void *p_host) {
    ChunkRec *rec;
    parse_mode();
    if (!ext_chunk_provider_enabled() || !helper) return 0;
    rec = find_by_helper(helper);
    if (!rec) {
        rec = find_by_p(p_guest);
        /*
         * E10A-3.1b: never retarget another module's chunk onto a new helper via P VA
         * collision (shell continue reuses the same guest P). Force a fresh chunk.
         */
        if (rec && rec->helper && rec->helper != helper) {
            printf("[JJFB_E10A31B] milestone=GAMELIST_CHUNK_REUSE_REFUSED chunk=0x%X P=0x%X "
                   "old_helper=0x%X new_helper=0x%X old_module=%s note=cross_helper_p_hit "
                   "evidence=OBSERVED\n",
                   rec->chunk_guest, p_guest, rec->helper, helper,
                   rec->name[0] ? rec->name : "?");
            fflush(stdout);
            return 0;
        }
    }
    if (!rec || !rec->chunk_host || !rec->chunk_guest) return 0;
    if (p_guest) rec->p_guest = p_guest;
    if (p_host) rec->p_host = p_host;
    return ensure_and_publish(uc, helper, rec->p_guest, rec->p_host, rec->chunk_host,
                              rec->chunk_guest, "platform_publication_restore");
}

int ext_chunk_provider_on_c_function_new(void *uc, uint32_t helper, uint32_t p_guest, void *p_host,
                                         void *chunk_host, uint32_t chunk_guest) {
    return ensure_and_publish(uc, helper, p_guest, p_host, chunk_host, chunk_guest,
                              "mr_c_function_new_contract");
}

void ext_chunk_provider_on_module_registered(const char *module_name, uint64_t module_id,
                                             uint32_t helper, uint32_t code_base, uint32_t code_size,
                                             uint32_t p_guest, void *p_host) {
    ChunkRec *rec;
    parse_mode();
    if (!ext_chunk_provider_enabled()) return;
    if (!mode_applies_name(module_name)) return;
    rec = find_by_helper(helper);
    if (!rec) rec = find_by_p(p_guest);
    if (!rec || !rec->chunk_host || !rec->chunk_guest) return;
    if (code_base) {
        rec->code_base = code_base;
        rec->code_size = code_size;
    }
    if (p_guest) {
        rec->p_guest = p_guest;
        if (p_host) rec->p_host = p_host;
    }
    rec->module_id = module_id;
    if (module_name) {
        snprintf(rec->name, sizeof(rec->name), "%.47s", module_name);
    }
    rec->module_generation = ++g_prov.global_module_generation;
    ensure_and_publish(g_prov.uc, helper, rec->p_guest, rec->p_host, rec->chunk_host,
                       rec->chunk_guest, "ext_register_contract");
}

static void fill_owner_info(ChunkRec *rec, ExtChunkOwnerInfo *out) {
    GwyMrcExtChunk *ch;
    ModuleRegistry *reg;
    const GwyLoadedModule *gm;
    if (!rec || !out) return;
    memset(out, 0, sizeof(*out));
    out->helper = rec->helper;
    out->p_guest = rec->p_guest;
    out->chunk_guest = rec->chunk_guest;
    out->module_id = rec->module_id;
    out->module_generation = rec->module_generation;
    out->p_generation = rec->p_generation;
    snprintf(out->module_name, sizeof(out->module_name), "%.47s",
             rec->name[0] ? rec->name : "?");
    reg = gwy_ext_loader_bound_registry();
    gm = reg && rec->module_id ? module_registry_find_by_id(reg, rec->module_id) : NULL;
    if (gm && gm->package_path[0])
        snprintf(out->package_name, sizeof(out->package_name), "%.127s", gm->package_path);
    if (rec->chunk_host) {
        ch = (GwyMrcExtChunk *)rec->chunk_host;
        out->chunk_erw = ch->var_buf;
    }
    (void)ext_chunk_provider_peek_p_er_rw(rec->p_guest, &out->p_erw, NULL);
    out->erw = out->chunk_erw ? out->chunk_erw : out->p_erw;
    if (gm && gm->data.start_of_er_rw) out->registry_erw = gm->data.start_of_er_rw;
}

int ext_chunk_provider_owner_for_chunk(uint32_t chunk_guest, ExtChunkOwnerInfo *out) {
    ChunkRec *rec = find_by_chunk(chunk_guest);
    if (!rec || !out) return 0;
    fill_owner_info(rec, out);
    return 1;
}

int ext_chunk_provider_owner_for_helper(uint32_t helper, ExtChunkOwnerInfo *out) {
    ChunkRec *rec = find_by_helper(helper);
    if (!rec || !out) return 0;
    fill_owner_info(rec, out);
    return 1;
}

int ext_chunk_provider_owner_for_p(uint32_t p_guest, ExtChunkOwnerInfo *out) {
    /* Execution: refuse ambiguous P (multiple ChunkRecs). */
    ChunkRec *rec = find_by_p_unique(p_guest);
    if (!rec || !out) return 0;
    fill_owner_info(rec, out);
    return 1;
}

uint32_t ext_chunk_provider_last_published_p(void) { return g_prov.last_published_p; }

int ext_chunk_provider_is_tracked_p(uint32_t p_guest) {
    return p_guest && count_by_p(p_guest) > 0;
}

int ext_chunk_provider_parent_p_sha(uint32_t *out_before, uint32_t *out_after) {
    if (out_before) *out_before = g_prov.parent_p_sha_before;
    if (out_after) *out_after = g_prov.parent_p_sha_after;
    return g_prov.parent_p_sha_before != 0;
}

int ext_chunk_provider_owner_for_erw(uint32_t erw, ExtChunkOwnerInfo *out) {
    ModuleRegistry *reg = gwy_ext_loader_bound_registry();
    size_t i;
    if (!reg || !erw || !out) return 0;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].data.start_of_er_rw == erw) {
            ChunkRec *rec = find_by_helper(reg->modules[i].map.helper_address);
            memset(out, 0, sizeof(*out));
            out->erw = erw;
            out->registry_erw = erw;
            out->module_id = reg->modules[i].module_id;
            out->helper = reg->modules[i].map.helper_address;
            snprintf(out->module_name, sizeof(out->module_name), "%.47s",
                     reg->modules[i].resolved_name[0] ? reg->modules[i].resolved_name
                                                      : reg->modules[i].requested_name);
            snprintf(out->package_name, sizeof(out->package_name), "%.127s",
                     reg->modules[i].package_path);
            if (rec) {
                out->p_guest = rec->p_guest;
                out->chunk_guest = rec->chunk_guest;
                out->p_generation = rec->p_generation;
                out->module_generation = rec->module_generation;
            }
            return 1;
        }
    }
    return 0;
}

int ext_chunk_provider_rebind_chunk_erw(uint32_t chunk_guest, uint32_t new_erw, uint32_t p_guest) {
    ChunkRec *rec = find_by_chunk(chunk_guest);
    GwyMrcExtChunk *ch;
    uint32_t old_erw = 0;
    uint32_t var_len;
    uint32_t *words;
    if (!rec || !rec->chunk_host || !new_erw) return 0;
    ch = (GwyMrcExtChunk *)rec->chunk_host;
    old_erw = ch->var_buf;
    var_len = ch->var_len ? ch->var_len : 20u;
    ch->var_buf = new_erw;
    if (p_guest) {
        rec->p_guest = p_guest;
        ch->global_p_buf = p_guest;
    }
    /* Keep P+0/+4 coherent with the rebound ERW (not only chunk var_buf). */
    if (rec->p_host) {
        words = (uint32_t *)rec->p_host;
        if (!var_len && words[1]) var_len = words[1];
        words[0] = new_erw;
        if (var_len) words[1] = var_len;
    }
    (void)ext_chunk_provider_set_var_fields(rec->p_guest, new_erw, var_len ? var_len : 20u);
    g_prov.global_p_generation++;
    rec->p_generation = g_prov.global_p_generation;
    printf("[JJFB_E10A31_TIMER_REBIND] chunk=0x%X module=%s P=0x%X old_erw=0x%X new_erw=0x%X "
           "reason=helper_owner_mismatch evidence=TARGET_OBSERVED\n",
           chunk_guest, rec->name[0] ? rec->name : "?", rec->p_guest, old_erw, new_erw);
    fflush(stdout);
    return 1;
}

void ext_chunk_provider_on_pxc_cleared(uint32_t p_guest, uint32_t pc) {
    ChunkRec *rec;
    parse_mode();
    if (!ext_chunk_provider_enabled() || !p_guest) return;
    rec = find_by_p(p_guest);
    if (!rec || !rec->chunk_guest || !rec->p_host) return;
    printf("[JJFB_EXTCHUNK_REPUBLISH] P=0x%X cleared_at_pc=0x%X chunk=0x%X "
           "reason=platform_publication_restore evidence=TARGET_OBSERVED\n",
           p_guest, pc, rec->chunk_guest);
    fflush(stdout);
    ensure_and_publish(g_prov.uc, rec->helper, rec->p_guest, rec->p_host, rec->chunk_host,
                       rec->chunk_guest, "platform_publication_restore");
}

void ext_chunk_provider_after_entry_order(uint32_t helper) {
    ChunkRec *rec;
    uint32_t *words;
    parse_mode();
    if (!ext_chunk_provider_enabled() || !helper) return;
    rec = find_by_helper(helper);
    if (!rec || !rec->p_host || !rec->chunk_guest) return;
    words = (uint32_t *)rec->p_host;
    if (words[3] != 0) return;
    printf("[JJFB_EXTCHUNK_REPUBLISH] P=0x%X cleared_at_pc=0 chunk=0x%X "
           "reason=platform_publication_restore evidence=TARGET_OBSERVED\n",
           rec->p_guest, rec->chunk_guest);
    fflush(stdout);
    ensure_and_publish(g_prov.uc, rec->helper, rec->p_guest, rec->p_host, rec->chunk_host,
                       rec->chunk_guest, "platform_publication_restore");
}

void ext_chunk_provider_on_slot28_call(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2,
                                       uint32_t r3, uint32_t r4, uint32_t ret) {
    parse_mode();
    if (!g_prov.slot_trace && !ext_chunk_provider_enabled()) return;
    printf("[JJFB_EXTCHUNK_SLOT_CALL] off=0x28 pc=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X r4=0x%X "
           "ret=0x%X evidence=TARGET_OBSERVED\n",
           pc, r0, r1, r2, r3, r4, ret);
    fflush(stdout);
    ext_gwy_shell_native_exec_on_slot28(pc, r0, r1, r2, r3, ret);
}

void ext_chunk_provider_finalize(const char *stop_reason) {
    parse_mode();
    if (g_prov.finalized || g_prov.mode == GWY_EXTCHUNK_OFF) return;
    g_prov.finalized = 1;
    printf("[JJFB_6N_SUMMARY] mode=%s published=%s last_chunk=0x%X stop=%s "
           "evidence=TARGET_OBSERVED\n",
           ext_chunk_provider_mode_name(g_prov.mode), g_prov.any_published ? "yes" : "no",
           g_prov.last_chunk_guest, stop_reason ? stop_reason : "?");
    fflush(stdout);
}

uint32_t ext_chunk_provider_last_p_guest(void) {
    int i;
    for (i = g_prov.rec_n - 1; i >= 0; i--) {
        if (g_prov.recs[i].p_guest) return g_prov.recs[i].p_guest;
    }
    return 0;
}

const char *ext_chunk_provider_last_module(void) {
    int i;
    if (g_prov.last_module[0]) return g_prov.last_module;
    for (i = g_prov.rec_n - 1; i >= 0; i--) {
        if (g_prov.recs[i].name[0]) return g_prov.recs[i].name;
    }
    return "";
}

int ext_chunk_provider_lookup_p(uint32_t p_guest, void **out_p_host, uint32_t *out_helper,
                                uint32_t *out_chunk_guest) {
    ChunkRec *rec = find_by_p_unique(p_guest);
    if (!rec) return 0;
    if (out_p_host) *out_p_host = rec->p_host;
    if (out_helper) *out_helper = rec->helper;
    if (out_chunk_guest) *out_chunk_guest = rec->chunk_guest;
    return 1;
}

int ext_chunk_provider_set_var_fields(uint32_t p_guest, uint32_t var_buf, uint32_t var_len) {
    ChunkRec *rec = find_by_p_unique(p_guest);
    GwyMrcExtChunk *ch;
    if (!rec || !rec->chunk_host || !var_buf || !var_len) return 0;
    ch = (GwyMrcExtChunk *)rec->chunk_host;
    ch->var_buf = var_buf;
    ch->var_len = var_len;
    printf("[JJFB_EXTCHUNK_SLOT] module=%s off=0x14 value=0x%X meaning=var_buf "
           "evidence=DOCUMENTED\n",
           rec->name[0] ? rec->name : "?", var_buf);
    printf("[JJFB_EXTCHUNK_SLOT] module=%s off=0x18 value=0x%X meaning=var_len "
           "evidence=DOCUMENTED\n",
           rec->name[0] ? rec->name : "?", var_len);
    fflush(stdout);
    return 1;
}

int ext_chunk_provider_peek_p_er_rw(uint32_t p_guest, uint32_t *out_base, uint32_t *out_len) {
    ChunkRec *rec = find_by_p_unique(p_guest);
    uint32_t *words;
    if (!rec || !rec->p_host) return 0;
    words = (uint32_t *)rec->p_host;
    if (out_base) *out_base = words[0];
    if (out_len) *out_len = words[1];
    return 1;
}

static ChunkRec *find_by_chunk(uint32_t chunk_guest) {
    int i;
    if (!chunk_guest) return NULL;
    for (i = 0; i < g_prov.rec_n; i++) {
        if (g_prov.recs[i].chunk_guest == chunk_guest) return &g_prov.recs[i];
    }
    return NULL;
}

int ext_chunk_provider_lookup_chunk(uint32_t chunk_guest, void **out_chunk_host) {
    ChunkRec *rec = find_by_chunk(chunk_guest);
    if (!rec) return 0;
    if (out_chunk_host) *out_chunk_host = rec->chunk_host;
    return 1;
}

int ext_chunk_provider_set_timer_field(uint32_t chunk_guest, uint32_t timer_id) {
    ChunkRec *rec = find_by_chunk(chunk_guest);
    GwyMrcExtChunk *ch;
    if (!rec || !rec->chunk_host) return 0;
    ch = (GwyMrcExtChunk *)rec->chunk_host;
    ch->timer = timer_id;
    printf("[JJFB_EXTCHUNK_SLOT] module=%s off=0x24 value=0x%X meaning=timer "
           "evidence=DOCUMENTED\n",
           rec->name[0] ? rec->name : "?", timer_id);
    fflush(stdout);
    return 1;
}

int ext_chunk_provider_timer_dispatch_target(uint32_t chunk_guest, uint32_t *out_helper,
                                            uint32_t *out_p_guest, uint32_t *out_erw) {
    ChunkRec *rec = find_by_chunk(chunk_guest);
    GwyMrcExtChunk *ch;
    uint32_t erw = 0;
    if (!rec || !rec->chunk_host) return 0;
    ch = (GwyMrcExtChunk *)rec->chunk_host;
    if (!ch->event || !ch->global_p_buf) return 0;
    erw = ch->var_buf;
    if (!erw)
        (void)ext_chunk_provider_peek_p_er_rw(ch->global_p_buf, &erw, NULL);
    if (out_helper) *out_helper = ch->event;
    if (out_p_guest) *out_p_guest = ch->global_p_buf;
    if (out_erw) *out_erw = erw;
    return 1;
}
