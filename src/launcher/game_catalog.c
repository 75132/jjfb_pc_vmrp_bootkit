#include "gwy_launcher/game_catalog.h"
#include "gwy_launcher/gwy_cfg.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/utf.h"
#include <stdio.h>
#include <string.h>

static int is_printable_title(const char *s) {
    size_t n = 0;
    size_t i;
    if (!s || !s[0]) return 0;
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20) return 0;
        n++;
    }
    return n >= 2;
}

static int score_entry(const GwyCfgRecord *rec) {
    int score = 0;
    if (rec->icon.present && strstr(rec->icon.value, ".gif")) score += 50;
    if (rec->napptype.present && rec->napptype.value == 12) score += 40;
    if (rec->title_suffix.present && is_printable_title(rec->title_suffix.value)) score += 20;
    if (rec->nextid.present && rec->nextid.value > 0) score += 5;
    if (rec->ncode.present && rec->ncode.value > 0) score += 5;
    return score;
}

static void fill_title_utf8(const GwyCfgRecord *rec, char *out, size_t out_cap) {
    out[0] = '\0';
    if (!rec->title_suffix.present) return;
    /* title_suffix.value currently stores ASCII/? fallback; re-decode from raw */
    gwy_utf16be_to_utf8(rec->raw + 0x5C, 0x14, out, out_cap);
    if (!is_printable_title(out)) out[0] = '\0';
}

static int path_join(char *out, size_t cap, const char *root, const char *rel) {
    size_t n;
    if (!out || !root || !rel || cap == 0) return 0;
    n = snprintf(out, cap, "%s/%s", root, rel);
    return n > 0 && (size_t)n < cap;
}

static void basename_of(const char *path, char *out, size_t cap) {
    const char *p = path;
    const char *slash = path;
    while (*p) {
        if (*p == '/' || *p == '\\') slash = p + 1;
        p++;
    }
    snprintf(out, cap, "%s", slash);
}

LauncherStatus gwy_game_catalog_scan(const char *resource_root,
                                     GwyGameCatalog *out,
                                     LauncherError *err) {
    char cfg_path[1024];
    GwyCfgFile *cfg = NULL;
    LauncherStatus st;
    uint32_t max_index;
    uint32_t i;

    launcher_error_clear(err);
    if (!resource_root || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "game_catalog", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->resource_root, sizeof(out->resource_root), "%s", resource_root);
    if (!path_join(cfg_path, sizeof(cfg_path), resource_root, "gwy/cfg.bin")) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "game_catalog", "path too long", resource_root);
        return L_ERR_INVALID_ARGUMENT;
    }

    st = gwy_cfg_open(cfg_path, &cfg, err);
    if (st != L_OK) return st;

    if (cfg->size < GWY_CFG_RECORD_BASE + GWY_CFG_RECORD_SIZE) {
        gwy_cfg_close(cfg);
        launcher_error_set(err, L_ERR_FORMAT, "game_catalog", "cfg too small", cfg_path);
        return L_ERR_FORMAT;
    }
    max_index = (uint32_t)((cfg->size - GWY_CFG_RECORD_BASE) / GWY_CFG_RECORD_SIZE);

    for (i = 0; i < max_index; i++) {
        GwyCfgRecord rec;
        GwyGameEntry cand;
        size_t j;
        int prefer;
        char host_mrp[1024];

        st = gwy_cfg_read_record(cfg, i, &rec, err);
        if (st != L_OK) continue;
        if (!rec.target_mrp.present || !rec.target_mrp.value[0]) continue;

        memset(&cand, 0, sizeof(cand));
        cand.cfg_index = i;
        cand.napptype = rec.napptype.present ? (int32_t)rec.napptype.value : 0;
        cand.nextid = rec.nextid.present ? (int32_t)rec.nextid.value : 0;
        cand.ncode = rec.ncode.present ? (int32_t)rec.ncode.value : 0;
        cand.narg = rec.narg.present ? (int32_t)rec.narg.value : 0;
        cand.narg1 = rec.narg1.present ? (int32_t)rec.narg1.value : 0;
        snprintf(cand.target_mrp, sizeof(cand.target_mrp), "%s", rec.target_mrp.value);
        if (rec.icon.present) snprintf(cand.icon, sizeof(cand.icon), "%s", rec.icon.value);
        fill_title_utf8(&rec, cand.title_utf8, sizeof(cand.title_utf8));
        prefer = score_entry(&rec);
        cand.selected_prefer = prefer;

        if (path_join(host_mrp, sizeof(host_mrp), resource_root, cand.target_mrp)) {
            MrpArchive *a = NULL;
            LauncherError merr;
            if (mrp_archive_open(host_mrp, &a, &merr) == L_OK) {
                char gbk[64];
                cand.mrp_exists = 1;
                cand.appid = a->appid_le;
                cand.appver = a->appver_le;
                /* display name at offset 28, 24 bytes GBK */
                memset(gbk, 0, sizeof(gbk));
                memcpy(gbk, a->data + 28, 24);
                gwy_gbk_to_utf8(gbk, cand.display_name_utf8, sizeof(cand.display_name_utf8));
                mrp_archive_close(a);
            }
        }

        if (!cand.mrp_exists) {
            /* Still list missing targets lightly; skip obvious garbage wapgame spam later. */
        }

        /* Deduplicate by target: keep higher score / existing MRP. */
        for (j = 0; j < out->count; j++) {
            if (strcmp(out->entries[j].target_mrp, cand.target_mrp) == 0) {
                int old = out->entries[j].selected_prefer + (out->entries[j].mrp_exists ? 100 : 0);
                int neu = cand.selected_prefer + (cand.mrp_exists ? 100 : 0);
                if (neu >= old) out->entries[j] = cand;
                goto next_record;
            }
        }
        if (out->count < GWY_GAME_MAX) {
            if (!cand.title_utf8[0] && !cand.display_name_utf8[0]) {
                basename_of(cand.target_mrp, cand.title_utf8, sizeof(cand.title_utf8));
            }
            out->entries[out->count++] = cand;
        }
    next_record:
        (void)0;
    }

    gwy_cfg_close(cfg);

    /* Drop missing MRP entries to keep the list usable. */
    {
        size_t w = 0;
        size_t r;
        for (r = 0; r < out->count; r++) {
            if (out->entries[r].mrp_exists) {
                out->entries[w++] = out->entries[r];
            }
        }
        out->count = w;
    }

    if (out->count == 0) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "game_catalog", "no playable games in cfg", cfg_path);
        return L_ERR_NOT_FOUND;
    }
    return L_OK;
}
