# Ownership

### `LF_OrganizationManager` — `Feature/Organizations/` (verified 2026-10-11; ledger and invites 2026-11-08)
- ✅ A character founds one organization. The id comes from `PersistenceIdUtils`,
  never an entity id. The founder invites and kicks. A member can leave. The
  founder cannot leave or be kicked. A second membership is refused.
- ✅ The shared fund is a balance keyed by organization id, the same shape as
  `LF_BankAccount`. Deposit credits it. Withdraw requires the withdraw rank and
  takes the whole amount or nothing. Buying an upgrade (extra storage,
  processing slot, parked vehicle) debits the configured price in one step or
  leaves the fund and the flag untouched. The price and the new flag are logged.
- ✅ Cash moves only as MoneyStack through `LF_OrganizationPlayer`. A short
  removal is returned to the character. A short payout puts the fund back. The
  personal ATM balance is not read or written. The desk is
  `Roadmap_Org_Desk` on `Street_Org.layer` at `288.9 0 255.7` (org registry porch), using
  `{6B7B7CCC74AACA67}Prefabs/Props/Furniture/Desk_01_short.et`.
  The desk mesh origin is its back-right corner (bounds x -1.70..0, y 0..0.907, z 0..0.747), so the top is at y 0.907 and the props sit at x 287.6..288.6 (`Roadmap_Org_Desk_Phone/Files/Notebooks/Clock`, `_Chair`).
- ✅ (2026-11-08, M1-17) The desk opens the registry ledger `LF_OrgLedger`, a
  1989 ledger page on the UI kit (the old `OrganizationMenu.layout` is gone).
  No organization: name field and "Sign and pay $250"; the fee
  (`LF_OrgRules.FOUNDING_FEE`) leaves carried MoneyStack and comes back if the
  registration is refused. Names: 3 to 32 letters, digits, spaces, `- ' & .`,
  unique ignoring case and outer spaces (`LF_OrganizationManager.IsNameTaken`).
  A member's page: name, rank, fund with deposit (and withdraw for the rank
  that may), the processing upgrade, the alley yard lease
  (`LF_OrgHideoutLease`, free, founder takes it), the member list (offline
  members by their record name), and Leave (not for the founder). Found,
  deposit, withdraw, buy, lease, and leave are refused unless an
  `LF_OrganizationDeskAction` stands within `LF_OrganizationPlayer.DESK_RANGE`
  (5 m) (`#EL-Org_TooFar`; test `security/service-range`). The page closes at
  4 m from the desk, on death, and on MenuBack. Sounds: page turn on open,
  pencil on Sign, map close on close. LOGIC `org/registry`, `org/invite-role`.
- ✅ (2026-11-08, M1-17) Invite and remove are face to face, in the `org_invite`
  slot of the player interaction menu (`LF_OrgInviteRole`, registered in
  `LF_GameModeRoleplay.OnGameStart`). A leader facing someone with no
  organization sees "Invite to <org>" (offer, `Gesture_Point`); the invitee
  facing that leader sees "Join <org>" and joins with one press (raised hand,
  hand-over sound); a leader facing a member sees "Remove from <org>"; nobody
  can remove the founder. An offer is session state (`LF_OrgInvites`, not
  saved) and lapses after 2 minutes. The offer side reads the organization
  replicated on each character (`LF_OrgReplication`: id, name, rights bits,
  held offer; RplProps written on spawn, on change, and every 10 s); the
  server decides from the registry. Two-client proof:
  `node agent-box/session.mjs org [--shot]`.
