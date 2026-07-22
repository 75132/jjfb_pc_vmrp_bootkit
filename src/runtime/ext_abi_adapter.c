#include "gwy_launcher/ext_abi_adapter.h"
#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/ext_lifecycle.h"
#include "gwy_launcher/ext_loader.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/package_metadata.h"
#include "gwy_launcher/package_scope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GWY_EXT_ABI_CALL_LOG_MAX 8

static char g_run_id[64];
static GwyExtHelperCallFn g_call_fn;
static GwyExtAppInfoAllocFn g_appinfo_alloc;

static int g_pending;
static uint64_t g_pending_module_id;
static uint64_t g_pending_generation;
static char g_pending_module[64];
static uint64_t g_done_generation;

static int32_t g_last_version = -1;
static int32_t g_last_appinfo = -1;
static int32_t g_last_init = -1;
static int g_init_was_zero;

static int g_inject_active;
static GwyExtAbiContext g_inject_ctx;
static GwyExtAbiContextVerdict g_inject_verdict;

static uint32_t g_call_methods[GWY_EXT_ABI_CALL_LOG_MAX];
static int g_call_count;
static int g_frame_depth;

void ext_abi_adapter_reset(void) {
    g_pending = 0;
    g_pending_module_id = 0;
    g_pending_generation = 0;
    g_pending_module[0] = 0;
    g_done_generation = 0;
    g_last_version = g_last_appinfo = g_last_init = -1;
    g_init_was_zero = 0;
    g_inject_active = 0;
    memset(&g_inject_ctx, 0, sizeof(g_inject_ctx));
    g_inject_verdict = EXT_ABI_CONTEXT_INCOMPLETE;
    g_call_count = 0;
    g_frame_depth = 0;
}

void ext_abi_adapter_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *ext_abi_adapter_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

void ext_abi_adapter_set_helper_call(GwyExtHelperCallFn fn) { g_call_fn = fn; }
void ext_abi_adapter_set_appinfo_alloc(GwyExtAppInfoAllocFn fn) { g_appinfo_alloc = fn; }

const char *ext_abi_context_verdict_name(GwyExtAbiContextVerdict v) {
    switch (v) {
    case EXT_ABI_CONTEXT_READY: return "EXT_ABI_CONTEXT_READY";
    case EXT_ABI_CONTEXT_INCOMPLETE: return "EXT_ABI_CONTEXT_INCOMPLETE";
    case EXT_ABI_OWNER_MISMATCH: return "EXT_ABI_OWNER_MISMATCH";
    case EXT_ABI_STALE_GENERATION: return "EXT_ABI_STALE_GENERATION";
    default: return "EXT_ABI_UNKNOWN";
    }
}

void ext_abi_adapter_test_set_context(const GwyExtAbiContext *ctx, GwyExtAbiContextVerdict verdict) {
    g_inject_active = 1;
    g_inject_verdict = verdict;
    if (ctx)
        g_inject_ctx = *ctx;
    else
        memset(&g_inject_ctx, 0, sizeof(g_inject_ctx));
}

void ext_abi_adapter_test_clear_inject(void) { g_inject_active = 0; }

int32_t ext_abi_adapter_last_version_ret(void) { return g_last_version; }
int32_t ext_abi_adapter_last_appinfo_ret(void) { return g_last_appinfo; }
int32_t ext_abi_adapter_last_init_ret(void) { return g_last_init; }
int ext_abi_adapter_init_was_zero(void) { return g_init_was_zero; }
int ext_abi_adapter_test_call_count(void) { return g_call_count; }
uint32_t ext_abi_adapter_test_call_method(int index) {
    if (index < 0 || index >= g_call_count) return 0xffffffffu;
    return g_call_methods[index];
}

uint64_t ext_abi_adapter_done_generation(void) { return g_done_generation; }
int ext_abi_adapter_handshake_pending(void) { return g_pending; }

void ext_abi_adapter_request_handshake(uint64_t module_id, uint64_t generation,
                                       const char *module_name) {
    if (!module_id) return;
    if (g_done_generation && g_done_generation == generation) return;
    if (g_pending && g_pending_generation == generation) return;
    g_pending = 1;
    g_pending_module_id = module_id;
    g_pending_generation = generation;
    if (module_name && module_name[0])
        snprintf(g_pending_module, sizeof(g_pending_module), "%s", module_name);
    else
        snprintf(g_pending_module, sizeof(g_pending_module), "robotol.ext");
    printf("[EXT_ABI_HANDSHAKE_QUEUED] module=%s module_id=%llu generation=%llu "
           "run_id=%s evidence=OBSERVED\n",
           g_pending_module, (unsigned long long)module_id, (unsigned long long)generation,
           ext_abi_adapter_run_id());
    fflush(stdout);
}

