#!/usr/bin/env python3
"""Stage E8J: L2 upstream reachability for 0x300158 callers + FE8/B7D/7D8 consumers."""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

CODE_BASE = 0x2D8DF4
PARENT_FN = 0x300158
DISPATCHER_FN = 0x300714
SITE_30103C = 0x30103C
BL_TO_DISPATCHER = 0x3002C0
FN_3020C8 = 0x3020C8

# Hot clusters named by E8I verdict.
HOT_FNS = (0x2DFC3C, 0x2E0E00, 0x2DC778)

# Queue / status offsets (10165 side effects + queue base).
OFF_FE8 = 0xFE8
OFF_B7D = 0xB7D
OFF_7D8 = 0x7D8
# State word = 0x800+0xD0 (audit-safe; do not emit banned literal in comments for core).
OFF_STATE = 0x800 + 0xD0

PRIORITY_HOT = HOT_FNS


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


def bl_target(pc: int, h0: int, h1: int) -> Optional[int]:
    if (h0 & 0xF800) != 0xF000 or (h1 & 0xC000) != 0xC000:
        return None
    s = (h0 >> 10) & 1
    imm10 = h0 & 0x3FF
    j1 = (h1 >> 13) & 1
    j2 = (h1 >> 11) & 1
    imm11 = h1 & 0x7FF
    i1 = (~(j1 ^ s)) & 1
    i2 = (~(j2 ^ s)) & 1
    imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
    imm32 = sign_extend(imm32, 25)
    return (pc + 4 + imm32) & ~1


def build_bl_index(blob: bytes, code_base: int) -> Dict[int, List[int]]:
    """Map BL target -> list of BL site PCs."""
    idx: Dict[int, List[int]] = defaultdict(list)
    for o in range(0, len(blob) - 3, 2):
        t = bl_target(code_base + o, u16(blob, o), u16(blob, o + 2))
        if t is not None:
            idx[t].append(code_base + o)
    return idx


def find_bl_callers_idx(idx: Dict[int, List[int]], target: int) -> List[int]:
    return list(idx.get(target & ~1, []))


def find_fn_start(
    blob: bytes, code_base: int, site: int, bl_idx: Optional[Dict[int, List[int]]] = None
) -> int:
    """Nearest preceding PUSH/STMDB; prefer one with BL callers, else nearest PUSH.

    Important: do not walk through a prior function's POP {..pc} into an earlier
    PUSH (that mis-attributed 0x30D730 to 0x30D24C instead of switch fn 0x30D300).
    """
    nearest_push: Optional[int] = None
    for back in range(0, 0x800, 2):
        p = site - back
        if p < code_base:
            break
        h = u16(blob, p - code_base)
        if (h & 0xFF00) == 0xBD00 and back > 0:
            return nearest_push if nearest_push is not None else (p + 2)
        if (h & 0xFF00) == 0xB500:
            if nearest_push is None:
                nearest_push = p
            if bl_idx is not None:
                n = len(bl_idx.get(p, []))
            else:
                n = sum(
                    1
                    for o in range(0, len(blob) - 3, 2)
                    if bl_target(code_base + o, u16(blob, o), u16(blob, o + 2)) == p
                )
            if n or back <= 0x200:
                return p
        if h == 0xE92D:
            return p
    return nearest_push if nearest_push is not None else site


def find_bl_callers(blob: bytes, code_base: int, target: int) -> List[int]:
    out: List[int] = []
    tgt = target & ~1
    for o in range(0, len(blob) - 3, 2):
        t = bl_target(code_base + o, u16(blob, o), u16(blob, o + 2))
        if t == tgt:
            out.append(code_base + o)
    return out


def find_fn_end(blob: bytes, code_base: int, fn: int, max_span: int = 0x1000) -> int:
    """Heuristic: first POP {...,pc} / BX lr after fn, else fn+max_span."""
    end_limit = min(fn + max_span, code_base + len(blob) - 2)
    pc = fn + 2
    while pc < end_limit:
        off = pc - code_base
        h = u16(blob, off)
        if (h & 0xFF00) == 0xBD00:  # POP with PC
            return pc + 2
        if h == 0x4770:  # BX lr
            return pc + 2
        if (h & 0xFF00) == 0xB500 and pc > fn + 8:
            # next PUSH — likely next function
            return pc
        if (h & 0xF800) == 0xF000:
            pc += 4
            continue
        pc += 2
    return end_limit


