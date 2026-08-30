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
- ⚠️ Item is silently dropped if no matching storage; loadout recursion has no
  depth guard.

### `EL_CharacterCreationManager` — `Feature/CharacterCreation/EL_CharacterCreationManager.c`
- ✅ Flow: no account → create → faction menu → spawn-location picker →
  spawn. `OnCharacterCreated` persists a character, inits ATM + survival
  components, spawns.
- ✅ Real character prefab + real coordinates (verified 2026-08-30): the old
  `{YourTempCharacterPrefab}` placeholder and hard-coded `"0 0 0"`/
  `"1000 0 1000"` are gone; `CreateAndSpawnDefaultCharacter` uses
  `{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et`.

### `EL_CharacterCreationMenu` — `Feature/CharacterCreation/EL_CharacterCreationMenu.c`
- ✅ Validates first/last name non-empty and age in `[18, 80]`, then calls
  `OnCharacterCreated`.
- ⚠️ Validation failure silently early-returns with no error shown.

### `EL_FactionSelectionMenu` — `Feature/CharacterCreation/EL_FactionSelectionMenu.c`
- ✅ Buttons wired (Civilian/Police) set the account faction, save, re-enter
  `OnPlayerConnected`. `SaveAndReleaseAccount` keeps the account cached, so the
  re-entrant flow converges instead of looping.
- ✅ `EL_CharacterCreationManager.OnPlayerConnected` keys the account by
  `playerId` UID (not the controlled entity), so the faction choice lands on
  the same account the spawn logic reads.

### `EL_ShowIDAction` — `Feature/CharacterCreation/EL_ShowIDAction.c`
- ✅ Shows `"FirstName LastName, Age: N"` via notification.
- ⚠️ `CanBePerformedScript` returns true unconditionally; facing check is
  `dot > 0.707` both ways within 1.5 m.

### `EL_Utils` / `EL_FormatUtils` / `EL_Component` / `EL_ComponentData` — `Core/`
- ✅ `MaxInt`/`MinInt`, `AbbreviateNumber` (999→raw, 1000→`1.0K`,
  1e6→`M`, 1e9→`B`), typed component finders.
- ⚠️ `EL_ComponentData.Get` strips the last 5 chars of the class name
  (`XxxClass`→`Xxx`); breaks for any data class not ending in `Class`.

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

### `EL_PlayerAccountSaveData` — `Feature/Account/Persistence/`
- ✅ Field-for-field round trip of account state; `Equals` is
  order-insensitive over characters.
- ❌ `ApplyTo` returns `EPF_EReadResult.OK` instead of `EPF_EApplyResult.OK`
  (wrong enum). No version field.

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
- ⚠️ `SetBalance` (persistence path) has no clamp. `CreateAccount` silently
  overwrites an existing account.

### `EL_ATMMenu` — `Feature/ATM/EL_ATMMenu.c`
- ✅ **Cash moves** (verified 2026-08-30): the menu forwards through
  `EL_CharacterATMComponent.RequestDeposit/RequestWithdraw`, and the server
  RPC handlers `RpcAsk_Deposit`/`RpcAsk_Withdraw` remove cash on deposit
  (partial removals rolled back) and pay cash out on withdraw (all-or-nothing
  with balance restore), guarded by `EL_ATMManager.IsValidAmount`. No money
  from thin air.

### `EL_BankAccountComponent` — `Feature/Banking/` (deleted 2026-08-30)
- ❌ The second, unintegrated entity bank is **gone** (file deleted, no
  references remain). The $20k grant, the persistence race, and the duplicate
  money path died with it.

### `EL_ShopComponent` — `Feature/Shop/EL_ShopComponent.c`
- ✅ Catalog-driven buy: `BuyItem(item, qty, buyer)` guards null/qty, server-
  only, price × quantity, `EL_MoneyUtils.RemoveAmount`, refund on
  inventory-full.
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
- ✅ List UI; action opens menu within 3 m.
- ⚠️ Menu always opens for the local player regardless of who interacted.

