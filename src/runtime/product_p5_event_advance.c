#include "gwy_launcher/product_p5_event_advance.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

static char g_run_id[64];
static int g_enabled_known;
static int g_enabled;
static int g_one_shot_known;
static int g_one_shot;
static GwyPlatformEventTransaction g_txn[GWY_P5_MAX_TXN];
static uint32_t g_txn_n;
static uint64_t g_next_id = 1;
static GwyP5FamilyClass g_family_class;
static GwyP5TxnVerdict g_txn_verdict;
static GwyP5Role10165 g_role_10165;
static int g_state_advanced;
static int g_finalized;
static int g_guest_reissue_count;
static int g_host_replay_count;
static int g_dup_completion_count;
static uint32_t g_last_completed_code;
static uint32_t g_last_completed_app;
static uint32_t g_last_completed_state;
static int g_have_completed;
static int g_one_shot_logged;
static uint64_t g_hook_req;
static void *g_hook_uc;
#ifdef GWY_HAVE_UNICORN
static uc_hook g_code_hook;
static int g_code_hook_ok;
#endif

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

int product_p5_enabled(void) {
    if (!g_enabled_known) {
        g_enabled = env1("JJFB_PRODUCT_P5_MODE");
        g_enabled_known = 1;
        if (g_enabled) {
            printf("[P5_MODE] enabled=1 run_id=%s evidence=OBSERVED\n",
                   g_run_id[0] ? g_run_id : "unset");
            fflush(stdout);
        }
    }
    return g_enabled;
}

int product_p5_one_shot_enabled(void) {
    if (!g_one_shot_known) {
        /* Opt-in only — Round A observes duplicates; validate enables one-shot. */
        g_one_shot = env1("JJFB_PRODUCT_P5_ONE_SHOT");
        g_one_shot_known = 1;
    }
    return g_one_shot;
}

void product_p5_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
}

const char *product_p5_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

void product_p5_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_hook_uc && g_code_hook_ok) {
        uc_hook_del((uc_engine *)g_hook_uc, g_code_hook);
        g_code_hook_ok = 0;
    }
#endif
    memset(g_txn, 0, sizeof(g_txn));
    g_txn_n = 0;
    g_next_id = 1;
    g_family_class = GWY_P5_FAMILY_UNKNOWN;
    g_txn_verdict = GWY_P5_TXN_UNKNOWN;
    g_role_10165 = GWY_P5_10165_UNKNOWN;
    g_state_advanced = 0;
    g_finalized = 0;
    g_guest_reissue_count = 0;
    g_host_replay_count = 0;
    g_dup_completion_count = 0;
    g_have_completed = 0;
    g_one_shot_logged = 0;
    g_last_completed_code = 0;
    g_last_completed_app = 0;
    g_last_completed_state = 0;
    g_hook_req = 0;
    g_hook_uc = NULL;
    g_enabled_known = 0;
    g_one_shot_known = 0;
    /* allow finalize log again after reset */
    g_finalized = 0;
}

const char *gwy_p5_family_class_name(GwyP5FamilyClass c) {
    switch (c) {
    case GWY_P5_FAMILY_EVENT_ABI_CONFIRMED: return "FAMILY_EVENT_ABI_CONFIRMED";
    case GWY_P5_FAMILY_EVENT_ABI_INCOMPLETE: return "FAMILY_EVENT_ABI_INCOMPLETE";
    case GWY_P5_FAMILY_EVENT_DUPLICATE_COMPLETION: return "FAMILY_EVENT_DUPLICATE_COMPLETION";
    case GWY_P5_FAMILY_EVENT_ACK_WITHOUT_STATE_CHANGE:
        return "FAMILY_EVENT_ACK_WITHOUT_STATE_CHANGE";
    case GWY_P5_FAMILY_EVENT_REQUIRES_SECOND_STAGE: return "FAMILY_EVENT_REQUIRES_SECOND_STAGE";
    case GWY_P5_FAMILY_EVENT_HANDLER_STATE_CHANGED: return "FAMILY_EVENT_HANDLER_STATE_CHANGED";
    case GWY_P5_FAMILY_EVENT_FALSE_ACK: return "FAMILY_EVENT_FALSE_ACK";
    default: return "FAMILY_EVENT_UNKNOWN";
    }
}

