#!/usr/bin/env python3
"""Native GWY/JJFB Full Boot reports: fullboot_00..10 + final verdict."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def grab(pat: str, text: str, flags=0):
    return re.findall(pat, text, flags)


def write(path: Path, body: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body.rstrip() + "\n", encoding="utf-8")


def section(title: str, lines: list[str], empty: str = "(none)") -> str:
    body = "\n".join(f"- `{x}`" for x in lines[:40]) if lines else f"- {empty}"
    return f"## {title}\n\n{body}\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stdout")
    ap.add_argument("report_dir")
    ap.add_argument("--stderr", default="")
    ap.add_argument("--env-dump", default="")
    args = ap.parse_args()
    out = Path(args.stdout).read_text(errors="ignore")
    err = (
        Path(args.stderr).read_text(errors="ignore")
        if args.stderr and Path(args.stderr).is_file()
        else ""
    )
    blob = out + "\n" + err
    rdir = Path(args.report_dir)
    rdir.mkdir(parents=True, exist_ok=True)

    env_text = args.env_dump or "(see runner)"
    write(
        rdir / "fullboot_00_environment.md",
        "# Full Boot 00 — Environment\n\n```text\n" + env_text + "\n```\n",
    )

    pkg_scope = grab(r"\[JJFB_PACKAGE_SCOPE\].*", blob)
    member = grab(r"\[MRP_MEMBER_VIEW\].*", blob)
    reg = grab(r"\[REG_PRIMARY\].*", blob)
    write(
        rdir / "fullboot_01_package_scope_member_view.md",
        "# Full Boot 01 — Package Scope / Member View\n\n"
        + section("PACKAGE_SCOPE", pkg_scope)
        + section("MEMBER_VIEW", member)
        + section("REG_PRIMARY", reg),
    )

    extchunk = grab(r"\[JJFB_EXTCHUNK_(?:ALLOC|PUBLISH)\].*", blob)
    er_rw = grab(r"\[JJFB_ER_RW_BIND\].*", blob)
    r9 = grab(r"\[JJFB_R9_SWITCH_OK\].*", blob)
    init_ok = grab(r"\[JJFB_SHELL_CORE_MODULE\].*stage=init_ok.*", blob)
    write(
        rdir / "fullboot_02_shell_module_context.md",
        "# Full Boot 02 — Shell Module Context\n\n"
        + f"- gamelist EXTCHUNK: {len([x for x in extchunk if 'gamelist' in x])}\n"
        + f"- gamelist ER_RW: {len([x for x in er_rw if 'gamelist' in x])}\n"
        + f"- gamelist R9_OK: {len([x for x in r9 if 'gamelist' in x])}\n"
        + f"- init_ok lines: {len(init_ok)}\n\n"
        + section("EXTCHUNK", extchunk)
        + section("ER_RW_BIND", er_rw)
        + section("R9_SWITCH_OK", r9)
        + section("SHELL_CORE init_ok", init_ok),
    )

    cfg36 = grab(r"\[JJFB_GAMELIST_CFG36_BUILD\].*", blob)
    post = grab(r"\[JJFB_GAMELIST_POST_UPDATE\].*", blob)
    upd = grab(r"\[JJFB_GWY_UPDATE_STUB\].*", blob)
    write(
        rdir / "fullboot_03_gamelist_cfg36_no_update.md",
        "# Full Boot 03 — Gamelist cfg36 / no_update\n\n"
        + section("CFG36_BUILD", cfg36)
        + section("POST_UPDATE", post)
        + section("UPDATE_STUB", upd),
    )

    export_r = grab(r"\[JJFB_SHELL_EXPORT_RESOLVE\].*", blob)
    export_c = grab(r"\[JJFB_SHELL_EXPORT_CALL\].*", blob)
    runapp = grab(r"\[JJFB_RUNAPP\].*", blob)
    write(
        rdir / "fullboot_04_native_export_runapp.md",
        "# Full Boot 04 — Native Export / Runapp\n\n"
        + section("EXPORT_RESOLVE", export_r)
        + section("EXPORT_CALL", export_c)
        + section("RUNAPP", runapp),
    )

    jjfb_open = grab(r"\[JJFB_FILEOPEN\].*jjfb\.mrp.*", blob)
    strcom = grab(r"\[JJFB_STRCOM\].*", blob)
    mrc_loader = grab(r"\[JJFB_MRC_LOADER\].*", blob)
    mrc_init = grab(r"\[JJFB_MRC_INIT\].*", blob)
    robotol = grab(r".*robotol\.ext.*", blob)
    write(
        rdir / "fullboot_05_jjfb_mrc_loader_robotol.md",
        "# Full Boot 05 — JJFB mrc_loader / robotol\n\n"
        + section("FILEOPEN jjfb", jjfb_open)
        + section("STRCOM", strcom)
        + section("MRC_LOADER", mrc_loader)
        + section("MRC_INIT", mrc_init)
        + section("robotol mentions", robotol[:20]),
    )

    wx = grab(r".*wxjwq\.mrp.*|.*mmochat\.ext.*", blob)
    write(
        rdir / "fullboot_06_wxjwq_control.md",
        "# Full Boot 06 — WXJWQ Control\n\n" + section("wxjwq / mmochat", wx),
    )

    slots = grab(r"\[JJFB_EXTCHUNK_SLOT_CALL\].*", blob)
    write(
        rdir / "fullboot_07_slot_calls_if_any.md",
        "# Full Boot 07 — Slot Calls\n\n"
        + f"- SLOT_CALL count: {len(slots)}\n"
        + ("- action: observe-only; do not expand matrix without real calls\n\n" if not slots else "\n")
        + section("SLOT_CALL", slots),
    )

    fopen = grab(r"\[JJFB_FILEOPEN\].*", blob)
    res = grab(r"\[JJFB_RESOURCE_REQUEST\].*", blob)
    write(
        rdir / "fullboot_08_fileopen_resource_chain.md",
        "# Full Boot 08 — Fileopen / Resource Chain\n\n"
        + section("FILEOPEN", fopen)
        + section("RESOURCE_REQUEST", res),
    )

    draw = grab(r"\[JJFB_DRAW\].*", blob)
    refresh = grab(r"\[JJFB_REFRESH\].*", blob)
    write(
        rdir / "fullboot_09_visual_natural_chain.md",
        "# Full Boot 09 — Visual Natural Chain\n\n"
        + section("DRAW", draw)
        + section("REFRESH", refresh),
    )

    oom = bool(re.search(r"br_mem_get failed=no_memory|my_malloc no memory", blob))
    heap_free = grab(r"\[JJFB_DSM_HEAP\].*", blob)
    gl_init = bool(re.search(r"module=gamelist\.ext stage=init_ok", blob))
    gl_ctx = bool(
        re.search(r"\[JJFB_EXTCHUNK_PUBLISH\].*gamelist\.ext", blob)
        and re.search(r"\[JJFB_ER_RW_BIND\].*gamelist\.ext", blob)
        and re.search(r"\[JJFB_R9_SWITCH_OK\].*gamelist\.ext", blob)
    )
    native_runapp = bool(
        re.search(r"\[JJFB_RUNAPP\].*source=native_shell.*jjfb\.mrp", blob)
    )
    jjfb_file = bool(re.search(r"\[JJFB_FILEOPEN\].*jjfb\.mrp.*ok=1", blob))
    mrc_ok = bool(mrc_init)
    visual = bool(draw or refresh)
    host_equiv = bool(re.search(r"host_runapp_equivalent_after_no_update", blob))

    if visual and (mrc_ok or jjfb_file):
        verdict, klass, level = "HIGH_SUCCESS", "NATURAL_VISUAL", "advanced"
    elif mrc_ok and mrc_loader:
        verdict, klass, level = "MID_SUCCESS", "MRC_INIT_REACHED", "mid"
    elif native_runapp and jjfb_file:
        verdict, klass, level = "MINIMUM_SUCCESS", "NATIVE_SHELL_OPENS_JJFB", "minimum"
    elif gl_ctx or gl_init:
        verdict, klass, level = "PARTIAL", "GAMELIST_PLATFORM_CONTEXT", "shell_context"
    elif pkg_scope and not oom:
        verdict, klass, level = "PARTIAL", "PACKAGE_SCOPE_ACTIVE_NO_CONTEXT", "stage_a_partial"
    elif oom:
        verdict, klass, level = "FAIL", "DSM_HEAP_OOM_ON_CONTINUE", "blocked"
    else:
        verdict, klass, level = "FAIL", "SHELL_CHAIN_INCOMPLETE", "blocked"

    if host_equiv and "native_shell" not in "\n".join(runapp):
        verdict, klass = "FAIL", "HOST_RUNAPP_EQUIVALENT_FORBIDDEN"

    blockers = []
    if oom:
        blockers.append("DSM heap OOM on continue (4MB mem_get)")
    if not pkg_scope:
        blockers.append("no [JJFB_PACKAGE_SCOPE]")
    if not gl_init:
        blockers.append("gamelist.ext init_ok missing")
    if not gl_ctx:
        blockers.append("gamelist EXTCHUNK/ER_RW/R9 incomplete")
    if not cfg36:
        blockers.append("no CFG36_BUILD")
    if not native_runapp:
        blockers.append("no RUNAPP source=native_shell")
    if not jjfb_file:
        blockers.append("jjfb.mrp not opened")
    if not mrc_ok:
        blockers.append("mrc_init not reached")

    write(
        rdir / "fullboot_10_final_verdict.md",
        "# Full Boot 10 — Final Verdict\n\n"
        + f"- **verdict:** `{verdict}`\n"
        + f"- **class:** `{klass}`\n"
        + f"- **level:** `{level}`\n"
        + f"- package_scope: `{bool(pkg_scope)}`\n"
        + f"- gamelist init_ok: `{gl_init}`\n"
        + f"- gamelist platform context: `{gl_ctx}`\n"
        + f"- cfg36: `{bool(cfg36)}` post_update: `{bool(post)}`\n"
        + f"- native_shell runapp: `{native_runapp}` jjfb open: `{jjfb_file}`\n"
        + f"- mrc_init: `{mrc_ok}` visual: `{visual}`\n"
        + f"- host_runapp_equivalent used: `{host_equiv}`\n"
        + f"- OOM: `{oom}` heap_free events: `{len(heap_free)}`\n\n"
        + "## Blockers\n\n"
        + ("\n".join(f"- {b}" for b in blockers) if blockers else "- (none)\n")
        + "\n\n## DSM heap\n\n"
        + section("JJFB_DSM_HEAP", heap_free),
    )

    # Convenience copies at repo root style
    write(
        rdir / "CONCLUSION.md",
        f"# CONCLUSION — Native GWY/JJFB Full Boot\n\n"
        f"**Verdict:** {verdict} (`{klass}`)\n\n"
        f"| Gate | Result |\n|---|---|\n"
        f"| Package scope | {'PASS' if pkg_scope else 'FAIL'} |\n"
        f"| Gamelist platform context | {'PASS' if gl_ctx else 'FAIL'} |\n"
        f"| cfg36 / no_update | {'PASS' if cfg36 or post else 'observe'} |\n"
        f"| native_shell runapp + jjfb open | {'PASS' if native_runapp and jjfb_file else 'FAIL'} |\n"
        f"| mrc_init | {'PASS' if mrc_ok else 'FAIL'} |\n"
        f"| natural visual | {'PASS' if visual else 'FAIL'} |\n",
    )

    print(f"[fullboot_reports] verdict={verdict} class={klass} level={level}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
