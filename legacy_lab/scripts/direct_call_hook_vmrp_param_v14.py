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
    c=[]
    root=ROOT/"runtime"/"vmrp_win32"
    if root.exists():
        for p in root.rglob("main.exe"):
            if p.name.lower()=="main.exe":
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
    image_base = struct.unpack_from("<I", data, opt+28)[0]
    sec_off = opt + optsize
    secs=[]
    for i in range(nsec):
        o=sec_off+i*40
        name=data[o:o+8].rstrip(b"\x00").decode("ascii","replace")
        vs, va, rawsz, rawptr, ptrrel, ptrline, nrel, nline, schars = struct.unpack_from("<IIIIIIHHI", data, o+8)
        secs.append({"index":i+1, "hdr_off":o, "name":name, "vs":vs, "va":va, "rawsz":rawsz, "rawptr":rawptr, "chars":schars})
    return {"peoff":peoff, "machine":machine, "ptrsym":ptrsym, "numsym":numsym, "image_base":image_base, "sections":secs}

def rva_to_off(rva, secs):
    for s in secs:
        size=max(s["rawsz"], s["vs"])
        if s["va"] <= rva < s["va"] + size:
            off=s["rawptr"] + (rva - s["va"])
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

def coff_name(data, sym_off, str_base, str_size):
    raw=data[sym_off:sym_off+8]
    zero, off = struct.unpack_from("<II", raw, 0)
    if zero == 0 and off != 0:
        if off < str_size:
            p=str_base+off
            end=data.find(b"\x00", p, str_base+str_size)
            if end < 0: end=str_base+str_size
            return data[p:end].decode("ascii","replace")
        return f"<bad_str_{off:x}>"
    return raw.rstrip(b"\x00").decode("ascii","replace")

def parse_coff_symbols(data, pe):
    ptr=pe["ptrsym"]; n=pe["numsym"]
    syms=[]
    if ptr == 0 or n == 0:
        return syms
    str_base=ptr+n*18
    str_size=0
    if str_base+4 <= len(data):
        str_size=struct.unpack_from("<I", data, str_base)[0]
        if str_size < 4 or str_base+str_size > len(data):
            str_size=0
    i=0
    while i<n:
        o=ptr+i*18
        if o+18 > len(data): break
        name=coff_name(data,o,str_base,str_size) if str_size else data[o:o+8].rstrip(b"\x00").decode("ascii","replace")
        value, secnum, typ, storage, aux = struct.unpack_from("<IhHBB", data, o+8)
        va=None
        if secnum > 0 and secnum <= len(pe["sections"]):
            sec=pe["sections"][secnum-1]
            va=pe["image_base"] + sec["va"] + value
        syms.append({"name":name, "value":value, "secnum":secnum, "type":typ, "storage":storage, "aux":aux, "va":va, "sym_index":i})
        i += 1 + aux
    return syms

def find_symbol(syms, names):
    for name in names:
        for s in syms:
            if s["name"] == name and s["va"]:
                return s
    for name in names:
        for s in syms:
            if name.strip("_") in s["name"].strip("_") and s["va"]:
                return s
    return None

def find_cave(data, pe, min_len):
    secs=pe["sections"]
    order=[]
    # Prefer .text, then .rdata/.data. We can mark selected section executable.
    for pref in [".text","text",".rdata","rdata",".data","data"]:
        order += [s for s in secs if s["name"].lower()==pref]
    order += secs
    seen=set()
    for s in order:
        key=(s["rawptr"],s["rawsz"])
        if key in seen: continue
        seen.add(key)
        region=data[s["rawptr"]:s["rawptr"]+s["rawsz"]]
        for byte in [b"\x00", b"\xcc"]:
            # search long runs
            for m in re.finditer(re.escape(byte * min_len), region):
                off=s["rawptr"]+m.start()
                va=off_to_va(off, pe["image_base"], secs)
                if va:
                    return off, va, s
    return None, None, None

def scan_calls_to(data, pe, target_va):
    calls=[]
    secs=pe["sections"]
    # only scan executable-ish sections, but if flags missing, scan all.
    for s in secs:
        if not (s["chars"] & IMAGE_SCN_MEM_EXECUTE) and s["name"].lower() not in [".text","text"]:
            continue
        start=s["rawptr"]; end=s["rawptr"]+s["rawsz"]
        region=data[start:end]
        for m in re.finditer(b"\xE8....", region, flags=re.S):
            off=start+m.start()
            call_va=off_to_va(off, pe["image_base"], secs)
            if call_va is None:
                continue
            rel=struct.unpack_from("<i", data, off+1)[0]
            dest=call_va+5+rel
            if dest == target_va:
                calls.append(off)
    return calls

