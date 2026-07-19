# v47 startup string xref

## Watched strings (GBK in robotol image)

| VA | tag | meaning |
|----|-----|---------|
| 0x313B30 | check_net_timeout | 连接超时，请检查网络 |
| 0x313C48 | conn_fail | 连接失败，请重试 |
| 0x313C5C | connecting | 连接中，请稍等 |
| 0x313C74 | dl_resource | 正在下载资源文件… |
| 0x313CF4 | check_update_list | 检查更新列表 |
| 0x314204 | logging_in | 正在登陆,请稍等 |

## Runtime

```text
JJFB_STARTUP_STR watching …  (install only)
JJFB_STARTUP_STR] #…         = 0 hits  (natural + linear driver to prog=12)
```

## Static

Scanned code/pool bands `0x2EF000..`, `0x304000..`, `0x2D9000..`, `0x312000..` for LE word == string VA:

```text
str_check_update / connecting / … total_hits = 0
```

Likely causes:

1. References via register-computed addresses (not raw VA literals in those bands).
2. Referencing code lives outside scanned ranges.
3. Stage never linked from current splash path.

## Gate test (linear progress driver)

```text
progress_count driven 0 → 12
bar PROGRESS_DRAW occurs
STARTUP_STR still 0
AC8 still 0 (no guest write)
ui_mode stays 0x45
```

**Conclusion:** reaching progress=12 is **not sufficient** to enter 检查更新/网络 string UI.  
There is **another gate** beyond progress_count (event / AC8 / other ERW flag / different ui_mode sub-state).

## Next

1. Widen string VA scan (full robotol map) + ADR/LDR pool patterns.
2. Find functions that *display* text near splash (e.g. textbar draw) and their predicates.
3. Correlate with sendAppEvent / network callbacks that never fire.
