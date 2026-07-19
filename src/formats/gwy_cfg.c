#include "gwy_launcher/gwy_cfg.h"
#include "gwy_launcher/sha256.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t be24(const uint8_t *p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

static void copy_cstr_field(GwyCfgFieldString *f,
                            const uint8_t *rec,
                            uint32_t offset,
                            uint32_t max_len,
                            const char *encoding,
                            const char *confidence) {
    size_t i;
    f->present = true;
    f->offset = offset;
    f->length = max_len;
    f->encoding = encoding;
    f->confidence = confidence;
    memset(f->value, 0, sizeof(f->value));
    for (i = 0; i < max_len && i + 1 < sizeof(f->value); i++) {
        if (rec[offset + i] == 0) break;
        f->value[i] = (char)rec[offset + i];
    }
}

static int find_target_ascii(const uint8_t *rec, size_t rec_len, char *out, size_t out_sz, uint32_t *off_out) {
    static const char *needle = "gwy/";
    size_t i;
    for (i = 0; i + 4 < rec_len; i++) {
        if (memcmp(rec + i, needle, 4) == 0) {
            size_t j = i;
            size_t n = 0;
            while (j < rec_len && rec[j] != 0 && n + 1 < out_sz) {
                char c = (char)rec[j];
                if (!(isalnum((unsigned char)c) || c == '/' || c == '.' || c == '_' || c == '-')) break;
                out[n++] = c;
                j++;
            }
            out[n] = '\0';
            if (n > 4 && strstr(out, ".mrp") != NULL) {
                *off_out = (uint32_t)i;
                return 1;
            }
        }
    }
    return 0;
}

static LauncherStatus read_entire_file(const char *path, uint8_t **out_data, size_t *out_size, LauncherError *err) {
    FILE *fp;
    long sz;
    uint8_t *buf;
    size_t nread;

    fp = fopen(path, "rb");
    if (!fp) {
        launcher_error_set(err, L_ERR_IO, "gwy_cfg", "failed to open file", path);
        return L_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (sz = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        launcher_error_set(err, L_ERR_IO, "gwy_cfg", "seek/tell failed", path);
        return L_ERR_IO;
    }
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(fp);
        launcher_error_set(err, L_ERR_IO, "gwy_cfg", "out of memory", path);
        return L_ERR_IO;
    }
    nread = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (nread != (size_t)sz) {
        free(buf);
        launcher_error_set(err, L_ERR_IO, "gwy_cfg", "short read", path);
        return L_ERR_IO;
    }
    *out_data = buf;
    *out_size = (size_t)sz;
    return L_OK;
}

