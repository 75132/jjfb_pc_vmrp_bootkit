# v48: disasm around 0x2EFC40 — literal pool, not writer

## Critical correction to v47

```text
imm_BA0 @ 0x2EFC58
imm_AC8 @ 0x2EFC6C
```

These are **little-endian literal pool WORDS** (`0x00000BA0`, `0x00000AC8`), sitting **after** an unconditional branch — **not** executable STR sites.

## 0x2EFC40..0x2EFC90

```text
2EFC40  ADD/setup (h=0xAA06, 0x9200, movs r2,#0, movs r1,#0, ...)
2EFC4C  T32 insn
2EFC50  LDR r0,[sp,#..] / SUB
2EFC54  B → 0x2EFCC8          ; skip pool
2EFC56  pad
2EFC58  .word 0x00000BA0      ; LITERAL (BA0 offset)
2EFC5C  .word 0x000007DA
2EFC60  .word 0x00000818
2EFC64  .word 0x0000081C
2EFC68  LDR r2,[pc,#..]       ; more code interleaved / misalign risk in crude decode
2EFC6C  .word 0x00000AC8      ; LITERAL (AC8 offset)
... more pool / LDR islands ...
```

So “reaching 0x2EFC58” would mean executing **data** — only meaningful if PC falls into pool (bug). Real consumers `LDR Rn,[pc,#imm]` targeting these words live **elsewhere** (before pool or after `0x2EFCC8`).

## Epilogue that prevents reaching here

```text
0x2EFAE0..0x2EFB1F:
  ... progress idx end ...
  2EFAF2 CMP r4,#0
  2EFAF4 BEQ <exit>     ; natural max_pc
  2EFAF6+ BLX path      ; only if r4!=0
```

## FORCE into 0x2EFC40

Guest ran `0x2EFC40..0x2EFC54` then branched to `0x2EFCC8` continuum (`max_pc≈0x2EFCFE`) but:

```text
no ERW write to progress_count
no ERW write to AC8
```

**Conclusion:** `0x2EFC40` block is **not** the progress/AC8 store routine (or needs different register state).  
Writer hunt must follow **LDR→ADD r9, #BA0→STR [reg,#0x2C]** patterns after `0x2EFCC8` / other functions, gated by **r4≠0** path from `0x2EFAF2`.
