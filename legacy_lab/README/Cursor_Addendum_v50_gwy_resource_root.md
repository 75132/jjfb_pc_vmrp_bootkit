# Cursor 补充说明：gwy 资源包目录必须纳入启动契约

> 用户补充截图显示：除了 `gwy/jjfb.mrp`，还有一个完整的 `gwy` 资源包目录。  
> 路径形态类似：

```text
game_files/
  mythroad/
    240x320/
      gwy/
        cacggl/
        caclobby/
        caclottery/
        gifs/
        gkdxy/
        jjfbol/
        mhxx/
        sanguo/
        save/
        scgmj/
        smd/
        sound/
        spacetime/
        ssjx/
        tlbb/
        wm1/
        xjwq/
        ...
```

这非常重要。后续不能只把 `gwy/jjfb.mrp` 当成一个孤立文件启动。

---

## 1. 新判断

`game_files/mythroad/240x320/gwy` 很可能是冒泡网游/gwy 的完整资源根目录或游戏资源包目录。

其中：

```text
gifs/       可能是图标/列表资源；
jjfbol/     很可能是机甲风暴相关在线资源目录；
save/       可能是平台或游戏存档/缓存；
sound/      声音资源；
其他目录    可能对应其他游戏/应用。
```

所以 v49 的 GWY Launcher Shim 必须补齐的不只是参数，还包括：

```text
cwd / root / resource_root / app_root / file open path mapping
```

---

## 2. 正确目录模型

建议按下面模型理解：

```text
mythroad root:
  game_files/mythroad/240x320/

gwy app root:
  game_files/mythroad/240x320/gwy/

target mrp:
  game_files/mythroad/240x320/gwy/jjfb.mrp

possible jjfb resource dir:
  game_files/mythroad/240x320/gwy/jjfbol/
```

不要把 `gwy` 目录里的资源展平到 runtime 根目录。  
必须保持原目录结构。

---

## 3. 对 GWY Launcher Shim 的要求

启动时应打印并确认：

```text
[JJFB_GWY_ROOT] mythroad_root=...\game_files\mythroad\240x320
[JJFB_GWY_ROOT] gwy_root=...\game_files\mythroad\240x320\gwy
[JJFB_GWY_ROOT] target=...\game_files\mythroad\240x320\gwy\jjfb.mrp
[JJFB_GWY_ROOT] resource_dirs=gifs,jjfbol,save,sound,...
```

启动参数仍然是：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

但文件系统解析必须基于：

```text
mythroad_root = game_files/mythroad/240x320
```

这样 guest 里访问：

```text
gwy/jjfb.mrp
gwy/jjfbol/...
gwy/gifs/...
gwy/save/...
```

才能命中真实文件。

---

## 4. 文件打开路径追踪必须升级

后续日志不要只看 mrp/ext 加载。  
必须跟踪所有 file open：

```text
[JJFB_FILEOPEN] guest="gwy/jjfb.mrp" host="...\240x320\gwy\jjfb.mrp" ok=1
[JJFB_FILEOPEN] guest="gwy/jjfbol/..." host="...\240x320\gwy\jjfbol\..." ok=?
[JJFB_FILEOPEN] guest="gwy/gifs/..." host="...\240x320\gwy\gifs\..." ok=?
[JJFB_FILEOPEN_MISS] guest="..." tried=[...]
```

如果大量资源 miss，优先修 path mapping，而不是修 UI/动画。

---

## 5. v49/v50 新增任务

### P0：扫描资源包

新增报告：

```text
reports/v50_gwy_resource_tree.md
```

内容：

```text
1. game_files/mythroad/240x320/gwy 目录树；
2. 每个一级目录用途猜测；
3. jjfbol 目录内文件清单；
4. gifs 目录内文件清单；
5. 是否存在 jjfb.mrp、mrc_loader.ext、robotol.ext、cfg.bin、gamelist.ext 等关键文件；
6. 启动运行时哪些资源被请求、哪些 miss。
```

### P1：建立路径映射表

新增报告：

```text
reports/v50_file_path_mapping.md
```

包含：

```text
guest path
host resolved path
exists?
opened by pc/lr
result
```

### P2：修 runtime 目录准备脚本

准备脚本应确保：

```text
runtime/vmrp_win32/vmrp_win32_20220102/mythroad/240x320/gwy
```

或实际 vmrp 需要的等价目录中，完整存在 `gwy` 资源包。

不要只复制 `jjfb.mrp`。

---

## 6. 给 Cursor 的一句话

**补充关键事实：用户截图显示存在完整 `game_files/mythroad/240x320/gwy` 资源包目录，里面包含 `gifs/`、`jjfbol/`、`save/`、`sound/` 等。v49/v50 的 GWY Launcher Shim 不能只启动孤立的 `gwy/jjfb.mrp`，必须把 `mythroad/240x320` 作为 root，把 `gwy/` 作为 app/resource root，保留原目录结构，并升级 file open trace，确认 `gwy/jjfbol/...`、`gwy/gifs/...`、`gwy/save/...` 等资源是否能正确命中。若 UI/启动异常，优先查资源路径 miss，而不是再手动修 UI。**
