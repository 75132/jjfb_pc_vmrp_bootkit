#ifndef GWY_LAUNCHER_PLATFORM_MEMORY_OPS_H
#define GWY_LAUNCHER_PLATFORM_MEMORY_OPS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Formal guest memory ops (Task 10).
 *
 * platform_guest_memcpy — binary-safe Guest→Guest copy by guest VA.
 * Does not use strlen, does not leak host pointers, does not mutate Guest
 * state fields, and does not key off JJFB call-site PCs.
 *
 * Import binding (JJFB_PLATFORM_MEMCPY_IMPORT, default ON when unset):
 *   Robotol R9-relative copy import currently resolves to DSM VA 0x804A8
 *   (CODE_ADDRESS+0x4A8), which is not memcpy. When enabled, that import
 *   identity/slot is bound to a host callback that runs platform_guest_memcpy
 *   and returns via LR through the normal host-callback frame. Binding is by
 *   slot VA identity, not by PC==0x2E4ECA.
 *
 * mr_table+0xC (name=memcpy) also routes through platform_guest_memcpy via
 * br_memcpy.
 *
 * Calibration (temporary):
 *   JJFB_FIELD_STREAM_CONTRACT=0/1  — fixed-PC Scheme C pre-copy (default OFF)
 *   JJFB_PLATFORM_MEMCPY_IMPORT=0/1 — formal import (default ON)
 */

#define PLATFORM_MEMCPY_IMPORT_SLOT_VA 0x804A8u
#define PLATFORM_MEMCPY_MR_TABLE_OFF 0xCu

uint32_t platform_guest_memcpy(void *uc, uint32_t dst_guest, uint32_t src_guest, uint32_t size);

int platform_memcpy_import_enabled(void);
void platform_memcpy_import_reset(void);
void platform_memcpy_import_bind_uc(void *uc);
void platform_memcpy_import_arm(void *uc);

uint32_t platform_memcpy_import_call_count(void);
uint32_t platform_memcpy_import_fail_count(void);

#ifdef __cplusplus
}
#endif

#endif
