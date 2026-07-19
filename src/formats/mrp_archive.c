#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int looks_like_gzip(const uint8_t *p, size_t n) {
    return n >= 2 && p[0] == 0x1f && p[1] == 0x8b;
}

static LauncherStatus read_entire_file(const char *path, uint8_t **out_data, size_t *out_size, LauncherError *err) {
    FILE *fp;
    long sz;
    uint8_t *buf;
    size_t nread;

    fp = fopen(path, "rb");
    if (!fp) {
        launcher_error_set(err, L_ERR_IO, "mrp_archive", "failed to open file", path);
        return L_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        launcher_error_set(err, L_ERR_IO, "mrp_archive", "fseek end failed", path);
        return L_ERR_IO;
    }
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        launcher_error_set(err, L_ERR_IO, "mrp_archive", "ftell failed", path);
        return L_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        launcher_error_set(err, L_ERR_IO, "mrp_archive", "fseek set failed", path);
        return L_ERR_IO;
    }
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(fp);
        launcher_error_set(err, L_ERR_IO, "mrp_archive", "out of memory", path);
        return L_ERR_IO;
    }
    nread = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (nread != (size_t)sz) {
        free(buf);
        launcher_error_set(err, L_ERR_IO, "mrp_archive", "short read", path);
        return L_ERR_IO;
    }
    *out_data = buf;
    *out_size = (size_t)sz;
    return L_OK;
}

static LauncherStatus parse_index(MrpArchive *a, LauncherError *err) {
    size_t pos = a->header_length;
    size_t end = a->first_data_offset;

    a->member_count = 0;
    while (pos < end) {
        uint32_t name_len;
        MrpMember *m;
        size_t copy_len;

        if (pos + 4 > end) {
            launcher_error_set(err, L_ERR_FORMAT, "mrp_archive", "truncated member index", a->path);
            return L_ERR_FORMAT;
        }
        name_len = rd_u32_le(a->data + pos);
        pos += 4;
        if (name_len < 1 || name_len > 512 || pos + name_len + 12 > a->size) {
            launcher_error_set(err, L_ERR_FORMAT, "mrp_archive", "invalid member name length", a->path);
            return L_ERR_FORMAT;
        }
        if (a->member_count >= MRP_MAX_MEMBERS) {
            launcher_error_set(err, L_ERR_BOUNDS, "mrp_archive", "too many members", a->path);
            return L_ERR_BOUNDS;
        }
        m = &a->members[a->member_count];
        memset(m, 0, sizeof(*m));
        copy_len = name_len;
        if (copy_len >= MRP_MEMBER_NAME_MAX) copy_len = MRP_MEMBER_NAME_MAX - 1;
        memcpy(m->name, a->data + pos, copy_len);
        /* strip trailing NULs already in name field */
        while (copy_len > 0 && m->name[copy_len - 1] == '\0') copy_len--;
        m->name[copy_len] = '\0';
        pos += name_len;
        if (pos + 12 > a->size) {
            launcher_error_set(err, L_ERR_FORMAT, "mrp_archive", "truncated member meta", a->path);
            return L_ERR_FORMAT;
        }
        m->offset = rd_u32_le(a->data + pos);
        m->stored_size = rd_u32_le(a->data + pos + 4);
        m->reserved = rd_u32_le(a->data + pos + 8);
        pos += 12;
        if ((size_t)m->offset + (size_t)m->stored_size > a->size) {
            launcher_error_set(err, L_ERR_BOUNDS, "mrp_archive", "member outside archive", m->name);
            return L_ERR_BOUNDS;
        }
        m->looks_gzip = looks_like_gzip(a->data + m->offset, m->stored_size);
        if (m->looks_gzip) {
            m->compression = MRP_COMP_GZIP;
            /* gzip ISIZE (DOCUMENTED): last 4 bytes = uncompressed size mod 2^32 */
            if (m->stored_size >= 8) {
                m->unpacked_size = rd_u32_le(a->data + m->offset + m->stored_size - 4);
                m->unpacked_known = 1;
            }
        } else {
            m->compression = MRP_COMP_RAW;
            m->unpacked_size = m->stored_size;
            m->unpacked_known = 1;
        }
        a->member_count++;
    }
    return L_OK;
}

