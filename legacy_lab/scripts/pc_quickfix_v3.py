#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, shutil, zipfile, hashlib, time, os
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
    return h.hexdigest()[:10]

def find_main():
    candidates=list((ROOT/"runtime"/"vmrp_win32").rglob("main.exe"))
    candidates.sort(key=lambda p: len(str(p)))
    return candidates[0] if candidates else None

def runtime_root():
    exe=find_main()
    if not exe: return None
    # 日志已证明 main.exe 使用 main.exe 所在目录下的 mythroad
    return exe.parent/"mythroad"

def copy_merge(src, dst):
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        t=dst/item.name
        if item.is_dir():
            if t.exists() and t.is_dir():
                copy_merge(item,t)
            else:
                if t.exists(): t.unlink()
                shutil.copytree(item,t)
        else:
            shutil.copy2(item,t)

def prepare():
    src=ROOT/"game_files"/"mythroad"/"240x320"
    dst=runtime_root()
    rep=[]
    rep.append(f"src={src}")
    rep.append(f"dst={dst}")
    if not src.exists():
        rep.append("ERROR: game_files/mythroad/240x320 不存在")
        print("\n".join(rep)); log("quickfix_prepare.txt","\n".join(rep)); return
    if not dst:
        rep.append("ERROR: 找不到 runtime/vmrp_win32/**/main.exe")
        print("\n".join(rep)); log("quickfix_prepare.txt","\n".join(rep)); return

    # 只处理真实运行目录，不再递归所有 mythroad
    copy_merge(src, dst)
    if (src/"gwy").exists():
        copy_merge(src/"gwy", dst/"gwy")

    # 前置入口
    aliases=[
        (src/"gwy"/"jjfb.mrp", dst/"000_jjfb.mrp"),
        (src/"gwy.mrp", dst/"001_gwy.mrp"),
        (src/"gwy"/"gamelist.mrp", dst/"002_gamelist.mrp"),
        (src/"gwy"/"jjfb.mrp", dst/"jjfb.mrp"),
        (src/"gwy.mrp", dst/"gwy.mrp"),
        (src/"gwy"/"gamelist.mrp", dst/"gamelist.mrp"),
    ]
    for s,t in aliases:
        if s.exists():
            shutil.copy2(s,t)
            rep.append(f"alias {s.relative_to(src)} -> {t.name} {t.stat().st_size} sha1={sha1(t)}")

    # sdk key
    for rel in ["sdk_key.dat","gwy/sdk_key.dat"]:
        p=dst/rel; p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("123456789012345", encoding="ascii")
        rep.append(f"wrote {p}")

    # 删除缓存，强制重新扫描
    for rel in ["app.cfg","applist.mrp"]:
        p=dst/rel
        if p.exists():
            try:
                p.unlink(); rep.append(f"deleted {rel}")
            except Exception as e:
                rep.append(f"delete {rel} failed {e!r}")

    rep.append("root mrp list:")
    for p in sorted(dst.glob("*.mrp"))[:80]:
        rep.append(f"  {p.name} size={p.stat().st_size} sha1={sha1(p)}")
    log("quickfix_prepare.txt","\n".join(rep))
    print("\n".join(rep))

def launch():
    exe=find_main()
    if not exe:
        print("no main.exe"); return
    ts=datetime.now().strftime("%Y%m%d_%H%M%S")
    out=LOGS/f"quickfix_vmrp_stdout_{ts}.txt"
    with out.open("wb") as f:
        p=subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
    msg=f"started {exe}\npid={p.pid}\nstdout={out}\n请在窗口列表最前面点 001_gwy.mrp / 002_gamelist.mrp / 000_jjfb.mrp\n"
    print(msg); log("quickfix_launch.txt", msg)
    time.sleep(12)
    try:
        ns=subprocess.run(["cmd","/c","netstat -ano"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", timeout=20).stdout
        log(f"quickfix_netstat_{ts}.txt", ns)
    except Exception as e:
        log("quickfix_netstat_error.txt", repr(e))

def collect():
    time.sleep(2)
    try:
        tl=subprocess.run(["cmd","/c","tasklist"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", timeout=20).stdout
        log("quickfix_tasklist.txt", tl)
    except Exception as e:
        log("quickfix_tasklist_error.txt", repr(e))
    zipname=LOGS/f"quickfix_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname,"w",zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p, p.relative_to(ROOT))
    print("feedback:", zipname)

if __name__=="__main__":
    import sys
    cmd=sys.argv[1] if len(sys.argv)>1 else "prepare"
    globals()[cmd]()
