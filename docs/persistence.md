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
   character index, faction (the duty role: POLICE only while clocked in), on-duty,
   wanted level. A save from the faction-screen era loads POLICE-off-duty as CIVILIAN.
2. Bank balances.
3. Survival stats (hunger, thirst, health).
4. Stacked item quantity.
5. Organizations, hideout ownership, and owned vehicle records (lock
   identifier, key id, owner, lock flag).
6. Serving jail sentences (player uid, remaining seconds).
7. Each living player's carried inventory: the storage tree under the character
   (prefab, stack quantity, nesting). Death drops it.
8. Each player's progression, keyed by player uid: delivery career level, XP
   and paid total; police rank, on-duty and unpaid duty seconds; seconds since
   the last benefit collection; player level, XP and skill points; licences (by
   enum name); turned-in quest ids. Death keeps it.
9. The contents of placed storage (the hideout stash), keyed by a stable
   storage key such as `hideout:Roadmap_Hideout`, never an entity id.
10. Both duty inventories (M1-11): the police inventory stored at the desk
   (`police_duty`) or the civilian one (`civilian_stash`), whichever is not
   worn, keyed by character persistence id in the carried payload, and the
   rounds in every stored magazine (`itemstate` payload).
11. Wanted reports: offence, the description recorded at the crime, and the
   identified flag, keyed by player uid.

On connect the player spawns at a spawn point with the active character's
prefab. A saved carried inventory is applied to it; without one the character
gets the default loadout configured on `EL_SpawnLogic` in
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
  - item entity configs with `SelfSpawn 1` and `EL_QuantityComponentSerializer`.
    A stack saves with its parent (no `ParentHandling "Ignore always"`): with that
    flag a stack in a pocket was saved as a root and self-spawned on the ground
    after a restart, a copy of the cash the carried snapshot restores;
  - the player character config with `SelfSpawn 0` (the vanilla value). `EL_SpawnLogic`
    spawns every player character fresh and the carried snapshot restores what it
    carried; a self-spawned saved body would be an unowned copy of the same items.
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
  survival, organizations, hideout, vehicles, jail, carried, progression, introductions,
  itemstate, wanted, jobsignups, tickets. The written version is that list's length (currently 12). Deserialize stops at the stored version, so an
  older save leaves later payloads untouched. A new system appends one entry at
  the end and raises the version by one. Never insert earlier or reorder.
  The override bodies live in public static `SerializeStore` /
  `DeserializeStore` so a test drives the real path;
  `tools/validation/validate-persistence-version.ps1` checks those bodies.

  | Version | Adds |
  |---|---|
  | 1 | survival |
  | 2 | organizations |
  | 3 | hideout ownership |
  | 4 | owned vehicles |
  | 5 | serving jail sentences (`LF_Jail.ExportAll` / `ApplyAll`) |
  | 6 | carried inventories (`LF_StorageSnapshot.ExportAll` / `ApplyAll`); placed storage rides the same records (slot `placed`, no new version) |
  | 7 | progression (`LF_Progression.ExportAll` / `ApplyAll`; `ApplyToCharacter` on each spawn) |
  | 8 | introductions (`LF_Introductions.ExportAll` / `ApplyAll`) |
  | 9 | per-item state of every snapshot: magazine ammo by item index (`LF_StorageSnapshot.ExportItemState` / `ApplyItemState`) |
  | 10 | wanted reports (`LF_WantedReports.ExportAll` / `ApplyAll`) |
  | 11 | delivery-job sign-ups (`LF_CivilianJob.ExportSignUps` / `ApplySignUps`; `ApplySignUpToCharacter` on each spawn) |
  | 12 | police tickets: amount, online clock, stage, paid flag (`LF_FineTickets.ExportAll` / `ApplyAll`; the carried paper is reprinted on spawn) |
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
- **Session-start check** (dedicated only): `CheckSessionContinued()` logs
  `EL_Debug.Error` ("started a NEW session while N save point(s) exist") when
  the session was not launched from a save and the mission has saves.

