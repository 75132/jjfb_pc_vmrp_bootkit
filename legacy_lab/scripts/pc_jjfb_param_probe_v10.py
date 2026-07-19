#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, shutil, zipfile, hashlib, time, re, json
from pathlib import Path
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"; LOGS.mkdir(exist_ok=True)
SDK_KEY = b"g:u2"

PARAM_CANDIDATES = [
    "napptype=0_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/jjfb.mrp_gwyblink",
    "napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink",
    "napptype=1_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink",
    "napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=jjfb.mrp_gwyblink",
    "bapptype=0_bextid=482_bcode=512_barg=0_barg1=1_bmrpname=gwy/jjfb.mrp_gwyblink",
    "gwyblink",
]

PARAM_FILENAMES = [
    "_mr_param",
    "mr_param",
    "param",
    "param.txt",
    "gwyblink",
    "nmrpname",
    "start.arg",
    "gwy/_mr_param",
    "gwy/mr_param",
    "gwy/param.txt",
    "gwy/gwyblink",
    "240x320/_mr_param",
    "240x320/gwy/_mr_param",
]

def log(name, text):
    (LOGS / name).write_text(text, encoding="utf-8", errors="replace")

def run_cmd(cmd, timeout=30, cwd=None):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, encoding="utf-8", errors="replace",
                           timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"

def sha1(p):
    h=hashlib.sha1()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024), b''): h.update(b)
    return h.hexdigest()[:12]

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

def prepare_base():
    dst=mythroad()
    rep=[f"main={find_main()}", f"mythroad={dst}"]
    if not dst or not find_main():
        raise RuntimeError("main.exe or mythroad missing")
    src_240=ROOT/"game_files"/"mythroad"/"240x320"
    if src_240.exists():
        copy_merge(src_240,dst)
        if (src_240/"gwy").exists(): copy_merge(src_240/"gwy",dst/"gwy")
    jjfb = next((p for p in [
        ROOT/"game_files"/"mythroad"/"240x320"/"gwy"/"jjfb.mrp",
        ROOT/"game_files"/"mythroad"/"gwy"/"jjfb.mrp",
        dst/"gwy"/"jjfb.mrp",
        dst/"jjfb.mrp",
    ] if p.exists()), None)
    if not jjfb:
        raise RuntimeError("jjfb.mrp missing")
    dsm=dst/"dsm_gm.mrp"
    bak=dst/"dsm_gm.before_param_probe_v10.mrp"
    if dsm.exists() and not bak.exists():
        shutil.copy2(dsm,bak)
    shutil.copy2(jjfb,dsm)
    shutil.copy2(jjfb,dst/"jjfb.mrp")
    (dst/"gwy").mkdir(parents=True,exist_ok=True)
    shutil.copy2(jjfb,dst/"gwy"/"jjfb.mrp")
    for rel in ["sdk_key.dat","gwy/sdk_key.dat","gwy/jjfbol/sdk_key.dat","240x320/sdk_key.dat","240x320/gwy/sdk_key.dat"]:
        p=dst/rel; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(SDK_KEY)
    rep.append(f"dsm=jjfb sha1={sha1(dsm)} sdk_key=g:u2")
    log("jjfb_param_probe_prepare.txt","\n".join(rep))

def clear_param_files():
    dst=mythroad()
    for rel in PARAM_FILENAMES:
        p=dst/rel
        try:
            if p.exists(): p.unlink()
        except Exception:
            pass

def write_param_files(param):
    dst=mythroad()
    data=param.encode("utf-8")
    for rel in PARAM_FILENAMES:
        p=dst/rel
        p.parent.mkdir(parents=True,exist_ok=True)
        p.write_bytes(data)

def kill_vmrp():
    run_cmd(["cmd","/c","taskkill /IM main.exe /F"],timeout=10)

def start_once(tag, wait=12):
    exe=find_main()
    ts=datetime.now().strftime("%Y%m%d_%H%M%S")
    outp=LOGS/f"param_probe_{tag}_{ts}.txt"
    with outp.open("wb") as f:
        p=subprocess.Popen([str(exe)],cwd=str(exe.parent),stdout=f,stderr=subprocess.STDOUT)
    time.sleep(wait)
    alive = p.poll() is None
    try:
        if alive:
            p.terminate()
            time.sleep(1)
            if p.poll() is None: p.kill()
    except Exception:
        pass
    text=outp.read_text(encoding="utf-8",errors="replace")
    return {
        "tag": tag, "stdout": str(outp), "alive": alive,
        "len": len(text),
        "has_sdk_error": ("sdk key" in text.lower()),
        "has_connect": ("connect" in text.lower() or "mr_socket" in text.lower()),
        "png_count": text.count("libpng warning"),
        "tail": text[-1000:],
    }

def run_probe():
    prepare_base()
    results=[]
    # baseline no param files
    clear_param_files()
    kill_vmrp()
    results.append(start_once("baseline_no_param", wait=15))
    # candidates
    for i,param in enumerate(PARAM_CANDIDATES):
        clear_param_files()
        write_param_files(param)
        kill_vmrp()
        res=start_once(f"cand_{i}", wait=15)
        res["param"]=param
        results.append(res)
        print(f"[{i}] len={res['len']} png={res['png_count']} sdkerr={res['has_sdk_error']} connect={res['has_connect']}")
        # keep if we see new behavior beyond png warnings
        if res["has_connect"] or (not res["has_sdk_error"] and res["png_count"] < 9 and res["len"] != results[0]["len"]):
            break
    log("jjfb_param_probe_results.json", json.dumps(results, ensure_ascii=False, indent=2))
    collect_zip()

def collect_zip():
    rc,tl=run_cmd(["cmd","/c","tasklist"],timeout=20)
    log("jjfb_param_probe_tasklist.txt",tl)
    zipname=LOGS/f"jjfb_param_probe_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname,"w",zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p,p.relative_to(ROOT))
    print("feedback:",zipname)

if __name__=="__main__":
    run_probe()
