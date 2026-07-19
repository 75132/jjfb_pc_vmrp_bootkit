# 独立外壳启动器产品规格（MVP → 完整版）

## 1. 产品定位

这是本地 GWY/MRP 游戏启动器和兼容 runtime，不是旧“冒泡网游”商城复刻，也不是 JJFB 修改器。

## 2. 用户流程

### MVP CLI

```text
首次运行
→ 选择 mythroad resolution root
→ 扫描 gwy/cfg.bin 与 *.mrp
→ 显示发现的游戏/验证状态
→ 选择 profile/目标
→ validate
→ launch
→ 查看运行状态和日志
```

### 后续 GUI

左侧游戏列表，右侧显示：

- 名称/图标；
- MRP 路径、APPID、版本、hash；
- cfg 字段；
- profile 匹配状态；
- 平台 capability；
- Launch / Inspect / Open Logs。

GUI 只管理启动，不代画游戏内容。

## 3. CLI 命令

```text
gwy_launcher scan --root <root> --output games.json
gwy_launcher inspect --root <root> --cfg gwy/cfg.bin --index 36
gwy_launcher validate --profile <json>
gwy_launcher launch --profile <json> [--offline]
gwy_launcher replay --trace <jsonl>
gwy_launcher doctor --profile <json>
```

## 4. `scan` 输出

每个候选游戏：

```json
{
  "cfg_index": 36,
  "title_raw": "风暴(火爆公测)",
  "icon": "ng_jjfb.gif",
  "target": "gwy/jjfb.mrp",
  "exists": true,
  "mrp": {
    "sha256": "...",
    "appid": 400101,
    "appver": 12,
    "entry_present": true,
    "reg_present": true
  },
  "profile_matches": ["gwy.jjfb.original"],
  "confidence": "empirical_cfg_layout"
}
```

不要把解析不确定字段伪装成确定。

## 5. `doctor` 检查

- 32-bit runtime/DLL；
- resource root；
- cfg/profile/hash；
- overlay 可写；
- SDK key golden；
- MRP decode；
- SDL/Unicorn init；
- network policy；
- core anti-drift audit build marker。

## 6. 配置和数据目录

```text
program/
  gwy_launcher.exe
  profiles/
  runtime DLLs/

user data/
  saves/<profile-id>/
  cache/<profile-id>/
  logs/<launch-id>/
  config.json
```

原始 resource root 只读。

## 7. 安全默认值

- 默认不连接旧 shell/update endpoints；
- 默认不修改 hosts；
- 默认不写原资源；
- target hash 不匹配则拒绝 profile alias；
- 未知 platform service 返回 unsupported；
- 日志不记录密码/完整敏感身份信息；
- crash dump 可配置并默认最小化。

## 8. MVP 出包标准

MVP 不是以“联网玩到内容”为标准，而是：

- 扫描/验证/descriptor；
- 原始 loader chain；
- 基础平台 ABI；
- scheduler；
- framebuffer/input；
- 可解释的网络尝试；
- 一个稳定 JJFB profile + 两个 cross-target smoke profile；
- clean audit 通过。
