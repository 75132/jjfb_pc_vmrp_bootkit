#ifndef __VMRP_MEMORY_H__
#define __VMRP_MEMORY_H__

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

void *my_mallocExt(uint32 len);
void my_freeExt(void *p);
void *my_malloc(uint32 len);
void my_free(void *p, uint32 len);
/* After tiny mallocExt: unlink orphan_bytes after the chunk from the freelist
 * but leave the bytes uncleared so guest block+4 still sees stale free-node
 * words (avoids EXC) while writes no longer trash the live freelist. */
int my_orphan_redzone_after(void *user, uint32 user_len, uint32 orphan_bytes);
/* 1 if p is a my_mallocExt user pointer inside the host LG heap. */
int my_is_mallocExt_user(const void *p);
/* Free p if it is a user ptr, or LG block base (user-4 / size word). Returns 1 if freed. */
int my_safe_freeExt(void *p);
void initMemoryManager(uint32_t baseAddress, uint32_t len);

#endif
