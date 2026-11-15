# opening-b11 fixture

`console.log` is a server console log in the shape of a TestLogic run of `harness/staff-approve` (2026-11-15):
the engine header with the addon list and the mounted addon directories, the ready line and the script tail
with four Police warnings and one Whitelist warning. The checkout path is `C:\lf\host`. The first capture was
never committed (the `*.log` ignore kept it out of the PR), so this file was rebuilt from the same shape;
`.gitignore` now tracks `tools/validation/fixtures/**/*.log`. `triage.md` knows the Police warnings
(BY DESIGN) and not the Whitelist one, so the beat sees 4 known groups and 1 new one. The tail holds no
`SCRIPT (E)` line, because `server-doctor` fails a boot that has one and the beat judges the real doctor.
