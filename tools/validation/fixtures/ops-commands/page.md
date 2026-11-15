# Fixture ops page

Run `tools\cli host status` to see the log path: a real command, no marker.

Run `tools\cli host zz-planned-after` *(planned)* to compare the live config: a missing command marked planned.

Run `tools\cli host zz-not-a-command` to do something no file implements: missing and unmarked.

Run `tools\cli host backup` *(planned)* to take a backup: a real command still marked planned (stale).

*(planned)* `tools\cli host zz-planned-before` has its marker first, as the runbook writes it: missing, marked planned.

*(planned)* `tools\cli host restore` is a real command with the marker first (stale).

The `tools\cli host` commands need Node: a bare family name, nothing to check after it.

```
tools\cli validate
tools\cli zz-fixture-missing --flag
tools\cli host start (planned)
```
