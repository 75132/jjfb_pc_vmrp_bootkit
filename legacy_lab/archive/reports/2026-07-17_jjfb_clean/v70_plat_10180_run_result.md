# v70 plat 0x10180 / 0x10130 run result

## 1. goal

- 补齐 `0x10180`（2F65BC userinfo blob）与 `0x10130`（大块 alloc notify）
- 避免 unhandled ret=1 污染 ERW；观察是否推进 B71/B70

## 2. counts

- `v70_banner`: 2
- `h10180`: 2
- `h10130`: 2
- `un_10180`: 0
- `un_10130`: 0
- `leave_2fc26c`: 1
- `after_ret`: 1
- `b71_set`: 0
- `b70_set`: 0
- `entry_2febbc`: 0
- `fail_b71`: 16
- `pass_305eb8`: 0
- `bl_2fc03c`: 0
- `force_ui`: 0

## 3. conclusion

- 10180 已生效且 Path A 完成，但 B71 仍恒 0。

## 4. next

- B58 仍空 → 30ED2C 未跑；下一刀：第二包填 B58 或自然网络包，勿 FORCE B71。

## 5. notes

- MOUSE_UP→B71 已证伪（会置 134D=2）
- Path A 空 body → 2FC26C；B58 空 → 不进 30ED2C
- 禁止 FORCE B70/B71 / C0 / ui_mode
