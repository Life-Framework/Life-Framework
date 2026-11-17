# M6 Making a living (outline)

M6 is the update for players who have a job and some money and want more from both. It opens the
**working island**: the dock and the outer districts of Saint-Philippe, the road south to Meaux and the
work town of Montignac (map state 2). Work gets better as people come to know you (standing, a small
per-faction reputation), the coast gets a job of its own (fishing), and prices start to answer to what
players sell. This is an outline for the maintainer to review. No issues or slice cards are cut from it
until it is approved (the M5 rule). Read it with the [roadmap](README.md), the
[M5 outline](m5-outline.md), the [world bible](../everon/world-bible.md) and the
[Everon world plan](../architecture/everon-world.md).

**Theme.** The relocated have papers and a bed. Now they need a living, and the island has a limited
supply of work and trust. The story chapter, The state factory, brings work to Montignac.

**The feel (maintainer, 2026-11-16).** A grounded, light RPG with no visible levels. Progression comes
from licences, standing, jobs and story. The references give the shape, not the UI:

- **Hubs** (WoW): every place that opens has its own reasons to stay within 50 m: a counter, a board,
  a payphone, a sign-up.
- **Skill areas** (RuneScape): the work sits where it really is. Fish on the coast, crates on the dock,
  scrap at the yard, shifts at the factory.
- **A map that fills in** (The Division): places light up as you hear of them and visit them
  ([everon-map.md](../architecture/everon-map.md) section 4).

## Decided (maintainer, 2026-11-16, from PRs #2198 and #2199)

| # | Decision |
|---|---|
| 1 | M6 is map state 2, the **working island**: the dock and the outer districts open (world plan D5) |
| 2 | **Standing**: a small reputation system per faction (town, police, underworld) (D6) |
| 3 | Standing is **jobs-first**: earned and spent through jobs before anything else (D12) |
| 4 | **Fishing** is a chapter (D7) |
| 5 | **Travel** is taxis and cars. The bus stays cut (D8; M5 moved it to Backlog) |
| 6 | **No lawless zones until M7** (D11). The quarry, the airport and the military sites stay policed ground |
| 7 | **No visible levels** (D2). A gate speaks in words, never a number |

## Chapters

Each chapter is one player story and one 60 to 120 second video; its beat sheet is the acceptance
test. Chapters ship one at a time, each with the places it needs (see "Map state 2" below).
Chapters marked **new** have no GitHub issue yet.

| Order | Chapter | Framework (core) | Flagship (1989 pack) |
| --- | --- | --- | --- |
| 1 | The working island (**new**) | Taxi fares by distance between places (**new**), taxi stands as an `LF_Places` kind (**new**), map cards M8 onwards of #2223 (marks, shared marks for fares and job stops, discovery) | State 2a places through the town pipeline: the Saint-Philippe dock and districts, the road to Meaux and Meaux; a taxi stand per town; the harbour master (voiced); island data for the dispatch board's named places |
| 2 | Getting better at the job #1018 | **Standing** (**new**, [design](../architecture/reputation.md)), contract jobs with a standing stake (**new**), skill gates on job levels #764, pickaxe and shovel #650, medic ranks #652 | Tier words and awards (**new**), the first contracts (**new**), skill gates #765, tools #651, medic ranks #653 |
| 3 | The market moves #1016 | Market price state #748, price curve and board #762, stall board market #786, cash escrow #759, market stall placement #1034 | Market goods and the chalk board #763, market-day stalls #787, the supermarket on the square ([card](scopes/later/supermarket.md)) |
| 4 | Gone fishing (**new**) | **Fishing** (**new**, [design](../architecture/fishing.md)): rod, cast and strike, spots and stock, the FISHER track, the `FISHING` licence, the fish counter and chandler outlets | Species, prices and tables (**new**); rod, bait and fish through the 3D pipeline (**new**); the quay, Black Lake and two coves; the smokehouse; Gaspard's and the harbour master's lines and the inland delivery contract |
| 5 | Working the routes #1019 | Garbage rounds #205, fuel stock refilled by delivery #788, repair kit and unflip #648 | Collective-farm fuel depot #789, repair kit #649 |
| 6 | Scrap and salvage #1020 | Recycler #784, container loot #916 | Scrapyard recycler #785, 1989 loot tables #917, the scrapyard place |
| 7 | The bank #1010 | Bank transfer by account number #638; typed safes (#2288, landing from M5) | Bank interior #931 on the Everon bank building (merged, in-dev), transfers #639, store box safes in the shops #2282 |
| 8 | Story: The state factory #1021 | Uses existing systems: dialogue, quests, the event shell #757, standing gates; workplace tasks ([card](scopes/later/tasks.md)) | Montignac and the factory (state 2c); the factory director (voiced); quotas, shifts and the arc's beats; town standing earned on the line |

