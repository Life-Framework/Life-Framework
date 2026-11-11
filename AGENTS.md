# AGENTS.md - Life Framework development contract

The portable contract for any coding agent (Claude Code via `CLAUDE.md`,
opencode, Codex, Cursor). Read it before changing anything. Skills in
`.claude/skills/` add workflows (`life-mode` routes a task to a playbook); when
a skill and this file disagree, this file wins. Setup: `docs/setup.md`. Design
intent: `docs/design.md`. An agent handed a scope, issue, or PR starts with the
`agent-scope` skill.

## Model budget

`docs/agent-budget.md` routes work by cost. Haiku (`lf-scout`) does lookups
and Sonnet (`lf-builder`) does a scoped issue. Opus (`lf-hard`) takes design,
replication and persistence, and anything Sonnet failed twice. The free
DeepSeek model in opencode takes background docs, website work and issue
grooming. A subagent does the work itself and starts no helper agents unless
its brief says to. It reads files in pieces and stops after two environment
failures instead of grinding.

## Repo map

| Path | What lives there |
|---|---|
| `addons/LifeFramework/Scripts/Game/` | Enforce Script: `Core/`, `Feature/<Area>/` (each with a `Feature.md`), `Persistence/`, `UI/`, `Tests/`, ... |
| `addons/LifeFramework/{Prefabs,Configs,UI,Language,Missions}/` | Resources (`.et`, `.conf`, `.layout`, `.st`) |
| `addons/LF_Everon1989/` | The 1989 Everon flagship pack: a skeleton, not loaded yet. What goes there: `docs/architecture/core-vs-pack.md` |
| `addons/LifeFramework/Worlds/DebugWorld/` | Test world. `DebugWorld.ent` is 0 bytes on purpose; content is in `DebugWorld_Layers/*.layer` |
| `tools/` | `cli` (one entry point), `validation/`, `test/`, `merge/`, `actions/`, `street/` |
| `agent-box/` | Player-equivalent checks on a rendering Workbench (`agent-box/README.md`) |
| `docs/` | `features.md` (what each system does), `debug-log-triage.md`, `worktrees.md`, `setup.md`, `persistence.md`, `roadmap/` |
| `website/` | life-framework.org: Vite + React site built from `docs/` and GitHub issues; own `package.json`; deploys from `main` to Cloudflare Pages (`website/README.md`) |

## Golden rules

1. **Never commit generated files**: `resourceDatabase.rdb`, `*.rdb.lock`, the
   addon `log`, `*.gproj.user`, `node_modules/`, `tools/mcp/*/`. The pre-commit
   hook (`.githooks/pre-commit`) enforces this; fix what it flags.
2. **Resource GUIDs are unique and immutable.** Never reuse, shuffle, or
   hand-duplicate a `.meta` GUID. `tools\cli validate` catches duplicates and
   orphan metas.
3. **Reference resources by `{GUID}path`** with the real folder casing.
4. **Binary assets are plain blobs** marked `-text` in `.gitattributes`.
   Text is LF in the repo and in every checkout (`* text=auto eol=lf`; `.cmd`/`.bat`
   stay CRLF). Generators write LF; scripts that patch files need not handle `\r\n`.