LauncherStatus gwy_cfg_open(const char *path, GwyCfgFile **out, LauncherError *err) {
    GwyCfgFile *cfg;
    LauncherStatus st;

    launcher_error_clear(err);
    if (!path || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "gwy_cfg", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out = NULL;
    cfg = (GwyCfgFile *)calloc(1, sizeof(*cfg));
    if (!cfg) {
        launcher_error_set(err, L_ERR_IO, "gwy_cfg", "out of memory", path);
        return L_ERR_IO;
    }
    snprintf(cfg->path, sizeof(cfg->path), "%s", path);
    st = read_entire_file(path, &cfg->data, &cfg->size, err);
    if (st != L_OK) {
        free(cfg);
        return st;
    }
    if (cfg->size < GWY_CFG_RECORD_BASE + GWY_CFG_RECORD_SIZE) {
        launcher_error_set(err, L_ERR_FORMAT, "gwy_cfg", "cfg too small for record framing", path);
        gwy_cfg_close(cfg);
        return L_ERR_FORMAT;
    }
    gwy_sha256(cfg->data, cfg->size, cfg->sha256);
    *out = cfg;
    return L_OK;
}

void gwy_cfg_close(GwyCfgFile *cfg) {
    if (!cfg) return;
    free(cfg->data);
    free(cfg);
}

LauncherStatus gwy_cfg_read_record(const GwyCfgFile *cfg,
                                   uint32_t index,
                                   GwyCfgRecord *out,
                                   LauncherError *err) {
    uint32_t off;
    const uint8_t *rec;
    char target[128];
    uint32_t target_off = 0;

    launcher_error_clear(err);
    if (!cfg || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "gwy_cfg", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    off = GWY_CFG_RECORD_BASE + index * GWY_CFG_RECORD_SIZE;
    if ((size_t)off + GWY_CFG_RECORD_SIZE > cfg->size) {
        launcher_error_set(err, L_ERR_BOUNDS, "gwy_cfg", "record outside file", cfg->path);
        return L_ERR_BOUNDS;
    }
    rec = cfg->data + off;
    out->index = index;
    out->file_offset = off;
    out->record_size = GWY_CFG_RECORD_SIZE;
    out->layout_confidence = "empirical_record36";
    memcpy(out->raw, rec, GWY_CFG_RECORD_SIZE);

    copy_cstr_field(&out->icon, rec, 0x40, 0x18, "ascii", "empirical_record36");

    out->napptype.present = true;
    out->napptype.value = rec[0x57];
    out->napptype.offset = 0x57;
    out->napptype.length = 1;
    out->napptype.encoding = "u8";
    out->napptype.confidence = "empirical_record36";

    /* Title suffix is UTF-16BE in [0x5C,0x70). Keep as escaped hex-ish printable latin1 fallback for CLI;
       store raw bytes decoded conservatively as UTF-16BE code units into ASCII-safe buffer. */
    {
        size_t o = 0;
        uint32_t p;
        out->title_suffix.present = true;
        out->title_suffix.offset = 0x5C;
        out->title_suffix.length = 0x14;
        out->title_suffix.encoding = "utf-16be";
        out->title_suffix.confidence = "empirical_record36";
        memset(out->title_suffix.value, 0, sizeof(out->title_suffix.value));
        for (p = 0x5C; p + 1 < 0x70 && o + 1 < sizeof(out->title_suffix.value); p += 2) {
            uint16_t cu = ((uint16_t)rec[p] << 8) | (uint16_t)rec[p + 1];
            if (cu == 0) break;
            if (cu < 128) {
                out->title_suffix.value[o++] = (char)cu;
            } else {
                /* keep non-ASCII as '?' in this minimal buffer; full Unicode later */
                out->title_suffix.value[o++] = '?';
            }
        }
    }

    out->nextid.present = true;
    out->nextid.value = be24(rec + 0x72);
    out->nextid.offset = 0x72;
    out->nextid.length = 3;
    out->nextid.encoding = "be24";
    out->nextid.confidence = "empirical_record36";

    out->ncode.present = true;
    out->ncode.value = be24(rec + 0x78);
    out->ncode.offset = 0x78;
    out->ncode.length = 3;
    out->ncode.encoding = "be24";
    out->ncode.confidence = "empirical_record36";

    out->narg.present = true;
    out->narg.value = be24(rec + 0x7B);
    out->narg.offset = 0x7B;
    out->narg.length = 3;
    out->narg.encoding = "be24";
    out->narg.confidence = "empirical_record36";

    out->narg1.present = true;
    out->narg1.value = rec[0x7E];
    out->narg1.offset = 0x7E;
    out->narg1.length = 1;
    out->narg1.encoding = "u8";
    out->narg1.confidence = "empirical_record36";

    if (find_target_ascii(rec, GWY_CFG_RECORD_SIZE, target, sizeof(target), &target_off)) {
        out->target_mrp.present = true;
        out->target_mrp.offset = target_off;
        out->target_mrp.length = (uint32_t)strlen(target);
        out->target_mrp.encoding = "ascii";
        out->target_mrp.confidence = "empirical_record36";
        snprintf(out->target_mrp.value, sizeof(out->target_mrp.value), "%s", target);
    }

    return L_OK;
}
