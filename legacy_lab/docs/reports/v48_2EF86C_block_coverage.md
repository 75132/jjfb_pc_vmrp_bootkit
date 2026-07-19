# v48: 0x2EF86C basic-block coverage

## Verdict

```text
Natural path max_pc = 0x2EFAF4
0x2EFC40..0x2EFC80 NEVER reached (2EFC40_hits=0)
Gap is NOT “missing writer at literal pool” — function returns early after progress loop.
```

## Coverage (natural, event=0x13)

| Metric | Value |
|--------|-------|
| hit halfwords | 129 / 586 |
| max_pc | **0x2EFAF4** |
| 2EFC40 hits | **0** |
| last interesting | progress loop `0x2EFA9E..0x2EFAEC` |

### Blocks that DO run

```text
0x2EF86C  entry (r0=0x45 r1=0x13)
0x2EF87A  load imm BA0 → BA0 base 0x2B23F0
0x2EF8A0  AC8 read path (via later)
0x2EF9EC..0x2EFA54  loadingbar/bar/textbar bind
0x2EFA9E..0x2EFAF4  progress loop then EXIT
```

### Blocks that do NOT run

```text
0x2EFB00+   (BLX / further work after r4 check)
0x2EFC40..  (never)
```

## Early-exit branch (the real skip)

Epilogue bytes @ `0x2EFAE0`:

```text
2EFAE0: CMP r6,#0x0C / loop end ...
2EFAF0: BNE ...
2EFAF2: CMP r4, #0
2EFAF4: BEQ  <—— taken when r4==0 → leave splash; max_pc stops here
2EFAF6+ only if r4!=0: LDR/BLX path toward deeper tail
```

**Gate:** after progress animation loop, **`r4` must be non-zero** to continue toward `0x2EFB00` / later code.  
With current timer/event path, **r4 stays 0** → never approach `0x2EFC40`.

## FORCE_2EFC_TAIL probe

```text
divert 0x2EFAE8 → 0x2EFC40
2EFC_TAIL hits=10, max_pc reaches ~0x2EFCFE
progress_count writes = 0
AC8 writes = 0
STARTUP_STR = 0
```

Executing `0x2EFC40` by force does **not** write progress/AC8.  
That region starts with setup then `B → 0x2EFCC8` over a **literal pool** — not the STR writer.

## Next coverage target

1. Why is **r4==0** at `0x2EFAF2`?
2. Who normally sets r4 before that CMP?
3. Real STR to `BA0+0x2C` / `AC8` — search users of pool words after `0x2EFCC8`, not the pool addresses themselves.