5. **All classes are `LF_`; tests are `LF_Test_*`.** The `EL_` to `LF_` rename
   landed 2026-11-09 (#334). The real EveronLife mod (`EL_`) can load on the same
   server, so an `EL_` name can collide. `validate-no-el-prefix.mjs` rejects any
   `EL_` identifier in `addons/` (resource file names such as `EL_JailCell.et`
   keep their paths and are the only exception).
6. **Heavy commands run only in a worktree.** `build`, `test`, `dev`, `serve`,
   `ci` refuse to run in the main checkout, which is the human's world-editor
   copy.
7. **User-facing strings are localization keys** in
   `Language/everonlife_localization.st` (append, never rewrite; then
   `tools\cli regen-localization`). Layouts reference `#Key`.
8. **Update `docs/features.md` in the same change** that alters a feature.
   Append or edit only your rows; the merge driver keeps rows both sides
   append and stops with conflict markers when both edit the same line.
9. **Classify every new resource core or pack with the red line**
   (`docs/architecture/core-vs-pack.md`): the core decides what the game can
   do, the pack decides what this server is. The core never references
   `addons/LF_Everon1989/`; `validate-addon-boundary` enforces it. Until the
   M3 move loads the pack, a pack resource still lands in the core and gets a
   row in `docs/architecture/core-pack-inventory.md`.
10. **Shared registries are per-feature files; never append to a central list.**
    A proof verb is `agent-box/proofs/<verb>.mjs`; a bus event is in
    `Feature/<Area>/LF_<Area>Events.c` (`validate-registries` enforces both).

## Commands

`tools\cli help` and `tools\cli wt help` list everything; these are the daily ones.

| Command | Does |
|---|---|
| `tools\cli status` | Toolchain and MCP state |
| `tools\cli wt new <slug> [--wait]` | Create worktree `Life-Framework-ws-<slug>` on `ws/<slug>` with its own ports |
| `tools\cli validate [--full]` | The repo checks in `tools/validation/`. The default skips the Workbench compile (`validate-scripts.ps1`); `--full`, `ci`, `wt gate`, and `wt ship` run it |
| `tools\cli test --no-build --tier fast\|all\|persistence` | Boot DebugWorld, run the `LF_Test` suite; exit code is pass/fail; a compile error fails it |
| `tools\cli test --no-build --only <LF_Test_Class\|name\|LF_Test_File>[,...]` | Run just those tests (the red-proof loop); prints `SUMMARY tier=only` |
| `tools\cli test` / `e2e` / `pool status` | In-game runs take the first free slot on any pool machine (this PC, `lf-testbox`; `tools/pool.json`) and queue FIFO when all are busy; `--local` / `--remote [name]` pin it; remote runs carry uncommitted state and the rdb (`docs/worktrees.md` "Test pool") |
| `tools\cli dev --tier fast` | validate + test without a build, then dump `[LFDebug:*]` lines |
| `tools\cli wt e2e <slug>` | Two-boot save, restart, and reload proof |
| `tools\cli logs <pattern>` | Grep the newest server `console.log` |
| `tools\cli regen-tests` / `regen-localization` | Regenerate the test registry / language tables |
| `tools\cli wt gate <slug>` / `wt ship <slug> [--refs N] [--body-file F]` | validate --full + build + full suite / gate, push, PR, auto-merge. `--refs N` puts `Refs #N` in the body; `--closes N` is the same and merging never closes an issue |
| `tools\cli promote [--to sha] [--confirm]` | Human step: list PRs merged to `main` since `stable` and write the checklist; `--confirm` fast-forwards `stable` (`docs/worktrees.md` "Release gates"). Agents never run it |
| `tools\cli call <tool> '<json>'\|@file` | Call an MCP tool (`api_search`, `game_read`, ...) |

**Test and session queue.** Just run `test`, `e2e`, or `session up` as before; the pool queues itself (tests have their own lane beside sessions, runs get a time budget from history) and prints one line per outcome, so read it. `TIMEOUT after Ns` means the run hung and was killed; pass `--budget <s>` only for a known-long run. A session idle 15 min while others wait is closed; any harness command keeps it, `session.mjs lease [min]` holds it. A session older than 20 min while another session request queues gets a one-line warning on its next harness or session command and is closed 5 min later (`session up --hold <min>`, at most 45, for a known-long job). Development-track and spike work runs with `--priority low` (`wt new <slug> --priority low` records it). Details: `docs/worktrees.md` "Lanes, priority, leases, budgets".

**Session profiles.** `session up` defaults to `--profile lowspec` (640x360, lowest textures; the pool reserves less RAM for it and trims idle sessions before it refuses one), which is right for any proof that reads logs and harness results. Shots, `shots area`, filming, `journey` and any proof whose images go in a PR use `--profile full` (`docs/worktrees.md` "Session profiles").

**Sessions are shared: prepare every edit before `session up`, do the proof and the shots in one session, and `session down` straight after.** `session up` prints how many session requests are queued behind you; it boots one client unless you pass `--clients 2`.

Launch tools through `tools/lib/quiet-launch.{mjs,ps1}` (hidden `LF-Agents` desktop; node spawns get `windowsHide: true`), never in front of the user; `tools\cli hidden [--close]` lists them, `LF_SHOW_WINDOWS=1` shows them (`docs/setup.md`).

**Nothing waits forever.** Every external call has a bounded runtime:
- `ssh` inherits `ConnectTimeout 8` and `ServerAliveInterval 10` from `~/.ssh/config`, so a dead link fails in about 30 s.
- Wrap any other probe in `timeout <s>`.
- Never hand-write a `for`/`until` + `sleep` wait loop. Use the repo command that blocks with its own budget (`test`, `session up --wait`, `session wait`, `gh pr checks --watch`), or run in the background and end your turn.
- If you stop another agent's process, message that agent at once.

## Worktrees

Long-lived non-main bases (e.g. `m3`): `wt new <slug> --base m3` records the base and `wt ship` follows it; `wt sync-base m3` PRs main into it (`docs/worktrees.md` "Base branches").
Start with `tools\cli wt new <slug>` and work inside it on `ws/<slug>`; never
commit to `main`. `docs/worktrees.md` covers ports, locks, shipping, and pruning.
**Never run `git worktree remove --force`, `rm -rf`, or `Remove-Item -Recurse`
on a worktree or on `server/profile`.** Their `game-addons` folders are
junctions into the Steam installs; on 2026-11-07 `git worktree remove --force`
(Git for Windows 2.47) followed them and emptied the base game. Remove a
worktree with `tools\cli wt prune <slug>`; delete any other tree with
`node tools/safe-rm.mjs <dir>` (`link-safety-selftest` proves both).

- **Scratch goes in `<worktree>/tmp/`** (gitignored, created by `wt new`) with
  names unique to your task, such as `tmp/pr-body-<slug>.md`. Never use a
  shared session scratch dir for files you publish (one agent shipped another's
  PR body that way), and never write into the main checkout;
  `validate-worktrees` warns about untracked files under its `addons/` or `tools/`.
- **Merge `origin/main` only when GitHub reports the PR `CONFLICTING`, or when
  you need code that landed there.** Auto-merge and the PR's `validate` check
  cover the rest. Each extra merge re-runs validate and the suite for nothing.
  `wt ship` merges main only when `git merge-tree` reports a conflict. The
  generated and append-only files (string table, language confs, test
  registry, `features.md`, `debug-log-triage.md`, the enforce-lint baseline)
  have merge drivers in `tools/merge/`, so they do not count as conflicts.
- After your PR is up, `tools\cli prs status`; the maintainer/lead runs `prs heal`.

## Tests

- One file per test: `Scripts/Game/Tests/LF_Test_*.c` extending `LF_Test`, with
  `// tier: LOGIC|WORLD|PERSISTENCE` above the class and a `// red-proof:` line.
- Tiers are setup cost, not subject: `LOGIC` is pure script (`--tier fast`);
  `WORLD` loads a resource or touches the world; `PERSISTENCE` is a save round trip.
- Prove red once: break one assertion, watch it fail, restore it, record the line.
- The registry is generated by `tools\cli regen-tests`; never hand-edit it. Adding a test never touches `LF_TestManager.c`.
- Red-proof loop: `tools\cli test --no-build --only <LF_Test_Class>` (~10 s), then `--tier all` once at the end.
- Persistence tests go through public mutators and accessors only. In-session
  round trips prove nothing about restarts; use `wt e2e` or a real restart.
- Wait with one blocking call: run `test`, `e2e`, `session up` or `session wait` in the foreground with a timeout up to 600000 ms; never `sleep`/`until` loops, Monitor, ScheduleWakeup or status polls (`docs/agent-budget.md`).
- A rule that keeps getting broken becomes a check: `tools\cli validate` runs every script in `tools/validation/`.

## Logging and fail-safe

- Feature state changes and rejections log through `LF_Debug.Info/Warn/Error("<Feature>", msg)`,
  which prints the greppable `[LFDebug:<Feature>]` prefix. A silent no-op is a bug.
- A broken input (missing prefab, null entity, bad GUID, missing file) logs
  `LF_Debug.Error` and degrades that one feature. It never VMEs or aborts the game mode.
- One logging path per feature; no bare `Print("[LF_X] ...")` beside `LF_Debug`.
- Before chasing a console line, check `docs/debug-log-triage.md`; add new lines there when you explain them.

## Hard EnforceScript lessons (paid for by real bugs)

The engine does not forgive these. A sibling Reforger mod (Overthrow) shipped a
bug for each one; they transfer here because the engine is the same.

- **No ternary operator.** `cond ? a : b` is a compile error. Write `if/else`.
- **No null-coalescing operator.** Initialize in the declaration or guard with
  `if (!x)`.
- **Strong refs (`ref`) for Managed classes.** A Managed field not marked `ref`
  is garbage-collected at end of frame. Put `ref` on BOTH the collection and
  its elements: `ref array<ref LF_Foo>`, `ref map<string, ref LF_Foo>`.
- **Store `EntityID`, not `IEntity`, for anything long-lived.** Entities are
  deleted at any time. `FindEntityByID(id)` and null-check every use.
  `EntityID.INVALID` is the "none" state.
- **`EntityID` differs between server and client. Never send it over the
  network.** Use `RplId`; resolve via `Replication.FindItem(rplId)`.
- **`RplProp` replicates simple types only** (int, float, bool, short strings).
  Arrays, maps, and objects go through an RPC or `RplSave`/`RplLoad` (JIP).
- **A changed `RplProp` value does not broadcast itself.** Call
  `Replication.BumpMe()` after changing it. Throttle to significant changes or
  batch; per-frame `BumpMe` floods the network.
- **RPC direction.** `RplRcver.Server` = client→server (name `RpcAsk_*`);
  `RplRcver.Broadcast`/`Owner` = server→client (name `RpcDo_*`). Host check:
  `if (Replication.IsServer())` call the handler directly, else `Rpc(...)`.
- **A client can only RPC on entities it owns.** Inter-player communication
  goes through the server. Validate client requests on the server (money,
  inventory, RPC payloads); never trust the client.
- **`Init()` is not called automatically.** Initialize collections in the
  declaration or call `Init()` yourself from the game mode / owning manager.
- **Null-check singletons and UI.** `GetInstance()` returns null until the game
  mode has the component; `GetUI()` is null on a dedicated server.
- **Persistence.** Never persist `IEntity` or a session-local `EntityID`;
  persist a stable key and re-resolve on load, tolerating "not there". Write a
  `version` first; binary save contexts are positional (write order = read
  order). Load must be idempotent (runs on fresh load AND when re-applying to a
  live session). No console guards; consoles are handled internally.
  **In-session round trips prove nothing about restarts**; the restart path is
  ground truth.

## Enfusion data rules

Detail lives in `.claude/skills/enfusion-resource-authoring` and `enfusion-blast-radius`.

- Prefer prefab inheritance over duplication; duplicate vanilla prefabs with `game_duplicate`.
- Override an inherited component by its GUID; a second component of the same class shadows it.
- A `MeshObject` needs a real `.xob` or the entity is invisible.
- A textured mod item bakes its real `.emat` into the `.xob`; a prefab material override over a placeholder bake renders white.
- Textures are verified visually only; the dedicated server has no renderer.
- Place world props with `tools\cli place on|against|ground|arrange` (`docs/placement-tool.md`), never with typed coordinates: it reads real bounds, probes the surface or wall, checks, writes the layer entry and audits it.
- World props must pass `tools\cli audit placement --strict`; `wt gate` runs it when a `.layer`, a `Prefabs` `.et` or spawn code changed. A placement meant to look odd goes in `agent-box/place-audit-allow.json` with a reason (`docs/placement-audit.md`).
- A new world interactable runs `node tools/actions/place-action-contexts.mjs`.
- Moving or renaming a resource breaks every `{GUID}path` reference, saved worlds included.
- Layouts: unique widget GUIDs per file, inherited-component GUIDs equal the base, a sibling `.layout.meta`.
- Input: bind keyboard plus `gamepad0:`; never `W/A/S/D`, `a`, the d-pad, `T`, or `shoulder_left`.
- A persisted payload is appended to `RegisterPersistencePayloads` with a version bump (`validate-persistence-version`).

## Research before code

Never guess an Enfusion API. Look it up from any terminal:
`tools\cli call api_search|game_read|wb_read_props|asset_search '<json>'`
(use `@args.json` from PowerShell). `wb_read_props` gives resolved inherited
prefab values; `asset_search` gives `{GUID}path`. Reading sibling features and
existing code to learn the shape is encouraged. See `enfusion-api-research`.

## What "done" means (issue states)

Merging to `main` is not done. A human decides after play-testing `main`.
Issue labels: `agent:working` (you are building it), `in-dev` (merged, not
verified), `needs-playtest` (merged and the PR lists manual steps), `verified`
(a human signed it off; only this closes the issue), `regression` (failed
verification). The `issue-state` workflow moves the issues a merged PR refs
(`Refs #N`, also `Closes/Fixes #N`, which it reverses back to open); `tools\cli
promote --confirm` sets `verified` and closes. In your report and PR say
"merged, in-dev", never "done" or "fixed". Put manual steps under a
`## Manual testing` heading: that is what makes the issue `needs-playtest` and
what the human's promotion checklist shows. `docs/worktrees.md` "Release gates".

## Verification ladder

1. `tools\cli validate`: cheap, run it constantly. `validate --full` (adds the Workbench compile for every platform) runs in `wt gate`/`wt ship`.
2. The `LF_Test` suite in your worktree: `--only` or `--tier fast` while iterating, `--tier all` once before calling work done.
3. Drive the real path: DebugWorld play mode or `node agent-box/harness.mjs flow <name>`.
4. Read the specific log line (`SCRIPT (E)`, `VME:`, `Unable to find`), never an error count.

Proof classes that survive the ladder: restart (a real save, restart, load),
dedicated server (a green host run proves nothing), late joiner (JIP state that
was never written). There is no debugger or hot reload; debug with logs.

**Manual steps ship in the PR.** Multiplayer/JIP, UI, and the true restart path
stay manual; the PR says exactly what to do: host or join, actions, what to
observe, whether a restart is needed.

## CI

GitHub Actions `validate` runs `tools\cli validate` on every PR and is a
required check on `main`. CI sets `LF_VALIDATE_SKIP=validate-scripts.ps1,validate-action-contexts.mjs,validate-action-reach.mjs`
because the runner has no Reforger install or MCP build. Script compilation
and the in-game suite stay local (`tools\cli test --no-build` in a worktree).
