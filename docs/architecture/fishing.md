# Fishing

Design, 2026-11-17, for the maintainer to review with the [M6 outline](../roadmap/m6-outline.md)
(chapter "Gone fishing"). Nothing is built yet. Decided: fishing is a chapter in M6 (world plan D7).
The bible places it on the harbours and the coves ([`world-bible.md`](../everon/world-bible.md),
"The economy" and "Resource and job geography").

## 0. What the base game gives us (build 26266485)

Read from the installed paks (`tools\cli api grep`, `call asset_search`), not from memory.

| Need | In the base game | So |
|---|---|---|
| A fishing rod | None | A new item through the 3D pipeline (#339), with a ledger row |
| Fish | None (no fish item or prop) | New items through the 3D pipeline: one body mesh, a material per species |
| Boats you can use | None. `PedalBoat_01` and `BoatWreck_01` are static props | Shore and pier fishing only. Boats need the vehicle pipeline (#342, in Development) and are out of M6 |
| Harbour dressing | `FishingGear_01` (nets, buoys, rope), `BoatShed_01`, `BoatTrailer_01`, the quay pieces and `HarborCrane_01` on Everon | Kits use these as dressing |
| Where the water is | `ChimeraWorldUtils.TryGetWaterSurface(world, pos, out surfacePos, out EWaterSurfaceType, ...)` with `WST_NONE`, `WST_OCEAN`, `WST_POND`; `BaseWorld.GetOceanHeight(x, z)` | The server checks a cast lands on water, and which kind |
| An animation | No fishing animation | v1 holds the rod as a hand tool and plays a short cast gesture; a real cast animation waits for the animation pipeline (#340) |

## 1. What a player does

1. Buys a rod and a tin of bait at the chandler on the quay, and a fishing licence at the licence desk.
2. Stands on the quay or a shore, holds the rod and casts (the F key, or the fire input while holding
   it). A float lands on the water.
3. Waits. A bite comes after 15 to 60 seconds: the float dips, a sound plays, the prompt reads
   "Strike!".
4. Strikes inside the window (about 2 seconds): a fish goes into the inventory. Too late: the bait is
   gone.
5. Learns better places from people (Gaspard, the harbour master) and from the map. A cove at dusk
   pays better than the quay at noon.
6. Sells the catch at the fish counter on the quay, or smokes it first, or takes the harbour master's
   contract to drive the day's catch inland.

No minigame beyond the strike timing. No visible skill bar. The FISHER job level grows in the
background and shows only as what opens: a better rod at the chandler, a bite that comes sooner.

## 2. Data model (core)

New feature folder `Feature/Fishing/`.

**Config** `Configs/Fishing/LF_Fishing.conf`, class `LF_FishingConfig`, owner key `fishing`:

| Row | Fields |
|---|---|
| `LF_FishingRodDef` | `m_Prefab`, `m_fCastRange` (m, 20), `m_fBiteSpeed` (multiplier), `m_iMinFisherLevel` |
| `LF_FishingBaitDef` | `m_Prefab`, `m_fBiteSpeed`, `m_aBoostSpecies` |
| `LF_FishSpeciesDef` | `m_sId`, `m_aSizePrefabs` (small, medium, large: one prefab each), `m_aWater` (OCEAN, POND), `m_aHours` (when it bites; empty means any), `m_iMinFisherLevel` |
| `LF_FishTableDef` | `m_sId`, weighted `m_aSpecies` (id, weight), `m_aSizeWeights` |
| Globals | `m_fBiteMinSeconds` 15, `m_fBiteMaxSeconds` 60, `m_fStrikeWindowSeconds` 2.0, `m_fStrikeSlackSeconds` 0.5 (latency), `m_sOpenWaterTable` (the table for water with no spot; small fish only), `m_iCastsPerMinute` 6 |

The core ships one neutral species set with placeholder labels. The pack overlays the 1989 Everon
species (`#` keys in the pack table), the tables per spot and the prices.

**Size by prefab, not by a weight float.** A small, medium and large fish of one species are three
prefabs that inherit one base. They stack, they save as carried items with no new state, and the
price list prices each directly.

**Components.**

- `LF_FishingRodComponent` on the rod prefab, beside `LF_HandToolComponent` (tool id `rod`, as the
  parcel and the drill are hand tools).
- `LF_FishingSpotComponent` on a place entity in a pack layer: `m_sTable`, `m_fRadius` (the water area
  it covers, from the spot), `m_iStock` (fish before it thins out), `m_fRegrowSeconds`. It carries an
  `LF_PlaceComponent` of kind `FishingSpot`, so it registers in `LF_Places` on every machine and the
  core finds it by kind, never by name or coordinate.
- The fish counter is a catalogue outlet, `fish_counter` (#988), on an `LF_ShopComponent`: it buys the
  catch through the shop's existing "Sell these", and it sells bait and ice. The chandler is the outlet
  `chandler` (rods, bait, the tin).

## 3. Server authority

Every step that pays is decided on the server. The client shows the float and the prompt.

```
client (owner)                         server
-------------                          ------
Cast(target)  ── RpcAsk_Cast ───────>  rod held? not in a vehicle? within cast range? rate limit?
                                       TryGetWaterSurface(target): OCEAN or POND, else refuse
                                       spot = nearest FishingSpot whose radius covers target,
                                              else the open-water table
                                       biteAt = now + random(min, max) / rod speed / bait speed
              <── RpcDo_CastOk(castId) schedule one CallLater(biteAt)
float lands, waits
              <── RpcDo_Bite(castId)   at biteAt
"Strike!"
Strike()      ── RpcAsk_Strike(castId) ─> accept if now is within [biteAt, biteAt + window + slack]
                                       roll species (table, hour, water, FISHER level), roll size
                                       spot stock - 1; give the fish prefab (or drop beside, logged)
                                       bait - 1; LF_JobLevels.Award(fish_catch); publish LF_OnFishCaught
              <── RpcDo_Caught(castId, prefab) toast, sound
```

- **The server owns the bite time.** A macro cannot strike early: a strike before `biteAt` fails and
  costs the bait. A strike after the window fails the same way.
- **One cast per player.** A new cast, moving more than 5 m, putting the rod away, entering a vehicle,
  being downed or cuffed ends the open cast. The `CallLater` checks the cast is still current before it
  sends the bite.
- **Spot stock.** Each spot has a stock that drops by one per catch and regrows on a timer (the ore
  node pattern). An empty spot still bites, slower and only small fish. This spreads 128 players over
  the coast instead of one pier.
- **Refusals log.** A cast on land, out of range, without a rod or bait, or over the rate limit logs
  `[LFDebug:Fishing]` with the reason and toasts it. A missing species prefab logs `LF_Debug.Error` and
  gives nothing; fishing switches off with one line if its config does not load (`LF_Systems`).
- **The licence.** Fishing needs no licence. Selling to the fish counter does: the outlet's licence gate
  (`FISHING`, a new `LF_ELicenseType` value appended at the end) bought at the licence desk (#1766) for
  a small fee, with no clean-record rule. Without a licence, the fence buys fish at a lower price and
  the underworld notices ([reputation.md](reputation.md)). Poaching is not a crime in M6.

**Other players** see the rod held and the cast gesture. The float and the line are drawn only on the
owner's machine in v1 (no `RplComponent` on the float), so nothing per cast is broadcast. A shared
float is a later polish item.

## 4. Persistence

Fishing adds **no new payload**:

| State | Where it already persists |
|---|---|
| Fish, rods, bait in pockets and storage | The `carried` and storage payloads, as items |
| FISHER job XP | The `joblevels` payload: a new `FISHER` track row in `LF_Jobs.conf` and a new `LF_EJobType` value appended (never renumbered) |
| The fishing licence | The `progression` payload, as other licences |
| Spot stock | Not saved, like ore nodes; a restart refills every spot |
| An open cast | Session state; a restart ends it |

An optional "biggest catch" board (as race records) would be a payload `fishrecords` later; it is not in
the chapter.

## 5. Map and places

| POI kind | Visibility (everon-map.md) | Category, icon | Where from |
|---|---|---|---|
| `FishCounter` | `PUBLIC` | shop, a fish in black ink | The harbour template's `fish-counter` slot |
| `Chandler` | `DISCOVER` | shop, a hook | The `chandler` slot |
| `Harbour` | `PUBLIC` | travel, an anchor in blue | The harbour template's `dock` slot |
| `FishingSpot` | `DISCOVER` | work, a float in blue-green | A `fishing-spot` slot in the `site` template (coves, lakes) and the `dock` slot (the quay) |

- **Town pipeline.** The `harbour` template gains the slots `fish-counter`, `chandler`, `harbour-master`
  and `fishing-spot`. A kit (`tools/town/kits/fish-counter.json` and the rest) places the counter, the
  base-game fishing gear and nets as dressing, and the spot marker. Each slot names its POI kind, so
  `town check` keeps the map in step (M17).
- **Discovery.** The quay spot is "heard of" when the clerk marks the square (D-M3). Coves become "heard
  of" through Gaspard's lines, the harbour master's contract and the "Tourist map of Everon"; a visit
  makes them "visited" (everon-map.md section 4). A spot is never a public icon before a player has
  heard of it.
- **State 2 places** (from the bible and the world plan): the Saint-Philippe quay (the dock opens in
  M6), Black Lake (a pond table, state 1 to 2), Fisherman's Bay and Goat Bay (coves, state 2). Morton
  and Saint-Pierre harbours follow with their towns. The scout dossier of each place supplies the spot
  positions; no coordinate is typed anywhere, and the core holds none.

## 6. Economy

The target is a calm job, in the band of the parcel run, below the taxi at the top, and well below
police duty. Numbers are pack config and tunable by the owner.

| Item | Price (pack) |
|---|---|
| Rod | 60 |
| Better rod (FISHER level 3, faster bites) | 250 |
| Bait, tin of 10 | 15 |
| Fishing licence (permanent) | 50 |
| Small fish, sold | 6 |
| Medium fish, sold | 12 |
| Large fish, sold | 25 |
| A rare species (cove, dusk), sold | 40 |
| Smoked fish (smokehouse: `LF_ProcessAction`, one fish and one log) | the raw price + 50 %, and it is food |
| The fence, unlicensed | 60 % of the counter price |

Rough hourly pay, counting a cast cycle (bite wait, strike, re-cast) of about 60 to 75 seconds and a
miss rate of a fifth:

| Fisher | Where | Per hour |
|---|---|---|
| New, basic rod | The quay at noon | about 450 to 500 |
| FISHER level 3, better rod and bait | A cove at dusk | about 750 to 850 |
| The same, smoked and sold, or with the inland delivery contract | | about 900, the taxi's band |

Against the existing sources (parcel about 550 an hour, taxi about 900, police duty 1,800) this sits
where a calm outdoor job should. Sinks it adds: rods, bait (about 15 every ten casts), the licence, fuel
to the coves. The market curve (The market moves, #748) lowers the counter price when the catch floods
it and recovers over a day; until that chapter ships, the spot stock and the bait cost hold supply down.
The ledger records `fish-sale` and `fish-bait`, so the first week can be measured.

## 7. Performance at 128 players

| Cost | Assumption | Why it is bounded |
|---|---|---|
| Open casts | 30 fishers at once | One `CallLater` per cast for the bite; no tick, no per-frame work |
| Water check | One per cast | `TryGetWaterSurface` at one point; no world query |
| Spot lookup | One per cast | `LF_Places` by kind `FishingSpot` (a few dozen entries), distance-filtered |
| Network | Four small owner RPCs per catch | Never broadcast; the float is local |
| Items | A fish per catch | Fish stack by prefab. The counter takes a whole stack in one sale, so stockpiles stay in pockets and storage, not on the ground |
| Save | No new payload | Spot stock and casts are session state |

The server budget (all LF systems under 4 ms a frame, `performance.md`) is not at risk: 30 fishers
make about 30 RPCs and 30 timers a minute.

## 8. Assets

| Asset | Source | Note |
|---|---|---|
| Rod, float, bait tin | 3D pipeline (#339) after the licence gate (#664) | Asset source order: base game, then CC0 or CC-BY, then Meshy |
| Fish (one body, four to six species) | 3D pipeline, a material per species | Size by `scale` on the prefab is overridden by a script spawn (the parcel lesson), so each size is its own prefab with the same mesh at an authored scale, or three meshes |
| Counter, smokehouse, quay dressing | Base game: market stall, crates, `FishingGear_01`, nets, `BoatShed_01` | Kits only |
| Bite and strike sounds | Sound pipeline | A splash and a reel click |

## 9. Tests and proof

- LOGIC: the strike window (early, in, late, slack), the species and size roll against a fixed seed,
  the spot stock and regrow, the licence gate on the outlet.
- WORLD: a cast on GridWorld water (a pond cell in the debug grid, or a test seam for the water query
  if the grid has no water), bite, strike and the item in the inventory.
- Pack leg: the Everon tables resolve every species prefab.
- Session proof, once: cast, bite, strike and sell on the quay, with shots for the PR. The quay and the
  coves are checked by the town pipeline's audit and pictures, never by layout tests.

## 10. Build order (for the slice planner, not cards yet)

1. Config, species and table rolls, the strike-window rule: LOGIC tests.
2. Cast and strike RPCs on the rod, the water check, the bite timer: WORLD test.
3. The spot component, stock and regrow; the open-water table.
4. FISHER track and XP award; the `FISHING` licence; the counter and chandler outlets.
5. Assets through the pipeline (rod, bait, fish), then the pack's species and prices.
6. The harbour template slots and kits; the quay spot and two coves through the town pipeline.
7. The smokehouse recipe and the harbour master's delivery contract (town standing).
