#include "gwy_launcher/error.h"
#include <stdio.h>
#include <string.h>

void launcher_error_clear(LauncherError *err) {
    if (!err) return;
    memset(err, 0, sizeof(*err));
}

void launcher_error_set(LauncherError *err,
                        LauncherStatus code,
                        const char *subsystem,
                        const char *message,
                        const char *detail) {
    if (!err) return;
    err->code = code;
    err->subsystem = subsystem ? subsystem : "";
    if (message) {
        snprintf(err->message, sizeof(err->message), "%s", message);
    } else {
        err->message[0] = '\0';
    }
    if (detail) {
        snprintf(err->detail, sizeof(err->detail), "%s", detail);
    } else {
        err->detail[0] = '\0';
    }
}

const char *launcher_status_name(LauncherStatus code) {
    switch (code) {
    case L_OK: return "L_OK";
    case L_ERR_INVALID_ARGUMENT: return "L_ERR_INVALID_ARGUMENT";
    case L_ERR_IO: return "L_ERR_IO";
    case L_ERR_FORMAT: return "L_ERR_FORMAT";
    case L_ERR_BOUNDS: return "L_ERR_BOUNDS";
    case L_ERR_HASH_MISMATCH: return "L_ERR_HASH_MISMATCH";
    case L_ERR_PROFILE_MISMATCH: return "L_ERR_PROFILE_MISMATCH";
    case L_ERR_NOT_FOUND: return "L_ERR_NOT_FOUND";
    case L_ERR_UNSUPPORTED: return "L_ERR_UNSUPPORTED";
    case L_ERR_GUEST_FAULT: return "L_ERR_GUEST_FAULT";
    case L_ERR_DECOMPRESSION: return "L_ERR_DECOMPRESSION";
    case L_ERR_STATE: return "L_ERR_STATE";
    case L_ERR_NETWORK: return "L_ERR_NETWORK";
    default: return "L_ERR_UNKNOWN";
    }
}
