"""Negative/positive controls for audit_launcher_core.py (Task 0.4)."""
from __future__ import annotations
import subprocess
import sys
from pathlib import Path

# Construct without embedding a full banned literal in audited product sources.
BANNED = "0x2DAD" + "C4"


def run_audit(root: Path) -> int:
    script = root / "tools" / "audit_launcher_core.py"
    return subprocess.call([sys.executable, str(script), str(root)])


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    probe = root / "src" / "launcher" / "_audit_negative_probe.c"
    legacy = root / "legacy_lab" / "_audit_negative_probe.c"
    header_probe = root / "src" / "launcher" / "_audit_header_entry_probe.c"
    try:
        probe.write_text(f'/* probe */\nconst char *p = "{BANNED}";\n', encoding="ascii")
        rc = run_audit(root)
        if rc == 0:
            print("FAIL: core probe should fail audit", file=sys.stderr)
            return 1
        print("[OK] core probe correctly failed audit")

        probe.unlink(missing_ok=True)
        legacy.write_text(f'const char *p = "{BANNED}";\n', encoding="ascii")
        rc = run_audit(root)
        if rc != 0:
            print("FAIL: legacy_lab probe should be ignored", file=sys.stderr)
            return 1
        print("[OK] legacy_lab probe correctly ignored")

        # header_entry_candidate must not be passed to uc_emu_start / runCode.
        header_probe.write_text(
            "/* probe */\n"
            "extern int uc_emu_start(void *uc, unsigned long begin, unsigned long until,"
            " unsigned long timeout, unsigned long count);\n"
            "void bad(void *uc, unsigned long header_entry_candidate) {\n"
            "  uc_emu_start(uc, header_entry_candidate, 0, 0, 0);\n"
            "}\n",
            encoding="ascii",
        )
        rc = run_audit(root)
        if rc == 0:
            print("FAIL: header_entry execution probe should fail audit", file=sys.stderr)
            return 1
        print("[OK] header_entry execution probe correctly failed audit")
        header_probe.unlink(missing_ok=True)

        # Positive: printf of header_entry_candidate is allowed.
        header_probe.write_text(
            "/* probe */\n#include <stdio.h>\n"
            "void ok(unsigned long header_entry_candidate) {\n"
            "  printf(\"header_entry_candidate=0x%lX\\n\", header_entry_candidate);\n"
            "}\n",
            encoding="ascii",
        )
        rc = run_audit(root)
        if rc != 0:
            print("FAIL: printf of header_entry_candidate should pass audit", file=sys.stderr)
            return 1
        print("[OK] header_entry printf probe correctly passed audit")
        return 0
    finally:
        if probe.exists():
            probe.unlink()
        if legacy.exists():
            legacy.unlink()
        if header_probe.exists():
            header_probe.unlink()


if __name__ == "__main__":
    raise SystemExit(main())
