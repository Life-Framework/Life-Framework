# World of Everon

The vision for the flagship world: the island of Everon in 1989, played as one living place. This page
covers who lives there, what each town is for, how the island opens up chapter by chapter, and which game
systems live where. It grows with each chapter release.

Nothing on Everon is built yet. This is the plan the building work follows. Status on 2026-11-16:

- The playtest town stays as the test world.
- The real Everon map is coming to the `LF_Everon1989` pack.
- Placements on Everon wait for the maintainer's review of four documents: this bible, the building and
  prop catalogs (`docs/everon/catalog-*.md`), the scout dossiers (`docs/everon/places/<slug>/`) and the
  planning doc (`docs/architecture/everon-world.md`).

How it sits with the rest of the project:

- The framework (core) decides what the game can do. The flagship pack decides what this server is
  ([core vs pack](../architecture/core-vs-pack.md)).
- Everything here is pack content built on core systems. Place names never go into core script.
- Milestones and chapters follow the [roadmap](../roadmap/README.md). This page doesn't re-plan them.

**The first location is still open.** The maintainer chooses it with us. The candidates are in
[Where the story starts](#where-the-story-starts).

## Contents

1. [The premise](#the-premise)
2. [World design: progression and coherence](#world-design-progression-and-coherence)
3. [Where the story starts](#where-the-story-starts)
4. [Places and their roles](#places-and-their-roles)
5. [Systems map](#systems-map)
6. [People and factions](#people-and-factions)
7. [Phasing](#phasing)
8. [Decided questions](#decided-questions)

## The premise

### Everon, 1989

Everon is a small French-speaking island. In 1989 it is quiet and a little behind the times. It has:

- harbours and fishing coves on its coast;
- orchards, collective farms and pasture inland;
- warehouses and a state factory in the larger towns;
- a military presence that has outlived its purpose, left behind as an airbase and a military harbour.

Tourism is small: a mountain hotel and a few guesthouses for summer visitors and hunters. People read the
news on a notice board, hear it on the radio and pass it on over a café counter.

**The era rule.** Everything shows what a local actually used in 1989, not an antique or rustic version
of it:

- a Lada, not a horse cart;
- a cassette radio, not a gramophone;
- a payphone and a pager, not a mobile phone;
- a state-shop counter with an abacus and a cash box, not a general store from a western;
- concrete apartment blocks next to old stone houses.

### The relocated

Three years ago, in 1986, the reactor accident emptied a region on the mainland. Its families were told to
pack for three days. They never went back. In 1989 the latest group of them arrives on Everon.

The player is one of them. They walk into town at dawn with a suitcase. The clerk at the municipal
office files Form R-89 and issues an identity card. The first night is in the hostel dormitory. After
that they have to make a life.

The islanders feel two ways about the newcomers:

- Some, like the café owner Odile Marchand, came the same way a few years earlier and help.
- Some resent the housing and the jobs the newcomers take.

That tension drives the story chapters. The newcomers need work, papers, a roof and respect, and the
island has a limited supply of each.

### The economy

| Sector | Where it lives | Player work |
|---|---|---|
| Fishing and the port | The harbour towns and the coves | Fishing (planned), dock loading, parcel and fuel runs to the quays |
| Farming | Orchards, collective farms, pasture villages | Fruit picking (exists), farm plots (exist), working the land (M8) |
| Industry | Warehouse districts and the state factory (M6) | Factory shifts, processing (exists), haulage |
| Mining and quarrying | The quarry and the hills | Miner (exists); the quarry is policed until M7 |
| Services | Every town | Taxi, fuel delivery, parcel courier (exist), shop work |
| Tourism | The mountain hotel and the coast | Hostel and hotel staff, guide work (ideas, not planned) |
| The underworld | The edges of towns, coves, barns and the old military sites | Theft, robbery, illicit chains, fencing, smuggling (M7) |

Money is physical. Cash is a stack of notes you carry. The bank balance is the only abstract money and
lives at the bank and the ATM.

### The authorities

- **The municipality.** The town hall and its clerk handle papers, registrations, licences, fines and
  property records. This is where a newcomer starts.
- **The police (gendarmerie).** One station per policed town. Officers are whitelisted players with
  ranks, an armoury, a dispatch board and a radio net. They patrol the roads and answer calls from
  payphones.
- **The hospital and clinic.** Medics are whitelisted players. They revive the downed and carry them to
  the hospital. A clinic in a smaller town can stabilise a patient, not treat them.
- **The army.** It has gone quiet. Its old sites are the island's high-risk ground.

### The underworld

Crime on Everon is small, local and personal:

- Vince the fence buys and sells behind a barn.
- Moonshine is distilled in a shed.
- Stolen cars are broken up for parts.
- Now and then someone hits the bank vault or a fuel station till.

It grows over the milestones: organisations in M5, then gangs, turf and smuggling by sea in M7.

## World design: progression and coherence

Everon is built chapter by chapter, and once it is finished it has to read as one island. This section
says how, borrowing layout and pacing ideas from three well-made game worlds.

**The feel (maintainer, 2026-11-16).** Grounded, light RPG:

- The island is realistic 1989 first.
- Progression comes from licences, reputation, jobs and story beats.
- There are no visible zone levels and no level numbers on the map.
- The ideas below shape layout and pacing. They do not add UI.

### Lessons from the references

**World of Warcraft: zones, hubs, flight paths, capitals.**

| What it does well | What it becomes on Everon |
|---|---|
| A zone has one clear job and one difficulty band, so you always know why you are there | Each place has one main role (port, farm, factory, coast, lawless ground). Difficulty is risk (police presence, lawless rules, rival players), not monster level |
| A quest hub puts a dozen reasons to stay within 50 metres | Every live place has a hub: the square, the café, a notice board, a payphone, a workplace sign-up and a shop. You can always see what to do next from where you stand |
| Flight paths link hubs once you have walked there; travel is part of the world | No bus (cut in M5). Travel is on foot, by your own car, by the taxi job and the taxi service call. Fuel stations and payphones are the waypoints along the roads |
| One capital city where everyone meets, trades and shows off | One hub town where the bank, the main police station, the hospital, the town hall and the market are. Every road leads back to it |

**RuneScape: geography by skill, a gathering map, one exchange.**

| What it does well | What it becomes on Everon |
|---|---|
| Where you gather depends on the skill: ore in the hills, fish on the coast, logs in the woods | The island's economy is a map. Apples come from the orchards, fish from the coves, stone from the quarry, goods from the warehouses. A job sends you where the work really is |
| Open sandbox: no forced order, you go where your skills point | No locked roads. Every live place can be driven to on day one. What's gated is the work inside (a licence, a job level, a record) |
| The Grand Exchange: one market everyone visits, prices you can read | The market board in the hub (The market moves, M6) shows what each product sells for today. The supermarket and the trader counters are where goods turn into cash |

**The Division: safe houses, the Dark Zone, control points, a map that fills in.**

| What it does well | What it becomes on Everon |
|---|---|
| Safe houses: a secure base in each district where you resupply | The hostel, then your own room or house (M8), and the town squares (safe zones, which exist). For an organisation, its hideout |
| The Dark Zone: high reward, open PvP, you can lose what you carry | Lawless ground, from M7 only (the whole island is policed until the gang chapter). The rule set already exists. The old military sites and the parked Zone idea are the candidates. Better yields and contraband, no police protection, and you can be robbed of everything you carry |
| Control points: hold a spot, the area changes | Turf (M7): an organisation that holds a place gets something concrete there, such as the fence's best prices, a cut of a chain or a safe stash |
| The map fills in as you play | Places light up chapter by chapter on the in-game map (the in-game map design, in progress) and on the board on the square, and a player learns the island by working it. The town board and the clerk point you to what is open |

**Two more that fit.**

- **Red Dead Redemption 2 and Mafia, for grounded towns.** The town is its people: a barber, a saloon, a
  shopkeeper who remembers you. The townspeople with names and routines (Odile, Gaspard, Rémi) are how
  Everon earns that. A cheap way to do it is that night changes who is on the street.
- **Stardew Valley, for the calendar.** A small world stays alive through routine: the market day, the
  weekly race, the Saturday dance in the hall. The weekly alpha session (Saturday 19:00 UK) is the
  natural time for the island's events.

### The progression arc

Unlocks work two ways (maintainer, 2026-11-16):

1. **Server-wide, by chapter release.** Each chapter brings new places online for everyone, and the map
   grows.
2. **Per player, inside a place.** Some rooms, counters and jobs need something the player earns: a
   licence, a reputation, a job level, a story step or a criminal record.

**The per-player gates.**

| Gate | Earned by | Exists? | Examples |
|---|---|---|---|
| Papers | The arrival chain (the clerk's Form R-89, the identity card) | Yes (quest chain) | Bank account, job sign-ups, the licence desk |
| Licence | The licence desk, a fee and a clean record | Yes (licence framework); the firearm permit is #1069 | Gun counter, commercial driving, the police armoury |
| Whitelist and rank | Application, staff review, rank | Yes | Police station back rooms and armoury, hospital staff rooms |
| Job level | Doing the job | Yes (job levels and XP) | Better routes, better pay, harder deliveries |
| Criminal record | Committed crimes | Yes (the fence: drill after 3 crimes, charge after 5, keycard after 8) | The fence's back room, the chop shop, the hideout machines |
| Organisation | Founding or joining one | Yes | Hideout, shared stash, turf (M7) |
| Story step | Finishing a story chapter beat | Yes (quest signals) | A townsperson's favour, a door that opens after The state factory |
| Reputation (standing) | Work for a faction (town, police, underworld) | **M6** (decided): a small system per faction | Mainly better jobs and better pay; later shop terms and the landlord's trust |

**Map states.** Five states, one per stage of the roadmap. Each lists what is live server-wide.

| State | Ships with | Places live | Systems that come online |
|---|---|---|---|
| **0. The first town** | M5: Arrival, The relocated, Armed and dressed | The first location: its square, town hall, hostel, café, police station, clinic, bank and ATM, general store and clothing counter, gun counter, fuel station, a barn on the edge for the fence | Arrival chain, hostel first night, catalogue and outlets, licence desk, starter jobs (parcel, taxi, fuel), safe zone, payphones, robberies, the fence |
| **1. The roads and the evenings** | M5: Evenings in the village, On patrol | The first location's outskirts and the roads to the nearest towns; a pub or hall; a dirt track; a quarry or orchard within a short drive | Chess, range, hall, drunk effect, dirt race, roadblocks and spike strips, named places on the dispatch board, the quarry as a policed work site |
| **2. The working island** | M6 Making a living | The second town (the work town), the nearest harbour, an orchard village, a scrapyard | State factory, market board, supermarket, clothing store, the bank proper (transfers), routes, scrap and salvage, fishing |
| **3. The contested island** | M7 Power and turf | The first lawless ground (old military sites, chosen with the gang chapter), smuggling coves, the prison, turf points in the villages | Gangs and turf, convoy, the big heist, laundering, prison work, detectives |
| **4. The whole island** | M8 Roots | Every village with houses for sale or rent, farms, dachas, garages | Housing, farming, garages and lockers, neighbours |

How a place comes online:

1. A **scout dossier** surveys its vanilla buildings.
2. **Pack data** maps each system slot to a building (approved before placing).
3. A **kit** fits out each slot.
4. One agent per place does the work, and the place goes up as a **draft PR with pictures** for the
   maintainer.
5. The audit and the pictures validate it.

The chapter's release note names the places that opened, and the in-game map and the board on the square mark them.

### Coherence once complete

**Hub and spoke.**

- **The hub** holds what everyone needs: the bank, the main police station, the hospital, the town hall
  and licence desk, the market board, the supermarket. Everything else is a spoke.
- **Spokes** are the specialised towns. The work town (warehouses, the factory). The harbour towns
  (fishing, the docks, smuggling by night). The farm villages (orchards, plots, housing). The coast (the
  lighthouse, coves, the hotel).
- **The edge** becomes lawless in M7: the old military sites, and the Zone if it returns. Until then the whole island is policed (maintainer, 2026-11-16).

The hub is Saint-Philippe, which is also the first location (decided 2026-11-16).

**The risk gradient.** Three bands until M7, from the square outwards, and a fourth from the gang chapter on. A player always knows which band they are
in from what they see: the safe-zone board, the police car, the lawless sign and HUD cue.

| Band | Where | Rules | Reward |
|---|---|---|---|
| Safe | Town squares, the hostel, the clinic | Safe zone: no harm within the radius (exists) | None; this is where you rest and meet |
| Policed | Town streets and main roads | Normal law; police patrol, roadblocks, witnesses | Legal pay, shops |
| Countryside | Farms, small villages, back roads, coves | Law applies, but help is far away and police are rare | Gathering, illicit chains, quiet robberies |
| Lawless (from M7) | Old military sites chosen with the gang chapter, the Zone if it returns | Lawless zone rules (exist): no wanted level for violence, police do not respond | Best yields, contraband, rare stock; you can lose all you carry |

**Resource and job geography.** Why a player crosses the island:

| Region | Produces | Sells or turns into goods at | Why people travel |
|---|---|---|---|
| Orchards and farms | Apples, vegetables, farm plots | Processing (jam, cider), the market | Raw goods go to the hub and the factory |
| Harbours and coves | Fish, dock cargo | The fish counter, the supermarket | Fresh goods inland; contraband in by night |
| Quarry and hills | Stone, ore | Processing, the factory | Haulage by truck; lawless risk from M7 |
| The work town | Factory goods, warehouse stock | Shops across the island | Deliveries out to every town |
| The hub | Services: bank, licences, market | | Everyone comes back to bank and trade |
| The old military sites | Salvage, rare stock | The fence, the scrapyard | High risk, high reward |

**Travel loops.** Three loops cover most play:

1. **The work loop.** Hub, then the work town or the farms, then processing, then back to the hub. About
   10 to 15 minutes by car.
2. **The coast loop.** Hub, then the harbour, then the coves, then the lighthouse, and back on the coast
   road. Fishing by day, smuggling by night.
3. **The edge loop.** The countryside (lawless ground from M7), then the fence or the chop shop. Never through
   the hub with hot goods.

**Spacing.** No stretch of road should go more than about two minutes' drive without something on it. Use
the island's own buildings for this: a fuel station, a farm, a chapel, a payphone, a lay-by where police
set up a roadblock, a bar at a crossroads. The scout dossiers measure real drive times and flag the empty
stretches.

## Where the story starts

The maintainer decides the first location with us. Here are four candidates. The catalog and scout
dossiers will confirm what each town's buildings support.

The facts below come from the installed game (build 26266485). The positions are the vanilla map
descriptors in `worlds/Eden/Eden.ent`, decoded by the planning spike (x, z in metres):

| Town | Position | What the game data shows |
|---|---|---|
| Saint-Philippe | 4503, 10772 (north-west coast) | A port: a harbour quay and crane at 4390, 11100, a factory, a landfill, a pub, a clothes shop-house, an airport at 4901, 11787 |
| Saint-Pierre | 9689, 1558 (south-east coast) | A harbour town (harbour at 9925, 1525; `E_Base_StPierreHarbour_FIA`) |
| Montignac | 4773, 7095 (inland centre) | A crossroads town near Provins and Entre-Deux; warehouses (`E_Base_MontignacWarehouses_FIA`) and a collective farm gate |
| Morton | 5135, 4012 (south coast) | Morton Bay and a small harbour (4957, 3876) |

Saint-Philippe and Saint-Pierre are at opposite corners of the island, about 10 km apart. Montignac sits
between them on the way.

### A. Saint-Philippe: start in the capital

The port town with the dock, as proposed on 2026-11-16.

- **Pros.**
  - It is the town people think of first, with the scale and the dock a capital needs.
  - It already has a factory, a pub and a clothes shop-house, so work, evenings and clothing have
    buildings from the start.
  - Players meet each other from day one, which matters on a 40-player server.
  - The bank, police station, hospital and town hall can all sit in one town, so state 0 is complete.
  - It suits the port economy and the M7 smuggling story.
- **Cons.**
  - A big town is a big first build. Each slot needs a scout and a kit before the first chapter can go
    live there.
  - New players could feel lost on the first night.
  - Starting at the capital leaves no "arrive in the big city" moment for later.
- **Systems fit.** Every state-0 system fits. The dock gives M6 routes and M7 smuggling a home.

### B. Saint-Pierre: start in a harbour town, Saint-Philippe opens as the capital

The other large harbour town.

- **Pros.**
  - A real harbour from day one, so fishing and dock work can lead in M6.
  - Gives the arc a second big moment: the capital opens in state 2.
- **Cons.**
  - Two large towns to fit out before the island feels whole.
  - Players are split between two towns early, when the server is small.
- **Systems fit.** State 0 fits if its buildings include a bank or an office the bank kit can use (to be
  scouted).

### C. Montignac: start in the work town

The inland town with the warehouses and the collective farm.

- **Pros.**
  - Ties the start to work and to The state factory (M6).
  - Inland, so the coast and the harbours become the M6–M7 frontier.
  - Fits "the relocated were sent where the jobs are".
- **Cons.**
  - No harbour, so the port economy waits.
  - Industrial towns can look grey on the first night.
  - Moves the capital decision to later.
- **Systems fit.** Strong for jobs and processing. Weaker for the bank and the hospital, which may need
  the hub to open.

### D. Morton: start in a small harbour town, the hub opens in M6

The south-coast town on Morton Bay, with a small harbour.

- **Pros.**
  - Close to the scale of the current playtest town, so the M5 slices transfer almost one for one.
  - Cheapest to build, so the first live Everon chapter ships fastest.
  - A harbour from day one, at village size: Gaspard the retired fisherman belongs here.
  - The classic RPG arc: a quiet start, then the city.
- **Cons.**
  - A small-town bank and police station strain belief.
  - Players outgrow it in a few sessions.
  - The road to the hub must be good, or the wait for M6 feels long. Saint-Philippe is about 7 km away
    in a straight line.
- **Systems fit.** The state-0 systems fit as small versions (a post office counter as the bank, a
  gendarmerie post).

**Recommendation: A, Saint-Philippe, with a hard limit on state 0.**

- Fit out only the square block in state 0: town hall, hostel, café, bank, police, clinic, shops.
- Leave the dock and the outer districts dark until states 1 and 2.
- This keeps the capital moment, because the dock and harbour districts open as a second wave, and keeps
  the first build small.

If the scout shows the square block lacks the buildings, fall back to D (Morton) with Saint-Philippe as
the hub in M6.

## Places and their roles

Every named place in the game data, with a proposed role.

- **Positions.** Compass positions, drive times and building fits come from the scout dossiers, not this
  table.
- **Roles.** A role is a proposal until the place's dossier is approved.
- **"Hub" rows.** These apply to whichever town the maintainer picks as the hub. Saint-Philippe is
  written as the hub here because it is the recommendation.

### Towns

**Saint-Philippe: the capital and port (hub candidate).**

| | |
|---|---|
| Jobs | Dock loading, parcel depot, taxi rank, fuel delivery base, shop clerks, municipal jobs |
| Shops | General store, clothing, gun counter (permit), supermarket (M6), market board (M6) |
| Services | Bank and ATM, town hall (clerk, licence desk, registry), hostel, café, hospital, main police station, impound |
| Crime | Bank vault, shop and fuel tills, pickpocketing, car theft from the streets; the docks by night |
| Police | Highest: the main station, foot patrols on the square, the dispatch board |
| Story | Arrival at dawn; the clerk's welcome; the first night in the hostel; The anniversary (M8) on the square |
| Day | Market noise, the café terrace radio, couriers leaving the depot |
| Night | Bar light, the night shift walking to the docks, a police car parked by the bank |

**Saint-Pierre: the second harbour.**

| | |
|---|---|
| Jobs | Fishing, fish processing, dock work |
| Shops | Fish counter, chandler (boat and fishing kit) |
| Services | Gendarmerie post, clinic, ATM |
| Crime | Smuggling through the harbour (M7), boat theft |
| Police | Medium: one post, patrols in the evening |
| Story | The shipment (M7) lands near here |
| Day | Boats out, gulls, nets on the quay |
| Night | Lamps on the quay, boats coming in late |

**Montignac: the work town.**

| | |
|---|---|
| Jobs | Warehouse shifts, the state factory (M6), the collective farm, haulage |
| Shops | Workers' canteen, hardware |
| Services | Gendarmerie post, factory clinic, union hall |
| Crime | Warehouse theft, the cash van on payday, the convoy job (M7) |
| Police | Medium; spikes at shift change |
| Story | The state factory (M6): Rémi's night shift |
| Day | Shift siren, trucks |
| Night | Floodlit yard, the night shift |

**Lamentin: the north harbour.**

| | |
|---|---|
| Jobs | Fishing, harbour work |
| Shops | Small store, fuel |
| Services | Gendarmerie post |
| Crime | Night landings, contraband stashes |
| Police | Low to medium |
| Story | A smuggling hint in M7 |
| Day | A working harbour |
| Night | Very dark, a few lights |

**Levie, Chotain and Provins: villages for the early spokes.**

| | |
|---|---|
| Jobs | Orchard picking, farm plots, delivery drop-offs |
| Shops | Village shop, a bar |
| Services | Payphone, perhaps a post office counter |
| Crime | Moonshine sheds, quiet burglary, the fence's barn |
| Police | Low: patrols pass through |
| Story | The townspeople's homes; Gaspard's routines |
| Day | Tractors, a dog barking |
| Night | Bar light, the moonshine smell (exists as a trace) |

**Morton: the south-coast harbour village (candidate D).** Same pattern as the coastal villages below,
with a small harbour on Morton Bay. If it is not the start, it is the M6 fishing spoke and Gaspard's
harbour.

**Gravette and Meaux: coastal villages with harbours.**

| | |
|---|---|
| Jobs | Small-boat fishing, coast deliveries |
| Shops | Bar, bait shop |
| Services | Payphone |
| Crime | Cove smuggling, boat theft |
| Police | Low |
| Story | Gaspard's old harbour, a cove the old men talk about |
| Day | Quiet coast |
| Night | Lights out at sea |

**Durras, Figari, Entre-Deux, Laruns, Regina, Tyrone, Le Moule and Vernon: farm and inland villages.**

| | |
|---|---|
| Jobs | Farming (M8), logging, pasture work |
| Shops | One shop or bar each, at most |
| Services | Payphone; housing for sale or rent (M8) |
| Crime | Chop shop barns, moonshine, turf (M7) |
| Police | Rare |
| Story | Neighbours (M8); where the relocated put down roots |
| Day | Fields, orchards |
| Night | Dark roads, the odd window lit |

### Points of interest

| Place (game id) | Proposed role | State |
|---|---|---|
| The mountain hotel (`MountainHotel_01_green`) | The first-night hostel, wherever the start is (decision in [the first night](../roadmap/m5-first-night.md)) | 0 |
| The quarry | Mining job, a policed work site until M7 (the lawless rule set exists in the playtest town) | 1 |
| Fisherman's Bay, Goat Bay | Fishing spots, cove smuggling | 2–3 |
| Coal Harbour | Bulk cargo, haulage routes | 2 |
| Hospital Harbour | Marks where the hospital stands; the medics' boat access | 0–2 (with the hospital town) |
| Andre's Beacon (lighthouse) | A landmark on the coast loop; a night signal spot for smugglers (M7) | 3 |
| Docks: Lancre, Le Bosc, West Cape | Quiet landings for the shipment and contraband | 3 |
| Black Lake | Fishing and an evening spot; a dirt race venue | 1–2 |
| Calvary Hill, the cemetery | Story beats (The anniversary, M8); an evening walk | 4 |
| Briars | Countryside hideout candidate | 3 |
| The airbase (the airport north-east of Saint-Philippe, 4901, 11787, if it is the same site) | Lawless ground from M7: salvage, rare stock, the Dark Zone equivalent. It is close to new players at the hub, so it is the last site to turn lawless | 3 |
| The military harbour, Benac base | Lawless ground from M7, the heist or convoy's end point | 3 |

## Systems map

Each system, its home and its status. "Exists" means it is in the framework today
([features](../features.md)). The planned ones follow the [roadmap](../roadmap/README.md).

| System | Home on Everon | Status |
|---|---|---|
| Arrival chain, clerk, Form R-89, ID card | Town hall in the first location | Exists (quest chain, voiced clerk) |
| First night | Hostel (mountain hotel dormitory) | M5 (#1758) |
| Safe zone | Each town square | Exists |
| Bank and ATM | Hub bank; ATMs in larger towns | Exists; transfers in M6 |
| Shops, catalogue and outlets | Hub stores; village shops | Exists (M5 Armed and dressed) |
| Gun counter and firearm permit | Hub; licence desk at the town hall | M5 (#1066, #1069) |
| Clothing | Hub clothing counter; full store in M6 | Exists in the playtest town; store M6 |
| Supermarket | Hub | M6 |
| Market board | Hub, the market | M6 |
| Civilian jobs (parcel, taxi, fuel) | Hub depot, taxi rank, fuel station | Exist |
| Gathering and processing (apples, jam, cider, quarry) | Orchards, the quarry, processing sheds | Exist |
| Farm plots | Farm villages | Exist; Working the land M8 |
| State factory, tasks | Work town | M6 |
| Fishing | Harbours and coves | **Gap:** no fishing system planned |
| Scrap and salvage | Scrapyard; military sites | M6 |
| Police station, armoury, ranks, jail | Main station in the hub; posts in the larger towns | Exist |
| Prison and prison work | A separate site | M7 (Behind bars) |
| Dispatch board, payphones, pagers, service calls | Police station; payphones in every place | Exist |
| Roadblocks, spike strips, cones | Lay-bys on the main roads | M5 (On patrol) |
| Investigation and witnesses | Everywhere crime happens | Exists; Making detective M7 |
| Hospital, medics, revive | Hub hospital; clinic in larger towns | Exist |
| Robberies (bank, hospital, fuel, shops) | Hub bank, fuel stations, tills | Exist |
| Forced entry, lockpicking | Any locked door | Exist |
| Fence (Vince) | A barn on the edge of the first location | Exists |
| Chop shop | A countryside barn | Exists (M2) |
| Illicit chains (moonshine, others) | Sheds, barns, coves | Exist; more chains later |
| Lawless zones | Military sites, from M7 | Exists (rule set); none on the island before M7 |
| Organisations and hideouts | Registry at the town hall; hideout buildings | Exist |
| Gangs and turf | Villages and the edge | M7; **gap:** no territory system yet |
| Convoy job | Work town to the hub, or the coast | M7; needs the AI route driver (Backlog) |
| Racing | A dirt track near the first location; Black Lake | Exists (race runner, records) |
| Chess, range, hall | Café or hall in the first location | M5 (Evenings in the village) |
| Radio and music | Café, shops, cars | Exists |
| Events | The square, the hall, the coast | Exists (event shell); The relocated is the first |
| Licences | Town hall licence desk | Exists (framework) |
| Housing | Every town and village | M8 (A place of your own) |
| Garages and lockers | Hub and the larger towns | M8 |
| Vehicles and plates | Car dealer, registry at the town hall | Exist |
| Reputation (standing) | Everywhere | M6: per faction, unlocks better jobs and pay |
| Public transport between towns | Roads | Taxis and cars for now; the bus (#1793) is reviewed when the state-2 cards are cut |
| Tourism work | The hotel and the coast | **Gap:** not planned |

## People and factions

These are the people already in the game. The internal file adds the proposed new cast and the
spoilers.

| Person | Role | Where | First seen |
|---|---|---|---|
| The clerk | Municipal clerk; files Form R-89, issues the ID card, sends you to the hostel. Voiced | Town hall, first location | Arrival (M5) |
| Odile Marchand | Café owner; came out of the zone three years ago with her mother's cups. Voiced | The café, first location | The relocated (M5) |
| Gaspard Lenoir | Retired fisherman; one suitcase, his wife's coat. Voiced | The square, the harbour | The relocated (M5) |
| Rémi Duval | Night-shift worker; worked 12 km from the reactor and still keeps his house key. Voiced | The streets at night; the work town | The relocated (M5) |
| Vince | The fence; sells tools to people with a record | A barn at the edge of the first location | Crime pays (M2) |

**Factions.**

- **The municipality.** The clerk and the town hall.
- **The police.** Players, whitelisted, with ranks.
- **The medics.** Players, whitelisted.
- **The employers.** The depot, the factory, the farms, the harbour.
- **The relocated.** A community, not an organisation.
- **The islanders.** The old families.
- **Player organisations and gangs.** Exist now; turf comes in M7.

## Phasing

Places go live in this order. The chapter list is the [roadmap's](../roadmap/README.md); this table only
places them on the map.

| Milestone | Chapter | Map state | Places that go live |
|---|---|---|---|
| M5 | Armed and dressed | 0 | First location: shops, gun counter, licence desk, police station |
| M5 | Evenings in the village | 1 | Café or hall (chess), the range, a dirt track nearby |
| M5 | On patrol | 1 | Main roads out of the first location, lay-bys, fuel stations |
| M5 | Arrival | 0 | Town hall, square, hostel |
| M5 | Story: The relocated | 0 | The road into town at dawn, the square, the hostel, the café |
| M6 | Making a living (all chapters) | 2 | The hub (if not the first location), the work town, the nearest harbour, an orchard village, the scrapyard |
| M7 | Power and turf (all chapters) | 3 | Lawless military sites, coves and docks, the prison, turf villages |
| M8 | Roots (all chapters) | 4 | Every village for housing and farming; garages |

M5's chapters ship on the playtest town first and move to Everon when the first location is fitted out.
The planning doc (`docs/architecture/everon-world.md`) owns that switch.

## Decided questions

The maintainer answered the world questions on 2026-11-16:

1. **The first location** is Saint-Philippe, square first. The dock and the outer districts open in M6.
   Morton is the fallback if scouting shows the square lacks buildings.
2. **The hub** is the same town, Saint-Philippe.
3. **Reputation** is a small system in M6, per faction (town, police, underworld). It is jobs-first: it
   mainly unlocks better jobs and better pay.
4. **Travel** is by taxi and car for now. The bus (#1793) is reviewed when a second town opens.
5. **Fishing** is a chapter in M6 Making a living.
6. **No lawless zones yet.** The whole island stays policed until the gang chapter in M7.
7. **The map** is a fully modded in-game map with waypoints, areas, POI icons and system markers
   (the in-game map design, in progress). The board on the square stays as well.
8. **Buildings.** A vanilla town house is converted where one fits (hostel, town hall, police). A
   purpose-built building goes on a free plot near the square only where nothing fits.
9. **The public page** stays up.

The execution plan and its cards are in [`everon-world.md`](../architecture/everon-world.md).
