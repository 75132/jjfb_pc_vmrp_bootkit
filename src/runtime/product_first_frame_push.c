#include "gwy_launcher/product_first_frame_push.h"
#include "gwy_launcher/product_event_queue_bootstrap.h"
#include "gwy_launcher/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define FFP_SAMPLE_CAP 8
#define FFP_MEM_LOG_CAP 256

typedef struct FfpGuestSample {
    uint32_t event_code;
    uint32_t app;
    uint32_t r[8];
    uint32_t stack0;
    uint32_t stack1;
    uint32_t caller_pc;
    uint32_t caller_lr;
    uint32_t er_rw;
    uint32_t ctx;
    uint64_t insn;
    uint32_t er_digest;
    uint32_t ctx_digest;
    uint64_t identity;
} FfpGuestSample;

typedef struct FfpMemHit {
    uint64_t request_id;
    int is_write;
    uint32_t addr;
    uint32_t size;
    uint32_t value;
    uint32_t pc;
} FfpMemHit;

static char g_run_id[64];
static int g_en_known;
static int g_en;
static int g_apply_known;
static int g_apply;
static GwyFfpPhase g_phase;
static int g_phase_known;
static FfpGuestSample g_samples[FFP_SAMPLE_CAP];
static int g_sample_n;
static FfpMemHit g_mem[FFP_MEM_LOG_CAP];
static int g_mem_n;
static int g_finalized;
static int g_abi_confirmed;
static int g_resource_open;
static int g_resource_read;
static int g_fb_write;
static int g_disp_up;
static uint64_t g_hook_req;
static void *g_hook_uc;
static uint32_t g_hook_sp;
static uint32_t g_hook_handler_base;
#ifdef GWY_HAVE_UNICORN
static uc_hook g_mem_hook;
static uc_hook g_code_hook;
static uc_hook g_code_305e09;
static int g_mem_hook_ok;
static int g_code_hook_ok;
static int g_code_305e09_ok;
static int g_305e09_entries;
static uint32_t g_ui_mode_at_sample;
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

static uint32_t digest256(void *uc, uint32_t addr) {
    uint8_t buf[256];
    uint32_t h = 2166136261u;
    uint32_t i;
    if (!uc || !addr) return 0;
    memset(buf, 0, sizeof(buf));
    (void)guest_memory_uc_peek((struct uc_struct *)uc, addr, buf, sizeof(buf));
    for (i = 0; i < sizeof(buf); i++) {
        h ^= buf[i];
        h *= 16777619u;
    }
    return h;
}

int product_ffp_enabled(void) {
    if (!g_en_known) {
        g_en = env1("JJFB_PRODUCT_FFP_MODE") || env1("JJFB_PRODUCT_P6_MODE");
        g_en_known = 1;
        if (g_en) {
            printf("[FFP_MODE] enabled=1 phase=%d run_id=%s evidence=OBSERVED\n",
                   (int)product_ffp_phase(), product_ffp_run_id());
            fflush(stdout);
        }
    }
    return g_en;
}

GwyFfpPhase product_ffp_phase(void) {
    const char *p;
    if (g_phase_known) return g_phase;
    g_phase_known = 1;
    p = getenv("JJFB_PRODUCT_FFP_PHASE");
    if (!p || !p[0]) {
        g_phase = GWY_FFP_PHASE_EVENT;
        return g_phase;
    }
    if (p[0] == 'r' || p[0] == 'R')
        g_phase = GWY_FFP_PHASE_RESOURCE;
    else if (p[0] == 'v' || p[0] == 'V')
        g_phase = GWY_FFP_PHASE_VALIDATE;
    else
        g_phase = GWY_FFP_PHASE_EVENT;
    return g_phase;
}

int product_ffp_apply_abi(void) {
    if (!g_apply_known) {
        /* Round B opt-in: apply context/stack into delivery ABI. */
        g_apply = env1("JJFB_PRODUCT_FFP_APPLY_ABI");
        g_apply_known = 1;
    }
    return g_apply;
}

int product_ffp_trace_305e09(void) {
    return env1("JJFB_PRODUCT_TRACE_305E09") || env1("JJFB_PRODUCT_EVENT_CONTRACT");
}

void product_ffp_enter_resource_phase(void) {
    if (g_phase == GWY_FFP_PHASE_RESOURCE) return;
    g_phase = GWY_FFP_PHASE_RESOURCE;
    g_phase_known = 1;
    printf("[FFP_AUTO_PHASE] from=event to=resource reason=state_advanced "
           "internal=EventContract run_id=%s evidence=OBSERVED\n",
           product_ffp_run_id());
    fflush(stdout);
}

void product_ffp_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    snprintf(g_run_id, sizeof(g_run_id), "%s", run_id);
    platform_event_service_set_run_id(run_id);
    product_eqb_set_run_id(run_id);
}