static int name_is_primary_ext(const char *n) {
    if (!n) return 0;
    return strstr(n, "robotol") != NULL || strstr(n, "mmochat") != NULL;
}

static int guest_mapped_ok(uint32_t addr) {
    /* Plausible guest VA; full map check is owner/chunk based. */
    return addr != 0u && (addr & ~1u) > 0x1000u;
}

GwyExtAbiContextVerdict ext_abi_adapter_build_context(GwyExtAbiContext *out) {
    ModuleRegistry *reg;
    const GwyLoadedModule *m = NULL;
    ExtChunkOwnerInfo owner;
    uint32_t appid = 0, appver = 0;
    int src = 0;
    const char *primary;
    size_t i;

    if (!out) return EXT_ABI_CONTEXT_INCOMPLETE;
    memset(out, 0, sizeof(*out));
    out->mr_version = GWY_EXT_MR_VERSION;

    if (g_inject_active) {
        *out = g_inject_ctx;
        return g_inject_verdict;
    }

    reg = gwy_ext_loader_bound_registry();
    if (!reg) return EXT_ABI_CONTEXT_INCOMPLETE;

    primary = package_scope_active_primary();
    if (primary && primary[0]) {
        m = module_registry_find(reg, primary);
    }
    if (!m) {
        for (i = 0; i < reg->count; i++) {
            const GwyLoadedModule *cand = &reg->modules[i];
            const char *n = cand->resolved_name[0] ? cand->resolved_name : cand->requested_name;
            if (cand->origin != MODULE_ORIGIN_MRP_MEMBER) continue;
            if (name_is_primary_ext(n)) {
                m = cand;
                break;
            }
        }
    }
    if (!m) return EXT_ABI_CONTEXT_INCOMPLETE;

    snprintf(out->module_name, sizeof(out->module_name), "%s",
             m->resolved_name[0] ? m->resolved_name : m->requested_name);
    out->module_id = m->module_id;
    out->helper = m->map.helper_address ? m->map.helper_address : m->entries.registered_helper;
    out->er_rw_guest = m->data.start_of_er_rw;
    out->er_rw_size = m->data.er_rw_size;

    memset(&owner, 0, sizeof(owner));
    if (out->helper && ext_chunk_provider_owner_for_helper(out->helper, &owner)) {
        out->p_guest = owner.p_guest;
        out->ext_chunk_guest = owner.chunk_guest;
        out->module_generation = owner.module_generation;
        if (owner.erw && !out->er_rw_guest) out->er_rw_guest = owner.erw;
    } else if (ext_chunk_provider_last_p_guest()) {
        out->p_guest = ext_chunk_provider_last_p_guest();
        out->ext_chunk_guest = ext_chunk_provider_last_chunk_guest();
        if (ext_chunk_provider_owner_for_p(out->p_guest, &owner))
            out->module_generation = owner.module_generation;
    }

    if (g_pending_generation && out->module_generation &&
        out->module_generation != g_pending_generation && g_pending_module_id == out->module_id)
        return EXT_ABI_STALE_GENERATION;

    if (g_pending_generation && !out->module_generation)
        out->module_generation = g_pending_generation;
    if (!out->module_generation) out->module_generation = out->module_id;

    if (!gwy_package_appinfo_resolve_id_ver(&appid, &appver, &src) || !appid) {
        return EXT_ABI_CONTEXT_INCOMPLETE;
    }
    out->appid = appid;
    out->appver = appver;

    if (!guest_mapped_ok(out->helper) || !guest_mapped_ok(out->p_guest) ||
        !guest_mapped_ok(out->er_rw_guest) || !guest_mapped_ok(out->ext_chunk_guest))
        return EXT_ABI_CONTEXT_INCOMPLETE;

    /* Helper must land in this module's executable mapping. */
    if (m->map.guest_code_base && m->map.guest_code_size) {
        uint32_t hb = out->helper & ~1u;
        if (hb < m->map.guest_code_base ||
            hb >= m->map.guest_code_base + m->map.guest_code_size)
            return EXT_ABI_OWNER_MISMATCH;
    }

    if (owner.module_id && owner.module_id != out->module_id) return EXT_ABI_OWNER_MISMATCH;
    if (owner.helper && (owner.helper & ~1u) != (out->helper & ~1u)) return EXT_ABI_OWNER_MISMATCH;

    return EXT_ABI_CONTEXT_READY;
}

