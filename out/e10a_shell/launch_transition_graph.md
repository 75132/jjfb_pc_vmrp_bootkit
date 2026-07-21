# E10A GWY Launch Transition Graph

Parsed from `game_files/mythroad/320x480/gwy`.

## Observed chain (TARGET_OBSERVED + static inventory)

```
GWY entry (gwy.mrp / launcher)
  -> gbrwcore.mrp / gbrwcore.ext
  -> gamelist.mrp / gamelist.ext
  -> cfg.bin record select (index 36 observed for jjfb)
  -> update / no-update / post_update branch
  -> lib.startGame / lib.runapp
  -> target MRP (e.g. gwy/jjfb.mrp)
  -> robotol.ext splash @ 0x2EF86C
  -> AC8 gate @ 0x2EF8AE (logo vs loading-only)
```

## cfg index 36 (if present)

| field | value |
|-------|-------|
| target | `gwy/jjfb.mrp` |
| napptype | 12 |
| nextid | 482 |
| ncode | 512 |
| narg | 0 |
| narg1 | 1 |
| title_suffix | 风暴(火爆公测) |

## Side-pack / downimage

- `showN!WxH@downimageN.bmp` members live in `gwy/jjfbol/downimageN.mrp`
- `@downimage` string referenced from robotol.ext (not main jjfb.mrp members)

## Inventory summary

- shell packs scanned: 12
- cfg records with target: 60
- string hits: 233

Evidence: static parse only; does not prove runtime order without shell_trace.
