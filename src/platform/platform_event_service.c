#include "gwy_launcher/platform_event_service.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_run_id[64];
static GwyPlatformEvent g_ev[GWY_PES_MAX_EVENTS];
static int g_ev_n;
static uint64_t g_next_id = 1;
static GwyEventContextObject g_ctx[GWY_PES_MAX_CTX];
static int g_ctx_n;
static uint64_t g_insn;
static GwyEventIdentityClass g_id_class;
static int g_state_advanced;
static int g_finalized;
static int g_one_shot; /* diagnostic only — env JJFB_PRODUCT_P5_ONE_SHOT */
static int g_one_shot_known;

static int env1(const char *name) {
    const char *e = getenv(name);
    return e && e[0] == '1' && e[1] == '\0';
}

static void report_path(char *out, size_t n, const char *name) {
    const char *root = getenv("GWY_PRODUCT_REPORTS_DIR");
    if (root && root[0])
        snprintf(out, n, "%s/%s", root, name);
    else
        snprintf(out, n, "reports/%s", name);
}

static uint32_t digest_bytes(const uint8_t *buf, size_t n) {
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < n; i++) {
        h ^= buf[i];
        h *= 16777619u;
    }
    return h;
}

static uint32_t er_digest(void *uc, uint32_t er_rw) {
    uint8_t buf[256];
    uint32_t ui = 0;
    uint32_t h;
    if (!uc || !er_rw) return 0;
    memset(buf, 0, sizeof(buf));
    (void)guest_memory_uc_peek((struct uc_struct *)uc, er_rw, buf, sizeof(buf));
    h = digest_bytes(buf, sizeof(buf));
    /* UI_MODE / state switch lives past the 256B head window (0x800+0xD0). */
    if (guest_memory_uc_peek_u32((struct uc_struct *)uc, er_rw + GWY_PES_UI_MODE_OFF, &ui)) {
        h ^= ui + 0x9e3779b9u;
        h *= 16777619u;
    }
    return h;
}

static int one_shot_diag(void) {
    if (!g_one_shot_known) {
        g_one_shot = env1("JJFB_PRODUCT_P5_ONE_SHOT");
        g_one_shot_known = 1;
    }
    return g_one_shot;
}

void platform_event_service_reset(void) {
    memset(g_ev, 0, sizeof(g_ev));
    memset(g_ctx, 0, sizeof(g_ctx));
    g_ev_n = 0;
    g_ctx_n = 0;
    g_next_id = 1;
    g_insn = 0;
    g_id_class = GWY_EVT_ID_UNKNOWN;
    g_state_advanced = 0;
    g_finalized = 0;
    g_one_shot_known = 0;
    g_one_shot = 0;
}

void platform_event_service_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *platform_event_service_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

void platform_event_service_set_insn_count(uint64_t n) { g_insn = n; }
uint64_t platform_event_service_insn_count(void) { return g_insn; }
void platform_event_service_bump_insn(uint64_t delta) { g_insn += delta; }

const char *gwy_event_state_name(GwyEventState s) {
    switch (s) {
    case GWY_EVT_GUEST_REQUESTED: return "GUEST_REQUESTED";
    case GWY_EVT_ACCEPTED: return "ACCEPTED";
    case GWY_EVT_CONTEXT_PREPARED: return "CONTEXT_PREPARED";
    case GWY_EVT_QUEUED: return "QUEUED";
    case GWY_EVT_DELIVERED: return "DELIVERED";
    case GWY_EVT_GUEST_ACKNOWLEDGED: return "GUEST_ACKNOWLEDGED";
    case GWY_EVT_COMPLETED: return "COMPLETED";
    default: return "NONE";
    }
}

const char *gwy_event_identity_class_name(GwyEventIdentityClass c) {
    switch (c) {
    case GWY_EVT_ID_SAME_UNFINISHED: return "SAME_UNFINISHED_REQUEST";
    case GWY_EVT_ID_NEW_REQUEST: return "NEW_REQUEST_EACH_CALL";
    case GWY_EVT_ID_MIXED: return "MIXED_IDENTITY";
    default: return "IDENTITY_UNKNOWN";
    }
}

