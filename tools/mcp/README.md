# MCP Servers

AI-assistant tooling for Enfusion / Arma Reforger modding, installed as local
clones and wired into opencode via `opencode.json`. The clones themselves are
git-ignored; **manage everything through the CLI**, never by editing them by
hand.

```
tools\cli status
tools\cli mcp install              # clone + npm ci + build (idempotent)
tools\cli mcp update               # git pull --ff-only + npm ci + build
tools\cli mcp verify               # boot each server briefly to prove it starts
tools\cli mcp enable <name>        # enable in opencode.json (restart opencode)
tools\cli mcp disable <name>
```

After `enable`/`disable` or a fresh install, **quit and restart opencode** for
the `opencode.json` changes to take effect.

## Servers

| name | repo | dir | state | purpose |
| ---- | ---- | --- | ----- | ------- |
| `enfusion-mcp` | [steffenbk/enfusion-mcp-BK](https://github.com/steffenbk/enfusion-mcp-BK) | `enfusion-mcp-bk` | enabled | Primary. API search (8.7k classes), script/prefab/layout generation, `mod_validate`, `mod_build`, live Workbench `wb_*` tools |
| `enfusion-workbench` | [Goldwep/enfusion-workbench-mcp](https://github.com/Goldwep/enfusion-workbench-mcp) | `enfusion-workbench-mcp` | disabled | Back-pocket, 112 tools: project-wide GUID/resource index, reverse-query, refactoring, scenario tooling |

`enfusion-mcp` is the ecosystem hub (actively maintained, most forks build on
it). `enfusion-workbench` is kept disabled by default to avoid session context
bloat from 112 tool schemas; `tools\cli mcp enable enfusion-workbench` turns it
on when you want the extra surface.

Both run `node dist/index.js`; both read the same env vars
(`ENFUSION_WORKBENCH_PATH`, `ENFUSION_GAME_PATH`, `ENFUSION_PROJECT_PATH`) from
their `opencode.json` entries.

## Pinned commits

Recorded here so a fresh checkout can reproduce the exact builds used.

| server | commit |
| ------ | ------ |
| enfusion-mcp-bk | `0acfa88` |
| enfusion-workbench-mcp | `1329715` |

`tools\cli mcp update` moves these forward; re-run `tools\cli status` and update
this table when you bump them.

## Reinstalling on a fresh clone

```sh
tools\cli mcp install
tools\cli mcp verify
tools\cli status
```