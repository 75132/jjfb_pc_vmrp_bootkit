#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, json, subprocess, os, sys, time, shutil, zipfile, hashlib, csv, re
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"
LOGS.mkdir(exist_ok=True)

def load_config():
    return json.loads((ROOT / "CONFIG.json").read_text(encoding="utf-8"))

def log(name, text):
    (LOGS / name).write_text(text, encoding="utf-8", errors="replace")

def run(cmd, timeout=30, cwd=None):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout, cwd=cwd, text=True, encoding="utf-8", errors="replace")
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"

def sha1(p):
    h = hashlib.sha1()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1024*1024), b""):
            h.update(b)
    return h.hexdigest()

def find_exes():
    cfg = load_config()
    root = ROOT / cfg["vmrp_extract_dir"]
    names = ["main.exe", "vmrp.exe"]
    exes = []
    for p in root.rglob("*.exe"):
        if p.name.lower() in names or "vmrp" in p.name.lower() or p.name.lower() == "main.exe":
            exes.append(p)
    # stable order: main first
    exes.sort(key=lambda p: (0 if p.name.lower()=="main.exe" else 1, len(str(p))))
    return exes

def find_fs_dirs(exe):
    # vmrp release usually has bin/mythroad or mythroad near executable.
    candidates = []
    for base in [exe.parent, exe.parent / "bin", exe.parent.parent, exe.parent.parent / "bin"]:
        for rel in ["mythroad", "fs/mythroad", "wasm/dist/fs/mythroad"]:
            d = base / rel
            if d.exists() or base.exists():
                candidates.append(d)
    # plus all mythroad dirs in release
    cfg = load_config()
    root = ROOT / cfg["vmrp_extract_dir"]
    for d in root.rglob("mythroad"):
        if d.is_dir():
            candidates.append(d)
    # unique
    seen = []
    out = []
    for d in candidates:
        d = d.resolve()
        if d not in seen:
            seen.append(d)
            out.append(d)
    return out

def copy_tree_contents(src, dst):
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        target = dst / item.name
        if item.is_dir():
            if target.exists():
                shutil.rmtree(target)
            shutil.copytree(item, target)
        else:
            shutil.copy2(item, target)

def cmd_prepare(args):
    cfg = load_config()
    game_src = ROOT / cfg["game_source_dir"]
    if not game_src.exists():
        raise SystemExit("missing game_files/mythroad")
    # sdk keys
    for rel in ["sdk_key.dat", "gwy/sdk_key.dat"]:
        p = game_src / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        if not p.exists():
            p.write_text(cfg.get("sdk_key_content","123456789012345"), encoding="ascii")
    jjfb = game_src / "gwy" / "jjfb.mrp"
    status = []
    status.append(f"game_src={game_src}")
    status.append(f"jjfb_exists={jjfb.exists()}")
    if jjfb.exists():
        status.append(f"jjfb_sha1={sha1(jjfb)}")
    exes = find_exes()
    status.append("exes=" + repr([str(e) for e in exes]))
    if not exes:
        status.append("NO_EXE_FOUND. Put vmrp windows files into runtime/vmrp_win32 or run downloader.")
        log("prepare_status.txt", "\n".join(status))
        print("\n".join(status))
        return
    for exe in exes[:3]:
        fs_dirs = find_fs_dirs(exe)
        for d in fs_dirs:
            try:
                copy_tree_contents(game_src, d)
                status.append(f"copied mythroad to {d}")
                # make sure sdk keys exist
                for rel in ["sdk_key.dat", "gwy/sdk_key.dat"]:
                    p = d / rel
                    p.parent.mkdir(parents=True, exist_ok=True)
                    if not p.exists():
                        p.write_text(cfg.get("sdk_key_content","123456789012345"), encoding="ascii")
            except Exception as e:
                status.append(f"copy failed {d}: {e!r}")
    log("prepare_status.txt", "\n".join(status))
    print("\n".join(status))

