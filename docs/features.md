# Life Framework — Implemented Features & Contracts

> Ground truth of what the codebase currently contains and what each system is
> supposed to do. First written from a read-only sweep of
> `addons/LifeFramework/Scripts/Game/` (2026-08-29); rows marked **verified
> 2026-08-30** were re-audited against current code during the P0 ship-blocker
> pass. Every claim carries a file path. This is the contract test cases must
> prove.
>
> **Keep this file current.** It is the doc every agent reads to know what a
> system does. Update the affected feature's rows in the SAME change that
> alters the system — a stale row reads as "broken" and wastes the next
> agent's whole session (the 2026-08-29 sweep claimed several already-fixed
> exploits were live; that gaslight is what this note prevents).
> `tools\cli validate` runs `validate-docs-sync.ps1`, which warns when feature
> code changes without this file changing.
>
> **Status legend per feature:** ✅ implemented (what "working" means), ⚠️
> known-fragile/untested contract, ❌ known bug or broken path.

## Core

### `EL_GameModeRoleplay` — `Scripts/Game/Core/EL_GameModeRoleplay.c`
- ✅ Lazy-creates `EL_CharacterCreationManager`, `EL_ATMManager`,
  `EL_JobManager` on first player connect and drives character creation.
- ✅ **World-end static reset** (2026-11-08): the game-mode
  destructor calls `LF_StaticReset.ResetAll`, which drops `LF_Breach`, `LF_Jail`,
  `LF_JailEscort`, `LF_Contraband`, `LF_PoliceRanks`, the jail cell/arrestable
  tick owners, the garage and spawn-point registries, and the quantity transfer
  intents. It is not called at game start: placed components register in
  `OnPostInit`, before the game mode starts.
- ✅ **Disconnect cleanup** (2026-11-08): server
  `OnPlayerDisconnected` releases escort holds where the player is officer or
  prisoner (`LF_JailEscort.ReleasePlayer`) and fails a running convoy, dropping
  its vehicle watch (`LF_Contraband.ReleasePlayer`). Jail sentences stay (they
  resume on reconnect). A faction accepted in stage one of character creation
  whose location never came is dropped (`EL_CharacterCreationManager.ReleasePlayer`).
- ⚠️ Managers are only created on first connect; a server-side call before any
  player connects gets null.

### `EL_SpawnLogic` — `Scripts/Game/Feature/Character/Spawning/EL_SpawnLogic.c`
- ✅ Account-aware respawn: loads `EL_PlayerAccount`, spawns the active
  character's prefab at a spawn point for the account's faction (falls back to
  any point), and applies the default loadout (recursive, purpose-filtered
  storage fill).
- ✅ `ResolveSpawnPoint` (static) picks `SCR_SpawnPoint` by account faction key;
  shared with the character-creation flow so both spawn paths land in the same
  area. Verified 2026-08-30.
- ✅ **No-spawn-point fail-safe** (verified 2026-08-30): `GetCreationPosition`
  returns `bool` and `CreateCharacter` aborts the spawn with an
  `EL_Debug.Error` instead of dropping the player at world origin.
- ✅ **Registration retry cap** (2026-11-08): a UID or account that
  does not resolve retries about once a second up to `MAX_REGISTER_RETRIES` (60),
  then logs `EL_Debug.Error` and stops. A success clears the counter.
- ⚠️ Item is silently dropped if no matching storage; loadout recursion has no
  depth guard.

### `EL_CharacterCreationManager` — `Feature/CharacterCreation/EL_CharacterCreationManager.c`
- ✅ Flow: no account → create → faction menu → spawn-location picker →
  registration form R-89 (`LF_RegistrationForm`) → spawn with an ID card.
  A returning account skips the menus and the form. `OnCharacterCreated` persists a character, inits ATM + survival
  components, spawns.
- ✅ **Server-owned on a dedicated server** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the flow runs on the server
  only. Menus open on the joining client through owner RPCs on its
  `SCR_PlayerController` (`EL_ShowFactionMenu`, `EL_ShowSpawnLocationMenu`,
  `EL_CharacterCreationController.c`); picks come back as
  `RpcAsk_EL_SelectFaction` / `RpcAsk_EL_SpawnLocation` /
  `RpcAsk_EL_CreateCharacter` and land in `ServerSelectFaction` /
  `ServerSelectSpawnLocation` / `ServerCreateCharacter`. The player is the
  controller's id, never a client value. `EL_CharacterCreationRules` re-checks
  every pick: the police whitelist (`MayJoinFaction`, now enforced server-side),
  faction already chosen, spawn location belongs to the faction, one character
  per account, names 1..32 ASCII letters with single inner spaces, hyphens,
  or apostrophes (`LF_RegistrationRules`), age 18..80. Names are not unique. A refusal toasts the reason
  key (`#EL-CC_Error_*`) and re-opens the menu. A listen host's joiners now get
  their own menus instead of the host.
- ✅ Real character prefab (verified 2026-08-30):
  `{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et`.
- ✅ (2026-11-08, M1-08) No more "Default Citizen": a chosen faction with no
  character opens the registration form over `LF_ShowRegistrationForm`
  (owner RPC). A server refusal sends the form back with the reason written on
  it. The form never picks a faction.

### `EL_FactionSelectionMenu` — `Feature/CharacterCreation/EL_FactionSelectionMenu.c`
- ✅ Buttons wired (Civilian/Police) send `EL_AskSelectFaction`; the server
  validates, then sets the account faction, saves, re-enters
  `OnPlayerConnected`. `SaveAndReleaseAccount` keeps the account cached, so the
  re-entrant flow converges instead of looping.
- ✅ `EL_CharacterCreationManager.OnPlayerConnected` keys the account by
  `playerId` UID (not the controlled entity), so the faction choice lands on
  the same account the spawn logic reads.

### `EL_ShowIDAction` — `Feature/CharacterCreation/EL_ShowIDAction.c`
- ✅ (2026-11-08) Shows the ID card the user carries (their own first, else
  the first printed one) to the player they face: the server reads the item
  and opens `LF_IdCardView` on the viewer's machine. No card: not performable,
  reason `#LF-ID_NoCard`. Facing check is `dot > 0.707` both ways within 1.5 m.
