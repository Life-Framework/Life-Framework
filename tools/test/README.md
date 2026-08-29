# Tests

Drop any runnable script here (`.ps1`, `.cmd`, `.mjs`, `.js`, `.cjs`, `.sh`)
and it runs automatically via:

```
tools\cli test
```

Each script runs with the repo root as the working directory. Exit `0` =
pass, non-zero = fail (the CLI reports PASS/FAIL per script).

Ideas for this project:

- **resource tests** - verify every `.meta` has a sibling resource and every
  `{GUID}path` reference resolves to a registered resource (extend
  `tools/validation/validate-repo.ps1` or duplicate its logic here for wider
  checks).
- **smoke test** - boot the DebugWorld headless in a Reforger dedicated server
  (Docker) and grep the log for script/resource errors.
- **script tests** - run `vitest` inside the MCP clones (they ship test
  suites) to confirm the tooling itself still works after updates.