uint64_t platform_event_identity_hash(uint32_t event_code, uint32_t app, const uint32_t r[8],
                                      uint32_t stack0, uint32_t stack1, uint32_t guest_context,
                                      uint32_t guest_payload) {
    uint64_t h = 14695981039346656037ull;
    int i;
#define MIX(v)                                                                                     \
    do {                                                                                           \
        h ^= (uint64_t)(v);                                                                        \
        h *= 1099511628211ull;                                                                     \
    } while (0)
    MIX(event_code);
    MIX(app);
    if (r) {
        for (i = 0; i < 8; i++) MIX(r[i]);
    }
    MIX(stack0);
    MIX(stack1);
    MIX(guest_context);
    MIX(guest_payload);
#undef MIX
    return h;
}

static uint32_t pick_context_for_request(uint32_t er_rw, const uint32_t r[8]) {
    int i, j;
    /* Prefer live 10165 whose pointer appears in regs. */
    for (i = 0; i < g_ctx_n; i++) {
        if (!g_ctx[i].used || !g_ctx[i].guest_ptr || g_ctx[i].plat_code != 0x10165u) continue;
        if (r) {
            for (j = 0; j < 8; j++)
                if (r[j] == g_ctx[i].guest_ptr) return g_ctx[i].guest_ptr;
        }
    }
    /* Prefer 10165 with proven owner store — no latest-object fallback. */
    for (i = 0; i < g_ctx_n; i++) {
        if (!g_ctx[i].used || !g_ctx[i].guest_ptr || g_ctx[i].plat_code != 0x10165u) continue;
        if (g_ctx[i].owner_store_addr) return g_ctx[i].guest_ptr;
    }
    for (i = 0; i < g_ctx_n; i++) {
        if (!g_ctx[i].used || !g_ctx[i].guest_ptr) continue;
        if (r) {
            for (j = 0; j < 8; j++)
                if (r[j] == g_ctx[i].guest_ptr) return g_ctx[i].guest_ptr;
        }
    }
    (void)er_rw;
    return 0;
}

int platform_event_service_resolve_completion_objs(uint32_t *out_10165, uint32_t *out_10162,
                                                   uint32_t *out_enq_handler,
                                                   uint32_t *out_owner_store) {
    int i;
    uint32_t p65 = 0, p62 = 0, enq = 0, store = 0, store62 = 0;
    if (out_10165) *out_10165 = 0;
    if (out_10162) *out_10162 = 0;
    if (out_enq_handler) *out_enq_handler = 0;
    if (out_owner_store) *out_owner_store = 0;
    /* Prefer owner-scoped objects (ER_RW stores). Never use the first stale alloc pair. */
    for (i = 0; i < g_ctx_n; i++) {
        if (!g_ctx[i].used || !g_ctx[i].guest_ptr) continue;
        if (g_ctx[i].plat_code == 0x10165u && g_ctx[i].owner_store_addr) {
            p65 = g_ctx[i].guest_ptr;
            store = g_ctx[i].owner_store_addr;
            if (g_ctx[i].handler) enq = g_ctx[i].handler;
        }
    }
    for (i = 0; i < g_ctx_n; i++) {
        if (!g_ctx[i].used || !g_ctx[i].guest_ptr) continue;
        if (g_ctx[i].plat_code != 0x10162u) continue;
        if (g_ctx[i].owner_store_addr) {
            p62 = g_ctx[i].guest_ptr;
            store62 = g_ctx[i].owner_store_addr;
            if (!enq && g_ctx[i].handler) enq = g_ctx[i].handler;
        }
    }
    /* If 10162 lacks owner_store, pick latest 10162 (second alloc wave). */
    if (!p62) {
        for (i = g_ctx_n - 1; i >= 0; i--) {
            if (g_ctx[i].used && g_ctx[i].guest_ptr && g_ctx[i].plat_code == 0x10162u) {
                p62 = g_ctx[i].guest_ptr;
                break;
            }
        }
    }
    (void)store62;
    if (!p65 || !enq) return 0;
    if (!p62) p62 = p65;
    if (out_10165) *out_10165 = p65;
    if (out_10162) *out_10162 = p62;
    if (out_enq_handler) *out_enq_handler = enq;
    if (out_owner_store) *out_owner_store = store;
    return 1;
}

