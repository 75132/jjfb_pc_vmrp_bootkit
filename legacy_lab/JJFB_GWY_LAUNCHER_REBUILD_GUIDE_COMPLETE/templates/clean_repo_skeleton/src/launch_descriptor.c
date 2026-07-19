#include "launcher/launch_descriptor.h"
#include <string.h>

int launch_descriptor_validate_basic(const LaunchDescriptor *desc) {
    if (desc == NULL) return 0;
    if (desc->resource_root[0] == '\0') return 0;
    if (desc->target_mrp[0] == '\0') return 0;
    if (desc->entry_member[0] == '\0') return 0;
    if (strstr(desc->target_mrp, "..") != NULL) return 0;
    return 1;
}
