#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Local text-relay client: send / receive / store."""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CFG_PATH = ROOT / "config.json"


def load_config() -> dict:
    cfg = {
        "base_url": "https://jjfbol.cn/text-relay",
        "data_dir": "data",
        "poll_interval_sec": 2,
        "default_recent_n": 10,
    }
    if CFG_PATH.is_file():
        with CFG_PATH.open("r", encoding="utf-8") as f:
            cfg.update(json.load(f))
    return cfg


def data_dir(cfg: dict) -> Path:
    d = Path(cfg["data_dir"])
    if not d.is_absolute():
        d = ROOT / d
    d.mkdir(parents=True, exist_ok=True)
    return d


def inbox_path(cfg: dict) -> Path:
    return data_dir(cfg) / "inbox.jsonl"


def outbox_path(cfg: dict) -> Path:
    return data_dir(cfg) / "outbox.jsonl"


def state_path(cfg: dict) -> Path:
    return data_dir(cfg) / "state.json"


def now_iso() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


def load_state(cfg: dict) -> dict:
    p = state_path(cfg)
    if not p.is_file():
        return {"last_seen_fingerprint": None, "last_count": 0}
    with p.open("r", encoding="utf-8") as f:
        return json.load(f)


def save_state(cfg: dict, state: dict) -> None:
    with state_path(cfg).open("w", encoding="utf-8") as f:
        json.dump(state, f, ensure_ascii=False, indent=2)


