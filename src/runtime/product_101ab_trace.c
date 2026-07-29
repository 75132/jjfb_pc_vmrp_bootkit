#include "gwy_launcher/product_101ab_trace.h"
#include "gwy_launcher/guest_memory.h"
#include "gwy_launcher/platform_101ab_frame.h"
#include "gwy_launcher/platform_101ab_provider.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#define PC_30D24C 0x30D24Cu
#define PC_30D2AA 0x30D2AAu
#define PC_30D2B0 0x30D2B0u
#define PC_30D2BE 0x30D2BEu
#define PC_30D2CE 0x30D2CEu
#define PC_30D2D8 0x30D2D8u
#define PC_2E4D6C 0x2E4D6Cu
#define PC_2E4DA0 0x2E4DA0u
#define PC_2E4E98 0x2E4E98u
#define PC_2E4EA4 0x2E4EA4u
#define PC_2E4ED8 0x2E4ED8u
#define PC_2E4EE0 0x2E4EE0u
#define PC_2E4EE6 0x2E4EE6u
#define PC_2E4EEE 0x2E4EEEu
#define PC_2DC8D4 0x2DC8D4u
#define PC_2E2520 0x2E2520u
#define PC_2E5E60 0x2E5E60u
#define PC_2E5F8C 0x2E5F8Cu
#define PC_2E5FAE 0x2E5FAEu

#define FRAME_CAP 64
#define BODY_CAP 2048
#define SITE_CAP 24

typedef struct {
    uint32_t frame_id;
    uint32_t buf_va;
    uint32_t capacity;
    uint32_t guest_r0;
    uint32_t type;
    uint32_t payload_len;
    uint32_t header;
    uint32_t body_size;
    uint16_t event_code;
    uint32_t event_node_va;
    uint32_t body_ptr;
    uint32_t body_len;
    uint32_t queue_gen;
    int with_record;
    char body_sha[65];
    char dump_path[320];
    char provider[48];
    uint8_t raw[BODY_CAP];
    uint32_t raw_n;
    int code15_seen;
    int e6c_store_seen;
    uint32_t e6c_ptr;
} FrameState;

static int g_en;
static int g_en_known;
static int g_finalized;
static int g_hook_ok;
static char g_run_id[80];
static void *g_uc;
static uint32_t g_er_rw;
static uint32_t g_frame_seq;
static FrameState g_frames[FRAME_CAP];
static int g_frame_n;
static FrameState *g_cur;
static FILE *g_csv;
static char g_outdir[260];

static int env1(const char *k) {
    const char *v = getenv(k);
    return v && v[0] == '1' && v[1] == '\0';
}

int product_101ab_trace_enabled(void) {
    if (!g_en_known) {
        /* Default ON for product research; opt-out JJFB_101AB_TRACE=0 */
        const char *v = getenv("JJFB_101AB_TRACE");
        if (v && v[0] == '0' && v[1] == '\0')
            g_en = 0;
        else
            g_en = 1;
        g_en_known = 1;
    }
    return g_en;
}

void product_101ab_trace_reset(void) {
    if (g_csv) {
        fclose(g_csv);
        g_csv = NULL;
    }
    g_en_known = 0;
    g_en = 0;
    g_finalized = 0;
    g_hook_ok = 0;
    g_uc = NULL;
    g_er_rw = 0;
    g_frame_seq = 0;
    g_frame_n = 0;
    g_cur = NULL;
    memset(g_frames, 0, sizeof(g_frames));
    g_run_id[0] = 0;
    g_outdir[0] = 0;
}

void product_101ab_trace_set_run_id(const char *run_id) {
    if (!run_id) {
        g_run_id[0] = 0;
        return;
    }
    strncpy(g_run_id, run_id, sizeof(g_run_id) - 1u);
    g_run_id[sizeof(g_run_id) - 1u] = 0;
}

const char *product_101ab_trace_run_id(void) { return g_run_id; }

void product_101ab_trace_bind_uc(void *uc) { g_uc = uc; }

void product_101ab_trace_note_er_rw(uint32_t er_rw) { g_er_rw = er_rw; }

