#include "gwy_launcher/platform_mrp_resource_census.h"

#include "gwy_launcher/jjfbol_catalog.h"
#include "gwy_launcher/jjfbol_scope.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#define CENSUS_CAP 2048u
#define NAME_CAP 256u

typedef struct {
    int used;
    uint64_t sequence;
    char member_name[NAME_CAP];
    char extension[32];
    uint32_t caller_pc;
    uint32_t caller_lr;
    char active_package[128];
    uint64_t scope_generation;
    char catalog_result[16]; /* exact/unique/multi/miss */
    GwyMrpCensusKind kind;
    char host_action[32];
    uint32_t repeat_count;
} CensusRow;

static CensusRow g_rows[CENSUS_CAP];
static uint32_t g_row_n;
static uint64_t g_seq;
static int g_armed;
static int g_atexit_ok;
static int g_first_rejected;
static int g_first_ani;
static int g_first_txt;
static uint32_t g_kind_counts[8];
static uint32_t g_action_reject;
static uint32_t g_action_complete;
static uint32_t g_action_other;

static void extract_ext(const char *name, char *out, size_t out_cap) {
    const char *dot;
    size_t i, n;
    if (!out || out_cap == 0) return;
    out[0] = 0;
    if (!name || !name[0]) return;
    dot = strrchr(name, '.');
    if (!dot || !dot[1]) return;
    n = strlen(dot + 1);
    if (n >= out_cap) n = out_cap - 1;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)dot[1 + i]);
    out[n] = 0;
}

GwyMrpCensusKind platform_mrp_resource_census_classify(const char *member_name) {
    char ext[32];
    size_t n;
    if (!member_name || !member_name[0]) return GWY_MRP_CENSUS_UNKNOWN;
    extract_ext(member_name, ext, sizeof(ext));
    if (strcmp(ext, "bmp") == 0 || strcmp(ext, "gif") == 0) return GWY_MRP_CENSUS_BITMAP_CANDIDATE;
    if (strcmp(ext, "ani") == 0) return GWY_MRP_CENSUS_ANI_CANDIDATE;
    if (strcmp(ext, "txt") == 0) return GWY_MRP_CENSUS_TEXT_CANDIDATE;
    if (strcmp(ext, "ext") == 0 || strcmp(ext, "mr") == 0) return GWY_MRP_CENSUS_MODULE_CANDIDATE;
    n = strlen(member_name);
    if (!strchr(member_name, '.') && n > 0) return GWY_MRP_CENSUS_EXTENSIONLESS_CANDIDATE;
    return GWY_MRP_CENSUS_UNKNOWN;
}

const char *platform_mrp_resource_census_kind_name(GwyMrpCensusKind kind) {
    switch (kind) {
    case GWY_MRP_CENSUS_BITMAP_CANDIDATE:
        return "bitmap_candidate";
    case GWY_MRP_CENSUS_ANI_CANDIDATE:
        return "ani_candidate";
    case GWY_MRP_CENSUS_TEXT_CANDIDATE:
        return "text_candidate";
    case GWY_MRP_CENSUS_MODULE_CANDIDATE:
        return "module_candidate";
    case GWY_MRP_CENSUS_EXTENSIONLESS_CANDIDATE:
        return "extensionless_candidate";
    default:
        return "unknown";
    }
}

static void catalog_probe(const char *name, char *out, size_t out_cap) {
    JjfbolLookupResult lr;
    if (!out || out_cap == 0) return;
    snprintf(out, out_cap, "miss");
    if (!name || !name[0] || !jjfbol_catalog_ready()) return;
    if (jjfbol_catalog_lookup_exact(name, &lr) == JJFBOL_LOOKUP_UNIQUE) {
        snprintf(out, out_cap, "exact");
        return;
    }
    if (lr.kind == JJFBOL_LOOKUP_MULTI) {
        snprintf(out, out_cap, "multi");
        return;
    }
    if (jjfbol_catalog_lookup_casefold(name, &lr) == JJFBOL_LOOKUP_UNIQUE) {
        snprintf(out, out_cap, "unique");
        return;
    }
    if (lr.kind == JJFBOL_LOOKUP_MULTI) {
        snprintf(out, out_cap, "multi");
        return;
    }
}

static CensusRow *find_row(const char *name, uint32_t pc, uint32_t lr) {
    uint32_t i;
    for (i = 0; i < g_row_n; i++) {
        if (g_rows[i].used && g_rows[i].caller_pc == pc && g_rows[i].caller_lr == lr &&
            strcmp(g_rows[i].member_name, name) == 0)
            return &g_rows[i];
    }
    return NULL;
}