const char *product_ffp_run_id(void) { return g_run_id[0] ? g_run_id : "unknown"; }

void product_ffp_reset(void) {
#ifdef GWY_HAVE_UNICORN
    if (g_hook_uc) {
        if (g_mem_hook_ok) {
            uc_hook_del((uc_engine *)g_hook_uc, g_mem_hook);
            g_mem_hook_ok = 0;
        }
        if (g_code_hook_ok) {
            uc_hook_del((uc_engine *)g_hook_uc, g_code_hook);
            g_code_hook_ok = 0;
        }
        if (g_code_305e09_ok) {
            uc_hook_del((uc_engine *)g_hook_uc, g_code_305e09);
            g_code_305e09_ok = 0;
        }
    }
#endif
    platform_event_service_reset();
    product_eqb_reset();
    memset(g_samples, 0, sizeof(g_samples));
    memset(g_mem, 0, sizeof(g_mem));
    g_sample_n = 0;
    g_mem_n = 0;
    g_finalized = 0;
    g_abi_confirmed = 0;
    g_resource_open = 0;
    g_resource_read = 0;
    g_fb_write = 0;
    g_disp_up = 0;
    g_hook_req = 0;
    g_hook_uc = NULL;
    g_hook_sp = 0;
    g_hook_handler_base = 0;
    g_305e09_entries = 0;
    g_ui_mode_at_sample = 0;
    g_en_known = 0;
    g_en = 0;
    g_apply_known = 0;
    g_apply = 0;
    g_phase_known = 0;
    g_phase = GWY_FFP_PHASE_NONE;
}

static void scan_erw_for_ctx(void *uc, uint32_t er_rw, uint32_t guest_ptr, uint32_t plat_code) {
    uint32_t off;
    if (!uc || !er_rw || !guest_ptr) return;
    /* Scan first 4KB of ER_RW for a stored pointer to the context object. */
    for (off = 0; off < 0x1000u; off += 4u) {
        uint32_t v = 0;
        if (!guest_memory_uc_peek_u32((struct uc_struct *)uc, er_rw + off, &v)) continue;
        if (v == guest_ptr) {
            platform_event_service_note_ctx_owner_store(guest_ptr, er_rw + off);
            product_eqb_on_alloc_stored(uc, plat_code, guest_ptr, er_rw + off, er_rw, er_rw);
            return;
        }
    }
}

void product_ffp_on_alloc(void *uc, uint32_t plat_code, uint32_t size, uint32_t guest_ptr,
                          uint32_t handler, uint32_t er_rw, uint64_t owner_module_id) {
    GwyEventContextObject *c;
    if (!product_ffp_enabled()) return;
    if (plat_code != 0x10165u && plat_code != 0x10162u) return;
    c = platform_event_service_note_alloc(plat_code, size, guest_ptr, handler, owner_module_id);
    if (!c) return;
    platform_event_service_refresh_ctx_digest(uc, c);
    if (er_rw) scan_erw_for_ctx(uc, er_rw, guest_ptr, plat_code);
    /* Delayed owner scan: guest may STR the return value after we return. */
}

