#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, zipfile, shutil, hashlib, json, re, os, struct
from pathlib import Path
from datetime import datetime
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"; LOGS.mkdir(exist_ok=True)
RUNTIME = ROOT / "runtime"; RUNTIME.mkdir(exist_ok=True)

START_PARAM = "napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
SRC_URL = "https://github.com/vmrp/vmrp/archive/refs/heads/master.zip"

def log(name, text):
    (LOGS/name).write_text(text, encoding="utf-8", errors="replace")

def sha1_file(p):
    h=hashlib.sha1()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024), b''): h.update(b)
    return h.hexdigest()

def run(cmd, timeout=120, cwd=None):
    try:
        p=subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"
    except Exception as e:
        return 999, repr(e)

def download_source(report):
    zip_path = RUNTIME / "vmrp_master.zip"
    extract_dir = RUNTIME / "vmrp_src"
    if not zip_path.exists() or zip_path.stat().st_size < 100000:
        report.append(f"downloading {SRC_URL}")
        try:
            urllib.request.urlretrieve(SRC_URL, zip_path)
            report.append(f"downloaded {zip_path} size={zip_path.stat().st_size}")
        except Exception as e:
            report.append(f"python download failed: {e!r}")
            rc,out = run(["powershell","-NoProfile","-ExecutionPolicy","Bypass","-Command", f"Invoke-WebRequest -Uri '{SRC_URL}' -OutFile '{zip_path}'"], timeout=180)
            report.append(f"powershell download rc={rc}\n{out}")
    if extract_dir.exists():
        shutil.rmtree(extract_dir)
    extract_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path, 'r') as z:
        z.extractall(extract_dir)
    candidates=list(extract_dir.rglob("vmrp.c"))
    report.append("vmrp.c candidates:\n" + "\n".join(str(c) for c in candidates))
    return candidates[0] if candidates else None

def patch_vmrp_c(vmrp_c, report):
    text = vmrp_c.read_text(encoding="utf-8", errors="replace")
    old = 'uint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, NULL);\n\t\tprintf("bridge_dsm_mr_start_dsm(\'%s\',\'%s\',NULL): 0x%X\\n", filename, extName, ret);'
    if old not in text:
        old = 'uint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, NULL);\n\t\tprintf("bridge_dsm_mr_start_dsm(\'%s\',\'%s\',NULL): 0x%X\\n", filename, extName, ret);'
    new = f'char *startParam = "{START_PARAM}";\n\t\tuint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, startParam);\n\t\tprintf("bridge_dsm_mr_start_dsm(\'%s\',\'%s\',\'%s\'): 0x%X\\n", filename, extName, startParam, ret);'
    if old in text:
        patched=text.replace(old,new)
        vmrp_c.write_text(patched, encoding="utf-8")
        report.append("patched vmrp.c using exact replacement")
    else:
        # broader replacement
        pattern = r'uint32_t\s+ret\s*=\s*bridge_dsm_mr_start_dsm\(uc,\s*filename,\s*extName,\s*NULL\);\s*printf\("bridge_dsm_mr_start_dsm\(\'%s\',\'%s\',NULL\): 0x%X\\n",\s*filename,\s*extName,\s*ret\);'
        patched, n = re.subn(pattern, new, text)
        if n:
            vmrp_c.write_text(patched, encoding="utf-8")
            report.append(f"patched vmrp.c using regex n={n}")
        else:
            report.append("FAILED to patch vmrp.c automatically; wrote target snippet")
    out_copy = ROOT / "logs" / "vmrp_patched_vmrp.c"
    shutil.copy2(vmrp_c, out_copy)
    report.append(f"patched copy: {out_copy}")

def try_build(src_root, report):
    # find repo root: directory containing Makefile and vmrp.c
    repo = None
    for p in [src_root] + list(src_root.parents):
        if (p/"Makefile").exists() and (p/"vmrp.c").exists():
            repo = p
            break
    if not repo:
        report.append("no repo root/Makefile found")
        return
    rc,out = run(["cmd","/c","where mingw32-make"], timeout=20)
    report.append(f"where mingw32-make rc={rc}\n{out}")
    if rc != 0:
        report.append("mingw32-make not found; skip build")
        return
    rc,out = run(["mingw32-make"], timeout=300, cwd=repo)
    report.append(f"mingw32-make rc={rc}\n{out[-5000:]}")
    # collect exe
    for exe in repo.rglob("main.exe"):
        dst = LOGS / f"patched_main_{datetime.now().strftime('%Y%m%d_%H%M%S')}.exe"
        try:
            shutil.copy2(exe,dst)
            report.append(f"built exe copied: {dst} sha1={sha1_file(dst)}")
        except Exception as e:
            report.append(f"copy built exe failed {exe}: {e!r}")

def scan_binary_callsite(report):
    # No patch here, just gather data for possible binary patch.
    exes = list((ROOT/"runtime"/"vmrp_win32").rglob("main.exe"))
    if not exes:
        report.append("no runtime main.exe for binary scan")
        return
    exe = sorted(exes, key=lambda p: len(str(p)))[0]
    data = exe.read_bytes()
    report.append(f"binary scan main={exe} size={len(data)} sha1={sha1_file(exe)}")
    needles = [
        b"bridge_dsm_mr_start_dsm",
        b"dsm_gm.mrp",
        b"start.mr",
        b"NULL",
        START_PARAM.encode("ascii"),
    ]
    for nd in needles:
        offs=[m.start() for m in re.finditer(re.escape(nd), data)]
        report.append(f"needle {nd!r}: {offs[:20]} count={len(offs)}")
        for off in offs[:5]:
            chunk=data[max(0,off-64):off+len(nd)+64]
            report.append(f"  around 0x{off:x}: {chunk.hex()}")

    # Look for x86 pushes of 0 before call, around string references is not reliable without disasm.
    # Emit imports/strings only.
    strings=[]
    for m in re.finditer(rb"[\x20-\x7e]{6,}", data):
        s=m.group().decode("ascii","replace")
        if any(k in s for k in ["bridge_dsm", "dsm_gm", "start.mr", "mythroad", "NULL"]):
            strings.append((m.start(), s))
    report.append("interesting strings:\n" + "\n".join(f"0x{o:x} {s}" for o,s in strings[:200]))
    log("main_exe_callsite_scan.txt", "\n".join(report))

def collect_zip():
    zipname = LOGS / f"vmrp_param_patch_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname,"w",zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p,p.relative_to(ROOT))
        for p in (ROOT/"patches").glob("*"):
            if p.is_file():
                z.write(p,p.relative_to(ROOT))
    print("feedback:", zipname)

def main():
    report=[]
    report.append(f"START_PARAM={START_PARAM}")
    try:
        vmrp_c = download_source(report)
        if vmrp_c:
            patch_vmrp_c(vmrp_c, report)
            try_build(vmrp_c.parent, report)
    except Exception as e:
        report.append(f"source patch flow error: {e!r}")
    try:
        scan_binary_callsite(report)
    except Exception as e:
        report.append(f"binary scan error: {e!r}")
    log("vmrp_param_patch_report.txt", "\n".join(report))
    print("\n".join(report[-30:]))
    collect_zip()

if __name__=="__main__":
    main()
