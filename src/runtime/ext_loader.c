#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/ext_helper_handoff.h"
#include "gwy_launcher/ext_dsm_record_observe.h"
#include "gwy_launcher/ext_entry_null_contract.h"
#include "gwy_launcher/ext_module_data_init.h"
#include "gwy_launcher/guest_call_observer.h"
#include "gwy_launcher/package_scope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ExtLoadSession {
    int in_use;
    int pending_active;
    ExtLoadContext ctx;
    ExtLoader *owner;
};

struct ExtLoader {
    ModuleRegistry *reg;
    uint64_t next_load_id;
    ExtLoadSession sessions[GWY_EXT_LOAD_SESSION_MAX];
    /* Stash _mr_c_function_new before registry bind (DSM firmware load). */
    int early_valid;
    uint32_t early_helper;
    uint32_t early_p_len;
    uint32_t early_p_guest;
    uint32_t early_rw_base;
    uint32_t early_rw_size;
    uint32_t early_stack;
    /* Host DSM cfunction.ext image bounds from loadCode / code_image. */
    uint32_t dsm_code_base;
    uint32_t dsm_code_size;
    /* --- ext: @raw may arrive before cacheSync map; apply on next code_image. */
    uint32_t pending_ext_raw_base;
};

static ExtLoader g_default_loader;
static int g_default_inited = 0;
static ExtLoader *g_ext_loader = NULL;

ExtLoader *gwy_ext_loader_ensure(void) {
    if (!g_default_inited) {
        ext_loader_init(&g_default_loader);
        g_default_inited = 1;
        g_ext_loader = &g_default_loader;
    } else if (!g_ext_loader) {
        g_ext_loader = &g_default_loader;
    }
    return g_ext_loader;
}

ExtLoader *gwy_ext_loader_global(void) { return gwy_ext_loader_ensure(); }

void gwy_ext_loader_set_global(ExtLoader *loader) {
    g_ext_loader = loader ? loader : &g_default_loader;
}

ModuleRegistry *gwy_ext_loader_bound_registry(void) {
    ExtLoader *L = gwy_ext_loader_ensure();
    return L ? L->reg : NULL;
}

static void log_load(const ExtLoadContext *ctx, const char *stage, uint32_t size) {
    if (!ctx) return;
    printf("[EXT_LOAD] module_id=%llu load_id=%llu package=%s requested=%s resolved=%s "
           "origin=%s stage=%s size=%u\n",
           (unsigned long long)ctx->module_id, (unsigned long long)ctx->load_id,
           ctx->package_path, ctx->requested_name, ctx->resolved_name,
           gwy_module_origin_name(ctx->origin), stage ? stage : "?", (unsigned)size);
    fflush(stdout);
}

static const char *basename_of(const char *path) {
    const char *p;
    if (!path || !path[0]) return path;
    p = path;
    while (*path) {
        if (*path == '/' || *path == '\\') p = path + 1;
        path++;
    }
    return p;
}

static int name_eq_ci_suffix(const char *a, const char *b) {
    /* exact case-sensitive match on basename — guest names are usually lower. */
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
}

void ext_loader_init(ExtLoader *loader) {
    if (!loader) return;
    memset(loader, 0, sizeof(*loader));
    loader->next_load_id = 1;
}

void ext_loader_destroy_all(ExtLoader *loader) {
    size_t i;
    if (!loader) return;
    for (i = 0; i < GWY_EXT_LOAD_SESSION_MAX; i++) {
        if (loader->sessions[i].in_use) {
            log_load(&loader->sessions[i].ctx, "DESTROY", 0);
        }
        memset(&loader->sessions[i], 0, sizeof(loader->sessions[i]));
    }
    loader->reg = NULL;
    /* Preserve early DSM stash across VFS re-prepare (flush_early consumes it). */
}

void ext_loader_bind_registry(ExtLoader *loader, ModuleRegistry *reg) {
    if (!loader) return;
    loader->reg = reg;
}

static ExtLoadSession *alloc_session(ExtLoader *loader) {
    size_t i;
    for (i = 0; i < GWY_EXT_LOAD_SESSION_MAX; i++) {
        if (!loader->sessions[i].in_use) {
            memset(&loader->sessions[i], 0, sizeof(loader->sessions[i]));
            loader->sessions[i].in_use = 1;
            loader->sessions[i].owner = loader;
            return &loader->sessions[i];
        }
    }
    return NULL;
}

ExtLoadSession *ext_loader_find_session_by_module(ExtLoader *loader, uint64_t module_id) {
    size_t i;
    if (!loader) return NULL;
    for (i = 0; i < GWY_EXT_LOAD_SESSION_MAX; i++) {
        if (loader->sessions[i].in_use && loader->sessions[i].ctx.module_id == module_id) {
            return &loader->sessions[i];
        }
    }
    return NULL;
}

