#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, struct, re, shutil, zipfile, hashlib, subprocess, time, json
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"; LOGS.mkdir(exist_ok=True)

PARAM_TAG = "cfg_exact_type12_live"
START_PARAM_STR = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
START_PARAM = START_PARAM_STR.encode("ascii") + b"\x00"
SDK_KEY = b"g:u2"

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
        for b in iter(lambda:f.read(1024*1024), b""):
            h.update(b)
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

def restore_clean_main():
    exe=find_main()
    if not exe:
        return None
    for name in [
        "main.before_live_boot_v17.exe",
        "main.before_direct_arg_patch_v15.exe",
        "main.before_direct_call_hook_v14.exe",
        "main.before_iat_hook_v13.exe",
        "main.before_param_patch_v12.exe",
    ]:
        bak=exe.with_name(name)
        if bak.exists():
            shutil.copy2(bak, exe)
            return exe
    return exe

def parse_pe(data):
    if data[:2] != b"MZ":
        raise RuntimeError("not MZ")
    peoff=struct.unpack_from("<I", data, 0x3c)[0]
    if data[peoff:peoff+4] != b"PE\x00\x00":
        raise RuntimeError("not PE")
    machine,nsec,timedate,ptrsym,numsym,optsize,chars=struct.unpack_from("<HHIIIHH", data, peoff+4)
    opt=peoff+24
    if struct.unpack_from("<H",data,opt)[0] != 0x10b:
        raise RuntimeError("not PE32")
    image_base=struct.unpack_from("<I",data,opt+28)[0]
    sec_off=opt+optsize
    secs=[]
    for i in range(nsec):
        o=sec_off+i*40
        name=data[o:o+8].rstrip(b"\x00").decode("ascii","replace")
        vs,va,rawsz,rawptr,ptrrel,ptrline,nrel,nline,schars=struct.unpack_from("<IIIIIIHHI", data, o+8)
        secs.append({"index":i+1,"hdr_off":o,"name":name,"vs":vs,"va":va,"rawsz":rawsz,"rawptr":rawptr,"chars":schars})
    return {"ptrsym":ptrsym,"numsym":numsym,"image_base":image_base,"sections":secs}

def off_to_va(off, image_base, secs):
    for s in secs:
        if s["rawptr"] <= off < s["rawptr"] + s["rawsz"]:
            return image_base + s["va"] + (off - s["rawptr"])
    return None

def coff_name(data, sym_off, str_base, str_size):
    raw=data[sym_off:sym_off+8]
    zero,off=struct.unpack_from("<II", raw, 0)
    if zero == 0 and off != 0:
        if 0 < off < str_size:
            p=str_base+off
            end=data.find(b"\x00", p, str_base+str_size)
            if end < 0: end=str_base+str_size
            return data[p:end].decode("ascii","replace")
        return f"<bad_str_{off:x}>"
    return raw.rstrip(b"\x00").decode("ascii","replace")

def parse_coff_symbols(data, pe):
    ptr=pe["ptrsym"]; n=pe["numsym"]
    syms=[]
    if not ptr or not n: return syms
    str_base=ptr+n*18
    str_size=0
    if str_base+4 <= len(data):
        str_size=struct.unpack_from("<I", data, str_base)[0]
        if str_size < 4 or str_base+str_size > len(data): str_size=0
    i=0
    while i<n:
        o=ptr+i*18
        if o+18 > len(data): break
        name=coff_name(data,o,str_base,str_size) if str_size else data[o:o+8].rstrip(b"\x00").decode("ascii","replace")
        value,secnum,typ,storage,aux=struct.unpack_from("<IhHBB", data, o+8)
        va=None
        if 0 < secnum <= len(pe["sections"]):
            sec=pe["sections"][secnum-1]
            va=pe["image_base"]+sec["va"]+value
        syms.append({"name":name,"secnum":secnum,"va":va,"aux":aux})
        i += 1 + aux
    return syms

def find_bridge(syms):
    for nm in ["_bridge_dsm_mr_start_dsm","bridge_dsm_mr_start_dsm"]:
        for s in syms:
            if s["name"] == nm and s["va"]: return s
    for s in syms:
        if "bridge_dsm_mr_start_dsm" in s["name"] and s["va"]: return s
    return None

def scan_calls_to(data, pe, target_va):
    calls=[]
    for s in pe["sections"]:
        if not (s["chars"] & 0x20000000) and s["name"].lower() not in [".text","text"]:
            continue
        start=s["rawptr"]; end=s["rawptr"]+s["rawsz"]
        for m in re.finditer(b"\xE8....", data[start:end], flags=re.S):
            off=start+m.start()
            call_va=off_to_va(off, pe["image_base"], pe["sections"])
            if call_va is None: continue
            rel=struct.unpack_from("<i", data, off+1)[0]
            if call_va+5+rel == target_va:
                calls.append(off)
    return calls

def find_cave(data, pe, min_len):
    order=[]
    for pref in [".rdata","rdata",".data","data",".text","text"]:
        order += [s for s in pe["sections"] if s["name"].lower()==pref]
    order += pe["sections"]
    seen=set()
    for s in order:
        key=(s["rawptr"],s["rawsz"])
        if key in seen: continue
        seen.add(key)
        region=data[s["rawptr"]:s["rawptr"]+s["rawsz"]]
        for byte in [b"\x00", b"\xcc"]:
            m=re.search(re.escape(byte * min_len), region)
            if m:
                off=s["rawptr"]+m.start()
                va=off_to_va(off, pe["image_base"], pe["sections"])
                if va:
                    return off,va,s
    return None,None,None

