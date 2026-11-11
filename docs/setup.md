# Development setup

Everything you need to clone Life Framework, build the mod, run checks, and
(optionally) wire up AI tooling. For contribution norms see
[CONTRIBUTING.md](../CONTRIBUTING.md); for engineering rules see
[AGENTS.md](../AGENTS.md).

## Prerequisites

| Requirement | Purpose | Notes |
| --- | --- | --- |
| **Arma Reforger** | Play / host | Base game (Steam) |
| **Arma Reforger Tools** | Build in Workbench | Includes Workbench IDE |
| **Arma Reforger Server** | Automated tests | Steam app **1874900**; required for `tools/cli test` |
| **Node.js 18+** | Dev CLI & MCP | Ships with npm; no extra deps for the CLI itself |
| **Git** | Clone & hooks | Pre-commit validator wired via `.githooks/` |
| **PowerShell 7+** | Validation scripts | Required for `validate` checks today (Windows or cross-platform `pwsh`) |

> **Platform note:** Workbench build and the validation scripts are Windows-first
> today. Linux/macOS contributors can edit docs, scripts, and EnforceScript, and
> can point `ENFUSION_SERVER_PATH` at a Linux server build once paths
> are configured — but `node tools/cli.mjs build` and `validate` expect Windows
> Toolchain paths unless you set the `ENFUSION_*` env vars above.

## 1. Clone the repository

```sh
git clone https://github.com/Life-Framework/Life-Framework.git
cd Life-Framework
git config core.hooksPath .githooks
```

The pre-commit hook runs `tools/validation/validate-repo.ps1` on staged files.
It blocks generated artifacts, duplicate GUIDs, and other repo hygiene issues.

## 2. Toolchain paths

The committed [`opencode.json`](../opencode.json) is portable — MCP servers are
launched by relative path from the checkout, and the CLI, test harness, and MCP
servers all fall back to the standard Steam install locations:

- `...\Steam\steamapps\common\Arma Reforger Tools`
- `...\Steam\steamapps\common\Arma Reforger`
- `...\Steam\steamapps\common\Arma Reforger Server` (only needed for `test`/`serve`)

Resolution order for every tool path: **OS environment variable →
`opencode.json` → standard Steam install locations.** Machine-specific paths
go in your environment or your global opencode config, never in the repo.

If you installed outside Steam or want custom locations, set any of these OS
environment variables (they win over everything):

```sh
ENFUSION_WORKBENCH_PATH=C:/path/to/Arma Reforger Tools
ENFUSION_GAME_PATH=C:/path/to/Arma Reforger
ENFUSION_SERVER_PATH=C:/path/to/Arma Reforger Server
ENFUSION_PROJECT_PATH=C:/path/to/your/addons/folder   # MCP authoring tools
```

