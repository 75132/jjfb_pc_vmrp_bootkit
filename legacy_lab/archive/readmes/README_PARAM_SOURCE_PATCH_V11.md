# JJFB-only v11：给 vmrp 源码打补丁，让 bridge_dsm_mr_start_dsm 第三参数不再是 NULL

## 这次 v10 结果

v10 的所有候选结果都完全一样：

```text
baseline_no_param：只有 9 条 libpng warning
cand_0：一样
cand_1：一样
cand_2：一样
cand_3：一样
cand_4：一样
cand_5：一样
```

说明：

```text
把 _mr_param / gwyblink 写成外部文件，对 jjfb 没有任何作用。
```

现在已经确认：

```text
PC vmrp 直接启动 jjfb.mrp：成功
sdk_key.dat = g:u2：成功
图片资源加载：成功
外部文件补 _mr_param：失败
```

所以下一步不是继续换文件，而是改 vmrp 启动器本身。

## 为什么必须改 vmrp

vmrp 源码里固定这样启动：

```c
char *filename = "dsm_gm.mrp";
char *extName = "start.mr";
uint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, NULL);
```

第三参数是 `NULL`。

但 jjfb 原本不是裸启动，它需要 gwy/gamelist 传入类似：

```text
napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

所以 v11 的核心就是把源码改成：

```c
char *startParam = "napptype=0_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink";
uint32_t ret = bridge_dsm_mr_start_dsm(uc, filename, extName, startParam);
```

## 本包做什么

```text
1. 下载 vmrp 源码 master.zip；
2. 修改 vmrp.c；
3. 生成补丁版 vmrp.c；
4. 如果本机有 mingw32-make，就尝试编译；
5. 如果没有编译环境，就生成 patch/source 包和诊断报告；
6. 同时生成 main.exe 二进制 callsite 扫描报告，后续可尝试二进制补丁。
```

## 运行

在 `jjfb_pc_vmrp_bootkit` 根目录解压覆盖，然后运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_PATCH_VMRP_PARAM_V11.ps1
```

跑完发：

```text
logs\vmrp_param_patch_feedback_*.zip
```

## 说明

这一步不需要管理员，不跑网络，不跑 portproxy。

如果本机没有 `mingw32-make`，编译不会成功，但仍然会生成：
- patched vmrp.c
- patch diff
- main.exe callsite 扫描报告

把反馈包发回来后，我继续看能不能做二进制补丁。
