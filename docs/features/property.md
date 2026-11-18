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

### Plot purchase — `Feature/Property/LF_PlotComponent.c` (2026-11-18, #2649, spec #756)
- 🟡 A for-sale plot sign (`Prefabs/Props/Property/LF_PlotSign.et`, an `LF_PlotComponent` naming a
  `LF_Property.conf` plot key) shows "Buy plot ($price)". The request carries the used sign's plot key: the server sells
  it only when a live sign with that key is within 5 m of the caller's character, so a nearer neighbouring sign
  never sells instead (#2777 review). Rate-limited as a purchase; a dead or downed buyer is refused.
- 🟡 Refused, nothing taken: no sign in reach, a plot with no price, an owned plot, short carried cash, a
  cuffed buyer. Bought: exactly the price leaves the buyer's carried cash (ledger reason `property_buy`),
  the buyer's character key becomes the owner in `LF_Properties` with the purchase time, and a toast says
  so. The row rides the existing `properties` payload. Proof: `property/plot-buy`.

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
