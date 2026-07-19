# 测试与验收标准

## 1. 核心反跑偏门禁

每次构建前运行：

```bash
python tools/audit_launcher_core.py .
```

核心源代码不得包含：

- `ui_mode=0x45`；
- B70/B71/B6C/AC8/134D/BA0；
- `0x2DADC4/0x2FC418/0x2EF86C/0x305EB8/0x306344`；
- `FAMILY_C0_AFTER_B71` 等实验变量；
- host fake splash/progress。

## 2. 格式解析测试

### MRP

- magic 错误；
- total length 不一致；
- index 越界；
- member 越界；
- gzip 解压；
- raw member；
- duplicate member；
- hash。

黄金数据：`evidence/key_mrp_manifest.json`。

### cfg

- index 36 字段；
- record 越界；
- layout 不匹配；
- UTF-16BE title；
- target path；
- unknown raw preservation。

## 3. VFS 测试

```text
mythroad/gwy/jjfb.mrp
gwy/jjfb.mrp
mythroad/sdk_key.dat
../escape
absolute Windows path
mixed slash
case variant
write overlay
```

验收：任何逃逸被拒绝，source hash 不变。

## 4. LaunchDescriptor 测试

- cfg/profile 一致；
- nextid/ncode 错误；
- hash 错误；
- appid/appver 错误；
- parameter serialization；
- immutable after validation。

## 5. Scheduler 测试

- 注册 family handler；
- 注册 timer callback；
- timer 到期；
- callback 中再次 enqueue；
- 确认非重入；
- pause 停止或调整 timer；
- resume；
- exit 清理。

## 6. JJFB 分阶段验收

### A. 资源

- cfg index 36 解析；
- target hash 正确；
- VFS miss=0（loader 阶段）。

### B. Loader

```text
start.mr 1514
mrc_loader.ext 219
robotol.ext 161178
```

### C. EXT init

```text
version 6 → 0
appInfo 8 → 0
init 0 → 0
```

### D. Registry

至少捕获 guest 主动注册的 family/timer/enqueue/callback，不硬编码地址。

### E. Scheduler

由标准 lifecycle/timer 触发已注册 callback，不检查 JJFB 内存门。

### F. 自然行为

游戏自行请求资源、显示或网络；不以某个特定 splash 为硬验收。

## 7. 回归对照

至少选择两个其他 GWY MRP 做 smoke test，例如 `ssjx.mrp`、`tlbb.mrp`：

- MRP parser；
- descriptor；
- loader 到可到达阶段；
- 不要求游戏完整运行；
- 用于证明 core 不完全绑定 JJFB。

## 8. 发布验收

- 单个完整 ZIP；
- manifest SHA-256；
- source/test/docs/profile；
- 不包含被修改的原始游戏 MRP；
- 配置由用户指向本地 resource root；
- README 清楚标注 research/compatibility purpose。
