# Fixture triage table (log-triage-selftest)

| Message (pattern) | Verdict | What it is / what to do |
|---|---|---|
| `[LFDebug:Persistence] save point FAILED` | FIXED | Fixture row: a specific error line. |
| `[LFDebug:Jobs] job request refused` / `equipment refused` | BY DESIGN | Fixture row: the second span inherits the Jobs tag. |
| `Missing string ID = <key>` | FIXED | Fixture row with a `<key>` wildcard. |
| `VME: NULL pointer to instance in LF_BarComponent.OnInit` | OPEN | Fixture row: a VME line, line number varies. |
| `[LFDebug:Siren] ...` | INFO | Tag-only row: too generic to vouch for an error or warning. |