LauncherStatus mrp_archive_open(const char *path, MrpArchive **out, LauncherError *err) {
    MrpArchive *a;
    LauncherStatus st;
    uint32_t total;
    uint32_t first_data_plus4;

    launcher_error_clear(err);
    if (!path || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "mrp_archive", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out = NULL;
    a = (MrpArchive *)calloc(1, sizeof(*a));
    if (!a) {
        launcher_error_set(err, L_ERR_IO, "mrp_archive", "out of memory", path);
        return L_ERR_IO;
    }
    snprintf(a->path, sizeof(a->path), "%s", path);
    st = read_entire_file(path, &a->data, &a->size, err);
    if (st != L_OK) {
        free(a);
        return st;
    }
    if (a->size < 240 || memcmp(a->data, "MRPG", 4) != 0) {
        launcher_error_set(err, L_ERR_FORMAT, "mrp_archive", "not an MRPG archive", path);
        mrp_archive_close(a);
        return L_ERR_FORMAT;
    }
    first_data_plus4 = rd_u32_le(a->data + 4);
    total = rd_u32_le(a->data + 8);
    a->header_length = rd_u32_le(a->data + 12);
    if (first_data_plus4 < 4) {
        launcher_error_set(err, L_ERR_FORMAT, "mrp_archive", "invalid first_data field", path);
        mrp_archive_close(a);
        return L_ERR_FORMAT;
    }
    a->first_data_offset = first_data_plus4 - 4;
    if (total != (uint32_t)a->size) {
        launcher_error_set(err, L_ERR_FORMAT, "mrp_archive", "length mismatch", path);
        mrp_archive_close(a);
        return L_ERR_FORMAT;
    }
    if (!(a->header_length <= a->first_data_offset && a->first_data_offset <= a->size)) {
        launcher_error_set(err, L_ERR_FORMAT, "mrp_archive", "invalid index/data boundary", path);
        mrp_archive_close(a);
        return L_ERR_FORMAT;
    }
    memcpy(a->internal_name, a->data + 16, 12);
    a->internal_name[12] = '\0';
    a->appid_le = rd_u32_le(a->data + 68);
    a->appver_le = rd_u32_le(a->data + 72);
    a->flags = rd_u32_le(a->data + 76);
    gwy_sha256(a->data, a->size, a->sha256);

    st = parse_index(a, err);
    if (st != L_OK) {
        mrp_archive_close(a);
        return st;
    }
    *out = a;
    return L_OK;
}

void mrp_archive_close(MrpArchive *archive) {
    if (!archive) return;
    free(archive->data);
    free(archive);
}

LauncherStatus mrp_archive_find_exact(const MrpArchive *archive,
                                      const char *member_name,
                                      const MrpMember **out,
                                      LauncherError *err) {
    size_t i;
    launcher_error_clear(err);
    if (!archive || !member_name || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "mrp_archive", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out = NULL;
    for (i = 0; i < archive->member_count; i++) {
        if (strcmp(archive->members[i].name, member_name) == 0) {
            *out = &archive->members[i];
            return L_OK;
        }
    }
    launcher_error_set(err, L_ERR_NOT_FOUND, "mrp_archive", "member not found", member_name);
    return L_ERR_NOT_FOUND;
}

LauncherStatus mrp_archive_decode_member(const MrpArchive *archive,
                                         const MrpMember *member,
                                         size_t max_decoded_size,
                                         ByteBuffer *out,
                                         LauncherError *err) {
    const uint8_t *src;
    size_t src_len;

    launcher_error_clear(err);
    if (!archive || !member || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "mrp_archive", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    byte_buffer_init(out);
    src = archive->data + member->offset;
    src_len = member->stored_size;

    if (member->looks_gzip) {
        z_stream strm;
        int zret;
        uint8_t chunk[16384];

        memset(&strm, 0, sizeof(strm));
        if (inflateInit2(&strm, 15 + 32) != Z_OK) {
            launcher_error_set(err, L_ERR_DECOMPRESSION, "mrp_archive", "inflateInit2 failed", member->name);
            return L_ERR_DECOMPRESSION;
        }
        strm.next_in = (Bytef *)src;
        strm.avail_in = (uInt)src_len;
        do {
            strm.next_out = chunk;
            strm.avail_out = sizeof(chunk);
            zret = inflate(&strm, Z_NO_FLUSH);
            if (zret != Z_OK && zret != Z_STREAM_END) {
                inflateEnd(&strm);
                byte_buffer_free(out);
                launcher_error_set(err, L_ERR_DECOMPRESSION, "mrp_archive", "gzip inflate failed", member->name);
                return L_ERR_DECOMPRESSION;
            }
            {
                size_t got = sizeof(chunk) - strm.avail_out;
                if (out->size + got > max_decoded_size) {
                    inflateEnd(&strm);
                    byte_buffer_free(out);
                    launcher_error_set(err, L_ERR_BOUNDS, "mrp_archive", "decoded size exceeds limit", member->name);
                    return L_ERR_BOUNDS;
                }
                if (!byte_buffer_append(out, chunk, got)) {
                    inflateEnd(&strm);
                    byte_buffer_free(out);
                    launcher_error_set(err, L_ERR_IO, "mrp_archive", "out of memory decoding", member->name);
                    return L_ERR_IO;
                }
            }
        } while (zret != Z_STREAM_END);
        inflateEnd(&strm);
        return L_OK;
    }

    if (src_len > max_decoded_size) {
        launcher_error_set(err, L_ERR_BOUNDS, "mrp_archive", "raw member exceeds limit", member->name);
        return L_ERR_BOUNDS;
    }
    if (!byte_buffer_append(out, src, src_len)) {
        launcher_error_set(err, L_ERR_IO, "mrp_archive", "out of memory copying raw", member->name);
        return L_ERR_IO;
    }
    return L_OK;
}

static int ascii_ieq(const char *a, const char *b) {
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

LauncherStatus mrp_archive_find_casefold(const MrpArchive *archive,
                                         const char *member_name,
                                         const MrpMember **out,
                                         LauncherError *err) {
    size_t i;
    launcher_error_clear(err);
    if (!archive || !member_name || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "mrp_archive", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    *out = NULL;
    for (i = 0; i < archive->member_count; i++) {
        if (ascii_ieq(archive->members[i].name, member_name)) {
            *out = &archive->members[i];
            return L_OK;
        }
    }
    launcher_error_set(err, L_ERR_NOT_FOUND, "mrp_archive", "member not found (casefold)", member_name);
    return L_ERR_NOT_FOUND;
}

const char *mrp_compression_name(MrpCompression c) {
    switch (c) {
    case MRP_COMP_RAW: return "raw";
    case MRP_COMP_GZIP: return "gzip";
    default: return "unknown";
    }
}

LauncherStatus mrp_archive_probe_member(MrpArchive *archive,
                                        MrpMember *member,
                                        LauncherError *err) {
    ByteBuffer buf;
    LauncherStatus st;
    launcher_error_clear(err);
    if (!archive || !member) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "mrp_archive", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    if (member->unpacked_known) return L_OK;
    st = mrp_archive_decode_member(archive, member, 16u * 1024u * 1024u, &buf, err);
    if (st != L_OK) return st;
    member->unpacked_size = (uint32_t)buf.size;
    member->unpacked_known = 1;
    if (member->looks_gzip) member->compression = MRP_COMP_GZIP;
    else member->compression = MRP_COMP_RAW;
    byte_buffer_free(&buf);
    return L_OK;
}

LauncherStatus mrp_archive_member_info(MrpArchive *archive,
                                       const MrpMember *member,
                                       MrpMemberInfo *out,
                                       LauncherError *err) {
    MrpMember *mutable_m = NULL;
    size_t i;
    LauncherStatus st;
    launcher_error_clear(err);
    if (!archive || !member || !out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "mrp_archive", "null argument", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    for (i = 0; i < archive->member_count; i++) {
        if (&archive->members[i] == member || strcmp(archive->members[i].name, member->name) == 0) {
            mutable_m = &archive->members[i];
            break;
        }
    }
    if (!mutable_m) {
        launcher_error_set(err, L_ERR_NOT_FOUND, "mrp_archive", "member not in archive", member->name);
        return L_ERR_NOT_FOUND;
    }
    st = mrp_archive_probe_member(archive, mutable_m, err);
    if (st != L_OK) return st;
    memset(out, 0, sizeof(*out));
    snprintf(out->name, sizeof(out->name), "%s", mutable_m->name);
    out->stored_offset = mutable_m->offset;
    out->stored_size = mutable_m->stored_size;
    out->unpacked_size = mutable_m->unpacked_size;
    out->compression = mutable_m->compression;
    out->unpacked_known = mutable_m->unpacked_known;
    return L_OK;
}
