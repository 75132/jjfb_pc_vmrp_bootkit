# P11 Final Verdict — Case-9 Runtime Contract + Late P3 Provenance

## Bottom line

1. **Case-9 runtime contract recovered** for `0x1E209 / app=9` under locked product ABI (`direct_lr`, `apply_abi=0`, `EVENT_CONTRACT=0`).
2. **`0x1E205` provenance closed**: `0x1E209 - 4`, used as plat `0x10133` arg — not a mapped fetch pointer.
3. **Late `P3_FAULT @ 0x2D960E` NOT_SEEN** → Situation **C**; pass without sixth resource / stack forge.
4. **G0 identity incomplete**: evidence build was on a **dirty** tree (P11 sources uncommitted). Clean commit + rebuild still required before claiming SHA-aligned golden identity.

## Gate matrix

| Gate | Result | Evidence |
|------|--------|----------|
| G0 clean commit / EXE identity | **PARTIAL** | `p11_clean_build_identity.txt` — `source_tree_clean=False` |
| ABI locks held | **PASS** | apply_abi=0, EVENT_CONTRACT=0, direct_lr |
| 10102 / family register | **PASS** | handler `0x30D311`, family `0x1E200`, owner `robotol.ext` |
| Runtime dumps + disasm | **PASS** | `p11_runtime_*` + SHA256 list |
| Case-9 dynamic slice | **PASS** | 65 rows; enter→leave |
| Case-9 natural return | **PASS** | `CASE9_LEAVE ok=1` (×2 in run) |
| Stack Case A (no host 5th/6th) | **PASS** | stack0/1=0; still returns |
| `0x1E205` provenance | **PASS** | slice `0x305E2E` + plat call |
| Late P3 fault ring | **PASS (absent)** | Situation C; empty ring |
| Five BMP / Layer-1 SHA | **NOT_SEEN** | out of Case-9 window; not blocking contract |

## Required answers

| # | Question | Answer |
|---|----------|--------|
| 1 | Registered handler? | `0x30D311` (was not `0x30D301`) |
| 2 | Deliver ABI? | R0=app, R1=event; R2=R3=0; stack0=stack1=0 |
| 3 | Handler shape? | Direct Thumb app-id switch in `robotol.ext` |
| 4 | Does Case-9 need host stack args? | **No** (Case A) |
| 5 | What is `0x1E205`? | Event key `event-4` for `0x10133` |
| 6 | Late fault in this path? | **No** |
| 7 | Sixth resource this round? | **No** |
| 8 | Reopen EXIT_PARK / +0x7B6C? | **No** |

## Artifact index

| Path | Role |
|------|------|
| `reports/p11_clean_build_identity.txt` | build identity |
| `reports/p11_10102_registration_trace.csv` | register row |
| `reports/p11_10102_registration_contract.md` | register contract |
| `reports/p11_case9_dynamic_slice.csv` | dynamic slice |
| `reports/p11_case9_runtime_contract.md` | runtime contract |
| `reports/p11_runtime_case9_*.bin` / `*_disasm.txt` | handler window |
| `reports/p11_runtime_fault_*.bin` / `*_disasm.txt` | historical fault window |
| `reports/p11_runtime_dump_sha256.txt` | dump hashes |
| `reports/p11_late_fault_ring.csv` | empty ring |
| `reports/p11_late_fault_context.md` | Situation C writeup |
| `logs/p11_natural_case9_stdout.txt` | natural run |
| `research/runners/p11_run_natural_case9.ps1` | runner |
| `research/runners/p11_disasm_runtime_case9.py` | disasm helper |

## Freeze / next

- Keep EXIT_PARK frozen; no CFG forge; no epilogue / resource-handle returns.
- Optional next (outside P11 contract close): clean commit P11 sources → rebuild → re-assert G0; separately pursue Layer-1 / five-BMP if product needs that gate.
