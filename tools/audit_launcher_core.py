#!/usr/bin/env python3
"""Policy audit for the clean launcher rewrite.

Fails when product core contains known game-state forcing, JJFB absolute guest
addresses, or legacy experiment environment variables. Compatibility-profile
files are allowed to contain declarative aliases, but executable core is not.

Research-track sources (E10A / GWY shell / SMSCFG probes) are exempt from
product banned-pattern / Unicorn-surface rules, but must not appear in the
CMake launcher_core source list.
"""
from __future__ import annotations
import argparse, json, re
from pathlib import Path

BANNED = {
    'force_ui_mode': re.compile(r'FORCE_UI_MODE|ui_mode\s*=\s*0x45|0x8D0'),
    'progress_driver': re.compile(r'PROGRESS_DRIVER|progress_count|0xBA0'),
    'game_gate_offsets': re.compile(r'0xB70|0xB71|0xB6C|0xAC8|0x134D'),
    'jjfb_absolute_code': re.compile(r'0x2DADC4|0x2FC418|0x2EF86C|0x305EB8|0x306344'),
    'legacy_injection': re.compile(r'FAMILY_C0_AFTER_B71|V64_ENQUEUE_ONCE|PATH_A_EVENT_ONCE|B58_SECOND'),
    'host_fake_ui': re.compile(r'host.*(slogo|loadingbar|progress)|fake.*ui', re.I),
}
EXTS={'.c','.h','.cc','.cpp','.rs','.py','.ps1','.cmake','.txt'}

UC_MEM_RW = re.compile(r'\buc_mem_(?:read|write)\s*\(')
UC_MEM_MAP = re.compile(r'\buc_mem_map(?:_ptr)?\s*\(')
UC_REG_WRITE = re.compile(r'\buc_reg_write\s*\(')

# Must match CMake GWY_RESEARCH_GWY_SHELL_SOURCES basenames (+ headers / stubs).
RESEARCH_SOURCE_BASENAMES = {
    'robotol_idle_watch.c', 'robotol_flag_writer_trace.c',
    'jjfb_bmp_meta.c', 'jjfb_plat_11f00.c',
    'ext_gwy_shell_shim.c', 'ext_gwy_shell_native_exec.c',
    'e10a_shell_trace.c', 'e10a3_postselect_trace.c',
    'e10a31_gamelist_context.c', 'e10a31a_precont_diag.c', 'e10a31b_publication.c',
    'e10a31c_dispatch.c', 'e10a31d_provenance.c', 'e10a31e_appinfo.c',
    'e10a31f_failsite.c', 'e10a31g_strcmp.c', 'e10a31h_smscfg.c',
    'e10a31j_smscfg_long.c', 'e10a31l_config_map.c', 'e10a31m_fail_2e5404.c',
    'e10a31n_post_range.c', 'sms_cfg_compat_profile.c', 'gwy_sms_cfg.c',
    'research_gwy_shell_stubs.c',
}
RESEARCH_HEADER_BASENAMES = {
    'robotol_idle_watch.h', 'robotol_flag_writer_trace.h',
    'jjfb_bmp_meta.h', 'jjfb_plat_11f00.h',
    'ext_gwy_shell_shim.h', 'ext_gwy_shell_native_exec.h',
    'e10a_shell_trace.h', 'e10a3_postselect_trace.h',
    'e10a31_gamelist_context.h', 'e10a31a_precont_diag.h', 'e10a31b_publication.h',
    'e10a31c_dispatch.h', 'e10a31d_provenance.h', 'e10a31e_appinfo.h',
    'e10a31f_failsite.h', 'e10a31g_strcmp.h', 'e10a31h_smscfg.h',
    'e10a31j_smscfg_long.h', 'e10a31l_config_map.h', 'e10a31m_fail_2e5404.h',
    'e10a31n_post_range.h', 'sms_cfg_compat_profile.h', 'gwy_sms_cfg.h',
}
RESEARCH_SCRIPT_RE = re.compile(
    r'^(?:RUN_(?:E\d|LIVE_|PHASE6|RESEARCH|E10A)|.*e10a.*|.*e9[a-z]_.*)',
    re.I,
)


def is_research_path(rel: str) -> bool:
    """Research-track paths: exempt from product banned / Unicorn surface rules."""
    if rel.startswith(('src/research/', 'research/', 'reports/', 'out/e10a')):
        return True
    name = Path(rel).name
    if name in RESEARCH_SOURCE_BASENAMES or name in RESEARCH_HEADER_BASENAMES:
        return True
    if name.startswith(('e10a', 'e9', 'e8')):
        return True
    if rel.startswith('tools/') and (
        RESEARCH_SCRIPT_RE.match(name) or name.startswith(('e8', 'e9', 'e10'))
    ):
        return True
    if rel.endswith('.ps1') and RESEARCH_SCRIPT_RE.match(name):
        return True
    return False


