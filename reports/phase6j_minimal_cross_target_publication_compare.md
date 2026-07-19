# Phase 6J — minimal cross-target publication compare

## Member SHA (jjfb vs wxjwq)

| member | jjfb sha256 | jjfb len | wxjwq sha256 | wxjwq len | equal |
|---|---|---|---|---|---|
| `start.mr` | `c8d664aa7034d044ded428158f9cca1d49a3781242421eb1b1de7f201949ff05` | `3787` | `c8d664aa7034d044ded428158f9cca1d49a3781242421eb1b1de7f201949ff05` | `3787` | `yes` |

- `start.mr` identical → **CROSS_TARGET** shared bootstrap.
| `mrc_loader.ext` | `d36151ee3c119717305afe4b1f0ba47f0f0154f8ba6f2c5081d6402c8eddd938` | `232` | `d36151ee3c119717305afe4b1f0ba47f0f0154f8ba6f2c5081d6402c8eddd938` | `232` | `yes` |

- `mrc_loader.ext` identical → **CROSS_TARGET** shared bootstrap.

## Live metrics

| run | entry_class | first_pc | fault_pc | fault_addr | wrote_0/4/8/C | pxc_writes | export_call | gamelist |
|---|---|---|---|---|---|---|---|---|
| `gbrwcore_jjfb` | `WRONG_ENTRY_SELECTION` | `0x30CA96` | `0x30CCF8` | `0x28` | `yes/yes/yes/no` | `0` | `no` | `no` |
| `gbrwcore_wxjwq` | `WRONG_ENTRY_SELECTION` | `0x30CA96` | `0x30CCF8` | `0x28` | `yes/yes/yes/no` | `0` | `no` | `no` |

## Cross-target reading

- All compared live runs show `wrote_C=no` and a NULL/+0x28-class fault → **CROSS_TARGET** platform publication gap (not jjfb-only).
