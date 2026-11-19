# Property

### `LF_Properties` — `Feature/Property/` (2026-11-17, #2646, spec #756)
- 🟡 The server keeps one `LF_PropertyRecord` per house or plot, keyed by its stable id (the house
  identifier or the plot key, never coordinates): owner, organisation, layout, build stage, work done,
  deposited materials (`prefab=count;...`), purchase time, rent paid until (0 = owned outright) and
  whether the owner spawns there. `Create`, `SetStage`, `SetOwner`, `GetOwnerKey`, `ExportAll`, `ApplyAll`.
- 🟡 A house's owner of record stays in the `houses` payload (`LF_Houses`); `GetOwnerKey` reads it from
  there and `SetOwner` sets it on a house in the world. A plot's owner is the property row's.
- 🟡 Saved as the `properties` sidecar payload (save version 51, `docs/persistence.md` item 38). A row
  whose property is not in the world waits and is saved again. A version 1-50 save loads an empty book.
  Proof: `property/record`, `property/persist`, `wt e2e`. No gameplay uses the book yet (later M8 slices).

### `LF_BuildStageRules` — `Feature/Property/` (2026-11-18, #2648, spec #756)
- 🟡 Pure rules for a build stage (`LF_PropertyStageDef` plus the record's `prefab=count;...` string and
  work done): `Missing` and `MissingText` per item, `ItemsMet`, `WorkRemaining`, `WorkAfter` (clamped, work
  is the site's so every worker's units add), `RoundsLeft` (reuses `LF_StationProgress.Rounds`), and
  `IsDone`, true only when items and work are both met.
