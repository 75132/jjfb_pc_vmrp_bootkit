#include "gwy_launcher/gwy_sms_cfg.h"

#include "gwy_launcher/ext_chunk_provider.h"
#include "gwy_launcher/sha256.h"

#ifdef GWY_HAVE_UNICORN
#include <unicorn/unicorn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifndef MR_SUCCESS
#define MR_SUCCESS 0
#endif
#ifndef MR_FAILED
#define MR_FAILED (-1)
#endif

static GwySmsCfgState g_s;
static int g_known;
static int g_diag;
static int g_bootstrap;
static int g_applied_diag;
static int g_applied_bootstrap;
static void *g_uc;

static int env1(const char *k) {
    const char *e = getenv(k);
    return e && e[0] == '1';
}

static void ensure_flags(void) {
    if (g_known) return;
    g_known = 1;
    g_diag = env1("GWY_DIAG_SMSCFG_GPT_MINIMAL");
    g_bootstrap = env1("GWY_SMSCFG_BOOTSTRAP") || env1("JJFB_E10A31K_MODE") ||
                  env1("JJFB_E10A31L_MODE") || env1("JJFB_E10A31M_MODE");
    g_s.cfg_len = GWY_SMS_CFG_LEN;
    g_s.mr_version = GWY_SMS_CFG_MR_VERSION;
}

static int read_guest(void *uc, uint32_t addr, void *dst, size_t n) {
#ifdef GWY_HAVE_UNICORN
    if (!uc || !dst || !n) return 0;
    return uc_mem_read((uc_engine *)uc, addr, dst, n) == UC_ERR_OK;
#else
    (void)uc;
    (void)addr;
    (void)dst;
    (void)n;
    return 0;
#endif
}

static int write_guest(void *uc, uint32_t addr, const void *src, size_t n) {
#ifdef GWY_HAVE_UNICORN
    if (!uc || !src || !n) return 0;
    return uc_mem_write((uc_engine *)uc, addr, src, n) == UC_ERR_OK;
#else
    (void)uc;
    (void)addr;
    (void)src;
    (void)n;
    return 0;
#endif
}

static const char *source_name(GwySmsCfgSource s) {
    switch (s) {
        case GWY_SMSCFG_SRC_ZEROED:
            return "ZEROED";
        case GWY_SMSCFG_SRC_FILE:
            return "FILE";
        case GWY_SMSCFG_SRC_COMPAT_PROFILE:
            return "COMPAT_PROFILE";
        case GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY:
            return "DIAGNOSTIC_ONLY";
        case GWY_SMSCFG_SRC_GUEST_SET:
            return "GUEST_SET";
        default:
            return "NONE";
    }
}

static void hex_n(const uint8_t *b, int n, char *out, size_t out_n) {
    static const char *H = "0123456789ABCDEF";
    size_t i, w = 0;
    if (!out || out_n < 3) return;
    for (i = 0; i < (size_t)n && w + 2 < out_n; i++) {
        out[w++] = H[(b[i] >> 4) & 0xF];
        out[w++] = H[b[i] & 0xF];
    }
    out[w] = 0;
}

static void refresh_cfunction_sha(void) {
    const char *root = getenv("GWY_RESOURCE_ROOT");
    char path[640];
    uint8_t sha[32];
    if (root && root[0]) {
        snprintf(path, sizeof(path), "%s/cfunction.ext", root);
        if (gwy_sha256_file(path, sha) == 1) {
            memcpy(g_s.cfunction_sha256, sha, 32);
            return;
        }
        snprintf(path, sizeof(path), "%s/gwy/cfunction.ext", root);
        if (gwy_sha256_file(path, sha) == 1) {
            memcpy(g_s.cfunction_sha256, sha, 32);
            return;
        }
    }
    /* Runtime deploy copies cfunction.ext beside main.exe (cwd). */
    if (gwy_sha256_file("cfunction.ext", sha) == 1)
        memcpy(g_s.cfunction_sha256, sha, 32);
}

