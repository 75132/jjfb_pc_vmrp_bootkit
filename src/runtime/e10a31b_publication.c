#include "gwy_launcher/e10a31b_publication.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    int known;
    int enabled;
    unsigned long long run_id;
    uint32_t seq;
    uint32_t last_p;
    FILE *csv;
} g_b;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1' && e[1] == '\0';
}

static void parse(void) {
    if (g_b.known) return;
    g_b.enabled = env1("JJFB_E10A31B_MODE") || env1("JJFB_E10A31_TIMER_CONTEXT") ||
                  env1("JJFB_E10A31_MODE");
    {
        const char *rid = getenv("JJFB_E10A31_RUN_ID");
        if (!rid || !rid[0]) rid = getenv("JJFB_E10A_RUN_ID");
        if (rid && rid[0]) g_b.run_id = strtoull(rid, NULL, 10);
    }
    g_b.known = 1;
}

int e10a31b_enabled(void) {
    parse();
    return g_b.enabled;
}

void e10a31b_reset(void) {
    if (g_b.csv) fclose(g_b.csv);
    memset(&g_b, 0, sizeof(g_b));
}

static void ensure_csv(void) {
    const char *p;
    if (g_b.csv) return;
    p = getenv("JJFB_E10A31B_PUB_CSV");
    if (!p || !p[0]) p = "reports/e10a31b_publication_path.csv";
    g_b.csv = fopen(p, "wb");
    if (g_b.csv) {
        fputs("run_id,seq,event,module,helper,p_guest,chunk_guest,erw,erw_len,module_id,"
              "old_module_id,registry_erw,own_erw,note\n",
              g_b.csv);
        fflush(g_b.csv);
    }
}

static void row(const char *event, const char *module, uint32_t helper, uint32_t p_guest,
                uint32_t chunk_guest, uint32_t erw, uint32_t erw_len, uint64_t module_id,
                uint64_t old_mid, uint32_t registry_erw, int own_erw, const char *note) {
    parse();
    if (!g_b.enabled) return;
    ensure_csv();
    g_b.seq++;
    printf("[JJFB_E10A31B] milestone=%s module=%s helper=0x%X P=0x%X chunk=0x%X erw=0x%X "
           "registry_erw=0x%X own_erw=%d note=%s evidence=OBSERVED\n",
           event ? event : "?", module ? module : "?", helper, p_guest, chunk_guest, erw,
           registry_erw, own_erw, note ? note : "");
    fflush(stdout);
    if (!g_b.csv) return;
    fprintf(g_b.csv,
            "%llu,%u,%s,\"%s\",0x%X,0x%X,0x%X,0x%X,%u,%llu,%llu,0x%X,%d,\"%s\"\n",
            g_b.run_id, g_b.seq, event ? event : "?", module ? module : "?", helper, p_guest,
            chunk_guest, erw, erw_len, (unsigned long long)module_id,
            (unsigned long long)old_mid, registry_erw, own_erw, note ? note : "");
    fflush(g_b.csv);
}

void e10a31b_mark(const char *milestone, const char *module, uint32_t helper, uint32_t p_guest,
                  uint32_t chunk_guest, uint32_t erw, uint64_t module_id, const char *note) {
    row(milestone, module, helper, p_guest, chunk_guest, erw, 0, module_id, 0, 0, 0, note);
}

void e10a31b_note_cfn(const char *module, uint32_t helper, uint32_t p_guest, uint32_t p_len,
                      int p_reused, uint32_t prior_p) {
    char note[96];
    (void)p_len;
    snprintf(note, sizeof(note), "p_reused=%d prior_p=0x%X", p_reused, prior_p);
    row(p_reused ? "GAMELIST_P_REUSED" : "GAMELIST_P_ALLOCATED", module, helper, p_guest, 0, 0, 0,
        0, 0, 0, 0, note);
    if (p_guest) {
        row("GAMELIST_P_PUBLISHED", module, helper, p_guest, 0, 0, 0, 0, 0, 0, 0,
            p_reused ? "same_va_as_prior_package" : "fresh_or_distinct_va");
        g_b.last_p = p_guest;
    }
}

void e10a31b_note_chunk(const char *event, const char *module, uint32_t helper, uint32_t p_guest,
                        uint32_t chunk_guest, uint64_t old_mid, uint64_t new_mid,
                        const char *note) {
    row(event, module, helper, p_guest, chunk_guest, 0, 0, new_mid, old_mid, 0, 0, note);
}

void e10a31b_note_erw(const char *event, const char *module, uint32_t p_guest, uint32_t erw,
                      uint32_t erw_len, uint64_t module_id, const char *note) {
    int own = (strcmp(event, "GAMELIST_ERW_PUBLISHED") == 0) ? 1 : 0;
    row(event, module, 0, p_guest, 0, erw, erw_len, module_id, 0, erw, own, note);
}

void e10a31b_note_timer_arm(const char *module, uint32_t helper, uint32_t p_guest,
                            uint32_t chunk_guest, uint32_t erw, uint32_t registry_erw,
                            int own_erw) {
    row(own_erw ? "GAMELIST_TIMER_ARMED_WITH_OWN_CHUNK" : "GAMELIST_TIMER_ARMED_FOREIGN_ERW",
        module, helper, p_guest, chunk_guest, erw, 0, 0, 0, registry_erw, own_erw,
        own_erw ? "helper_p_chunk_erw_coherent" : "registry_erw_missing_or_foreign");
}
