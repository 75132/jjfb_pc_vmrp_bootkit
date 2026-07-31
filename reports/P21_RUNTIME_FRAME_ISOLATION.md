# P21 Runtime Frame Isolation (runner)

## Isolation gates

| Gate | Hit |
|---|---|
| 1 P isolated (log) | YES |
| 2 parent P SHA intact | YES |
| 3 callback R9 parent | YES |
| 4 no 0x30D5D2 fault | YES |
| 5/6 startGame live | NO (0x2AAD84) |

## P20 lifecycle (prerequisite)

| Gate | Hit |
|---|---|
| command=0 | YES |
| 0x10102 | YES |
| callback | YES |
| lazy | YES |
| builder | YES |
| sg_ptr | YES |
| fire_ext | YES |
| continue | YES |
| opcode300 | NO |
| nested | NO |

- image_base: 0x2EB7FC
- log: C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\out\\p21_isolation\gate_20260731_015850\vm_stdout.txt
- Policy: no forced R9/slot, no shared-P overwrite, no find_by_p(latest) for exec

