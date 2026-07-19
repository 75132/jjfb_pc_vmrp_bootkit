# 本包工具与骨架自测报告

## 1. Python 工具语法

```text
python3 -m py_compile tools/*.py
PASS
```

## 2. JJFB MRP golden

实测断言：

```text
SHA256 = 52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036
start.mr       = 1514 / 3787
mrc_loader.ext = 219 / 232
robotol.ext    = 161178 / 253420
PASS
```

## 3. cfg index 36 golden

字节级断言：

```text
napptype = 12
nextid   = 482 (BE24 @ 0x72)
ncode    = 512 (BE24 @ 0x78)
narg     = 0   (BE24 @ 0x7B)
narg1    = 1   (U8   @ 0x7E)
target   = gwy/jjfb.mrp
PASS
```

## 4. clean repo skeleton

```text
cmake configure: PASS
cmake build:     PASS
ctest smoke:     PASS
```

该测试在当前 Linux 容器验证的是骨架和接口；Windows 32-bit vmrp/SDL/Unicorn 集成仍由 Cursor 在本机工具链完成。

## 5. 反跑偏审计

```text
clean skeleton: PASS
人工加入 0x2DADC4 到 src/main.c: 正确失败，exit code 2
```

说明审计器能把固定 JJFB 地址挡在 clean core 之外。
