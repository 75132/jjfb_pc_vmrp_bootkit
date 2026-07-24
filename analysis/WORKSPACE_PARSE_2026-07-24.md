# Workspace Parse — 2026-07-24

Local baseline rebuilt from the full desktop git clone (not the sandbox text-only snapshot).

## Repo identity

See `analysis/repo_rev.txt` and `analysis/core_file_manifest.csv`.

## Confirmed control flow

```text
Path-A push → ER_RW+B54 → 0x305EB8 → 0x2DC80C → 0x312AB4/0x312C0C
  → node consumed → 0x305EC2 post-drain gate
```

Gate needs `15D==1`, `B71!=0`, `134D==0`. On pass: `0x305EF4 → 0x2DADC4 → 0x2FC418 (UI_MODE=0x45)`.

## Product vs research isolation

Default `launcher_core` still compiles JJFB fixed-PC observe modules (`product_p4/p5/ffp/eqb/na/eqc`). `Gwy+stubs` means research lib is not linked; it does **not** mean the product binary has zero JJFB address observation.

## Runner vs checked-in verdict (pre-fix)

Checked-in `reports/product_first_frame_push_verdict.md` contained `Ack path (live)`, post-drain gate prose, and `last_successful_transaction: ... EVENT_NODE_CONSUMED` that the then-current `RUN_PRODUCT_FIRST_FRAME_PUSH.ps1` could not emit. `$farthest` stopped at Path-A / node-linked and under-reported when consumer milestones were already observed.

## Evidence strength

| Target | Role | Grade |
|--------|------|-------|
| Queue drain + node consume | live | proven |
| Post-drain gate sample values | live | proven (15D=0 B71=0) |
| `0x30CBBC` as 15D writer | static + legacy | candidate |
| `0x2E2520` / `0x2DC4D8` B71 | static + legacy | candidate / alternate |

## Next stage

**Post-Drain Gate Contract Closure** — observe-only writers + ER_RW+15D/B71 watchpoints; fix report provenance first. No forced flag/UI patches; do not regress to node construction.
