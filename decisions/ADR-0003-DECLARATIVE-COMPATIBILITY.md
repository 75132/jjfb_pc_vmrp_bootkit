# ADR-0003：兼容差异必须声明式表达

- 状态：Accepted
- 日期：2026-07-18

目标差异使用 profile、resolver alias、capability flag 和严格后置条件表达。核心禁止固定地址、ERW offset 和 host 假 UI。

JJFB 首个合法例子：`cfunction.ext -> robotol.ext`，且仅 exact miss 后生效。
