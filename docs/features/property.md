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

### Sale percent (alpha pacing) — `Feature/Houses/LF_HousesConfig.c` (rc 0.2.0)
- 🧪 Owner key `houses.salePercent` (10 to 100, default 100) sells every house and plot at that percent of its listed price: `LF_PropertyConfig.HousePrice` and `PlotPrice` return `LF_HousesConfig.SalePrice` (int math, rounded down). The config stream carries it (`LF_ConfigStream.BuildPayload` houses section), so the client's "Buy house ($price)" and "Buy plot" labels show what the server charges. The flagship alpha sets 50: the cheapest Saint-Philippe cottage is $2000 (listed 4000) and plot p4 $1400 (listed 2800). No sell-back exists, so no loop. LOGIC `property/house-sale-percent`.

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
- 🟡 (sold-sign-board) The board's printed face follows the same flag. It printed "FOR SALE" once and never again, so a bought
  cottage still read for sale to a passer-by while the action said Sold (playtest round 5). `LF_SignComponent.m_sSoldSignId`
  (`house_sold` on the house sign, `plot_sold` on the plot sign) makes `GetSign()` return the sold sign while the sale
  component is owned, so the face and the Read card say "Sold"; the sign's own id (`house_for_sale`) stays the place and quest
  id. The owned flag now has an `onRplName` callback that reprints the face on every client, a listen host reprints from the
  server's `RefreshOwned`, a late joiner prints from the flag it receives, and a restart's `ApplyAll` refreshes every sign.
  Proof: `property/sold-sign-face`; the pictures from a two-client island session are in the PR.

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
- 🟡 A dedicated client sees the offer too (#3375): the property book and `LF_Houses.Find` are server-only, so the client
  compares its key with the house's replicated `GetOwnerKey()` and skips the `SpawnHouseOf` check (`LF_SetHomeAction.OfferSet`,
  shared with the house sign, #3208). The server re-checks in `SetSpawnHouse`. Proof: `property/set-home-client-offer`
  (host test of the client branch; dedicated-client session is a manual step).

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
  enterable building no fitted slot carries, at the footprint centre, with a pack sign key (`LF-Everon-Lock_*`, US English since 2026-11-22: Private
  property for homes of residents, barns, lock-ups and outbuildings; Staff only; For sale soon for the homes
  for sale; Opening soon for planned shops; Military zone; School: closed for summer; For rent soon; Closed).
  Left out: fitted or in-flight rows, the churches and chapels (D1), `sp:b49` (org registry) and side buildings of a
  fitted business. `tools\cli town doors saint-philippe --planned` shows the carriers; a shop or home card that fits
  its building needs no edit here (the open marker wins). Selftest: `node tools/town/locks.selftest.mjs`.

### Homes for sale in the real town — `LF_HouseMarker.et`, `tools/town/homes.mjs` (2026-11-19, #3584, chapter #3201)
- 🟡 `Prefabs/Systems/Houses/LF_HouseMarker.et` is the lock marker plus an `LF_HouseManagerComponent` (and an `RplComponent`): dropped inside a
  vanilla building with a `m_sDebugIdentifier`, it makes that building a house. Its doors refuse a stranger; the owner (their character
  key against the replicated owner key) and anyone holding a key bound to the house id pass, on the client prompt and on the server. An
  unowned house shows the marker's `m_sSignKey`; once it has an owner a stranger sees `m_sOwnedSignKey`. A plain lock marker is unchanged.
- 🟡 `tools\cli town homes saint-philippe` (from `ws/town-draft-saint-philippe-homes`) does it for every purpose row with purpose `a`, a
  `houseId` and a tier: a house marker at the footprint centre, `LF_HouseSaleSign.et` with the same house id on the entrance wall (the
  drop planner's wall seat; a building whose entrance wall takes no sign uses another side), one `m_aHouses` row per home in
  `addons/LF_Everon1989/Configs/Property/LF_Property_Everon1989/LF_Property_Homes_SaintPhilippe.conf`, and `docs/everon/places/<place>/homes.json`,
  which `town locks` reads to drop those homes' ForSaleSoon markers. Layer `Homes_saint-philippe.layer`; a generated file, rerun the script.
- 🟡 A sale sign stands beside the door frame (#3886): `town homes` takes a wall spot only when the sign is out of every door leaf's swing
  (1 m) and ground walk-up (3 m), by 0.2 m, using the door leaves in `tools/town/probes/doors-saint-philippe.json` (the audit's runtime
  dump); the planner slides the sign along the wall and tries the other sides. Selftest cases: a fixture sign beside, in front of and
  in the walk-up of a door, and every committed sign against the probe.
- 🟡 Prices by tier (a courier earns about $600 an hour, `tools\cli economy`): cottage 4000, village house 6000, family house 8000,
  town house 10000, farmhouse 13000, villa 20000. Selftest `node tools/town/homes.selftest.mjs`; in-game `houses/house-marker`.
- 🟡 The cottage's door stage asks for a door kit (`Prefabs/Items/Resources/LF_DoorKit.et`, #3172), not the structure door
  prefab (a building part nobody can pick up, so no house could be finished). The kit is a boxed item ($250 at the Everon
  1989 hardware store, `hardware` stock and `LF_ShopPrices_GeneralStore.conf`). A save that holds a half-deposited door stage
  loses that one count (deposits are counts by prefab). Proof: `property/build-door-kit` (every stage requirement loads and
  fits a character's inventory, the kit counts at a build site) and `everon/door-kit-stock` (pack leg).

- 🔧 (2026-11-27, playtest-1127-r4) **The house purchase toast no longer prints the internal house id.** `LF-Property_HouseBought` read "You bought house saint-philippe.home.sp-b83. The key is in your inventory."; the en_us text is now "You bought the house. The key is in your inventory." (the key is unchanged and the id is still passed, unused). Pictures: `website/public/docs-images/playtest-1127-r4/house-bought-raw-id-before.png`, `house-bought-fixed-after.png`. Merged, in-dev.