### `EL_TraderManagerComponent` + `EL_InventoryStorageManagerComponent` — `Components/InventorySystem/`
- ✅ Sell-to-trader pays **cash** (verified 2026-08-30): inserting into a
  trader-owned storage pays `unitValue × stackSize` via `EL_MoneyUtils.AddCash`
  (pay-before-delete, clawed back if the item cannot be removed); black-market
  rejects non-civilian factions. The old "payout to ATM bank" and the
  `Print("testtesttest")` debug line are gone.
- ⚠️ Unguarded `pStorageFrom.GetOwner()` (null crash); null `m_aTradableItems`
  foreach crash; delete-then-pay not atomic; `RETCODE_ITEM_TOO_BIG` used for
  "not tradable"; black-market check fails closed but is only cache-keyed.

## Gathering / Processing / Resources

### `EL_GatherAction` — `Feature/Gathering/EL_GatherAction.c`
- ✅ Interact action granting `m_GatherAmount` of `m_GatherItemPrefab` into
  the user inventory; tool requirement (right hand / left gadget / anywhere
  in inventory); per-resource gather counter + cooldown restock.
- ❌ `PerformAction` does not re-validate the tool (client-evaluated
  `CanBePerformedScript` is the only gate) — hostile RPC can gather without
  the tool. `m_GatherAmountMax` 0/unset cycles oddly. Timeout attribute says
  "ms" but is compared as seconds-compatible — fragile.

### `EL_ProcessAction` — `Feature/Processing/EL_ProcessAction.c`
- ✅ Recipe: consume all `m_aProcessingInputs`, produce all
  `m_aProcessingOutputs` (into inventory or force-dropped).
- ❌ **Free-output exploit**: `PerformAction` grants all outputs even when
  inputs are missing (`RemoveAmount` clamps to available). No server-side
  re-validation. Empty input list means free outputs.

### `EL_DestructibleResourceComponent` + `EL_DestructibleResourceHitZone` — `Feature/Resources/`
- ✅ Damage gated to configured tools; tool → per-hit damage; FX on hit.
- ⚠️ Non-tool or unmapped damage source deals **0 damage silently** (no
  feedback); damage-source prefab string comparison is fragile on casing.

## Jobs / Level / Licenses / Whitelist

### `EL_PlayerJobComponent` — `Feature/Jobs/EL_PlayerJobComponent.c`
- ✅ Per-player job state, paycheck clock (`salary * (1 + (playerLevel-1)*0.05)`,
  `Math.Round`), per-job level/XP maps, license gates (POLICE→POLICE_ACCESS,
  MEDIC→MEDIC_ACCESS), client→server `RpcAsk_SetJob` re-validates licenses.
- ✅ **Fruit-catcher mint closed** (verified 2026-08-30): the server clamps any
  claimed score through `EL_GetFruitCatcherRewardCount` to
  `EL_FRUIT_CATCHER_MAX_SCORE` (100); pin test `security/fruitcatcher-reward-clamp`.
- ✅ **Debug-license backdoor removed** (verified 2026-08-30): `RpcAsk_DebugGrantLicense`
  no longer exists anywhere in `Scripts/Game`.
- ✅ **Paychecks pay cash** (verified 2026-08-30): `ProcessPaycheck` uses
  `EL_MoneyUtils.GiveCash`; the old `EL_BankAccountComponent` deposit branch is
  gone (that entity bank is deleted). The only cash↔account boundary is the ATM.
- ⚠️ Whitelist branch in `SetJob` is dead (only POLICE is restricted and it is
  special-cased). `AddExperience` is a no-op, so `OnLevelUp` can never fire.
  Hard-coded Spanish strings (not localized). Per-job level/XP persistence is
  convention-only, no SaveData pair.

### `EL_JobManager` — `Feature/Jobs/EL_JobManager.c`
- ✅ Singleton reward funnel; currently `GetGatherReward`/`GetProcessReward`
  return 0 (sell to traders instead). Dead-but-safe.
- ⚠️ `GiveReward` pays cash when reached (verified 2026-08-30) but the funnel
  is unreachable today; `#EL-Job_Earned` misused as a format string (no `%1`).

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
- ❌ Whitelist hard-coded to POLICE in `UnlockLicense` — a MEDIC whitelisted
  license would wrongly require police whitelist. `PurchaseLicense` spends SP
  even if `UnlockLicense` later rejects (double-charge). `SetUnlockedLicenses`
  bypasses all checks. Not localized. `m_bIsInitialLicense` never read.

