#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, struct, re, shutil, zipfile, hashlib, subprocess, time, json, zlib, gzip, io, os
from pathlib import Path
from datetime import datetime
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"; LOGS.mkdir(exist_ok=True)
RUNTIME = ROOT / "runtime"; RUNTIME.mkdir(exist_ok=True)

SRC_URL = "https://github.com/vmrp/vmrp/archive/refs/heads/master.zip"
START_PARAM_STR = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
SDK_KEY = b"g:u2"
TARGET_NET_RE = re.compile(r"(:20000\b|:21002\b|:6009\b|211\.155\.236\.|111\.1\.17\.)")

def log(name, text):
    (LOGS/name).write_text(text, encoding="utf-8", errors="replace")

def run_cmd(cmd, timeout=60, cwd=None, env=None):
    try:
        p=subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, encoding="utf-8", errors="replace",
                         timeout=timeout, cwd=cwd, env=env)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"
    except Exception as e:
        return 999, repr(e)

def sha1_file(p):
    h=hashlib.sha1()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024), b""):
            h.update(b)
    return h.hexdigest()

def find_runtime_main():
    c=[]
    root=ROOT/"runtime"/"vmrp_win32"
    if root.exists():
        for p in root.rglob("main.exe"):
            if p.name.lower()=="main.exe":
                c.append(p)
    c.sort(key=lambda p: len(str(p)))
    return c[0] if c else None

def find_source_jjfb(runtime_dst=None):
    candidates=[
        ROOT/"game_files"/"mythroad"/"240x320"/"gwy"/"jjfb.mrp",
        ROOT/"game_files"/"mythroad"/"gwy"/"jjfb.mrp",
    ]
    if runtime_dst:
        candidates += [runtime_dst/"gwy"/"jjfb.mrp", runtime_dst/"jjfb.mrp", runtime_dst/"dsm_gm.mrp"]
    for p in candidates:
        if p.exists():
            return p
    return None

def check_env():
    rep=[]
    checks=[["cmd","/c","where mingw32-make"],
            ["cmd","/c","where gcc"],
            ["cmd","/c","where pkg-config"],
            ["cmd","/c","where sdl2-config"]]
    ok=True
    for cmd in checks:
        rc,out=run_cmd(cmd, timeout=20)
        rep.append(f"$ {' '.join(cmd)}\nrc={rc}\n{out}")
        if "mingw32-make" in cmd[-1] and rc != 0:
            ok=False
    log("source_build_loader_v27_env_check.txt", "\n\n".join(rep))
    return ok, "\n\n".join(rep)

def download_source(report):
    srczip=RUNTIME/"vmrp_master_v27.zip"
    srcdir=RUNTIME/"vmrp_src_build_v27"
    if not srczip.exists() or srczip.stat().st_size < 100000:
        report.append(f"downloading {SRC_URL}")
        try:
            urllib.request.urlretrieve(SRC_URL, srczip)
            report.append(f"downloaded {srczip} size={srczip.stat().st_size}")
        except Exception as e:
            report.append(f"python download failed: {e!r}")
            rc,out=run_cmd(["powershell","-NoProfile","-ExecutionPolicy","Bypass","-Command",f"Invoke-WebRequest -Uri '{SRC_URL}' -OutFile '{srczip}'"], timeout=180)
            report.append(f"powershell download rc={rc}\n{out[-4000:]}")
    if srcdir.exists():
        shutil.rmtree(srcdir)
    srcdir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(srczip) as z:
        z.extractall(srcdir)
    candidates=list(srcdir.rglob("vmrp.c"))
    report.append("vmrp.c candidates:\n" + "\n".join(str(c) for c in candidates))
    if not candidates:
        raise RuntimeError("vmrp.c not found after extract")
    # Prefer root vmrp.c that has bridge_dsm_mr_start_dsm.
    for c in candidates:
        txt=c.read_text(encoding="utf-8", errors="replace")
        if "bridge_dsm_mr_start_dsm" in txt:
            return c
    return candidates[0]

