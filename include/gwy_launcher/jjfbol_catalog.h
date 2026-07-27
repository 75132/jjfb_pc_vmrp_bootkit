#ifndef GWY_LAUNCHER_JJFBOL_CATALOG_H
#define GWY_LAUNCHER_JJFBOL_CATALOG_H

#include "gwy_launcher/error.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JJFBOL_PATH_MAX 1024
#define JJFBOL_STEM_MAX 128
#define JJFBOL_MEMBER_NAME_MAX 256
#define JJFBOL_MAX_PACKAGES 128
#define JJFBOL_MAX_MEMBERS 65536
#define JJFBOL_LRU_SLOTS 8

typedef struct JjfbolPackageIndex {
    char path[JJFBOL_PATH_MAX];
    char stem[JJFBOL_STEM_MAX];
    uint32_t version; /* BE from *.mrp.v; 0 if absent */
    uint32_t first_member;
    uint32_t member_count;
} JjfbolPackageIndex;

typedef struct JjfbolMemberIndex {
    char name[JJFBOL_MEMBER_NAME_MAX];
    uint16_t package_index;
    uint32_t offset;
    uint32_t stored_size;
} JjfbolMemberIndex;

typedef enum JjfbolLookupKind {
    JJFBOL_LOOKUP_MISS = 0,
    JJFBOL_LOOKUP_UNIQUE = 1,
    JJFBOL_LOOKUP_MULTI = 2
} JjfbolLookupKind;

typedef struct JjfbolLookupResult {
    JjfbolLookupKind kind;
    uint16_t package_index; /* valid when UNIQUE */
    uint16_t hit_count;
    uint16_t hit_packages[16];
} JjfbolLookupResult;

/* Scan gwy/jjfbol under resource_root. Open each MRP briefly, copy index, close. */
LauncherStatus jjfbol_catalog_init(const char *resource_root, LauncherError *err);
void jjfbol_catalog_reset(void);
int jjfbol_catalog_ready(void);

const char *jjfbol_catalog_root(void);
uint32_t jjfbol_catalog_package_count(void);
uint32_t jjfbol_catalog_version_file_count(void);
uint32_t jjfbol_catalog_down_version(void); /* host-endian value from BE file */
uint32_t jjfbol_catalog_down_version_raw_be(void);

const JjfbolPackageIndex *jjfbol_catalog_package(uint32_t index);
int jjfbol_catalog_find_package_stem(const char *stem, uint32_t *out_index);

/* Exact then optional case-fold unique lookup across indexed members. */
JjfbolLookupKind jjfbol_catalog_lookup_exact(const char *member_name, JjfbolLookupResult *out);
JjfbolLookupKind jjfbol_catalog_lookup_casefold(const char *member_name, JjfbolLookupResult *out);

/*
 * Decode a member from package_index via small LRU reopen (never keeps all archives).
 * out_buf/out_cap receive decoded bytes; returns L_OK on success.
 */
LauncherStatus jjfbol_catalog_decode_member(uint32_t package_index,
                                            const char *member_name,
                                            uint8_t *out_buf,
                                            size_t out_cap,
                                            size_t *out_len,
                                            LauncherError *err);

#ifdef __cplusplus
}
#endif

#endif
