#ifndef GWY_LAUNCHER_ERROR_H
#define GWY_LAUNCHER_ERROR_H

typedef enum LauncherStatus {
    L_OK = 0,
    L_ERR_INVALID_ARGUMENT,
    L_ERR_IO,
    L_ERR_FORMAT,
    L_ERR_BOUNDS,
    L_ERR_HASH_MISMATCH,
    L_ERR_PROFILE_MISMATCH,
    L_ERR_NOT_FOUND,
    L_ERR_UNSUPPORTED,
    L_ERR_GUEST_FAULT,
    L_ERR_DECOMPRESSION,
    L_ERR_STATE,
    L_ERR_NETWORK
} LauncherStatus;

typedef struct LauncherError {
    LauncherStatus code;
    const char *subsystem;
    char message[256];
    char detail[512];
} LauncherError;

void launcher_error_clear(LauncherError *err);
void launcher_error_set(LauncherError *err,
                        LauncherStatus code,
                        const char *subsystem,
                        const char *message,
                        const char *detail);
const char *launcher_status_name(LauncherStatus code);

#endif
