#ifndef GWY_LAUNCHER_MRP_ARCHIVE_H
#define GWY_LAUNCHER_MRP_ARCHIVE_H

#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MRP_MEMBER_NAME_MAX 256
#define MRP_MAX_MEMBERS 512

typedef enum MrpCompression {
    MRP_COMP_RAW = 0,
    MRP_COMP_GZIP = 1,
    MRP_COMP_UNKNOWN = 2
} MrpCompression;

typedef struct MrpMember {
    char name[MRP_MEMBER_NAME_MAX];
    uint32_t offset;          /* stored_offset in archive */
    uint32_t stored_size;     /* compressed or raw bytes in archive */
    uint32_t unpacked_size;   /* decoded length; 0 if unknown */
    uint32_t reserved;
    MrpCompression compression;
    bool looks_gzip;
    int unpacked_known;
} MrpMember;

typedef struct MrpMemberInfo {
    char name[MRP_MEMBER_NAME_MAX];
    uint32_t stored_offset;
    uint32_t stored_size;
    uint32_t unpacked_size;
    MrpCompression compression;
    int unpacked_known;
} MrpMemberInfo;

typedef struct MrpArchive {
    char path[1024];
    uint8_t *data;
    size_t size;
    uint32_t header_length;
    uint32_t first_data_offset;
    uint32_t appid_le;
    uint32_t appver_le;
    uint32_t flags;
    char internal_name[32];
    uint8_t sha256[32];
    MrpMember members[MRP_MAX_MEMBERS];
    size_t member_count;
} MrpArchive;

LauncherStatus mrp_archive_open(const char *path, MrpArchive **out, LauncherError *err);
void mrp_archive_close(MrpArchive *archive);

LauncherStatus mrp_archive_find_exact(const MrpArchive *archive,
                                      const char *member_name,
                                      const MrpMember **out,
                                      LauncherError *err);

/* Case-insensitive ASCII compare for member names. */
LauncherStatus mrp_archive_find_casefold(const MrpArchive *archive,
                                         const char *member_name,
                                         const MrpMember **out,
                                         LauncherError *err);

LauncherStatus mrp_archive_probe_member(MrpArchive *archive,
                                        MrpMember *member,
                                        LauncherError *err);

LauncherStatus mrp_archive_member_info(MrpArchive *archive,
                                       const MrpMember *member,
                                       MrpMemberInfo *out,
                                       LauncherError *err);

LauncherStatus mrp_archive_decode_member(const MrpArchive *archive,
                                         const MrpMember *member,
                                         size_t max_decoded_size,
                                         ByteBuffer *out,
                                         LauncherError *err);

const char *mrp_compression_name(MrpCompression c);

#endif