- ⏳ Proof on a dedicated server with two clients is manual (PR #93 steps).

### `EL_Utils` / `EL_FormatUtils` / `EL_Component` / `EL_ComponentData` — `Core/`
- ✅ `MaxInt`/`MinInt`, `AbbreviateNumber` (999→raw, 1000→`1.0K`,
  1e6→`M`, 1e9→`B`), typed component finders.
- ⚠️ `EL_ComponentData.Get` strips the last 5 chars of the class name
  (`XxxClass`→`Xxx`); breaks for any data class not ending in `Class`.

### `EL_InventoryUtils` (Core) / `EL_BuildingManagerComponent` / `EL_ChatManagerComponent`
- ✅ (2026-11-08) `EL_InventoryUtils.AddItem` spawns through
  `EL_Utils.SpawnEntityPrefab` (unresolved prefab logs and returns false);
  `IsStorageHierachyRoot`, `GetStorageOwner`, and `DropItem` tolerate an
  entity without `InventoryItemComponent` / a character without an inventory
  manager.
- ✅ The building preview is a local (non-replicated) entity on the owning
  client and is deleted in `OnDelete` and the destructor, so a life no longer
  leaves an orphan preview on every client. A null camera manager retries next
  frame; a preview that fails to spawn logs once and stops the frame loop.
- ✅ `EL_ChatManagerComponent` registers only `EntityEvent.INIT`; it has no
  frame handler.
- ✅ Vehicle garage and spawn-point registries remove themselves in `OnDelete`.

### `LF_RegistrationForm` — `Feature/CharacterCreation/LF_RegistrationForm.c`
- ✅ (2026-11-08) Civil registry form R-89 on `LF_UIScreen`: given name,
  surname, age, Sign. Client checks only write the error line; the server
  decides. MenuBack brings the form back unsigned ("sign before you go into
  town"). Harness: `type RegFirstName Ada` ... `click SignButton`.

### `LF_IdCardComponent` / `LF_IdCard` — `Feature/CharacterCreation/LF_IdCardComponent.c`
- ✅ (2026-11-08) `IDCard.et` carries holder character id, first name, last
  name, age (RplProps, server-issued once). Max stack 1, so cards never merge.
- ✅ `EL_SpawnLogic.m_bGrantIdAtSpawn` (default on) grants a card printed
  from the account's character record on every spawn. The record is
  persisted, so the card survives a restart through the account. A character
  already carrying their own card is refused a second; a blank carried card is
  printed instead of adding one. Turn the flag off when the arrival clerk
  (M1-18) hands the card over.

## Character & Survival

### `EL_SurvivalStats` — `Feature/Survival/EL_SurvivalStats.c`
- ✅ All stats clamped `[0,100]`. `Eat`/`Drink`/`Heal` are additive through
  clamped setters. `UpdateStats(dt)`: hunger `-dt*0.1`, thirst `-dt*0.15`,
  and if hunger or thirst `< 20`, health `-dt*0.05`. `Create(id)` starts all
  at 100.
- ⚠️ No recovery (health only ever falls); negative `dt` heals. Zero health
  hands off to the death flow below (survival does not model a down-state).

### `EL_CharacterSurvivalComponent` — `Feature/Survival/EL_CharacterSurvivalComponent.c`
- ✅ Per-frame decay driver; async-loads stats by character id; null-guards
  until loaded.
- ⚠️ No clamp on delta time (a hitch drops a big stat chunk in one frame).

### `EL_ConsumableEffectSurvival` — `Feature/Survival/EL_ConsumableEffectSurvival.c`
- ✅ Applies configured hunger/thirst/health restore on item use; only applies
  values `> 0`.

### `EL_SurvivalStatsSaveData` — `Feature/Survival/Persistence/`
- ✅ Save/apply round-trip re-clamps through setters (corrupt save sanitized).
- ⚠️ `Equals` uses exact float `==` (no epsilon).

### Death & Respawn — `Feature/Character/Spawning/`
- ✅ **Death is defined** (verified 2026-08-30): the dead body stays in the
  world with everything it carried — it is never deleted by the spawn flow and
  is only removed by a server restart or world cleanup.
- ✅ **Respawn via the death screen**: `EL_SpawnLogic.OnPlayerKilled_S` no
  longer auto-respawns; it calls `NotifyReadyForSpawn_S`, the client opens
  `EL_DeathScreen` (workspace widget), and its Respawn button runs
  `SCR_RespawnComponent.RpcAsk_EL_Respawn` → `EL_SpawnLogic.RespawnPlayer_S`
  (account-aware, re-applies the faction loadout).
- ⚠️ Body persistence until restart is default-engine behavior (no explicit
  corpse-cleanup control in this mod yet) — verify on a dedicated server.

### Carried inventory — `Feature/Character/Persistence/LF_StorageSnapshot.c`
- ✅ **Carried cash and items survive a restart** (2026-11-08, M1-04, tests
  `persist/carried-roundtrip`, `persist/carried-death-no-dupe`, and
  `e2e/pass2-verify-load` across a real two-boot restart): the whole storage
  tree (prefab, stack quantity, storage index, slot id) is the `carried` payload
  of `EL_PersistenceComponentSerializer`, appended at version 6. Keyed by
  character persistence id plus a slot name, so one owner can hold several named
  snapshots (`carried` now; police duty / civilian stash later, M1-11).
- ✅ Captured at every save point (autosave, manual, shutdown) for each living
  player and on disconnect before the character is deleted. On spawn a snapshot
  replaces the starter kit; without one the starter kit is granted.
- ✅ Apply reconciles instead of adding: a matching occupant is kept, an occupant
  no record claims is deleted, a missing record is spawned. A second apply
  spawns nothing. A prefab that no longer loads logs `EL_Debug.Error` and is
  skipped with its contents.
- ✅ Death drops what you carry: `OnPlayerKilled_S` drops the snapshot and a dead
  body is never captured, so the body keeps the items and a restart grants the
  starter kit.
- ✅ No ground copy: `LifeFramework.conf` saves a stack with its parent and no
  longer self-spawns the player character, so a pocket `MoneyStack` does not
  come back loose on the ground next to the restored one (the two-boot e2e
  counts it).
- ⚠️ Ammo count, weapon state, and placed storage (hideout stash, M1-15) are not
  in the snapshot. Attachments and magazines are kept as items.

## Account & Persistence

### `EL_PlayerAccount` — `Feature/Account/EL_PlayerAccount.c`
- ✅ Persistent per-player state: faction, on-duty flag, wanted level
  (clamped `[0,5]`), character roster. `AddCharacter`/`RemoveCharacter`,
  `GetActiveCharacter`, `SetActiveCharacter`, `IncreaseWantedLevel`.
- ⚠️ `RemoveCharacter` does not repair `m_iActiveCharacterIdx`; a stale index
  is an out-of-bounds read. `SetActiveCharacter` with a non-member sets idx
  to -1 with no guard. Fields are effectively public (workaround for BIS
  ticket T174113).

### `EL_PlayerCharacter` — `Feature/Account/EL_PlayerAccount.c`
- ✅ Immutable record (id, prefab, name, age), `GetFullName`, `Create`
  generates a persistence id.

### `EL_PlayerAccountManager` — `Feature/Account/EL_PlayerAccountManager.c`
- ✅ Async load + cache + `SaveAndReleaseAccount`. Verified 2026-08-30:
  `SaveAndReleaseAccount` now keeps the account resident, so consecutive
  wanted bumps and police-menu reads do not lose the account mid-session
  (the old evict-on-write churn is gone).

### `EL_PlayerAccountRecord` — `Feature/Account/Persistence/` (verified 2026-11-07)
- ✅ Explicit persisted form of an account and its characters (faction,
  duty, wanted, roster, active index). `EL_PlayerAccountManager.ExportAll` /
  `ApplyAll` convert it; `EL_PersistenceComponentSerializer` writes it after
  its `version` field. `account/save-roundtrip` covers the
  export → reset → apply path. EPF is gone; there is no `ApplyTo`.

### `EL_PersistenceManagerComponent` — `Persistence/` (verified 2026-08-30)
- ✅ Server-side save orchestration on the game-mode entity: autosave timer
  (one AUTO slot per playthrough via `EL_SaveSelection`), manual saves,
  save-cache scan, load/re-apply paths, wipe. Every path passes
  `PassesSaveGates()` (manager present, saving allowed, persistence system
  active) and logs `EL_Debug` under `Persistence`.
- ✅ Shutdown save is engine-owned: the engine requests the SHUTDOWN save point
  on graceful exit; the manager observes the result via `OnAfterSave` and logs
  an `EL_Debug.Error` if a session ends with no shutdown save. The old manual
  `RequestSavePoint(SHUTDOWN)` from `OnGameEnd` always lost the race against
  the persistence teardown on the Workbench (fixed 2026-08-30).
- ✅ Persistence system resolution is stable across reloads: the systems config
  is now `Configs/Systems/LifeFrameworkSystems.conf` (unique GUID, referenced
  explicitly from both missions), replacing the GUID-colliding
  `ChimeraSystemsConfig.conf` override. Resolution retries at game start and
  logs an `EL_Debug.Error` when the entry is truly missing.
- ✅ **Continuing a saved world needs `-loadSessionSave`** (verified 2026-11-07 on
  the dedicated test server: fresh, `-loadSessionSave`, and kept-saves-without-flag
  boots each logged their line): the engine loads the latest save point at launch only with
  that flag (vanilla behavior). Script does not auto-load: `LoadLatestSave()` is a
  game-state transition that boots the world twice and drops connected players.
  At game start on a dedicated server that was not launched from a save, the
  manager scans the mission's saves and logs `EL_Debug.Error` ("started a NEW
  session while N save point(s) exist") when any exist; with none it logs a
  fresh-session info line. See `docs/persistence.md`.
- ✅ **Playtest host** (`toolscli host start|stop|status|backup`, `tools/host/host.mjs`;
  verified 2026-11-08 on the dedicated server): `start` runs `LF_PlaytestTown.conf`
  from a profile outside every worktree (`%LOCALAPPDATA%LifeFrameworkhost`,
  `LF_HOST_DIR`), passes `-loadSessionSave` when a save exists, and backs the saves
  up first. `stop` writes `$profile:lf-host-stop`; `LF_HostStop` closes the game
  normally so the engine writes the SHUTDOWN save, then the CLI backs the saves up.
  The server owns the machine-wide `server-2001` lock while it runs. The
  no-shutdown-save error now fires at teardown, not on the SHUTDOWN state change
  that comes before the save. Selftest: `tools/validation/host-selftest.mjs`.

### `EL_AccountReplication` — `Feature/Account/EL_AccountReplication.c`
- ✅ **Client copy of the account** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): `EL_PlayerAccountManager` is
  server-only, so client-side offers read a replicated copy. Faction, duty,
  wanted, and police rank pack into one int (`EL_AccountStateBits`) carried by
  an `[RplProp]` on the player's `SCR_ChimeraCharacter`; the on-duty police
  count is an `[RplProp]` on `EL_GameModeRoleplay`. The server writes both
  from the account setters (`SetFaction`/`SetOnDuty`/`SetWantedLevel`/
  `SetPoliceRank`), on `OnPlayerSpawned`, and on a 10 s self-healing pass;
  `BumpMe` only on change. Late joiners get the values with the snapshot.
  `GetState(entity)` answers from the account on the server and from the
  replica on a client. Consumers: `EL_RobAction`, `EL_DutyAction`,
  `EL_ConfiscateAction`, `EL_OpenPoliceMenuAction`, `LF_ArmoryDrawAction`,
  `LF_RankPromoteAction`, `LF_BreachRepairAction`. Every server path still
  re-checks the real account.
- ⚠️ `EL_RobWeaponAction`, `EL_RobVehicleAction`, and `LF_Bank` still count
  police and read accounts directly in their offers; move them to
  `EL_AccountReplication` next.

## Money & Economy

### `EL_MoneyUtils` — `Feature/Money/EL_MoneyUtils.c`
- ✅ Cash is the `MoneyStack` inventory item. `GetCash` (‑1 on fault),
  `AddCash`/`RemoveCash` (actual amount, 0 on fault).
- ⚠️ Inconsistent sentinels (`-1` vs `0`); doc comments wrong on
  Remove/Take.

### `EL_BankAccount` + `EL_ATMManager` — `Feature/ATM/`
- ✅ **The canonical bank** (verified 2026-08-30): every payout in the game pays
  cash; `EL_ATMManager` + `EL_BankAccount` (per-player, keyed by UID, persisted)
  is the only bank, and the ATM is the only cash↔account boundary.
  `Deposit` only if `>0`, `Withdraw` only if sufficient; balance never negative
  through these paths. Manager is a session map registry + async loader.
- ✅ `SetBalance` clamps corrupt negative persistence values, and
  `CreateAccount` is idempotent and rejects empty IDs (verified 2026-08-30).
- ✅ **No bank wipe on the first connect after a restart** (2026-11-07, test
  `restart/bank-survives-connect`, verified 2026-11-07): the game mode used to
  `new EL_ATMManager()` on the first connect, and the constructor stole the
  singleton the save load had filled. `EL_ATMManager`, `EL_JobManager`, and
  `EL_CharacterCreationManager` now share one pattern: `static ref s_Instance`,
  created only by `GetInstance()`, the constructor never registers itself, and
  the destructor only clears the singleton when it is that instance. The game
  mode keeps no manager fields; `EL_GameModeRoleplay.ResolveConnectManagers()`
  only resolves them.

### `EL_ATMMenu` — `Feature/ATM/EL_ATMMenu.c`
- ✅ **Cash moves** (verified 2026-08-30): the menu forwards through
  `EL_CharacterATMComponent.RequestDeposit/RequestWithdraw`, and the server
  RPC handlers `RpcAsk_Deposit`/`RpcAsk_Withdraw` remove cash on deposit
  (partial removals rolled back) and pay cash out on withdraw (all-or-nothing
  with balance restore), guarded by `EL_ATMManager.IsValidAmount`. No money
  from thin air.
- ✅ The money-moving bodies are the static seams
  `EL_CharacterATMComponent.ServeDeposit/ServeWithdraw` (extracted 2026-11-07,
  no behavior change; the RPC handlers keep the range check and reply);
  `atm/request-cash-boundary` (WORLD) drives them with a real character
  (red-proof observed 2026-11-08). The withdraw rollback case pays into an
  entity with no inventory: a bare `Character_Roleplay` still holds MoneyStack.
- ✅ **At the ATM only** (2026-11-07): the server refuses a
  deposit or withdraw unless an entity carrying `EL_ATMAction` stands within
  `EL_CharacterATMComponent.ATM_RANGE` (5 m) of the character
  (`EL_ActionSite`, server-side search; the client names no site). A refusal
  logs `deposit refused: not at an ATM` / `withdraw refused: ...` and shows
  `#EL-ATM_TooFar`. On a listen host the requests and the replies call their
  handlers directly, like the shop bridge. Test: `security/service-range`.

### `EL_BankAccountComponent` — `Feature/Banking/` (deleted 2026-08-30)
- ❌ The second, unintegrated entity bank is **gone** (file deleted, no
  references remain). The $20k grant, the persistence race, and the duplicate
  money path died with it.

### `EL_ShopComponent` — `Feature/Shop/EL_ShopComponent.c`
- ✅ Catalog-driven buy: `BuyItem(item, qty, buyer)` guards null/qty, server-
  only, price × quantity, `EL_MoneyUtils.RemoveAmount`, refund on
  inventory-full. A sell or buy of a stack pays or gives one unit per
  quantity through `EL_ShopRules.StackTradeTotal` (verified 2026-10-11).
  The menu sells at most 99 of a held stack per click, the server cap
  `EL_CharacterShopComponent.MAX_TRANSACTION_QUANTITY`; a request outside
  1..99 is refused and logged. (2026-11-07)
- ✅ **Money correctness** (verified 2026-08-30): the partial-funds exploit is
  closed — `BuyItem` compares the actual removed amount against the total and
  refunds only what was removed before failing (a $50 buyer cannot buy a $100
  item), and a partial delivery refunds only the undelivered quantity.
  `SellItem` pays cash only after the goods are confirmed removable and
  claws back overpayments. `IsItemSoldHere` gates buy and sell.
- ⚠️ No shop stock (infinite supply). Client→server bridge
  (`EL_CharacterShopComponent.RpcAsk_Buy/Sell`) does not enforce a max
  quantity per item.

### `EL_ShopMenu` / `EL_ShopAction` — `Feature/Shop/`
- ✅ List UI; action opens menu within 3 m and counts items across managed nested storage.
- ⚠️ Menu always opens for the local player regardless of who interacted.

### `EL_TraderManagerComponent` + `EL_InventoryStorageManagerComponent` — `Components/InventorySystem/`
- ✅ Sell-to-trader pays **cash** (verified 2026-10-11): inserting into a
  trader-owned storage pays one cash unit per stack quantity
  (`EL_ShopRules.StackTradeTotal`) via `EL_MoneyUtils.AddCash`
  (pay-before-delete, clawed back if the item cannot be removed); black-market
  rejects non-civilian factions. The old "payout to ATM bank" and the
  `Print("testtesttest")` debug line are gone.
- ✅ The value lookup and the black-market decision are the static seams
  `EL_TraderManagerComponent.GetUnitValue` / `CanAccessBlackMarket`
  (extracted 2026-11-07; the lookup now skips a null row instead of crashing),
  pinned by `trader/sell-rules` (LOGIC; red-proof observed 2026-11-08).
- ⚠️ Unguarded `pStorageFrom.GetOwner()` (null crash); null `m_aTradableItems`
  foreach crash; delete-then-pay not atomic; `RETCODE_ITEM_TOO_BIG` used for
  "not tradable"; black-market check fails closed but is only cache-keyed.

## Gathering / Processing / Resources

### `EL_GatherAction` — `Feature/Gathering/EL_GatherAction.c`
- ✅ Interact action granting `m_GatherAmount` of `m_GatherItemPrefab` into
  the user inventory; tool requirement (right hand / left gadget / anywhere
  in inventory); per-resource gather counter + cooldown restock.
- ✅ Server re-validates tool, crop stage, cooldown, and malformed item/amount
  configuration in `PerformAction` (verified 2026-08-30). Remaining: timeout
  attribute says "ms" but is compared as seconds-compatible — fragile.
- ✅ The server applies a gather when no peer owns the prop, and a grant that
  adds nothing is an error log (verified 2026-10-10). A sandbag the inventory
  rejects, or a nugget with no usable slot, used to look like a successful use.
- ✅ A node id copies tool, yield, and cooldown from `ResourceChains.conf`.
  A missing row or a prefab that does not resolve logs `EL_Debug.Error` and
  grants nothing (verified 2026-10-11). The action stays on the node.

### `EL_ProcessAction` — `Feature/Processing/EL_ProcessAction.c`
- ✅ Recipe: consume all `m_aProcessingInputs`, produce all
  `m_aProcessingOutputs` (into inventory or force-dropped).
- ✅ **Free-output exploit closed** (verified 2026-08-30): malformed recipes,
  empty input/output lists, missing inputs, and non-positive amounts are
  rejected before inventory mutation. Regression coverage is in
  `EL_Test_ActionGuards`.
- ✅ A machine id copies inputs and outputs from `ResourceChains.conf`. An
  empty recipe or a prefab that does not resolve logs `EL_Debug.Error` and
  returns before inventory changes (verified 2026-10-11).
- ✅ Cocaine, jam, and beer are one-input, one-output recipe prefabs under
  `Prefabs/Processing/Chains/`, each parented to `ItemProcessor_Base`
  (verified 2026-10-11). Cocaine consumes coca leaves and produces a cocaine
  pouch. Jam consumes rowan berries and produces berry jam. Beer consumes an
  empty beer bottle and produces a beer bottle. `EL_Test_RecipeChains` checks
  that each input prefab and output prefab is set and distinct.

### `EL_ResourceChainCatalog` — `Feature/Resources/EL_ResourceChainCatalog.c`
- ✅ `Configs/Resources/ResourceChains.conf` names each gather node's tool,
  yield, and cooldown, and each machine's inputs and outputs (verified
  2026-10-11). Yields are item prefabs. Gather and process do not mint cash.
  The playable iron, copper, and gold rows name the pickaxe. The log row does
  not: the axe stays on `CuttableTree_Base`, and `LogPile_Debug` grants a log
  with no held tool so the sawmill still runs before the axe pickup.
