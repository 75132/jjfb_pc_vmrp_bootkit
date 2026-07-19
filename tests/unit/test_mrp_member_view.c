#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/mrp_archive.h"
#include "gwy_launcher/mrp_guest_index.h"
#include "gwy_launcher/mrp_member_view.h"
#include "gwy_launcher/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static const char *fixture_jjfb(void) {
    const char *root = getenv("GWY_FIXTURE_ROOT");
    static char path[1024];
    if (!root || !root[0]) return NULL;
    snprintf(path, sizeof(path), "%s/gwy/jjfb.mrp", root);
    return path;
}

int main(void) {
    const char *jjfb = fixture_jjfb();
    MrpArchive *arch = NULL;
    const MrpMember *robotol = NULL;
    LauncherError err;
    uint8_t *view = NULL;
    size_t view_size = 0;
    MrpGuestLookupResult lr;
    MrpGuestLookupCode code;
    ByteBuffer decoded_alias;
    ByteBuffer decoded_robotol;
    uint8_t sha_alias[32];
    uint8_t sha_robotol[32];
    char hex_a[65];
    char hex_r[65];
    size_t entry_size;

    if (!jjfb) {
        puts("skip: GWY_FIXTURE_ROOT unset");
        return 0;
    }
    if (mrp_archive_open(jjfb, &arch, &err) != L_OK) {
        fprintf(stderr, "open failed: %s\n", err.message);
        return 1;
    }
    if (mrp_archive_find_exact(arch, "robotol.ext", &robotol, &err) != L_OK) {
        mrp_archive_close(arch);
        return 1;
    }

    /* Original pack: cfunction.ext must be clean miss (3006), not 3004. */
    code = mrp_guest_index_lookup(arch->data, arch->size, "cfunction.ext", &lr);
    if (code != MRP_GUEST_LOOKUP_ERR_3006) {
        fprintf(stderr, "expected original 3006 got %d\n", (int)code);
        mrp_archive_close(arch);
        return 1;
    }

    if (mrp_member_view_synthesize_alias(arch, "cfunction.ext", robotol, &view, &view_size, &err) !=
        L_OK) {
        fprintf(stderr, "synthesize failed: %s\n", err.message);
        mrp_archive_close(arch);
        return 1;
    }
    entry_size = 4 + strlen("cfunction.ext") + 1 + 12;
    if (view_size != arch->size + entry_size) {
        fprintf(stderr, "view size mismatch\n");
        free(view);
        mrp_archive_close(arch);
        return 1;
    }

    /* Fixed view: guest LOOKUP must HIT without 3004. */
    code = mrp_guest_index_lookup(view, view_size, "cfunction.ext", &lr);
    if (code != MRP_GUEST_LOOKUP_HIT) {
        fprintf(stderr, "guest lookup failed code=%d (3004 means torn index)\n", (int)code);
        free(view);
        mrp_archive_close(arch);
        return 1;
    }
    if (lr.offset != robotol->offset + (uint32_t)entry_size) {
        fprintf(stderr, "alias offset mismatch got=%u want=%u\n", lr.offset,
                robotol->offset + (uint32_t)entry_size);
        free(view);
        mrp_archive_close(arch);
        return 1;
    }
    if (lr.stored_size != robotol->stored_size) {
        fprintf(stderr, "stored_size mismatch\n");
        free(view);
        mrp_archive_close(arch);
        return 1;
    }

    /* Decode via host archive reopen of view file bytes. */
    {
        MrpArchive *varch = NULL;
        const MrpMember *alias_m = NULL;
        char tmp[512];
#ifdef _WIN32
        char tmpdir[MAX_PATH];
        GetTempPathA(sizeof(tmpdir), tmpdir);
        snprintf(tmp, sizeof(tmp), "%s\\gwy_view_test.mrp", tmpdir);
#else
        snprintf(tmp, sizeof(tmp), "/tmp/gwy_view_test.mrp");
#endif
        {
            FILE *fp = fopen(tmp, "wb");
            if (!fp || fwrite(view, 1, view_size, fp) != view_size) {
                if (fp) fclose(fp);
                free(view);
                mrp_archive_close(arch);
                return 1;
            }
            fclose(fp);
        }
        if (mrp_archive_open(tmp, &varch, &err) != L_OK) {
            fprintf(stderr, "reopen view: %s\n", err.message);
            free(view);
            mrp_archive_close(arch);
            return 1;
        }
        if (mrp_archive_find_exact(varch, "cfunction.ext", &alias_m, &err) != L_OK) {
            fprintf(stderr, "host find alias miss\n");
            mrp_archive_close(varch);
            free(view);
            mrp_archive_close(arch);
            return 1;
        }
        byte_buffer_init(&decoded_alias);
        byte_buffer_init(&decoded_robotol);
        if (mrp_archive_decode_member(varch, alias_m, 8u * 1024u * 1024u, &decoded_alias, &err) !=
            L_OK) {
            fprintf(stderr, "decode alias fail\n");
            mrp_archive_close(varch);
            free(view);
            mrp_archive_close(arch);
            return 1;
        }
        if (mrp_archive_decode_member(arch, robotol, 8u * 1024u * 1024u, &decoded_robotol, &err) !=
            L_OK) {
            fprintf(stderr, "decode robotol fail\n");
            byte_buffer_free(&decoded_alias);
            mrp_archive_close(varch);
            free(view);
            mrp_archive_close(arch);
            return 1;
        }
        if (decoded_alias.size != 253420 || decoded_robotol.size != 253420) {
            fprintf(stderr, "size want 253420 got alias=%zu robotol=%zu\n", decoded_alias.size,
                    decoded_robotol.size);
            byte_buffer_free(&decoded_alias);
            byte_buffer_free(&decoded_robotol);
            mrp_archive_close(varch);
            free(view);
            mrp_archive_close(arch);
            return 1;
        }
        gwy_sha256(decoded_alias.data, decoded_alias.size, sha_alias);
        gwy_sha256(decoded_robotol.data, decoded_robotol.size, sha_robotol);
        gwy_sha256_hex(sha_alias, hex_a);
        gwy_sha256_hex(sha_robotol, hex_r);
        if (memcmp(sha_alias, sha_robotol, 32) != 0) {
            fprintf(stderr, "sha mismatch alias=%s robotol=%s\n", hex_a, hex_r);
            byte_buffer_free(&decoded_alias);
            byte_buffer_free(&decoded_robotol);
            mrp_archive_close(varch);
            free(view);
            mrp_archive_close(arch);
            return 1;
        }
        printf("[MRP_VIEW_READ] member=cfunction.ext offset=%u requested_bytes=%u returned_bytes=%u "
               "result=OK\n",
               lr.offset, lr.stored_size, lr.stored_size);
        printf("[MRP_VIEW_DECODE] member=cfunction.ext stored_size=%u unpacked_size=%zu "
               "sha256=%s result=OK\n",
               lr.stored_size, decoded_alias.size, hex_a);
        byte_buffer_free(&decoded_alias);
        byte_buffer_free(&decoded_robotol);
        mrp_archive_close(varch);
    }

    free(view);
    mrp_archive_close(arch);
    puts("test_mrp_member_view OK");
    puts("3004 root_cause=LOOKUP invalid_name_len after torn index (insert_at=first_data); "
         "fix=insert_at=first_data+12");
    return 0;
}
