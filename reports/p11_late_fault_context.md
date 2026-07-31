# P11 — Late P3 Fault Context (`0x2D960E` / addr=`0x1E205`)

## Verdict

**Late P3 Fault NOT_SEEN** on natural Case-9 with `product_ffp_apply_abi=0` and `JJFB_PRODUCT_EVENT_CONTRACT=0` (run `p11_20260801_043903_31840`, ~70s, Case-9 delivered twice, both `ok=1`).

Classification: **Situation C** — Case-9 returns; historical `P3_FAULT @ 0x2D960E, addr=0x1E205` is **not** Case-9-internal under the locked ABI. Do not enable speculative stack/R2/R3 or EVENT_CONTRACT to force the fault this round.

## Ring

`reports/p11_late_fault_ring.csv` — header only (zero fault rows).

Stdout grep for `P3_FAULT` / `2D960E`: **0** hits.

## `0x1E205` vs fault address

Inside Case-9, `0x1E205` is **event_code−4** passed to plat `0x10133`, not a load base. Treating it as a pointer (historical fault addr) is a **misclassification** under apply_abi=0.

## Runtime window at historical PC

Dump: `p11_runtime_fault_2d95c0_2d9660.bin` (taken at Case-9 enter).

| Addr | Observation |
|------|-------------|
| `0x2D960E` | `00 00` — not a live LDR fault site in this image window |
| `0x2D9610` | nearby `PUSH` / small function — layout ≠ “execute LDR from 0x1E205” |

Fault-site dump may be **unreached / dead** on this path, or only active under older ABI/EVENT_CONTRACT configurations. P11 forbids re-enabling those to chase it.

## Decision

| Option | Result |
|--------|--------|
| A — fault inside Case-9 | **Rejected** (Case-9 leaves clean) |
| B — need sixth resource / stack forge | **Rejected** (path closes without) |
| **C — post-Case-9 / other config** | **Accepted** for historical fault; closed for P11 Case-9 contract |
