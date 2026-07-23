# Stage Product P5 Event Advance

- **run_id (validate):** p5_validate_20260723_033710_58895
- **run_id (transaction / Round A):** p5_transaction_20260723_033052_37039
- **mode:** validate (Round B after Round A provenance)
- **verdict:** EVENT_TRANSACTION_LIFETIME_FIXED_NO_ADVANCE_YET
- **runtime:** Gwy+stubs
- **Round B fix applied:** B — one completion per guest request (`JJFB_PRODUCT_P5_ONE_SHOT=1`)

## Summary

P4 restored callable `0x10102` family delivery. P5 proves the next stall is **not** host timer replay inventing completions: the guest itself reissues `sendAppEvent(0x1E209,0x9)` every 10140 tick from `pc=0x304589`. Round B makes completion **one-shot**; duplicate host deliveries stop (`ACCEPT=1`, `DELIVER=1`, `SUPPRESS=66`). Guest ER_RW digest is unchanged after the single delivery, so the state machine still does not advance.

## Round A — transaction provenance

| Step | Evidence |
|------|----------|
| sendAppEvent | `r0=0x1E209 r1=0x9 r2=0 r3=0` caller `0x304589` every tick |
| Host acceptance | `P5_TXN ACCEPT` + `PLATFORM_FAMILY_EVENT ENQUEUE` |
| 10102 delivery | `handler=0x30D301` ABI `r0=app=9 r1=event=0x1E209` `ret=0` |
| State after handler | digest `0x25D6A3D6` → same |
| 10165 | Registered via `PLATFORM_ALLOC` as stub `0x30D2F9`; **never** in delivery chain |
| Next 10140 | Same poll path; guest reissues identical request |

### Classifications (Round A)

- `GUEST_REISSUED_REQUEST` — not `HOST_REPLAYED_COMPLETION_FOUND`
- `FAMILY_EVENT_FALSE_ACK` / incomplete contract — `ret=0`, ER_RW unchanged, `payload=0`
- `SECOND_STAGE_COMPLETION_MISSING` — hypothesis only; 10165 is ALLOC+stub, not proven drain
- Static: `0x30D301` switch on **r0** (bound `0x157`); case 9 at `0x30E1A0` does `bl` then `movs r0,#0`; also loads **stack args** at `sp+32/36`

## Round B — one proven fix

**B. Make completion one-shot per guest request.**

- Marker: `[PROVEN_PLATFORM_CONTRACT_FIXED] contract=event_transaction_one_shot`
- Validate result: **1** accept / **1** deliver / **66** suppress
- `EVENT_TRANSACTION_LIFETIME_FIXED` = yes
- `ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION` = **no**

One-shot is correct lifetime semantics but insufficient alone: the single delivery still does not change Robotol state.

## 10165 role

- Observed registration: `plat_10165_alloc` with `buf/handler=0x30D2F9` (4-insn trampoline before family switch)
- **Not** delivered after 10102 (product does not fabricate 10165)
- Verdict retained: `SECOND_STAGE_COMPLETION_MISSING` as open hypothesis for P6 — do not treat as confirmed queue drain

## Window presentation (separate)

- HWND created with `SDL_WINDOW_HIDDEN` when `JJFB_PRODUCT_P5_MODE` / `JJFB_HWND_UNTIL_DISPUP`
- Reveal only after nonempty guest `_DispUpEx` via `guiProductShowWindowIfReady`
- No DispUpEx this run → window correctly stays hidden (`hwnd_visible=no`)

## Artifacts

| Artifact | Path |
|----------|------|
| Stage report | `reports/stage_product_p5_event_advance.md` |
| Transaction CSV | `reports/product_p5_event_transaction.csv` |
| Family handler CSV | `reports/product_p5_family_handler_state.csv` |
| 10165 provenance | `reports/product_p5_10165_provenance.csv` |
| Post-completion progress | `reports/product_p5_post_completion_progress.csv` |
| stdout / stderr | `logs/product_p5_stdout.txt`, `logs/product_p5_stderr.txt` |
| Runner | `RUN_PRODUCT_P5_EVENT_ADVANCE.ps1` |

## Exact marker gates (validate)

| Gate | OK |
|------|----|
| SCHEDULER_NATURAL_CALLBACK forced=no | yes |
| ROBOTOL_INIT_RETURN_ZERO | yes |
| EVENT_TRANSACTION_ONE_SHOT / LIFETIME_FIXED | yes |
| FAMILY DELIVER (exactly one under one-shot) | yes |
| ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION | no |
| FIRST_NATURAL_REFRESH | no |
| FRAMEBUFFER_NONEMPTY | no |
| HWND_VISIBLE | no (deferred until DispUpEx) |

## Next blocker (P6 candidates — do not combine here)

1. **Family ABI / context object** — case 9 needs non-null payload/context (`r2` / stack args); current `r2=r3=0`
2. **Proven 10165 second-stage** — only if natural enqueue/drain data-flow is observed
3. Not: replaying 1E209, forcing DRAW/`_DispUpEx`, or fabricating 10165

## Strong success status

| Marker | Status |
|--------|--------|
| EVENT_TRANSACTION_ONE_SHOT | **yes** |
| FAMILY_EVENT_ABI_CONFIRMED | no |
| ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION | no |
| FIRST_NATURAL_RESOURCE_REQUEST | no |
| DISPLAY_PREDECESSOR_REACHED | no |
| FIRST_NATURAL_REFRESH | no |