def patch_vmrp_source(vmrp_c, report):
    text=vmrp_c.read_text(encoding="utf-8", errors="replace")
    original=text
    report.append(f"patching {vmrp_c}")

    # Ensure startParam declaration exists before bridge call.
    # Pattern handles current vmrp.c variants.
    pat = r'(char\s*\*\s*filename\s*=\s*"dsm_gm\.mrp"\s*;\s*\n\s*char\s*\*\s*extName\s*=\s*"start\.mr"\s*;\s*\n)'
    repl = r'\1    char *startParam = "' + START_PARAM_STR + r'";' + "\n"
    text2, n_decl = re.subn(pat, repl, text, count=1)
    if n_decl == 0 and "startParam" not in text:
        # fallback insert before bridge call
        idx=text.find("bridge_dsm_mr_start_dsm")
        if idx >= 0:
            line_start=text.rfind("\n",0,idx)+1
            text2=text[:line_start] + f'    char *startParam = "{START_PARAM_STR}";\n' + text[line_start:]
            n_decl=1
        else:
            text2=text

    # Replace NULL argument with startParam.
    text3, n_call = re.subn(r'bridge_dsm_mr_start_dsm\(([^;]*?),\s*NULL\)', r'bridge_dsm_mr_start_dsm(\1, startParam)', text2, count=1)
    # Replace visible printf NULL marker if exists, not required.
    text3 = text3.replace("bridge_dsm_mr_start_dsm('%s','%s',NULL): 0x%X", "bridge_dsm_mr_start_dsm('%s','%s',SRCV): 0x%X")
    # If printf now has 3 placeholders and not enough args we do not change it; compile will reveal.

    vmrp_c.write_text(text3, encoding="utf-8")
    out=LOGS/"vmrp_v27_patched.c"
    shutil.copy2(vmrp_c, out)
    report.append(f"decl_inserted={n_decl} call_replaced={n_call} changed={text3 != original}")
    report.append(f"patched copy={out}")

def try_build(vmrp_c, report):
    # repo root = nearest parent with Makefile
    repo=None
    for p in [vmrp_c.parent]+list(vmrp_c.parents):
        if (p/"Makefile").exists():
            repo=p
            break
    if not repo:
        report.append("Makefile not found near vmrp.c")
        return None
    report.append(f"repo={repo}")
    # show files
    report.append("repo files:\n" + "\n".join(x.name for x in repo.iterdir() if x.is_file())[:2000])
    rc,out=run_cmd(["cmd","/c","where mingw32-make"], timeout=20)
    report.append(f"where mingw32-make rc={rc}\n{out}")
    if rc != 0:
        report.append("BUILD_SKIPPED: mingw32-make not found")
        return None

    # build
    rc,out=run_cmd(["mingw32-make","clean"], timeout=180, cwd=repo)
    report.append(f"mingw32-make clean rc={rc}\n{out[-4000:]}")
    rc,out=run_cmd(["mingw32-make"], timeout=900, cwd=repo)
    report.append(f"mingw32-make rc={rc}\n{out[-12000:]}")
    if rc != 0:
        return None

    exes=list(repo.rglob("main.exe"))
    exes += list(repo.rglob("vmrp.exe"))
    exes=sorted(set(exes), key=lambda p: (len(str(p)), str(p)))
    report.append("built exe candidates:\n" + "\n".join(str(e) for e in exes))
    if not exes:
        return None
    built=exes[0]
    copied=LOGS/f"built_main_v27_{datetime.now().strftime('%Y%m%d_%H%M%S')}.exe"
    shutil.copy2(built, copied)
    report.append(f"built exe copied={copied} sha1={sha1_file(copied)}")
    return built

# MRP start.mr patch: skip sdk error only, DO NOT skip line157.
def read_u32(buf, off):
    return int.from_bytes(buf[off:off+4], "little"), off+4
def read_u8(buf, off):
    return buf[off], off+1
def read_str(buf, off):
    l, off = read_u32(buf, off)
    s=buf[off:off+l]
    off += l
    return s.rstrip(b"\x00").decode("ascii","replace"), off
def parse_consts(buf, off):
    n, off = read_u32(buf, off)
    consts=[]
    for i in range(n):
        tag=buf[off]; off+=1
        if tag==3:
            off+=4
        elif tag==4:
            l,off=read_u32(buf,off); off+=l
        elif tag==5:
            pass
        else:
            raise RuntimeError(f"unknown const tag {tag}")
    return consts, off
def parse_func(buf, off, top=False):
    if top:
        if buf[off:off+4] != b"\x1bMRP":
            raise RuntimeError("not MRP bytecode")
        off += 4
        off += 2
    src, off = read_str(buf, off)
    linedef, off = read_u32(buf, off)
    nups, off = read_u8(buf, off)
    numparams, off = read_u8(buf, off)
    vararg, off = read_u8(buf, off)
    maxstack, off = read_u8(buf, off)
    sizeline, off = read_u32(buf, off)
    lineinfo=[int.from_bytes(buf[off+i*4:off+i*4+4],"little") for i in range(sizeline)]
    off += sizeline*4
    nloc, off = read_u32(buf, off)
    for _ in range(nloc):
        name, off=read_str(buf, off)
        off += 8
    nupnames, off = read_u32(buf, off)
    for _ in range(nupnames):
        name, off = read_str(buf, off)
    _, off = parse_consts(buf, off)
    nproto, off = read_u32(buf, off)
    for _ in range(nproto):
        _, off = parse_func(buf, off, top=False)
    ncode, off = read_u32(buf, off)
    code_off=off
    code=[int.from_bytes(buf[off+i*4:off+i*4+4],"little") for i in range(ncode)]
    off += ncode*4
    return {"lineinfo":lineinfo,"code_off":code_off,"code":code}, off
