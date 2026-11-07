# Persistence

How Life Framework saves and restores player and world state. It uses the base
game's first-party persistence (`SCR_PersistenceSystem` + `SaveGameManager` +
scripted serializers bound in a `.conf`). There is no EPF/EDF dependency: EPF
does not compile on Reforger 1.8, so the mod was migrated off it (2026-08-29)
and `LifeFramework.gproj` depends on the base game (`58D0FB3206B6F859`) only.
Do not re-add EPF.

## Observable contract

Each of these must survive a server restart:

1. A player's account: characters (id, prefab, first/last name, age), active
   character index, faction, on-duty, wanted level.
2. Bank balances.
3. Survival stats (hunger, thirst, health).
4. Stacked item quantity.
5. Organizations, hideout ownership, and owned vehicle records (lock
   identifier, key id, owner, lock flag).

On connect the player spawns at a spawn point with the active character's
prefab and the default loadout configured on `EL_SpawnLogic` in
`GameMode_Roleplay.et` (jacket carrying `MoneyStack`, plus clothing).

Identity:

- Account key = player identity id (`SCR_PlayerIdentityUtils.GetPlayerIdentityId`),
  resolved through `EL_Utils.GetPlayerUID`. On a non-dedicated session (offline
  Workbench play, listen host) it falls back to a name-derived identity.
- Character persistence id = `PersistenceIdUtils.Generate()` (stable UUID).

## Layer 1: data (serializers and config)

- **Config binding (load-bearing).** `Configs/Systems/Persistence/LifeFramework.conf`
  is a `PersistenceSystemConfig` inheriting the vanilla `Common.conf`. It binds:
  - the game-mode entity serializer and its component serializers, including
    `EL_PersistenceComponentSerializer`;
  - item entity configs with `SelfSpawn 1` and `EL_QuantityComponentSerializer`;
  - the player character config with `SelfSpawn 1`.
- **System registration.** `SCR_PersistenceSystem` (pointing at
  `LifeFramework.conf`) is registered in `Configs/Systems/LifeFrameworkSystems.conf`,
  which inherits vanilla `ChimeraSystemsConfig.conf` by GUID and is referenced
  from each mission header's `SystemsConfig`
  (`Missions/EL_DebugTest.conf`, `Missions/EveronLifeGameMode.conf`). Do not
  override the base `ChimeraSystemsConfig.conf` at a case-differing path: two
  resources claiming one GUID resolve flakily between boots.
- **Game-mode prefab.** `GameMode_Roleplay.et` carries the native
  `Persistence` component, `EL_PersistenceComponent`, and
  `EL_PersistenceManagerComponent`.
- **`EL_PersistenceComponent`** (`Scripts/Game/Persistence/`) is a
  `ScriptComponent` on the game-mode entity. It owns only the survival-stats
  map (`GetSurvivalStats` / `SetSurvivalStats`, keyed by character
  persistence id). Accounts, bank accounts, and organizations live in their
  manager singletons (`EL_PlayerAccountManager`, `EL_ATMManager`,
  `EL_OrganizationManager`); hideout records in `LF_Hideout`; vehicle records
  in `EL_VehicleOwnership`, keyed by lock identifier.
- **`EL_PersistenceComponentSerializer`** (`Scripts/Game/Persistence/Serializers/`)
  writes `version` first, then accounts and bank records (the prefix on every
  version), then the payloads listed in `RegisterPersistencePayloads`:
  survival, organizations, hideout, vehicles. The written version is that
  list's length (currently 4). Deserialize stops at the stored version, so an
  older save leaves later payloads untouched. A new system appends one entry at
  the end and raises the version by one. Never insert earlier or reorder.
- Managers expose `ExportAll()` / `ApplyAll(records)` / `GetOrCreate(id)`.
  Record classes are plain Managed: `EL_PlayerAccountRecord`,
  `EL_BankAccountRecord`, `EL_SurvivalStatsRecord` (each under the feature's
  `Persistence/` folder).
- **`EL_QuantityComponentSerializer`** (`Scripts/Game/Feature/Quantity/Persistence/`)
  writes the stack quantity (skipped when 1).
- Spawning: `EL_SpawnLogic : SCR_SpawnLogic`; `Prefabs/MP/Spawn/SpawnPoint_LIFE.et`
  is a vanilla `SCR_SpawnPoint`.

