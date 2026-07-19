# Latest Direction v51 — Valid SDK Key on Original GWY Launch Chain

## 主线

继续 v50 GWY Launcher Mode，保留原始 `gwy/jjfb.mrp`，只修 host 侧：

1. 将 `mythroad/sdk_key.dat` 优先映射到选定的 `mythroad/240x320` 根；
2. 按 VMRP 当前 `GetSysInfo` 身份生成 48 字节有效 SDK key；
3. 观察原始 `start.mr` 是否自然继续加载 `mrc_loader.ext` 和 `robotol.ext`。

## 已否定的误判

- `g:u2` 不是有效 key；
- v49 `dsm_gm.mrp` 不是原始 MRP，而是历史 `skip_sdk_only_v27` 补丁文件；
- v49 的 robotol 成功链不能用来证明原始 `start.mr` 已满足 SDK 校验；
- 不回 UI，不继续 v49 splash/progress gate。

## 当前有效身份

```text
vmver=1968
IMEI=864086040622841（GetSysInfo 中为 16 字节，含末尾 NUL）
hsman=vmrp
hstype=vmrp
```

有效 key：48 字节，SHA-256：

```text
5d87a42f3d47ac8ddaf892f08409373b18936af761c6b9c8331750dbad3cc436
```

## 唯一运行入口

```powershell
.\RUN_V51_VALID_SDK_KEY.ps1 -Seconds 25
```

结果：

```text
logs/v51_valid_sdk_key_stdout.txt
reports/v51_valid_sdk_key_run_result.md
```