static void ensure_paths(void) {
    const char *reports = getenv("GWY_PRODUCT_REPORTS_DIR");
    const char *dump_root = getenv("JJFB_101AB_TRACE_DIR");
    char csv_path[320];
    if (!reports || !reports[0]) reports = "reports";
    if (dump_root && dump_root[0])
        snprintf(g_outdir, sizeof(g_outdir), "%s/frames", dump_root);
    else
        snprintf(g_outdir, sizeof(g_outdir), "out/p15_101ab/frames");
#ifdef _WIN32
    {
        char tmp[260];
        if (dump_root && dump_root[0]) {
            _mkdir(dump_root);
        } else {
            _mkdir("out");
            _mkdir("out/p15_101ab");
        }
        snprintf(tmp, sizeof(tmp), "%s", g_outdir);
        _mkdir(tmp);
    }
#else
    if (dump_root && dump_root[0]) {
        mkdir(dump_root, 0755);
    } else {
        mkdir("out", 0755);
        mkdir("out/p15_101ab", 0755);
    }
    mkdir(g_outdir, 0755);
#endif
    if (!g_csv) {
        snprintf(csv_path, sizeof(csv_path), "%s/P15_101AB_TRACE.csv", reports);
        g_csv = fopen(csv_path, "w");
        if (g_csv) {
            fprintf(g_csv,
                    "frame_id,site,pc,buf_va,capacity,guest_r0,type,payload_len,header,"
                    "body_size,event_code,body_sha,body_len,event_node_va,body_ptr,"
                    "queue_gen,provider,dump_path,with_record,extra\n");
            fflush(g_csv);
        }
    }
}

static void csv_row(const char *site, uint32_t pc, FrameState *f, const char *extra) {
    if (!g_csv || !f) return;
    fprintf(g_csv,
            "%u,%s,0x%X,0x%X,%u,%u,%u,%u,%u,%u,%u,%s,%u,0x%X,0x%X,%u,%s,%s,%d,%s\n",
            f->frame_id, site, pc, f->buf_va, f->capacity, f->guest_r0, f->type, f->payload_len,
            f->header, f->body_size, (unsigned)f->event_code, f->body_sha[0] ? f->body_sha : "",
            f->body_len, f->event_node_va, f->body_ptr, f->queue_gen, f->provider,
            f->dump_path[0] ? f->dump_path : "", f->with_record, extra ? extra : "");
    fflush(g_csv);
}

static FrameState *new_frame(void) {
    FrameState *f;
    if (g_frame_n >= FRAME_CAP) return g_frame_n ? &g_frames[g_frame_n - 1] : NULL;
    f = &g_frames[g_frame_n++];
    memset(f, 0, sizeof(*f));
    f->frame_id = ++g_frame_seq;
    return f;
}

static void dump_raw(FrameState *f) {
    FILE *fp;
    if (!f || !f->raw_n) return;
    snprintf(f->dump_path, sizeof(f->dump_path), "%s/frame_%u.bin", g_outdir, f->frame_id);
    fp = fopen(f->dump_path, "wb");
    if (!fp) return;
    fwrite(f->raw, 1, f->raw_n, fp);
    fclose(fp);
}

static void apply_decoded(FrameState *f, const Gwy101AbDecodedFrame *dec) {
    uint8_t dig[32];
    if (!f || !dec) return;
    f->type = dec->type;
    f->payload_len = dec->payload_length;
    f->header = dec->header;
    if (dec->record_count > 0) {
        const Gwy101AbRecord *r = &dec->records[0];
        f->body_size = r->body_size;
        f->event_code = r->event_code;
        f->body_len = r->body_len;
        if (r->body_off + r->body_len <= f->raw_n) {
            gwy_sha256(f->raw + r->body_off, r->body_len, dig);
            gwy_sha256_hex(dig, f->body_sha);
        }
        if (r->event_code == 15u) f->code15_seen = 1;
    }
}

void product_101ab_trace_on_platform_fill(void *uc, uint32_t buf_va, uint32_t capacity,
                                          const uint8_t *host_bytes, uint32_t host_n,
                                          uint32_t guest_r0_cursor, int with_record,
                                          const char *provider_name) {
    FrameState *f;
    Gwy101AbDecodedFrame dec;
    (void)uc;
    if (!product_101ab_trace_enabled()) return;
    ensure_paths();
    f = new_frame();
    if (!f) return;
    g_cur = f;
    f->buf_va = buf_va;
    f->capacity = capacity;
    f->guest_r0 = guest_r0_cursor;
    f->with_record = with_record;
    strncpy(f->provider, provider_name ? provider_name : "?", sizeof(f->provider) - 1u);
    if (host_bytes && host_n) {
        f->raw_n = host_n > BODY_CAP ? BODY_CAP : host_n;
        memcpy(f->raw, host_bytes, f->raw_n);
        dump_raw(f);
        if (platform_101ab_decode_frame(f->raw, f->raw_n, &dec) == GWY_101AB_DECODE_OK ||
            dec.record_count > 0)
            apply_decoded(f, &dec);
    }
    csv_row("PLAT_101AB_FILL", 0, f, platform_101ab_provider_transport_name(
                                         platform_101ab_provider_transport_class()));
    printf("[P15_101AB] frame_id=%u fill bytes=%u type=%u hdr=%u code=%u provider=%s "
           "cursor=%u dump=%s evidence=OBSERVED\n",
           f->frame_id, f->raw_n, f->type, f->header, (unsigned)f->event_code, f->provider,
           f->guest_r0, f->dump_path[0] ? f->dump_path : "-");
    fflush(stdout);
}