static void append_csv_row(FILE *fp, const CensusRow *r) {
    if (!fp || !r) return;
    fprintf(fp,
            "%llu,\"%s\",%s,0x%X,0x%X,\"%s\",%llu,%s,%s,%s,%u\n",
            (unsigned long long)r->sequence, r->member_name, r->extension[0] ? r->extension : "",
            r->caller_pc, r->caller_lr, r->active_package[0] ? r->active_package : "",
            (unsigned long long)r->scope_generation, r->catalog_result,
            platform_mrp_resource_census_kind_name(r->kind), r->host_action, r->repeat_count);
}

static void ensure_reports_dir(void) {
#ifdef _WIN32
    CreateDirectoryA("reports", NULL);
    CreateDirectoryA("../../reports", NULL);
#else
    mkdir("reports", 0755);
    mkdir("../../reports", 0755);
#endif
}

static FILE *open_census_csv(const char *mode) {
    FILE *fp;
    ensure_reports_dir();
    /* Prefer repo-root reports when cwd is out/vmrp_run. */
    fp = fopen("../../reports/p3_resource_request_census.csv", mode);
    if (fp) return fp;
    return fopen("reports/p3_resource_request_census.csv", mode);
}

static FILE *open_summary_json(void) {
    FILE *fp;
    ensure_reports_dir();
    fp = fopen("../../reports/p3_resource_request_summary.json", "wb");
    if (fp) return fp;
    return fopen("reports/p3_resource_request_summary.json", "wb");
}

static void rewrite_csv(void) {
    FILE *fp;
    uint32_t i;
    fp = open_census_csv("wb");
    if (!fp) return;
    fprintf(fp, "sequence,member_name,extension,caller_pc,caller_lr,active_package,"
                "scope_generation,catalog,classified_kind,host_action,repeat_count\n");
    for (i = 0; i < g_row_n; i++) {
        if (g_rows[i].used) append_csv_row(fp, &g_rows[i]);
    }
    fclose(fp);
}

static void write_summary_json(const char *reason) {
    FILE *fp = open_summary_json();
    if (!fp) return;
    fprintf(fp,
            "{\n"
            "  \"reason\": \"%s\",\n"
            "  \"total_unique_rows\": %u,\n"
            "  \"total_sequences\": %llu,\n"
            "  \"kind_counts\": {\n"
            "    \"bitmap_candidate\": %u,\n"
            "    \"ani_candidate\": %u,\n"
            "    \"text_candidate\": %u,\n"
            "    \"module_candidate\": %u,\n"
            "    \"extensionless_candidate\": %u,\n"
            "    \"unknown\": %u\n"
            "  },\n"
            "  \"host_action\": {\n"
            "    \"reject_filter\": %u,\n"
            "    \"complete\": %u,\n"
            "    \"other\": %u\n"
            "  },\n"
            "  \"first_rejected\": %s,\n"
            "  \"first_ani\": %s,\n"
            "  \"first_txt\": %s\n"
            "}\n",
            reason ? reason : "?", g_row_n, (unsigned long long)g_seq,
            g_kind_counts[GWY_MRP_CENSUS_BITMAP_CANDIDATE],
            g_kind_counts[GWY_MRP_CENSUS_ANI_CANDIDATE],
            g_kind_counts[GWY_MRP_CENSUS_TEXT_CANDIDATE],
            g_kind_counts[GWY_MRP_CENSUS_MODULE_CANDIDATE],
            g_kind_counts[GWY_MRP_CENSUS_EXTENSIONLESS_CANDIDATE],
            g_kind_counts[GWY_MRP_CENSUS_UNKNOWN], g_action_reject, g_action_complete,
            g_action_other, g_first_rejected ? "true" : "false", g_first_ani ? "true" : "false",
            g_first_txt ? "true" : "false");
    fclose(fp);
}

void platform_mrp_resource_census_flush(const char *reason) {
    if (!g_armed && g_row_n == 0) return;
    rewrite_csv();
    write_summary_json(reason ? reason : "flush");
    printf("[P3_RESOURCE_CENSUS] flush reason=%s rows=%u seq=%llu reject=%u complete=%u "
           "ani=%u txt=%u evidence=OBSERVED\n",
           reason ? reason : "?", g_row_n, (unsigned long long)g_seq, g_action_reject,
           g_action_complete, g_kind_counts[GWY_MRP_CENSUS_ANI_CANDIDATE],
           g_kind_counts[GWY_MRP_CENSUS_TEXT_CANDIDATE]);
    fflush(stdout);
}

static void census_atexit(void) { platform_mrp_resource_census_flush("atexit"); }

void platform_mrp_resource_census_reset(void) {
    memset(g_rows, 0, sizeof(g_rows));
    g_row_n = 0;
    g_seq = 0;
    g_first_rejected = 0;
    g_first_ani = 0;
    g_first_txt = 0;
    memset(g_kind_counts, 0, sizeof(g_kind_counts));
    g_action_reject = 0;
    g_action_complete = 0;
    g_action_other = 0;
}

