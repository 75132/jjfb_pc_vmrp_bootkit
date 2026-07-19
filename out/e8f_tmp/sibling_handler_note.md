# E8F sibling note (corrected)

## 0x10162 @ 0x30D249 (even 0x30D248)

```
0x30D248: MOVS r0, #0
0x30D24A: BX lr
```

This is a **null stub** registered for the 0x10162 alloc/register family — not an enqueue implementation.
The enqueue core `0x30D24C` immediately follows in the image but is reached via `0x10165` trampoline `0x30D2F8`, not via 10162.

ZERO_ARGS fire of 10162: ok=1, no C44/C9D/CF5/FE8/B7D side effects (OBSERVED).

## 0x10102 @ 0x30D301

Larger PUSH frame; ZERO_ARGS fire ok=1 with no idle-flag side effects (OBSERVED).
Semantics remain HYPOTHESIS (family register / init control).
