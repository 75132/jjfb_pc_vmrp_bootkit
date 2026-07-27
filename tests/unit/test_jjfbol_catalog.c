#include "gwy_launcher/jjfbol_catalog.h"
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
#include <unistd.h>
#endif

static void wr_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

static void wr_u32_be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xff);
    p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >> 8) & 0xff);
    p[3] = (uint8_t)(v & 0xff);
}

static int write_file(const char *path, const void *data, size_t n) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    if (fwrite(data, 1, n, fp) != n) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

static int mk_dir(const char *path) {
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path, 0755) == 0;
#endif
}

/* Minimal one-member MRP: member "foo.bmp", 16-byte payload. */
static int write_mini_mrp(const char *path) {
    uint8_t buf[296];
    const char *name = "foo.bmp";
    uint32_t name_len = 8; /* incl NUL */
    uint32_t file_start = 256;
    uint32_t payload_off = 280;
    uint32_t payload_sz = 16;
    uint32_t total = 296;
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "MRPG", 4);
    wr_u32_le(buf + 4, file_start);
    wr_u32_le(buf + 8, total);
    wr_u32_le(buf + 12, 240);
    memcpy(buf + 16, "test", 4);
    wr_u32_le(buf + 0x44, 1);
    wr_u32_le(buf + 0x48, 1);
    wr_u32_be(buf + 0xC0, 1);
    wr_u32_be(buf + 0xC4, 1);
    /* index @ 240 */
    wr_u32_le(buf + 240, name_len);
    memcpy(buf + 244, name, 7);
    buf[251] = 0;
    wr_u32_le(buf + 252, payload_off);
    wr_u32_le(buf + 256, payload_sz);
    wr_u32_le(buf + 260, 0);
    /* data record @ 264 */
    wr_u32_le(buf + 264, name_len);
    memcpy(buf + 268, name, 7);
    buf[275] = 0;
    wr_u32_le(buf + 276, payload_sz);
    memset(buf + 280, 0xAB, 16);
    return write_file(path, buf, total);
}

int main(void) {
    char tmp[512];
    char myth[512];
    char gwy[512];
    char jjfbol[512];
    char mrp[512];
    char ver[512];
    char down[512];
    uint8_t be[4];
    LauncherError err;
    JjfbolLookupResult lr;
    const char *td;

#ifdef _WIN32
    char tmpdir[MAX_PATH];
    DWORD n = GetTempPathA(sizeof(tmpdir), tmpdir);
    if (!n) return 1;
    snprintf(tmp, sizeof(tmp), "%sjjfbol_cat_%lu", tmpdir, (unsigned long)GetTickCount());
#else
    snprintf(tmp, sizeof(tmp), "/tmp/jjfbol_cat_%d", (int)getpid());
#endif
    td = tmp;
    snprintf(myth, sizeof(myth), "%s/mythroad_root", td);
    snprintf(gwy, sizeof(gwy), "%s/gwy", myth);
    snprintf(jjfbol, sizeof(jjfbol), "%s/jjfbol", gwy);
    if (!mk_dir(td) || !mk_dir(myth) || !mk_dir(gwy) || !mk_dir(jjfbol)) {
        fprintf(stderr, "mkdir fail %s\n", td);
        return 1;
    }
    snprintf(mrp, sizeof(mrp), "%s/pack1.mrp", jjfbol);
    if (!write_mini_mrp(mrp)) {
        fprintf(stderr, "write mrp fail\n");
        return 1;
    }
    snprintf(ver, sizeof(ver), "%s/pack1.mrp.v", jjfbol);
    wr_u32_be(be, 0x1234);
    if (!write_file(ver, be, 4)) return 1;
    snprintf(down, sizeof(down), "%s/downVersion", jjfbol);
    wr_u32_be(be, 1006);
    if (!write_file(down, be, 4)) return 1;
    snprintf(ver, sizeof(ver), "%s/downVersion.v", jjfbol);
    wr_u32_be(be, 1006);
    if (!write_file(ver, be, 4)) return 1;

    if (jjfbol_catalog_init(myth, &err) != L_OK) {
        fprintf(stderr, "init fail: %s %s\n", err.message, err.detail);
        return 1;
    }
    if (!jjfbol_catalog_ready()) return 1;
    if (jjfbol_catalog_package_count() != 1) {
        fprintf(stderr, "packages=%u\n", jjfbol_catalog_package_count());
        return 1;
    }
    if (jjfbol_catalog_down_version() != 1006) {
        fprintf(stderr, "down=%u\n", jjfbol_catalog_down_version());
        return 1;
    }
    if (jjfbol_catalog_lookup_exact("foo.bmp", &lr) != JJFBOL_LOOKUP_UNIQUE) {
        fprintf(stderr, "lookup kind=%d hits=%u\n", (int)lr.kind, lr.hit_count);
        return 1;
    }
    jjfbol_catalog_reset();
    printf("test_jjfbol_catalog OK\n");
    return 0;
}
