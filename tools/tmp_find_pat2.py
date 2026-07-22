import sys
from pathlib import Path
sys.path.append(str(Path('tools').resolve()))
from phase6j_common import member_blob
blob=member_blob(Path('game_files/mythroad/320x480/gwy/gamelist.mrp'),'gamelist.ext')
gbase=0x2D4354
for hx in ['24FF34C1E458','FF2434C1E458','FF2131C1E458','FF2333C1']:
    p=bytes.fromhex(hx)
    i=blob.find(p)
    print(hx, '->', hex(gbase+i) if i>=0 else None)
