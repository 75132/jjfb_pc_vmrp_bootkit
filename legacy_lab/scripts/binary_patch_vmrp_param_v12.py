#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, struct, re, shutil, zipfile, hashlib, subprocess, time
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"; LOGS.mkdir(exist_ok=True)
START_PARAM = b"napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink\x00"
SDK_KEY = b"g:u2"

def log(name, text):
    (LOGS/name).write_text(text, encoding="utf-8", errors="replace")

def sha1(p):
    h=hashlib.sha1()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024), b''): h.update(b)
    return h.hexdigest()

def find_main():
    c=list((ROOT/"runtime"/"vmrp_win32").rglob("main.exe"))
    c=[p for p in c if "original" not in p.name.lower()]
    c.sort(key=lambda p: len(str(p)))
    return c[0] if c else None

def parse_pe(data):
    if data[:2] != b"MZ":
        raise RuntimeError("not PE")
    peoff = struct.unpack_from("<I", data, 0x3c)[0]
    if data[peoff:peoff+4] != b"PE\x00\x00":
        raise RuntimeError("bad PE")
    machine, nsec, _, _, _, optsize, _ = struct.unpack_from("<HHIIIHH", data, peoff+4)
    opt = peoff + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x10b:
        raise RuntimeError(f"not PE32 magic={magic:x}")
    image_base = struct.unpack_from("<I", data, opt+28)[0]
    sec_off = opt + optsize
    secs=[]
    for i in range(nsec):
        o=sec_off+i*40
        name=data[o:o+8].rstrip(b"\x00").decode("ascii","replace")
        vs, va, rawsz, rawptr, _, _, _, _, chars = struct.unpack_from("<IIIIIIHHI", data, o+8)
        secs.append({"name":name,"vs":vs,"va":va,"rawsz":rawsz,"rawptr":rawptr,"chars":chars})
    return image_base, secs

def off_to_va(off, image_base, secs):
    for s in secs:
        if s["rawptr"] <= off < s["rawptr"] + s["rawsz"]:
            return image_base + s["va"] + (off - s["rawptr"])
    return None

def va_to_off(va, image_base, secs):
    rva = va - image_base
    for s in secs:
        size=max(s["rawsz"], s["vs"])
        if s["va"] <= rva < s["va"] + size:
            off=s["rawptr"] + (rva - s["va"])
            if off < s["rawptr"] + s["rawsz"]:
                return off
    return None

def find_code_cave(data, image_base, secs, size):
    # Prefer .rdata/.data caves, then any raw section.
    prefer = [s for s in secs if s["name"].lower() in [".rdata",".data","rdata","data"]]
    prefer += secs
    for s in prefer:
        rawptr=s["rawptr"]; rawsz=s["rawsz"]
        region=data[rawptr:rawptr+rawsz]
        # find zeros not too close to start
        for m in re.finditer(b"\x00" * size, region):
            off=rawptr+m.start()
            va=off_to_va(off,image_base,secs)
            if va:
                return off,va,s["name"]
    return None,None,None