### Continuing after a restart (production launch)

A dedicated server continues its saved world only when launched with
`-loadSessionSave` (bare = latest save point of the scenario, or a save UUID
from that save's `meta-info.json`). This is the vanilla engine mechanism: the
save loads at launch, before the game starts, with no extra transition.

```
ArmaReforgerServer.exe -config <server.json> -loadSessionSave
```

Without the flag the engine starts a new playthrough even when saves exist; to
players that is a full wipe, and the next autosave makes the fresh session the
latest save. Script deliberately does not auto-load at game start:
`LoadLatestSave()` is a game-state transition that boots the world a second
time, drops connected players, and would hijack Workbench play and the test
server. A missing save under the flag is engine-handled and starts fresh; an
EL payload that fails to read logs a `Persistence` warning and leaves that
feature's live state untouched.

`toolscli host start` passes the flag whenever the host profile has a save, and
`host stop` closes the server through `$profile:lf-host-stop` so the engine
writes the SHUTDOWN save (see `docs/playtest.md`).

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
- Manager singletons (`EL_ATMManager`, `EL_JobManager`,
  `EL_CharacterCreationManager`) are created only by `GetInstance()`. A
  constructor never registers itself and a destructor only clears its own
  registration. The game mode once did `new EL_ATMManager()` on the first
  connect, which replaced the bank cache the load had filled and wiped every
  balance.
- Carried inventory is a snapshot, not engine entity persistence. The player
  character is spawned fresh by `EL_SpawnLogic`, so the snapshot (keyed by
  character persistence id and slot name) is captured at each save point and on
  disconnect, and applied on spawn in place of the starter kit. Apply reconciles
  against what the entity already has, so it is idempotent. Death clears it.
- A field added to a persisted record class breaks every older positional save.
  Per-item state (`LF_StorageItemRecord.m_iAmmo`) is `[NonSerialized()]` and
  travels in its own payload instead; `police/item-state` proves the carried
  record format did not change.
- Session-only registries must be rebuilt on load: saved plates are
  re-reserved in `EL_LicensePlateRegistry` by `EL_VehicleOwnership.ApplyAll`.
- Garage it to keep it: after a load, an owned vehicle that was not stored is
  not spawned; it is at the impound lot (`EL_VehicleOwnership.IsImpounded`,
  derived from "not stored and no live entity", so the save has no impound
  field). No key is spawned at load. The impound desk releases it for the
  configured cash fee and hands the owner a key bound to it.
- The whitelist file is outside this serializer; an unreadable file blocks
  `EL_WhitelistManager.Save()` so it is never overwritten.

## Tests and proof

- `save/autosave-selection` (LOGIC) drives `EL_SaveSelection` across bitmask
  and playthrough edge cases.
- `save/manager-wiring` (WORLD) checks the manager is on the game-mode entity
  and resolved the persistence system.
- `restart/*` (`EL_Test_RestartSafety.c`): bank cache survives the connect
  path (LOGIC), jail payload through the real serializer write and read
  (WORLD), street vehicle impounded at load with no key (WORLD), plates re-reserved on
  load (LOGIC), unreadable whitelist file is kept (LOGIC).
- `e2e/pass1-write-save` / `e2e/pass2-verify-load` (PERSISTENCE, two boots)
  also carry a jail sentence and a carried inventory (237 cash, 3 apples, a
  water bottle inside a backpack) across the real restart.
- `persist/carried-roundtrip` (PERSISTENCE) and `persist/carried-death-no-dupe`
  (WORLD) drive the carried payload through the real serializer and the death rule.
- In-session round trips prove nothing; the restart is ground truth. The
  restart pass (save -> shutdown -> boot -> load) stays manual because the
  `EL_Test` runner cannot survive a game-state transition.
