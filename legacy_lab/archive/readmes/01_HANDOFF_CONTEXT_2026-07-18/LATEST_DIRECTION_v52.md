# 最新主线：v52 MRP 内成员别名

## 已确认前提

v51 已解决 SDK key，并在原始 `gwy/jjfb.mrp` 上稳定达到：

```text
start.mr (1514) → mrc_loader.ext (219)
```

随后 loader 请求 MRP 内不存在的 `cfunction.ext`，报 `3006` 并退出。原包实际包含 `robotol.ext`，压缩长度为 `161178`。

## v52 唯一任务

不改 MRP 文件，只在 guest 内存中把第二阶段 loader 的请求字面量：

```text
cfunction.ext → robotol.ext
```

静态审计证明该字面量位于解压后 loader 文件偏移 `0xD4`，即运行时 `helper + 0xCC`。

v52 同时禁止在第三个 EXT helper 尚未注册时调用 host 801，避免把 mrc_loader 的 `mrc_init=0` 误判为 robotol 成功。

## 运行

```powershell
.\RUN_V52_MRP_MEMBER_ALIAS.ps1 -Seconds 25
```

## 结果分流

1. `[1514,219,161178]` 且出现 `[JJFB_ROBOTOL_LOAD]`：别名成功，下一轮只查 robotol `_strCom 800/801`。
2. 出现 `161178` 但无第三个 helper：查 robotol EXT 注册入口。
3. alias patched 但仍无 `161178`：查 mrc_loader 是否在 host 打补丁前复制了请求名，或后续是否恢复了原字符串。
4. `[JJFB_801_GUARD] ... skip_host_801`：保护生效，不得把当前 helper 的 `ret=0` 当作 robotol。

不要回 UI、不要使用 v49 的 `dsm_gm.mrp`，也不要修改 canonical `jjfb.mrp`。