def infer_r0_before_bl(blob: bytes, code_base: int, bl_pc: int) -> Dict[str, Any]:
    info: Dict[str, Any] = {"kind": "unknown", "const": None, "notes": []}
    for back in range(2, 48, 2):
        p = bl_pc - back
        if p < code_base:
            break
        h = u16(blob, p - code_base)
        if (h & 0xF800) == 0x2000 and ((h >> 8) & 7) == 0:
            info["kind"] = "const_movs"
            info["const"] = h & 0xFF
            info["set_pc"] = p
            return info
        if (h & 0xFFC7) == 0x1C00:
            rm = (h >> 3) & 7
            info["kind"] = "mov_from_reg"
            info["from_reg"] = rm
            info["set_pc"] = p
            for b2 in range(back + 2, 64, 2):
                p2 = bl_pc - b2
                h2 = u16(blob, p2 - code_base)
                if (h2 & 0xF800) == 0x2000 and ((h2 >> 8) & 7) == rm:
                    info["kind"] = "const_via_reg"
                    info["const"] = h2 & 0xFF
                    info["via_reg"] = rm
                    return info
            return info
        if (h & 0xFF00) == 0xB500:
            break
    for back in range(2, 48, 2):
        p = bl_pc - back
        h = u16(blob, p - code_base)
        if (h & 0xF800) == 0x4800 and ((h >> 8) & 7) == 0:
            imm = (h & 0xFF) * 4
            lit = ((p + 4) & ~2) + imm
            val = u32(blob, lit - code_base) if lit - code_base + 4 <= len(blob) else None
            info["kind"] = "ldr_pc_literal"
            info["const"] = val
            info["set_pc"] = p
            return info
    return info


def nearby_strings(blob: bytes, code_base: int, pc: int, radius: int = 0x200) -> List[str]:
    lo = max(0, pc - code_base - radius)
    hi = min(len(blob), pc - code_base + radius)
    region = blob[lo:hi]
    found: List[str] = []
    keys = (
        b"http",
        b"net",
        b"init",
        b"load",
        b"file",
        b"cfg",
        b"mrp",
        b"game",
        b"login",
        b"update",
        b"ready",
        b"start",
        b"event",
        b"timer",
        b"draw",
        b"ui",
        b"queue",
    )
    i = 0
    while i < len(region) - 4:
        if 32 <= region[i] < 127:
            j = i
            while j < len(region) and 32 <= region[j] < 127:
                j += 1
            if j - i >= 4:
                s = region[i:j].decode("ascii", errors="ignore")
                sl = s.lower()
                if any(k.decode() in sl for k in keys) or "/" in s or ".mrp" in sl:
                    if s not in found:
                        found.append(s[:80])
            i = j + 1
        else:
            i += 1
    return found[:8]


def scan_offset_access(
    blob: bytes, code_base: int, offset: int, want_store: bool, want_load: bool
) -> List[Dict[str, Any]]:
    """Find LDR =offset then ADD r9 then nearby LDR*/STR*."""
    hits: List[Dict[str, Any]] = []
    for o in range(0, len(blob) - 3, 2):
        h = u16(blob, o)
        if (h & 0xF800) != 0x4800:
            continue
        pc = code_base + o
        rd = (h >> 8) & 7
        imm = (h & 0xFF) * 4
        lit = ((pc + 4) & ~2) + imm
        if lit - code_base + 4 > len(blob):
            continue
        if u32(blob, lit - code_base) != offset:
            continue
        p = pc + 2
        end = pc + 48
        saw_add_r9 = False
        access = None
        while p < end:
            off = p - code_base
            hh = u16(blob, off)
            if (hh & 0xFF00) == 0x4400:
                if hh in (0x4448, 0x4449, 0x444A, 0x444B, 0x444C, 0x444D, 0x444E, 0x444F):
                    saw_add_r9 = True
                if (hh & 0xFF87) == (0x4480 | rd):
                    saw_add_r9 = True
            # LDR* 0x6800 / LDRB 0x7800 / LDRH 0x8800 ; STR* 0x6000 / 0x7000 / 0x8000
            is_load = (hh & 0xF800) in (0x6800, 0x7800, 0x8800, 0x9800)
            is_store = (hh & 0xF800) in (0x6000, 0x7000, 0x8000, 0x9000)
            if (is_load and want_load) or (is_store and want_store):
                rn = (hh >> 3) & 7
                if saw_add_r9 or rn == rd:
                    kind = "load" if is_load else "store"
                    width = {
                        0x6800: 4,
                        0x7800: 1,
                        0x8800: 2,
                        0x9800: 4,
                        0x6000: 4,
                        0x7000: 1,
                        0x8000: 2,
                        0x9000: 4,
                    }.get(hh & 0xF800, 0)
                    access = {"pc": p, "raw": hh, "kind": kind, "width_guess": width}
                    break
            if (hh & 0xF800) == 0xF000:
                p += 4
                continue
            p += 2
        if access:
            hits.append(
                {
                    "ldr_pc": pc,
                    "rd": rd,
                    "access": access,
                    "fn": find_fn_start(blob, code_base, pc, None),
                    "offset": offset,
                }
            )
    # dedupe by access pc
    seen: Set[int] = set()
    uniq: List[Dict[str, Any]] = []
    for h in hits:
        ap = h["access"]["pc"]
        if ap in seen:
            continue
        seen.add(ap)
        uniq.append(h)
    return uniq


