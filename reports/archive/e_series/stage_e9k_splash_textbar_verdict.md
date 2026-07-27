# Stage E9K: Post-r4 Textbar / 0x305BFC Path

**Verdict:** `SPLASH_POST_R4_BLOCKED_BY_R4_FIELD` — **NOT product success.**

Reached post-r4 (`0x2EFB0E`) with deferred hold; blocked before `0x305BFC` / textbar blit by missing font/text context (`R9+0x818` / `R9+0x81C` == 0).

| Field | Value |
|-------|-------|
| Mode | `postr4` |
| Evidence | OBSERVED |
| Loadingbar + 4× progress | YES (natural coords; HWND nonblank=21124) |
| `0x2EFAFA` r4 gate | PASSED (`r4=0x314338`, `B6C=0`) |
| Validate `blx [R9+0x143C]` @ `0x2EFB08` | OK (`fptr=0xAC2D0`, ret=1) |
| Post-r4 `0x2EFB0E` | YES |
| Text measure `0x305E78` | ENTERED (`lr=0x2EFB49`) |
| `0x2F2174` / `0x305BFC` / textbar blit | NO |
| BD0 assist kept | YES (`0x314338` / count=4) |
| Rewrite / bridge | NO |

## Artifacts

- Log: `logs/e9k_splash_textbar_stdout.txt`
- Post-r4 CSV: `reports/e9k_post_r4_trace.csv`
- Draw CSV: `reports/e9k_draw_sequence.csv`
- HWND: `screenshots/e9k_actual_window_capture.png`
- SDL: `screenshots/e9k_textbar_splash_ui.png`

## Path observed

```text
progress loop (count=4)
  → 0x2EFAF2 B6C load (actual B6C=0; prior false positive was &B6C)
  → 0x2EFAFA cmp r4 ≠ 0  PASS
  → 0x2EFB08 blx strlen/validate  OK
  → 0x2EFB0E post-r4 enter
  → 0x2FD868 / 0x305E78 text measure
  → BLOCK: R9+0x818=0, R9+0x81C=0 → r1=0 at measure
  → (never reaches 0x2F2174 layout / 0x305BFC text draw)
```

## `0x305BFC` ABI (static; not executed this run)

From caller `0x2EFBA2` and function prologue:

| Arg | Role |
|-----|------|
| R0 | status C-string |
| R1 | x (layout) |
| R2 | y (layout) |
| R3 / stack | color / flags |
| Uses | `R9+0x818` / `R9+0x81C` via earlier `0x305E78` measure |

Do **not** call `0x305BFC` directly until font ctx is natural.

## Hold policy (E9K)

- `JJFB_E9K_HOLD_AFTER_POST_R4=1` — no early blit_n=5 stop
- Hold armed at text-measure miss via `jjfb_e9k_request_hold` → `guiVisibleWindowFinalize`
- BD0 assist retained (E9L will naturalize `0x2FC418/444`)

## Next gap (not E9L yet)

1. Find natural writers for **`R9+0x818` / `R9+0x81C`** (font / text graphics context; flag watch already shows `0x81C` stays 0).
2. Re-enter `0x305E78` → `0x2F2174` → `0x305BFC` for status text UI.
3. Textbar bmp may be width-only (`BA0+0x24`); bitmap draw is optional success if it appears.

## Class notes

| Class | Meaning |
|-------|---------|
| `SPLASH_POST_R4_REACHED_NEXT_GAP` | Entered `0x2EFB0E` |
| `SPLASH_POST_R4_BLOCKED_BY_R4_FIELD` | Missing field (here: `R9+0x81C`) before text draw |
| `SPLASH_305BFC_REACHED_NEXT_GAP` | Text API hit (not this run) |
| `SPLASH_TEXTBAR_DRAWN_NO_REWRITE` | textbar bmp via real blit (not this run) |