ExtLoadSession *ext_loader_find_by_code_addr(ExtLoader *loader, uint32_t guest_addr) {
    const GwyLoadedModule *m;
    if (!loader || !loader->reg) return NULL;
    m = module_registry_find_by_code_addr(loader->reg, guest_addr);
    if (!m) return NULL;
    return ext_loader_find_session_by_module(loader, m->module_id);
}

const ExtLoadContext *ext_loader_session_context(const ExtLoadSession *session) {
    return session && session->in_use ? &session->ctx : NULL;
}

LauncherStatus ext_loader_begin(ExtLoader *loader,
                                uint64_t module_id,
                                const uint8_t *image,
                                size_t image_size,
                                ExtLoadSession **out_session,
                                LauncherError *err) {
    ExtLoadSession *s;
    const GwyLoadedModule *m;
    launcher_error_clear(err);
    (void)image;
    if (!loader || !out_session) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "ext_loader", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out_session = NULL;
    if (!loader->reg) {
        launcher_error_set(err, L_ERR_STATE, "ext_loader", "registry not bound", NULL);
        return L_ERR_STATE;
    }
    m = module_registry_find_by_id(loader->reg, module_id);
    if (!m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "ext_loader", "module_id not found", NULL);
        return L_ERR_NOT_FOUND;
    }
    s = ext_loader_find_session_by_module(loader, module_id);
    if (!s) {
        s = alloc_session(loader);
        if (!s) {
            launcher_error_set(err, L_ERR_BOUNDS, "ext_loader", "session table full", NULL);
            return L_ERR_BOUNDS;
        }
        s->ctx.load_id = loader->next_load_id++;
        s->ctx.module_id = module_id;
        s->ctx.origin = m->origin;
        snprintf(s->ctx.package_path, sizeof(s->ctx.package_path), "%s", m->package_path);
        snprintf(s->ctx.requested_name, sizeof(s->ctx.requested_name), "%s", m->requested_name);
        snprintf(s->ctx.resolved_name, sizeof(s->ctx.resolved_name), "%s",
                 m->resolved_name[0] ? m->resolved_name : m->requested_name);
    }
    log_load(&s->ctx, gwy_module_state_name(m->state),
             image_size ? (uint32_t)image_size : m->extracted_size);
    *out_session = s;
    return L_OK;
}

void ext_loader_destroy(ExtLoadSession *session) {
    if (!session || !session->in_use) return;
    log_load(&session->ctx, "DESTROY", 0);
    memset(session, 0, sizeof(*session));
}

LauncherStatus ext_loader_map(ExtLoadSession *session,
                              const GwyModuleMapping *mapping,
                              LauncherError *err) {
    if (!session || !session->owner || !mapping) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "ext_loader", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    return module_registry_set_mapping(session->owner->reg, session->ctx.module_id, mapping, err);
}

LauncherStatus ext_loader_register(ExtLoadSession *session,
                                   uint32_t helper_address,
                                   uint32_t entry_address,
                                   LauncherError *err) {
    if (!session || !session->owner) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "ext_loader", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    return module_registry_set_registered(session->owner->reg, session->ctx.module_id,
                                          helper_address, entry_address, err);
}

LauncherStatus ext_loader_call_entry(ExtLoadSession *session,
                                     uint32_t method,
                                     uint32_t input,
                                     uint32_t input_len,
                                     int32_t ret_value,
                                     LauncherError *err) {
    (void)input;
    (void)input_len;
    if (!session || !session->owner) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "ext_loader", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    return module_registry_set_entry_called(session->owner->reg, session->ctx.module_id, method,
                                            ret_value, err);
}

LauncherStatus ext_loader_seed_from_registry(ExtLoader *loader, LauncherError *err) {
    size_t i;
    launcher_error_clear(err);
    if (!loader || !loader->reg) {
        launcher_error_set(err, L_ERR_STATE, "ext_loader", "registry not bound", NULL);
        return L_ERR_STATE;
    }
    for (i = 0; i < loader->reg->count; i++) {
        const GwyLoadedModule *m = &loader->reg->modules[i];
        ExtLoadSession *s;
        if (m->state < GWY_MODULE_EXTRACTED) continue;
        if (ext_loader_begin(loader, m->module_id, NULL, 0, &s, err) != L_OK) return err->code;
    }
    return L_OK;
}

