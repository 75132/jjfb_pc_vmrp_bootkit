#ifndef GWY_LAUNCHER_E10A_SHELL_TRACE_H
#define GWY_LAUNCHER_E10A_SHELL_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int e10a_shell_trace_enabled(void);

void e10a_shell_phase(const char *phase, const char *module, uint32_t pc, uint32_t lr,
                      uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3, uint32_t r9,
                      uint32_t ac8, const char *note);

void e10a_shell_vfs(const char *op, const char *module, const char *guest_path,
                    const char *host_path, int exists, int rc, uint32_t pc, uint32_t lr);

void e10a_vfs_set_ctx_from_uc(void *uc, const char *api_name, const char *module,
                              const char *phase, uint32_t path_ptr, const char *arg_source);
void e10a_vfs_note_guest_code(const char *module, uint32_t pc);

/* E10A-2: track real guest transfers into mr_open stub (bl/blx/bx), not only LR. */
void e10a_note_mr_open_stub(uint32_t stub_addr);
int e10a_is_mr_open_stub(uint32_t addr);
void e10a_note_open_enter_branch(uint32_t call_pc, uint32_t target, const char *kind, int rm,
                                 uint32_t r0, uint32_t r1, uint32_t r9, uint32_t lr);
void e10a_dump_open_enter_branches(const char *why);
void e10a_note_guest_pc_sample(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t lr);
void e10a_dump_guest_pc_ring(const char *why);

void e10a_shell_cfg_runtime(const char *stage, uint32_t cfg_buf_ptr, uint32_t cfg_buf_size,
                            uint32_t record_offset, const char *title, const char *target_path,
                            const char *icon, uint32_t target_ptr, const char *target_bytes,
                            const char *file_api_args, const char *note);

void e10a_shell_event(uint32_t event_code, uint32_t handler_pc, uint32_t pc, uint32_t lr,
                      const char *note);

void e10a_shell_update(const char *request, const char *response, const char *note);

#ifdef __cplusplus
}
#endif

#endif