static void build_persistence_path(void) {
    const char *ov = getenv("GWY_OVERLAY_ROOT");
    if (!ov || !ov[0]) {
        g_s.persistence_path[0] = 0;
        return;
    }
    /* Never write into the original game tree — overlay only. */
    snprintf(g_s.persistence_path, sizeof(g_s.persistence_path), "%s/system/dsm.cfg", ov);
}

static int try_load_file_into(void *uc, uint32_t cfg_guest, char *used_path, size_t used_n) {
    const char *ov = getenv("GWY_OVERLAY_ROOT");
    char candidates[2][640];
    int i;
    if (!ov || !ov[0] || !uc || !cfg_guest) return 0;
    snprintf(candidates[0], sizeof(candidates[0]), "%s/system/dsm.cfg", ov);
    snprintf(candidates[1], sizeof(candidates[1]), "%s/dsm.cfg", ov);
    for (i = 0; i < 2; i++) {
        FILE *f = fopen(candidates[i], "rb");
        uint8_t tmp[GWY_SMS_CFG_LEN];
        size_t got;
        long sz;
        if (!f) continue;
        if (fseek(f, 0, SEEK_END) != 0) {
            fclose(f);
            continue;
        }
        sz = ftell(f);
        if (sz != (long)GWY_SMS_CFG_LEN) {
            fclose(f);
            continue;
        }
        if (fseek(f, 0, SEEK_SET) != 0) {
            fclose(f);
            continue;
        }
        memset(tmp, 0, sizeof(tmp));
        got = fread(tmp, 1, sizeof(tmp), f);
        fclose(f);
        if (got != GWY_SMS_CFG_LEN) continue;
        if (!write_guest(uc, cfg_guest, tmp, GWY_SMS_CFG_LEN)) continue;
        if (used_path && used_n)
            snprintf(used_path, used_n, "%s", candidates[i]);
        return 1;
    }
    return 0;
}

static int apply_compat_profile(void *uc, uint32_t cfg_guest) {
    const SmsCfgCompatProfile *prof;
    uint32_t i;
    refresh_cfunction_sha();
    prof = sms_cfg_compat_select(g_s.cfunction_sha256, g_s.mr_version, g_s.cfg_len);
    if (!prof || !uc || !cfg_guest) return 0;
    for (i = 0; i < prof->required_tag_count; i++) {
        const SmsCfgCompatTag *t = &prof->required_tags[i];
        if (!t->length || t->length > GWY_SMS_CFG_TAG_DATA_MAX) continue;
        if ((uint32_t)t->offset + t->length > g_s.cfg_len) continue;
        if (!write_guest(uc, cfg_guest + t->offset, t->data, t->length)) return 0;
    }
    snprintf(g_s.profile_id, sizeof(g_s.profile_id), "%s",
             prof->profile_id ? prof->profile_id : "unknown");
    return 1;
}

static void log_bootstrap(GwySmsCfgSource src) {
    char sha_hex[65];
    gwy_sha256_hex(g_s.cfunction_sha256, sha_hex);
    printf("[SMSCFG_BOOTSTRAP] profile=%s cfunction_sha256=%s mr_version=%u cfg_len=0x%X "
           "source=%s original_default_recovered=false cfg_guest=0x%X dsm_generation=%u "
           "evidence=OBSERVED\n",
           g_s.profile_id[0] ? g_s.profile_id : "-", sha_hex, g_s.mr_version, g_s.cfg_len,
           source_name(src), g_s.cfg_guest, g_s.dsm_generation);
    fflush(stdout);
}

static void zero_cfg(void *uc, uint32_t cfg_guest) {
    uint8_t z[256];
    uint32_t off = 0;
    memset(z, 0, sizeof(z));
    while (off < GWY_SMS_CFG_LEN) {
        uint32_t n = GWY_SMS_CFG_LEN - off;
        if (n > sizeof(z)) n = (uint32_t)sizeof(z);
        if (!write_guest(uc, cfg_guest + off, z, n)) break;
        off += n;
    }
}

