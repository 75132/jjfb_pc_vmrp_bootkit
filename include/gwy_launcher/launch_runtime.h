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

#endif