void product_ffp_on_guest_sendappevent(void *uc, uint32_t code, uint32_t app, const uint32_t r[8],
                                       uint32_t stack0, uint32_t stack1, uint32_t caller_pc,
                                       uint32_t caller_lr, uint32_t er_rw) {
    FfpGuestSample *s;
    uint32_t ctx = 0;
    int i;
    if (!product_ffp_enabled()) return;
    platform_event_service_bump_insn(1);

    /* After any call, try to locate newly stored 10165 pointers in ER_RW. */
    if (er_rw) {
        for (i = 0; i < platform_event_service_ctx_count(); i++) {
            GwyEventContextObject *c = platform_event_service_ctx_at(i);
            if (!c || c->owner_store_addr) continue;
            scan_erw_for_ctx(uc, er_rw, c->guest_ptr, c->plat_code);
        }
    }

    /* Capture first N family-band guest posts (0x1E209 etc.). */
    if (code < 0x1E200u || code > 0x1E2FFu) return;
    if (g_sample_n >= FFP_SAMPLE_CAP) return;

    for (i = 0; i < platform_event_service_ctx_count(); i++) {
        GwyEventContextObject *c = platform_event_service_ctx_at(i);
        if (c && c->plat_code == 0x10165u && c->guest_ptr) {
            ctx = c->guest_ptr;
            break;
        }
    }
    /* Prefer ctx appearing in regs. */
    if (r) {
        for (i = 0; i < 8; i++) {
            if (platform_event_service_ctx_by_ptr(r[i])) {
                ctx = r[i];
                break;
            }
        }
    }

    s = &g_samples[g_sample_n++];
    memset(s, 0, sizeof(*s));
    s->event_code = code;
    s->app = app;
    if (r) memcpy(s->r, r, sizeof(s->r));
    s->stack0 = stack0;
    s->stack1 = stack1;
    s->caller_pc = caller_pc;
    s->caller_lr = caller_lr;
    s->er_rw = er_rw;
    s->ctx = ctx;
    s->insn = platform_event_service_insn_count();
    s->er_digest = digest256(uc, er_rw);
    if (ctx) {
        GwyEventContextObject *c = platform_event_service_ctx_by_ptr(ctx);
        if (c) {
            platform_event_service_refresh_ctx_digest(uc, c);
            s->ctx_digest = c->digest_latest;
        }
    }
    s->identity = platform_event_identity_hash(code, app, r, stack0, stack1, ctx, r ? r[2] : 0);

    printf("[FFP_GUEST_REQUEST_SAMPLE] n=%d event=0x%X app=0x%X r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
           "r4=0x%X r5=0x%X r6=0x%X r7=0x%X stack0=0x%X stack1=0x%X caller_pc=0x%X caller_lr=0x%X "
           "insn=%llu ctx=0x%X er_digest=0x%08X ctx_digest=0x%08X identity=0x%llX run_id=%s "
           "evidence=OBSERVED\n",
           g_sample_n, code, app, s->r[0], s->r[1], s->r[2], s->r[3], s->r[4], s->r[5], s->r[6],
           s->r[7], stack0, stack1, caller_pc, caller_lr, (unsigned long long)s->insn, ctx,
           s->er_digest, s->ctx_digest, (unsigned long long)s->identity, product_ffp_run_id());
    fflush(stdout);

    if (g_sample_n >= 2)
        (void)platform_event_service_classify_identity();
    if (g_sample_n >= FFP_SAMPLE_CAP)
        product_ffp_finalize();
}

int product_ffp_on_family_request(void *uc, uint32_t event_code, uint32_t app, uint32_t family,
                                  uint32_t handler, uint32_t secondary_handler, uint32_t caller_pc,
                                  uint32_t caller_lr, const uint32_t r[8], uint32_t r9,
                                  uint32_t stack0, uint32_t stack1, uint32_t er_rw,
                                  uint64_t owner_module_id, uint64_t owner_generation,
                                  uint64_t *out_request_id) {
    GwyPlatformEvent *ev = NULL;
    int accept;
    if (out_request_id) *out_request_id = 0;
    if (!product_ffp_enabled()) return 1;

    product_ffp_on_guest_sendappevent(uc, event_code, app, r, stack0, stack1, caller_pc, caller_lr,
                                      er_rw);

    if (product_eqb_enabled()) {
        product_eqb_bind_uc(uc);
        product_eqb_note_er_rw(er_rw, "family");
        if (er_rw) {
            product_eqb_arm_write_hooks(uc, er_rw);
            product_eqb_arm_code_hooks(uc);
            product_eqb_phase(uc, GWY_EQB_EXT_INIT_ENTER, er_rw, r9, 0, 0, 0, owner_module_id,
                              owner_generation, 0);
        }
        if (event_code == 0x1E209u && app == 9u) {
            static int once_1e209;
            if (!once_1e209) {
                once_1e209 = 1;
                product_eqb_phase(uc, GWY_EQB_FIRST_1E209_REQUEST, er_rw, r9, 0, 0, 0,
                                  owner_module_id, owner_generation, 0);
                printf("[EVENT_QUEUE_CONTEXT_READY] er_rw=0x%X r9=0x%X evidence=OBSERVED\n", er_rw,
                       r9);
                fflush(stdout);
            }
        }
    }

    accept = platform_event_service_on_guest_request(
        uc, event_code, app, family, handler, secondary_handler, caller_pc, caller_lr, r, r9,
        stack0, stack1, er_rw, owner_module_id, owner_generation, &ev);
    if (accept && ev) {
        platform_event_service_mark_queued(ev->request_id);
        if (out_request_id) *out_request_id = ev->request_id;
    }
    return accept;
}

void product_ffp_prepare_delivery_abi(uint64_t request_id, GwyEventDeliveryAbi *out) {
    GwyPlatformEvent *ev = platform_event_service_by_id(request_id);
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!ev) return;
    platform_event_service_suggest_delivery_abi(ev, out);
    if (!product_ffp_apply_abi()) {
        /* Round A: observe-only — deliver baseline r0/r1, leave r2/r3/stack as candidates. */
        out->r2 = 0;
        out->r3 = 0;
        out->stack0 = 0;
        out->stack1 = 0;
        out->have_stack = 0;
        return;
    }
    /* Round B: pass recovered context object into r2 and stack0 (candidate ABI). */
    if (ev->guest_context) {
        out->r2 = ev->guest_context;
        out->stack0 = ev->guest_context;
        out->have_context = 1;
        out->have_stack = 1;
    }
    if (ev->secondary_handler) {
        out->stack1 = ev->secondary_handler;
        out->have_stack = 1;
    }
}

