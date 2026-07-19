#!/usr/bin/env python3
from pathlib import Path
from collections import Counter
import argparse,re

def count(t,p): return len(re.findall(p,t,re.M))
def ints(pat,t,group=1):
    out=[]
    for m in re.finditer(pat,t,re.M):
        try: out.append(int(m.group(group),0))
        except ValueError: pass
    return out

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('log',type=Path); ap.add_argument('--markdown',type=Path,required=True); a=ap.parse_args()
    t=a.log.read_text(encoding='utf-8',errors='replace')
    apps=ints(r'\[JJFB_V57_FAMILY_SOURCE\].*?app=(0x[0-9A-Fa-f]+|\d+)',t)
    sites=ints(r'\[JJFB_V57_FAMILY_SOURCE\].*?site_lr=(0x[0-9A-Fa-f]+|\d+)',t)
    methods=ints(r'\[JJFB_V57_ROBOTOL_EXT\] entry .*?method=(\d+)',t)
    cmds=ints(r'\[JJFB_V57_LIFECYCLE_CMD\] entry_303E14 .*?cmd=(\d+)',t)
    v={
      'family_reg':count(t,r'\[JJFB_V57_FAMILY_REG\]'),
      'family_guest_source':len(apps),
      'family_c0_source':sum(x==0xC0 for x in apps),
      'family_invoke_guest':count(t,r'\[JJFB_V57_FAMILY_INVOKE\].*source=guest_deferred_1e209'),
      'family_dispatch':count(t,r'\[JJFB_V56_FAMILY\]'),
      'family_c0_dispatch':count(t,r'\[JJFB_V56_FAMILY\].*TARGETS_2FEBBC'),
      'tick_reg':count(t,r'\[JJFB_V57_TICK_REG\]'),
      'tick_call':count(t,r'\[JJFB_V57_TICK_CALL\]'),
      'tick_entry':count(t,r'\[JJFB_V57_TICK_HANDLER\] entry_30630C'),
      'ext_entry':count(t,r'\[JJFB_V57_ROBOTOL_EXT\] entry'),
      'method1':count(t,r'\[JJFB_V57_ROBOTOL_EXT\] method1'),
      'method5':count(t,r'\[JJFB_V57_ROBOTOL_EXT\] method5_dispatch'),
      'lifecycle_cmd':count(t,r'\[JJFB_V57_LIFECYCLE_CMD\] entry_303E14'),
      'cmd10002':sum(x==10002 for x in cmds),
      'cmd10002_branch':count(t,r'\[JJFB_V57_LIFECYCLE_CMD\] command10002_branch'),
      'direct_method5':count(t,r'\[JJFB_V57_CALLBACK_SOURCE\] direct_method5'),
      'callback_register_target':count(t,r'\[JJFB_V56_CALLBACK\].*CALLBACK_2F5404'),
      'callback_entry':count(t,r'\[JJFB_V56_CALLBACK\] entry_2F5404'),
      'gate':count(t,r'gate_init_2DADC4'),
      'writer':count(t,r'uimode_writer ENTER'),
      'force':count(t,r'FORCE.*(?:0x45|ui_mode)|host.*blit',),
      'host_ext1':count(t,r'\[JJFB_801\] ext_call code=1\b'),
      'host_ext5':count(t,r'\[JJFB_801\] ext_call code=5\b'),
    }
    if v['writer']:
        verdict='Natural writer executed; v57 source task is complete.'
    elif v['gate']:
        verdict='A natural source reached 0x2DADC4; move back to the internal B70/B58/DB0 gates.'
    elif v['callback_register_target']:
        verdict='Callback registration now occurred; next distinguish registration from actual scheduler invocation.'
    elif not v['method1'] and not v['method5'] and not v['cmd10002']:
        verdict=('Callback registration producers are absent: robotol received neither EXT method 1 with command 10002 '
                 'nor EXT method 5. Separately, family app=0xC0 ingress is absent.')
    elif v['method1'] and not v['cmd10002'] and not v['method5']:
        verdict='Robotol EXT method 1 is alive, but lifecycle command 10002 was never delivered; that command is the callback-registration gate.'
    elif v['cmd10002'] and not v['callback_register_target']:
        verdict='Command 10002 reached the lifecycle dispatcher but callback registration did not complete; inspect 0x304418/0x2F5390 conditions.'
    else:
        verdict='Source coverage is partial; use the counters and histograms below to locate the first missing edge.'
    lines=['# v57 Lifecycle Source Coverage 运行结果','',f'- 日志：`{a.log}`','', '## 覆盖计数','', '| 探针 | 次数 |','|---|---:|']
    labels=[
      ('0x10102 family 注册','family_reg'),('guest family 发送源','family_guest_source'),('guest app=0xC0 源','family_c0_source'),
      ('guest-deferred family 调用','family_invoke_guest'),('family dispatcher','family_dispatch'),('family app=0xC0','family_c0_dispatch'),
      ('0x10140 tick 注册','tick_reg'),('host periodic tick 调用','tick_call'),('真实 0x30630C entry','tick_entry'),
      ('robotol EXT dispatcher entry','ext_entry'),('EXT method 1','method1'),('EXT method 5','method5'),
      ('0x303E14 lifecycle command','lifecycle_cmd'),('command 10002','cmd10002'),('10002 -> 2F5390 branch','cmd10002_branch'),
      ('method5 -> 2F5390','direct_method5'),('注册 callback=2F5405','callback_register_target'),('callback 2F5404 entry','callback_entry'),
      ('2DADC4 gate','gate'),('ui_mode writer','writer'),('host EXT code1','host_ext1'),('host EXT code5','host_ext5')]
    for lab,k in labels: lines.append(f'| {lab} | {v[k]} |')
    lines += ['', '## guest family app 分布','']
    ac=Counter(apps)
    if ac:
        lines += ['| app | 次数 |','|---|---:|']+[f'| `0x{k:X}` | {n} |' for k,n in sorted(ac.items())]
    else: lines.append('（无 guest family 发送）')
    lines += ['', '## guest family site_lr 分布','']
    sc=Counter(sites)
    if sc:
        lines += ['| site_lr | 次数 |','|---|---:|']+[f'| `0x{k:X}` | {n} |' for k,n in sc.most_common(32)]
    else: lines.append('（无）')
    lines += ['', '## robotol EXT method 分布','']
    mc=Counter(methods)
    if mc:
        lines += ['| method | 次数 |','|---|---:|']+[f'| `{k}` | {n} |' for k,n in sorted(mc.items())]
    else: lines.append('（无）')
    lines += ['', '## lifecycle command 分布','']
    cc=Counter(cmds)
    if cc:
        lines += ['| command | 次数 |','|---|---:|']+[f'| `{k}` (`0x{k:X}`) | {n} |' for k,n in sorted(cc.items())]
    else: lines.append('（无）')
    lines += ['', '## 自动判定','',f'- **{verdict}**','', '## 关键解释','',
      '- `0x10140` 的真实 handler 是 `0x30630D`（代码 `0x30630C`）；它与 `0x303E14` lifecycle-command dispatcher 不是同一函数。',
      '- callback `0x2F5405` 的自然注册生产者有两条：EXT method 1 + command 10002，或 EXT method 5。',
      '- family app=0xC0 没有在本轮被注入；guest app 分布用于证明现有 family 流量来自哪些真实 guest callsite。',
      '- 本报告不把 periodic `r0=0,r1=0` tick 误判成 lifecycle command 10002。']
    if v['force']:
        lines += ['',f'> 警告：日志中检测到 {v["force"]} 条疑似 FORCE/host blit 文本，请人工复核。']
    a.markdown.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(verdict)
if __name__=='__main__': main()
