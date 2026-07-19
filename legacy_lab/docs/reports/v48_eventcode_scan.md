# v48 event_code scan (r1 override)

## Method

```text
JJFB_EVENT_CODE = N
override r1 at 0x30662C / 0x2EF86C
JJFB_PROGRESS_DRIVER=off
observe: 2EFC_TAIL, progress_count write, AC8 write, STARTUP_STR, max_pc
```

## Results

| event | max_pc | 2EFC40 | prog_w | ac8_w | str |
|-------|--------|--------|--------|-------|-----|
| 0x00..0x08 | 0x2EFAF4 | 0 | 0 | 0 | 0 |
| 0x10..0x15 | 0x2EFAF4 (or short) | 0 | 0 | 0 | 0 |
| 0x18,0x1A,0x20,0x24,0x28,0x30 | same | 0 | 0 | 0 | 0 |
| natural 0x13 | 0x2EFAF4 | 0 | 0 | 0 | 0 |

**No scanned r1 value alone extends past `0x2EFAF4` or hits `0x2EFC40`.**

## Interpretation

1. Splash depth is gated by **internal state (r4 / objects / flags)**, not solely by `r1` event code in this range.
2. `r1=0x13` is still the natural timer paint code from `0x306305→0x306344→0x30662C`; overriding it does not unlock the r4 path.
3. Event matrix is **negative evidence**: wrong-event-code-only hypothesis is insufficient (may still need *additional* events or different handler args, but r1∈[0,0x30] alone is not the unlock).

## 0x306305 chain

```text
[JJFB_10140_REG] handler=0x306305
[JJFB_HANDLER_306344] event_code_before=0x13
[JJFB_DISPATCH_30662C] ui_mode=0x45 event=0x13
SPLASH_ENTER r0=0x45 r1=0x13
```

Timer feeds a constant paint/tick code **0x13** into splash; splash draws loading UI then exits on **r4==0**.
