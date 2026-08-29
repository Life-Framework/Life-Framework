# AGENTS.md — Life Framework development contract

Instructions for AI coding agents working in this repository (opencode, Codex,
Cursor, Claude Code, and any tool that reads `AGENTS.md`). Read this file
before making changes. This file is the portable, tool-agnostic contract; the
`.opencode/skills/` folder holds opencode's deeper workflow layer (see
"Rigorous workflows" below).

## Golden rules

1. **Never commit generated/artifact files**: `resourceDatabase.rdb`,
   `*.rdb.lock`, the addon `log` file, `*.gproj.user`, `node_modules/`, and the
   MCP clones under `tools/mcp/*/`. A pre-commit hook
   (`.githooks/pre-commit`, wired via `core.hooksPath`) enforces this — fix
   anything it flags rather than bypassing it.
2. **Resource GUIDs are unique and immutable.** Every resource has a `.meta`
   with a unique 16-hex GUID. Never reuse, shuffle, or hand-duplicate GUIDs
   across files. `tools\cli validate` detects duplicates and orphan `.meta`
   files.
3. **Reference resources by `{GUID}path`.** Path hints must match the real
   location (folder casing included). A lowercase `data/` in a path where the
   folder is `Data/` is a bug — the validator warns on it.
4. **Binary assets are committed as plain blobs** (no LFS yet). Keep them
   binary (`-text`) in `.gitattributes`; never run text conversion over them.
5. **Enforce Script lives under `addons/LifeFramework/Scripts/Game/`**, custom
   classes are prefixed `EL_`, and script classes referenced by prefabs/entities
   need the matching `XxxClass: YyyClass` pair where the base declares one.

## Build & test (the whole point)

Everything is driven through the unified CLI — agents should never need a human
or the Workbench GUI:

```sh
tools\cli status              # toolchain + MCP servers + environment
tools\cli build               # headless Workbench build of the addon (PC)
tools\cli test                # build + boot dedicated server on DebugWorld +
                              #   run the ELTEST suite + parse results (exit code)
tools\cli test --no-build     # skip the build, just run the server suite
tools\cli test --tier fast    # LOGIC-tier tests only (~seconds, no world deps)
tools\cli serve               # boot the test server and stream logs (blocks)
tools\cli ci                  # validate + build + test — the full gate
tools\cli validate            # repo consistency checks (also runs on commit)
tools\cli lint                # run tools/lint/* checks
tools\cli mcp install|update|verify|enable|disable   # manage MCP servers
```

The test flow: `Missions/EL_DebugTest.conf` loads `Worlds/DebugWorld`, the
`EL_TestRunnerComponent` runs the `EL_Test*` suite on server start and prints
machine-readable markers:

```
[ELTEST] START <name>
[ELTEST] PASS <name> (N assertions)
[ELTEST] FAIL <name>
[ELTEST]   - failure detail
[ELTEST] SUMMARY tier=fast|all passed=N failed=N total=N
```

`tools\cli test` waits for `SUMMARY`, asserts the reported tier matches the
requested one, scans for engine errors `( E )`, and exits nonzero on any
failure — that exit code is the agent's pass/fail signal. Default tier is
`all`; use `--tier fast` while iterating on pure logic and run the full `all`
tier before declaring a phase or fix complete.

## Adding tests / checks

- **In-game tests**: create `addons/LifeFramework/Scripts/Game/Tests/EL_Test_*.c`
  extending `EL_Test` (implement `GetName()` and `Run(ctx)`), then register it
  in `EL_TestManager.c::CollectTests` with a tier:
  `Register(new EL_Test_X(), EL_TestTier.WORLD)`.
  - **Tiers are setup cost, not subject.** `LOGIC` = pure EnforceScript, runs
    in `--tier fast` (use for every logic change). `WORLD` = loads a resource,
    spawns an entity, or reads the world (runs only in the full `all` tier).
    `PERSISTENCE` = reserved for save/reload round trips (roadmap Phase 1).
  - **Red-proof**: the test file must carry a `// red-proof:` comment recording
    how its assertions were observed failing at least once. A test that cannot
    go red is a defect — prove red by breaking an assertion and running the
    suite before you trust it green. `tools\cli validate` enforces registration
    and the red-proof comment.
  - **Persistence tests route through public API only**: mutate via manager
    mutators, reload, read back via accessors. Assertions that touch the
    persistence internals pass while the player-facing contract breaks.
