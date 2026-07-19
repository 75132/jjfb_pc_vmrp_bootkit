#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, struct, re, shutil, zipfile, hashlib, subprocess, time
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"; LOGS.mkdir(exist_ok=True)

START_PARAM_STR = "napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
START_PARAM = START_PARAM_STR.encode("ascii") + b"\x00"
SDK_KEY = b"g:u2"

def log(name, text):
    (LOGS / name).write_text(text, encoding="utf-8", errors="replace")

def run_cmd(cmd, timeout=30, cwd=None):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, encoding="utf-8", errors="replace",
                           timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"
    except Exception as e:
        return 999, repr(e)

def sha1_file(p):
    h = hashlib.sha1()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1024*1024), b""):
            h.update(b)
    return h.hexdigest()

def find_main():
    c = []
    root = ROOT / "runtime" / "vmrp_win32"
    if root.exists():
        for p in root.rglob("main.exe"):
            if p.name.lower() == "main.exe":
                c.append(p)
    c.sort(key=lambda p: len(str(p)))
    return c[0] if c else None

def parse_pe(data):
    if data[:2] != b"MZ":
        raise RuntimeError("not MZ")
    peoff = struct.unpack_from("<I", data, 0x3c)[0]
    if data[peoff:peoff+4] != b"PE\x00\x00":
        raise RuntimeError("not PE")
    machine, nsec, timedate, ptrsym, numsym, optsize, chars = struct.unpack_from("<HHIIIHH", data, peoff+4)
    opt = peoff + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x10b:
        raise RuntimeError(f"not PE32 magic=0x{magic:x}")
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    sec_off = opt + optsize
    secs = []
    for i in range(nsec):
        o = sec_off + i*40
        name = data[o:o+8].rstrip(b"\x00").decode("ascii", "replace")
        vs, va, rawsz, rawptr, ptrrel, ptrline, nrel, nline, schars = struct.unpack_from("<IIIIIIHHI", data, o+8)
        secs.append({"index": i+1, "hdr_off": o, "name": name, "vs": vs, "va": va, "rawsz": rawsz, "rawptr": rawptr, "chars": schars})
    return {"peoff": peoff, "machine": machine, "ptrsym": ptrsym, "numsym": numsym, "image_base": image_base, "sections": secs}

def rva_to_off(rva, secs):
    for s in secs:
        size = max(s["rawsz"], s["vs"])
        if s["va"] <= rva < s["va"] + size:
            off = s["rawptr"] + (rva - s["va"])
            if 0 <= off < s["rawptr"] + s["rawsz"]:
                return off
    return None

def off_to_va(off, image_base, secs):
    for s in secs:
        if s["rawptr"] <= off < s["rawptr"] + s["rawsz"]:
            return image_base + s["va"] + (off - s["rawptr"])
    return None

def coff_name(data, sym_off, str_base, str_size):
    raw = data[sym_off:sym_off+8]
    zero, off = struct.unpack_from("<II", raw, 0)
    if zero == 0 and off != 0:
        if 0 < off < str_size:
            p = str_base + off
            end = data.find(b"\x00", p, str_base + str_size)
            if end < 0:
                end = str_base + str_size
            return data[p:end].decode("ascii", "replace")
        return f"<bad_str_{off:x}>"
    return raw.rstrip(b"\x00").decode("ascii", "replace")

def parse_coff_symbols(data, pe):
    ptr = pe["ptrsym"]; n = pe["numsym"]
    syms = []
    if not ptr or not n:
        return syms
    str_base = ptr + n*18
    str_size = 0
    if str_base + 4 <= len(data):
        str_size = struct.unpack_from("<I", data, str_base)[0]
        if str_size < 4 or str_base + str_size > len(data):
            str_size = 0
    i = 0
    while i < n:
        o = ptr + i*18
        if o + 18 > len(data):
            break
        name = coff_name(data, o, str_base, str_size) if str_size else data[o:o+8].rstrip(b"\x00").decode("ascii", "replace")
        value, secnum, typ, storage, aux = struct.unpack_from("<IhHBB", data, o+8)
        va = None
        if 0 < secnum <= len(pe["sections"]):
            sec = pe["sections"][secnum-1]
            va = pe["image_base"] + sec["va"] + value
        syms.append({"name": name, "value": value, "secnum": secnum, "va": va, "aux": aux, "sym_index": i})
        i += 1 + aux
    return syms

