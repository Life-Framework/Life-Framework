# AGENTS.md - Life Framework development contract

The portable contract for any coding agent (Claude Code via `CLAUDE.md`, opencode, Codex, Cursor). Read it before
changing anything. Skills in `.claude/skills/` add workflows (`life-mode` routes a task to a playbook); when a skill
and this file disagree, this file wins. Setup: `docs/setup.md`. Design intent: `docs/design.md`. An agent handed a
scope, issue, or PR starts with the `agent-scope` skill. This file keeps the rules; the doc it points to owns the
detail.

## Model budget

`docs/agent-budget.md` routes work by cost: Haiku (`lf-scout`) for lookups, Sonnet (`lf-builder`) for a scoped issue,
Opus (`lf-hard`) for design, replication, persistence and anything Sonnet failed twice, the free DeepSeek model in
opencode for background docs, website work and issue grooming. A subagent does the work itself and starts no helper
agents unless its brief says to (`lf-scout` for a search is the exception). It reads files in pieces and stops after
two environment failures instead of grinding. Each agent type has a tool-call budget; past it the agent writes
`tmp/handoff-<slug>.md` and ends with a `RESUME` line. A town-wide job is one agent per area. `tools\cli usage` shows
where the tokens went; `usage --plan` reads the real 5 h and weekly use.

**Starter packs.** `tools\cli brief building|slice|map|town|grid` prints one page of exact commands and known traps
for that kind of work (`docs/starter-packs/`); read it instead of the docs it points to.

## Repo map

| Path | What lives there |
|---|---|
| `addons/LifeFramework/Scripts/Game/` | Enforce Script: `Core/`, `Feature/<Area>/` (each with a `Feature.md`), `Persistence/`, `UI/`, `Tests/`, ... |
| `addons/LifeFramework/{Prefabs,Configs,UI,Language,Missions}/` | Resources (`.et`, `.conf`, `.layout`, `.st`) |
| `addons/LF_Everon1989/` | The 1989 Everon flagship pack. Every launch loads it beside the core (`tools/lib/addons.mjs`); its own header is `Missions/LF_Everon1989.conf`. What goes there: `docs/architecture/core-vs-pack.md` |
| `addons/LifeFramework/Worlds/DebugWorld/` | Test world. `DebugWorld.ent` is 0 bytes on purpose; content is in `DebugWorld_Layers/*.layer` |
| `tools/` | `cli` (one entry point), `commands/`, `validation/`, `test/`, `merge/`, `actions/`, `street/` |
| `agent-box/` | Player-equivalent checks on a rendering Workbench (`agent-box/README.md`) |
| `docs/` | `features/<area>.md` (what each system does; `features.md` is the generated index), `debug-log-triage.md`, `worktrees.md`, `setup.md`, `persistence.md`, `roadmap/` |
| `website/` | life-framework.org: Vite + React site built from `docs/` and GitHub issues; own `package.json`; deploys from `main` to Cloudflare Pages (`website/README.md`) |

## Golden rules

1. **Never commit generated files**: `resourceDatabase.rdb`, `*.rdb.lock`, the addon `log`, `*.gproj.user`,
   `node_modules/`, `tools/mcp/*/`. The pre-commit hook (`.githooks/pre-commit`) enforces this; fix what it flags.
2. **Resource GUIDs are unique and immutable.** Never reuse, shuffle, or hand-duplicate a `.meta` GUID.
   `tools\cli validate` catches duplicates and orphan metas.
3. **Reference resources by `{GUID}path`** with the real folder casing.
4. **Binary assets are plain blobs** marked `-text` in `.gitattributes`. Text is LF everywhere (`* text=auto eol=lf`;
   `.cmd`/`.bat` stay CRLF); generators write LF. A shell eats backslashes: a `\return` becomes a lone CR plus `eturn`
   (`validate-no-lone-cr`), `/\s+/` becomes `/s+/`, `\b` a backspace byte. Write regex-heavy code with the Write/Edit
   tools; `validate-regex-backslashes` catches what slips (`// lf-regex-ok: <why>`).
