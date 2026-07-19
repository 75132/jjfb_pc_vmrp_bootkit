#ifndef GWY_LAUNCHER_GWY_CFG_H
#define GWY_LAUNCHER_GWY_CFG_H

#include "gwy_launcher/error.h"
#include <stdbool.h>
#include <stdint.h>

/* Empirical framing for the supplied GWY cfg.bin (CROSS_TARGET / TARGET_OBSERVED). */
#define GWY_CFG_RECORD_BASE 1024u
#define GWY_CFG_RECORD_SIZE 272u

typedef struct GwyCfgFieldU32 {
    bool present;
    uint32_t value;
    uint32_t offset;
    uint32_t length;
    const char *encoding;
    const char *confidence;
} GwyCfgFieldU32;

typedef struct GwyCfgFieldString {
    bool present;
    char value[128];
    uint32_t offset;
    uint32_t length;
    const char *encoding;
    const char *confidence;
} GwyCfgFieldString;

typedef struct GwyCfgRecord {
    uint32_t index;
    uint32_t file_offset;
    uint32_t record_size;
    const char *layout_confidence;
    GwyCfgFieldString icon;
    GwyCfgFieldU32 napptype;
    GwyCfgFieldString title_suffix;
    GwyCfgFieldU32 nextid;
    GwyCfgFieldU32 ncode;
    GwyCfgFieldU32 narg;
    GwyCfgFieldU32 narg1;
    GwyCfgFieldString target_mrp;
    uint8_t raw[GWY_CFG_RECORD_SIZE];
} GwyCfgRecord;

typedef struct GwyCfgFile {
    char path[1024];
    uint8_t *data;
    size_t size;
    uint8_t sha256[32];
} GwyCfgFile;

LauncherStatus gwy_cfg_open(const char *path, GwyCfgFile **out, LauncherError *err);
void gwy_cfg_close(GwyCfgFile *cfg);

LauncherStatus gwy_cfg_read_record(const GwyCfgFile *cfg,
                                   uint32_t index,
                                   GwyCfgRecord *out,
                                   LauncherError *err);

#endif