def patch():
    report=[]
    exe=find_main()
    if not exe:
        raise RuntimeError("main.exe not found")
    # Restore from previous v14 backup if main.exe already patched by v14.
    v14bak=exe.with_name("main.before_direct_call_hook_v14.exe")
    if v14bak.exists():
        shutil.copy2(v14bak, exe)
        report.append(f"restored clean main from existing {v14bak} before repatching")
    data=bytearray(exe.read_bytes())
    pe=parse_pe(data)
    syms=parse_coff_symbols(data, pe)
    report.append(f"main={exe}")
    report.append(f"sha1_before={sha1_file(exe)} size={len(data)}")
    report.append(f"image_base=0x{pe['image_base']:x} ptrsym=0x{pe['ptrsym']:x} numsym={pe['numsym']} syms={len(syms)}")
    report.append("sections:\n"+"\n".join(f"{s['index']} {s['name']} hdr=0x{s['hdr_off']:x} va=0x{s['va']:x} raw=0x{s['rawptr']:x} rawsz=0x{s['rawsz']:x} chars=0x{s['chars']:x}" for s in pe["sections"]))

    interesting=[s for s in syms if "bridge_dsm_mr_start_dsm" in s["name"] or "startVmrp" in s["name"] or s["name"] in ["_main","main"]]
    report.append("interesting symbols:\n" + "\n".join(f"{s['sym_index']} {s['name']} sec={s['secnum']} val=0x{s['value']:x} va={hex(s['va']) if s['va'] else None}" for s in interesting[:200]))

    bridge = find_symbol(syms, ["_bridge_dsm_mr_start_dsm", "bridge_dsm_mr_start_dsm"])
    if not bridge:
        log("direct_call_hook_patch_report.txt", "\n".join(report))
        raise RuntimeError("COFF symbol _bridge_dsm_mr_start_dsm not found")
    bridge_va=bridge["va"]
    report.append(f"bridge symbol selected: {bridge['name']} va=0x{bridge_va:x}")

    dsm_off=data.find(b"dsm_gm.mrp\x00")
    start_off=data.find(b"start.mr\x00")
    dsm_va=off_to_va(dsm_off, pe["image_base"], pe["sections"]) if dsm_off>=0 else None
    start_va=off_to_va(start_off, pe["image_base"], pe["sections"]) if start_off>=0 else None
    report.append(f"dsm_off=0x{dsm_off:x} dsm_va={hex(dsm_va) if dsm_va else None}")
    report.append(f"start_off=0x{start_off:x} start_va={hex(start_va) if start_va else None}")

    calls=scan_calls_to(data, pe, bridge_va)
    report.append(f"direct calls to bridge={['0x%x'%c for c in calls]} count={len(calls)}")
    if not calls:
        # Fallback: use all E8 near dsm/start refs and dump.
        all_calls=[]
        for m in re.finditer(b"\xE8....", data, flags=re.S):
            off=m.start()
            call_va=off_to_va(off, pe["image_base"], pe["sections"])
            if call_va:
                rel=struct.unpack_from("<i", data, off+1)[0]
                dest=call_va+5+rel
                if abs(dest-bridge_va) < 0x1000:
                    all_calls.append((off,dest))
        report.append("near bridge calls:\n"+"\n".join(f"off=0x{o:x} dest=0x{d:x}" for o,d in all_calls[:200]))
        log("direct_call_hook_patch_report.txt", "\n".join(report))
        raise RuntimeError("no direct E8 call to bridge symbol found")

    dsm_refs=[m.start() for m in re.finditer(re.escape(struct.pack("<I", dsm_va)), data)] if dsm_va else []
    start_refs=[m.start() for m in re.finditer(re.escape(struct.pack("<I", start_va)), data)] if start_va else []
    report.append(f"dsm_refs={['0x%x'%x for x in dsm_refs[:50]]}")
    report.append(f"start_refs={['0x%x'%x for x in start_refs[:50]]}")

    scored=[]
    for off in calls:
        near=data[max(0,off-220):off+120]
        score=0
        if dsm_va and struct.pack("<I", dsm_va) in near: score+=20
        if start_va and struct.pack("<I", start_va) in near: score+=20
        if any(abs(off-r)<250 for r in dsm_refs): score+=10
        if any(abs(off-r)<250 for r in start_refs): score+=10
        scored.append((score, off))
        report.append(f"callsite 0x{off:x} score={score} around={bytes(data[max(0,off-100):off+100]).hex()}")
    scored.sort(reverse=True)
    target_call=scored[0][1]
    target_call_va=off_to_va(target_call, pe["image_base"], pe["sections"])
    report.append(f"selected_call=0x{target_call:x} va=0x{target_call_va:x} score={scored[0][0]}")

    min_len=64+len(START_PARAM)
    cave_off,cave_va,cave_sec=find_cave(data, pe, min_len)
    if cave_off is None:
        log("direct_call_hook_patch_report.txt", "\n".join(report))
        raise RuntimeError("no cave found")
    param_off=cave_off+32
    param_va=cave_va+32
    report.append(f"cave_off=0x{cave_off:x} cave_va=0x{cave_va:x} section={cave_sec['name']} param_va=0x{param_va:x}")

    # Mark section executable+readable.
    old_chars=cave_sec["chars"]
    new_chars=old_chars | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ
    struct.pack_into("<I", data, cave_sec["hdr_off"]+36, new_chars)
    report.append(f"section chars {cave_sec['name']}: 0x{old_chars:x}->0x{new_chars:x}")

    # Wrapper:
    # mov dword ptr [esp+0x10], param_va
    # jmp bridge_va
    wrapper_va=cave_va
    jmp_rel=bridge_va - (wrapper_va + 8 + 5)
    wrapper = b"\xC7\x44\x24\x10" + struct.pack("<I", param_va) + b"\xE9" + struct.pack("<i", jmp_rel)
    data[cave_off:cave_off+len(wrapper)] = wrapper
    data[param_off:param_off+len(START_PARAM)] = START_PARAM

    call_rel=wrapper_va - (target_call_va + 5)
    if not -(2**31) <= call_rel < 2**31:
        log("direct_call_hook_patch_report.txt", "\n".join(report))
        raise RuntimeError("call rel out of range")
    data[target_call:target_call+5] = b"\xE8" + struct.pack("<i", call_rel)

    # Optional: change visible printf marker NULL -> HOOK, same length, so stdout proves patched exe ran.
    marker=b"bridge_dsm_mr_start_dsm('%s','%s',NULL)"
    mo=data.find(marker)
    if mo>=0:
        null_off=mo+marker.find(b"NULL")
        data[null_off:null_off+4] = b"HOOK"
        report.append(f"changed printf NULL marker to HOOK at 0x{null_off:x}")

    report.append(f"wrapper_va=0x{wrapper_va:x} wrapper_bytes={wrapper.hex()}")
    report.append(f"param={START_PARAM_STR}")

    bak=exe.with_name("main.before_direct_call_hook_v14.exe")
    if not bak.exists():
        shutil.copy2(exe, bak)
    patched=exe.with_name("main.direct_call_hook_v14.exe")
    patched.write_bytes(data)
    shutil.copy2(patched, exe)
    report.append(f"backup={bak}")
    report.append(f"patched={patched} sha1={sha1_file(patched)}")

    log("direct_call_hook_patch_report.txt", "\n".join(report))
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
        bak=dst/"dsm_gm.before_direct_call_hook_v14.mrp"
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
        p=dst/rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(SDK_KEY)
    rep.append("sdk_key=g:u2")
    log("direct_call_hook_boot_prepare.txt", "\n".join(rep))

