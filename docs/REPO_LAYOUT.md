# Repository layout

```text
jjfb_pc_vmrp_bootkit/
├─ README.md
├─ CMakeLists.txt
├─ RUN_BUILD.ps1 / RUN_BUILD_VMRP.ps1 / RUN_TESTS.ps1
├─ RUN_PRODUCT_DIRECT_JJFB.ps1 / RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1
├─ RUN_GAMES.ps1 / RUN_VMRP_VISUAL.ps1 / RUN_JJFB_LAUNCHER.ps1
├─ RUN_RESEARCH_GWY_SHELL.ps1
├─ include/ src/ tests/ tools/ profiles/ schemas/
├─ docs/                 # guides + cursor entry
├─ reports/              # ACTIVE stage notes only (~20 files)
│  └─ archive/           # historical phase / E-series / fullboot
├─ evidence/             # frozen evidence (screenshots ignored by Cursor)
├─ decisions/
├─ research/runners/     # stage / task runners (not product root)
├─ packages/
│  ├─ reference/
│  └─ archive/           # zip packs
├─ game_files/           # original resources (immutable)
├─ out/ logs/ build-i686/  # local artifacts (gitignored)
├─ third_party/vmrp_upstream/
└─ legacy_lab/           # frozen old bootkit — do not browse for product work
```

## Root policy

根目录只保留：**构建、产品测试、产品验收、显式研究入口**。

| Put new work here | Not here |
|-------------------|----------|
| `research/runners/` | root `RUN_TASK*.ps1` |
| `reports/` (active only) | `reports/archive/` for old verdicts |
| `packages/archive/` | root zip dumps |
| `evidence/` | `out/` run trees |

## What to ignore when reading

1. `legacy_lab/` — frozen history  
2. `reports/archive/` — old phase/E/fullboot notes  
3. `out/` `logs/` `build-i686/` — regenerable  
4. `packages/archive/` — reference zips  

Start from [`docs/00_READ_ME_FIRST.md`](00_READ_ME_FIRST.md) and [`reports/README.md`](../reports/README.md).
