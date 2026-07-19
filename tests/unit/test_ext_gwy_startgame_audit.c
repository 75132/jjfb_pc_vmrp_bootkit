#include "gwy_launcher/ext_gwy_startgame_audit.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <stdlib.h>
#endif

static void env_on(void) {
#ifdef _WIN32
    _putenv("GWY_GWY_STARTGAME_AUDIT=1");
    _putenv("GWY_LAUNCH_PARAM=napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink");
    _putenv("GWY_LAUNCH_TARGET=gwy/jjfb.mrp");
    _putenv("GWY_RESOURCE_ROOT=game_files/mythroad/240x320");
#else
    setenv("GWY_GWY_STARTGAME_AUDIT", "1", 1);
    setenv("GWY_LAUNCH_PARAM",
           "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink", 1);
    setenv("GWY_LAUNCH_TARGET", "gwy/jjfb.mrp", 1);
    setenv("GWY_RESOURCE_ROOT", "game_files/mythroad/240x320", 1);
#endif
}

static void env_off(void) {
#ifdef _WIN32
    _putenv("GWY_GWY_STARTGAME_AUDIT=");
#else
    unsetenv("GWY_GWY_STARTGAME_AUDIT");
#endif
}

int main(void) {
    ext_gwy_startgame_audit_reset();
    env_off();
    ext_gwy_startgame_audit_reset();

    if (strcmp(ext_gwy_startgame_class_name(GWY_SG_SHELL_BYPASSED_DIRECT_JJFB),
               "SHELL_BYPASSED_DIRECT_JJFB") != 0)
        return 1;
    if (ext_gwy_startgame_context_gate_open()) return 2;

    env_on();
    ext_gwy_startgame_audit_reset();
    if (!ext_gwy_startgame_audit_enabled()) return 3;

    ext_gwy_startgame_audit_emit_launch_context();
    ext_gwy_startgame_audit_on_start_dsm("gwy/jjfb.mrp", "*", NULL);
    ext_gwy_startgame_audit_on_file_open("gwy/jjfb.mrp", 1);
    ext_gwy_startgame_audit_finalize("HARNESS_TIMEOUT");
    if (ext_gwy_startgame_context_gate_open()) return 4;
    if (ext_gwy_startgame_audit_last_class() != GWY_SG_SHELL_BYPASSED_DIRECT_JJFB) return 5;

    env_off();
    printf("test_ext_gwy_startgame_audit OK\n");
    return 0;
}
