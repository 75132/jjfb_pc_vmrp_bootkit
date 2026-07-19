# Cursor 执行手册

## 1. Cursor 每次开工前必须回答

```text
本任务是否在开发“外壳启动器/平台 ABI”？
是否需要修改原始 MRP？（正确答案：否）
是否涉及 JJFB 固定地址/ERW offset？（若是，放 legacy_lab，不进 core）
能否写成通用模块和测试？
本阶段完成标准是什么？
```

## 2. 首轮任务建议

### Task 1：创建 clean repository layout

不要复制修改版 `bridge.c`。复制：

- upstream vmrp；
- 本包 tools；
- schema/profile；
- rules/skill；
- tests skeleton。

### Task 2：实现 MRP parser

对照 `evidence/key_mrp_manifest.json` 做黄金测试。

### Task 3：实现 cfg parser/descriptor

对照 `evidence/cfg_index36_record.json`。

### Task 4：实现 VFS overlay

先不启动游戏，写完整测试。

## 3. 修改要求

- 每次最多推进一个 Phase；
- 先测试再接入 runtime；
- 新文件优先，避免巨型 bridge；
- 返回值和 ownership 写在头文件；
- 未知字段明确命名 `unknown_*`；
- 不能靠“日志里看起来像成功”宣布完成；
- 不删除 legacy 证据。

## 4. 禁止 Cursor 自作主张

- 不继续 v87/v88 固定地址路线；
- 不写新的 FORCE 环境变量；
- 不把 C0、event 5/12 当作已知标准；
- 不 host 画 UI；
- 不修改 MRP hash；
- 不把 `start_dsm` 调用等同完整 launcher；
- 不用网络 mock 伪造游戏业务成功。

## 5. 每次提交模板

```text
feat(formats): parse MRPG member index safely
feat(vfs): add canonical + writable overlay
feat(runtime): resolve main EXT through reg/profile
feat(platform): add non-reentrant callback scheduler
test(jjfb): validate original target loader chain
```

## 6. 每轮回复用户模板

```text
本轮阶段：Phase X
完成：...
修改：...
测试：命令 + 结果
证据：...
未解决：...
反跑偏审计：通过/失败
下一步：Phase X 的下一项
```
