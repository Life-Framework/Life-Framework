# Session Handoff — 2026-08-29

What happened this session, what is done, what is pending, and the environment
facts a future session must know before touching this repo. Read this before
anything else if you are resuming this work.

## State

- **Single branch `main`.** All work is committed here. `arribas-files` and
  `foundation-fixes` were deleted locally; the worktree
  (`C:\Users\jaspe\Documents\Reforger\Life-Framework-wt`) was removed.
- **Remote:** `origin` → https://github.com/Life-Framework/Life-Framework.git.
  `main` is **ahead of origin/main by ~18 commits and NOT pushed.** Pushing is
  the owner's call (it overwrites the public main).
- Last fully-green verification on this tree: `tools\cli ci` → validate OK +
  Workbench build (exit 0) + server e2e `SUMMARY tier=all passed=20 failed=0`.

## What was done

1. **EPF → first-party persistence migration (shipped).**
   - gproj depends on the base game only (`58D0FB3206B6F859`); EPF/EDF gone.
   - `Configs/Systems/Persistence/LifeFramework.conf` +
     `ChimeraSystemsConfig.conf` bind `SCR_PersistenceSystem` and the
     serializers.
   - New: `EL_PersistenceComponent`, `EL_PersistenceComponentSerializer`,
     `EL_QuantityComponentSerializer`, record classes
     (`EL_PlayerAccountRecord`, `EL_BankAccountRecord`, `EL_SurvivalStatsRecord`).
   - Rewritten: `EL_PlayerAccountManager`, `EL_ATMManager`, `EL_BankAccount`,
     `EL_SurvivalStats`, `EL_CharacterSurvivalComponent`, `EL_SpawnLogic`
     (now `SCR_SpawnLogic`), `EL_Utils`/`EL_NetworkUtils` (off EPF bases),
     `EL_CharacterCreationManager`.
   - Deleted: 4 EPF SaveData classes, `EL_BitFlags`.
   - See `docs/persistence-migration.md` for the design and gotchas.
2. **Foundation cleanup** — resolved every placeholder GUID across prefabs and
   the debug CrimeChain.layer (real AK74/BRDM2 refs), purged dead
   Group/Property/Garage/Furniture localization, fixed the chimeraMenus
   duplicate preset, manager singletons no longer extend `ScriptedUserAction`,
   README rewritten with provenance (Everon Life fork) + no-EPF dependency.
3. **MCP call tooling** — `tools/mcp-call.mjs` + `tools\cli call <tool> '<json>'|@file`
   (JSON-RPC over stdio, no opencode restart). `--env <file>` override and
   `.json`-arg-file detection included.
4. **Server e2e harness (green).**
   - `Missions/EL_DebugTest.conf` + DebugWorld Tests.layer.
   - Tier-based `EL_Test` suite (`EL_TestManager.CollectTests`, LOGIC/WORLD).
   - `tools/test/test-e2e.ps1`: boots the dedicated server, polls the profile
     console.log every 2s for `[ELTEST] SUMMARY`, self-terminates (~90s cap).
   - `tools\cli test [--no-build] [--tier fast|all]` delegates to it.
   - 20/20 tests pass (all tier); 14/14 pass (fast tier).
5. **Fixes found by making the suite go red:**
   - `EL_PlayerAccount.RemoveCharacter` corrupted the active index after
     `RemoveItemOrdered` reindexes — re-clamped.
   - `EL_ATMManager` singleton: lazy `GetInstance()` + `ref s_Instance`
     (was null after `Reset()`).
   - Test prefab/layout/localization paths used bare paths that resolve to
     `{0000000000000000}` on the dedicated server — now `{GUID}path` form.
   - Strong-`ref` to engine-managed types (`PlayerController`, ScriptComponents)
     is a compile error — removed across UI/menu files.
   - Survival test used `Eat(50)` where `SetHunger(50)` was intended (Eat adds).

## Pending / owner actions

- **Restore base-game data (blocker).** Steam emptied
  `Arma Reforger\addons\core` and `\data` (no `ArmaReforger.gproj`/`.pak`).
  Until restored, `cli build`/`cli test` report "Game addon not found".
  Fix: Steam → Arma Reforger → Properties → Installed Files → Verify integrity
  of game files. Then re-run `tools\cli ci` to confirm green.
- **Push `main`** (owner's decision): `git push origin main`.
- **Remaining manual proof:** the restart round-trip (save → restart → load)
  and a real dedicated-server + JIP session. The suite covers LOGIC + WORLD
  seams; persistence restart and multiplayer are still manual per AGENTS.md.

## Environment facts (verified this session)

- **Reforger 1.8.0.10** (game, Workbench, server all on this version).
- **EPF does not compile on 1.8.0.10** — this was the trigger for the
  migration; do not re-add it.
- **Dedicated server:** `-server <world>` + `-addonsDir` + `-addons` is the way
  to test a local addon. `-config` cannot be combined with `-addons`
  ("-config cannot be used together with addons"). The server's own
  `./addons` contains packed EPF/EDF paks that must be excluded — the harness
  uses a neutral addonsDir (repo addons + a junction to game `core`/`data`).
- **Headless Workbench compile:** `-gproj <gproj> -addonsDir <game\addons>
  -wbsilent -validate -profile <name>` works. `cli build` uses
  `-buildData` + `ENFUSION_PROJECT_PATH` (Workbench profile addons) + a
  junction for the base game.
- **MCP servers** (in `tools/mcp/`, git-ignored clones): `enfusion-mcp`
  (enabled) and `enfusion-workbench` (opt-in). Queryable via `tools\cli call`.
- **Logs:** server → `server/profile/test/logs/<ts>/console.log`; Workbench →
  `%USERPROFILE%\Documents\My Games\{ArmaReforgerWorkbench,LifeFrameworkCI}\logs`.

## Conventions in force (AGENTS.md)

- No ternary, no null-coalescing, explicit types, `ref` on Managed collections.
- `EL_` prefix for mod classes; `m_`/`m_i/m_f/m_s/m_b/m_a/m_m` member prefixes.
- Every test file needs a `// red-proof:` comment; every `EL_Test` class must be
  registered in `EL_TestManager.CollectTests()` (enforced by
  `tools/validation/validate-tests.ps1`).
- `tools\cli ci` is the full gate. Run `cli validate` constantly.
- Never commit: `*.rdb`, `*.rdb.lock`, addon `log`, `*.gproj.user`,
  `node_modules/`, `tools/mcp/*` clones. Pre-commit hook enforces this.
- Localization keys, not literals, for user-facing strings.