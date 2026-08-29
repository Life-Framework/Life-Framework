# Life Framework — Implemented Features & Contracts

> Ground truth of what the codebase currently contains and what each system is
> supposed to do. Written from a read-only sweep of
> `addons/LifeFramework/Scripts/Game/` (2026-08-29). Every claim carries a
> file path. This is the contract test cases must prove.
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
  character's prefab at a random `EPF_SpawnPoint`, and applies the default
  loadout (recursive, purpose-filtered storage fill).
- ⚠️ `GetCreationPosition` leaves `out` params unset when no spawn point
  exists; item is silently dropped if no matching storage; loadout recursion
  has no depth guard.

### `EL_CharacterCreationManager` — `Feature/CharacterCreation/EL_CharacterCreationManager.c`
- ✅ Flow: no account → create → faction menu → character creation menu →
  spawn. `OnCharacterCreated` persists a character, inits ATM + survival
  components, spawns.
- ❌ Uses placeholder prefab GUID `{YourTempCharacterPrefab}` and hard-coded
  coordinates (`"0 0 0"`, `"1000 0 1000"`). The temp-character path will fail
  if reached.

### `EL_CharacterCreationMenu` — `Feature/CharacterCreation/EL_CharacterCreationMenu.c`
- ✅ Validates first/last name non-empty and age in `[18, 80]`, then calls
  `OnCharacterCreated`.
- ⚠️ Validation failure silently early-returns with no error shown.

### `EL_FactionSelectionMenu` — `Feature/CharacterCreation/EL_FactionSelectionMenu.c`
- ✅ Sets faction on account, saves, re-enters `OnPlayerConnected`.
- ⚠️ Re-entrant flow can loop if the faction write does not persist before the
  re-entrant call.

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
- ⚠️ No recovery (health only ever falls); negative `dt` heals; no
  death/zero-health handling at the state level.

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
- ✅ Async load + cache + `SaveAndReleaseAccount` (pause, save, evict).
- ⚠️ **Cache-eviction churn**: every crime, duty toggle, arrest, and fine
  evicts the account; the next `GetAccount` returns null until an async
  reload, so consecutive wanted-level bumps and the police menu silently
  degrade after the first event per player.

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
- ✅ Persistent per-player balance; `Deposit` only if `>0`, `Withdraw` only if
  sufficient, returns success; balance never negative through these paths.
  Manager is a session map registry + async loader.
- ⚠️ `SetBalance` (persistence path) has no clamp. `CreateAccount` silently
  overwrites an existing account. Manager inherits `ScriptedUserAction`
  (misfit).

### `EL_ATMMenu` — `Feature/ATM/EL_ATMMenu.c`
- ❌ **Money from thin air**: Deposit never deducts cash from inventory,
  Withdraw never pays out. Explicit placeholder. No RPC path; buttons are
  server-guarded so a client does nothing. No insufficient-funds feedback.
  `WidgetLocalize` returns the key verbatim (not localized).

### `EL_BankAccountComponent` — `Feature/Banking/EL_BankAccountComponent.c`
- ✅ Entity bank with replicated balance, interest, transaction history
  (capped 100), Deposit/Withdraw/Transfer with guards, per-change EPF save.
- ❌ **Grants $20,000 to every owner in `OnPostInit`** and races persistence
  restore. RplProp + `BumpMe` on every change, unthrottled. `PAYMENT`/
  `REFUND` transaction types never emitted. **This is a second, unintegrated
  banking system alongside `EL_ATMManager`.**

### `EL_ShopComponent` — `Feature/Shop/EL_ShopComponent.c`
- ✅ Catalog-driven buy: `BuyItem(item, qty, buyer)` guards null/qty/max,
  server-only, price × quantity, `EL_MoneyUtils.RemoveAmount`, refund on
  inventory-full.
- ❌ **Money exploit**: insufficient-funds check treats partial `RemoveAmount`
  as full success (buyer with $50 buys a $100 item); refund credits the full
  price even when only part was removed. No shop stock (infinite supply).
  No validation the item is actually sold by that shop.

### `EL_ShopMenu` / `EL_ShopAction` — `Feature/Shop/`
- ✅ List UI; action opens menu within 3 m.
- ⚠️ Menu always opens for the local player regardless of who interacted.

### `EL_TraderManagerComponent` + `EL_InventoryStorageManagerComponent` — `Components/InventorySystem/`
- ✅ Sell-to-trader: inserting into a trader-owned storage deletes the item
  and deposits `m_ValuePerItem` into the seller's ATM account; black-market
  rejects non-civilian factions.
