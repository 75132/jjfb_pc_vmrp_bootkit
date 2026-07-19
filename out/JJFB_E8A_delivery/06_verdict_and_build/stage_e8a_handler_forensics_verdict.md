# Stage E8A — Handler Forensics First

## Verdict

**THUMB_STATE_LOST** (pre-fix root cause)

After the single minimal fix in `guest_memory_uc_run_entry_ex`, lifecycle advances to:

**HANDLER_RETURNED_NEXT_PLATFORM_GAP**

Evidence level: **OBSERVED** (isolated Unicorn 1.0.2 probe + full jjfb lifecycle forensic ring)

---

## Baseline (F0)

| Item | Value |
|------|-------|
| commit | `6b515f5c2e6101eb3447f3c2a950f01246167bf0` |
| jjfb.mrp SHA-256 | `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036` |
| wxjwq.mrp SHA-256 | `6ec628419bc4c0ca1f8fba37b0c5179961220cd53591fc55eba26735defbd02d` |
| unicorn | project `unicorn-1.0.2-win32` |
| jjfb hash after run | unchanged |

## Member compare (F1)

| Member | jjfb | wxjwq | Match |
|--------|------|-------|-------|
| start.mr | `c8d664aa…` | `c8d664aa…` | yes |
| mrc_loader.ext | `d36151ee…` | `d36151ee…` | yes |
| reg.ext | different | different | no (expected) |
| robotol.ext | 253420 B | — | |
| mmochat.ext | — | 320292 B | |

## Handler map (F2) — live bytes proof

```
robotol code_base = 0x2D8DF4   (EXT_MAP raw_base_refine)
handler VA        = 0x30630C   file_off = 0x2D518
fault VA          = 0x306338   file_off = 0x2D544
file@0x2D544      = 63D1F8F72BFCEF48  == E7 FAULT_BYTES
```

Assumption `0x300000 + file_off` is **wrong**. Mapping is `guest_va - code_base`.

## Dual disasm (F3)

- Thumb @ 0x30630C: `LDR` + `PUSH {r4-r7,lr}` — legal
- Thumb @ 0x306338: `BNE` (`0xD163`) — legal
- ARM @ 0x306338: word `0xF7F8D163` — invalid → matches `UC_ERR_INSN_INVALID` if T=0

## Unicorn probe (F6) — discriminating experiment

| Start | CPSR.T preset | First insn size | First T | Result @ 0x306338 |
|-------|---------------|-----------------|---------|-------------------|
| even `0x30630C` | 1 | 4 | **0** | INSN_INVALID |
| odd `0x30630D` | 1 or 0 | 2 | **1** | executes past fault (T=1) |

**Unicorn 1.0.2 selects Thumb from `uc_emu_start` address LSB, not from CPSR.T alone.**

Old code cleared LSB before `uc_emu_start` → always ARM → fault at legal Thumb bytes.

## Full run after one-line fix

```
ENTRY T=1 R9=0x2B1858
FIRE_DONE ok=1 end=stop_at_base uc_err=0
fault-time T=1 r9_unchanged=1
ticks 1..8 all ok — no UC_ERR_INSN_INVALID
```

No DRAW/REFRESH yet → next gap is platform/API after successful callback return.

## What was changed

1. Observe-only: `handler_forensic.c` + `JJFB_HANDLER_FORENSIC=1`
2. **One root-cause fix:** keep Thumb LSB in `guest_memory_uc_run_entry_ex` → `uc_emu_start`
3. Tools: `mrp_inspect` extract, `handler_dual_disasm.py`, `unicorn_thumb_probe.c`, `RUN_E8A_HANDLER_FORENSICS.ps1`

## Explicitly NOT done

- No LR/SP/R0–R3 ABI change
- No force UI / skip fault / MRP or EXT mutation
- No Unicorn upgrade (not needed — not a translation gap)

## Next task

Stage E8B — observe which platform slots the now-returning handler actually touches (draw/refresh/timer/net); fill only those.

## wxjwq control note

`RUN_E8A` with `-Target gwy/wxjwq.mrp` extracted mmochat and installed its member view, but the product runner still launched the jjfb/robotol lifecycle path in this session (cfg36/APPID overwrite). Treat the wxjwq log as **contaminated**; jjfb post-fix evidence stands alone. A clean wxjwq control needs a dedicated runner param/APPID path (next chore).
