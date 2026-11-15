# Fixture ops page

Run `tools\cli host status` to see the log path: a real command, no marker.

Run `tools\cli host drift` *(planned)* to compare the live config: a missing command marked planned.

Run `tools\cli host zz-not-a-command` to do something no file implements: missing and unmarked.

The `tools\cli host` commands need Node: a bare family name, nothing to check after it.

```
tools\cli validate
tools\cli zz-fixture-missing --flag
```
