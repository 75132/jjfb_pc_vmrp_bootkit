#!/usr/bin/env python3
"""Read-only inspector for the observed GWY cfg.bin record-36 layout.

Important: the 1024-byte prefix / 272-byte record framing is an empirical
layout for the supplied cfg.bin region, not yet a universally proven GWY cfg
specification. Known launch fields in record 36 use compact big-endian fields;
unknown bytes are preserved and reported rather than guessed.
"""
from __future__ import annotations
import argparse, hashlib, json, re
from pathlib import Path

RECORD_BASE = 1024
RECORD_SIZE = 272


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def be24(buf: bytes) -> int:
    if len(buf) != 3:
        raise ValueError('be24 requires exactly 3 bytes')
    return int.from_bytes(buf, 'big')


def parse_record(data: bytes, index: int) -> dict:
    off = RECORD_BASE + index * RECORD_SIZE
    rec = data[off:off + RECORD_SIZE]
    if len(rec) != RECORD_SIZE:
        raise ValueError(f'record {index} is outside file')
    path_match = re.search(rb'gwy/[A-Za-z0-9_./-]+\.mrp', rec)
    # The supplied record stores only the visible suffix "风暴(火爆公测)" in
    # this fixed slice. Do not silently prepend "机甲" in the parser.
    title_suffix = rec[0x5C:0x70].decode('utf-16be', 'replace').rstrip('\0')
    return {
        'index': index,
        'offset': off,
        'record_size': RECORD_SIZE,
        'layout_confidence': 'empirical_record36',
        'icon_ascii_0x40': rec[0x40:0x58].split(b'\0')[0].decode('ascii', 'replace'),
        'napptype_u8_0x57': rec[0x57],
        'legacy_4byte_ascii_0x58': rec[0x58:0x5C].decode('ascii', 'replace'),
        'title_suffix_utf16be_0x5c': title_suffix,
        'unknown_0x70_0x71_hex': rec[0x70:0x72].hex(),
        'nextid_be24_0x72': be24(rec[0x72:0x75]),
        'unknown_0x75_0x77_hex': rec[0x75:0x78].hex(),
        'ncode_be24_0x78': be24(rec[0x78:0x7B]),
        'narg_be24_0x7b': be24(rec[0x7B:0x7E]),
        'narg1_u8_0x7e': rec[0x7E],
        'target': path_match.group().decode() if path_match else None,
        'raw_hex': rec.hex(),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('cfg', type=Path)
    ap.add_argument('--index', type=int, default=36)
    ap.add_argument('--json', type=Path)
    args = ap.parse_args()
    data = args.cfg.read_bytes()
    out = {'cfg': str(args.cfg), 'sha256': sha256(args.cfg), 'record': parse_record(data, args.index)}
    text = json.dumps(out, ensure_ascii=False, indent=2)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(text, encoding='utf-8')
    print(text)
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