def patch_binary():
    report=[]
    exe=find_main()
    if not exe:
        raise RuntimeError("main.exe not found")
    data=bytearray(exe.read_bytes())
    report.append(f"main={exe}")
    report.append(f"sha1_before={sha1(exe)} size={len(data)}")
    image_base,secs=parse_pe(data)
    report.append(f"image_base=0x{image_base:x}")
    report.append("sections:\n"+"\n".join(f"{s['name']} va=0x{s['va']:x} raw=0x{s['rawptr']:x} rawsz=0x{s['rawsz']:x}" for s in secs))

    dsm_off=data.find(b"dsm_gm.mrp\x00")
    start_off=data.find(b"start.mr\x00")
    fmt_off=data.find(b"bridge_dsm_mr_start_dsm('%s','%s',NULL)")
    if dsm_off < 0 or start_off < 0:
        raise RuntimeError("required strings not found")
    dsm_va=off_to_va(dsm_off,image_base,secs)
    start_va=off_to_va(start_off,image_base,secs)
    report.append(f"dsm off=0x{dsm_off:x} va=0x{dsm_va:x}")
    report.append(f"start off=0x{start_off:x} va=0x{start_va:x}")
    report.append(f"fmt off=0x{fmt_off:x}" if fmt_off>=0 else "fmt not found")

    # Write param into cave.
    cave_off,cave_va,cave_sec=find_code_cave(data,image_base,secs,len(START_PARAM)+8)
    if cave_off is None:
        raise RuntimeError("no code cave for start param")
    data[cave_off:cave_off+len(START_PARAM)] = START_PARAM
    report.append(f"param cave off=0x{cave_off:x} va=0x{cave_va:x} sec={cave_sec} len={len(START_PARAM)}")

    dsm_refs=[m.start() for m in re.finditer(re.escape(struct.pack("<I", dsm_va)), data)]
    start_refs=[m.start() for m in re.finditer(re.escape(struct.pack("<I", start_va)), data)]
    report.append(f"dsm_refs={['0x%x'%x for x in dsm_refs[:20]]} count={len(dsm_refs)}")
    report.append(f"start_refs={['0x%x'%x for x in start_refs[:20]]} count={len(start_refs)}")

    patched_sites=[]
    # Inspect windows around references. Find c7 44 24 xx 00 00 00 00 near both refs.
    candidate_windows=set()
    for r in dsm_refs+start_refs:
        candidate_windows.add(max(0,r-96))
    for wstart in candidate_windows:
        wend=min(len(data),wstart+240)
        window=data[wstart:wend]
        # candidate zero imm patterns: c7 44 24 ?? 00000000 / c7 04 24 00000000 / 6a00
        for m in re.finditer(b"\xc7\x44\x24(.)\x00\x00\x00\x00", window, flags=re.S):
            off=wstart+m.start()
            # require dsm/start ref within +-120
            if any(abs(off-r)<140 for r in dsm_refs) and any(abs(off-r)<140 for r in start_refs):
                data[off+4:off+8]=struct.pack("<I",cave_va)
                patched_sites.append((off,"mov_dword_ptr_esp_disp_imm32"))
        for m in re.finditer(b"\xc7\x04\x24\x00\x00\x00\x00", window):
            off=wstart+m.start()
            if any(abs(off-r)<140 for r in dsm_refs) and any(abs(off-r)<140 for r in start_refs):
                data[off+3:off+7]=struct.pack("<I",cave_va)
                patched_sites.append((off,"mov_dword_ptr_esp_imm32"))
        # Detect short push 0
        for m in re.finditer(b"\x6a\x00", window):
            off=wstart+m.start()
            if any(abs(off-r)<80 for r in dsm_refs) and any(abs(off-r)<80 for r in start_refs):
                report.append(f"FOUND short push0 near callsite at 0x{off:x}; cannot safely expand in-place")

    if not patched_sites:
        # Try replacing a literal 4-byte zero immediately before start/dsm refs in stack-arg setup.
        # Report surrounding bytes for next manual patch.
        for r in dsm_refs[:5]+start_refs[:5]:
            report.append(f"around ref 0x{r:x}: {bytes(data[max(0,r-64):r+96]).hex()}")
        raise RuntimeError("no safe patchable NULL argument pattern found")

    report.append("patched_sites=" + repr([(hex(o),t) for o,t in patched_sites]))

    bak=exe.with_name("main.before_param_patch_v12.exe")
    if not bak.exists():
        shutil.copy2(exe,bak)
    patched=exe.with_name("main.param_patch_v12.exe")
    patched.write_bytes(data)
    shutil.copy2(patched, exe)
    report.append(f"backup={bak}")
    report.append(f"patched={patched} sha1={sha1(patched)}")
    log("binary_patch_v12_report.txt","\n".join(report))
    print("\n".join(report))

