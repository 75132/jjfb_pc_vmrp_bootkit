import sys
from pathlib import Path
sys.path.append(str(Path('tools').resolve()))
from phase6j_common import member_blob
blob = member_blob(Path('game_files/mythroad/320x480/gwy/gamelist.mrp'),'gamelist.ext')
gbase=0x2D4354

def u16_at(va):
    o=va-gbase
    return blob[o] | (blob[o+1]<<8)

def decode_bl(va):
    h=u16_at(va); h2=u16_at(va+2)
    if (h & 0xF800)==0xF000 and (h2 & 0xF800)==0xF800:
        s=(h>>10)&1; imm10=h&0x3FF; j1=(h2>>13)&1; j2=(h2>>11)&1; imm11=h2&0x7FF
        I1=1-(j1^s); I2=1-(j2^s)
        imm=((s<<24)|(I1<<23)|(I2<<22)|(imm10<<12)|(imm11<<1))
        if s: imm |= ~((1<<25)-1)
        tgt=(va+4+imm)&0xFFFFFFFF
        return tgt
    return None

for va in [0x2E58AC,0x2E7F0A,0x2E5396]:
    print('\nsite',hex(va),'h=',hex(u16_at(va)))
    for d in range(-20,24,2):
        a=va+d; h=u16_at(a)
        line=f'{hex(a)}: {h:04X}'
        t=decode_bl(a)
        if t is not None:
            line += f' BL {hex(t)}'
        print(line)
