# Repository layout

```text
jjfb_pc_vmrp_bootkit/
├─ README.md
├─ CMakeLists.txt
├─ RUN_BUILD.ps1 / RUN_BUILD_VMRP.ps1 / RUN_TESTS.ps1
├─ RUN_PRODUCT_DIRECT_JJFB.ps1          # product golden chain
├─ RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1    # product smoke
├─ RUN_GAMES.ps1 / RUN_VMRP_VISUAL.ps1
├─ RUN_RESEARCH_GWY_SHELL.ps1           # explicit research entry
├─ include/ src/ tests/ tools/ profiles/ schemas/
├─ docs/          # guides, ADR index, cursor prompts
├─ evidence/      # screenshots + frozen evidence
├─ decisions/
├─ research/
│  ├─ runners/    # RUN_E5..E10A stage scripts (not product)
│  ├─ e10a31r/    # 0xA1B8C provenance artifacts
│  └─ packs/      # analysis packs (e.g. E8B)
├─ packages/      # reference packs + PACKAGE_INDEX
├─ reports/       # stage verdicts
├─ game_files/    # original resources (immutable)
├─ logs/ out/ build-i686/   # local build/run artifacts
├─ third_party/vmrp_upstream/
└─ legacy_lab/    # frozen old bootkit / LIVE / PHASE6 runners
```

## Root policy

根目录只保留：**构建、产品测试、产品验收、显式研究入口**。

- 新阶段 runner → `research/runners/`
- 新截图 → `evidence/screenshots/`
- 新参考包 → `packages/reference/` 或 `research/packs/`
- 历史 LIVE/PHASE6 → `legacy_lab/runners/`

Do not add one-off `RUN_E*.ps1` or analysis packs back to the repo root.