void platform_mrp_resource_census_arm(void) {
    g_armed = 1;
    if (!g_atexit_ok) {
        atexit(census_atexit);
        g_atexit_ok = 1;
    }
    printf("[P3_RESOURCE_CENSUS] armed observe_only=1 evidence=OBSERVED\n");
    fflush(stdout);
}

void platform_mrp_resource_census_note(const char *member_name, uint32_t caller_pc,
                                      uint32_t caller_lr, int passes_legacy_filter,
                                      const char *host_action_override) {
    CensusRow *row;
    GwyMrpCensusKind kind;
    const char *action;
    const char *active;
    char catalog[16];

    if (!member_name || !member_name[0]) return;
    if (!g_armed) platform_mrp_resource_census_arm();

    kind = platform_mrp_resource_census_classify(member_name);
    if (host_action_override && host_action_override[0])
        action = host_action_override;
    else if (!passes_legacy_filter)
        action = "reject_filter";
    else
        action = "load_attempt";

    row = find_row(member_name, caller_pc, caller_lr);
    if (row) {
        row->repeat_count++;
        if (strcmp(action, "complete") == 0) snprintf(row->host_action, sizeof(row->host_action), "%s", action);
        if ((g_seq % 32u) == 0u) {
            rewrite_csv();
            write_summary_json("periodic");
        }
        return;
    }
    if (g_row_n >= CENSUS_CAP) return;

    row = &g_rows[g_row_n++];
    memset(row, 0, sizeof(*row));
    row->used = 1;
    g_seq++;
    row->sequence = g_seq;
    snprintf(row->member_name, sizeof(row->member_name), "%s", member_name);
    extract_ext(member_name, row->extension, sizeof(row->extension));
    row->caller_pc = caller_pc;
    row->caller_lr = caller_lr;
    active = jjfbol_scope_active_package();
    if (active) snprintf(row->active_package, sizeof(row->active_package), "%s", active);
    row->scope_generation = jjfbol_scope_generation();
    catalog_probe(member_name, catalog, sizeof(catalog));
    snprintf(row->catalog_result, sizeof(row->catalog_result), "%s", catalog);
    row->kind = kind;
    snprintf(row->host_action, sizeof(row->host_action), "%s", action);
    row->repeat_count = 1;
    if (kind < 8) g_kind_counts[kind]++;

    if (strcmp(action, "reject_filter") == 0) {
        g_action_reject++;
        if (!g_first_rejected) {
            g_first_rejected = 1;
            printf("[FIRST_REJECTED_RESOURCE] name=\"%s\" kind=%s catalog=%s pc=0x%X lr=0x%X "
                   "evidence=OBSERVED\n",
                   member_name, platform_mrp_resource_census_kind_name(kind), catalog, caller_pc,
                   caller_lr);
            fflush(stdout);
        }
    } else if (strcmp(action, "complete") == 0) {
        g_action_complete++;
    } else {
        g_action_other++;
    }

    if (kind == GWY_MRP_CENSUS_ANI_CANDIDATE && !g_first_ani) {
        g_first_ani = 1;
        printf("[FIRST_ANI_REQUEST] name=\"%s\" catalog=%s action=%s pc=0x%X lr=0x%X "
               "evidence=OBSERVED\n",
               member_name, catalog, action, caller_pc, caller_lr);
        fflush(stdout);
    }
    if (kind == GWY_MRP_CENSUS_TEXT_CANDIDATE && !g_first_txt) {
        g_first_txt = 1;
        printf("[FIRST_TXT_REQUEST] name=\"%s\" catalog=%s action=%s pc=0x%X lr=0x%X "
               "evidence=OBSERVED\n",
               member_name, catalog, action, caller_pc, caller_lr);
        fflush(stdout);
    }

    /* Kill-safe incremental append. */
    {
        FILE *fp = open_census_csv(g_row_n == 1 ? "wb" : "ab");
        if (fp) {
            if (g_row_n == 1)
                fprintf(fp, "sequence,member_name,extension,caller_pc,caller_lr,active_package,"
                            "scope_generation,catalog,classified_kind,host_action,repeat_count\n");
            append_csv_row(fp, row);
            fclose(fp);
        }
        write_summary_json("incremental");
    }
}

void platform_mrp_resource_census_note_complete(const char *member_name) {
    uint32_t i;
    if (!member_name || !member_name[0]) return;
    for (i = 0; i < g_row_n; i++) {
        if (g_rows[i].used && strcmp(g_rows[i].member_name, member_name) == 0) {
            snprintf(g_rows[i].host_action, sizeof(g_rows[i].host_action), "complete");
            g_action_complete++;
            return;
        }
    }
    platform_mrp_resource_census_note(member_name, 0, 0, 1, "complete");
}
