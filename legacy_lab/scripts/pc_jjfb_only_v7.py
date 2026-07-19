#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, subprocess, shutil, zipfile, hashlib, time, os
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"
LOGS.mkdir(exist_ok=True)

def log(name, text):
    (LOGS / name).write_text(text, encoding="utf-8", errors="replace")

def run(cmd, timeout=30, cwd=None):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, encoding="utf-8", errors="replace",
                           timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"

def sha1(p):
    h = hashlib.sha1()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1024*1024), b""):
            h.update(b)
    return h.hexdigest()[:12]

def find_main():
    roots = [ROOT / "runtime" / "vmrp_win32"]
    candidates = []
    for r in roots:
        if r.exists():
            candidates += list(r.rglob("main.exe"))
    candidates.sort(key=lambda p: len(str(p)))
    return candidates[0] if candidates else None

def mythroad_root():
    exe = find_main()
    return exe.parent / "mythroad" if exe else None

def find_jjfb_sources():
    candidates = [
        ROOT / "game_files" / "mythroad" / "240x320" / "gwy" / "jjfb.mrp",
        ROOT / "game_files" / "mythroad" / "gwy" / "jjfb.mrp",
        ROOT / "game_files" / "mythroad" / "jjfb.mrp",
    ]
    rt = mythroad_root()
    if rt:
        candidates += [
            rt / "gwy" / "jjfb.mrp",
            rt / "jjfb.mrp",
            rt / "000_jjfb.mrp",
        ]
    return [p for p in candidates if p.exists()]

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

def maybe_copy_extra_exts(dst, rep):
    # Direct jjfb may search these near cwd or gwy/.
    names = ["mrc_loader.ext", "robotol.ext", "cfunction.ext"]
    search_roots = [
        ROOT / "game_files" / "mythroad",
        dst,
    ]
    found = {}
    for sr in search_roots:
        if sr.exists():
            for name in names:
                for p in sr.rglob(name):
                    found.setdefault(name, p)
    for name, src in found.items():
        for rel in [name, "gwy/" + name, "gwy/jjfbol/" + name]:
            t = dst / rel
            t.parent.mkdir(parents=True, exist_ok=True)
            try:
                shutil.copy2(src, t)
                rep.append(f"extra {name}: {src} -> {t}")
            except Exception as e:
                rep.append(f"extra copy failed {src} -> {t}: {e!r}")

def prepare(args=None):
    rep = []
    exe = find_main()
    dst = mythroad_root()
    rep.append(f"main={exe}")
    rep.append(f"mythroad={dst}")
    if not exe or not dst:
        rep.append("ERROR: main.exe or mythroad root not found. Run original PC bootkit first.")
        out = "\n".join(rep)
        print(out); log("jjfb_only_prepare.txt", out); return

    src_240 = ROOT / "game_files" / "mythroad" / "240x320"
    src_root = ROOT / "game_files" / "mythroad"
    if src_240.exists():
        copy_merge(src_240, dst)
        rep.append(f"merged 240x320 -> {dst}")
        if (src_240 / "gwy").exists():
            copy_merge(src_240 / "gwy", dst / "gwy")
            rep.append(f"merged 240x320/gwy -> {dst/'gwy'}")
    elif src_root.exists():
        copy_merge(src_root, dst)
        rep.append(f"merged mythroad -> {dst}")

    jjfbs = find_jjfb_sources()
    rep.append("jjfb_sources:")
    for p in jjfbs:
        rep.append(f"  {p} size={p.stat().st_size} sha1={sha1(p)}")
    if not jjfbs:
        rep.append("ERROR: jjfb.mrp not found")
        out = "\n".join(rep)
        print(out); log("jjfb_only_prepare.txt", out); return

    jjfb = jjfbs[0]
    dsm = dst / "dsm_gm.mrp"
    bak = dst / "dsm_gm.before_jjfb_only.mrp"
    if dsm.exists() and not bak.exists():
        shutil.copy2(dsm, bak)
        rep.append(f"backup original dsm_gm -> {bak.name} sha1={sha1(bak)}")

    # Direct replace.
    shutil.copy2(jjfb, dsm)
    shutil.copy2(jjfb, dst / "jjfb.mrp")
    (dst / "gwy").mkdir(parents=True, exist_ok=True)
    shutil.copy2(jjfb, dst / "gwy" / "jjfb.mrp")
    rep.append(f"REPLACED dsm_gm.mrp <= jjfb.mrp size={dsm.stat().st_size} sha1={sha1(dsm)}")

    # sdk keys in multiple plausible locations.
    for rel in [
        "sdk_key.dat",
        "gwy/sdk_key.dat",
        "gwy/jjfbol/sdk_key.dat",
        "240x320/sdk_key.dat",
        "240x320/gwy/sdk_key.dat",
    ]:
        p = dst / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("123456789012345", encoding="ascii")
        rep.append(f"wrote {p}")

    maybe_copy_extra_exts(dst, rep)

    # Remove list cache only; not relevant to direct start but avoids stale launcher if restore.
    for rel in ["app.cfg", "applist.mrp"]:
        p = dst / rel
        if p.exists():
            try:
                p.unlink()
                rep.append(f"deleted cache {p}")
            except Exception as e:
                rep.append(f"delete cache failed {p}: {e!r}")

    rep.append("final key files:")
    for rel in [
        "dsm_gm.mrp",
        "jjfb.mrp",
        "gwy/jjfb.mrp",
        "sdk_key.dat",
        "gwy/sdk_key.dat",
        "mrc_loader.ext",
        "robotol.ext",
        "gwy/mrc_loader.ext",
        "gwy/robotol.ext",
    ]:
        p = dst / rel
        rep.append(f"  {rel}: exists={p.exists()} size={p.stat().st_size if p.exists() and p.is_file() else ''} sha1={sha1(p) if p.exists() and p.is_file() else ''}")

    out = "\n".join(rep)
    print(out)
    log("jjfb_only_prepare.txt", out)

def launch(args=None):
    exe = find_main()
    if not exe:
        print("ERROR: main.exe not found")
        return
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    outp = LOGS / f"jjfb_only_vmrp_stdout_{ts}.txt"
    with outp.open("wb") as f:
        p = subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
    msg = f"started {exe}\npid={p.pid}\nstdout={outp}\nwaiting 60s for jjfb direct boot...\n"
    print(msg)
    log("jjfb_only_launch.txt", msg)
    time.sleep(60)
    rc, ns = run(["cmd", "/c", "netstat -ano"], timeout=20)
    log(f"jjfb_only_netstat_{ts}.txt", ns)

def restore(args=None):
    dst = mythroad_root()
    rep = [f"mythroad={dst}"]
    if dst:
        bak = dst / "dsm_gm.before_jjfb_only.mrp"
        dsm = dst / "dsm_gm.mrp"
        if bak.exists():
            shutil.copy2(bak, dsm)
            rep.append(f"restored dsm_gm.mrp sha1={sha1(dsm)}")
        else:
            rep.append("no backup found")
    out = "\n".join(rep)
    print(out)
    log("jjfb_only_restore.txt", out)

def collect(args=None):
    rc, tl = run(["cmd", "/c", "tasklist"], timeout=20)
    log("jjfb_only_tasklist.txt", tl)
    zipname = LOGS / f"jjfb_only_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname, "w", zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p, p.relative_to(ROOT))
    print("feedback:", zipname)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["prepare", "launch", "collect", "restore"])
    args = ap.parse_args()
    globals()[args.cmd](args)

if __name__ == "__main__":
    main()