def snapshot_dir(root, phase):
    rows = []
    if not root.exists():
        return rows
    for p in root.rglob("*"):
        if p.is_file():
            try:
                st = p.stat()
                rows.append([phase, str(root), str(p.relative_to(root)), st.st_size, int(st.st_mtime), sha1(p)])
            except Exception as e:
                rows.append([phase, str(root), str(p), "", "", f"ERR:{e!r}"])
    return rows

def cmd_snapshot(args):
    cfg = load_config()
    rows = [["phase","root","relative_path","size","mtime","sha1"]]
    game_src = ROOT / cfg["game_source_dir"]
    rows += snapshot_dir(game_src, args.phase)
    for exe in find_exes()[:3]:
        for d in find_fs_dirs(exe):
            rows += snapshot_dir(d, args.phase)
    name = LOGS / f"fs_snapshot_{args.phase}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    with name.open("w", newline="", encoding="utf-8-sig") as f:
        csv.writer(f).writerows(rows)
    print("snapshot:", name)

def netstat_log(tag):
    rc, out = run(["cmd", "/c", "netstat -ano"], timeout=20)
    log(f"netstat_{tag}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt", out)

def launch_attempt(exe, args, tag, wait):
    ts = datetime.now().strftime('%Y%m%d_%H%M%S')
    stdout_path = LOGS / f"vmrp_stdout_{tag}_{ts}.txt"
    meta = {"tag": tag, "exe": str(exe), "args": args, "cwd": str(exe.parent), "stdout": str(stdout_path), "start": datetime.now().isoformat()}
    with stdout_path.open("wb") as out:
        try:
            p = subprocess.Popen([str(exe)] + args, cwd=str(exe.parent), stdout=out, stderr=subprocess.STDOUT)
            meta["pid"] = p.pid
            time.sleep(wait)
            meta["alive_after_wait"] = (p.poll() is None)
            if p.poll() is None:
                # leave window open for visual test but record netstat
                pass
            else:
                meta["returncode"] = p.returncode
        except Exception as e:
            meta["error"] = repr(e)
    netstat_log(tag)
    return meta

def cmd_launch(args):
    cfg = load_config()
    exes = find_exes()
    attempts = []
    if not exes:
        log("vmrp_launch_attempts.json", json.dumps({"error":"no exe found"}, ensure_ascii=False, indent=2))
        print("no exe found")
        return
    exe = exes[0]
    wait = int(cfg.get("test_wait_seconds", 45))
    target_paths = []
    for target in cfg.get("target_mrps", ["gwy/jjfb.mrp"]):
        target_paths += [
            target,
            "mythroad/" + target,
            "bin/mythroad/" + target,
            str((ROOT / cfg["game_source_dir"] / target).resolve()),
        ]
    # Try no args first; user may need choose in GUI.
    attempts.append(launch_attempt(exe, [], "no_args_open_gui", 10))
    # Try target args.
    for i, t in enumerate(target_paths):
        attempts.append(launch_attempt(exe, [t], f"target_arg_{i}", wait))
    # Try target + param variants.
    for i, param in enumerate(cfg.get("mr_param_candidates", [])):
        attempts.append(launch_attempt(exe, ["gwy/jjfb.mrp", param], f"target_param_space_{i}", wait))
        attempts.append(launch_attempt(exe, ["gwy/jjfb.mrp", "_mr_param=" + param], f"target_param_key_{i}", wait))
    log("vmrp_launch_attempts.json", json.dumps(attempts, ensure_ascii=False, indent=2))
    print("launch attempts done; if vmrp GUI is open, manually select gwy/jjfb.mrp once, then send feedback zip.")

def cmd_collect(args):
    # collect tasklist and netstat
    rc, out = run(["cmd","/c","tasklist"], timeout=20)
    log("tasklist.txt", out)
    netstat_log("final")
    zipname = LOGS / f"feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname, "w", zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p, p.relative_to(ROOT))
    print("feedback zip:", zipname)

def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("prepare")
    sp = sub.add_parser("snapshot"); sp.add_argument("--phase", default="pre")
    sub.add_parser("launch")
    sub.add_parser("collect")
    args = ap.parse_args()
    globals()["cmd_" + args.cmd](args)

if __name__ == "__main__":
    main()
