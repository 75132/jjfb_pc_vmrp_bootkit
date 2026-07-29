# P19 startGame Runtime Contract

## Verdict

**Gates 1–6 not hit in Quick (35s).**  
Infrastructure is correct: `gbrwcore` mapped at `0x2EB7E0`, research breakpoints armed at the reverse-engineered VAs, but **guest never executed `0x306BE0` / `0x306CDA`**. No `br_exit` → no continue into `gamelist`. Therefore no natural `lib.startGame` VM call, no three-arg capture, no opcode 300, no nested JJFB.

This confirms the P16 finding that string-table `API_REGISTER` is insufficient, and adds: **the real API-table builder is not reached on the current init/event-loop path within the gate window.**

## Answers (required)

| Question | Answer |
|---|---|
| startGame actual function pointer | **Not observed.** Research assert for this SHA remains `0x306655` (Thumb); product must not hardcode. |
| Three real arguments | **Not captured** (entry `0x306654` not hit). |
| Opcode 300 dispatcher `[R9+0x1488]` | **Not observed** at BLX `0x306698`. |
| Nested JJFB success | **NO** |
| Parent produces code15 | **NO evidence** (parent never reached startGame/opcode300). |
| First screen vs direct_boot | **N/A** — long holds skipped per gate policy. |

## Armed breakpoints (research only)

```text
gbrwcore base     = 0x2EB7E0
api_builder       = 0x306BE0   (file +0x1B400)  ARMED, NOT HIT
sg_name_store     = 0x306CD4   ARMED, NOT HIT
sg_fn_store       = 0x306CDA   ARMED, NOT HIT
sg_entry          = 0x306654   ARMED, NOT HIT
parser_return     = 0x30667A   ARMED, NOT HIT
opcode300_blx     = 0x306698   ARMED, NOT HIT
```

Observed instead:

```text
first guest PC        = 0x2EB804   (base+0x24)
helper/entry activity ≈ 0x30CA94   (file +0x212B4, MRPGCMAP/helper path)
gbrwcore ER_RW R9     = 0x2B0D18   (seen on R9_SWITCH leave; table not yet populated)
R9 at BOOTSTRAP_ENTRY = 0x0        (CALLEE_ER_RW_NOT_AVAILABLE)
br_exit / continue    = NOT SEEN
```

## Precise blocker

```text
1. gbrwcore maps + P19 BPs arm OK
2. guest enters early PC / helper path (+0x212B4)
3. BOOTSTRAP_ENTRY blocked: no ER_RW → R9=0 at first entry
4. later ER_RW appears (R9=0x2B0D18) but API builder 0x306BE0 still never runs
5. no mr_exit/br_exit → no continue to gamelist logic
6. therefore no natural VM call object into lib.startGame
```

**Not:** “startGame pointer does not exist.”  
**Is:** “current lifecycle never reaches the table-construction site that writes `table+0x78`.”

## Next contract to close (in order)

1. **ER_RW / R9 for gbrwcore BOOTSTRAP_ENTRY** so `[R9+8]` is meaningful during init.
2. **Progress init past helper idle** until natural PC hits `0x306BE0` (no forced jump).
3. **`br_exit` → continue → `gwy/gamelist.mrp`** with UI suppressed, cfg36 auto-select.
4. Capture VM call object + three args at `0x306654` / `0x30667A`.
5. Close `[R9+0x1488]` opcode `0x12C` dispatcher (generic nested MRP start).

## Policy held

- No descriptor-string call into `startGame`
- No product hardcode of `0x306655`
- No synthetic code15 / forced E6C
- cfg36 uses **napptype=12** from live `cfg.bin`
- Long 180/300s matrix **not run** (gates incomplete)

## Artifacts

- `reports/P19_API_TABLE.csv`
- `reports/P19_OPCODE300_TRACE.csv`
- `reports/P19_NESTED_JJFB_MATRIX.csv`
- Runner: `research/runners/RUN_P19_STARTGAME_CONTRACT.ps1`
