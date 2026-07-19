#include "launcher/launch_descriptor.h"
#include <assert.h>
#include <string.h>

int main(void) {
    LaunchDescriptor d;
    memset(&d, 0, sizeof(d));
    strcpy(d.resource_root, "fixture-root");
    strcpy(d.target_mrp, "gwy/example.mrp");
    strcpy(d.entry_member, "start.mr");
    assert(launch_descriptor_validate_basic(&d) == 1);
    strcpy(d.target_mrp, "../escape.mrp");
    assert(launch_descriptor_validate_basic(&d) == 0);
    return 0;
}
