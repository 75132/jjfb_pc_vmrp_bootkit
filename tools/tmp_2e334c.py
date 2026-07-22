import sys
from pathlib import Path
sys.path.append(str(Path('tools').resolve()))
from phase6j_common import member_blob
blob=member_blob(Path('game_files/mythroad/320x480/gwy/gamelist.mrp'),'gamelist.ext')
gbase=0x2D4354
start=0x2E334C
for a in range(start,start+0x90,2):
    o=a-gbase
    h=blob[o]|(blob[o+1]<<8)
    print(hex(a),f'{h:04X}')
