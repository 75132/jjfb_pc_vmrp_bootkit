import sys
from pathlib import Path
sys.path.append(str(Path('tools').resolve()))
from phase6j_common import member_blob
blob = member_blob(Path('game_files/mythroad/320x480/gwy/gamelist.mrp'),'gamelist.ext')
gbase=0x2D4354
addrs=[0x2E58E4,0x2E7F60,0x2E5396]
for a in addrs:
    off=a-gbase
    print('\n=== literal',hex(a),'file',hex(off),'===')
    print(blob[off-16:off+16].hex())

sites=[0x2E57E0,0x2E7ED0,0x2E52F0]
for s in sites:
    print('\n--- around',hex(s),'---')
    d=blob[s-gbase:s-gbase+0x180]
    def u16(i): return d[i]|(d[i+1]<<8)
    i=0; pc=s
    while i+1 < len(d):
        h=u16(i)
        op=None
        if (h&0xF800)==0x4800:
            imm=(h&0xFF)*4; rd=(h>>8)&7; base=(pc+4)&~3; lit=base+imm
            mark='***' if lit in addrs else ''
            op=f"{mark}LDR r{rd}, [pc,#0x{imm:X}] lit@{hex(lit)}{mark}"
        elif (h&0xFF00)==0x2000: op=f'MOVS r0,#{h&0xFF}'
        elif (h&0xFF00)==0x2100: op=f'MOVS r1,#{h&0xFF}'
        elif (h&0xFF00)==0x2200: op=f'MOVS r2,#{h&0xFF}'
        elif h in (0x9847,0x4798): op='BLX r3'
        elif h in (0x9047,0x4790): op='BLX r2'
        if op and ('***' in op or 'BLX' in op or 'MOVS r0,#' in op):
            print(hex(pc),op)
        i+=2; pc+=2
