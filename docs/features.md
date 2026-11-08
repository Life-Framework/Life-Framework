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
  resume on reconnect). An officer who leaves on duty stays on duty with the
  police inventory carried; the civilian inventory waits at the desk (M1-11).
- ⚠️ Managers are only created on first connect; a server-side call before any
  player connects gets null.

### `EL_SpawnLogic` — `Scripts/Game/Feature/Character/Spawning/EL_SpawnLogic.c`
- ✅ Account-aware respawn: loads `EL_PlayerAccount`, spawns the active
  character's prefab at a spawn point for the account's role (CIVILIAN, or
  POLICE for an officer who reconnects on duty; falls back to any point), and
  applies the role's kit (`GiveRoleKit`, recursive, purpose-filtered storage
  fill). The POLICE kit is the starter police kit the duty desk issues (M1-11).
- ✅ `ResolveSpawnPoint` (static) picks `SCR_SpawnPoint` by account role key;
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
- ✅ Flow (M1-11, 2026-11-08): no account → create → registration form R-89
  (`LF_RegistrationForm`) → civilian spawn with an ID card. There is no faction
  screen and no spawn-location picker; police clock in at the duty desk.
  A returning account skips the menus and the form. `OnCharacterCreated` persists a character, inits ATM + survival
  components, spawns.
- ✅ **Server-owned on a dedicated server** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the flow runs on the server
  only. The form opens on the joining client through an owner RPC on its
  `SCR_PlayerController` (`LF_ShowRegistrationForm`,
  `EL_CharacterCreationController.c`); the answer comes back as
  `RpcAsk_EL_CreateCharacter` and lands in `ServerCreateCharacter`. The player
  is the controller's id, never a client value. `EL_CharacterCreationRules`
  re-checks it: one character per account, names 1..32 ASCII letters with single inner spaces, hyphens,
  or apostrophes (`LF_RegistrationRules`), age 18..80. Names are not unique. A refusal toasts the reason
  key (`#EL-CC_Error_*`) and re-opens the menu. A listen host's joiners now get
  their own menus instead of the host.
- ✅ Real character prefab (verified 2026-08-30):
  `{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et`.
- ✅ (2026-11-08, M1-08) No more "Default Citizen": an account with no
  character opens the registration form over `LF_ShowRegistrationForm`
  (owner RPC). A server refusal sends the form back with the reason written on
  it. The form never picks a faction.

### `LF_PlayerInteraction` — `Feature/Interaction/` (M1-12, 2026-11-08)
- ✅ One list on another player's body (`Character_Roleplay.et`, context
  `EL_ID`): Introduce yourself, Show ID, Give cash, Give what you hold; on-duty
  police also Search and Arrest. `LF_PlayerRoleAction` slots `fine` and
  `org_invite` stay hidden until `LF_PlayerInteraction.RegisterRole(slot,
  handler)` is called on every machine. Reach: 2 m between origins, both
  alive, the target a player, not yourself; the server re-checks every action.
- ✅ Give cash: the server opens `LF_GiveCashSheet` on the giver (`GiveCashAmount`,
  `GiveCashButton`), then `RpcAsk_LF_GiveCash` moves MoneyStack cash through
  `LF_CashHandover.Move`: more than carried is refused, what the receiver cannot
  take goes back (no cash created or lost). Gesture `Point` + `SOUND_PICK_UP`.
- ✅ Give item: the giver's machine names the item in hand (gadget, right-hand
  item, or current weapon); the server moves it only when its storage root is
  the giver and the receiver has room.
- ✅ Introduce: the target learns the user's character name (`LF_Introductions`,
  keyed by player uid, one way). Persisted as the `introductions` payload (save
  version 8); names re-sync to clients on every spawn. Gesture `Halt`.
- ✅ `LF_KnownNames` (client): a known name shows under the crosshair only while
  looking at that player (15 degree cone) within 4 m. Strangers show nothing.
