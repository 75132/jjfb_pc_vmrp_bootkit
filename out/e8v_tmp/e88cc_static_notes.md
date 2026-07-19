# E8V static notes (0x2E88CC)

- Idle success lands at 0x2E88CC (layout/draw scheduler).
- Early exits: D14 gate @ 0x2E88E6; null F6C+4/+8 @ 0x2E8914.
- Draw-ish BLs: 0x2F2854, 0x305BFC, 0x2EA058 (only if object path open).
- Case A product path often only hits 0x2F9970 / 0x305E78 then returns.
