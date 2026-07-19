#include "gwy_launcher/ext_er_rw_bind_restore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
static void set_env(const char *kv) { _putenv(kv); }
#else
static void set_env_pair(const char *k, const char *v) { setenv(k, v, 1); }
#endif

int main(void) {
#ifdef _WIN32
    set_env("JJFB_ER_RW_BIND_RESTORE=off");
#else
    set_env_pair("JJFB_ER_RW_BIND_RESTORE", "off");
#endif
    ext_er_rw_bind_restore_reset();
    if (ext_er_rw_bind_restore_enabled()) return 1;
    if (ext_er_rw_bind_restore_mode() != GWY_ER_RW_BIND_OFF) return 2;

#ifdef _WIN32
    set_env("JJFB_ER_RW_BIND_RESTORE=gbrwcore_only");
#else
    set_env_pair("JJFB_ER_RW_BIND_RESTORE", "gbrwcore_only");
#endif
    ext_er_rw_bind_restore_reset();
    if (!ext_er_rw_bind_restore_enabled()) return 3;
    if (ext_er_rw_bind_restore_mode() != GWY_ER_RW_BIND_GBRWCORE_ONLY) return 4;
    if (strcmp(ext_er_rw_bind_restore_mode_name(ext_er_rw_bind_restore_mode()), "gbrwcore_only") !=
        0)
        return 5;

    /* Zero reject: no registry/P — must not claim bound. */
    ext_er_rw_bind_restore_on_p_write(0x2AC8DCu, 0x00u, 0, "gbrwcore.ext");
    ext_er_rw_bind_restore_peek_and_bind(0x2AC8DCu, "mr_c_function_st_metadata_bind");
    if (ext_er_rw_bind_restore_bound()) return 6;

#ifdef _WIN32
    set_env("JJFB_ER_RW_BIND_RESTORE=shell_core");
#else
    set_env_pair("JJFB_ER_RW_BIND_RESTORE", "shell_core");
#endif
    ext_er_rw_bind_restore_reset();
    if (!ext_er_rw_bind_restore_enabled()) return 7;
    if (ext_er_rw_bind_restore_mode() != GWY_ER_RW_BIND_SHELL_CORE) return 8;
    if (strcmp(ext_er_rw_bind_restore_mode_name(ext_er_rw_bind_restore_mode()), "shell_core") != 0)
        return 9;

#ifdef _WIN32
    set_env("JJFB_ER_RW_BIND_RESTORE=shell_and_game");
#else
    set_env_pair("JJFB_ER_RW_BIND_RESTORE", "shell_and_game");
#endif
    ext_er_rw_bind_restore_reset();
    if (!ext_er_rw_bind_restore_enabled()) return 10;
    if (ext_er_rw_bind_restore_mode() != GWY_ER_RW_BIND_SHELL_AND_GAME) return 11;
    if (strcmp(ext_er_rw_bind_restore_mode_name(ext_er_rw_bind_restore_mode()),
               "shell_and_game") != 0)
        return 12;

#ifdef _WIN32
    set_env("JJFB_ER_RW_BIND_RESTORE=game_package");
#else
    set_env_pair("JJFB_ER_RW_BIND_RESTORE", "game_package");
#endif
    ext_er_rw_bind_restore_reset();
    if (!ext_er_rw_bind_restore_enabled()) return 13;
    if (ext_er_rw_bind_restore_mode() != GWY_ER_RW_BIND_GAME_PACKAGE) return 14;
    if (strcmp(ext_er_rw_bind_restore_mode_name(ext_er_rw_bind_restore_mode()), "game_package") !=
        0)
        return 15;

    printf("test_ext_er_rw_bind_restore OK\n");
    return 0;
}
