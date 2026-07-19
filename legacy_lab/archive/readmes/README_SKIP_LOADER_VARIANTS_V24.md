# JJFB v24：跳过 loader object 调用的多目标探测

## v23 结果

v23 已经证明：

```text
sdk_key 错误分支跳过成功
程序进入 start.mr 正常流程
当前卡点变成：
start.mr:157: err:call field `hsman' (object)
```

另外 v23 的 `target_net=True` 需要修正：  
那是脚本误判了 `111.123.57.113:80` 这种无关连接，不是 jjfb 的 20000/21002/6009。

## 当前判断

现在卡点已经非常明确：

```text
PC vmrp 的 _mr_c_load / hsman C loader 对象不能像原平台那样调用
```

也就是：

```text
不是 sdk_key
不是 start.mr 读不出来
不是 main.exe 第 4 参数没有注入
而是 PC vmrp 的 C/EXT loader 支持不完整
```

## v24 做什么

v24 不再手工猜一个跳转点，而是自动做多目标探测：

```text
1. 解析 start.mr 字节码 lineinfo；
2. 保留 main.exe 第 4 参数注入；
3. 保留跳过 sdk 错误分支；
4. 对 line157 后面的多个候选目标行逐个测试；
5. 每个候选都把 line143/147/157 直接 JMP 到该目标；
6. 启动 15 秒；
7. 记录新的错误、mrc_loader、robotol、connect、20000/21002/6009。
```

如果所有候选都只是白屏或新 object 错误，就可以确认：

```text
必须修 vmrp 的 _mr_c_load/hsman 支持，继续 patch jjfb.mrp 意义不大。
```

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_SKIP_LOADER_VARIANTS_V24.ps1
```

跑完发：

```text
logs\skip_loader_variants_v24_feedback_*.zip
```

以及窗口截图。
