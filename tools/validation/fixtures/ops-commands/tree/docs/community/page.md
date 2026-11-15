# Fixture community page

Type `tools\cli zz-community-unmarked` to do something no file implements: missing and unmarked (warns).

Type `tools\cli host zz-community-planned` *(planned, #1500)* once it exists: missing, numbered marker (clean).

Type `tools\cli host status` *(planned)* to see the log path: a real command still marked planned (stale marker).

Type `tools\cli host status` for the same, no marker: clean.