# Lua 5.0 JMP: sBx is excess-K with K=MAXARG_sBx(131071).
# Old formula 0x00800000|((skip&0xff)<<6)|20 encoded sBx=skip+1 (off-by-one),
# which skipped GETGLOBAL and CALLed leftover R(A) -> false "call global (object)".
MAXARG_SBX = 131071
def encode_jmp(skip):
    bx = (int(skip) + MAXARG_SBX) & 0x3FFFF
    return 20 | (bx << 6)
def patch_line_to_target(buf, f, line, min_target_line=149):
    pcs=[i for i,l in enumerate(f["lineinfo"], start=1) if l==line]
    if not pcs: raise RuntimeError(f"line {line} not found")
    pc=pcs[0]
    target_pc=None
    for i,l in enumerate(f["lineinfo"], start=1):
        if i>pc and l>=min_target_line:
            target_pc=i; break
    if not target_pc: raise RuntimeError(f"target for line {line} not found")
    skip=target_pc-pc-1
    off=f["code_off"]+(pc-1)*4
    old=bytes(buf[off:off+4])
    new=struct.pack("<I", encode_jmp(skip))
    buf[off:off+4]=new
    # After JMP at pc: next_pc = pc+1+sBx; must equal target_pc.
    land = pc + 1 + skip
    return {"line":line,"pc":pc,"target_pc":target_pc,"target_line":f["lineinfo"][target_pc-1],"skip":skip,"land_pc":land,"off":hex(off),"old":old.hex(),"new":new.hex(),"enc":hex(encode_jmp(skip))}
def gzip_bytes(payload):
    buf=io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", compresslevel=9, mtime=0) as gz:
        gz.write(payload)
    return buf.getvalue()
def patch_jjfb_skip_sdk(src, out):
    data=bytearray(src.read_bytes())
    marker=b"start.mr\x00"
    pos=data.find(marker)
    if pos<0: raise RuntimeError("start.mr not found")
    off_field=pos+len(marker)
    start_off=struct.unpack_from("<I", data, off_field)[0]
    old_size=struct.unpack_from("<I", data, off_field+4)[0]
    decomp=bytearray(zlib.decompress(bytes(data[start_off:start_off+old_size]), wbits=31))
    f,end=parse_func(decomp,0,top=True)
    patches=[patch_line_to_target(decomp,f,143,149), patch_line_to_target(decomp,f,147,149)]
    new_comp=gzip_bytes(bytes(decomp))
    if len(new_comp)>old_size:
        raise RuntimeError(f"patched start.mr gzip too large {len(new_comp)}>{old_size}")
    struct.pack_into("<I", data, off_field+4, len(new_comp))
    data[start_off:start_off+len(new_comp)] = new_comp
    if len(new_comp)<old_size:
        data[start_off+len(new_comp):start_off+old_size]=b"\x00"*(old_size-len(new_comp))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    report={"src":str(src),"out":str(out),"old_size":old_size,"new_size":len(new_comp),"patches":patches,"sha1":sha1_file(out)}
    return report

def copy_merge(src,dst):
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

