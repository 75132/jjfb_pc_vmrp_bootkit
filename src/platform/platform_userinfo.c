#include "gwy_launcher/platform_userinfo.h"
#include <string.h>

LauncherStatus platform_userinfo_fill(const PlatformIdentity *id,
                                      const char *imsi,
                                      PlatformUserInfoBlob *out,
                                      LauncherError *err) {
    LauncherStatus st;
    const char *use_imsi;
    size_t n;

    launcher_error_clear(err);
    if (!out) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "userinfo", "null out", NULL);
        return L_ERR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    st = platform_identity_validate(id, err);
    if (st != L_OK) return st;

    use_imsi = (imsi && imsi[0]) ? imsi : GWY_DEFAULT_IMSI;
    if (strlen(use_imsi) == 0 || strlen(use_imsi) > 15) {
        launcher_error_set(err, L_ERR_INVALID_ARGUMENT, "userinfo", "IMSI length", use_imsi);
        return L_ERR_INVALID_ARGUMENT;
    }

    /* DOCUMENTED mr_userinfo field packing (little-endian host == guest). */
    n = strlen(id->imei);
    if (n > 15) n = 15;
    memcpy(out->bytes + 0x00, id->imei, n);

    n = strlen(use_imsi);
    if (n > 15) n = 15;
    memcpy(out->bytes + 0x10, use_imsi, n);

    n = strlen(id->manufacturer);
    if (n > 7) n = 7;
    memcpy(out->bytes + 0x20, id->manufacturer, n);

    n = strlen(id->model);
    if (n > 7) n = 7;
    memcpy(out->bytes + 0x28, id->model, n);

    out->ver = GWY_DEFAULT_USERINFO_VER;
    memcpy(out->bytes + 0x30, &out->ver, 4);

    out->filled = GWY_MR_USERINFO_BYTES;
    return L_OK;
}
