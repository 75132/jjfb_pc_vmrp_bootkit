#ifndef GWY_LAUNCHER_LAUNCH_RUNTIME_H
#define GWY_LAUNCHER_LAUNCH_RUNTIME_H

#include "gwy_launcher/error.h"
#include "gwy_launcher/launch_descriptor.h"

/* Prepare VFS checks + manifest, then spawn clean upstream vmrp. */
LauncherStatus gwy_launch_spawn_vmrp(const LaunchDescriptor *desc,
                                     const char *vmrp_exe,
                                     const char *vmrp_cwd,
                                     const char *manifest_path,
                                     LauncherError *err);

/*
 * Same as gwy_launch_spawn_vmrp, but keeps the child process handle for the
 * caller (Win32 HANDLE*). Caller must CloseHandle. out_process/out_pid optional.
 */
LauncherStatus gwy_launch_spawn_vmrp_ex(const LaunchDescriptor *desc,
                                        const char *vmrp_exe,
                                        const char *vmrp_cwd,
                                        const char *manifest_path,
                                        void **out_process,
                                        unsigned long *out_pid,
                                        LauncherError *err);

#endif