def deploy_and_boot(built_exe, report):
    runtime_main=find_runtime_main()
    if not runtime_main:
        report.append("runtime main.exe not found; cannot deploy")
        return
    runtime_dir=runtime_main.parent
    report.append(f"runtime_main={runtime_main}")
    # backup current runtime main and deploy built
    bak=runtime_main.with_name("main.before_source_build_loader_v27.exe")
    if not bak.exists():
        shutil.copy2(runtime_main,bak)
    shutil.copy2(built_exe, runtime_main)
    report.append(f"deployed built exe to {runtime_main} sha1={sha1_file(runtime_main)}")

    # prepare mythroad
    dst=runtime_dir/"mythroad"
    src_240=ROOT/"game_files"/"mythroad"/"240x320"
    if src_240.exists():
        copy_merge(src_240,dst)
        if (src_240/"gwy").exists():
            copy_merge(src_240/"gwy",dst/"gwy")
        report.append(f"merged {src_240} -> {dst}")
    src_jjfb=find_source_jjfb(dst)
    if not src_jjfb:
        report.append("jjfb.mrp not found")
        return
    patched_jjfb=LOGS/"jjfb_skip_sdk_only_v27.mrp"
    mrp_report=patch_jjfb_skip_sdk(src_jjfb, patched_jjfb)
    log("source_build_loader_v27_mrp_patch_report.json", json.dumps(mrp_report, ensure_ascii=False, indent=2))
    report.append("mrp_patch=" + json.dumps(mrp_report, ensure_ascii=False))

    dsm=dst/"dsm_gm.mrp"
    bak_mrp=dst/"dsm_gm.before_source_build_loader_v27.mrp"
    if dsm.exists() and not bak_mrp.exists():
        shutil.copy2(dsm,bak_mrp)
    shutil.copy2(patched_jjfb,dsm)
    shutil.copy2(patched_jjfb,dst/"jjfb.mrp")
    (dst/"gwy").mkdir(parents=True, exist_ok=True)
    shutil.copy2(patched_jjfb,dst/"gwy"/"jjfb.mrp")
    for rel in ["sdk_key.dat","gwy/sdk_key.dat","gwy/jjfbol/sdk_key.dat","240x320/sdk_key.dat","240x320/gwy/sdk_key.dat"]:
        p=dst/rel; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(SDK_KEY)
    report.append("runtime prepared, booting...")

    run_cmd(["cmd","/c","taskkill /IM main.exe /F"], timeout=10)
    ts=datetime.now().strftime("%Y%m%d_%H%M%S")
    stdout=LOGS/f"source_build_loader_v27_stdout_{ts}.txt"
    with stdout.open("wb") as f:
        p=subprocess.Popen([str(runtime_main)], cwd=str(runtime_dir), stdout=f, stderr=subprocess.STDOUT)
    report.append(f"started pid={p.pid} stdout={stdout}")
    snapshots=[]
    for i in range(30):
        time.sleep(10)
        rc,ns=run_cmd(["cmd","/c","netstat -ano"], timeout=20)
        ns_name=f"source_build_loader_v27_netstat_{i:02d}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        log(ns_name, ns)
        has=bool(TARGET_NET_RE.search(ns))
        alive=p.poll() is None
        snapshots.append({"i":i,"alive":alive,"has_target":has,"netstat":ns_name})
        print(f"[{i:02d}] alive={alive} target_net={has}")
        if has:
            break
    log("source_build_loader_v27_monitor.json", json.dumps(snapshots, ensure_ascii=False, indent=2))

def restore():
    report=[]
    runtime_main=find_runtime_main()
    if runtime_main:
        for name in ["main.before_source_build_loader_v27.exe","main.before_direct_ext_boot_v26.exe","main.before_skip_loader_variants_v24.exe","main.before_direct_arg_patch_v15.exe"]:
            bak=runtime_main.with_name(name)
            if bak.exists():
                shutil.copy2(bak,runtime_main)
                report.append(f"restored main.exe from {bak}")
                break
        dst=runtime_main.parent/"mythroad"
        for name in ["dsm_gm.before_source_build_loader_v27.mrp","dsm_gm.before_direct_ext_boot_v26.mrp","dsm_gm.before_skip_loader_variants_v24.mrp","dsm_gm.before_direct_arg_patch_v15.mrp"]:
            bak=dst/name
            if bak.exists():
                shutil.copy2(bak,dst/"dsm_gm.mrp")
                report.append(f"restored dsm_gm from {bak}")
                break
    log("source_build_loader_v27_restore.txt","\n".join(report))
    print("\n".join(report))

def collect_zip():
    zipname=LOGS/f"source_build_loader_v27_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname,"w",zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p,p.relative_to(ROOT))
    print("feedback:", zipname)

def run():
    report=[]
    ok, envrep = check_env()
    report.append("ENV_OK=" + str(ok))
    try:
        vmrp_c=download_source(report)
        patch_vmrp_source(vmrp_c, report)
        built=try_build(vmrp_c, report)
        if built is not None:
            deploy_and_boot(built, report)
        else:
            report.append("NO_BUILT_EXE: see build log; install MSYS2/MinGW dependencies then rerun.")
    except Exception as e:
        report.append("ERROR: " + repr(e))
    log("source_build_loader_v27_report.txt", "\n".join(report))
    print("\n".join(report[-80:]))
    collect_zip()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["run","restore"])
    args=ap.parse_args()
    {"run":run,"restore":restore}[args.cmd]()

if __name__=="__main__":
    main()