def prepare_jjfb_runtime():
    dst = find_main().parent / "mythroad"
    rep=[]
    # copy game files if available
    src_240=ROOT/"game_files"/"mythroad"/"240x320"
    def copy_merge(src,dst):
        dst.mkdir(parents=True, exist_ok=True)
        for item in src.iterdir():
            t=dst/item.name
            if item.is_dir():
                if t.exists() and t.is_dir(): copy_merge(item,t)
                else:
                    if t.exists(): t.unlink()
                    shutil.copytree(item,t)
            else: shutil.copy2(item,t)
    if src_240.exists():
        copy_merge(src_240,dst)
        if (src_240/"gwy").exists(): copy_merge(src_240/"gwy",dst/"gwy")
    jjfb = next((p for p in [
        ROOT/"game_files"/"mythroad"/"240x320"/"gwy"/"jjfb.mrp",
        dst/"gwy"/"jjfb.mrp",
        dst/"jjfb.mrp",
    ] if p.exists()), None)
    if jjfb:
        dsm=dst/"dsm_gm.mrp"
        bak=dst/"dsm_gm.before_binary_patch_v12.mrp"
        if dsm.exists() and not bak.exists():
            shutil.copy2(dsm,bak)
        shutil.copy2(jjfb,dsm)
        shutil.copy2(jjfb,dst/"jjfb.mrp")
        (dst/"gwy").mkdir(exist_ok=True)
        shutil.copy2(jjfb,dst/"gwy"/"jjfb.mrp")
        rep.append(f"dsm_gm<=jjfb {jjfb}")
    for rel in ["sdk_key.dat","gwy/sdk_key.dat","gwy/jjfbol/sdk_key.dat","240x320/sdk_key.dat","240x320/gwy/sdk_key.dat"]:
        p=dst/rel; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(SDK_KEY)
    rep.append("sdk_key=g:u2")
    log("binary_patch_boot_prepare.txt","\n".join(rep))

def boot():
    exe=find_main()
    if not exe:
        print("no main.exe")
        return
    prepare_jjfb_runtime()
    run_cmd(["cmd","/c","taskkill /IM main.exe /F"],timeout=10)
    ts=datetime.now().strftime("%Y%m%d_%H%M%S")
    outp=LOGS/f"binary_patch_boot_stdout_{ts}.txt"
    with outp.open("wb") as f:
        p=subprocess.Popen([str(exe)],cwd=str(exe.parent),stdout=f,stderr=subprocess.STDOUT)
    msg=f"started patched main {exe}\npid={p.pid}\nstdout={outp}\nwaiting 90s\n"
    print(msg); log("binary_patch_boot_launch.txt",msg)
    time.sleep(90)
    rc,ns=run_cmd(["cmd","/c","netstat -ano"],timeout=20)
    log(f"binary_patch_boot_netstat_{ts}.txt",ns)

def run_cmd(cmd, timeout=30, cwd=None):
    try:
        p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding="utf-8",errors="replace",timeout=timeout,cwd=cwd)
        return p.returncode,p.stdout
    except subprocess.TimeoutExpired as e:
        return 124,(e.stdout or "")+"\n[TIMEOUT]\n"

def restore():
    exe=find_main()
    if exe:
        bak=exe.with_name("main.before_param_patch_v12.exe")
        if bak.exists():
            shutil.copy2(bak,exe)
            print(f"restored {exe} from {bak}")
    dst = exe.parent/"mythroad" if exe else None
    if dst:
        bak=dst/"dsm_gm.before_binary_patch_v12.mrp"
        if bak.exists():
            shutil.copy2(bak,dst/"dsm_gm.mrp")
            print("restored dsm_gm")

def collect():
    rc,tl=run_cmd(["cmd","/c","tasklist"],timeout=20)
    log("binary_patch_tasklist.txt",tl)
    zipname=LOGS/f"binary_patch_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname,"w",zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p,p.relative_to(ROOT))
    print("feedback:",zipname)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("cmd",choices=["patch","boot","collect","restore"])
    args=ap.parse_args()
    {"patch":patch_binary,"boot":boot,"collect":collect,"restore":restore}[args.cmd]()

if __name__=="__main__":
    import argparse
    main()