- ✅ MainWorld's St Philippe farm (`Area_StPhillipe_AppleFarm.layer`) places
  the apple trees, the juicer, and a juice trader (15 cash, matching the
  DebugWorld juice crate), plus iron ore, a pickaxe, the furnace, and an
  iron-bar trader (45 cash, the ShopPrices iron-bar sell price). That iron
  node requires the pickaxe in hand. DebugWorld keeps Apple, Plum, Tomato,
  Logging, Mining, Cement, Gravel, and Sand. `IronOre_Debug` uses
  `iron-debug` (4 nuggets, no held tool) so one gather still feeds both
  furnaces. `GoldOre_Debug` and `CopperOre_Debug` use the tool-free debug
  rows with the prefab yields.

### `EL_DestructibleResourceComponent` + `EL_DestructibleResourceHitZone` — `Feature/Resources/`
- ✅ Damage gated to configured tools; tool → per-hit damage; FX on hit.
- ⚠️ Non-tool or unmapped damage source deals **0 damage silently** (no
  feedback); damage-source prefab string comparison is fragile on casing.

### `LF_MiningPhases` + `LF_MiningSpawn` + `LF_MiningForge` — `Feature/Mining/`
- ✅ Ore health phases (verified 2026-10-11). A strike counts only when its
  damage equals the tool damage. Non-negative damage walks phase budgets in
  order; a budget of zero stops the walk. Passing the last phase destroys the
  ore when delete-after-final is set. Applying a phase sets health to that
  phase's budget and swaps the mesh when the phase names one. An empty mesh
  keeps the current mesh. No phase data restores the original health and the
  original mesh. Ported from EveronLife `EL_DestructionMultiPhaseSingleComponent`
  and `EL_ToolHitZone`. Logic test `mining/phases`.
- ✅ Weighted ore spawn. A roll in `[0, total)` maps onto the first bin, including
  a roll of 0. The spawn count is the shortfall to the configured amount, and
  an over-full area spawns nothing. An empty list logs `EL_Debug.Error`.
- ✅ Timed forge. One iron nugget
  (`{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et`) becomes one iron
  bar (`{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et`) after 5 seconds
  (5000 ms). The nugget stays until the delay elapses. A missing nugget, a
  second start, or a nugget removed before the end logs `EL_Debug.Error` and
  yields no bar. A zero delay finishes inside the start.

## Jobs / Level / Licenses / Whitelist

### `EL_PlayerJobComponent` — `Feature/Jobs/EL_PlayerJobComponent.c`
- ✅ Per-player job state, per-job level/XP maps, license gates (POLICE→POLICE_ACCESS,
  MEDIC→MEDIC_ACCESS), client→server `RpcAsk_SetJob` re-validates licenses.
- ✅ **Fruit-catcher mint closed** (verified 2026-08-30): the server clamps any
  claimed score through `EL_GetFruitCatcherRewardCount` to
  `EL_FRUIT_CATCHER_MAX_SCORE` (100); pin test `security/fruitcatcher-reward-clamp`.
- ✅ **Fruit-catcher claim loop closed** (verified 2026-11-07): a claim
  pays only a server-opened round. The minigame calls `AskStartFruitCatcherRound`
  when a round starts; the server records the round id and start time
  (`LF_FruitCatcherRound`) and `AskClaimFruitCatcherReward` sends that id back.
  One claim per round, same fruit, at least `LF_FruitCatcherRound.MIN_ROUND_SECONDS`
  (30 s) after the start, and the caller must hold FARMER. The minigame screen
  is not in this repo; when it lands it must call the start ask. Pin tests
  `security/fruitcatcher-rounds`, `security/job-workplace-and-seed`.
- ✅ **Job switch and kit gates** (verified 2026-11-07): a
  client-requested switch (`RequestJobFromClient`, behind `RpcAsk_SetJob`) needs
  an `LF_JobSignUpAction` for that job within `LF_JobGuards.WORKPLACE_RADIUS`
  (5 m) and `SWITCH_COOLDOWN_SECONDS` (30 s) since the last switch; going
  UNEMPLOYED needs no workplace. Only delivery jobs have a placed workplace today,
  so a client cannot switch itself to MINER/LUMBERJACK/FARMER/MEDIC/POLICE by RPC.
  `GiveJobEquipment` hands each job's kit once per account per server session
  (`LF_JobGuards.ClaimEquipmentGrant`). Pin test `security/job-switch-guards`.
- ✅ **Debug-license backdoor removed** (verified 2026-08-30): `RpcAsk_DebugGrantLicense`
  no longer exists anywhere in `Scripts/Game`.
- ✅ **No passive paycheck** (verified 2026-11-08, M1-07): the timer paycheck
  (700-1000 cash every 300 s to every job, unemployed included) is deleted; the
  component has no frame event. `EL_JobConfig` has no salary fields. Money comes
  from completed runs, trader sales, police duty pay (`LF_DutyPay`), and the
  unemployment benefit (`LF_UnemploymentBenefit`). Pin test `jobs/no-passive-paycheck`.
- ✅ **Civilian run XP** (verified 2026-10-11): `AddExperience` forwards to
  `LF_CivilianJob`. A completed courier, fuel, or taxi run adds 100 XP.
  100 XP levels that career and `PullCareer` copies the level onto this
  component, so `OnLevelUp` fires. Level 1 may take courier work, level 2
  opens fuel delivery, level 3 opens taxi. Under-level signup refuses and is
  logged.
- ✅ **Job tool grant** (2026-11-08): miner gets
  `{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et` (the old `Prefabs/Tools/Axe/Pico.et`
  never existed), lumberjack gets `{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et`
  (the old GUID was wrong). Spawn goes through `EL_Utils.SpawnEntityPrefab`; a
  tool that does not fit is logged and left at the player's feet.
- ⚠️ Whitelist branch in `SetJob` is dead (only POLICE is restricted and it is
  special-cased). Hard-coded Spanish strings (not localized). Per-job level/XP
  persistence is convention-only, no SaveData pair.

### `LF_CivilianJob` — `Feature/Jobs/LF_CivilianJob.c`
- ✅ Workplace signup, one active run, cash once, abandon pays nothing
  (verified 2026-10-11). The bank balance stays 0. Parcel cargo is
  `{6BF776E22ECF67E7}Prefabs/Items/Core/HandCarryItem_Base.et`. Fuel cargo is
  `{12D5AD21E383B768}Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et` and
  the drop consumes it. The taxi fare uses the rank vehicle
  `{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et` and does
  not need a second client. A missing cargo prefab or a full inventory logs
  `EL_Debug.Error` and that run does not pay. DebugWorld places
  `Roadmap_Job_Depot` (post counter
  `{28F9F4DC40089B51}Prefabs/Props/Services/Post/CounterPostOffice_01.et`),
  `Roadmap_Job_Drop`, `Roadmap_Job_TaxiPad`, and `Roadmap_Job_FuelCan` on
  `Street_Jobs.layer`.

### `EL_JobManager` — `Feature/Jobs/EL_JobManager.c`
- ✅ `GetGatherReward` and `GetProcessReward` stay 0 unless a job scope calls
  `SetScopeReward` for that prefab (verified 2026-10-11). Gather and process
  mint nothing on their own. The player sells the product to a trader. A
  scope payout, when one exists, is cash via `EL_MoneyUtils.GiveCash`. The
  ATM stays the only cash-to-account boundary.
- ✅ `#EL-Job_Earned` is `Earned $%1 for work!` and `GiveReward` passes the
  paid amount through `WidgetManager.Translate` (verified 2026-10-11).

