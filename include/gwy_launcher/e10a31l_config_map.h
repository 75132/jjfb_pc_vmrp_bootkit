#ifndef GWY_LAUNCHER_E10A31L_CONFIG_MAP_H
#define GWY_LAUNCHER_E10A31L_CONFIG_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1l: batch extraction of method0 platform-config preconditions.
 *
 * Env:
 *   JJFB_E10A31L_MODE=1
 *   JJFB_E10A31L_MEM_READ=1   (default on with MODE)
 *   JJFB_E10A31L_COMPARE=1
 *   JJFB_E10A31L_RUN_ID
 *   JJFB_E10A31L_READ_CSV / COMPARE_CSV / GWY_CSV / MANIFEST_JSON
 *
 * Observe only during config_map: no profile writes beyond existing bootstrap.
 */

#define E10A31L_PC_SMSGETBYTES_BL 0x2E5396u
#define E10A31L_PC_MEMCPY_BLX 0x2E31AEu
#define E10A31L_PC_STRCMP_BLX 0x2E53A6u
#define E10A31L_PC_STRCMP_ENTER 0xAC2D0u
#define E10A31L_PC_STRCMP_TRUE_FAIL 0xAC2E8u

typedef enum E10a31lLhsSource {
    E10A31L_LHS_UNKNOWN = 0,
    E10A31L_LHS_SMSCFG_FIELD = 1,
    E10A31L_LHS_LAUNCH_PARAM = 2,
    E10A31L_LHS_DESCRIPTOR_FIELD = 3,
    E10A31L_LHS_PACKAGE_NAME = 4,
    E10A31L_LHS_VFS_PATH = 5,
    E10A31L_LHS_GLOBAL_PLATFORM_STATE = 6,
    E10A31L_LHS_STACK_LOCAL = 7,
    E10A31L_LHS_HEAP_BUFFER = 8,
    E10A31L_LHS_STACK_FROM_SMSCFG = 9 /* stack/heap copy proven from sms_cfg read */
} E10a31lLhsSource;

int e10a31l_enabled(void);
void e10a31l_reset(void);
void e10a31l_on_method0_enter(void *uc, uint32_t helper);
void e10a31l_on_method0_return(void *uc, uint32_t helper, int32_t ret);
void e10a31l_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9, uint32_t cpsr,
                             const uint8_t *bytes, uint32_t size);
void e10a31l_mark_milestone(const char *name, const char *note);

const char *e10a31l_lhs_source_name(E10a31lLhsSource s);
E10a31lLhsSource e10a31l_last_gwy_lhs_source(void);

#ifdef __cplusplus
}
#endif

#endif
