# P19 startGame Runtime Contract

## Gates

| Gate | Hit |
|---|---|
| 1 API builder 0x1B400 | YES |
| 2 startGame function_ptr nonzero | YES (0x2AAD84) |
| 3 startGame entry | NO |
| 4 three args parsed | NO |
| 5 opcode 300 | NO |
| 6 nested jjfb | NO |
| 7 child robotol | NO |

## startGame pointer

- gbrwcore base: `0x2EB7E0`
- live R9: `0x2B0D18`
- table `[R9+8]`: `0x2ACFA0`
- name `@table+0x74`: `0x6` ("lib.startGame")
- function_ptr `@table+0x78`: `0x2AAD84`
- research assert Thumb ptr: `0x306655` (not a product hardcode)

## Three arguments

- not captured (parser return not hit)

## Opcode 300 dispatcher

- `[R9+0x1488]`: `0xDE207313`
- observed owner/module: `gamelist.ext`
- writer pc hint: `0x2D4758`

## Nested JJFB / first screen

- nested jjfb: NO
- child robotol: NO
- gbrwcore max PC offset seen: `0x222CA` (api_builder needs `0x1B400`)
- parent code15: not claimed here (see matrix)
- first-screen vs direct_boot: see `P19_NESTED_JJFB_MATRIX.csv`

## Policy

- No descriptor-string call into startGame
- No static `0x306655` as product entry
- No synthetic code15 / forced E6C
- cfg36 napptype=12 from live cfg.bin
