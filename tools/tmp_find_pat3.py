import sys
from pathlib import Path
sys.path.append(str(Path('tools').resolve()))
from phase6j_common import member_blob
blob=member_blob(Path('game_files/mythroad/320x480/gwy/gamelist.mrp'),'gamelist.ext')
gbase=0x2D4354
for hx in ['FF24C134E458','FF24C134','C134E458','E458DB68']:
    p=bytes.fromhex(hx)
    idx=0
    hs=[]
    while True:
        i=blob.find(p,idx)
        if i<0: break
        hs.append(i); idx=i+1
    print(hx, len(hs), [hex(gbase+h) for h in hs[:5]])