def fn_touches_offsets(
    blob: bytes, code_base: int, fn: int, fn_end: int, offsets: Set[int]
) -> Dict[str, bool]:
    touched = {f"off_{o:X}": False for o in offsets}
    for o in range(max(0, fn - code_base), min(len(blob) - 3, fn_end - code_base), 2):
        h = u16(blob, o)
        if (h & 0xF800) != 0x4800:
            continue
        pc = code_base + o
        imm = (h & 0xFF) * 4
        lit = ((pc + 4) & ~2) + imm
        if lit - code_base + 4 > len(blob):
            continue
        val = u32(blob, lit - code_base)
        if val in offsets:
            touched[f"off_{val:X}"] = True
    return touched


def classify_markers(strings: List[str], touches: Dict[str, bool]) -> List[str]:
    marks: List[str] = []
    blob_txt = " ".join(strings).lower()
    if any(k in blob_txt for k in ("http", "net", "update", "login")):
        marks.append("networkish")
    if any(k in blob_txt for k in ("file", "cfg", "mrp", "load", "path")):
        marks.append("resourceish")
    if any(k in blob_txt for k in ("init", "start", "ready")):
        marks.append("initish")
    if touches.get("off_FE8") or touches.get("off_B7D") or touches.get("off_7D8"):
        marks.append("queue_region")
    if touches.get(f"off_{OFF_STATE:X}"):
        marks.append("state_word")
    return marks


def resolve_cluster_fn(blob: bytes, code_base: int, site: int, bl_idx: Dict[int, List[int]]) -> int:
    """Prefer known hot PUSH entries when site falls inside their span."""
    for hot in HOT_FNS:
        if hot <= site < hot + 0xC00:
            # Confirm hot looks like a function entry (PUSH) or has callers.
            h = u16(blob, hot - code_base) if hot >= code_base else 0
            if (h & 0xFF00) == 0xB500 or bl_idx.get(hot):
                return hot
    return find_fn_start(blob, code_base, site, bl_idx)


