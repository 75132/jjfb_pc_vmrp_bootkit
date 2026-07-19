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

IMAGE_SCN_MEM_EXECUTE = 0x20000000
IMAGE_SCN_MEM_READ = 0x40000000
IMAGE_SCN_MEM_WRITE = 0x80000000

def log(name, text):
    (LOGS/name).write_text(text, encoding="utf-8", errors="replace")

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
    h=hashlib.sha1()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024), b''): h.update(b)
    return h.hexdigest()

def find_main():
    roots = [ROOT/"runtime"/"vmrp_win32"]
    candidates=[]
    for r in roots:
        if r.exists():
            for p in r.rglob("main.exe"):
                # Ignore backups/alternate patched names
                if p.name.lower() == "main.exe":
                    candidates.append(p)
    candidates.sort(key=lambda p: len(str(p)))
    return candidates[0] if candidates else None

def parse_pe(data):
    if data[:2] != b"MZ":
        raise RuntimeError("not MZ")
    peoff = struct.unpack_from("<I", data, 0x3c)[0]
    if data[peoff:peoff+4] != b"PE\x00\x00":
        raise RuntimeError("not PE")
    machine, nsec, _, _, _, optsize, _ = struct.unpack_from("<HHIIIHH", data, peoff+4)
    opt = peoff + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x10b:
        raise RuntimeError(f"not PE32 magic=0x{magic:x}")
    image_base = struct.unpack_from("<I", data, opt+28)[0]
    # PE32 data directories start at opt + 96
    data_dir = opt + 96
    import_rva, import_size = struct.unpack_from("<II", data, data_dir + 8)  # index 1
    sec_off = opt + optsize
    secs=[]
    for i in range(nsec):
        o=sec_off+i*40
        name=data[o:o+8].rstrip(b"\x00").decode("ascii","replace")
        vs, va, rawsz, rawptr, ptrrel, ptrline, nrel, nline, chars = struct.unpack_from("<IIIIIIHHI", data, o+8)
        secs.append({
            "index":i, "hdr_off":o, "name":name, "vs":vs, "va":va, "rawsz":rawsz,
            "rawptr":rawptr, "chars":chars
        })
    return {"peoff":peoff, "image_base":image_base, "sections":secs, "import_rva":import_rva, "import_size":import_size}

def rva_to_off(rva, secs):
    for s in secs:
        size = max(s["rawsz"], s["vs"])
        if s["va"] <= rva < s["va"] + size:
            off = s["rawptr"] + (rva - s["va"])
            if 0 <= off < s["rawptr"] + s["rawsz"]:
                return off
    return None

def va_to_off(va, image_base, secs):
    return rva_to_off(va - image_base, secs)

def off_to_va(off, image_base, secs):
    for s in secs:
        if s["rawptr"] <= off < s["rawptr"] + s["rawsz"]:
            return image_base + s["va"] + (off - s["rawptr"])
    return None

def read_cstr(data, off):
    end = data.find(b"\x00", off)
    if end < 0:
        return b""
    return data[off:end]

def find_import_iat(data, pe, target_sub=b"bridge_dsm_mr_start_dsm"):
    secs=pe["sections"]; image_base=pe["image_base"]
    imp_off = rva_to_off(pe["import_rva"], secs)
    if imp_off is None:
        raise RuntimeError("import table not found")
    imports=[]
    i=0
    while True:
        desc_off = imp_off + i*20
        orig, tstamp, fwd, name_rva, first = struct.unpack_from("<IIIII", data, desc_off)
        if orig == 0 and name_rva == 0 and first == 0:
            break
        dll_off = rva_to_off(name_rva, secs)
        dll = read_cstr(data, dll_off).decode("ascii","replace") if dll_off is not None else "?"
        thunk_rva = orig if orig else first
        thunk_off = rva_to_off(thunk_rva, secs)
        idx=0
        while thunk_off is not None:
            val = struct.unpack_from("<I", data, thunk_off + idx*4)[0]
            if val == 0:
                break
            if val & 0x80000000:
                name = f"ordinal_{val & 0xffff}".encode()
            else:
                name_off = rva_to_off(val, secs)
                if name_off is None:
                    name = b"?"
                else:
                    name = read_cstr(data, name_off+2)
            iat_va = image_base + first + idx*4
            imports.append((dll, name, iat_va))
            if target_sub in name:
                return iat_va, dll, name, imports
            idx += 1
        i += 1
    return None, None, None, imports