static ExtLoadSession *active_pending(ExtLoader *loader) {
    size_t i;
    ExtLoadSession *found = NULL;
    for (i = 0; i < GWY_EXT_LOAD_SESSION_MAX; i++) {
        if (loader->sessions[i].in_use && loader->sessions[i].pending_active) {
            if (found) return NULL; /* ambiguous */
            found = &loader->sessions[i];
        }
    }
    return found;
}

static void activate_session(ExtLoadSession *s) {
    size_t i;
    if (!s || !s->owner) return;
    for (i = 0; i < GWY_EXT_LOAD_SESSION_MAX; i++) {
        s->owner->sessions[i].pending_active = 0;
    }
    s->pending_active = 1;
    log_load(&s->ctx, "PENDING", 0);
}

void ext_loader_on_member_open(ExtLoader *loader, const char *guest_path) {
    const char *base;
    size_t i;
    if (!loader || !loader->reg || !guest_path) return;
    base = basename_of(guest_path);
    if (!base || !base[0]) return;
    /* cfunction.ext under package scope → activate package primary session. */
    if (package_scope_enabled() &&
        (strcmp(base, "cfunction.ext") == 0 || strcmp(base, "Cfunction.ext") == 0)) {
        const char *primary = package_scope_active_primary();
        if (primary && primary[0]) {
            const GwyLoadedModule *pm = module_registry_find(loader->reg, primary);
            ExtLoadSession *s;
            LauncherError err;
            printf("[JJFB_CLOAD_SCOPE] request=%s package=%s resolved=%s "
                   "reason=package_reg_primary evidence=CROSS_TARGET\n",
                   base,
                   package_scope_active_package() ? package_scope_active_package() : "?",
                   primary);
            fflush(stdout);
            if (pm) {
                s = ext_loader_find_session_by_module(loader, pm->module_id);
                if (!s) ext_loader_begin(loader, pm->module_id, NULL, 0, &s, &err);
                if (s) {
                    activate_session(s);
                    printf("[EXT_LOAD] stage=MEMBER_OPEN_SCOPED requested=%s primary=%s "
                           "module_id=%llu evidence=CROSS_TARGET\n",
                           base, primary, (unsigned long long)pm->module_id);
                    fflush(stdout);
                    return;
                }
            }
        }
    }
    for (i = 0; i < loader->reg->count; i++) {
        const GwyLoadedModule *m = &loader->reg->modules[i];
        ExtLoadSession *s;
        if (m->origin != MODULE_ORIGIN_MRP_MEMBER) continue;
        if (m->state != GWY_MODULE_EXTRACTED && m->state != GWY_MODULE_MAPPED &&
            m->state != GWY_MODULE_REGISTERED) {
            continue;
        }
        if (!name_eq_ci_suffix(base, m->requested_name) &&
            !name_eq_ci_suffix(base, m->resolved_name)) {
            continue;
        }
        s = ext_loader_find_session_by_module(loader, m->module_id);
        if (!s) {
            LauncherError err;
            if (ext_loader_begin(loader, m->module_id, NULL, 0, &s, &err) != L_OK) return;
        }
        activate_session(s);
        return;
    }
}

void ext_loader_on_alloc(ExtLoader *loader, uint32_t guest_addr, uint32_t size) {
    size_t i;
    ExtLoadSession *match = NULL;
    int matches = 0;
    if (!loader || !loader->reg || size == 0) return;
    for (i = 0; i < loader->reg->count; i++) {
        const GwyLoadedModule *m = &loader->reg->modules[i];
        ExtLoadSession *s;
        if (m->origin != MODULE_ORIGIN_MRP_MEMBER) continue;
        if (m->state != GWY_MODULE_EXTRACTED) continue;
        if (m->extracted_size == 0 || m->extracted_size != size) continue;
        s = ext_loader_find_session_by_module(loader, m->module_id);
        if (!s) {
            LauncherError err;
            if (ext_loader_begin(loader, m->module_id, NULL, 0, &s, &err) != L_OK) continue;
        }
        matches++;
        match = s;
    }
    if (matches == 1 && match) {
        GwyModuleMapping map;
        LauncherError err;
        const GwyLoadedModule *m;
        activate_session(match);
        /* Provisional map from observed decompress buffer (not JJFB literals). */
        m = module_registry_find_by_id(loader->reg, match->ctx.module_id);
        if (m && m->state == GWY_MODULE_EXTRACTED && guest_addr != 0) {
            memset(&map, 0, sizeof(map));
            map.guest_code_base = guest_addr;
            map.guest_code_size = size;
            if (ext_loader_map(match, &map, &err) == L_OK) {
                printf("[EXT_LOAD] module_id=%llu load_id=%llu stage=MAPPED "
                       "code_base=0x%X code_size=%u reason=decompress_alloc\n",
                       (unsigned long long)match->ctx.module_id,
                       (unsigned long long)match->ctx.load_id, guest_addr, size);
                fflush(stdout);
                /* Observe-only: watch mapped region for nested helper registration. */
                (void)guest_call_observer_attach_module(gwy_guest_call_observer_ensure(),
                                                        match->ctx.module_id, &err);
            }
        }
    }
}