- ⏳ Gesture visibility, the name label, and the restart path on a real
  dedicated server are manual (PR #98 steps).

### `EL_ShowIDAction` — `Feature/CharacterCreation/EL_ShowIDAction.c`
- ✅ (2026-11-08) Shows the ID card the user carries (their own first, else
  the first printed one) to the player they face: the server reads the item
  and opens `LF_IdCardView` on the viewer's machine. No card: not performable,
  reason `#LF-ID_NoCard`. Offered by the player interaction reach rule
  (`LF_PlayerInteraction.CanOffer`, 2 m, both alive, a player); the server
  re-checks it. The shower points (`Gesture_Point`) and the viewer hears
  `SOUND_PICK_UP` (M1-12).
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
- ✅ (M1-18, 2026-11-08) The town clerk hands the card over at arrival step 1
  (a quest with `m_bRewardIdCard`). From then on `EL_SpawnLogic` prints one
  from the account's character record on every spawn of that player
  (`LF_QuestManager.HasEarnedIdCard`, from the persisted turn-ins), so the card
  survives death and restart. `m_bGrantIdAtSpawn` (now default off) gives every
  spawn a card for a mode with no clerk. A character already carrying their own
  card is refused a second; a blank carried card is printed instead of adding
  one. The DebugWorld civilian starter kit no longer carries a blank card.

### `EL_NpcBaseEntity` station NPCs — `Prefabs/Characters/Core/Npc_Base.et`
- ✅ **Dressed on every machine** (2026-11-08, #150): `BaseLoadoutManagerComponent`
  spawns the slot clothing (jacket, pants, boots) on the server, and that
  clothing is replicated. `Npc_Base.et` carries an `RplComponent`
  (`"Rpl State Override" Runtime`, `"Parent Node From Parent Entity" 0`) and a
  `Hierarchy`, as vanilla `Character_Base.et` does: the server streams each
  placed NPC with its worn clothes as children. Before this fix, a dedicated client saw
  the NPCs in underwear. Test: `npc/clothes-replicate` (WORLD).
- ⚠️ A client builds a placed NPC from its prefab, not from the layer. A
  layer may override only `CharacterIdentityComponent` on an NPC. An added
  component kicks every client (`RplCreationError: Number of items mismatch`),
  and other overrides never reach clients. `validate-npc-layer-overrides.mjs`
  enforces this. Names set in the layer do not reach clients either
  (`FindEntityByName` works only on the server).

## Character & Survival

### `EL_SurvivalStats` — `Feature/Survival/EL_SurvivalStats.c`
- ✅ Hunger and thirst clamped `[0,100]`. `Eat`/`Drink` are additive through
  clamped setters. `Create(id)` starts both at 100.
- ✅ (2026-11-08, M1-28) `UpdateStats(dt, config)` drains needs at the
  `LF_SurvivalConfig` rates. Flagship: hunger `0.01/s` (about 2.8 h from
  full), thirst `0.0125/s` (about 2.2 h). `dt <= 0` is a no-op.
- ✅ (2026-11-08) The separate survival health stat is gone. Needs act on the
  character's real health (`LF_SurvivalHealth`). The save record keeps its
  `m_fHealth` slot, written as 100 and ignored on load, so the save format
  does not change.

### `LF_SurvivalHealth` — `Feature/Survival/LF_SurvivalHealth.c`
- ✅ (2026-11-08) **Needs never kill.** On the server tick, with hunger or
  thirst below 20, the damage manager's default `Health` hit zone falls at
  `0.0005` of max per second (about 8 min) to `0.75` of max, then stops. An
  injury already below the cap is not touched. Survival tracks what it took;
  once needs are met it gives back only that, at `0.001`/s. Vanilla healing
  above the cap is never pulled down while fed. A dead body is skipped.
  Tests: `survival/light` (math), `survival/real-health` (WORLD, spawned
  character).
- ✅ The consumable `m_fHealthRestore` heals real health (percent of max).

### `LF_SurvivalConfig` — `Feature/Survival/LF_SurvivalConfig.c`
- ✅ (2026-11-08) Framework knobs: hunger and thirst drain, the low-needs
  threshold, the real-health cap, drain and give-back (fractions of max), the
  stamina drain multiplier, the tick interval and the max tick delta. A
  missing `.conf` logs `[ELDebug:Survival]` once and falls back to the same
  built-in defaults.

### `EL_CharacterSurvivalComponent` — `Feature/Survival/EL_CharacterSurvivalComponent.c`
- ✅ (2026-11-08) Server-authoritative: stats tick on the server only, on a
  1 s call-queue timer. The delta is clamped to 5 s so a hitch does not drop a
  chunk. The owner gets whole-point hunger and thirst and the low-needs flag as
  owner-only RplProps; `BumpMe` fires only when one of them changes (about
  once per 100 s at flagship rates), and right after eating or drinking. The
  low-needs flip logs `[ELDebug:Survival] <id> needs low=...`. Tests:
  `survival/sync`.
- ✅ The old per-frame `EOnFrame` driver is gone. It had no event mask and
  treated world-time milliseconds as seconds.

### `LF_SurvivalStaminaPenalty` — `Feature/Survival/LF_SurvivalStaminaPenalty.c`
- ✅ (2026-11-08) Modded `SCR_CharacterStaminaComponent.OnStaminaDrain`: with
  low needs, exertion drains stamina `1.5x` (config), so a hungry player
  sprints for less time and breathes hard sooner. It lifts on the next sync
  after eating and drinking. Same reentrancy pattern as the base hydration
  penalty.
- ⚠️ Not yet felt in game: set survival to 10 in the DebugMenu and sprint.

### `EL_BetaHud` survival gauges — `HUD/EL_BetaHud.c`
- ✅ (2026-11-08) Hunger and thirst gauges read the replicated whole points.
  The HUD stays second to the body cue. The health gauge already reads the
  damage manager, so the needs cap now shows there too.

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
- ✅ **You wake up in hospital** (M1-26, #121; tests `respawn/bill-once`,
  `respawn/bill-debt`, `respawn/bed-pick`, `respawn/ward`). A death marks the
  player's uid with a pending wake (`LF_HospitalRespawn.MarkDied`). The next
  spawn of that player (death-screen Respawn, or a reconnect while dead) takes
  it once: the body spawns at a free `LF_HospitalBedComponent` bed (rotating,
  occupied beds skipped), the bill is charged to the bank, a printed paper
  `HospitalBill.et` (amount, balance after, in-game date) goes in the pockets,
  and a toast names the amount. The owner plays the base game's
  bed wake loiter (the tutorial intro's, type 7) with the spawn sound, gets up
  after 4 s and steps down to the bed's stand point on the floor,
  then reads the bill: the note "You don't remember how you got here." and the
  New Life Rule window (a printed server rule, nothing enforces it). A bill
  lying in the world has Read. Server-decided; the client never sends an
  amount or a bed. Every charge and debt logs `[ELDebug:Respawn]`.
- ✅ Debt: `EL_BankAccount.Charge` may go below zero when debt is allowed; a
  negative balance is kept through save and load, deposits pay it off first,
  withdrawals are refused while in debt.
- ✅ Config `Configs/Spawning/LF_Respawn.conf` (`LF_RespawnConfig`): wake in
  hospital (off, or no bed placed: the spawn plaza), bill amount (flagship
  150), debt allowed, New Life Rule minutes (15), wake animation. The beds are
  the respawn point list: put `LF_HospitalBedComponent` on any bed (wake offset
  and yaw are attributes). DebugWorld: three `BedMedical_01` in the front west
  room of the hospital (`Street_Hospital.layer`); the hospital building is now
  the unfurnished `House_Town_E_2I03_Base`.
- ⚠️ The pending wake is session state: a restart between death and respawn
  forgets it (plaza, no bill). A carried bill comes back blank after a restart
  and hides Read.

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
- ✅ **Placed storage** (2026-11-08, M1-15): `LF_PersistentStorageComponent`
  (`Scripts/Game/Persistence/`) saves a placed container's tree in the same store
  under its `m_sStorageKey` and slot `placed`, so it rides the `carried` payload
  with no new save version. Captured on every item added or removed and at every
  save point; restored a frame after the entity initializes and a frame after a
  load. Restore reconciles, so a second restore spawns nothing. The prefab carries
  no native `Persistence` component, so the engine does not save the contents a
  second time (the two-boot e2e finds the 151 cash in the stash and no loose
  copy in the world).
- ⚠️ Ammo count and weapon state are not in the snapshot. Attachments and
  magazines are kept as items.

### Progression — `Feature/Account/LF_Progression.c`
- ✅ **Progression survives a restart** (2026-11-08, M1-10, tests
  `persist/progression-roundtrip`, `persist/progression-old-save`, and
  `e2e/pass2-verify-load` across a real two-boot restart). The `progression`
  payload, appended at save version 7, holds one `LF_ProgressionRecord` per
  player uid: the civilian delivery career (level, XP, paid total), police rank,
  on-duty seconds and unpaid duty seconds, seconds since the last benefit
  collection, the player level (level, XP, skill points), licences by enum name,
  and turned-in quest ids. A courier at level 2 still opens fuel delivery and a
  senior officer stays senior.
- ✅ Level, licences, and turn-ins live on the character, so their owners write
  through to the store and `EL_SpawnLogic` applies the record to every new body.
  They now survive death as well; only carried items drop.
- ✅ Tolerant load: an unknown licence name is skipped with a warning, an unknown
  quest id is kept with a warning, a rank above the ladder clamps to senior. A
  version 6 save leaves live rank and duty totals alone (the account apply
  carries them over). A running duty clock is settled into the save.
- ✅ **The delivery sign-up survives a restart** (2026-11-08, test
  `persist/job-signup-roundtrip`). The `jobsignups` payload, appended at save
  version 11, holds one `LF_JobSignUpRecord` (player uid, job by enum name) per
  signed-up player. It loads after `progression`, so the career level that gates
  the job is back first. `EL_SpawnLogic` puts the job on every new body after the
  hand-over, so the sign-up also survives death. An unknown name, a job that is
  not delivery work, or one above the player's level stays unemployed with a
  `[ELDebug:Jobs]` warning.
- ⚠️ An active run and the open duty clock are session state; after a restart the
  courier is still signed up and starts a new run. `EL_LicenseManagerComponent` is not on
  `Character_Roleplay.et`, so licences persist only once a prefab carries it.

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
  Joining: the host runs `-noBackend`, so every player has the NULL platform
  identity. Until 2026-11-08 that made `GetPlayerUID` empty and the faction pick
  failed with `#EL-CC_Error_NoAccount` (nobody could join).
  `EL_Utils.ChooseIdentityFallback` now keys a player by Steam name on a
  `-noBackend` dedicated server: stable across restarts, one character per name
  (`spawn/identity-fallback`; join, restart, rejoin seen live).

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
  `LF_RankPromoteAction`, `LF_BreachRepairAction`, and (since 2026-11-08) the
  jail actions: arrest, escort, escort-arrest, put in and take out of a
  vehicle. Before that their offers read the server-only account, so on a
  dedicated client an on-duty officer was never offered an arrest. Every
  server path still re-checks the real account; the jail cell and escort holds
  are checked on the server only. `tools/validation/validate-offer-checks.mjs`
  fails a `CanBeShownScript` / `CanBePerformedScript` that reads
  `EL_PoliceUtils.GetAccountForEntity` or `EL_PlayerAccountManager`.
- ✅ **Armory actions hidden from non-police** (2026-11-08, #157): Draw and
  Promote at `Roadmap_Police_Armory` are shown only to on-duty police
  (`LF_ArmoryDrawAction.ShownTo`, replicated state); a civilian no longer sees
  them greyed. The server still refuses in `PerformAction`. Test:
  `playtest/offers`.
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

### ATM terminal — `Feature/ATM/LF_ATMTerminal.c` (was `EL_ATMMenu`)
- ✅ (2026-11-08, M1-13) **Terminal screen** on `LF_UIScreen` with the
  `LF_UISkin.Terminal()` skin: green phosphor (vanilla `robotomono` face) in a
  bezel, balance and carried cash from the server, an `AMOUNT >` entry line, a
  status line, a 3x4 keypad (CLR, 0, DEL), DEPOSIT, WITHDRAW, EXIT. Keys: mouse,
  pad focus that moves like a grid (MenuUp/Down keep the column, MenuLeft/Right
  walk a row), keyboard digits and numpad (`LF_Keypad0..9`), Backspace or pad X
  (`LF_KeypadBack`), Delete or pad Y (`LF_KeypadClear`). The entry cannot pass
  `MAX_TRANSACTION_AMOUNT`. Closes on EXIT, MenuBack, death, or 3.5 m from the
  machine. `EL_ATMAction` runs PerformAction only on the acting machine
  (`HasLocalEffectOnlyScript`) and opens only when the user is this machine's
  character (`LF_CounterAccess.IsLocalActor`).
- ✅ (2026-11-08) **Refusals say why, on the screen**: the server replies
  `LF_EATMRefusal` (amount, cash short, balance short, pockets full, not at an
  ATM, no account) with the new balance and carried cash; no toast (the old
  away-from-ATM toast plus the menu toast was the duplicate). No reply in 5 s
  prints `NO REPLY FROM THE BANK`. Sounds: key click, accept and refuse are UI
  events set on the `EL_ATMAction` attributes; an accepted deposit or
  withdrawal plays `SOUND_PICK_UP` on the character for everyone nearby.
  Tests: `atm/keypad`, `atm/refusal-reasons`, `counter/refusal-text`,
  `counter/local-only`, `counter/screen-layout`.
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
  logs `deposit refused: not at an ATM` / `withdraw refused: ...` and the
  terminal prints `REFUSED: STAND AT THE MACHINE` (2026-11-08; was a toast). On a listen host the requests and the replies call their
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
- ⚠️ No shop stock (infinite supply).

### Shop price list — `Feature/Shop/LF_ShopPriceList.c` (was `EL_ShopMenu`)
- ✅ (2026-11-08, M1-13) Typed price list on `LF_UIScreen` with the
  `LF_UISkin.PriceList()` skin (paper, mono face, ink buttons): carried cash, a
  "Look for" field that narrows the list as you type, up to 40 lines of item
  and buy price with an Order button, an order slip (quantity with One less /
  One more, 1..99), Pay <total> cash, Sell these, Leave. Buy and sell are the
  existing `EL_CharacterShopComponent` RPCs; every request now gets a reply
  with `LF_EShopRefusal` (not sold here, unpriced, cash short, pockets full,
  nothing to sell, quantity, too far, no shop), the money moved and the carried
  cash, printed on the slip. The shop no longer raises toasts. An accepted
  trade plays `SOUND_PICK_UP` on the character; UI sounds are `EL_ShopAction`
  attributes. `EL_ShopAction` opens only on the acting machine for its own
  character. Closes on Leave, MenuBack, death, or 3.5 m from the counter.
  Tests: `shop/refusal-reasons`, `counter/local-only`, `counter/screen-layout`.

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
- ✅ **Crop growth runs on the server only** (2026-11-08, #157): `EL_BaseCrop`
  starts its growth clock only on the server (`Replication.IsServer()` and
  `RplSession.Mode()` is not `Client`), not on
  `RplComponent.IsOwner()`, which is still true on a client while the world
  loads. Every stage read goes through `EL_BaseCrop.StageAt`, which returns
  null for an index outside the list. Before this each game client logged a
  `NULL pointer to instance` VME in `EL_BaseCrop.EOnFrame` for every crop.
  Each crop logs `crop <name> init: grows here <bool> (rpl mode <mode>)` once.
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
  `LF_CivilianJob`. A completed courier, fuel, or taxi run adds
  `m_iJobXpPerRun` XP and `PullCareer` copies the level onto this component,
  so `OnLevelUp` fires. Level 1 may take courier work, level 2 opens fuel
  delivery, level 3 opens taxi. Under-level signup refuses and is logged.
- ✅ **Job pacing is config and hidden** (2026-11-08, #156): `LF_Jobs.conf`
  sets `m_iJobXpPerRun` (100), `m_iJobXpFirstLevel` (500), and
  `m_fJobXpLevelGrowth` (2): 5 runs open fuel delivery, 15 open the taxi
  (`LF_CivilianJob.RunsToReach`). No level text reaches the player: the
  level-up hint ("grade N, +N%") and the grade in the hire hint are gone
  (`#LF-Job_HiredHidden`). When a run opens new work the drop-off shows
  `#LF-Job_NewWork` titled with the job name (`LF_CivilianJob.NewlyOpened`).
  Pay per run is unchanged (40/60/80): 5 parcels then 5 fuel runs is about
  500 cash, so a new player can buy the S105 (1000, from the bank) after about
  10 runs. Test: `jobs/pacing`.
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
  `{9CA41609F6C806AE}Prefabs/Items/Roleplay/LF_Parcel.et`, a cardboard box carried
  in both hands (#155: the bare `HandCarryItem_Base.et` had no mesh, so the
  hands were empty on a client). Test: `items/carry-meshes`. Fuel cargo is
  `{12D5AD21E383B768}Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et` and
  the drop consumes it. The taxi fare uses the rank vehicle
  `{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et` and does
  not need a second client. A missing cargo prefab or a full inventory logs
  `EL_Debug.Error` and that run does not pay. DebugWorld places
  `Roadmap_Job_Depot` (post counter
  `{28F9F4DC40089B51}Prefabs/Props/Services/Post/CounterPostOffice_01.et`),
  `Roadmap_Job_Drop`, `Roadmap_Job_TaxiPad`, and `Roadmap_Job_FuelCan` on
  `Street_Jobs.layer`: the counter on the post office porch, the drop on a
  house porch at the east end, the taxi pad at the bus stop by the square, the
  fuel can at the fuel station.

- ✅ **Sign-up offers on a dedicated client** (2026-11-08, #157): the greyed reason reads
  `#LF-Job_MoreRunsFirst` ("Do more parcel runs first"), not "Your work level is too low".
  A client reads the level replicated on `EL_PlayerJobComponent` (the career map is
  server-only), so fuel and taxi open on the client once the server levels the career.

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
  `LF_DutyLocker.RefuseClockIn` when a player clocks in at the duty desk (M1-11;
  the faction screen is gone). A client has no whitelist; the desk offer reads
  the roster bit of `EL_AccountReplication`. A uid
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
- ✅ The cooldown survives a restart (M1-10): the save holds seconds since the
  last collection, because world time starts over.

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
  DebugWorld places `Roadmap_Bank_Vault` at 194.1 0 265 (on the bank's east
  wall) and `Roadmap_Bank_Bag` at 195.6 0 263 on `Street_Bank.layer`. The bag is
  `{65DE48DCC1D4DD01}Prefabs/Items/Roleplay/LF_MoneyBag.et` (a vanilla suitcase mesh,
  #155). Cash is
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
  `Roadmap_Hospital_Till` at `257 0 256.4` (hospital porch) on `Street_Hospital.layer`.
- ✅ The cabinet is a forced-entry container. `BreachCabinet` calls
  `LF_Breach.Arm`. When that lock opens it spawns the configured medicine
  prefabs: field dressing, morphine, saline, and tourniquet. A missing prefab
  logs and that line is skipped. A second breach while the cabinet is open
  spawns nothing and does not alert again. `Roadmap_Hospital_Cabinet` is at
  `198 0 120`. Items stay ordinary; confiscation of a contraband flag belongs
  to the contraband scope.

### `LF_Murder` / `LF_SafeZone` — `Feature/Crime/` (M1-27, #125)
- ✅ **Murder makes you wanted** (test `crime/murder`). Every character death
  reaches `EL_GameModeRoleplay.OnControllableDestroyedEx` on the server. A
  player who kills another player or an NPC gets wanted +`m_iMurderWanted`
  (`LF_Police.conf`, default 3), one `LF_WantedReports` report with the
  `#LF-Offence_Murder` offence and their description (no name), an alert to
  every on-duty officer, and a toast. Not murder: a death with no player behind
  it, a self-kill, a destroyed vehicle, and (config
  `m_bOfficerKillOfWantedExempt`, default on) an on-duty officer killing a
  wanted suspect. Each decision logs `[ELDebug:Murder]`. No automatic ban.
- ✅ **Spawn safe zone** (test `safezone/blocks`). `LF_SafeZoneComponent` on
  `LF_SafeZone_Square` (`Gamemode.layer`, fountain 115 0 250, radius 30 m as
  the component attribute) registers a flat circle. The modded
  `SCR_CharacterDamageManagerComponent.HijackDamageHandling` swallows a hit
  when a player hurts a different player and either stands inside: no health,
  bleeding, or ragdoll. NPC, world, and self damage pass. Weapons still raise
  and fire. No police exemption: arrest is an interact action. Each refusal
  logs `[ELDebug:SafeZone] refused ...`; the shooter gets a toast at most every
  8 s. Two `safe_zone` notice boards (`Sign_SafeZone_Spawn`,
  `Sign_SafeZone_East`) say so in the world.
- ✅ Both civilian spawn points stand on the square (the second moved from
  170 0 199, by the quarry, to 121 0 253); `safezone/blocks` asserts every
  civilian spawn point is inside a zone. The hospital wake (M1-26) is not a
  spawn and stays outside.

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
  `Street_Fuel.layer` at `259 1.04 241.2` (on a counter at the fuel station) is
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
  `Roadmap_Escort_Car` at `247.5 0 238` (beside the police station) on `Street_Jail.layer`, the police M151 with
  `LF_JailPutInVehicleAction`. The action is listed at the car. Performing it
  still requires an on-duty officer and a prisoner on foot. (verified 2026-10-11)
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
  prisoner seated in a vehicle is dismounted straight to the target first
  (`GetOutVehicle_NoDoor`; a client-owned player through the vanilla Game Master
  `SCR_EditableCharacterComponent.SetTransform`), so the vehicle stays put (M1-21,
  test `jail/vehicle-arrest`). Not yet checked on a dedicated server with a remote client.
- ✅ **Online time** (M1-21, 2026-11-08, tests `jail/online-time`, `e2e/pass2-verify-load`): the
  sentence counts down only for connected players (`LF_Jail.Tick` reads
  `PlayerManager.GetPlayers`; `TickOnline` is the testable seam). An offline prisoner keeps
  the time they had at logoff and logs `sentence paused` / `sentence resumed`. A prisoner who
  spawns with time left (rejoin, restart) goes back to the cell with their slip (`EL_SpawnLogic`).
- ✅ **Sentence slip** (M1-21, test `jail/slip`): sentencing hands the prisoner
  `Prefabs/Items/Roleplay/SentenceSlip.et` (vanilla personal-papers mesh). `LF_JailSlipComponent`
  prints name, offence (wanted level), and online minutes left; the minutes replicate only when
  the whole minute changes. Hovering it in the inventory reads it (`LF_JailSlipUIInfo`). A
  respawn or a second arrest reprints the carried slip instead of adding one. No new save data.
- ✅ **Visible release** (M1-21, tests `jail/slip`, `jail/online-time`): release opens the cell
  door (`DoorComponent.SetControlValue`, the vanilla door motion and sound; it closes after
  5 s), places the prisoner at the cell release offset (`m_vReleaseOffset`; DebugWorld `0.15 0 10`, outside
  the door), stamps the slip, then shows the toast 1.5 s later. Release runs once per sentence.
  The DebugWorld cell now sentences 60 s per wanted level, max 300 s (it was zero-length).


### `EL_PoliceMenu` — `Feature/Police/EL_PoliceMenu.c`
- ✅ `ArrestPlayer`/`FinePlayer` are client-side entry points. The wanted list
  left the menu in M1-11: wanted people are on the station wanted board.
- ✅ **Client→server bridge works** (verified 2026-08-30): the menu routes
  through `SCR_ChimeraCharacter.EL_AskPoliceArrest/Fine`, and the server
  handlers `RpcAsk_EL_PoliceArrest/Fine` validate on-duty officer, wanted
  target, and fine range before acting. Arrest also requires the configured
  arrest rank (patrol on the flagship, #165), and the fine must sit inside
  that rank's cap. `EL_PoliceUtils` (pure) holds the
  sentence-length math, wanted-reduction and fine-range checks for tests.
  `EL_AskPoliceSelfPromote` always refuses.
- ✅ **Arrest starts a sentence** (verified 2026-10-11): the arrest RPC calls
  `LF_Jail.Arrest` after the rank gate and no longer clears wanted or teleports
  to `"127 1 137"`.
- ✅ **Hands-on only** (2026-11-07): arrest and fine refuse a
  target that is not connected or stands beyond `EL_PoliceUtils.POLICE_REACH`
  (5 m, the in-world arrest range) and show `#EL-Police_TooFar`. Neither the
  framework nor vanilla has a restraint state, so reach is the requirement. A
  fine that clears no wanted level (below `WANTED_STEP`, 1000) is refused with
  `#EL-Fine_TooSmall`, so repeated small fines cannot drain a wanted player.
  The fine RPC logs through `EL_Debug.Warn("Police", "fine refused: ...")`.
  Test: `security/police-abuse`.

### `EL_DutyAction` — `Feature/Police/EL_DutyAction.c`
- ✅ **Clock in / Clock out at the duty desk** (M1-11, 2026-11-08; WORLD
  `police/duty-swap`, `police/death-on-duty`, `police/station`; restart via
  `wt e2e`). `Roadmap_Police_Desk` is `Prefabs/Police/LF_PoliceDutyDesk.et`
  (desk + vanilla `ClockPunch_01`). The offer reads the roster bit of
  `EL_AccountReplication`; the server re-checks `EL_WhitelistManager` and
  `LF_DutyLocker` swaps the whole carried inventory, clothes included:
  clock-in stores the civilian tree as `civilian_stash` and restores
  `police_duty` (or the starter police kit, the POLICE entry of
  `EL_SpawnLogic.m_aFactionCharacterItems`); clock-out does the reverse and pays
  duty pay. The faction is the duty role: POLICE only while clocked in.
  Feedback: the vanilla pick-up gesture on the acting client, the range control
  panel button click (`SOUND_RANGECP_STARTBUTTON`) broadcast from the desk, then
  a toast. Death on duty ends the shift: the body keeps the police gear, the
  civilian stash becomes the carried snapshot the respawn restores, and the
  next shift starts from the starter kit. Going on duty starts the rank clock;
  going off duty settles it. `PoliceStation_Debug` and `PoliceDutyNPC` no
  longer offer duty. The debug menu's police button still sets the role without
  a swap (dev only).
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
  runs on the server only; the offer and the name (`#LF-Duty_ClockIn` /
  `#LF-Duty_ClockOut`) read `EL_AccountReplication`.

### `LF_WantedReports` / `LF_WantedBoard` — `Feature/Police/`
- ✅ **Crime reports** (M1-11, LOGIC `police/wanted-board`): each crime that
  raises wanted (till, vehicle and weapon robbery, breach, hospital till) calls
  `LF_WantedReports.Report`, which records the offence and a description of the
  suspect at that moment: clothing display names, the vehicle they sit in, and
  the nearest sign title within `m_fPlaceRadius` (else a 100 m grid square).
  An on-duty officer within `m_fCatchInActRadius` (`Configs/Police/LF_Police.conf`)
  catches it in the act and the report is identified at once. Showing your own
  ID card to an on-duty officer (`EL_ShowIDAction`) identifies every open
  report. Reports come down when wanted returns to 0, and persist as the
  `wanted` payload (save version 10).
- ✅ **Wanted board** `Roadmap_Police_WantedBoard` (`Prefabs/Police/LF_WantedBoard.et`,
  a cork `LF_Sign_Board` printing WANTED): "Read the wanted posters" asks the
  server per read (`RpcAsk_LF_WantedBoard`, the server finds the board near
  the reader). On-duty police get offence, stars, clothing, vehicle, and place;
  everyone else gets the public poster without the place. A name appears only
  on identified reports. The card is an `LF_UIScreen` paper sheet that closes
  4 m from the board; reading plays the map pick-up sound and gesture.

### `LF_PoliceRanks` — `Feature/Police/LF_PoliceRanks.c`
- ✅ Two ranks (verified 2026-10-11). Patrol officer draws
  `{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et`, fines up to
  1000 (`LF_PoliceConfig.PatrolFineCap`). Senior officer draws
  `{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et` and fines up
  to the existing 5000 cap. The resolved kit is the intersection
  of that rank's list and the station catalog; a prefab that does not load is
  logged and skipped, and the rank stays usable. An on-duty senior promotes a
  lower officer. 3600 seconds on duty promotes patrol. `RequestSelfPromotion`
  is the self-promotion RPC body and always refuses. Rank does not change
  faction and does not write the police whitelist. Lower ranks still open the
  wanted list. DebugWorld places `Roadmap_Police_Armory` at `241.2 0 244.2`
  (police station forecourt) on `Street_Police.layer`.
- ✅ **One armory draw per duty shift** (2026-11-07): a draw
  that hands out any kit line marks the account; a second draw logs
  `draw refused: <uid> already drew a kit this shift` and shows
  `#EL-Rank_AlreadyDrawn`. `OnDutyChanged` clears the mark. Test:
  `security/police-abuse`.
- ✅ Rank, on-duty seconds, and unpaid duty seconds survive a restart (M1-10,
  `progression` payload, save version 7).
- ⚠️ The shift draw mark is session state, so a restart allows one more draw.
- ✅ **Arrest and escort are config, open to every officer** (2026-11-08, #165):
  `LF_PoliceConfig` (`{916BDC092C01B7D1}Configs/Police/LF_Police.conf`) sets the
  lowest rank that may arrest (`m_iArrestMinRank`) and escort
  (`m_iEscortMinRank`), and the patrol fine cap. Flagship: 0, 0, 1000, so a
  patrol officer arrests on a fresh server. `LF_Jail.Arrest` and
  `LF_JailEscort` re-check the rank on the server for every path (action,
  menu, escort to the cell); the offers read the replicated rank and show
  `#EL-Rank_ArrestDenied` / `#LF-Rank_EscortDenied` when a server raises the
  minimum. A file that does not resolve logs once and uses the same defaults.
  Test: `police/rank-permissions`.

### `LF_Contraband` — `Feature/Crime/LF_Contraband.c` (verified 2026-10-11)
- ✅ One convoy. A member of the organization that owns the hideout hauls
  `{73CF2BF72525F5D5}Prefabs/Items/Food/RowanBerries.et` from
  `Roadmap_Convoy_Start` at the barn out of town (`466 0 331`) to
  `Roadmap_Convoy_Drop` in the hideout yard (`299 0 293`) on
  `Street_Contraband.layer`. Both markers are pallet boxes carrying the marker
  component, not debug boundaries. Delivery pays 200 MoneyStack cash once. A second
  delivery on that run pays nothing. Leaving the vehicle or dying fails the
  run and logs it; a failed run pays nothing. A non-member is refused and the
  refusal is logged. Markers are `{AF76315F0A38D270}Prefabs/Debug/EL_DebugAreaBoundary.et`.
  Player strings: `#EL-Contraband_Start`, `#EL-Contraband_Deliver`,
  `#EL-Contraband_NotMember`, `#EL-Contraband_Paid`, `#EL-Contraband_Failed`.
- ✅ **Vehicle watch lifetime** (2026-11-08): payment, failure, and
  `Reset` unregister the `OnCompartmentLeft` handler; a restarted run drops the
  old watch and watches the vehicle the player is in now. A disconnect fails the
  running convoy.
- ✅ **The refusal reaches the player** (2026-11-08, #157): a start refused for
  membership shows a toast with the reason, `#LF-Contraband_JoinOrg` (no
  organization: join or found one at the organization desk) or
  `#LF-Contraband_NotHideoutOwner` (your organization does not hold the
  hideout). `LF_Contraband.StartRefusalKey` picks it. Before this the server
  logged `convoy refused: not a member` and the player saw nothing.
- ⚠️ The crate still offers "Start convoy" to everyone: a client cannot see
  organization membership, so the reason arrives after the press, not on the
  offer.

### `EL_ConfiscateAction` — `Feature/Police/EL_ConfiscateAction.c`
- ✅ (2026-11-08, M1-12) Search in the player interaction menu: shown to on-duty
  police within the 2 m reach; deletes the weapons of the wanted player in
  front of them (the action owner), not the nearest wanted player. Weapons are
  collected before deletion. Earlier contract: on-duty police delete weapons
  from a wanted player.
  The offer reads replicated officer and target state; the deletion runs on the
  server only and re-checks the officer's account (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual).
- ⚠️ No compensation or wanted reduction.

## Ownership

### `EL_OrganizationManager` — `Feature/Organizations/` (verified 2026-10-11; ledger and invites 2026-11-08)
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
  `Roadmap_Org_Desk` on `Street_Org.layer` at `288.9 0 255.7` (org registry porch), using
  `{6B7B7CCC74AACA67}Prefabs/Props/Furniture/Desk_01_short.et`.
- ✅ (2026-11-08, M1-17) The desk opens the registry ledger `LF_OrgLedger`, a
  1989 ledger page on the UI kit (the old `OrganizationMenu.layout` is gone).
  No organization: name field and "Sign and pay $250"; the fee
  (`LF_OrgRules.FOUNDING_FEE`) leaves carried MoneyStack and comes back if the
  registration is refused. Names: 3 to 32 letters, digits, spaces, `- ' & .`,
  unique ignoring case and outer spaces (`EL_OrganizationManager.IsNameTaken`).
  A member's page: name, rank, fund with deposit (and withdraw for the rank
  that may), the processing upgrade, the alley yard lease
  (`LF_OrgHideoutLease`, free, founder takes it), the member list (offline
  members by their record name), and Leave (not for the founder). Found,
  deposit, withdraw, buy, lease, and leave are refused unless an
  `EL_OrganizationDeskAction` stands within `EL_OrganizationPlayer.DESK_RANGE`
  (5 m) (`#EL-Org_TooFar`; test `security/service-range`). The page closes at
  4 m from the desk, on death, and on MenuBack. Sounds: page turn on open,
  pencil on Sign, map close on close. LOGIC `org/registry`, `org/invite-role`.
- ✅ (2026-11-08, M1-17) Invite and remove are face to face, in the `org_invite`
  slot of the player interaction menu (`LF_OrgInviteRole`, registered in
  `EL_GameModeRoleplay.OnGameStart`). A leader facing someone with no
  organization sees "Invite to <org>" (offer, `Gesture_Point`); the invitee
  facing that leader sees "Join <org>" and joins with one press (raised hand,
  hand-over sound); a leader facing a member sees "Remove from <org>"; nobody
  can remove the founder. An offer is session state (`LF_OrgInvites`, not
  saved) and lapses after 2 minutes. The offer side reads the organization
  replicated on each character (`LF_OrgReplication`: id, name, rights bits,
  held offer; RplProps written on spawn, on change, and every 10 s); the
  server decides from the registry. Two-client proof:
  `node agent-box/session.mjs org [--shot]`.
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
- ✅ (2026-11-08, M1-17) An organization takes the yard (`LF_Hideout.HIDEOUT_ID`,
  gate `302.4 0 288` since M1-25) with "Take the lease" on the registry ledger; taking it locks the gate.
  The gate prompt on a client compares the organization replicated on the
  character with the owner on the lock and shows `#EL-Hideout_NotMember` to a
  non-member; the server still decides from the registry.
- ✅ `Roadmap_Hideout_Machine` at `216 0 120` uses the existing
  `EL_ProcessAction` on `ItemProcessor_Base`. It runs only when the player's
  organization owns the hideout and has the processing upgrade. Otherwise it
  refuses and logs. The recipe turns one cabbage into one RowanBerries stack,
  the configured contraband prefab. A chain machine with no hideout component
  still reads `ResourceChains.conf`.
- ✅ **Hideout stash holds items** (2026-11-08, M1-15, tests `hideout/stash`,
  `hideout/stash-save`, and `e2e/pass2-verify-load` across a real two-boot
  restart). `Roadmap_Hideout_Stash` (now at `312 0 296` in the village) is an instance of the reusable
  prefab `Prefabs/Props/Hideouts/LF_HideoutStash.et` (the base-game
  `Chest_01.xob` chest, a crate was too small to notice): a real container (`LF_HideoutStashStorageComponent`,
  200 kg) whose items are entities. Open storage asks the server
  (`SCR_PlayerController.LF_RequestOpenStash`); a member of the organization that
  owns the hideout (`m_sHideoutId` on `LF_HideoutStashStorageManagerComponent`)
  gets the inventory, anyone else hears a refusal sound and gets the
  `#LF-HideoutStash_NotMember` toast, and the refusal is logged
  (`stash refused: not a member`). The server also refuses a non-member moving an
  item out (`HandleAccessValidation`) or in (`CanStoreItem`). Membership is the
  only key: an open or breached door does not open the stash.
- ✅ Stash contents survive a restart through `LF_PersistentStorageComponent`
  keyed `hideout:Roadmap_Hideout` (not an entity id), so the stash keeps its
  items wherever the layer places it. See the placed-storage row under Carried
  inventory.
- ✅ `ExportAll` / `ApplyAll` put the organization id and the lock flag back,
  and a second apply does not change them. The game-mode serializer writes
  these records at version 3. A version 2 save has no hideout payload and does
  not clear a live hideout. Player strings: `#EL-Hideout_NotMember`,
  `#EL-Hideout_NoUpgrade`, `#EL-Hideout_Process`, `#LF-HideoutStash_Name`,
  `#LF-HideoutStash_Desc`, `#LF-HideoutStash_Title`, `#LF-HideoutStash_NotMember`.

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
- ✅ **Toasts are visible on a game client** (2026-11-08, #157): until now no toast
  was ever drawn. The slip's frame slot had a height of -48 (`OffsetBottom -92` under
  `OffsetTop 140`), the fills were `PanelWidget`s (near-transparent) nested inside
  an `ImageWidget` (which draws no children), and the text column's negative
  stretch offsets ran the message past the slip. Now: 420x120 slip, sibling image
  fills, wrapped message, fade driven from `Tick` (`EL_NotificationToastUI.OpacityAt`).
  Seen at 1280x720 on a dedicated client (contraband refusal). New
  `tools/validation/validate-layout-slots.mjs` fails a point-anchored slot whose
  `Offset*` and `Size*` disagree (it also fixed `EL_SurvivalHUD.layout`).
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
  the calling machine only. Since 2026-11-08 the courier job (sign-up, paid,
  abandoned), gather/process rewards, weapon and vehicle robbery, and the
  contraband payout toast through `NotifyEntity`; on a dedicated server they
  had reached nobody. enforce-lint L8 fails an `EL_Utils.Notify` after an
  `if (!Replication.IsServer()) return;` guard.
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
- ✅ A price at or below the lot cash ceiling (500) debits MoneyStack. A higher price debits the ATM balance and does not convert cash. A short payment is refused and charges nothing. Above the ceiling the refusal reads `#LF-Vehicle_BankShort` (pay from the bank; deposit at an ATM), so a buyer holding enough cash knows why (`vehicles/bank-refusal`).
- ✅ Purchase spawns the locked child, sets the buyer as owner, locks it, issues a unique plate, and gives one key. Extra keys are an owner action at the configured cash price.
- ✅ The vehicle record is keyed by the lock identifier. `Export`/`Apply` restore the owner and the key id. `EL_PersistenceComponentSerializer` version 4 writes those records after the hideout records; a version 3 save has hideouts and no vehicle payload. Store keeps the prefab and the lock flag; retrieve spawns on the garage's own pad (`EL_VehicleGarageComponent.m_sPadId` names an `EL_VehicleSpawnPointComponent`), not at the last world coordinates.
- ✅ **Garage it to keep it** (M1-16, #101, decision 2026-11-07; tests `vehicles/impound`, `restart/vehicle-street-impounded`, `garage/ledger-card`): a vehicle survives a restart only when it is stored in a garage. After a load, `EL_VehicleOwnership.ImpoundStreetVehicles` leaves every owned vehicle that was not stored unspawned: it is at the impound lot (`IsImpounded` = owned, not stored, no live entity; derived, so no save field and no version bump). No key is spawned at load. `Roadmap_Impound_Desk` (`246 0 269`, at the gate of the net-fenced yard behind the garage, x 236..256, z 271..283) releases the owner's latest impounded vehicle for `LF_GarageConfig.m_iImpoundFee` (`Configs/Vehicles/LF_Garage.conf`, 250) in carried cash onto a yard pad, once; short cash, a stranger, a full yard, or a second collect is refused and charges nothing. A collect or retrieve hands the owner a key bound to the car when they hold no live one (a restart unbinds carried keys). A purchase is followed by a toast that teaches the rule (`#LF-Garage_BuyHint`); the garage sign and a new `Sign_Impound` board at the desk explain store, retrieve, the ledger, and the fee.
- ✅ `Garage_E_01` (`{A548C1C5370D0734}Prefabs/Structures/Houses/Garage/Garage_E_01/Garage_E_01.et`) is the garage shell. DebugWorld places the use point `Roadmap_Garage` at `238 0 256.6` on `Street_Vehicles.layer`, with store, retrieve, and read the ledger. A retrieved car is parked along the kerb in front of the doors (`254.6`, then 7 m west). Store takes the owner's car parked within 15 m of that garage; the lot is no longer a garage. The shell is its child 5.9 m north, turned so its doors face the street; the player stands in front of the doors. The lot counter is `Roadmap_Vehicle_Lot`: bought cars land on a pad 5 m west and 5 m south of it, then 8 m steps north across the concrete lot. The showroom body is `Roadmap_Vehicle_S105`. (verified 2026-10-11; moved with M1-25)
- ✅ `LF_Garage` holds the park rules and the ledger. Park refuses a seated player or a missing garage, and a vehicle that is not within the same garage's radius as the player (`EL_VehicleGarageComponent.SharesGarage`, `#LF-Garage_VehicleTooFar`, test `security/garage-store-range`), then stops the engine and teleports occupants out before store. Reading the garage or the impound desk sends the reader's rows from the server (`SCR_PlayerController.LF_ShowGarageLedger`) and opens `LF_GarageLedger`, a read-only paper card: model, plate, and where each vehicle is (in the garage, at the impound lot), plus the release fee when one is impounded. The old catalog screen (buy, paint, preview, stats), unreachable from the world and a duplicate of the lot's buy path, is deleted with `UI/Layouts/Garage/LF_GarageVehicleRow.layout`. (2026-11-08)

## UI / Layouts / Localization

### UI kit — `Scripts/Game/Feature/UIKit/`
- Post-release polish. `LF_UITheme` holds the 1989 type, padding, and paper/metal colors. `LF_UIScreen` assembles a frame, title, body line, WLib edit box (ink, no text outline or shadow; the box is not a `TextWidget`), list row, status row, quantity slider, and pause-menu button with `WorkspaceWidget.CreateWidgets`, wires `SCR_ButtonTextComponent`, and activates `MenuContext` every frame (`MenuTextEditContext` while an edit box is focused, `MenuWithEditorContext` so a pad can move). Close removes the widgets and that call. It does not use MenuManager.
- Input (verified 2026-11-07): focus lands on the first non-field control at open, so MenuSelect works without MenuDown first. MenuBack closes. `GetOnButton()` hands the feature the part name of a pressed BUTTON, STATUS action, or list row action. `GetOnClosed()` fires once for any close. A LIST part (`LF_UIList.layout`, `AddListRow`/`ClearList`) scrolls to the focused row. Close defers the removal of its per-frame call by one tick, because it can run inside that call. `ui-kit/initial-focus` and `ui-kit/button-invoker` (LOGIC) cover the pure parts; the on-screen focus ring is visual-only.
- `LF_UIMenuInput` gives the remaining MenuManager menus (ATM, shop, quest, police) and the workspace pickers (faction, spawn location, death, organization) the same input: MenuContext plus MenuWithEditorContext every frame, MenuUp/MenuDown focus ring, MenuSelect on the focused button, MenuBack closes (not on the mandatory faction, spawn and death pickers), and a 150 ms debounce so a click and the poll do not double-fire. The ATM has a Close button and labelled Deposit/Withdraw buttons, tells the player when the amount is blank or zero, and names deposit and withdraw refusals separately. The shop and quest Close buttons work. Shop and ATM moved onto the kit in M1-13 (2026-11-08); their MenuManager presets are gone. The shop, quest, organization desk, and police menu actions are `HasLocalEffectOnlyScript`, so the menu opens only on the acting client.
- A line can be rewritten after open. A status row has one action that hides when the action is empty. A quantity splits one stack into keep and take. The sheet can close on death, on losing the character, or past a distance from an anchor. Nothing in the world opens the sample. `node agent-box/harness.mjs kit` screenshots the sheet, a rewritten balance, a hidden action, both split faces, and focus on the field. It reuses a play session that is already up. Faction, character creation, death, quest, police, and spawn menus were not migrated.
- (2026-11-08, M1-13) Skins: `LF_UIScreen.Open(parts, skin)` paints from an `LF_UISkin` (`Paper()` is the old look and the default, `Terminal()` the ATM, `PriceList()` the shop): frame colours, face, sheet width, blur on or off, and, for skins that ask, the vanilla button text colours (and its arrow glyph hidden). A KEYS part (`LF_UIKeyRow.layout`, `LF_UIPart.CreateKeys`) lays buttons side by side; MenuLeft/MenuRight walk the row and MenuUp/MenuDown keep the column in the next row. `BindHotkey(action, name)` reports an input action as a button press while no field is focused. `SetButtonText`, `SetListHeight`. `LF_UISound.Play` plays a UI sound event. Seen on a dedicated client (agent-box session, 640x360): the frame `Paper` panel draws nearly transparent and the blur darkens it, so paper sheets read dark; the price list turns the blur off to stay legible. The registration form shows the same on that client.
- ✅ (2026-11-08) Opaque paper. The frame's `Metal`, `Paper`, `Rule` and the row plates were `PanelWidget`s, which draw the engine default style's panel image under their colour, so the sheet read dark and see-through. They are untextured `ImageWidget`s now (the vanilla solid-fill pattern, `DialogBackground.layout`), the paper is opaque warm off-white (238, 230, 212), and a full-screen `BackgroundBlur` behind the `Sheet` dims and softens the world. Paper skins paint button labels in ink with a red focus. A field gets an ink caption above it (`<Part>Caption`) because the vanilla placeholder draws light grey. `LF_UIEditBox.layout` now overrides the base `SCR_EditBoxComponent` by its GUID, which removes the stray "Editbox" label and applies the paper-coloured well. The sheet is 900 tall so the price list's Leave and the ATM's EXIT fit. Seen on a session client (WORLD `ui-kit/paper-sheet`, flow `ui-paper`).

### Signs — `Scripts/Game/Feature/Signs/`
- ✅ Diegetic signs (M1-01). `LF_SignComponent.m_sSignId` names a row in `Configs/Signs/LF_Signs.conf` (id, title, what, how, needs, rules, next). A value starting with `#` is a localization key; any other value is shown as written. `LF_SignReadAction` is local-effect only and names itself `Read: <title>` (`#LF-Sign_Read`). Perform opens `LF_SignCard` on the reading client: a UI kit sheet with the title, one line per filled section (What, How, Needs, Rules, Next; empty ones omitted), and Close. Close, MenuBack, death, losing the character, or walking 4 m from the sign closes it. No RPC. An unknown or empty id, or a row with no title, logs `EL_Debug.Error("Signs", ...)` once and the action hides. LOGIC `signs/catalog`, `signs/missing-id`, `signs/localization`; WORLD `signs/prefabs`.
- ✅ Prefabs `Prefabs/Props/Signs/LF_Sign_Board` (cork board), `LF_Sign_Pub` (shop sign), `LF_Sign_Notice` (A4 notice), `LF_Sign_Chalkboard` (standing chalkboard; the vanilla `CardboardSign_01` lives in the Rally addon and does not resolve for this mod) inherit the vanilla props (no new meshes) and add the component, an `LF_Sign` action context on the face, and the Read action. A placed instance sets the id. The title is also printed on the prop on rendering clients: `LF_SignComponent` draws `UI/Layouts/Signs/LF_SignFace.layout` into an `RTTextureWidget` bound (`SetRenderTarget`) to a mesh whose material `Assets/Signs/Data/LF_SignFace.emat` samples `$rendertarget`. `LF_Sign_Board` spawns a local `LF_SignFace` sheet (vanilla A4 poster mesh) on its face; `LF_Sign_Notice` retargets its own poster material. A dedicated server skips the face. `LF_Sign_Pub` and `LF_Sign_Chalkboard` ship with the face off until their sheet placement is checked in a rendering client.
- ✅ The printed title fits the shown part of the render target at any window size. The target is the layout size times the screen DPI scale, but the widgets inside lay out one reference unit per pixel, so a 720p Workbench viewport printed "MINI / ARE". `LF_SignComponent.FaceTitleRect` places the title box and font by `DPIScale(1)`. Board and notice photographed reading MINING AREA, not mirrored, in a Workbench play session (2026-11-08). LOGIC `signs/face-fit`.
- ✅ **Game client face size** (M1-09, 2026-11-08). On a 640x360 dedicated-server client the DPI-scaled title printed tiny in the sheet's top-left third. Without the DPI scaling, the full 432-unit box at font 96 cut WELCOME to WELCO at 640x360 and at 1920x1080 alike: only about 328 of the 512 units across reach the paper. The DPI compensation is now Workbench only (`#ifdef WORKBENCH`); a game client uses `LF_SignComponent.ClientTitleRect` (box x 20..310, font 96 or smaller so the longest word fits). All 18 board titles photographed whole and unmirrored at 1920x1080 on a dedicated-server client. LOGIC `signs/face-client`.
- ✅ **Placed in DebugWorld** (M1-09, 2026-11-08). `DebugWorld_Layers/Signs.layer`
  holds one sign per town manifest site, within 8 m of it, facing the street:
  `Sign_TownMap` at the spawn square (`town_map`, "Welcome to Everon": the
  loop in order and where each place stands, left and right walking east),
  then post office, ATM, bank, general store, market, vehicle lot, garage,
  police station, jail (inside the yard), fuel station, hospital, organization
  office, hideout and convoy drop (in the yard), convoy pickup at the barn, and
  farm, all `LF_Sign_Board`. The back room is an `LF_Sign_Chalkboard` with no
  printed title. `Sign_PoliceWhitelist` places `EL_WhitelistSign` beside the
  whitelist station. Cards are `#LF-Sign_<Site>_<Field>` keys: what, how with
  the exact action names, needs, rules, next (where to go after this place,
  so a stranger can follow the loop from sign to sign; LOGIC `signs/next`), with the DebugWorld numbers (prices,
  pay, cooldowns). WORLD `signs/coverage`: every placed sign resolves a titled
  catalog row, every catalog row is placed, and each title and What resolves.
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
- ✅ **Guided arrival chain** (M1-18, 2026-11-08): `QuestGiver_Clerk.et`
  (`Quest_Clerk` on the square, 134 0 243.5) runs five steps: `arrival_register`
  (talk; hands over the ID card), `arrival_board` (`SIGNAL sign.read:town_map`),
  `arrival_work` (`SIGNAL job.run_complete:parcel`), `arrival_bank`
  (`SIGNAL atm.deposit`), `arrival_done` (talk; $250 and "the town is yours" with
  pointers to the rest of town). Steps 2-4 start and finish on their own
  (`m_bAutoAccept`, `m_bAutoTurnIn`), so the player never walks back between
  errands; each new step's errand shows as a 12 s hint. A fresh spawn without the
  card is pointed at the clerk. A giver's menu lists one chain step at a time and
  never re-lists a one-off quest already turned in (`LF_QuestDefinition.IsListed`),
  so a returning player is not offered the chain; a player who left mid-chain
  gets the next step back on spawn (`SettleChain` after `RestoreTurnedIn`).
  Accepting at a giver counts that giver's talk objective at once. No waypoints.
  The clerk is a converse giver (`m_bConverse`): "Talk to the clerk" opens no menu; one talk
  turns in what is done, takes on the next quest, and the clerk's lines show as slips
  (`RequestConverse`); mid-errand he repeats the errand, after the chain his idle line
  (`m_sIdleKey`). A second NPC with a placed giver's id no longer takes the giver over.
  Harness: `agent-box/flows/arrival-chain.txt`; `state` adds `idcard=` and `quests=`;
  `stand`/`use` reach a replicated NPC (no layer name on a client).
- ✅ `SIGNAL` event type with a string key (`LF_QuestManager.ReportSignal`).
  Hooks in `LF_QuestHooks.c` (modded classes; the job, ATM, and sign files are
  untouched): `LF_JobDropoffAction` reports `job.run_complete:<job>` for a paid
  run, `EL_CharacterATMComponent.ReplyResult` reports `atm.deposit` for a
  successful deposit, and `LF_SignReadAction` asks the server (`RpcAsk_SignRead`)
  to report `sign.read:<id>`, counted only with that sign within 6 m.
  `QuestGiver_Start` (test quests, "Test quest board") moved to the test annex
  (`Quest_TestGiver`, 104 0 70). Tests: `quests/signal-event` (LOGIC),
  `quests/arrival-chain` (WORLD).

### Layouts (loaded by `EL_Test_Data`)
- ✅ `EL_SplitQuantityDialog`, `EL_CharacterCreationMenu`, `EL_ATMMenu`,
  `EL_SurvivalHUD`, `ShopMenu`, `PoliceMenu`,
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
- ✅ DebugWorld is a one-street 1989 Everon village (M1-25, 2026-11-08;
  `docs/roadmap/m1-25-village.md` has the map). The main street runs east-west
  at z = 250 on the flat 512 m terrain (world 0..512, `TERRAIN_MAX` in
  `tools/street/manifest.mjs`; every heightmap sample is 0, so ground props sit
  at Y = 0). Sites stand in arrival order in front of vanilla buildings: the
  square (spawn, clerk, pub, church), post office, bank, general store and
  market, vehicle lot and garage, police station with a jail yard, hospital and
  fuel station, org registry, and the hideout down a side alley; the contraband
  barn is out of town on the east track. The farm, quarry, builders' yard and
  forest edge are on the outskirts. `Village_Roads`, `Village_Buildings` and
  `Village_Dressing` hold the roads (road generator splines saved as
  `RoadEntity`), vanilla buildings, walls, lamps and trees; `Annex.layer` is
  the net fence of the test annex at x 20..112, z 20..112, which holds the test
  runner, the PR fixtures, and the legacy debug stations (with their
  boundaries) on the ground where world tests spawn their characters. Sites
  keep their names; chain layers moved as blocks. The terrain material
  (`Assets/World/GridSurface.emat`, name kept so the terrain reference holds)
  is the vanilla lawn surface without clutter, so grass does not grow through
  roads and floors.
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
  the body before the picture. `SetCamera` does not move that view. (verified 2026-10-11) On a
  dedicated server the server also sends the pose to the owning client
  (`RpcDo_EL_HarnessPose`), which would otherwise overwrite it; session
  clients take world shots this way (verified 2026-11-08, M1-25). A filtered row's wide shot
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
- ✅ A world-shot pose turns off damage handling on the posed character for
  8 s, so the fall from the camera height no longer kills it (a 4.5 m pose
  ended on the death screen; verified 2026-11-08 with an 18 m row shot).
  `shot world <name> x y z ox oy oz` takes a close camera offset. The poll
  restarts when a new game mode appears, so a second Play in the same
  Workbench no longer answers `no_session`. The command file is shared by
  every default-profile session on the machine; the `agent-box-play` lock
  ties it to one checkout's Workbench or game process.
- (2026-11-08, #108) Screens: `click` accepts every UI kit button by the name
  the screen gave it (`LF_UIButtonRelay`: ATM keys, `IdClose`, the sign card's
  `Close`, the garage's `Buy`). `ui text` reads a sheet as `kind:name=text`
  (button labels, typed fields, printed lines, `#keys` translated);
  `wait-text` polls it; `ui-shot` is `shot ui`. `stand` turns the character to
  face the entity. `session.mjs up --mission playtest` runs `LF_PlaytestTown`
  with `-scrDefine LF_HARNESS` on server and clients: harness RPCs pass
  `EL_DevGate.CheckHarnessRequest` in that build only, the debug menu stays
  closed, a client without the define is refused by the script checksum, and
  `host start` never passes it. `session.mjs restart` (playtest) stops with the
  host stop file (SHUTDOWN save), loads, and rejoins the same profiles.
- (2026-11-08, #108) `node agent-box/journey.mjs` plays the M1 journey on a
  two-client session from the player's view (job, parcel run, ATM keypad, price
  list, car, give cash, till robbery and alert, org desk, garage, and on the
  playtest mission a restart), judges `state` before and after each step, and
  writes `agent-box/reports/journey.html` with the client screenshots.
  `tools/validation/agent-box-selftest.mjs` runs the harness, session, and
  journey selftests in `validate`.
  Every step ends with a picture from each client it touched. Clients default to 1280x720 (`session.mjs up --res 1920x1080` for evidence) on the hidden desktop; `--visible` plus `journey.mjs --film` records client 1, but the first clip held one frame (README, Filming).

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
  card) fail the row: `requireSigns` is true since M1-09 placed the signs.
  The spawn site is the named civilian town spawn `Spawn_Town`; the clerk
  (`QuestGiver_Start`, prefab mode) is a site so the square meets the 30 m
  neighbour rule.
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