const char *gwy_p5_txn_verdict_name(GwyP5TxnVerdict v) {
    switch (v) {
    case GWY_P5_TXN_ONE_SHOT: return "EVENT_TRANSACTION_ONE_SHOT";
    case GWY_P5_TXN_HOST_REPLAYED: return "HOST_REPLAYED_COMPLETION_FOUND";
    case GWY_P5_TXN_GUEST_REISSUED: return "GUEST_REISSUED_REQUEST";
    case GWY_P5_TXN_LIFETIME_FIXED: return "EVENT_TRANSACTION_LIFETIME_FIXED";
    default: return "EVENT_TRANSACTION_UNKNOWN";
    }
}

const char *gwy_p5_10165_role_name(GwyP5Role10165 r) {
    switch (r) {
    case GWY_P5_10165_QUEUE_DRAIN_CONFIRMED: return "HANDLER_10165_QUEUE_DRAIN_CONFIRMED";
    case GWY_P5_10165_NOT_REQUIRED: return "HANDLER_10165_NOT_REQUIRED";
    case GWY_P5_10165_SECOND_STAGE_MISSING: return "SECOND_STAGE_COMPLETION_MISSING";
    case GWY_P5_10165_ROLE_UNKNOWN: return "HANDLER_10165_ROLE_UNKNOWN";
    default: return "HANDLER_10165_UNKNOWN";
    }
}

uint32_t product_p5_er_rw_digest(void *uc, uint32_t er_rw) {
    uint8_t buf[256];
    uint32_t h = 2166136261u;
    uint32_t i;
    if (!uc || !er_rw) return 0;
    memset(buf, 0, sizeof(buf));
    (void)guest_memory_uc_peek((struct uc_struct *)uc, er_rw, buf, sizeof(buf));
    for (i = 0; i < sizeof(buf); i++) {
        h ^= buf[i];
        h *= 16777619u;
    }
    return h;
}

GwyPlatformEventTransaction *product_p5_txn_by_id(uint64_t request_id) {
    uint32_t i;
    for (i = 0; i < g_txn_n; i++)
        if (g_txn[i].request_id == request_id) return &g_txn[i];
    return NULL;
}

uint64_t product_p5_pending_request_id(uint32_t event_code, uint32_t app) {
    int i;
    /* Prefer most recently accepted undelivered transaction. */
    for (i = (int)g_txn_n - 1; i >= 0; i--) {
        if (g_txn[i].event_code == event_code && g_txn[i].app == app && g_txn[i].accepted &&
            g_txn[i].completion_enqueued && !g_txn[i].completion_delivered)
            return g_txn[i].request_id;
    }
    return 0;
}

static GwyPlatformEventTransaction *find_open_same(uint32_t event_code, uint32_t app) {
    uint32_t i;
    for (i = 0; i < g_txn_n; i++) {
        if (g_txn[i].event_code == event_code && g_txn[i].app == app && g_txn[i].accepted &&
            !g_txn[i].guest_acknowledged)
            return &g_txn[i];
    }
    return NULL;
}

