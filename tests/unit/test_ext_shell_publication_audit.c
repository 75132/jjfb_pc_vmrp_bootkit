#include "gwy_launcher/ext_shell_publication_audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
#ifdef _WIN32
    _putenv("JJFB_PUBLICATION_AUDIT=1");
#else
    setenv("JJFB_PUBLICATION_AUDIT", "1", 1);
#endif
    ext_shell_publication_audit_reset();
    if (!ext_shell_publication_audit_enabled()) {
        const char *e = getenv("JJFB_PUBLICATION_AUDIT");
        fprintf(stderr, "expected enabled getenv=%s\n", e ? e : "(null)");
        return 1;
    }
    ext_shell_publication_audit_on_p_candidate(0x2AC8DC);
    ext_shell_publication_audit_on_p_write(0x2AC8DC, 0x00, 0, 0x111, 0x30CA00, 0, "gbrwcore.ext");
    ext_shell_publication_audit_on_p_write(0x2AC8DC, 0x04, 0, 0x19A8, 0x30CA10, 0, "gbrwcore.ext");
    ext_shell_publication_audit_on_p_write(0x2AC8DC, 0x08, 0, 1, 0x30CA20, 0, "gbrwcore.ext");
    ext_shell_publication_audit_on_mem_fault(0x30CCF8, 0x28, 0x2AC8DC, 0);
    printf("test_ext_shell_publication_audit OK\n");
    return 0;
}