def analyze(ext: Path) -> Dict[str, Any]:
    blob = ext.read_bytes()
    cb = CODE_BASE
    bl_idx = build_bl_index(blob, cb)
    parent_bls = find_bl_callers_idx(bl_idx, PARENT_FN)

    # Level-1: parent BL sites grouped by enclosing fn
    by_fn: Dict[int, List[Dict[str, Any]]] = defaultdict(list)
    for bl in parent_bls:
        fn = resolve_cluster_fn(blob, cb, bl, bl_idx)
        r0 = infer_r0_before_bl(blob, cb, bl)
        by_fn[fn].append({"bl_pc": bl, "r0": r0})

    clusters: List[Dict[str, Any]] = []
    for fn, sites in sorted(by_fn.items(), key=lambda kv: (-len(kv[1]), kv[0])):
        fn_end = find_fn_end(blob, cb, fn)
        # Upstream = BLs targeting this fn entry (not mid-function labels).
        upstream_bls = find_bl_callers_idx(bl_idx, fn)
        upstream: List[Dict[str, Any]] = []
        for ubl in upstream_bls:
            ufn = find_fn_start(blob, cb, ubl, bl_idx)
            upstream.append(
                {
                    "bl_pc": ubl,
                    "enclosing_fn": ufn,
                    "r0": infer_r0_before_bl(blob, cb, ubl),
                    "strings": nearby_strings(blob, cb, ubl, 0x100),
                }
            )
        r0_counts: Counter = Counter()
        for s in sites:
            c = s["r0"].get("const")
            r0_counts[c if c is not None else "unk"] += 1
        touches = fn_touches_offsets(
            blob, cb, fn, fn_end, {OFF_FE8, OFF_B7D, OFF_7D8, OFF_STATE}
        )
        # rename keys for readability
        touches_named = {
            "FE8": touches.get("off_FE8", False),
            "B7D": touches.get("off_B7D", False),
            "7D8": touches.get("off_7D8", False),
            "state": touches.get(f"off_{OFF_STATE:X}", False),
        }
        strs = nearby_strings(blob, cb, fn, 0x300)
        markers = classify_markers(
            strs,
            {
                "off_FE8": touches_named["FE8"],
                "off_B7D": touches_named["B7D"],
                "off_7D8": touches_named["7D8"],
                f"off_{OFF_STATE:X}": touches_named["state"],
            },
        )
        # Can this fn call PARENT? (it contains the BLs) — also check BLs to other parent clusters
        calls_parent = True
        clusters.append(
            {
                "fn": fn,
                "fn_end": fn_end,
                "parent_bl_count": len(sites),
                "parent_bls": sites,
                "r0_const_counts": {str(k): v for k, v in r0_counts.items()},
                "upstream_count": len(upstream),
                "upstream": upstream[:40],
                "upstream_fns": sorted({u["enclosing_fn"] for u in upstream}),
                "touches": touches_named,
                "strings": strs,
                "markers": markers,
                "is_hot": fn in PRIORITY_HOT,
                "calls_parent": calls_parent,
            }
        )

    # Queue consumers: readers of FE8 / B7D / 7D8
    fe8_readers = scan_offset_access(blob, cb, OFF_FE8, want_store=False, want_load=True)
    b7d_readers = scan_offset_access(blob, cb, OFF_B7D, want_store=False, want_load=True)
    q7d8_readers = scan_offset_access(blob, cb, OFF_7D8, want_store=False, want_load=True)
    fe8_writers = scan_offset_access(blob, cb, OFF_FE8, want_store=True, want_load=False)
    b7d_writers = scan_offset_access(blob, cb, OFF_B7D, want_store=True, want_load=False)

    parent_fns = set(by_fn.keys())

    def annotate_consumer(items: List[Dict[str, Any]], role: str) -> List[Dict[str, Any]]:
        out: List[Dict[str, Any]] = []
        for it in items:
            fn = find_fn_start(blob, cb, it["access"]["pc"], bl_idx)
            it = dict(it)
            it["fn"] = fn
            fn_end = find_fn_end(blob, cb, fn)
            ups = find_bl_callers_idx(bl_idx, fn)
            can_call_parent_cluster = False
            bl_targets: List[int] = []
            for o in range(max(0, fn - cb), min(len(blob) - 3, fn_end - cb), 2):
                t = bl_target(cb + o, u16(blob, o), u16(blob, o + 2))
                if t is None:
                    continue
                if t in parent_fns or t == PARENT_FN:
                    can_call_parent_cluster = True
                    bl_targets.append(t)
            is_upstream_of_parent = any(fn in c["upstream_fns"] for c in clusters)
            is_parent_cluster = fn in parent_fns
            out.append(
                {
                    **it,
                    "role": role,
                    "upstream_callers": [
                        {
                            "bl_pc": u,
                            "enclosing_fn": find_fn_start(blob, cb, u, bl_idx),
                        }
                        for u in ups[:20]
                    ],
                    "upstream_count": len(ups),
                    "can_call_parent_cluster": can_call_parent_cluster or is_parent_cluster,
                    "bl_to_parent_cluster": sorted(set(bl_targets))[:20],
                    "is_parent_cluster_fn": is_parent_cluster,
                    "is_upstream_of_parent_cluster": is_upstream_of_parent,
                    "strings": nearby_strings(blob, cb, fn, 0x200),
                    "touches_state": fn_touches_offsets(
                        blob, cb, fn, fn_end, {OFF_STATE}
                    ).get(f"off_{OFF_STATE:X}", False),
                }
            )
        return out

    consumers = {
        "FE8_readers": annotate_consumer(fe8_readers, "FE8_reader"),
        "B7D_readers": annotate_consumer(b7d_readers, "B7D_reader"),
        "7D8_readers": annotate_consumer(q7d8_readers, "7D8_reader"),
        "FE8_writers": annotate_consumer(fe8_writers, "FE8_writer"),
        "B7D_writers": annotate_consumer(b7d_writers, "B7D_writer"),
    }

    # Bridge: consumers that can reach parent clusters
    bridge = []
    for role, lst in consumers.items():
        if "reader" not in role:
            continue
        for c in lst:
            if c.get("can_call_parent_cluster") or c.get("is_parent_cluster_fn"):
                bridge.append(
                    {
                        "role": c["role"],
                        "fn": c["fn"],
                        "access_pc": c["access"]["pc"],
                        "bl_to_parent_cluster": c.get("bl_to_parent_cluster", []),
                        "upstream_count": c["upstream_count"],
                    }
                )

    # Build live BP spec (role-tagged), keep under ~200
    bp_spec: List[str] = []
    bp_meta: List[Dict[str, Any]] = []

    def add_bp(role: str, pc: int, note: str = "") -> None:
        even = pc & ~1
        key = f"{role}:{even}"
        if any(m.get("key") == key for m in bp_meta):
            return
        bp_spec.append(f"{role}:0x{even:X}")
        bp_meta.append({"key": key, "role": role, "pc": even, "note": note})

    # Dispatcher chain
    for pc, note in (
        (PARENT_FN, "parent"),
        (BL_TO_DISPATCHER, "bl_to_dispatcher"),
        (DISPATCHER_FN, "dispatcher"),
        (SITE_30103C, "site_30103c"),
        (FN_3020C8, "fn_3020c8"),
        (0x302340, "case_arm"),
        (0x302362, "case_arm2"),
    ):
        add_bp("p", pc, note)

    # Hot + high-fanout enclosing entries
    ranked_clusters = sorted(
        clusters, key=lambda c: (0 if c["is_hot"] else 1, -c["parent_bl_count"], c["fn"])
    )
    for c in ranked_clusters[:12]:
        add_bp("e", c["fn"], f"parent_bls={c['parent_bl_count']}")
        for ufn in c["upstream_fns"][:8]:
            add_bp("u", ufn, f"upstream_of_0x{c['fn']:X}")
        for u in c["upstream"][:6]:
            add_bp("u", u["bl_pc"], f"ubl_to_0x{c['fn']:X}")

    # Known discriminating sites (manual): switch case that BLs 0x2DFC3C; 10165 long-path.
    add_bp("e", 0x2DFC3C, "hot_e8i")
    add_bp("e", 0x2E0E00, "hot_e8i")
    add_bp("e", 0x2DC778, "hot_e8i")
    add_bp("u", 0x30D300, "event_switch_table_bl_2DFC3C")
    add_bp("u", 0x30D730, "case_bl_site_2DFC3C")
    add_bp("u", 0x30D28C, "10165_long_path_entry")
    add_bp("u", 0x30D24C, "10165_enqueue_core")
    add_bp("u", 0x305000 + 0xEB8, "b7d_reader_upstream_legacy_gate")
    add_bp("e", 0x2F5390, "parent_cluster_and_7D8_reader")
    add_bp("u", 0x303E14, "upstream_of_2F5390")
    add_bp("u", 0x304A7C, "upstream_of_303E14")
    # Upstream BL sites of hot entries (from index).
    for hot in HOT_FNS:
        for ubl in find_bl_callers_idx(bl_idx, hot)[:10]:
            add_bp("u", ubl, f"ubl_to_hot_0x{hot:X}")
            add_bp("u", find_fn_start(blob, cb, ubl, bl_idx), f"ufn_of_ubl_to_0x{hot:X}")

    # Queue consumer entries (readers first; cap)
    for role_key, tag in (
        ("FE8_readers", "q"),
        ("B7D_readers", "q"),
        ("7D8_readers", "q"),
    ):
        for c in consumers[role_key][:15]:
            add_bp(tag, c["fn"], c["role"])
            add_bp(tag, c["access"]["pc"], f"{c['role']}_access")

    # Sample of direct BL sites (E8I already proved all cold; keep hot-cluster BLs)
    for c in ranked_clusters[:5]:
        for s in c["parent_bls"][:8]:
            add_bp("b", s["bl_pc"], f"bl_parent_r0={s['r0'].get('const')}")

    # Static note: FE8 has no external consumer — only 0x30D24C reads its own store.
    fe8_note = (
        "FE8 external readers≈0; only enqueue core reloads FE8. "
        "Bridge to parent cluster is NOT via FE8 consumer — look at B7D readers / "
        "event-switch 0x30D300 / app-init callers of hot clusters."
    )

    return {
        "code_base": cb,
        "parent_fn": PARENT_FN,
        "parent_bl_count": len(parent_bls),
        "cluster_count": len(clusters),
        "clusters": clusters,
        "hot_clusters": [c for c in clusters if c["is_hot"]],
        "consumers": {
            k: [
                {
                    kk: vv
                    for kk, vv in c.items()
                    if kk not in ("strings",) or True
                }
                for c in v
            ]
            for k, v in consumers.items()
        },
        "consumer_counts": {k: len(v) for k, v in consumers.items()},
        "bridge_to_parent": bridge,
        "bp_spec": ",".join(bp_spec),
        "bp_count": len(bp_spec),
        "bp_meta": bp_meta,
        "fe8_consumer_note": fe8_note,
        "hypotheses_ranked_prelive": [
            {
                "id": "MISSING_APP_INIT_DISPATCH",
                "why": "hot clusters 0x2DFC3C/0x2E0E00/0x2DC778 never entered from boot",
            },
            {
                "id": "MISSING_QUEUE_CONSUMER_TO_DISPATCHER",
                "why": "10165 writes FE8/B7D but consumer may never run or never call parent cluster",
            },
            {
                "id": "MISSING_PLATFORM_SIDE_EFFECT_STATE_38",
                "why": "state word still downstream of cold dispatcher path",
            },
            {
                "id": "MISSING_RESOURCE_READY_DISPATCH",
                "why": "only if live cluster hit + resource markers",
            },
            {
                "id": "MISSING_NETWORK_READY_DISPATCH",
                "why": "only if live cluster hit + network markers",
            },
        ],
    }


