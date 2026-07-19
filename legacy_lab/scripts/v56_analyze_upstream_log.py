#!/usr/bin/env python3
from pathlib import Path
import argparse,re

def count(txt,pat): return len(re.findall(pat,txt,re.M))
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('log',type=Path); ap.add_argument('--markdown',type=Path,required=True); a=ap.parse_args()
    t=a.log.read_text(encoding='utf-8',errors='replace')
    vals={
      'event_dispatch':count(t,r'\[JJFB_V56_EVENT\]'),
      'event_5_12':count(t,r'\[JJFB_V56_EVENT\].*TARGETS_2DADC4'),
      'queue':count(t,r'\[JJFB_V56_QUEUE\]'),
      'family':count(t,r'\[JJFB_V56_FAMILY\]'),
      'family_c0':count(t,r'\[JJFB_V56_FAMILY\].*TARGETS_2FEBBC'),
      'reset_entry':count(t,r'\[JJFB_V56_RESET\] entry_2FEBBC'),
      'reset_calls':count(t,r'\[JJFB_V56_RESET\] call_2FEBBC'),
      'cb_register':count(t,r'\[JJFB_V56_CALLBACK\] register'),
      'cb_register_target':count(t,r'\[JJFB_V56_CALLBACK\] register.*CALLBACK_2F5404'),
      'cb_entry':count(t,r'\[JJFB_V56_CALLBACK\] entry_2F5404'),
      'cb_tail':count(t,r'\[JJFB_V56_CALLBACK\] tail_call_305EB8'),
      'periodic':count(t,r'\[JJFB_V56_PERIODIC\] entry_305EB8'),
      'gate':count(t,r'gate_init_2DADC4'),
      'writer':count(t,r'uimode_writer ENTER'),
      'store':count(t,r'uimode_writer STORE'),
    }
    if vals['writer']:
      verdict='Natural writer executed; inspect STORE and resulting UI state.'
    elif vals['gate']:
      verdict='Upstream reached 2DADC4; next blocker is inside B70/B58/DB0 gate logic.'
    elif vals['cb_register_target'] and not vals['cb_entry']:
      verdict='Callback 0x2F5404 was registered but never invoked: host callback scheduler/dispatch is the leading blocker.'
    elif vals['family'] and not vals['family_c0'] and not vals['event_5_12']:
      verdict='Runtime dispatch is alive, but neither family app=0xC0 nor event 5/12 was delivered; startup/event source contract is missing.'
    elif vals['queue'] and not vals['event_5_12']:
      verdict='Event queues ran, but no MR_MENU_RETURN(5)/MR_MOUSE_MOVE(12) reached the dispatcher.'
    else:
      verdict='No natural upstream source reached 2DADC4; compare registration, family and event counters below.'
    lines=['# v56 Upstream Trigger Coverage 运行结果','',f'- 日志：`{a.log}`','', '## 覆盖计数','', '| 路径/探针 | 次数 |','|---|---:|']
    labels=[('事件队列入口/调用', 'queue'),('事件分派', 'event_dispatch'),('目标事件 5/12','event_5_12'),('family dispatcher','family'),('family app=0xC0','family_c0'),('2FEBBC entry','reset_entry'),('2FEBBC direct calls','reset_calls'),('callback registration','cb_register'),('callback=2F5404 registration','cb_register_target'),('2F5404 callback entry','cb_entry'),('2F5734 -> 305EB8','cb_tail'),('305EB8 entry','periodic'),('2DADC4 gate','gate'),('ui_mode writer','writer'),('ui_mode store','store')]
    for lab,k in labels: lines.append(f'| {lab} | {vals[k]} |')
    lines += ['', '## 自动判定','', f'- **{verdict}**','', '## 解释规则','', '- 注册了 `0x2F5404` 但 entry 为 0：优先补 host callback scheduler。','- family 一直只有 app=9、没有 app=0xC0：优先追启动 family 命令来源。','- event dispatcher 有活动但没有 5/12：不要再把 `0x13` 当作自然 writer 触发源。','- 一旦命中 `0x2DADC4`，v56 上游任务完成，下一轮只查内部 ERW 门。']
    a.markdown.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(verdict)
if __name__=='__main__': main()
