#ifndef GWY_LAUNCHER_E10A31J_SMSCFG_LONG_H
#define GWY_LAUNCHER_E10A31J_SMSCFG_LONG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * E10A-3.1j: SMSCFG boot-to-method0 writer provenance (long window).
 *
 * Env:
 *   JJFB_E10A31J_MODE=1
 *   JJFB_E10A31J_WINDOW=full|gamelist   (default full)
 *   JJFB_E10A31J_RUN_ID=<n>
 *   JJFB_E10A31J_PTR_CSV / WRITE_CSV / API_CSV / IO_CSV / CKPT_CSV / COMPARE_CSV
 *
 * Observe only: no GPT write, no method0 patch, no invented dsm.cfg.
 */

#define E10A31J_SMS_CFG_LEN 0x10E0u
#define E10A31J_GPT_OFF 0x349u
#define E10A31J_MR_TABLE_SMSCFG_OFF 0x1C0u

int e10a31j_enabled(void);
void e10a31j_reset(void);
void e10a31j_bind_uc(void *uc);
void e10a31j_mark_milestone(const char *name, const char *note);

/* Lane A: mr_table / slot+0x1C0 lifetime. */
void e10a31j_on_mr_table(uint32_t mr_table_guest);
void e10a31j_on_erw(void *uc, uint32_t erw_base, uint32_t erw_size);
void e10a31j_poll_pointer(void *uc, const char *phase, const char *package, const char *module);

/* Lane E/F: phase checkpoints (also arms write hooks when pointer known). */
void e10a31j_on_phase(void *uc, const char *phase, const char *module, const char *package);

/* Lane B: UC write + direct host memcpy/memset/read overlap. */
void e10a31j_on_block_copy(void *uc, uint32_t dst, uint32_t src, uint32_t len);
void e10a31j_on_memset(void *uc, uint32_t dst, uint32_t value, uint32_t len);
void e10a31j_on_file_read(void *uc, uint32_t dst, uint32_t len, int32_t ret, const char *api);

/* Lane C: bridge / unimplemented MAP_FUNC / TestCom-like. */
void e10a31j_on_host_api_enter(void *uc, uint32_t slot, const char *name);
void e10a31j_on_host_api_leave(void *uc, uint32_t slot, const char *name);
void e10a31j_on_unimpl_api(void *uc, uint32_t slot, const char *name);

/* Lane D: VFS paths involving dsm.cfg. */
void e10a31j_on_vfs(void *uc, const char *op, const char *module, const char *guest_path,
                    const char *host_path, int exists, int rc, uint32_t pc, uint32_t lr);

/* Stop / method0 boundary. */
void e10a31j_on_method0_enter(void *uc, uint32_t helper);
void e10a31j_on_method0_return(void *uc, uint32_t helper, int32_t ret);
void e10a31j_on_ext_first_pc(void);

/* Optional: gamelist insn window (smsGetBytes / GPT strcmp). */
void e10a31j_on_method0_insn(void *uc, uint32_t pc, uint32_t lr, uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3, uint32_t r4, uint32_t r9);

int e10a31j_stop_requested(void);
const char *e10a31j_window_name(void);

#ifdef __cplusplus
}
#endif

#endif
