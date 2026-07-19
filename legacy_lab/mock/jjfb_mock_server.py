#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import socket,threading,time,json
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]; LOGS=ROOT/'logs'; LOGS.mkdir(exist_ok=True)
J=LOGS/f"mock_localnet_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jsonl"
LOCK=threading.Lock()
ACK=bytes.fromhex('9800010000130100'); ACK2=bytes.fromhex('9800010000170100')
AUTH=bytes.fromhex('980001000021010000010001980400001d31')
NOUP=bytes.fromhex('980001000021010000010001980400001d31'+'9800010000130100'+'9800010000170100'+'9800010000130100'+'9800010000130100')
def log(o):
    o['ts']=datetime.now().strftime('%H:%M:%S.%f')[:-3]
    with LOCK:
        with J.open('a',encoding='utf-8') as f:f.write(json.dumps(o,ensure_ascii=False)+'\n')
def cls(d):
    if len(d)==103:return'LOGIN_103B'
    if len(d)==27:return'HEARTBEAT_27B'
    if len(d)==25:return'UNKNOWN_25B'
    if len(d)==48:return'UNKNOWN_48B'
    if len(d) in (270,426,430,529) or b'skymp' in d or b'android' in d:return f'DEVICE_OR_COMBINED_{len(d)}B'
    return f'UNKNOWN_{len(d)}B'
def send(c,sid,p,d,n):
    c.sendall(d); log({'sid':sid,'port':p,'dir':'send','len':len(d),'hex':d.hex(),'note':n})
def h(c,a,p):
    sid=f"{p}-{int(time.time()*1000)%100000}"; hb=0
    log({'sid':sid,'port':p,'dir':'open','peer':str(a),'sockname':str(c.getsockname())})
    c.settimeout(120)
    try:
        while True:
            d=c.recv(8192)
            if not d: log({'sid':sid,'port':p,'dir':'eof'}); break
            k=cls(d); log({'sid':sid,'port':p,'dir':'recv','len':len(d),'class':k,'hex':d.hex()})
            if p in (21002,21003):
                send(c,sid,p,AUTH+ACK2 if k=='LOGIN_103B' else ACK,'platform')
            elif p==20000:
                if k=='LOGIN_103B': send(c,sid,p,AUTH+ACK2+ACK,'login34')
                elif k.startswith('DEVICE'):
                    if len(d)>=500:
                        send(c,sid,p,AUTH+ACK2+ACK,'combined_login_part')
                        time.sleep(0.05)
                    send(c,sid,p,ACK,'device_ack')
                elif k=='HEARTBEAT_27B':
                    hb+=1; send(c,sid,p,ACK if hb==1 else NOUP,'heartbeat')
                elif k=='UNKNOWN_25B':
                    send(c,sid,p,NOUP,'25B_no_update')
                else:
                    send(c,sid,p,ACK,'default')
            else:
                send(c,sid,p,ACK,'default')
    except Exception as e:
        log({'sid':sid,'port':p,'dir':'error','err':repr(e)})
    try:c.close()
    except:pass
    log({'sid':sid,'port':p,'dir':'close'})
def srv(p):
    s=socket.socket(); s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    s.bind(('0.0.0.0',p)); s.listen(50); print('listen',p,flush=True)
    while True:
        c,a=s.accept(); threading.Thread(target=h,args=(c,a,p),daemon=True).start()
for p in [21002,21003,20000,6009]: threading.Thread(target=srv,args=(p,),daemon=True).start()
print('mock jsonl',J,flush=True)
while True:time.sleep(3600)
