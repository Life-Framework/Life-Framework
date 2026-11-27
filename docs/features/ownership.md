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
- ✅ (2026-11-23, GA-C5 #4214) Decline sits beside Join on the action list: slot `org_decline`
  (`LF_OrgDeclineRole`, `Feature/Organizations/LF_OrgInviteActions.c`), shown exactly when
  `org_invite` reads Join. `LF_OrgInvites.Decline` drops only the offer of the one faced (a
  wrong inviter or a lapsed offer is refused and logged); both players get a toast and the
  invitee's offer copy clears. All four entries (invite, join, decline, remove) are
  `LF_PlayerRoleAction` rows in the character's vanilla action list. LOGIC `org/invite-actions`.
- ✅ (2026-11-12, #750a) Ranks are a ladder with a rights bitmask
  (`LF_OrgRights`; bit = `LF_EOrganizationPermission`: withdraw, invite, kick,
  buy upgrade, manage ranks, use stash, and the reserved declare war, manage
  territory, spend fund). A new organization gets founder (every right), officer
  (invite, kick, declare war since #2769, stash) and member (stash); an invite joins on the bottom rung.
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

- 🧪 (2026-11-22, #4213, gangs C2) **Gang flag, stored and saved.** Each organization flies one of
  20 preset flags, numbered 1 to 20 (`LF_OrgFlags`, `Feature/Organizations/LF_OrgFlag.c`); 0 is no
  flag. `LF_OrgFlags.Set` (server, a registered organization, 1 to 20) picks it; `FlagOf` reads it.
  A side table keyed by organization id, so the positional `orgs` record does not move. Saved in the
  `orgflags` section (`LF_OrgFlagSavePayload`, one `LF_OrgFlagRow` per organization). After every
  load each organization flies a flag: one with no row takes the first free flag (the lowest no
  other registered organization holds), in organization id order, so a second load gives the same
  flags. A save from before the section (`OnAbsent`) keeps the live flags (none on a boot) and gives
  the rest the first free flag. With all 20 held, a new one shares flag 1 (warning). Nothing here
  refuses two organizations the same flag: the choosing rule (decision N21, the flag wall) decides.
  A row whose organization is not on the register is kept and counts for nothing. Nothing
  replicates yet. PERSISTENCE `org/flag-save`; `wt e2e` carries the e2e organization's flag 13.
- 🧪 (2026-11-23, #4220, gangs C3, decision N21) **Flag lock.** `LF_OrgFlagRules`
  (`Feature/Organizations/LF_OrgFlagRules.c`): the first gang keeps its flag. `Pick` (server) refuses a
  flag another organization holds while one of that holder's members is online; the holder offline,
  the pick goes through and both fly the flag. `CanPick` is the same check without the write, for the
  flag wall's "Taken by X (online)" line. When two organizations fly one flag and both have a member
  online, `FliesPlainCloth` is true for the later one: its poles fly a plain cloth in the flag's
  colour (the pole reads it; nothing renders yet). "Online" is a list of character ids (`PickLive` and
  `FliesPlainClothLive` read the live players). "Later" is the claim order of `Pick` in this session;
  an organization with no claim yet counts as earlier, and a tie ranks by organization id, so after a
  restart a pair sharing a flag ranks by id until someone picks again (the order is not saved).
  Re-picking its own flag changes nothing. LOGIC `org/flag-lock`.
- 🧪 (2026-11-23, #4449, gangs C4) **Flag wall.** `LF_OrgFlagWall` (`Feature/Organizations/LF_OrgFlagWall.c`,
  `LF_OrgFlagWallAction.c`, prefab `Prefabs/Organizations/LF_OrgFlagWall.et`): the wall is the menu, no
  window. It stands the 20 sample flags (each screen, never a dedicated server) and carries 20 actions,
  one per flag, "Fly the <name> flag". The action shows to a leader (the right to manage ranks, so the
  founder) and the server checks again: `Pick` returns `PICKED`, `KEPT` (own flag), `NO_ORG`,
  `NOT_LEADER`, `BAD_FLAG` or `HELD` (the holder named in the toast), reusing `LF_OrgFlagRules`
  and `LF_OrgFlags`. Samples come from prefab slots `gang_flag_01` to `gang_flag_20`, whose rows the pack
  writes (`LF_PrefabSlots_Everon1989.conf`); the core slot file lists none, so `LF_OrgFlagWall.SamplePrefab`
  reads the layered slot config itself, and the gang-pole card reuses it. Flag names are neutral core
  strings `LF-OrgFlag_Name_01..20` that the pack overrides with the real names. Placed in a grid cell
  (`flag-wall`); the Saint-Philippe registry placement is below (#4457 follow-up). WORLD `org/flag-wall-actions`, pack
  `everon/flag-wall-slots`.
- 🧪 (2026-11-23, #4457, gangs C14) **Registrar voice.** The pack's registrar, Gaspard Perrin (`LF_Npc_Townsfolk_Registrar.et`,
  dialogue `registrar`, ElevenLabs, six lines), talks like any dialogue NPC: the founding line "Name, flag, and two hundred
  fifty dollars." and how flags work. After a pick at the flag wall `LF_OrgFlagWall.PickBy` finds the registrar within 25 m
  and `LF_Dialogue.Announce`s his `for_key` line for the toast key: `PICKED` says "That is your flag", `HELD` ("another crew
  flies it") says "That one is taken". Other results stay toast-only; no registrar near logs `[LFDebug:OrgFlagWall]` and the
  toast stands alone. The voice manifest is `tools/assets/dialogue/registrar/elevenlabs.json`. Pack `everon/registrar`.
- 🧪 (2026-11-24, #4457 follow-up) **Registry placement.** The org slot of Saint-Philippe (`tools/town/kits/org.json`, cabin b49)
  now holds the registrar (`Fit_saint_philippe_org_registrar`, an `LF_NpcBaseEntity` standing behind the chair, facing the desk) and the
  flag wall (`Fit_saint_philippe_org_flag_wall`, on the open ground against the cabin's south wall; its twenty samples fill
  the grass in front of it). The wall board is about 2.5 m from the registrar and its farthest sample under 8 m, inside the 25 m a pick is voiced from. His Talk
  context is 2.4 m (counter staff, `validate-action-reach`) so the prompt shows across the desk. Session proof
  `agent-box/session.mjs registrar-proof` (talk line `[LFDebug:Dialogue] player 1 talks to 'registrar'`, then the pick logs
  `'registrar' says 'picked'` and the client plays `registrar/picked`).

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
  lockpick opened. A door left open now closes itself (#2472).
- ✅ **Lock and unlock at the door** (#2471, WORLD `houses/lock-action`): `LF_HouseLockAction` on
  `EL_Door_Base.et` offers `#LF-HouseLock_Lock` / `#LF-HouseLock_Unlock` to the owner and to a carried
  key holder (`LF_LockComponent.UserMayEnter`); a stranger does not see it. The server re-checks and
  refuses a forged request with `[LFDebug:Houses]`, then flips the house through
  `LF_HouseManagerComponent.SetHouseLocked` (saved, mirrored on every door). Not offered on a hideout
  door, on a door with no house manager, or while Houses is off.
- ✅ **A locked house door left open can be closed and closes itself** (#2472, WORLD `houses/door-auto-close`):
  on a locked house anyone may close an open door, only the owner or a key holder opens it (the server
  re-checks in `PerformAction`), and the server closes a door left open after `LF_HouseDoorClose` delay
  (the owner's `houses.doorCloseDelaySeconds`, default 20 s, 0 turns it off, over 600 clamps to 600 with a warning;
  `/configreload` applies it to the next door that opens, #2505, WORLD `houses/door-delay-config`). An unlocked house's door and a
  hideout door are untouched; the lock never changes.
- ✅ **Lock state, owner and carried keys survive a restart** (#2410, PERSISTENCE `houses/registry-persist`,
  `houses/carried-key-persist`, `wt e2e`): `LF_Houses` keeps one row per house with a stable identifier
  (the editor identifier, or `BindIdentifier`) in the `houses` payload (save version 46); a carried house
  key keeps the house it opens in the `itemkeys` payload (version 47). A house with no editor identifier
  gets a per-boot id and is not saved (logged). Nothing assigns an owner in play yet (purchase flow).
- ✅ **Every house and car key shows its own name on every client** (#2470, WORLD `houses/key-item-name`):
  `LF_KeyItemUIInfo` (an `SCR_InventoryUIInfo` on `HouseKeyItem.et` and `VehicleKey.et`, so `LF_PoliceCarKey.et` too)
  builds "House Key [id]" / "Vehicle Key [id]" per item from the replicated identifier (`#LF-ItemName_HouseKeyOf`,
  `#LF-ItemName_VehicleKeyOf`); nothing writes the shared prefab `UIInfo` any more. A late joiner gets the identifier with
  the item. An unbound key shows the plain prefab name. No save change.

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
  item out (`HandleAccessValidation`) or in (`CanStoreItem`). `CanStoreItem` has no
  instigator, so it only takes an item from a living member's own inventory, the stash
  itself or a server spawn (restore, `AddAmount` seeding, a stack split, all inside
  `LF_ServerInsertScope`); an item in a vehicle, crate or body, or lying on the ground, is
  refused even for a member, who carries it first (#2574, #2584, WORLD `hideout/stash-put-carrier`;
  log `stash <id> refused <prefab> from the ground`). Membership and the
  rank's stash right (`USE_STASH`) are the key: an open or breached door does not open the stash.
- 🧪 **The server-insert bypass is per item** (#2835, WORLD `inventory/server-insert-scope-aborted`):
  `LF_ServerInsertScope.Begin(item)` opens the bypass for that one item only, and the stash and the
  locked-car gate ask `IsOpenFor(item)`. A script error between `Begin` and `End` cannot leave a locked
  car or a stash open to other items, and the frame-end sweep closes what it left open (log
  `server insert scope left open for <n> item(s) by an aborted insert; closed`).
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

- ✅ **Organisations review fixes (2026-11-17, `docs/reviews/2026-11-17-orgs.md`).** The seven ledger
  requests and the ledger reply now have a listen-host path (#2525): the host's own ask calls its
  handler and the reply is applied directly when the server owns the character. The ledger no longer
  sells extra storage or the parked vehicle (`LF_OrganizationManager.IsUpgradeWired`): nothing reads
  those flags, so a crafted request burned the fund for nothing. A deposit that would pass the fund's
  int ceiling is refused (the cash goes back). The stash checks the rank's `USE_STASH` right, which
  the rank config could already take off. A hideout gate saved open queues the relock sweep on load,
  so a restart no longer leaves it open for anyone. Tests `organization/upgrade-unwired`,
  `organization/fund-overflow`, `hideout/stash-right`, `hideout/load-relock` (LOGIC). The lease that
  never ends is #2533.

### `LF_PropertyConfig` — `Feature/Property/` (config only, #2647, slice P2 of #747)
- ✅ One config, `Configs/Property/LF_Property.conf`, describes the plot catalogue (id, tier id, price), the house
  tiers and what each build stage needs. A tier has four stages in order (excavation: work only; foundation:
  gravel and cement bags; walls and roof: planks and bricks; doors and windows: planks and a door), each with
  work units and a list of prefab and count requirements, plus `m_aShellPrefabs` (the vanilla prefab shown after
  each stage). `m_iRentIntervalHours` and `m_iGraceHours` are 0 = owned outright (no rent in M8 wave 1). The core
  ships generic example plots; 1989 plots and prices are the pack's (#808). Nothing reads it in play yet.
  Getters (`PlotPrice`, `StageCount`, `StageWork`, `RequirementCount`, `Requirement`, `ShellPrefab`, ...) return
  0, empty or null for an unknown id; a missing config logs `LF_Debug.Error("Property", ...)` and degrades to the
  same defaults. Test `property/config` (LOGIC).
- ✅ **A plot key equal to a house identifier is refused** (#2783, merged, in-dev). `LF_Properties` keys houses and
  plots in one table and a house wins, so such a plot would sell the house at the plot price. At config load
  `LF_PropertyConfig.RefuseHouseCollisions` drops every plot whose key is a known house (`LF_Houses`), and a house that
  registers later drops the loaded plot (`RefuseHouseId`, called from `LF_Houses.Register`); each logs
  `[LFDebug:Property]` at Error naming the key, and the house and the other plots are untouched. The validate check
  `validate-plot-house-ids` fails a PR whose core or pack `Configs/Property` conf names a plot like a house
  identifier set on an `LF_HouseManagerComponent` or a door `LF_LockComponent` in a layer or prefab. Test
  `property/plot-house-collision` (LOGIC).
- ✅ **Ledger asks are rate-limited; the test sign only leaves its own org** (#2536, review items 30 and 31 of #2415,
  merged, in-dev). View, Deposit, Withdraw, Leave and Lease now go through `LF_OrgLedgerGate` (`LF_OrganizationCharacter.c`)
  on the existing `PURCHASE` bucket, the same as founding and upgrades (View walks every member, so a spammed ask cost
  a pass over the whole roster). The first refusal in a window logs `[LFDebug:RateLimit]` and toasts; a refused ask
  returns no page. The dev-gated test sign's Leave refuses with `OTHER_ORG` (`#LF-TestOrgSign_OtherOrg`) unless the
  player's org is the sign's own "Test Organisation", so it can never strip a real org of its founder. Test
  `organizations/ledger-rate` (LOGIC).
- 🧪 **A hideout lease ends with its organisation's last member** (#2571, part of #2533, merged, in-dev):
  when the last member leaves or is kicked (`LF_OrganizationManager.Leave`, `LeaveDirect`, `Kick`) the
  organisation publishes `LF_OnOrganizationEmptied` (`Feature/Organizations/LF_OrganizationsEvents.c`)
  and `LF_Hideout` releases every site it leases (`ReleaseAllOf`, logged
  `[LFDebug:Hideout] released hideout=… reason=organization emptied: …`). There is no disband verb
  (#2415), so this is the disband; the organisation record stays on the register. `PushLock` now writes
  the lock's house identifier on every push, empty included: a released door reads *Unowned* to everyone
  (`LF_HideoutDoorRules.View`), former members no longer see *Open*, and an empty id matches no house in
  `LF_HouseLockAction`. The `org_base` place goes with the lease and another organisation can lease the
  site. Test `hideout/lease-empty-org` (WORLD).
- 🧪 **A hideout lease lapses after 14 real days with no member online** (#2573, part of #2533, merged, in-dev):
  each lease keeps the last time (unix seconds) a member of its organisation was connected, saved in the
  `leaseidle` payload (save version 50). A server sweep every 5 minutes (`LF_Hideout.SweepIdle`) restarts
  the clock of a lease whose organisation has a member connected (the site does not have to be visited) and
  releases a lease idle longer than `LF_OrgConfig.HideoutLeaseIdleDays()` (owner key
  `organizations.hideoutLeaseIdleDays`, default 14, 0 = never), logged
  `[LFDebug:Hideout] released hideout=… org=… reason=idle N days`; the release frees the site like the
  emptied-organisation release. An old save seeds the clock at the first load, and a saved clock already past
  the limit at load (the server was down) gets one more day. Test `persist/hideout-lease-idle` (PERSISTENCE).
- 🧪 **The ledger lets a holder give the lease up** (#2572, part of #2533, merged, in-dev): a member whose rank holds `BUY_UPGRADE` sees **Give up the lease** on the hideout row while the yard is theirs (`LF_OrgLedger.HideoutAction`; the same button is **Take the lease** when it is free). The ask is `LF_AskOrganizationLeaseRelease` / `RpcAsk_LF_OrganizationLeaseRelease` (`LF_OrganizationCharacter.c`, desk-only, PURCHASE rate bucket); the server calls `LF_OrgHideoutLease.Release`, which checks the right and that the organisation holds the lease and then `LF_Hideout.Release(..., "given up")` (gate shut, held ground lost). A refusal (no right, not our lease, away from the desk) toasts the player and logs `[LFDebug:Organizations] lease release refused: ...` as a warning. Test `organization/lease-release` (WORLD).
- 🧪 **Every organisation hideout door is a base site** (#3408, part of #3339, merged, in-dev): the desk lease
  no longer looks up one fixed place. Each `LF_HideoutDoorComponent` with **organization hideout** set marks
  itself through `LF_Hideout.MarkBaseDoor` (place kind `HideoutBaseDoor`, its own hideout id); the first is
  still the `OrgHideout`/`primary` place contraband starts from. The desk offers an organisation
  `LF_Hideout.BaseSiteFor`: the base it already leases, else the first free site (primary first, then by id).
  The one-base-lease limit stays (#2816). Saves were already keyed by each door's own hideout id, so an old
  save loads unchanged. Test `hideout/lease-per-site` (WORLD).
- ✅ (2026-11-19, #3410) Saint-Philippe has a registry: the site cabin `b49` beside the workshop row holds a clerk's
  desk (`LF_OrganizationDeskAction`), an office chair and the war board (`LF_OrgWarBoard.et`), with a "Registry"
  plate on the outside wall (core sign `org_registry`). Fitted from the `org` kit by `town fit saint-philippe org`
  into `Place_saint-philippe_org.layer` (place kind `OrgDesk`); a gang can now form on the island, not only in
  DebugWorld and the grid.
- 🧪 (2026-11-24, #4180, town-draft-saint-philippe-org-flagwall) **The flag wall stands in even rows, upright and clear of each other.** The twenty samples were on a 1.2 m grid, closer than a waving cloth is wide (about 1.5 m), so the cloths piled into one heap and their poles read as tilted. `LF_OrgFlagWall` now sets them on a 1.9 m pitch with 2.2 m between rows, every other row slid half a pitch so the row behind shows between the cloths in front (an 8.6 x 5.9 m field, still about 12 m from the registrar at its far corner), and each sample stands on the ground under its own spot (`GetSurfaceY`) instead of at the board's height. The prefab's twenty action contexts moved to the same grid. The registry plate on the cabin wall already prints "Organization office" in a client (`look` draws it blank), so it is unchanged.
- 🔧 (2026-11-26, registrar-proof-topup, tooling) **`registrar-proof` no longer needs a newcomer's $200.** It read the carried cash and, under the $250 founding fee, topped the character up with the harness `resetplayer 300 keep` (the ATM withdrawal it used to rely on is gone); when the top-up is refused it prints `SKIP` with the reason instead of failing at the founding. `--selftest` runs the pure `topUpTarget`. Run live from a $100 character: topped up to $300, founded, $50 left, flag picked.
- 🔧 (2026-11-27, playtest-1127-r4) **The org ledger's lease row is neutral.** The row, the take toast and the give-back toast said "alley yard", the old playtest village's name, while the lease on Everon is the Farm Compound yard (the ledger's own "Held ground" line says so). The en_us rows now say "Yard lease", "The yard is yours. Members open its gate." and "You gave the yard back. Its gate is shut." (core text keeps no place names). Pictures: `website/public/docs-images/playtest-1127-r4/ledger-alley-yard-before.png`, `ledger-yard-fixed-after.png`. Merged, in-dev.
