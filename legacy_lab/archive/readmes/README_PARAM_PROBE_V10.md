# JJFB-only v10：_mr_param / gwyblink 启动上下文探测

## 这次结果结论

v9 已确认：

```text
jjfb.mrp 已经作为 dsm_gm.mrp 被 PC vmrp 直接启动
sdk_key.dat = g:u2 已通过
stdout 不再报 cann`t find sdk key
```

但 stdout 只有 PNG warning，没有网络，没有明确进入游戏登录。这说明下一关大概率是：

```text
jjfb 需要 gwy/gamelist 启动时传入的 _mr_param / gwyblink 上下文
```

而 vmrp 当前固定调用：

```text
bridge_dsm_mr_start_dsm('dsm_gm.mrp','start.mr', NULL)
```

第三参数是 NULL。

## v10 做什么

在不改 vmrp 源码的前提下，先做低成本探测：

```text
1. 固定 sdk_key.dat = g:u2
2. 固定 dsm_gm.mrp = jjfb.mrp
3. 把候选 _mr_param / gwyblink 写到多个可能文件名：
   _mr_param
   mr_param
   param
   param.txt
   gwyblink
   nmrpname
   start.arg
4. 每个候选启动一次 jjfb
5. 比较 stdout 是否出现新日志/网络/不再只有 PNG warning
```

如果 v10 仍然全部无变化，就可以确认：

```text
这个 jjfb 的启动上下文必须由 vmrp 的第三参数传入，不能靠外部文件补。
下一步必须改 vmrp 源码/二进制入口，让 bridge_dsm_mr_start_dsm 第三参数不再是 NULL。
```

## 使用

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_JJFB_PARAM_PROBE_V10.ps1
```

跑完发：

```text
logs\jjfb_param_probe_feedback_*.zip
```
