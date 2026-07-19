# 最新路线：v55 ui_mode Writer Coverage

## 已锁定

- v53：alias + MR_IGNORE → host 6/8/0 → timer。
- v54：GWY 默认 NO FORCE；ui_mode 自然停在 0。
- v55：**自然 writer = `0x2FC418`**（`STR 0x45 → ERW+0x8D0`）。
- 上游：`2FC03C ← 2DAE24(B70≠0) ← 2DADC4 ← {2FECA2, 2E4066, 305EF4}`。
- 动态：handoff 后 25s 内上述 PC **全部 0 命中**。

## v56 唯一任务

找出谁应调用 `2DADC4` / `2FECA2` / `2E4066` / `305EF4`：

```text
平台事件？ _strCom？ 网络回调？ 未置位的 ERW 门？
```

禁止 FORCE ui_mode / AC8 / progress。

## 证据位置

```text
reports/v55_uimode_writer_implementation.md
reports/v55_uimode_writer_run_result.md
reports/v55_uimode_writer_static_map.md
logs/v55_uimode_writer_stdout.txt
```