- ❌ Unguarded `pStorageFrom.GetOwner()` (null crash); null `m_aTradableItems`
  foreach crash; delete-then-pay not atomic; quantity stacks sell as one
  item; `RETCODE_ITEM_TOO_BIG` used for "not tradable"; black-market check
  likely fails open (account keyed by Steam UID here vs character ID in the
  caller); sell payout goes to ATM bank, not cash.
- ❌ Debug `Print("testtesttest")` in
  `EL_RestrictedInventoryStorageComponent.CanStoreItem`.

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
- ❌ **`RpcAsk_ClaimFruitCatcherReward` mints unbounded fruit + XP** (only
  `score <= 0` checked). **`RpcAsk_DebugGrantLicense` is a live backdoor**
  granting any license. Whitelist branch in `SetJob` is dead (only POLICE is
  restricted and it is special-cased). `AddExperience` is a no-op, so
  `OnLevelUp` can never fire. Hard-coded Spanish strings (not localized).
  Per-job level/XP persistence is convention-only, no SaveData pair.

### `EL_JobManager` — `Feature/Jobs/EL_JobManager.c`
- ✅ Singleton reward funnel; currently `GetGatherReward`/`GetProcessReward`
  return 0 (sell to traders instead). Dead-but-safe.
- ⚠️ `GiveReward` unreachable; no null-check on `EL_ATMManager.GetInstance()`;
  `#EL-Job_Earned` misused as a format string (no `%1`).

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
- ❌ **`EL_EFactionType` is referenced 16× but defined nowhere in the repo**
  — the file cannot compile as-is unless that type lives in another addon.
  No persistence (resets every server start). Getter returns the live
  internal array (callers can mutate without logging).

## Crime & Police

### `EL_RobAction` / `EL_RobWeaponAction` / `EL_RobVehicleAction` — `Feature/Crime/`
- ✅ Cash/weapon/vehicle robbery; cooldown + civilian-only + min-police-on-duty
  gates; raises wanted level (+1/+2/+3); notifies police.
- ⚠️ Near-duplicated guard logic across all three. Police-alert fires from
  *inside* the counting predicate (spam on every offer refresh). `#EL-Stole_Money`
  has no `%1` placeholder (amount dropped). `GetPlayerAccount` is cache-only,
  so after the first rob the wanted bump is skipped (see account cache
  churn). `EL_RobWeaponAction.AlertPolice` hardcodes "ALERTA POLICIAL".

### `EL_PoliceMenu` — `Feature/Police/EL_PoliceMenu.c`
- ✅ Wanted list; `ArrestPlayer` (teleport to jail, wanted→0, save) and
  `FinePlayer` (remove cash, wanted −amount/1000) are server-guarded.
- ❌ **Arrest/Fine unreachable from a real client**: the menu runs on the
  client but both methods early-return unless `Replication.IsServer()`, with
  no `RpcAsk_*` bridge. `account.GetActiveCharacter()` deref is unguarded.
  Jail position hard-coded `"0 0 0"`. `#EL-Fined %1!` builds a non-key.

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
- ❌ **RPC targeting suspect**: `RPC_ShowNotification` is `RplRcver.Owner` on
  the game-mode entity, so delivery to the intended player is likely wrong.
  `SendToJob` conflates "job != UNEMPLOYED" with `account.IsOnDuty()`. Hard-
  coded Spanish prefixes, emoji in logs, `m_Color` never transmitted.

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
- ⚠️ **Anti-cheat is range-only**: any entity within 10 m can trigger
  splits/transfers of another player's items. No ownership validation.

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
  `EL_SurvivalHUD`, `FactionSelectionMenu`, `ShopMenu`, `PoliceMenu` load.
- ⚠️ Localization keys `#EL-*` are frequently passed through `string.Format`
  (corrupting keys with `%1`) and/or handed to
  `SCR_HintManagerComponent.ShowCustomHint`, which does not localize `#Key`
  — the player sees raw keys. Spanish literals throughout jobs/licenses/
  notifications. Violates the AGENTS.md localization contract.

## Cross-cutting risk summary

1. **Money exploits**: `EL_ShopComponent.BuyItem` partial-funds + refund
   over-credit; `EL_ATMMenu` money-from-thin-air; `EL_BankAccountComponent`
   $20k grant.
2. **Security**: `RpcAsk_ClaimFruitCatcherReward` mint; `RpcAsk_DebugGrantLicense`
   backdoor; quantity RPCs range-only; gather/process no server re-validation.
3. **Persistence**: no version fields; `ApplyTo` wrong enum in
   `EL_PlayerAccountSaveData`; cache-eviction churn breaks in-session wanted
   stacking; two unintegrated bank systems.
4. **Compile risk**: `EL_EFactionType` unresolved in `EL_WhitelistManager`.
5. **Client/server breaks**: police menu arrest/fine unreachable; notification
   RPC targeting suspect; hand-carry flow client-only.