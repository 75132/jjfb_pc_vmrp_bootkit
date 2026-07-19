#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os, re, struct, json, zlib, zipfile, hashlib, subprocess, shutil
from pathlib import Path
from datetime import datetime
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"
LOGS.mkdir(exist_ok=True)
RUNTIME = ROOT / "runtime"
RUNTIME.mkdir(exist_ok=True)

SRC_URL = "https://github.com/vmrp/vmrp/archive/refs/heads/master.zip"

SEARCH_TERMS = [
    "_mr_c_load", "_mr_c_buf", "hsman", "mrc_loader", "robotol",
    "bridge_dsm_mr_start_dsm", "mr_load", "mr_c_function", "MRPGCMAP",
    "ext", "mr_open", "mr_get_method", "start.mr"
]

def log(name, text):
    (LOGS/name).write_text(text, encoding="utf-8", errors="replace")

def sha1_file(p):
    h=hashlib.sha1()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024), b""):
            h.update(b)
    return h.hexdigest()

def run_cmd(cmd, timeout=60, cwd=None):
    try:
        p=subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, encoding="utf-8", errors="replace",
                         timeout=timeout, cwd=cwd)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "") + "\n[TIMEOUT]\n"
    except Exception as e:
        return 999, repr(e)

def find_main():
    c=[]
    root=ROOT/"runtime"/"vmrp_win32"
    if root.exists():
        for p in root.rglob("main.exe"):
            if p.name.lower()=="main.exe":
                c.append(p)
    c.sort(key=lambda p: len(str(p)))
    return c[0] if c else None

def scan_binary():
    report=[]
    exe=find_main()
    if not exe:
        report.append("main.exe not found")
        log("loader_binary_scan.txt", "\n".join(report))
        return
    data=exe.read_bytes()
    report.append(f"main={exe}")
    report.append(f"size={len(data)} sha1={sha1_file(exe)}")
    # ASCII strings containing terms
    strings=[]
    for m in re.finditer(rb"[\x20-\x7e]{4,}", data):
        s=m.group().decode("ascii","replace")
        if any(t in s for t in SEARCH_TERMS):
            strings.append((m.start(), s))
    report.append("interesting strings:")
    for off,s in strings[:500]:
        report.append(f"0x{off:x}: {s}")
    # Raw occurrences
    report.append("\nraw term offsets:")
    for term in SEARCH_TERMS:
        b=term.encode("ascii", "ignore")
        offs=[m.start() for m in re.finditer(re.escape(b), data)]
        report.append(f"{term}: count={len(offs)} offsets={[hex(x) for x in offs[:30]]}")
    log("loader_binary_scan.txt", "\n".join(report))

# Parse start.mr bytecode.
def read_u32(buf, off):
    return int.from_bytes(buf[off:off+4], "little"), off+4
def read_u8(buf, off):
    return buf[off], off+1
def read_str(buf, off):
    l, off = read_u32(buf, off)
    raw = buf[off:off+l]
    off += l
    return raw.rstrip(b"\x00").decode("ascii", "replace"), off
def parse_consts(buf, off):
    n, off = read_u32(buf, off)
    consts=[]
    for i in range(n):
        c_off=off
        tag=buf[off]; off += 1
        if tag == 3:
            val=int.from_bytes(buf[off:off+4],"little",signed=True); off += 4
        elif tag == 4:
            l, off = read_u32(buf, off)
            raw=buf[off:off+l]; off += l
            val=raw.rstrip(b"\x00").decode("ascii","replace")
        elif tag == 5:
            val=None
        else:
            raise RuntimeError(f"unknown const tag {tag} at 0x{c_off:x}")
        consts.append({"idx":i,"off":hex(c_off),"tag":tag,"val":val})
    return consts, off