int product_p5_on_guest_request(void *uc, uint32_t event_code, uint32_t app, uint32_t arg2,
                                uint32_t arg3, uint32_t arg4, uint32_t caller_pc, uint32_t caller_lr,
                                const uint32_t r[8], uint32_t r9, uint32_t er_rw,
                                uint64_t owner_module_id, uint64_t owner_generation,
                                uint32_t handler_10102, uint32_t handler_10165) {
    GwyPlatformEventTransaction *t;
    uint32_t digest;
    (void)arg3;
    (void)arg4;
    if (!product_p5_enabled()) return 1; /* P4 path may still run */

    digest = product_p5_er_rw_digest(uc, er_rw);

    /* Guest reissue detection: same code/app after a prior guest request. */
    if (g_have_completed && g_last_completed_code == event_code &&
        g_last_completed_app == app) {
        g_guest_reissue_count++;
        g_txn_verdict = GWY_P5_TXN_GUEST_REISSUED;
        if (product_p5_one_shot_enabled()) {
            /* One completion per request generation until state advances. */
            if (digest == g_last_completed_state) {
                g_dup_completion_count++;
                g_family_class = GWY_P5_FAMILY_EVENT_DUPLICATE_COMPLETION;
                g_txn_verdict = GWY_P5_TXN_LIFETIME_FIXED;
                if (!g_one_shot_logged) {
                    g_one_shot_logged = 1;
                    printf("[PROVEN_PLATFORM_CONTRACT_FIXED] contract=event_transaction_one_shot "
                           "event=0x%X app=0x%X note=guest_reissue_suppressed run_id=%s "
                           "evidence=OBSERVED\n",
                           event_code, app, product_p5_run_id());
                    fflush(stdout);
                }
                printf("[P5_TXN] op=SUPPRESS_DUP event=0x%X app=0x%X digest=0x%08X "
                       "verdict=GUEST_REISSUED_ONE_SHOT run_id=%s evidence=OBSERVED\n",
                       event_code, app, digest, product_p5_run_id());
                fflush(stdout);
                return 0;
            }
            /* State changed — allow a new transaction. */
            g_state_advanced = 1;
        }
    }

    t = find_open_same(event_code, app);
    if (t && t->completion_enqueued && !t->completion_delivered) {
        /* Still pending delivery — do not enqueue another. */
        g_dup_completion_count++;
        return 0;
    }
    if (t && t->completion_delivered && !t->guest_acknowledged && product_p5_one_shot_enabled()) {
        g_dup_completion_count++;
        g_family_class = GWY_P5_FAMILY_EVENT_ACK_WITHOUT_STATE_CHANGE;
        return 0;
    }

    if (g_txn_n >= GWY_P5_MAX_TXN) return 0;
    t = &g_txn[g_txn_n++];
    memset(t, 0, sizeof(*t));
    t->request_id = g_next_id++;
    t->owner_module_id = owner_module_id;
    t->owner_generation = owner_generation;
    t->event_code = event_code;
    t->app = app;
    t->payload = arg2;
    t->caller_pc = caller_pc;
    t->caller_lr = caller_lr;
    if (r) memcpy(t->r, r, sizeof(t->r));
    t->r9 = r9;
    t->state_before = digest;
    t->handler_10102 = handler_10102;
    t->handler_10165 = handler_10165;
    t->accepted = 1;
    t->from_guest_sendappevent = 1;
    t->completion_enqueued = 1;
    snprintf(t->phase, sizeof(t->phase), "%s", "guest_request");

    printf("[P5_TXN] op=ACCEPT request_id=%llu event=0x%X app=0x%X payload=0x%X "
           "caller_pc=0x%X state=0x%08X handler=0x%X run_id=%s evidence=OBSERVED\n",
           (unsigned long long)t->request_id, event_code, app, arg2, caller_pc, digest,
           handler_10102, product_p5_run_id());
    fflush(stdout);
    return 1;
}

#ifdef GWY_HAVE_UNICORN
static void p5_hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    GwyPlatformEventTransaction *t = product_p5_txn_by_id(g_hook_req);
    uint8_t bytes[4];
    uint16_t h0;
    uint32_t r[13], sp = 0;
    (void)user_data;
    (void)size;
    if (!t) return;
    /* Count LDR-ish / pointer reads heuristically via Thumb LDR immediate. */
    memset(bytes, 0, sizeof(bytes));
    (void)guest_memory_uc_peek((struct uc_struct *)uc, (uint32_t)address, bytes, 2);
    h0 = (uint16_t)(bytes[0] | (bytes[1] << 8));
    if ((h0 & 0xF800u) == 0x6800u || (h0 & 0xF800u) == 0x4800u) t->pointer_reads++;
    if ((h0 & 0xF800u) == 0x6000u || (h0 & 0xF800u) == 0x7000u) {
        int i;
        for (i = 0; i < 13; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &r[i]);
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        t->guest_writes++;
        (void)sp;
    }
}
#endif

void product_p5_on_handler_enter(void *uc, uint64_t request_id, uint32_t handler, uint32_t r0,
                                 uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9) {
    GwyPlatformEventTransaction *t = product_p5_txn_by_id(request_id);
    if (!product_p5_enabled() || !t) return;
    t->r[0] = r0;
    t->r[1] = r1;
    t->r[2] = r2;
    t->r[3] = r3;
    t->r9 = r9;
    snprintf(t->phase, sizeof(t->phase), "%s", "handler_10102");
    printf("[P5_FAMILY_ENTER] request_id=%llu handler=0x%X R0=0x%X R1=0x%X R2=0x%X R3=0x%X "
           "R9=0x%X run_id=%s evidence=OBSERVED\n",
           (unsigned long long)request_id, handler, r0, r1, r2, r3, r9, product_p5_run_id());
    fflush(stdout);

#ifdef GWY_HAVE_UNICORN
    if (uc && handler) {
        uint32_t base = handler & ~1u;
        uc_err e;
        g_hook_req = request_id;
        g_hook_uc = uc;
        if (g_code_hook_ok) {
            uc_hook_del((uc_engine *)uc, g_code_hook);
            g_code_hook_ok = 0;
        }
        e = uc_hook_add((uc_engine *)uc, &g_code_hook, UC_HOOK_CODE, (void *)p5_hook_code, NULL,
                        base, (uint64_t)base + 0x800ull);
        g_code_hook_ok = (e == UC_ERR_OK);
    }
#else
    (void)uc;
    (void)handler;
#endif
}

