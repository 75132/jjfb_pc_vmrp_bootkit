#include "gwy_launcher/platform_identity.h"
#include "gwy_launcher/sha256.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Compact MD5 (public-domain style) for Mythroad key digests only. */

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} Md5Ctx;

static uint32_t md5_F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
static uint32_t md5_G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
static uint32_t md5_H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static uint32_t md5_I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
static uint32_t md5_rot(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void md5_FF(uint32_t *a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, int s, uint32_t ac) {
    *a += md5_F(b, c, d) + x + ac;
    *a = md5_rot(*a, s);
    *a += b;
}
static void md5_GG(uint32_t *a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, int s, uint32_t ac) {
    *a += md5_G(b, c, d) + x + ac;
    *a = md5_rot(*a, s);
    *a += b;
}
static void md5_HH(uint32_t *a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, int s, uint32_t ac) {
    *a += md5_H(b, c, d) + x + ac;
    *a = md5_rot(*a, s);
    *a += b;
}
static void md5_II(uint32_t *a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, int s, uint32_t ac) {
    *a += md5_I(b, c, d) + x + ac;
    *a = md5_rot(*a, s);
    *a += b;
}

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    int i;
    for (i = 0; i < 16; i++) {
        x[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) | ((uint32_t)block[i * 4 + 3] << 24);
    }
    md5_FF(&a,b,c,d,x[ 0], 7,0xd76aa478); md5_FF(&d,a,b,c,x[ 1],12,0xe8c7b756);
    md5_FF(&c,d,a,b,x[ 2],17,0x242070db); md5_FF(&b,c,d,a,x[ 3],22,0xc1bdceee);
    md5_FF(&a,b,c,d,x[ 4], 7,0xf57c0faf); md5_FF(&d,a,b,c,x[ 5],12,0x4787c62a);
    md5_FF(&c,d,a,b,x[ 6],17,0xa8304613); md5_FF(&b,c,d,a,x[ 7],22,0xfd469501);
    md5_FF(&a,b,c,d,x[ 8], 7,0x698098d8); md5_FF(&d,a,b,c,x[ 9],12,0x8b44f7af);
    md5_FF(&c,d,a,b,x[10],17,0xffff5bb1); md5_FF(&b,c,d,a,x[11],22,0x895cd7be);
    md5_FF(&a,b,c,d,x[12], 7,0x6b901122); md5_FF(&d,a,b,c,x[13],12,0xfd987193);
    md5_FF(&c,d,a,b,x[14],17,0xa679438e); md5_FF(&b,c,d,a,x[15],22,0x49b40821);
    md5_GG(&a,b,c,d,x[ 1], 5,0xf61e2562); md5_GG(&d,a,b,c,x[ 6], 9,0xc040b340);
    md5_GG(&c,d,a,b,x[11],14,0x265e5a51); md5_GG(&b,c,d,a,x[ 0],20,0xe9b6c7aa);
    md5_GG(&a,b,c,d,x[ 5], 5,0xd62f105d); md5_GG(&d,a,b,c,x[10], 9,0x02441453);
    md5_GG(&c,d,a,b,x[15],14,0xd8a1e681); md5_GG(&b,c,d,a,x[ 4],20,0xe7d3fbc8);
    md5_GG(&a,b,c,d,x[ 9], 5,0x21e1cde6); md5_GG(&d,a,b,c,x[14], 9,0xc33707d6);
    md5_GG(&c,d,a,b,x[ 3],14,0xf4d50d87); md5_GG(&b,c,d,a,x[ 8],20,0x455a14ed);
    md5_GG(&a,b,c,d,x[13], 5,0xa9e3e905); md5_GG(&d,a,b,c,x[ 2], 9,0xfcefa3f8);
    md5_GG(&c,d,a,b,x[ 7],14,0x676f02d9); md5_GG(&b,c,d,a,x[12],20,0x8d2a4c8a);
    md5_HH(&a,b,c,d,x[ 5], 4,0xfffa3942); md5_HH(&d,a,b,c,x[ 8],11,0x8771f681);
    md5_HH(&c,d,a,b,x[11],16,0x6d9d6122); md5_HH(&b,c,d,a,x[14],23,0xfde5380c);
    md5_HH(&a,b,c,d,x[ 1], 4,0xa4beea44); md5_HH(&d,a,b,c,x[ 4],11,0x4bdecfa9);
    md5_HH(&c,d,a,b,x[ 7],16,0xf6bb4b60); md5_HH(&b,c,d,a,x[10],23,0xbebfbc70);
    md5_HH(&a,b,c,d,x[13], 4,0x289b7ec6); md5_HH(&d,a,b,c,x[ 0],11,0xeaa127fa);
    md5_HH(&c,d,a,b,x[ 3],16,0xd4ef3085); md5_HH(&b,c,d,a,x[ 6],23,0x04881d05);
    md5_HH(&a,b,c,d,x[ 9], 4,0xd9d4d039); md5_HH(&d,a,b,c,x[12],11,0xe6db99e5);
    md5_HH(&c,d,a,b,x[15],16,0x1fa27cf8); md5_HH(&b,c,d,a,x[ 2],23,0xc4ac5665);
    md5_II(&a,b,c,d,x[ 0], 6,0xf4292244); md5_II(&d,a,b,c,x[ 7],10,0x432aff97);
    md5_II(&c,d,a,b,x[14],15,0xab9423a7); md5_II(&b,c,d,a,x[ 5],21,0xfc93a039);
    md5_II(&a,b,c,d,x[12], 6,0x655b59c3); md5_II(&d,a,b,c,x[ 3],10,0x8f0ccc92);
    md5_II(&c,d,a,b,x[10],15,0xffeff47d); md5_II(&b,c,d,a,x[ 1],21,0x85845dd1);
    md5_II(&a,b,c,d,x[ 8], 6,0x6fa87e4f); md5_II(&d,a,b,c,x[15],10,0xfe2ce6e0);
    md5_II(&c,d,a,b,x[ 6],15,0xa3014314); md5_II(&b,c,d,a,x[13],21,0x4e0811a1);
    md5_II(&a,b,c,d,x[ 4], 6,0xf7537e82); md5_II(&d,a,b,c,x[11],10,0xbd3af235);
    md5_II(&c,d,a,b,x[ 2],15,0x2ad7d2bb); md5_II(&b,c,d,a,x[ 9],21,0xeb86d391);
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static void md5_init(Md5Ctx *ctx) {
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

static void md5_update(Md5Ctx *ctx, const uint8_t *input, size_t len) {
    size_t i, index, partLen;
    index = (size_t)((ctx->count[0] >> 3) & 0x3F);
    if ((ctx->count[0] += ((uint32_t)len << 3)) < ((uint32_t)len << 3)) ctx->count[1]++;
    ctx->count[1] += (uint32_t)(len >> 29);
    partLen = 64 - index;
    if (len >= partLen) {
        memcpy(&ctx->buffer[index], input, partLen);
        md5_transform(ctx->state, ctx->buffer);
        for (i = partLen; i + 63 < len; i += 64) md5_transform(ctx->state, &input[i]);
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[index], &input[i], len - i);
}

static void md5_final(Md5Ctx *ctx, uint8_t digest[16]) {
    uint8_t bits[8];
    size_t index, padLen;
    int i;
    for (i = 0; i < 4; i++) {
        bits[i] = (uint8_t)(ctx->count[0] >> (i * 8));
        bits[i + 4] = (uint8_t)(ctx->count[1] >> (i * 8));
    }
    index = (size_t)((ctx->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    {
        uint8_t padding[64];
        memset(padding, 0, sizeof(padding));
        padding[0] = 0x80;
        md5_update(ctx, padding, padLen);
    }
    md5_update(ctx, bits, 8);
    for (i = 0; i < 4; i++) {
        digest[i * 4] = (uint8_t)(ctx->state[i] & 0xff);
        digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 8) & 0xff);
        digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 16) & 0xff);
        digest[i * 4 + 3] = (uint8_t)((ctx->state[i] >> 24) & 0xff);
    }
}