def parse_func(buf, off, top=False):
    start=off
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
    lineinfo_off=off
    lineinfo=[int.from_bytes(buf[off+i*4:off+i*4+4],"little") for i in range(sizeline)]
    off += sizeline*4
    nloc, off = read_u32(buf, off)
    locals_=[]
    for _ in range(nloc):
        name, off = read_str(buf, off)
        sp, off = read_u32(buf, off)
        ep, off = read_u32(buf, off)
        locals_.append({"name":name,"startpc":sp,"endpc":ep})
    nupnames, off = read_u32(buf, off)
    upnames=[]
    for _ in range(nupnames):
        name, off = read_str(buf, off)
        upnames.append(name)
    consts, off = parse_consts(buf, off)
    nproto, off = read_u32(buf, off)
    protos=[]
    for _ in range(nproto):
        pr, off = parse_func(buf, off, top=False)
        protos.append(pr)
    ncode, off = read_u32(buf, off)
    code_off=off
    code=[int.from_bytes(buf[off+i*4:off+i*4+4],"little") for i in range(ncode)]
    off += ncode*4
    return {"start":start,"end":off,"src":src,"linedef":linedef,"nups":nups,"numparams":numparams,"vararg":vararg,"maxstack":maxstack,"lineinfo":lineinfo,"lineinfo_off":lineinfo_off,"locals":locals_,"upnames":upnames,"consts":consts,"protos":protos,"code_off":code_off,"code":code}, off

def decode_inst(inst):
    op=inst & 0x3f
    A=(inst >> 6) & 0xff
    C=(inst >> 14) & 0x1ff
    B=(inst >> 23) & 0x1ff
    Bx=(inst >> 14) & 0x3ffff
    sBx=Bx - 131071
    return {"hex":f"{inst:08x}","op":op,"A":A,"B":B,"C":C,"Bx":Bx,"sBx":sBx}

def line_groups(f):
    groups=[]
    last=None
    for i,l in enumerate(f["lineinfo"], start=1):
        if l != last:
            groups.append({"pc":i,"line":l})
            last=l
    return groups

def find_source_jjfb():
    candidates=[
        ROOT/"game_files"/"mythroad"/"240x320"/"gwy"/"jjfb.mrp",
        ROOT/"game_files"/"mythroad"/"gwy"/"jjfb.mrp",
    ]
    exe=find_main()
    if exe:
        dst=exe.parent/"mythroad"
        candidates += [dst/"gwy"/"jjfb.mrp", dst/"jjfb.mrp", dst/"dsm_gm.mrp"]
    for p in candidates:
        if p.exists():
            return p
    return None

def extract_start_mr():
    report={}
    src=find_source_jjfb()
    if not src:
        log("start_mr_audit_error.txt", "jjfb.mrp not found")
        return
    data=src.read_bytes()
    marker=b"start.mr\x00"
    pos=data.find(marker)
    if pos < 0:
        log("start_mr_audit_error.txt", f"start.mr marker not found in {src}")
        return
    off_field=pos+len(marker)
    start_off=struct.unpack_from("<I", data, off_field)[0]
    comp_size=struct.unpack_from("<I", data, off_field+4)[0]
    comp=data[start_off:start_off+comp_size]
    decomp=zlib.decompress(comp, wbits=31)
    out=LOGS/"start_mr_original.bin"
    out.write_bytes(decomp)
    f,end=parse_func(decomp,0,top=True)
    report["src"]=str(src)
    report["src_sha1"]=sha1_file(src)
    report["start_off"]=hex(start_off)
    report["comp_size"]=comp_size
    report["decomp_len"]=len(decomp)
    report["parse_end"]=hex(end)
    report["top"]={k:f[k] for k in ["src","linedef","nups","numparams","vararg","maxstack","code_off"]}
    report["line_groups"]=line_groups(f)
    report["interesting_consts"]=[c for c in f["consts"] if isinstance(c["val"], str) and any(t in c["val"] for t in ["_mr_c","hsman","loader","robotol","sdk","mrc","gwy","IMEI","hstype","strCom","com","gc","error"])]
    # Dump all consts too
    log("start_mr_consts.json", json.dumps(f["consts"], ensure_ascii=False, indent=2))
    # Dump PCs around 90-150 plus all line groups >130.
    pcs=[]
    for pc, inst in enumerate(f["code"], start=1):
        if 90 <= pc <= 150 or f["lineinfo"][pc-1] >= 130:
            d=decode_inst(inst)
            d.update({"pc":pc, "line":f["lineinfo"][pc-1], "file_off":hex(f["code_off"]+(pc-1)*4)})
            pcs.append(d)
    log("start_mr_pc90_150.json", json.dumps(pcs, ensure_ascii=False, indent=2))
    log("start_mr_audit_report.json", json.dumps(report, ensure_ascii=False, indent=2))

