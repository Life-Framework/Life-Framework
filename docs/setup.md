# Development setup

Everything you need to clone Life Framework, open the mod, run the checks, and
send a pull request. For how a change flows see
[CONTRIBUTING.md](../CONTRIBUTING.md); for engineering rules see
[AGENTS.md](../AGENTS.md). Agent, pool-machine and maintainer tooling is in
[dev-tooling.md](dev-tooling.md); you do not need it to start.

## The fastest path

1. Install what is under "What you need" below.
2. Clone, and point git at the repo's hooks:

   ```sh
   git clone https://github.com/Life-Framework/Life-Framework.git
   cd Life-Framework
   git config core.hooksPath .githooks
   ```

3. Check that your tools are found: `node tools/cli.mjs status`. It prints what
   it found and what is missing.
4. Run the cheap repo checks: `node tools/cli.mjs validate`. Docs, tooling and
   website changes need nothing more than this and Node.
5. Start your change in its own worktree (see [section 3](#3-work-in-a-worktree)):
   `node tools/cli.mjs wt new <short-name>`.
6. For game changes, open `addons/LifeFramework/LifeFramework.gproj` in
   Workbench (see [section 4](#4-open-the-mod-in-workbench)).

On Windows `tools\cli` is the same as `node tools/cli.mjs`; this page uses the
`node` form because it works everywhere.

## What you need

| Requirement | For | Notes |
| --- | --- | --- |
| **Git** | Everything | The pre-commit hook in `.githooks/` runs the repo checks on staged files |
| **Node.js 20+** | The dev CLI, the website | Ships with npm; the CLI itself has no dependencies to install |
| **PowerShell 7+ (`pwsh`)** | `validate` | Some checks are PowerShell scripts. Windows or cross-platform `pwsh` |
| **Arma Reforger** (Steam) | Playing, Workbench | The base game |
| **Arma Reforger Tools** (Steam) | Opening and building the mod | Includes Workbench |
| **Arma Reforger Server** (Steam app **1874900**) | `test` and `serve` | Only for the in-game test suite |

Optional, and not needed for a first change: the GitHub CLI (`gh auth login`,
needed by `wt ship`), the MCP servers that give a coding agent API search
([section 6](#6-ai-tooling-optional)), and pool or remote test machines (they
are in [dev-tooling.md](dev-tooling.md)).

> **Platform note:** Workbench and the validation scripts are Windows-first
> today. On Linux or macOS you can edit docs, scripts, EnforceScript and the
> website, and `validate` runs under `pwsh`. `build` and `test` expect Windows
> toolchain paths unless you set the `ENFUSION_*` variables from section 2.

## 1. Clone the repository

The commands are in "The fastest path". The pre-commit hook runs
`tools/validation/validate-repo.ps1` on staged files. It blocks generated
artifacts, duplicate GUIDs, and other repo hygiene issues; fix what it flags
rather than skipping it.

## 2. Toolchain paths

The CLI looks for the game, the tools and the server in the standard Steam
install folders:

- `...\Steam\steamapps\common\Arma Reforger Tools`
- `...\Steam\steamapps\common\Arma Reforger`
- `...\Steam\steamapps\common\Arma Reforger Server` (only for `test` and `serve`)

If you installed somewhere else, set any of these OS environment variables (they
win over everything):

```sh
ENFUSION_WORKBENCH_PATH=C:/path/to/Arma Reforger Tools
ENFUSION_GAME_PATH=C:/path/to/Arma Reforger
ENFUSION_SERVER_PATH=C:/path/to/Arma Reforger Server
ENFUSION_PROJECT_PATH=C:/path/to/your/addons/folder   # only for the MCP authoring tools
```

Resolution order for every path: **OS environment variable, then
`lf.local.json`, then `opencode.json`, then the Steam defaults.** Machine paths
go in your environment or in `lf.local.json`, never in committed files.

`lf.local.json` is an optional, git-ignored file in your clone's top folder. It
takes the same `ENFUSION_*_PATH` keys, and `LF_GAME_ADDONS` (the folder with the
base game's `core/` and `data/` projects, if it is not the server install's
`addons` folder). The rest of its keys are for test machines
([dev-tooling.md](dev-tooling.md)).

Check that everything resolves:

```sh
node tools/cli.mjs status
```

## 3. Work in a worktree

Heavy commands (`build`, `test`, `dev`, `serve`, `ci`) refuse to run in the
checkout you cloned, because that folder is kept clean as the place you open in
the world editor. Do each change in its own worktree, a second folder with its
own branch:

```sh
node tools/cli.mjs wt new <short-name>    # creates ../Life-Framework-ws-<short-name> on branch ws/<short-name>
cd ../Life-Framework-ws-<short-name>
node tools/cli.mjs wt list                # your worktrees, ports, merged state
```

Run every command below from inside the worktree. `validate` and `status` work in
either place. When the change has merged, `node tools/cli.mjs wt prune
<short-name>` removes the worktree. Never delete a worktree folder by hand; the
reasons are in [AGENTS.md](../AGENTS.md) and [worktrees.md](worktrees.md).

## 4. Open the mod in Workbench

The core project is `addons/LifeFramework/LifeFramework.gproj`. The 1989 Everon
town is a second project, `addons/LF_Everon1989/LF_Everon1989.gproj`, that
depends on the core (`addons/README.md`, `docs/architecture/core-vs-pack.md`).

| Scenario | Project | Use |
| --- | --- | --- |
| `Missions/EL_DebugTest.conf` | core, DebugWorld | The `LF_Test*` suite and the per-feature test layers |
| `Missions/EveronLifeGameMode.conf` | pack, MainWorld | Normal play, the 1989 town |
| `Missions/LF_PlaytestTown.conf` | pack | The small playtest town |

Typical loop:

1. Open the `.gproj` in Workbench.
2. Build (PC).
3. Play the scenario from the picker.

Playtesters start at [Playtest](playtest.md). Workbench play of DebugWorld is for
developers; it is not the automated suite.

**Optional: link the mod into your Reforger profile** instead of copying it:

```powershell
New-Item -ItemType SymbolicLink `
  -Path "$env:USERPROFILE\Documents\My Games\ArmaReforger\profile\addons\LifeFramework" `
  -Target "C:\path\to\your\Life-Framework\addons\LifeFramework"
```

## 5. Dev CLI (build, validate, test)

Run these from your worktree:

```sh
node tools/cli.mjs status              # toolchain state
node tools/cli.mjs validate            # repo checks: GUIDs, metas, artifacts, links
node tools/cli.mjs validate --full     # adds the Workbench compile
node tools/cli.mjs build               # headless Workbench build into server/build/
node tools/cli.mjs test --tier fast    # LOGIC-tier in-game tests only
node tools/cli.mjs test                # the full LF_Test* suite on DebugWorld
node tools/cli.mjs ci                  # validate + build + test
```

`test` boots a dedicated server on DebugWorld, runs the `LF_Test*` suite, reads
the `[LFTEST] SUMMARY` line, and exits nonzero on failure. While you iterate,
run only your test: `node tools/cli.mjs test --no-build --only LF_Test_<Name>`.
Details: [server/README.md](../server/README.md) and [AGENTS.md](../AGENTS.md).

- `build` needs Arma Reforger Tools (Workbench) and the game.
- `test` and `serve` need Arma Reforger Server (Steam app 1874900).
- Server config (scenario and RCON): `server/configs/test-server.json`.

The long list of commands is in `node tools/cli.mjs help`. A change that only
touches docs, tools or the website needs `validate`, not the game.

## 6. AI tooling (optional)

Coding agents are welcome. Claude Code reads [`CLAUDE.md`](../CLAUDE.md),
which points at [`AGENTS.md`](../AGENTS.md); opencode, Codex and Cursor read
`AGENTS.md` directly. Skills and playbooks live in `.claude/skills/` and
[`.opencode/`](../.opencode/README.md).

For Enfusion API search, base-game source and asset lookups, the repo has two
MCP servers configured in [`opencode.json`](../opencode.json) (their clones live
git-ignored under `tools/mcp/`):

```sh
node tools/cli.mjs mcp install [name]   # clone if missing, npm ci, build
node tools/cli.mjs mcp verify [name]    # boot briefly to prove it starts
node tools/cli.mjs call api_search '{"query":"SCR_SpawnLogic","format":"tree"}'
```

`[name]` is `enfusion-mcp` or `enfusion-workbench`; leave it out for both. With
no MCP set up you can still read the real declarations from the installed game
with `node tools/cli.mjs api class <Name>`. More commands (`mcp update`,
`enable`, `disable`, batch calls, the Node ABI note) are in
[dev-tooling.md](dev-tooling.md). The full agent workflow is in
[CONTRIBUTING.md](../CONTRIBUTING.md#working-with-coding-agents).

## 7. Before you open a PR

1. Run `validate`, and the `test` tier that fits (`--tier fast` for script
   logic).
2. Fill in [`.github/PULL_REQUEST_TEMPLATE.md`](../.github/PULL_REQUEST_TEMPLATE.md).
3. Write the manual steps for UI, multiplayer, join-in-progress, or save and
   restart changes.
4. Open a PR against `main`.

If you have push access to the repo, `node tools/cli.mjs wt ship <short-name>`
runs the gate, pushes the branch, opens the PR and queues auto-merge. It needs
the GitHub CLI signed in once (`gh auth login`). If you do not, fork the repo,
push your branch to your fork, and open the PR from there (`gh pr create` or the
GitHub page); the same `validate` check runs on it.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| `validate` fails with `spawn powershell ENOENT` | Install PowerShell 7 (`pwsh`) or run validation on Windows |
| Workbench not found | Set `ENFUSION_WORKBENCH_PATH` (environment or `lf.local.json`); run `status` |
| Workbench: *Arma Reforger base game project not found* / game: *Unable to initialize Enfusion* | The game install is missing `addons\data\ArmaReforger.gproj` and `addons\core\*` (a partial Steam update). Run `node tools/cli.mjs status` for the exact missing file, then Steam, **Arma Reforger (the game, app 1874880)**, Properties, Installed Files, **Verify integrity of game files**. Verifying only *Arma Reforger Tools* does not restore these. Relaunch the game once afterward |
| Workbench: *Missing Addon* for game GUID `58D0FB3206B6F859` | `powershell -ExecutionPolicy Bypass -File tools\utilities\repair-base-addons.ps1`. It restores only a missing loose `ArmaReforger.gproj` from the installed Server package; it never overwrites a manifest with the wrong GUID |
| Server not found | Install Arma Reforger Server (1874900) or set `ENFUSION_SERVER_PATH` |
| `build`, `test` or `dev` refuses to run | You are in the folder you cloned. Create a worktree (section 3) and run it there |
| Pre-commit blocks `.rdb`, `log` or `.gproj.user` | Unstage generated Workbench artifacts; never commit them |
| Duplicate GUID error | Every resource needs a unique 16-hex GUID in its `.meta`; never copy one |
| MCP `installed=false` | Run `node tools/cli.mjs mcp install` |

Logs: `server/logs/` (test server), and the Workbench console under
`Documents/My Games/ArmaReforger/logs/<date>/`.

## Workshop items

The two Workshop items are the core `LifeFramework`
([`4550A2B656E1B569`](https://reforger.armaplatform.com/workshop/4550A2B656E1B569))
and the 1989 Everon pack `LF_Everon1989`
([`B1E84D5687E11724`](https://reforger.armaplatform.com/workshop/B1E84D5687E11724)).
A server owner lists both ids in `game.mods` (`server/example/server.json`) and
the server downloads them on first boot. Running your own server is covered by
[owner/install.md](owner/install.md).

## Next steps

- [Roadmap](roadmap/README.md): the live plan
- [Features](features.md): implemented behavior contracts
- [Testing plan](roadmap/testing/README.md): how we prove features
- [Feature design files](../addons/LifeFramework/Scripts/Game/Feature/README.md): per-feature `Feature.md` index
- [Developer tooling](dev-tooling.md): pool machines, hidden windows, MCP details, agent box
