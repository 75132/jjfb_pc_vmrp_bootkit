# P20 gbrwcore Module Lifecycle

## Gates

| Gate | Hit |
|---|---|
| 1 command=0 | YES |
| 2 0x10102 register | YES |
| 3 callback 0x30B7C4 | YES |
| 4 lazy init | YES |
| 5 API builder | YES |
| 6 startGame ptr | YES (0x2AAD84) |
| 7 startGame entry | NO |
| 8 opcode 300 | NO |
| 9 nested jjfb | NO |

## Identity

- map_base: `0x2EB7E0`
- image_base: `0x2EB7FC` (pad `0x1C`)
- helper: `0x30CFE9` P: `0x2AC8DC` live R9: `0x2B0D18`
- 10102 family=`0x11100` callback=`0x30B7E1` owner=`gbrwcore.ext`
- first event: `0x2` skipped_7d7e=0
- startGame name=`?` fn=`0x2AAD84` table=`0x2ACFA0`

Policy: no forced PC/R9, no host callback write, no forged events, no code15/E6C.