def patch_param():
    exe=restore_clean_main()
    if not exe:
        raise RuntimeError("main.exe not found")
    # Save a dedicated clean backup for v17 after restoring from older backups.
    bak=exe.with_name("main.before_live_boot_v17.exe")
    if not bak.exists():
        shutil.copy2(exe,bak)

    data=bytearray(exe.read_bytes())
    pe=parse_pe(data)
    syms=parse_coff_symbols(data, pe)
    bridge=find_bridge(syms)
    if not bridge: raise RuntimeError("bridge symbol not found")
    calls=scan_calls_to(data, pe, bridge["va"])
    if not calls: raise RuntimeError("direct call not found")
    target_call=calls[0]

    cave_off,param_va,cave_sec=find_cave(data, pe, len(START_PARAM)+8)
    if cave_off is None: raise RuntimeError("no cave")
    data[cave_off:cave_off+len(START_PARAM)] = START_PARAM

    win_start=max(0,target_call-160)
    win=data[win_start:target_call]
    idx=win.rfind(b"\xC7\x44\x24\x0C\x00\x00\x00\x00")
    if idx < 0:
        idx=win.rfind(b"\xC7\x44\x24\x10\x00\x00\x00\x00")
    if idx < 0:
        ms=list(re.finditer(b"\xC7\x44\x24.\x00\x00\x00\x00", win, flags=re.S))
        if not ms: raise RuntimeError("arg pattern not found")
        idx=ms[-1].start()
    arg_off=win_start+idx
    old=bytes(data[arg_off:arg_off+8])
    data[arg_off+4:arg_off+8] = struct.pack("<I", param_va)
    new=bytes(data[arg_off:arg_off+8])

    marker=b"bridge_dsm_mr_start_dsm('%s','%s',NULL)"
    mo=data.find(marker)
    if mo >= 0:
        data[mo+marker.find(b"NULL"):mo+marker.find(b"NULL")+4] = b"LIVE"

    patched=exe.with_name("main.live_boot_v17.exe")
    patched.write_bytes(data)
    shutil.copy2(patched, exe)

    report = {
        "param": START_PARAM_STR,
        "param_va": hex(param_va),
        "target_call": hex(target_call),
        "arg_off": hex(arg_off),
        "old": old.hex(),
        "new": new.hex(),
        "patched_sha1": sha1_file(patched),
        "main": str(exe),
    }
    log("live_boot_patch_report.json", json.dumps(report, ensure_ascii=False, indent=2))
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return exe

def copy_merge(src,dst):
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        t=dst/item.name
        if item.is_dir():
            if t.exists() and t.is_dir(): copy_merge(item,t)
            else:
                if t.exists(): t.unlink()
                shutil.copytree(item,t)
        else:
            shutil.copy2(item,t)

def prepare_runtime(exe):
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
        bak=dst/"dsm_gm.before_live_boot_v17.mrp"
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
    log("live_boot_prepare.txt","\n".join(rep))

def collect_zip():
    rc,tl=run_cmd(["cmd","/c","tasklist"], timeout=20)
    log("live_boot_tasklist.txt", tl)
    zipname=LOGS/f"live_boot_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname,"w",zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p,p.relative_to(ROOT))
    print("feedback:", zipname)

def live():
    exe=patch_param()
    prepare_runtime(exe)
    run_cmd(["cmd","/c","taskkill /IM main.exe /F"], timeout=10)
    print("")
    print("Launching vmrp live. Watch the vmrp window. Do NOT close it during the 5 minute monitor.")
    print("Param:", START_PARAM_STR)
    print("")
    # Inherit stdout/stderr so console may show buffered output more naturally.
    p=subprocess.Popen([str(exe)], cwd=str(exe.parent))
    log("live_boot_launch.txt", f"pid={p.pid}\nexe={exe}\nparam={START_PARAM_STR}\n")
    # monitor 5 minutes, do not kill process.
    snapshots=[]
    for i in range(30):
        time.sleep(10)
        rc,ns=run_cmd(["cmd","/c","netstat -ano"], timeout=20)
        name=f"live_boot_netstat_{i:02d}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        log(name, ns)
        has = any(x in ns for x in ["20000","21002","211.155","111.1"])
        snapshots.append({"i":i,"has_target":has,"process_alive":p.poll() is None,"netstat":name})
        print(f"[{i:02d}] alive={p.poll() is None} target_net={has}")
        if has:
            print("Target network appeared. Keeping vmrp open and collecting logs.")
            break
    log("live_boot_monitor.json", json.dumps(snapshots, ensure_ascii=False, indent=2))
    collect_zip()
    print("Leave vmrp open if you still need screenshot. Close it manually when done.")

def restore():
    exe=find_main()
    if exe:
        for name in ["main.before_live_boot_v17.exe","main.before_direct_arg_patch_v15.exe","main.before_direct_call_hook_v14.exe","main.before_iat_hook_v13.exe","main.before_param_patch_v12.exe"]:
            bak=exe.with_name(name)
            if bak.exists():
                shutil.copy2(bak, exe)
                print(f"restored main.exe from {bak}")
                break
        dst=exe.parent/"mythroad"
        for name in ["dsm_gm.before_live_boot_v17.mrp","dsm_gm.before_param_variants_v16.mrp","dsm_gm.before_direct_arg_patch_v15.mrp","dsm_gm.before_fixed_key_v9.mrp"]:
            bak=dst/name
            if bak.exists():
                shutil.copy2(bak, dst/"dsm_gm.mrp")
                print(f"restored dsm_gm from {bak}")
                break

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["live","restore"])
    args=ap.parse_args()
    {"live":live,"restore":restore}[args.cmd]()

if __name__=="__main__":
    main()