### `EL_PlayerLevelComponent` — `Feature/Level/EL_PlayerLevelComponent.c`
- ✅ Level/XP/skill-point state; `GetExperienceForNextLevel = level * 100`;
  passive bonuses (`gatheringSpeed 1+(level-1)*0.1`, `gatheringAmount` same,
  `saleBonus 1+(level-1)*0.05`); `CheckLevelUp` drains XP cascades, +1 SP and
  +1 total SP per level.
- ⚠️ `SetLevel`/`SetExperience`/`SetSkillPoints`/`SetTotalSkillPointsEarned`
  have no server guard, no validation, no recompute (inconsistent state
  possible). Saves on every XP gain (DB-write hazard). `GetBonusesText`
  returns a Spanish literal.

### `EL_LicenseManagerComponent` — `Feature/License/EL_LicenseManagerComponent.c`
- ✅ License catalog (26 entries), `CanUnlockLicense` (owned/config/whitelist/
  level), `CanAffordLicense` (SP cost), `PurchaseLicense` (spends SP +
  unlock), `HasLicense`, persistence restore.
- ✅ **Whitelist mapping and purchase ordering fixed** (verified 2026-08-30):
  `GetLicenseWhitelistJob` maps MEDIC access to MEDIC and police licenses to
  POLICE; `PurchaseLicense` validates requirements before `UnlockLicense`
  spends skill points. `SetUnlockedLicenses` is the persistence restore seam,
  not a player purchase path. Remaining: not localized; `m_bIsInitialLicense`
  is used for initial grants.
- ✅ **Replicates as a bitmask** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the old `[RplProp] ref array` broke the
  simple-types rule. The server list stays local; `m_iUnlockedBits` (an int
  `RplProp`, `EL_LicenseBits`) is written on every change. On a client
  `HasLicense` and `GetUnlockedLicenses` decode the mask. Bit order is
  `EL_LicenseBits.Order()`: append new types, never reorder.

### `EL_WhitelistManager` — `Feature/Whitelist/EL_WhitelistManager.c`
- ✅ Pure static in-memory whitelists; `IsFactionRestricted` (POLICE,
  MILITARY, MAFIA), `IsJobRestricted` (POLICE only), add/remove/get.
- ✅ **Compiles** (verified 2026-08-30): `enum EL_EFactionType` is now defined
  in this file (CIVILIAN/POLICE/MILITARY/MAFIA).
- ✅ **Restart save and police gate** (verified 2026-10-11): the manager
  writes `$profile:LifeFramework/whitelist.txt` itself, version first, and
  `Load` replaces the cache. A second load does not duplicate entries. A
  missing file logs `EL_Debug.Error` and leaves the police faction closed.
  Civilian stays open. `MayJoinFaction` refuses a uid that is not on the
  police list and logs that refusal. The server calls it in
  `EL_CharacterCreationRules.RefuseFaction` when the faction RPC arrives; the
  client menu no longer checks it (a client has no whitelist). A uid
  that was not in the file is still refused after load. This file is not
  `EL_PersistenceComponentSerializer`.
- ✅ **An unreadable file is never overwritten** (2026-11-07, test
  `restart/whitelist-unreadable-file-kept`, verified 2026-11-07): a file that exists
  but cannot be opened, has no or a wrong version, or fails to parse sets
  `IsLoadFailed()`. `Save()` then refuses with `EL_Debug.Error` and returns false,
  so the next admin add cannot rewrite the file with only the new entry. A
  missing file is a first run and saving stays allowed. A later good `Load`
  clears the flag.
- ⚠️ Getter returns the live internal array (callers can mutate without logging).

### `AdminChatChannel` — `Feature/Chat/AdminChatChannel.c`
- ✅ **Server re-checks the admin role** (verified 2026-11-07):
  `ProcessMessage` on the authority refuses any sender without
  `EPlayerRole.ADMINISTRATOR` (`IsAdminSender`) before parsing `!wadd`, so a
  modded client cannot whitelist itself for POLICE. Pin test
  `security/admin-chat-role`.

### `EL_BuildingManagerComponent` — `Components/BuildingSystem/EL_BuildingManagerComponent.c`
- ✅ **Placement gated and paid** (verified 2026-11-07): the server
  (`PlaceHeldBuilding`, behind `RpcAsk_SpawnEntityPrefab`) requires the held
  performer for that prefab, a position within `MAX_PLACEMENT_DISTANCE` (5 m)
  of the character, and consumes the held item (`ConsumeBuildingItem`: one off
  a quantity stack, else the item is deleted) before spawning. The client
  preview stops offering placements past that range. Pin tests
  `security/building-placement-range`, `security/job-workplace-and-seed`.

### `LF_UnemploymentBenefit` — `Feature/Jobs/LF_UnemploymentBenefit.c`
- ✅ "Collect benefit" (`LF_CollectBenefitAction`) at `Roadmap_Job_Depot`
  (verified 2026-11-08: LOGIC test `jobs/unemployment-benefit`; in-world collection
  is manual). The server reads the bank balance (`EL_ATMManager`) and pays
  `m_iBenefitAmount` cash when the balance is strictly below
  `m_iBenefitBalanceThreshold`, the player is not on duty as police and has no
  active delivery run, and `m_fBenefitCooldownSeconds` passed since that
  account's last collection. Values in `Configs/Jobs/LF_Jobs.conf` (flagship
  $100, below $500, 1200 s). Pick-up sound, then a `#EL-Benefit_Paid` toast; a
  refusal toasts its reason. A full inventory pays nothing and does not start
  the cooldown.
- ⚠️ The cooldown ledger is session state; a restart lets a broke player collect
  once more.

## Crime & Police

### `LF_Breach` — `Feature/Crime/LF_Breach.c`
- ✅ Configured charge opens an `EL_LockComponent` after the fuse and leaves it
  open until repair (verified 2026-10-11). `Arm(targetId, playerUid)` is the
  call later robberies use. Fuse 0 opens in that call. Wanted and the police
  notify fire once, at arm time, not when the offer refreshes. A second charge
  on an open target refuses. Police can always repair; the owning organization
  can repair its own door. `Export` / `Apply` put the open lock back in memory
  but are not wired into the save serializer, so a restart loses it. The
  vanilla charge is not detonated. DebugWorld places `Roadmap_Breach_Door` and
  `Roadmap_Breach_Charge` on `Street_Breach.layer`. The charge prefab is
  `{33CBDE73AB48172A}Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et`.
  **Charge consumed** (verified 2026-11-07): `Arm(targetId, playerUid,
  chargeCarrier)` takes one charge from `chargeCarrier` before the wanted bump and
  refuses (`arm refused: ... carries no breach charge`) when there is none. The
  door, bank vault, and hospital cabinet actions pass the acting player; the
  door action shows `#EL-Breach_NeedCharge`. A null carrier is the
  server-scripted arm used by tests. Pin test `security/breach-needs-charge`.
  `EL_Door_Base` inherits
  `{776E0766BBDB17A3}Prefabs/Structures/BuildingParts/Doors/Door_HouseWooden_E_A_LEFT_EXT_COV_B.et`.
- ✅ **Client offers** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the target table is server-only, so
  `LF_BreachTargetComponent` mirrors open/armed as `RplProp`s
  (`SetReplicatedState`, written from `Open`/`Repair`/`Apply` and the fuse
  schedule). `CanArmForOffer` / `CanRepairForOffer` ask `LF_Breach` on the
  server and the mirror on a client. A client offers repair to police and, on an
  organization door, to anyone; the server decides membership.
- ✅ **Charge lifetime** (2026-11-08): a charge whose fuse cannot
  fire is dropped with `EL_Debug.Error` (`CancelCharge`) instead of leaving the
  target armed forever: no world, a placed door without
  `LF_BreachTargetComponent`, a timer that could not be scheduled, or the door
  deleted before the fuse fired (`LF_BreachTargetComponent.OnDelete` also
  removes the callqueue entry). The cancel also clears the replicated armed
  mirror. An unplaced, script-driven target keeps its charge and opens through
  `Advance`. `Configure` clears the armed flag and elapsed time; the open flag
  is kept. Test `robustness/lifetime`.

### `LF_Bank` — `Feature/Crime/LF_Bank.c`
- ✅ A vault calls `LF_Breach.Arm`, then spills a config number of bags of
  MoneyStack cash (verified 2026-10-11). The haul never deposits or withdraws
  `EL_BankAccount`. Civilian-only, cooldown, and minimum police on duty use
  the same comparisons as `EL_RobAction`. A second breach inside the cooldown
  spawns nothing. Over-capacity requests are cut to the configured bag count.
  Carry leaves the cash on the bag; dropping it leaves that cash in the world.
  `Export` / `Apply` restore the ground bags and do not write a bank balance;
  they are not wired into the save serializer, so a restart loses the bags.
  DebugWorld places `Roadmap_Bank_Vault` at 189 0 120 and `Roadmap_Bank_Bag`
  at 189 0 122 on `Street_Bank.layer`. The bag is
  `{6BF776E22ECF67E7}Prefabs/Items/Core/HandCarryItem_Base.et` with the crate
  mesh `{55991E1823C60AB3}Assets/Props/Crates/Crate_01.xob`. Cash is
  `{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et`. The shell is
  `{D1859197E1AB4E8A}Prefabs/Structures/Services/EL_Bank.et`, which inherits
  `{363A616E6C64BB7F}Prefabs/Structures/Services/Government/MunicipalOffice_E_01/MunicipalOffice_E_01.et`.
  That building is capture-only and is not placed in the debug row.

### `LF_Hospital` — `Feature/Crime/LF_Hospital.c`
- ✅ Hospital till pays `{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et`
  through the civilian, cooldown, and minimum-police gates and does not deposit
  to the bank (verified 2026-10-11). The placed action is `EL_RobAction` on the
  till's single `ActionsManagerComponent`. Wanted rises by 1. One police alert per
  successful rob; an offer refresh does not alert. DebugWorld places
  `Roadmap_Hospital_Till` at `195 0 120` on `Street_Hospital.layer`.
- ✅ The cabinet is a forced-entry container. `BreachCabinet` calls
  `LF_Breach.Arm`. When that lock opens it spawns the configured medicine
  prefabs: field dressing, morphine, saline, and tourniquet. A missing prefab
  logs and that line is skipped. A second breach while the cabinet is open
  spawns nothing and does not alert again. `Roadmap_Hospital_Cabinet` is at
  `198 0 120`. Items stay ordinary; confiscation of a contraband flag belongs
  to the contraband scope.

### `EL_RobAction` / `EL_RobWeaponAction` / `EL_RobVehicleAction` — `Feature/Crime/`
- ✅ Cash/weapon/vehicle robbery; cooldown + civilian-only + min-police-on-duty
  gates; raises wanted level (+1/+2/+3); notifies police. DebugWorld's three
  points set the minimum to 0 so one civilian can run them (verified 2026-10-10).
  The action classes still default to 2 / 1 / 3.
- ✅ **Cash payout + reliable wanted bump** (verified 2026-08-30): the haul is
  `EL_MoneyUtils.AddCash` (never the bank — pin test `crime/rob-reward`
  asserts the bank balance stays 0), and the account cache churn is fixed so
  every robbery escalates wanted.
