# P21 Runtime Frame Isolation

| Gate | Hit |
|---|---|
| 1 gbrwcore P != gamelist P | NO (gbrw=0x2AC8DC gl=0x0) |
| 2 parent P intact | NO (sha before=0x0 after=0x0) |
| 3 callback R9 = parent | YES (owner=GBRWCORE r9=0x2B0D18) |
| 4 no 0x30D5D2 fault | YES |
| 5 startGame enter | NO |

## Fault dataflow

- FAULT_R9_OWNER = **GBRWCORE**
- [R9+0x1914] = 0x2AC8F8
- FAULT_SLOT_1914_LAST_WRITER = pc=0x30D52E module=gbrwcore.ext writes=8 first_val=0x0
- stop=mem_fault