**Why this order.**

- **The working island first.** Every later chapter needs a place in state 2. It also moves the
  map epic's discovery and shared marks (#2223, M8 onwards) into players' hands, so the map fills in from
  the first week of M6.
- **Standing second.** Fishing, the routes and the factory all gate their better work on it. Built
  early, each later chapter only adds rows to the award and gate tables.
- **The market before fishing.** Fishing, scrap and loot are new money sources. The price curve lets a
  flooded counter fall and recover, so it should be live before they are.
- **Fishing fourth.** It is the headline new job and needs the asset pipeline for its rod and fish
  (#664 decides Meshy), which gives that pipeline three chapters of lead time.
- **The story last**, as in M5. It needs Montignac, the factory and standing, and carries the
  update's trailer.

### What each chapter gives a player

**1. The working island.** You can now take a taxi across town or down to Meaux. The quay is lit, the
harbour master has crates to move, and new places turn up on your paper map as people tell you about
them. Beats: a taxi called from a payphone on the square; the fare to the dock with the destination on
both maps; the harbour master's first job; a place marked "heard of" that becomes "visited".

**2. Getting better at the job.** The harbour master won't give you the early crates until he knows
you. A week of honest runs later he does, and they pay better. Walk away from one and he remembers.
Vince never says what he thinks of you, but his back room opens. Beats: a refusal in words; runs done;
the toast "people in town have started to trust you"; the contract taken and finished; a new miner's
shovel and the pickaxe unlocked; a medic promoted.

**3. The market moves.** Flood the trader with planks and the chalk board shows the price fall; a day
later it recovers. On market day players rent a stall on the square and sell to each other, paid in
cash at the clerk. The supermarket opens on the square.

**4. Gone fishing.** Buy a rod at the chandler and a licence at the town hall, cast off the quay and
strike when the float dips. Gaspard tells you about a cove; at dusk it pays better. Sell at the fish
counter, smoke the catch, or drive the day's catch inland on the harbour master's contract.

**5. Working the routes.** Empty the bins on a round, haul fuel from the collective-farm depot so the
stations stay stocked, and fix a broken-down car on the roadside with a repair kit.

**6. Scrap and salvage.** Search sheds, homes and the dump for loot that refills over time, and break
it down at the scrapyard recycler.

**7. The bank.** Walk into the Everon bank, deposit at the teller, send money to another account by
number. The vault room behind the locked door, and the small safes behind the shop counters, can be
robbed.

**8. Story: The state factory.** The factory in Montignac reopens and hires. The foreman sets the
quota, the shifts start, and something goes missing from the stores. The town talks about who did it.

## Map state 2: the working island

The world plan lists state 2 as `saint-philippe-dock`, `saint-philippe-districts`, `meaux`,
`montignac`, an orchard village and a scrapyard, with new slots `dock`, `fish-counter`, `factory`,
`market`, `supermarket`, `scrapyard` and a taxi stand per town (everon-world.md section 2).

Chapters ship one at a time, but a live state is one switch. **Recommended (question Q1): split state 2
into sub-states**, each its own id in `m_aLF_LiveStates` (#2208), each passing `town ready` on its own:

| Sub-state | Places | Opens with |
|---|---|---|
| 2a | Saint-Philippe dock and districts, the road to Meaux, Meaux | Chapter 1 |
| 2b | The coves and Black Lake (fishing spots), the chandler and fish counter on the quay | Chapter 4 |
| 2c | Montignac, the factory, the collective-farm depot, an orchard village | Chapters 5 and 8 |
| 2d | The scrapyard | Chapter 6 |

The market, the supermarket and the bank interior are slots in places that are already live (the
square), so they need no sub-state of their own.

**Travel.** Taxis and cars only. Saint-Philippe to Montignac is about 3.7 km in a straight line, five or six minutes by road. A
player without a car calls a taxi from a payphone (the taxi service call exists in
`LF_ServiceCalls.conf`), and the fare needs a price by distance between named places instead of today's
fixed 80. Each live town gets a taxi stand (slot `taxi`). The bible's spacing rule (nothing more than
about two minutes' drive without something on the road) holds through fuel stations, payphones and the
lay-bys from On patrol.

## Changes to the M6 issues already filed

| Issue | Change | Why |
|---|---|---|
| #658, #659 Bus and garbage routes | Split: the garbage round stays in Working the routes (with #205); the bus route goes to Backlog with the bus (#1793) | Decision 5 |
| #1018 Getting better at the job, beat 3 "a gate refuses and names the level" | The gate refuses **in words** ("Do more runs first", "The harbour master doesn't know you yet") | Decision 7 |
| The clothing store ([card](scopes/later/clothing-store.md)), listed under M6 in the README | Drop as an M6 chapter: the store was built in M5 (Armed and dressed) and becomes the `clothing` slot of state 0 | Already done |
| The supermarket ([card](scopes/later/supermarket.md)) | Lands inside The market moves as the `supermarket` slot, not a chapter of its own | Fewer, fuller chapters |
| #2283 Reserve vault building | Move to M7 (The big heist) | It is a heist target, and M6 has the bank and the shop safes |
| #2282 Store box safes | Into The bank chapter | Same robbery family as the vault |
| #1034 Market stall on the square | Into The market moves | The stall board needs it |

## Dependencies on M5 work in flight

| M5 work | What M6 needs from it | If it slips |
|---|---|---|
| Everon island epic #2201: the town pipeline (`scout`, `town map`, `fit`, `check`, `ready`), live states #2208, the cutover #2212 | State 0 and 1 live on MainWorld; the pipeline running places in parallel; sub-state ids on the header | M6 chapters ship on the playtest town and the grid first (as M5 did) and move to the island with the cutover; the island-only chapter 1 waits |
| Everon map epic #2223 (M0 to M7 before the cutover, M8 onwards through M5 and M6) | Shared marks for taxi fares and job stops (M11), discovery and `LF_MapKnown.Hear` (M13), the POI-kind slot check (M17), the perf budget (M18) | Chapter 1 ships with the waypoint only; "heard of" places wait for M13 |
| Safe types #2288 (`ws/safe-types`) | The `store_box` type for the shop safes (#2282) | The bank chapter ships the teller and transfers first, the safes after |
| Licence desk #1766 and permit #1069 | The desk sells the `FISHING` licence | Fishing ships with the counter open to all, and the gate is added later |
| Catalogue and outlets #988 | `fish_counter` and `chandler` outlets, the market's goods | None: merged |
| Job levels #749 | The FISHER track and new XP awards are config rows | None: merged |
| 3D asset pipeline #339, licence gate #664 | Rod, bait tin and fish meshes | Fishing waits; it is chapter 4 for this reason |
| Voice pipeline #343 | The harbour master and the factory director | Lines ship as captions first |
| Event shell #757, event scenes #1754 | The state factory's scenes | The story stays last |
| Placement #927, child #1034 | The market stall placed on the square | The market ships the trader board first |
| The Everon bank building (merged, in-dev) | The teller counter and vault room (#931) | None |

## Foundation

Plumbing that ships with M6 and has no video of its own:

- **Standing** ([reputation.md](../architecture/reputation.md)): the store, the event subscriptions, the
  payload, the gate and the owner RPC. Chapter 2 films it; every later chapter adds rows.
- **Contract jobs**: a standing gate and a stake on an ordinary job run.
- **Taxi fares by distance** and taxi stands per town.
- **The state 2 bake**: town-draft cards per slot, sub-state readiness, island data files for the
  dispatch board, the town board and racing.

## Risks

- **The bake is the long pole.** State 2 is four or more places and about 40 slots, each a town-draft
  PR the maintainer reviews with pictures. Mitigation: sub-states, so chapter 1 needs only the dock,
  the districts and Meaux; slot cards cut as soon as each place's mapping merges; the maintainer
  reviews by place, not by slot.
- **Island RAM on the alpha box.** A 16-slot 0grind box with the island, AI walkers and the new places
  (world plan D17). Measure the box after the cutover and before 2a goes live; keep each place under
  the per-place light and entity budget (`performance.md`).
- **New money in the economy.** Fishing, scrap, loot and contract pay all add cash. Mitigation: the
  market chapter before fishing, spot stock and loot cooldowns, the ledger reasons per source, and a
  one-week review of the ledger after each chapter ships.
- **Standing farmed by collusion.** Two players staging witness statements or chop-shop deliveries.
  Mitigation: daily caps, per-action cooldowns, losses only on proof, and the ledger and crime record to
  spot patterns.
- **Visible-number creep.** A sheet that shows points, a bar or a level would break decision 7.
  Mitigation: tier words only, the underworld never shown, and the UI kit review in each chapter's PR.
- **Underworld work without lawless ground.** M7 brings the lawless zones; in M6 the underworld earns
  standing only through the fence, the chop shop, the chains and Vince's contracts, on policed ground.
  That is intended (decision 6), but the underworld track may feel thin until M7.
- **No boats.** The base game has none, so fishing is shore and pier only. Players will ask; boats need
  the vehicle pipeline (#342, in Development) and belong to a later milestone.
- **Long walks.** With no bus, a player without a car depends on a taxi driver being on. Mitigation:
  the taxi service call answers on the pager; question Q4 covers the case with no driver online.
- **Spoilers.** The story chapter's public pitch, release note and dossiers stay inside the public
  bible; the internal spoiler list (`validate-public-bible`) guards the website.

## Rules

- Flagship content lives in `LF_Everon1989`, and the core never references it
  (`validate-addon-boundary`). Each new core system has a Framework issue and a Flagship issue.
- Core holds no coordinates, place names or layer names; systems find their places through `LF_Places`
  (golden rule 9). Island places come through the town pipeline only.
- Tests stay on GridWorld and the playtest town; island places are checked by audit and pictures.
- Base-game assets first; a new mesh comes through the 3D harness with a ledger row.
- Slices are cut per chapter with the roadmap pipeline (`docs/process/roadmap-pipeline.md`) after the
  maintainer approves this outline.

## Open questions for the maintainer

Each has a recommendation. "Accept all" takes every recommendation.

**Map and travel**

- **Q1. Live states per chapter.** Split state 2 into sub-states (2a to 2d above), each a live-state id
  that passes `town ready` on its own, or open state 2 in one go? **Recommend sub-states**: chapters
  ship one at a time, and a half-fitted Montignac must not hold up the dock.
- **Q2. Which harbour.** The bible lists the Saint-Philippe quay, Morton and Saint-Pierre. **Recommend
  the Saint-Philippe quay** for M6 (it is the hub's own dock, decision 1), with Fisherman's Bay and
  Goat Bay as the coves; Morton and Saint-Pierre open with their towns.
- **Q3. Taxi fares.** Fixed fare, or by distance? **Recommend by distance between named places**, with a
  floor (today's 80) and a cap, so a run to Montignac pays for the drive back.
- **Q4. No taxi driver online.** Leave the player to walk, or add an NPC taxi when no driver is on duty?
  **Recommend walking in M6**: an AI driver needs the route driver (#772, Backlog with the bus). Revisit
  if the playtests show long walks.

**Standing** ([reputation.md](../architecture/reputation.md))

- **R1. What a player sees.** Words only: the toast, the clerk's "My file" and a line on the ID card for
  town and police; the underworld never shown. **Recommend yes.**
- **R2. Spending.** Standing as a stake on contract jobs (lost on abandon), never a currency, never
  bought or traded. **Recommend yes.**
- **R3. Decay.** Does unused standing fade? **Recommend no decay in M6**; review after the first month.
- **R4. Talking to the police.** Does a witness statement that identifies a suspect cost underworld
  standing? **Recommend yes**, a small loss: it gives the street a reason to keep quiet, and the police
  gain stays.
- **R5. Police officers.** Does police standing apply to on-duty officers? **Recommend no**: officers
  progress by rank (the whitelist ladder); police standing is how the gendarmerie sees a civilian.

**Fishing** ([fishing.md](../architecture/fishing.md))

- **F1. Licence.** Fishing free, selling to the counter needs a 50 cash permanent licence; unlicensed
  fish go to the fence at 60 %. **Recommend yes.**
- **F2. Boats.** Shore and pier only in M6. **Recommend yes**; boats wait for the vehicle pipeline.
- **F3. Pay band.** About 450 an hour for a new fisher, up to about 900 with level, a better rod, a cove
  at dusk and smoking: the parcel to taxi band, under police duty. **Recommend yes.**

**Chapters**

- **C1. Standing inside #1018.** Put standing in Getting better at the job (it is about better work)
  rather than a chapter of its own. **Recommend yes.**
- **C2. The supermarket and the clothing store.** The supermarket becomes a slot in The market moves;
  the clothing store is dropped from M6 because M5 built it. **Recommend yes.**
- **C3. The reserve vault (#2283).** Move to M7's The big heist. **Recommend yes.**
- **C4. Underworld contracts in M6.** Vince offers two contracts (a car to order, a package at night),
  on policed ground. **Recommend yes**; more wait for M7's gangs.
