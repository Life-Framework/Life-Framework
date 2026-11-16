# world-diff fixtures

Inputs for `tools/validation/world-diff-selftest.mjs` (`tools\cli world diff`).

- `Street_Jobs.before.layer`: `Street_Jobs.layer` as on main (2026-11-16).
- `Street_Jobs.after.layer`: the maintainer's real Workbench save: the S105 child of `Roadmap_Job_TaxiPad`
  moved from `0 0 4` to `-0.773 -0.055 -0.119`, the `Roadmap_Job_Depot` block moved to the end of the file
  with `angleY 180` written as `angles 0 180 0`, and the explicit `GenericEntity` class dropped from
  `Roadmap_Job_Drop`. Expected: 1 MOVED (S105 at TaxiPad, about 4.2 m) + noise.
- `default.before.layer` / `default.after.layer`: the head of `default.layer` with the BSP line Workbench
  rewrites on every save. Expected: noise only.