#ifdef GWY_HAVE_UNICORN
static uint32_t reg_u32(uc_engine *uc, int reg) {
    uint32_t v = 0;
    uc_reg_read(uc, reg, &v);
    return v;
}

static void on_site(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
    uint32_t pc = (uint32_t)address;
    uint32_t tag = (uint32_t)(uintptr_t)user;
    FrameState *f = g_cur;
    char extra[96];
    (void)size;
    if (!product_101ab_trace_enabled() || g_finalized) return;
    ensure_paths();
    extra[0] = 0;

    switch (tag) {
    case 1: /* 30D24C enter: r0=buf r1=cap */
        if (!f || f->buf_va == 0) {
            f = new_frame();
            g_cur = f;
        }
        if (f) {
            f->buf_va = reg_u32(uc, UC_ARM_REG_R0);
            f->capacity = reg_u32(uc, UC_ARM_REG_R1);
            csv_row("30D24C_ENTER", pc, f, "");
        }
        break;
    case 2: /* 30D2AA pre-101AB */
        if (f) csv_row("30D2AA_PRE_101AB", pc, f, "");
        break;
    case 3: /* 30D2B0 post-101AB: r0=cursor */
        if (f) {
            f->guest_r0 = reg_u32(uc, UC_ARM_REG_R0);
            snprintf(extra, sizeof(extra), "r0_cursor=%u", f->guest_r0);
            csv_row("30D2B0_POST_101AB", pc, f, extra);
        }
        break;
    case 4: /* 30D2BE type parse */
        if (f) csv_row("30D2BE_TYPE", pc, f, "");
        break;
    case 5: /* 30D2CE payload_len parse */
        if (f) csv_row("30D2CE_PAYLOAD_LEN", pc, f, "");
        break;
    case 6: /* 30D2D8 call 2E4D6C: r0=payload r1=len */
        if (f) {
            snprintf(extra, sizeof(extra), "payload=0x%X len=%u", reg_u32(uc, UC_ARM_REG_R0),
                     reg_u32(uc, UC_ARM_REG_R1));
            f->payload_len = reg_u32(uc, UC_ARM_REG_R1);
            csv_row("30D2D8_CALL_PARSER", pc, f, extra);
        }
        break;
    case 7: /* 2E4D6C enter */
        if (f) csv_row("2E4D6C_ENTER", pc, f, "");
        break;
    case 8: /* 2E4DA0 header read */
        if (f) csv_row("2E4DA0_HEADER", pc, f, "");
        break;
    case 9: /* 2E4E98 body_size */
        if (f) csv_row("2E4E98_BODY_SIZE", pc, f, "");
        break;
    case 10: /* 2E4EA4 event_code via 308D28 */
        if (f) csv_row("2E4EA4_EVENT_CODE", pc, f, "");
        break;
    case 11: /* 2E4ED8 store code to node */
        if (f) {
            f->event_node_va = reg_u32(uc, UC_ARM_REG_R5);
            f->event_code = (uint16_t)reg_u32(uc, UC_ARM_REG_R1);
            snprintf(extra, sizeof(extra), "node=0x%X code=%u", f->event_node_va,
                     (unsigned)f->event_code);
            if (f->event_code == 15u) {
                f->code15_seen = 1;
                printf("[P15_CODE15] frame_id=%u NATURAL event_code=15 node=0x%X "
                       "evidence=OBSERVED\n",
                       f->frame_id, f->event_node_va);
                fflush(stdout);
            }
            csv_row("2E4ED8_NODE_CODE", pc, f, extra);
        }
        break;
    case 12: /* 2E4EE0 body ptr */
        if (f) {
            f->body_ptr = reg_u32(uc, UC_ARM_REG_R0);
            csv_row("2E4EE0_BODY_PTR", pc, f, "");
        }
        break;
    case 13: /* 2E4EE6 body len */
        if (f) {
            f->body_len = reg_u32(uc, UC_ARM_REG_R0);
            csv_row("2E4EE6_BODY_LEN", pc, f, "");
        }
        break;
    case 14: /* 2E4EEE B54 enqueue */
        if (f) {
            f->queue_gen++;
            csv_row("2E4EEE_B54_ENQUEUE", pc, f, "B54");
        }
        break;
    case 15: /* 2DC8D4 consume → 2E2520 */
        if (f) csv_row("2DC8D4_B54_CONSUME", pc, f, "");
        break;
    case 16: /* 2E2520 dispatch */
        if (f) {
            snprintf(extra, sizeof(extra), "r0=0x%X", reg_u32(uc, UC_ARM_REG_R0));
            csv_row("2E2520_DISPATCH", pc, f, extra);
        }
        break;
    case 17: /* 2E5E60 code15 handler */
        if (f) {
            f->code15_seen = 1;
            csv_row("2E5E60_CODE15_ENTER", pc, f, "schema_capture");
            printf("[P15_CODE15_SCHEMA] frame_id=%u enter_2E5E60 node=0x%X evidence=OBSERVED\n",
                   f->frame_id, reg_u32(uc, UC_ARM_REG_R0));
            fflush(stdout);
        }
        break;
    case 18: /* 2E5FAE E6C store site (approx) */
        if (f) {
            f->e6c_store_seen = 1;
            f->e6c_ptr = reg_u32(uc, UC_ARM_REG_R0);
            csv_row("2E5FAE_E6C_STORE", pc, f, "guest_str");
            printf("[P15_E6C_NATURAL_STORE] frame_id=%u ptr=0x%X evidence=OBSERVED\n", f->frame_id,
                   f->e6c_ptr);
            fflush(stdout);
        }
        break;
    default:
        break;
    }
}

