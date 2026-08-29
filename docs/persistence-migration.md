# EPF -> First-Party Persistence Migration Plan

Status: PLANNED. Owner: foundation-fixes worktree.

## Why

`LifeFramework.gproj` declares EPF (`5D6EBC81EB1842EF`) + EDF. EPF does not
compile against Reforger 1.8.0.10 (verified: every `EPF_*SaveData.c` errors
with "Expected attribute call"; Game module fails). The base game ships a
first-party persistence system (SCR_PersistenceSystem + SaveGameManager +
scripted serializers bound in .conf) that Overthrow already migrated to.
Removing EPF/EDF both removes the broken dependency and matches the vanilla
system. Target: `Dependencies { "58D0FB3206B6F859" }` only.

## Observable contract (what must still work after migration)

1. A player's account survives a restart: characters (id, prefab, first/last
   name, age), active character index, faction, on-duty, wanted level.
2. An ATM/bank balance survives a restart.
3. Survival stats (hunger/thirst/health) survive a restart.
4. Stacked item quantity survives a restart.
5. On connect, the player spawns at a spawn point with their active character's
   prefab and the default loadout (jacket with 100x MoneyStack, pants, boots).
6. Identity: account keyed by Steam UID; character id is a stable generated
   UUID; `EL_Utils.GetPlayerUID(entity)` returns a stable id.

## Target architecture (mirrors Overthrow)

- **Config binding** (load-bearing): `Configs/Systems/Persistence/LifeFramework.conf`
  = `PersistenceSystemConfig` inheriting vanilla `Common.conf`
  ({3A03B52D11F7C36A}). Binds:
  - game-mode entity serializers (component serializers on the game-mode prefab),
  - item entity configs with `SelfSpawn 1` + quantity serializer,
  - player character config with `SelfSpawn 1`.
