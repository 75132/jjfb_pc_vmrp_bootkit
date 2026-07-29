#ifndef GWY_LAUNCHER_P19_STARTGAME_CONTRACT_H
#define GWY_LAUNCHER_P19_STARTGAME_CONTRACT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * P19: close API table → lib.startGame VM call → opcode 300 → nested MRP.
 *
 * Env:
 *   JJFB_P19_STARTGAME_CONTRACT=1
 *
 * Static VAs below are research breakpoints for current gbrwcore.ext SHA only.
 * Live code uses mapped_base + file_offset; never hardcode as product entry.
 *
 * File offsets (gbrwcore.ext):
 *   API table builder  0x1B400
 *   startGame name STR 0x1B4F4
 *   startGame fn  STR  0x1B4FA
 *   startGame entry    0x1AE74
 *   parser return      0x1AE9A
 *   opcode300 BLX      0x1AEB8
 */

int p19_startgame_contract_enabled(void);
void p19_startgame_contract_reset(void);

void p19_startgame_contract_on_module_map(const char *module_name, uint32_t base, uint32_t size);
void p19_startgame_contract_bind_uc(void *uc);
void p19_startgame_contract_on_code(void *uc, uint64_t module_id, const char *module_name,
                                    uint32_t pc, const uint32_t regs[16]);

/* Prefer gamelist logic (UI may be suppressed) over gbrwshell-only continue. */
int p19_startgame_contract_prefer_gamelist_continue(void);

void p19_startgame_contract_finalize(const char *stop_reason);

/* Gate progress (1 = hit). */
int p19_gate_api_builder(void);
int p19_gate_startgame_ptr(void);
int p19_gate_startgame_entry(void);
int p19_gate_three_args(void);
int p19_gate_opcode300(void);
int p19_gate_nested_jjfb(void);
int p19_gate_child_robotol(void);

#ifdef __cplusplus
}
#endif

#endif