uint32_t platform_event_service_read_ui_mode(void *uc, uint32_t er_rw) {
    uint32_t ui = 0;
    if (!uc || !er_rw) return 0;
    (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, er_rw + GWY_PES_UI_MODE_OFF, &ui);
    return ui;
}

void platform_event_service_note_ui_mode(void *uc, uint32_t er_rw, uint64_t request_id) {
    uint32_t ui;
    if (!uc || !er_rw) return;
    ui = platform_event_service_read_ui_mode(uc, er_rw);
    if (ui != 0u) {
        printf("[EVENT_COMPLETION_GUEST_VISIBLE] request_id=%llu ui_mode=0x%X off=0x%X "
               "er_rw=0x%X run_id=%s evidence=OBSERVED\n",
               (unsigned long long)request_id, ui, GWY_PES_UI_MODE_OFF, er_rw,
               platform_event_service_run_id());
        fflush(stdout);
    }
}

GwyEventContextObject *platform_event_service_note_alloc(uint32_t plat_code, uint32_t size,
                                                         uint32_t guest_ptr, uint32_t handler,
                                                         uint64_t owner_module_id) {
    GwyEventContextObject *c;
    if (!guest_ptr || g_ctx_n >= GWY_PES_MAX_CTX) return NULL;
    c = &g_ctx[g_ctx_n++];
    memset(c, 0, sizeof(*c));
    c->used = 1;
    c->plat_code = plat_code;
    c->size = size;
    c->guest_ptr = guest_ptr;
    c->handler = handler;
    c->owner_module_id = owner_module_id;
    c->alloc_insn_count = g_insn;
    printf("[EVENT_CONTEXT_OBJECT_IDENTIFIED] plat=0x%X ptr=0x%X size=0x%X handler=0x%X "
           "run_id=%s evidence=OBSERVED\n",
           plat_code, guest_ptr, size, handler, platform_event_service_run_id());
    fflush(stdout);
    return c;
}

GwyEventContextObject *platform_event_service_ctx_by_ptr(uint32_t guest_ptr) {
    int i;
    for (i = 0; i < g_ctx_n; i++)
        if (g_ctx[i].used && g_ctx[i].guest_ptr == guest_ptr) return &g_ctx[i];
    return NULL;
}

int platform_event_service_ctx_count(void) { return g_ctx_n; }

GwyEventContextObject *platform_event_service_ctx_at(int index) {
    if (index < 0 || index >= g_ctx_n) return NULL;
    return &g_ctx[index];
}

void platform_event_service_note_ctx_mem(uint32_t guest_ptr, int is_write, uint32_t addr,
                                         uint32_t size) {
    GwyEventContextObject *c = platform_event_service_ctx_by_ptr(guest_ptr);
    (void)addr;
    (void)size;
    if (!c) {
        /* Allow addr inside any known object. */
        int i;
        for (i = 0; i < g_ctx_n; i++) {
            if (!g_ctx[i].used) continue;
            if (addr >= g_ctx[i].guest_ptr &&
                addr < g_ctx[i].guest_ptr + (g_ctx[i].size ? g_ctx[i].size : 0x100u)) {
                c = &g_ctx[i];
                break;
            }
        }
    }
    if (!c) return;
    if (is_write)
        c->write_count++;
    else
        c->read_count++;
}

void platform_event_service_note_ctx_owner_store(uint32_t guest_ptr, uint32_t store_addr) {
    GwyEventContextObject *c = platform_event_service_ctx_by_ptr(guest_ptr);
    if (!c) return;
    c->owner_store_addr = store_addr;
    printf("[EVENT_CONTEXT_OWNER_CONFIRMED] ptr=0x%X store=0x%X run_id=%s evidence=OBSERVED\n",
           guest_ptr, store_addr, platform_event_service_run_id());
    fflush(stdout);
}

