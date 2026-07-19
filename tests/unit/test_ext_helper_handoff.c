#include "gwy_launcher/ext_helper_handoff.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    ext_helper_handoff_reset();
    if (strcmp(ext_handoff_case_name(HANDOFF_CASE_FIELD_NOT_UPDATED), "FIELD_NOT_UPDATED") != 0)
        return 1;
    if (strcmp(ext_handoff_case_name(HANDOFF_CASE_RETURN_LOST), "RETURN_LOST") != 0) return 1;
    if (ext_helper_handoff_enabled()) return 1;
    if (ext_helper_handoff_last_case() != HANDOFF_CASE_UNKNOWN) return 1;
    /* A8: FIELD_NOT_UPDATED remains a named case; status/causal are live log fields. */
    printf("test_ext_helper_handoff OK\n");
    return 0;
}