def find_cave(data, pe, min_len):
    secs=pe["sections"]; image_base=pe["image_base"]
    # Prefer a cave in .text or .rdata; if .rdata is used, mark it executable.
    order=[]
    for pref in [".text", "text", ".rdata", "rdata", ".data", "data"]:
        order += [s for s in secs if s["name"].lower() == pref]
    order += secs
    seen=set()
    for s in order:
        key=(s["rawptr"],s["rawsz"])
        if key in seen: continue
        seen.add(key)
        rawptr=s["rawptr"]; rawsz=s["rawsz"]
        region = data[rawptr:rawptr+rawsz]
        for byte in [b"\x00", b"\xcc"]:
            pat = byte * min_len
            m = re.search(re.escape(pat), region)
            if m:
                off=rawptr+m.start()
                va=off_to_va(off, image_base, secs)
                return off, va, s
    return None, None, None

def section_for_off(off, secs):
    for s in secs:
        if s["rawptr"] <= off < s["rawptr"] + s["rawsz"]:
            return s
    return None

def patch():
    report=[]
    exe=find_main()
    if not exe:
        raise RuntimeError("main.exe not found")
    data=bytearray(exe.read_bytes())
    pe=parse_pe(data)
    image_base=pe["image_base"]; secs=pe["sections"]
    report.append(f"main={exe}")
    report.append(f"sha1_before={sha1_file(exe)} size={len(data)}")
    report.append(f"image_base=0x{image_base:x}")
    report.append("sections:\n"+"\n".join(f"{s['name']} hdr=0x{s['hdr_off']:x} va=0x{s['va']:x} raw=0x{s['rawptr']:x} rawsz=0x{s['rawsz']:x} chars=0x{s['chars']:x}" for s in secs))

    iat_va, dll, imp_name, imports = find_import_iat(data, pe)
    report.append(f"import target dll={dll} name={imp_name} iat_va={hex(iat_va) if iat_va else None}")
    if not iat_va:
        report.append("imports_tail:\n" + "\n".join(f"{dll} {name!r} {hex(va)}" for dll,name,va in imports[-100:]))
        raise RuntimeError("bridge_dsm_mr_start_dsm import not found")

    dsm_off=data.find(b"dsm_gm.mrp\x00")
    start_off=data.find(b"start.mr\x00")
    dsm_va=off_to_va(dsm_off,image_base,secs) if dsm_off>=0 else None
    start_va=off_to_va(start_off,image_base,secs) if start_off>=0 else None
    report.append(f"dsm_off=0x{dsm_off:x} dsm_va={hex(dsm_va) if dsm_va else None}")
    report.append(f"start_off=0x{start_off:x} start_va={hex(start_va) if start_va else None}")

    call_pat = b"\xff\x15" + struct.pack("<I", iat_va)
    call_offs = [m.start() for m in re.finditer(re.escape(call_pat), data)]
    report.append(f"call_pat={call_pat.hex()} call_offs={['0x%x'%x for x in call_offs]} count={len(call_offs)}")

    if not call_offs:
        # Sometimes call references IAT RVA rather than VA, or goes through an import stub.
        iat_rva = iat_va - image_base
        alt_pat = b"\xff\x15" + struct.pack("<I", iat_rva)
        alt_offs = [m.start() for m in re.finditer(re.escape(alt_pat), data)]
        report.append(f"alt_call_pat_rva={alt_pat.hex()} alt_call_offs={['0x%x'%x for x in alt_offs]} count={len(alt_offs)}")
        # Dump all ff15 calls around relevant area.
        all_ff15=[m.start() for m in re.finditer(b"\xff\x15....", data, flags=re.S)]
        report.append("all ff15 first 100:\n" + "\n".join(f"0x{o:x} -> {data[o:o+6].hex()}" for o in all_ff15[:100]))
        raise RuntimeError("no direct call [IAT] pattern found")

    # Choose best callsite: one near references to dsm/start VA if present; else the first.
    dsm_refs=[m.start() for m in re.finditer(re.escape(struct.pack("<I", dsm_va)), data)] if dsm_va else []
    start_refs=[m.start() for m in re.finditer(re.escape(struct.pack("<I", start_va)), data)] if start_va else []
    report.append(f"dsm_refs={['0x%x'%x for x in dsm_refs[:20]]} count={len(dsm_refs)}")
    report.append(f"start_refs={['0x%x'%x for x in start_refs[:20]]} count={len(start_refs)}")

    scored=[]
    for off in call_offs:
        score=0
        if any(abs(off-r) < 200 for r in dsm_refs): score += 10
        if any(abs(off-r) < 200 for r in start_refs): score += 10
        # Strong score if nearby bytes contain little endian string VAs directly.
        near=data[max(0,off-180):off+80]
        if dsm_va and struct.pack("<I", dsm_va) in near: score += 20
        if start_va and struct.pack("<I", start_va) in near: score += 20
        scored.append((score, off))
        report.append(f"callsite 0x{off:x} score={score} around={bytes(data[max(0,off-80):off+80]).hex()}")
    scored.sort(reverse=True)
    target_call = scored[0][1]
    target_call_va = off_to_va(target_call, image_base, secs)
    report.append(f"selected_call_off=0x{target_call:x} va=0x{target_call_va:x}")

    # Need wrapper + param in same cave.
    wrapper_len = 7 + 6
    min_len = wrapper_len + 16 + len(START_PARAM)
    cave_off, cave_va, cave_sec = find_cave(data, pe, min_len)
    if cave_off is None:
        raise RuntimeError("no cave for wrapper+param")
    param_off = cave_off + 32
    param_va = cave_va + 32
    report.append(f"cave_off=0x{cave_off:x} cave_va=0x{cave_va:x} section={cave_sec['name']} param_va=0x{param_va:x}")

    # Mark cave section executable+readable, writable not necessary but harmless for patched bytes.
    new_chars = cave_sec["chars"] | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ
    struct.pack_into("<I", data, cave_sec["hdr_off"] + 36, new_chars)
    report.append(f"section {cave_sec['name']} chars 0x{cave_sec['chars']:x} -> 0x{new_chars:x}")

    # wrapper:
    # C7 44 24 10 <param_va>     mov dword ptr [esp+0x10], param_va
    # FF 25 <iat_va>              jmp dword ptr [iat_va]
    wrapper = b"\xC7\x44\x24\x10" + struct.pack("<I", param_va) + b"\xFF\x25" + struct.pack("<I", iat_va)
    data[cave_off:cave_off+len(wrapper)] = wrapper
    data[param_off:param_off+len(START_PARAM)] = START_PARAM

    # Patch call [iat] (6 bytes) -> call rel32 wrapper (5 bytes) + nop
    rel = cave_va - (target_call_va + 5)
    if not -(2**31) <= rel < 2**31:
        raise RuntimeError("wrapper too far for rel32")
    data[target_call:target_call+6] = b"\xE8" + struct.pack("<i", rel) + b"\x90"
    report.append(f"patched call at 0x{target_call:x}: E8 {rel} NOP")
    report.append(f"wrapper bytes={wrapper.hex()}")
    report.append(f"param={START_PARAM_STR}")

    # Backup and write.
    bak = exe.with_name("main.before_iat_hook_v13.exe")
    if not bak.exists():
        shutil.copy2(exe, bak)
    patched = exe.with_name("main.iat_hook_v13.exe")
    patched.write_bytes(data)
    shutil.copy2(patched, exe)
    report.append(f"backup={bak}")
    report.append(f"patched={patched} sha1={sha1_file(patched)}")
    log("iat_hook_patch_report.txt", "\n".join(report))
    print("\n".join(report))

