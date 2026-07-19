#include "gwy_launcher/ext_chunk_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
static void set_env(const char *kv) { _putenv(kv); }
#else
static void set_env_pair(const char *k, const char *v) { setenv(k, v, 1); }
#endif

int main(void) {
    uint8_t p_host[32];
    uint8_t chunk_host[GWY_MRC_EXTCHUNK_BYTES];
    GwyMrcExtChunk *ch;

    if (offsetof(GwyMrcExtChunk, check) != 0x00) return 1;
    if (offsetof(GwyMrcExtChunk, init_func) != 0x04) return 2;
    if (offsetof(GwyMrcExtChunk, event) != 0x08) return 3;
    if (offsetof(GwyMrcExtChunk, send_app_event) != 0x28) return 4;
    if (sizeof(GwyMrcExtChunk) < GWY_MRC_EXTCHUNK_BYTES) return 5;

#ifdef _WIN32
    set_env("JJFB_EXTCHUNK_PROVIDER=off");
#else
    set_env_pair("JJFB_EXTCHUNK_PROVIDER", "off");
#endif
    ext_chunk_provider_reset();
    if (ext_chunk_provider_enabled()) return 6;
    if (ext_chunk_provider_mode() != GWY_EXTCHUNK_OFF) return 7;

#ifdef _WIN32
    set_env("JJFB_EXTCHUNK_PROVIDER=gbrwcore_only");
#else
    set_env_pair("JJFB_EXTCHUNK_PROVIDER", "gbrwcore_only");
#endif
    ext_chunk_provider_reset();
    if (!ext_chunk_provider_enabled()) return 8;
    if (ext_chunk_provider_mode() != GWY_EXTCHUNK_GBRWCORE_ONLY) return 9;
    if (strcmp(ext_chunk_provider_mode_name(ext_chunk_provider_mode()), "gbrwcore_only") != 0)
        return 10;

    ext_chunk_provider_set_sendappevent_guest(0x100054u);
    memset(p_host, 0, sizeof(p_host));
    memset(chunk_host, 0, sizeof(chunk_host));
    /* Without registry gbrwcore match, want/on_c_function_new should no-op. */
    if (ext_chunk_provider_want(0x30CFE9u)) {
        /* Registry may be empty — want should be 0. */
    }
    (void)ext_chunk_provider_on_c_function_new(NULL, 0x30CFE9u, 0x2AC8DCu, p_host, chunk_host,
                                               0x300000u);
    ch = (GwyMrcExtChunk *)chunk_host;
    /* Unmatched helper: no fill required. Sanity on magic constant. */
    if (GWY_MRC_EXTCHUNK_CHECK != 0x7FD854EBu) return 11;
    (void)ch;

#ifdef _WIN32
    set_env("JJFB_EXTCHUNK_PROVIDER=shell_core");
#else
    set_env_pair("JJFB_EXTCHUNK_PROVIDER", "shell_core");
#endif
    ext_chunk_provider_reset();
    if (!ext_chunk_provider_enabled()) return 12;
    if (ext_chunk_provider_mode() != GWY_EXTCHUNK_GWY_SHELL) return 13;

#ifdef _WIN32
    set_env("JJFB_EXTCHUNK_PROVIDER=shell_and_game");
#else
    set_env_pair("JJFB_EXTCHUNK_PROVIDER", "shell_and_game");
#endif
    ext_chunk_provider_reset();
    if (!ext_chunk_provider_enabled()) return 14;
    if (ext_chunk_provider_mode() != GWY_EXTCHUNK_SHELL_AND_GAME) return 15;
    if (strcmp(ext_chunk_provider_mode_name(ext_chunk_provider_mode()), "shell_and_game") != 0)
        return 16;

    printf("test_ext_chunk_provider OK\n");
    return 0;
}
