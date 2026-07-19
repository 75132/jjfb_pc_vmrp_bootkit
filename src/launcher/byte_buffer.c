#include "gwy_launcher/byte_buffer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void byte_buffer_init(ByteBuffer *buf) {
    if (!buf) return;
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

void byte_buffer_free(ByteBuffer *buf) {
    if (!buf) return;
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

int byte_buffer_reserve(ByteBuffer *buf, size_t capacity) {
    uint8_t *p;
    if (!buf) return 0;
    if (capacity <= buf->capacity) return 1;
    p = (uint8_t *)realloc(buf->data, capacity);
    if (!p) return 0;
    buf->data = p;
    buf->capacity = capacity;
    return 1;
}

int byte_buffer_append(ByteBuffer *buf, const void *data, size_t len) {
    size_t need;
    if (!buf || (!data && len > 0)) return 0;
    need = buf->size + len;
    if (need < buf->size) return 0; /* overflow */
    if (need > buf->capacity) {
        size_t cap = buf->capacity ? buf->capacity : 256;
        while (cap < need) {
            if (cap > (SIZE_MAX / 2)) {
                cap = need;
                break;
            }
            cap *= 2;
        }
        if (!byte_buffer_reserve(buf, cap)) return 0;
    }
    if (len) memcpy(buf->data + buf->size, data, len);
    buf->size = need;
    return 1;
}