- ✅ **Fuel-station till** (verified 2026-10-11): `Roadmap_Fuel_Till` on
  `Street_Fuel.layer` at `201 0 120` is
  `{4E4E1620C6B02E1F}Prefabs/Props/Commercial/EL_FuelTill.et`, which inherits
  `{51D31AB07AE3C1C7}Prefabs/Props/Commercial/CashierShop_01.et`,
  with `EL_RobAction`. Payout is a config range (`m_iMinPayout`..`m_iMaxPayout`);
  the server rolls it, and min equal to max pays that fixed amount. The debug
  till pays 150 MoneyStack cash. The bank balance stays 0. Wanted rises by
  `m_iWantedIncrease` (default 1) and still clamps at 5. A second attempt
  inside the cooldown is refused and logged with `EL_Debug.Error`. The police
  alert fires once, inside `CommitRobbery`, not while counting on-duty police
  and not when the offer refreshes. The fuel station building
  `{9269BD1B6BF3A441}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/FuelStation_E_01_V1.et`
  is the capture prefab and is not placed in the row. Player strings:
  `#EL-Fuel_Till_Rob`, `#EL-Rob_Cooldown`, `#EL-Rob_CiviliansOnly`,
  `#EL-Rob_NeedPolice`.
- ✅ **Tests call the real actions** (2026-11-07; red-proofs observed
  2026-11-08). `crime/rob-reward` (WORLD) runs `EL_RobAction.CommitRobbery` with a
  real character: MoneyStack cash, bank 0, wanted +1, cooldown, civilian gate.
  The weapon and vehicle grant/spawn, wanted, and alert moved into
  `EL_RobWeaponAction.CommitWeaponRobbery` and
  `EL_RobVehicleAction.CommitVehicleRobbery` (wanted `WANTED_INCREASE` 2 / 3;
  `PerformAction` keeps the server-only guard), and their civilian check into
  `EL_RobAction.IsFactionAllowedToRob`; `crime/rob-weapon-vehicle` (WORLD)
  drives them. The earlier `crime/rob-reward` and `crime/rob-wanted-clamp`
  never called an action and are gone; the clamp is pinned by `roadmap/fuel`.
- ✅ **No account, no robbery** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): `CommitRobbery` refuses and logs
  `robbery refused: no account` instead of paying without the civilian gate and
  wanted bump. `PerformAction` runs on the server only; offers read
  `EL_AccountReplication`, so the till is offered on a dedicated client and the
  min-police gate uses the replicated count. The success toast goes to the robber
  only (`EL_Utils.NotifyEntity`), the alert to on-duty police only.
- ✅ **Weapon/vehicle cooldown in seconds** (2026-11-08, test `robustness/lifetime`): both
  compare through `LF_RobCooldown.IsActive`, which converts the seconds attribute
  to world-time milliseconds (before, 300 s was 0.3 s). A never-robbed action
  has no cooldown. `PerformAction` grants/spawns, raises wanted, and alerts on
  the server only. `EL_RobVehicleAction` no longer notifies from its on-duty
  count (that ran every offer refresh) and refuses an empty vehicle prefab with
  `EL_Debug.Error`; the spawn goes through `EL_Utils.SpawnEntityPrefab`.
- ⚠️ Near-duplicated guard logic across all three.
  `EL_RobWeaponAction.AlertPolice` hardcodes "ALERTA POLICIAL".

### `LF_Jail` — `Feature/Police/LF_Jail.c`
- ✅ Placed cell and timed sentence (verified 2026-10-11). `GetJailPosition()`
  reads the cell origin. Sentence length is wanted level times the config
  step, clamped to the config max. Arrest of a clean player refuses. Expiry
  reduces wanted by the configured amount. Early release refuses. A locked
  jail door returns a prisoner who leaves the volume; an open door (the shared
  `LF_Breach.Arm` on `Roadmap_Jail_Door`) does not, and the remaining time
  stays. `Export` / `Apply` put the remaining seconds back, and `ExportAll` /
  `ApplyAll` carry every serving sentence through the save. DebugWorld places
  `Roadmap_Jail_Door`, `Roadmap_Jail_Cell`, and `Roadmap_Police_Desk` on
  `Street_Jail.layer` (the desk on `Street_Police.layer`). The placed door is
  `{787617D5B683FE41}Prefabs/Structures/BuildingParts/Doors/EL_JailDoor.et`, which
  inherits
  `{B597177F09247CE9}Prefabs/Structures/BuildingParts/Doors/Door_PrisonCell_E_A_Left_INT_ST_B.et`
  and adds `EL_LockComponent`, so the prison mesh and the shared breach actions
  stay together. That vanilla door is the prefab the world test loads.
  The officer action `LF_JailArrestAction` is on the suspect. `LF_JailEscort`
  is the vehicle path: an on-duty officer loads a wanted player into the nearest
  free non-driver seat of a base-game vehicle. The sentence starts when they
  arrive at the cell, not when they enter the car. A player who is not wanted
  is refused and logged. Early release stays refused. A reconnect or a
  restart keeps the remaining sentence. DebugWorld places
  `Roadmap_Escort_Car` at `204 0 108` on `Street_Jail.layer`, the police M151 with
  `LF_JailPutInVehicleAction`. The action is listed at the car. Performing it
  still requires an on-duty officer and a prisoner on foot. The release street stays at Z=120. (verified 2026-10-11)
- ✅ **Sentences survive a restart** (2026-11-07, tests
  `restart/jail-serializer-roundtrip` and `e2e/pass2-verify-load` across a real
  restart, verified 2026-11-07): `LF_Jail.ExportAll` / `ApplyAll` are the `jail` payload of
  `EL_PersistenceComponentSerializer`, appended at version 5. Only serving
  sentences with time left are saved; `ApplyAll` replaces the live set, so a
  second apply converges. A version 1-4 save stops before the payload and leaves
  live sentences alone.
- ✅ **One jail tick** (2026-11-08, test `robustness/single-jail-tick`): the first placed
  `LF_JailCellComponent` owns the jail config and the repeating `LF_Jail.Tick`;
  a second cell logs `EL_Debug.Error` and neither reconfigures nor ticks (two
  cells used to run sentences at double speed). `OnDelete` removes the tick.
  `LF_JailArrestableComponent` hosts `LF_JailEscort.Tick` on one live instance
  and hands it to the next registered one when that host is deleted.
- ⚠️ Moving a connected prisoner to the cell, or pulling them back into it,
  goes through `SCR_Global.TeleportPlayer` (the vanilla server-side player
  teleport) instead of a server-only `SetOrigin` that the owning client can
  overwrite; a stand-in with no player is still set directly (2026-11-07). A
  prisoner seated in a vehicle moves the vehicle (vanilla behaviour). Not yet
  checked on a dedicated server with a remote client.


### `EL_PoliceMenu` — `Feature/Police/EL_PoliceMenu.c`
- ✅ Wanted list; `ArrestPlayer`/`FinePlayer` are client-side entry points.
- ✅ **Client→server bridge works** (verified 2026-08-30): the menu routes
  through `SCR_ChimeraCharacter.EL_AskPoliceArrest/Fine`, and the server
  handlers `RpcAsk_EL_PoliceArrest/Fine` validate on-duty officer, wanted
  target, and fine range before acting. Arrest also requires senior rank, and
  the fine must sit inside that rank's cap. `EL_PoliceUtils` (pure) holds the
  sentence-length math, wanted-reduction and fine-range checks for tests.
  `EL_AskPoliceSelfPromote` always refuses.
- ✅ **Arrest starts a sentence** (verified 2026-10-11): the arrest RPC calls
  `LF_Jail.Arrest` after the rank gate and no longer clears wanted or teleports
  to `"127 1 137"`.
- ✅ **Wanted list over RPC** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the menu asks the server
  (`EL_AskPoliceWantedList` → `RpcAsk_EL_PoliceWantedList`, which re-checks
  `CanViewWantedList`); the server answers the officer's client with parallel
  uid/name/level arrays (`RpcDo_EL_PoliceWantedList`, `SetWantedList`). The
  open action's offer reads `EL_AccountReplication`.
- ✅ **Hands-on only** (2026-11-07): arrest and fine refuse a
  target that is not connected or stands beyond `EL_PoliceUtils.POLICE_REACH`
  (5 m, the in-world arrest range) and show `#EL-Police_TooFar`. Neither the
  framework nor vanilla has a restraint state, so reach is the requirement. A
  fine that clears no wanted level (below `WANTED_STEP`, 1000) is refused with
  `#EL-Fine_TooSmall`, so repeated small fines cannot drain a wanted player.
  The fine RPC logs through `EL_Debug.Warn("Police", "fine refused: ...")`.
  Test: `security/police-abuse`.
- ⚠️ The menu layout does not render `GetWantedPlayers()` yet.

### `EL_DutyAction` — `Feature/Police/EL_DutyAction.c`
- ✅ Toggles police on-duty flag. Civilians are refused. The debug menu's
  police faction button sets faction and duty, which is how a solo session
  reaches the toggle (verified 2026-10-10). Going on duty starts the rank
  clock; going off duty settles it.
- ✅ **Duty pay at the desk** (verified 2026-11-08: LOGIC test `jobs/duty-pay`;
  the 2-client cash check is manual). Going off duty pays the shift's on-duty
  seconds times `m_fPoliceDutyPayPerSecond` (`Configs/Jobs/LF_Jobs.conf`, flagship
  0.5) as cash through `LF_DutyPay.PayAtClockOut`, with the pick-up sound and a
  `#EL-Duty_Paid` toast. Seconds settled by an armory draw mid-shift are kept
  for the clock-out. A second clock-out, a clock-out with no clock-in, and a
  non-police account pay nothing. Unpaid seconds are session state: a restart
  during a shift drops that shift's pay, it never pays twice.
- ✅ Toggling keeps the account resident: `SaveAndReleaseAccount` re-caches
  the account instead of evicting it, so the officer can reopen the police
  menu and re-toggle at once.
- ✅ **Dedicated client** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the toggle
  runs on the server only; the offer and the name (`#EL-Duty_GoOn` /
  `#EL-Duty_GoOff`) read `EL_AccountReplication`.

### `LF_PoliceRanks` — `Feature/Police/LF_PoliceRanks.c`
- ✅ Two ranks (verified 2026-10-11). Patrol officer draws
  `{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et`, fines up to
  1000, and cannot arrest. Senior officer draws
  `{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et`, fines up to
  the existing 5000 cap, and can arrest. The resolved kit is the intersection
  of that rank's list and the station catalog; a prefab that does not load is
  logged and skipped, and the rank stays usable. An on-duty senior promotes a
  lower officer. 3600 seconds on duty promotes patrol. `RequestSelfPromotion`
  is the self-promotion RPC body and always refuses. Rank does not change
  faction and does not write the police whitelist. Lower ranks still open the
  wanted list. DebugWorld places `Roadmap_Police_Armory` at `207 0 120` on
  `Street_Police.layer`.
- ✅ **One armory draw per duty shift** (2026-11-07): a draw
  that hands out any kit line marks the account; a second draw logs
  `draw refused: <uid> already drew a kit this shift` and shows
  `#EL-Rank_AlreadyDrawn`. `OnDutyChanged` clears the mark. Test:
  `security/police-abuse`.
- ⚠️ Rank, on-duty seconds, and the shift draw mark live on the account object
  and are not in `EL_PlayerAccountRecord`, so a restart returns the officer to
  patrol and allows one more draw.

