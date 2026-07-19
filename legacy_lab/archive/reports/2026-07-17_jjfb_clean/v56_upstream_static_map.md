# v56 Upstream Trigger Static Map

- MRP SHA256: `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`
- robotol.ext: compressed `161178`, decompressed `253420`

## Path A — event queue

- `0x2DC80C -> 0x2DC8D4 -> 0x2E2520`
- `0x2E7B7C -> 0x2E7B9E -> 0x2E2520`
- The `0x2E2520` switch reaches `0x2E4066 -> 0x2DADC4` only for event codes **5** and **12**.
- Mythroad constants: 5=`MR_MENU_RETURN`; 12=`MR_MOUSE_MOVE`. Event `0x13` is not this path.

## Path B — startup/reset

- Natural init callsite: `0x2FECA2 -> 0x2DADC4`, inside function `0x2FEBBC`.
- `0x2FEBBC` has 14 direct BL callsites: `2DCC60,2DCCC4,2DCD4E,2DCDEE,2DCF4C,2DD626,2DDA82,2E02E0,2E351A,2E7528,2E77B4,2E799E,2FBED6,30DC44`.
- Family dispatcher `0x30D300` reaches `0x30DC44` only when `app=0xC0`. Existing v55 logs showed only `app=9`.

## Path C — registered callback

- `0x2F5404` has no direct BL caller; its Thumb pointer `0x2F5405` is supplied to `0x3054A4` at `0x2F53AC` and `0x30D128`.
- Callback tail: `0x2F5734 -> 0x305EB8 -> (conditional) 0x305EF4 -> 0x2DADC4`.
- This is the strongest candidate for a platform callback/timer contract not being driven by the current host.

## v56 dynamic decision tree

1. Registration seen, callback entry absent: host scheduler/callback dispatch missing.
2. Family dispatcher seen but no `app=0xC0`: startup family command missing.
3. Event dispatcher seen but no code 5/12: required event source missing.
4. Any path reaches `0x2DADC4` but writer remains absent: blocker moves back to B70/B58/DB0 gates.

## Restrictions

- No `ui_mode=0x45` force.
- No AC8/progress driver.
- v56 disables the old synthetic `mrc_event(0,0,0)` so the coverage run is natural.