static void md5(const uint8_t *data, size_t len, uint8_t out[16]) {
    Md5Ctx ctx;
    md5_init(&ctx);
    md5_update(&ctx, data, len);
    md5_final(&ctx, out);
}

static int enc_char(int value, uint8_t *out) {
    if (value == 7) { *out = (uint8_t)'D'; return 1; }
    if (value == 14) { *out = (uint8_t)'h'; return 1; }
    if (value == 59) { *out = (uint8_t)'/'; return 1; }
    if (11 <= value && value <= 36) { *out = (uint8_t)(value + 'A' - 11); return 1; }
    if (47 <= value && value <= 61) { *out = (uint8_t)(value + 'l' - 47); return 1; }
    if (value <= 10) { *out = (uint8_t)(value + 'a'); return 1; }
    if (37 <= value && value <= 46) { *out = (uint8_t)(value + '0' - 37); return 1; }
    if (value == 62) { *out = (uint8_t)'+'; return 1; }
    if (value == 63) { *out = (uint8_t)'x'; return 1; }
    return 0;
}

static int mythroad_base64(const uint8_t *data, size_t len, uint8_t *out, size_t out_cap, size_t *out_len) {
    size_t whole = len / 3;
    size_t remain = len % 3;
    size_t pos = 0;
    size_t o = 0;
    size_t i;
    for (i = 0; i < whole; i++) {
        uint8_t a = data[pos], b = data[pos + 1], c = data[pos + 2];
        uint8_t q[4];
        pos += 3;
        if (!enc_char(a >> 2, &q[0]) ||
            !enc_char(((a & 0x03) << 4) | (b >> 4), &q[1]) ||
            !enc_char(((b & 0x0F) << 2) | (c >> 6), &q[2]) ||
            !enc_char(c & 0x3F, &q[3])) {
            return 0;
        }
        if (o + 4 > out_cap) return 0;
        memcpy(out + o, q, 4);
        o += 4;
    }
    if (remain) {
        uint8_t tail[3] = {0, 0, 0};
        uint8_t a, b, c;
        uint8_t q[4];
        int z;
        memcpy(tail, data + pos, remain);
        a = tail[0]; b = tail[1]; c = tail[2];
        if (!enc_char(a >> 2, &q[0]) ||
            !enc_char(((a & 0x03) << 4) | (b >> 4), &q[1]) ||
            !enc_char(((b & 0x0F) << 2) | (c >> 6), &q[2]) ||
            !enc_char(c & 0x3F, &q[3])) {
            return 0;
        }
        for (z = 0; z < (int)(3 - remain); z++) q[3 - z] = (uint8_t)'=';
        if (o + 4 > out_cap) return 0;
        memcpy(out + o, q, 4);
        o += 4;
    }
    *out_len = o;
    return 1;
}

