#!/usr/bin/env python3
"""Generate noop stubs for research_gwy_shell APIs used by product builds."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_HEADERS = [
    "e10a31a_precont_diag.h",
    "e10a31b_publication.h",
    "e10a31c_dispatch.h",
    "e10a31d_provenance.h",
    "e10a31e_appinfo.h",
    "e10a31f_failsite.h",
    "e10a31g_strcmp.h",
    "e10a31h_smscfg.h",
    "e10a31j_smscfg_long.h",
    "e10a31l_config_map.h",
    "e10a31m_fail_2e5404.h",
    "e10a31n_post_range.h",
    "e10a31_gamelist_context.h",
    "e10a3_postselect_trace.h",
    "e10a_shell_trace.h",
    "ext_gwy_shell_native_exec.h",
    "ext_gwy_shell_shim.h",
    "jjfb_bmp_meta.h",
    "jjfb_plat_11f00.h",
    "robotol_flag_writer_trace.h",
    "robotol_idle_watch.h",
    "sms_cfg_compat_profile.h",
    "gwy_sms_cfg.h",
]


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*?$", "", text, flags=re.M)
    return text


def extract_funcs(text: str):
    text = strip_comments(text)
    text = re.sub(r"typedef\s+[^;{]+;", "", text)
    text = re.sub(
        r"typedef\s+(?:struct|enum|union)\s+\w*\s*\{.*?\}\s*\w+\s*;",
        "",
        text,
        flags=re.S,
    )
    text = re.sub(r"(?:struct|enum|union)\s+\w+\s*\{.*?\}\s*;", "", text, flags=re.S)
    funcs = []
    for m in re.finditer(
        r"(?m)^(?!\s*#)\s*((?:const\s+)?[\w\s\*]+?)\s+(\w+)\s*\((.*?)\)\s*;",
        text,
        flags=re.S,
    ):
        ret = re.sub(r"\s+", " ", m.group(1)).strip()
        name = m.group(2)
        args = re.sub(r"\s+", " ", m.group(3)).strip()
        if not ret or name in {"if", "for", "while", "switch", "return"}:
            continue
        if "typedef" in ret:
            continue
        funcs.append((ret, name, args))
    return funcs


def unused_guard(args: str) -> str:
    if not args or args == "void":
        return ""
    names = []
    for part in args.split(","):
        part = part.strip()
        if not part or part == "void":
            continue
        tok = part.split()[-1]
        # regs[16] -> regs; *ptr -> ptr
        tok = tok.lstrip("*")
        tok = re.sub(r"\[.*", "", tok)
        if tok and tok not in {"*", "..."} and not tok.isdigit():
            names.append(tok)
    if not names:
        return ""
    return "\n    (void)" + "; (void)".join(names) + ";"


def stub_body(ret: str, name: str, args: str) -> str:
    arglist = args if args and args != "void" else "void"
    # Avoid fragile unused-arg parsing for array params; silence via attribute.
    if ret == "void":
        return f"void {name}({arglist}) {{}}\n"
    if "*" in ret:
        return f"{ret} {name}({arglist}) {{ return 0; }}\n"
    if ret in {"int", "int32_t", "uint32_t", "uint64_t", "uint16_t", "size_t"}:
        return f"{ret} {name}({arglist}) {{ return 0; }}\n"
    return f"{ret} {name}({arglist}) {{ return ({ret})0; }}\n"


def main() -> None:
    incs = "\n".join(f'#include "gwy_launcher/{h}"' for h in INCLUDE_HEADERS)
    chunks = [
        "/* Auto-generated research-track stubs for product builds.",
        " * Real implementations live in research_gwy_shell.",
        " * Regenerate: python tools/gen_research_stubs.py",
        " */",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "#include <string.h>",
        incs,
        "",
    ]
    seen: set[str] = set()
    for h in INCLUDE_HEADERS:
        text = (ROOT / "include" / "gwy_launcher" / h).read_text(
            encoding="utf-8", errors="ignore"
        )
        for ret, name, args in extract_funcs(text):
            if name in seen:
                continue
            seen.add(name)
            chunks.append(stub_body(ret, name, args))

    manuals = {
        "e10a31a_continue_decision_name": """
const char *e10a31a_continue_decision_name(GwyContinueDecision d) {
    (void)d;
    return "stub";
}
""",
        "e10a31c_state": """
const GwyDispatchState *e10a31c_state(void) {
    static GwyDispatchState z;
    memset(&z, 0, sizeof(z));
    return &z;
}
""",
        "e10a31l_lhs_source_name": """
const char *e10a31l_lhs_source_name(E10a31lLhsSource s) {
    (void)s;
    return "stub";
}
""",
        "ext_gwy_shell_native_exec_class_name": """
const char *ext_gwy_shell_native_exec_class_name(GwyShellNativeExecClass c) {
    (void)c;
    return "stub";
}
""",
        "ext_gwy_shell_shim_class_name": """
const char *ext_gwy_shell_shim_class_name(GwyShellLaunchClass c) {
    (void)c;
    return "stub";
}
""",
        "ext_gwy_shell_shim_jjfb_target": """
const char *ext_gwy_shell_shim_jjfb_target(void) { return ""; }
""",
        "ext_gwy_shell_shim_jjfb_param": """
const char *ext_gwy_shell_shim_jjfb_param(void) { return ""; }
""",
        "ext_gwy_shell_shim_active_package": """
const char *ext_gwy_shell_shim_active_package(void) { return ""; }
""",
        "gwy_sms_cfg_state": """
const GwySmsCfgState *gwy_sms_cfg_state(void) {
    static GwySmsCfgState z;
    memset(&z, 0, sizeof(z));
    return &z;
}
""",
        "sms_cfg_compat_profiles": """
const SmsCfgCompatProfile *sms_cfg_compat_profiles(uint32_t *out_count) {
    if (out_count) *out_count = 0;
    return 0;
}
""",
        "sms_cfg_compat_select": """
const SmsCfgCompatProfile *sms_cfg_compat_select(const uint8_t cfunction_sha256[32],
                                                 uint32_t mr_version, uint32_t cfg_length) {
    (void)cfunction_sha256;
    (void)mr_version;
    (void)cfg_length;
    return 0;
}
""",
    }
    for name, body in manuals.items():
        if name not in seen:
            seen.add(name)
            chunks.append(body.strip() + "\n")

    out = ROOT / "src" / "research" / "gwy_shell" / "research_gwy_shell_stubs.c"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(chunks), encoding="utf-8")
    print(f"wrote {out.relative_to(ROOT)} ({len(seen)} symbols)")


if __name__ == "__main__":
    main()
