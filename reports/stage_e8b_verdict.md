# Stage E8B — Handler Returned, Find Real Blocker

## Verdict

**NO_DRAW_NO_NEW_PLAT**

Handler lifecycle returns `ok=1` every tick; guest repeatedly calls unsupported plat `0x1E209/0x9` with host `ret=0` (unchanged). No natural `JJFB_DRAW`. Census shows the same plat signature set repeating (refresh stubs via `0x10113` only). VFS alias unused (local `320x480` already has jjfbol/gb16).

## Gates

| Gate | Pass |
|------|------|
| No Thumb/ABI edits | yes |
| Census present | yes — unique codes + counts; dump at tick 1 and 25 |
| 0x1E209 observe | yes — regs + ret=0; no ret mutation |
| Caller disasm | yes — `out/e8b_tmp/caller_30666A_disasm.txt` |
| VFS alias | armed (`JJFB_VFS_RES_ALIAS=1`); no hit this run; file_miss=0 |
| Long run | yes — ≥25 lifecycle ticks / ~50s wall |
| wxjwq control | clean — `PACKAGE_SCOPE … primary=mmochat.ext`, not CONTAMINATED; incidental `DRAW_SEEN` |
| audit_launcher_core | ok (findings=[]) |
| jjfb.mrp hash | unchanged `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036` |

## 0x1E209 slot note (observe-only)

- Live: every lifecycle tick `GUEST_INDIRECT_CALL pc=0x30666A target=0x304559 arg0=0x1E209 arg1=0x9` then host ret=0.
- Thumb at `0x30666A`: `BL 0x304559` then **unconditional** `B 0x3066AD` — return value is **not** branch-tested at this site.
- Slot-alone label: `IDLE_NO_EVENT` (do **not** invent ret=1). Evidence class: TARGET_OBSERVED for the call; CROSS_TARGET not claimed for semantics.
- Trace `caller_pc` is LR at stub entry (`0x304589`); guest BL site remains `0x30666A`.

## Artifacts

- `logs/stage_e8b_jjfb_stdout.txt` / `reports/stage_e8b_jjfb_verdict.md`
- `logs/stage_e8b_wxjwq_stdout.txt` / `reports/stage_e8b_wxjwq_verdict.md`
- `out/e8b_tmp/caller_30666A_disasm.txt`
- Runner: `RUN_E8B_POST_HANDLER_BLOCKER.ps1`

## Next stage (not E8B)

Do not mutate `0x1E209` ret without a ret-sensitive branch proof. Next discriminating work is whichever real event/input/network/platform path can leave the wait loop — not graphics invention.
