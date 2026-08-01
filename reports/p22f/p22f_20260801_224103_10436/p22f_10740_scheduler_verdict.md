# P22F-CLEAN +0x10740 scheduler provenance verdict

## Bottom line

**Class: C**

```text
callback itself never registered
→ registration-producing Guest init function 未执行
→ +0xF670 / +0x8CDC / +0xD978 never entered
→ all 11 BL +0x10740 callsites hit=0
```

Natural schedule of `+0x10740` is blocked **before** any dispatcher case / once-flag / mode gate.
Observed gamelist work in this window is only the timer tag path `+0x133E0` / `+0x1344A` (12 hits = 12 FIRE_EXT).

---

## Identity

```
source commit：3a1fdfa12492827ce1a183af464001d7c6398841
main.exe SHA：47de8b9c133ff3634f464e32f62732956ca69714e19d887a186036b6bc01b079
raw gamelist.ext SHA：70abc063dae99c95e7d9d7a44db5117c9512a430078c2672ecae9e28c3616414
runtime image SHA：e510fe8795381f27e1ec49048f04ee94a486baf0ede0c9382825d2d606427ca8
runtime base/end：0x2D4364 / 0x2EA940
runtime size：0x165DC
raw_base_refine_pad：0x4
module id：0x4
ERW：0x682B8C
P：0x2AC8EC
generation：2
package owner：gamelist.ext
offset_map：runtime_module_offset = guest_pc - runtime_base;
            raw_file_offset ≈ runtime_module_offset (after RAW_BASE_REFINE pad=0x4)
```

## Freeze (Lane A)

| Check | Result |
|------|--------|
| research_assisted / FAST_* | 0 / OFF |
| JJFB_P22_MODE / HEADLESS / P25 / force-10140 | OFF |
| gbrwcore / br_exit CONTINUE / gamelist / ERW iso | PASS |
| natural FIRE_EXT | PASS (12) |
| forced 10140 / 0x30D5D2 | 0 |

## Runtime callers of +0x10740

All **11** static direct BL sites re-verified on refined runtime image (`match=1`). Extra direct/literal callers: **0**.

| caller_offset | containing_function | hit_n |
|---------------|---------------------|-------|
| +0x4076 … +0x5918 (9 sites) | dispatcher_392C | 0 |
| +0x8D26 | cb_8CDC | 0 |
| +0x12D0E | fn_12CF0 | 0 |

Static inbound BL to group entries: only `+0xF672→+0x392C` and `+0xD984→+0x12CF0`. **No direct BL to `+0xF670` / `+0x8CDC` / `+0xD978`** → they are callback/indirect candidates.

## Group results

### A — `+0xF670` → `+0x392C`

- `+0xF670` hit=0, `+0x392C` hit=0, all 9 callsites hit=0
- Classification: **NEVER_REGISTERED** (wrapper never entered; no platform arg registered `(base+F670)|1`)

### B — `+0x8CDC` → `+0x8D26`

- `+0x8CDC` / `+0x8D26` hit=0
- `p22f_callback_registration.csv`: **0 rows** (no plat-arg / observed write of `(base+8CDC)|1`)
- Classification: **NEVER_REGISTERED**

### C — `+0xD978` → `+0x12CF0` → `+0x12D0E`

- All hit=0
- `[R9+0x450]` prep snap=0x0 (not a live lock; `+0x10740` not entered)
- Classification: **NEVER_REGISTERED / never entered**

### What did run

- `+0x133E0` hit=12, `+0x1344A` hit=12 — timer/0x10204 tag activity only (not a `+0x10740` producer)
- Platform timeline: natural `plat` + `FIRE_EXT` only; no delivery to F670/8CDC/D978

## Determined chain

```text
callback itself never registered
→ expected registration-producing Guest init 未执行
→ +0x10740 caller never entered
```

Core judgment form:

