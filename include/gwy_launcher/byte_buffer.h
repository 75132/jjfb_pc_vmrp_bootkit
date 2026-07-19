#ifndef GWY_LAUNCHER_BYTE_BUFFER_H
#define GWY_LAUNCHER_BYTE_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct ByteBuffer {
    uint8_t *data;
    size_t size;
    size_t capacity;
} ByteBuffer;

void byte_buffer_init(ByteBuffer *buf);
void byte_buffer_free(ByteBuffer *buf);
int byte_buffer_reserve(ByteBuffer *buf, size_t capacity);
int byte_buffer_append(ByteBuffer *buf, const void *data, size_t len);

#endif
