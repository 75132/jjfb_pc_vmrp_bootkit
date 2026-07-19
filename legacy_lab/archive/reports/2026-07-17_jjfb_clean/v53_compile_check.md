# v53 C 静态编译检查

- 检查目标：叠加 v50、v51、v52、v53 后的完整 `bridge.c` 与 `vmrp.c`
- 编译器：Linux GCC，仅做语法检查
- 命令：

```text
gcc -fsyntax-only -g -Wall -DNETWORK_SUPPORT -DVMRP -D_WIN32 \
  -I./windows/unicorn-1.0.2-win32/include bridge.c vmrp.c
```

- 退出码：`0`
- warning：15 条
  - pointer → integer 宽度：7
  - integer → pointer 宽度：7
  - 原工程未使用变量：1
- error：0

这些 warning 来自用 64 位 Linux 编译器检查面向 32 位 Windows 的旧工程，不是 v53 新增语法错误。
