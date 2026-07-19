# Phase 6K — gbrwcore entry result

- MRPGCMAP_ENTRY_HIT: `yes`
- entry emu OK: `yes`
- natural P+0xC write: `no`
- live image_base: `0x2eb7e0`

## Static +0xC init clusters vs image+8

Evidence: **HYPOTHESIS** until live PC lands in cluster during ENTRY_HIT path.

| module | file_off | VA if base known | notes |
|---|---|---|---|
| `gbrwcore.ext` | file_off=15542 | `0x2EF496` | STR cluster with +0/+4/+8/+0xC/+0x10 |
| `gbrwcore.ext` | file_off=40586 | `0x2F566A` | STR cluster with +0/+4/+8/+0xC/+0x10 |
| `gbrwcore.ext` | file_off=42998 | `0x2F5FD6` | STR cluster with +0/+4/+8/+0xC/+0x10 |
| `gbrwcore.ext` | file_off=59094 | `0x2F9EB6` | STR cluster with +0/+4/+8/+0xC/+0x10 |
| `gbrwcore.ext` | file_off=138168 | `0x30D398` | STR cluster with +0/+4/+8/+0xC/+0x10 |
| `gamelist.ext` | file_off=61560 | `n/a` | STR cluster with +0/+4/+8/+0xC/+0x10 |
| `gbrwshell.ext` | file_off=36100 | `n/a` | STR cluster with +0/+4/+8/+0xC/+0x10 |

## Live order tags

- `loaded`: `yes`
- `entry_called`: `yes`
- `entry_returned`: `yes`
- `callback_continuation`: `yes`