void platform_event_service_refresh_ctx_digest(void *uc, GwyEventContextObject *ctx) {
    if (!uc || !ctx || !ctx->guest_ptr) return;
    memset(ctx->snap_latest, 0, sizeof(ctx->snap_latest));
    (void)guest_memory_uc_peek((struct uc_struct *)uc, ctx->guest_ptr, ctx->snap_latest,
                               sizeof(ctx->snap_latest));
    ctx->digest_latest = digest_bytes(ctx->snap_latest, sizeof(ctx->snap_latest));
    if (!ctx->digest_at_alloc) {
        memcpy(ctx->snap_alloc, ctx->snap_latest, sizeof(ctx->snap_alloc));
        ctx->digest_at_alloc = ctx->digest_latest;
    }
}

GwyPlatformEvent *platform_event_service_by_id(uint64_t request_id) {
    int i;
    for (i = 0; i < g_ev_n; i++)
        if (g_ev[i].request_id == request_id) return &g_ev[i];
    return NULL;
}

GwyPlatformEvent *platform_event_service_pending_for_handler(uint32_t handler) {
    int i;
    for (i = g_ev_n - 1; i >= 0; i--) {
        if (g_ev[i].accepted && !g_ev[i].delivered && g_ev[i].handler == handler)
            return &g_ev[i];
    }
    return NULL;
}

int platform_event_service_event_count(void) { return g_ev_n; }

GwyPlatformEvent *platform_event_service_event_at(int index) {
    if (index < 0 || index >= g_ev_n) return NULL;
    return &g_ev[index];
}

int platform_event_service_on_guest_request(void *uc, uint32_t event_code, uint32_t app,
                                            uint32_t family, uint32_t handler,
                                            uint32_t secondary_handler, uint32_t caller_pc,
                                            uint32_t caller_lr, const uint32_t r[8], uint32_t r9,
                                            uint32_t stack0, uint32_t stack1, uint32_t er_rw,
                                            uint64_t owner_module_id, uint64_t owner_generation,
                                            GwyPlatformEvent **out_ev) {
    GwyPlatformEvent *ev;
    uint32_t ctx_ptr;
    uint32_t payload;
    uint64_t idh;
    uint32_t dig;
    int i;

    if (out_ev) *out_ev = NULL;
    ctx_ptr = pick_context_for_request(er_rw, r);
    payload = r ? r[2] : 0;
    idh = platform_event_identity_hash(event_code, app, r, stack0, stack1, ctx_ptr, payload);
    dig = er_digest(uc, er_rw);

    /* Still-queued identical identity → same unfinished request (do not double-queue). */
    for (i = 0; i < g_ev_n; i++) {
        if (g_ev[i].identity_hash == idh && g_ev[i].accepted && !g_ev[i].delivered) {
            printf("[EVENT_TXN] op=ALREADY_QUEUED request_id=%llu identity=0x%llX "
                   "event=0x%X app=0x%X ctx=0x%X run_id=%s evidence=OBSERVED\n",
                   (unsigned long long)g_ev[i].request_id, (unsigned long long)idh, event_code, app,
                   ctx_ptr, platform_event_service_run_id());
            fflush(stdout);
            if (out_ev) *out_ev = &g_ev[i];
            return 0;
        }
    }

    /*
     * Diagnostic one-shot only: suppress re-delivery when identity matches a
     * completed request AND ER_RW digest is unchanged. Not default product policy.
     */
    if (one_shot_diag()) {
        for (i = g_ev_n - 1; i >= 0; i--) {
            if (g_ev[i].identity_hash == idh && g_ev[i].delivered &&
                g_ev[i].state_digest_after == dig && dig != 0) {
                printf("[EVENT_TXN] op=SUPPRESS_DIAG_ONE_SHOT identity=0x%llX event=0x%X "
                       "app=0x%X digest=0x%08X run_id=%s evidence=OBSERVED\n",
                       (unsigned long long)idh, event_code, app, dig,
                       platform_event_service_run_id());
                fflush(stdout);
                return 0;
            }
        }
    }

    if (g_ev_n >= GWY_PES_MAX_EVENTS) return 0;
    ev = &g_ev[g_ev_n++];
    memset(ev, 0, sizeof(*ev));
    ev->request_id = g_next_id++;
    ev->identity_hash = idh;
    ev->owner_module_id = owner_module_id;
    ev->owner_generation = owner_generation;
    ev->family = family;
    ev->event_code = event_code;
    ev->app = app;
    ev->guest_context = ctx_ptr;
    ev->guest_payload = payload;
    ev->stack_arg0 = stack0;
    ev->stack_arg1 = stack1;
    ev->handler = handler;
    ev->secondary_handler = secondary_handler;
    ev->caller_pc = caller_pc;
    ev->caller_lr = caller_lr;
    if (r) memcpy(ev->r, r, sizeof(ev->r));
    ev->r9 = r9;
    ev->guest_insn_count = g_insn;
    ev->state_digest_before = dig;
    if (ctx_ptr) {
        GwyEventContextObject *c = platform_event_service_ctx_by_ptr(ctx_ptr);
        if (c) {
            platform_event_service_refresh_ctx_digest(uc, c);
            ev->context_digest_before = c->digest_latest;
        }
    }
    ev->state = GWY_EVT_ACCEPTED;
    ev->from_guest = 1;
    ev->accepted = 1;

    printf("[EVENT_TXN] op=ACCEPT request_id=%llu identity=0x%llX event=0x%X app=0x%X "
           "ctx=0x%X payload=0x%X stack0=0x%X stack1=0x%X caller_pc=0x%X insn=%llu "
           "digest=0x%08X handler=0x%X run_id=%s evidence=OBSERVED\n",
           (unsigned long long)ev->request_id, (unsigned long long)idh, event_code, app, ctx_ptr,
           payload, stack0, stack1, caller_pc, (unsigned long long)g_insn, dig, handler,
           platform_event_service_run_id());
    fflush(stdout);

    if (out_ev) *out_ev = ev;
    return 1;
}