## Layer 2: orchestration (when a save happens)

Without this layer nothing asked for a save point, so a dedicated-server
restart could lose everything. `EL_PersistenceManagerComponent`
(`Scripts/Game/Persistence/`) is a server-side `ScriptComponent` on the game-mode
entity that drives two engine layers:

1. `SCR_PersistenceSystem` - server-only world system that tracks and
   serializes instances. Resolved via `GetScriptedInstance()`.
2. `SaveGameManager` - engine singleton that owns save points
   (create / list / load / delete).

Tuning lives in `[Attribute]`s on the component: `m_bEnableAutosave`
(default on) and `m_fAutosaveInterval` (default 600 s). A prefab override
tunes it per server.

- **Resolve and subscribe** (server only): resolve the persistence system at
  entity init and again on game start; subscribe to its `OnStateChanged` /
  `OnBeforeSave` / `OnAfterSave` and to the game mode's `OnGameStart` /
  `OnGameEnd`. Seed the has-save cache with an async `GetSaves()` scan.
- **Autosave**: one repeating `CallLater` timer. A tick is skipped (not
  queued) while a save or load is in flight. One `AUTO` slot per playthrough:
  the latest AUTO save of the current playthrough is overwritten, else one is
  created. The selection is `EL_SaveSelection` (pure, tested).
- **Manual save**: `SaveGame()` -> `RequestSavePoint(MANUAL)`. Result via
  `GetOnSaveFinished()` and `GetCompletedSaveCount()`.
- **Shutdown save is engine-owned.** The engine requests the SHUTDOWN save
  point itself on graceful exit. A manual request from `OnGameEnd` is rejected
  because the persistence system is already tearing down (the callback
  arrives false and nothing is written). The manager marks the session as
  ending, reports the engine's outcome through `OnAfterSave`, and logs an
  `EL_Debug.Error` if the session ends without a shutdown save.
- **Save cache**: `HasSaveGame()` / `IsSaveCacheSeeded()`, kept current by
  `OnAfterSave` (fires for engine-initiated saves too).
- **Load / continue**: `LoadLatestSave()` (engine transition into the save),
  `ReapplyLatestSaveData()` / `ReapplyEntitySaveData()` (idempotent live
  re-apply). Diagnostics: `GetLastLoadDiagnostic()` / `GetLastReapplyDiagnostic()`.
- **Wipe**: `WipeSave()` purges every save point of the mission.

## Gates, logging, fail-safes

- Every save path passes `PassesSaveGates()`: save manager present, saving
  allowed, persistence system active. Each rejection logs an `EL_Debug`
  message under the `Persistence` feature with a reason and degrades that one
  request.
- Logged transitions: autosave scheduled, tick skipped (busy), save point
  requested / created / failed, session end marked, load requested, re-apply
  accepted/rejected, wipe complete, system state change.
- No persistence system at game start (server): `EL_Debug.Error` (the systems
  config lacks the `SCR_PersistenceSystem` entry). Clients and editor worlds
  degrade to a no-op.
- A rejected save request logs and fires `GetOnSaveFinished()` with `false`.
- `OnGameEnd` raised twice (dedicated-server shutdown) is a no-op the second
  time.

## Rules (paid for by real bugs)

- A serializer that compiles but is not listed in the `.conf` is silently
  never called.
- Binary save contexts are positional: write order == read order. Version
  first; `if (version < 1)` skip.
- Load must be idempotent: it runs on a fresh load and when re-applying to a
  live session. `ApplyAll()` replaces cached state; the survival map is
  cleared before rebuild.
- `SelfSpawn` is the only mechanism that crosses a restart boundary for
  entities.
- No console guards. `ESaveGameType` is a bitmask. `GetSaves()` is async.
- GUID-matched `.conf` overrides replace list entries: never write partial
  lists.
- Never persist `IEntity` or a session-local `EntityID`; persist a stable key
  and re-resolve on load.

## Tests and proof

- `save/autosave-selection` (LOGIC) drives `EL_SaveSelection` across bitmask
  and playthrough edge cases.
- `save/manager-wiring` (WORLD) checks the manager is on the game-mode entity
  and resolved the persistence system.
- In-session round trips prove nothing; the restart is ground truth. The
  restart pass (save -> shutdown -> boot -> load) stays manual because the
  `EL_Test` runner cannot survive a game-state transition.
