#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, subprocess, shutil, zipfile, hashlib, time, json, os
from pathlib import Path
from datetime import datetime

ROOT=Path(__file__).resolve().parents[1]
LOGS=ROOT/"logs"; LOGS.mkdir(exist_ok=True)

def log(name, text):
    (LOGS/name).write_text(text, encoding="utf-8", errors="replace")

def sha1(p):
    h=hashlib.sha1()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024), b''): h.update(b)
    return h.hexdigest()[:12]

def run(cmd, timeout=30, cwd=None):
    try:
        p=subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "")+"\n[TIMEOUT]\n"

def find_main():
    c=list((ROOT/"runtime"/"vmrp_win32").rglob("main.exe"))
    c.sort(key=lambda p: len(str(p)))
    return c[0] if c else None

def mythroad():
    exe=find_main()
    return exe.parent/"mythroad" if exe else None

def copy_merge(src,dst):
    dst.mkdir(parents=True,exist_ok=True)
    for item in src.iterdir():
        t=dst/item.name
        if item.is_dir():
            if t.exists() and t.is_dir(): copy_merge(item,t)
            else:
                if t.exists(): t.unlink()
                shutil.copytree(item,t)
        else:
            shutil.copy2(item,t)

def prepare(args=None):
    dst=mythroad()
    rep=[f"main={find_main()}", f"mythroad={dst}"]
    if not dst:
        rep.append("ERROR: no main.exe")
        print("\n".join(rep)); log("localnet_prepare.txt","\n".join(rep)); return
    src=ROOT/"game_files"/"mythroad"/"240x320"
    if src.exists():
        copy_merge(src,dst)
        if (src/"gwy").exists(): copy_merge(src/"gwy", dst/"gwy")
        aliases=[
            (src/"gwy.mrp", dst/"001_gwy.mrp"),
            (src/"gwy"/"gamelist.mrp", dst/"002_gamelist.mrp"),
            (src/"gwy"/"jjfb.mrp", dst/"000_jjfb.mrp"),
            (src/"gwy.mrp", dst/"gwy.mrp"),
            (src/"gwy"/"gamelist.mrp", dst/"gamelist.mrp"),
            (src/"gwy"/"jjfb.mrp", dst/"jjfb.mrp"),
        ]
        for s,t in aliases:
            if s.exists():
                shutil.copy2(s,t)
                rep.append(f"alias {t.name} <= {s.relative_to(src)} {t.stat().st_size} {sha1(t)}")
    for rel in ["sdk_key.dat","gwy/sdk_key.dat"]:
        p=dst/rel; p.parent.mkdir(parents=True,exist_ok=True); p.write_text("123456789012345",encoding="ascii")
        rep.append(f"wrote {p}")
    dsm=dst/"dsm_gm.mrp"; bak=dst/"dsm_gm.original.mrp"
    if dsm.exists() and not bak.exists():
        shutil.copy2(dsm,bak); rep.append(f"backup dsm_gm.original {sha1(bak)}")
    src_gwy=dst/"001_gwy.mrp"
    if src_gwy.exists():
        shutil.copy2(src_gwy,dsm); rep.append(f"REPLACED dsm_gm.mrp <= 001_gwy.mrp {dsm.stat().st_size} {sha1(dsm)}")
    else:
        rep.append("ERROR: 001_gwy.mrp missing")
    rep.append("targets:")
    for name in ["dsm_gm.mrp","001_gwy.mrp","002_gamelist.mrp","000_jjfb.mrp","gwy/cfg.bin"]:
        p=dst/name
        rep.append(f"{name}: exists={p.exists()} size={p.stat().st_size if p.exists() else ''} sha1={sha1(p) if p.exists() and p.is_file() else ''}")
    out="\n".join(rep)
    print(out); log("localnet_prepare.txt", out)

def launch(args=None):
    exe=find_main()
    if not exe:
        print("no main.exe"); return
    ts=datetime.now().strftime("%Y%m%d_%H%M%S")
    outp=LOGS/f"localnet_vmrp_stdout_{ts}.txt"
    with outp.open("wb") as f:
        p=subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
    msg=f"started {exe}\npid={p.pid}\nstdout={outp}\nwaiting 90s for network...\n"
    print(msg); log("localnet_launch.txt", msg)
    time.sleep(90)
    rc,ns=run(["cmd","/c","netstat -ano"],timeout=20)
    log(f"localnet_netstat_{ts}.txt", ns)

def restore(args=None):
    dst=mythroad()
    rep=[f"mythroad={dst}"]
    if dst:
        bak=dst/"dsm_gm.original.mrp"; dsm=dst/"dsm_gm.mrp"
        if bak.exists():
            shutil.copy2(bak,dsm); rep.append(f"restored dsm_gm.mrp {sha1(dsm)}")
        else:
            rep.append("no backup")
    print("\n".join(rep)); log("localnet_restore_dsm.txt","\n".join(rep))

def collect(args=None):
    rc,tl=run(["cmd","/c","tasklist"],timeout=20)
    log("localnet_tasklist.txt", tl)
    zipname=LOGS/f"localnet_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname,"w",zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p,p.relative_to(ROOT))
    print("feedback:", zipname)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["prepare","launch","collect","restore"])
    args=ap.parse_args()
    globals()[args.cmd](args)

if __name__=="__main__":
    main()
