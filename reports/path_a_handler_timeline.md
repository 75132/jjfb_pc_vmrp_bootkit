# Path-A Handler Timeline (Task 5)

**Run A:** `pah_A_20260724_234631`  
**Contract:** `JJFB_PATH_A_EVENT_CONTRACT=1`  
**Trace:** `JJFB_PATH_A_HANDLER_TRACE=1`

## Milestones

| t (approx) | Event |
|---|---|
| T0 | `runtime_spawned` |
| +30s | `guest_entry_called` / `waiting_for_first_frame` |
| +38s | `PATH_A_CONTRACT_ARM` reason=`FIRST_PATH_A_FILL` module_id=3 gen=3 |
| +39s | Path-A publish / `event_path_a_seen` |
| +42s | `0x2E2520` Call1 `word0=5` `word8=4` → `path_a_valid_dispatch` |
| +42s | `path_a_handler_entered` `0x2E4040` |
| +42s… | Inside `0x2F68E4` + nested Path-A (`push_312A60`, `event_node_linked` code=5) |
| hold end | Handler **not** returned; `2E4066` / `2DADC4` **not** seen |

## Handler enter snapshot

```text
PC=0x2E4040 LR=0x2DC8D9
R0=0x2 (switch index)  R4=0x2A8374 (event entry)
entry+0=0x5  +4=0x2A8364  +8=0x4  +C=0x12
ER_RW=0x2B1854
gate: 15C=1 15D=0 B71=0 134D=0 C76=0 UI=0
```

## Call tree (observed)

```text
0x2E4040
  stores ER_RW+B5C / +B60 / +AC8
  BL 0x2F68E4
    BL 0x308D98   (BE u32 read)
    BL 0x30A0CC
      BL 0x2D99AC (malloc)
        BL 0x304558 plat 0x10138  → slot28 → …
        … repeated 0x10138 …
      nested Path-A framing 0x2E4EFx → 0x312A60 push (code 5 nodes linked)
  (not reached) 0x2E4066 BL 0x2DADC4
  (not reached) B 0x2E4194
```

## Gate samples (read-only)

| stage | 15C | 15D | B71 | 134D | C76 | UI |
|---|---|---|---|---|---|---|
| before_2E4040 | 1 | 0 | 0 | 0 | 0 | 0 |
| handler_enter | 1 | 0 | 0 | 0 | 0 | 0 |
| (no clean handler_return in this run) | | | | | | |

## Side effects while handler active

- ER_RW queue/lifecycle cursor fields (`B5C`, `B60`, `AC8`) written
- Nested Path-A events published (`event_node_linked`, code=5)
- Platform `0x10138` invoked many times (natural; ret observed 0 via slot28)
- Transient UC fault at `0x312A78` (`r4=0`) then continued linking later nodes
- No DispUpEx / first-frame / B71 / 15D writers

## Artifacts

- `out/pah_task5/A/stdout.txt`
- `out/pah_task5/A/runtime_progress.jsonl`
- `out/pah_task5/{A,B,C}/meta.txt`
