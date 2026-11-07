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

- **Resolve and subscribe** (server only): resolve `SCR_PersistenceSystem` at
  entity init when it is already up, and again when the game mode starts (the
  system is guaranteed present by then on a configured world). Subscribe to its
  `OnStateChanged` / `OnBeforeSave` / `OnAfterSave` invokers, and to the game
  mode's `OnGameStart` / `OnGameEnd` invokers. Seed the has-save cache with an
  async `GetSaves()` scan.
- **Autosave**: one repeating timer (`CallLater`). Skips a tick when a save or
  load is already in flight. Bounded to one `AUTO` slot per playthrough: finds
  the latest AUTO save for the *current* playthrough and overwrites it, else
  creates one. The selection is `EL_SaveSelection` (pure, tested).
- **Manual save**: `SaveGame()` -> `RequestSavePoint(MANUAL)`. Honest result via
  `GetOnSaveFinished()` and `GetCompletedSaveCount()`.
- **Shutdown save**: engine-owned. The engine requests the SHUTDOWN save point
  itself on graceful exit (return to main menu / dedicated-server shutdown); a
  manual request from the game mode's `OnGameEnd` is rejected because the
  persistence system is already tearing down by then (the request callback
  arrives false and nothing is serialized). The manager marks the session as
  ending, reports the engine's outcome through `OnAfterSave` (fires for
  engine-initiated saves too), and logs a fail-safe error when a session ends
  without a shutdown save. This was fixed 2026-08-30; the previous
  `RequestSavePoint(SHUTDOWN)` from `OnGameEnd` always lost the race on the
  Workbench play mode.
- **Save cache**: `HasSaveGame()` / `IsSaveCacheSeeded()` from the async scan,
  kept current by `OnAfterSave` (fires for engine-initiated saves too).
- **Load / continue**: `LoadLatestSave()` (engine transition into the save) and
  `ReapplyLatestSaveData()` / `ReapplyEntitySaveData()` (live re-apply of a
  tracked instance's record, idempotent). Diagnostics via
  `GetLastLoadDiagnostic()` / `GetLastReapplyDiagnostic()`.
- **Wipe**: `WipeSave()` purges every save point of the mission.
- **Session-start check** (dedicated only): if the session was not launched from
  a save point and the mission has saves, logs `EL_Debug.Error` ("started a NEW
  session while N save point(s) exist"). See "Continuing after a restart".

## Continuing after a restart (production launch)

A dedicated server continues its saved world only when it is launched with
`-loadSessionSave` (bare = the latest save point of the scenario, or a save UUID
from that save's `meta-info.json`). This is the vanilla engine mechanism: the save
is loaded at launch, before the game starts, with no extra world transition.

```
ArmaReforgerServer.exe -config <server.json> -loadSessionSave
```

Without the flag the engine starts a new playthrough even when saves exist; to
players that is a full wipe, and the next autosave makes the fresh session the
latest save. Script deliberately does not auto-load at game start:
`LoadLatestSave()` is a game-state transition that boots the world a second time,
drops connected players, and would also hijack Workbench play and the test
server. Instead `CheckSessionContinued()` reports the missing flag loudly.

Fail-safe: a missing save under the flag is engine-handled and the session starts
fresh. A save whose EL payload cannot be read logs a `Persistence` warning per
payload and leaves that feature's live state untouched (`... failed to read,
leaving live ... untouched`).

## Save payload versions

`EL_PersistenceComponentSerializer` writes `version`, then accounts and bank, then
the append-only payload list. `tools/validation/validate-persistence-version.ps1`
enforces that the written version equals the list length and that read and write
walk the same list (`SerializeStore` / `DeserializeStore`).

| Version | Adds |
|---|---|
| 1 | survival |
| 2 | organizations |
| 3 | hideout ownership |
| 4 | owned vehicles |
| 5 | serving jail sentences (`LF_Jail.ExportAll` / `ApplyAll`) |

An older save stops at its stored version; payloads it does not contain leave the
live state untouched.

## Restart rules for session state

- Manager singletons (`EL_ATMManager`, `EL_JobManager`,
  `EL_CharacterCreationManager`) are created only by `GetInstance()`. A
  constructor never registers itself and a destructor only clears its own
  registration, so nothing can replace the cache a load filled. (The first
  connect after a restart used to replace the bank cache and wipe balances.)
- A restored out-in-world vehicle spawns without a key. The owner re-keys with
  garage store then retrieve, or buys an extra key.
- Saved plates are re-reserved in `EL_LicensePlateRegistry` on load.
- The whitelist file is outside this serializer; an unreadable file blocks
  `EL_WhitelistManager.Save()` so it is never overwritten.

## Gates (boundary discipline)

Every save path passes `PassesSaveGates()`: save manager present, saving
allowed, persistence system active. Every rejection logs an `EL_Debug` message
with the feature tag `Persistence` and a reason, and degrades that one request.
A misconfigured server gets a greppable line, never a VME.

## Debug logging contract

All state transitions log through `EL_Debug` under the `Persistence` feature:
autosave scheduled, autosave tick skipped (busy), save point requested, save
point created, save point failed, session end marked, load requested,
re-apply accepted/rejected, wipe complete, system state change. A silent no-op
is a bug.

## Tests

- `save/autosave-selection` (LOGIC): drives `EL_SaveSelection` across the
  bitmask + playthrough edge cases (combined flags, wrong playthrough, newest
  AUTO wins, manual-only list).
- `save/manager-wiring` (WORLD): on the DebugWorld test server, the manager
  component is on the game-mode entity and resolved the persistence system.
- `restart/*` (`EL_Test_RestartSafety.c`): bank cache survives the connect path
  (LOGIC), jail payload through the real serializer write and read (WORLD),
  restored vehicle has no ground key (WORLD), plates re-reserved on load (LOGIC),
  unreadable whitelist file is kept (LOGIC).
- `e2e/pass1-write-save` / `e2e/pass2-verify-load` (PERSISTENCE, two boots): also
  carry a jail sentence across the real restart.

The true restart proof (save -> shutdown -> boot -> load) stays a manual server
pass per AGENTS.md: the EL_Test runner cannot survive a game-state transition.

## Fail-safes

- No persistence system at game start (server): `EL_Debug.Error` — the world
  systems config lacks the SCR_PersistenceSystem entry. Clients and Workbench
  editor worlds (no game start) degrade silently to a no-op with
  `GetPersistenceSystem()` null.
- Save request rejected by the engine: the callback logs the failure and fires
  `GetOnSaveFinished()` with `false`; nothing throws.
- Autosave while busy: the tick is skipped, not queued.
- Session end without a shutdown save: `OnPersistenceStateChanged` reaching
  SHUTDOWN with the session-ending flag set and no SHUTDOWN result observed
  logs an `EL_Debug.Error`, so a silently lost save is impossible.
- `OnGameEnd` raised twice (dedicated-server shutdown): the once-only guard
  makes the second pass a no-op.