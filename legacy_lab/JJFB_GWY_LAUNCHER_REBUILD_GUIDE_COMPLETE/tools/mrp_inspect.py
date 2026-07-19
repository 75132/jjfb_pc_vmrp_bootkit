#!/usr/bin/env python3
"""Read-only MRP inspector for MRPG archives.

It parses the public header/index layout used by this project and optionally
extracts members. It never writes into the source MRP.
"""
from __future__ import annotations
import argparse, csv, gzip, hashlib, json, struct, sys, zlib
from pathlib import Path


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def try_decode(raw: bytes) -> tuple[bytes, str]:
    for label, fn in (
        ('gzip', gzip.decompress),
        ('zlib-gzip', lambda b: zlib.decompress(b, 16 + zlib.MAX_WBITS)),
        ('zlib', zlib.decompress),
    ):
        try:
            return fn(raw), label
        except Exception:
            pass
    return raw, 'raw'


def parse(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 240 or data[:4] != b'MRPG':
        raise ValueError(f'not an MRPG archive: {path}')
    first_data = struct.unpack_from('<I', data, 4)[0] - 4
    total = struct.unpack_from('<I', data, 8)[0]
    header_len = struct.unpack_from('<I', data, 12)[0]
    if total != len(data):
        raise ValueError(f'length mismatch: header={total}, actual={len(data)}')
    if not (header_len <= first_data <= len(data)):
        raise ValueError(f'invalid index/data boundary: header={header_len}, data={first_data}')
    pos = header_len
    members = []
    while pos < first_data:
        if pos + 4 > first_data:
            raise ValueError(f'truncated index at 0x{pos:X}')
        name_len = struct.unpack_from('<I', data, pos)[0]
        pos += 4
        if not 1 <= name_len <= 512 or pos + name_len + 12 > first_data + 16:
            raise ValueError(f'invalid member name length {name_len} at 0x{pos-4:X}')
        name = data[pos:pos + name_len].rstrip(b'\0').decode('latin1')
        pos += name_len
        offset, stored_len, reserved = struct.unpack_from('<III', data, pos)
        pos += 12
        if offset + stored_len > len(data):
            raise ValueError(f'member outside archive: {name}')
        decoded, encoding = try_decode(data[offset:offset + stored_len])
        members.append({
            'name': name,
            'offset': offset,
            'stored_length': stored_len,
            'decoded_length': len(decoded),
            'encoding': encoding,
            'reserved': reserved,
        })
    return {
        'path': str(path),
        'sha256': sha256(path),
        'size': len(data),
        'header_length': header_len,
        'first_data_offset': first_data,
        'internal_name': data[16:28].split(b'\0')[0].decode('latin1', 'replace'),
        'display_name': data[28:52].split(b'\0')[0].decode('gbk', 'replace'),
        'appid_le': struct.unpack_from('<I', data, 68)[0],
        'appver_le': struct.unpack_from('<I', data, 72)[0],
        'flags': struct.unpack_from('<I', data, 76)[0],
        'appid_be': struct.unpack_from('>I', data, 192)[0],
        'appver_be': struct.unpack_from('>I', data, 196)[0],
        'members': members,
    }


def extract(path: Path, report: dict, out_dir: Path) -> None:
    data = path.read_bytes()
    out_dir.mkdir(parents=True, exist_ok=True)
    for member in report['members']:
        raw = data[member['offset']:member['offset'] + member['stored_length']]
        decoded, _ = try_decode(raw)
        target = out_dir / member['name']
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(decoded)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('mrp', type=Path)
    ap.add_argument('--json', type=Path)
    ap.add_argument('--csv', type=Path)
    ap.add_argument('--extract', type=Path)
    args = ap.parse_args()
    report = parse(args.mrp)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    if args.csv:
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        with args.csv.open('w', newline='', encoding='utf-8-sig') as f:
            w = csv.DictWriter(f, fieldnames=['name','offset','stored_length','decoded_length','encoding','reserved'])
            w.writeheader(); w.writerows(report['members'])
    if args.extract:
        extract(args.mrp, report, args.extract)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
