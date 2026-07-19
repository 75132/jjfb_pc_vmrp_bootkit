#ifndef GWY_LAUNCHER_SHA256_H
#define GWY_LAUNCHER_SHA256_H

#include <stddef.h>
#include <stdint.h>

void gwy_sha256(const void *data, size_t len, uint8_t out[32]);
void gwy_sha256_hex(const uint8_t digest[32], char out_hex[65]);
int gwy_sha256_file(const char *path, uint8_t out[32]);

#endif