def boot():
    exe=find_main()
    if not exe:
        print("no main.exe")
        return
    prepare_jjfb_runtime()
    run_cmd(["cmd","/c","taskkill /IM main.exe /F"], timeout=10)
    ts=datetime.now().strftime("%Y%m%d_%H%M%S")
    outp=LOGS/f"direct_call_hook_boot_stdout_{ts}.txt"
    with outp.open("wb") as f:
        p=subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
    msg=f"started {exe}\npid={p.pid}\nstdout={outp}\nwaiting 120s\n"
    print(msg); log("direct_call_hook_boot_launch.txt", msg)
    time.sleep(120)
    rc,ns=run_cmd(["cmd","/c","netstat -ano"], timeout=20)
    log(f"direct_call_hook_boot_netstat_{ts}.txt", ns)

def restore():
    exe=find_main()
    if exe:
        for name in ["main.before_direct_call_hook_v14.exe", "main.before_iat_hook_v13.exe", "main.before_param_patch_v12.exe"]:
            bak=exe.with_name(name)
            if bak.exists():
                shutil.copy2(bak, exe)
                print(f"restored main.exe from {bak}")
                break
    if exe:
        dst=exe.parent/"mythroad"
        for name in ["dsm_gm.before_direct_call_hook_v14.mrp", "dsm_gm.before_iat_hook_v13.mrp", "dsm_gm.before_binary_patch_v12.mrp", "dsm_gm.before_fixed_key_v9.mrp"]:
            bak=dst/name
            if bak.exists():
                shutil.copy2(bak, dst/"dsm_gm.mrp")
                print(f"restored dsm_gm from {bak}")
                break

def collect():
    rc,tl=run_cmd(["cmd","/c","tasklist"], timeout=20)
    log("direct_call_hook_tasklist.txt", tl)
    zipname=LOGS/f"direct_call_hook_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
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
