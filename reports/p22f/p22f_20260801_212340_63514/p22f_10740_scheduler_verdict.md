# P22F-CLEAN +0x10740 scheduler provenance verdict

## Bottom line

**Class: G** (runner fallback 鈥?native verdict missing)

process killed before p22f_finalize; see logs

## Freeze checks

| Check | Result |
|------|--------|
| gbrwcore | PASS |
| br_exit CONTINUE | PASS |
| gamelist | PASS |
| ERW isolated | PASS |
| FIRE_EXT | PASS (16) |
| forced 10140 | PASS |
| 0x30D5D2 | PASS |
| headless OFF | PASS |

## Log extract

```

gl_base=0x0 hit_10740=0 nearest=finalize_missing
```

See out/p22f/p22f_20260801_212340_63514/ and reports/p22f/p22f_20260801_212340_63514/.
