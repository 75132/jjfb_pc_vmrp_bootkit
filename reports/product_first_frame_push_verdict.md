# Product First-Frame Push Verdict — Event Completion Contract Closure

- **run_id:** ffp_event_20260724_014705_31801
- **mode:** Event (internal phase **EventContract**; not P6c)
- **verdict:** EVENT_COMPLETION_CONTRACT_IDENTIFIED_PATH_A_BLOCKED
- **runtime:** Gwy+stubs
- **Trace305E09:** yes
- **state_advanced:** no

## Proven contract (this knife)

### A. `0x305E09` / `0x305E08` function contract — **IDENTIFIED**

| Item | Value |
|------|--------|
| Entry | Thumb `0x305E08` (live `0x305E09`) |
| Role | Thin **platform free wrapper** |
| ABI | `sendAppEvent(0x10133, r0-4, 0, 0, …)` |
| Case-9 call | `lr=0x30E1A5`, `r0=0x1E209` (event code, **not** heap) |
| Nested plat | `0x10133` / app=`0x1E205` → status success (non-heap no-op) |

Live: `[EVENT_305E09_FUNCTION_CONTRACT_IDENTIFIED]`, `reports/product_event_305e09_entry.csv` (6+ entries).

### B/C. Context role + missing producer

| Classification | Result |
|----------------|--------|
| `EVENT_CONTEXT_NOT_AN_INPUT` | case 9 / `0x305E08` does **not** read/write 10165 |
| `FAMILY_HANDLER_IS_NOTIFICATION_ONLY` | case 9 only frees via `0x10133` |
| `PLATFORM_MUST_PUBLISH_COMPLETION_BEFORE_HANDLER` | Path A via `0x30D2F9→0x30D24C→0x101AB→0x2E4D6C` |
| `EVENT_FIRST_MISSING_OUTPUT_PRODUCER_FOUND` | **`0x101AB` Path-A fill** (implemented) |

Round B falsified “pass 10165 into family handler ⇒ complete”.

### D. Unfinished predicate — **FOUND**

| Field | Value |
|-------|--------|
| PC | `0x30634C` (timer load of switch subject) |
| Object | `ER_RW` |
| Offset | `0x800+0xD0` (UI_MODE / state) |
| Condition | `ui_mode == 0` → state-0 arm → `sendAppEvent(0x1E209, 9)` forever |
| Completed | nonzero (natural writer e.g. `0x2FC418` stores `0x45`) |
| Writer kind | platform Path A (`0x101AB` + enqueue) → guest STR to UI_MODE |

Artifacts: `reports/product_event_unfinished_predicate.csv`, `product_event_ack_contract.json`.

### E. 10165 role

- Owner store confirmed: `0x2B215C` → `0x6AD11C`
- Sibling 10162 (owner-scoped): `0x69EF14` @ `0x2B2158`
- Enqueue handler: `0x30D2F9`
- Not consumed by case 9; expects platform Path-A publish into owner-scoped buffer

### F. Platform fix applied (evidence-backed)

1. **`0x10133`** — documented free/status (`plat_10133_free`)
2. **`0x101AB`** — Path-A buffer fill (`GWY_PLAT_KIND_BUFFER_FILL` + `platform_101ab_fill_path_a`)
3. **Removed latest-10165 fallback** — resolve via owner_store only
4. **ER digest includes UI_MODE** (`0x800+0xD0`) so real state change is observable
5. **Enqueue gated:** do **not** call `0x30D2F9` while `ER_RW+0xB54 == 0` (live fault at `0x312A78` / null list head)

Ungated enqueue previously: `101AB` filled OK → then `UC_FAULT` `LDR [r4,#4]` @ `0x312A60` with `r4=0`.

### G. Forward progress / Resource

- Same unfinished request still reissued (`SAME_UNFINISHED_REQUEST`)
- UI_MODE stays `0` → no `ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION`
- Auto Resource **not** entered (correct: gate not cleared)

**New precise blocker (apply-run stop rule):**

```text
EVENT_PATH_A_BLOCKED_NULL_LIST_HEAD
  ER_RW+0xB54 == 0
  flag15c == 0
  cannot safely run 30D24C→2E4D6C→312A60
```

Next work (not another event ABI guess): recover who initializes B54 / `15C`, then one gated Path-A enqueue → expect UI_MODE advance → auto Resource.

## Artifacts

| Path | Status |
|------|--------|
| `reports/product_event_305e09_entry.csv` | live |
| `reports/product_event_305e09_callers.csv` | static+case9 |
| `reports/product_event_305e09_memory.csv` | stub+live hooks |
| `reports/product_event_305e09_call_tree.csv` | static |
| `reports/product_event_305e09_platform_calls.csv` | static |
| `reports/product_event_context_access_map.csv` | yes |
| `reports/product_event_unfinished_predicate.csv` | yes |
| `reports/product_event_ack_contract.json` | yes |
| `reports/product_event_completion_validate.csv` | yes |
| `out/product_event/305e09_annotated.txt` | yes |
| `out/product_event/305e09_cfg.dot` | yes |

## Runner

```powershell
.\RUN_PRODUCT_FIRST_FRAME_PUSH.ps1 -Mode Event -Seconds 60 -SkipBuild -Trace305E09
# sets JJFB_PRODUCT_EVENT_CONTRACT=1 (EventContract) — no P6c
```

## Discipline

- No P6c, no forced UI_MODE, no fixed object addresses in product logic, no guessed r2/r3 matrix.
- Stopped on precise subsystem blocker after contract + writer identification.
