#include "gwy_launcher/ext_gwy_shell_shim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
static void set_env(const char *k, const char *v) {
    char buf[512];
    if (!v) {
        snprintf(buf, sizeof(buf), "%s=", k);
    } else {
        snprintf(buf, sizeof(buf), "%s=%s", k, v);
    }
    _putenv(buf);
}
#else
static void set_env(const char *k, const char *v) {
    if (v) setenv(k, v, 1);
    else unsetenv(k);
}
#endif

int main(void) {
    int32_t ret = -1;

    set_env("JJFB_GWY_LAUNCHER_MODE", "1");
    set_env("JJFB_LAUNCH_PATH", "gwy_shell_post_update");
    set_env("JJFB_DISABLE_JJFB_ALIAS_DIRECT", "1");
    set_env("JJFB_GWY_UPDATE_STUB", "no_update");
    set_env("JJFB_GAME_SELF_PATCH", "0");
    /* Clear Full Boot leftovers so shell_core_continue_mode() stays off. */
    set_env("JJFB_NATIVE_BOOT_FULL", NULL);
    set_env("JJFB_SHELL_CHAIN_MODE", NULL);
    set_env("JJFB_RUNAPP_NATIVE_ONLY", NULL);
    set_env("GWY_LAUNCH_PARAM",
            "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink");

    ext_gwy_shell_shim_reset();
    if (!ext_gwy_shell_shim_enabled()) {
        fprintf(stderr, "shim not enabled\n");
        return 1;
    }
    if (!ext_gwy_shell_shim_alias_direct_disabled()) {
        fprintf(stderr, "alias disable expected\n");
        return 1;
    }

    ext_gwy_shell_shim_emit_banner("game_files/mythroad/240x320",
                                   "game_files/mythroad/240x320/gwy");
    ext_gwy_shell_shim_on_start_dsm("mythroad/gwy/gbrwcore.mrp", "start.mr",
                                    "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_"
                                    "nmrpname=gwy/jjfb.mrp_gwyblink");
    ext_gwy_shell_shim_on_file_open("gwy/gbrwcore.mrp",
                                    "game_files/mythroad/240x320/gwy/gbrwcore.mrp", 1);

    if (!ext_gwy_shell_shim_try_init_network(NULL, 0, "cmnet", 0, &ret) || ret != 0) {
        fprintf(stderr, "update stub failed ret=%d\n", (int)ret);
        return 1;
    }

    /* Game self must not be stubbed. */
    ext_gwy_shell_shim_on_start_dsm("mythroad/gwy/jjfb.mrp", "start.mr", NULL);
    if (ext_gwy_shell_shim_try_init_network(NULL, 0, "cmnet", 0, &ret)) {
        fprintf(stderr, "game self was stubbed (forbidden)\n");
        return 1;
    }

    /* Prepare path: shell warmup tags + jjfb target (VFS may be unbound in unit). */
    {
        char tgt[160];
        char prm[320];
        ext_gwy_shell_shim_reset();
        set_env("JJFB_GWY_LAUNCHER_MODE", "1");
        set_env("JJFB_LAUNCH_PATH", "gwy_shell_post_update");
        set_env("JJFB_GWY_UPDATE_STUB", "no_update");
        set_env("JJFB_GAME_SELF_PATCH", "0");
        set_env("GWY_LAUNCH_PARAM",
                "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink");
        if (!ext_gwy_shell_shim_prepare_jjfb_launch(tgt, sizeof(tgt), prm, sizeof(prm))) {
            fprintf(stderr, "prepare_jjfb_launch failed\n");
            return 1;
        }
        if (strcmp(tgt, "gwy/jjfb.mrp") != 0) {
            fprintf(stderr, "unexpected target %s\n", tgt);
            return 1;
        }
        ext_gwy_shell_shim_on_file_open("gwy/jjfb.mrp",
                                        "game_files/mythroad/240x320/gwy/jjfb.mrp", 1);
        ext_gwy_shell_shim_finalize("unit");
        if (ext_gwy_shell_shim_last_class() != GWY_SHELL_CLASS_SHELL_RUNAPP_CHAINED) {
            fprintf(stderr, "class=%s\n",
                    ext_gwy_shell_shim_class_name(ext_gwy_shell_shim_last_class()));
            return 1;
        }
    }

    /* Phase 6H: guest-native mode must NOT override DSM target to jjfb. */
    {
        char tgt[160];
        char prm[320];
        ext_gwy_shell_shim_reset();
        set_env("JJFB_GWY_LAUNCHER_MODE", "1");
        set_env("JJFB_LAUNCH_PATH", "gwy_guest_native_runapp");
        set_env("JJFB_SHELL_NATIVE_EXEC_TRACE", "1");
        set_env("JJFB_GWY_UPDATE_STUB", "no_update");
        set_env("JJFB_GAME_SELF_PATCH", "0");
        if (!ext_gwy_shell_shim_guest_native_mode()) {
            fprintf(stderr, "guest_native_mode expected\n");
            return 1;
        }
        if (ext_gwy_shell_shim_prepare_jjfb_launch(tgt, sizeof(tgt), prm, sizeof(prm))) {
            fprintf(stderr, "native mode must not host-override to jjfb\n");
            return 1;
        }
        ext_gwy_shell_shim_on_start_dsm("gwy/gbrwcore.mrp", "start.mr",
                                        "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_"
                                        "nmrpname=gwy/jjfb.mrp_gwyblink");
        ext_gwy_shell_shim_on_file_open("gwy/gbrwcore.mrp",
                                        "game_files/mythroad/240x320/gwy/gbrwcore.mrp", 1);
        ext_gwy_shell_shim_on_file_open("gwy/gbrwshell.mrp",
                                        "game_files/mythroad/240x320/gwy/gbrwshell.mrp", 1);
        ext_gwy_shell_shim_finalize("unit_native");
        if (ext_gwy_shell_shim_last_class() != GWY_SHELL_CLASS_SHELL_NATIVE_EXEC_PENDING) {
            fprintf(stderr, "native class=%s\n",
                    ext_gwy_shell_shim_class_name(ext_gwy_shell_shim_last_class()));
            return 1;
        }
    }

    /* Phase 6P: continue gate after gbrwcore start, before gamelist. */
    {
        char tgt[160];
        char prm[320];
        ext_gwy_shell_shim_reset();
        set_env("JJFB_GWY_LAUNCHER_MODE", "1");
        set_env("JJFB_LAUNCH_PATH", "gwy_shell_core_continue");
        set_env("JJFB_SHELL_CHAIN_MODE", "continue_after_gbrwcore_init");
        set_env("JJFB_SHELL_NATIVE_EXEC_TRACE", "1");
        set_env("JJFB_GWY_UPDATE_STUB", "no_update");
        set_env("JJFB_GAME_SELF_PATCH", "0");
        set_env("GWY_LAUNCH_PARAM",
                "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink");
        if (!ext_gwy_shell_shim_shell_core_continue_mode()) {
            fprintf(stderr, "shell_core_continue_mode expected\n");
            return 1;
        }
        if (ext_gwy_shell_shim_prepare_jjfb_launch(tgt, sizeof(tgt), prm, sizeof(prm))) {
            fprintf(stderr, "6P must not host-override to jjfb\n");
            return 1;
        }
        if (ext_gwy_shell_shim_try_continue_after_mr_exit(NULL, tgt, sizeof(tgt), prm,
                                                          sizeof(prm))) {
            fprintf(stderr, "continue before gbrwcore start must fail\n");
            return 1;
        }
        ext_gwy_shell_shim_on_start_dsm("gwy/gbrwcore.mrp", "start.mr",
                                        "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_"
                                        "nmrpname=gwy/jjfb.mrp_gwyblink");
        if (!ext_gwy_shell_shim_try_continue_after_mr_exit(NULL, tgt, sizeof(tgt), prm,
                                                           sizeof(prm))) {
            fprintf(stderr, "continue after gbrwcore start expected\n");
            return 1;
        }
        if (strcmp(tgt, "gwy/gamelist.mrp") != 0) {
            fprintf(stderr, "continue target=%s\n", tgt);
            return 1;
        }
        /* Second continue must not fire. */
        if (ext_gwy_shell_shim_try_continue_after_mr_exit(NULL, tgt, sizeof(tgt), prm,
                                                          sizeof(prm))) {
            fprintf(stderr, "second continue must fail\n");
            return 1;
        }
    }

    puts("ok");
    return 0;
}
