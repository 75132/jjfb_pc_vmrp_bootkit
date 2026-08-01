# P22-CLEAN param→cfg branch chain

## Static call chain (module offsets, raw MRPG base)

```
callers of +0x10740  (e.g. +0x4076, +0x12D0E, …)
        │
        ▼
 +0x10740  UI/init
   ldrb r0, [R9+#0x3E4]     ; once-flag
   cmp  r0, #1
   beq  +0x107AA            ; early return — skips FF00 path
   …
   cmp  r0, #0xF            ; mode field [R9+#0x6C4]
   beq  +0x10814
        │
        ▼
 +0x10814  bl +0xFF00
        │
        ▼
 +0xFF00   bl +0x7DB0
   adds r0, #5
   cmp  r0, #5
   bhs  early_exit          ; if 7DB0 returned >= 0
   … switch cases …
   +0xFF3A / +0xFFB2  bl +0x7B6C   ; CFG LOADER ENTRY
        │
        ▼
 +0x7B6C   cfg loader → plat 0x10112
```

Also: `+0xD964` wrap (`bl +0xD768; movs r0,#0; bl +0x7B6C; bl +0xFF00`) — not reached this run.

## Dynamic (Lane A, base=0x2D4364)

| Site | Hits | Note |
|------|------|------|
| +0x133E0 / +0x1344A | 3 | Real site behind abs PC 0x2E77AE — `ldrh [r4]` 0x10204 tag check, **not** napptype parser |
| +0x77AE (hist label) | 0 | Bytes OK at offset; never executed this run |
| +0x10740 | **0** | **First missing scheduler layer** |
| +0x10814 / +0xFF00 / +0x7B6C | 0 | Blocked upstream |

## First blocking “branch”

Not a taken conditional inside a reached function — the cfg-loader path’s owning function **is never entered**:

- **block_off:** `+0x10740`
- **actual:** `CALLER_NOT_REACHED`
- **loader path:** `+0x10740 → +0x10814 → +0xFF00 → +0x7B6C`
- **natural producer to chase next:** callers of `+0x10740` (static BL sites include `+0x4076`, `+0x12D0E`, …) — typically event/UI dispatch

If `+0x10740` later becomes reached, the next candidate gates inside it are:

1. `+0x1074E` `beq` when `[R9+0x3E4]==1`
2. `+0x107F8` `beq +0x10814` only when mode field `== 0xF`
