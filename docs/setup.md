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
> can run the headless test server via `server/scripts/launch-test.sh` once paths
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

## 3. Open the mod in Workbench

The mod project is:

```
addons/LifeFramework/LifeFramework.gproj
```

**Playable scenarios**

| Scenario | World | Use |
| --- | --- | --- |
| `Missions/EveronLifeGameMode.conf` | MainWorld | Normal play / feature demo |
| `Missions/EL_DebugTest.conf` | DebugWorld | Automated `EL_Test*` suite + per-feature layers |

**Typical loop**

1. Open `LifeFramework.gproj` in Workbench.
2. Build (PC).
3. Play DebugWorld or MainWorld from the scenario picker.

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
node tools/cli.mjs test                # full EL_Test* suite on DebugWorld
node tools/cli.mjs ci                  # validate + build + test
```

The test pipeline boots a dedicated server on DebugWorld, runs the `EL_Test*`
suite, parses `[ELTEST] SUMMARY`, and exits nonzero on failure. Details:
[server/README.md](../server/README.md) and [AGENTS.md](../AGENTS.md).

## 5. AI tooling (optional)

We encourage AI-assisted contributions. Install MCP servers once:

```sh
node tools/cli.mjs mcp install
node tools/cli.mjs mcp verify
```

Research Enfusion APIs from any terminal (no Workbench GUI):

```sh
node tools/cli.mjs call list
node tools/cli.mjs call api_search '{"query":"SCR_SpawnLogic","format":"tree"}'
```

For [opencode](https://opencode.ai) users: skills and playbooks live in
[`.opencode/`](../.opencode/README.md). Restart opencode after changing MCP
settings.

Full harness overview: [CONTRIBUTING.md § AI-assisted development](../CONTRIBUTING.md#-ai-assisted-development).

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

## 6. Before you open a PR

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
| Server not found | Install Arma Reforger Server (1874900) or set `ENFUSION_SERVER_PATH` |
| `cli build/test/dev` refuses in the main checkout | Create a worktree: `cli wt new <feature>`, then work there |
| Pre-commit blocks `.rdb` / `log` / `.gproj.user` | Unstage generated Workbench artifacts; never commit them |
| Duplicate GUID error | Every resource needs a unique 16-hex GUID in its `.meta` |
| MCP `installed=false` | Run `node tools/cli.mjs mcp install` |

Logs: `server/logs/` (test server), Workbench console under
`Documents/My Games/ArmaReforger/logs/<date>/`.

## Next steps

- [Roadmap](roadmap.md) — what to build next
- [Features](features.md) — implemented behavior contracts
- [Test plan](test-plan.md) — how we prove features
- [Debug World scripts guide](../addons/LifeFramework/GUIDE_DEBUG_WORLD_SCRIPTS.md) — attaching test prefabs
