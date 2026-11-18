# Lumber slice plan (parent #3169)

Design and numbers: `docs/roadmap/lumber.md`. Cards follow `docs/process/slice-card.md`. LB1 is built
in `ws/tree-cutting`, and the rest go to the queue. Already-carded pieces this plan uses: #3171 (the
general store sells the axe), #3174 (the woodlot slot). #3175 is rewritten in place as the
Saint-Philippe edge timber yard. It used to place `CuttableTree` prefabs; the island now fells real
map trees, so it places none.

Every site card (#3175, LB14, LB15, LB16) carries the maintainer's site bar:

- A real fitting Everon location.
- Dressed with base-game props: log piles, stacked planks, a saw bench, a truck loading area,
  fencing, work lights and a sign.
- The machine inside or at a real building.
- Every prop passes `look contact` at 1 cm or better.
- `look` before and after, plus a ground-level walk-up picture.
- The `polish` checklist run on the pictures.
- A map POI and a sign.

# Slice LB1: Tree felling rules: species, size, yield, refusals and the regrow ledger
## Card
- type: system
- tier: T2
- model: opus
- machines: test
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Lumber/LF_TreeFelling*.c`, `addons/LifeFramework/Scripts/Game/Feature/Lumber/Feature.md`, `addons/LifeFramework/Configs/Lumber/LF_TreeSpecies.conf`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_TreeFellingRules.c`, `docs/roadmap/lumber.md`, `docs/roadmap/lumber-slices.md`
- depends: none
- acceptance: test LF_Test_TreeFellingRules
- risk: none
- release-blocker: no

## Capability
Pure rules, no world, say which map tree an axe can fell and what it gives. A vanilla tree prefab
path maps to a species class (softwood, hardwood or none) and a size (0 to 3). A class and size map
to chop seconds, logs, XP action and stamina cost. The ordered refusals (no axe, not a tree, too
small, already felled, too far, safe zone, near road, near building, tired) give one reason and a
string key. A position key (x and z to 0.1 m plus the prefab) and a ledger record felled keys with a
regrow time on a caller clock.

## Acceptance
LF_Test_TreeFellingRules (LOGIC) covers:
- Classify: real Everon prefab paths (`t_pinus_sylvestris_2f`, `T_Pinus_sylvestris_02_3d`,
  `t_carpinus_betulus_3s_aut`, a stump, a fallen trunk, an apple tree, a size 0 sapling).
- The yield table.
- Each refusal in order.
- Ledger: fell, refuse a second fell, regrow after the time, time left.
- The config loads from `LF_TreeSpecies.conf`.

Red with the hook on.

## Notes
Built by the planner in `ws/tree-cutting`. Numbers are in `docs/roadmap/lumber.md` "Species and size".

# Slice LB2: An axe fells a real map tree: hold, stump, logs on the ground, LUMBERJACK XP
## Card
- type: capability
- tier: T2
- model: sonnet
- machines: test
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Lumber/LF_FellAction.c`, `addons/LifeFramework/Scripts/Game/Feature/Lumber/LF_FelledTrees.c`, `addons/LifeFramework/Scripts/Game/Feature/Lumber/LF_LumberEvents.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_TreeFellWorld.c`
- depends: LB1
- acceptance: test LF_Test_TreeFellWorld
- risk: none
- release-blocker: no

## Capability
With the axe in the hands, the player gets a *Fell tree* hold action on the nearest standing map tree
within 3 m. The prompt shows the species class and how long the hold takes. When the hold completes,
the server re-checks every LB1 rule. It then:
- hides the tree (visibility and collision off);
- records the key in the server ledger (`LF_FelledTrees`);
- drops the tree's logs on the ground beside the trunk (one `WoodLog_01_Small` per log; hardwood logs
  come with LB6);
- takes the stamina cost and awards `gather_logging` (softwood) or `fell_hardwood` (hardwood);
- publishes `LF_OnTreeFelled` and `LF_OnGathered` (family LOGGING).

A refusal toasts `#LF-Lumber_<Reason>` and logs `[LFDebug:Lumber] fell refused: <reason>`.

## Acceptance
LF_Test_TreeFellWorld (WORLD) spawns a vanilla `t_pinus_sylvestris_2f.et` and an axe-holding fixture
character in the test world, then fells the tree:
- the tree is hidden and in the ledger;
- 2 logs lie within 3 m;
- XP rose;
- a second fell is refused ALREADY_FELLED;
- a fell without the axe is refused NO_AXE.

The test deletes what it spawns.

## Notes
- The vanilla pattern is `SCR_FoliageDestructionComponent`/`SCR_CutFoliageAction` (client sends the
  map tree `EntityID`, server checks the distance). We hide instead of `ForceKill_S` so the tree can
  regrow. Research the hide call (`ClearFlags(EntityFlags.VISIBLE)`, the physics call on a static
  `Tree`) with `tools\cli api class` and never guess.
- Hard surface: `LF_PlantRules.IsHardSurface`. Safe: `LF_Zones.IsSafe`.
- Add the `fell_hardwood` XP award row (20 XP) to `Configs/Jobs/LF_Jobs.conf`, a shared row
  append.
- Replication is LB3; regrowth and restart are LB4. Here the server ledger only grows.

# Slice LB3: Felled trees disappear on every client and for late joiners
## Card
- type: system
- tier: T2
- model: opus
- machines: session-2p
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Lumber/LF_FelledTreesReplication.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_FelledTreesSync.c`, `agent-box/proofs/fell-sync.mjs`
- depends: LB2
- acceptance: test LF_Test_FelledTreesSync
- risk: replication
- release-blocker: no

## Capability
A game-mode component broadcasts each felled or regrown key. Each client resolves the map tree near
the key's position by prefab and hides or shows it. A late joiner gets the whole ledger in `RplLoad`
and hides those trees on arrival.

## Acceptance
LF_Test_FelledTreesSync (LOGIC) checks:
- the ledger snapshot round-trips through the `RplSave`/`RplLoad` writer;
- client-side resolve picks the right tree among neighbours 0.5 m apart.

The session proof `fell-sync` (2 clients, one joins late) fells a tree on client A, then shows it gone
on B and on a late-joining C, with pictures.

## Notes
- Opens as a draft PR with REVIEW-NEEDED. Ship after the interrogate review.
- Send keys (positions), never an `EntityID`. Bump only on a change, never per frame.

# Slice LB4: Felled trees regrow on time and survive a restart
## Card
- type: system
- tier: T2
- model: opus
- machines: test
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Lumber/LF_TreeRegrow.c`, `addons/LifeFramework/Scripts/Game/Feature/Lumber/LF_LumberSavePayload.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_TreeRegrowPersist.c`
- depends: LB2
- acceptance: test LF_Test_TreeRegrowPersist
- risk: persistence
- release-blocker: no

## Capability
A 10 s server tick regrows the trees whose time has passed: they are shown again and the key is
cleared through LB2's ledger API. The ledger is saved as its own section (`LF_LumberSavePayload :
LF_SavePayload`), each key with its regrow time left. After a restart, the server hides those trees
again, so a restart does not regrow a stand that was just cut. `OnAbsent` covers an older save. A
world reset clears the ledger (an `LF_StaticResetHook` in the file).

## Acceptance
LF_Test_TreeRegrowPersist (PERSISTENCE) covers:
- write, read and re-apply idempotently;
- an expired entry is dropped on load;
- the regrow tick shows the tree;
- `validate-record-shapes --update` locks the shape.

`tools\cli wt e2e` proves the real restart.

## Notes
Opens as a draft PR with REVIEW-NEEDED, with `interrogate` findings in `tmp/review-<slug>.md`. If
reviewers rule that "restart regrows all" is enough, ship only the tick and say so.

# Slice LB5: Shoulder carry: hoist a log, walk slowly with busy hands, drop it
## Card
- type: system
- tier: T2
- model: opus
- machines: session-2p
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Carry/**`, `addons/LifeFramework/Prefabs/Items/Core/ShoulderCarryItem_Base.et`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_ShoulderCarry.c`, `agent-box/proofs/shoulder-carry.mjs`
- depends: none
- acceptance: test LF_Test_ShoulderCarry
- risk: replication
- release-blocker: no

## Capability
A generic carried object, the base for a log now and an ore sack or crate later:
- *Hoist* on a ground item with `LF_ShoulderCarryComponent` puts it on the shoulder through the hand
  carry slot (`LF_HandCarryComponent`). The weapon is holstered, hands are busy, and no other item
  can be used.
- The speed is capped by profile (`SetDynamicSpeed`; softwood log 0.7, hardwood 0.5, ore sack 0.75,
  crate 0.6), with no sprint.
- *Drop* sets it down at the feet. A downed carrier drops it.
- Others and late joiners see it on the shoulder.

## Acceptance
LF_Test_ShoulderCarry (WORLD) covers:
- a fixture hoists `ShoulderCarryItem_Base`;
- the item is in the hand slot and the speed cap applies;
- a second hoist is refused;
- drop puts it on the ground and lifts the cap.

The session proof `shoulder-carry` shows player A carrying, seen from B and from a late-joining C,
with pictures (full profile).

## Notes
- No vanilla carry animation exists (`Feature/Medic/LF_Carry.c` header). The pose is a hand-slot
  offset onto the shoulder, the `LF_CarriedPropComponent` slot-transform pattern.
- No sprint-block API turned up. Research `CharacterControllerComponent` before relying on the speed
  cap alone.
- Opens as a draft PR with REVIEW-NEEDED.

# Slice LB6: Lumber goods and the yard's processing choices
## Card
- type: data
- tier: T1
- model: sonnet
- machines: test
- budget: 40 min / 50 calls
- owns: `addons/LifeFramework/Prefabs/Resources/WoodCutting/WoodLog_Hardwood.et`, `addons/LifeFramework/Prefabs/Resources/WoodCutting/HardwoodBoard.et`, `addons/LifeFramework/Prefabs/Resources/WoodCutting/FencePost.et`, `addons/LifeFramework/Prefabs/Resources/WoodCutting/FirewoodBundle.et`, `addons/LifeFramework/Prefabs/Processor/Sawmill.et`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_LumberProcessing.c`
- depends: none
- acceptance: test LF_Test_LumberProcessing
- risk: none
- release-blocker: no

## Capability
New goods: a hardwood log, a hardwood board, a fence post and a firewood bundle. Each is built from
base-game meshes (vanilla first: the firewood and log piles, beams, posts), with an inventory preview
and `.meta` files. The sawmill offers one action per choice: softwood log to 4 planks (and kindling),
to 2 posts or to 4 firewood; hardwood log to 2 boards or to 5 firewood. The log prefabs are not
inventory items: they carry no pocket storage.

## Acceptance
LF_Test_LumberProcessing (WORLD) checks:
- every prefab loads;
- each sawmill action turns its input into the right outputs and counts;
- a wrong input (a softwood log in the boards action) is refused.

`tools\cli look item <prefab>` for each new good goes in the PR.

## Notes
Adding `LF_ShoulderCarryComponent` to the logs belongs to LB7 (after LB5). The XP for processing
uses the existing `LUMBERJACK_PROCESS` licence row.

# Slice LB7: Load logs into a truck bed, see them stack, unload them at the yard
## Card
- type: system
- tier: T2
- model: opus
- machines: session-2p
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Carry/Cargo/**`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_CargoBed.c`, `agent-box/proofs/cargo-bed.mjs`, `addons/LifeFramework/Prefabs/Resources/WoodCutting/WoodLog_01_Small.et`, `addons/LifeFramework/Prefabs/Resources/WoodCutting/WoodLog_Hardwood.et`
- depends: LB5, LB6
- acceptance: test LF_Test_CargoBed
- risk: replication
- release-blocker: no

## Capability
`LF_CargoBedComponent` on a vehicle gives it a capacity in units and accepted kinds, each with a unit
size (softwood log 1, hardwood log 2, ore sack 1, crate 2):
- *Load* at the tailgate with a carried item: the server deletes the item and counts it in.
- *Unload*: the server spawns the top item onto the unloader's shoulder.
- A full bed refuses with a toast.
- The counts replicate as one small array, with an RPC per change and `RplSave`/`RplLoad`. Every
  machine draws the stack as local, physics-free copies on bed slots (the `LF_FarmPlotComponent`
  visual pattern).

Capacities: car 0, pickup or van 6, the 1989 truck 12. Both log prefabs gain
`LF_ShoulderCarryComponent` here.

## Acceptance
LF_Test_CargoBed (WORLD) covers:
- load 3 softwood and 2 hardwood logs (7 units) into a 12-unit bed;
- a 13th unit is refused;
- unloading returns the last kind first;
- the counts round-trip through the save writer.

The session proof `cargo-bed` loads a truck, drives 100 m, and shows the stack from a second client
and from a late joiner, with pictures.

## Notes
- Vehicle prefabs: put the component on the pack's 1989 vehicle prefabs through their own `.et`
  (core base where they inherit one). Report the owns you add.
- Opens as a draft PR with REVIEW-NEEDED.

# Slice LB8: A parked truck keeps its logs across a restart
## Card
- type: system
- tier: T2
- model: opus
- machines: test
- budget: 40 min / 55 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Carry/Cargo/LF_CargoBedSavePayload.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_CargoBedPersist.c`
- depends: LB7
- acceptance: test LF_Test_CargoBedPersist
- risk: persistence
- release-blocker: no

## Capability
The bed counts are saved as their own section, keyed by the vehicle's persistent id (the garage key).
They are re-applied on load, idempotently. A missing vehicle is tolerated with an Info log.

## Acceptance
LF_Test_CargoBedPersist (PERSISTENCE) covers write, read, re-apply twice with the same counts, an
unknown vehicle key, and a shape locked by `validate-record-shapes --update`. `tools\cli wt e2e`
proves the restart.

## Notes
Opens as a draft PR with REVIEW-NEEDED, with interrogate findings in `tmp/review-<slug>.md`.

# Slice LB9: Buyer with demand: a premium buyer's purse that refills
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 55 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Market/LF_BuyerDemand*.c`, `addons/LifeFramework/Configs/Market/LF_BuyerDemand.conf`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_BuyerDemand.c`
- depends: none
- acceptance: test LF_Test_BuyerDemand
- risk: none
- release-blocker: no

## Capability
Pure core rules for a buyer with demand:
- A row per buyer id and good holds a cap, a refill per hour and a price. It can also name a curve
  good (#2844).
- Sell N: accept `min(N, remaining)` and report the rest.
- Refill by elapsed unix seconds: integer maths through `LF_Utils.MinInt/MaxInt/ClampInt`, never past
  the cap, and clamped after a long gap.
- Seconds until the next unit.

The config (`LF_BuyerDemandConfig`, a list section) takes pack overlays the way `LF_ZoneConfig`
does. The core default ships no rows.

## Acceptance
LF_Test_BuyerDemand (LOGIC) covers:
- a full purse takes 40 of 50 and refuses 10;
- one hour refills 20;
- a refill never passes the cap;
- a 1.8e9 unix time stays exact (#2787);
- a premium row with a curve prices each unit marginally;
- a bad row (cap 0, negative refill) logs `LF_Debug.Error("Market", ...)` naming it.

## Notes
The wholesale side needs nothing new: it is `LF_PriceCurve`. Design: `docs/roadmap/lumber.md`
"Buyers".

# Slice LB10: Selling to a premium buyer spends its demand, and the purse is saved
## Card
- type: system
- tier: T2
- model: opus
- machines: test
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Market/LF_BuyerDemandBook.c`, `addons/LifeFramework/Scripts/Game/Feature/Market/LF_BuyerDemandSavePayload.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_BuyerDemandSell.c`
- depends: LB9
- acceptance: test LF_Test_BuyerDemandSell
- risk: money
- release-blocker: no

## Capability
The trader sell path asks the demand book before paying. A buyer with a demand row pays only for the
accepted units and hands back the rest with the toast `#LF-Market_BuyerFull` ("has enough <good>;
back in ~N min, or try the warehouse"). It logs `[LFDebug:Market] demand <buyer>/<good> N/cap`. The
purses are saved as their own section (`LF_SavePayload`) with the unix stamp, so a restart neither
refills nor empties them.

## Acceptance
LF_Test_BuyerDemandSell (WORLD) covers:
- a fixture sells 50 planks to a 40-cap buyer, is paid for 40 and keeps 10;
- a second sale is refused;
- after a simulated hour it sells 20;
- the save round-trips.

The test is red before the wiring.

## Notes
Find the trader sell path from `Feature/Trader/` and `LF_MarketBook`. Never trust a client count.
Opens as a draft PR with REVIEW-NEEDED.

# Slice LB11: The pack's lumber buyers: premium rows and wholesale curves
## Card
- type: data
- tier: T1
- model: sonnet
- machines: test
- budget: 35 min / 45 calls
- owns: `addons/LF_Everon1989/Configs/Market/LF_BuyerDemand_Everon1989.conf`, `addons/LF_Everon1989/Configs/Market/LF_Market_Everon1989.conf`, `addons/LF_Everon1989/Scripts/Game/Tests/LF_Test_EveronLumberBuyers.c`
- depends: LB6, LB10
- acceptance: test LF_Test_EveronLumberBuyers
- risk: none
- release-blocker: no

## Capability
The design table becomes pack data:
- Premium rows: planks and boards at the builders' merchant, posts at seed and feed, firewood at the
  bakery.
- Wholesale curve rows for the timber warehouse (plank $6 → $4, board $7 → $4, post $5 → $3,
  firewood $2 → $1, 1 to 2 % drop, 5 to 10 % recovery).
- The old trader plank row (6) moves to the warehouse buyer.

## Acceptance
LF_Test_EveronLumberBuyers (pack leg, WORLD) checks:
- every lumber good has a premium row and a wholesale row;
- no buy-back is above a shop's sell price (#3121);
- the premium price exceeds wholesale for every good;
- the rows load through the overlay.

## Notes
Numbers: `docs/roadmap/lumber.md` "Buyers". Buyer ids match the site cards: `builders-merchant`,
`seed-feed`, `bakery`, `timber-warehouse`.

# Slice LB12: tools\cli economy prints the lumber loops and flags the duty band
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/commands/economy.mjs`, `tools/lib/economy-*.mjs`, `tools/validation/economy-*selftest.mjs`
- depends: LB11
- acceptance: fixture tools/validation/economy-lumber-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli economy` gains gather-chain loops read from the merged configs (yield table, processing
outputs, demand rows, curves) plus stated route assumptions. The loops are hand-haul softwood
(planks only, and mixed) and truck hardwood. For each it prints the steady and first-hour $/h. It
flags a loop whose steady rate puts duty pay (police and medic rank 0) outside 1.5 to 2.5 times
civilian.

## Acceptance
A selftest fixture covers:
- a loop at $300/h with duty $900 is flagged (3.0);
- $450 is not flagged;
- the demand cap limits the premium units in the steady hour.

Run on main, the tool prints the three lumber loops inside the band. Paste the output in the PR. If a
loop is out of band, tune the pack rows in a follow-up card, not here.

## Notes
Courier, taxi and duty rows stay as they are. Assumptions: `docs/roadmap/lumber.md` "Money ladder".

# Slice LB14: Everon's Sawmill: the hardwood yard at the island's real sawmill
## Card
- type: town
- tier: T1
- model: sonnet
- machines: full-profile
- budget: 45 min / 60 calls
- owns: `addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_everon-sawmill_yard.layer`, `docs/everon/places/everon-sawmill/**`
- depends: LB6
- acceptance: proof look place:everon-sawmill/yard before and after, a walk-up picture, look contact clean at 1 cm; town check everon-sawmill --complete clean
- risk: none
- release-blocker: no

## Capability
The island's own Sawmill (`Sawmill_E_01` at 3093, 5241, 5.7 km from Saint-Philippe) works again. Its
dressing:
- two sawmill machines inside the mill hall, hardwood boards and firewood choices;
- log decks and a truck turning and loading bay;
- stacked boards, work lights, the fence line;
- a sign and a map POI.

It is the truck destination for hardwood.

## Acceptance
- `town survey` gives a dossier for the site.
- `town fit` places the kit.
- The PR carries `look` before and after, a walk-up picture, `look contact` clean at 1 cm, the polish
  checklist, and `audit placement --strict` clean.

## Notes
- Site bar: see the plan header.
- A new place: survey it first (`docs/everon/places/<place>/dossier.json`, the #3058 pattern). No
  buyer here: boards sell in Saint-Philippe.
- Worktree `town-draft-everon-sawmill-yard`.

# Slice LB15: The harbour timber warehouse buys any amount at the wholesale price
## Card
- type: town
- tier: T1
- model: sonnet
- machines: full-profile
- budget: 45 min / 60 calls
- owns: `addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_timber-warehouse.layer`, `tools/town/kits/timber-warehouse.json`
- depends: LB11, LB17
- acceptance: proof look place:saint-philippe/timber-warehouse before and after, a walk-up picture, look contact clean at 1 cm; town check saint-philippe --complete clean for timber-warehouse
- risk: none
- release-blocker: no

## Capability
The harbour warehouse `dk:b22` (Warehouse_01_V3, 4373, 10989; fallback the boatyard shed `dk:b24`)
becomes the wholesale timber warehouse:
- a dressed counter with a clerk NPC as buyer `timber-warehouse`;
- the price on a chalk board;
- timber stacks and pallets inside;
- a truck bay at the door, work lights;
- a sign and a map POI.

## Acceptance
- `town fit` places the kit.
- The PR carries `look` before and after, a walk-up picture, `look contact` clean at 1 cm, the polish
  checklist, and `audit placement --strict` and `town check --complete` clean.
- A session sale of 10 planks lowers the plank price. Paste the log lines.

## Notes
- Site bar: see the plan header.
- `dk:b22` is a staff-only lock in the PC1 lock layer. A fitted place marker beats the lock (the
  #3201 precedence rule). Set its `purpose.json` row (`use: timber warehouse`, `purpose: b`).
- Worktree `town-draft-saint-philippe-timber-warehouse`.

# Slice LB16: The builders' merchant buys planks and boards to its demand
## Card
- type: town
- tier: T1
- model: sonnet
- machines: full-profile
- budget: 35 min / 45 calls
- owns: `addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_merchant-timber.layer`
- depends: LB11, LB17
- acceptance: proof look place:saint-philippe/merchant-timber before and after, a walk-up picture, look contact clean at 1 cm
- risk: none
- release-blocker: no

## Capability
The builders' merchant `sp:b59` (Workshop_02, allocation card S2) gets a timber counter: a buyer
`builders-merchant` that takes planks and boards up to its demand, a "we buy timber" board with the
prices, and stacked timber behind the counter.

## Acceptance
- The PR carries `look` before and after, a walk-up picture, `look contact` clean at 1 cm, and the
  polish checklist.
- A session sale past the purse is refused with the "try the warehouse" toast. Paste the log lines.

## Notes
- Site bar: see the plan header.
- If the merchant card S2 (#3201) has not landed, fit only the counter and say so. Never edit S2's
  layer.
- Worktree `town-draft-saint-philippe-merchant-timber`.

# Slice LB17: Saint-Philippe mapping gets the timber-warehouse and merchant-timber slots
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/town/templates/town.json`, `tools/validation/town-check-selftest.mjs`, `docs/everon/places/saint-philippe/mapping.json`, `docs/everon/places/saint-philippe/state0-readiness.md`
- depends: #3174, #3190
- acceptance: fixture tools/validation/town-check-selftest.mjs
- risk: none
- release-blocker: no

## Capability
After #3174's woodlot slot, the mapping names two more slots: `timber-warehouse` (building
`dk/b22`) and `merchant-timber` (building `sp/b59`). It also points the woodlot slot at the yard
building #3175 uses.

## Acceptance
The town-check selftest gains a fixture where an unfitted timber-warehouse slot is reported
unfitted, not unknown. `tools\cli town check saint-philippe` names both slots. Red first.

## Notes
The pattern is #3078 and #3174. No layers.

# Slice LB18: One lumber shift on the island, end to end, two players
## Card
- type: capability
- tier: T2
- model: sonnet
- machines: session-2p
- budget: 45 min / 60 calls
- owns: `agent-box/proofs/lumber-shift.mjs`, `docs/playtest/lumber-shift.md`
- depends: LB3, LB4, LB7, LB8, LB12, #3175, LB15, LB16
- acceptance: proof lumber-shift
- risk: none
- release-blocker: no

## Capability
The beat sheet in play:
1. Buy an axe.
2. Fell a pine and an oak or hornbeam near the yard.
3. Shoulder a log to the yard and saw planks.
4. Load hardwood into a truck, drive and unload.
5. Sell planks to the merchant until its purse is full, then the rest at the harbour warehouse.

The second player sees the tree fall, the log on the shoulder and the stack on the bed. A late joiner
sees the stump area and the loaded bed.

## Acceptance
The `lumber-shift` proof passes and writes pictures for each beat (full profile). The playtest page
lists the manual steps. Every failed beat becomes a new card under #3169, not a fix here.

## Notes
This is the integration run. It fixes nothing itself.
