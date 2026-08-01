# P18 — 0x10140 Activation Contract

## Verdict

**Product forced 50ms lifecycle is OFF.** Closing it eliminates the artificial `0x1E209` echo. On the natural direct path, `0x10140` is **registered but never invoked** after `start_dsm` returns. A research one-shot kick fires the handler once, re-posts the same `0x1E209` with **unchanged digest**, then idles — it does **not** create a guest `mr_timerStart`.

Working classification:

```text
E_REGISTERED_BUT_NOT_ACTIVATED_THIS_STAGE
(+ A-leaning hypothesis: missing parent/platform first activator)
```

Not proven as platform-periodic (B): register ABI has **no period**.  
Not C: guest never called `timerStart` after the one-shot kick.

## Policy (permanent)

| Mode | Env | Product valid |
|------|-----|---------------|
| NATURAL_ONLY (default) | unset / `0` | yes |
| RESEARCH_PERIODIC | `JJFB_FORCE_10140_LIFECYCLE=1` | no |
| RESEARCH_ONESHOT | `JJFB_FORCE_10140_ONESHOT=1` | no |

Live:

```text
[10140_ACTIVATION_POLICY] mode=NATURAL_ONLY forced_arm=0 forced_enqueue=0 forced_rearm=0
[JJFB_LIFECYCLE] op=SKIP_FORCED_ARM handler=0x30631D reason=NATURAL_ONLY_waiting_for_natural_activator
```

## Group A — force off (3×180s, same binary)

| run | policy | forced_host | life_fire | 1E209 | case9 | genuine | guest_timerStart |
|-----|--------|-------------|-----------|-------|-------|---------|------------------|
| natural1 | NATURAL_ONLY | 0 | 0 | 0 | 0 | 0 | 0 |
| natural2 | NATURAL_ONLY | 0 | 0 | 0 | 0 | 0 | 0 |
| natural3 | NATURAL_ONLY | 0 | 0 | 0 | 0 | 0 | 0 |

### Registration (identical ×3)

```text
[10140_REGISTER] family=0x5 handler=0x30631D
  r0=0x10140 r1=0x5 r2=0x682AB4 r3=0x30631D
  pc=0x304599 lr=0x304599 r9=0x2B1868 owner=robotol.ext ret=1
  abi_note=no_period_in_register_args
```

### Natural stop point (after start_dsm)

```text
… plat 0x10102 / 0x10120 / 0x10140 / 0x10162 / 0x10165 …
→ plat 0x10800 app=0x4 ack=1
→ ROBOTOL_INIT_RETURN_ZERO
→ START_DSM_RETURN ret=0
→ TIMER_ARM_ABSENT
→ 10140_ACTIVATION_POLICY NATURAL_ONLY
→ SKIP_FORCED_ARM
→ (host SDL loop; no further Guest platform progress in 180s)
```

Last Guest branch before idle: `pc=0x304A28 → sendAppEvent(0x10800, 0x4)`.

## Group B — research oneshot (product_valid=no)

| field | value |
|-------|-------|
| policy | RESEARCH_ONESHOT |
| ARM | `lifecycle_10140_research_oneshot` forced=yes one_shot=yes |
| FIRE | tick=1 handler=0x30631D |
| 0x1E209 | produced once (digest `0xFF5FCBC8`) |
| Case-9 | REACHED_STOP ×1 |
| state digest | `before=after=0xFF5FCBC8` **changed=0** |
| guest mr_timerStart | **0** (matrix `natural_timer=1` was host `op=START chunk` false positive) |
| rearm | ONESHOT_DONE no_rearm=1 |

Conclusion: a single host kick only reproduces the P17 echo once. It is **not** a missing “first tick that lets Guest self-arm”. Forced periodic ticks therefore have **no progress value**.

## Group C — original shell

`RUN_RESEARCH_GWY_SHELL.ps1` ran (shim + gbrwcore→gamelist continue). Live runners listed in the suite script are **missing** on disk (`[SKIP] missing RUN_LIVE_*.ps1`), so this pass **did not** capture a JJFB-stage `0x10140` first-call on the shell path.

Honest shell status:

| item | status |
|------|--------|
| shell suite executed | yes |
| JJFB 10140 first-call live | **not captured** (suite gap / update hall) |
| docs/06 wording | “周期/主 handler” = CROSS_TARGET observation, not ABI-proven period |
| legacy lab | treated 10140 as timer-driven under older host-tick experiments — not reusable as product proof |

## Semantic classification

| Option | Decision |
|--------|----------|
| A parent one-shot start | **Leading hypothesis** — direct path never activates after child init; needs parent/runapp continuation proof |
| B platform periodic | **Not proven** — no period in register args; 50ms was host fabrication |
| C guest should timerStart | **Falsified by oneshot** — kick did not produce guest timerStart |
| D external event wait | possible later; not required to explain current idle |
| E registered, not yet triggered | **Confirmed on product natural path** |

## Missing launch contract

```text
natural_first_activator_of_registered_10140_after_child_init
```

Product direct reaches:

```text
robotol registers 0x10140 (handler 0x30631D)
→ 0x10800 ack
→ start_dsm returns
→ waits
```

It does **not** include whoever originally first called that handler. That is the “冒泡启动东西” gap — **not** a 50ms clock, and **not** Case-9 state mutation.

## Next minimal fix (P19 direction)

1. Keep product `NATURAL_ONLY` (already default).
2. On original shell / sibling netgame, capture the **first** call into the registered 10140 handler: caller module, BL/BLX, timer vs parent scheduler, timing vs start_dsm.
3. Port only that proven activator as a **generic** launch-contract continuation.
4. Do not reintroduce forced 50ms; do not implement fake timerStart; do not chase P13/P14 APIs.

## PASS answers

```text
产品默认 forced 10140 是否关闭：是（NATURAL_ONLY；forced_arm=0）
关闭后 0x10140 自然调用次数：0（×3）
关闭后最后一个真实 Guest 行为：sendAppEvent(0x10800,4) → INIT_RETURN_ZERO → start_dsm return → idle
关闭后是否仍出现 0x1E209 循环：否（0）

单次研究 kick 后是否产生自然 timer/event：无 guest timerStart；仅产生一次同 digest 的 0x1E209/Case-9
单次 kick 后状态 digest 是否变化：否（changed=0）

原冒泡中 0x10140 注册 caller：产品侧 pc=0x304599 / robotol.ext（shell live 未捕获对照）
原冒泡中首次调用来源：未在本轮 shell suite 捕获（runners 缺失 / 未达 JJFB 阶段）
原冒泡中首次调用时机：未知（待 live shell）
原冒泡中是否周期调用：未知（注册 ABI 无 period；勿默认 50ms）
原冒泡实际周期：未知
原冒泡由谁 rearm：未知
0x10140 的最终语义分类：E（已确认）+ A 倾向（父级首次激活假设）

产品直启缺失的真实 launch contract：child init 完成后对已注册 0x10140 的自然首次激活者
下一处最小通用修复：证明并移植该首次激活合同；禁止 50ms forced
是否出现真实游戏画面：否
```

## Artifacts

- `out/p18/p18_build_identity.txt`
- `reports/p18_forced_off_matrix.csv`
- `reports/p18_shell_10140_trace.csv`
- `out/p18/p18_natural_last_activity.txt`
- Runner: `research/runners/p18_run_10140_activation_contract.ps1`
- Code: `gwy_ext_obs_on_start_dsm_return` gated by `JJFB_FORCE_10140_*`
