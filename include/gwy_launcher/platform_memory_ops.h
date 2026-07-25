#ifndef GWY_LAUNCHER_PLATFORM_MEMORY_OPS_H
#define GWY_LAUNCHER_PLATFORM_MEMORY_OPS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Formal guest memory ops (Task 10 / Task 12).
 *
 * platform_guest_memcpy — binary-safe Guest→Guest copy by guest VA.
 * platform_guest_strlen — bounded C-string length by guest VA.
 *
 * Import binding (JJFB_PLATFORM_MEMCPY_IMPORT, default ON when unset):
 *   Robotol R9-relative copy import currently resolves to DSM VA 0x804A8
 *   (CODE_ADDRESS+0x4A8), which is not memcpy. When enabled, that import
 *   identity/slot is bound to a host callback that runs platform_guest_memcpy
 *   and returns via LR through the normal host-callback frame. Binding is by
 *   slot VA identity, not by PC==0x2E4ECA.
 *
 * DSM strlen body (0xAC374, E10A-proven): post-gate 0x2FD886 BLX's this VA
 * directly (not only via mr_table+0x3C). A bad C-string (e.g. cursor idx 0x16)
 * otherwise UC_MEM_READ_UNMAPPED. Host-bind the same way as 0x804A8.
 *
 * DSM strcpy body (0xAC300): post-gate 0x310740 BLX's strcpy(dst, src) with
 * src sometimes the same cursor idx after a zero-length strlen — bind equally.
 *
 * mr_table:
 *   +0xC  memcpy  → platform_guest_memcpy (br_memcpy)
 *   +0x14 strcpy  → platform_guest_strcpy
 *   +0x38 memset  → br_memset
 *   +0x3C strlen  → platform_guest_strlen (br_strlen)
 *
 * Robotol libc FP cache (module registration, not PC heal):
 *   ER_RW+0x144C = memset, ER_RW+0x1450 = strlen
 *   downVersion compare (0x2D9648) BLX's +0x1450; a mis-filled memset VA
 *   (DSM 0x94F04) turns strlen into a multi-megabyte fill loop.
 *
 * Calibration (temporary):
 *   JJFB_FIELD_STREAM_CONTRACT=0/1  — fixed-PC Scheme C pre-copy (default OFF)
 *   JJFB_PLATFORM_MEMCPY_IMPORT=0/1 — formal import (default ON)
 */

#define PLATFORM_MEMCPY_IMPORT_SLOT_VA 0x804A8u
#define PLATFORM_STRLEN_DSM_BODY_VA 0xAC374u
#define PLATFORM_STRCPY_DSM_BODY_VA 0xAC300u
#define PLATFORM_MEMCPY_MR_TABLE_OFF 0xCu
#define PLATFORM_STRCPY_MR_TABLE_OFF 0x14u
#define PLATFORM_MEMSET_MR_TABLE_OFF 0x38u
#define PLATFORM_STRLEN_MR_TABLE_OFF 0x3Cu
#define PLATFORM_ROBOTOL_ERW_MEMSET_OFF 0x144Cu
#define PLATFORM_ROBOTOL_ERW_STRLEN_OFF 0x1450u

uint32_t platform_guest_memcpy(void *uc, uint32_t dst_guest, uint32_t src_guest, uint32_t size);
uint32_t platform_guest_strlen(void *uc, uint32_t str_guest);
uint32_t platform_guest_strcpy(void *uc, uint32_t dst_guest, uint32_t src_guest);

int platform_memcpy_import_enabled(void);
void platform_memcpy_import_reset(void);
void platform_memcpy_import_bind_uc(void *uc);
void platform_memcpy_import_arm(void *uc);

/* Publish mr_table memset/strlen into Robotol ER_RW libc cache once per ER_RW. */
int platform_libc_cache_publish(void *uc, uint32_t er_rw, uint32_t mr_table);

uint32_t platform_memcpy_import_call_count(void);
uint32_t platform_memcpy_import_fail_count(void);

#ifdef __cplusplus
}
#endif

#endif