def copy_merge(src,dst):
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        t=dst/item.name
        if item.is_dir():
            if t.exists() and t.is_dir():
                copy_merge(item,t)
            else:
                if t.exists():
                    t.unlink()
                shutil.copytree(item,t)
        else:
            shutil.copy2(item,t)

def prepare_jjfb_runtime():
    exe=find_main()
    dst=exe.parent/"mythroad"
    rep=[]
    src_240=ROOT/"game_files"/"mythroad"/"240x320"
    if src_240.exists():
        copy_merge(src_240,dst)
        if (src_240/"gwy").exists():
            copy_merge(src_240/"gwy",dst/"gwy")
        rep.append(f"merged {src_240} -> {dst}")
    jjfb=next((p for p in [
        ROOT/"game_files"/"mythroad"/"240x320"/"gwy"/"jjfb.mrp",
        ROOT/"game_files"/"mythroad"/"gwy"/"jjfb.mrp",
        dst/"gwy"/"jjfb.mrp",
        dst/"jjfb.mrp",
    ] if p.exists()), None)
    if jjfb:
        dsm=dst/"dsm_gm.mrp"
        bak=dst/"dsm_gm.before_iat_hook_v13.mrp"
        if dsm.exists() and not bak.exists():
            shutil.copy2(dsm,bak)
        shutil.copy2(jjfb,dsm)
        shutil.copy2(jjfb,dst/"jjfb.mrp")
        (dst/"gwy").mkdir(parents=True, exist_ok=True)
        shutil.copy2(jjfb,dst/"gwy"/"jjfb.mrp")
        rep.append(f"dsm_gm<=jjfb {jjfb}")
    else:
        rep.append("ERROR jjfb.mrp not found")
    for rel in ["sdk_key.dat","gwy/sdk_key.dat","gwy/jjfbol/sdk_key.dat","240x320/sdk_key.dat","240x320/gwy/sdk_key.dat"]:
        p=dst/rel; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(SDK_KEY)
    rep.append("sdk_key=g:u2")
    log("iat_hook_boot_prepare.txt","\n".join(rep))

