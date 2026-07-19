#ifndef GWY_LAUNCHER_MRP_GUEST_INDEX_H
#define GWY_LAUNCHER_MRP_GUEST_INDEX_H

#include "gwy_launcher/error.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DOCUMENTED: mythroad _mr_readFile new-MRP LOOKUP (MR_MAX_FILENAME_SIZE=128). */
#define MRP_GUEST_MAX_FILENAME 128

typedef enum MrpGuestLookupCode {
    MRP_GUEST_LOOKUP_HIT = 0,
    MRP_GUEST_LOOKUP_ERR_3001 = 3001,
    MRP_GUEST_LOOKUP_ERR_3004 = 3004,
    MRP_GUEST_LOOKUP_ERR_3006 = 3006
} MrpGuestLookupCode;

typedef struct MrpGuestLookupResult {
    MrpGuestLookupCode code;
    uint32_t offset;
    uint32_t stored_size;
    uint32_t reserved;
    uint32_t indexlen;
    uint32_t header_length;
    uint32_t first_data_plus4;
} MrpGuestLookupResult;

/*
 * Simulate guest new-MRP member LOOKUP only (no payload read).
 * Returns HIT, 3004 (invalid name length mid-walk), or 3006 (not found).
 */
MrpGuestLookupCode mrp_guest_index_lookup(const uint8_t *data,
                                          size_t size,
                                          const char *member_name,
                                          MrpGuestLookupResult *out_opt);

#ifdef __cplusplus
}
#endif

#endif
