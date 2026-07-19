# JJFB v51 Valid SDK Key Patch

这是 v50 的增量补丁。解压后把全部内容覆盖到 `jjfb_pc_vmrp_bootkit` 项目根目录。

## 本轮改动

- 不修改 `gwy/jjfb.mrp`；
- 修正 `mythroad/sdk_key.dat` 的 canonical 根优先映射；
- 按当前 VMRP `GetSysInfo` 身份生成正确的 48 字节二进制 key；
- 同步写入旧扁平路径，确保使用 `-SkipBuild` 时也不会再读到 4 字节 `g:u2`；
- 自动判断原始 `start.mr → mrc_loader.ext → robotol.ext` 是否恢复。

## 运行

```powershell
.\RUN_V51_VALID_SDK_KEY.ps1 -Seconds 25
```

正常情况下不应再出现：

```text
cann`t find sdk key!
```

并应依次出现：

```text
mr_get_method(1514)
mr_get_method(217)
mr_get_method(161178)
```

运行后发送：

```text
reports/v51_valid_sdk_key_run_result.md
```

不要运行 v49 UI gate，也不要把 `dsm_gm.mrp` 复制回 canonical `gwy/jjfb.mrp`。
