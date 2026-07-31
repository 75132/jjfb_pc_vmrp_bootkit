#!/usr/bin/env python3
# Minimal MRP table parser used for JJFB resource coverage checks.
from pathlib import Path
import struct, hashlib, sys, json

def parse_mrp(path):
    b=Path(path).read_bytes()
    if b[:4] != b'MRPG':
        raise ValueError(f'not MRPG: {path}')
    header_len=struct.unpack_from('<I', b, 12)[0]
    pos=header_len; members=[]
    while pos+4 <= len(b):
        n=struct.unpack_from('<I', b, pos)[0]
        if not (1 <= n <= 260): break
        if pos+4+n+12 > len(b): break
        name=b[pos+4:pos+4+n].split(b'\0')[0].decode('latin1','replace')
        off, stored, reserved=struct.unpack_from('<III', b, pos+4+n)
        if not (0 < off <= len(b)) or off+stored > len(b)+4096 or not name:
            break
        members.append({'name':name,'offset':off,'stored_length':stored,'reserved':reserved})
        pos += 4+n+12
    return {'path':str(path),'size':len(b),'sha256':hashlib.sha256(b).hexdigest(),'header_len':header_len,'members':members}

if __name__ == '__main__':
    for arg in sys.argv[1:]:
        print(json.dumps(parse_mrp(arg), ensure_ascii=False, indent=2))
