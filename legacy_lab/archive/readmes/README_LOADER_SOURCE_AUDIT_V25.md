# JJFB v25：停止盲跳，转向 vmrp 的 C/EXT loader 支持定位

## v24 结论

v24 已经确认：

```text
跳过 line157 后面的多个候选目标：
pc113 line163
pc122 line165
pc124 line166
pc129 line168
pc132 line169
pc136 line171
pc147 line172
pc148 line174
pc150 line175
```

全部都只有 PNG warning，没有 mrc_loader / robotol / 20000 / 21002。

这说明继续 patch `jjfb.mrp/start.mr` 已经没有意义。真正卡点是：

```text
PC vmrp 对 _mr_c_load / hsman / C 扩展 loader 支持不完整。
```

## v25 做什么

v25 不再启动游戏，不再 patch MRP。

它做定位报告：

```text
1. 扫描 runtime main.exe 符号和字符串：
   _mr_c_load
   _mr_c_buf
   hsman
   mrc_loader
   ext
   bridge_dsm
   mr_load
2. 提取原始 jjfb.mrp 里的 start.mr；
3. 解析 start.mr 的常量表、行号表、PC/line 对应关系；
4. 导出 line143/147/157/163/165/166/168/169/171/172/174/175 附近的 raw bytecode；
5. 下载/使用 vmrp 源码，全文搜索 loader 相关函数；
6. 打包报告。
```

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File .\RUN_PC_LOADER_SOURCE_AUDIT_V25.ps1
```

跑完发：

```text
logs\loader_source_audit_v25_feedback_*.zip
```

## 这一步的目的

定位下一步到底要改 vmrp 哪个源码函数，而不是继续盲目修改 jjfb.mrp。
