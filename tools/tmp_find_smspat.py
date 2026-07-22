import sys
from pathlib import Path
sys.path.append(str(Path('tools').resolve()))
from phase6j_common import member_blob
blob=member_blob(Path('game_files/mythroad/320x480/gwy/gamelist.mrp'),'gamelist.ext')
gbase=0x2D4354
# find halfword sequence FF21 C131 0959 (mov r1,#ff ; add #c1 ; ldr r1,[r1,r4])
pat=bytes.fromhex('FF21C1310959')
idx=0
hits=[]
while True:
    i=blob.find(pat,idx)
    if i<0: break
    hits.append(i); idx=i+1
print('hits',len(hits))
for i in hits[:20]:
    va=gbase+i
    print(hex(va))
    win=blob[i-20:i+28]
    print(win.hex())
