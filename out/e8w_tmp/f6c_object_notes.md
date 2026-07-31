# E8W F6C object layout / writers

## Layout (corrected)

- `R9+0xF6C` is an **embedded struct base**, not a heap object pointer.
- Gate open iff `*(R9+0xF74) != 0` **OR** `*(R9+0xF70) != 0`.
- Early exit `0x2E8914` is BEQ on **F70==0** after F74 was already 0.

## Draw BLs from 0x2E88CC

- `0x2F2854` from: 0x2E8980, 0x2E89A8
- `0x305BFC` from: 0x2E8A22
- `0x2EA058` from: 0x2E8A44

## Writers

| Store PC | Field | Fn | Callers | Relation | Insn |
| --- | --- | --- | --- | --- | --- |
| `0x2D9CFC` | `R9+0xF6C` | `0x2D9CBC` | 10 | none | `STR r0, [r6, #0x0]` |
| `0x2E8920` | `R9+0xF74` | `0x2E88CC` | 1 | none | `STR r0, [r5, #0x8]` |
| `0x2F28DC` | `R9+0xF6C` | `0x2F287C` | 3 | none | `STR r0, [r7, #0x0]` |
| `0x2FAEF0` | `R9+0xF6C` | `0x2FAE98` | 2 | none | `STR r0, [r1, #0x0]` |
| `0x2FBD18` | `R9+0xF70` | `0x2FBB6C` | 1 | none | `STR r0, [r7, #0x4]` |
| `0x2FC200` | `R9+0xF6C` | `0x2FC1E0` | 1 | none | `STR r0, [r1, #0x0]` |
| `0x2FC240` | `R9+0xF6C` | `0x2FC22C` | 1 | none | `STR r1, [r2, #0x0]` |
| `0x2FE2AA` | `R9+0xF74` | `0x2FE17E` | 0 | none | `STR r3, [r7, #0x8]` |
| `0x2FE2B8` | `R9+0xF6C` | `0x2FE17E` | 0 | none | `STR r3, [r5, #0x0]` |
| `0x2FEC8A` | `R9+0xF6C` | `0x2FEBBC` | 14 | ui_2E2520 | `STR r7, [r0, #0x0]` |
| `0x30AA32` | `R9+0xF70` | `0x30A9EC` | 2 | none | `STR r4, [r6, #0x4]` |
| `0x30AA34` | `R9+0xF6C` | `0x30A9EC` | 2 | none | `STR r4, [r6, #0x0]` |
| `0x30AA42` | `R9+0xF74` | `0x30A9EC` | 2 | none | `STR r4, [r6, #0x8]` |
| `0x30B0A6` | `R9+0xF6C` | `0x30AED2` | 0 | none | `STR r7, [r6, #0x0]` |
| `0x31157C` | `R9+0xF70` | `0x311546` | 0 | none | `STR r0, [r4, #0x4]` |
| `0x3117A4` | `R9+0xF6C` | `0x311774` | 1 | none | `STR r0, [r1, #0x0]` |
| `0x311AA6` | `R9+0xF70` | `0x311890` | 5 | none | `STR r4, [r5, #0x4]` |
| `0x311AB8` | `R9+0xF6C` | `0x311890` | 5 | none | `STR r4, [r5, #0x0]` |
| `0x311AC4` | `R9+0xF74` | `0x311890` | 5 | none | `STR r4, [r5, #0x8]` |

