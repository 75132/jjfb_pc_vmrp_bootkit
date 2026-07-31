# Stage E10A-3.1f method0 Fail-site Verdict

- **Mode**: `branch_trace`
- **run_id**: `1784665648370`
- **Primary verdict**: `MRC_INIT_TRUE_FAILURE_PC_FOUND`
- **True fail PC**: `0xAC2E8` (dsm:`cfunction.ext`)
- **Old “fail” PC**: `0x2E1C24` — **false positive** (sentinel store)

## Headline

E10A-3.1d’s `RETURN_NEG1_IMMEDIATE @ 0x2E1C24` was **not** the method0 failure gate.
It is an unconditional `MVNS r0,r4` / `STRH` that materializes `-1` as a stored halfword while execution continues (~4540 more guest instructions).

After ignoring that sentinel, the **first real r0→-1 edge** is:

| Field | Value |
|-------|-------|
| failure_pc | **`0xAC2E8`** |
| class | `RETURN_NEG1_IMMEDIATE` |
| failing_callee / entry | `0xAC2D0` (`dsm:cfunction.ext`) |
| caller | gamelist `BLX` @ `0x2E53A6` (lr=`0x2E53A9`) |
| R9 at fail | `0x280400` (cfunction ERW — **not** gamelist `0x682B6C`) |
| insn to fail | **5055** (was ~518 when stopped at sentinel) |
| platform APIs (e10a31d) | 0 |
| method0 input | still `0x0` / `input_len=2011` |

## Static proof for `0x2E1C24`

From `out/e10a31f/gamelist_2e1bbd_fail_annotated.txt`:

- `0x2E1BC0`: `MOVS r4, #0`
- Many `STR r4,[R9+off]` zero-inits
- **No Bcond targets `0x2E1C24`** (`branches_to_0x2E1C24_count: 0`)
- `0x2E1C24`: unconditional `MVNS r0, r4` → r0=-1
- `0x2E1C26`: `STRH r0, [r1]` stores the sentinel
- Function continues (BL `0x2E33AD` … POP `0x2E1C72`)
- Offsets used: `0x3E4`, `0x120`, `0x6B4`, **`0x95C`**, … — **not** `0x91C` / `0x920`

This matches E10A-3.1e `METHOD0_DOES_NOT_READ_APPINFO_BEFORE_FAILURE`.

## Milestones

- `FAILSITE_2E1C24_IS_NEG1_SENTINEL_STORE`
- `FAILSITE_NO_BRANCH_PREDICATE_INTO_2E1C24`
- `E10A31D_FIRST_FAILURE_FALSE_POSITIVE`
- `FAILSITE_2E1C24_IGNORED_AS_SENTINEL`
- `FAILSITE_EXECUTION_CONTINUES_PAST_2E1C24`
- `MRC_INIT_TRUE_FAILURE_PC_FOUND` (`pc=0xAC2E8`)

## Call path (refined)

```text
helper 0x2E3089
  → 0x2DB045
  → 0x2E2529
  → 0x2E1BBD   (sentinel MVNS @ 0x2E1C24 — NOT a return)
  → … continues …
  → gamelist 0x2E53A6 BLX 0xAC2D0  (cfunction.ext)
  → TRUE FAIL @ 0xAC2E8   (r0 becomes -1)
```

## Implications

1. **appInfo / ver=1006** was never on the causal path to the old “fail” — confirmed again.
2. Next work should target **`cfunction.ext` @ `0xAC2D0`–`0xAC2E8`**, with R9=`0x280400`, not gamelist `0x2E1C24`.
3. method0 `input=0` vs mythroad `filebuf` remains an open ABI question, but is secondary to understanding why `cfunction` returns -1 here.
4. Do **not** start cfg yet.

## Artifacts

- `out/e10a31f/gamelist_2e1bbd_fail_annotated.txt`
- `reports/e10a31f_fail_branch_trace.csv`
- `reports/e10a31f_failsite_dense_trace.csv`
- `reports/e10a31d_method0_return_provenance.csv`
- `reports/e10a31d_method0_call_tree.csv`
- `reports/e10a31d_method0_instruction_trace.csv`
- `logs/e10a31f_method0_failsite_stdout.txt`
- `RUN_E10A31F_METHOD0_FAILSITE.ps1`
