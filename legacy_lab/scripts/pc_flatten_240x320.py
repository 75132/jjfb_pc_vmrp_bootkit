#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, subprocess, time, shutil, zipfile, hashlib, csv, json, os
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"
LOGS.mkdir(exist_ok=True)

def log(name, text):
    (LOGS/name).write_text(text, encoding="utf-8", errors="replace")

def run(cmd, timeout=30, cwd=None):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"

def sha1(p):
    h=hashlib.sha1()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024), b''):
            h.update(b)
    return h.hexdigest()

def find_exes():
    roots = [ROOT/"runtime"/"vmrp_win32"]
    exes=[]
    for r in roots:
        if r.exists():
            for p in r.rglob("*.exe"):
                if p.name.lower() in ("main.exe","vmrp.exe") or "vmrp" in p.name.lower():
                    exes.append(p)
    exes.sort(key=lambda p:(0 if p.name.lower()=="main.exe" else 1, len(str(p))))
    return exes

def find_mythroad_roots():
    roots=[]
    for exe in find_exes():
        for d in [
            exe.parent/"mythroad",
            exe.parent/"fs"/"mythroad",
            exe.parent/"wasm"/"dist"/"fs"/"mythroad",
            exe.parent.parent/"mythroad",
            exe.parent.parent/"fs"/"mythroad",
            exe.parent.parent/"wasm"/"dist"/"fs"/"mythroad",
        ]:
            if d.parent.exists():
                roots.append(d)
    # also existing dirs
    for d in (ROOT/"runtime").rglob("mythroad"):
        if d.is_dir():
            roots.append(d)
    out=[]
    seen=set()
    for d in roots:
        s=str(d.resolve())
        if s not in seen:
            seen.add(s); out.append(d)
    return out

def copy_merge(src, dst):
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        target = dst / item.name
        if item.is_dir():
            if target.exists() and target.is_dir():
                copy_merge(item, target)
            else:
                if target.exists(): target.unlink()
                shutil.copytree(item, target)
        else:
            shutil.copy2(item, target)

def flatten():
    src_root = ROOT/"game_files"/"mythroad"
    src_240 = src_root/"240x320"
    report=[]
    report.append(f"src_root={src_root}")
    report.append(f"src_240_exists={src_240.exists()}")
    report.append(f"src_240_gwy_mrp={(src_240/'gwy.mrp').exists()}")
    report.append(f"src_240_jjfb={(src_240/'gwy'/'jjfb.mrp').exists()}")
    if not src_240.exists():
        report.append("ERROR: game_files/mythroad/240x320 不存在。你需要把原始 mythroad 放回 game_files/mythroad/")
        log("flatten_report.txt", "\n".join(report))
        print("\n".join(report))
        return

    roots = find_mythroad_roots()
    report.append("vmrp_mythroad_roots:")
    report += [f"  {r}" for r in roots]
    if not roots:
        report.append("ERROR: 没找到 vmrp mythroad 根目录。先运行原 bootkit 下载/解压 vmrp。")

    for dst in roots:
        dst.mkdir(parents=True, exist_ok=True)

        # 1. copy all 240x320 contents to root
        copy_merge(src_240, dst)
        report.append(f"merged 240x320 -> {dst}")

        # 2. ensure gwy dir exists at root
        if (src_240/"gwy").exists():
            copy_merge(src_240/"gwy", dst/"gwy")
            report.append(f"merged 240x320/gwy -> {dst/'gwy'}")

        # 3. create direct root aliases
        aliases = [
            (src_240/"gwy.mrp", dst/"gwy.mrp"),
            (src_240/"gwy"/"jjfb.mrp", dst/"jjfb.mrp"),
            (src_240/"gwy"/"gamelist.mrp", dst/"gamelist.mrp"),
        ]
        for s,t in aliases:
            if s.exists():
                shutil.copy2(s,t)
                report.append(f"alias {s.relative_to(src_root)} -> {t}")

        # 4. sdk keys
        for rel in ["sdk_key.dat", "gwy/sdk_key.dat"]:
            p = dst/rel
            p.parent.mkdir(parents=True, exist_ok=True)
            if not p.exists():
                p.write_text("123456789012345", encoding="ascii")
                report.append(f"created {p}")

        # 5. remove cached app.cfg so dsm_gm rescans
        for rel in ["app.cfg", "applist.mrp"]:
            p = dst/rel
            if p.exists():
                try:
                    p.unlink()
                    report.append(f"deleted cache {p}")
                except Exception as e:
                    report.append(f"failed delete {p}: {e!r}")

        # 6. root listing
        report.append(f"root mrp listing for {dst}:")
        for p in sorted(dst.glob("*.mrp")):
            report.append(f"  {p.name} {p.stat().st_size} sha1={sha1(p)[:10]}")

    log("flatten_report.txt", "\n".join(report))
    print("\n".join(report))

def launch():
    exes=find_exes()
    if not exes:
        print("ERROR: no vmrp exe found")
        log("flatten_launch.txt","ERROR: no exe")
        return
    exe=exes[0]
    ts=datetime.now().strftime("%Y%m%d_%H%M%S")
    outp=LOGS/f"vmrp_flatten_stdout_{ts}.txt"
    with outp.open("wb") as out:
        p=subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=out, stderr=subprocess.STDOUT)
    msg=f"launched {exe}\npid={p.pid}\nstdout={outp}\n请在 vmrp 窗口列表中点 gwy.mrp / gamelist.mrp / jjfb.mrp。\n"
    log("flatten_launch.txt", msg)
    print(msg)
    time.sleep(8)
    # capture netstat
    rc, ns = run(["cmd","/c","netstat -ano"], timeout=20)
    log(f"netstat_after_flatten_launch_{ts}.txt", ns)

def collect():
    # wait a little for user click
    time.sleep(3)
    rc, tl = run(["cmd","/c","tasklist"], timeout=20)
    log("tasklist_flatten.txt", tl)
    rc, ns = run(["cmd","/c","netstat -ano"], timeout=20)
    log(f"netstat_flatten_final_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt", ns)
    zipname=LOGS/f"flatten_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname, "w", zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p, p.relative_to(ROOT))
    print("feedback zip:", zipname)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["flatten","launch","collect"])
    args=ap.parse_args()
    globals()[args.cmd]()

if __name__=="__main__":
    main()