```text
callback/function +0xF670|+0x8CDC|+0xD978 未注册且未进入;
事件/分支: 12× FIRE_EXT 仅驱动 +0x133E0/+0x1344A;
产品自然路径期望: 某 Guest init 应注册 UI/dispatcher callback 并被投递;
因此没有执行任何 callsite → +0x10740;
该注册本应由 producer W=registration-producing Guest init（尚未定位具体函数）自然产生。
```

## PASS answers (required)

```
source commit：3a1fdfa12492827ce1a183af464001d7c6398841
main.exe SHA：47de8b9c133ff3634f464e32f62732956ca69714e19d887a186036b6bc01b079
raw gamelist.ext SHA：70abc063dae99c95e7d9d7a44db5117c9512a430078c2672ecae9e28c3616414
runtime image SHA：e510fe8795381f27e1ec49048f04ee94a486baf0ede0c9382825d2d606427ca8
runtime base/end：0x2D4364 / 0x2EA940
module id：0x4
ERW：0x682B8C
P：0x2AC8EC
generation：2
package owner：gamelist.ext

+0x10740 的全部 runtime caller：11 direct BL (verified); no extra
11个静态 caller 是否全部验证：YES
是否存在新增直接或间接 caller：NO (literal_ptrs=0 extra_direct=0)

A组 +0x392C 是否进入：NO
+0xF670 的自然调用者：NONE observed (no direct BL inbound; never registered/delivered)
dispatcher event/opcode：n/a (not entered)
选择的 case：n/a
最接近 +0x10740 的分支：n/a

B组 +0x8CDC 是否注册：NO (reg_n=0)
注册位置/API：NONE
是否投递：NO
投递事件：n/a
是否命中 +0x8D26：NO

C组 +0xD978 是否进入：NO
[R9+0x450] 当前值：0x0 (prep snap only)
+0x12CF0 是否进入：NO
0x10204 返回值：n/a on this path (tag path uses 0x10204 separately)
是否命中 +0x12D0E：NO

最接近执行的 +0x10740 caller：NONE (nearest live gamelist code: +0x133E0/+0x1344A)
第一条阻断分支：n/a — producer never scheduled
比较操作数：n/a
实际路径：timer → +0x133E0/+0x1344A only
目标路径：register+deliver → +0xF670|+0x8CDC|+0xD978 → BL +0x10740

自然生产者属于：unknown (candidate: Guest init that registers UI/dispatcher callback; not timer alone)
+0x10740 是否自然进入：NO
[R9+0x3E4]：0x0 (prep; NOT current lock)
[R9+0x6C4]：0x1 (prep; NOT current lock)
+0x10814 是否进入：NO
+0xFF00 是否进入：NO
+0x7B6C 是否进入：NO
真实 cfg open 是否出现：NO

是否写入 Guest 状态：NO
是否注入事件：NO
是否使用 headless：NO
当前唯一门锁：Class C — F670/8CDC/D978 never registered and never entered
下一处最小通用修复：定位并恢复会注册 +0xF670 或 +0x8CDC（或进入 +0xD978）的 Guest init / 平台 callback 注册合同；禁止 Host 直调 +0x10740
```

## Artifacts

- `reports/p22f/p22f_20260801_224103_10436/p22f_10740_scheduler_verdict.md`
- `reports/p22f/p22f_20260801_224103_10436/p22f_10740_all_xrefs.csv`
- `reports/p22f/p22f_20260801_224103_10436/p22f_10740_caller_hits.csv`
- `reports/p22f/p22f_20260801_224103_10436/p22f_caller_branch_slices.csv`
- `reports/p22f/p22f_20260801_224103_10436/p22f_callback_registration.csv`
- `reports/p22f/p22f_20260801_224103_10436/p22f_event_delivery_timeline.csv`
- `reports/p22f/p22f_20260801_224103_10436/p22f_scheduler_provenance.csv`
- `reports/p22f/p22f_20260801_224103_10436/p22f_10740_runtime_disasm.txt`
- `out/p22f/p22f_20260801_224103_10436/p22f_build_identity.txt`
- `out/p22f/p22f_20260801_224103_10436/p22f_runtime_summary.txt`
- `research/runners/p22f_run_10740_scheduler_provenance.ps1`
