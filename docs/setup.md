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
> Toolchain paths unless you adapt `opencode.json`.

## 1. Clone the repository

```sh
git clone https://github.com/Life-Framework/Life-Framework.git
cd Life-Framework
git config core.hooksPath .githooks
```

The pre-commit hook runs `tools/validation/validate-repo.ps1` on staged files.
It blocks generated artifacts, duplicate GUIDs, and other repo hygiene issues.

## 2. Configure toolchain paths

Edit [`opencode.json`](../opencode.json) at the repo root. Set paths for your
machine:

```json
"environment": {
  "ENFUSION_WORKBENCH_PATH": "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools",
  "ENFUSION_GAME_PATH": "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger",
  "ENFUSION_PROJECT_PATH": "C:/Users/you/Documents/My Games/ArmaReforgerWorkbench/addons",
  "ENFUSION_SERVER_PATH": "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server"
}
```

`ENFUSION_SERVER_PATH` is optional; the CLI falls back to the default Steam
install location.

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
settings in `opencode.json`.

Full harness overview: [CONTRIBUTING.md § AI-assisted development](../CONTRIBUTING.md#-ai-assisted-development).

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
| Workbench not found | Set `ENFUSION_WORKBENCH_PATH` in `opencode.json`; run `status` |
| Server not found | Install Arma Reforger Server (1874900) or set `ENFUSION_SERVER_PATH` |
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
