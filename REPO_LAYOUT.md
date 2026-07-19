# Repository layout

`	ext
jjfb_pc_vmrp_bootkit/
├─ README.md / 00_READ_ME_FIRST.md / START_CURSOR_HERE.md
├─ CURSOR_MASTER_PROMPT.md
├─ CMakeLists.txt
├─ RUN_BUILD.ps1 / RUN_BUILD_VMRP.ps1 / RUN_TESTS.ps1 / RUN_GAMES.ps1 / RUN_VMRP_VISUAL.ps1
├─ RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1   # product track entry
├─ RUN_E5_*.ps1 … RUN_E8A_*.ps1        # current stage runners
├─ include/ src/ tests/ tools/ profiles/ schemas/
├─ docs/ evidence/ decisions/ templates/
├─ game_files/                         # original resources (immutable)
├─ logs/                               # latest run logs
├─ reports/
│  ├─ phase_verdicts/                  # D/E/6x stage verdicts (moved from root)
│  └─ stage_e8a_*.md
├─ out/
│  ├─ JJFB_E8A_delivery/ + .zip
│  └─ JJFB_E8B_fast_route_analysis_pack/
├─ third_party/vmrp_upstream/
└─ legacy_lab/
   ├─ runners/                         # RUN_LIVE_* / RUN_PHASE6_* / old D6/E2/NATIVE
   └─ archive/
      ├─ cursor_phase_prompts/
      ├─ root_phase_docs/
      └─ native_fullboot_pack/
`

Historical root clutter (phase markdowns, LIVE/PHASE6 runners, Native pack) lives under `legacy_lab/` or `reports/phase_verdicts/`. Do not add new phase docs or one-off runners to the repo root.
