# Task 9: Field Length / Cursor Provenance and Contract Repair

- **run_id:** task14_C4_221233
- **verdict:** `FIELD_PARSER_CONTRACT_REPAIRED`
- **FIELD_STREAM_CONTRACT:** 0
- **copy repairs:** 0
- **parser calls:** 2
- **bad r5=0x7374 seen:** no
- **0x2F68E4 return:** yes
- **0x2E4066 entered:** yes
- **0x2DADC4 entered:** yes

## Length decode at 0x30A0CC

```text
LDR  r0, [r4]           ; cursor index
LDRB r1, [r6, r0]       ; lo = stream[cursor]
cursor++
LDRB r2, [r6, r0]       ; hi = stream[cursor]
r5 = (lo << 8) | hi     ; BE u16 field length  @0x30A0E8
CMP  r5, #0             ; @0x30A0EA (not the write)
```

## Framing layout (dynamic)

```text
[BE u32 tag]           ; 0x308D98 — exit when tag == -1
[BE u16 len][bytes][0] ; 0x30A0CC string field
[BE u16 len][bytes][0] ; 0x30A0CC string field
[BE u32][BE u32]       ; two more 0x308D98 words
```

Empty Path-A body (with_rec=0) inner must be **`FF FF FF FF`** (BE -1).
Observed pre-repair: **`00 00 00 00`** (malloc zero-fill; copy never wrote).
Adjacent OOB at inner+4 showed ASCII `"stat"` → BE length `0x7374`.

## GOOD vs BAD

| item | GOOD | BAD |
|---|---:|---:|
| call_id | 1 | n/a |

## Copy contract @0x2E4ECA

| dest | src | n | repaired |
|---|---|---|---|
| 0x6BBAB8 | 0x6AD12B | 0x1F | 0 |

## Classification

```text
FIELD_PARSER_CONTRACT_REPAIRED
```

## Required answers

| Q | A |
|---|---|
| 0x30A0EA length source? | BE u16 at `stream_base + cursor_index` (written at `0x30A0E8`; `0x30A0EA` is `CMP r5,#0`) |
| correct field length? | For empty body: **no field** — tag should be `0xFFFFFFFF`; length N/A. `0x10` is entry+8 capacity, not field length. |
| "stat" meaning? | **OOB adjacent heap bytes**, not a field name |
| heap/cursor wrong why? | Framing BLX→`0x804A8` is not memcpy; inner stays zeros; cursor advances past 4-byte buffer |
| first diverge insn? | Missing write at intended memcpy `@0x2E4ECA`; first wrong read `@0x30A0DA` with cursor=4 |
| 0x10132 wrong? | **No** — size-malloc OK; zero-fill expected before copy |
| repair domain? | **copy** (Scheme C) at Path-A framing |
| helper returned? | yes |
| 0x2E4066 / 0x2DADC4? | yes / yes |