### `LF_Contraband` — `Feature/Crime/LF_Contraband.c` (verified 2026-10-11)
- ✅ One convoy. A member of the organization that owns the hideout hauls
  `{73CF2BF72525F5D5}Prefabs/Items/Food/RowanBerries.et` from
  `Roadmap_Convoy_Start` at `222 0 120` to `Roadmap_Convoy_Drop` at `225 0 120`
  on `Street_Contraband.layer`. Delivery pays 200 MoneyStack cash once. A second
  delivery on that run pays nothing. Leaving the vehicle or dying fails the
  run and logs it; a failed run pays nothing. A non-member is refused and the
  refusal is logged. Markers are `{AF76315F0A38D270}Prefabs/Debug/EL_DebugAreaBoundary.et`.
  Player strings: `#EL-Contraband_Start`, `#EL-Contraband_Deliver`,
  `#EL-Contraband_NotMember`, `#EL-Contraband_Paid`, `#EL-Contraband_Failed`.
- ✅ **Vehicle watch lifetime** (2026-11-08): payment, failure, and
  `Reset` unregister the `OnCompartmentLeft` handler; a restarted run drops the
  old watch and watches the vehicle the player is in now. A disconnect fails the
  running convoy.

### `EL_ConfiscateAction` — `Feature/Police/EL_ConfiscateAction.c`
- ✅ On-duty police delete weapons from the nearest wanted player within 5 m.
  The offer reads replicated officer and target state; the deletion runs on the
  server only and re-checks the officer's account (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual).
- ⚠️ Deletes items while iterating a live `FindItems` array (mutation-during-
  iteration risk); no compensation or wanted reduction.

## Ownership

### `EL_OrganizationManager` — `Feature/Organizations/` (verified 2026-10-11)
- ✅ A character founds one organization. The id comes from `PersistenceIdUtils`,
  never an entity id. The founder invites and kicks. A member can leave. The
  founder cannot leave or be kicked. A second membership is refused.
- ✅ The shared fund is a balance keyed by organization id, the same shape as
  `EL_BankAccount`. Deposit credits it. Withdraw requires the withdraw rank and
  takes the whole amount or nothing. Buying an upgrade (extra storage,
  processing slot, parked vehicle) debits the configured price in one step or
  leaves the fund and the flag untouched. The price and the new flag are logged.
- ✅ Cash moves only as MoneyStack through `EL_OrganizationPlayer`. A short
  removal is returned to the character. A short payout puts the fund back. The
  personal ATM balance is not read or written. The desk is
  `Roadmap_Org_Desk` on `Street_Org.layer` at `210 0 120`, using
  `{6B7B7CCC74AACA67}Prefabs/Props/Furniture/Desk_01_short.et`. The menu is a
  workspace widget on `{977FCAE08556886B}UI/Layouts/OrganizationMenu.layout`.
  It lists the name, the fund, and the processing-slot upgrade, and shows an
  empty state before create. Create, deposit, withdraw, buy-processing, and
  close stay on the character RPCs. Fund deposit and withdraw are refused
  unless an entity carrying `EL_OrganizationDeskAction` stands within
  `EL_OrganizationPlayer.DESK_RANGE` (5 m) of the character
  (`#EL-Org_TooFar`; test `security/service-range`, 2026-11-07). Cash and
  membership still go through the server.
- ✅ `ExportAll` / `ApplyAll` replace the cache, so a second apply does not
  compound the fund. The game-mode serializer writes these records at version 2,
  after survival. A version 1 save has no organization payload and does not
  clear a live fund. Version 3 appends hideout ownership after these records.
  Version 4 appends owned vehicles after the hideout records. Version 5 appends
  serving jail sentences after the vehicles.

### `LF_Hideout` — `Feature/Hideouts/` (verified 2026-10-11)
- ✅ One building owned by one organization. Members unlock
  `Roadmap_Hideout_Door` at `213 0 120`. A non-member is refused and the
  refusal is logged. The door is `EL_Door_Base` with the inherited
  `EL_LockComponent`. The organization id is written onto that lock. The door
  does not call `LF_Breach` and does not add a second `ActionsManagerComponent`.
- ✅ `Roadmap_Hideout_Machine` at `216 0 120` uses the existing
  `EL_ProcessAction` on `ItemProcessor_Base`. It runs only when the player's
  organization owns the hideout and has the processing upgrade. Otherwise it
  refuses and logs. The recipe turns one cabbage into one RowanBerries stack,
  the configured contraband prefab. A chain machine with no hideout component
  still reads `ResourceChains.conf`.
- ✅ `Roadmap_Hideout_Stash` at `219 0 120` is the base-game crate mesh
  `{55991E1823C60AB3}Assets/Props/Crates/Crate_01.xob`.
- ✅ `ExportAll` / `ApplyAll` put the organization id and the lock flag back,
  and a second apply does not change them. The game-mode serializer writes
  these records at version 3. A version 2 save has no hideout payload and does
  not clear a live hideout. Player strings: `#EL-Hideout_NotMember`,
  `#EL-Hideout_NoUpgrade`, `#EL-Hideout_Process`.

## Notifications

### `EL_NotificationManagerComponent` — `Feature/Notifications/`
- ✅ Server→client notification broadcast with toast renderer; `SendToPlayer`/
  `SendToJob`/`SendToAll` + static `NotifyPlayer`/`NotifyJob`/`NotifyAll`.
  Before 2026-11-07 the toast layout
  `{4C38FD68D45325D2}UI/Layouts/Notifications/EL_NotificationToast.layout` did not
  exist, so every toast (fines, arrests, rank denials, lot, lock, jail) failed silently. It
  now exists: a paper slip in the UI kit palette with a stamp-ink type strip (`Background`,
  `ColorStrip`, `Icon`, `Title`, `Message`, `EL_NotificationToastUI` on the root).
  Slips that overlap in time stack downwards. Timing reads the real clock
  (`System.GetTickCount`), not an assumed 16 ms frame. `notifications/toast-layout` (WORLD)
  instantiates the layout on the dedicated server and checks every named widget;
  `notifications/toast-timing` (LOGIC) checks show/fade/remove. On-screen look is
  visual-only: manual check pending (verified 2026-11-07)
- ✅ **Targeted delivery fixed** (verified 2026-08-30): `RpcDo_ShowNotification`
  is `RplRcver.Broadcast` with a client-side `playerId` filter (`-1` = all),
  plus a direct call for the listen-server host so the authority sees its own
  toast. `SendToJob` duty semantics now read the account's `IsOnDuty()`, not a
  `job != UNEMPLOYED` tautology.
- ✅ **Private targeted delivery** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): `SendToPlayer` sends an Owner RPC on
  the target's `SCR_PlayerController` (`EL_DeliverNotification`), so only that
  client receives it; the listen host's own controller calls the handler
  directly. Broadcast is left for `SendToAll` only. `SendToPolice` /
  `NotifyPolice` reach connected on-duty police by account faction.
  `EL_Utils.AlertPolice` and `EL_Utils.NotifyEntity` (shop, robbery, duty,
  confiscate toasts) route through it; `EL_Utils.Notify` still shows a hint on
  the calling machine only.
- ⚠️ Hard-coded Spanish prefixes, emoji in logs, `m_Color` never transmitted.

## Inventory / Quantity

### `EL_QuantityComponent` — `Feature/Quantity/EL_QuantityComponent.c`
- ✅ Virtual stack system: `AddQuantity`/`RemoveQuantity` clamp to
  `[0, max]` (0 = unlimited), `SetQuantity(0)` deletes the entity,
  `CanCombine`/`Combine` (conservation: source decrease == dest increase ==
  transferred), `CombineQuantities` (2 and 3 yield 5), `Split`,
  `QuantityRemainingAfterSplit` (5 into 2 leaves 3), `SortByQuantity`
  (descending), `GetRemainingCapacity` (verified 2026-10-11).
- ✅ **In-inventory split mint closed** (verified 2026-11-07): the
  split stack is sized and marked keep-separate before `TryInsertItemInStorage`,
  so `HandleOnItemAdded` no longer merges the spawned init quantity (100 for
  MoneyStack) into the source. Split 1 used to add 99. Pin test
  `security/inventory-split-no-mint`.
- ✅ Stack tier meshes hold their `Resource` while applied, so a tier swap no longer logs
  `Usage of released BaseResourceObject` (2026-11-08, `quantity/operations` visual path).
- ✅ **Transfer intent lifetime** (2026-11-08, test `robustness/lifetime`): `OnDelete` removes
  the entity's intent (no dead keys); an intent older than `INTENT_TTL_MS` (5 s)
  belonged to a failed move and is ignored by the next add.
  `EL_DefaultLoadoutItemQuantityComponent` logs `EL_Debug.Error` instead of
  crashing on a prefab without `EL_QuantityComponent`.
- ⚠️ `Split` with `splitSize` 0 spawns an item then deletes it via
  `SetQuantity(0)`; `ExtractQuantityComponents` inserts null components with
  no check; `HandleOnItemAdded` may refresh UI on a just-deleted entity.

### `EL_InventoryUtils` (Quantity) — `Feature/Quantity/EL_InventoryUtils.c`
- ✅ Quantity-aware `GetAmount`/`AddAmount`/`RemoveAmount` returning *actual*
  amounts; AddAmount fills stacks then spawns; RemoveAmount drains smallest
  stacks first (ascending sort).
- ⚠️ `AddAmount` fills **largest** stacks first (descending) while Remove
  drains smallest — asymmetric by design, untested. Non-quantity
  `RemoveAmount` permanently deletes entities. `GetAmount` returns −1 on
  null target.

### `ScriptedInventoryStorageManagerComponent` (Quantity) — `Feature/Quantity/ScriptedInventoryStorageManagerComponent.c`
- ✅ RPC endpoints for split/transfer/intent (`RPC_EL_RequestQuantitySplit`,
  `RPC_EL_QuantityTransfer`, `RPC_EL_SetTransferIntent`), all server-side,
  range-gated.
- ✅ **Ownership gate added** (verified 2026-08-30): `EL_CanManipulateOwned`
  rejects another living player's carried items (only your own storage or
  unowned containers/ground loot); the old range-only anti-cheat is closed.
- ✅ **Locked owners refused** (verified 2026-11-07):
  `EL_CanManipulateOwned` also refuses a storage whose owner has a locked
  `EL_VehicleLockComponent` or `EL_LockComponent` (`EL_IsStorageOwnerLocked`).
  Pin test `security/locked-storage-refused`.

### Hand-Carry — `Feature/Character/Inventory/HandCarry/`
- ✅ `EL_HandInventoryStorageComponent` state machine
  (`NONE→READY→AWAIT_HOLSTER→ACTIVE→AWAIT_SWAP→NONE`), hands-only storage,
  modded vanilla SCR inventory/gadget wiring.
- ✅ Missing `CharacterControllerComponent`, weapon manager, or inventory
  manager logs `EL_Debug.Error("HandCarry", ...)` and skips that step instead of
  a null dereference (2026-11-07).
- ⚠️ Weapon-holster flow is client-only (server never sees the transition);
  `OnGadgetModeSet` lacks the `HasLocalControl` guard its siblings have;
  `SCR_CharacterControllerComponent` destructor touches `GetOwner()` at
  teardown.

## Vehicles & License Plates

### `EL_VehicleLockComponent` / `EL_VehicleKeyComponent` — `Feature/VehicleLock/`
- ✅ Server-assigned vehicle and debug-key identifiers explicitly bump their replicated state during initialization, so clients receive the identifier after picking up a key.
- ✅ The lock identifier, key id, owner, and lock flag are stored by `EL_VehicleOwnership`. A restart writes that identifier back with `SetVehicleIdentifier`, so the key still matches. Purchased vehicles replace the vanilla `SCR_BaseLockComponent` slot `{54D6FBC0EA806FE5}`, so get-in reads this lock. (verified 2026-10-11)