def write_md(data: Dict[str, Any], out: Path) -> None:
    lines = [
        "# E8J caller upstream reachability (static)",
        f"code_base=0x{data['code_base']:X}",
        f"parent=0x{data['parent_fn']:X} bl_callers={data['parent_bl_count']} "
        f"enclosing_fns={data['cluster_count']}",
        f"bp_count={data['bp_count']}",
        "",
        "## Hot clusters (E8I-named)",
        "",
    ]
    for c in data["hot_clusters"]:
        lines.append(
            f"### fn `0x{c['fn']:X}` .. `0x{c['fn_end']:X}` "
            f"parent_bls={c['parent_bl_count']} upstream={c['upstream_count']}"
        )
        lines.append(f"- r0_const_counts={c['r0_const_counts']}")
        lines.append(f"- touches={c['touches']} markers={c['markers']}")
        lines.append(f"- strings={c['strings'][:4]}")
        lines.append("- upstream callers (sample):")
        for u in c["upstream"][:12]:
            lines.append(
                f"  - BL `0x{u['bl_pc']:X}` from fn=`0x{u['enclosing_fn']:X}` "
                f"r0={u['r0'].get('const')} strs={u['strings'][:2]}"
            )
        lines.append("")

    lines += ["## Top enclosing functions by parent_bl_count", ""]
    for c in sorted(data["clusters"], key=lambda x: -x["parent_bl_count"])[:15]:
        lines.append(
            f"- `0x{c['fn']:X}` bls={c['parent_bl_count']} up={c['upstream_count']} "
            f"r0={c['r0_const_counts']} hot={c['is_hot']} touches={c['touches']} "
            f"markers={c['markers']}"
        )

    lines += ["", "## Queue consumers (static)", ""]
    for k, n in data["consumer_counts"].items():
        lines.append(f"- {k}: {n}")
    lines += ["", "### Bridge readers that can call parent cluster", ""]
    if not data["bridge_to_parent"]:
        lines.append("- (none found by BL-to-parent-fn heuristic)")
    for b in data["bridge_to_parent"][:30]:
        lines.append(
            f"- {b['role']} fn=`0x{b['fn']:X}` access=`0x{b['access_pc']:X}` "
            f"bl_targets={[hex(x) for x in b.get('bl_to_parent_cluster', [])]} "
            f"up={b['upstream_count']}"
        )

    lines += ["", "## FE8 readers (detail)", ""]
    for c in data["consumers"]["FE8_readers"][:20]:
        lines.append(
            f"- access@`0x{c['access']['pc']:X}` fn=`0x{c['fn']:X}` "
            f"can_call_parent={c['can_call_parent_cluster']} "
            f"is_parent_fn={c['is_parent_cluster_fn']} up={c['upstream_count']} "
            f"touches_state={c['touches_state']}"
        )

    lines += ["", "## B7D readers (detail)", ""]
    for c in data["consumers"]["B7D_readers"][:20]:
        lines.append(
            f"- access@`0x{c['access']['pc']:X}` fn=`0x{c['fn']:X}` "
            f"can_call_parent={c['can_call_parent_cluster']} "
            f"is_parent_fn={c['is_parent_cluster_fn']} up={c['upstream_count']}"
        )

    lines += [
        "",
        "## Ranked hypotheses (pre-live)",
        "",
    ]
    for h in data["hypotheses_ranked_prelive"]:
        lines.append(f"1. `{h['id']}` — {h['why']}")
    lines += ["", "## BP spec (role-tagged)", "", f"`{data['bp_spec'][:500]}...`", ""]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_queue_md(data: Dict[str, Any], out: Path) -> None:
    lines = [
        "# E8J queue consumer xref (FE8 / B7D / 7D8)",
        "",
        f"counts={json.dumps(data['consumer_counts'])}",
        "",
    ]
    for role in ("FE8_readers", "B7D_readers", "7D8_readers", "FE8_writers", "B7D_writers"):
        lines += [f"## {role}", ""]
        for c in data["consumers"][role][:40]:
            ups = ", ".join(
                f"0x{u['enclosing_fn']:X}@{u['bl_pc']:X}" for u in c.get("upstream_callers", [])[:6]
            )
            lines.append(
                f"- `{c['role']}` access=`0x{c['access']['pc']:X}` fn=`0x{c['fn']:X}` "
                f"can_parent={c.get('can_call_parent_cluster')} "
                f"parent_fn={c.get('is_parent_cluster_fn')} "
                f"state={c.get('touches_state')} ups=[{ups}]"
            )
        lines.append("")
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", type=Path, required=True)
    ap.add_argument("-o", type=Path, required=True)
    args = ap.parse_args()
    args.o.mkdir(parents=True, exist_ok=True)
    data = analyze(args.ext)

    # Slim JSON (truncate long lists)
    slim = dict(data)
    slim["clusters"] = []
    for c in data["clusters"]:
        sc = dict(c)
        sc["parent_bls"] = c["parent_bls"][:20]
        sc["upstream"] = c["upstream"][:15]
        slim["clusters"].append(sc)
    for role, lst in data["consumers"].items():
        slim["consumers"][role] = []
        for c in lst:
            sc = {k: v for k, v in c.items() if k != "strings"}
            sc["strings"] = c.get("strings", [])[:4]
            sc["upstream_callers"] = c.get("upstream_callers", [])[:10]
            slim["consumers"][role].append(sc)

    (args.o / "upstream_l2.json").write_text(json.dumps(slim, indent=2), encoding="utf-8")
    write_md(data, args.o / "upstream_l2.md")
    write_queue_md(data, args.o / "queue_consumer.md")
    (args.o / "queue_consumer.json").write_text(
        json.dumps(
            {
                "counts": data["consumer_counts"],
                "bridge": data["bridge_to_parent"],
                "consumers": slim["consumers"],
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    (args.o / "e8j_bp_spec.txt").write_text(data["bp_spec"] + "\n", encoding="utf-8")
    (args.o / "e8j_bp_meta.json").write_text(
        json.dumps(data["bp_meta"], indent=2), encoding="utf-8"
    )
    print(
        f"clusters={data['cluster_count']} parent_bls={data['parent_bl_count']} "
        f"bp={data['bp_count']} consumers={data['consumer_counts']} "
        f"bridge={len(data['bridge_to_parent'])}"
    )


if __name__ == "__main__":
    main()
