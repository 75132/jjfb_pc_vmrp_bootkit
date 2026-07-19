#include "gwy_launcher/mrp_reg_primary.h"
#include "gwy_launcher/byte_buffer.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int name_eq_ci(const char *a, const char *b) {
    size_t i;
    if (!a || !b) return 0;
    for (i = 0; a[i] && b[i]; i++) {
        char ca = (char)tolower((unsigned char)a[i]);
        char cb = (char)tolower((unsigned char)b[i]);
        if (ca != cb) return 0;
    }
    return a[i] == 0 && b[i] == 0;
}

static int ends_with_ci(const char *s, const char *suf) {
    size_t n, m;
    if (!s || !suf) return 0;
    n = strlen(s);
    m = strlen(suf);
    if (n < m) return 0;
    return name_eq_ci(s + (n - m), suf);
}

static int is_skip_ext(const char *name) {
    return name_eq_ci(name, "reg.ext") || name_eq_ci(name, "mrc_loader.ext") ||
           name_eq_ci(name, "logo.ext") || name_eq_ci(name, "cfunction.ext");
}

static int member_exists(const MrpArchive *archive, const char *name) {
    size_t i;
    for (i = 0; i < archive->member_count; i++) {
        if (name_eq_ci(archive->members[i].name, name)) return 1;
    }
    return 0;
}

static void package_stem(const char *path, char *out, size_t cap) {
    const char *base;
    const char *slash;
    size_t n;
    if (!path || !out || cap == 0) return;
    out[0] = '\0';
    slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, '\\');
    base = slash ? slash + 1 : path;
    n = strlen(base);
    if (n > 4 && name_eq_ci(base + n - 4, ".mrp")) n -= 4;
    if (n >= cap) n = cap - 1;
    memcpy(out, base, n);
    out[n] = '\0';
}

static int is_name_boundary(unsigned char c) {
    return c == 0 || !(isalnum(c) || c == '_' || c == '.' || c == '-');
}

/*
 * CROSS_TARGET: some shell packages (e.g. gamelist) embed primary EXT name as a
 * substring inside a binary NUL-run in reg.ext, so a pure NUL-token scan misses it.
 */
static int buf_contains_member_name(const uint8_t *data, size_t size, const char *name) {
    size_t n;
    size_t i;
    if (!data || !name || !name[0]) return 0;
    n = strlen(name);
    if (n < 5 || n > size) return 0;
    for (i = 0; i + n <= size; i++) {
        size_t k;
        int match = 1;
        for (k = 0; k < n; k++) {
            char a = (char)tolower((unsigned char)data[i + k]);
            char b = (char)tolower((unsigned char)name[k]);
            if (a != b) {
                match = 0;
                break;
            }
        }
        if (!match) continue;
        if (i > 0 && !is_name_boundary(data[i - 1])) continue;
        if (i + n < size && !is_name_boundary(data[i + n])) continue;
        return 1;
    }
    return 0;
}

int mrp_archive_find_reg_primary(const MrpArchive *archive, char *out_name, size_t out_cap) {
    const MrpMember *reg = NULL;
    ByteBuffer buf;
    LauncherError err;
    char stem[64];
    char prefer[MRP_MEMBER_NAME_MAX];
    char first[MRP_MEMBER_NAME_MAX];
    size_t i;
    int found_prefer = 0;
    int found_first = 0;
    const char *source = NULL;

    if (!archive || !out_name || out_cap == 0) return 0;
    out_name[0] = '\0';
    prefer[0] = '\0';
    first[0] = '\0';

    for (i = 0; i < archive->member_count; i++) {
        if (name_eq_ci(archive->members[i].name, "reg.ext")) {
            reg = &archive->members[i];
            break;
        }
    }
    if (!reg) return 0;

    memset(&buf, 0, sizeof(buf));
    if (mrp_archive_decode_member(archive, reg, 256u * 1024u, &buf, &err) != L_OK) return 0;

    package_stem(archive->path, stem, sizeof(stem));
    if (stem[0]) {
        snprintf(prefer, sizeof(prefer), "%s.ext", stem);
    }

    i = 0;
    while (i < buf.size) {
        size_t start = i;
        size_t len;
        char cand[MRP_MEMBER_NAME_MAX];
        while (i < buf.size && buf.data[i] != 0) i++;
        len = i - start;
        if (i < buf.size) i++; /* skip NUL */
        if (len < 5) continue;
        if (len < sizeof(cand)) {
            memcpy(cand, buf.data + start, len);
            cand[len] = '\0';
            if (!ends_with_ci(cand, ".ext")) continue;
            if (is_skip_ext(cand)) continue;
            if (!member_exists(archive, cand)) continue;
            if (!found_first) {
                snprintf(first, sizeof(first), "%s", cand);
                found_first = 1;
            }
            if (prefer[0] && name_eq_ci(cand, prefer)) {
                found_prefer = 1;
                snprintf(out_name, out_cap, "%s", cand);
                source = "reg.ext+package_stem";
                break;
            }
        } else if (prefer[0] && member_exists(archive, prefer) &&
                   buf_contains_member_name(buf.data + start, len, prefer)) {
            /* Long binary NUL-run ending with / containing package stem primary. */
            found_prefer = 1;
            snprintf(out_name, out_cap, "%s", prefer);
            source = "reg.ext+embedded_package_stem";
            break;
        }
    }

    if (!found_prefer && prefer[0] && member_exists(archive, prefer) &&
        buf_contains_member_name(buf.data, buf.size, prefer)) {
        found_prefer = 1;
        snprintf(out_name, out_cap, "%s", prefer);
        source = "reg.ext+embedded_package_stem";
    }

    if (!found_prefer && !found_first) {
        /* Last resort: any non-skip .ext member name appearing in reg.ext bytes. */
        for (i = 0; i < archive->member_count; i++) {
            const char *mn = archive->members[i].name;
            if (!ends_with_ci(mn, ".ext") || is_skip_ext(mn)) continue;
            if (!buf_contains_member_name(buf.data, buf.size, mn)) continue;
            snprintf(first, sizeof(first), "%s", mn);
            found_first = 1;
            break;
        }
    }

    byte_buffer_free(&buf);

    if (found_prefer) {
        printf("[REG_PRIMARY] package=%s primary=%s evidence=CROSS_TARGET source=%s\n",
               archive->path, out_name, source ? source : "reg.ext+package_stem");
        fflush(stdout);
        return 1;
    }
    if (found_first) {
        snprintf(out_name, out_cap, "%s", first);
        printf("[REG_PRIMARY] package=%s primary=%s evidence=CROSS_TARGET "
               "source=reg.ext+first_ext_member\n",
               archive->path, out_name);
        fflush(stdout);
        return 1;
    }
    return 0;
}
