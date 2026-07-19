# Forbidden Shortcuts and Safety Boundaries

This project is local archival/compatibility boot restoration.

Forbidden:

```text
patching original game logic
fake game UI
host overlay pretending game screen
hardcoded P+0xC to arbitrary address
R9 promotion without module ER_RW evidence
skip fault and call it success
host_runapp_equivalent pretending native shell
using string VA as function entry
fabricating server/login success
```

Allowed:

```text
platform shim reconstruction
GWY obsolete update no_update result
package scoped cfunction primary resolution
member view generation
mrc_extChunk platform publication
ER_RW/R9 module binding
slot API stubs only after real slot call, with observe logs
local file path repair
natural rendering API repair
```