- **System registration**: SCR_PersistenceSystem registered via a systems
  config override (Overthrow's ChimeraSystemsConfig pattern), `SystemLocation Server`.
- **Game-mode prefab**: native `Persistence` component replaces
  `EPF_PersistenceManagerComponent` + `EDF_JsonFileDbConnectionInfo` +
  `EPF_PersistentDoorStateManagerComponent`. A new `EL_PersistenceComponent`
  (ScriptComponent on the game-mode entity) owns the account/bank/survival
  maps; its `ScriptedComponentSerializer` writes them.
- **Spawn**: `EL_SpawnLogic : SCR_SpawnLogic` (was EPF_BaseSpawnLogic);
  `SpawnPoint_LIFE.et` becomes `SCR_SpawnPoint`.
- **Identity**: `EL_PlayerCharacter.m_sId` = `PersistenceIdUtils.Generate()`;
  account key = `SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId)`;
  `EL_Utils.GetPlayerUID` no longer reads EPF component.
- **Utils**: `EL_Utils : Managed` (reimplement GetPrefabName/SpawnEntityPrefab/
  IsInstanceAnyInherited/FindCharacterById); `EL_NetworkUtils : Managed`
  (IsOwner/GetRplId/FindEntityByRplId); delete `EL_BitFlags`.

## Gotchas (from Overthrow recipe, transfer here)

- Serializer compiled but not listed in the .conf is silently never called.
- Binary save contexts are positional: write order == read order. Version
  first (`WriteValue("version", N)`), read first, `if (version < 1) return true;`.
- Load must be idempotent (fresh load AND re-apply to a live session).
- `SelfSpawn` is the only mechanism that crosses a restart boundary.
- No console guards. `ESaveGameType` is a bitmask. `GetSaves()` is async.
- In-session round trips prove nothing; restart is ground truth.
- GUID-matched conf overrides REPLACE list entries — never partial lists.

## Phase chain (each ends verifiable)

1. **Config + prefab + gproj** (no scripts): persistence .conf, systems
   config, native Persistence on game-mode + item prefabs, SCR_SpawnPoint,
   drop EPF dep. Verify: `tools\cli validate`.
2. **Serializers + manager apply APIs**: EL_PersistenceComponent + 4 serializers
   (account, bank, survival, quantity); manager load/apply methods. Verify: compile.
3. **Spawn + identity + utils**: SCR_SpawnLogic rewrite, PersistenceIdUtils,
   EL_Utils/NetworkUtils off EPF bases, delete EL_BitFlags. Verify: compile.
4. **Strip EPF**: remove all remaining EPF_/EDF_ references, drop dependency.
   Verify: `tools\cli validate` + Workbench `-wbsilent -validate` (game module compiles).
5. **In-game**: boot DebugWorld server, exercise account/survival/bank/quantity
   path, restart round-trip.

## Validation tooling

- `tools\cli validate` (repo hygiene + script header checks).
- Workbench headless: `run-wb-validate.cmd` (or MCP `mod` build) with
  `-addonsDir` = workbench addons + game addons junction
  (`C:\Users\jaspe\Documents\Reforger\GameAddonsLink`).
- In-game: dedicated server boot + EL_Test suite + restart round-trip.

## Concrete contract (fixes the interfaces before parallel work)

### New game-mode persistence component + serializer
- `Scripts/Game/Persistence/EL_PersistenceComponent.c`:
  `class EL_PersistenceComponent : ScriptComponent`, attached to the game-mode
  entity (GameMode_Roleplay.et). Owns nothing by itself; delegates to the
  existing manager singletons.
- `Scripts/Game/Persistence/Serializers/EL_PersistenceComponentSerializer.c`:
  `class EL_PersistenceComponentSerializer : ScriptedComponentSerializer`,
  `GetTargetType() -> EL_PersistenceComponent`.
  - `Serialize`: `WriteValue("version", 1)`, then writes the account records,
    bank records, survival records by pulling from the managers. If a manager
    is null, write empty arrays.
  - `Deserialize`: version-first; `if (version < 1) return true;`; creates the
    managers if null (idempotent), applies records via the manager apply APIs.
- Manager apply APIs to add (each manager keeps its existing getters; add
  record export/import):
  - `EL_PlayerAccountManager`: `array<ref EL_PlayerAccountRecord> ExportAll()`
    / `void ApplyAll(array<ref EL_PlayerAccountRecord>)`.
  - `EL_ATMManager`: `array<ref EL_BankAccountRecord> ExportAll()` /
    `void ApplyAll(...)`.
  - survival handled through `EL_CharacterSurvivalComponent` is per-entity;
    instead persist survival as a map on the persistence component too:
    `EL_PersistenceComponent` holds `map<string, ref EL_SurvivalStats>`
    (keyed by character id) + `GetStats(id)/SetStats(id, stats)`.
- Record classes replace the EPF SaveData classes (plain Managed, no EPF base):
  - `EL_PlayerAccountRecord` (in EL_PlayerAccountSaveData.c, renamed class, no
    EPF base, `[BaseContainerProps()]`), fields mirror EL_PlayerAccount.
  - `EL_BankAccountRecord` (was EL_BankAccountSaveData).
  - `EL_SurvivalStatsRecord` (was EL_SurvivalStatsSaveData).
- Quantity: `Scripts/Game/Feature/Quantity/Persistence/EL_QuantityComponentSerializer.c`
  `class EL_QuantityComponentSerializer : ScriptedComponentSerializer`,
  `GetTargetType() -> EL_QuantityComponent`, writes `m_iQuantity` (skip when 1),
  apply keeps the one-frame transfer-intent. Bound in .conf under item configs.

### Shared naming (all agents MUST use these)
- Component: `EL_PersistenceComponent`
- Game-mode serializer: `EL_PersistenceComponentSerializer`
- Records: `EL_PlayerAccountRecord`, `EL_BankAccountRecord`, `EL_SurvivalStatsRecord`
- Quantity serializer: `EL_QuantityComponentSerializer`
- New .conf: `Configs/Systems/Persistence/LifeFramework.conf` (+ .meta, new GUID)
- Systems override: `Configs/Systems/ChimeraSystemsConfig.conf` (+ .meta)

### Manager public API (the seam agents compile against)

Keep the existing public getters so ~30 call sites do not change. Change the
internals to be synchronous and persistence-backed.

- `EL_PlayerAccountManager` (singleton, now `: Managed`):
  - `GetAccount(string playerUid)` / `GetFromCache(...)` — unchanged signatures.
  - `AddToCache(account)` / `Reset()` — unchanged.
  - REPLACE `LoadAccountAsync` + `SaveAndReleaseAccount` + `EPF_*` with:
    - `static array<ref EL_PlayerAccountRecord> ExportAll()`
    - `static void ApplyAll(notnull array<ref EL_PlayerAccountRecord> records)`
    - `static EL_PlayerAccount GetOrCreate(string playerUid)`
  - Drop `EL_PlayerAccountManagerProcessorCallback` (no more EDF).
- `EL_ATMManager` (singleton, now `: Managed`):
  - `GetAccount(accountId)` / `CreateAccount(...)` / `Deposit` / `Withdraw` — unchanged.
  - REPLACE `LoadAccountAsync` + processor callback with:
    - `static array<ref EL_BankAccountRecord> ExportAll()`
    - `static void ApplyAll(notnull array<ref EL_BankAccountRecord> records)`
    - `static EL_BankAccount GetOrCreate(accountId)`
- `EL_CharacterSurvivalComponent`:
  - Keep `Init(characterId, callback)` shape but route through
    `EL_PersistenceComponent.GetSurvivalStats(characterId)` /
    `SetSurvivalStats(characterId, stats)` instead of EPF loaders. The stats
    store lives on `EL_PersistenceComponent` (map keyed by character id).
- `EL_CharacterCreationManager`: replace the `EL_SurvivalInitCallback` EPF path
  with a synchronous read from `EL_PersistenceComponent`.

### EL_PersistenceComponent (game-mode entity)
`class EL_PersistenceComponent : ScriptComponent` with:
- `map<string, ref EL_PlayerAccount> m_mAccounts` (key = Steam UID)
- `map<string, ref EL_BankAccount> m_mBankAccounts` (key = character persistence id)
- `map<string, ref EL_SurvivalStats> m_mSurvivalStats` (key = character persistence id)
- `GetSurvivalStats(id)` / `SetSurvivalStats(id, stats)`
- The serializer (`EL_PersistenceComponentSerializer`) is bound to it in
  LifeFramework.conf; Serialize writes version 1 then the three maps via the
  managers' ExportAll; Deserialize applies via ApplyAll (idempotent, null-safe).

### Identity (critical)
- Account key = player Steam UID (`SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId)`).
- Character persistence id = `PersistenceIdUtils.Generate().ToString()` (stable UUID).
- `EL_Utils.GetPlayerUID(IEntity)` — no longer reads EPF; resolve via player
  manager / identity utils. Keep Steam-UID fallback.
- `EL_Utils.FindCharacterById` — use `SCR_PersistenceSystem` or account lookup.