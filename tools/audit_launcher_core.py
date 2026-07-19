#!/usr/bin/env python3
"""Policy audit for the clean launcher rewrite.

Fails when core source contains known game-state forcing, JJFB absolute guest
addresses, or legacy experiment environment variables. Compatibility-profile
files are allowed to contain declarative aliases, but executable core is not.
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


def audit_filelib_bound_no_host_fallback(root: Path, findings: list) -> None:
    """In fileLib.c, each gwy_vm_file_is_bound() true-branch must return without open/fopen."""
    path = root / 'third_party' / 'vmrp_upstream' / 'fileLib.c'
    if not path.is_file():
        return
    text = path.read_text(errors='ignore')
    rel = 'third_party/vmrp_upstream/fileLib.c'
    # Find each bound check and inspect the immediate block until matching close brace depth.
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
    # Collect all lines belonging to gwy recipes (target lines + tab-indented recipes until next target)
    lines = text.splitlines()
    in_gwy = False
    recipe_lines = []
    for i, line in enumerate(lines):
        if re.match(r'^gwy:', line):
            in_gwy = True
            recipe_lines.append(line)
            continue
        if in_gwy:
            if line.startswith('\t') or line.startswith(' '):
                recipe_lines.append(line)
            elif line.strip() == '':
                continue
            elif re.match(r'^\S[^:]*:', line) or re.match(r'^\.\PHONY', line) or re.match(r'^\.PHONY', line):
                in_gwy = False
            else:
                in_gwy = False
    recipe = '\n'.join(recipe_lines)
    start_line = next((i + 1 for i, l in enumerate(lines) if re.match(r'^gwy:', l)), 1)
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


def audit_header_entry_not_execution_target(root: Path, findings: list) -> None:
    """header_entry_candidate / map.entry_address must not drive guest execution."""
    exec_call = re.compile(
        r'\b(?:uc_emu_start|runCode)\s*\('
    )
    # Assignment into a start-PC / entry-PC variable from banned symbols.
    assign_to_pc = re.compile(
        r'(?:start_pc|entry_pc|emu_pc|pc_start)\s*=\s*[^=\n]*'
        r'(?:header_entry_candidate|map\.entry_address|\.entry_address)\b'
    )
    # Passing banned symbols as arguments near execute calls (same statement).
    banned_as_arg = re.compile(
        r'(?:uc_emu_start|runCode)\s*\([^)]*'
        r'(?:header_entry_candidate|(?<![A-Za-z0-9_])map\.entry_address|(?<![A-Za-z0-9_])entry_address)'
    )
    src = root / 'src'
    if not src.is_dir():
        return
    for p in src.rglob('*.c'):
        rel = p.relative_to(root).as_posix()
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
        # Also catch: foo = header_entry_candidate; ... uc_emu_start(uc, foo
        # Conservative: any line that both references banned field and an exec call.
        for i, line_txt in enumerate(text.splitlines(), 1):
            if 'header_entry_candidate' not in line_txt and 'entry_address' not in line_txt:
                continue
            if not exec_call.search(line_txt):
                continue
            # Allow printf/classify/JSON/disasm compare (no exec call on same line typically).
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
    for p in args.root.rglob('*'):
        if not p.is_file() or p.suffix.lower() not in EXTS: continue
        rel=p.relative_to(args.root).as_posix()
        if rel == 'tools/audit_launcher_core.py': continue
        if rel == 'tools/test_audit_gate.py': continue
        if rel in ('RUN_TESTS.ps1', 'RUN_BUILD.ps1'): continue
        if rel.startswith(('legacy_lab/','legacy_lab_external/','third_party/','docs/','evidence/','profiles/','templates/','decisions/','cursor/','.cursor/','logs/','out/','build-i686/','build/')): continue

        text=p.read_text(errors='ignore')

        # Product code must not reference legacy lab paths.
        if rel.startswith(('src/','include/')) and 'legacy_lab' in text.replace('\\','/'):
            findings.append({'rule':'legacy_lab_include','file':rel,'line':0,'match':'legacy_lab'})
        if rel.startswith(('src/','include/')) and re.search(r'appid\s*==\s*400101', text):
            findings.append({'rule':'appid_special_case','file':rel,'line':text.count('\n',0,re.search(r'appid\s*==\s*400101', text).start())+1,'match':'appid == 400101'})

        # Unicorn mem R/W only in guest_memory.c; map only in vm_runtime.c;
        # reg write only in guest_memory.c (Phase 6C-A R9 switch surface).
        if rel.startswith('src/') and rel.endswith('.c'):
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

        for name,pat in BANNED.items():
            for m in pat.finditer(text):
                line=text.count('\n',0,m.start())+1
                findings.append({'rule':name,'file':rel,'line':line,'match':m.group(0)[:120]})

    audit_filelib_bound_no_host_fallback(args.root, findings)
    audit_makefile_no_weak_in_gwy(args.root, findings)
    audit_header_entry_not_execution_target(args.root, findings)

    report={'root':str(args.root),'ok':not findings,'findings':findings}
    if args.json:
        args.json.parent.mkdir(parents=True,exist_ok=True)
        args.json.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))
    return 0 if not findings else 2

if __name__=='__main__': raise SystemExit(main())