void product_101ab_trace_arm_hooks(void *uc) {
    static const struct {
        uint32_t pc;
        uint32_t tag;
    } sites[] = {
        {PC_30D24C, 1},  {PC_30D2AA, 2},  {PC_30D2B0, 3},  {PC_30D2BE, 4},  {PC_30D2CE, 5},
        {PC_30D2D8, 6},  {PC_2E4D6C, 7},  {PC_2E4DA0, 8},  {PC_2E4E98, 9},  {PC_2E4EA4, 10},
        {PC_2E4ED8, 11}, {PC_2E4EE0, 12}, {PC_2E4EE6, 13}, {PC_2E4EEE, 14}, {PC_2DC8D4, 15},
        {PC_2E2520, 16}, {PC_2E5E60, 17}, {PC_2E5FAE, 18},
    };
    size_t i;
    if (!product_101ab_trace_enabled() || !uc || g_hook_ok) return;
    g_uc = uc;
    ensure_paths();
    for (i = 0; i < sizeof(sites) / sizeof(sites[0]); i++) {
        uc_hook h = 0;
        uc_err e = uc_hook_add((uc_engine *)uc, &h, UC_HOOK_CODE, (void *)on_site,
                               (void *)(uintptr_t)sites[i].tag, sites[i].pc, sites[i].pc);
        (void)e;
    }
    g_hook_ok = 1;
    printf("[P15_101AB_TRACE] armed sites=%u transport=%s provider=%s evidence=OBSERVED\n",
           (unsigned)(sizeof(sites) / sizeof(sites[0])),
           platform_101ab_provider_transport_name(platform_101ab_provider_transport_class()),
           platform_101ab_provider_mode_name(platform_101ab_provider_mode()));
    fflush(stdout);
}
#else
void product_101ab_trace_arm_hooks(void *uc) { (void)uc; }
#endif

void product_101ab_trace_finalize(void) {
    int i, code15 = 0, e6c = 0;
    if (g_finalized) return;
    g_finalized = 1;
    if (!product_101ab_trace_enabled()) return;
    for (i = 0; i < g_frame_n; i++) {
        if (g_frames[i].code15_seen) code15++;
        if (g_frames[i].e6c_store_seen) e6c++;
    }
    printf("[P15_101AB_TRACE_FINAL] frames=%d code15=%d e6c_natural=%d transport=%s "
           "evidence=OBSERVED\n",
           g_frame_n, code15, e6c,
           platform_101ab_provider_transport_name(platform_101ab_provider_transport_class()));
    fflush(stdout);
    if (g_csv) {
        fclose(g_csv);
        g_csv = NULL;
    }
    (void)env1;
}
