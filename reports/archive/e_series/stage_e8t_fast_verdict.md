# Stage E8T-Fast Verdict

**Verdict:** `POST_C44_STILL_BLOCKED_BY_C9D`

**Static co-claims:**
- `C9D_NONZERO_WRITER_NEVER_REACHED` — robotol.ext has **no** `STRB #1 @ R9+0xC9D`
- `UI_INIT_REJECTS_STATE38_NEXT_GAP` — UI-init `0x2E4788` BEQ-rejects `{38,46,69,252,300}`
- `C9D_REQUIRES_UI_INIT` — natural C44 unlock cluster lives under UI-init; product still never enters it

**NOT product success.** Lane D `JJFB_FAST_C9D_UNLOCK_CALL` **not enabled** (no real nonzero C9D writer fn).

## Static (Lane A / C)

| Item | Finding |
| --- | --- |
| Gate | Idle path LDRSB `R9+0xC9D`, need `==1` @ `0x3066BC` |
| Exact STRB @ C9D | `0x30AA46` write_0; `0x3115BA` write_0 (`r7=#0`) |
| Misclassified | `0x2E3A68` / `0x2FB008` write **C9C=#1**, not C9D |
| Literal `0xC9D` | Only gate **read** @ `0x3066BC` |
| UI-init reject states | `{38,46,69,252,300}` |
| UI-init ED8 | `CMP #0; BGT early-out` → continue only if **ED8==0** (E8S “ED8!=0” was wrong) |
| UI-init other | `C8E==0`, `CA3==1` |
| UI-init ABI | Caller `0x2E2F50` passes `r0=r4` (object). Entry does `LDR [r5,#4]` with `r5=r0` |

## Dynamic quick (2 cases + ED8 fix re-run)

| Case | Verdict | Elapsed | C44 | C9D | UI entry | UI `0x2E4840` | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A_unlock_c9d_watch | POST_C44_STILL_BLOCKED_BY_C9D | 62.2s | 1 | 0 | no | no | C44 unlock only; C9D idle |
| B_unlock_ui_st20 (ED8=1, bad) | false accept | ~39s | 1 | 0 | yes | no | ED8=1 forced early-out |
| B_unlock_ui_st20 (ED8=0, CA3=1, state=20) | UI_INIT_BRANCH_UNMET | 45.9s | 1 | 0 | yes | no | **UC_MEM_READ_UNMAPPED @ 0x4** — null object |

## Interpretation

1. **C44 is open on the fast line; C9D is still closed.**
2. There is **no local MOVS#1→C9D writer** to FAST-call. Next C9D work is memcpy/other-module/computed-base, not Lane D assist.
3. UI-init **state=20 is plausible** (not in reject set), but a real **object pointer in r0** is required before state/C9D effects can be tested. `r0=0` faults at `[0+4]`.
4. Product natural DRAW still needs: leave state=38 before UI-init **or** enter UI-init from `0x2E2520` with a live object while ED8==0 / CA3==1 / C8E==0.

## Artifacts

- `reports/stage_e8t_speed_summary.jsonl`
- `logs/stage_e8t_*_stdout.txt`
- `out/e8t_tmp/e8t_c9d_xref.md`
- `tools/e8t_c9d_writer_xref.py`
- `RUN_E8T_FAST.ps1`
