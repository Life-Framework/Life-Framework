# Fixture triage table (opening-b11-selftest)

| Message (pattern) | Verdict | What it is / what to do |
|---|---|---|
| `[LFDebug:Police] staff approve refused` / `staff approve by <name> refused` | BY DESIGN | Fixture row: a non-staff or invalid approve is refused, the second span inherits the Police tag. |
