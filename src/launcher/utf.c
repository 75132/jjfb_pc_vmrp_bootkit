#include "gwy_launcher/utf.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

int gwy_utf16be_to_utf8(const uint8_t *src, size_t src_bytes, char *dst, size_t dst_cap) {
    size_t i = 0;
    size_t o = 0;
    if (!src || !dst || dst_cap == 0) return -1;
    dst[0] = '\0';
    while (i + 1 < src_bytes) {
        uint32_t cu = ((uint32_t)src[i] << 8) | (uint32_t)src[i + 1];
        i += 2;
        if (cu == 0) break;
        if (cu < 0x80) {
            if (o + 1 >= dst_cap) return -1;
            dst[o++] = (char)cu;
        } else if (cu < 0x800) {
            if (o + 2 >= dst_cap) return -1;
            dst[o++] = (char)(0xC0 | (cu >> 6));
            dst[o++] = (char)(0x80 | (cu & 0x3F));
        } else {
            if (o + 3 >= dst_cap) return -1;
            dst[o++] = (char)(0xE0 | (cu >> 12));
            dst[o++] = (char)(0x80 | ((cu >> 6) & 0x3F));
            dst[o++] = (char)(0x80 | (cu & 0x3F));
        }
    }
    if (o >= dst_cap) return -1;
    dst[o] = '\0';
    return (int)o;
}

int gwy_gbk_to_utf8(const char *src, char *dst, size_t dst_cap) {
#ifdef _WIN32
    wchar_t wbuf[256];
    int wlen;
    int ulen;
    if (!src || !dst || dst_cap == 0) return -1;
    wlen = MultiByteToWideChar(936, 0, src, -1, wbuf, (int)(sizeof(wbuf) / sizeof(wbuf[0])));
    if (wlen <= 0) {
        snprintf(dst, dst_cap, "%s", src);
        return (int)strlen(dst);
    }
    ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, dst, (int)dst_cap, NULL, NULL);
    if (ulen <= 0) {
        snprintf(dst, dst_cap, "%s", src);
        return (int)strlen(dst);
    }
    return ulen - 1;
#else
    size_t i;
    if (!src || !dst || dst_cap == 0) return -1;
    for (i = 0; src[i] && i + 1 < dst_cap; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = (c >= 32 && c < 127) ? (char)c : '?';
    }
    dst[i] = '\0';
    return (int)i;
#endif
}
