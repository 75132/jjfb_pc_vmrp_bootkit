# Repository layout after Phase 0 / Task 0.1

```text
jjfb_pc_vmrp_bootkit/
├─ .cursor/                         # rebuild rules + skill
├─ CMakeLists.txt                   # clean product only
├─ README.md
├─ 00_READ_ME_FIRST.md
├─ START_CURSOR_HERE.md
├─ CURSOR_MASTER_PROMPT.md
├─ PACKAGE_INDEX.md
├─ include/gwy_launcher/            # public headers
├─ src/
│  ├─ app/                          # CLI entry
│  ├─ formats/                      # MRP/cfg/reg (to implement)
│  ├─ launcher/                     # descriptor/context
│  ├─ vfs/
│  ├─ runtime/
│  ├─ platform/
│  ├─ profiles/
│  └─ trace/
├─ third_party/vmrp_upstream/       # clean vmrp (~51KB bridge.c)
├─ game_files/                      # original mythroad/gwy resources
├─ profiles/  schemas/
├─ tests/{unit,integration,fixtures,golden}/
├─ tools/                           # mrp_inspect / gwy_cfg_inspect / audit
├─ docs/  evidence/  decisions/  templates/
├─ logs/                            # new-run logs only
└─ legacy_lab/                      # frozen old bootkit (not in build)
```

## Mapping from old root

| 旧路径 | 新位置 |
|---|---|
| `runtime/vmrp_src/vmrp-master` | `third_party/vmrp_upstream`（副本）+ `legacy_lab/runtime/...` |
| `runtime/vmrp_src_build_v27` | `legacy_lab/runtime/vmrp_src_build_v27` |
| `scripts/` `tools/`（旧） | `legacy_lab/scripts` `legacy_lab/tools` |
| `docs/` `README/` `reports/` | `legacy_lab/...` |
| `RUN_*.ps1` `CONFIG.json` | `legacy_lab/runners/` |
| `JJFB_GWY_LAUNCHER_REBUILD_GUIDE_COMPLETE/` | 内容提升到根；原包在 `legacy_lab/...` |
| `game_files/` | **仍在根**（资源根不变） |