void platform_event_service_mark_queued(uint64_t request_id) {
    GwyPlatformEvent *ev = platform_event_service_by_id(request_id);
    if (!ev) return;
    ev->state = GWY_EVT_QUEUED;
}

void platform_event_service_suggest_delivery_abi(const GwyPlatformEvent *ev,
                                                GwyEventDeliveryAbi *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!ev) return;
    /* Baseline proven so far: r0=app (case), r1=event_code. */
    out->r0 = ev->app;
    out->r1 = ev->event_code;
    /* Candidate extras — not confirmed until handler reads them. */
    out->r2 = ev->guest_context ? ev->guest_context : ev->guest_payload;
    out->r3 = ev->payload_len;
    out->stack0 = ev->guest_context ? ev->guest_context : ev->stack_arg0;
    out->stack1 = ev->secondary_handler ? ev->secondary_handler : ev->stack_arg1;
    out->guest_context = ev->guest_context;
    out->guest_payload = ev->guest_payload;
    out->have_context = ev->guest_context ? 1 : 0;
    out->have_stack = (out->stack0 || out->stack1) ? 1 : 0;
    out->confirmed = 0;
}

void platform_event_service_on_deliver_enter(void *uc, uint64_t request_id,
                                             const GwyEventDeliveryAbi *abi) {
    GwyPlatformEvent *ev = platform_event_service_by_id(request_id);
    if (!ev) return;
    ev->state = GWY_EVT_DELIVERED;
    if (abi) {
        if (abi->r2) ev->guest_payload = abi->r2;
        if (abi->guest_context) ev->guest_context = abi->guest_context;
        if (abi->stack0) ev->stack_arg0 = abi->stack0;
        if (abi->stack1) ev->stack_arg1 = abi->stack1;
    }
    if (ev->guest_context)
        platform_event_service_refresh_ctx_digest(uc,
                                                  platform_event_service_ctx_by_ptr(ev->guest_context));
    printf("[EVENT_DELIVER_ENTER] request_id=%llu handler=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
           "stack0=0x%X stack1=0x%X ctx=0x%X run_id=%s evidence=OBSERVED\n",
           (unsigned long long)request_id, ev->handler, abi ? abi->r0 : ev->app,
           abi ? abi->r1 : ev->event_code, abi ? abi->r2 : 0, abi ? abi->r3 : 0,
           abi ? abi->stack0 : 0, abi ? abi->stack1 : 0, ev->guest_context,
           platform_event_service_run_id());
    fflush(stdout);
}

