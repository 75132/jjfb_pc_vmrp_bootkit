#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, shutil, zipfile, hashlib, time, os, re, json, struct
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"
LOGS.mkdir(exist_ok=True)

FAKE_KEY_SHA1 = "65cc4c0b6cf9c56e2a2d801df1b99dc933db9991"

def log(name, text):
    (LOGS / name).write_text(text, encoding="utf-8", errors="replace")

def sha1_bytes(b):
    h = hashlib.sha1(); h.update(b); return h.hexdigest()

def sha1_file(p):
    return sha1_bytes(p.read_bytes())

def run_cmd(cmd, timeout=30, cwd=None):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, encoding="utf-8", errors="replace",
                           timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"

def find_main():
    candidates = list((ROOT / "runtime" / "vmrp_win32").rglob("main.exe"))
    candidates.sort(key=lambda p: len(str(p)))
    return candidates[0] if candidates else None

def mythroad_root():
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

def prepare_jjfb(rep):
    dst = mythroad_root()
    exe = find_main()
    rep.append(f"main={exe}")
    rep.append(f"mythroad={dst}")
    if not exe or not dst:
        raise RuntimeError("main.exe or mythroad root not found")

    src_240 = ROOT / "game_files" / "mythroad" / "240x320"
    if src_240.exists():
        copy_merge(src_240, dst)
        rep.append(f"merged 240x320 -> {dst}")
        if (src_240 / "gwy").exists():
            copy_merge(src_240 / "gwy", dst / "gwy")
            rep.append("merged 240x320/gwy")

    jjfb_candidates = [
        ROOT / "game_files" / "mythroad" / "240x320" / "gwy" / "jjfb.mrp",
        ROOT / "game_files" / "mythroad" / "gwy" / "jjfb.mrp",
        dst / "gwy" / "jjfb.mrp",
        dst / "jjfb.mrp",
        dst / "000_jjfb.mrp",
    ]
    jjfb = next((p for p in jjfb_candidates if p.exists()), None)
    if not jjfb:
        raise RuntimeError("jjfb.mrp not found")

    dsm = dst / "dsm_gm.mrp"
    bak = dst / "dsm_gm.before_sdkkey_probe.mrp"
    if dsm.exists() and not bak.exists():
        shutil.copy2(dsm, bak)
        rep.append(f"backup dsm_gm -> {bak.name}")

    shutil.copy2(jjfb, dsm)
    shutil.copy2(jjfb, dst / "jjfb.mrp")
    (dst / "gwy").mkdir(parents=True, exist_ok=True)
    shutil.copy2(jjfb, dst / "gwy" / "jjfb.mrp")
    rep.append(f"REPLACED dsm_gm <= {jjfb} size={jjfb.stat().st_size} sha1={sha1_file(jjfb)[:12]}")

def parse_cfg_candidates():
    out = []
    cfgs = [
        ROOT / "game_files" / "mythroad" / "240x320" / "gwy" / "cfg.bin",
        ROOT / "game_files" / "mythroad" / "gwy" / "cfg.bin",
    ]
    for cfg in cfgs:
        if not cfg.exists():
            continue
        data = cfg.read_bytes()
        # Find jjfb path in cfg and take bytes before/around it.
        m = re.search(rb"gwy/jjfb\.mrp", data)
        if m:
            for n in [8, 12, 16, 20, 24, 32]:
                start = max(0, m.start() - n)
                raw = data[start:m.start()]
                if raw:
                    out.append((f"cfg_raw_{n}b_before_path", raw))
                    out.append((f"cfg_hex_{n}b_before_path", raw.hex().encode("ascii")))
            # complete app record guess: base 1024 + 36*272
            off = 1024 + 36*272
            rec = data[off:off+272]
            if len(rec) == 272:
                out.append(("cfg_record36_raw_272b", rec))
                # ascii fragments
                for s in re.findall(rb"[\x20-\x7e]{3,}", rec):
                    out.append((f"cfg_record36_ascii_{s[:20].decode('ascii','replace')}", s))
        # any existing sdk-like data near cfg?
    return out

def collect_existing_key_candidates():
    out = []
    roots = [
        ROOT / "game_files" / "mythroad",
        ROOT / "game_files" / "mythroad" / "240x320",
        mythroad_root() or ROOT,
    ]
    seen = set()
    for r in roots:
        if r.exists():
            for p in r.rglob("sdk_key.dat"):
                try:
                    b = p.read_bytes()
                    if not b:
                        continue
                    key = sha1_bytes(b)
                    if key in seen:
                        continue
                    seen.add(key)
                    out.append((f"existing_{p}", b))
                except Exception:
                    pass
    return out