void gwy_sms_cfg_reset(void) {
    memset(&g_s, 0, sizeof(g_s));
    g_known = 0;
    g_diag = 0;
    g_bootstrap = 0;
    g_applied_diag = 0;
    g_applied_bootstrap = 0;
    g_uc = NULL;
}

const GwySmsCfgState *gwy_sms_cfg_state(void) {
    ensure_flags();
    return &g_s;
}

int gwy_sms_cfg_diag_minimal_enabled(void) {
    ensure_flags();
    return g_diag;
}

int gwy_sms_cfg_bootstrap_enabled(void) {
    ensure_flags();
    return g_bootstrap;
}

int gwy_sms_cfg_resolve(void *uc, uint32_t erw_base, uint32_t erw_size, uint32_t mr_table) {
    uint32_t slot = 0;
    uint32_t cfg = 0;
    ensure_flags();
    if (uc) g_uc = uc;
    if (mr_table) g_s.mr_table = mr_table;
    if (!g_s.mr_table) g_s.mr_table = ext_chunk_provider_mr_table_guest();
    if (erw_base) {
        if (erw_base != g_s.erw_base) {
            g_s.dsm_generation++;
            g_s.cfunction_generation++;
            g_s.initialized = 0;
            g_applied_diag = 0;
            g_applied_bootstrap = 0;
        }
        g_s.erw_base = erw_base;
        g_s.erw_size = erw_size;
    }
    if (g_s.mr_table && g_uc) {
        if (read_guest(g_uc, g_s.mr_table + GWY_SMS_CFG_MR_TABLE_OFF, &slot, 4) && slot)
            cfg = slot;
    }
    if (!cfg && g_s.erw_base &&
        g_s.erw_size >= (GWY_SMS_CFG_ERW_OFF + GWY_SMS_CFG_LEN)) {
        cfg = g_s.erw_base + GWY_SMS_CFG_ERW_OFF;
    }
    if (!cfg) return 0;
    g_s.cfg_guest = cfg;
    g_s.cfg_len = GWY_SMS_CFG_LEN;
    build_persistence_path();
    return 1;
}

int32_t gwy_sms_cfg_get_bytes(void *uc, int32_t pos, void *dst, int32_t len) {
    ensure_flags();
    if (!uc) uc = g_uc;
    if (!uc || !dst || len <= 0 || pos < 0) return MR_FAILED;
    if (!g_s.cfg_guest) return MR_FAILED;
    if ((uint32_t)pos >= g_s.cfg_len || (uint32_t)pos + (uint32_t)len > g_s.cfg_len)
        return MR_FAILED;
    if (!read_guest(uc, g_s.cfg_guest + (uint32_t)pos, dst, (size_t)len)) return MR_FAILED;
    return MR_SUCCESS;
}

int32_t gwy_sms_cfg_set_bytes(void *uc, int32_t pos, const void *src, int32_t len,
                              GwySmsCfgSource source_tag) {
    ensure_flags();
    if (!uc) uc = g_uc;
    if (!uc || !src || len <= 0 || pos < 0) return MR_FAILED;
    if (!g_s.cfg_guest) return MR_FAILED;
    if ((uint32_t)pos >= g_s.cfg_len || (uint32_t)pos + (uint32_t)len > g_s.cfg_len)
        return MR_FAILED;
    if (!write_guest(uc, g_s.cfg_guest + (uint32_t)pos, src, (size_t)len)) return MR_FAILED;
    g_s.source = source_tag;
    if (source_tag != GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY &&
        source_tag != GWY_SMSCFG_SRC_COMPAT_PROFILE && source_tag != GWY_SMSCFG_SRC_FILE) {
        g_s.dirty = 1;
    }
    return MR_SUCCESS;
}

