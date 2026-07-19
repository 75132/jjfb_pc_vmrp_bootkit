#include "gwy_launcher/gwy_cfg.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *k_cfg_sha =
    "0d5837d251e9e0bbfb82495c0b73fd32ea4bcb5d817ca55495c35a23e8b7a806";

static const char *fixture_cfg(void) {
    const char *root = getenv("GWY_FIXTURE_ROOT");
    static char path[1024];
    if (!root || !root[0]) {
        fprintf(stderr, "GWY_FIXTURE_ROOT not set\n");
        return NULL;
    }
    snprintf(path, sizeof(path), "%s/gwy/cfg.bin", root);
    return path;
}

int main(void) {
    const char *path;
    GwyCfgFile *cfg = NULL;
    GwyCfgRecord rec;
    LauncherError err;
    LauncherStatus st;
    char hex[65];

    path = fixture_cfg();
    if (!path) return 1;

    st = gwy_cfg_open(path, &cfg, &err);
    if (st != L_OK) {
        fprintf(stderr, "cfg open failed: %s\n", err.message);
        return 1;
    }
    gwy_sha256_hex(cfg->sha256, hex);
    if (strcmp(hex, k_cfg_sha) != 0) {
        fprintf(stderr, "cfg sha mismatch %s\n", hex);
        gwy_cfg_close(cfg);
        return 1;
    }

    st = gwy_cfg_read_record(cfg, 36, &rec, &err);
    if (st != L_OK) {
        fprintf(stderr, "read record failed: %s\n", err.message);
        gwy_cfg_close(cfg);
        return 1;
    }
    if (rec.file_offset != 10816 || rec.record_size != 272) {
        fprintf(stderr, "framing mismatch off=%u size=%u\n", rec.file_offset, rec.record_size);
        gwy_cfg_close(cfg);
        return 1;
    }
    if (!rec.icon.present || strcmp(rec.icon.value, "ng_jjfb.gif") != 0) {
        fprintf(stderr, "icon mismatch '%s'\n", rec.icon.value);
        gwy_cfg_close(cfg);
        return 1;
    }
    if (!rec.napptype.present || rec.napptype.value != 12) {
        fprintf(stderr, "napptype mismatch\n");
        gwy_cfg_close(cfg);
        return 1;
    }
    if (!rec.nextid.present || rec.nextid.value != 482) {
        fprintf(stderr, "nextid mismatch %u\n", rec.nextid.value);
        gwy_cfg_close(cfg);
        return 1;
    }
    if (!rec.ncode.present || rec.ncode.value != 512) {
        fprintf(stderr, "ncode mismatch %u\n", rec.ncode.value);
        gwy_cfg_close(cfg);
        return 1;
    }
    if (!rec.narg.present || rec.narg.value != 0) {
        fprintf(stderr, "narg mismatch\n");
        gwy_cfg_close(cfg);
        return 1;
    }
    if (!rec.narg1.present || rec.narg1.value != 1) {
        fprintf(stderr, "narg1 mismatch\n");
        gwy_cfg_close(cfg);
        return 1;
    }
    if (!rec.target_mrp.present || strcmp(rec.target_mrp.value, "gwy/jjfb.mrp") != 0) {
        fprintf(stderr, "target mismatch '%s'\n", rec.target_mrp.value);
        gwy_cfg_close(cfg);
        return 1;
    }

    /* fail-closed: out of range index */
    st = gwy_cfg_read_record(cfg, 999999, &rec, &err);
    if (st != L_ERR_BOUNDS) {
        fprintf(stderr, "expected BOUNDS for huge index\n");
        gwy_cfg_close(cfg);
        return 1;
    }

    gwy_cfg_close(cfg);
    puts("test_cfg36 OK");
    return 0;
}