def find_symbol(syms):
    for nm in ["_bridge_dsm_mr_start_dsm", "bridge_dsm_mr_start_dsm"]:
        for s in syms:
            if s["name"] == nm and s["va"]:
                return s
    for s in syms:
        if "bridge_dsm_mr_start_dsm" in s["name"] and s["va"]:
            return s
    return None

def scan_calls_to(data, pe, target_va):
    calls = []
    for s in pe["sections"]:
        if not (s["chars"] & 0x20000000) and s["name"].lower() not in [".text", "text"]:
            continue
        start = s["rawptr"]; end = s["rawptr"] + s["rawsz"]
        for m in re.finditer(b"\xE8....", data[start:end], flags=re.S):
            off = start + m.start()
            call_va = off_to_va(off, pe["image_base"], pe["sections"])
            if call_va is None:
                continue
            rel = struct.unpack_from("<i", data, off+1)[0]
            dest = call_va + 5 + rel
            if dest == target_va:
                calls.append(off)
    return calls

def find_param_cave(data, pe, min_len):
    # Only needs readable memory. .rdata/.data preferred.
    order = []
    for pref in [".rdata", "rdata", ".data", "data", ".text", "text"]:
        order += [s for s in pe["sections"] if s["name"].lower() == pref]
    order += pe["sections"]
    seen = set()
    for s in order:
        key = (s["rawptr"], s["rawsz"])
        if key in seen:
            continue
        seen.add(key)
        region = data[s["rawptr"]:s["rawptr"]+s["rawsz"]]
        for byte in [b"\x00", b"\xcc"]:
            m = re.search(re.escape(byte * min_len), region)
            if m:
                off = s["rawptr"] + m.start()
                va = off_to_va(off, pe["image_base"], pe["sections"])
                if va:
                    return off, va, s
    return None, None, None

