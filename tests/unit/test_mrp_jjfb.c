#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *k_jjfb_sha =
    "52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036";

static char *join3(const char *a, const char *b, const char *c) {
    size_t n = strlen(a) + strlen(b) + strlen(c) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    snprintf(p, n, "%s%s%s", a, b, c);
    return p;
}

static const char *fixture_jjfb(void) {
    const char *root = getenv("GWY_FIXTURE_ROOT");
    static char path[1024];
    if (!root || !root[0]) {
        fprintf(stderr, "GWY_FIXTURE_ROOT not set\n");
        return NULL;
    }
    snprintf(path, sizeof(path), "%s/gwy/jjfb.mrp", root);
    return path;
}

static int expect_decoded(const MrpArchive *a, const char *name, size_t expect_len) {
    const MrpMember *m = NULL;
    ByteBuffer buf;
    LauncherError err;
    LauncherStatus st;

    st = mrp_archive_find_exact(a, name, &m, &err);
    if (st != L_OK) {
        fprintf(stderr, "find %s failed: %s\n", name, err.message);
        return 0;
    }
    st = mrp_archive_decode_member(a, m, 4u * 1024u * 1024u, &buf, &err);
    if (st != L_OK) {
        fprintf(stderr, "decode %s failed: %s\n", name, err.message);
        return 0;
    }
    if (buf.size != expect_len) {
        fprintf(stderr, "decode %s len=%zu expect=%zu\n", name, buf.size, expect_len);
        byte_buffer_free(&buf);
        return 0;
    }
    byte_buffer_free(&buf);
    return 1;
}

int main(void) {
    const char *path;
    MrpArchive *a = NULL;
    LauncherError err;
    char hex[65];
    const MrpMember *m = NULL;
    LauncherStatus st;
    uint8_t badmagic[16];

    path = fixture_jjfb();
    if (!path) return 1;

    st = mrp_archive_open(path, &a, &err);
    if (st != L_OK) {
        fprintf(stderr, "open failed: %s %s\n", err.message, err.detail);
        return 1;
    }
    gwy_sha256_hex(a->sha256, hex);
    if (strcmp(hex, k_jjfb_sha) != 0) {
        fprintf(stderr, "sha mismatch %s\n", hex);
        mrp_archive_close(a);
        return 1;
    }
    if (a->appid_le != 400101 || a->appver_le != 12) {
        fprintf(stderr, "appid/appver mismatch %u/%u\n", a->appid_le, a->appver_le);
        mrp_archive_close(a);
        return 1;
    }
    if (a->header_length != 240 || a->first_data_offset != 1892) {
        fprintf(stderr, "header/index boundary mismatch %u/%u\n",
                a->header_length, a->first_data_offset);
        mrp_archive_close(a);
        return 1;
    }
    if (!expect_decoded(a, "start.mr", 3787)) { mrp_archive_close(a); return 1; }
    if (!expect_decoded(a, "mrc_loader.ext", 232)) { mrp_archive_close(a); return 1; }
    if (!expect_decoded(a, "robotol.ext", 253420)) { mrp_archive_close(a); return 1; }

    st = mrp_archive_find_exact(a, "cfunction.ext", &m, &err);
    if (st != L_ERR_NOT_FOUND) {
        fprintf(stderr, "cfunction.ext should miss exact lookup\n");
        mrp_archive_close(a);
        return 1;
    }

    mrp_archive_close(a);

    /* malformed: bad magic */
    {
        char *tmp = join3(getenv("TEMP") ? getenv("TEMP") : ".", "\\", "gwy_bad_magic.mrp");
        FILE *fp;
        if (!tmp) return 1;
        memset(badmagic, 0, sizeof(badmagic));
        memcpy(badmagic, "XXXX", 4);
        fp = fopen(tmp, "wb");
        if (!fp) { free(tmp); return 1; }
        fwrite(badmagic, 1, sizeof(badmagic), fp);
        fclose(fp);
        st = mrp_archive_open(tmp, &a, &err);
        remove(tmp);
        free(tmp);
        if (st != L_ERR_FORMAT) {
            fprintf(stderr, "expected FORMAT on bad magic, got %s\n", launcher_status_name(st));
            if (a) mrp_archive_close(a);
            return 1;
        }
    }

    puts("test_mrp_jjfb OK");
    return 0;
}