static int apply_diagnostic_gpt(void *uc) {
    uint8_t oldb[3] = {0, 0, 0};
    uint8_t gpt[3] = {'G', 'P', 'T'};
    char old_hex[16], new_hex[16];
    if (g_applied_diag || !g_s.cfg_guest || !uc) return 0;
    (void)gwy_sms_cfg_get_bytes(uc, (int32_t)GWY_SMS_CFG_GPT_OFF, oldb, 3);
    if (gwy_sms_cfg_set_bytes(uc, (int32_t)GWY_SMS_CFG_GPT_OFF, gpt, 3,
                              GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY) != MR_SUCCESS)
        return 0;
    g_s.dirty = 0; /* never auto-persist diagnostic A/B */
    g_applied_diag = 1;
    g_s.initialized = 1;
    hex_n(oldb, 3, old_hex, sizeof(old_hex));
    hex_n(gpt, 3, new_hex, sizeof(new_hex));
    printf("[SMSCFG_DIAG_SET] dsm_generation=%u cfg_base=0x%X offset=0x%X length=3 "
           "old=%s new=%s source=DIAGNOSTIC_ONLY evidence=OBSERVED\n",
           g_s.dsm_generation, g_s.cfg_guest, GWY_SMS_CFG_GPT_OFF, old_hex, new_hex);
    fflush(stdout);
    return 1;
}

static int apply_bootstrap(void *uc) {
    char used[640];
    GwySmsCfgSource src;
    if (g_applied_bootstrap || !g_s.cfg_guest || !uc) return 0;
    zero_cfg(uc, g_s.cfg_guest);
    g_s.source = GWY_SMSCFG_SRC_ZEROED;
    used[0] = 0;
    if (try_load_file_into(uc, g_s.cfg_guest, used, sizeof(used))) {
        src = GWY_SMSCFG_SRC_FILE;
        if (used[0])
            snprintf(g_s.persistence_path, sizeof(g_s.persistence_path), "%s", used);
        snprintf(g_s.profile_id, sizeof(g_s.profile_id), "file");
    } else if (apply_compat_profile(uc, g_s.cfg_guest)) {
        src = GWY_SMSCFG_SRC_COMPAT_PROFILE;
    } else {
        return 0;
    }
    g_s.source = src;
    g_s.dirty = 0;
    g_s.initialized = 1;
    g_applied_bootstrap = 1;
    log_bootstrap(src);
    /* Bootstrap-time APPLY_INT16 disabled: early write at 0x355 caused host crash
     * before method0 (see APPLY_HOST_CRASH_BEFORE_METHOD0). Controlled apply is
     * performed at method0 enter in e10a31m instead. */
    {
        uint8_t gpt[3] = {0, 0, 0};
        uint8_t gwy[3] = {0, 0, 0};
        char gpt_hex[16], gwy_hex[16];
        (void)read_guest(uc, g_s.cfg_guest + GWY_SMS_CFG_GPT_OFF, gpt, 3);
        (void)read_guest(uc, g_s.cfg_guest + 0x34Cu, gwy, 3);
        hex_n(gpt, 3, gpt_hex, sizeof(gpt_hex));
        hex_n(gwy, 3, gwy_hex, sizeof(gwy_hex));
        printf("[SMSCFG_BOOTSTRAP_COMPLETE] source=%s cfg_guest=0x%X gpt_off=0x%X "
               "gpt_hex=%s gwy_off=0x34C gwy_hex=%s evidence=OBSERVED\n",
               source_name(src), g_s.cfg_guest, GWY_SMS_CFG_GPT_OFF, gpt_hex, gwy_hex);
        fflush(stdout);
    }
    return 1;
}

void gwy_sms_cfg_on_erw(void *uc, uint32_t helper, uint32_t erw_base, uint32_t erw_size) {
    (void)helper;
    ensure_flags();
    if (!g_diag && !g_bootstrap) return;
    if (!erw_base || erw_size < (GWY_SMS_CFG_ERW_OFF + GWY_SMS_CFG_LEN)) return;
    /* Stick to the first DSM-sized ERW (cfunction). Do not rebind to later packages. */
    if (g_s.cfg_guest && g_s.erw_base && g_s.erw_base != erw_base &&
        (g_applied_diag || g_applied_bootstrap || g_s.initialized)) {
        return;
    }
    if (!gwy_sms_cfg_resolve(uc, erw_base, erw_size, 0)) return;
    /* Diagnostic A/B takes precedence when explicitly requested. */
    if (g_diag) {
        (void)apply_diagnostic_gpt(uc ? uc : g_uc);
        return;
    }
    if (g_bootstrap) (void)apply_bootstrap(uc ? uc : g_uc);
}

