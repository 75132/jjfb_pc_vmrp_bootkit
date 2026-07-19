#include "gwy_launcher/byte_buffer.h"
#include "gwy_launcher/ext_callback_frame.h"
#include "gwy_launcher/module_registry.h"
#include "gwy_launcher/mrp_archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Golden file offsets (test-only; not used by executable core logic). */
enum {
    MRC_CALL_OFF = 0x2Cu,
    MRC_CONT_OFF = 0x30u,
    MRC_HELPER_OFF = 0x44u,
    ROB_CALL_OFF = 0x2AD9Cu,
    ROB_CONT_OFF = 0x2AD9Eu,
    ROB_HELPER_OFF = 0x2BCF9u
};

static int decode_member(const MrpArchive *a, const char *name, ByteBuffer *out) {
    const MrpMember *m = NULL;
    LauncherError err;
    if (mrp_archive_find_exact(a, name, &m, &err) != L_OK) return 0;
    if (mrp_archive_decode_member(a, m, 4u * 1024u * 1024u, out, &err) != L_OK) return 0;
    return 1;
}

static int check_mrc_loader(const ByteBuffer *buf) {
    const uint8_t *p = buf->data;
    uint32_t w;
    if (buf->size < MRC_HELPER_OFF + 4u) return 0;
    /* ARM BX r2 at +0x2C: 0xE12FFF12 */
    w = (uint32_t)p[MRC_CALL_OFF] | ((uint32_t)p[MRC_CALL_OFF + 1] << 8) |
        ((uint32_t)p[MRC_CALL_OFF + 2] << 16) | ((uint32_t)p[MRC_CALL_OFF + 3] << 24);
    if (w != 0xE12FFF12u) {
        fprintf(stderr, "mrc_loader call insn=0x%08X expect BX r2\n", w);
        return 0;
    }
    /* Continuation at +0x30 should be POP {r3,lr} or similar — just ensure not zero pad. */
    w = (uint32_t)p[MRC_CONT_OFF] | ((uint32_t)p[MRC_CONT_OFF + 1] << 8) |
        ((uint32_t)p[MRC_CONT_OFF + 2] << 16) | ((uint32_t)p[MRC_CONT_OFF + 3] << 24);
    if (w == 0) return 0;
    (void)MRC_HELPER_OFF;
    return 1;
}

static int check_robotol(const ByteBuffer *buf) {
    const uint8_t *p = buf->data;
    uint16_t h0, h1;
    if (buf->size < ROB_HELPER_OFF + 2u) return 0;
    /* Thumb BLX r2 at +0x2AD9C: 0x4790 */
    h0 = (uint16_t)(p[ROB_CALL_OFF] | (p[ROB_CALL_OFF + 1] << 8));
    if (h0 != 0x4790u) {
        fprintf(stderr, "robotol call half=0x%04X expect BLX r2\n", h0);
        return 0;
    }
    /* Continuation MOVS r5,#0 at +0x2AD9E: 0x2500 */
    h1 = (uint16_t)(p[ROB_CONT_OFF] | (p[ROB_CONT_OFF + 1] << 8));
    if (h1 != 0x2500u) {
        fprintf(stderr, "robotol cont half=0x%04X expect MOVS r5,#0\n", h1);
        return 0;
    }
    return 1;
}

int main(void) {
    const char *root;
    char path[1024];
    MrpArchive *a = NULL;
    LauncherError err;
    ByteBuffer loader;
    ByteBuffer robotol;

    ext_callback_frame_reset();
    if (strcmp(ext_callback_frame_class_name(CF_CLASS_CONTINUATION_CONFIRMED_R9_LOST),
               "CONTINUATION_CONFIRMED_R9_LOST") != 0)
        return 1;
    if (strcmp(gwy_entry_relation_name(GWY_ENTRY_REL_CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW),
               "CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW") != 0)
        return 2;
    if (ext_callback_frame_enabled()) return 3;
    if (ext_callback_frame_gate_open()) return 4;
    if (ext_callback_frame_last_class() != CF_CLASS_UNKNOWN) return 5;

    root = getenv("GWY_FIXTURE_ROOT");
    if (!root || !root[0]) {
        printf("test_ext_callback_frame OK (no fixture; API only)\n");
        return 0;
    }
    snprintf(path, sizeof(path), "%s/gwy/jjfb.mrp", root);
    if (mrp_archive_open(path, &a, &err) != L_OK) {
        fprintf(stderr, "open jjfb: %s\n", err.message);
        return 6;
    }
    memset(&loader, 0, sizeof(loader));
    memset(&robotol, 0, sizeof(robotol));
    if (!decode_member(a, "mrc_loader.ext", &loader)) {
        fprintf(stderr, "decode mrc_loader failed\n");
        mrp_archive_close(a);
        return 7;
    }
    if (!decode_member(a, "robotol.ext", &robotol)) {
        fprintf(stderr, "decode robotol failed\n");
        byte_buffer_free(&loader);
        mrp_archive_close(a);
        return 8;
    }
    if (!check_mrc_loader(&loader)) {
        byte_buffer_free(&loader);
        byte_buffer_free(&robotol);
        mrp_archive_close(a);
        return 9;
    }
    if (!check_robotol(&robotol)) {
        byte_buffer_free(&loader);
        byte_buffer_free(&robotol);
        mrp_archive_close(a);
        return 10;
    }
    byte_buffer_free(&loader);
    byte_buffer_free(&robotol);
    mrp_archive_close(a);
    printf("test_ext_callback_frame OK (static continuation offsets)\n");
    return 0;
}