### `EL_LicensePlateManager` + generators — `Feature/LicensePlate/`
- ✅ `EL_LicensePlateGeneratorGeneric.GenerateLicensePlate()` is `"AA BB NNNN"`.
- ✅ The alphabet includes `Z` (`LetterAt(25)`) and the number range includes `9999`. `Math.RandomInt` is exclusive at the top, so the generator rolls 26 letters and a number end of 10000. (verified 2026-10-11)
- ✅ `EL_LicensePlateRegistry` keeps live plates. A second issue of a live plate is refused, so two vehicles do not share one by construction. (verified 2026-10-11)
- ✅ The registry is session memory, so `EL_VehicleOwnership.ApplyAll` re-reserves every saved plate on load; a purchase after a restart cannot be issued a plate already on a saved vehicle (2026-11-07, test `restart/vehicle-plates-reserved`, verified 2026-11-07).

### Vehicle lot and garage — `Feature/Vehicles/`
- ✅ `Roadmap_Vehicle_Lot` sells vanilla S105, UAZ469, S1203, UAZ452, and M923. Each spawn is a thin child of that vanilla prefab with `EL_VehicleLockComponent` and `{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et`. (verified 2026-10-11)
- ✅ A price at or below the lot cash ceiling (500) debits MoneyStack. A higher price debits the ATM balance and does not convert cash. A short payment is refused and charges nothing.
- ✅ Purchase spawns the locked child, sets the buyer as owner, locks it, issues a unique plate, and gives one key. Extra keys are an owner action at the configured cash price.
- ✅ The vehicle record is keyed by the lock identifier. `Export`/`Apply` restore the owner and the key id. `EL_PersistenceComponentSerializer` version 4 writes those records after the hideout records; a version 3 save has hideouts and no vehicle payload. Store keeps the prefab and the lock flag; retrieve spawns at the lot pad (`EL_VehicleSpawnPointComponent`), the same spawn-point rule as a character, not at the last world coordinates.
- ✅ **A restart drops no ground key** (2026-11-07, test `restart/vehicle-restore-no-ground-key`, verified 2026-11-07): `RestoreWorld` / `RestoreVehicle` put each out-in-world vehicle back on the pad, locked as saved, without spawning a key. Before, every restart left a fresh key beside the pad for every such vehicle (anyone could take the car, and keys piled up). The owner re-keys through the existing owner-only flow: garage store, then retrieve (retrieve spawns a key when the session has none), or an extra key at the lot.
- ✅ `Garage_E_01` (`{A548C1C5370D0734}Prefabs/Structures/Houses/Garage/Garage_E_01/Garage_E_01.et`) is the garage shell. DebugWorld places the use point `Roadmap_Garage` at `186 0 102.1` on `Street_Vehicles.layer`, with store and retrieve. The shell sits 5.9 m north of that point, so the player stands outside the wall. The release street stays at Z=120 from x=186 to x=243. The lot pad remains `Roadmap_Vehicle_Lot`, and the showroom body is `Roadmap_Vehicle_S105`. (verified 2026-10-11)
- ✅ `LF_Garage` is the catalog on the UI kit. The rows are the civilian S105, covered UAZ469, S1203, UAZ452, and M923 transport, at the lot cash prices. Each row keeps a preview slot that names that prefab and spawns the existing locked child. The texture path is empty, and `UI/Layouts/Garage/LF_GarageVehicleRow.layout` still has `VehicleImage` plus the name. `LF_GarageScreen.Open` uses `LF_UIScreen.Open` and `LF_UIPart`. A price at or below 500 debits MoneyStack. A higher price debits the ATM balance and does not take cash. Paint is stored on the record and applied with `EL_Utils`. Park refuses a seated player or a missing garage, and refuses a vehicle that is not within the same garage's radius as the player (`EL_VehicleGarageComponent.IsVehicleAtPlayersGarage`, `#LF-Garage_VehicleTooFar`, test `security/garage-store-range`, 2026-11-07), then stops the engine and teleports remaining occupants out with `GetOutVehicle` before store. A failed eject is logged. Retrieve returns the same prefab and key to the owner. Another player is refused and that refusal is logged. A missing prefab logs `EL_Debug.Error` and that row is skipped. Preview and the editor camera are skipped when the building or prefab is missing. (verified 2026-10-11)

## UI / Layouts / Localization

### UI kit — `Scripts/Game/Feature/UIKit/`
- Post-release polish. `LF_UITheme` holds the 1989 type, padding, and paper/metal colors. `LF_UIScreen` assembles a frame, title, body line, WLib edit box (ink, no text outline or shadow; the box is not a `TextWidget`), list row, status row, quantity slider, and pause-menu button with `WorkspaceWidget.CreateWidgets`, wires `SCR_ButtonTextComponent`, and activates `MenuContext` every frame (`MenuTextEditContext` while an edit box is focused, `MenuWithEditorContext` so a pad can move). Close removes the widgets and that call. It does not use MenuManager.
- Input (verified 2026-11-07): focus lands on the first non-field control at open, so MenuSelect works without MenuDown first. MenuBack closes. `GetOnButton()` hands the feature the part name of a pressed BUTTON, STATUS action, or list row action. `GetOnClosed()` fires once for any close. A LIST part (`LF_UIList.layout`, `AddListRow`/`ClearList`) scrolls to the focused row. Close defers the removal of its per-frame call by one tick, because it can run inside that call. `ui-kit/initial-focus` and `ui-kit/button-invoker` (LOGIC) cover the pure parts; the on-screen focus ring is visual-only.
- `LF_UIMenuInput` gives the remaining MenuManager menus (ATM, shop, quest, police) and the workspace pickers (faction, spawn location, death, organization) the same input: MenuContext plus MenuWithEditorContext every frame, MenuUp/MenuDown focus ring, MenuSelect on the focused button, MenuBack closes (not on the mandatory faction, spawn and death pickers), and a 150 ms debounce so a click and the poll do not double-fire. The ATM has a Close button and labelled Deposit/Withdraw buttons, tells the player when the amount is blank or zero, and names deposit and withdraw refusals separately. The shop and quest Close buttons work. Shop and ATM were not migrated onto the kit: `EL_CharacterATMComponent`, `EL_ShopComponent` and `EL_CharacterShopComponent` find these menus by MenuManager preset, and moving them would rewrite that lookup in files other work owns. The shop, quest, organization desk, and police menu actions are `HasLocalEffectOnlyScript`, so the menu opens only on the acting client.
- A line can be rewritten after open. A status row has one action that hides when the action is empty. A quantity splits one stack into keep and take. The sheet can close on death, on losing the character, or past a distance from an anchor. Nothing in the world opens the sample. `node agent-box/harness.mjs kit` screenshots the sheet, a rewritten balance, a hidden action, both split faces, and focus on the field. It reuses a play session that is already up. Faction, character creation, death, shop, ATM, quest, police, and spawn menus were not migrated.

### Signs — `Scripts/Game/Feature/Signs/`
- ✅ Diegetic signs (M1-01). `LF_SignComponent.m_sSignId` names a row in `Configs/Signs/LF_Signs.conf` (id, title, what, how, needs, rules). A value starting with `#` is a localization key; any other value is shown as written. `LF_SignReadAction` is local-effect only and names itself `Read: <title>` (`#LF-Sign_Read`). Perform opens `LF_SignCard` on the reading client: a UI kit sheet with the title, one line per filled section (What, How, Needs, Rules; empty ones omitted), and Close. Close, MenuBack, death, losing the character, or walking 4 m from the sign closes it. No RPC. An unknown or empty id, or a row with no title, logs `EL_Debug.Error("Signs", ...)` once and the action hides. LOGIC `signs/catalog`, `signs/missing-id`, `signs/localization`; WORLD `signs/prefabs`.
- ✅ Prefabs `Prefabs/Props/Signs/LF_Sign_Board` (cork board), `LF_Sign_Pub` (shop sign), `LF_Sign_Notice` (A4 notice), `LF_Sign_Chalkboard` (standing chalkboard; the vanilla `CardboardSign_01` lives in the Rally addon and does not resolve for this mod) inherit the vanilla props (no new meshes) and add the component, an `LF_Sign` action context on the face, and the Read action. A placed instance sets the id. The title is also printed on the prop on rendering clients: `LF_SignComponent` draws `UI/Layouts/Signs/LF_SignFace.layout` into an `RTTextureWidget` bound (`SetRenderTarget`) to a mesh whose material `Assets/Signs/Data/LF_SignFace.emat` samples `$rendertarget`. `LF_Sign_Board` spawns a local `LF_SignFace` sheet (vanilla A4 poster mesh) on its face; `LF_Sign_Notice` retargets its own poster material. A dedicated server skips the face. `LF_Sign_Pub` and `LF_Sign_Chalkboard` ship with the face off until their sheet placement is checked in a rendering client.
- ✅ `EL_MiningSign` reads `mining` (`Read: Mining area`) and `EL_WhitelistSign` reads `police_whitelist` beside its whitelist action. `EL_TextLabelComponent` (floating debug text) is deleted.
- ⚠️ MenuBack closes the card through the UI kit. The printed face, the card render, the prompt, and the walk-away close are not yet seen in a rendering client (manual steps in the M1-01 PR).

## Quests

### `LF_QuestTrackerComponent` — `Scripts/Game/Feature/Quests/`
- ✅ Server-authoritative quest definitions, acceptance, objective progress,
  timed expiry, collect recounts across managed nested storage, rewards, and
  client quest-log replication.
- ✅ `QuestGiver_Start` and `DebugWorld` provide a world-facing onboarding
  example with localized menu layouts and registered logic/world tests.
- ✅ **Turn-in loop closed** (verified 2026-11-07): turn-in removes each
  `COLLECT_REPORT` objective's target amount through `EL_InventoryUtils` before
  the reward, all or nothing (a short removal puts the items back and refuses).
  Client open/accept/turn-in (`RequestOpenGiver`/`RequestAccept`/`RequestTurnIn`)
  require the registered giver that offers the quest within
  `LF_QuestManager.GIVER_RADIUS` (5 m); an unknown giver id is refused. Server
  scripts may still call `AcceptQuest`/`TurnInQuest` directly. Pin test
  `security/quest-turnin-takes-items`.

### Layouts (loaded by `EL_Test_Data`)
- ✅ `EL_SplitQuantityDialog`, `EL_CharacterCreationMenu`, `EL_ATMMenu`,
  `EL_SurvivalHUD`, `FactionSelectionMenu`, `ShopMenu`, `PoliceMenu`,
  `DeathScreen` load (WORLD tier). The `SurvivalHUD` menu preset and its
  `EL_SurvivalHUD` class were never opened and are removed (2026-11-07); `EL_BetaHud`
  draws the gauges. The layout stays only as this load fixture. The unused
  falling-fruit widget (`EL_FallingFruit`, which loaded a layout that never existed) is
  removed; `EHarvestJobType` moved to `Feature/Jobs/EL_HarvestJobType.c`.
- ✅ `EL_DebugMenu` is a development menu opened with F10 (or its rebind),
  with server-routed controls for cash, wanted state, faction, jobs, survival,
  XP, and skill points. Actions affect only the requesting player's state
  (verified 2026-08-30). `EL_DevGate` (`Core/EL_DevGate.c`) checks every
  action on the server. It logs `[ELDebug:DebugMenu] debug action refused: ...`
  and refuses unless the game mode sets `m_bDevToolsEnabled` or the sender
  holds `EPlayerRole.ADMINISTRATOR`. Only `GameMode_Roleplay_Debug.et`
  (DebugWorld) sets the flag. The production `GameMode_Roleplay.et` leaves it
  off. The open key runs the same check on the client and logs
  `open refused` instead of opening. `EL_DevGate.IsAllowed` has the LOGIC test
  `debug-menu/gate` (2026-11-07).