- **Repo checks (pre-commit + `cli validate`)**: drop a script in
  `tools/validation/` — the CLI runs every script in the folder.
- **Other lint/test scripts**: drop them in `tools/lint/` or `tools/test/` and
  run `tools\cli lint` / `tools\cli run test`.
- **Manual test steps ship with the change.** The suite covers logic and world
  seams; multiplayer/JIP, UI, and the true restart path stay manual. A change
  touching any of those is not done until its PR description says exactly what
  to do: host or join, actions, what to observe, whether a restart is needed.
  In-session round trips prove nothing about restarts.
- **Use the MCPs instead of writing code.** `wb_read_props` returns resolved
  (inherited) prefab values — paste them into test expectations instead of
  hand-walking ancestry. `api_search`/`game_read` tell you which vanilla class
  already does the job; reusing it beats reimplementing it. `game_duplicate`
  keeps test fixture prefabs on correct inheritance so tests assert only the
  delta. `asset_search` gives the `{GUID}path` for fixture lists.

## MCP servers (AI tooling)

Configured in `opencode.json`; the clones live (git-ignored) under
`tools/mcp/`. `tools\cli mcp install|update|verify|enable|disable` manages
them. `enfusion-mcp` is enabled by default; `enfusion-workbench` is opt-in.
Environment paths (`ENFUSION_WORKBENCH_PATH`, `ENFUSION_GAME_PATH`,
`ENFUSION_PROJECT_PATH`, optional `ENFUSION_SERVER_PATH`) are set there.

### Calling MCP tools without an opencode session

`tools\cli call <tool> '<json args>'` (or `node tools/mcp-call.mjs ...`) invokes
a server tool directly over stdio JSON-RPC, no opencode restart needed. This is
the way to research the Enfusion/vanilla API from subagents or a plain terminal:

```
tools\cli call list
tools\cli call api_search '{"query":"SCR_SpawnLogic","format":"tree"}'
tools\cli call game_read @tmp/args.json     # file-based args: PowerShell mangles inline JSON
```

Tools of interest: `api_search` (classes/methods with inheritance), `game_read`
(vanilla source from `.pak`), `game_browse`, `asset_search` (GUID-prefixed
paths), `wiki_search`/`wiki_read`, `component_search`, `wb_*` (live Workbench).
Never guess an Enfusion API - look it up with `tools\cli call` first. See
`tools/mcp-call.mjs` and `tools/README.md`.

## Hard EnforceScript lessons (paid for by real bugs)

The engine does not forgive these. A sibling Reforger mod (Overthrow) shipped a
bug for each one; they transfer here because the engine is the same.

- **No ternary operator.** `cond ? a : b` is a compile error. Write `if/else`.
- **No null-coalescing operator.** Initialize in the declaration or guard with
  `if (!x)`.
- **Strong refs (`ref`) for Managed classes.** A Managed field not marked `ref`
  is garbage-collected at end of frame. Put `ref` on BOTH the collection and
  its elements: `ref array<ref EL_Foo>`, `ref map<string, ref EL_Foo>`.
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
  inventory, RPC payloads) — never trust the client.
- **`Init()` is not called automatically.** Initialize collections in the
  declaration or call `Init()` yourself from the game mode / owning manager.
- **Null-check singletons and UI.** `GetInstance()` returns null until the game
  mode has the component; `GetUI()` is null on a dedicated server.
- **Persistence.** Never persist `IEntity` or a session-local `EntityID`;
  persist a stable key and re-resolve on load, tolerating "not there". Write a
  `version` first; binary save contexts are positional (write order = read
  order). Load must be idempotent (runs on fresh load AND when re-applying to a
  live session). No console guards — consoles are handled internally.
  **In-session round trips prove nothing about restarts**; the restart path is
  ground truth.

## Enfusion data rules

- **Prefabs.** Prefer inheritance over duplication; `prefab inspect` reads the
  full ancestor chain. Duplicate base-game prefabs with the MCP `game_duplicate`
  tool so GUIDs stay unique. A `MeshObject` needs a real `.xob` model or the
  entity is invisible. Every new resource needs a `.meta` with a unique GUID.
  A move/rename breaks every `{GUID}path` reference, including saved worlds —
  map references before moving.