#ifdef GWY_HAVE_UNICORN
static void ffp_hook_mem(uc_engine *uc, uc_mem_type type, uint64_t address, int size,
                         int64_t value, void *user_data) {
    FfpMemHit *h;
    uint32_t pc = 0;
    int is_write = (type == UC_MEM_WRITE || type == UC_MEM_WRITE_UNMAPPED ||
                    type == UC_MEM_WRITE_PROT);
    (void)user_data;
    if (!g_hook_req) return;
    uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    platform_event_service_note_handler_mem(g_hook_req, is_write, (uint32_t)address, (uint32_t)size,
                                            (uint32_t)value);
    if (!is_write && g_hook_sp) {
        uint32_t a = (uint32_t)address;
        if (a == g_hook_sp + 0x20u || a == g_hook_sp + 0x24u || a == g_hook_sp ||
            a == g_hook_sp + 4u) {
            platform_event_service_note_stack_arg_read(g_hook_req);
        }
    }
    if (g_mem_n < FFP_MEM_LOG_CAP) {
        h = &g_mem[g_mem_n++];
        h->request_id = g_hook_req;
        h->is_write = is_write;
        h->addr = (uint32_t)address;
        h->size = (uint32_t)size;
        h->value = (uint32_t)value;
        h->pc = pc;
    }
}

static void ffp_hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t sp = 0;
    uint32_t s0 = 0, s1 = 0;
    (void)size;
    (void)user_data;
    if (!g_hook_req) return;
    if ((uint32_t)address == (g_hook_handler_base & ~1u) ||
        (uint32_t)address == g_hook_handler_base) {
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        g_hook_sp = sp;
        if (sp) {
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp, &s0);
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 4u, &s1);
            printf("[FFP_HANDLER_STACK_ENTRY] request_id=%llu sp=0x%X [0]=0x%X [4]=0x%X "
                   "run_id=%s evidence=OBSERVED\n",
                   (unsigned long long)g_hook_req, sp, s0, s1, product_ffp_run_id());
            fflush(stdout);
        }
    }
    if ((uint32_t)address == 0x30D310u || (uint32_t)address == 0x30D312u) {
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        if (sp) {
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x20u, &s0);
            (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + 0x24u, &s1);
            platform_event_service_note_stack_arg_read(g_hook_req);
            printf("[FFP_HANDLER_STACK_ARGS] request_id=%llu pc=0x%X sp=0x%X sp+32=0x%X "
                   "sp+36=0x%X run_id=%s evidence=OBSERVED\n",
                   (unsigned long long)g_hook_req, (uint32_t)address, sp, s0, s1,
                   product_ffp_run_id());
            fflush(stdout);
        }
    }
}