void product_p5_note_handler_mem(uint64_t request_id, int is_write, uint32_t addr, uint32_t size,
                                 uint32_t value) {
    GwyPlatformEventTransaction *t = product_p5_txn_by_id(request_id);
    (void)addr;
    (void)size;
    (void)value;
    if (!t) return;
    if (is_write)
        t->guest_writes++;
    else
        t->pointer_reads++;
}

void product_p5_on_handler_leave(void *uc, uint64_t request_id, int ok, int32_t ret,
                                 uint32_t er_rw) {
    GwyPlatformEventTransaction *t = product_p5_txn_by_id(request_id);
    uint32_t digest;
    if (!product_p5_enabled() || !t) return;

#ifdef GWY_HAVE_UNICORN
    if (g_hook_uc && g_code_hook_ok) {
        uc_hook_del((uc_engine *)g_hook_uc, g_code_hook);
        g_code_hook_ok = 0;
    }
#else
    (void)ok;
#endif

    digest = product_p5_er_rw_digest(uc, er_rw);
    t->state_after_handler = digest;
    t->handler_ret = ret;
    t->completion_delivered = 1;
    t->state_changed = (digest != t->state_before) ? 1 : 0;

    if (t->state_changed) {
        g_family_class = GWY_P5_FAMILY_EVENT_HANDLER_STATE_CHANGED;
        g_state_advanced = 1;
        t->guest_acknowledged = 1;
    } else if (ret == 0 && !t->state_changed) {
        /* ret=0 alone is not ABI proof. Case bodies often return 0 after work. */
        g_family_class = GWY_P5_FAMILY_EVENT_ACK_WITHOUT_STATE_CHANGE;
        if (t->handler_10165 && !t->reached_10165)
            g_role_10165 = GWY_P5_10165_SECOND_STAGE_MISSING;
        /* Null payload with unchanged ER_RW → incomplete contract, not mere ack. */
        if (t->r[2] == 0 && t->payload == 0)
            g_family_class = GWY_P5_FAMILY_EVENT_ABI_INCOMPLETE;
        if (t->pointer_reads == 0 && t->guest_writes == 0 && t->r[2] == 0)
            g_family_class = GWY_P5_FAMILY_EVENT_FALSE_ACK;
    }

    g_have_completed = 1;
    g_last_completed_code = t->event_code;
    g_last_completed_app = t->app;
    g_last_completed_state = digest;

    if (product_p5_one_shot_enabled()) g_txn_verdict = GWY_P5_TXN_LIFETIME_FIXED;

    printf("[P5_FAMILY_LEAVE] request_id=%llu ok=%d ret=%d writes=%d reads=%d "
           "state_before=0x%08X state_after=0x%08X changed=%d class=%s run_id=%s "
           "evidence=OBSERVED\n",
           (unsigned long long)request_id, ok, (int)ret, t->guest_writes, t->pointer_reads,
           t->state_before, digest, t->state_changed, gwy_p5_family_class_name(g_family_class),
           product_p5_run_id());
    fflush(stdout);
}

void product_p5_on_10165(void *uc, uint64_t request_id, uint32_t handler, int ok, int32_t ret,
                         uint32_t er_rw) {
    GwyPlatformEventTransaction *t = product_p5_txn_by_id(request_id);
    if (!product_p5_enabled() || !t) return;
    t->reached_10165 = 1;
    t->handler_10165 = handler;
    t->handler_10165_ret = ret;
    t->state_after_10165 = product_p5_er_rw_digest(uc, er_rw);
    if (t->state_after_10165 != t->state_after_handler) {
        g_role_10165 = GWY_P5_10165_QUEUE_DRAIN_CONFIRMED;
        g_state_advanced = 1;
        t->state_changed = 1;
        t->guest_acknowledged = 1;
    } else if (ok) {
        g_role_10165 = GWY_P5_10165_ROLE_UNKNOWN;
    }
    printf("[P5_10165] request_id=%llu handler=0x%X ok=%d ret=%d state=0x%08X role=%s "
           "run_id=%s evidence=OBSERVED\n",
           (unsigned long long)request_id, handler, ok, (int)ret, t->state_after_10165,
           gwy_p5_10165_role_name(g_role_10165), product_p5_run_id());
    fflush(stdout);
}