### `EL_WhitelistManager` — `Feature/Whitelist/EL_WhitelistManager.c`
- ✅ Pure static in-memory whitelists; `IsFactionRestricted` (POLICE,
  MILITARY, MAFIA), `IsJobRestricted` (POLICE only), add/remove/get.
- ✅ **Compiles** (verified 2026-08-30): `enum EL_EFactionType` is now defined
  in this file (CIVILIAN/POLICE/MILITARY/MAFIA).
- ⚠️ No persistence (resets every server start). Getter returns the live
  internal array (callers can mutate without logging). The faction-selection
  flow does not yet consult `IsWhitelistedForFaction` — anyone can pick
  POLICE on first join; gating it requires admin tooling first (a fresh
  server would otherwise have no police at all).

## Crime & Police

### `EL_RobAction` / `EL_RobWeaponAction` / `EL_RobVehicleAction` — `Feature/Crime/`
- ✅ Cash/weapon/vehicle robbery; cooldown + civilian-only + min-police-on-duty
  gates; raises wanted level (+1/+2/+3); notifies police.
- ✅ **Cash payout + reliable wanted bump** (verified 2026-08-30): the haul is
  `EL_MoneyUtils.AddCash` (never the bank — pin test `crime/rob-reward`
  asserts the bank balance stays 0), and the account cache churn is fixed so
  every robbery escalates wanted (pin test `crime/rob-wanted-clamp`).
- ⚠️ Near-duplicated guard logic across all three. Police-alert fires from
  *inside* the counting predicate (spam on every offer refresh). `#EL-Stole_Money`
  has no `%1` placeholder (amount dropped). `EL_RobWeaponAction.AlertPolice`
  hardcodes "ALERTA POLICIAL".

### `EL_PoliceMenu` — `Feature/Police/EL_PoliceMenu.c`
- ✅ Wanted list; `ArrestPlayer`/`FinePlayer` are client-side entry points.
- ✅ **Client→server bridge works** (verified 2026-08-30): the menu routes
  through `SCR_ChimeraCharacter.EL_AskPoliceArrest/Fine`, and the server
  handlers `RpcAsk_EL_PoliceArrest/Fine` validate on-duty officer, wanted
  target, and fine range before acting. `EL_PoliceUtils` (pure) holds the
  jail position, wanted-reduction and fine-range math for tests.
- ⚠️ RPC handlers still log denials via `Print` (should be `EL_Debug`);
  `account.GetActiveCharacter()` deref in the wanted list is unguarded.

### `EL_DutyAction` — `Feature/Police/EL_DutyAction.c`
- ✅ Toggles police on-duty flag.
- ⚠️ Toggling evicts the account from the cache, so the officer cannot reopen
  the police menu or re-toggle until the account reloads.

### `EL_ConfiscateAction` — `Feature/Police/EL_ConfiscateAction.c`
- ✅ On-duty police delete weapons from the nearest wanted player within 5 m.
- ⚠️ Deletes items while iterating a live `FindItems` array (mutation-during-
  iteration risk); no compensation or wanted reduction.

## Notifications

### `EL_NotificationManagerComponent` — `Feature/Notifications/`
- ✅ Server→client notification broadcast with toast renderer; `SendToPlayer`/
  `SendToJob`/`SendToAll` + static `NotifyPlayer`/`NotifyJob`/`NotifyAll`.
- ✅ **Targeted delivery fixed** (verified 2026-08-30): `RpcDo_ShowNotification`
  is `RplRcver.Broadcast` with a client-side `playerId` filter (`-1` = all),
  plus a direct call for the listen-server host so the authority sees its own
  toast. `SendToJob` duty semantics now read the account's `IsOnDuty()`, not a
  `job != UNEMPLOYED` tautology.
- ⚠️ Hard-coded Spanish prefixes, emoji in logs, `m_Color` never transmitted.