def boot():
    exe=find_main()
    if not exe:
        print("no main.exe")
        return
    prepare_jjfb_runtime()
    run_cmd(["cmd","/c","taskkill /IM main.exe /F"], timeout=10)
    ts=datetime.now().strftime("%Y%m%d_%H%M%S")
    outp=LOGS/f"iat_hook_boot_stdout_{ts}.txt"
    with outp.open("wb") as f:
        p=subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
    msg=f"started {exe}\npid={p.pid}\nstdout={outp}\nwaiting 120s\n"
    print(msg); log("iat_hook_boot_launch.txt", msg)
    time.sleep(120)
    rc,ns=run_cmd(["cmd","/c","netstat -ano"], timeout=20)
    log(f"iat_hook_boot_netstat_{ts}.txt", ns)

def restore():
    exe=find_main()
    if exe:
        for name in ["main.before_iat_hook_v13.exe", "main.before_param_patch_v12.exe"]:
            bak=exe.with_name(name)
            if bak.exists():
                shutil.copy2(bak, exe)
                print(f"restored main.exe from {bak}")
                break
    if exe:
        dst=exe.parent/"mythroad"
        for name in ["dsm_gm.before_iat_hook_v13.mrp", "dsm_gm.before_binary_patch_v12.mrp", "dsm_gm.before_fixed_key_v9.mrp"]:
            bak=dst/name
            if bak.exists():
                shutil.copy2(bak, dst/"dsm_gm.mrp")
                print(f"restored dsm_gm from {bak}")
                break

def collect():
    rc,tl=run_cmd(["cmd","/c","tasklist"], timeout=20)
    log("iat_hook_tasklist.txt", tl)
    zipname=LOGS/f"iat_hook_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname,"w",zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p,p.relative_to(ROOT))
    print("feedback:", zipname)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["patch","boot","collect","restore"])
    args=ap.parse_args()
    {"patch":patch, "boot":boot, "collect":collect, "restore":restore}[args.cmd]()

if __name__=="__main__":
    main()
