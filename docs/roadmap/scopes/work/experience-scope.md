# Work: the experience scope

Milestone: M6 Making a living (chapters 2 and 5, the new Bigger wheels chapter), with tails in M7 and M8
Extends: `LF_JobLevels` (#749), skill gates #764/#765, standing ([reputation.md](../../../architecture/reputation.md)),
the market curve (`LF_PriceCurve`), the carry system and cargo bed (`docs/features/carry.md`), the vehicle lot and
garage, the licence desk, qualifications, the F2 personal file and its Next tab (`docs/features/activities.md`),
the mining-chains epic #3206, the lumber plan ([lumber.md](../../lumber.md)), fishing ([fishing.md](../../../architecture/fishing.md))
Epics this scope plans for: #4140 Professions and skills, #4144 Vehicle and inventory progression, #4141 Truck
dealer, #4142 Civilian helicopter dealer, #4143 Truck and pilot licences

This is a design for the maintainer to review. Nothing here is filed or dispatched. The two older cards in this
folder ([civilian-jobs.md](civilian-jobs.md), [resources.md](resources.md)) stay as the record of what M1 built.
This file covers how all the work fits together from hour 1 to hour 50.

**The idea in one line.** Every way to earn a living is a **trade** with its own place on the island, its own
foreman, its own tools and its own long climb. Each climb pays off in three things a player can feel: a better
tool in the hand, a bigger vehicle at the curb, and a new place on the map. Pay rises a little. Comfort, reach
and status rise a lot.

---

## 1. Player fantasy and core loop

**Hour 1.** You are a relocated worker with $100, a borrowed car and a paper map. The clerk sends you to the
orchard. You pick apples, press juice and sell it at the market. The F2 Next tab offers the mine on Coalman's
Brow, 400 m from the square. You buy a pickaxe, take the job at the foreman's clock and swing at a rust-veined
rock. Every few swings the ore drops with a clack, a small bell rings and the toast says "Miner: level 2. Bruno
says the coal seam at East Rise will take you on soon." By the end of the hour you have reached level 3 or 4 in two trades.
You know where the hardware store, the dock warehouse and the post office are, and you have about $450 in your
pocket. The hook is that something new happens every 8 to 12 minutes: a level, a place you hear about, a
tool you can now afford.

**Hour 10.** You have picked a main trade and a side trade. Say mining and hauling. You have a steel pickaxe
that wears down and gets repaired at the hardware bench with scrap you salvaged. You own a UAZ-452 van. You
passed the truck check in a lent Ural at the licence desk and you are saving for your own truck. Mining level
12 opened Highstone hill and the smelter in the centre-east compound. The coal you need comes from a level-4
player who digs it at East Rise and sells it to you at the smelter gate. The mine's shift board lists the
top five miners of the week, and you are sixth. Your evenings are planned: the first shift of the day pays
double XP, so you log in, run one van load and check the board. Hauling and mining feed each other. A truck
fills the smelter's hopper in one back-up, and Hauling 10 lets you take the dock's export orders.

**Hour 50.** You are a master in one trade and a journeyman in two. "Master Miner" is printed on your ID card.
You wear the yellow hard hat with the lamp that only masters get, and once every 20 minutes you can call
"Second wind", which doubles your swing rate for a minute. You drive your own Ural with its company livery, and
you run gold at Ore Ridge, 10 km out, where the police rarely patrol and other players know what is on your
truck. You hire newer players at the gate: they dig, you haul and smelt, and you all split the take in cash
at the furnace. You have chosen two perks. A season of play ends with your name on the Workers of the Month
board in the town hall. What keeps you here is the people. Your coal supplier, your hauler, the rival miner on
the board and the robber who watches the copper road are all other players.

**Why it is addictive.**

| Hook | How it shows | Where it lives |
|---|---|---|
| Short, frequent rewards early | Levels 1 to 5 in each trade come inside the first 2 hours of doing it; each one has a named unlock | `jobs.tracks` thresholds, the level-up moment (card W3) |
| Long climb later | 30 levels, about 60 hours to master one trade | Same table |
| Choices | A perk pick at levels 10 and 20: one of two, changeable at the labour office for a fee | Perk section (card W8) |
| Something to show | Titles, mastery clothing, liveries, the shift boards | Cards W13, W15, W15b, W22 |
| A reason to come back daily | First shift of the day: double XP on the first 30 minutes of a trade | Card W5 |
| Variety | 9 trades on 25+ real places; every chain differs in distance, vehicle, risk and market | Sections 3 and 7 |
| Other players | Reagents and inputs that one trade makes and another needs; hauling for others; robbery risk on rich cargo | Section 2 |

---

## 2. Systems: how work fits the other areas

### The trades

One XP track per trade, all through `LF_JobLevels` (the store, the awards, the daily caps and
`LF_OnJobLevelChanged` already exist). Two tracks are new: SMELTING (split from MINER so the furnace has its
own climb and a reason to trade with miners) and SALVAGE (scrap, garbage and wreck salvage). DELIVERY is shown
to players as **Hauling**.

| Trade | Track | XP sources (rows in `jobs.xpAwards`) | Buyers it feeds |
|---|---|---|---|
| Mining | MINER | ore swing (exists), stone dug, gold flake | smelter, cement works, wholesale |
| Smelting | SMELTING (new) | per bar out of a furnace or refinery batch (#3239 moves here) | hardware, wholesale, building work |
| Lumber | LUMBERJACK | log felled (exists), log sawn | timber counter, bakery firewood, smokehouse |
| Fishing | FISHER | fish landed (exists), smoked fish | fish counter, cannery, smokehouse |
| Farming | FARMER | fruit, crop, grain lot, honey | market stall, juicer, mill, bakery |
| Hauling | DELIVERY | parcel, postal, fuel, taxi, export order, timber haul | everyone: it moves other trades' goods |
| Salvage | SALVAGE (new) | pile searched, bench load, bin round, wreck stripped | garage, recycler, smelter (scrap iron) |
| Police | POLICE | patrol minutes, arrest, report closed | kit, titles, liveries only (K4) |
| Medic | MEDIC | revive, delivery (exists) | kit, titles, liveries only (K4) |

Crime trades (lockpicking, the illicit chains) are the crime scope's. They reuse the same track type and the
same level-up moment so all progression feels like one system.

### Shared systems this scope uses or needs

| System | What work takes from it | What work gives back |
|---|---|---|
| XP and skills (`LF_JobLevels`) | Tracks, awards, caps, the change event | Longer tables, perks, abilities, the level-up moment |
| Standing (town, police, underworld) | Contract gates; the harbour master's crates; the dealer's credit | Every finished contract adds town standing. Abandoning one costs standing |
| Wanted level and the legal system | A wanted player is refused at every desk; licences can be seized (`LF_LicenceSeizure`) | New licences to seize: truck and pilot. Overloading and flying in a town become charges for the law book (M7) |
| Gangs and organizations | Gang hideout perks (the crime scope) can include a "company yard" for legal hauling crews | Rich cargo on the road is the gangs' target: hijack, chop and fence |
| Vehicles and cargo | Lot, garage, keys, cargo bed, vehicle storage, tow | The capacity ladder, feed-from-truck, two dealers, liveries |
| Economy and market | `LF_PriceCurve`, buyers with a demand cap, the wholesale warehouse, the ledger | New goods and curves per chain; a "full perks" line per loop in `tools\cli economy` |
| Map and run chains | `LF_JobMapMarks`, run lines on the map, "heard of" places (#2223 M13) | Each new site is a POI that a foreman or a level-up "tells you about" |
| HUD | Toasts, the hint card | A cooldown chip for an active ability; the level-up card |
| F2 personal file | Skills column, detail card, Next tab | Perk picks shown, the title, "next level opens", the weekly rank |
| F3 welcome guide | The first-hours route | A "Trades" page: one paragraph per trade, where it starts |
| Quests and activities | Guided starters, quest XP | A starter per trade, each ending at a foreman who explains the climb |

**Cross-trade links.** These are what make it one economy rather than nine reskins:

- Coal (Mining L4) is the smelter's reagent (Smelting). The power plant also buys it (suggested addition A1).
- Firewood (Lumber) is fuel for the smokehouse (Fishing) and the bakery oven (Farming flour into bread).
- Scrap iron (Salvage) goes into the smelter as a cheap iron charge, and repairs tools at the hardware bench.
- Sand and limestone (Mining) plus cement bags (Smelting at the works) feed player building work.
- Every bulk chain sells best at a buyer far from the source, so a Hauling player can take a cut by moving
  someone else's goods (the haul order, card W24).

---

## 3. Places

All coordinates are scout centres (`tools\cli scout list`). Distances are straight lines from the
Saint-Philippe square. Every site is built through the town pipeline (`scout`, `town map`, kit, `town fit`,
`look contact`, polish), dressed with base-game props, with a sign and a map POI. Machines stand at or in a
real building. Sites already planned in another epic are marked with that epic and are not re-planned here.

| Place | Where | What it looks like | What is there | Trade, level | Status |
|---|---|---|---|---|---|
| Coalman's Brow iron mine | 4337, 10349; 0.4 km | Granite face above the farms, timber props, a rusty tin shed | Foreman's clock, six iron rocks, furnace, iron buyer | Mining 1, Smelting 1 | Live |
| Saint-Philippe labour office | Town hall, the square | A counter with a brass bell, filing cabinets, a cork board | The Workers of the Month board, perk change, the trade pamphlets | All | New (W13) |
| Hardware store bench | South-east road | Back room with a vise, grinder and pegboard | Tool repair, tool tier sales | All tools | New (W9) |
| Highstone hill iron mine | 5014, 8475; 2.3 km | Open cut on a wooded hill, ore heaps, a conveyor | Richer iron rocks (6 gathers) | Mining 12 | #3242 |
| East Rise coal seam | 6712, 7944; 3.6 km | A black cut along a ridge, a loading ramp | Coal lumps | Mining 4 | #3243 |
| Centre-east smelter | 6459, 6482; 5.1 km | Industrial compound, a brick furnace hall, coal pile, truck bay | Smelter (2 ore + 1 coal) | Smelting 8 | #3244 |
| Humboldt Hill copper mine | 2453, 7188; 4.0 km | Green-stained rock, a fenced adit | Copper rocks | Mining 15 | #3247 |
| Levie refinery | 7464, 4739; 6.7 km | Tanks and pipework behind a fence | Refinery (2 ore + 1 acid) | Smelting 15 | #3248 |
| Chotain hardware | 7086, 6012; 5.4 km | Village shop with carboys on a pallet | Acid sales | - | #3235 |
| East mountains quarry | 8849, 3957; 8.1 km | Terraced quarry, crusher, dust | Limestone face (20 gathers) | Mining 6 | #3250 |
| Morton cement works | 5135, 4012; 6.7 km | Silos by the harbour | Cement works | Smelting 6 | #3251 |
| Goat Bay sand pit | 6295, 3957; 7.0 km | Beach with a sand ramp | Sand, dug free | Mining 1 | #3252 |
| Ore Ridge gold workings | 10741, 2515; 10.3 km | Remote ridge, an old adit, a hut with a stove | Gold-bearing quartz, a small assay table | Mining 25, Smelting 25 | New (W27) |
| Saint-Philippe timber yard | West edge of town | Fenced yard, log stacks | Lumberjack desk, saw | Lumber 1 | Live |
| Le Moule sawmill (the island sawmill) | 3116, 5215; 5.7 km | Long saw hall by the road | Desk, two saws, boards | Lumber 1 (sawing gated by level) | Live |
| Hornbeam Valley | 5322, 7757; 3.1 km | Old hardwood forest along a stream | Hornbeam and linden, hardwood only | Lumber 10 | New site (W28) |
| Régina sawmill | 7280, 2519; 8.7 km | South sawmill with a rail yard | Bulk log buyer at a falling price | Lumber 8 | New (W28) |
| Spring Pond, the cove, Black Lake | Town; west coast; 9482, 3048 | Jetty, sheltered cove, far lake | Fishing spots | Fishing 1 / 1 / 10 | Live |
| Pinewood Lake | 4538, 5967; 4.7 km | Lake by the Lakeside hostel | Pike and eel spot (new species) | Fishing 15 | New (W29) |
| The fish cannery | Scout entry `Fish Cannery` (its centre matches the Lakeside hostel's; check with `scout`) | A long shed with tins on pallets | Bulk fish buyer with demand cap | Fishing 5 | New (W29) |
| Wheat field and weighbridge | South-west edge of town | Gold field, scale house | Contracts, sell or bag grain | Farming 1 | Live |
| North-east farm | 5005, 9969; 0.95 km | Farmyard, barns, a tractor shed | Second field, the mill | Farming 8 | New (W30) |
| Étoupe orchard hamlet | 5358, 9972; 1.2 km | Hamlet with walled orchards | Orchard rows, the cider press | Farming 3 | New (W30) |
| Post office and Meaux office | Main street; Meaux square, 1.25 km | Kiosk and stand | Parcel, postal van | Hauling 1 | Live |
| Montignac fuel depot | 4773, 7095; 3.6 km | Collective-farm depot with tanks and a pump island | Fuel haul start | Hauling 2 | #1019 cards |
| Saint-Philippe dock warehouse | The harbour | Brick warehouse, roller doors, crates | Wholesale buyer, export order board | Hauling 10 | Live (buyer), new board (W24) |
| North-east industrial compound truck yard | 5054, 10855; 0.64 km | Fenced yard, a sales hut, trucks in a row | The truck dealer | Hauling licence | New (W19) |
| Landfill | 5353, 10920; 0.95 km | Fenced tip with a bulldozer, gulls | Garbage tip, wreck salvage | Salvage 1 | New (W25) |
| Scrapyard | East of town, 0.6 km | Piles, cranes, the recycler bench | Search, recycle | Salvage 1 | Live |
| Power plant | 4355, 10098; 0.6 km | Cooling stack, coal yard | Coal buyer (suggested A1) | Mining 4 | Proposal |
| Airport | 4901, 11787; 1.2 km | Runway, two hangars, a tower | Flight school, helicopter dealer, pad | Pilot licence | New (W31-W33) |
| Lighthouses | 3921, 10769 (0.5 km); 6483, 9656; Morton 6054, 3382; Erquy 11380, 11197 | Keeper's cottage at each | Supply drop for the heli run (suggested A4) | Hauling 15 | Proposal |

**Hub rule (M6 "Hubs").** Each work site has at least three things within 50 m: the foreman or desk, a shift
board, and either a buyer, a payphone or a bench. A site with only rocks is a resource node, not a place.

---

## 4. People

All are vanilla-dressed NPCs through `tools\cli outfits generate` (roles in `tools/assets/outfits/roles.json`)
and the NPC dresser. Voice lines go through the dialogue pipeline (`tools/assets/dialogue/<npc>/`, ElevenLabs,
US English, a French accent allowed). Dialogue uses number keys with "0. Goodbye", and every line is voiced.
Each foreman speaks the gate words ("The foreman doesn't trust you with the deep face yet"), never a number.

| NPC | Where | Role | Clothing (vanilla prefabs) | Voice lines (about 8 each) | Behaviour |
|---|---|---|---|---|---|
| Bruno, mine foreman | Coalman's Brow | Takes miners on, explains the climb, names Highstone at Mining 10 | `Hat_Hard_01_yellow`, `Jacket_Denim_01_strippedShirt_dirty`, `Pants_Trousers_01_DarkGrey_Dirty` | Greet, take the job, first ore, refuse (no pickaxe), level 5 ("You've got the knack"), tells of Highstone, shift whistle, goodbye | Stands at the clock 06:00-20:00, then in the shed with a lamp |
| Smelter master | Centre-east smelter | Gates the smelter, buys coal from players at a fixed floor | `Hat_FlatCap_01_darkgrey`, `Jacket_Raincoat_01_green` (a leather apron is a Blender option) | Greet, "no coal, no iron", level gates, synergy hint | Walks between furnace and coal pile |
| Sawmill boss | Le Moule sawmill | Hires lumberjacks, tells of Hornbeam Valley at Lumber 10 | `Hat_TruckerCap_01_sawmillregina`, `Shirt_CottonShirt_01_V4_Dirty`, `Pants_Trousers_01_DarkBrown` | Greet, hire, "mind the saw", hardwood hint, goodbye | At the desk; ear protectors prop on the bench |
| Gaspard (exists) and the fishmonger | Bench in town; dock counter | Gaspard tells of spots; the fishmonger buys and names the rare fish | Fishmonger: `Pants_Fisherman_01_yellow`, `Jacket_Raincoat_01_yellow`, `Hat_Knitted_01_blue` | Greet, sell, "tuna today?", rare fish praise | Behind the counter by day; nets mending at dusk |
| Farm co-op chair | North-east farm | Hands out field contracts, the mill | `Hat_FlatCap_01_lightgrey`, `Shirt_CottonShirt_01_V2`, `Pants_Trousers_01_LightBrown` | Greet, contract, harvest praise | Walks the yard; sits on the bench at noon |
| Depot dispatcher (fuel_clerk role exists) | Montignac depot | Fuel loads, the tanker | Existing fuel clerk outfit | Existing plus "tanker's yours" | At the pump hut |
| Harbour master (exists) | Harbour office | Export orders, contracts | Existing | Adds export-order lines | Existing |
| Garbage foreman | Depot by the landfill | Hands out rounds | `Jacket_Raincoat_01_orange`, `Hat_Knitted_01_red`, dirty trousers | Greet, round, "mind the gulls", tip praise | Smokes by the truck |
| Driving examiner | Licence desk | Runs the truck check course | `Jacket_Suit_01_Grey_V1`, `Pants_Suit_01_black_V1`, a clipboard prop | Briefing, pass, fail with the reason, retake | Rides in the passenger seat during the check (an NPC seated, or a voice from the radio if seating an AI is unreliable) |
| Truck salesman | Truck yard | Sells trucks | `Jacket_Suit_01_Brown_V1`, `Hat_FlatCap_01_blue` | Greet, pitch per model, "licence first", sold | Leans on the lead truck |
| Flight instructor | Airport | Paper test, check flight, pilot licence | `Jacket_Denim_01_brownShirt`, `Watch_Vostok`, sunglasses if a vanilla prop exists | Briefing, pass, fail, safety lines | At the hangar desk; on the pad during a check |
| Helicopter dealer | Airport hangar | Sells the helicopter | `Jacket_Suit_01_black_V1` | Greet, pitch, "pilot licence first" | In the hangar office |
| Labour office clerk | Town hall | Perk changes, the board, trade pamphlets | office role outfit | Greet, "change your specialty for $500", weekly winners | At the counter in office hours |

**Shift life.** Each site sounds a shift whistle at 06:00 and 18:00 game time. Foremen stand at the clock in
the day shift, and two or three walkers in work clothes stand around at the bigger sites (the miner, logger
and farmer roles exist). The daily first-shift bonus resets with the morning whistle, so the sound means
something.

---

## 5. Assets

Source order: vanilla first, then CC0/CC-BY, then our Blender build scripts. Meshy is not used anywhere in
this scope (it is only for fish and masks). Sounds are ElevenLabs.

### 3D assets, tools and props

| Asset | Source | Why |
|---|---|---|
| Tool tiers: worn, standard and steel pickaxe and axe | Blender build scripts over our existing `Pickaxe.et` / `Axe.et` meshes: rust, clean, blued steel with a red handle | Players must see a better tool at a glance. A material swap is cheap, and a new mesh is not needed |
| Sledgehammer (stone, Mining 6) | Blender build script | No vanilla sledge as an item |
| Two-man crosscut saw (Lumber 8, co-op) | Blender build script | A 1989 forest tool that makes two players work together |
| Chainsaw (Lumber 20) | CC0 search first (Poly Haven, Sketchfab CC0), else Blender | Not in vanilla |
| Miner's lamp hard hat (Mining 20, mastery) | Vanilla `Hat_Hard_01_yellow` plus a vanilla head-lamp light component (asset search for a light prefab), configured | Re-use; only the light is new config |
| Fishing rods: bamboo, fiberglass, competition | Existing rod (`tools/assets/fishing-rod`) with Blender material and length variants | Same pipeline as the current rod |
| Ore sack, coal sack, crate | Vanilla sandbag and wooden crate families (`Sandbag`, `BoxWooden_01`) as shoulder loads | The carry system already takes them |
| Shift board, Workers of the Month board | Vanilla notice/cork board props with our sign and visual-board system (`docs/architecture/visual-boards.md`) | In-world text |
| Work bench with vise and grinder | Vanilla workshop furniture and tool clutter | Exists in the base game |
| Gold assay table, quartz ore | Vanilla table plus a Blender ore lump recoloured from the existing `ore-nugget` asset | Re-use the ore pipeline |
| Mine, smelter, sawmill and yard dressing | Vanilla: ore heaps, conveyors, timber props, fences, work lights, pallets, drums | Mining-chains rule: real sites, base-game props |
| Flight school props: windsock, pad markers, a chalkboard | Vanilla airfield props where they exist; the pad marker as a decal | Asset search at card time |

### Vehicles and liveries

| Vehicle | Prefab family (vanilla) | Tier | Livery |
|---|---|---|---|
| Sputnik T601, Dana 2101, S105 | `Prefabs/Vehicles/Wheeled/T601`, `V2101`, `S105` | Car | Civilian colours (exist) |
| UAZ-469 | `UAZ469_*_CIV_*` | Pickup / light 4x4 | Civilian |
| UAZ-452 van | `Prefabs/Vehicles/Wheeled/UAZ452` | Van | "Everon Post" (the postal van exists), a "Co-op" farm van |
| S1203 van | `Prefabs/Vehicles/Wheeled/S1203` | Van | Civilian; Bakery |
| Ural-4320 | `Prefabs/Vehicles/Wheeled/Ural4320` (cargo variants) | Truck | Company liveries through the livery pipeline (#4155): "Highstone Mining", "Montignac Timber", "Everon Fuel" |
| M923A1 | `Prefabs/Vehicles/Wheeled/M923A1/M923A1.et` | Heavy truck | Civilian grey, "Harbor Freight Co." |
| Fuel tanker | Vanilla Ural or M923 fuel variant (asset search at card time) | Tanker | "Everon Fuel" |
| UH-1H | `Prefabs/Vehicles/Helicopters/UH1H/UH1H.et` (unarmed) | Helicopter | Civilian white and red (livery spike) |
| Mi-8MT | `Prefabs/Vehicles/Helicopters/Mi8MT` (unarmed transport, e.g. the SP01 variant's base) | Heavy helicopter | "Aero Everon" |

Every lettering is US English. Liveries are the mastery reward and the company look. They come from the
livery pipeline epic (#4155), never by hand per vehicle.

### Clothing

All vanilla, picked from what the repo already uses. Mastery sets are a fixed outfit per trade, granted at
level 30 and sold nowhere:

- Miner: `Hat_Hard_01_yellow` (with the lamp), `Jacket_Denim_01_strippedShirt_dirty`, `Pants_Trousers_01_DarkGrey_Dirty`
- Lumberjack: `Hat_TruckerCap_01_sawmillregina`, `Shirt_CottonShirt_01_V5_Dirty`, `Pants_Trousers_01_DarkBrown`
- Fisher: `Hat_Knitted_01_blue`, `Jacket_Raincoat_01_yellow`, `Pants_Fisherman_01_yellow`
- Farmer: `Hat_FlatCap_01_lightgrey`, `Shirt_Turtleneck_01_beige`, `Pants_Trousers_01_LightBrown`
- Hauler: `Hat_TruckerCap_01_everon`, `Jacket_Denim_01_brownShirt`
- Salvager: `Hat_Hard_01_white`, `Jacket_Raincoat_01_orange`
- Smelter: `Hat_FlatCap_01_darkgrey`, a leather apron (Blender, the only new garment)

Ordinary versions of these items stay on sale at the clothing store. The mastery outfit differs in the
title on the ID card and in one item that is never on sale (the lamp, the apron, the "everon" trucker cap
variant).

### Sounds (ElevenLabs SFX unless the base game has it)

| Sound | Use |
|---|---|
| Level-up sting: a short bell and a single brass note | Every level; a longer version at 10, 20, 30 |
| Shift whistle (steam, two blasts) | 06:00 and 18:00 at the mines, sawmill and dock |
| Ability start and end, one per trade (pick ring, axe thunk, reel spin, engine rev) | Active abilities |
| Furnace roar loop, sawmill whine loop, conveyor clank loop | Site ambience |
| Crosscut saw stroke, chainsaw idle and cut | Tools |
| Tool wear crack and "dull" scrape | Tool at low condition |
| Gulls and harbour bell | Dock and landfill ambience |
| Truck air brakes, tipper hydraulics | Truck bed tip (vanilla engines cover the rest) |
| Rotor wash and engines | Vanilla helicopters |

### UI elements (in-world first)

| Element | Where | Kind |
|---|---|---|
| Shift board | Each work site | In-world printed board: the top five of the week, today's quota, "First shift bonus: open until the evening whistle" |
| Workers of the Month board | Labour office | In-world board: the top worker per trade |
| Price chalk board | Each buyer | In-world (exists through the market chapter) |
| Tool tag | On the tool | The action list shows "Pickaxe (steel), condition: good". No number on the tool |
| Vehicle load line | On the tailgate | The action list: "Cargo bed: 4 of 12", "Feed the furnace from the truck (3 coal sacks)" |
| Weighbridge display | Weighbridge | In-world screen (exists) |
| Level-up card | HUD, bottom corner, 6 s, no input | Small kit card: trade, new level, "Opens: Highstone hill" |
| Ability cooldown chip | HUD, beside the hint area | A small icon with a ring while the ability recharges |
| Skills detail, perks, title | F2 personal file | Full screen (allowed) |
| Work Book | Paperwork (full screen allowed) | Suggested addition A2: a stamped trade record book |
| Licence exam paper | Paperwork | The pilot's written test: ten multiple-choice questions on one sheet |

---

## 6. Interactions

Every interaction is a scroll action on a real object. One action per option. A greyed action gives the reason
in words.

| Object | Actions |
|---|---|
| Foreman / desk clock | Take the job: Miner · Quit the job · Ask about the work (voiced dialogue: what the next level opens, where else to go) |
| Ore rock, tree, crop, fish spot | Existing gather actions. With an ability ready: "Second wind" on the held pickaxe, "Clean fell" on the held axe, "Patience" on the rod |
| Held tool | Inspect (condition in words: good, worn, nearly broken) · Use the ability when ready |
| Hardware bench | Repair the pickaxe ($12 + 1 scrap metal) · Sharpen the axe · Re-string the rod |
| Hardware till | One buy line per tier: "Steel pickaxe: $600 (Miner level 10)" |
| Furnace / saw / smoker | Existing Load, Work, Collect · Feed from the truck (when a truck with inputs is backed within 6 m) |
| Truck tailgate | Load the shoulder load · Unload the top load · Tip the bed (tipper only, at a tip or pile) · Check the load |
| Vehicle | Existing lock, store, search · "Load sheet" (read only, lists the bed and the boot in words) |
| Shift board | Read the board (the text is on the board; the action only zooms the camera on it) |
| Labour office counter | Change specialty: Mining (pick A or B) for $500 · Collect your mastery outfit · Ask about the trades |
| Export order board (dock) | One line per open order: "40 iron bars to the steamer, $1,000, by 18:00" · Take the order · Hand in at the crane |
| Licence desk | Book the truck check ($300) · Start the check (a lent truck appears on the pad) |
| Check course cones | Driven through, no action; the examiner calls each gate on the radio |
| Airport desk | Take the written test ($200) · Book the check flight ($1,300) |
| Truck yard / hangar | One "Buy" line per model and colour, e.g. "Buy Ural (blue): $9,500" |
| Other player | "Hire for the shift" (suggested A3) |

Paperwork that opens a full screen: the pilot's written test, the Work Book (A2), and the F2 file.

---

## 7. Balance

### Pay bands

The duty rule fixes the ceiling. Police patrol pays $900 an hour and medic orderly pays $792. Both must stay
1.5 to 2.5 times the best civilian loop. So **the best fully perked civilian loop is capped at about $600 an
hour**, and a new player's loop sits at $400 to $450. Pay rises by about 40 % from novice to master. The climb is
felt mostly in time saved, reach and risk choice, not in pay.

| Stage | Trade level | Vehicle | Typical loop | $/h now (`tools\cli economy`) | Target with perks |
|---|---|---|---|---|---|
| First hour | 1-5 | Starter car or on foot | Parcel, postal, apples, Coalman's Brow iron on foot | $400 to $480 | $400 to $480 (no perks yet) |
| Hour 2-10 | 5-12 | Car, then van | Fuel haul, iron by pickup, softwood by hand, the cove | $470 to $560 | $480 to $540 |
| Hour 10-30 | 12-20 | Van, then truck | Copper by car, stone by truck, hardwood by truck, export orders | $460 to $520 net | $520 to $580 |
| Hour 30-60 | 20-30 | Own truck, helicopter | Gold at Ore Ridge, the Régina bulk run, lighthouse supply | not modelled yet | $560 to $600 |

The flagged loop "stone on foot, $8/h" is intended. It tells the player the quarry needs a truck. It needs a
refusal in words at the quarry ("You'll want a truck for this face") instead of a silent bad deal.

### Perk caps (answers K3 on #4140)

| Perk kind | Cap at level 30 with both picks | Notes |
|---|---|---|
| Double-yield chance | 10 % | Rolled on the server per gather |
| Work speed | 20 % faster swing, cast or saw | Bounded by node stock and regrow, so it does not raise $/h beyond the cap |
| Buyer price | +8 % at that trade's premium buyers only | Never at the wholesale warehouse |
| Tool wear | -40 % | A sink reduction, not income |
| Carry | +1 shoulder load step (e.g. two ore sacks) | Mining and Salvage only |

`tools\cli economy` gets a "full perks" line per loop (card W11). The loop must stay at or under $600.

### XP curve

| Level | Total XP | Hours in that trade at about 700 XP an hour |
|---|---|---|
| 2 | 100 | 10 min |
| 5 | 1,000 | 1.5 h |
| 10 | 5,000 | 7 h |
| 15 | 11,000 | 16 h |
| 20 | 20,000 | 29 h |
| 25 | 30,000 | 43 h |
| 30 | 42,000 | 60 h |

The awards are tuned so an honest hour in any trade gives about 700 XP. Today: about 70 ore swings × 10, 6 to 10
runs × 100, 40 fish × 10. Smelting and Salvage rows are set to match. The daily cap rises from 2,000 to 3,000 per
track, which lets a long evening count. The first shift of the day doubles XP for the first 30 minutes in
each trade (session time), at most once per trade per in-game day.

### Risk against reward

| Loop | Risk | Reward |
|---|---|---|
| Coalman's Brow, parcels | None: in town, police near | Steady, low |
| Copper (Humboldt to Levie) | Cash spent on acid up front; a market that drops 3 % per bar | Highest early price |
| Export orders | A deadline; the stake is the town standing lost if late | +15 % over wholesale |
| Gold (Ore Ridge) | 10 km out, a slow truck, a cargo worth $1,500+, few police | The best $/h at master level, only with a crew |
| Helicopter supply | $15,000 machine, fuel, a crash wrecks it (tow fee at the hangar) | Speed: the same pay band, less travel |

Loaded trucks are targets. A hijacked load is a stolen good (the crime scope's fence and chop shop), the
police get a "cargo theft" call, and the owner's standing does not drop for being robbed.

### Cooldowns

- Abilities: 60 s active, 20 min cooldown, server clock, cleared on restart.
- Perk change: $500 and once per in-game week.
- Export orders: three open at a time server-wide, a new one every 30 minutes.
- Licence checks: a failed check waits 10 minutes before a retake.
- Existing: gather cooldowns, node regrow, buyer demand refill, run minimum time.

### Sinks

| Sink | Price | Rate |
|---|---|---|
| Tool tiers | $150 / $600 / $1,800 (pickaxe; axe and rod similar) | Once per tier |
| Tool repair | $12 + 1 scrap metal | About every 40 minutes of work |
| Van | $3,000 to $4,000 | Once |
| Truck | $8,000 to $12,000 | Once |
| Helicopter | $15,000 (UH-1H) / $24,000 (Mi-8) | Once |
| Truck licence | $300 | Once (again after a seizure) |
| Pilot licence | $200 test + $1,300 check flight | Once |
| Fuel | Pump price $2 a litre | Every haul |
| Perk change | $500 | Optional |
| Garage slots | Existing ($500 + $250 a step) | As the fleet grows |

A truck at $10,000 is about 20 hours of mid-game work. That makes it the hour-10 to hour-30 goal, which is
the point of the ladder.

### Capacity ladder (answers V1 and V3 on #4144)

| Carrier | Shoulder loads (cargo bed units) | Sacks in storage | Gate |
|---|---|---|---|
| On foot | 1 (2 with the Mining or Salvage carry perk) | Pockets: about 10 ore | - |
| Small pack (`Backpack_Kolobok`, `Backpack_Veshmeshok`) | - | +10 ore | - |
| Large pack (`Backpack_ALICE_Medium`, `Backpack_M70_Swiss`) | - | +20 ore | - |
| Car (T601, 2101, S105) | 0 | 4 sacks | Driving licence |
| Pickup (UAZ-469) | 2 | 6 sacks | Driving licence |
| Van (UAZ-452, S1203) | 6 | 8 sacks | Driving licence |
| Truck (Ural, M923) | 12 | 16 sacks | Truck licence |
| Helicopter | 0 | 4 sacks, passengers | Pilot licence |

A validator keeps the order (a car never out-carries a van). Each chain's best loop needs the next vehicle up
to stay in its band. The helicopter is fast but carries little, so it never replaces the truck.

---

## 8. Slices

Chapters run in this order, and each one ends playable and fun by itself. The mining and lumber site cards
already filed (#3206 and its sub-issues, the lumber plan) are not re-cut here. Chapter E lists only what they
lack. All cards are 30 to 45 minutes, file-disjoint, acceptance first (`docs/process/slice-card.md`).
Paths are abbreviated: `core/` = `addons/LifeFramework/`, `pack/` = `addons/LF_Everon1989/`, and `Level/` =
`core/Scripts/Game/Feature/Level/`.

### Chapters

| Chapter | Player result at the end | Epics |
|---|---|---|
| A. A trade of your own | Every job levels to 30 with a sound, a card and a named unlock at every level; the first shift of the day pays double XP | #4140 |
| B. Better tools | Tool tiers you can see, wear and repair, passive perks at set levels | #4140 |
| C. Bigger wheels | The capacity ladder, feed-from-truck, the truck check and the truck yard | #4144, #4141, #4143 |
| D. Masters of the trade | Perk picks, active abilities, titles, mastery outfits, shift boards and the Workers of the Month board | #4140 |
| E. Across the island | Hauling export orders, Salvage as a trade, the landfill, Ore Ridge gold, Hornbeam Valley, Pinewood Lake | #4140, #4144, #3206 |
| F. Above the island | The pilot licence, the helicopter dealer and the airport pad | #4142, #4143 |

### Cards

| Key | Title | Type | Tier | Model | Owns | Depends | Acceptance |
|---|---|---|---|---|---|---|---|
| W1 | 30-level tables for every trade, words for every tier | data | T1 | sonnet | `pack/Configs/Jobs/LF_Jobs_Everon1989.conf`, `core/Configs/Jobs/LF_Jobs.conf` | - | test LF_Test_EveronTradeTables |
| W2 | SMELTING and SALVAGE tracks with awards | system | T2 | sonnet | `Level/LF_JobLevelSources.c`, `core/Scripts/Game/Feature/Jobs/LF_EJobType*.c` | W1 | test LF_Test_TradeSources |
| W3 | The level-up moment: sting, HUD card, "Opens:" text | visual | T2 | sonnet | `core/Scripts/Game/Feature/Level/LF_LevelUpCard*.c`, `core/UI/Layouts/LevelUp/*` | W1 | test LF_Test_LevelUpCard; proof session level-up picture |
| W4 | Unlock rows: one named unlock or perk per level per trade | data | T1 | sonnet | `pack/Configs/Jobs/LF_TradeUnlocks_Everon1989.conf` | W1 | test LF_Test_EveronTradeUnlocks (every level 2-30 has a row) |
| W5 | First shift of the day: double XP for 30 minutes | system | T2 | sonnet | `Level/LF_FirstShift.c` | W1 | test LF_Test_FirstShift |
| W6 | F2 detail card: title, next unlock, weekly rank slot | visual | T2 | sonnet | `core/Scripts/Game/Feature/PlayerMenu/LF_PlayerMenuSkill*.c` | W4 | test LF_Test_PlayerMenuTrade |
| W7 | Tool tier rule: tier, condition, wear per use | system | T2 | sonnet | `core/Scripts/Game/Feature/Tools/LF_ToolCondition*.c` | - | test LF_Test_ToolCondition |
| W8 | Perk store as a save section (`LF_TradePerksSavePayload`) | system | T2 | opus | `Level/LF_TradePerks*.c` | W1 | test LF_Test_TradePerksRestart (PERSISTENCE); risk persistence |
| W9 | Hardware bench: repair, sharpen, re-string | capability | T2 | sonnet | `core/Scripts/Game/Feature/Tools/LF_ToolBench*.c`, `core/Prefabs/Props/LF_ToolBench.et` | W7 | test LF_Test_ToolBench; risk money |
| W10 | Tool tier meshes: pickaxe, axe, rod in three finishes | visual | T1 | sonnet | `tools/assets/tool-tiers/*`, `pack/Prefabs/Tools/Tiers/*` | W7 | proof look item for each tier |
| W11 | Passive perks: double yield, speed, buyer price, wear; `economy` full-perks line | system | T2 | sonnet | `Level/LF_PerkEffects.c`, `tools/economy/perks.mjs` | W4, W7 | test LF_Test_PerkEffects; economy flags none |
| W12 | Tool tiers on sale at the hardware and miners' supply tills | data | T1 | sonnet | `pack/Configs/Shop/LF_Catalogue_ToolTiers_Everon1989.conf` | W10 | test LF_Test_EveronToolTiers |
| W16 | Capacity table and its validator | system | T2 | sonnet | `core/Configs/Vehicles/LF_Capacity.conf`, `core/Scripts/Game/Feature/Vehicles/LF_Capacity*.c`, `tools/validation/validate-capacity-ladder.mjs` | - | test LF_Test_CapacityTable; selftest validate-capacity-ladder |
| W17 | Feed from the truck: a station takes inputs from a backed-up vehicle | capability | T2 | opus | `core/Scripts/Game/Feature/Processing/LF_FeedFromVehicle*.c` | W16 | test LF_Test_FeedFromVehicle; risk inventory |
| W18 | Truck licence and the check course | capability | T2 | sonnet | `core/Scripts/Game/Feature/License/LF_DrivingCheck*.c` | - | test LF_Test_TruckCheck |
| W19 | Truck yard at the north-east industrial compound | town | T1 | sonnet | `pack/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_truckyard.layer`, `tools/town/kits/truckyard.json` | W18 | proof look truckyard before and after; audit placement strict |
| W20 | Truck catalogue, prices and the lot move (trucks off the car lot) | data | T1 | sonnet | `pack/Configs/Vehicles/LF_TruckYard_Everon1989.conf` | W16, W18 | test LF_Test_EveronTruckYard |
| W21 | Truck salesman and driving examiner: outfits and voiced lines | visual | T1 | sonnet | `tools/assets/dialogue/truck_salesman/*`, `tools/assets/dialogue/examiner/*` | W19 | proof npc dress pictures; dialogue lines play |
| W8b | Perk pick at levels 10 and 20, and the change at the labour office | capability | T2 | sonnet | `Level/LF_PerkPick*.c` | W8, W11 | test LF_Test_PerkPick; risk money |
| W13 | Labour office counter and the Workers of the Month board | town | T1 | sonnet | `pack/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_labour.layer` | W6 | proof look labour before and after |
| W14 | Active abilities: one per trade, 60 s / 20 min, HUD chip | system | T2 | opus | `Level/LF_TradeAbility*.c`, `core/UI/Layouts/HUD/LF_AbilityChip*` | W11 | test LF_Test_TradeAbility; session proof two players |
| W15 | Titles on the ID card and in F2 | data | T1 | sonnet | `pack/Configs/Jobs/LF_TradeTitles_Everon1989.conf` | W4 | test LF_Test_EveronTitles |
| W15b | Mastery outfits at level 30, collected at the labour office | capability | T2 | sonnet | `core/Scripts/Game/Feature/Level/LF_MasteryOutfit*.c`, `pack/Configs/Jobs/LF_Mastery_Everon1989.conf` | W13, W15 | test LF_Test_MasteryOutfit |
| W22 | Shift boards: weekly top five per site | capability | T2 | sonnet | `core/Scripts/Game/Feature/Level/LF_ShiftBoard*.c` | W6 | test LF_Test_ShiftBoard; proof look a board |
| W23 | Shift whistle and site ambience | visual | T1 | sonnet | `tools/assets/site-ambience/*`, `pack/Sounds/Work/*` | - | proof session sound log line at 06:00 |
| W24 | Export order board at the dock warehouse | capability | T2 | sonnet | `core/Scripts/Game/Feature/Jobs/LF_ExportOrder*.c` | W16 | test LF_Test_ExportOrder; risk money |
| W25 | Landfill tip and wreck salvage as Salvage work | capability | T2 | sonnet | `core/Scripts/Game/Feature/Salvage/LF_WreckSalvage*.c` | W2 | test LF_Test_WreckSalvage |
| W26 | Landfill place: tip, depot, wrecks | town | T1 | sonnet | `pack/Worlds/MainWorld/MainWorld_Layers/Place_landfill.layer` | W25 | proof look landfill; audit strict |
| W27 | Ore Ridge gold workings and the assay table | town | T1 | sonnet | `pack/Worlds/MainWorld/MainWorld_Layers/Place_ore-ridge.layer`, `pack/Configs/Resources/LF_Gold_Everon1989.conf` | #3232, W4 | proof look ore-ridge; test LF_Test_EveronGold |
| W28 | Hornbeam Valley hardwood and the Régina bulk buyer | town | T1 | sonnet | `pack/Worlds/MainWorld/MainWorld_Layers/Place_hornbeam-valley.layer`, `Place_regina-sawmill.layer` | W4 | proof look both; audit strict |
| W29 | Pinewood Lake spot, pike and eel rows, the cannery buyer | data | T1 | sonnet | `pack/Configs/Fishing/LF_PinewoodLake_Everon1989.conf`, `Place_pinewood-lake.layer` | W4 | test LF_Test_EveronPinewood |
| W30 | North-east farm and the Étoupe orchard | town | T1 | sonnet | `Place_ne-farm.layer`, `Place_etoupe-orchard.layer` | W4 | proof look both; audit strict |
| W31 | Pilot licence: written test sheet and check flight | capability | T2 | sonnet | `core/Scripts/Game/Feature/License/LF_FlightCheck*.c` | W18 | test LF_Test_FlightCheck |
| W32 | Airport place: flight school desk, hangar, pad, windsock | town | T1 | sonnet | `pack/Worlds/MainWorld/MainWorld_Layers/Place_airport.layer` | W31 | proof look airport; audit strict |
| W33 | Helicopter catalogue, hangar pad storage, no-land rule in towns | capability | T2 | sonnet | `core/Scripts/Game/Feature/Vehicles/LF_HangarPad*.c`, `pack/Configs/Vehicles/LF_Hangar_Everon1989.conf` | W31, W32 | test LF_Test_HangarPad; restart check |
| W34 | Flight instructor and helicopter dealer: outfits and voiced lines | visual | T1 | sonnet | `tools/assets/dialogue/flight_instructor/*`, `tools/assets/dialogue/heli_dealer/*` | W32 | proof npc dress pictures |
| W35 | Manual and F3 Trades page | docs | T0 | haiku | `docs/player-manual/03-jobs.md`, `core/Configs/Guide/LF_GuideTrades*.conf` | W4, W20 | doc review; guide page opens |
| W36 | Economy tuning pass with full perks and the capacity ladder | data | T1 | sonnet | `pack/Configs/Market/LF_Market_Everon1989.conf` (rows), `pack/Configs/Jobs/LF_Jobs_Everon1989.conf` (awards) | W11, W16, W24 | `tools\cli economy` shows no flag except the intended on-foot stone loop |

Card order by chapter: A = W1, W2, W3, W4, W5, W6. B = W7, W8, W9, W10, W11, W12. C = W16, W17, W18, W19, W20,
W21. D = W8b, W13, W14, W15, W15b, W22, W23. E = W24, W25, W26, W27, W28, W29, W30. F = W31, W32, W33, W34. Then
W35 and W36 close the scope. Each chapter adds the town-placement card that the pipeline requires
(`slice-card.md`), and that card is not listed here.

**Ready at once (nothing before them):** W1, W7, W16, W18, W23. They own disjoint files.

---

## 9. Risks and open questions

### Risks

- **Visible levels.** Decision 7 says "no visible levels", but F2 already shows a level chip and #4140 K1
  proposes showing levels. This scope assumes K1: levels in F2, on the level-up card and on the boards, while
  gates still refuse in words. If the maintainer keeps decision 7, cards W3, W6 and W22 show tier words
  (Apprentice, Hand, Journeyman, Foreman, Master) in place of numbers, and nothing else changes.
- **The pay ceiling is low.** With police patrol at $900 an hour, a master earns at most about 40 % more than
  a novice. If the climb feels flat in playtests, the lever is the duty rate, not civilian pay. See Q2.
- **Ability exploits.** Abilities speed up work but node stock limits the yield. W14 must be tested against a
  rock with stock, not an infinite node.
- **Perks as save data.** Perk picks are a new save section. They must load from a save that predates them
  (`OnAbsent` gives no picks) and survive `wt e2e`.
- **Feed from the truck touches inventory.** Moving items from a vehicle into a machine with all-or-nothing
  rollback is the riskiest card (W17, Opus, risk inventory).
- **Helicopters bypass the world.** A helicopter makes every distance short. The small cargo, the no-landing
  rule in towns, fuel and the price keep it a status and travel item. If players use it to rob and flee, M7's
  law book needs a "flying without a licence" charge and police access to a helicopter.
- **AI examiner in the passenger seat.** Seating an AI in a player-driven vehicle may be unreliable on a
  dedicated server. The fallback is the examiner's voice on the vehicle radio, with gates checked by trigger.
- **Site load.** Six new places add entities on the alpha box. Each place follows the per-place light and
  entity budget (`performance.md`).
- **Too many trades for a small server.** Nine trades on a 16-slot server spread players thin. The shared
  reagents and haul orders are there to pull them back together.

### Open questions for the maintainer

Each has a recommendation. "Accept all" takes every recommendation.

- **Q1. Visible levels.** Show levels 1 to 30 in F2, on the level-up card and on the boards (with gates in
  words)? **Recommend yes** (same as #4140 K1).
- **Q2. Pay ceiling.** Keep the best civilian loop at about $600 an hour, or raise police patrol to $1,080 so a
  master can reach $720? **Recommend keeping $600 for M6** and reviewing after the first month of the ledger.
- **Q3. Separate Smelting and Salvage tracks.** Split SMELTING from MINER and add SALVAGE? **Recommend yes**:
  it makes the coal and ore trade between players, and it gives the scrapyard a climb.
- **Q4. Perk respec.** Allow a perk change at the labour office for $500 once a week? **Recommend yes**.
- **Q5. Mastery outfits.** Grant the outfit at level 30, with one item that is never sold? **Recommend yes**.
- **Q6. Truck yard place.** The north-east industrial compound (0.6 km, near the town) or Montignac (3.6 km,
  the work town)? **Recommend the north-east compound** so the truck is reachable before Montignac is live.
  It keeps the mining sites out of that compound, as mining-chains asked.
- **Q7. Helicopter models.** The UH-1H only for M6, with the Mi-8 later? **Recommend the UH-1H only**: one
  livery to prove, one price to tune.
- **Q8. Gold.** Ore Ridge gold at Mining 25 as the master loop, with no police exclusion? **Recommend yes**.
  Its risk comes from the distance and the cargo value, not from a lawless zone (no lawless zones until M7).
- **Q9. Leaderboard reset.** Weekly shift boards and a monthly Workers of the Month board? **Recommend yes**,
  reset by the server clock and saved.

---

## Suggested additions (proposals for the maintainer)

These go beyond the wishlist. Each one is a proposal, not a plan, until the maintainer approves it.

| # | What | Why it helps the whole | Size | Touches |
|---|---|---|---|---|
| A1 | **Power plant coal buyer.** The power plant at 4355, 10098 buys coal by the truck load, with a demand cap that refills over time. | It gives coal a second buyer near town, so a level-4 miner has a full loop before the smelter opens. It also uses a real landmark that sits empty today. | S | Economy, mining |
| A2 | **The Work Book.** A paper record book every worker carries: each trade's page carries the foremen's stamps at levels 5, 10, 20 and 30, and the employer's name. It opens as paperwork. | It is a 1989 object that shows progression without a pop-up. Police can ask to see it, so it also works as roleplay ("Where do you work?"). | M | Paperwork UI, police, F2 |
| A3 | **Hire for the shift.** Scroll on another player, "Hire for the shift: 50/50 split". Both get XP, and the yield is split at the buyer in cash. | Gold, the crosscut saw and truck runs become social by design. New players get a fast path into a master's work. | M | Social, gangs (the same scroll-to-recruit as gang recruiting) |
| A4 | **Lighthouse supply run.** Five real lighthouses each want a crate of supplies once a game day: by truck to the near ones, by helicopter to Erquy and the southern ones. | It gives the helicopter a legal job and sends players to the island's corners. | S | Hauling, helicopters, places |
| A5 | **The bakery link.** Flour (Farming), firewood (Lumber) and the oven make bread that the café and the supermarket buy. | It closes the farm chain, which has "no mill or bakery flour" today, and links two trades. | M | Farming, lumber, market, shops |
| A6 | **Company yards.** A player who owns a truck and a garage slot can register a small company name at the labour office. The name goes on the livery and the shift boards. | It is status without a gang. It is also a legal counterpart to gang hideouts and something to spend money on at hour 30. | M | Organizations, liveries, boards |
| A7 | **Breakdowns on the road.** A worn truck can blow a tyre or overheat on a long haul. The repair kit fixes it at the roadside, or a tow truck player comes (a mechanic job). | It adds tension to the long hauls and creates a mechanic or tow job that other players provide. | M | Vehicles, repair kit, service calls |
| A8 | **Weather on the work.** Rain slows the reaper and doubles bites at dusk; a storm closes the quarry face. | It makes days on the island differ without new content. | S | Atmosphere, farming, fishing, mining |
| A9 | **Strikes and quotas (story hook).** The state factory arc (M6 chapter 8) can call a "quota week": one trade pays +10 % for a week. | It gives the server a shared goal and moves players between trades. | S | Events, story |
| A10 | **Apprentice bonus.** A master working next to someone under level 10 gets a small XP bonus, and so does the apprentice. | It rewards teaching new players, and onboarding is the maintainer's top focus. | S | XP, onboarding |