For opencode users, machine-specific env can also live in your **global**
config at `~/.config/opencode/opencode.json` (deep-merged over the repo's) —
that keeps machine paths out of the repo entirely:

```json
{
  "mcp": {
    "enfusion-mcp": {
      "type": "local",
      "command": ["node", "dist/index.js"],
      "cwd": "tools/mcp/enfusion-mcp-bk",
      "enabled": true,
      "environment": {
        "ENFUSION_WORKBENCH_PATH": "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools",
        "ENFUSION_GAME_PATH": "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger",
        "ENFUSION_PROJECT_PATH": "C:/Users/you/Documents/My Games/ArmaReforgerWorkbench/addons"
      }
    }
  }
}
```

Check that everything resolves:

```sh
node tools/cli.mjs status
```

### Machine-local settings (`lf.local.json`)

A gitignored `lf.local.json` in the main checkout holds one machine's
settings for the CLI and every worktree of that checkout
(`tools/lib/local-config.mjs`). Precedence: OS env > `lf.local.json` >
`opencode.json` > the Steam defaults. Without the file nothing changes.

| Key | Meaning |
| --- | --- |
| `ENFUSION_*_PATH` | Same as the env vars above |
| `LF_GAME_ADDONS` | Folder with the base-game `core/` and `data/` projects the test server loads through the `game-addons` junction. Default: the server install's `addons`, else the game client's |
| `LF_TESTBOX` | ssh host of a second test machine; added to the pool when `tools/pool.json` lacks it (`docs/worktrees.md` "Test pool") |
| `LF_TESTBOX_ROOT` | Main checkout on that machine (default `C:/LF/Life-Framework`) |
| `LF_TESTBOX_AUTO` | `0` keeps `test` on this machine by default (`--remote` and `--machine` still work) |
| `pool` | Object: `{ "machines": [ { "name": "lf-testbox", "slots": 2 } ] }` merges by name over `tools/pool.json`; `"disabled": true` drops a machine, a new name adds one (`docs/worktrees.md` "Test pool") |

## 3. Open the mod in Workbench

The mod project is:

```
addons/LifeFramework/LifeFramework.gproj
```

**Playable scenarios**

| Scenario | World | Use |
| --- | --- | --- |
| `Missions/EveronLifeGameMode.conf` | MainWorld | Normal play / feature demo |
| `Missions/EL_DebugTest.conf` | DebugWorld | Automated `LF_Test*` suite + per-feature layers |

**Typical loop**

1. Open `LifeFramework.gproj` in Workbench.
2. Build (PC).
3. Play DebugWorld or MainWorld from the scenario picker.

Playtesters start at [Playtest](playtest.md). Workbench play of DebugWorld is for developers; it is not the automated suite.

**Optional — symlink for faster iteration**

Link the mod into your Reforger profile instead of copying:

```powershell
New-Item -ItemType SymbolicLink `
  -Path "$env:USERPROFILE\Documents\My Games\ArmaReforger\profile\addons\LifeFramework" `
  -Target "C:\path\to\Life-Framework\addons\LifeFramework"
```

See [addons/README.md](../addons/README.md) for layout details.

## 4. Dev CLI (build, validate, test)

Cross-platform entry point (Windows shim: `tools\cli`):

```sh
node tools/cli.mjs status              # toolchain + MCP state
node tools/cli.mjs validate            # repo hygiene (GUIDs, metas, artifacts)
node tools/cli.mjs build               # headless Workbench build → server/build/
node tools/cli.mjs test --tier fast    # LOGIC-tier in-game tests only
node tools/cli.mjs test                # full LF_Test* suite on DebugWorld
node tools/cli.mjs ci                  # validate + build + test
```

The test pipeline boots a dedicated server on DebugWorld, runs the `LF_Test*`
suite, parses `[LFTEST] SUMMARY`, and exits nonzero on failure. Details:
[server/README.md](../server/README.md) and [AGENTS.md](../AGENTS.md).

- `build` needs Arma Reforger Tools (Workbench) and the game installed.
- `test` and `serve` need Arma Reforger Server (Steam app 1874900).
- Server config (scenario + RCON): `server/configs/test-server.json`.

### Tool windows stay off your screen

Agents run Workbench and the server many times an hour. Those runs start on a
private Win32 desktop named `LF-Agents` (`tools/lib/quiet-launch.ps1`), so they
never draw a window on your monitor and never take focus:

| Run | Where it goes |
| --- | --- |
| `validate --full` (Workbench `-validate` in `validate-scripts.ps1`), `cli build` | `LF-Agents` desktop; exit code and console.log as before |
| `cli test` / `wt e2e` dedicated server | `LF-Agents`, no console window |
| `agent-box/session.mjs up` game clients | `LF-Agents`; the server runs detached with no console |
| `cli call wb_*`, `agent-box/harness.mjs play` | When no Workbench serves NET API port 5775, `tools/lib/quiet-workbench.mjs` starts one on `LF-Agents` first, so the MCP never launches a visible one |
| every node-spawned console tool | `windowsHide: true` |

Workbench play mode and a game client both render on `LF-Agents`: harness
screenshots (`LF_ScreenshotCapture`) come back as the real 3D world. Screen
recording (`tools/film`) cannot see that desktop; film a game client you start
yourself on a visible monitor. `film.mjs` no longer brings that window to the
front; on a second monitor it lifts it without activating it.

- `tools\cli hidden` lists processes with a window on `LF-Agents`;
  `tools\cli hidden --close` ends them (a hidden Workbench stays up between agent
  calls, as the visible one used to).
- `LF_SHOW_WINDOWS=1` turns all of this off for debugging: every run starts on
  your desktop again.
- `wt open --workbench` and `wt open` are for you, so they still open visibly.

### Video post-production (optional, `film-make`)

`tools\cli film-make video/<name>.json` joins footage into a trailer with cards, crossfades, captions, voice-over and a
music bed (`video/README.md`, `docs/film-pipeline.md`). It needs the portable ffmpeg (`node tools/film/film.mjs fetch`) and,
for voice-over, Piper (MIT, offline) in a tools-local folder: `tools\cli film-make --setup` downloads the Windows Piper
release and the `en_US-ljspeech-high` voice (public domain) into `tools/film/.tts/` (git-ignored). Nothing is installed globally and no
Python is needed. Licences of the voice and music: `docs/credits.md`.

## 5. AI tooling (optional)

We encourage AI-assisted contributions. MCP servers are configured in
[`opencode.json`](../opencode.json); their clones live git-ignored under
`tools/mcp/`. `enfusion-mcp` (API search, vanilla source, assets, prefabs) is
enabled; `enfusion-workbench` (live Workbench bridge, `wb_*` tools) is also
enabled in the committed `opencode.json` and only works while Workbench is
open. Manage them with:

```sh
node tools/cli.mjs mcp install [name]   # clone if missing, npm ci, build
node tools/cli.mjs mcp update [name]    # git pull --ff-only, npm ci, build
node tools/cli.mjs mcp verify [name]    # boot briefly to prove it starts
node tools/cli.mjs mcp enable <name>    # enabled=true in opencode.json
node tools/cli.mjs mcp disable <name>   # enabled=false in opencode.json
```

`[name]` is `enfusion-mcp` or `enfusion-workbench`; omitted means all.

`enfusion-workbench` carries a native addon (`better-sqlite3`) compiled for one
Node ABI. After a Node upgrade the server dies at startup with "compiled
against a different Node.js version". `toolscli status` names the addon and
exits 1; `toolscli mcp install` (or `update`) runs `npm rebuild` on it and
checks it loads. A new worktree has no `tools/mcp/` clones of its own; the CLI
and `toolscli call` use the main checkout's clones.

Call any server tool from a plain terminal, no opencode session or restart
needed (see `tools/mcp-call.mjs`):

```sh
node tools/cli.mjs call list
node tools/cli.mjs call api_search '{"query":"SCR_SpawnLogic","format":"tree"}'
node tools/cli.mjs call game_read @tmp/args.json     # file args: PowerShell mangles inline JSON
node tools/cli.mjs call --batch @tmp/wb-session.json # several calls in ONE server process
```

The Workbench bridge caches the editor mode per process, so a fresh process
starts in an "unknown" mode and mode-gated tools refuse. Run `wb_state` and
`wb_play` in one `--batch` session.

For [opencode](https://opencode.ai) users: skills and playbooks live in
[`.opencode/`](../.opencode/README.md). Restart opencode after changing MCP
settings.

Full agent workflow: [CONTRIBUTING.md, Working with coding agents](../CONTRIBUTING.md#working-with-coding-agents).

### Parallel agents (worktrees)

Heavy commands (`build`/`test`/`dev`/`serve`/`ci`) refuse to run in the main
checkout — it is reserved for the world editor. Agents work in per-feature
worktrees:

```sh
node tools/cli.mjs wt new <feature>     # create Life-Framework-ws-<feature> @ ws/<feature>
node tools/cli.mjs wt list              # worktrees, ports, merged state
node tools/cli.mjs wt ship <feature>    # gate, PR, auto-merge into main
```

`wt ship` needs the GitHub CLI authenticated once (`gh auth login`), or a
`GITHUB_TOKEN` env var. Full detail: [worktrees.md](worktrees.md).

## 6. Agent box (GPU or remote machine)

Clone, then install the Workbench MCP servers and the player harness:

```powershell
powershell -ExecutionPolicy Bypass -File agent-box\setup.ps1
node agent-box/harness.mjs selftest
```

That writes `.cursor/mcp.json` for this checkout (gitignored; it contains
absolute paths). Restart the agent session so the servers load. The three
pillars and the player-key hooks are in [agent-box/README.md](../agent-box/README.md).

On a T3 server, register the clone as a project and use the scripts in
`agent-box/t3-project.json`. Threads should use worktrees so the main checkout
stays the world-editor copy.

## 7. Before you open a PR

1. Run `validate` and the appropriate `test` tier.
2. Fill in [`.github/PULL_REQUEST_TEMPLATE.md`](../.github/PULL_REQUEST_TEMPLATE.md).
3. Document manual steps for UI, multiplayer, JIP, or save/restart if your
   change touches those paths.
4. Open a PR against `main`.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| `validate` fails with `spawn powershell ENOENT` | Install PowerShell 7 (`pwsh`) or run validation on Windows |
| Workbench not found | Set `ENFUSION_WORKBENCH_PATH` (OS env or global opencode config); run `status` |
| Workbench: *Arma Reforger base game project not found* / game: *Unable to initialize Enfusion* | The game install is missing `addons\data\ArmaReforger.gproj` + `addons\core\*` (partial Steam update). Run `tools\cli status` for the exact missing file, then Steam → **Arma Reforger (the GAME, app 1874880)** → Properties → Installed Files → **Verify integrity of game files**. Verifying only *Arma Reforger Tools* does not restore these. Relaunch the game once afterward. |
| Workbench: *Missing Addon* for game GUID `58D0FB3206B6F859` | `powershell -ExecutionPolicy Bypass -File tools\utilities\repair-base-addons.ps1`. It restores only a missing loose `ArmaReforger.gproj` from the installed Server package; it never overwrites a manifest with the wrong GUID. |
| Server not found | Install Arma Reforger Server (1874900) or set `ENFUSION_SERVER_PATH` |
| `cli build/test/dev` refuses in the main checkout | Create a worktree: `cli wt new <feature>`, then work there |
| Pre-commit blocks `.rdb` / `log` / `.gproj.user` | Unstage generated Workbench artifacts; never commit them |
| Duplicate GUID error | Every resource needs a unique 16-hex GUID in its `.meta` |
| MCP `installed=false` | Run `node tools/cli.mjs mcp install` |

Logs: `server/logs/` (test server), Workbench console under
`Documents/My Games/ArmaReforger/logs/<date>/`.

## Next steps

- [Roadmap](roadmap/README.md) — the live plan
- [Features](features.md) — implemented behavior contracts
- [Testing plan](roadmap/testing/README.md) — how we prove features
- [Feature design files](../addons/LifeFramework/Scripts/Game/Feature/README.md) — per-feature `Feature.md` index