/* DOCUMENTED: mythroad case800 cacheSync pad = (len + 0x1F*3) & ~0x1F */
static uint32_t cache_sync_align(uint32_t len) {
    return (len + 0x1Fu * 3u) & ~0x1Fu;
}

static int size_matches_extracted(uint32_t observed, uint32_t extracted) {
    uint32_t aligned;
    if (extracted == 0 || observed == 0) return 0;
    if (observed == extracted) return 1;
    aligned = cache_sync_align(extracted);
    if (observed == aligned) return 1;
    if (observed > extracted && observed <= aligned) return 1;
    return 0;
}

static GwyLoadedModule *module_mut_id(ModuleRegistry *reg, uint64_t module_id) {
    size_t i;
    for (i = 0; i < reg->count; i++) {
        if (reg->modules[i].module_id == module_id) return &reg->modules[i];
    }
    return NULL;
}

static void apply_dsm_code_image(ExtLoader *loader, uint32_t guest_addr, uint32_t size) {
    const GwyLoadedModule *existing;
    GwyLoadedModule *mut;
    LauncherError err;

    loader->dsm_code_base = guest_addr;
    loader->dsm_code_size = size;
    printf("[EXT_LOAD] stage=DSM_CODE_IMAGE code_base=0x%X code_size=%u\n", guest_addr, size);
    fflush(stdout);

    if (!loader->reg) return;
    existing = module_registry_find(loader->reg, "dsm:cfunction.ext");
    if (!existing) return;
    mut = module_mut_id(loader->reg, existing->module_id);
    if (!mut) return;
    mut->map.guest_code_base = guest_addr;
    mut->map.guest_code_size = size;
    if (mut->state >= GWY_MODULE_MAPPED) {
        /* Bounds only — no full DSM CODE hook (see ensure_dsm_module). */
        (void)err;
    }
}

/* DOCUMENTED: cacheSync uses addr&~0x1F; real MRPG is within the 32-byte pad window. */
static int refine_mrp_code_base(ExtLoader *loader, GwyLoadedModule *mut, uint32_t raw_base) {
    uint32_t aligned;
    uint32_t pad;
    LauncherError err;
    const char *mn;

    if (!loader || !loader->reg || !mut || !raw_base) return 0;
    if (mut->origin != MODULE_ORIGIN_MRP_MEMBER) return 0;
    aligned = mut->map.guest_code_base;
    if (!aligned || raw_base == aligned) return 0;
    if (raw_base < aligned || raw_base - aligned >= 0x20u) return 0;
    pad = raw_base - aligned;
    if (mut->map.guest_code_size <= pad) return 0;

    mut->map.guest_code_base = raw_base;
    mut->map.guest_code_size -= pad;
    mut->entries.image_base = raw_base;
    mut->entries.header_entry_candidate = raw_base + 8u;
    mut->map.entry_address = mut->entries.header_entry_candidate;

    mn = mut->resolved_name[0] ? mut->resolved_name : mut->requested_name;
    printf("[EXT_LOAD] module_id=%llu stage=RAW_BASE_REFINE aligned=0x%X raw=0x%X pad=0x%X "
           "code_size=%u header_entry=0x%X evidence=DOCUMENTED "
           "source=mythroad.c:case800_input1_vs_cacheSync_align\n",
           (unsigned long long)mut->module_id, aligned, raw_base, pad, mut->map.guest_code_size,
           mut->entries.header_entry_candidate);
    fflush(stdout);

    launcher_error_clear(&err);
    (void)guest_call_observer_attach_module(gwy_guest_call_observer_ensure(), mut->module_id,
                                            &err);
    printf("[EXT_MAP] module_id=%llu code_base=0x%X code_size=%u rw_base=0x%X rw_size=%u "
           "stack_base=0x%X stack_size=%u header_entry_candidate=0x%X result=OK "
           "reason=raw_base_refine\n",
           (unsigned long long)mut->module_id, mut->map.guest_code_base, mut->map.guest_code_size,
           mut->map.guest_rw_base, mut->map.guest_rw_size, mut->map.guest_stack_base,
           mut->map.guest_stack_size, mut->entries.header_entry_candidate);
    fflush(stdout);
    (void)mn;
    return 1;
}