def make_text_candidates():
    base_vals = [
        b"123456789012345",
        b"123456789012345\x00",
        b"869169020812718",
        b"460006887846701",
        b"skymp.android",
        b"jjfb",
        b"gwy/jjfb.mrp",
        b"napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink",
    ]
    out = []
    for v in base_vals:
        out.append((f"raw_{v[:20]!r}", v))
        out.append((f"sdk_key_eq_{v[:20]!r}", b"sdk_key=" + v))
        out.append((f"key_eq_{v[:20]!r}", b"key=" + v))
        out.append((f"sdkkey_eq_{v[:20]!r}", b"sdkkey=" + v))
    out.append(("ini_imei_imsi_hstype", b"IMEI=869169020812718\nIMSI=460006887846701\nhstype=android\nsdk_key=123456789012345\n"))
    out.append(("mr_param_only", b"_mr_param=napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink\n"))
    return out

def write_key_to_all_locations(data):
    dst = mythroad_root()
    rels = [
        "sdk_key.dat",
        "gwy/sdk_key.dat",
        "gwy/jjfbol/sdk_key.dat",
        "240x320/sdk_key.dat",
        "240x320/gwy/sdk_key.dat",
        "240x320/gwy/jjfbol/sdk_key.dat",
    ]
    for rel in rels:
        p = dst / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)

def kill_existing_vmrp():
    # kill main.exe instances to avoid many windows.
    run_cmd(["cmd", "/c", "taskkill /IM main.exe /F"], timeout=10)

def try_candidate(idx, name, data, wait=8):
    exe = find_main()
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    safe = re.sub(r"[^a-zA-Z0-9_.-]+", "_", name)[:80]
    stdout = LOGS / f"sdkkey_try_{idx:03d}_{safe}_{ts}.txt"
    write_key_to_all_locations(data)
    kill_existing_vmrp()
    with stdout.open("wb") as f:
        p = subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
    time.sleep(wait)
    # If still alive, leave briefly but kill after reading; direct boot should print error fast if fail.
    try:
        if p.poll() is None:
            p.terminate()
            time.sleep(1)
            if p.poll() is None:
                p.kill()
    except Exception:
        pass
    text = stdout.read_text(encoding="utf-8", errors="replace")
    fail = ("cann`t find sdk key" in text) or ("can't find sdk key" in text) or ("can`t find sdk key" in text)
    return {
        "idx": idx,
        "name": name,
        "sha1": sha1_bytes(data),
        "size": len(data),
        "stdout": str(stdout),
        "failed_sdk_key": fail,
        "text_tail": text[-1200:],
    }

def run_probe():
    rep = []
    prepare_jjfb(rep)
    log("sdkkey_probe_prepare.txt", "\n".join(rep))

    candidates = []
    candidates += collect_existing_key_candidates()
    candidates += parse_cfg_candidates()
    candidates += make_text_candidates()

    # Deduplicate exact bytes but preserve first name.
    uniq = []
    seen = set()
    for name, data in candidates:
        h = sha1_bytes(data)
        if h in seen:
            continue
        seen.add(h)
        if not data:
            continue
        # Avoid huge raw records first; keep under 512 bytes for probe but include record36 at end.
        uniq.append((name, data))
    # Put non-fake existing first, fake existing later.
    uniq.sort(key=lambda x: (sha1_bytes(x[1]) == FAKE_KEY_SHA1, len(x[1]) > 128, len(x[1])))

    manifest = []
    success = None
    for idx, (name, data) in enumerate(uniq[:80]):
        res = try_candidate(idx, name, data)
        manifest.append(res)
        print(f"[{idx}] {name} size={len(data)} fail_sdk_key={res['failed_sdk_key']}")
        if not res["failed_sdk_key"]:
            success = res
            print("POSSIBLE SUCCESS:", res)
            # Run once more and keep window/log longer.
            write_key_to_all_locations(data)
            exe = find_main()
            ts = datetime.now().strftime("%Y%m%d_%H%M%S")
            outp = LOGS / f"sdkkey_SUCCESS_keep_{idx:03d}_{ts}.txt"
            with outp.open("wb") as f:
                p = subprocess.Popen([str(exe)], cwd=str(exe.parent), stdout=f, stderr=subprocess.STDOUT)
            time.sleep(60)
            try:
                if p.poll() is None:
                    p.terminate()
            except Exception:
                pass
            break

    log("sdkkey_probe_manifest.json", json.dumps({"success": success, "tries": manifest}, ensure_ascii=False, indent=2))
    collect_zip()

def collect_zip():
    rc, tl = run_cmd(["cmd", "/c", "tasklist"], timeout=20)
    log("sdkkey_probe_tasklist.txt", tl)
    zipname = LOGS / f"sdkkey_probe_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname, "w", zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p, p.relative_to(ROOT))
    print("feedback:", zipname)

if __name__ == "__main__":
    run_probe()
