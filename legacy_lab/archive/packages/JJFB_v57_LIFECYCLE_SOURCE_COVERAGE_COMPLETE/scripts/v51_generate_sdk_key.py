#!/usr/bin/env python3
"""Generate the raw 48-byte Mythroad sdk_key.dat expected by start.mr.

Algorithm recovered from the bundled SDK key generator and the original
jjfb.mrp start.mr:
  MD5(custom_base64(part1)) || MD5(custom_base64(part2)) || MD5(custom_base64(part3))

The defaults match this vmrp source tree's GetSysInfo implementation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def _enc_char(value: int) -> int:
    if value == 7:
        return ord("D")
    if value == 14:
        return ord("h")
    if value == 59:
        return ord("/")
    if 11 <= value <= 36:
        return value + ord("A") - 11
    if 47 <= value <= 61:
        return value + ord("l") - 47
    if value <= 10:
        return value + ord("a")
    if 37 <= value <= 46:
        return value + ord("0") - 37
    if value == 62:
        return ord("+")
    if value == 63:
        return ord("x")
    raise ValueError(f"invalid 6-bit value: {value}")


def mythroad_base64(data: bytes) -> bytes:
    out = bytearray()
    whole, remain = divmod(len(data), 3)
    pos = 0
    for _ in range(whole):
        a, b, c = data[pos : pos + 3]
        pos += 3
        out.extend(
            (
                _enc_char(a >> 2),
                _enc_char(((a & 0x03) << 4) | (b >> 4)),
                _enc_char(((b & 0x0F) << 2) | (c >> 6)),
                _enc_char(c & 0x3F),
            )
        )
    if remain:
        tail = bytearray(3)
        tail[:remain] = data[pos:]
        a, b, c = tail
        quartet = [
            _enc_char(a >> 2),
            _enc_char(((a & 0x03) << 4) | (b >> 4)),
            _enc_char(((b & 0x0F) << 2) | (c >> 6)),
            _enc_char(c & 0x3F),
        ]
        for z in range(3 - remain):
            quartet[3 - z] = ord("=")
        out.extend(quartet)
    return bytes(out)


def generate_key(vmver: str, imei: str, hsman: str, hstype: str) -> tuple[bytes, list[dict[str, str | int]]]:
    if len(imei) != 15 or not imei.isdigit():
        raise ValueError("IMEI must be exactly 15 decimal digits")
    if not hsman or not hstype:
        raise ValueError("hsman and hstype must be non-empty")

    # GetSysInfo pushes IMEI as a 16-byte Lua string, including the C NUL.
    imei_raw = imei.encode("ascii") + b"\x00"
    hsman_b = hsman.encode("ascii")
    hstype_b = hstype.encode("ascii")
    parts = [
        vmver.encode("ascii") + imei_raw[2:],
        imei_raw[1:7] + hsman_b + hstype_b[1:],
        imei_raw[8:14] + hstype_b[:3],
    ]

    details: list[dict[str, str | int]] = []
    digests: list[bytes] = []
    for index, part in enumerate(parts, start=1):
        encoded = mythroad_base64(part)
        digest = hashlib.md5(encoded).digest()
        digests.append(digest)
        details.append(
            {
                "part": index,
                "input_hex": part.hex(),
                "input_len": len(part),
                "encoded_ascii": encoded.decode("ascii"),
                "encoded_len": len(encoded),
                "md5_hex": digest.hex(),
            }
        )
    return b"".join(digests), details


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--vmver", default="1968")
    parser.add_argument("--imei", default="864086040622841")
    parser.add_argument("--hsman", default="vmrp")
    parser.add_argument("--hstype", default="vmrp")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    key, details = generate_key(args.vmver, args.imei, args.hsman, args.hstype)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(key)
    report = {
        "vmver": args.vmver,
        "imei": args.imei,
        "hsman": args.hsman,
        "hstype": args.hstype,
        "key_len": len(key),
        "key_hex": key.hex(),
        "key_sha256": hashlib.sha256(key).hexdigest(),
        "parts": details,
        "output": str(args.output),
    }
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
