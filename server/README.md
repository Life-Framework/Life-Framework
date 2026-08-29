# Server Setup

Headless server tooling for Life Framework. The automated test pipeline runs a
dedicated server on the DebugWorld scenario and parses the `ELTEST` suite
results — see `../AGENTS.md` for the full agent workflow.

## Test server

```
tools\cli test        # build + boot + run ELTEST suite + parse results
tools\cli serve       # boot the test server and stream logs (blocks)
```

- **Config**: `configs/test-server.json` — runs `Missions/EL_DebugTest.conf`
  (DebugWorld), RCON on UDP 19999 (password `lf-test`), local mod loaded via
  `-addonsDir`/`-addons LifeFramework`.
- **Wrappers**: `scripts/launch-test.ps1` (Windows, `-Diag` for the diag build)
  and `scripts/launch-test.sh` (Linux, `SERVER_EXE` env).
- **Requirements**: Arma Reforger Server (Steam app 1874900). The CLI looks for
  it at the default Steam path or `ENFUSION_SERVER_PATH` in `opencode.json`.
- **Runtime files** (`profile/`, `logs/`, `build/`) are git-ignored.

## Production

*Coming soon — example configs and launch scripts for a public server once the
mod is playable. The test config above is for automation only.*