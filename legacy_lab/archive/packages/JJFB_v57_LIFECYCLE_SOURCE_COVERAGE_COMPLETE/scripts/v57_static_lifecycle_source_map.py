#!/usr/bin/env python3
"""v57 static map: family C0 ingress and callback-registration producers."""
from pathlib import Path
import argparse, hashlib, struct, zlib
BASE=0x2D8DF4

def parse_mrp(p):
    d=p.read_bytes(); magic,hend,total,idx=struct.unpack_from('<4I',d,0)
    if magic != 0x4750524D: raise SystemExit('bad MRP magic')
    pos=idx; end=hend+8; ent={}
    while pos<end:
        n=struct.unpack_from('<I',d,pos)[0]; pos+=4
        name=d[pos:pos+n].rstrip(b'\0').decode('latin1'); pos+=n
        off,clen,flags=struct.unpack_from('<3I',d,pos); pos+=12
        ent[name]=(off,clen,flags)
    return d,ent

def u32(b,va): return struct.unpack_from('<I',b,va-BASE)[0]
def codeptr_from_pcadd(value, add_insn): return (value + add_insn + 4) & 0xffffffff

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('mrp',type=Path); ap.add_argument('--markdown',type=Path,required=True); a=ap.parse_args()
    d,e=parse_mrp(a.mrp); off,clen,_=e['robotol.ext']; rob=zlib.decompress(d[off:off+clen],31)
    family_ptr=codeptr_from_pcadd(u32(rob,0x304A20),0x30492A)
    tick_ptr=codeptr_from_pcadd(u32(rob,0x304A38),0x304990)
    table=rob[0x304B10-BASE:0x304B10-BASE+11]
    targets=[0x304B12+x*2 for x in table]
    guest_apps=[0x02,0x03,0x09,0x0A,0x1D,0x1F,0x22,0x23,0x24,0x25,0x26,0x27]
    checks=[]
    checks.append(('10102 family handler',family_ptr==0x30D301,hex(family_ptr)))
    checks.append(('10140 tick handler',tick_ptr==0x30630D,hex(tick_ptr)))
    checks.append(('EXT method 1 target',targets[1]==0x304B30,hex(targets[1])))
    checks.append(('EXT method 5 target',targets[5]==0x304B5A,hex(targets[5])))
    lines=['# v57 Lifecycle Source Static Map','',f'- MRP SHA-256: `{hashlib.sha256(d).hexdigest()}`',f'- robotol.ext: compressed `{clen}`, decompressed `{len(rob)}`','',
           '## 1. Registration contracts','',
           f'- `0x10102` registers family handler pointer `{family_ptr:#x}` → code `0x30D300`.',
           f'- `0x10140` registers tick/state handler pointer `{tick_ptr:#x}` → code `0x30630C`.',
           '- **Correction:** `0x303E14` is not the `0x10140` handler. It is a lifecycle-command dispatcher reached through robotol EXT method 1.','',
           '## 2. Who can produce family app=0xC0?','',
           '- `0x30D300` has no direct `BL` caller; it is entered through the handler registered by `0x10102`.',
           '- Guest `sendAppEvent(0x1E209, app, ...)` producers found in robotol use low app values: '+', '.join(f'`0x{x:X}`' for x in guest_apps)+'.',
           '- No direct guest producer of `app=0xC0` was found in this static set.',
           '- Therefore the `app=0xC0 → 0x30DC44 → 0x2FEBBC` path is best classified as **platform-originated family lifecycle ingress**, not an ordinary guest `0x1E209` emission. This is an inference from the registered-callback architecture and producer scan.','',
           '## 3. Why 0x3054A4 never registers 0x2F5405','',
           '- Callback constructor: `0x2F5390 → 0x2F53AC(load 0x2F5405) → 0x3054A4`.',
           '- Producer A: robotol EXT **method 1** dispatches to `0x304B30`, reads a 3-word payload, then calls `0x303E14(cmd,arg1,arg2)`. Only `cmd=10002 (0x2712)` reaches `0x304418 → 0x2F5390`.',
           '- Producer B: robotol EXT **method 5** dispatches to `0x304B5A → 0x3053B8 → 0x2F5390` directly.',
           '- If neither EXT method 1 with command 10002 nor EXT method 5 occurs, `0x3054A4` cannot register `0x2F5405` through these natural producers.','',
           '## 4. Separation from the periodic handler','',
           '- Host currently calls the registered `0x10140` handler at `0x30630D` with `r0=0,r1=0` as a periodic tick.',
           '- That periodic handler is separate from robotol EXT method 1/5 and does not by itself prove delivery of lifecycle command 10002.','',
           '## 5. Static assertions','', '| Assertion | Pass | Value |','|---|---:|---|']
    for n,ok,val in checks: lines.append(f'| {n} | {"yes" if ok else "NO"} | `{val}` |')
    lines += ['', '## 6. v57 restrictions','', '- No family `app=0xC0` injection.', '- No robotol EXT method 1/5 injection.', '- No command 10002 injection.', '- No `ui_mode=0x45` FORCE, AC8 driver, progress driver, or host-rendered UI.']
    a.markdown.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    if not all(x[1] for x in checks): raise SystemExit('static assertion failed')
    print('v57 static lifecycle map: PASS')
if __name__=='__main__': main()
