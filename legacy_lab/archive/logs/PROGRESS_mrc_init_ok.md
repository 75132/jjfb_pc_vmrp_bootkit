# Progress 2026-07-17 — mrc_init SUCCESS

## Breakthrough
robotol `mrc_init` now returns **0**.

Root cause: `P->mrc_extChunk == NULL` after mythroad 800 load.
`mrc_init` calls through `chunk->+0x28`; with NULL that was `*(0x28)` (low-mem map), returning garbage/1.

## Fix (host)
In `bridge.c` `jjfb_ensure_ext_chunk()` before host code=0:
- synthesize `mrc_extChunk_st` (magic 0x7FD854EB, code_buf, helper, etc.)
- install host hook as `sendAppEvent` at +0x28
- write pointer to `P+0xc`

## Verified log
`logs/v27_host_init_extchunk_stdout.txt` / `v27_host_init_timer_stdout.txt`
- `host mrc_init(0) ret=0`
- sendAppEvent codes: 0x1, 0x10113 x3, 0x10102, 0x10120, 0x10140, 0x10162, 0x10165, 0x10800

## Still open
- No `:20000/:21002/:6009` yet
- Timer code=2 returns 0 but no module/network
- `mr_timer event unexpected!` — DSM timer state not RUNNING
- sendAppEvent stub is heuristic (malloc on large a1); need real `mrc_extMainSendAppMsg` / platEx 0x101xx semantics

## Next
1. Implement real sendAppEvent (see doc 反汇编研究.c signature)
2. Start DSM timer state + continuous code=2 / events
3. Watch module.ext loads + initNetwork