void platform_event_service_note_handler_mem(uint64_t request_id, int is_write, uint32_t addr,
                                             uint32_t size, uint32_t value) {
    GwyPlatformEvent *ev = platform_event_service_by_id(request_id);
    int i;
    (void)value;
    (void)size;
    if (!ev) return;
    if (is_write)
        ev->mem_writes++;
    else
        ev->mem_reads++;
    for (i = 0; i < g_ctx_n; i++) {
        if (!g_ctx[i].used) continue;
        if (addr >= g_ctx[i].guest_ptr &&
            addr < g_ctx[i].guest_ptr + (g_ctx[i].size ? g_ctx[i].size : 0x100u)) {
            ev->context_touched = 1;
            platform_event_service_note_ctx_mem(g_ctx[i].guest_ptr, is_write, addr, size);
        }
    }
}

void platform_event_service_note_stack_arg_read(uint64_t request_id) {
    GwyPlatformEvent *ev = platform_event_service_by_id(request_id);
    if (!ev) return;
    ev->stack_arg_reads++;
}

void platform_event_service_on_deliver_leave(void *uc, uint64_t request_id, int ok, int32_t ret,
                                             uint32_t er_rw) {
    GwyPlatformEvent *ev = platform_event_service_by_id(request_id);
    uint32_t dig;
    uint32_t ui_before = 0, ui_after = 0;
    (void)ok;
    if (!ev) return;
    ui_before = platform_event_service_read_ui_mode(uc, er_rw);
    dig = er_digest(uc, er_rw);
    ui_after = platform_event_service_read_ui_mode(uc, er_rw);
    ev->state_digest_after = dig;
    ev->handler_ret = ret;
    ev->delivered = 1;
    ev->state_changed = (dig != ev->state_digest_before && dig != 0 && ev->state_digest_before != 0) ||
                        (ui_after != ui_before);
    if (ui_after != 0u && (ui_after != ui_before || ev->state_changed)) {
        platform_event_service_note_ui_mode(uc, er_rw, request_id);
        printf("[EVENT_UNFINISHED_PREDICATE_FOUND] pc=0x30634C object=ER_RW offset=0x%X "
               "size=4 condition=ui_mode==0 current=0x%X completed=nonzero "
               "run_id=%s evidence=OBSERVED\n",
               GWY_PES_UI_MODE_OFF, ui_before, platform_event_service_run_id());
        fflush(stdout);
    }
    if (ev->guest_context) {
        GwyEventContextObject *c = platform_event_service_ctx_by_ptr(ev->guest_context);
        if (c) {
            platform_event_service_refresh_ctx_digest(uc, c);
            ev->context_digest_after = c->digest_latest;
            if (ev->context_digest_after != ev->context_digest_before &&
                ev->context_digest_before != 0) {
                printf("[EVENT_CONTEXT_LIFETIME_CONFIRMED] ptr=0x%X before=0x%08X after=0x%08X "
                       "run_id=%s evidence=OBSERVED\n",
                       ev->guest_context, ev->context_digest_before, ev->context_digest_after,
                       platform_event_service_run_id());
                fflush(stdout);
            }
        }
    }
    if (ev->state_changed) {
        g_state_advanced = 1;
        ev->acknowledged = 1;
        ev->state = GWY_EVT_GUEST_ACKNOWLEDGED;
        printf("[ROBOTOL_STATE_DIGEST_CHANGED] request_id=%llu before=0x%08X after=0x%08X "
               "run_id=%s evidence=OBSERVED\n",
               (unsigned long long)request_id, ev->state_digest_before, dig,
               platform_event_service_run_id());
        printf("[ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION] request_id=%llu run_id=%s "
               "evidence=OBSERVED\n",
               (unsigned long long)request_id, platform_event_service_run_id());
        fflush(stdout);
    } else {
        ev->state = GWY_EVT_DELIVERED;
    }
    if (ev->mem_writes > 0 || ev->context_touched) {
        printf("[FAMILY_HANDLER_OUTPUT_WRITES_OBSERVED] request_id=%llu writes=%d ctx_touch=%d "
               "run_id=%s evidence=OBSERVED\n",
               (unsigned long long)request_id, ev->mem_writes, ev->context_touched,
               platform_event_service_run_id());
        fflush(stdout);
    }
    printf("[EVENT_DELIVER_LEAVE] request_id=%llu ok=%d ret=%d reads=%d writes=%d "
           "stack_reads=%d ctx_touch=%d state_before=0x%08X state_after=0x%08X changed=%d "
           "run_id=%s evidence=OBSERVED\n",
           (unsigned long long)request_id, ok, (int)ret, ev->mem_reads, ev->mem_writes,
           ev->stack_arg_reads, ev->context_touched, ev->state_digest_before, dig,
           ev->state_changed, platform_event_service_run_id());
    fflush(stdout);
}