- **Layouts.** A widget-instance GUID must be unique within the file; an
  inherited-component GUID must equal the GUID in the base layout (a fresh one
  adds a second, unconfigured component and the widget silently does nothing).
  Every new `.layout` needs a sibling `.layout.meta` with the
  PC/XBOX/PS4/HEADLESS configs. `m_sActionName` must match an `Action` AND be
  listed in the screen's `ActionContext`, or the key never fires.
- **Input.** A menu's input context must be activated every frame while open.
  List `MenuBack` plus `MenuUp/Down/Left/Right` and `MenuSelect` or a pad cannot
  drive the menu. Never bind a verb to `W/A/S/D` (menu nav), `a` (`MenuSelect`
  double-fire), the d-pad, `T`, or `shoulder_left` (VON owns it at priority
  110). Bind a `gamepad0:` source alongside every keyboard source.
- **Localization.** User-facing strings are keys in
  `Language/everonlife_localization.st`, never literals. A key missing from a
  language renders as the key at runtime. Fill in the `Comment` field for
  translators. Layouts reference the key (`#Key`), not the string.

## Verification (the ladder)

A change is not done until it climbs the rungs its scope requires:

1. **Repo hygiene + headless compile** — `tools\cli validate`. Cheap; run it
   constantly. Do not ask the user to compile.
2. **In-game suite** — `tools\cli test`. Boots the DebugWorld server, runs the
   `EL_Test*` suite, exits nonzero on failure. A scarce gate: once after a phase
   or fix is complete, never mid-edit.
3. **DebugWorld play** — drive the actual feature path in the running game and
   read the observable result (log line, UI value, inventory change).
4. **Logs** — `Documents/My Games/ArmaReforger/logs/<date>/console.log` and
   `error.log`; script errors match `SCRIPT (E)` / `VME:` / `Unable to find`.
   Judge by specific patterns, never by total error counts.

**Proof classes that survive the ladder.** Restart (a real save → restart →
load; in-session round trips prove nothing). Dedicated server (a green host run
proves nothing; UI without a server-side guard breaks). Late joiner (JIP state
that was never written is invisible to players who were there from the start).
There is no interactive debugger and no hot reload: debug with `Print()` + logs,
then restart play mode.

## Environment

- Arma Reforger Tools (Workbench) and the game are required for `cli build`.
- Arma Reforger Server (Steam app 1874900) is required for `cli test`/`serve`.
- Server config: `server/configs/test-server.json` (scenario + RCON).
  Launch wrapper: `server/scripts/launch-test.ps1` / `.sh`.

## Rigorous workflows (any tool may read these)

The `.opencode/skills/` folder is opencode's skill loader, but the files are
plain markdown any agent can read directly for the rigorous engineering
workflows this project uses:

- **Routing mode + playbooks** — `.opencode/skills/life-mode/SKILL.md` and
  `playbooks/*.md` (investigation, bug-fix, feature, refactoring, prototype,
  runtime-forensics, multi-phase-plan, session-pickup). Match the task to a
  playbook, copy its steps, apply the principles.
- **Principles** — `.opencode/skills/principle-*/SKILL.md`. The load-bearing
  ones: prove-it-works, fix-root-causes, sequence-verifiable-units,
  laziness-protocol, foundational-thinking, boundary-discipline,
  guard-the-context-window.
- **Domain authoring guides** — `.opencode/skills/enfusion-api-research`,
  `enfusion-script-authoring`, `enfusion-prefab-authoring`,
  `enfusion-config-authoring`, `enfusion-verify`.
- **Situational skills** — `el-tdd`, `enfusion-how`, `enfusion-architect`,
  `enfusion-blast-radius`, `interrogate`, `show-me-your-work`, `unslop`.

If a rule in AGENTS.md and a skill file ever disagree, **AGENTS.md wins** — it
is the portable contract every tool reads.

## Reminders

- After editing `opencode.json`, tell the user to restart opencode.
- Keep commits focused; run `tools\cli ci` (or at least `cli validate`) before
  finishing a task.