def patch():
    report = []
    exe = find_main()
    if not exe:
        raise RuntimeError("main.exe not found")

    # Start clean: restore v14/v13/v12 backups if they exist.
    for name in ["main.before_direct_call_hook_v14.exe", "main.before_iat_hook_v13.exe", "main.before_param_patch_v12.exe"]:
        bak = exe.with_name(name)
        if bak.exists():
            shutil.copy2(bak, exe)
            report.append(f"restored clean main from {bak}")
            break

    data = bytearray(exe.read_bytes())
    pe = parse_pe(data)
    syms = parse_coff_symbols(data, pe)
    bridge = find_symbol(syms)
    if not bridge:
        raise RuntimeError("bridge_dsm_mr_start_dsm symbol not found")

    report.append(f"main={exe}")
    report.append(f"sha1_before={sha1_file(exe)} size={len(data)}")
    report.append(f"image_base=0x{pe['image_base']:x} bridge={bridge['name']} va=0x{bridge['va']:x}")

    calls = scan_calls_to(data, pe, bridge["va"])
    report.append(f"direct calls to bridge={['0x%x'%c for c in calls]}")
    if not calls:
        raise RuntimeError("no direct call to bridge")

    dsm_off = data.find(b"dsm_gm.mrp\x00")
    start_off = data.find(b"start.mr\x00")
    dsm_va = off_to_va(dsm_off, pe["image_base"], pe["sections"]) if dsm_off >= 0 else None
    start_va = off_to_va(start_off, pe["image_base"], pe["sections"]) if start_off >= 0 else None
    report.append(f"dsm_off=0x{dsm_off:x} dsm_va={hex(dsm_va) if dsm_va else None}")
    report.append(f"start_off=0x{start_off:x} start_va={hex(start_va) if start_va else None}")

    dsm_refs = [m.start() for m in re.finditer(re.escape(struct.pack("<I", dsm_va)), data)] if dsm_va else []
    start_refs = [m.start() for m in re.finditer(re.escape(struct.pack("<I", start_va)), data)] if start_va else []

    scored = []
    for off in calls:
        near = data[max(0, off-220):off+120]
        score = 0
        if dsm_va and struct.pack("<I", dsm_va) in near:
            score += 20
        if start_va and struct.pack("<I", start_va) in near:
            score += 20
        if any(abs(off-r) < 250 for r in dsm_refs):
            score += 10
        if any(abs(off-r) < 250 for r in start_refs):
            score += 10
        scored.append((score, off))
        report.append(f"callsite 0x{off:x} score={score} around={bytes(data[max(0,off-120):off+80]).hex()}")
    scored.sort(reverse=True)
    target_call = scored[0][1]
    report.append(f"selected_call=0x{target_call:x} score={scored[0][0]}")

    cave_off, param_va, cave_sec = find_param_cave(data, pe, len(START_PARAM)+8)
    if cave_off is None:
        raise RuntimeError("no param cave found")
    data[cave_off:cave_off+len(START_PARAM)] = START_PARAM
    report.append(f"param_off=0x{cave_off:x} param_va=0x{param_va:x} section={cave_sec['name']}")

    # Search only before the selected call for zero 4th arg.
    win_start = max(0, target_call - 160)
    win = data[win_start:target_call]
    patterns = [
        (b"\xC7\x44\x24\x0C\x00\x00\x00\x00", 4, "mov [esp+0x0c],0"),
        (b"\xC7\x44\x24\x10\x00\x00\x00\x00", 4, "mov [esp+0x10],0"),
        (b"\xC7\x44\x24.\x00\x00\x00\x00", 4, "mov [esp+disp],0 wildcard"),
    ]
    matches = []
    # exact first
    for pat, imm_off, desc in patterns[:2]:
        idx = win.rfind(pat)
        if idx >= 0:
            matches.append((win_start+idx, imm_off, desc))
    # wildcard if exact not found
    if not matches:
        for m in re.finditer(patterns[2][0], win, flags=re.S):
            matches.append((win_start+m.start(), 4, patterns[2][2]))
    if not matches:
        report.append(f"pre_call_window={bytes(win).hex()}")
        log("direct_arg_patch_report.txt", "\n".join(report))
        raise RuntimeError("no mov [esp+disp],0 4th arg pattern before call")

    # Choose last before call.
    arg_off, imm_off, desc = sorted(matches, key=lambda x: x[0])[-1]
    old = bytes(data[arg_off:arg_off+8])
    data[arg_off+imm_off:arg_off+imm_off+4] = struct.pack("<I", param_va)
    new = bytes(data[arg_off:arg_off+8])
    report.append(f"patched_arg at 0x{arg_off:x} desc={desc} old={old.hex()} new={new.hex()}")

    # Visible marker NULL -> ARGP to prove patched exe, same 4 bytes.
    marker = b"bridge_dsm_mr_start_dsm('%s','%s',NULL)"
    mo = data.find(marker)
    if mo >= 0:
        null_off = mo + marker.find(b"NULL")
        data[null_off:null_off+4] = b"ARGP"
        report.append(f"changed printf NULL marker to ARGP at 0x{null_off:x}")

    report.append(f"param={START_PARAM_STR}")

    bak = exe.with_name("main.before_direct_arg_patch_v15.exe")
    if not bak.exists():
        shutil.copy2(exe, bak)
    patched = exe.with_name("main.direct_arg_patch_v15.exe")
    patched.write_bytes(data)
    shutil.copy2(patched, exe)
    report.append(f"backup={bak}")
    report.append(f"patched={patched} sha1={sha1_file(patched)}")
    log("direct_arg_patch_report.txt", "\n".join(report))
    print("\n".join(report))

def copy_merge(src, dst):
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        t = dst / item.name
        if item.is_dir():
            if t.exists() and t.is_dir():
                copy_merge(item, t)
            else:
                if t.exists():
                    t.unlink()
                shutil.copytree(item, t)
        else:
            shutil.copy2(item, t)

