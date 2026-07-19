#!/usr/bin/env python3
"""v56 static contract map for upstream paths leading to ui_mode writer."""
from pathlib import Path
import argparse, hashlib, struct, zlib

def parse_mrp(p):
    d=p.read_bytes(); magic, hend, total, idx=struct.unpack_from('<4I',d,0)
    if magic != 0x4750524D: raise SystemExit('bad MRP magic')
    pos=idx; end=hend+8; e={}
    while pos<end:
        n=struct.unpack_from('<I',d,pos)[0]; pos+=4
        name=d[pos:pos+n].rstrip(b'\0').decode('latin1'); pos+=n
        off,clen,flags=struct.unpack_from('<3I',d,pos); pos+=12
        e[name]=(off,clen,flags)
    return d,e

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('mrp',type=Path); ap.add_argument('--markdown',type=Path)
    a=ap.parse_args(); d,e=parse_mrp(a.mrp)
    off,clen,_=e['robotol.ext']; rob=zlib.decompress(d[off:off+clen],31)
    lines=['# v56 Upstream Trigger Static Map','',f'- MRP SHA256: `{hashlib.sha256(d).hexdigest()}`',f'- robotol.ext: compressed `{clen}`, decompressed `{len(rob)}`','',
    '## Path A — event queue','',
    '- `0x2DC80C -> 0x2DC8D4 -> 0x2E2520`','- `0x2E7B7C -> 0x2E7B9E -> 0x2E2520`','- The `0x2E2520` switch reaches `0x2E4066 -> 0x2DADC4` only for event codes **5** and **12**.','- Mythroad constants: 5=`MR_MENU_RETURN`; 12=`MR_MOUSE_MOVE`. Event `0x13` is not this path.','',
    '## Path B — startup/reset','',
    '- Natural init callsite: `0x2FECA2 -> 0x2DADC4`, inside function `0x2FEBBC`.','- `0x2FEBBC` has 14 direct BL callsites: `2DCC60,2DCCC4,2DCD4E,2DCDEE,2DCF4C,2DD626,2DDA82,2E02E0,2E351A,2E7528,2E77B4,2E799E,2FBED6,30DC44`.','- Family dispatcher `0x30D300` reaches `0x30DC44` only when `app=0xC0`. Existing v55 logs showed only `app=9`.','',
    '## Path C — registered callback','',
    '- `0x2F5404` has no direct BL caller; its Thumb pointer `0x2F5405` is supplied to `0x3054A4` at `0x2F53AC` and `0x30D128`.','- Callback tail: `0x2F5734 -> 0x305EB8 -> (conditional) 0x305EF4 -> 0x2DADC4`.','- This is the strongest candidate for a platform callback/timer contract not being driven by the current host.','',
    '## v56 dynamic decision tree','',
    '1. Registration seen, callback entry absent: host scheduler/callback dispatch missing.','2. Family dispatcher seen but no `app=0xC0`: startup family command missing.','3. Event dispatcher seen but no code 5/12: required event source missing.','4. Any path reaches `0x2DADC4` but writer remains absent: blocker moves back to B70/B58/DB0 gates.','',
    '## Restrictions','', '- No `ui_mode=0x45` force.', '- No AC8/progress driver.', '- v56 disables the old synthetic `mrc_event(0,0,0)` so the coverage run is natural.']
    out='\n'.join(lines)+'\n'
    if a.markdown: a.markdown.write_text(out,encoding='utf-8')
    print(out)
if __name__=='__main__': main()
