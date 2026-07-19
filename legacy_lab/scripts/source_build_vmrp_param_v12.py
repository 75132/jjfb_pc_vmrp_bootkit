#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, zipfile, shutil, hashlib, json, re, os
from pathlib import Path
from datetime import datetime
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
LOGS=ROOT/"logs"; LOGS.mkdir(exist_ok=True)
RUNTIME=ROOT/"runtime"; RUNTIME.mkdir(exist_ok=True)
START_PARAM='napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
SRC_URL='https://github.com/vmrp/vmrp/archive/refs/heads/master.zip'

def log(n,t): (LOGS/n).write_text(t,encoding='utf-8',errors='replace')
def run(cmd,timeout=300,cwd=None):
    try:
        p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace',timeout=timeout,cwd=cwd)
        return p.returncode,p.stdout
    except subprocess.TimeoutExpired as e:
        return 124,(e.stdout or '')+'\n[TIMEOUT]\n'
def sha1(p):
    h=hashlib.sha1()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def main():
    rep=[]
    z=RUNTIME/'vmrp_master.zip'
    if not z.exists() or z.stat().st_size<100000:
        rep.append(f'downloading {SRC_URL}')
        try: urllib.request.urlretrieve(SRC_URL,z)
        except Exception as e: rep.append(f'download failed {e!r}')
    srcdir=RUNTIME/'vmrp_src_v12'
    if srcdir.exists(): shutil.rmtree(srcdir)
    srcdir.mkdir()
    with zipfile.ZipFile(z) as zz: zz.extractall(srcdir)
    vmrp_cs=list(srcdir.rglob('vmrp.c'))
    rep.append('vmrp.c candidates:\n'+'\n'.join(map(str,vmrp_cs)))
    if not vmrp_cs:
        log('source_build_report.txt','\n'.join(rep)); print('\n'.join(rep)); return
    c=vmrp_cs[0]
    text=c.read_text(encoding='utf-8',errors='replace')
    pattern=r'uint32_t\s+ret\s*=\s*bridge_dsm_mr_start_dsm\(uc,\s*filename,\s*extName,\s*NULL\);\s*printf\("bridge_dsm_mr_start_dsm\(\'%s\',\'%s\',NULL\): 0x%X\\n",\s*filename,\s*extName,\s*ret\);'
    new=('char *startParam = "'+START_PARAM+'";\n'
         '        uint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, startParam);\n'
         '        printf("bridge_dsm_mr_start_dsm(\'%s\',\'%s\',\'%s\'): 0x%X\\\\n", filename, extName, startParam, ret);')
    patched,n=re.subn(pattern,new,text)
    if n==0:
        # fallback simpler line replace, no printf edit
        patched=text.replace('uint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, NULL);',
                             'char *startParam = "'+START_PARAM+'";\n        uint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, startParam);')
        n=1 if patched!=text else 0
    c.write_text(patched,encoding='utf-8')
    shutil.copy2(c,LOGS/'vmrp_patched_v12.c')
    rep.append(f'patched n={n} copied logs/vmrp_patched_v12.c')
    repo=c.parent
    rc,out=run(['cmd','/c','where mingw32-make'],timeout=20)
    rep.append(f'where mingw32-make rc={rc}\n{out}')
    if rc==0:
        rc,out=run(['mingw32-make'],timeout=600,cwd=repo)
        rep.append(f'mingw32-make rc={rc}\n{out[-8000:]}')
        for exe in repo.rglob('main.exe'):
            dst=LOGS/f'patched_main_v12_{datetime.now().strftime("%Y%m%d_%H%M%S")}.exe'
            shutil.copy2(exe,dst)
            rep.append(f'copied {dst} sha1={sha1(dst)}')
    else:
        rep.append('No mingw32-make. Install MSYS2/MinGW or use binary patch route.')
    log('source_build_report.txt','\n'.join(rep))
    print('\n'.join(rep[-20:]))
    zipname=LOGS/f'source_build_feedback_{datetime.now().strftime("%Y%m%d_%H%M%S")}.zip'
    with zipfile.ZipFile(zipname,'w',zipfile.ZIP_DEFLATED) as zz:
        for p in LOGS.glob('*'):
            if p.is_file() and p.name!=zipname.name:
                zz.write(p,p.relative_to(ROOT))
    print('feedback:',zipname)
if __name__=='__main__': main()