## Inventory / Quantity

### `EL_QuantityComponent` — `Feature/Quantity/EL_QuantityComponent.c`
- ✅ Virtual stack system: `AddQuantity`/`RemoveQuantity` clamp to
  `[0, max]` (0 = unlimited), `SetQuantity(0)` deletes the entity,
  `CanCombine`/`Combine` (conservation: source decrease == dest increase ==
  transferred), `Split`, `SortByQuantity` (descending), `GetRemainingCapacity`.
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

### Hand-Carry — `Feature/Character/Inventory/HandCarry/`
- ✅ `EL_HandInventoryStorageComponent` state machine
  (`NONE→READY→AWAIT_HOLSTER→ACTIVE→AWAIT_SWAP→NONE`), hands-only storage,
  modded vanilla SCR inventory/gadget wiring.
- ⚠️ Weapon-holster flow is client-only (server never sees the transition);
  `OnGadgetModeSet` lacks the `HasLocalControl` guard its siblings have;
  `SCR_CharacterControllerComponent` destructor touches `GetOwner()` at
  teardown.

## Vehicles & License Plates

### `EL_LicensePlateManager` + generators — `Feature/LicensePlate/`
- ✅ `EL_LicensePlateGeneratorGeneric.GenerateLicensePlate()` is pure:
  `"AA BB NNNN"`.
- ❌ **Off-by-one bugs**: `Math.RandomInt(0, 25)` excludes `Z`;
  `Math.RandomInt(100, 9999)` excludes `9999`. No uniqueness guarantee and
  no registry of issued plates. `EL_LicensePlateManager` has no destructor to
  clear `s_Instance`.

## UI / Layouts / Localization

### Layouts (loaded by `EL_Test_Data`)
- ✅ `EL_SplitQuantityDialog`, `EL_CharacterCreationMenu`, `EL_ATMMenu`,
  `EL_SurvivalHUD`, `FactionSelectionMenu`, `ShopMenu`, `PoliceMenu`,
  `DeathScreen` load (WORLD tier).
- ✅ `EL_DebugMenu` is a DebugWorld-only menu opened with F10 (or its
  rebind), with server-routed controls for cash, wanted state, faction, jobs,
  survival, XP, and skill points. Its RPC rejects non-DebugWorld requests
  (verified 2026-08-30).
- ⚠️ Localization keys `#EL-*` are frequently passed through `string.Format`
  (corrupting keys with `%1`) and/or handed to
  `SCR_HintManagerComponent.ShowCustomHint`, which does not localize `#Key`
  — the player sees raw keys. Spanish literals throughout jobs/licenses/
  notifications. Violates the AGENTS.md localization contract.

## Cross-cutting risk summary (re-audited 2026-08-30; see per-feature rows)

1. **Money — one canonical path, all payouts cash.** The entity bank
   `EL_BankAccountComponent` is **deleted**; `EL_ATMManager`/`EL_BankAccount`
   is the only bank and the ATM is the only cash↔account boundary. Paychecks,
   trader sells, shop sells, robberies and job rewards all pay `EL_MoneyUtils`
   cash. Shop buy/sell correctness closed (partial refunds), ATM deposit/
   withdraw moves real cash with rollback. Remaining: no shop stock / per-item
   max quantity; ATM `SetBalance` unclamped.
2. **Security — mostly closed.** Fruit-catcher score is server-clamped
   (pin test); `RpcAsk_DebugGrantLicense` removed; quantity RPCs have an
   ownership gate. Remaining: gather/process actions have no server-side
   re-validation in `PerformAction`.
3. **Persistence — improved.** Cache-eviction churn fixed (account stays
   resident). Remaining: no save version fields; `ApplyTo` returns the wrong
   enum in `EL_PlayerAccountSaveData`.
4. **Compile risk — resolved.** `EL_EFactionType` is defined in
   `EL_WhitelistManager.c`.
5. **Client/server breaks — mostly closed.** Police arrest/fine bridge and
   notification targeting are fixed; the death screen (client) → respawn RPC
   (server) bridge is new and needs a manual multiplay pass. Remaining:
   hand-carry holster flow is client-only.
