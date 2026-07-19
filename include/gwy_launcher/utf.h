#ifndef GWY_LAUNCHER_UTF_H
#define GWY_LAUNCHER_UTF_H

#include <stddef.h>
#include <stdint.h>

/* Decode UTF-16BE (no BOM) into UTF-8. Returns bytes written excluding NUL, or -1. */
int gwy_utf16be_to_utf8(const uint8_t *src, size_t src_bytes, char *dst, size_t dst_cap);

/* Decode legacy GBK/CP936 into UTF-8 using Windows MultiByteToWideChar when available.
 * On non-Windows, copies printable ASCII and replaces others with '?'. */
int gwy_gbk_to_utf8(const char *src, char *dst, size_t dst_cap);

#endif