void platform_event_service_on_next_timer(void *uc, uint32_t er_rw) {
    int i;
    uint32_t dig;
    uint32_t ui;
    if (!er_rw) return;
    dig = er_digest(uc, er_rw);
    ui = platform_event_service_read_ui_mode(uc, er_rw);
    if (!dig && ui == 0u) return;
    for (i = 0; i < g_ev_n; i++) {
        if (!g_ev[i].delivered || g_ev[i].acknowledged) continue;
        if ((dig != g_ev[i].state_digest_before && g_ev[i].state_digest_before != 0) || ui != 0u) {
            g_ev[i].acknowledged = 1;
            g_ev[i].state_changed = 1;
            g_ev[i].state = GWY_EVT_GUEST_ACKNOWLEDGED;
            g_state_advanced = 1;
            platform_event_service_note_ui_mode(uc, er_rw, g_ev[i].request_id);
            printf("[EVENT_TRANSACTION_GUEST_ACKNOWLEDGED] request_id=%llu ui_mode=0x%X "
                   "run_id=%s evidence=OBSERVED\n",
                   (unsigned long long)g_ev[i].request_id, ui, platform_event_service_run_id());
            printf("[ROBOTOL_STATE_DIGEST_CHANGED] request_id=%llu before=0x%08X after=0x%08X "
                   "phase=next_timer run_id=%s evidence=OBSERVED\n",
                   (unsigned long long)g_ev[i].request_id, g_ev[i].state_digest_before, dig,
                   platform_event_service_run_id());
            printf("[ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION] request_id=%llu run_id=%s "
                   "evidence=OBSERVED\n",
                   (unsigned long long)g_ev[i].request_id, platform_event_service_run_id());
            fflush(stdout);
        }
    }
}

GwyEventIdentityClass platform_event_service_classify_identity(void) {
    int i, family_n = 0;
    int same_hash = 1;
    int same_ctx = 1;
    uint64_t first_hash = 0;
    uint32_t first_ctx = 0;
    int have = 0;

    for (i = 0; i < g_ev_n; i++) {
        if (!g_ev[i].from_guest) continue;
        if (!(g_ev[i].event_code & 0x1E200u) && g_ev[i].event_code != 0x1E209u) {
            /* still count family-looking codes */
        }
        family_n++;
        if (!have) {
            first_hash = g_ev[i].identity_hash;
            first_ctx = g_ev[i].guest_context;
            have = 1;
        } else {
            if (g_ev[i].identity_hash != first_hash) same_hash = 0;
            if (g_ev[i].guest_context != first_ctx) same_ctx = 0;
        }
    }
    if (family_n < 2) {
        g_id_class = GWY_EVT_ID_UNKNOWN;
        return g_id_class;
    }
    if (same_hash && same_ctx) {
        /* Same identity across reissues → unfinished poll, not N independent txs. */
        g_id_class = GWY_EVT_ID_SAME_UNFINISHED;
    } else if (!same_hash) {
        g_id_class = GWY_EVT_ID_NEW_REQUEST;
    } else {
        g_id_class = GWY_EVT_ID_MIXED;
    }
    printf("[EVENT_TRANSACTION_IDENTITY_CONFIRMED] class=%s samples=%d same_hash=%d same_ctx=%d "
           "run_id=%s evidence=OBSERVED\n",
           gwy_event_identity_class_name(g_id_class), family_n, same_hash, same_ctx,
           platform_event_service_run_id());
    fflush(stdout);
    return g_id_class;
}