def download_and_scan_source():
    report=[]
    srczip=RUNTIME/"vmrp_master.zip"
    srcdir=RUNTIME/"vmrp_src_audit_v25"
    if not srczip.exists() or srczip.stat().st_size < 100000:
        report.append(f"downloading {SRC_URL}")
        try:
            urllib.request.urlretrieve(SRC_URL, srczip)
            report.append(f"downloaded {srczip} size={srczip.stat().st_size}")
        except Exception as e:
            report.append(f"python download failed: {e!r}")
            rc,out=run_cmd(["powershell","-NoProfile","-ExecutionPolicy","Bypass","-Command",f"Invoke-WebRequest -Uri '{SRC_URL}' -OutFile '{srczip}'"], timeout=180)
            report.append(f"powershell download rc={rc}\n{out[-2000:]}")
    if srcdir.exists():
        shutil.rmtree(srcdir)
    srcdir.mkdir(parents=True, exist_ok=True)
    if srczip.exists() and srczip.stat().st_size > 100000:
        try:
            with zipfile.ZipFile(srczip) as z:
                z.extractall(srcdir)
            report.append(f"extracted source to {srcdir}")
        except Exception as e:
            report.append(f"extract failed: {e!r}")
    # Search current extracted source plus any old vmrp_src dirs.
    roots=[]
    if srcdir.exists():
        roots.append(srcdir)
    for p in RUNTIME.glob("vmrp_src*"):
        if p.is_dir() and p not in roots:
            roots.append(p)
    hits=[]
    for root in roots:
        for p in root.rglob("*"):
            if not p.is_file():
                continue
            if p.suffix.lower() not in [".c",".h",".cpp",".hpp",".md",".txt",".mk",".lua"]:
                continue
            try:
                text=p.read_text(encoding="utf-8", errors="replace")
            except Exception:
                continue
            for term in SEARCH_TERMS:
                if term in text:
                    for m in re.finditer(re.escape(term), text):
                        line=text.count("\n",0,m.start())+1
                        # Extract context line
                        start=text.rfind("\n",0,m.start())+1
                        end=text.find("\n",m.start())
                        if end < 0: end=len(text)
                        hits.append({"file":str(p), "term":term, "line":line, "context":text[start:end][:300]})
    log("vmrp_source_search_hits.json", json.dumps(hits, ensure_ascii=False, indent=2))
    report.append(f"source hits={len(hits)}")
    for h in hits[:200]:
        report.append(f"{h['file']}:{h['line']} [{h['term']}] {h['context']}")
    log("vmrp_source_audit_report.txt", "\n".join(report))

def collect_zip():
    zipname=LOGS/f"loader_source_audit_v25_feedback_{datetime.now().strftime('%Y%m%d_%H%M%S')}.zip"
    with zipfile.ZipFile(zipname, "w", zipfile.ZIP_DEFLATED) as z:
        for p in LOGS.glob("*"):
            if p.is_file() and p.name != zipname.name:
                z.write(p, p.relative_to(ROOT))
    print("feedback:", zipname)

def main():
    scan_binary()
    extract_start_mr()
    download_and_scan_source()
    collect_zip()

if __name__ == "__main__":
    main()
