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
- **Requirements**: Arma Reforger Server (Steam app 1874900). The CLI looks for
  it at the default Steam path or `ENFUSION_SERVER_PATH` in `opencode.json`.
- **Runtime files** (`profile/`, `logs/`, `build/`) are git-ignored.

## Production

`toolscli host start|stop|status|backup` runs the playtest server on
`Missions/LF_PlaytestTown.conf` from a profile outside every worktree. It
continues the latest save and backs saves up. See `../docs/playtest.md`.
The test config above is for automation only.