void product_p5_on_next_timer(void *uc, uint32_t er_rw) {
    uint32_t i;
    uint32_t dig;
    if (!product_p5_enabled()) return;
    /* Ignore transient r9=0 snapshots — that is frame restore, not guest progress. */
    if (!er_rw) return;
    dig = product_p5_er_rw_digest(uc, er_rw);
    if (!dig) return;
    for (i = 0; i < g_txn_n; i++) {
        if (g_txn[i].completion_delivered && !g_txn[i].state_next_timer) {
            g_txn[i].state_next_timer = dig;
            if (dig != g_txn[i].state_before && g_txn[i].state_before != 0) {
                g_state_advanced = 1;
                g_txn[i].guest_acknowledged = 1;
                printf("[ROBOTOL_STATE_DIGEST_CHANGED] request_id=%llu before=0x%08X after=0x%08X "
                       "run_id=%s evidence=OBSERVED\n",
                       (unsigned long long)g_txn[i].request_id, g_txn[i].state_before, dig,
                       product_p5_run_id());
                fflush(stdout);
            }
        }
    }
}

static void write_csvs(void) {
    char path[512];
    FILE *f;
    uint32_t i;

    report_path(path, sizeof(path), "product_p5_event_transaction.csv");
    f = fopen(path, "w");
    if (f) {
        fprintf(f,
                "run_id,request_id,event_code,app,payload,caller_pc,caller_lr,r0,r1,r2,r3,r9,"
                "owner_module_id,owner_generation,accepted,enqueued,delivered,acked,"
                "from_guest,host_replay,handler_ret,state_before,state_after_handler,"
                "state_next_timer,writes,reads,reached_10165,state_changed\n");
        for (i = 0; i < g_txn_n; i++) {
            GwyPlatformEventTransaction *t = &g_txn[i];
            fprintf(f,
                    "%s,%llu,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%llu,%llu,%d,%d,"
                    "%d,%d,%d,%d,%d,0x%08X,0x%08X,0x%08X,%d,%d,%d,%d\n",
                    product_p5_run_id(), (unsigned long long)t->request_id, t->event_code, t->app,
                    t->payload, t->caller_pc, t->caller_lr, t->r[0], t->r[1], t->r[2], t->r[3],
                    t->r9, (unsigned long long)t->owner_module_id,
                    (unsigned long long)t->owner_generation, t->accepted, t->completion_enqueued,
                    t->completion_delivered, t->guest_acknowledged, t->from_guest_sendappevent,
                    t->host_replay, (int)t->handler_ret, t->state_before, t->state_after_handler,
                    t->state_next_timer, t->guest_writes, t->pointer_reads, t->reached_10165,
                    t->state_changed);
        }
        fclose(f);
    }

    report_path(path, sizeof(path), "product_p5_family_handler_state.csv");
    f = fopen(path, "w");
    if (f) {
        fprintf(f,
                "run_id,request_id,handler,r0,r1,r2,r3,ret,pointer_reads,guest_writes,"
                "state_before,state_after,class\n");
        for (i = 0; i < g_txn_n; i++) {
            GwyPlatformEventTransaction *t = &g_txn[i];
            if (!t->completion_delivered) continue;
            fprintf(f, "%s,%llu,0x%X,0x%X,0x%X,0x%X,0x%X,%d,%d,%d,0x%08X,0x%08X,%s\n",
                    product_p5_run_id(), (unsigned long long)t->request_id, t->handler_10102,
                    t->r[0], t->r[1], t->r[2], t->r[3], (int)t->handler_ret, t->pointer_reads,
                    t->guest_writes, t->state_before, t->state_after_handler,
                    gwy_p5_family_class_name(g_family_class));
        }
        fclose(f);
    }

    report_path(path, sizeof(path), "product_p5_10165_provenance.csv");
    f = fopen(path, "w");
    if (f) {
        fprintf(f,
                "run_id,request_id,handler_10165,reached,ret,state_after,role,evidence\n");
        for (i = 0; i < g_txn_n; i++) {
            GwyPlatformEventTransaction *t = &g_txn[i];
            fprintf(f, "%s,%llu,0x%X,%d,%d,0x%08X,%s,%s\n", product_p5_run_id(),
                    (unsigned long long)t->request_id, t->handler_10165, t->reached_10165,
                    (int)t->handler_10165_ret, t->state_after_10165,
                    gwy_p5_10165_role_name(g_role_10165),
                    t->reached_10165 ? "DELIVERED" : "NOT_REACHED");
        }
        if (g_txn_n == 0)
            fprintf(f, "%s,0,0x0,0,0,0x0,%s,NONE\n", product_p5_run_id(),
                    gwy_p5_10165_role_name(g_role_10165));
        fclose(f);
    }

    report_path(path, sizeof(path), "product_p5_post_completion_progress.csv");
    f = fopen(path, "w");
    if (f) {
        fprintf(f,
                "run_id,request_id,state_before,state_after_handler,state_next_timer,"
                "signature_changed,state_advanced,family_class,txn_verdict,role_10165\n");
        for (i = 0; i < g_txn_n; i++) {
            GwyPlatformEventTransaction *t = &g_txn[i];
            fprintf(f, "%s,%llu,0x%08X,0x%08X,0x%08X,%d,%d,%s,%s,%s\n", product_p5_run_id(),
                    (unsigned long long)t->request_id, t->state_before, t->state_after_handler,
                    t->state_next_timer, t->state_changed, g_state_advanced,
                    gwy_p5_family_class_name(g_family_class), gwy_p5_txn_verdict_name(g_txn_verdict),
                    gwy_p5_10165_role_name(g_role_10165));
        }
        fclose(f);
    }
}

