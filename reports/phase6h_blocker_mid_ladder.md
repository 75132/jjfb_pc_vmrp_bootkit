# Phase 6H — mid-ladder blocker

## Verdict

**Min ladder PASS** (`shell_native_exec_gate=open`, guest PC in `gbrwcore.ext`, exports registered as string VAs).  
**Mid ladder BLOCKED**: guest faults inside `gbrwcore.ext` with `P+0xC=0` before any gamelist/`lib.runapp` call.

## Evidence (TARGET_OBSERVED)

| Claim | Tag / fact |
|---|---|
| Guest shell EXT executes | `[JJFB_SHELL_GUEST_PC] pc=0x2EB804 module=gbrwcore.ext` |
| Gate open (progress) | `[JJFB_SHELL_NATIVE_GATE] ... continue_observe` |
| Final class | `[JJFB_SHELL_NATIVE_SUMMARY] class=EXEC_GATE_OPEN stop=shell_ext_fault_in_guest_pc` |
| Exports are string VAs, not entries | `kind=string_va_not_entry` for `lib.startGame` / `lib.runapp` |
| Host equivalent absent | no `host_runapp_equivalent_after_no_update` |
| Callee ER_RW missing | `R9_SWITCH_BLOCKED reason=CALLEE_ER_RW_NOT_AVAILABLE module=gbrwcore.ext` |
| Entry vs header | first_pc=`0x30CA96` header=`0x2EB7E8` (`WRONG_ENTRY_SELECTION`) |
| Nested P empty at +0xC | `P=0x2AC8DC P+0xC=0x0` pre/post continuation **and** pre_fault |
| Fault | `UC_MEM_READ_UNMAPPED` `fault_pc=0x30CCF8` `fault_addr=0x28` `expr=r0+0x28 r0=0x0` |
| Context class | `gwy_context_class=SHELL_LOADED_BUT_NO_EXTCHUNK` |
| Gamelist not started | SUMMARY `gamelist=no` `export_called=no` |

## Classification

- Shell `cfunction.ext` → reg.ext primary: **CROSS_TARGET** (working for gbrwcore).
- Gate / guest PC / fault-through-null: **TARGET_OBSERVED**.
- “Gamelist post-update builds cfg36 then calls runapp”: **HYPOTHESIS** (not reached; blocked earlier).

## Forbidden

No invented P+0xC, no R9 promotion, no force UI, no host runapp equivalent.

## Discriminating result

Fault is **`*(NULL + 0x28)`** while `r3/P=0x2AC8DC` and `P+0xC=0`. Mid is blocked on **guest shell publication / extChunk fill**, not on runapp dispatch.

`next_allowed_fix=SHELL_PUBLICATION_ROUTINE_AUDIT` (observe which guest routine should write `P+0xC` before `0x30CCD0` path; do not invent the pointer).
