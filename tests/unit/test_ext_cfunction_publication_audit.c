#include "gwy_launcher/ext_cfunction_publication_audit.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
static void set_env(const char *kv) { _putenv(kv); }
#else
static void set_env_pair(const char *k, const char *v) { setenv(k, v, 1); }
#endif

int main(void) {
#ifdef _WIN32
    set_env("JJFB_CFUNCTION_PUBLICATION_AUDIT=1");
    set_env("JJFB_CHUNK_FIELD04_AUDIT=1");
    set_env("JJFB_P_TIMELINE_TRACE=1");
#else
    set_env_pair("JJFB_CFUNCTION_PUBLICATION_AUDIT", "1");
    set_env_pair("JJFB_CHUNK_FIELD04_AUDIT", "1");
    set_env_pair("JJFB_P_TIMELINE_TRACE", "1");
#endif
    ext_cfunction_publication_audit_reset();
    if (!ext_cfunction_publication_audit_enabled()) return 1;
    ext_cfunction_publication_audit_on_cfunction_new(0x30CFE9u, 20u, 0x2AC8DCu, "GUEST_NESTED");
    ext_cfunction_publication_audit_on_ext_register("gbrwcore.ext", 3, 0x30CFE9u, 0x2EB7E8u, 0);
    ext_cfunction_publication_audit_on_p_write(0x2AC8DCu, 0x0Cu, 0, 0, 0x94F04u, 0x89D5Cu,
                                               "dsm:cfunction.ext");
    ext_cfunction_publication_audit_finalize("harness");
    printf("test_ext_cfunction_publication_audit OK\n");
    return 0;
}