5. **All classes are `LF_`; tests are `LF_Test_*`** (rename #334). The real EveronLife mod (`EL_`) can load on the
   same server, so `validate-no-el-prefix.mjs` rejects any `EL_` identifier in `addons/` (resource file names such as
   `EL_JailCell.et` keep their paths and are the only exception).
6. **Heavy commands run only in a worktree.** `build`, `test`, `dev`, `serve`, `ci` refuse to run in the main
   checkout, which is the human's world-editor copy.
7. **User-facing strings are localization keys** in `Language/everonlife_localization.st` (append, never rewrite; then
   `tools\cli regen-localization`, which sorts it by Id). Layouts reference `#Key`. Core text is neutral: place names
   and flavour copy go in the pack table (`LF_Everon1989/Language/`), where a key with the same Id overrides the core
   row (`validate-core-place-names`).
8. **Update the area's `docs/features/<area>.md` in the same change** that alters a feature. `docs/features.md` is a
   generated index (`tools\cli regen-features`); never add rows to it. Append or edit only your rows; the merge driver
   keeps rows both sides append and stops with conflict markers when both edit the same line. GitHub runs no merge
   driver, so such a PR goes CONFLICTING when main moves; the `pr-autoheal` workflow merges main in with the drivers
   within minutes. Leave it alone.
9. **Classify every new resource core or pack with the red line** (`docs/architecture/core-vs-pack.md`): the core
   decides what the game can do, the pack decides what this server is. The core never references
   `addons/LF_Everon1989/`; `validate-addon-boundary` enforces it. A pack resource goes in `addons/LF_Everon1989/`;
   one the rule calls pack that still sits in the core keeps its row in `docs/architecture/core-pack-inventory.md`.
   **Map independence:** core script never holds world coordinates, map, entity or layer names; a system finds its
   places by component through `LF_Places` (core-vs-pack.md "Map independence"). Tests keep annex names.
10. **Shared registries are per-feature files; never append to a central list.** A proof verb is
    `agent-box/proofs/<verb>.mjs`; a bus event is in `Feature/<Area>/LF_<Area>Events.c`; a world-end static reset is
    an `LF_StaticResetHook` subclass in the owning class's file, found by reflection (no list, no modded chain #2034,
    no static initializer #2755); a harness verb is a `modded class LF_AgentHarnessVerbs` in its feature
    (`LF_AgentHarness.Dispatch` is frozen). `validate-registries` enforces all four; `tools\cli heal-hotspots <slug>`
    moves an old branch's StaticReset and `docs/features.md` edits to their new files. A new CLI command is
    `tools/commands/<name>.mjs`.
11. **What a player sees is looked at before it ships.** `tools\cli look <area>` (`look item <prefab>`, or
    `wb_preview` for one resource) before and after, the `polish` checklist against the pictures, and the pair
    embedded in the PR (`tools\cli evidence`; `docs/style-guide.md`). **PR pictures come from `look`, not a session.**
    A session is only for what Workbench play cannot capture: HUD and other UI widgets, a second player or late
    joiner, motion, animation, sound, filming. A screen is built from UI kit parts; nothing debug-looking goes in a
    village layer (`validate-ui-kit`, `validate-player-props`). Slices put world props in a debug-grid cell
    (`tools\cli grid place`); only a `town-` pass edits town layers (`docs/architecture/world-zones.md`,
    `validate-world-zones`).

## Commands

`tools\cli help` and `tools\cli wt help` list everything (a command with no arguments prints its usage); these are the
daily ones.

| Command | Does |
|---|---|
| `tools\cli status` | Toolchain and MCP state; `workbench: ALLOWED` or `NOT allowed` for this machine |
| `tools\cli lead` / `lead prune-safe [--dry-run]` | The lead's one-screen view (issues by milestone, PRs, worktrees, pool, main guard); `prune-safe` prunes only worktrees whose PR is MERGED, clean, idle and unlocked, never forced (`docs/worktrees.md` "Lead view") |
| `tools\cli issue-check <N...>` | Before dispatching or building an old issue: LIKELY DONE, CHECK or CLEAR; it follows `Sliced: #N` cards, title quotes and key symbols. Read the PRs it cites before writing code (nine agents went to already-fixed issues on 2026-11-18, three of five old bugs on 2026-11-27) |
| `tools\cli wt new <slug> [--wait] [--priority low] [--base B]` | Worktree `Life-Framework-ws-<slug>` on `ws/<slug>` with its own ports (`docs/worktrees.md` "Fast `wt new`") |
| `tools\cli feature new <Area> <Name> [--kind logic\|world\|ui\|item] [--issue N]` | Scaffold a feature with tests, strings, docs rows, a PR body and `tmp/feature-<name>.md` (the ordered checklist); `feature check` lists the `TODO(scaffold)` left |
| `tools\cli evidence <issue> before\|after <area\|png>` / `... table` | Keep `look` pictures in `website/public/docs-images/<issue>/` and print the PR markdown |
| `tools\cli validate [--full\|--all]` | The checks in `tools/validation/`, skipping the Workbench compile and those green on the same inputs or untouched by the branch. `--all`: every check; `--full`, `ci`, `wt gate`: plus the compile (`docs/worktrees.md` "Incremental validate") |
| `tools\cli test --no-build --tier fast\|all\|persistence\|perf` | Boot DebugWorld, run the `LF_Test` suite; exit code is pass/fail; a compile error fails it |
| `tools\cli test --no-build --only\|--red <LF_Test_Class\|name\|file>[,...]` | `--only`: just those tests (the loop). `--red`: one boot with each `RedProof()` hook on; exit 0 only if every listed test FAILED, else it names the hollow ones. `--no-cache` forces a run past the pass cache |
| `tools\cli dev --tier fast` / `wt e2e <slug>` / `logs <pattern>` | validate + test, then the `[LFDebug:*]` lines / the two-boot save, restart and reload proof / grep the newest `console.log` |
| `tools\cli regen-tests` / `regen-localization` | Check every test's `[LF_TestInfo]` / regenerate the language tables |
| `tools\cli wt ship <slug> [--refs N] [--body-file F] [--commit "<subject>"]` | PR and auto-merge at once (see "Worktrees"). `--refs N` puts `Refs #N` in the body; merging never closes an issue. Before the preflight it prints the tests your diff can break that GitHub's `validate` never runs (any pack data change prints the whole pack leg); run that line, then ship |
| `tools\cli wt gate <slug>` | Optional manual gate for the diff's tier (`tools/lib/gate-tier.mjs`); not part of ship |
| `tools\cli prs status` | Read-only: every open `ws/*` PR, its checks and its worktree (`LIVE`, `done (ship)`, `idle`) |
| `tools\cli main-guard start\|stop\|status\|watch` | The gate on main after the merge (see "CI"; `docs/worktrees.md` "Main guard") |
| `tools\cli playtest checklist [--all] [--budget 60]` / `playtest verdicts <file>` | One playtest session's route through the `## Manual testing` steps of merged PRs, smoke list first (`tools/commands/playtest-checklist.mjs`). `verdicts` PRINTS the `gh` commands; the maintainer or lead runs them, agents never do |
| `tools\cli promote [--to sha] [--confirm]` | Human step: the checklist since `stable`; `--confirm` fast-forwards `stable` (`docs/worktrees.md` "Release gates"). Agents never run it |
| `tools\cli call <tool> '<json>'\|@file` | Call an MCP tool (`api_search`, `game_read`, ...) |
| `tools\cli usage --plan` / `usage --accounts` / `usage --pick` | Real plan use and pace; per-account usage; the provider id for the next agent |

**Compact output.** Heavy commands (`validate`, `test`, `session up`, `look`, `wt gate|ship`, `asset run`,
`harness.mjs flow`, proof verbs) print one `SUMMARY <cmd> ok|fail ... log=<path>` line and only the failing items with
file:line; the full transcript is the log file in `tmp/logs/`. Never pipe them through `tail`/`Select-Object`.
`--verbose` prints everything (`docs/agent-budget.md` "Compact command output").

**Test and session queue.** Just run `test`, `e2e`, or `session up`: the pool (`tools/pool.json`, `pool status`)
queues the run, sends it to the machine with the most RAM headroom and prints one line per outcome; read it.
`TIMEOUT after Ns` means the run hung and was killed; pass `--budget <s>` only for a known-long run. **Never pin**
(`--local`, `--remote`, `--machine`). Sessions flow like a river: one idle 3 min while a job waits (5 min otherwise)
is closed; any harness command resets the clock, `session.mjs lease [min]` holds it, and one older than 10 min while
another session queues is closed 2 min after a warning (`session up --hold <min>`, at most 20, for a known-long job).
Development-track and spike work runs with `--priority low`. Details: `docs/worktrees.md` "Lanes, priority, leases,
budgets".

**Session profiles.** `session up` defaults to `--profile lowspec` (640x360), right for any proof that reads logs and
harness results. Shots, filming, `journey` and a session whose images go in a PR (UI, motion, a second player) use
`--profile full` (`docs/worktrees.md` "Session profiles").

**Sessions are shared: prepare every edit before `session up`, do the proof and the shots in one session, and
`session down` straight after.** `session up` prints how many requests queue behind you; it boots one client unless
you pass `--clients 2`.

Launch tools through `tools/lib/quiet-launch.{mjs,ps1}` (hidden `LF-Agents` desktop; node spawns get
`windowsHide: true`), never in front of the user; `tools\cli hidden [--close]` lists them, `LF_SHOW_WINDOWS=1` shows
them (`docs/setup.md`).

**No Workbench on the maintainer's PC (Gaming-Box).** Its world editor lives there, so its pool caps lack `workbench`
and no agent starts a headless Workbench on it; its Workbench work goes to a pool machine with the cap and the `wb_*`
tools refuse (use `api_search`, `game_read`, `asset_search`, `api class`). On lf-testbox (PC1, the laptop, the primary
dev box) Workbench runs through the pool: a rdb refresh, compile or `look` there is correct; never kill it.
`tools\cli status` says which you are on. Never pass `--local` or set `LF_WB_LOCAL=1` to get around it
(`docs/worktrees.md` "No Workbench on this PC").

**Nothing waits forever.** Every external call has a bounded runtime:
- `ssh` inherits `ConnectTimeout 8` and `ServerAliveInterval 10` from `~/.ssh/config`, so a dead link fails in about
  30 s.
- Wrap any other probe in `timeout <s>`.
- Never hand-write a `for`/`until` + `sleep` wait loop, and never wait with Monitor, ScheduleWakeup or status polls.
  Use one blocking call with its own budget (`test`, `e2e`, `session up --wait`, `session wait`,
  `gh pr checks --watch`) in the foreground (timeout up to 600000 ms), or run it in the background and call
  `t3_background_wait` (`docs/agent-budget.md`).
- **A background run never wakes a subagent or delegated task:** ending the turn while one runs ends the task
  unfinished. Only a top-level session may end its turn and be re-invoked.
- If you stop another agent's process, message that agent at once.

## Worktrees

Start with `tools\cli wt new <slug>` and work inside it on `ws/<slug>`; never commit to `main`. `docs/worktrees.md`
covers ports, locks, shipping, and pruning. A long-lived base (e.g. `m3`): `wt new <slug> --base m3` records it and
`wt ship` follows it; `wt sync-base m3` PRs main into it ("Base branches").

**Never run `git worktree remove --force`, `rm -rf`, or `Remove-Item -Recurse` on a worktree or on `server/profile`.**
Their `game-addons` folders are junctions into the Steam installs; on 2026-11-07 `git worktree remove --force` (Git
for Windows 2.47) followed them and emptied the base game. Remove a worktree with `tools\cli wt prune <slug>`; delete
any other tree with `node tools/safe-rm.mjs <dir>` (`link-safety-selftest` proves both).

- **Never run git in the main checkout.** `wt new` is your first command (it fetches origin itself; no `git pull`
  first); the lead view and the main guard fast-forward that checkout. A hook (`tools/lib/main-write-guard.mjs`)
  refuses git writes there for every session; reads (`status`, `log`, `diff`, `fetch`) stay open.
  `LF_ALLOW_MAIN_GIT=1` is the human override.
- **Never write into the main checkout.** Use absolute paths under your worktree. The same hook denies file writes
  there once your session is in a worktree; `lead` moves strays to `C:\LF\lead\stray` ("Writes into the main
  checkout").
- **Scratch goes in `<worktree>/tmp/`** (gitignored) with names unique to your task, such as `tmp/pr-body-<slug>.md`.
  Never use a shared session scratch dir for files you publish (one agent shipped another's PR body that way);
  `validate-worktrees` warns about untracked files under the main checkout's `addons/` or `tools/`.
- **Ship with `wt ship`; there is no pre-merge gate** (maintainer, 2026-11-15). It checks the PR body
  (`tools/lib/pr-quality.mjs`), runs a seconds-long preflight, pushes, opens the PR and queues auto-merge; GitHub's
  `validate` is the only check before the merge and the main guard checks `main` after it ("Ship"). Do not poll for
  the merge: `tools\cli prs status` once; a shipped worktree reads `done (ship)`.
- **Merge `origin/main` only when GitHub reports the PR `CONFLICTING`, or when you need code that landed there.** Each
  extra merge re-runs validate for nothing; `wt ship` merges main itself when it must. The generated and append-only
  files (`.gitattributes` `merge=lf-*`) have merge drivers in `tools/merge/` and are not conflicts: on GitHub the
  `pr-autoheal` workflow merges main into a PR whose only conflicts are those files ("Auto-heal on GitHub"), and the
  maintainer or lead runs `prs heal` for what it skips.

## Tests

- One file per test: `Scripts/Game/Tests/LF_Test_*.c` extending `LF_Test`, with
  `[LF_TestInfo(LF_TestTier.LOGIC|WORLD|PERSISTENCE|PERF)]` directly above the class (the only tier source and the
  only registration: `LF_TestManager` finds tests by reflection, #2610) and a `// red-proof:` line.
  `validate-registries` fails a class with no attribute, with two, or a central list.
- Tiers are setup cost, not subject: `LOGIC` is pure script (`--tier fast`); `WORLD` loads a resource or touches the
  world; `PERSISTENCE` is a save round trip; `PERF` is a timing gate (`perf/budget-128`), not in `fast` or `all`, run
  alone with `--tier perf` (never cached or sharded).
- Prove red once, cheaply (each run is a server boot). A bug fix: the run where the new test fails on the unfixed code
  and passes on the fixed code IS the red proof; record it in `// red-proof:`, no extra break-and-restore run. New
  tests with no failing state: one batched `test --no-build --red <LF_Test_A,LF_Test_B>`; each test's `RedProof()`
  hook (`if (!RedProof()) <the act>;`) skips what its assertions guard. Record the `RED-OK` line. Node selftests in
  `tools/` keep their own red proofs and need no game run.
- Loop: `--only <LF_Test_Class>` (~10 s) until green, one `--red` run for the new tests, then `--tier all` once at the
  end. **Pass cache:** a pass on byte-identical inputs (`addons/`, its rdb, `tools/test/`, the selection, the build)
  is reused in about 1 s; a failure is never reused, and persistence, `--red`, `dev`, `e2e` and the placement audit
  always run (`docs/worktrees.md` "Pass cache").
- **A flake is a bug, not noise.** The main guard calls a step red only when it fails twice at the same head; one
  failure goes to `tmp/logs/main-guard-flakes.log` with its evidence. Fix the cause like any bug
  (`docs/testing/flake-audit-2026-11-27.md`).
- **Pack data is tested in the pack.** A test that names pack files or needs the pack's overlays is
  `addons/LF_Everon1989/Scripts/Game/Tests/LF_Test_Everon*.c` (same rules; attribute `pack: true`), run in the pack
  leg (`--tier all`, or `--only`/`--red` by name). Never a node selftest that reads `.conf` text
  (`docs/architecture/three-worlds.md` "Pack test leg").
- **A test deletes what it spawns** (`LF_TestCharacterFixture.Delete`, `LF_TestLooseSweep.DeleteBuilding`,
  `LF_TestVaultDrillFixture.Release`); `suite/no-leftover-fixtures` names the test that left a character or
  prefab-less prop at the gate (#2792), and a body that must outlive the run is declared with
  `LF_TestDebrisProbe.Keep(entity)`. Never delete characters in a sweep after each test (it crashed the engine), and
  never delete a character carrying a path-loaded pack (use the GUID, `LF_TestVaultDrillFixture.PACK_PREFAB`).
- **A test never calls `LF_StaticReset.ResetAll()`**: it wipes every feature's statics and fails every later test
  (#2092). Apply only your own feature's hook (`<Owner>.ResetStatics()`; `validate-no-test-resetall`).
- **A node selftest** that builds a scratch git repo calls `useHermeticGit()` and `removeScratch()`
  (`tools/lib/selftest-git.mjs`), and takes its temp folder from `scratchDir(prefix)` (`tools/lib/scratch.mjs`), never
  a bare `mkdtempSync(join(tmpdir(), ...))` (400,000 leaked folders, #2589; `selftest-scratch-selftest`).
- Persistence tests go through public mutators and accessors only; use `wt e2e` or a real restart for restarts.
- Wait for a run as in "Nothing waits forever". A rule that keeps getting broken becomes a check in
  `tools/validation/`.

## Logging and fail-safe

- Feature state changes and rejections log through `LF_Debug.Info/Warn/Error("<Feature>", msg)`, which prints the
  greppable `[LFDebug:<Feature>]` prefix. A silent no-op is a bug.
- A broken input (missing prefab, null entity, bad GUID, missing file) logs `LF_Debug.Error` and degrades that one
  feature. It never VMEs or aborts the game mode.
- One logging path per feature; no bare `Print("[LF_X] ...")` beside `LF_Debug`.
- Before chasing a console line, check `docs/debug-log-triage.md`; add new lines there when you explain them.

## Hard EnforceScript lessons (paid for by real bugs)

The engine does not forgive these. A sibling Reforger mod (Overthrow) shipped a bug for each one; they transfer here
because the engine is the same.

- **No ternary operator.** `cond ? a : b` is a compile error. Write `if/else`.
- **No null-coalescing operator.** Initialize in the declaration or guard with `if (!x)`.
- **`Substring` and `string.Format` cut at 8191 chars, silently.** Rebuilding a long string (a save, a config, a
  payload) with `a.Substring(..) + b` loses its tail with no error; join pieces under 8 KB with `+` or write through a
  JSON context. `validate-string-splice` fails a new site; a bounded one is marked `// lf-bounded: <why>` (the
  baseline only shrinks).
- **`Math.Max`, `Math.Min` and `Math.Clamp` work in floats.** An int above 2^24 (unix seconds, money, XP, a running
  counter) comes back rounded: 1800000480 became 1800000512 (#2787). Use `LF_Utils.MaxInt`, `MinInt`, `ClampInt` (or
  `Math.ClampInt`) for any int that can be large; `validate-int-time-math` fails a time-named int argument (real float
  time math is marked `// lf-float: <why>`).
- **Strong refs (`ref`) for Managed classes.** A Managed field not marked `ref` is garbage-collected at end of frame.
  Put `ref` on BOTH the collection and its elements: `ref array<ref LF_Foo>`, `ref map<string, ref LF_Foo>`.
- **Store `EntityID`, not `IEntity`, for anything long-lived.** Entities are deleted at any time. `FindEntityByID(id)`
  and null-check every use. `EntityID.INVALID` is the "none" state.
- **`EntityID` differs between server and client. Never send it over the network.** Use `RplId`; resolve via
  `Replication.FindItem(rplId)`.
- **`RplProp` replicates simple types only** (int, float, bool, short strings). Arrays, maps, and objects go through
  an RPC or `RplSave`/`RplLoad` (JIP).
- **A changed `RplProp` value does not broadcast itself.** Call `Replication.BumpMe()` after changing it. Throttle to
  significant changes or batch; per-frame `BumpMe` floods the network.
- **RPC direction.** `RplRcver.Server` = client→server (name `RpcAsk_*`); `RplRcver.Broadcast`/`Owner` = server→client
  (name `RpcDo_*`). On the server (a listen host too), `Rpc()` runs a `Server` handler, or an `Owner` handler on an
  entity the server owns, at once inside the call: no host branch, and a direct call beside it runs the handler twice.
  A `Broadcast` never runs on the server; call its effect locally when the host's player must see it. An entity with
  no `RplComponent` drops every Rpc (`docs/architecture/rpc-routing.md`).
- **A client can only RPC on entities it owns.** Inter-player communication goes through the server. Validate client
  requests on the server (money, inventory, RPC payloads); never trust the client.
- **`Init()` is not called automatically.** Initialize collections in the declaration or call `Init()` yourself from
  the game mode / owning manager.
- **Null-check singletons and UI.** `GetInstance()` returns null until the game mode has the component; `GetUI()` is
  null on a dedicated server.
- **Persistence.** Never persist `IEntity` or a session-local `EntityID`; persist a stable key and re-resolve on load,
  tolerating "not there". Load must be idempotent (it runs on fresh load AND when re-applying to a live session). No
  console guards; consoles are handled internally. A new payload is a section (#2841, `docs/persistence.md`
  "Sections"): `class LF_<Area>SavePayload : LF_SavePayload` in its feature folder with its own name, `Version()` and
  file; it reads every older version of itself, `OnAbsent()` covers an older save, `validate-record-shapes --update`
  locks its shape. `RegisterPersistencePayloads` is frozen (`validate-persistence-version`); no global save version
  moves. Legacy payloads stay positional and frozen (`version` first, read order = write order). A record name given
  to `context.Write`/`Read` is never reused (a reused `slotRows` once loaded every garage slot as 0;
  `validate-record-var-names`). **In-session round trips prove nothing about restarts**; the restart path is ground
  truth.

## Enfusion data rules

Detail lives in `.claude/skills/enfusion-resource-authoring` and `enfusion-blast-radius`.

- Prefer prefab inheritance over duplication; duplicate vanilla prefabs with `game_duplicate`.
- Override an inherited component by its GUID; a second component of the same class shadows it.
- A `MeshObject` needs a real `.xob` or the entity is invisible.
- A vector is written bare (`m_vHalfExtents 5 3 7`, `coords 1 2 3`), never quoted: a quoted list is a string, the load
  logs `Unexpected data at offset` and drops the entity at world origin. `validate-quoted-vectors` fails a layer,
  prefab, `m_v*` conf member or generator that writes one (`// lf-quoted-vector-ok: <why>` allows a real string).
- A textured mod item bakes its real `.emat` into the `.xob`; a prefab material override over a placeholder bake
  renders white.
- Textures are verified visually only; the dedicated server has no renderer.
- Place world props with `tools\cli place on|against|ground|arrange` (`docs/placement-tool.md`), never with typed
  coordinates: it reads real bounds, probes the surface or wall, writes the layer entry and audits it.
- World props must pass `tools\cli audit placement --strict` (`wt gate` runs it when a `.layer`, a `Prefabs` `.et` or
  spawn code changed). A placement meant to look odd goes in `agent-box/place-audit-allow.json` with a reason
  (`docs/placement-audit.md`).
- Island placement is gated: `wt ship` and the main guard fail a finding not in `tools/place/island-baseline.json`,
  and the baseline only shrinks (`docs/placement-audit.md` "The island gate").
- A new world interactable runs `node tools/actions/place-action-contexts.mjs`.
- A prefab with user actions carries an `RplComponent`, or its actions work only for a host
  (`validate-actions-have-rpl`; a local-mesh prop whose action overrides `HasLocalEffectOnlyScript()` is exempt).
- A price has one owner: a shop price rule, a catalogue `m_iPrice`/`m_iSellPrice` or a catalogue offer is written in one
  pack file, the one whose shop sells it, never two (`validate-shop-price-owners`; the later file wins in silence).
- Every piece a mannequin wears is a priced catalogue item its outlet still offers; drop an offer and the mannequin
  outfit goes with it (`validate-mannequin-outfits`; #5096 went red on `everon/mannequins`).
- Moving or renaming a resource breaks every `{GUID}path` reference, saved worlds included.
- Layouts: unique widget GUIDs per file, inherited-component GUIDs equal the base, a sibling `.layout.meta`.
- Input: bind keyboard plus `gamepad0:`; never `W/A/S/D`, `a`, the d-pad, `T`, or `shoulder_left`.
- A new persisted payload is a section (see "Persistence" above).

## Research before code

Never guess an Enfusion API. Start from the installed game, not the docs dump: `tools\cli api class <Name>` prints the
real declaration from the game paks and `api grep <regex>` searches them; `api_search` is the fallback. Other lookups:
`tools\cli call api_search|game_read|wb_read_props|asset_search '<json>'` (`@args.json` from PowerShell). Reading
sibling features to learn the shape is encouraged. See `enfusion-api-research`.

## Building a feature

The order is design, scaffold, look, place, build, test, look, review, ship
(`.claude/skills/life-mode/playbooks/feature.md`). Start with `tools\cli feature new`; do not hand-write what it
generates. Run **interrogate** when the diff touches replication, an RPC, persistence, money, inventory or GUIDs:
findings go in `tmp/review-<slug>.md`, each ticked `fixed:` or `dismissed:` with a reason, and ship carries the file
into the PR.

A chapter or epic runs through the roadmap pipeline (`docs/process/roadmap-pipeline.md`): one Opus planner cuts
file-disjoint slice cards (`docs/process/slice-card.md`, at most 45 min / 60 calls each),
`tools\cli slice list <parent> --ready` is the dispatch queue, `slice brief <issue>` the agent's brief, `slice record`
/ `slice stats` tune model routing. A slice that runs over is split, never extended.

## What "done" means (issue states)

Merging to `main` is not done. A human decides after play-testing `main`. Issue labels: `agent:working` (you are
building it), `in-dev` (merged, not verified), `needs-playtest` (merged and the PR lists manual steps), `verified` (a
human signed it off; only this closes the issue), `regression` (failed verification). The `issue-state` workflow moves
the issues a merged PR refs (`Refs #N`, also `Closes/Fixes #N`, which it reverses back to open);
`tools\cli promote --confirm` sets `verified` and closes. In your report and PR say "merged, in-dev", never "done" or
"fixed". Put manual steps under a `## Manual testing` heading: that is what makes the issue `needs-playtest` and what
the human's promotion checklist shows. `docs/worktrees.md` "Release gates".

## Verification ladder

1. `tools\cli validate`: cheap, run it constantly (`--full` adds the Workbench compile).
2. The `LF_Test` suite in your worktree: `--only` or `--tier fast` while iterating, `--tier all` once before calling
   work done.
3. Drive the real path: DebugWorld play mode or `node agent-box/harness.mjs flow <name>`; the harness `vehicle` and
   `drive` verbs drive a car as a player does (`agent-box/README.md`). How a place looks is `look` first (rule 11,
   `docs/visual-testing.md`).
4. Read the specific log line (`SCRIPT (E)`, `VME:`, `Unable to find`), never an error count.

Proof classes that survive the ladder: restart (a real save, restart, load), dedicated server (a green host run proves
nothing), late joiner (JIP state that was never written). There is no debugger or hot reload; debug with logs.

**Manual steps ship in the PR.** Multiplayer/JIP, UI, and the true restart path stay manual; the PR says exactly what
to do: host or join, actions, what to observe, whether a restart is needed.

## CI

GitHub Actions `validate` runs `tools\cli validate` on every PR and is the only required check on `main`. CI sets
`LF_VALIDATE_SKIP=validate-scripts.ps1,validate-action-contexts.mjs,validate-action-reach.mjs,validate-modded-container-props.mjs`
because the runner has no Reforger install or MCP build. Script compilation and the in-game suite stay local.

**The main guard** checks `main` after each merge (`docs/worktrees.md` "Main guard"): `validate --all`, then the
Workbench compile (a red compile is reverted at once), then build, full suite, placement audit and e2e (red twice
bisects). Several red merges with no compiler output are marked `infra`, not reverted, and a test-only merge is bisected with the
merge it follows (`docs/worktrees.md` "Main guard"). Its verdicts are the `main-guard/compile` and `main-guard/suite` commit statuses; a watchdog restarts a
stalled worker. `wt new` and `world-sync` start from the newest `compiled-ok` commit; `promote --confirm` needs
`green`.