void product_p5_finalize(void) {
    if (!product_p5_enabled()) return;

    if (g_guest_reissue_count > 0 && g_txn_verdict == GWY_P5_TXN_UNKNOWN)
        g_txn_verdict = GWY_P5_TXN_GUEST_REISSUED;
    if (g_host_replay_count > 0) g_txn_verdict = GWY_P5_TXN_HOST_REPLAYED;
    if (product_p5_one_shot_enabled() && g_dup_completion_count > 0)
        g_txn_verdict = GWY_P5_TXN_LIFETIME_FIXED;
    if (g_role_10165 == GWY_P5_10165_UNKNOWN) {
        /* Registered but never naturally reached after family handler. */
        uint32_t i;
        int any_10165 = 0, any_reg = 0;
        for (i = 0; i < g_txn_n; i++) {
            if (g_txn[i].handler_10165) any_reg = 1;
            if (g_txn[i].reached_10165) any_10165 = 1;
        }
        if (any_reg && !any_10165) {
            /* 10165 was registered via PLATFORM_ALLOC (enqueue stub), not proven as
             * a required post-10102 drain. Keep SECOND_STAGE_MISSING only as a
             * hypothesis marker when family ack did not advance state. */
            if (g_family_class == GWY_P5_FAMILY_EVENT_FALSE_ACK ||
                g_family_class == GWY_P5_FAMILY_EVENT_ABI_INCOMPLETE ||
                g_family_class == GWY_P5_FAMILY_EVENT_ACK_WITHOUT_STATE_CHANGE)
                g_role_10165 = GWY_P5_10165_SECOND_STAGE_MISSING;
            else
                g_role_10165 = GWY_P5_10165_ROLE_UNKNOWN;
        } else if (!any_reg)
            g_role_10165 = GWY_P5_10165_NOT_REQUIRED;
        else
            g_role_10165 = GWY_P5_10165_ROLE_UNKNOWN;
    }

    write_csvs();
    if (!g_finalized) {
        g_finalized = 1;
        printf("[P5_FINALIZE] txns=%u family=%s txn=%s role10165=%s state_advanced=%d "
               "guest_reissue=%d dup_suppress=%d run_id=%s evidence=OBSERVED\n",
               g_txn_n, gwy_p5_family_class_name(g_family_class),
               gwy_p5_txn_verdict_name(g_txn_verdict), gwy_p5_10165_role_name(g_role_10165),
               g_state_advanced, g_guest_reissue_count, g_dup_completion_count,
               product_p5_run_id());
        if (g_state_advanced)
            printf("[ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION] run_id=%s evidence=OBSERVED\n",
                   product_p5_run_id());
        fflush(stdout);
    }
}

GwyP5FamilyClass product_p5_family_class(void) { return g_family_class; }
GwyP5TxnVerdict product_p5_txn_verdict(void) { return g_txn_verdict; }
GwyP5Role10165 product_p5_10165_role(void) { return g_role_10165; }
int product_p5_state_advanced(void) { return g_state_advanced; }