def prepare_jjfb_runtime():
    exe = find_main()
    dst = exe.parent / "mythroad"
    rep = []
    src_240 = ROOT / "game_files" / "mythroad" / "240x320"
    if src_240.exists():
        copy_merge(src_240, dst)
        if (src_240 / "gwy").exists():
            copy_merge(src_240 / "gwy", dst / "gwy")
        rep.append(f"merged {src_240} -> {dst}")
    jjfb = next((p for p in [
        ROOT / "game_files" / "mythroad" / "240x320" / "gwy" / "jjfb.mrp",
        ROOT / "game_files" / "mythroad" / "gwy" / "jjfb.mrp",
        dst / "gwy" / "jjfb.mrp",
        dst / "jjfb.mrp",
    ] if p.exists()), None)
    if jjfb:
        dsm = dst / "dsm_gm.mrp"
        bak = dst / "dsm_gm.before_direct_arg_patch_v15.mrp"
        if dsm.exists() and not bak.exists():
            shutil.copy2(dsm, bak)
        shutil.copy2(jjfb, dsm)
        shutil.copy2(jjfb, dst / "jjfb.mrp")
        (dst / "gwy").mkdir(parents=True, exist_ok=True)
        shutil.copy2(jjfb, dst / "gwy" / "jjfb.mrp")
        rep.append(f"dsm_gm<=jjfb {jjfb}")
    else:
        rep.append("ERROR jjfb.mrp not found")
    for rel in ["sdk_key.dat", "gwy/sdk_key.dat", "gwy/jjfbol/sdk_key.dat", "240x320/sdk_key.dat", "240x320/gwy/sdk_key.dat"]:
        p = dst / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(SDK_KEY)
    rep.append("sdk_key=g:u2")
    log("direct_arg_patch_boot_prepare.txt", "\n".join(rep))

def boot():
    exe = find_main()
    if not exe:
        print("no main.exe")
        return
    prepare_jjfb_runtime()
    run_cmd(["cmd", "/c", "taskkill /IM main.exe /F"], timeout=10)
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    outp = LOGS / f"direct_arg_patch_boot_stdout_{ts}.txt"
    with outp.open("wb") as f:
        p = subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
    msg = f"started {exe}\npid={p.pid}\nstdout={outp}\nwaiting 120s\n"
    print(msg)
    log("direct_arg_patch_boot_launch.txt", msg)
    time.sleep(120)
    rc, ns = run_cmd(["cmd", "/c", "netstat -ano"], timeout=20)
    log(f"direct_arg_patch_boot_netstat_{ts}.txt", ns)

def restore():
    exe = find_main()
    if exe:
        for name in ["main.before_direct_arg_patch_v15.exe", "main.before_direct_call_hook_v14.exe", "main.before_iat_hook_v13.exe", "main.before_param_patch_v12.exe"]:
            bak = exe.with_name(name)
            if bak.exists():
                shutil.copy2(bak, exe)
                print(f"restored main.exe from {bak}")
                break
    if exe:
        dst = exe.parent / "mythroad"
        for name in ["dsm_gm.before_direct_arg_patch_v15.mrp", "dsm_gm.before_direct_call_hook_v14.mrp", "dsm_gm.before_iat_hook_v13.mrp", "dsm_gm.before_binary_patch_v12.mrp", "dsm_gm.before_fixed_key_v9.mrp"]:
            bak = dst / name
            if bak.exists():
                shutil.copy2(bak, dst / "dsm_gm.mrp")
                print(f"restored dsm_gm from {bak}")
                break

def collect():
    rc, tl = run_cmd(["cmd", "/c", "tasklist"], timeout=20)
    log("direct_arg_patch_tasklist.txt", tl)
    zipname = LOGS / f"direct_arg_patch_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname, "w", zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p, p.relative_to(ROOT))
    print("feedback:", zipname)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["patch", "boot", "collect", "restore"])
    args = ap.parse_args()
    {"patch": patch, "boot": boot, "collect": collect, "restore": restore}[args.cmd]()

if __name__ == "__main__":
    main()