def audit_filelib_bound_no_host_fallback(root: Path, findings: list) -> None:
    """In fileLib.c, each gwy_vm_file_is_bound() true-branch must return without open/fopen."""
    path = root / 'third_party' / 'vmrp_upstream' / 'fileLib.c'
    if not path.is_file():
        return
    text = path.read_text(errors='ignore')
    rel = 'third_party/vmrp_upstream/fileLib.c'
    for m in re.finditer(r'if\s*\(\s*gwy_vm_file_is_bound\s*\(\s*\)\s*\)\s*\{', text):
        start = m.end()
        depth = 1
        i = start
        while i < len(text) and depth:
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
            i += 1
        block = text[start:i - 1]
        line = text.count('\n', 0, m.start()) + 1
        if re.search(r'\bopen\s*\(|\bfopen\s*\(', block):
            findings.append({
                'rule': 'bound_host_fallback',
                'file': rel,
                'line': line,
                'match': 'host open/fopen inside gwy_vm_file_is_bound true branch',
            })
        if 'return' not in block:
            findings.append({
                'rule': 'bound_missing_return',
                'file': rel,
                'line': line,
                'match': 'gwy_vm_file_is_bound true branch without return',
            })


def _makefile_recipe(text: str, target: str) -> tuple[str, int]:
    lines = text.splitlines()
    in_tgt = False
    recipe_lines = []
    start_line = 0
    for i, line in enumerate(lines):
        if re.match(rf'^{re.escape(target)}:', line):
            in_tgt = True
            start_line = i + 1
            recipe_lines.append(line)
            continue
        if in_tgt:
            if line.startswith('\t') or line.startswith(' '):
                recipe_lines.append(line)
            elif line.strip() == '':
                continue
            elif re.match(r'^\S[^:]*:', line) or re.match(r'^\.\.?PHONY', line):
                break
            else:
                break
    return '\n'.join(recipe_lines), start_line


def audit_makefile_no_weak_in_gwy(root: Path, findings: list) -> None:
    path = root / 'third_party' / 'vmrp_upstream' / 'Makefile'
    if not path.is_file():
        return
    text = path.read_text(errors='ignore')
    rel = 'third_party/vmrp_upstream/Makefile'
    if not re.search(r'(?m)^gwy:', text):
        findings.append({'rule': 'makefile_gwy_missing', 'file': rel,
                         'line': 0, 'match': 'no gwy target'})
        return
    recipe, start_line = _makefile_recipe(text, 'gwy')
    if 'gwy_vm_file_weak' in recipe:
        findings.append({
            'rule': 'gwy_links_weak',
            'file': rel,
            'line': start_line,
            'match': 'gwy target must not link gwy_vm_file_weak',
        })
    if 'LAUNCHER_LIB' not in recipe and 'liblauncher_core' not in recipe:
        findings.append({
            'rule': 'gwy_missing_launcher_lib',
            'file': rel,
            'line': start_line,
            'match': 'gwy target must link launcher_core',
        })
    if 'RESEARCH_LIB' not in recipe and 'research_gwy_shell' not in recipe:
        findings.append({
            'rule': 'gwy_missing_research_lib',
            'file': rel,
            'line': start_line,
            'match': 'gwy product target must link RESEARCH_LIB (stubs)',
        })


def audit_cmake_launcher_core_no_research(root: Path, findings: list) -> None:
    """launcher_core CMake sources must not list research probe .c files."""
    path = root / 'CMakeLists.txt'
    if not path.is_file():
        return
    text = path.read_text(errors='ignore')
    rel = 'CMakeLists.txt'
    m = re.search(
        r'set\s*\(\s*GWY_LAUNCHER_CORE_SOURCES\b(.*?)\)\s*\n\s*set\s*\(\s*GWY_RESEARCH',
        text,
        flags=re.S,
    )
    if not m:
        findings.append({
            'rule': 'cmake_launcher_core_list_missing',
            'file': rel,
            'line': 0,
            'match': 'GWY_LAUNCHER_CORE_SOURCES not found',
        })
        return
    block = m.group(1)
    start = text.count('\n', 0, m.start()) + 1
    for name in sorted(RESEARCH_SOURCE_BASENAMES):
        if name == 'research_gwy_shell_stubs.c':
            continue
        if re.search(rf'[\\/]{re.escape(name)}\b', block):
            findings.append({
                'rule': 'research_in_launcher_core',
                'file': rel,
                'line': start,
                'match': name,
            })


