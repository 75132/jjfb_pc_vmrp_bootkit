# ORIGINAL GWY Bootstrap Rehost (P16–P18)

## Verdict

**NO first-screen gain vs direct_boot in the Quick matrix.**  
Headless parent bootstrap **did run** and captured real `API_REGISTER` string VAs for `lib.startGame` / `lib.runflashmrp` / `lib.runapp`, but did **not** obtain a runtime `function_pointer`, did **not** nest-start `gwy/jjfb.mrp`, and produced **no natural code15**.

## Baseline

- P15 already frozen on clean tree (`reports/P15_BUILD_IDENTITY.txt`, commit `f112b6d`)
- `JJFB_304BF0_RESUME_MODE=direct_lr` kept as product-safe baseline
- No code15 synthesis, no E6C force-write, no forged next screen

## Catalog

Required resources present under `game_files/mythroad/240x320`:

| Package | Role | Present |
|---|---|---|
| gwy.mrp | parent_platform | yes |
| gwy/gbrwcore.mrp | shared_core | yes |
| gwy/gbrwshell.mrp | mrp_shell | yes |
| gwy/font.mrp | font | yes |
| gwy/jjfb.mrp | game | yes |
| gwy/jjfbol/ | game_assets | yes |
| gwy/cfg.bin | cfg36 | yes |

cfg36 descriptor (proven, napptype=12):

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

## Modes exercised

| Cell | Mode | Launch | Result |
|---|---|---|---|
| A | `direct_boot` | `gwy/jjfb.mrp` | robotol entered; Quick hold too short for full 5-BMP census |
| B | `original_headless` | `gwy/gbrwcore.mrp` | gbrwcore guest PC `0x2EB804`; shell gate open; gbrwshell warmup only |
| C | `startgame_only` | `gwy/gbrwcore.mrp` | same observation window as B in Quick |

## API map (runtime, not hardcoded entry)

From B cell:

| api_name | function_pointer | string_va | kind |
|---|---|---|---|
| lib.startGame | 0x0 | 0x30DBB4 | string_va_not_entry |
| lib.runflashmrp | 0x0 | 0x30DAC4 | string_va_not_entry |
| lib.runapp | 0x0 | 0x30DCA0 | string_va_not_entry |
| lib.checkmrpver | 0x0 | 0x30DC24 | string_va_not_entry |
| lib.getmrpver | 0x0 | 0x0 | string_va_not_entry |

**Rule held:** no static address call to `startGame`. Entry PC remains unknown until a real registry bind / lookup exposes a code pointer.

## Runtime stack

```text
frame0: parent_gbrwcore (gwy/gbrwcore.mrp / gbrwcore.ext)
frame1: (not created — nested jjfb not observed)
```

## 0x101AB answers

1. **Does original parent provide code5?** Not observed in this hold window. Product path still uses `SYNTHETIC_CODE5_COMPAT` on direct_boot.
2. **Does code5 naturally become code15?** No natural code15 from parent in B/C.
3. **Is code15 local parent or network?** Still unknown — parent did not produce a 0x101AB game frame sequence here.
4. **What should replace synthetic code5?** Still the missing original producer after a real `startGame → runflashmrp/runapp → jjfb` nest. Not inventable.

## Precise blocker (not “launcher cannot run”)

Observed stop point:

```text
gbrwcore.mrp mr_start
→ gbrwcore.ext guest PC hit (0x2EB804)
→ string-table API_REGISTER (startGame/runflashmrp/runapp)
→ HOLD: no mr_exit→gbrwshell continue in window
→ no lib.startGame entry_pc
→ no nested gwy/jjfb.mrp
→ no parent 0x101AB code15
```

Likely missing next contracts:

1. **Continue gate / idle exit** — gbrwcore stayed in guest loop; `try_continue_after_mr_exit` toward `gwy/gbrwshell.mrp` did not fire within Quick hold.
2. **startGame ABI producer** — without gamelist cfg-select (intentionally skipped for headless UI), nothing called the registered `lib.startGame` service with the cfg36 descriptor as a live guest call.
3. **Entry pointer bind** — only string VAs were published; function table write at init (~0x1B400 region) was not yet captured as `function_pointer`.

Local first screen before remote: **still not proven**. Parent shared-core load alone is insufficient; need either:

- headless gamelist logic path that builds cfg36 and looks up `lib.startGame`, or  
- observed function-table bind + ABI-closed host call into the registered entry while keeping parent VM alive.

## Artifacts

- `reports/ORIGINAL_GWY_API_MAP.csv`
- `reports/ORIGINAL_GWY_AB_MATRIX.csv`
- `reports/ORIGINAL_GWY_RUNTIME_STACK.json`
- Runner: `research/runners/RUN_P16_ORIGINAL_GWY_HEADLESS.ps1`

## Policy

- `direct_boot` remains the safe JJFB baseline
- Do not hardcode `startGame` entry addresses
- Do not synthesize code15 / force E6C / forge next UI