- 🟡 `Deposit` accepts only the owner and invited key holders (`MayContribute`, grief rule #807). A deposit
  past the need is refused whole and names the item and the overflow; a refused try changes nothing.
- 🟡 Deposited items are counts, never entities; `Parse`, `Format` and `CountOf` read and write the string.
  Nothing calls the rules yet (later M8 slices). Proof: `property/build-stage-rules`.

### Property config reads pack overlays — `Feature/Property/LF_PropertyConfig.c` (2026-11-18, #2976)
- 🟡 `LF_PropertyConfig.Get()` merges every pack overlay the mission header lists (`m_aLF_ConfigOverlays`, load order)
  over `LF_Property.conf`: the plot catalogue is an `LF_ConfigList` keyed by `m_sPlotId`, so a pack plot with the
  same id patches the core price or tier, a new id adds a plot (needs a tier id), and `remove` takes one out
  (`docs/config/override-model.md`). Tiers stay core-only. A pack plot whose key is a house id is refused like a
  core one (#2783). A path override (tests) reads that file alone. Proof: `property/pack-overlay`.

### Saint-Philippe plot signs — `LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Plots_saint-philippe.layer` (2026-11-18, #2936)
- 🟡 The four free plots of the scout dossier (p1 to p4) each carry an `LF_PlotSign.et` on the island, keys
  `saint-philippe.p1` to `.p4`, priced by the pack overlay `Configs/Property/LF_Property_Everon1989.conf` (4500, 4000,
  3500, 2800; tier `cottage`), which the header lists. The signs rest on the terrain (contact gap under 1 mm). Proof:
  `everon/plot-prices` (pack leg), `look contact place:saint-philippe/plots`.
- 🟡 (#2918) The merged catalogue (core plots plus the pack overlay) is checked as a whole: keys unique and no house id, every
  plot names a core tier whose stages have work and item counts, prices above 0 and under ten times the priciest core plot.
  Plot labels in period words and pack-set build stages are not possible yet: `LF_PropertyPlotDef` has no label field and tiers
  are core-only (a core change, split off). Proof: `everon/plots` (pack leg).

### Plot purchase — `Feature/Property/LF_PlotComponent.c` (2026-11-18, #2649, spec #756)
- 🟡 A for-sale plot sign (`Prefabs/Props/Property/LF_PlotSign.et`, an `LF_PlotComponent` naming a
  `LF_Property.conf` plot key) shows "Buy plot ($price)". The request carries the used sign's plot key: the server sells
  it only when a live sign with that key is within 5 m of the caller's character, so a nearer neighbouring sign
  never sells instead (#2777 review). Rate-limited as a purchase; a dead or downed buyer is refused.
- 🟡 Refused, nothing taken: no sign in reach, a plot with no price, an owned plot, short carried cash, a
  cuffed buyer. Bought: exactly the price leaves the buyer's carried cash (ledger reason `property_buy`),
  the buyer's character key becomes the owner in `LF_Properties` with the purchase time, and a toast says
  so. The row rides the existing `properties` payload. Proof: `property/plot-buy`.
- 🟡 (#2780) The plot sign reads "For sale" (core sign `plot_for_sale` in `LF_Signs.conf`, neutral strings; a pack words it
  by overriding the same keys). A client has no property book, so each sign replicates one bool, `m_bOwned`
  (`RplProp` + `BumpMe` on change; a late joiner gets it with the entity). `LF_Properties.SetOwner` and `ApplyAll` refresh the
  live signs, and a sign that enters the world after a sale starts owned. `LF_PlotBuyAction` shows only when
  `LF_PlotComponent.IsForSale()` (a price and not owned), so an owned plot no longer offers Buy; the server still refuses a
  forged buy. The owner's key is never replicated. Proof: `property/plot-owned-state` (the flag drives the action with the
  book emptied, as on a client); the late-joiner view is a manual step.

### Plots for sale on the map and the town board — `Feature/Property/LF_PlotComponent.c` (2026-11-18, #2984)
- 🟡 A plot sign that is for sale (a price, nobody owns it) registers in `LF_Places` as kind `PlotForSale`, id = its plot
  key, and leaves it once the plot is owned (server: `RefreshOwned`; client: the replicated flag's callback, so a late
  joiner sees only the plots still for sale). The core POI table has a PUBLIC `PlotForSale` row ("Plot for sale", civic icon),
  so every player's map draws a mark per unowned plot and drops it when the plot is bought or comes back when the owner is cleared.
  No coordinate or map name in core: the placed signs are the plots, so a pack's plots show as soon as they are placed.
- 🟡 A town board card whose place id is `*` (`LF_TownBoard.PLACE_NEAREST`) points at the nearest place of its kind, so the
  Everon 1989 board's "Plots for sale" card (it took the rules card's slot until #3077 made room for twelve cards and the rules card came back) names the nearest
  plot still for sale and "Set waypoint" marks it. Reading it records no single plot as heard of. Proof:
  `property/plot-for-sale-marks` (owned vs unowned map marks, the card's nearest plot, a cleared owner).

### Buy a house at its "For sale" sign — `Feature/Property/LF_HouseSaleComponent.c` (2026-11-19, #3207, chapter #3201)
- 🟡 `Prefabs/Props/Property/LF_HouseSaleSign.et` (face: core sign `house_for_sale`) names a house identifier. The house's
  price and tier label are `LF_PropertyHouseDef` rows in the property config's `m_aHouses` (`LF_Property.conf`: `sale-house-a`,
  `sale-house-b` as neutral examples); a pack prices its houses through its overlay, patching price and label by house id
  like plots. A house id equal to a plot key is refused (`RefuseHouseCollisions` works both ways).
- 🟡 "Buy house (tier, $price)" sends the used sign's house id; the server sells only when a sign of that id is within 5 m
  of a free (not cuffed), conscious buyer who carries the price (rate-limited as a purchase). Exactly the price leaves carried
  cash (ledger reason `property_buy_house`), the buyer becomes the owner (`LF_Properties.SetOwner` sets it on the live house,
  saved in `houses`; the `properties` row records the purchase time), a `HouseKeyItem` bound to the identifier goes into their
  inventory and a toast says so. Refused with nothing taken: an owned house, short cash, no sign in reach, a cuffed or downed
  buyer, a house with no price. If the owner cannot be set the cash is refunded.
- 🟡 The sign replicates one bool, `m_bOwned` (the #2780 plot-sign pattern), refreshed from `LF_Properties.SetOwner` / `ApplyAll`
  and from every `LF_HouseManagerComponent.SetOwner`; the action reads only that flag: "Buy house" while for sale, "Sold"
  (not usable) after. The owner key is never sent. Proof: `property/house-sale-buy`; the dedicated-client view of the Sold flip
  is a manual step.

### Build site — `Feature/Property/LF_BuildSiteComponent.c` (2026-11-18, #2650, spec #756)
- 🟡 The plot sign also carries an `LF_BuildSiteComponent` and two actions. "Need N more <item>" deposits
  everything the caller carries of what the stage being built still lacks, up to what it lacks: the items leave
  the inventory and are counted in the property row's `m_sDeposited`. A stranger, someone out of reach (6 m) or
  someone carrying none of it is refused with a toast.
- 🟡 Building is cooperative (#2815): the owner and any **key holder** of the plot may deposit and work. A key
  holder is a character carrying (left hand or anywhere in the inventory) a key item (`LF_KeyComponent`) bound to the
  plot key, the same key system as a house door (`LF_BuildSiteComponent.UserHoldsKey`). The server reads the key again
  on every deposit and every work unit, so a key handed away or rebound stops working at once. A character with
  neither gets "Only the owner and key holders can build here." and nothing leaves their inventory. A plot sold or
  repossessed mid-build passes its stage, work and deposits to the next owner (they live on the property row, not on
  the owner): no refund, the old owner is refused, and the new owner continues from where it stood. Proof:
  `property/build-site-keyholder`.
- 🟡 "Build (x/y)" is a 2 s held action, offered once the items are in: one work unit per hold (a second unit
  from the same character within 1.5 s is refused). Items and work met: the stage advances, the next stage
  starts empty, and the shell swaps to the finished stage's prefab from `LF_Property.conf` (spawned at the sign's
  local `m_vShellOffset`, default 8 m in front).
- 🟡 Progress is the row (`m_iBuildStage` = finished stages, `m_iWorkDone`, `m_sDeposited`), saved by the
  existing `properties` payload; no save version change. The shell is not saved: the site re-reads the row
  every 2 s and spawns the shell after a load. Clients see a replicated mirror for the labels. A shell
  prefab with an RplComponent is spawned by the server and replicates (late joiners too); a plain prop such as
  DirtPile is spawned by every machine from the replicated stage. Shell prefabs are scenery: none carries an
  action (the stage 2 cement pile was a cement gather node and is now a plank pile; #2807 review).
  Proof: `property/build-site`, `property/build-site-persist`, `property/build-site-shell`, `wt e2e`.

### Spawn house — `Feature/Property/LF_SetHomeAction.c` (2026-11-18, #2651, spec #811)
- 🟡 The owner of a house sees "Make this my spawn" at its door (`EL_Door_Base.et`, beside the lock action).
  The server takes the house from the door used; the client sends no id. A key holder or a stranger does not see
  it, and a forged perform or a direct `LF_Properties.SetSpawnHouse` by them is refused with a logged reason.
- 🟡 The choice is `m_bOwnerSpawnsHere` on the house's `properties` row (no new payload, no version change). One
  per owner: setting a second house clears the first. `LF_Properties.SpawnHouseOf(ownerKey)` returns it while the
  owner who set it still owns the house; a sold house is never the buyer's spawn, in session or after a save. Any owner change
  (`LF_HouseManagerComponent.SetOwner`, whatever calls it) clears the flag, so buying the house back does not restore it.
- 🟡 Applies at login only (the spawn flow reads `SpawnHouseOf`); deaths still wake at hospital. Proof: `property/set-home`.

### Spawn order at login — `Feature/Character/Spawning/LF_SpawnPriority.c`, `Feature/Property/LF_HouseSpawnComponent.c` (2026-11-18, #2652, spec #747)
- 🟡 Login order: jail (the body is moved to the cell after it spawns), the hospital wake after a death, a rejoin
  spot (`LF_RejoinSpot`, off by default, #1778), the player's own spawn house, then the role's spawn point.
  `LF_SpawnPriority.Decide` is the pure order. Proof: `spawn/priority`.
- 🟡 A house is used only if it is in the world and its live owner is still the player. A dead or sold house
  logs `LF_Debug.Warn("Spawn", ...)` and the player takes the role spawn; the flag stays on the row.
- 🟡 `LF_HouseSpawnComponent` on a house holds authored points and one outside-the-door point. Points are tried in
  random order and the first with room for a person (a box trace) wins; none clear gives the outside point;
  no outside point gives the role spawn. A house with no component takes the role spawn (an Info line). The core
  authors no points; the pack adds them (#812). Port of `OVT_SpawnPointComponent` (`THIRD_PARTY.md`). Proof: `spawn/house-point`.

### Locked vanilla buildings — `Feature/Houses/LF_BuildingLocks.c` (2026-11-18, #3203, parent #3201)
- 🟡 A map maker drops `Prefabs/Systems/Houses/LF_BuildingLock.et` inside a vanilla house and nobody opens
  that building's doors. The marker (`LF_BuildingLockComponent`) finds its building the way `LF_FittedBuildings`
  does (one bounded sphere query, the building whose bounds hold the marker) and registers the building's
  `EntityID` with the marker's sign key on **every machine**; nothing replicates and nothing is saved (the markers
  load again after a restart and for a late joiner). `LF_DoorUserAction` walks the door's parents to the building:
  the prompt names the reason, and the server refuses a forged open. Closing an open door still works.
- 🟡 Sign keys: `#LF-BuildingLock_Private`, `_StaffOnly`, `_ForSaleSoon`, `_OpeningSoon`, `_Closed` (neutral core text;
  a pack rewords them by overriding the same Ids). An unmarked or unfound building stays open; no building within the
  radius logs `LF_Debug.Error("BuildingLock", ...)`.
- 🟡 Precedence: a building that also holds an open marker (a fitted place's `LF_FittedBuildingComponent`; a house
  marker will call `LF_BuildingLocks.RegisterOpenAt`) ignores the private lock, so later cards never edit the lock
  layer. Proof: `houses/building-lock-private`.
- 🟡 Saint-Philippe lock layer (#3204): `node tools/town/locks.mjs saint-philippe` writes
  `LF_Everon1989/.../MainWorld_Layers/Locks_saint-philippe.layer` from `purpose.json` (never hand-edited): one marker per
  enterable building no fitted slot carries, at the footprint centre, with a pack sign key (`LF-Everon-Lock_*`: Propriété
  privée for homes of residents, barns, lock-ups and outbuildings; Réservé au personnel; À vendre bientôt for the homes
  for sale; Ouverture prochaine for planned shops; Zone militaire; École : vacances d'été; À louer bientôt; Fermé).
  Left out: fitted or in-flight rows, the churches and chapels (D1), `sp:b49` (org registry) and side buildings of a
  fitted business. `tools\cli town doors saint-philippe --planned` shows the carriers; a shop or home card that fits
  its building needs no edit here (the open marker wins). Selftest: `node tools/town/locks.selftest.mjs`.
