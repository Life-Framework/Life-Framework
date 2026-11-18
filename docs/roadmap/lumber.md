# Lumber: fell, shoulder, haul, saw, sell

Design note for the lumber chain on the Everon island (maintainer request 2026-11-18). The slice plan
is `docs/roadmap/lumber-slices.md`; the cards are sub-issues of #3169. The numbers are a starting
point. The economy card (LB12) checks them with `tools\cli economy` and tunes them there.

## What already exists (reused, not rebuilt)

| Piece | Where | Use in this chain |
|---|---|---|
| LUMBERJACK job, `gather_logging` XP award (10 XP, 8 s cooldown) | `Configs/Jobs/LF_Jobs.conf:42,93`, pack table `LF_Jobs_Everon1989.conf:16` (levels 100/300/600, licences BASIC/PROCESS/SPEED) | a fell pays this award; a hardwood fell gets a new `fell_hardwood` award |
| Log, plank, kindling items | `Prefabs/Resources/WoodCutting/WoodLog_01_Small.et`, `WoodPlank.et`, `WoodKindling.et` | the softwood log and the plank stay; new goods inherit from these |
| Sawmill machine (log → plank + kindling) | `Prefabs/Processor/Sawmill.et`, `LF_ProcessAction` | the yard machine; each output choice is one more `LF_ProcessAction` on it |
| Placed cuttable trees (melee damage) | `CuttableTree_*.et`, `LF_DestructibleResourceComponent` | DebugWorld and GridWorld test trees only. The island uses real map trees |
| Gather stock, tool gate, `LF_OnGathered` | `Feature/Gathering/` | the event shape; a fell publishes `LF_OnGathered` with family LOGGING |
| Open ground, hard surface | `LF_PlantRules.IsHardSurface` (#2821/#3023) | the "not on a road" refusal |
| Safe zones | `LF_Zones.IsSafe(pos)` | no felling inside a SAFE zone |
| Hand carry (item forced into the hands, kept on sprint) | `Feature/Character/Inventory/HandCarry/` | the base of the shoulder carry |
| Local visuals driven by a replicated number | `LF_FarmPlotComponent` (crop copies from an `RplProp` stage) | the stacked logs on a truck bed |
| Market curve | `LF_PriceCurve` (#2844), pack plank row base 6 | the wholesale buyer |
| Axe at the general store | #3171 (Y2) | the tool. No new card |
| Woodlot slot in the town tools | #3174 (Y5) | site cards fill it. #3175 (placed `CuttableTree` prefabs) is superseded by real map trees |

## Felling a real map tree

Everon's trees are map entities of class `Tree` (`BaseTree : SCR_DestructibleEntity`), prefabs under
`Prefabs/Vegetation/Tree/<species>/<species>_<size><variant>.et` (for example
`t_pinus_sylvestris_2f.et`, `T_Pinus_sylvestris_02_3d.et`). The vanilla path that acts on them is
`SCR_FoliageDestructionComponent` (on the player controller). The client sends the tree's
`EntityID` (map entities have the same id on server and client). The server checks distance (8 m)
and `TreeClass.m_bDestructibleByHand`, then `SCR_CutFoliageAction` calls `Tree.ForceKill_S()`. The
engine syncs a destructible's state to every client and to late joiners (`OnStateChanged(..., JIP)`).
It has no restore call, so a tree killed that way stays down until the restart.

Decision: we do not kill the tree. A felled tree is **hidden** for its regrow time:

- The server keeps a ledger of felled trees, keyed by position (x and z to 0.1 m) plus prefab. That is
  a stable key, the same on every machine and after a restart.
- The server hides the tree (visibility off, collision off) and broadcasts the key. A client resolves
  the tree near that position by prefab and hides it too. A late joiner gets the ledger in `RplLoad`.
- Regrow clears the key and shows the tree again, with the same broadcast.
- A restart reloads the map, so every tree stands again, which is safe. Whether to persist the ledger
  is decided in LB4 (draft, REVIEW-NEEDED). The default is to persist it with the regrow time left,
  so a restart does not regrow a stand that was just cut.

Finding the tree: the axe in the player's hands targets the nearest standing `Tree` within 3 m in the
look direction (`QueryEntitiesBySphere` on the server, the vanilla helper pattern on the client for
the prompt). The server re-checks every rule.

### Species and size

The class comes from the species folder. The size comes from the digit after the species name. The
digit stands at the start of the size group (`1f`, `2d_fallen`), or after `02_` in the v2 trees.

| Class | Species folders | Notes |
|---|---|---|
| SOFTWOOD | `t_pinus_sylvestris`, `t_picea_abies`, `t_larix_decidua` | quick, light logs |
| HARDWOOD | `t_carpinus_betulus`, `t_tilia_cordata`, `t_alnus_glutinosa`, `t_betula_pendula`, `t_populus_nigra` | slow, heavy, pays more |
| none | `t_malus_domestica`, `t_prunus_domestica` (orchards, the gather trees), `t_salix_fragilis`, `t_sorbus_aucuparia`, `Debris`, `Dst`, `Roots` | not felled |

The following are never felled: stumps (`stump`), branches (`branch`), `base` prefabs, fallen trunks
(`fallen`), and size 0 (saplings, which vanilla clears with the e-tool).

| Tree | Chop (hold) | Logs | XP | Stamina |
|---|---|---|---|---|
| softwood size 1 | 15 s | 1 | 10 (`gather_logging`) | 15 % |
| softwood size 2 | 25 s | 2 | 10 | 25 % |
| softwood size 3 | 35 s | 3 | 10 | 35 % |
| hardwood size 1 | 30 s | 1 | 20 (`fell_hardwood`) | 25 % |
| hardwood size 2 | 45 s | 2 | 20 | 40 % |
| hardwood size 3 | 60 s | 3 | 20 | 50 % |

Regrow: softwood 45 min, hardwood 90 min. A fell is refused (toast `#LF-Lumber_*`, log
`[LFDebug:Lumber] fell refused: <reason>`), in this order:

1. NO_AXE: no axe in the hands.
2. NOT_A_TREE: the class is none, or the prefab is a stump, branch, base or fallen trunk.
3. TOO_SMALL: size 0.
4. ALREADY_FELLED: the key is in the ledger.
5. TOO_FAR: more than 3 m from the trunk.
6. SAFE_ZONE: `LF_Zones.IsSafe`.
7. NEAR_ROAD: hard surface within 4 m (`LF_PlantRules.IsHardSurface`), because the tree would fall on a road.
8. NEAR_BUILDING: a building within 12 m. This also keeps town gardens and avenues standing.
9. TIRED: stamina below the tree's cost.

## Carrying (shared with mining)

A log is never an inventory item in the pockets. A fell drops its logs on the ground beside the stump.
**Hoist** puts one on the shoulder:

- The log becomes a hand-carry item (`LF_HandCarryComponent`) with a shoulder pose offset. Hands are
  busy: the weapon is holstered and nothing else can be used. Gadgets in the hands replicate natively,
  late joiners included.
- `LF_ShoulderCarryComponent` holds the carry profile: speed cap (`SetDynamicSpeed`), no sprint (the
  cap sits under jog), and bone and offset. Profiles: softwood log 0.7, hardwood log 0.5. Mining uses
  the same component for an ore sack (0.75) and a crate (0.6).
- Drop puts the item down where the carrier stands. A downed carrier drops it.

## Truck bed (shared with mining)

`LF_CargoBedComponent` on a vehicle prefab holds a per-vehicle capacity in units and a list of item
kinds. Each kind has a unit size: softwood log 1, hardwood log 2, ore sack 1, crate 2.

- **Load** (at the tailgate, a log on the shoulder): the server deletes the carried entity and adds
  one to that kind's count.
- **Unload**: the server spawns one of the top kind and puts it on the shoulder of the player who
  unloads it.
- The counts are a small replicated array (`RplSave`/`RplLoad` plus one RPC per change, never per
  frame). Each machine draws the stack itself as local, physics-free copies on the bed's slots, the
  `LF_FarmPlotComponent` pattern. A late joiner sees the stack from `RplLoad`.
- Capacities: car 0 (no bed), pickup or van 6 units, the 1989 truck 12 units.
- A parked truck keeps its load across a restart: a section payload keyed by the vehicle's persistent
  id (LB8, draft, REVIEW-NEEDED).

## Processing at the yard

The yard machine is the core `Sawmill.et` with one `LF_ProcessAction` per choice. Each choice takes
one log:

| Input | Choice | Output | Time |
|---|---|---|---|
| softwood log | planks | 4 × `WoodPlank` | 10 s |
| softwood log | fence posts | 2 × `FencePost` (new) | 8 s |
| softwood log | firewood | 4 × `FirewoodBundle` (new) | 6 s |
| hardwood log | boards | 2 × `HardwoodBoard` (new) | 15 s |
| hardwood log | firewood | 5 × `FirewoodBundle` | 6 s |

Kindling stays a by-product of the plank choice.

## Buyers: premium with demand, wholesale with a curve

There are two kinds of buyer, both in core (`Feature/Market/`), with the numbers in the pack:

- A **premium buyer** pays a high fixed price, but only up to its **demand**: a stock limit that
  refills over time, like a merchant's purse. A full purse buys nothing until it refills. Rule:
  `LF_BuyerDemand` (cap, refill per hour, current, last update in unix seconds, all in int maths).
  Sell N: the buyer accepts `min(N, remaining)`, and the rest is refused with "has enough, back in
  ~M min, or try the harbour warehouse". A premium row can also name a curve (#2844), so the price
  falls inside the cap too. The default is a flat price.
- A **wholesale buyer** takes any amount at a lower bulk price on the market curve (`LF_PriceCurve`:
  drop per unit, floor, slow recovery). Flooding the town pulls it down for hours.

| Good | Premium buyer (building) | Price | Demand cap | Refill /h | Wholesale (dk:b22) base → floor | Drop /unit | Recover /h |
|---|---|---|---|---|---|---|---|
| WoodPlank | builders' merchant `sp:b59` | $8 | 40 | 20 | $6 → $4 | 1 % | 5 % |
| HardwoodBoard | builders' merchant `sp:b59` | $12 | 24 | 8 | $7 → $4 | 1 % | 5 % |
| FencePost | seed and feed shop (allocation card S-seedfeed) | $11 | 12 | 6 | $5 → $3 | 2 % | 5 % |
| FirewoodBundle | bakery (wood oven) | $4 | 30 | 30 | $2 → $1 | 2 % | 10 % |

Shop buy prices stay above every buy-back (#3121): the general store sells planks at $15 (#3171). The
existing pack trader row (plank 6) moves to the wholesale warehouse.

**Player trade:** the cottage stages need 18 planks (`LF_Property.conf`: walls-roof 12, doors 6),
which costs $270 at the store. A lumberjack who sells to a builder at $10 to $12 a plank beats both
the store and the premium buyer, so a builder and a lumberjack each gain by trading. Hardwood boards
are the planned input of the furniture bench (a follow-up, not in this plan).

## Money ladder (checked with `tools\cli economy`)

Assumptions: walk with a log at about 1.1 m/s, run back at 3.4 m/s, woodlot to yard 150 m. A truck
cycle is load, a 2 km drive, unload and process.

| Loop | Logs /h | Steady $/h (demand used up) | First hour (purse full) |
|---|---|---|---|
| Hand-haul softwood → planks | about 18 | about $410 (20 premium + 52 wholesale planks) | about $540 |
| Hand-haul softwood, mixed planks, posts and firewood | about 18 | about $430 | about $520 |
| Truck hardwood (12-unit truck, 6 logs a cycle, 4 cycles an hour) → boards | about 24 | about $470 (8 premium + 40 wholesale boards, plus firewood) | about $600 |

Courier work today is $400 to $560 an hour (parcel, fuel, contracts; taxi $960 at 15 runs). Duty pay
is police $900 and medic $792 an hour.

- Duty against hand-haul: 900 / 410 = 2.2 and 792 / 430 = 1.8 at the steady rate, inside 1.5 to 2.5.
- Duty against truck hardwood: 900 / 470 = 1.9 at the steady rate. The first-hour peak ($540 to $600,
  792 / 600 = 1.3) lasts one hour per purse refill. The steady rate is the one that counts, and LB12
  keeps it inside 1.5 to 2.5.
- LB12 adds these loops as rows in `tools\cli economy` (today the tool prints courier, taxi and duty
  only) and flags a loop whose steady rate leaves the band.

What makes lumber different from mining and courier work:

- Your route and what you sell decide the pay, not a fixed fare. A purse fills, then the warehouse
  price drops.
- A truck multiplies throughput, but only with hardwood and only with buyers that still have demand.
- Planks are a good players trade with each other.

## Sites (quality bar in every site card)

| Site | Building | Purpose |
|---|---|---|
| Saint-Philippe edge yard | a town-edge industrial building from the allocation (`di:b76` Workshop_01_Garage or `sp:b121` Warehouse_01); the card names the one it takes in `purpose.json` | hand-haul softwood, the newcomer loop |
| Everon Sawmill | the island's own Sawmill site, `Sawmill_E_01` at (3093, 5241), 5.7 km from Saint-Philippe | truck hardwood, the yard with the most machines |
| Wholesale timber warehouse | harbour `dk:b22` (Warehouse_01_V3; fallback the boatyard shed `dk:b24`) | takes any amount on the curve |
| Premium buyer | builders' merchant `sp:b59` (allocation card S2) | demand-capped purchase of planks and boards |

Every site card holds the maintainer's bar:

- A real fitting location, not an empty field.
- Dressed with base-game props: log piles, stacked planks, a saw bench, a truck loading area, fencing,
  work lights and a sign.
- The machine inside or at a real building.
- Every prop passes `look contact` at 1 cm.
- `look` pictures before and after, plus a ground-level walk-up picture.
- The `polish` checklist run on the pictures.
- A map POI and a sign.