- ✅ (2026-11-12, #750a) Ranks are a ladder with a rights bitmask
  (`LF_OrgRights`; bit = `LF_EOrganizationPermission`: withdraw, invite, kick,
  buy upgrade, manage ranks, and the reserved declare war, use stash, manage
  territory, spend fund). A new organization gets founder (every right), officer
  (invite, kick, stash) and member (stash); an invite joins on the bottom rung.
  Face to face, a member with manage ranks sees "Promote in <org>" on a member
  whose next rung up is still below them and "Demote in <org>" on one who has a
  rung below (`org_promote` / `org_demote` slots, `LF_OrgRankRole`); nobody
  reaches their own rung or the founder's. Kick also needs the actor to outrank
  the target, so an officer removes members but not another officer. Every move
  is re-checked on the server (`LF_OrganizationManager.Promote/Demote/Kick`) and
  logged with the new rights. The ledger shows the rank's own name. The view
  replicates the ladder position. LOGIC `org/rights`.
- ✅ `ExportAll` / `ApplyAll` replace the cache, so a second apply does not
  compound the fund. The game-mode serializer writes these records at version 2,
  after survival. A version 1 save has no organization payload and does not
  clear a live fund. Version 3 appends hideout ownership after these records.
  Version 4 appends owned vehicles after the hideout records. Version 5 appends
  serving jail sentences after the vehicles.
- ✅ (2026-11-12, #750a) Save version 27 appends `orgranks`: each rank's whole
  rights mask and ladder order, keyed by organization id and rank id. The
  `organizations` record layout is unchanged (its rank still saves four bools,
  bits 0-3 of the mask); the record version is 2. A version 26 or older save has
  record version 1: load turns the bools into masks (founder every right, any
  other rank its bools plus the stash), adds the officer rung, and keeps every
  member on their rank. PERSISTENCE `org/rights-persist`; `wt e2e` carries a
  founder, a promoted officer and a member across the restart.

- ✅ **World-end reset** (2026-11-17 property review): `LF_OrganizationManager_StaticResetHook` drops the
  singleton, so a world loaded after another in one process no longer keeps the last world's
  organizations, members and funds (checked by `map/org-marks-character-id`).

### Houses — `Feature/Houses/`
- ✅ **Init runs** (2026-11-17 property review, WORLD `houses/identifier-init`): `LF_HouseManagerComponent`
  and an editor-bound `LF_KeyComponent` now set the INIT event mask, as `LF_LockComponent` and
  `LF_VehicleKeyComponent` do. Before, a building never got its identifier or synced its doors, and
  an editor-bound house key (`HouseKey_Matching_Debug`) stayed unbound.
- ✅ **A keyed open keeps the lock** (#2410, WORLD `houses/keyed-open-keeps-lock`): the owner and a
  key holder pass a locked house door and the door stays locked, so a stranger after them is still
  refused. The server decides in `SCR_DoorUserAction.CanBePerformedScript` (`LF_LockComponent.UserMayEnter`);
  the owner's character key is replicated on the door so a client's prompt matches. Only
  `LF_HouseManagerComponent.SetHouseLocked` changes a house's lock, and it re-syncs a door a breach or a
  lockpick opened. No in-world lock/unlock action yet, and a door the owner opened stays open (#2435).
- ✅ **Lock state, owner and carried keys survive a restart** (#2410, PERSISTENCE `houses/registry-persist`,
  `houses/carried-key-persist`, `wt e2e`): `LF_Houses` keeps one row per house with a stable identifier
  (the editor identifier, or `BindIdentifier`) in the `houses` payload (save version 45); a carried house
  key keeps the house it opens in the `itemkeys` payload (version 46). A house with no editor identifier
  gets a per-boot id and is not saved (logged). Nothing assigns an owner in play yet (purchase flow).
- ⚠️ House key names: `LF_KeyComponent` renames the prefab's shared `UIInfo`, so the name is wrong on
  clients and would be shared between keys; needs a per-item inventory info class (#2435).

### `LF_Hideout` — `Feature/Hideouts/` (verified 2026-10-11)
- ✅ One building owned by one organization. Members unlock
  `Roadmap_Hideout_Door` at `213 0 120`. A non-member is refused and the
  refusal is logged. The door is `EL_Door_Base` with the inherited
  `LF_LockComponent`. The organization id is written onto that lock. The door
  does not call `LF_Breach` and does not add a second `ActionsManagerComponent`.
- ✅ (2026-11-08, M1-17) An organization takes the yard (the door marked `m_bOrganizationHideout`, id `Roadmap_Hideout`, found through `LF_Places`;
  gate `302.4 0 288` since M1-25) with "Take the lease" on the registry ledger; taking it locks the gate.
  The gate prompt on a client compares the organization replicated on the
  character with the owner on the lock and shows `#EL-Hideout_NotMember` to a
  non-member; the server still decides from the registry.
- ✅ `Roadmap_Hideout_Machine` at `216 0 120` uses the existing
  `LF_ProcessAction` on `ItemProcessor_Base`. It runs only when the player's
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

