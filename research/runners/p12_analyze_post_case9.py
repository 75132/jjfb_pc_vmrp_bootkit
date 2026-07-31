#!/usr/bin/env python3
"""P12: parse product/P11 logs after first CASE9_LEAVE (or DELIVER_DONE) into timeline + hot blocks."""
from __future__ import annotations

import csv
import hashlib
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out" / "p12"
REPORTS = ROOT / "reports"

PATTERNS = [
    ("case9_leave", re.compile(r"CASE9_LEAVE\s+ok=(\d+).*pc_after=(0x[0-9A-Fa-f]+).*ret=(-?\d+)")),
    ("family_deliver_done", re.compile(r"\[PLATFORM_FAMILY_EVENT\]\s+op=DELIVER_DONE\s+ok=(\d+).*handler=(0x[0-9A-Fa-f]+).*pc_after=(0x[0-9A-Fa-f]+).*lr_after=(0x[0-9A-Fa-f]+)")),
    ("scheduler_cb", re.compile(r"\[SCHEDULER_NATURAL_CALLBACK\].*handler=(0x[0-9A-Fa-f]+).*ret=(-?\d+)")),
    ("plat_call", re.compile(r"\[JJFB_PLAT_CALL\]\s+code=(0x[0-9A-Fa-f]+).*?(?:name=(\S+))?")),
    ("extchunk_call", re.compile(r"\[JJFB_EXTCHUNK_SLOT_CALL\].*pc=(0x[0-9A-Fa-f]+)\s+r0=(0x[0-9A-Fa-f]+)\s+r1=(0x[0-9A-Fa-f]+).*ret=(0x[0-9A-Fa-f]+)")),
    ("indirect", re.compile(r"\[GUEST_INDIRECT_CALL\].*pc=(0x[0-9A-Fa-f]+)\s+target=(0x[0-9A-Fa-f]+)\s+arg0=(0x[0-9A-Fa-f]+)\s+arg1=(0x[0-9A-Fa-f]+)")),
    ("cfunction_new", re.compile(r"ext call _mr_c_function_new\((0x[0-9A-Fa-f]+)\[(\d+)\],\s*(0x[0-9A-Fa-f]+)\[(\d+)\]\)")),
    ("module_reg", re.compile(r"\[MODULE_REGISTRY\]\s+package=(\S+)\s+module=(\S+)\s+module_id=(\d+).*state=(\S+)")),
    ("draw", re.compile(r"\[JJFB_DRAW\]\s+api=(\S+)\s+bmp=(0x[0-9A-Fa-f]+)")),
    ("e8z", re.compile(r"\[JJFB_E8Z_CLASS\]\s+class=(\S+)")),
    ("unimpl", re.compile(r"\[POST_CONT_UNIMPLEMENTED_API\]\s+api=(\S+)\s+slot=(0x[0-9A-Fa-f]+)")),
    ("unimpl_bang", re.compile(r"!!!\s*(mr_\w+)\(\)\s*Not yet implemented", re.I)),
    ("family_enq", re.compile(r"\[PLATFORM_FAMILY_EVENT\]\s+op=ENQUEUE\s+event=(0x[0-9A-Fa-f]+)\s+app=(0x[0-9A-Fa-f]+)\s+handler=(0x[0-9A-Fa-f]+)")),
    ("bmp_req", re.compile(r"\[JJFB_BMP_REQ\]\s+name=(\S+)")),
    ("first_resource", re.compile(r"\[FIRST_NATURAL_RESOURCE_REQUEST\]\s+path=(\S+)")),
    ("first_refresh", re.compile(r"\[FIRST_NATURAL_REFRESH\]")),
    ("first_draw", re.compile(r"\[FIRST_NATURAL_DRAW\]|\[JJFB_FIRST_REAL_DRAW_CANDIDATE\]")),
    ("p3_fault", re.compile(r"\[P3_FAULT\]|FIRE_DONE ok=0")),
    ("timer", re.compile(r"\[PLATFORM_TIMER|timer.?callback|TIMER_", re.I)),
    ("net", re.compile(r"socket|connect|net_|NET_|mr_initNetwork|initNetwork", re.I)),
    ("exit_park", re.compile(r"EXIT_PARK|process.?exit|StartMR|initMemoryManager", re.I)),
    ("window", re.compile(r"\[JJFB_WINDOW\]")),
    ("callback_frame", re.compile(r"\[CALLBACK_FRAME\]\s+boundary=(\S+).*slot_name=(\S+).*call_pc=(0x[0-9A-Fa-f]+).*continuation_pc=(0x[0-9A-Fa-f]+)")),
]


