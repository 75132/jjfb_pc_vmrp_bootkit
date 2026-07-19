# v50 GWY Launcher Mode 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v50_gwy_launcher_mode_stdout.txt`
- 总行数：75
- FILEOPEN 成功：3
- FILEOPEN_MISS：0（唯一 guest：0）
- `gwy/jjfb.mrp` 主机打开成功：是
- `mrc_init(0)` 返回 0：否/未捕获
- robotol 相关日志出现：否

## 1. 启动契约日志

```text
[JJFB_GWY_LAUNCH] cfg_index=36 target=gwy/jjfb.mrp
[JJFB_GWY_LAUNCH] param=napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
[JJFB_GWY_LAUNCH] startGame/runapp equivalent called
[JJFB_GWY_ROOT] mythroad_root=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_win32\vmrp_win32_20220102\mythroad\240x320
[JJFB_GWY_ROOT] gwy_root=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\runtime\vmrp_win32\vmrp_win32_20220102\mythroad\240x320\gwy
[JJFB_GWY_ROOT] target=C:/Users/24231/Desktop/jjfb_pc_vmrp_bootkit/runtime/vmrp_win32/vmrp_win32_20220102/mythroad/240x320/gwy/jjfb.mrp
[JJFB_GWY_ROOT] resource_dirs=gifs,jjfbol,save,sound,...
[JJFB_CFG36] nextid=482 ncode=512 napptype=12 narg=0 narg1=1 appid=400101 appver=12
```

## 2. 路径打开统计

| guest | 成功次数 | miss 次数 | 最近 host/尝试路径 |
|---|---:|---:|---|
| `mythroad/gwy/jjfb.mrp` | 1 | 0 | `C:/Users/24231/Desktop/jjfb_pc_vmrp_bootkit/runtime/vmrp_win32/vmrp_win32_20220102/mythroad/240x320/gwy/jjfb.mrp` |
| `mythroad/sdk_key.dat` | 1 | 0 | `mythroad/sdk_key.dat` |
| `mythroad/system/gb16.uc2` | 1 | 0 | `mythroad/system/gb16.uc2` |

## 3. 首批 miss（最多 80 条）

```text
（无）
```

## 4. loader / robotol 关键日志（最多 120 条）

```text
[JJFB_LOADER] main start
[JJFB_LOADER] _mr_c_function_new #1 helper=000a4088 len=20
[JJFB_LOADER] mr_c_function_load done, r9/ER_RW=@00280400
[JJFB_LOADER] bridge_dsm_mr_start_dsm filename=gwy/jjfb.mrp extName=start.mr entry=napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
[JJFB_LOADER] bridge_dsm_mr_start_dsm ret=0x0
[JJFB_801] P@0x2803E4 = {ER_RW=0x280400 len=9680 type=1 chunk=0x0 stack=0x0}
[JJFB_801] ext_call code=6 input=0x0 len=1968 P=0x2803E4 erw=0x0 helper=0xA4088
[JJFB_801] ext_call code=6 ret=0 out_len=0
[JJFB_801] host version(6) ret=0
[JJFB_801] P@0x2803E4 = {ER_RW=0x280400 len=9680 type=1 chunk=0x0 stack=0x0}
[JJFB_801] ext_call code=8 input=0x2829D4 len=16 P=0x2803E4 erw=0x0 helper=0xA4088
[JJFB_801] ext_call code=8 ret=0 out_len=0
[JJFB_801] host appInfo(8) ret=0
[JJFB_801] P@0x2803E4 = {ER_RW=0x280400 len=9680 type=1 chunk=0x0 stack=0x0}
[JJFB_801] installed sendAppEvent hook @0x2829D4
[JJFB_801] synthesized mrc_extChunk @0x2829DC -> P+0xc (send=@0x2829D4 extMrTable=0x0)
[JJFB_801] ext_call code=0 input=0x0 len=1968 P=0x2803E4 erw=0x0 helper=0xA4088
[JJFB_801] _mr_TestCom(L=0x0, in0=7, in1=9999)
[JJFB_801] ext_call code=0 ret=20210701 out_len=0
[JJFB_801] host mrc_init(0) ret=20210701
```

## 5. 网络相关日志（最多 80 条）

```text
（无）
```

## 6. 自动判定

- 文件路径未见 miss，但 loader 初始化未确认成功；下一轮审计 `_strCom 601/800/801` 的实际顺序和返回值。

## 7. 人工对照结论（本机 Windows 实测后补充）

### 7.1 三个 v50 修复已在运行中成立

| 检查项 | 结果 |
|---|---|
| 入口 `gwy/jjfb.mrp` | 是，canonical host 打开成功 |
| APPID/APPVER | `400101/12`（非 nextid=482） |
| FILEOPEN_MISS | 0 |

### 7.2 真正的下一 blocker：`start.mr` 未加载 robotol

对比 v49（`dsm_gm.mrp`）与 v50（`gwy/jjfb.mrp`）：

```text
v49: start.mr → sdk_key.dat → 再开 MRP unzip(217)=mrc_loader
         → 再开 MRP unzip(161178)=robotol → helper=0x304AE5
         → host mrc_init ret=0 → robotol timer RUNNING

v50: start.mr → sdk_key.dat → "cann`t find sdk key!"
         → 不再二次/三次打开 MRP，无 mrc_loader/robotol
         → host 仍打 code=0，但 helper 仍是平台桩 0xA4088
         → mrc_init ret=20210701(=VMRP_VER) → 不启动 robotol timer
```

因此 `mrc_init ret=20210701` **不是**路径映射失败，而是 host 801 打到了错误 EXT（平台 stub），因为 `start.mr` 在 sdk key 检查后提前结束了 loader 链。

### 7.3 旁证

- `dsm_gm.mrp` 与 canonical `jjfb.mrp` 同为 414602 字节，但 **SHA256 不同**（内容非同一副本）。
- `start.mr` 解压长度：v49=`1513`，v50=`1514`。
- `mythroad/sdk_key.dat` 仅 4 字节：`67:3A:75:32`（`g:u2`）；文件能打开，但当前 `jjfb.mrp` 内 `start.mr` 判定 key 无效。

### 7.4 下一步最小任务

**不要回 UI。** 只查：为何当前 `gwy/jjfb.mrp` 的 `start.mr` 在 sdk key 后中断，而不再 `_mr_c_load` `mrc_loader.ext` / `robotol.ext`。

允许方向：

1. 对照 `dsm_gm.mrp` 与 `jjfb.mrp` 的 `start.mr` / sdk 校验差异（不改 MRP 逻辑作正式方案）；
2. 补齐/修正 host 侧 `sdk_key.dat` 或平台能力，使原始 `start.mr` 自然继续；
3. 确认二次 `mr_open(mythroad/gwy/jjfb.mrp)` 是否因路径形态被拒（若出现 FILEOPEN_MISS 再修映射）。
