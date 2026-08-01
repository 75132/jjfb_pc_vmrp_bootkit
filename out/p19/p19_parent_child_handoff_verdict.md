# P19 — Parent→Child 启动交接裁决

## 结论（精度：E 已证 / A–D 分层）

| 主张 | 精度 | 说明 |
|------|------|------|
| 产品直启：0x10140 只注册、不自然激活 | **E 已证明**（延续 P18） | NATURAL_ONLY；life_fire=0；SKIP_FORCED_ARM |
| 产品直启：start_dsm return 后宿主 idle，无 Guest 后继 | **D 领先假设** | POST_CHILD_FIRST_ACTION=NO_GUEST_SUCCESSOR_HOST_IDLE |
| 父级一次性调用 0x10140（A） | **未证明** | Shell 未进入 JJFB；无 PARENT_LAUNCH_ENTER / 无 DIRECT_10140_ENTER |
| 平台激活事件（B）/ 周期（C） | **未观察到** | 无 post-child plat activate；无 period |

**本轮未实现任何 activator。**

---

## Shell 路径（GwyResearch，gwy_shell_core_continue）

```text
gbrwcore start_dsm(entry=cfg36…nmrpname=gwy/jjfb.mrp…)
→ gbrwcore 返回 0
→ 未出现 guest 对 lib.startGame / lib.runapp 的真实调用
→ 未出现 nested start_dsm(gwy/jjfb.mrp)
→ PARENT_LAUNCH_ENTER = 0
→ CHILD_INIT_RETURN(jjfb) = 0
```

修正：早期把 entry 参数里的 jjfb 误判成 child enter；已改为仅 filename 判定 package。重跑后 phase=shell_parent，不再假报 JJFB child。

历史仓库亦一致：0x2AAD84 startGame body enter 从未 live 捕获；export 仅为 string_va_not_entry。

→ **原 Shell 本轮无法自然进入 JJFB 父子交接。触发回退条件。**

---

## 产品直启对照（短窗，非 3×180）

```text
[P19_START_DSM] package=gwy/jjfb.mrp phase=jjfb_child_enter
[10140_REGISTER] handler=0x30631D owner=robotol.ext  (×2, no period)
[CHILD_INIT_RETURN] package=gwy/jjfb.mrp ret=0 handler_10140=0x30631D ack_10800=1
[10140_ACTIVATION_POLICY] mode=NATURAL_ONLY
[JJFB_LIFECYCLE] op=SKIP_FORCED_ARM
[POST_CHILD_FIRST_ACTION] kind=NO_GUEST_SUCCESSOR_HOST_IDLE
  actor=host_sdl_loop handler=0x30631D
```

含义（领先，非终裁）：产品把 start_dsm return 当成控制流结束 → 宿主 SDL idle；已注册的 0x10140 没有被父级/平台/自然定时器激活。更像 **D：return ≠ 子应用结束；缺的是 child 应用生命周期**，而不是已证明的「父级 one-shot 调 10140」。

注意：产品直启没有冒泡父级，故不能从产品路径单独证伪/证实 A。

---

## 验收答案

```text
原 Shell 是否自然进入 JJFB：否（仅 gbrwcore shell_parent；无 jjfb nested start_dsm）
真实 startGame/runapp 函数：未捕获 Guest 调用（仅 string table 注册 lib.startGame/lib.runapp）
父级 caller module：NOT_CAPTURED
父级 call instruction：NOT_CAPTURED
父级 continuation：NOT_CAPTURED
child start_dsm 返回后父级是否恢复：N/A（Shell 未到 JJFB child）

0x10140 首次激活者：NONE_OBSERVED（产品侧注册后零次自然调用）
首次激活方式：NONE
首次激活时机：N/A（未激活）
首次激活参数：N/A
是否只调用一次：n/a（调用次数=0）
是否周期调用：否（NATURAL_ONLY；lifecycle FIRE=0；注册 ABI 无 period）
是否由平台事件触发：未观察到

start_dsm return 的真实语义：
  Shell：未捕获 JJFB 边界
  产品（领先假设 D）：child initialized，不是 app finished；随后宿主 idle

产品直启当前错误解释：
  把 start_dsm return 当成子应用结束并进入宿主 idle，
  缺少 INIT→ACTIVATE/EVENT LOOP 合同（D 领先；A 仍是假设）

缺失的冒泡 launch contract：
  parent startGame/runapp → child init → first activator（A/B/C/D 四选一尚未由 Shell 实锤）

下一处唯一通用修复：
  禁止实现 0x10140 activator。
  下一证据步：回退一 sibling 同壳交接，或回退二 research launch capsule
  （从历史/静态 startGame call site 前回放，research_replay=yes product_valid=no）。
  产品实现方向在 D 被 Shell 或 capsule 证实时：恢复 child 应用生命周期，而非父级硬敲 10140。
```

---

## 回退状态

| 回退 | 状态 |
|------|------|
| 一：sibling 同壳网游 | 待开（本轮聚焦 JJFB；壳未到 startGame，sibling 很可能同阻） |
| 二：research launch capsule | 待开（仓库无一次完整 live PARENT_LAUNCH_ENTER；仅有 string VA / 静态 xref） |

---

## 产物

- reports/p19_parent_child_handoff_verdict.md（本文件）
- reports/p19_startgame_call_trace.csv
- reports/p19_post_child_first_action.csv
- out/p19/p19_build_identity.txt
- logs/p19_shell_vmrp.txt / logs/p19_product_vmrp.txt
- research/runners/p19_run_parent_child_handoff.ps1
- 观测：ext_parent_child_handoff + 接线 gwy_ext_obs / guest_call_observer / shell_native_exec

## 纪律

- JJFB_FORCE_10140_LIFECYCLE=0 / ONESHOT=0 / NATURAL_ONLY
- 禁止产品路径主动调 0x10140
- 禁止 Hook 内嵌套 uc_emu_start
- A 不得升级为已证明
