# v57 Lifecycle Source Static Map

- MRP SHA-256: `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`
- robotol.ext: compressed `161178`, decompressed `253420`

## 1. Registration contracts

- `0x10102` registers family handler pointer `0x30d301` → code `0x30D300`.
- `0x10140` registers tick/state handler pointer `0x30630d` → code `0x30630C`.
- **Correction:** `0x303E14` is not the `0x10140` handler. It is a lifecycle-command dispatcher reached through robotol EXT method 1.

## 2. Who can produce family app=0xC0?

- `0x30D300` has no direct `BL` caller; it is entered through the handler registered by `0x10102`.
- Guest `sendAppEvent(0x1E209, app, ...)` producers found in robotol use low app values: `0x2`, `0x3`, `0x9`, `0xA`, `0x1D`, `0x1F`, `0x22`, `0x23`, `0x24`, `0x25`, `0x26`, `0x27`.
- No direct guest producer of `app=0xC0` was found in this static set.
- Therefore the `app=0xC0 → 0x30DC44 → 0x2FEBBC` path is best classified as **platform-originated family lifecycle ingress**, not an ordinary guest `0x1E209` emission. This is an inference from the registered-callback architecture and producer scan.

## 3. Why 0x3054A4 never registers 0x2F5405

- Callback constructor: `0x2F5390 → 0x2F53AC(load 0x2F5405) → 0x3054A4`.
- Producer A: robotol EXT **method 1** dispatches to `0x304B30`, reads a 3-word payload, then calls `0x303E14(cmd,arg1,arg2)`. Only `cmd=10002 (0x2712)` reaches `0x304418 → 0x2F5390`.
- Producer B: robotol EXT **method 5** dispatches to `0x304B5A → 0x3053B8 → 0x2F5390` directly.
- If neither EXT method 1 with command 10002 nor EXT method 5 occurs, `0x3054A4` cannot register `0x2F5405` through these natural producers.

## 4. Separation from the periodic handler

- Host currently calls the registered `0x10140` handler at `0x30630D` with `r0=0,r1=0` as a periodic tick.
- That periodic handler is separate from robotol EXT method 1/5 and does not by itself prove delivery of lifecycle command 10002.

## 5. Static assertions

| Assertion | Pass | Value |
|---|---:|---|
| 10102 family handler | yes | `0x30d301` |
| 10140 tick handler | yes | `0x30630d` |
| EXT method 1 target | yes | `0x304b30` |
| EXT method 5 target | yes | `0x304b5a` |

## 6. v57 restrictions

- No family `app=0xC0` injection.
- No robotol EXT method 1/5 injection.
- No command 10002 injection.
- No `ui_mode=0x45` FORCE, AC8 driver, progress driver, or host-rendered UI.
