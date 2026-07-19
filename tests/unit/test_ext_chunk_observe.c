#include "gwy_launcher/ext_chunk_observe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void set_env(const char *k, const char *v) {
#ifdef _WIN32
    _putenv_s(k, v);
#else
    setenv(k, v, 1);
#endif
}

int main(void) {
    ExtChunkRecord *a;
    ExtChunkRecord *b;

    ext_chunk_observe_reset();
    set_env("GWY_CHUNK_PROVENANCE", "1");

    if (!ext_chunk_provenance_enabled()) {
        fprintf(stderr, "provenance env not enabled\n");
        return 1;
    }

    a = ext_chunk_observe_register(0x200000, 0x40u, GWY_CHUNK_ORIGIN_GUEST_ALLOCATED, 0x1000, 1);
    if (!a || a->chunk_id == 0 || a->guest_base != 0x200000) {
        fprintf(stderr, "register failed\n");
        return 1;
    }
    if (strcmp(gwy_chunk_origin_name(a->origin), "GUEST_ALLOCATED") != 0) return 1;

    b = ext_chunk_observe_find(0x200000);
    if (b != a) return 1;
    if (!ext_chunk_observe_find_covering(0x200010)) return 1;
    if (ext_chunk_observe_find_covering(0x300000)) return 1;

    ext_chunk_observe_on_p_plus_0c(0x300000, 0x100000);
    b = ext_chunk_observe_find(0x300000);
    if (!b || b->origin != GWY_CHUNK_ORIGIN_FROM_P_PLUS_0C) return 1;

    if (strcmp(ext_chunk_field_04_class(0), "unknown") != 0) return 1;
    if (strcmp(ext_chunk_field_04_class(0x42), "data") != 0) return 1;

    ext_chunk_observe_reset();
    printf("test_ext_chunk_observe OK\n");
    return 0;
}
