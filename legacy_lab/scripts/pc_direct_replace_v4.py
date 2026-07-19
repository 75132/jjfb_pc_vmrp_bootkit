#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, subprocess, shutil, zipfile, hashlib, time, os, json
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"; LOGS.mkdir(exist_ok=True)

def log(name, text):
    (LOGS/name).write_text(text, encoding="utf-8", errors="replace")

def run(cmd, timeout=30, cwd=None):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"

def sha1(p):
    h = hashlib.sha1()
    with open(p, 'rb') as f:
        for b in iter(lambda:f.read(1024*1024), b''): h.update(b)
    return h.hexdigest()[:12]

def find_main():
    candidates = list((ROOT/"runtime"/"vmrp_win32").rglob("main.exe"))
    candidates.sort(key=lambda p: len(str(p)))
    return candidates[0] if candidates else None

def vmrp_mythroad():
    exe = find_main()
    return exe.parent/"mythroad" if exe else None

def copy_merge(src, dst):
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        t = dst / item.name
        if item.is_dir():
            if t.exists() and t.is_dir():
                copy_merge(item,t)
            else:
                if t.exists(): t.unlink()
                shutil.copytree(item,t)
        else:
            shutil.copy2(item,t)

def ensure_flatten(dst, rep):
    src = ROOT/"game_files"/"mythroad"/"240x320"
    if not src.exists():
        rep.append("WARN: game_files/mythroad/240x320 not found; using existing runtime files")
        return
    copy_merge(src, dst)
    if (src/"gwy").exists():
        copy_merge(src/"gwy", dst/"gwy")
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
            rep.append(f"alias {t.name} size={t.stat().st_size} sha1={sha1(t)}")
    for rel in ["sdk_key.dat","gwy/sdk_key.dat"]:
        p = dst/rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("123456789012345", encoding="ascii")
        rep.append(f"wrote {p}")

def prepare(args):
    dst = vmrp_mythroad()
    rep=[f"mode={args.mode}", f"mythroad={dst}"]
    if not dst:
        rep.append("ERROR: main.exe not found")
        print("\n".join(rep)); log(f"direct_replace_prepare_{args.mode}.txt","\n".join(rep)); return
    dst.mkdir(parents=True, exist_ok=True)
    ensure_flatten(dst, rep)

    dsm = dst/"dsm_gm.mrp"
    backup = dst/"dsm_gm.original.mrp"
    if dsm.exists() and not backup.exists():
        shutil.copy2(dsm, backup)
        rep.append(f"backup original dsm_gm -> {backup.name} sha1={sha1(backup)}")

    src_map = {
        "gwy": dst/"001_gwy.mrp",
        "gamelist": dst/"002_gamelist.mrp",
        "jjfb": dst/"000_jjfb.mrp",
    }
    src = src_map[args.mode]
    if not src.exists():
        rep.append(f"ERROR: source not found: {src}")
    else:
        shutil.copy2(src, dsm)
        rep.append(f"REPLACED dsm_gm.mrp <= {src.name} size={dsm.stat().st_size} sha1={sha1(dsm)}")

    rep.append("root targets:")
    for name in ["dsm_gm.mrp","001_gwy.mrp","002_gamelist.mrp","000_jjfb.mrp","gwy.mrp","gamelist.mrp","jjfb.mrp"]:
        p=dst/name
        if p.exists(): rep.append(f"  {name} size={p.stat().st_size} sha1={sha1(p)}")
    out="\n".join(rep)
    print(out); log(f"direct_replace_prepare_{args.mode}.txt", out)

def launch(args):
    exe = find_main()
    if not exe:
        print("ERROR: main.exe not found"); return
    ts=datetime.now().strftime("%Y%m%d_%H%M%S")
    outp=LOGS/f"direct_replace_{args.mode}_stdout_{ts}.txt"
    with outp.open("wb") as f:
        p=subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
    msg=f"started {exe}\nmode={args.mode}\npid={p.pid}\nstdout={outp}\nWait 45s. If window opens, observe UI. Mock will log network if any.\n"
    print(msg); log(f"direct_replace_launch_{args.mode}.txt", msg)
    time.sleep(45)
    rc, ns = run(["cmd","/c","netstat -ano"], timeout=20)
    log(f"direct_replace_netstat_{args.mode}_{ts}.txt", ns)

def restore(args):
    dst=vmrp_mythroad()
    rep=[f"mythroad={dst}"]
    if not dst:
        rep.append("ERROR: main.exe not found")
    else:
        dsm=dst/"dsm_gm.mrp"; backup=dst/"dsm_gm.original.mrp"
        if backup.exists():
            shutil.copy2(backup,dsm)
            rep.append(f"restored dsm_gm.mrp sha1={sha1(dsm)}")
        else:
            rep.append("no backup found")
    print("\n".join(rep)); log("direct_replace_restore.txt","\n".join(rep))

def collect(args):
    rc, tl = run(["cmd","/c","tasklist"], timeout=20)
    log(f"direct_replace_tasklist_{args.mode}.txt", tl)
    zipname=LOGS/f"direct_replace_feedback_{args.mode}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname,"w",zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p, p.relative_to(ROOT))
    print("feedback:", zipname)

def main():
    ap=argparse.ArgumentParser()
    sub=ap.add_subparsers(dest="cmd", required=True)
    for c in ["prepare","launch","collect"]:
        sp=sub.add_parser(c); sp.add_argument("--mode", choices=["gwy","gamelist","jjfb"], default="gwy")
    sub.add_parser("restore")
    args=ap.parse_args()
    globals()[args.cmd](args)

if __name__=="__main__":
    main()