void ext_loader_on_ext_image_raw(ExtLoader *loader, uint32_t raw_base) {
    size_t i;
    int refined = 0;
    if (!loader || !raw_base) return;
    if (!loader->reg) {
        loader->pending_ext_raw_base = raw_base;
        printf("[EXT_LOAD] stage=RAW_BASE_PENDING raw=0x%X evidence=DOCUMENTED "
               "note=await_cache_sync_map\n",
               raw_base);
        fflush(stdout);
        return;
    }
    for (i = 0; i < loader->reg->count; i++) {
        GwyLoadedModule *mut = &loader->reg->modules[i];
        if (mut->origin != MODULE_ORIGIN_MRP_MEMBER) continue;
        if (mut->state < GWY_MODULE_MAPPED) continue;
        if (refine_mrp_code_base(loader, mut, raw_base)) {
            refined = 1;
            break;
        }
    }
    if (!refined) {
        loader->pending_ext_raw_base = raw_base;
        printf("[EXT_LOAD] stage=RAW_BASE_PENDING raw=0x%X evidence=DOCUMENTED "
               "note=no_aligned_map_in_pad_window\n",
               raw_base);
        fflush(stdout);
    } else {
        loader->pending_ext_raw_base = 0;
    }
}

void ext_loader_on_code_image(ExtLoader *loader, uint32_t guest_addr, uint32_t size) {
    size_t i;
    ExtLoadSession *match = NULL;
    int matches = 0;
    if (!loader || guest_addr == 0 || size == 0) return;

    /* Host DSM firmware (loadCode) — may arrive before registry bind. */
    if (!loader->reg) {
        apply_dsm_code_image(loader, guest_addr, size);
        return;
    }

    for (i = 0; i < loader->reg->count; i++) {
        const GwyLoadedModule *m = &loader->reg->modules[i];
        ExtLoadSession *s;
        if (m->origin != MODULE_ORIGIN_MRP_MEMBER) continue;
        if (m->state != GWY_MODULE_EXTRACTED) continue;
        if (!size_matches_extracted(size, m->extracted_size)) continue;
        s = ext_loader_find_session_by_module(loader, m->module_id);
        if (!s) {
            LauncherError err;
            if (ext_loader_begin(loader, m->module_id, NULL, 0, &s, &err) != L_OK) continue;
        }
        matches++;
        match = s;
    }
    if (matches == 1 && match) {
        GwyModuleMapping map;
        LauncherError err;
        GwyLoadedModule *mut;
        const GwyLoadedModule *m = module_registry_find_by_id(loader->reg, match->ctx.module_id);
        activate_session(match);
        if (!m || m->state != GWY_MODULE_EXTRACTED) return;
        memset(&map, 0, sizeof(map));
        map.guest_code_base = guest_addr;
        map.guest_code_size = size;
        if (ext_loader_map(match, &map, &err) == L_OK) {
            printf("[EXT_LOAD] module_id=%llu load_id=%llu stage=MAPPED "
                   "code_base=0x%X code_size=%u reason=cache_sync\n",
                   (unsigned long long)match->ctx.module_id,
                   (unsigned long long)match->ctx.load_id, guest_addr, size);
            fflush(stdout);
            (void)guest_call_observer_attach_module(gwy_guest_call_observer_ensure(),
                                                    match->ctx.module_id, &err);
            /* Apply --- ext: @raw if it arrived before this cacheSync map. */
            if (loader->pending_ext_raw_base) {
                uint32_t raw = loader->pending_ext_raw_base;
                loader->pending_ext_raw_base = 0;
                mut = module_mut_id(loader->reg, match->ctx.module_id);
                if (mut) (void)refine_mrp_code_base(loader, mut, raw);
            }
        }
        return;
    }

    /* Refresh DSM image only for the known firmware base (or first DSM record). */
    if (loader->dsm_code_base == 0 || guest_addr == loader->dsm_code_base) {
        apply_dsm_code_image(loader, guest_addr, size);
    }
    printf("[EXT_LOAD] stage=CODE_IMAGE code_base=0x%X code_size=%u matches=%d "
           "reason=cache_sync\n",
           guest_addr, size, matches);
    fflush(stdout);
}

