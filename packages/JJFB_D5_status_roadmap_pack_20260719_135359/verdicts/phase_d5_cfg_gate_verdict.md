# Stage D5 — cfg.bin gate (partial)

- **task:** RESEARCH_ONLY — why guest never opens `gwy/cfg.bin`
- **verdict:** `PARTIAL` — timer/init path falsified; command-dispatch path never entered
- **evidence:** TARGET_OBSERVED

## Discriminating result (45s quiet boot)

| Probe | Hits |
|-------|------|
| `[JJFB_GAMELIST_CFG_GATE]` (cfg-open / gate / wrap) | **0** |
| `[JJFB_GAMELIST_CMD_DISP]` (`off=0xF6C4` char switch) | **0** |
| `cfg.bin` open | **0** |
| `FIRE_EXT_DONE` | **4** |

Instrumentation: module-relative offsets in `ext_gwy_shell_native_exec.c` (same style as `GAMELIST_OFF_CFG36_FMT`).

## Hypotheses after D5

| ID | Claim | Status |
|----|--------|--------|
| A | Timer FIRE reaches cfg-open | **FALSIFIED** (D4 callgraph + D5 PC probe) |
| B | Init path reaches cfg-open | **FALSIFIED** (0 gate hits through init + 4 ticks) |
| C | Command/char dispatcher (`'B'/'H'/'S'/'c'/…` @ cmd switch) must run first | **OPEN** — never entered in Full Boot |
| D | `mr_entry` / table[144] consumption missing | **OPEN** — no `_mr_entry` / `mr_entry` in log |

## Static notes (TARGET_OBSERVED)

- Sole `bl` to cfg-gate `0x2D7C80` is tiny wrap @ off `0xF670`; **no** other `bl`/`b`/literal/`movw` to that wrap found (may be dead or reached only via runtime table).
- Other cfg-open call sites exist (`0x2E11DE`, `0x2D829A`, `0x2DAB90`, …) but also never hit.
- Nearby cmd switch compares bytes to ASCII `B/H/I/P/S/c/s/p` — looks like a protocol/command stream, not keepalive timer.

## Anti-drift

- `audit_launcher_core.py` → ok
- No host_runapp / UI force / jjfb patch

## Next smallest experiment (D5b)

**Question:** What DOCUMENTED/CROSS_TARGET input delivers the command stream (or calls into cmd-disp / cfg-wrap) after gamelist init — network payload, `mr_entry` parse, DSM event, or gbrwcore export — without injecting fake UI state?

**Success:** first `[JJFB_GAMELIST_CMD_DISP]` or `[JJFB_GAMELIST_CFG_GATE]` or `cfg.bin` `[JJFB_RESOURCE_REQUEST]`.
