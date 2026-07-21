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

void e10a_shell_event(uint32_t event_code, uint32_t handler_pc, uint32_t pc, uint32_t lr,
                      const char *note);

void e10a_shell_update(const char *request, const char *response, const char *note);

#ifdef __cplusplus
}
#endif

#endif