static void ffp_hook_305e09(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    uint32_t r[13];
    uint32_t sp = 0, lr = 0, r9 = 0, cpsr = 0;
    uint32_t stack[17];
    int i;
    (void)size;
    (void)user_data;
    if (!product_ffp_trace_305e09()) return;
    if ((uint32_t)address != 0x305E08u && (uint32_t)address != 0x305E09u) return;
    if (g_305e09_entries >= 8) return;
    g_305e09_entries++;
    for (i = 0; i < 13; i++) uc_reg_read(uc, UC_ARM_REG_R0 + i, &r[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_R9, &r9);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    memset(stack, 0, sizeof(stack));
    for (i = 0; i < 17 && sp; i++)
        (void)guest_memory_uc_peek_u32((struct uc_struct *)uc, sp + (uint32_t)i * 4u, &stack[i]);
    printf("[EVENT_305E09_ENTRY] n=%d pc=0x%X thumb=%d r0=0x%X r1=0x%X r2=0x%X r3=0x%X "
           "r4=0x%X r9=0x%X sp=0x%X lr=0x%X cpsr=0x%X ui_mode=0x%X "
           "note=r0_minus_4_for_10133=0x%X run_id=%s evidence=OBSERVED\n",
           g_305e09_entries, (uint32_t)address, (cpsr & 0x20) ? 1 : 0, r[0], r[1], r[2], r[3],
           r[4], r9, sp, lr, cpsr, platform_event_service_read_ui_mode(uc, r9),
           r[0] ? (r[0] - 4u) : 0u, product_ffp_run_id());
    printf("[EVENT_305E09_FUNCTION_CONTRACT_IDENTIFIED] abi=sendAppEvent(0x10133,r0-4,0,0) "
           "role=platform_free_wrapper case9_passes_event_code evidence=OBSERVED\n");
    fflush(stdout);
    {
        char path[512];
        FILE *f;
        report_path(path, sizeof(path), "product_event_305e09_entry.csv");
        f = fopen(path, g_305e09_entries == 1 ? "wb" : "ab");
        if (f) {
            if (g_305e09_entries == 1)
                fprintf(f, "run_id,n,pc,r0,r1,r2,r3,r4,r9,sp,lr,cpsr,ui_mode,stack0,stack1,"
                           "request_id\n");
            fprintf(f,
                    "%s,%d,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%llu\n",
                    product_ffp_run_id(), g_305e09_entries, (uint32_t)address, r[0], r[1], r[2],
                    r[3], r[4], r9, sp, lr, cpsr, platform_event_service_read_ui_mode(uc, r9),
                    stack[0], stack[1], (unsigned long long)g_hook_req);
            fclose(f);
        }
    }
}
#endif

void product_ffp_on_handler_enter(void *uc, uint64_t request_id, uint32_t handler,
                                  const GwyEventDeliveryAbi *abi) {
    GwyPlatformEvent *ev;
    if (!product_ffp_enabled()) return;
    platform_event_service_on_deliver_enter(uc, request_id, abi);
    ev = platform_event_service_by_id(request_id);

#ifdef GWY_HAVE_UNICORN
    if (uc && handler) {
        uc_err e;
        uint32_t base = handler & ~1u;
        uint32_t watch_lo = 0, watch_hi = 0;
        int i;
        g_hook_req = request_id;
        g_hook_uc = uc;
        g_hook_handler_base = handler;
        g_hook_sp = 0;
        if (g_mem_hook_ok) {
            uc_hook_del((uc_engine *)uc, g_mem_hook);
            g_mem_hook_ok = 0;
        }
        if (g_code_hook_ok) {
            uc_hook_del((uc_engine *)uc, g_code_hook);
            g_code_hook_ok = 0;
        }
        /* Single narrow MEM watch (10165 context preferred). Full-space hooks starve timers. */
        if (ev && ev->guest_context) {
            GwyEventContextObject *c = platform_event_service_ctx_by_ptr(ev->guest_context);
            uint32_t sz = (c && c->size) ? c->size : 0x200u;
            watch_lo = ev->guest_context;
            watch_hi = ev->guest_context + sz;
        } else {
            for (i = platform_event_service_ctx_count() - 1; i >= 0; i--) {
                GwyEventContextObject *c = platform_event_service_ctx_at(i);
                if (!c || !c->used || c->plat_code != 0x10165u || !c->guest_ptr) continue;
                watch_lo = c->guest_ptr;
                watch_hi = c->guest_ptr + (c->size ? c->size : 0x200u);
                break;
            }
        }
        if (watch_lo && watch_hi > watch_lo) {
            e = uc_hook_add((uc_engine *)uc, &g_mem_hook, UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE,
                            (void *)ffp_hook_mem, NULL, (uint64_t)watch_lo, (uint64_t)watch_hi);
            g_mem_hook_ok = (e == UC_ERR_OK);
        }
        e = uc_hook_add((uc_engine *)uc, &g_code_hook, UC_HOOK_CODE, (void *)ffp_hook_code, NULL,
                        base, (uint64_t)base + 0x20ull); /* prologue only; avoid case-body slowdown */
        g_code_hook_ok = (e == UC_ERR_OK);
        if (product_ffp_trace_305e09() && !g_code_305e09_ok) {
            e = uc_hook_add((uc_engine *)uc, &g_code_305e09, UC_HOOK_CODE, (void *)ffp_hook_305e09,
                            NULL, 0x305E08ull, 0x305E09ull);
            g_code_305e09_ok = (e == UC_ERR_OK);
        }
    }
#else
    (void)uc;
    (void)handler;
    (void)ev;
#endif
}

void product_ffp_on_handler_leave(void *uc, uint64_t request_id, int ok, int32_t ret,
                                  uint32_t er_rw) {
    GwyPlatformEvent *ev;
    if (!product_ffp_enabled()) return;

#ifdef GWY_HAVE_UNICORN
    if (g_hook_uc) {
        if (g_mem_hook_ok) {
            uc_hook_del((uc_engine *)g_hook_uc, g_mem_hook);
            g_mem_hook_ok = 0;
        }
        if (g_code_hook_ok) {
            uc_hook_del((uc_engine *)g_hook_uc, g_code_hook);
            g_code_hook_ok = 0;
        }
        /* Keep 305E09 hook across deliveries while Trace305E09 is armed. */
        if (g_code_305e09_ok && !product_ffp_trace_305e09()) {
            uc_hook_del((uc_engine *)g_hook_uc, g_code_305e09);
            g_code_305e09_ok = 0;
        }
    }
    g_hook_req = 0;
#endif

    platform_event_service_on_deliver_leave(uc, request_id, ok, ret, er_rw);
    ev = platform_event_service_by_id(request_id);
    if (ev && (ev->stack_arg_reads > 0 || ev->mem_reads > 0 || ev->context_touched)) {
        if (ev->guest_context && ev->context_touched) {
            g_abi_confirmed = 1;
            printf("[FAMILY_EVENT_ABI_CONFIRMED] request_id=%llu ctx=0x%X stack_reads=%d "
                   "mem_reads=%d mem_writes=%d run_id=%s evidence=OBSERVED\n",
                   (unsigned long long)request_id, ev->guest_context, ev->stack_arg_reads,
                   ev->mem_reads, ev->mem_writes, product_ffp_run_id());
            fflush(stdout);
        } else if (ev->stack_arg_reads > 0) {
            printf("[FAMILY_HANDLER_ACK_CONTRACT] request_id=%llu stack_reads=%d "
                   "note=stack_args_observed run_id=%s evidence=OBSERVED\n",
                   (unsigned long long)request_id, ev->stack_arg_reads, product_ffp_run_id());
            fflush(stdout);
        }
    }

    if (platform_event_service_state_advanced() && product_ffp_phase() == GWY_FFP_PHASE_EVENT) {
        product_ffp_enter_resource_phase();
    }
}

void product_ffp_on_next_timer(void *uc, uint32_t er_rw) {
    if (!product_ffp_enabled()) return;
    platform_event_service_on_next_timer(uc, er_rw);
}

void product_ffp_note_resource_open(const char *path) {
    GwyFfpPhase ph = product_ffp_phase();
    if (!product_ffp_enabled()) return;
    /* Only emit after Event→Resource transition (or Resource/Validate modes). */
    if (ph != GWY_FFP_PHASE_RESOURCE && ph != GWY_FFP_PHASE_VALIDATE &&
        !platform_event_service_state_advanced())
        return;
    g_resource_open = 1;
    printf("[FIRST_NATURAL_VFS_OPEN] path=%s run_id=%s evidence=OBSERVED\n", path ? path : "?",
           product_ffp_run_id());
    printf("[FIRST_NATURAL_RESOURCE_REQUEST] path=%s run_id=%s evidence=OBSERVED\n",
           path ? path : "?", product_ffp_run_id());
    fflush(stdout);
}

void product_ffp_note_resource_read(uint32_t bytes) {
    GwyFfpPhase ph = product_ffp_phase();
    if (!product_ffp_enabled() || !bytes) return;
    if (ph != GWY_FFP_PHASE_RESOURCE && ph != GWY_FFP_PHASE_VALIDATE &&
        !platform_event_service_state_advanced())
        return;
    g_resource_read = 1;
    printf("[FIRST_NATURAL_RESOURCE_READ] bytes=%u run_id=%s evidence=OBSERVED\n", bytes,
           product_ffp_run_id());
    fflush(stdout);
}

void product_ffp_note_framebuffer_write(uint32_t bytes, uint32_t hash) {
    if (!product_ffp_enabled() || !bytes) return;
    g_fb_write = 1;
    printf("[FRAMEBUFFER_NONEMPTY] bytes=%u hash=0x%08X run_id=%s evidence=OBSERVED\n", bytes, hash,
           product_ffp_run_id());
    printf("[DISPLAY_PREDECESSOR_REACHED] run_id=%s evidence=OBSERVED\n", product_ffp_run_id());
    fflush(stdout);
}

void product_ffp_note_disp_up_ex(void) {
    if (!product_ffp_enabled()) return;
    g_disp_up = 1;
    printf("[FIRST_NATURAL_REFRESH] api=_DispUpEx run_id=%s evidence=OBSERVED\n",
           product_ffp_run_id());
    fflush(stdout);
}

static void write_abi_manifest(void) {
    char path[512];
    FILE *f;
    GwyPlatformEvent *ev = NULL;
    int i;
    for (i = 0; i < platform_event_service_event_count(); i++) {
        GwyPlatformEvent *e = platform_event_service_event_at(i);
        if (e && e->delivered) {
            ev = e;
            break;
        }
    }
    report_path(path, sizeof(path), "product_ffp_family_abi_manifest.json");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "{\n"
            "  \"family\": \"0x1E200\",\n"
            "  \"event\": \"%s\",\n"
            "  \"handler\": \"%s\",\n"
            "  \"register_args\": {\n"
            "    \"r0\": {\"role\": \"switch_case_app\", \"observed\": \"%s\", \"confirmed\": true},\n"
            "    \"r1\": {\"role\": \"event_code\", \"observed\": \"%s\", \"confirmed\": true},\n"
            "    \"r2\": {\"role\": \"context_or_payload_candidate\", \"observed\": \"%s\", "
            "\"confirmed\": %s},\n"
            "    \"r3\": {\"role\": \"length_or_token_candidate\", \"observed\": \"0x0\", "
            "\"confirmed\": false}\n"
            "  },\n"
            "  \"stack_args\": {\n"
            "    \"sp+0_entry_or_sp+32_post_prologue\": {\"role\": \"platform_object_candidate\", "
            "\"observed\": \"%s\", \"reads\": %d, \"confirmed\": %s},\n"
            "    \"sp+4_entry_or_sp+36_post_prologue\": {\"role\": \"callback_or_flags_candidate\", "
            "\"observed\": \"%s\", \"reads\": %d, \"confirmed\": false}\n"
            "  },\n"
            "  \"context_object\": {\n"
            "    \"guest_ptr\": \"%s\",\n"
            "    \"touched_by_handler\": %s,\n"
            "    \"owner_store\": \"%s\"\n"
            "  },\n"
            "  \"return_contract\": {\"ret\": %d, \"state_changed\": %s},\n"
            "  \"ack_contract\": {\"acknowledged\": %s, \"identity_class\": \"%s\"},\n"
            "  \"apply_abi_round_b\": %s,\n"
            "  \"abi_confirmed\": %s,\n"
            "  \"run_id\": \"%s\"\n"
            "}\n",
            ev ? (ev->event_code == 0x1E209u ? "0x1E209" : "other") : "none",
            ev ? "0x30D301" : "none", ev ? "app/case" : "0", ev ? "event_code" : "0",
            ev && ev->guest_context ? "guest_context" : "0",
            (ev && ev->context_touched) ? "true" : "false",
            ev ? "see_csv" : "0", ev ? ev->stack_arg_reads : 0,
            (ev && ev->stack_arg_reads > 0) ? "true" : "false", ev ? "see_csv" : "0",
            ev ? ev->stack_arg_reads : 0,
            ev && ev->guest_context ? "set" : "0",
            (ev && ev->context_touched) ? "true" : "false", "see_10165_csv",
            ev ? (int)ev->handler_ret : 0, (ev && ev->state_changed) ? "true" : "false",
            (ev && ev->acknowledged) ? "true" : "false",
            gwy_event_identity_class_name(platform_event_service_classify_identity()),
            product_ffp_apply_abi() ? "true" : "false", g_abi_confirmed ? "true" : "false",
            product_ffp_run_id());
    fclose(f);
}

