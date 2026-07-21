#!/usr/bin/env python3
"""Read-only exhaustive scanner for a Mythroad/GWY runtime tree.

The scanner never modifies MRP/EXT/game files. It inventories all files, parses
MRPG archives, decodes compressed members, groups bootstrap templates, infers
reg.ext primary modules, extracts dependency/resource-name evidence, and emits
CSV/JSON/Markdown reference data for a generic launcher implementation.

The cfg.bin record layout used here is the current repository parser model
(base=1024, record=272). It is reported as PARSER_MODEL rather than documented
ABI, and the scanner records consistency metrics so it can be challenged.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import re
import struct
import sys
import zlib
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional

ASCII_RE = re.compile(rb"[\x20-\x7e]{4,240}")
PATH_RE = re.compile(r"(?i)(?:mythroad/)?(?:gwy/)?[A-Za-z0-9_./@!\-]+\.(?:mrp|ext|bmp|gif|bin|uc2|mid|pak|ypak|res|dat|cfg|sav)")
MRP_PATH_RE_B = re.compile(rb"gwy/[A-Za-z0-9_./-]+\.mrp")
RESOURCE_GRAMMAR_RE = re.compile(r"^(?P<stem>.+)!(?P<w>\d+)!(?P<h>\d+)(?:@(?P<pack>[^.]+))?\.(?P<ext>[A-Za-z0-9]+)$")
URL_RE = re.compile(r"(?i)(?:https?://|[A-Za-z0-9.-]+:\d{2,5}/)[^\x00\s\"']+")
INTEREST = (
    "_mr_c_load", "_strcom", "cfunction.ext", "sdk_key.dat", "dealevent",
    "dealtimer", "suspend", "resume", "sysinfo", "p_mr_param", "mr_param",
    "mrc_loader", "mrc_init", "mrc_extchunk", "startgame", "runapp", "download",
    "update", "no_update", "post_update", "gwyblink", "napptype", "nmrpname",
    "cfg.bin", "checkmrpver", "isfileonservernewer", "getfileversion",
    "simpledownload", "continuedownload", "downimage", "sendappevent",
    "drawbitmap", "refresh", "timer", "event", "connect", "recv", "send",
)

CFG_BASE = 1024
CFG_RECORD_SIZE = 272


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def try_decode(raw: bytes) -> tuple[bytes, str]:
    for label, fn in (
        ("gzip", gzip.decompress),
        ("zlib-gzip", lambda b: zlib.decompress(b, 16 + zlib.MAX_WBITS)),
        ("zlib", zlib.decompress),
    ):
        try:
            return fn(raw), label
        except Exception:
            pass
    return raw, "raw"


def clean_rel(path: Path, root: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except Exception:
        return path.as_posix()


def write_csv(path: Path, rows: list[dict], fieldnames: Optional[list[str]] = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if fieldnames is None:
        keys: list[str] = []
        seen = set()
        for row in rows:
            for k in row:
                if k not in seen:
                    seen.add(k); keys.append(k)
        fieldnames = keys
    with path.open("w", newline="", encoding="utf-8-sig") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)


def extract_ascii_strings(data: bytes) -> list[tuple[int, str]]:
    return [(m.start(), m.group().decode("ascii", "replace")) for m in ASCII_RE.finditer(data)]


def plausible_gbk_strings(data: bytes, min_chars: int = 4) -> list[tuple[int, str]]:
    # Conservative segment decoder: useful for Chinese titles without flooding output.
    rows: list[tuple[int, str]] = []
    start = None
    for i, b in enumerate(data):
        if b == 0 or 32 <= b < 127 or 0x81 <= b <= 0xFE:
            if start is None: start = i
        else:
            if start is not None and i - start >= min_chars:
                chunk = data[start:i].strip(b"\0")
                try:
                    s = chunk.decode("gbk")
                    if len(s) >= min_chars and any("\u4e00" <= c <= "\u9fff" for c in s): rows.append((start, s[:160]))
                except Exception: pass
            start = None
    return rows


@dataclass
class Member:
    name: str
    offset: int
    stored_length: int
    decoded: bytes
    encoding: str
    reserved: int


@dataclass
class Archive:
    path: Path
    data: bytes
    header_length: int
    first_data_offset: int
    internal_name: str
    display_name: str
    appid_le: int
    appver_le: int
    flags: int
    appid_be: int
    appver_be: int
    members: list[Member]


def parse_mrp(path: Path) -> Archive:
    data = path.read_bytes()
    if len(data) < 240 or data[:4] != b"MRPG":
        raise ValueError("not MRPG")
    first_data = struct.unpack_from("<I", data, 4)[0] - 4
    total = struct.unpack_from("<I", data, 8)[0]
    header_len = struct.unpack_from("<I", data, 12)[0]
    if total != len(data):
        raise ValueError(f"length mismatch header={total} actual={len(data)}")
    if not (header_len <= first_data <= len(data)):
        raise ValueError(f"invalid boundaries header={header_len} data={first_data}")
    pos = header_len
    members: list[Member] = []
    while pos < first_data:
        if pos + 4 > first_data: raise ValueError("truncated index")
        name_len = struct.unpack_from("<I", data, pos)[0]; pos += 4
        if not 1 <= name_len <= 512 or pos + name_len + 12 > first_data + 16:
            raise ValueError(f"invalid member name length {name_len}")
        name = data[pos:pos+name_len].rstrip(b"\0").decode("latin1", "replace"); pos += name_len
        offset, stored_len, reserved = struct.unpack_from("<III", data, pos); pos += 12
        if offset + stored_len > len(data): raise ValueError(f"member outside archive: {name}")
        decoded, enc = try_decode(data[offset:offset+stored_len])
        members.append(Member(name, offset, stored_len, decoded, enc, reserved))
    def u32(off: int, endian: str = "<") -> int:
        return struct.unpack_from(endian + "I", data, off)[0] if len(data) >= off+4 else 0
    return Archive(
        path=path, data=data, header_length=header_len, first_data_offset=first_data,
        internal_name=data[16:28].split(b"\0")[0].decode("latin1", "replace"),
        display_name=data[28:52].split(b"\0")[0].decode("gbk", "replace"),
        appid_le=u32(68), appver_le=u32(72), flags=u32(76),
        appid_be=u32(192, ">"), appver_be=u32(196, ">"), members=members,
    )


def primary_from_reg(reg_data: bytes, ext_names: list[str]) -> tuple[str, str]:
    candidates: list[str] = []
    for _, s in extract_ascii_strings(reg_data):
        for token in re.findall(r"[A-Za-z0-9_.-]+\.ext", s):
            if token in ext_names and token not in candidates: candidates.append(token)
    if candidates:
        return candidates[0], "reg.ext_string_first"
    non_loader = [x for x in ext_names if x not in ("reg.ext", "mrc_loader.ext")]
    if len(non_loader) == 1: return non_loader[0], "single_ext_fallback"
    return "", "unresolved"


def classify_archive(rel: str, member_names: list[str], primary: str) -> str:
    lower = rel.lower()
    base = Path(rel).name.lower()
    if base in {"gbrwcore.mrp", "gbrwshell.mrp", "gamelist.mrp"}: return "gwy_shell_core"
    if base in {"roomlist.mrp", "reglogin.mrp", "resmng.mrp", "font.mrp", "dload.mrp", "vdload.mrp", "directpay.mrp", "smscharge.mrp", "smsbase.mrp", "svrctrl.mrp", "pmsg.mrp", "rollscr.mrp"}: return "gwy_service_module"
    if "jjfbol/" in lower or "/res/mrp/" in lower or "downimage" in base: return "side_resource_pack"
    if "mrc_loader.ext" in member_names: return "mrc_loader_game"
    if primary: return "native_ext_package"
    if any(x.endswith(".ext") for x in member_names): return "multi_ext_or_unknown"
    return "resource_only_mrp"


def cfg_records(cfg: bytes) -> tuple[list[dict], dict]:
    rows: list[dict] = []
    max_idx = max(0, (len(cfg)-CFG_BASE)//CFG_RECORD_SIZE)
    target_count = title_count = icon_count = 0
    for idx in range(max_idx):
        off = CFG_BASE + idx*CFG_RECORD_SIZE
        rec = cfg[off:off+CFG_RECORD_SIZE]
        if len(rec) != CFG_RECORD_SIZE: continue
        pm = MRP_PATH_RE_B.search(rec)
        try: title = rec[0x5C:0x70].decode("utf-16be", "replace").rstrip("\0")
        except Exception: title = ""
        try: title_full = rec[0x58:0x70].decode("utf-16be", "replace").rstrip("\0")
        except Exception: title_full = ""
        icon = rec[0x40:0x58].split(b"\0")[0].decode("ascii", "replace")
        target = pm.group().decode("ascii") if pm else ""
        if target: target_count += 1
        if title_full.strip("\ufffd\0 "): title_count += 1
        if icon: icon_count += 1
        if not target and not title_full.strip("\ufffd\0 ") and not icon: continue
        be24 = lambda b: int.from_bytes(b, "big")
        rows.append({
            "evidence":"PARSER_MODEL", "record_base":CFG_BASE, "record_size":CFG_RECORD_SIZE,
            "index":idx, "file_offset":off, "icon":icon, "napptype":rec[0x57],
            "title_suffix_repo_model_0x5C":title, "title_candidate_0x58":title_full, "nextid":be24(rec[0x72:0x75]),
            "ncode":be24(rec[0x78:0x7B]), "narg":be24(rec[0x7B:0x7E]),
            "narg1":rec[0x7E], "target_mrp":target,
            "record_sha256":sha256_bytes(rec), "raw_prefix_hex":rec[:32].hex(),
        })
    metrics = {
        "cfg_size":len(cfg), "record_base":CFG_BASE, "record_size":CFG_RECORD_SIZE,
        "candidate_records":max_idx, "rows_emitted":len(rows), "target_rows":target_count,
        "title_rows":title_count, "icon_rows":icon_count,
        "model_status":"PARSER_MODEL_CROSS_RECORD_PLAUSIBLE" if target_count >= 40 else "PARSER_MODEL_WEAK",
    }
    return rows, metrics


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, required=True, help="mythroad/240x320 root or equivalent")
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    root = args.root.resolve(); out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    gwy = root / "gwy"
    if not gwy.is_dir(): raise SystemExit(f"missing gwy directory: {gwy}")

    all_files: list[dict] = []
    dir_counts: dict[str, dict] = defaultdict(lambda:{"files":0,"bytes":0,"suffixes":Counter()})
    for p in sorted(root.rglob("*")):
        if not p.is_file(): continue
        rel = clean_rel(p, root); st = p.stat(); suffix = p.suffix.lower() or "(none)"
        all_files.append({"path":rel,"size":st.st_size,"suffix":suffix,"sha256":sha256_file(p)})
        top = rel.split("/",1)[0] if "/" in rel else "(root)"
        d = dir_counts[top]; d["files"] += 1; d["bytes"] += st.st_size; d["suffixes"][suffix] += 1
    write_csv(out/"all_files.csv", all_files)
    dir_rows = [{"group":k,"file_count":v["files"],"bytes":v["bytes"],"mib":round(v["bytes"]/1048576,3),"suffix_distribution":json.dumps(v["suffixes"],ensure_ascii=False,sort_keys=True)} for k,v in sorted(dir_counts.items())]
    write_csv(out/"directory_stats.csv", dir_rows)

    archive_rows: list[dict] = []
    member_rows: list[dict] = []
    ext_rows: list[dict] = []
    string_rows: list[dict] = []
    dependency_rows: list[dict] = []
    resource_rows: list[dict] = []
    errors: list[dict] = []
    start_groups: dict[str,list[str]] = defaultdict(list)
    loader_groups: dict[str,list[str]] = defaultdict(list)
    primary_groups: dict[str,list[str]] = defaultdict(list)
    archive_cache: dict[str,Archive] = {}

    mrps = sorted(gwy.rglob("*.mrp"))
    root_mrps = sorted(root.glob("*.mrp"))
    for p in root_mrps:
        if p not in mrps: mrps.append(p)
    mrps = sorted(set(mrps))

    for p in mrps:
        rel = clean_rel(p, root)
        try:
            ar = parse_mrp(p); archive_cache[rel] = ar
        except Exception as e:
            errors.append({"path":rel,"stage":"parse_mrp","error":repr(e)}); continue
        names = [m.name for m in ar.members]
        ext_names = [n for n in names if n.lower().endswith(".ext")]
        reg = next((m for m in ar.members if m.name.lower()=="reg.ext"), None)
        primary, primary_evidence = primary_from_reg(reg.decoded if reg else b"", ext_names)
        cls = classify_archive(rel, names, primary)
        start = next((m for m in ar.members if m.name.lower()=="start.mr"),None)
        loader = next((m for m in ar.members if m.name.lower()=="mrc_loader.ext"),None)
        start_sha = sha256_bytes(start.decoded) if start else ""
        loader_sha = sha256_bytes(loader.decoded) if loader else ""
        if start_sha: start_groups[start_sha].append(rel)
        if loader_sha: loader_groups[loader_sha].append(rel)
        if primary: primary_groups[primary].append(rel)
        assets = [n for n in names if not n.lower().endswith((".ext",".mr")) and n.lower()!="reg.ext"]
        archive_rows.append({
            "path":rel,"basename":p.name,"size":len(ar.data),"sha256":sha256_bytes(ar.data),
            "header_length":ar.header_length,"first_data_offset":ar.first_data_offset,
            "internal_name":ar.internal_name,"display_name":ar.display_name,
            "appid_le":ar.appid_le,"appver_le":ar.appver_le,"flags":ar.flags,
            "appid_be":ar.appid_be,"appver_be":ar.appver_be,
            "member_count":len(names),"ext_count":len(ext_names),"asset_count":len(assets),
            "class":cls,"primary":primary,"primary_evidence":primary_evidence,
            "has_start_mr":int(start is not None),"start_sha256":start_sha,
            "has_mrc_loader":int(loader is not None),"mrc_loader_sha256":loader_sha,
            "ext_members":"|".join(ext_names),
        })
        for m in ar.members:
            low = m.name.lower(); kind = "other"
            if low=="start.mr": kind="start_mr"
            elif low=="reg.ext": kind="reg_ext"
            elif low.endswith(".ext"): kind="ext"
            elif low.endswith(".mrp"): kind="nested_mrp_member"
            elif RESOURCE_GRAMMAR_RE.match(m.name): kind="dimensioned_resource"
            elif low.endswith((".bmp",".gif",".png",".ani",".mid")): kind="media_resource"
            member_rows.append({
                "archive":rel,"name":m.name,"kind":kind,"offset":m.offset,
                "stored_length":m.stored_length,"decoded_length":len(m.decoded),
                "encoding":m.encoding,"reserved":m.reserved,"sha256":sha256_bytes(m.decoded),
                "magic8":m.decoded[:8].hex(),
            })
            gm = RESOURCE_GRAMMAR_RE.match(m.name)
            if gm:
                resource_rows.append({"archive":rel,"member":m.name,"stem":gm.group("stem"),"width":int(gm.group("w")),"height":int(gm.group("h")),"pack":gm.group("pack") or "","extension":gm.group("ext").lower(),"decoded_length":len(m.decoded),"sha256":sha256_bytes(m.decoded),"evidence":"ARCHIVE_MEMBER"})
            if kind in {"ext","start_mr","reg_ext"} or low.endswith((".bin",".cfg",".sav")):
                # EXT format / string and dependency evidence.
                ascii_strings = extract_ascii_strings(m.decoded)
                if kind == "ext":
                    ext_rows.append({
                        "archive":rel,"member":m.name,"decoded_length":len(m.decoded),
                        "sha256":sha256_bytes(m.decoded),"magic8_ascii":m.decoded[:8].decode("latin1","replace"),
                        "is_mrpgcmap":int(m.decoded.startswith(b"MRPGCMAP")),
                        "entry_candidate_offset":8 if m.decoded.startswith(b"MRPGCMAP") else "",
                        "string_count":len(ascii_strings),
                        "has_cfunction":int(b"cfunction.ext" in m.decoded),
                        "has_strcom":int(b"_strCom" in m.decoded or b"strCom" in m.decoded),
                        "has_mrc_init":int(b"mrc_init" in m.decoded),
                    })
                seen_hit=set()
                for off_s, s in ascii_strings:
                    sl = s.lower()
                    hits = [needle for needle in INTEREST if needle in sl]
                    if hits or PATH_RE.search(s) or URL_RE.search(s):
                        key=(off_s,s)
                        if key in seen_hit: continue
                        seen_hit.add(key)
                        string_rows.append({"archive":rel,"member":m.name,"offset":off_s,"text":s[:240],"matched":"|".join(hits),"source":"ascii"})
                    for dep in PATH_RE.findall(s):
                        dependency_rows.append({"source_archive":rel,"source_member":m.name,"offset":off_s,"dependency":dep,"type":"string_path"})
                    for url in URL_RE.findall(s):
                        dependency_rows.append({"source_archive":rel,"source_member":m.name,"offset":off_s,"dependency":url,"type":"url_or_endpoint"})
                # Dynamic @pack grammar references may exist in code strings, not member names.
                for off_s, s in ascii_strings:
                    if "@" in s and ("!" in s or "downimage" in s.lower()):
                        string_rows.append({"archive":rel,"member":m.name,"offset":off_s,"text":s[:240],"matched":"resource_grammar","source":"ascii"})

    write_csv(out/"mrp_archives.csv", archive_rows)
    write_csv(out/"mrp_members.csv", member_rows)
    write_csv(out/"ext_modules.csv", ext_rows)
    write_csv(out/"interesting_strings.csv", string_rows)
    write_csv(out/"dependency_edges.csv", dependency_rows)
    write_csv(out/"resource_name_grammar.csv", resource_rows)
    write_csv(out/"parse_errors.csv", errors)

    group_rows=[]
    for sha, paths in sorted(start_groups.items(), key=lambda kv:(-len(kv[1]),kv[0])):
        group_rows.append({"kind":"start.mr","sha256":sha,"count":len(paths),"members":"|".join(paths)})
    for sha, paths in sorted(loader_groups.items(), key=lambda kv:(-len(kv[1]),kv[0])):
        group_rows.append({"kind":"mrc_loader.ext","sha256":sha,"count":len(paths),"members":"|".join(paths)})
    write_csv(out/"bootstrap_hash_groups.csv", group_rows)
    write_csv(out/"reg_primary_groups.csv", [{"primary":k,"count":len(v),"archives":"|".join(v)} for k,v in sorted(primary_groups.items())])

    cfg_path = gwy/"cfg.bin"
    cfg_rows=[]; cfg_metrics={"present":False}
    if cfg_path.is_file():
        cfg_rows,cfg_metrics=cfg_records(cfg_path.read_bytes()); cfg_metrics["present"]=True; cfg_metrics["sha256"]=sha256_file(cfg_path)
    write_csv(out/"cfg_records_parser_model.csv", cfg_rows)
    (out/"cfg_parser_model_metrics.json").write_text(json.dumps(cfg_metrics,ensure_ascii=False,indent=2),encoding="utf-8")

    # Side pack registry: nested resource MRPs + pairing files.
    side_rows=[]
    for r in archive_rows:
        if r["class"]=="side_resource_pack":
            p=root/r["path"]
            sibling_v=p.with_suffix(".v")
            side_rows.append({"path":r["path"],"basename":r["basename"],"member_count":r["member_count"],"asset_count":r["asset_count"],"sha256":r["sha256"],"sibling_v_exists":int(sibling_v.is_file()),"sibling_v_size":sibling_v.stat().st_size if sibling_v.is_file() else 0,"primary":r["primary"]})
    write_csv(out/"side_pack_registry.csv",side_rows)

    # Core package / control matrix generated from actual scan.
    by_base={r["basename"]:r for r in archive_rows if "/" not in r["path"] or r["path"].startswith("gwy/") and r["path"].count("/")==1}
    controls=[]
    control_specs=[
        ("gwy.mrp","platform_root","Root shell package; cfunction/graphics bootstrap"),
        ("gbrwcore.mrp","shell_api_broker","Export broker, file/update/runapp services"),
        ("gamelist.mrp","catalog_descriptor","cfg.bin parsing, descriptor construction, selection"),
        ("gbrwshell.mrp","download_ui","download/task/file-manager shell"),
        ("roomlist.mrp","minimal_shell_control","small direct-primary shell/service control"),
        ("vdload.mrp","transport_control","small direct-primary network/download control"),
        ("jjfb.mrp","mrc_loader_complex","target: shared loader + robotol + many modules + side packs"),
        ("wxjwq.mrp","mrc_loader_positive_control","same start/mrc_loader, different main EXT"),
        ("tlbb.mrp","direct_primary_control","single primary dream.ext family"),
        ("spacetime.mrp","resource_heavy_control","single primary with hundreds of resources"),
        ("sanguo.mrp","large_ext_control","large primary EXT and many resources"),
    ]
    # search basename globally
    for name,role,purpose in control_specs:
        matches=[r for r in archive_rows if r["basename"]==name]
        r=matches[0] if matches else {}
        controls.append({"target":name,"role":role,"purpose":purpose,"found":int(bool(r)),"path":r.get("path",""),"class":r.get("class",""),"primary":r.get("primary",""),"start_sha256":r.get("start_sha256",""),"mrc_loader_sha256":r.get("mrc_loader_sha256",""),"member_count":r.get("member_count",0),"asset_count":r.get("asset_count",0)})
    write_csv(out/"control_target_matrix.csv",controls)

    # Common platform contract evidence matrix.
    evidence=[]
    def ev(contract, level, sources, implication): evidence.append({"contract":contract,"evidence_level":level,"sources":sources,"launcher_implication":implication})
    ev("MRPG container/index/decompression","RAW_SCAN","all 130 gwy MRPs","Implement strict read-only archive parser; preserve package identity")
    ev("start.mr bootstrap families","RAW_SCAN","bootstrap_hash_groups.csv","Select bootstrap by archive content/hash family, not target name")
    ev("reg.ext primary selection","RAW_SCAN","mrp_members.csv; reg_primary_groups.csv","Resolve primary within package scope")
    ev("MRPGCMAP EXT image+8 candidate","RAW_SCAN+REPO_MODEL","ext_modules.csv","Map image and preserve Thumb/entry/callback distinctions")
    ev("cfg descriptor table","PARSER_MODEL","cfg_records_parser_model.csv","Treat layout as versioned parser model; validate before launch")
    ev("gbrwcore named service exports","RAW_STRINGS","interesting_strings.csv","Build generic named service dispatcher")
    ev("side-pack @pack resource resolution","RAW_MEMBERS+RUNTIME_OBSERVED","resource_name_grammar.csv; side_pack_registry.csv","First-class side-pack registry; exact member resolution")
    ev("package-scoped VFS/member view","CROSS_TARGET_INFERENCE","all shell/game classes","Do not use global cfunction.ext or global primary")
    ev("P/extChunk + ER_RW/R9 context","LATEST_RUNTIME_OBSERVED","repository stages 6N/6O/E-series","Module context provider must be common and nest-safe")
    ev("timer/event/lifecycle dispatch","LATEST_RUNTIME_OBSERVED","E7-E9Y traces","Deliver registered guest callbacks with correct ABI")
    ev("graphics/text/color-key","LATEST_RUNTIME_OBSERVED","E9V","Generic platform APIs, not app-specific drawing")
    ev("legacy shell update boundary","RAW_STRINGS+RUNTIME_OPEN","gamelist/gbrwcore/gbrwshell/vdload","Separate shell update compatibility from game-server behavior")
    write_csv(out/"platform_contract_evidence.csv",evidence)

    summary={
        "root":str(root),"file_count":len(all_files),"total_bytes":sum(r["size"] for r in all_files),
        "gwy_file_count":sum(1 for p in gwy.rglob("*") if p.is_file()),
        "mrp_count_scanned":len(mrps),"mrp_count_parsed":len(archive_rows),"parse_error_count":len(errors),
        "top_level_gwy_mrp_count":len(list(gwy.glob("*.mrp"))),
        "member_count":len(member_rows),"ext_count":len(ext_rows),"side_pack_count":len(side_rows),
        "start_template_family_count":len(start_groups),"mrc_loader_family_count":len(loader_groups),
        "cfg_metrics":cfg_metrics,
    }
    (out/"scan_summary.json").write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding="utf-8")

    md=["# GWY/MRP exhaustive scan summary","",f"- Root: `{root}`",f"- Files: **{summary['file_count']}** ({summary['total_bytes']/1048576:.2f} MiB)",f"- `gwy/` files: **{summary['gwy_file_count']}**",f"- MRP scanned/parsed/errors: **{summary['mrp_count_scanned']} / {summary['mrp_count_parsed']} / {summary['parse_error_count']}**",f"- Top-level `gwy/*.mrp`: **{summary['top_level_gwy_mrp_count']}**",f"- MRP members: **{summary['member_count']}**",f"- EXT modules: **{summary['ext_count']}**",f"- Side/resource packs: **{summary['side_pack_count']}**",f"- start.mr hash families: **{summary['start_template_family_count']}**",f"- mrc_loader hash families: **{summary['mrc_loader_family_count']}**","", "## cfg parser model", "", "```json",json.dumps(cfg_metrics,ensure_ascii=False,indent=2),"```","","> The cfg layout is a parser model inferred from cross-record regularity, not a documented SDK ABI."]
    (out/"scan_summary.md").write_text("\n".join(md),encoding="utf-8")
    print(json.dumps(summary,ensure_ascii=False,indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