def append_jsonl(path: Path, record: dict) -> None:
    with path.open("a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=False) + "\n")


def http_get(url: str, timeout: float = 30.0) -> str:
    req = urllib.request.Request(url, method="GET", headers={"User-Agent": "text-relay-local/1.0"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read().decode("utf-8", errors="replace")


def http_post_text(url: str, text: str, timeout: float = 30.0) -> str:
    data = text.encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        method="POST",
        headers={
            "User-Agent": "text-relay-local/1.0",
            "Content-Type": "text/plain; charset=utf-8",
        },
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read().decode("utf-8", errors="replace")


def api_url(cfg: dict, path: str, query: dict | None = None) -> str:
    base = cfg["base_url"].rstrip("/")
    url = f"{base}/{path.lstrip('/')}"
    if query:
        url += "?" + urllib.parse.urlencode(query)
    return url


def normalize_messages(payload: object) -> list[dict]:
    """Accept plain list / {items|messages|data: [...]} / single string."""
    if payload is None:
        return []
    if isinstance(payload, str):
        return [{"text": payload}]
    if isinstance(payload, list):
        out = []
        for i, item in enumerate(payload):
            if isinstance(item, str):
                out.append({"index": i, "text": item})
            elif isinstance(item, dict):
                text = (
                    item.get("text")
                    or item.get("content")
                    or item.get("body")
                    or item.get("msg")
                    or item.get("m")  # text-relay server field
                    or item.get("message")
                )
                if text is None:
                    text = json.dumps(item, ensure_ascii=False)
                rec = dict(item)
                rec["text"] = str(text)
                if "index" not in rec:
                    rec["index"] = item.get("idx", item.get("id", i))
                if "id" not in rec and "t" in item:
                    rec["id"] = item.get("t")
                out.append(rec)
            else:
                out.append({"index": i, "text": str(item)})
        return out
    if isinstance(payload, dict):
        for key in ("items", "messages", "data", "list", "records"):
            if key in payload and isinstance(payload[key], list):
                return normalize_messages(payload[key])
        if "text" in payload or "content" in payload:
            return normalize_messages([payload])
        # unknown object → stringify
        return [{"text": json.dumps(payload, ensure_ascii=False)}]
    return [{"text": str(payload)}]


def fetch_json_or_text(url: str) -> object:
    raw = http_get(url)
    raw_strip = raw.strip()
    if not raw_strip:
        return []
    try:
        return json.loads(raw_strip)
    except json.JSONDecodeError:
        return raw_strip


def fingerprint(msg: dict) -> str:
    idx = msg.get("id", msg.get("t", msg.get("index", msg.get("idx", ""))))
    text = msg.get("text", msg.get("m", ""))
    return f"{idx}|{text}"


def cmd_send(args: argparse.Namespace, cfg: dict) -> int:
    if args.file:
        text = Path(args.file).read_text(encoding="utf-8")
    elif args.text is not None:
        text = args.text
    elif not sys.stdin.isatty():
        text = sys.stdin.read()
    else:
        print("用法: relay.py send \"文本\"  或  relay.py send -f note.txt  或  echo hi | relay.py send", file=sys.stderr)
        return 2

    if text == "" and not args.allow_empty:
        print("空文本，已取消（需要可加 --allow-empty）", file=sys.stderr)
        return 2

    url = api_url(cfg, "send")
    try:
        resp = http_post_text(url, text)
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", errors="replace")
        print(f"发送失败 HTTP {e.code}: {body}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"发送失败: {e}", file=sys.stderr)
        return 1

    rec = {
        "ts": now_iso(),
        "direction": "out",
        "text": text,
        "server_response": resp.strip(),
    }
    append_jsonl(outbox_path(cfg), rec)
    print(f"[OK] 已发送 {len(text)} 字符 → {url}")
    if resp.strip():
        print(f"server: {resp.strip()}")
    print(f"本地: {outbox_path(cfg)}")
    return 0


def save_inbox_new(cfg: dict, messages: list[dict], seen: set[str]) -> list[dict]:
    saved = []
    for msg in messages:
        fp = fingerprint(msg)
        if fp in seen:
            continue
        rec = {
            "ts": now_iso(),
            "direction": "in",
            "index": msg.get("index"),
            "id": msg.get("id"),
            "text": msg.get("text", ""),
            "raw": {k: v for k, v in msg.items() if k != "text"} or None,
        }
        append_jsonl(inbox_path(cfg), rec)
        seen.add(fp)
        saved.append(rec)
    return saved


def cmd_recv(args: argparse.Namespace, cfg: dict) -> int:
    n = args.n if args.n is not None else int(cfg.get("default_recent_n", 10))
    url = api_url(cfg, "recent", {"n": n})
    try:
        payload = fetch_json_or_text(url)
    except Exception as e:
        print(f"接收失败: {e}", file=sys.stderr)
        return 1

    messages = normalize_messages(payload)
    state = load_state(cfg)
    last_fp = state.get("last_seen_fingerprint")
    seen = set()
    # load recent local fingerprints lightly from last lines
    inbox = inbox_path(cfg)
    if inbox.is_file():
        lines = inbox.read_text(encoding="utf-8").splitlines()[-500:]
        for line in lines:
            try:
                r = json.loads(line)
                seen.add(fingerprint({"index": r.get("index", r.get("id", "")), "text": r.get("text", "")}))
            except json.JSONDecodeError:
                pass

    new_msgs = save_inbox_new(cfg, messages, seen)

    if args.all or not last_fp:
        to_show = messages
    else:
        # show only newer than last fingerprint if possible
        to_show = []
        for m in messages:
            to_show.append(m)
            if fingerprint(m) == last_fp:
                to_show = []
        if not to_show:
            to_show = messages[-1:] if messages else []

    if args.json:
        print(json.dumps(messages, ensure_ascii=False, indent=2))
    else:
        if not messages:
            print("(空)")
        for m in to_show if args.only_new else messages:
            idx = m.get("index", m.get("id", "?"))
            text = m.get("text", "")
            print(f"----- [{idx}] -----")
            print(text)
        print(f"\n共 {len(messages)} 条；本次新写入本地 {len(new_msgs)} 条 → {inbox}")

    if messages:
        state["last_seen_fingerprint"] = fingerprint(messages[-1])
        try:
            count_raw = http_get(api_url(cfg, "count"))
            try:
                state["last_count"] = int(json.loads(count_raw).get("count", count_raw))
            except Exception:
                try:
                    state["last_count"] = int(count_raw.strip())
                except Exception:
                    pass
        except Exception:
            pass
        save_state(cfg, state)
    return 0


def cmd_watch(args: argparse.Namespace, cfg: dict) -> int:
    interval = args.interval if args.interval is not None else float(cfg.get("poll_interval_sec", 2))
    n = args.n if args.n is not None else 5
    print(f"监听中… 每 {interval}s 拉 recent?n={n}  （Ctrl+C 停止）")
    print(f"收件箱: {inbox_path(cfg)}")
    try:
        while True:
            url = api_url(cfg, "recent", {"n": n})
            try:
                payload = fetch_json_or_text(url)
                messages = normalize_messages(payload)
            except Exception as e:
                print(f"[{now_iso()}] 拉取失败: {e}", file=sys.stderr)
                time.sleep(interval)
                continue

            state = load_state(cfg)
            seen = set()
            inbox = inbox_path(cfg)
            if inbox.is_file():
                for line in inbox.read_text(encoding="utf-8").splitlines()[-500:]:
                    try:
                        r = json.loads(line)
                        seen.add(
                            fingerprint(
                                {"index": r.get("index", r.get("id", "")), "text": r.get("text", "")}
                            )
                        )
                    except json.JSONDecodeError:
                        pass

            new_msgs = save_inbox_new(cfg, messages, seen)
            for rec in new_msgs:
                print(f"\n===== 新消息 {rec.get('ts')} =====")
                print(rec.get("text", ""))
                print(f"(已存 {inbox})")

            if messages:
                state["last_seen_fingerprint"] = fingerprint(messages[-1])
                save_state(cfg, state)
            time.sleep(interval)
    except KeyboardInterrupt:
        print("\n已停止")
        return 0


def cmd_count(args: argparse.Namespace, cfg: dict) -> int:
    try:
        raw = http_get(api_url(cfg, "count"))
    except Exception as e:
        print(f"失败: {e}", file=sys.stderr)
        return 1
    print(raw.strip())
    return 0


def cmd_clear_remote(args: argparse.Namespace, cfg: dict) -> int:
    if not args.yes:
        print("确认清空远端？加上 --yes", file=sys.stderr)
        return 2
    try:
        raw = http_get(api_url(cfg, "clear"))
    except Exception as e:
        print(f"失败: {e}", file=sys.stderr)
        return 1
    print(raw.strip() or "[OK] cleared")
    return 0


def cmd_history(args: argparse.Namespace, cfg: dict) -> int:
    which = args.which
    paths = {
        "in": inbox_path(cfg),
        "out": outbox_path(cfg),
        "all": None,
    }
    files = []
    if which == "all":
        files = [inbox_path(cfg), outbox_path(cfg)]
    else:
        files = [paths[which]]

    rows = []
    for p in files:
        if not p.is_file():
            continue
        for line in p.read_text(encoding="utf-8").splitlines():
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                continue
    rows.sort(key=lambda r: r.get("ts") or "")
    if args.n:
        rows = rows[-args.n :]
    if args.json:
        print(json.dumps(rows, ensure_ascii=False, indent=2))
    else:
        if not rows:
            print("(本地无记录)")
            return 0
        for r in rows:
            d = r.get("direction", "?")
            ts = r.get("ts", "")
            text = r.get("text", "")
            print(f"[{ts}] ({d})")
            print(text)
            print("---")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="文本中继本地客户端（jjfbol.cn/text-relay）")
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("send", help="发送文本到远端并写入 outbox")
    s.add_argument("text", nargs="?", help="要发送的文本")
    s.add_argument("-f", "--file", help="从文件读取发送内容")
    s.add_argument("--allow-empty", action="store_true")
    s.set_defaults(func=cmd_send)

    r = sub.add_parser("recv", help="拉取最近消息并写入 inbox")
    r.add_argument("-n", type=int, default=None, help="最近 N 条")
    r.add_argument("--json", action="store_true")
    r.add_argument("--all", action="store_true", help="展示全部拉取到的条目")
    r.add_argument("--only-new", action="store_true", help="尽量只显示相对上次的新消息")
    r.set_defaults(func=cmd_recv)

    w = sub.add_parser("watch", help="轮询接收，新消息打印并落盘")
    w.add_argument("-n", type=int, default=5)
    w.add_argument("-i", "--interval", type=float, default=None)
    w.set_defaults(func=cmd_watch)

    c = sub.add_parser("count", help="远端总条数")
    c.set_defaults(func=cmd_count)

    cl = sub.add_parser("clear-remote", help="清空远端记录")
    cl.add_argument("--yes", action="store_true")
    cl.set_defaults(func=cmd_clear_remote)

    h = sub.add_parser("history", help="查看本地收发记录")
    h.add_argument("which", nargs="?", choices=("in", "out", "all"), default="all")
    h.add_argument("-n", type=int, default=20)
    h.add_argument("--json", action="store_true")
    h.set_defaults(func=cmd_history)

    return p


def main() -> int:
    cfg = load_config()
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args, cfg)


if __name__ == "__main__":
    raise SystemExit(main())