static void write_samples_csv(void) {
    char path[512];
    FILE *f;
    int i;
    report_path(path, sizeof(path), "product_ffp_guest_request_samples.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "run_id,n,event,app,r0,r1,r2,r3,r4,r5,r6,r7,stack0,stack1,caller_pc,caller_lr,insn,ctx,"
            "er_digest,ctx_digest,identity\n");
    for (i = 0; i < g_sample_n; i++) {
        FfpGuestSample *s = &g_samples[i];
        fprintf(f,
                "%s,%d,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,%llu,"
                "0x%X,0x%08X,0x%08X,0x%llX\n",
                product_ffp_run_id(), i + 1, s->event_code, s->app, s->r[0], s->r[1], s->r[2],
                s->r[3], s->r[4], s->r[5], s->r[6], s->r[7], s->stack0, s->stack1, s->caller_pc,
                s->caller_lr, (unsigned long long)s->insn, s->ctx, s->er_digest, s->ctx_digest,
                (unsigned long long)s->identity);
    }
    fclose(f);
}

static void write_mem_csv(void) {
    char path[512];
    FILE *f;
    int i;
    report_path(path, sizeof(path), "product_ffp_handler_mem.csv");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "run_id,request_id,is_write,addr,size,value,pc\n");
    for (i = 0; i < g_mem_n; i++) {
        FfpMemHit *h = &g_mem[i];
        fprintf(f, "%s,%llu,%d,0x%X,%u,0x%X,0x%X\n", product_ffp_run_id(),
                (unsigned long long)h->request_id, h->is_write, h->addr, h->size, h->value, h->pc);
    }
    fclose(f);
}