void platform_identity_set_defaults(PlatformIdentity *id) {
    if (!id) return;
    memset(id, 0, sizeof(*id));
    snprintf(id->vmver, sizeof(id->vmver), "%s", "1968");
    snprintf(id->imei, sizeof(id->imei), "%s", "864086040622841");
    snprintf(id->manufacturer, sizeof(id->manufacturer), "%s", "vmrp");
    snprintf(id->model, sizeof(id->model), "%s", "vmrp");
}

LauncherStatus platform_identity_validate(const PlatformIdentity *id, LauncherError *err) {
    size_t i;
    launcher_error_clear(err);
    if (!id) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "identity", "null", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (strlen(id->imei) != GWY_IMEI_DIGITS) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "identity", "IMEI must be 15 digits", id->imei);
        return L_ERR_INVALID_ARGUMENT;
    }
    for (i = 0; i < GWY_IMEI_DIGITS; i++) {
        if (!isdigit((unsigned char)id->imei[i])) {
            launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "identity", "IMEI non-digit", id->imei);
            return L_ERR_INVALID_ARGUMENT;
        }
    }
    if (!id->vmver[0] || !id->manufacturer[0] || !id->model[0]) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "identity", "empty field", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    return L_OK;
}

LauncherStatus platform_sdk_key_generate(const PlatformIdentity *id,
                                         SdkKey *out,
                                         LauncherError *err) {
    uint8_t imei_raw[16];
    uint8_t part1[64], part2[128], part3[64];
    size_t p1, p2, p3;
    uint8_t enc[256];
    size_t enc_len;
    uint8_t dig[16];
    size_t man_len, model_len, vm_len;
    LauncherStatus st;

    launcher_error_clear(err);
    if (!id || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "identity", "null", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    st = platform_identity_validate(id, err);
    if (st != L_OK) return st;

    memset(out, 0, sizeof(*out));
    memcpy(imei_raw, id->imei, 15);
    imei_raw[15] = 0; /* 16-byte Lua string including NUL */

    vm_len = strlen(id->vmver);
    man_len = strlen(id->manufacturer);
    model_len = strlen(id->model);
    if (vm_len + 14 > sizeof(part1) || 6 + man_len + (model_len > 0 ? model_len - 1 : 0) > sizeof(part2) ||
        6 + (model_len >= 3 ? 3 : model_len) > sizeof(part3)) {
        launcher_error_set(err, L_ERR_BOUNDS, "identity", "identity fields too long", NULL);
        return L_ERR_BOUNDS;
    }

    /* part1: vmver + imei_raw[2:]  (14 bytes of imei including trailing NUL) */
    memcpy(part1, id->vmver, vm_len);
    memcpy(part1 + vm_len, imei_raw + 2, 14);
    p1 = vm_len + 14;

    /* part2: imei_raw[1:7] + hsman + hstype[1:] */
    memcpy(part2, imei_raw + 1, 6);
    memcpy(part2 + 6, id->manufacturer, man_len);
    if (model_len > 1) memcpy(part2 + 6 + man_len, id->model + 1, model_len - 1);
    p2 = 6 + man_len + (model_len > 1 ? model_len - 1 : 0);

    /* part3: imei_raw[8:14] + hstype[:3] */
    memcpy(part3, imei_raw + 8, 6);
    memcpy(part3 + 6, id->model, model_len >= 3 ? 3 : model_len);
    p3 = 6 + (model_len >= 3 ? 3 : model_len);

    if (!mythroad_base64(part1, p1, enc, sizeof(enc), &enc_len)) {
        launcher_error_set(err, L_ERR_FORMAT, "identity", "b64 part1 failed", NULL);
        return L_ERR_FORMAT;
    }
    md5(enc, enc_len, dig);
    memcpy(out->bytes + 0, dig, 16);

    if (!mythroad_base64(part2, p2, enc, sizeof(enc), &enc_len)) {
        launcher_error_set(err, L_ERR_FORMAT, "identity", "b64 part2 failed", NULL);
        return L_ERR_FORMAT;
    }
    md5(enc, enc_len, dig);
    memcpy(out->bytes + 16, dig, 16);

    if (!mythroad_base64(part3, p3, enc, sizeof(enc), &enc_len)) {
        launcher_error_set(err, L_ERR_FORMAT, "identity", "b64 part3 failed", NULL);
        return L_ERR_FORMAT;
    }
    md5(enc, enc_len, dig);
    memcpy(out->bytes + 32, dig, 16);

    {
        uint8_t sha[32];
        gwy_sha256(out->bytes, GWY_SDK_KEY_SIZE, sha);
        gwy_sha256_hex(sha, out->sha256_hex);
    }
    return L_OK;
}

LauncherStatus platform_sdk_key_write_file(const char *path,
                                           const SdkKey *key,
                                           LauncherError *err) {
    FILE *fp;
    launcher_error_clear(err);
    if (!path || !key) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "identity", "null", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        launcher_error_set(err, L_ERR_IO, "identity", "open failed", path);
        return L_ERR_IO;
    }
    if (fwrite(key->bytes, 1, GWY_SDK_KEY_SIZE, fp) != GWY_SDK_KEY_SIZE) {
        fclose(fp);
        launcher_error_set(err, L_ERR_IO, "identity", "short write", path);
        return L_ERR_IO;
    }
    fclose(fp);
    return L_OK;
}