int gwy_sms_cfg_ensure_ready(void *uc) {
    uint32_t erw;
    ensure_flags();
    if (!g_diag && !g_bootstrap) return 0;
    if (uc) g_uc = uc;
    if (!g_s.cfg_guest) {
        erw = g_s.erw_base;
        if (!erw) {
            /* Prefer DSM cfunction ERW identity. */
            erw = 0; /* resolved only via prior on_erw */
        }
        if (!gwy_sms_cfg_resolve(g_uc, erw, g_s.erw_size, g_s.mr_table)) return 0;
    }
    if (g_diag) return apply_diagnostic_gpt(g_uc) || g_applied_diag;
    return apply_bootstrap(g_uc) || g_applied_bootstrap;
}

int32_t gwy_sms_cfg_load(void *uc) {
    ensure_flags();
    if (uc) g_uc = uc;
    if (!g_s.cfg_guest && !gwy_sms_cfg_resolve(g_uc, g_s.erw_base, g_s.erw_size, g_s.mr_table))
        return MR_FAILED;
    g_applied_bootstrap = 0;
    if (!apply_bootstrap(g_uc)) {
        /* Still succeed with zeroed buffer (upstream also falls back to defaults). */
        zero_cfg(g_uc, g_s.cfg_guest);
        if (apply_compat_profile(g_uc, g_s.cfg_guest)) {
            g_s.source = GWY_SMSCFG_SRC_COMPAT_PROFILE;
            g_s.initialized = 1;
            g_applied_bootstrap = 1;
            log_bootstrap(g_s.source);
        }
    }
    printf("[SMSCFG_LOAD] source=%s cfg_guest=0x%X evidence=OBSERVED\n", source_name(g_s.source),
           g_s.cfg_guest);
    fflush(stdout);
    return MR_SUCCESS;
}

int32_t gwy_sms_cfg_save(void *uc) {
    FILE *f;
    char tmp_path[560];
    uint8_t buf[GWY_SMS_CFG_LEN];
    ensure_flags();
    if (uc) g_uc = uc;
    if (!g_s.cfg_guest) return MR_FAILED;
    if (g_s.source == GWY_SMSCFG_SRC_DIAGNOSTIC_ONLY) {
        printf("[SMSCFG_SAVE] skipped=diagnostic_only evidence=OBSERVED\n");
        fflush(stdout);
        return MR_SUCCESS;
    }
    if (!g_s.dirty) return MR_SUCCESS;
    build_persistence_path();
    if (!g_s.persistence_path[0]) return MR_FAILED;
    if (!read_guest(g_uc, g_s.cfg_guest, buf, sizeof(buf))) return MR_FAILED;
    {
        /* Ensure overlay/system exists (best-effort). */
        char dir[512];
        const char *ov = getenv("GWY_OVERLAY_ROOT");
        if (ov && ov[0]) {
            snprintf(dir, sizeof(dir), "%s/system", ov);
#ifdef _WIN32
            (void)_mkdir(dir);
#else
            (void)mkdir(dir, 0755);
#endif
        }
    }
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_s.persistence_path);
    f = fopen(tmp_path, "wb");
    if (!f) return MR_FAILED;
    if (fwrite(buf, 1, sizeof(buf), f) != sizeof(buf)) {
        fclose(f);
        remove(tmp_path);
        return MR_FAILED;
    }
    fclose(f);
    remove(g_s.persistence_path);
    if (rename(tmp_path, g_s.persistence_path) != 0) {
        remove(tmp_path);
        return MR_FAILED;
    }
    g_s.dirty = 0;
    printf("[SMSCFG_SAVE] path=%s len=0x%X evidence=OBSERVED\n", g_s.persistence_path,
           GWY_SMS_CFG_LEN);
    fflush(stdout);
    return MR_SUCCESS;
}