static void log_call(const GwyExtAbiContext *ctx, uint32_t method, uint32_t input,
                     uint32_t input_len) {
    printf("[EXT_ABI_CALL] module=%s method=%u source=PRODUCT_ADAPTER "
           "helper=0x%X P=0x%X erw=0x%X chunk=0x%X input=0x%X input_len=%u "
           "module_id=%llu generation=%llu run_id=%s evidence=OBSERVED\n",
           ctx->module_name, method, ctx->helper, ctx->p_guest, ctx->er_rw_guest,
           ctx->ext_chunk_guest, input, input_len, (unsigned long long)ctx->module_id,
           (unsigned long long)ctx->module_generation, ext_abi_adapter_run_id());
    fflush(stdout);
}

static void log_return(const GwyExtAbiContext *ctx, uint32_t method, int32_t ret) {
    printf("[EXT_ABI_RETURN] module=%s method=%u ret=%d source=PRODUCT_ADAPTER "
           "module_id=%llu generation=%llu run_id=%s evidence=OBSERVED\n",
           ctx->module_name, method, (int)ret, (unsigned long long)ctx->module_id,
           (unsigned long long)ctx->module_generation, ext_abi_adapter_run_id());
    if (method == GWY_EXT_METHOD_VERSION && ret == 0) {
        printf("[EXT_VERSION_RETURN_ZERO] module=%s ret=0 run_id=%s evidence=OBSERVED\n",
               ctx->module_name, ext_abi_adapter_run_id());
    }
    if (method == GWY_EXT_METHOD_APPINFO && ret == 0) {
        printf("[EXT_APPINFO_RETURN_ZERO] module=%s ret=0 appid=%u appver=%u run_id=%s "
               "evidence=OBSERVED\n",
               ctx->module_name, ctx->appid, ctx->appver, ext_abi_adapter_run_id());
    }
    fflush(stdout);
}

static int32_t call_method(void *uc, const GwyExtAbiContext *ctx, uint32_t method, uint32_t input,
                           uint32_t input_len) {
    int32_t ret;
    uint32_t restored_r9 = 0;

    if (g_call_count < GWY_EXT_ABI_CALL_LOG_MAX) g_call_methods[g_call_count++] = method;

    g_frame_depth++;
    printf("[EXT_ABI_FRAME_ENTER] method=%u owner_module_id=%llu owner_generation=%llu "
           "r9=0x%X helper=0x%X p=0x%X chunk=0x%X depth=%d run_id=%s evidence=OBSERVED\n",
           method, (unsigned long long)ctx->module_id, (unsigned long long)ctx->module_generation,
           ctx->er_rw_guest, ctx->helper, ctx->p_guest, ctx->ext_chunk_guest, g_frame_depth,
           ext_abi_adapter_run_id());
    fflush(stdout);

    log_call(ctx, method, input, input_len);
    if (!g_call_fn) {
        ret = -1;
    } else {
        ret = g_call_fn(uc, ctx->helper, ctx->p_guest, method, input, input_len, ctx->er_rw_guest);
    }
    log_return(ctx, method, ret);

    restored_r9 = 0; /* caller regs restored inside helper call fn */
    printf("[EXT_ABI_FRAME_LEAVE] method=%u return=%d restored_r9=0x%X depth=%d run_id=%s "
           "evidence=OBSERVED\n",
           method, (int)ret, restored_r9, g_frame_depth, ext_abi_adapter_run_id());
    fflush(stdout);
    g_frame_depth--;
    return ret;
}

static void product_report_path(char *out, size_t out_sz, const char *name) {
    const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (root && root[0])
        snprintf(out, out_sz, "%s/%s", root, name);
    else
        snprintf(out, out_sz, "reports/%s", name);
}

static void append_handshake_csv(const GwyExtAbiContext *ctx, GwyExtAbiContextVerdict v,
                                 int32_t r6, int32_t r8, int32_t r0, const char *result) {
    char path[512];
    FILE *f;
    product_report_path(path, sizeof(path), "product_ext_abi_handshake.csv");
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f,
            "%s,%s,%llu,%llu,0x%X,0x%X,0x%X,0x%X,%u,%u,%s,%d,%d,%d,%s\n",
            ext_abi_adapter_run_id(), ctx->module_name, (unsigned long long)ctx->module_id,
            (unsigned long long)ctx->module_generation, ctx->helper, ctx->p_guest, ctx->er_rw_guest,
            ctx->ext_chunk_guest, ctx->appid, ctx->appver, ext_abi_context_verdict_name(v), (int)r6,
            (int)r8, (int)r0, result ? result : "?");
    fclose(f);
}