def audit_header_entry_not_execution_target(root: Path, findings: list) -> None:
    """header_entry_candidate / map.entry_address must not drive guest execution."""
    exec_call = re.compile(r'\b(?:uc_emu_start|runCode)\s*\(')
    assign_to_pc = re.compile(
        r'(?:start_pc|entry_pc|emu_pc|pc_start)\s*=\s*[^=\n]*'
        r'(?:header_entry_candidate|map\.entry_address|\.entry_address)\b'
    )
    banned_as_arg = re.compile(
        r'(?:uc_emu_start|runCode)\s*\([^)]*'
        r'(?:header_entry_candidate|(?<![A-Za-z0-9_])map\.entry_address|(?<![A-Za-z0-9_])entry_address)'
    )
    src = root / 'src'
    if not src.is_dir():
        return
    for p in src.rglob('*.c'):
        rel = p.relative_to(root).as_posix()
        if is_research_path(rel):
            continue
        text = p.read_text(errors='ignore')
        for m in banned_as_arg.finditer(text):
            line = text.count('\n', 0, m.start()) + 1
            findings.append({
                'rule': 'header_entry_execution_target',
                'file': rel,
                'line': line,
                'match': m.group(0)[:120],
            })
        for m in assign_to_pc.finditer(text):
            line = text.count('\n', 0, m.start()) + 1
            findings.append({
                'rule': 'header_entry_execution_target',
                'file': rel,
                'line': line,
                'match': m.group(0)[:120],
            })
        for i, line_txt in enumerate(text.splitlines(), 1):
            if 'header_entry_candidate' not in line_txt and 'entry_address' not in line_txt:
                continue
            if not exec_call.search(line_txt):
                continue
            if re.search(r'\bprintf\s*\(|\bfprintf\s*\(|\bsnprintf\s*\(', line_txt):
                continue
            findings.append({
                'rule': 'header_entry_execution_target',
                'file': rel,
                'line': i,
                'match': line_txt.strip()[:120],
            })


def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('root',type=Path)
    ap.add_argument('--json',type=Path)
    args=ap.parse_args()
    findings=[]
    skip_exact = {
        'tools/audit_launcher_core.py',
        'tools/test_audit_gate.py',
        'tools/gen_research_stubs.py',
        'RUN_TESTS.ps1',
        'RUN_BUILD.ps1',
        'RUN_BUILD_VMRP.ps1',
        'RUN_RESEARCH_GWY_SHELL.ps1',
        'RUN_PRODUCT_DIRECT_JJFB.ps1',
        'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1',
        'RUN_GAMES.ps1',
        'RUN_VMRP_VISUAL.ps1',
        'research/runners/README.md',
        'docs/REPO_LAYOUT.md',
        'docs/00_READ_ME_FIRST.md',
        'docs/cursor/START_CURSOR_HERE.md',
        'docs/cursor/CURSOR_MASTER_PROMPT.md',
    }
    for p in args.root.rglob('*'):
        if not p.is_file() or p.suffix.lower() not in EXTS: continue
        rel=p.relative_to(args.root).as_posix()
        if rel in skip_exact: continue
        if rel.startswith(('legacy_lab/','legacy_lab_external/','third_party/','docs/','evidence/',
                           'profiles/','templates/','decisions/','cursor/','.cursor/','logs/',
                           'out/','build-i686/','build/','research/','reports/')): continue

        research = is_research_path(rel)
        text=p.read_text(errors='ignore')

        if rel.startswith(('src/','include/')) and not research and 'legacy_lab' in text.replace('\\','/'):
            findings.append({'rule':'legacy_lab_include','file':rel,'line':0,'match':'legacy_lab'})
        if rel.startswith(('src/','include/')) and not research and re.search(r'appid\s*==\s*400101', text):
            findings.append({'rule':'appid_special_case','file':rel,'line':text.count('\n',0,re.search(r'appid\s*==\s*400101', text).start())+1,'match':'appid == 400101'})

        if rel.startswith('src/') and rel.endswith('.c') and not research:
            for m in UC_MEM_RW.finditer(text):
                if rel != 'src/runtime/guest_memory.c':
                    line=text.count('\n',0,m.start())+1
                    findings.append({'rule':'uc_mem_rw_outside_guest_memory','file':rel,'line':line,'match':m.group(0)[:80]})
            for m in UC_MEM_MAP.finditer(text):
                if rel != 'src/runtime/vm_runtime.c':
                    line=text.count('\n',0,m.start())+1
                    findings.append({'rule':'uc_mem_map_outside_vm_runtime','file':rel,'line':line,'match':m.group(0)[:80]})
            for m in UC_REG_WRITE.finditer(text):
                if rel != 'src/runtime/guest_memory.c':
                    line=text.count('\n',0,m.start())+1
                    findings.append({'rule':'uc_reg_write_outside_guest_memory','file':rel,'line':line,'match':m.group(0)[:80]})

        if not research:
            for name,pat in BANNED.items():
                for m in pat.finditer(text):
                    line=text.count('\n',0,m.start())+1
                    findings.append({'rule':name,'file':rel,'line':line,'match':m.group(0)[:120]})

    audit_filelib_bound_no_host_fallback(args.root, findings)
    audit_makefile_no_weak_in_gwy(args.root, findings)
    audit_cmake_launcher_core_no_research(args.root, findings)
    audit_header_entry_not_execution_target(args.root, findings)

    report={'root':str(args.root),'ok':not findings,'findings':findings}
    if args.json:
        args.json.parent.mkdir(parents=True,exist_ok=True)
        args.json.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))
    return 0 if not findings else 2

if __name__=='__main__': raise SystemExit(main())