/* DSM host firmware is always a distinct row from MRP member cfunction.ext→robotol. */
static LauncherStatus ensure_dsm_module(ExtLoader *loader,
                                        uint32_t helper,
                                        uint32_t rw_base,
                                        uint32_t rw_size,
                                        uint32_t stack_base,
                                        LauncherError *err) {
    const char *dsm_name = "dsm:cfunction.ext";
    const GwyLoadedModule *existing;
    GwyLoadedModule *mut;
    ExtLoadSession *s;
    GwyModuleMapping map;
    uint64_t mid;

    existing = module_registry_find(loader->reg, dsm_name);
    if (!existing) {
        if (module_registry_discover(loader->reg, dsm_name, err) != L_OK) return err->code;
        existing = module_registry_find(loader->reg, dsm_name);
        if (!existing) return L_ERR_NOT_FOUND;
    }
    mid = existing->module_id;
    mut = module_mut_id(loader->reg, mid);
    if (!mut) return L_ERR_NOT_FOUND;
    mut->origin = MODULE_ORIGIN_DSM;
    snprintf(mut->resolved_name, sizeof(mut->resolved_name), "%s", "cfunction.ext");
    if (mut->state < GWY_MODULE_EXTRACTED) {
        mut->state = GWY_MODULE_EXTRACTED;
    }

    if (ext_loader_begin(loader, mid, NULL, 0, &s, err) != L_OK) return err->code;
    s->ctx.origin = MODULE_ORIGIN_DSM;

    memset(&map, 0, sizeof(map));
    /* Prefer full DSM firmware image bounds from loadCode (not helper-only size=1). */
    if (loader->dsm_code_base && loader->dsm_code_size &&
        helper >= loader->dsm_code_base &&
        helper < loader->dsm_code_base + loader->dsm_code_size) {
        map.guest_code_base = loader->dsm_code_base;
        map.guest_code_size = loader->dsm_code_size;
    } else {
        map.guest_code_base = helper;
        map.guest_code_size = 1;
    }
    map.guest_rw_base = rw_base;
    map.guest_rw_size = rw_size;
    map.guest_stack_base = stack_base;
    map.helper_address = helper;
    map.entry_address = 0; /* registry derives header_entry_candidate = image+8 */

    existing = module_registry_find_by_id(loader->reg, mid);
    if (existing->state == GWY_MODULE_EXTRACTED) {
        if (module_registry_set_mapping(loader->reg, mid, &map, err) != L_OK) return err->code;
    } else if (existing->state >= GWY_MODULE_MAPPED && rw_base) {
        mut = module_mut_id(loader->reg, mid);
        if (mut) {
            if (loader->dsm_code_base && loader->dsm_code_size) {
                mut->map.guest_code_base = loader->dsm_code_base;
                mut->map.guest_code_size = loader->dsm_code_size;
                mut->entries.image_base = loader->dsm_code_base;
                if (mut->entries.header_entry_candidate == 0) {
                    mut->entries.header_entry_candidate = loader->dsm_code_base + 8u;
                    mut->entries.header_evidence = GWY_ADDR_EV_DOCUMENTED;
                    mut->map.entry_address = mut->entries.header_entry_candidate;
                }
            }
            mut->map.guest_rw_base = rw_base;
            mut->map.guest_rw_size = rw_size;
            mut->map.guest_stack_base = stack_base;
            mut->map.helper_address = helper;
            {
                LauncherError e2;
                module_registry_set_er_rw(loader->reg, mid, rw_base, rw_size,
                                          GWY_ER_RW_SRC_LOADER_DOCUMENTED, GWY_ADDR_EV_DOCUMENTED,
                                          &e2);
            }
        }
    }

    existing = module_registry_find_by_id(loader->reg, mid);
    if (existing && existing->state == GWY_MODULE_MAPPED) {
        if (module_registry_set_registered(loader->reg, mid, helper, helper, err) != L_OK) {
            return err->code;
        }
    }
    if (rw_base) {
        LauncherError e2;
        module_registry_set_er_rw(loader->reg, mid, rw_base, rw_size, GWY_ER_RW_SRC_LOADER_DOCUMENTED,
                                  GWY_ADDR_EV_DOCUMENTED, &e2);
    }
    /* Do not attach full-code GCO on DSM firmware (hundreds of KB) — too slow.
     * CALL_SITE is recovered from LR at nested ROBOTOL_ENTER (+ MRP module watches). */
    log_load(&s->ctx, gwy_module_state_name(module_registry_find_by_id(loader->reg, mid)->state),
             loader->dsm_code_size);
    return L_OK;
}

static ExtLoadSession *unique_unfinished_mrp(ExtLoader *loader) {
    size_t i;
    ExtLoadSession *found = NULL;
    int n = 0;
    for (i = 0; i < loader->reg->count; i++) {
        const GwyLoadedModule *m = &loader->reg->modules[i];
        ExtLoadSession *s;
        if (m->origin != MODULE_ORIGIN_MRP_MEMBER) continue;
        if (m->state != GWY_MODULE_EXTRACTED && m->state != GWY_MODULE_MAPPED) continue;
        /* Prefer EXT-like names; skip pure scripts without .ext */
        if (!strstr(m->resolved_name, ".ext") && !strstr(m->requested_name, ".ext")) continue;
        s = ext_loader_find_session_by_module(loader, m->module_id);
        if (!s) {
            LauncherError err;
            if (ext_loader_begin(loader, m->module_id, NULL, 0, &s, &err) != L_OK) continue;
        }
        n++;
        found = s;
    }
    return n == 1 ? found : NULL;
}

