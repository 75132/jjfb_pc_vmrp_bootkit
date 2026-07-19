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
        if rel.startswith(('legacy_lab/','legacy_lab_external/','third_party/','docs/','evidence/','profiles/','templates/','decisions/','cursor/')): continue
        text=p.read_text(errors='ignore')
        for name,pat in BANNED.items():
            for m in pat.finditer(text):
                line=text.count('\n',0,m.start())+1
                findings.append({'rule':name,'file':rel,'line':line,'match':m.group(0)[:120]})
    report={'root':str(args.root),'ok':not findings,'findings':findings}
    if args.json:
        args.json.parent.mkdir(parents=True,exist_ok=True)
        args.json.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))
    return 0 if not findings else 2

if __name__=='__main__': raise SystemExit(main())