def load_lines(paths):
    lines = []
    for p in paths:
        path = Path(p)
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace").splitlines()
        lines.extend(text)
    return lines


def find_t0(lines):
    for i, line in enumerate(lines):
        if "CASE9_LEAVE" in line and "ok=1" in line:
            return i, "CASE9_LEAVE"
        if "[PLATFORM_FAMILY_EVENT]" in line and "op=DELIVER_DONE" in line and "ok=1" in line and "handler=0x30D311" in line:
            return i, "DELIVER_DONE"
    return None, None


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    REPORTS.mkdir(parents=True, exist_ok=True)
    args = sys.argv[1:]
    if not args:
        args = [
            str(ROOT / "logs" / "p12_successor_stdout.txt"),
            str(ROOT / "logs" / "p12_successor_vmrp.txt"),
            str(ROOT / "logs" / "p11_natural_case9_stdout.txt"),
        ]
    lines = load_lines(args)
    t0, t0_kind = find_t0(lines)
    if t0 is None:
        print("NO_T0", file=sys.stderr)
        sys.exit(2)

    window = lines[t0 : t0 + 800]
    timeline_path = REPORTS / "p12_post_case9_timeline.csv"
    hot_path = REPORTS / "p12_post_case9_hot_blocks.csv"
    summary_path = OUT / "p12_post_case9_summary.txt"

    rows = []
    pcs = []
    seq = 0
    firsts = {
        "plat_101xx": None,
        "sendAppEvent": None,
        "resource": None,
        "timer": None,
        "scheduler": None,
        "network": None,
        "module_switch": None,
        "exception_or_exit": None,
        "unimpl_api": None,
    }

    for off, line in enumerate(window):
        for kind, rx in PATTERNS:
            m = rx.search(line)
            if not m:
                continue
            seq += 1
            pc = ""
            lr = ""
            r0 = ""
            r1 = ""
            event_id = ""
            plat = ""
            resource = ""
            branch = ""
            result = ""
            op = kind
            if kind == "case9_leave":
                result = f"ok={m.group(1)} pc_after={m.group(2)} ret={m.group(3)}"
            elif kind == "family_deliver_done":
                plat = "PLATFORM_FAMILY_EVENT"
                result = f"ok={m.group(1)} handler={m.group(2)}"
                pc = m.group(3)
                lr = m.group(4)
            elif kind == "scheduler_cb":
                if firsts["scheduler"] is None:
                    firsts["scheduler"] = off
                plat = "SCHEDULER_NATURAL_CALLBACK"
                result = f"handler={m.group(1)} ret={m.group(2)}"
            elif kind == "plat_call":
                code = m.group(1)
                if firsts["plat_101xx"] is None and code.lower().startswith("0x101"):
                    firsts["plat_101xx"] = off
                plat = m.group(2) or code
                event_id = code
            elif kind == "extchunk_call":
                if firsts["sendAppEvent"] is None:
                    firsts["sendAppEvent"] = off
                pc, r0, r1, result = m.group(1), m.group(2), m.group(3), f"ret={m.group(4)}"
                event_id = r0
                plat = "sendAppEvent/extchunk"
            elif kind == "indirect":
                pc, branch, r0, r1 = m.groups()
                pcs.append(int(pc, 16))
            elif kind == "cfunction_new":
                op = "cfunction_new"
                r0, _, r1, _ = m.groups()
                result = f"helper={m.group(1)} len={m.group(4)}"
            elif kind == "module_reg":
                if firsts["module_switch"] is None and m.group(4) in ("MAPPED", "REGISTERED", "EXTRACTED"):
                    if "mmochat" in m.group(2) or "wxjwq" in m.group(1):
                        firsts["module_switch"] = off
                resource = f"{m.group(1)}:{m.group(2)}"
                result = f"id={m.group(3)} state={m.group(4)}"
            elif kind == "draw":
                plat = m.group(1)
                result = f"bmp={m.group(2)}"
            elif kind == "e8z":
                result = m.group(1)
            elif kind in ("unimpl", "unimpl_bang"):
                if firsts["unimpl_api"] is None:
                    firsts["unimpl_api"] = off
                if firsts["exception_or_exit"] is None:
                    firsts["exception_or_exit"] = off
                plat = m.group(1)
                result = "UNIMPLEMENTED"
                if kind == "unimpl":
                    branch = m.group(2)
            elif kind == "bmp_req" or kind == "first_resource":
                if firsts["resource"] is None:
                    firsts["resource"] = off
                resource = m.group(1) if m.lastindex else "yes"
            elif kind == "timer":
                if firsts["timer"] is None:
                    firsts["timer"] = off
            elif kind == "net":
                if firsts["network"] is None:
                    firsts["network"] = off
            elif kind == "p3_fault":
                if firsts["exception_or_exit"] is None:
                    firsts["exception_or_exit"] = off
                result = "P3_FAULT"
            elif kind == "callback_frame":
                op = f"callback:{m.group(1)}"
                plat = m.group(2)
                pc = m.group(3)
                branch = m.group(4)
            rows.append(
                {
                    "sequence": seq,
                    "time_from_case9_leave": off,  # line offset proxy
                    "guest_instruction_index": "",
                    "PC": pc,
                    "LR": lr,
                    "SP": "",
                    "R0": r0,
                    "R1": r1,
                    "R2": "",
                    "R3": "",
                    "event_request_id": event_id,
                    "operation_type": op,
                    "platform_function": plat,
                    "resource_name": resource,
                    "branch_target": branch,
                    "result": result,
                    "raw": line[:240].replace(",", ";"),
                }
            )
            break

    # Hot blocks from guest PCs in window
    ctr = Counter(pcs)
    with hot_path.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["rank", "pc", "hits", "note"])
        for i, (pc, hits) in enumerate(ctr.most_common(32), 1):
            w.writerow([i, f"0x{pc:X}", hits, "GUEST_INDIRECT_CALL_pc"])
        if not ctr:
            # fallback: parse pc= from window
            pc_rx = re.compile(r"\bpc=(0x[0-9A-Fa-f]+)")
            fallback = Counter()
            for line in window[:400]:
                for m in pc_rx.finditer(line):
                    fallback[int(m.group(1), 16)] += 1
            for i, (pc, hits) in enumerate(fallback.most_common(32), 1):
                w.writerow([i, f"0x{pc:X}", hits, "log_pc_token"])

    fields = [
        "sequence",
        "time_from_case9_leave",
        "guest_instruction_index",
        "PC",
        "LR",
        "SP",
        "R0",
        "R1",
        "R2",
        "R3",
        "event_request_id",
        "operation_type",
        "platform_function",
        "resource_name",
        "branch_target",
        "result",
        "raw",
    ]
    with timeline_path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for row in rows:
            w.writerow(row)

    # Distinct PCs last 256
    distinct = []
    seen = set()
    for pc in reversed(pcs):
        if pc in seen:
            continue
        seen.add(pc)
        distinct.append(pc)
        if len(distinct) >= 256:
            break
    distinct.reverse()
    (OUT / "p12_last_256_guest_pcs.txt").write_text(
        "\n".join(f"0x{p:X}" for p in distinct) + ("\n" if distinct else "NONE\n"),
        encoding="utf-8",
    )

    stall = "A"
    blocker = "unknown"
    if firsts["unimpl_api"] is not None:
        stall = "A"
        # find api name
        for row in rows:
            if row["operation_type"] in ("unimpl", "unimpl_bang"):
                blocker = row["platform_function"]
                break
    elif firsts["module_switch"] is not None and firsts["exception_or_exit"] is None:
        stall = "D"
        blocker = "post_case9_module_or_resource"
    elif firsts["scheduler"] is not None and firsts["plat_101xx"] is None and firsts["unimpl_api"] is None:
        stall = "B"
        blocker = "wait_loop_after_scheduler"

    summary = [
        f"t0_kind={t0_kind}",
        f"t0_line={t0}",
        f"timeline_rows={len(rows)}",
        f"stall_type={stall}",
        f"blocker={blocker}",
        f"first_plat_101xx_off={firsts['plat_101xx']}",
        f"first_sendAppEvent_off={firsts['sendAppEvent']}",
        f"first_resource_off={firsts['resource']}",
        f"first_timer_off={firsts['timer']}",
        f"first_scheduler_off={firsts['scheduler']}",
        f"first_network_off={firsts['network']}",
        f"first_module_switch_off={firsts['module_switch']}",
        f"first_exception_or_exit_off={firsts['exception_or_exit']}",
        f"first_unimpl_api_off={firsts['unimpl_api']}",
        f"timeline={timeline_path}",
        f"hot_blocks={hot_path}",
    ]
    summary_path.write_text("\n".join(summary) + "\n", encoding="utf-8")
    print("\n".join(summary))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
