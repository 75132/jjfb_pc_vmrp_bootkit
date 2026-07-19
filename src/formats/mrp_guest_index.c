#include "gwy_launcher/mrp_guest_index.h"
#include <string.h>

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

MrpGuestLookupCode mrp_guest_index_lookup(const uint8_t *data,
                                          size_t size,
                                          const char *member_name,
                                          MrpGuestLookupResult *out_opt) {
    uint32_t header_length;
    uint32_t first_data_plus4;
    uint32_t indexlen;
    uint32_t index_off;
    uint32_t pos;

    if (out_opt) memset(out_opt, 0, sizeof(*out_opt));
    if (!data || size < 16 || !member_name || !member_name[0]) {
        if (out_opt) out_opt->code = MRP_GUEST_LOOKUP_ERR_3001;
        return MRP_GUEST_LOOKUP_ERR_3001;
    }

    first_data_plus4 = rd_u32_le(data + 4);
    header_length = rd_u32_le(data + 12);
    if (first_data_plus4 <= 232) {
        if (out_opt) out_opt->code = MRP_GUEST_LOOKUP_ERR_3001;
        return MRP_GUEST_LOOKUP_ERR_3001;
    }
    indexlen = first_data_plus4 + 8 - header_length;
    if (header_length < 16 || indexlen < 1 || (size_t)header_length + (size_t)indexlen > size) {
        if (out_opt) {
            out_opt->code = MRP_GUEST_LOOKUP_ERR_3001;
            out_opt->header_length = header_length;
            out_opt->first_data_plus4 = first_data_plus4;
            out_opt->indexlen = indexlen;
        }
        return MRP_GUEST_LOOKUP_ERR_3001;
    }

    index_off = header_length;
    pos = 0;

    if (out_opt) {
        out_opt->header_length = header_length;
        out_opt->first_data_plus4 = first_data_plus4;
        out_opt->indexlen = indexlen;
    }

    while (1) {
        uint32_t len;
        uint32_t file_pos;
        uint32_t file_len;
        char temp_name[MRP_GUEST_MAX_FILENAME];
        if (pos + 4 > indexlen) {
            if (out_opt) out_opt->code = MRP_GUEST_LOOKUP_ERR_3006;
            return MRP_GUEST_LOOKUP_ERR_3006;
        }
        len = rd_u32_le(data + index_off + pos);
        pos += 4;
        if (((len + pos) > indexlen) || (len < 1) || (len >= MRP_GUEST_MAX_FILENAME)) {
            if (out_opt) out_opt->code = MRP_GUEST_LOOKUP_ERR_3004;
            return MRP_GUEST_LOOKUP_ERR_3004;
        }
        memset(temp_name, 0, sizeof(temp_name));
        memcpy(temp_name, data + index_off + pos, len);
        pos += len;
        if (strcmp(member_name, temp_name) == 0) {
            if (pos + 8 > indexlen) {
                if (out_opt) out_opt->code = MRP_GUEST_LOOKUP_ERR_3004;
                return MRP_GUEST_LOOKUP_ERR_3004;
            }
            file_pos = rd_u32_le(data + index_off + pos);
            file_len = rd_u32_le(data + index_off + pos + 4);
            if (out_opt) {
                out_opt->code = MRP_GUEST_LOOKUP_HIT;
                out_opt->offset = file_pos;
                out_opt->stored_size = file_len;
                out_opt->reserved = (pos + 12 <= indexlen)
                                        ? rd_u32_le(data + index_off + pos + 8)
                                        : 0;
            }
            return MRP_GUEST_LOOKUP_HIT;
        }
        pos += 12;
        if (pos >= indexlen) {
            if (out_opt) out_opt->code = MRP_GUEST_LOOKUP_ERR_3006;
            return MRP_GUEST_LOOKUP_ERR_3006;
        }
    }
}
