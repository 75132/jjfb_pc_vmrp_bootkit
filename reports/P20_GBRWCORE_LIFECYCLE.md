# P20 gbrwcore Module Lifecycle

## Gates

| Gate | Hit |
|---|---|
| 1 command=0 | YES |
| 2 0x10102 register | YES |
| 3 callback enter | YES |
| 4 lazy init | YES |
| 5 API builder | YES |
| 6 startGame ptr | YES (0x2AAD84) |
| 7 startGame entry | NO |
| 8 opcode 300 | NO |
| 9 nested jjfb | NO |

## Notes

- image_base (pad-refined): 0x2EB7FC
- timer FIRE observed: YES
- shell continue after init_ok: YES (`via=timer_fire_ext_init_ok` → `gwy/gamelist.mrp`)
- Policy: no forced PC/R9, no host callback write, no forged events, no code15/E6C
- cfg36 napptype=12 (live cfg.bin)
- Shortest path: command=0 → 0x10102/0x11100/callback → natural event → lazy init → API builder → startGame

## Closed this phase

1. Product `0x10102` accepts non-robotol MRP EXT owners (`ACCEPTED_MODULE`).
2. Family/event ABI for gbrwcore: R0=event_code.
3. After EXT `FIRE_EXT`, dequeue/deliver scheduled `PLATFORM_TIMER` handler (registered `0x10102` callback) — Gate 3–6.
4. After API table publishes `lib.startGame`, shell continue into gamelist without waiting for `br_exit`.

## Remaining blocker (Gate 7+)

After `init_ok` continue, gamelist starts but guest faults before natural `startGame` entry:

- `UC_MEM_READ_UNMAPPED` at `fault_pc=0x30D5D2` (gbrwcore) `fault_addr=0xD09C6C91`
- `JJFB_HELPER_RETARGET` / `GAMELIST_P_COLLISION` — gamelist reuses gbrwcore `P=0x2AC8DC`
- Live startGame fn `0x2AAD84` armed (`sg_entry_live`) but not entered

Cell: `out/p20_lifecycle/gate_20260730_104832`
