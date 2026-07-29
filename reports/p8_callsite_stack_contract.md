# P8 callsite stack contract

## Status

P8 A/B landed. Product default remains `JJFB_304BF0_RESUME_MODE=direct_lr`.

| mode | Layer-1 | resources | LOOKUP_CONTINUATION | DISPATCH_EPILOGUE | CALLER_CONTINUATION | CALLER_SP_DELTA=0 | 6th resource |
|------|---------|-----------|---------------------|-------------------|---------------------|-------------------|--------------|
| `direct_lr` | PASS (SHA `c789a129…bffda`) | 5 | 0 | 0 | 5 | **5/5 YES** | NO |
| `callsite` | FAIL (`no_drawfp`, `bmp_missing`) | 2 | 2 | 0 | 1 | n/a (broke early) | NO |

## Verdict class

**Situation C** — `callsite` resume to `0x304BF4` is unsafe on this binary.

## Why `0x304BF4` is wrong

Live ABI at every `LOOKUP_CALLSITE`:

```text
PC = 0x304BF0
LR = 0x2D93D1   (builder — not 0x304BF4)
```

Prologue dump (`p8_prologue_dump` / details hex):

```text
FFB50E1C0021AFB0F04C2D9129917C44
```

Thumb decode (LE halfwords):

```text
0x304BF0  B5FF   PUSH {r0-r7, lr}
0x304BF2  1C0E   ...
0x304BF4  2100   MOVS r1, #0     ← callsite resume lands here
0x304BF6  B0AF   SUB SP, #Imm    (large frame)
```

So `0x304BF0` is a **function entry with PUSH**, not an inner BL callsite whose link register is `0x304BF4`. Host jump to `+4` skips the PUSH → stack contract breaks → white screen after ~2 resources.

`DISPATCH_ENTER` (`0x30450C`) never fired; live BL from the builder targets `0x304BF0` directly.

## `direct_lr` baseline (product-safe)

- Restore SP/R4–R11/R9; `PC = LR|1` (`0x2D93D1`).
- Five BMPs complete; first frame stable.
- Per-call SP invariant (LOOKUP SP vs CALLER_CONTINUATION SP): **delta 0 for all five** — see `reports/p8_direct_lr/`.
- Successor Gate still open (no 6th natural resource).

## `callsite` failure detail

See `reports/p8_callsite/p8_sp_invariant.csv`:

1. `loadingbar` — CONTINUATION hit (`R0=0`).
2. Re-enter / dispatcher-band `LR=0x304609` noise.
3. `bar` — CONTINUATION then stall; no DrawFP / screenshot.

## Zero-image enqueue

`note_pixels(_ex)` only warns: `[NOTE_PIXELS_LEGACY] deprecated_no_enqueue` — no `calloc` / Pending enqueue.

## Env

```text
JJFB_304BF0_RESUME_MODE=direct_lr|callsite   (default direct_lr)
```

Address roles in `platform_mrp_resource.h`:

```c
PLATFORM_MRP_DISPATCH_ENTRY_PC       0x30450C
PLATFORM_MRP_LOOKUP_CALLSITE_PC      0x304BF0  /* live: callee entry */
PLATFORM_MRP_LOOKUP_CONTINUATION_PC  0x304BF4  /* unsafe resume target */
PLATFORM_MRP_DISPATCH_EPILOGUE_PC    0x3047E8
PLATFORM_MRP_CALLER_CONTINUATION_PC  0x2D93D2
```

## Next (do not touch family ABI yet)

1. Find real epilogue of the `0x304BF0` function (matching `POP` after the PUSH frame) → research resume-to-epilogue A/B.
2. If stack can close without white screen but still 5 resources → Situation B: A/B `R0=0` vs `R0=handle_guest`.
3. Only then revisit `0x30D301` case 9 loads.