int platform_event_service_state_advanced(void) { return g_state_advanced; }

static void write_csv(void) {
    char path[512];
    FILE *f;
    int i;

    report_path(path, sizeof(path), "product_ffp_event_requests.csv");
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "run_id,request_id,identity,event,app,ctx,payload,stack0,stack1,r0,r1,r2,r3,r4,r5,"
                "r6,r7,caller_pc,caller_lr,insn,handler,sec_handler,state,digest_before,"
                "digest_after,ctx_before,ctx_after,ret,reads,writes,stack_reads,ctx_touch,"
                "changed,acked\n");
        for (i = 0; i < g_ev_n; i++) {
            GwyPlatformEvent *e = &g_ev[i];
            fprintf(f,
                    "%s,%llu,0x%llX,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,"
                    "0x%X,0x%X,0x%X,0x%X,%llu,0x%X,0x%X,%s,0x%08X,0x%08X,0x%08X,0x%08X,%d,%d,%d,%d,"
                    "%d,%d,%d\n",
                    platform_event_service_run_id(), (unsigned long long)e->request_id,
                    (unsigned long long)e->identity_hash, e->event_code, e->app, e->guest_context,
                    e->guest_payload, e->stack_arg0, e->stack_arg1, e->r[0], e->r[1], e->r[2],
                    e->r[3], e->r[4], e->r[5], e->r[6], e->r[7], e->caller_pc, e->caller_lr,
                    (unsigned long long)e->guest_insn_count, e->handler, e->secondary_handler,
                    gwy_event_state_name(e->state), e->state_digest_before, e->state_digest_after,
                    e->context_digest_before, e->context_digest_after, (int)e->handler_ret,
                    e->mem_reads, e->mem_writes, e->stack_arg_reads, e->context_touched,
                    e->state_changed, e->acknowledged);
        }
        fclose(f);
    }

    report_path(path, sizeof(path), "product_ffp_10165_objects.csv");
    f = fopen(path, "wb");
    if (f) {
        fprintf(f,
                "run_id,plat,ptr,size,handler,owner_store,reads,writes,digest_alloc,digest_latest,"
                "alloc_insn\n");
        for (i = 0; i < g_ctx_n; i++) {
            GwyEventContextObject *c = &g_ctx[i];
            fprintf(f, "%s,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%d,0x%08X,0x%08X,%llu\n",
                    platform_event_service_run_id(), c->plat_code, c->guest_ptr, c->size,
                    c->handler, c->owner_store_addr, c->read_count, c->write_count,
                    c->digest_at_alloc, c->digest_latest, (unsigned long long)c->alloc_insn_count);
        }
        fclose(f);
    }
}

void platform_event_service_finalize(void) {
    GwyEventIdentityClass idc;
    if (g_finalized) return;
    g_finalized = 1;
    idc = platform_event_service_classify_identity();
    write_csv();
    printf("[PES_FINALIZE] events=%d ctx=%d identity=%s state_advanced=%d run_id=%s "
           "evidence=OBSERVED\n",
           g_ev_n, g_ctx_n, gwy_event_identity_class_name(idc), g_state_advanced,
           platform_event_service_run_id());
    fflush(stdout);
}