void ext_loader_on_c_function_new(ExtLoader *loader,
                                  uint32_t helper,
                                  uint32_t p_len,
                                  uint32_t p_guest_addr,
                                  uint32_t rw_base,
                                  uint32_t rw_size,
                                  uint32_t stack_base) {
    ExtLoadSession *s = NULL;
    const GwyLoadedModule *by_region;
    const GwyLoadedModule *dsm;
    GwyModuleMapping map;
    LauncherError err;
    if (!loader) return;
    printf("[EXT_LOAD] stage=C_FUNCTION_NEW_ARGS helper=0x%X p_len=%u p_guest=0x%X "
           "rw_base=0x%X rw_size=%u stack=0x%X evidence=OBSERVED\n",
           helper, p_len, p_guest_addr, rw_base, rw_size, stack_base);
    fflush(stdout);

    if (!loader->reg) {
        loader->early_valid = 1;
        loader->early_helper = helper;
        loader->early_p_len = p_len;
        loader->early_p_guest = p_guest_addr;
        loader->early_rw_base = rw_base;
        loader->early_rw_size = rw_size;
        loader->early_stack = stack_base;
        printf("[EXT_LOAD] stage=EARLY_STASH helper=0x%X origin=DSM\n", helper);
        fflush(stdout);
        return;
    }

    /* Already registered helper → refresh RW only. */
    {
        const GwyLoadedModule *by_h = module_registry_find_by_helper(loader->reg, helper);
        if (by_h && by_h->state >= GWY_MODULE_REGISTERED) {
            if (rw_base) {
                GwyLoadedModule *mut = module_mut_id(loader->reg, by_h->module_id);
                if (mut) {
                    mut->map.guest_rw_base = rw_base;
                    mut->map.guest_rw_size = rw_size;
                    mut->map.guest_stack_base = stack_base;
                }
                {
                    LauncherError e2;
                    module_registry_set_er_rw(loader->reg, by_h->module_id, rw_base, rw_size,
                                              GWY_ER_RW_SRC_MR_C_FUNCTION_LOAD,
                                              GWY_ADDR_EV_DOCUMENTED, &e2);
                }
            }
            return;
        }
    }

    s = active_pending(loader);
    if (!s) {
        by_region = module_registry_find_by_code_addr(loader->reg, helper);
        if (by_region && by_region->origin == MODULE_ORIGIN_MRP_MEMBER) {
            s = ext_loader_find_session_by_module(loader, by_region->module_id);
        }
    }
    /* Package scope: prefer active package primary over ambiguous unfinished MRP / DSM. */
    if (!s && package_scope_enabled()) {
        const char *primary = package_scope_active_primary();
        if (primary && primary[0]) {
            const GwyLoadedModule *pm = module_registry_find(loader->reg, primary);
            if (pm && pm->origin == MODULE_ORIGIN_MRP_MEMBER &&
                (pm->state == GWY_MODULE_EXTRACTED || pm->state == GWY_MODULE_MAPPED)) {
                s = ext_loader_find_session_by_module(loader, pm->module_id);
                if (!s) {
                    if (ext_loader_begin(loader, pm->module_id, NULL, 0, &s, &err) != L_OK)
                        s = NULL;
                }
                if (s) {
                    activate_session(s);
                    printf("[EXT_LOAD] stage=PACKAGE_SCOPE_PRIMARY package=%s primary=%s "
                           "module_id=%llu helper=0x%X evidence=CROSS_TARGET\n",
                           package_scope_active_package() ? package_scope_active_package() : "?",
                           primary, (unsigned long long)pm->module_id, helper);
                    fflush(stdout);
                }
            }
        }
    }
    if (!s) {
        s = unique_unfinished_mrp(loader);
    }

    dsm = module_registry_find(loader->reg, "dsm:cfunction.ext");
    if (!s) {
        /* Scoped cload: do not fall back to global DSM for package primary handoff. */
        if (package_scope_enabled() && package_scope_active_primary()) {
            printf("[EXT_LOAD] stage=PACKAGE_SCOPE_MISS primary=%s helper=0x%X "
                   "refuse_dsm_fallback evidence=TARGET_OBSERVED\n",
                   package_scope_active_primary(), helper);
            fflush(stdout);
            return;
        }
        if (!dsm || dsm->state < GWY_MODULE_REGISTERED) {
            ensure_dsm_module(loader, helper, rw_base, rw_size, stack_base, &err);
            return;
        }
        printf("[EXT_LOAD] stage=UNASSOCIATED helper=0x%X (no load_id; Case D)\n", helper);
        fflush(stdout);
        return;
    }

    {
        const GwyLoadedModule *m = module_registry_find_by_id(loader->reg, s->ctx.module_id);
        if (!m) return;

        memset(&map, 0, sizeof(map));
        map.guest_code_base = helper;
        map.guest_code_size = m->extracted_size ? m->extracted_size : 1;
        map.guest_rw_base = rw_base;
        map.guest_rw_size = rw_size;
        map.guest_stack_base = stack_base;
        map.helper_address = helper;
        /* entry_address filled by registry from image+8; do not set = helper */
        map.entry_address = 0;

        if (m->state == GWY_MODULE_EXTRACTED) {
            if (ext_loader_map(s, &map, &err) != L_OK) {
                module_registry_set_state(loader->reg, s->ctx.module_id, GWY_MODULE_FAILED,
                                          (int)err.code, &err);
                return;
            }
        } else if (m->state == GWY_MODULE_MAPPED && rw_base) {
            GwyLoadedModule *mut = module_mut_id(loader->reg, m->module_id);
            if (mut) {
                mut->map.guest_rw_base = rw_base;
                mut->map.guest_rw_size = rw_size;
                mut->map.guest_stack_base = stack_base;
            }
        }

        if (rw_base) {
            LauncherError e2;
            module_registry_set_er_rw(loader->reg, s->ctx.module_id, rw_base, rw_size,
                                      GWY_ER_RW_SRC_MR_C_FUNCTION_LOAD, GWY_ADDR_EV_DOCUMENTED,
                                      &e2);
        }

        m = module_registry_find_by_id(loader->reg, s->ctx.module_id);
        if (m && m->state == GWY_MODULE_MAPPED) {
            /* Register helper only; second arg ignored when == helper */
            if (ext_loader_register(s, helper, helper, &err) != L_OK) {
                module_registry_set_state(loader->reg, s->ctx.module_id, GWY_MODULE_FAILED,
                                          (int)err.code, &err);
                return;
            }
        }
        s->pending_active = 0;
        log_load(&s->ctx, "REGISTERED", m ? m->extracted_size : 0);
        {
            const GwyLoadedModule *rm =
                module_registry_find_by_id(loader->reg, s->ctx.module_id);
            if (rm) {
                ext_helper_handoff_note_registered(
                    rm->module_id, helper,
                    rm->resolved_name[0] ? rm->resolved_name : rm->requested_name);
                ext_dsm_record_note_registered(rm->module_id, helper);
                ext_dsm_record_note_post_register(rm->module_id, helper);
                ext_entry_null_contract_on_registered(rm->module_id, helper);
                ext_module_data_init_on_registered(rm->module_id, helper);
            }
        }
    }

    {
        const char *snap = getenv("GWY_MODULE_SNAPSHOT");
        if (snap && snap[0] && loader->reg) {
            LauncherError e2;
            module_registry_write_snapshot(loader->reg, snap, &e2);
        }
    }
}

void ext_loader_on_helper_call(ExtLoader *loader,
                               uint32_t helper,
                               uint32_t method,
                               int32_t ret_value) {
    const GwyLoadedModule *m;
    ExtLoadSession *s;
    LauncherError err;
    if (!loader || !loader->reg || helper == 0) return;
    m = module_registry_find_by_helper(loader->reg, helper);
    if (!m) {
        m = module_registry_find_by_code_addr(loader->reg, helper);
    }
    if (!m) return;
    s = ext_loader_find_session_by_module(loader, m->module_id);
    if (!s) {
        if (ext_loader_begin(loader, m->module_id, NULL, 0, &s, &err) != L_OK) return;
    }
    if (m->state == GWY_MODULE_REGISTERED || m->state == GWY_MODULE_ENTRY_CALLED) {
        ext_loader_call_entry(s, method, 0, 0, ret_value, &err);
    }
}

/* Flush early DSM stash after registry bind. */
void ext_loader_flush_early(ExtLoader *loader) {
    LauncherError err;
    if (!loader || !loader->reg || !loader->early_valid) return;
    ensure_dsm_module(loader, loader->early_helper, loader->early_rw_base, loader->early_rw_size,
                      loader->early_stack, &err);
    loader->early_valid = 0;
}