static void ensure_csv_header(void) {
    char path[512];
    FILE *f;
    product_report_path(path, sizeof(path), "product_ext_abi_handshake.csv");
    f = fopen(path, "r");
    if (f) {
        fclose(f);
        return;
    }
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "run_id,module,module_id,generation,helper,p,er_rw,chunk,appid,appver,verdict,"
               "ret6,ret8,ret0,result\n");
    fclose(f);
}

GwyExtHandshakeResult ext_abi_adapter_try_deliver(void *uc) {
    GwyExtAbiContext ctx;
    GwyExtAbiContextVerdict verdict;
    uint32_t appinfo = 0;
    int32_t r6 = -1, r8 = -1, r0 = -1;

    if (!g_pending) return GWY_EXT_HS_NOT_PENDING;
    if (g_done_generation && g_done_generation == g_pending_generation)
        return GWY_EXT_HS_ALREADY_DONE;
    if (!g_call_fn) return GWY_EXT_HS_NO_CALL_FN;

    ensure_csv_header();
    verdict = ext_abi_adapter_build_context(&ctx);
    printf("[EXT_ABI_CONTEXT] verdict=%s module=%s module_id=%llu generation=%llu "
           "helper=0x%X P=0x%X erw=0x%X chunk=0x%X appid=%u appver=%u run_id=%s "
           "evidence=OBSERVED\n",
           ext_abi_context_verdict_name(verdict), ctx.module_name,
           (unsigned long long)ctx.module_id, (unsigned long long)ctx.module_generation,
           ctx.helper, ctx.p_guest, ctx.er_rw_guest, ctx.ext_chunk_guest, ctx.appid, ctx.appver,
           ext_abi_adapter_run_id());
    fflush(stdout);

    if (verdict != EXT_ABI_CONTEXT_READY) {
        append_handshake_csv(&ctx, verdict, -1, -1, -1, ext_abi_context_verdict_name(verdict));
        return GWY_EXT_HS_CONTEXT_NOT_READY;
    }

    if (g_pending_module_id && ctx.module_id != g_pending_module_id) {
        append_handshake_csv(&ctx, EXT_ABI_OWNER_MISMATCH, -1, -1, -1, "OWNER_MISMATCH");
        return GWY_EXT_HS_CONTEXT_NOT_READY;
    }

    ext_lifecycle_ensure(ctx.module_id, ctx.module_generation, ctx.module_name);
    ext_lifecycle_note_abi_ready(ctx.module_id);

    /* Exactly once: mark done before calls so re-entry cannot double-deliver. */
    g_pending = 0;
    g_done_generation = ctx.module_generation;
    g_call_count = 0;

    r6 = call_method(uc, &ctx, GWY_EXT_METHOD_VERSION, 0u, ctx.mr_version);
    g_last_version = r6;
    ext_lifecycle_note_version(ctx.module_id, r6);
    if (r6 != 0) {
        append_handshake_csv(&ctx, verdict, r6, -1, -1, "VERSION_FAILED");
        return GWY_EXT_HS_VERSION_FAILED;
    }

    if (g_appinfo_alloc)
        appinfo = g_appinfo_alloc(ctx.appid, ctx.appver);
    if (!appinfo) {
        append_handshake_csv(&ctx, verdict, r6, -1, -1, "APPINFO_ALLOC_FAILED");
        return GWY_EXT_HS_APPINFO_FAILED;
    }

    r8 = call_method(uc, &ctx, GWY_EXT_METHOD_APPINFO, appinfo, 16u);
    g_last_appinfo = r8;
    ext_lifecycle_note_appinfo(ctx.module_id, r8);
    if (r8 != 0) {
        append_handshake_csv(&ctx, verdict, r6, r8, -1, "APPINFO_FAILED");
        return GWY_EXT_HS_APPINFO_FAILED;
    }

    r0 = call_method(uc, &ctx, GWY_EXT_METHOD_INIT, 0u, ctx.mr_version);
    g_last_init = r0;
    ext_lifecycle_note_init(ctx.module_id, r0);
    g_init_was_zero = (r0 == 0);
    append_handshake_csv(&ctx, verdict, r6, r8, r0, r0 == 0 ? "INIT_OK" : "INIT_FAILED");
    if (r0 != 0) return GWY_EXT_HS_INIT_FAILED;
    return GWY_EXT_HS_OK;
}