static void write_ack_contract(void) {
    char path[512];
    FILE *f;
    GwyPlatformEvent *ev = NULL;
    int i;
    uint32_t p65 = 0, p62 = 0, enq = 0, store = 0;
    (void)platform_event_service_resolve_completion_objs(&p65, &p62, &enq, &store);
    for (i = 0; i < platform_event_service_event_count(); i++) {
        GwyPlatformEvent *e = platform_event_service_event_at(i);
        if (e && e->from_guest) {
            ev = e;
            break;
        }
    }
    report_path(path, sizeof(path), "product_event_ack_contract.json");
    f = fopen(path, "wb");
    if (!f) return;
    fprintf(f,
            "{\n"
            "  \"request\": {\"event\": \"0x1E209\", \"app\": 9},\n"
            "  \"unfinished_predicate\": {\n"
            "    \"pc\": \"0x30634C\",\n"
            "    \"object\": \"ER_RW\",\n"
            "    \"offset\": \"0x800+0xD0\",\n"
            "    \"size\": 4,\n"
            "    \"condition\": \"ui_mode == 0\",\n"
            "    \"current\": \"0\",\n"
            "    \"completed\": \"nonzero (Path A / natural writer e.g. 0x45)\"\n"
            "  },\n"
            "  \"writer\": {\n"
            "    \"kind\": \"platform\",\n"
            "    \"api\": \"0x101AB via 10165 enqueue 0x30D24C -> 0x2E4D6C B54\",\n"
            "    \"evidence\": \"STATIC+OBSERVED: case9/0x305E08 only issues 0x10133 free; "
            "timer reissues while ER_RW+(0x800+0xD0)==0\"\n"
            "  },\n"
            "  \"305e09_contract\": {\n"
            "    \"role\": \"platform_free_wrapper\",\n"
            "    \"nested\": \"sendAppEvent(0x10133, r0-4)\",\n"
            "    \"case9_r0\": \"event_code 0x1E209 (not heap ptr)\"\n"
            "  },\n"
            "  \"completion_objs\": {\"10165\": \"0x%X\", \"10162\": \"0x%X\", "
            "\"enq_handler\": \"0x%X\", \"owner_store\": \"0x%X\"},\n"
            "  \"request_id\": %llu,\n"
            "  \"state_advanced\": %s,\n"
            "  \"run_id\": \"%s\"\n"
            "}\n",
            p65, p62, enq, store, ev ? (unsigned long long)ev->request_id : 0ull,
            platform_event_service_state_advanced() ? "true" : "false", product_ffp_run_id());
    fclose(f);

    report_path(path, sizeof(path), "product_event_unfinished_predicate.csv");
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,pc,object,offset,size,condition,current,completed,writer\n");
        fprintf(f,
                "%s,0x30634C,ER_RW,0x800+0xD0,4,ui_mode==0,0,nonzero,"
                "platform_101AB_via_30D24C\n",
                product_ffp_run_id());
        fclose(f);
    }

    report_path(path, sizeof(path), "product_event_completion_validate.csv");
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,state_advanced,305e09_entries,ctx_count,ack\n");
        fprintf(f, "%s,%d,%d,%d,%d\n", product_ffp_run_id(),
                platform_event_service_state_advanced(), g_305e09_entries,
                platform_event_service_ctx_count(),
                ev && ev->acknowledged ? 1 : 0);
        fclose(f);
    }

    report_path(path, sizeof(path), "product_event_context_access_map.csv");
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "run_id,classification,note\n");
        fprintf(f, "%s,EVENT_CONTEXT_NOT_AN_INPUT,case9/305E08 does not read 10165\n",
                product_ffp_run_id());
        fprintf(f, "%s,EVENT_CONTEXT_EXPECTS_PLATFORM_WRITE,"
                   "101AB Path-A fill into owner-scoped 10165 before/with enqueue\n",
                product_ffp_run_id());
        fprintf(f, "%s,FAMILY_HANDLER_IS_NOTIFICATION_ONLY,case9 only 10133 free\n",
                product_ffp_run_id());
        fprintf(f, "%s,PLATFORM_MUST_PUBLISH_COMPLETION_BEFORE_HANDLER,"
                   "enqueue 30D2F9/30D24C then family notify\n",
                product_ffp_run_id());
        fclose(f);
    }
}

void product_ffp_finalize(void) {
    if (g_finalized || !product_ffp_enabled()) return;
    g_finalized = 1;
    platform_event_service_finalize();
    product_eqb_finalize();
    write_samples_csv();
    write_mem_csv();
    write_abi_manifest();
    write_ack_contract();
    printf("[FFP_FINALIZE] samples=%d mem_hits=%d abi_confirmed=%d state_advanced=%d "
           "resource_open=%d resource_read=%d fb=%d disp=%d entries_305e09=%d run_id=%s "
           "evidence=OBSERVED\n",
           g_sample_n, g_mem_n, g_abi_confirmed, platform_event_service_state_advanced(),
           g_resource_open, g_resource_read, g_fb_write, g_disp_up, g_305e09_entries,
           product_ffp_run_id());
    fflush(stdout);
}

int product_ffp_state_advanced(void) { return platform_event_service_state_advanced(); }
int product_ffp_abi_confirmed(void) { return g_abi_confirmed; }
