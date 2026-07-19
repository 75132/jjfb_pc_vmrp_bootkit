#include "./header/vmrp.h"
#include "./header/memory.h"
#include "./header/utils.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

typedef struct {
    uint32 next;
    uint32 len;
} LG_mem_free_t;

uint32 LG_mem_min;  // 从未分配过的长度？
uint32 LG_mem_top;  // 动态申请到达的最高内存值
LG_mem_free_t LG_mem_free;
char *LG_mem_base;
uint32 LG_mem_len;
char *Origin_LG_mem_base;
uint32 Origin_LG_mem_len;
char *LG_mem_end;
uint32 LG_mem_left;  // 剩余内存

#define realLGmemSize(x) (((x) + 7) & (0xfffffff8))

void initMemoryManager(uint32_t baseAddress, uint32_t len) {
    printf("initMemoryManager: baseAddress:0x%X len: 0x%X\n", baseAddress, len);
    Origin_LG_mem_base = getMrpMemPtr(baseAddress);
    Origin_LG_mem_len = len;

    LG_mem_base = (char *)((uint32)(Origin_LG_mem_base + 3) & (~3));
    LG_mem_len = (Origin_LG_mem_len - (LG_mem_base - Origin_LG_mem_base)) & (~3);
    LG_mem_end = LG_mem_base + LG_mem_len;
    LG_mem_free.next = 0;
    LG_mem_free.len = 0;
    ((LG_mem_free_t *)LG_mem_base)->next = LG_mem_len;
    ((LG_mem_free_t *)LG_mem_base)->len = LG_mem_len;
    LG_mem_left = LG_mem_len;
#ifdef MEM_DEBUG
    LG_mem_min = LG_mem_len;
    LG_mem_top = 0;
#endif
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void printMemoryInfo() {
    printf(".......total:%d, min:%d, free:%d, top:%d\n", LG_mem_len, LG_mem_min, LG_mem_left, LG_mem_top);
    printf(".......base:%p, end:%p\n", LG_mem_base, LG_mem_end);
    printf(".......obase:%p, olen:%d\n", Origin_LG_mem_base, Origin_LG_mem_len);
}

void *my_malloc(uint32 len) {
    LG_mem_free_t *previous, *nextfree, *l;
    void *ret;

    len = (uint32)realLGmemSize(len);
    if (len >= LG_mem_left) {
        printf("my_malloc no memory\n");
        goto err;
    }
    if (!len) {
        printf("my_malloc invalid memory request");
        goto err;
    }
    if (LG_mem_base + LG_mem_free.next > LG_mem_end) {
        printf("my_malloc corrupted memory");
        goto err;
    }
    previous = &LG_mem_free;
    nextfree = (LG_mem_free_t *)(LG_mem_base + previous->next);
    while ((char *)nextfree < LG_mem_end) {
        if (nextfree->len == len) {
            previous->next = nextfree->next;
            LG_mem_left -= len;
#ifdef MEM_DEBUG
            if (LG_mem_left < LG_mem_min)
                LG_mem_min = LG_mem_left;
            if (LG_mem_top < previous->next)
                LG_mem_top = previous->next;
#endif
            ret = (void *)nextfree;
            goto end;
        }
        if (nextfree->len > len) {
            l = (LG_mem_free_t *)((char *)nextfree + len);
            l->next = nextfree->next;
            l->len = (uint32)(nextfree->len - len);
            previous->next += len;
            LG_mem_left -= len;
#ifdef MEM_DEBUG
            if (LG_mem_left < LG_mem_min)
                LG_mem_min = LG_mem_left;
            if (LG_mem_top < previous->next)
                LG_mem_top = previous->next;
#endif
            ret = (void *)nextfree;
            goto end;
        }
        previous = nextfree;
        nextfree = (LG_mem_free_t *)(LG_mem_base + nextfree->next);
    }
    printf("my_malloc no memory\n");
err:
    return 0;
end:
    return ret;
}

void my_free(void *p, uint32 len) {
    LG_mem_free_t *free, *n;
    len = (uint32)realLGmemSize(len);
    /* Always validate: bad frees used to corrupt the freelist (no MEM_DEBUG)
     * and later surface as plat 0x10132 alloc with pointer-like sizes. */
    if (!len || !p || (char *)p < LG_mem_base || (char *)p >= LG_mem_end ||
        (char *)p + len > LG_mem_end || (char *)p + len <= LG_mem_base) {
        printf("my_free invalid\n");
        printf("p=%d,l=%d,base=%d,LG_mem_end=%d\n", (int32)p, len, (int32)LG_mem_base,
               (int32)LG_mem_end);
        return;
    }
    free = &LG_mem_free;
    n = (LG_mem_free_t *)(LG_mem_base + free->next);
    while (((char *)n < LG_mem_end) && ((void *)n < p)) {
        free = n;
        n = (LG_mem_free_t *)(LG_mem_base + n->next);
    }
    if (p == (void *)free || p == (void *)n) {
        printf("my_free:already free\n");
        return;
    }
    if ((free != &LG_mem_free) && ((char *)free + free->len == p)) {
        free->len += len;
    } else {
        free->next = (uint32)((char *)p - LG_mem_base);
        free = (LG_mem_free_t *)p;
        free->next = (uint32)((char *)n - LG_mem_base);
        free->len = len;
    }
    if (((char *)n < LG_mem_end) && ((char *)p + len == (char *)n)) {
        free->next = n->next;
        free->len += n->len;
    }
    LG_mem_left += len;
}

int my_orphan_redzone_after(void *user, uint32 user_len, uint32 orphan_bytes) {
    char *chunk;
    uint32 clen;
    char *node;
    uint32 orphan;
    LG_mem_free_t *prev, *n, *neu;
    uint32 head_off;

    if (!user || !user_len || !orphan_bytes || !LG_mem_base)
        return 0;
    chunk = (char *)user - (int)sizeof(uint32);
    clen = (uint32)realLGmemSize(user_len + sizeof(uint32));
    node = chunk + clen;
    orphan = (uint32)realLGmemSize(orphan_bytes);
    head_off = LG_mem_free.next;
    if (node < LG_mem_base || node + orphan + 8 > LG_mem_end) {
        printf("[JJFB_ORPHAN] reject range chunk=%p clen=%u node=%p orphan=%u\n",
               (void *)chunk, clen, (void *)node, orphan);
        return 0;
    }

    prev = &LG_mem_free;
    n = (LG_mem_free_t *)(LG_mem_base + prev->next);
    {
        int steps = 0;
        while ((char *)n < LG_mem_end && steps++ < 100000) {
            if ((char *)n == node)
                break;
            if (n->next >= LG_mem_len)
                break;
            prev = n;
            n = (LG_mem_free_t *)(LG_mem_base + n->next);
        }
        if (steps >= 100000) {
            printf("[JJFB_ORPHAN] walk overflow\n");
            return 0;
        }
    }
    if ((char *)n != node) {
        printf("[JJFB_ORPHAN] miss node=%p head_off=0x%X first=%p first_next=0x%X first_len=%u\n",
               (void *)node, head_off,
               (void *)(LG_mem_base + head_off),
               head_off < LG_mem_len ? ((LG_mem_free_t *)(LG_mem_base + head_off))->next : 0,
               head_off < LG_mem_len ? ((LG_mem_free_t *)(LG_mem_base + head_off))->len : 0);
        return 0;
    }
    if (n->len < orphan + 8) {
        printf("[JJFB_ORPHAN] too small len=%u need=%u\n", n->len, orphan + 8);
        return 0;
    }

    /* Leave node bytes intact (stale next/len) for guest block+4 reads. */
    neu = (LG_mem_free_t *)(node + orphan);
    neu->next = n->next;
    neu->len = n->len - orphan;
    prev->next = (uint32)((char *)neu - LG_mem_base);
    LG_mem_left -= orphan;
    return 1;
}

void *my_realloc(void *p, uint32 oldlen, uint32 len) {
    unsigned long minsize = (oldlen > len) ? len : oldlen;
    void *newblock;
    if (p == NULL) {
        return my_malloc(len);
    }
    if (len == 0) {
        my_free(p, oldlen);
        return NULL;
    }
    newblock = my_malloc(len);
    if (newblock == NULL) {
        return newblock;
    }
    memmove(newblock, p, minsize);
    my_free(p, oldlen);
    return newblock;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void *my_mallocExt(uint32 len) {
    uint32 *p;
    if (len == 0) {
        return NULL;
    }
    p = my_malloc(len + sizeof(uint32));
    if (p) {
        *p = len;
        return (void *)(p + 1);
    }
    return p;
}

void *my_mallocExt0(uint32 len) {
    uint32 *p = my_mallocExt(len);
    if (p) {
        memset(p, 0, len);
        return p;
    }
    return p;
}

int my_is_mallocExt_user(const void *p) {
    const uint32 *hdr;
    uint32 sz;
    if (!p || !LG_mem_base || !LG_mem_end)
        return 0;
    if ((const char *)p < LG_mem_base + (int)sizeof(uint32) ||
        (const char *)p >= LG_mem_end)
        return 0;
    hdr = (const uint32 *)p - 1;
    if ((const char *)hdr < LG_mem_base)
        return 0;
    sz = *hdr;
    if (sz == 0 || sz > LG_mem_len)
        return 0;
    if ((const char *)p + sz > LG_mem_end)
        return 0;
    return 1;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void my_freeExt(void *p) {
    if (p) {
        uint32 *t = (uint32 *)p - 1;
        if (!my_is_mallocExt_user(p)) {
            printf("my_freeExt reject p=%p\n", p);
            return;
        }
        my_free(t, *t + sizeof(uint32));
    }
}

int my_safe_freeExt(void *p) {
    if (!p)
        return 1;
    if (my_is_mallocExt_user(p)) {
        my_freeExt(p);
        return 1;
    }
    /* 0x10133 / 10134 path: guest passes block base = user - 4 (size word). */
    if (my_is_mallocExt_user((char *)p + 4)) {
        my_freeExt((char *)p + 4);
        return 1;
    }
    return 0;
}

void *my_reallocExt(void *p, uint32 newLen) {
    if (p == NULL) {
        return my_mallocExt(newLen);
    } else if (newLen == 0) {
        my_freeExt(p);
        return NULL;
    } else {
        uint32 oldlen = *((uint32 *)p - 1) + sizeof(uint32);
        uint32 minsize = (oldlen < newLen) ? oldlen : newLen;
        void *newblock = my_mallocExt(newLen);
        if (newblock == NULL) {
            return newblock;
        }
        memmove(newblock, p, minsize);
        my_freeExt(p);
        return newblock;
    }
}
