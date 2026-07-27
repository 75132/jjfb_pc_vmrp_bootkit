# E10A shell file API ABI

## Verdict (current)

Host rejects `mr_open(NULL)` (no more `"\b"` garbage).  
Product still **not** past cfg36/runapp/jjfb splash.

## Fixes landed

1. **`br_mr_open`**: reject `filename==0` before `getMrpMemPtr`.
2. **`br_mem_get` reuse**: no longer `memset` the 4MB DSM heap (preserves in-heap EXT images such as gbrwcore `@0x2EB7FC`).
3. **Nested `runCode` / helper**: save/restore full UC regs around `bridge_ext_helper_call` / `bridge_mr_extHelper`; also restore PC/LR if timer poll nested a helper that left PC at the shared stop address.

## Evidence chain

### Code **-99**

Built at `gbrwcore` `0x2F6188` when a check fails; posted into P-dispatch as `[r6]=0xFFFFFF9D`.  
Dispatch calls `0x30CC60` which ignores non-6 and returns 1 — **not** an `mr_open` path.

### Stale LR `0x30D043` (fixed)

Before reg restore, after nested helper:

```text
srand → rand → mem_get → mem_free …  all with LR=0x30D043
```

Same sticky LR then hit `open` with `R0=0`.  
Cause: nested helper `BL` left `LR=0x30D043` while `pop {pc}` returned to stop; outer guest kept that LR.  
After fix: **no more `BRIDGE_STALE_LR` with `0x30D043`**.

### Remaining NULL open

```text
[JJFB_E10A_OPEN_NULL] lr=0xA2548 r1=0x280258
```

- `0xA2548` is inside DSM/cfunction CODE image (not gbrwcore dispatch).
- Guest PC ring last saw gbrwcore epilogue `pop {pc}` @ `0x30D078`, then open from unwatched CODE.
- Follow-on `drawBitmap` also uses `LR=0xA2548` with garbage args.

So E10A-2 still needs: why DSM/CODE @ `~0xA2544` invokes `open` with `R0=0` (and whether sticky-LR walk remains for CODE-space callers).

## Live map reminder

`gbrwcore.ext` base = **`0x2EB7FC`** (not `0x300000`).

## Not yet claimed

- `SHELL_FILE_API_ABI_FIXED`
- `SHELL_CALLBACK_RETURN_ABI_FIXED` (partial: nested helper LR clobber addressed)
- cfg36 record select / runapp / jjfb splash