- ✅ The playtest mission `Missions/LF_PlaytestTown.conf` (`LF_MissionHeader`,
  `Core/LF_MissionHeader.c`) runs DebugWorld with `m_bLF_DevToolsOff 1`: the
  header overrides the DebugWorld game mode opt-in, so only administrators
  pass `EL_DevGate` there. A bare world (Workbench play, `toolscli test`) has
  no LF header and keeps the opt-in. LOGIC test `host/playtest-mission`
  (2026-11-08).
- ⚠️ Localization keys `#EL-*` are frequently passed through `string.Format`
  (corrupting keys with `%1`) and/or handed to
  `SCR_HintManagerComponent.ShowCustomHint`, which does not localize `#Key`
  — the player sees raw keys. Spanish literals in notifications remain. The job,
  level and licence texts, the duty status words, the robbery, gather and process action
  texts, and the DebugWorld shop names moved to `#LF-*` keys on 2026-11-07, worded as
  employment rather than XP. Violates the AGENTS.md localization contract until the
  notification prefixes move too.

## DebugWorld coverage

 - ℹ️ **`Worlds/DebugWorld/DebugWorld.ent` is intentionally 0 bytes.** All
  DebugWorld content lives in `Worlds/DebugWorld/DebugWorld_Layers/*.layer`;
  `default.layer` carries the `GenericWorldEntity world` BSP and terrain
  entity. This layer loads before gameplay layers. Placing the world entity in
  `Terrain.layer` left earlier props without physics bodies even when they had
  `RigidBody` and `TRACEABLE` configured. Do not repair the empty `.ent`.
  `MainWorld.ent` differs because it
  is a `SubScene` wrapper over `worlds/Eden/Eden.ent`.
- ℹ️ **Direct collision fixtures need `Flags 0x403 0`.** This includes the
  `Traceable` flag. Without it, a `MeshObject` + `RigidBody` can render while
  character movement and projectile traces pass through it. Prefab fixtures
  such as `EL_TestRunner` inherit the flag; hand-authored `GenericEntity`
  fixtures must set it explicitly.
- ✅ DebugWorld keeps the reference gameplay surface populated: ATM, Apple/
  Plum/Tomato farming, Cement/Gravel/Sand/Logging/Mining chains, shops,
  traders, jobs, police, crime, houses, survival, tests, and the release
  street on `Roadmap.layer` (street root and boundary) plus one
  `Street_<Site>.layer` per site. Sites sit 3 m apart on +X, from the breach door
  at `186 0 120` through the vehicle lot at `243 0 120`. The debug terrain is
  the 512 m square (world 0..512, `TERRAIN_MAX` in `tools/street/manifest.mjs`).
  The row stays on that ground. That
  one layer holds the breach door at `186 0 120`
  and the charge at `186 0 122`, the bank vault at `189 0 120` and the bag at
  `189 0 122`, the jail door at `192 0 120` and the cell at `192 0 122`, the
  hospital till at `195 0 120` and the cabinet at `198 0 120`, the fuel till at
  `201 0 120`, the police desk at `204 0 120`, the armory at `207 0 120`, the
  organization desk at `210 0 120`, the hideout door at `213 0 120`, the
  machine at `216 0 120`, the stash at `219 0 120`, the convoy start at
  `222 0 120`, the convoy drop at `225 0 120`, the job depot at `228 0 120`,
  the job drop at `231 0 120`, the taxi pad at `234 0 120`, the fuel can at
  `237 0 120`, the S105 at `240 0 124`, and the vehicle lot at `243 0 120`.
  The second row is `Roadmap_Garage` at `186 0 102.1` and `Roadmap_Escort_Car`
  at `204 0 108`. The garage shell is north of its use point. The Z=120 street is not moved.
  `tools\cli validate` runs
  `validate-debugworld.ps1` to guard the layer set.
- ✅ The WORLD-tier `e2e/narrative-loop` exercises the live Apple gather →
  trader cash payout → shop purchase → ATM deposit path. The robbery step calls
  `EL_RobAction.CommitRobbery` (2026-11-07; it used to hand-write the payout);
  its proceeds are asserted as cash and are not added to the bank.
- ✅ `debugworld/entity-collision` checks live physics bodies, geometry, and
  sphere sweeps through both apple trade crates and all three trader tables.
  Flags alone cannot prove collision. World initialization runs first in
  `default.layer` (verified 2026-09-19).
- Manual collision smoke after integrating the change: reopen DebugWorld,
  start a fresh play session, spawn as Civilian, and walk into each of the
  three apple trader tables from both sides. The character must stop at the
  tables, and the apple trade actions must remain usable. Repeat on a client
  joining a running dedicated server. These client checks are not automated.

## Agent box

### `EL_AgentHarness` — `Feature/AgentHarness/`
- ✅ Dev-only. A rendering play session polls `$profile:agent-box/command.txt`.
  `press F` calls `SCR_InteractionHandlerComponent.ActionPerform`. Wheel scroll
  calls `ActionScroll`. `click` invokes the same button invoker or `OnClick` a
  mouse click does. `use` still checks `CanBeShownScript` and
  `CanBePerformedScript` before `PerformAction`. Menu keys are an allowlist
  (`MenuUp`, `MenuDown`, `MenuLeft`, `MenuRight`, `MenuSelect`, `MenuBack`,
  `EL_OpenDebugMenu`); movement actions are refused.
- ✅ `stand` is the placement exception: the server moves the local character
  beside a named entity. The poll does not start on a dedicated server
  (`RplMode.Dedicated`). The server checks the `stand`, `pose`, and `use` RPCs
  with `EL_DevGate`, the same gate as the debug menu. It refuses and logs
  (`[ELDebug:AgentHarness] <request> refused: ...`) unless the game mode sets
  `m_bDevToolsEnabled` or the sender is an administrator. On a production or
  listen-hosted server a client cannot teleport or reach past action range
  (2026-11-07). On a client it retries until a local controller exists,
  including while faction and spawn menus are up.
- ✅ Screenshots go through `EL_ScreenshotCapture`. UI shots leave the camera
  alone. A world shot teleports the character to the camera offset (default
  `10 3 10`, or `ox oy oz` for the row), faces the target, and screenshots the
  player view. That view faces -Z, so a picture stands on the +Z side of
  its target. The pose is applied again 700 ms later so gravity does not drop
  the body before the picture. `SetCamera` does not move that view. (verified 2026-10-11) A filtered row's wide shot
  stands just north of that row when the street is further north. If the new PNG matches the
  previous PNG, `report roadmap` records `camera_stuck` once and stops.
  `Roadmap_Jail_Door` and `Roadmap_Jail_Cell` are painted-light when look
  returns an action. Any other white mesh fails. Menu shots use `shot ui`; the
  report judges `classify` and the widget list. Names after `roadmap` walk
  only that row. `node agent-box/harness.mjs kit` photographs the UI kit
  sample in the current play session when play is already up, and enters play
  only when it is not. `node agent-box/harness.mjs report roadmap` writes
  `agent-box/reports/roadmap.html` from a play session. The Node harness, MCP
  tools, layers, and named flows live in `agent-box/`. See `agent-box/README.md`.
  (verified 2026-10-11)
- ✅ Town manifest (`agent-box/town.mjs`): every M1 site by entity name (or
  prefab, for the unnamed spawn points), the actions look must offer, its sign
  id, and its journey step. No positions: sites and signs resolve against the
  layers, so a site can move layers without a manifest edit.
  `node agent-box/harness.mjs town` and `validate-town-manifest.mjs` check it
  offline; `report town [site-id...]` walks it in play mode and writes
  `agent-box/reports/town.html`. Sign problems (none within 8 m, blank Read
  card) are `sign: pending` warnings until `requireSigns` is true (M1-09).
  Play-mode run not yet recorded.

### `LF_FilmCameraPath` — `Feature/Film/` (filming harness)
- ✅ Dev-only, like the agent harness. `LF_FilmCameraPath` holds timed camera
  keys (position, look-at, FOV); `Sample(t)` eases between keys with
  smoothstep and clamps at both ends. Out-of-order keys are rejected and
  logged (`[ELDebug:Film]`). Covered by `film/camera-path` (LOGIC).
- ⚠️ `LF_FilmCameraRunner.Play(path)` spawns a manual camera with input off,
  hides the HUD, moves the camera each frame, then restores the previous
  camera and HUD. It compiles but has not run in a rendering session and no
  agent-box verb calls it yet.
- ✅ `tools/film/film.mjs` records a window or region to mp4 (ddagrab +
  NVENC; gdigrab when ddagrab delivers no frame in 5 s), makes a GIF preview,
  films an agent-box flow, and publishes a clip to the `film-evidence`
  prerelease for a PR comment. Proven in game 2026-11-08: a game client on
  DebugWorld, flow `film-debugworld`, 20.7 s at 1280x720, 2.8 MB (PR #111).

## Cross-cutting risk summary (re-audited 2026-08-30; see per-feature rows)

1. **Money — one canonical path, all payouts cash.** The entity bank
   `EL_BankAccountComponent` is **deleted**; `EL_ATMManager`/`EL_BankAccount`
   is the only bank and the ATM is the only cash↔account boundary. Duty pay, benefits,
   trader sells, shop sells, robberies and job rewards all pay `EL_MoneyUtils`
   cash. Shop buy/sell correctness closed (partial refunds), ATM deposit/
   withdraw moves real cash with rollback. Remaining: no shop stock / per-item
   max quantity.
2. **Security — mostly closed.** Fruit-catcher score is server-clamped
   (pin test); `RpcAsk_DebugGrantLicense` removed; quantity RPCs have an
   ownership gate; gather/process actions re-validate server-side and reject
   malformed configurations. Remaining: multiplayer proof for the RPC edges.
3. **Persistence — improved.** Cache-eviction churn fixed (account stays
   resident). The serializer writes a `version` header
   first (4) and `validate-persistence-version.ps1` checks it against the
   payload order. Remaining: jail, breach, and bank-vault state has
   `Export` / `Apply` but is not in the save serializer, so it is lost on
   restart.
4. **Compile risk — resolved.** `EL_EFactionType` is defined in
   `EL_WhitelistManager.c`.
5. **Client/server breaks — mostly closed.** Police arrest/fine bridge and
   notification targeting are fixed; the death screen (client) → respawn RPC
   (server) bridge is new and needs a manual multiplay pass. Remaining:
   hand-carry holster flow is client-only.

## Toys

### `LF_JumpPad` — `Feature/Toys/LF_JumpPad.c`
- ✅ A trigger sets the entering player's velocity upward once for that enter.
  A second callback before they leave applies the zero vector. Leaving clears
  the enter, so the next entry launches again. A disabled pad applies the zero
  vector and does not call `SetVelocity`. Ported from EveronLife
  `EL_JumpTrigger` (MIT, Copyright (c) 2022 EveronLife). (verified 2026-10-11)

### `LF_SpikeStrip` — `Feature/Toys/LF_SpikeStrip.c`
- ✅ A trigger. A wheeled vehicle that enters has each wheel inside the strip
  box set to zero health. Ported from EveronLife `EL_SpikeTriggerComponent`
  (MIT, Copyright (c) 2022 EveronLife). (verified 2026-10-11)
