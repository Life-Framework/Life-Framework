# Save System (orchestration layer)

Status: **implemented 2026-08-30**. Adds the missing "what triggers a save" half of
persistence. The serializers and `.conf` binding shipped in the EPF migration
(`docs/persistence-migration.md`) already write account, bank, survival and
quantity data; this layer is what makes those writes actually happen on a
schedule and on shutdown, and what lets a server owner or admin ask for one.

## The gap this closes

Before this, nothing in `addons/LifeFramework` called `SaveGameManager` or
`SCR_PersistenceSystem`. The serializers were only exercised when the base game
happened to save. On a dedicated server nothing asked for a save point, so a
restart could lose everything.

## Data shape

`EL_PersistenceManagerComponent` — a `ScriptComponent` on the game-mode entity
(`GameMode_Roleplay.et`), server-side orchestration over the two engine layers:

1. **`SCR_PersistenceSystem`** — server-only world system that tracks instances
   and serializes them. Configured by `Configs/Systems/Persistence/LifeFramework.conf`,
   registered by `ChimeraSystemsConfig.conf`. Resolved via `GetScriptedInstance()`.
2. **`SaveGameManager`** — the engine singleton that owns save points
   (create / list / load / delete). "Press save" talks to this.

Config lives as `[Attribute]`s on the component (`m_bEnableAutosave`,
`m_fAutosaveInterval`), so a prefab or per-server prefab override tunes it
without touching code.

## What the manager does

- **Resolve and subscribe** (server only): resolve `SCR_PersistenceSystem`,
  subscribe to its `OnStateChanged` / `OnBeforeSave` / `OnAfterSave` invokers,
  and to the game mode's `OnGameStart` / `OnGameEnd` invokers. Seed the
  has-save cache with an async `GetSaves()` scan.
- **Autosave**: one repeating timer (`CallLater`). Skips a tick when a save or
  load is already in flight. Bounded to one `AUTO` slot per playthrough: finds
  the latest AUTO save for the *current* playthrough and overwrites it, else
  creates one. The selection is `EL_SaveSelection` (pure, tested).
- **Manual save**: `SaveGame()` -> `RequestSavePoint(MANUAL)`. Honest result via
  `GetOnSaveFinished()` and `GetCompletedSaveCount()`.
- **Shutdown save**: fires on the game mode's `OnGameEnd`, once per session
  (the engine raises it twice on a dedicated server shutdown). Blocking request
  so it completes before teardown.
- **Save cache**: `HasSaveGame()` / `IsSaveCacheSeeded()` from the async scan,
  kept current by `OnAfterSave` (fires for engine-initiated saves too).
- **Load / continue**: `LoadLatestSave()` (engine transition into the save) and
  `ReapplyLatestSaveData()` / `ReapplyEntitySaveData()` (live re-apply of a
  tracked instance's record, idempotent). Diagnostics via
  `GetLastLoadDiagnostic()` / `GetLastReapplyDiagnostic()`.
- **Wipe**: `WipeSave()` purges every save point of the mission.

## Gates (boundary discipline)

Every save path passes `PassesSaveGates()`: save manager present, saving
allowed, persistence system active. Every rejection logs an `EL_Debug` message
with the feature tag `Persistence` and a reason, and degrades that one request.
A misconfigured server gets a greppable line, never a VME.

## Debug logging contract

All state transitions log through `EL_Debug` under the `Persistence` feature:
autosave scheduled, autosave tick skipped (busy), save point requested, save
point created, save point failed, shutdown save requested, load requested,
re-apply accepted/rejected, wipe complete, system state change. A silent no-op
is a bug.

## Tests

- `save/autosave-selection` (LOGIC): drives `EL_SaveSelection` across the
  bitmask + playthrough edge cases (combined flags, wrong playthrough, newest
  AUTO wins, manual-only list).
- `save/manager-wiring` (WORLD): on the DebugWorld test server, the manager
  component is on the game-mode entity and resolved the persistence system.

The true restart proof (save -> shutdown -> boot -> load) stays a manual server
pass per AGENTS.md: the EL_Test runner cannot survive a game-state transition.

## Fail-safes

- No persistence system (client, Workbench editor world): manager degrades to a
  no-op, `GetPersistenceSystem()` null, `EL_Debug.Warn` once.
- Save request rejected by the engine: the callback logs the failure and fires
  `GetOnSaveFinished()` with `false`; nothing throws.
- Autosave while busy: the tick is skipped, not queued.
- Shutdown save requested twice: the once-only guard makes the second a no-op.