#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, subprocess, shutil, zipfile, hashlib, time
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"
LOGS.mkdir(exist_ok=True)

SDK_KEY = b"g:u2"

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

def sha1_bytes(b):
    h = hashlib.sha1(); h.update(b); return h.hexdigest()

def sha1_file(p):
    h = hashlib.sha1()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1024*1024), b""):
            h.update(b)
    return h.hexdigest()[:12]

def find_main():
    c = list((ROOT / "runtime" / "vmrp_win32").rglob("main.exe"))
    c.sort(key=lambda p: len(str(p)))
    return c[0] if c else None

def mythroad():
    exe = find_main()
    return exe.parent / "mythroad" if exe else None

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

def find_jjfb(dst):
    candidates = [
        ROOT / "game_files" / "mythroad" / "240x320" / "gwy" / "jjfb.mrp",
        ROOT / "game_files" / "mythroad" / "gwy" / "jjfb.mrp",
        dst / "gwy" / "jjfb.mrp",
        dst / "jjfb.mrp",
        dst / "000_jjfb.mrp",
    ]
    return next((p for p in candidates if p.exists()), None)

def prepare(args=None):
    rep = []
    exe = find_main()
    dst = mythroad()
    rep.append(f"main={exe}")
    rep.append(f"mythroad={dst}")
    rep.append(f"sdk_key={SDK_KEY!r} sha1={sha1_bytes(SDK_KEY)}")
    if not exe or not dst:
        rep.append("ERROR: main.exe or mythroad root not found")
        out = "\n".join(rep); print(out); log("jjfb_fixed_key_prepare.txt", out); return

    src_240 = ROOT / "game_files" / "mythroad" / "240x320"
    if src_240.exists():
        copy_merge(src_240, dst)
        rep.append(f"merged 240x320 -> {dst}")
        if (src_240 / "gwy").exists():
            copy_merge(src_240 / "gwy", dst / "gwy")
            rep.append("merged 240x320/gwy")

    jjfb = find_jjfb(dst)
    if not jjfb:
        rep.append("ERROR: jjfb.mrp not found")
        out = "\n".join(rep); print(out); log("jjfb_fixed_key_prepare.txt", out); return

    dsm = dst / "dsm_gm.mrp"
    bak = dst / "dsm_gm.before_fixed_key_v9.mrp"
    if dsm.exists() and not bak.exists():
        shutil.copy2(dsm, bak)
        rep.append(f"backup dsm_gm -> {bak.name} sha1={sha1_file(bak)}")

    shutil.copy2(jjfb, dsm)
    shutil.copy2(jjfb, dst / "jjfb.mrp")
    (dst / "gwy").mkdir(parents=True, exist_ok=True)
    shutil.copy2(jjfb, dst / "gwy" / "jjfb.mrp")
    rep.append(f"REPLACED dsm_gm <= jjfb size={dsm.stat().st_size} sha1={sha1_file(dsm)}")

    for rel in [
        "sdk_key.dat",
        "gwy/sdk_key.dat",
        "gwy/jjfbol/sdk_key.dat",
        "240x320/sdk_key.dat",
        "240x320/gwy/sdk_key.dat",
        "240x320/gwy/jjfbol/sdk_key.dat",
    ]:
        p = dst / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(SDK_KEY)
        rep.append(f"wrote {rel} size={p.stat().st_size} sha1={sha1_file(p)}")

    # Keep aliases for manual check.
    for rel, src in [
        ("000_jjfb.mrp", jjfb),
        ("gwy.mrp", ROOT / "game_files" / "mythroad" / "240x320" / "gwy.mrp"),
        ("gamelist.mrp", ROOT / "game_files" / "mythroad" / "240x320" / "gwy" / "gamelist.mrp"),
    ]:
        t = dst / rel
        try:
            if src.exists():
                shutil.copy2(src, t)
                rep.append(f"alias {rel} sha1={sha1_file(t)}")
        except Exception as e:
            rep.append(f"alias failed {rel}: {e!r}")

    rep.append("final files:")
    for rel in ["dsm_gm.mrp", "jjfb.mrp", "gwy/jjfb.mrp", "sdk_key.dat", "gwy/sdk_key.dat"]:
        p = dst / rel
        rep.append(f"  {rel}: exists={p.exists()} size={p.stat().st_size if p.exists() else ''} sha1={sha1_file(p) if p.exists() and p.is_file() else ''}")

    out = "\n".join(rep)
    print(out)
    log("jjfb_fixed_key_prepare.txt", out)

def launch(args=None):
    exe = find_main()
    if not exe:
        print("ERROR: main.exe not found")
        return
    # Kill previous main.exe to avoid reading stale windows.
    run(["cmd", "/c", "taskkill /IM main.exe /F"], timeout=10)
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    outp = LOGS / f"jjfb_fixed_key_stdout_{ts}.txt"
    with outp.open("wb") as f:
        p = subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
    msg = f"started {exe}\npid={p.pid}\nstdout={outp}\nwaiting 120s for direct jjfb...\n"
    print(msg)
    log("jjfb_fixed_key_launch.txt", msg)
    time.sleep(120)
    rc, ns = run(["cmd", "/c", "netstat -ano"], timeout=20)
    log(f"jjfb_fixed_key_netstat_{ts}.txt", ns)

def restore(args=None):
    dst = mythroad()
    rep = [f"mythroad={dst}"]
    if dst:
        bak = dst / "dsm_gm.before_fixed_key_v9.mrp"
        if bak.exists():
            shutil.copy2(bak, dst / "dsm_gm.mrp")
            rep.append(f"restored dsm_gm sha1={sha1_file(dst/'dsm_gm.mrp')}")
        else:
            rep.append("no backup")
    out = "\n".join(rep); print(out); log("jjfb_fixed_key_restore.txt", out)

def collect(args=None):
    rc, tl = run(["cmd", "/c", "tasklist"], timeout=20)
    log("jjfb_fixed_key_tasklist.txt", tl)
    zipname = LOGS / f"jjfb_fixed_key_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
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
