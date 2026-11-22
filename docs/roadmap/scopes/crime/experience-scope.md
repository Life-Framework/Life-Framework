# Crime: the experience scope

Milestone: M7 Power and turf (Dirty money #1025, The big heist #1024, Story: The shipment #1036), with a
pre-M7 fix list
Extends: illicit chains and hands-on stations (#416, #417), cook mistakes (#2855), smoke and smell (#418),
marked cash, the rotating buyer and laundering (#1025 wave), the informant tip (#2921), safe types (#2288),
heist stages and runs (#3409, #3411), forced entry, lockpicking, the chop shop, standing (`UNDERWORLD`),
hidden crops (#809), the crime record and wanted level
Epics it plans for: #4152 drug chains, #4153 reserve heist, #4154 hospital robbery; it leans on #4145
outposts, #4146 gang hideouts and #4148 gang radio

This is the whole crime area as one plan for players: what it feels like, where it happens, who you meet,
what it is built from and in what order. The single-loop cards in this folder ([drug chains](drug-chains.md),
[bank robbery](bank-robbery.md), [chop shop](chop-shop.md) and the rest) stay as the system contracts. This
file says how they fit together. It is a proposal for the maintainer; nothing here is an issue yet.

**One rule runs through it.** Crime on Everon is small, local and physical. You learn about it from people,
you do it with your hands at a real place, and you carry the result. The money it pays is marked and has to
be washed. A risk you can see (smoke, a patrol, a witness) is better than a hidden timer.

## 1. Player fantasy and core loop

**Hour 1: the small job.** You have a car, $100 and a mining run you did once. On the café terrace the
night-shift worker mutters that someone is buying, quiet like. You pick plums at the orchard, but instead of
the juicer you drive them to the still barn in the hills. You mash at a barrel, tend the still while it
hisses, and keep looking down the track for headlights. One bottle comes out, and the batch nearly blows
because you hurried the third tick. You sell it behind the dock warehouse for $80 in salmon-tinted notes that
no shop will take. You have learned three things: crime happens at places, it pays in dirty money, and the
police can smell it.

**Hour 10: a crew and a name.** You and two friends run moonshine and a cigarette run from the container yard
to the bars in Chotain and Levie. Vince now sells you a drill because your record says you are serious. You
did the cash van twice and lost once to a spike strip on the Montignac road. The fence knows you; the
informant charges you less; the buyer at the cove has started to offer hashish if you can be at Goat Bay at
two in the morning. You have a cannabis plot in a clearing above Levie that you visit every evening, and you
worry that the farmer's dog will bring the police. Money goes out as fast as it comes in: the wash takes a
third, the drill bits wear, a seized crate was $500 gone. The hook is that every run is a little different
(a new buyer spot, a different patrol, a ruined batch) and that your crew is getting better at it together.

**Hour 50: the organisation.** Your gang holds a hideout with a pill press and a drying loft. You run three
chains on a weekly rhythm and trade inputs with other players: the hospital crew brings you the pharmacy
stock you press into pills, a fisher sells you the boat schedule he heard on the quay, and a car dealer
launders through his cash till. The bank job is routine; the reserve depot is the thing people talk about.
You spent a week buying the guard rota, watching the substation and timing patrols, then ran it with six
people against five officers. Half of you went to jail, the take was $18,000 marked, and the town still
argues about who talked to the police. The hook is status and story: rare jobs, rivals, betrayal, and an
island where the police know your face.

**Why it keeps players.**

- **Progression you feel, never a number.** Your record opens Vince's shelf; underworld standing opens the
  informant, the cove contact and the outpost's better stock; each chain tier needs equipment you saved for.
  Gates speak in words ("Vince doesn't sell that to strangers").
- **Variety from the world, not from content copies.** Each chain uses a different place, verb, vehicle and
  hour of day: an orchard and a still by day, a field plot at dusk, a cove at night, a clinic in a hurry, a
  depot over a week. Buyers move, patrols vary, batches fail.
- **Social play.** Most chains run solo but slowly; a crew is faster at the stations, safer on the road and
  needed for the top robberies. Inputs cross between chains and players. Police are players too, so every
  robbery is a chase someone else is living.
- **Short and long goals at once.** A bottle in 10 minutes, a plot that ripens in an evening, a landing that
  is announced a day before, a reserve heist that takes a week to prepare.

## 2. Systems: how crime fits the other areas

| Shared system | What crime reuses | What crime needs that is new or not finished |
|---|---|---|
| XP and skills (job levels #749, the professions plan, wishlist item 17) | The chain gate reads a minimum level in a job (`m_sLevelJob`) | A `CRIMINAL` skill line is **not** proposed: crime progresses through record, standing and equipment, never a visible level (M6 decision 7). Legal skills feed crime: a farmer grows a better plot, a fisher reads the coves, a mechanic chops faster (perk rows in the professions plan) |
| Standing (`LF_Reputation`, `UNDERWORLD`) | Gates on the informant (#2921) and contracts; never shown | Awards per chain sale, landing and heist; a loss for an identified informant; outpost "known here" (#4145 O1) |
| Wanted level and crime record | +1 to +4 per offence; the record gates Vince | A chain-goods search adds by amount class (below); the record stays the gate for tools |
| The legal system (the law book and charge sheet, police scope) | Wanted and booking, bail | Narcotics classes A, B, C by amount (personal, dealing, trafficking) read from `m_bGoods` items; marked cash on a suspect is evidence, not a crime by itself |
| Investigation (witnesses, evidence, payphones) | Smell leads, witness calls | Informant tips land here as leads; a tip names an area, never a player |
| Gangs (#4146 hideouts, #4148 radio, recruiting, flags) | Organisations and hideouts exist | The hideout **processing perk** hosts the tier-3 stations (pill press, cutting table); gang radio coordinates landings and heists |
| Rebel outposts (#4145) | none | The outposts buy contraband crates in bulk and sell the reserve's inside info and the weapons; they are the safe end of a smuggling run |
| Vehicles and cargo (wishlist item 5) | Car cargo, keys, lockpick, hotwire, plates | Cargo volume decides chain scale: a hatchback carries a crate, a UAZ452 van four, a truck a pallet. Contraband in a car is found by a search at a stop |
| Economy and market | Marked notes, the ledger, the buyer price list, laundering | Demand caps per buyer that refill (the "buyer with demand" model of the resource chains), so flooding one buyer drops the price |
| Map and run chains (#2223, `LF_MapShares`) | Private marks (the informant's mark) | Crime places are **never** on the public map. You learn them as private "heard of" marks from people; the alarm badge is public |
| HUD | The heist clock line, hands-up prompt | Nothing new beyond a heist clock and a single "You are carrying something that smells" hint |
| F2 player menu | Skills, quests | Crime gets no F2 tab. A starter crime lead may appear in "What to do next" only after the player has a record, worded as a rumour |
| Police and medic duty | Dispatch, roadblocks, spike strips, armory | Presence rules per robbery rung; the clinic shortage after a pharmacy robbery; a recapture or seizure bonus on duty pay stays inside the 1.5 to 2.5x band |
| Persistence | Station batches, laundering, cooldowns | Plots, the landing calendar, heist prep progress and the server-wide big-job lock must survive a restart |

## 3. Places

Positions and fits come from the scout dossiers and `tools\cli town` data, not this table. Rows marked
**exists** are fitted today (`docs/everon/places/`). Everything else is a candidate for the town pipeline.

| Place | What it looks like | What's there | Chains and jobs |
|---|---|---|---|
| **Saint-Philippe fence yard** (exists, `saint-philippe` b33) | A workshop yard behind a corrugated gate, a lamp over the bench, a dog chain | Vince, his tool crate, the chop bench, the laundering station (#3417), a chalk board of what he buys this week | Tools, stolen parts, the entry point |
| **Saint-Philippe dock warehouse and lanes** (exists, buyer spots) | Wet concrete, stacked pallets, a single sodium lamp | Rotating buyer spots | First sale of every chain |
| **The still barn** (exists, `stillbarn`, about 1 km out) | A lone barn, hay, three barrels, a copper still, two lamps | Mash, distill, pack; laundering bench | Moonshine (tier 1) |
| **The barn compound** (exists, chop yard) | A farm yard with a lean-to, oil stains, a car on blocks | The back-country chop bench | Chop shop |
| **The farm compound and hilltop mast** (exist, contested) | A walled farm and a radio mast on a hill | Hideout sites | Gang processing perks |
| **Saint-Philippe container yard** (candidate, part of the dock state 2a) | Rows of rusting 20 ft containers, one with a fresh padlock | A "short-weighted" container that refills twice a day with cartons of cigarettes and crates of vodka; a crooked tally clerk | Cigarettes and vodka (tier 2) |
| **Village bars: Chotain and Levie** (candidates, village shop or bar slot) | A low bar with a cassette radio and a back door | The barkeeper buys cigarettes and vodka under the counter, up to a weekly demand | Tier 2 outlets |
| **Clearings above Levie and near Briars** (candidates, hidden field plots) | A tight clearing in the pines, a rain barrel, a broken fence, a hunter's stand nearby | Two to four plantable plot spots per clearing; a drying loft in a ruined barn near Briars | Cannabis (tier 2) |
| **Goat Bay and Fisherman's Bay** (candidates, already fishing spots in M6 state 2b) | A pebble cove, a boat shed, nets, a wreck on the rocks | The night landing point; a signal-lamp post on the rocks | Hashish and heroin landings (tier 3) |
| **Andre's Beacon** (candidate, the lighthouse) | The lighthouse on the coast loop | Where the signal is answered; the cove contact waits here | The landing call |
| **Lamentin north harbour** (candidate) | A dark working harbour, a few lights | The boatman's shed; the alternate landing | Landings in the north |
| **The cutting barn** (candidate, a gang hideout with the processing perk) | A barn with a plastic-sheeted table, a shop scale, a camp lantern | Cutting table and bagging bench | Hashish and heroin processing |
| **Saint-Philippe hospital and dispensary** (the hospital town) | The clinic's back corridor, a glass-fronted drug cabinet | The pharmacy cabinet, the night pharmacist | Pharmacy stock (rung 4), speed inputs |
| **The pill room** (gang hideout perk) | A back room with a hand-cranked press bolted to a bench | The pill press | Amphetamine (tier 3) |
| **The cash van route** (#3398) | The bank, the fuel station and the general store; Montignac on payday | The van's stops | Rung 3 |
| **The Everon bank** (exists, building in tree) | Teller floor, staff door, vault room | Vault, money bags | Rung 5 |
| **The reserve depot** (candidate, #2283 building, walled site outside a town) | A concrete compound with a guard hut, floodlights, a loading bay, a 110 kV transformer station down the road | Outer door, inner vault, armoured cash boxes, the guard hut's rota book | Rung 6 |
| **The substation** (candidate, the vanilla 110 kV transformer station on the line feeding the depot) | Fenced transformers humming | The breaker cabinet | The power-cut stage |
| **Rebel outposts north and south** (#4145) | Tents, a mast, a fire, a quartermaster's table | Bulk contraband buyer, weapons, the reserve's rota papers | The safe end of a run |

**Distances matter.** The still barn and the fence are near Saint-Philippe so a new criminal can start with a
car. Plots sit 2 to 4 km out, coves 5 km or more and on bad roads, and the depot is far from the main police
station. The further and higher the tier, the longer the drive with goods in the car.

## 4. People

All lines are original, US English, voiced through the ElevenLabs pipeline (French accent allowed). Clothing
is vanilla prefabs through the NPC preset system (`Prefabs/Characters/Presets`).

| NPC | Role and behaviour | Clothing (vanilla) | Sample lines |
|---|---|---|---|
| **Vince** (exists) | The fence. Sells tools by record, buys parts, keeps a weekly chalk list. Leans on the bench, smokes | `Jacket_Denim_01_strippedShirt_dirty`, `Pants_M70_Dirty`, `Hat_FlatCap_01_darkgrey_Dirty` | "I don't sell drills to tourists." / "This week I want radios and tires. Nothing with a plate." |
| **The night-shift worker** (exists, the informant) | Sells the buyer's spot; now also hears about landings | Work jacket, flat cap | "The man's moved. Harbor end, tonight." |
| **The buyer** (exists, rotating) | Buys chain goods at the open spot; at a hidden chance per sale he is the police's informant | Denim work jacket (pack dress, B4) | "Put it in the bag. Don't count it here." |
| **Marcel, the tally clerk** (new) | At the container yard. For a cut he leaves one container short on the books; he tells you when it is full | `Jacket_M70` without insignia over a grey shirt, `Hat_TruckerCap_01` | "Container nine is short this week. Funny, that." / "Not while the customs man is here." |
| **Josette, the barkeeper** (new, one per village bar) | Buys cigarettes and vodka under the counter up to a weekly demand; gives a rumour with each sale | Apron over a civilian shirt (vanilla civilian preset) | "Two cartons. More than that and my husband notices." |
| **Old Fabien** (new) | A farmer whose land borders the plot clearings. His dog barks at strangers; he calls the gendarmes if he sees you twice | `Pants_Fisherman_01_green`, denim jacket, `Hat_Boonie_US_olive` | "That's not mushrooms you're picking." |
| **Lucien, the boatman** (new) | At Lamentin or Meaux. Sells the night and cove of the next landing at a high underworld tier; flashes the lamp back from the sea | `Pants_Fisherman_01_yellow`, oilskin-look jacket, flat cap | "Thursday, two o'clock, the goat cove. Three flashes, then wait." |
| **The night pharmacist** (new) | At the dispensary. Can be bribed for a small, slow, quiet supply of diverted stock instead of a robbery; refuses if the clinic was robbed this week | White coat (the medic preset without the cross) | "I count the boxes on Mondays. Be gone by Sunday." |
| **The depot watchman** (new) | Unarmed, in the guard hut. Rings the alarm if he sees a weapon; hands up under threat; keeps the rota book | `Jacket_KZS`, `Hat_PeakedCap_USSR_01` (no badges, pack re-texture as "DEPOT SECURITY") | "Shift change at six. Don't make me ring it." |
| **The quartermaster** (#4145) | At the outposts. Buys contraband crates, sells weapons and the rota papers by standing | Partisan-style mixed kit (FIA preset) | "Bring me crates, I'll open the rack." |

**Behaviour rules.** NPCs loiter, smoke and sit through `LF_NpcLoiter`. None of them fights; a threat makes
them raise hands (`LF_HandsUp`) and, after a robbery, they talk to the police as witnesses. Each one says no
in words, so a player always knows why a door is shut.

## 5. Assets

Source order: vanilla, then CC0 or CC-BY, then our Blender build scripts. Meshy only for the planned masks.

### 3D props, items and tools

| Asset | Source | Prefab family or plan | Why |
|---|---|---|---|
| Barrels, still, packing bench | Exists | `LF_ChainBarrel.et`, `LF_ChainStill.et` | Built |
| Shop scale on the cutting table | Vanilla | `Prefabs/Props/Commercial/ScaleShop_01/ScaleShop_01_*` | Reads as "weighing" with no new mesh |
| Cutting and bagging bench | Vanilla | `Prefabs/Props/Industrial/Workbench_01.et`, `Workbench_Vice_01.et` | Same component as the barrel |
| Pill press | **Blender build** | A hand-cranked cast-iron tablet press, one moving lever | No vanilla equivalent; one simple part with a lever animation in our anim stage |
| Cannabis plant, 3 growth stages | **Blender build** (foliage cards) | Rendered as a crop row through the hidden-crop system (#809) | Vanilla has no such plant; cards are cheap and fit the vegetation shaders. Policy check first (#4152 D2) |
| Drying rack with hanging bunches | Blender build on vanilla | Vanilla hay and a wooden frame; bunches are the stage-3 card mesh | Visible progress on the rack |
| Bale or sack of produce | Vanilla | `Prefabs/Props/Civilian/SackUniversal_01.et` as the carried item base | Carried over the shoulder through the shared carry system |
| Hashish brick, heroin bundle | **Blender build** | Small taped blocks with a US English stamp-free wrap | Tiny, period-plausible, one material each |
| Cigarette carton and sleeve | **Blender build** | A box with a US English brand-free "CIGARETTES" carton print | Vanilla only has litter packs |
| Vodka bottle and crate | Vanilla first | `Prefabs/Compositions/.../Crate_Bottles_01.et`, a bottle variant of `BeerBottle_01_*` with a "VODKA" label | Label texture only |
| Pill bottle | Vanilla | `Prefabs/Items/Medicine/AspirinBottle.et` (relabel "TABLETS") | Exists |
| Morphine, field dressing | Vanilla | `MorphineInjection_01`, `FieldDressing_US_01` | The hospital cabinet already uses them |
| Contraband crate, money bag, marked cash | Exists | `LF_ContrabandCrate`, `LF_MoneyBag.et`, `LF_MarkedCash.et` | Built |
| Armoured cash box (reserve) | **Blender build** | Steel box with two handles, a two-hand carry | No vanilla box reads as a cash box; heavy carry slows the run |
| Signal lamp | Vanilla | `Prefabs/Props/Industrial/Mine/MiningLantern_01/MiningLantern_01.et` | Held and flashed at the cove |
| Offshore flare | Vanilla | `Prefabs/Weapons/Ammo/FlareEffect_30mm_RSP30_White.et` (effect only, far out at sea) | Tells the cove the boat is there |
| Boat shed, wreck, nets, buoys | Vanilla | `BoatShed_01`, `BoatWreck_01`, `FishingNet_0*`, `FishingBuoy_01` | Cove dressing |
| Substation and breaker cabinet | Vanilla | `TransformerStation110kV_E_01` family; a vanilla electrical cabinet for the breaker | Exists on the terrain along the line |
| Floodlights at the depot | Vanilla | `GeneratorFloodlight_USSR_01` | Go dark in the power-cut stage |
| Containers | Vanilla | `CargoContainer_01` family | Container yard |
| Heist mask | **Meshy** (already budgeted) | The 1 to 2 period masks of the Meshy plan | Sold by Vince at record 5; hides identity from witnesses |
| Binoculars | Vanilla | `Binoculars_B8.et` | Casing the depot, watching a cove |

### Vehicles and liveries

| Vehicle | Source | Use |
|---|---|---|
| UAZ452 cargo, civilian colours | Vanilla `UAZ452_cargo_CIV_*` | The smuggler's van: four crates |
| A light car (the T601 family) | Vanilla | Tier 1 runs, one crate |
| Ural cargo | Vanilla `Ural4320` | The reserve's cash-box truck and the getaway truck |
| Cash van | Exists (cash van run) | Rung 3 |
| A "DEPOT SECURITY" livery on a UAZ | The livery pipeline (wishlist item 14) | Parked at the depot; a prop that says what the place is |

No boats: the base game has none. A landing is a sound and a flare at sea, then crates found on the shore.

### Sounds (ElevenLabs SFX unless named)

| Sound | Where |
|---|---|
| Pill press clunk and lever squeal | Pill room |
| Plastic rustle and tape tear | Cutting and bagging |
| Small diesel boat engine idling offshore, fading in and out | Cove, landing night only |
| Waves on pebbles, gulls at dawn, crickets at night (ambience loops) | Coves, clearings, barns |
| A farm dog barking, getting closer | Plot clearings when Fabien's dog notices |
| Transformer hum and its power-down | Substation and depot |
| Old electric alarm bell | Depot and clinic (vanilla first, ElevenLabs if none fits) |
| Drill whine on steel | Exists or vanilla |
| Air wrench and panel clank | Chop bench |
| Still bubbling, a pop before a ruin | Still barn (cue for a mistake) |

### UI elements (in-world first)

| Element | Rung | Where |
|---|---|---|
| Vince's chalk board: what he buys this week | 1 (physical) | Fence yard wall |
| The bar's back-door price: the barkeeper says it | 1 | Dialogue |
| A folded note item ("Thursday, 2 AM, goat cove, three flashes") | 1, readable as paperwork | Bought from Lucien; inspect opens the paper card |
| The rota papers (reserve) | Paperwork screen (allowed) | Bought at an outpost; required to start the depot |
| Plant growth by mesh stage, a dry rack that fills | 1 | Plot, loft |
| Station prompts in words ("The mash smells sharp"), smoke colour | 1 | Every station |
| The notice on the clinic door during a shortage | 1 (sign system) | Clinic |
| Heist clock | 4 (HUD line, exists) | During a heist |
| Private "heard of" marks for crime places | Map (allowed) | `LF_MapShares` PLAYER rows |

## 6. Interactions

Every action is a scroll entry on the object, one per option. No menus except paperwork and the map.

| Object | Actions |
|---|---|
| Plot spot (clearing) | Plant seeds · Water (rain barrel can) · Check the plants (a word for the stage) · Harvest · Burn the plot (police) |
| Drying rack | Hang the harvest · Take down the dried bunches |
| Press or bagging bench | Load · Work (hold) · Collect · Bag into 10 or 50 |
| Short container (yard) | Pay Marcel (dialogue) · Open the container (needs the key from him) · Take a carton · Take a crate |
| Barkeeper | Sell two cartons · Sell a crate of vodka · "Anything I should know?" (rumour) |
| Signal lamp | Flash the signal (three holds) |
| Landed crate on the shore | Open · Take a bundle · Carry the crate |
| Cutting table | Load bundle · Cut (hold) · Weigh · Bag |
| Pharmacy cabinet | Pick the lock · Place a charge · Fill a bag with ampoules · Fill a bag with tablets |
| Night pharmacist | "Got anything going spare?" (bribe, dialogue) |
| Pill press | Load stock and filler · Turn the press (hold) · Collect tablets · Bottle |
| Buyer, quartermaster | Sell the goods (one action per good type) · Ask what is wanted |
| Depot guard hut | Threaten the watchman · Take the rota book · Ring the alarm (watchman, NPC only) |
| Breaker cabinet (substation) | Open with a lockpick · Pull the breakers (hold) · Reset (police or depot NPC) |
| Depot outer door, inner vault | Place a charge · Drill (hold, with the drill) · Open |
| Armoured cash box | Lift (two-hand carry) · Load into a vehicle · Crack open (drill, at a hideout) |
| Any car with contraband | Search the trunk (police) · Seize goods (police) |
| Payphone | "Call the tip line" (anonymous, for police lead money, suggested addition) |

## 7. Balance

Numbers from `tools\cli economy` on 2026-11-22: civilian runs and chains pay **$400 to $600 an hour**,
police duty **$900 to $1,260**, medic **$792 to $1,224**. The pack's laundering cut is **35 %**, batch
1,500, daily cap 6,000.

**The rule: crime pays about the same as duty on average, with far more swing.** A criminal's expected
clean money per hour (after the wash, inputs, tool wear, seizures and failed runs) sits in the same 1.5 to
2.5x band as police duty. A good night pays 4x; a bad one costs a car and a drill. If crime paid more on
average, nobody would join the police; if it paid less, nobody would risk it.

| Tier | Activity | Gross marked $/h per head (good run) | Clean after wash and inputs | Risk | Cooldown or cap |
|---|---|---|---|---|---|
| 1 | Moonshine | 900 to 1,100 | 500 to 650 | Smoke, a 6 % mistake per tick, a search at a stop | Buyer demand cap per period |
| 1 | Chop shop (pays clean) | 600 to 900 | 600 to 900 | +1 wanted, stolen-plate reports | 5 min bench, $3,000 per car cap |
| 2 | Cigarettes and vodka run | 1,200 to 1,500 | 700 to 900 | Search at a stop, a bar's weekly cap | Container refills twice a day |
| 2 | Cannabis | 1,400 to 1,800 (one harvest per evening) | 800 to 1,100 | The dog, the farmer's call, a burnt plot | One crop at a time per plot, grows 60 to 90 min |
| 3 | Hashish landing and cut | 2,500 to 3,500 per head for a crew of 3 | 1,400 to 2,000 | Night drive, a tip to police, the landing can be raided | One landing a day server-wide, announced 12 to 24 h before |
| 3 | Speed pills | 2,000 to 2,800 | 1,200 to 1,600 | Inputs need a pharmacy robbery or a slow bribe | Press throughput, input supply |
| Rung 1 to 6 | Robberies | the ladder table in #4153 | Expected about 40 to 60 % success | Police presence rules, the server-wide big-job lock | 20 min to 6 h |

**XP and standing.** Crime grants no job XP. Underworld standing comes from sales, landings and heists, each
capped per day like the other factions (60 a day). Tiers in words open the informant (tier 1), the cove
contact (tier 2), the rota papers (tier 3).

**Sinks.** The wash (35 %), consumed tools (the drill wears over 5 vaults, a charge is used once), the
informant and Lucien's fees, seeds and fillers, bribes, seized goods and marked cash on arrest, fines and
bail, impounded vehicles, hideout upkeep. Seizure is the largest sink by design: police work removes money
from the island.

**Anti-farm.** Buyer demand caps; one landing a day; robberies need real police online (presence rule); the
server-wide big-job lock across rungs 4 to 6 and the prison break; marked notes cannot be handed to a friend
as clean cash; a buyer pays less for goods sold by the same character within a period.

**Narcotics classes (for the charge sheet).** Proposed per good: class A up to 2 units (personal), B up to 10
(dealing), C above 10 (trafficking). The police scope sets the fines and sentences; crime only tags goods
and counts them.

## 8. Slices

Chapters in an order where each one ends playable and fun. All cards are 30 to 45 min, file-disjoint, with
the acceptance written first (`docs/process/slice-card.md`). Card keys are local to this plan; existing
issues are named where a card only finishes them. Pack paths are `addons/LF_Everon1989/`, core paths
`addons/LifeFramework/`, both shortened to `pack/` and `core/` here.

**Chapters**

| # | Chapter | Lands in | Playable result |
|---|---|---|---|
| K0 | The ladder works on the island | Pre-M7 fix list | Till, store safe, cash van and bank all run on Everon with real kit |
| K1 | Smoke and bottles | Dirty money #1025 | Moonshine plus the cigarette and vodka run, sold to the buyer and the bars |
| K2 | The green plots | Dirty money #1025 | Grow, dry and bag cannabis in the hills; the farmer's dog |
| K3 | The pharmacy | The big heist #1024 (#4154) | Rob the clinic or bribe the pharmacist; press speed pills |
| K4 | Night landings | Story: The shipment #1036 | Signal a boat at a cove, carry the crates, cut and bag in a barn |
| K5 | Word gets around | Making detective #1035 | Informants both ways, narcotics classes on goods, the tip line |
| K6 | The reserve | The big heist #1024 (#4153) | The week-long plan and the depot run |

**Cards**

| Key | Ch | Title | Type | Tier | Model | Machines | Owns | Depends | Acceptance | Risk |
|---|---|---|---|---|---|---|---|---|---|---|
| A1 | K0 | Vault drill picks up and drills (#4156) | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Crime/LF_Drill*.c` | none | test LF_Test_VaultDrillPickup | inventory |
| A2 | K0 | Playtest robbery kit works (#4156) | data | T1 | sonnet | test | `core/Configs/Crime/LF_RobberyKit*.conf` | A1 | test LF_Test_RobberyKitLoadout | none |
| A3 | K0 | Store box safes in two island shops (#2282) | town | T1 | sonnet | full-profile | `pack/Worlds/**/Place_saint-philippe_safes.layer` | A1 | proof look saint-philippe; audit placement --strict | none |
| A4 | K0 | Cash van route live on the island (#3398) | town | T1 | sonnet | full-profile | `pack/Worlds/**/Place_saint-philippe_cashvan.layer` | none | proof look saint-philippe | none |
| A5 | K0 | Bank job proof, 2 robbers vs 2 officers (#3752) | docs | T0 | sonnet | session-2p | `agent-box/proofs/bank-job-island.mjs` | A1 | proof bank-job-island | none |
| A6 | K0 | Server-wide big-job lock | system | T2 | opus | test | `core/Scripts/Game/Feature/Crime/LF_BigJobLock*.c` | none | test LF_Test_BigJobLock | persistence |
| B1 | K1 | Buyer demand cap per good and period | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Laundering/LF_BuyerDemand*.c` | none | test LF_Test_BuyerDemand | money |
| B2 | K1 | Cigarette carton and vodka bottle items | visual | T1 | sonnet | none | `pack/Prefabs/Items/Contraband/*`, `pack/Assets/Items/Contraband/*` | none | proof look item cigarette carton | none |
| B3 | K1 | Short container: refill timer and Marcel's key | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Crime/LF_StashContainer*.c` | none | test LF_Test_StashContainerRefill | inventory |
| B4 | K1 | Marcel's dialogue and fee (pack) | data | T1 | sonnet | test | `pack/Configs/Dialogue/LF_Dialogue_TallyClerk.conf` | B3 | test LF_Test_EveronTallyClerk | none |
| B5 | K1 | Under-the-counter outlet on a barkeeper | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Laundering/LF_CounterOutlet*.c` | B1 | test LF_Test_CounterOutlet | money |
| B6 | K1 | Josette's lines, prices and caps (pack) | data | T1 | sonnet | test | `pack/Configs/Crime/LF_CounterOutlets_Everon1989.conf`, `pack/Configs/Dialogue/LF_Dialogue_Barkeeper.conf` | B5 | test LF_Test_EveronBarkeeper | none |
| B7 | K1 | Contraband in a car is found by a stop search | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Police/LF_VehicleSearch*.c` | none | test LF_Test_VehicleSearchGoods | inventory |
| B8 | K1 | Voice lines: Marcel, Josette, Vince's new lines | data | T1 | sonnet | none | `pack/Sounds/Voice/Crime/K1/*` | B4, B6 | fixture pack/Sounds/Voice/Crime/K1/manifest.json | none |
| B9 | K1 | Town: container yard and two village bars | town | T1 | sonnet | full-profile | `pack/Worlds/**/Place_*_contraband.layer` | B3, B5 | proof look saint-philippe-dock; audit placement --strict | none |
| C1 | K2 | Cannabis plant mesh, 3 stages (after the policy check) | visual | T1 | sonnet | none | `pack/Assets/Crops/Cannabis/*` | none | proof look item cannabis stage 3 | none |
| C2 | K2 | Hidden crop rows: plot spots in a clearing | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Farming/LF_HiddenPlot*.c` | none | test LF_Test_HiddenPlot | persistence |
| C3 | K2 | Drying rack station (chain step `dry`) | data | T1 | sonnet | test | `pack/Prefabs/Chains/LF_DryingRack.et`, `pack/Configs/Resources/ResourceChains_Cannabis.conf` | C2 | test LF_Test_EveronCannabisChain | none |
| C4 | K2 | The watchdog: a nosy farmer NPC who calls it in | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Residents/LF_Watcher*.c` | none | test LF_Test_WatcherCallsLead | none |
| C5 | K2 | Burn a plot (police action) and the seizure | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Police/LF_BurnPlotAction.c` | C2 | test LF_Test_BurnPlot | none |
| C6 | K2 | Dog and crickets SFX, Fabien's lines | data | T1 | sonnet | none | `pack/Sounds/Crime/K2/*` | none | fixture pack/Sounds/Crime/K2/manifest.json | none |
| C7 | K2 | Town: two clearings and the drying loft | town | T1 | sonnet | full-profile | `pack/Worlds/**/Place_levie-clearings.layer` | C2, C3, C4 | proof look levie; audit placement --strict | none |
| D1 | K3 | Clinic shortage after a pharmacy robbery | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Crime/LF_ClinicShortage*.c` | none | test LF_Test_ClinicShortage | money |
| D2 | K3 | Pharmacy presence rule (medic or 2 police) | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Crime/LF_HospitalPresence.c` | none | test LF_Test_HospitalPresence | none |
| D3 | K3 | Pharmacy cabinet loot as chain goods (pack) | data | T1 | sonnet | test | `pack/Configs/Crime/LF_HospitalCabinet_Everon1989.conf` | none | test LF_Test_EveronPharmacyLoot | none |
| D4 | K3 | The night pharmacist's slow bribe | data | T1 | sonnet | test | `pack/Configs/Dialogue/LF_Dialogue_NightPharmacist.conf` | D1 | test LF_Test_EveronPharmacistBribe | money |
| D5 | K3 | Pill press mesh and lever animation | visual | T1 | sonnet | none | `pack/Assets/Props/PillPress/*` | none | proof look item pill press | none |
| D6 | K3 | Speed chain rows: press and bottle | data | T1 | sonnet | test | `pack/Configs/Resources/ResourceChains_Speed.conf`, `pack/Prefabs/Chains/LF_PillPress.et` | D3, D5 | test LF_Test_EveronSpeedChain | none |
| D7 | K3 | Hideout processing perk hosts a station | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Hideouts/LF_HideoutPerkStation*.c` | #4146 | test LF_Test_HideoutPerkStation | none |
| D8 | K3 | Clinic door notice during a shortage | visual | T1 | sonnet | none | `pack/Configs/Signs/LF_Sign_ClinicShortage.conf` | D1 | proof look item clinic notice | none |
| E1 | K4 | Landing calendar: one a day, announced ahead | system | T2 | opus | test | `core/Scripts/Game/Feature/Crime/LF_Landing*.c` | A6 | test LF_Test_LandingCalendar | persistence |
| E2 | K4 | Signal lamp: three flashes start the landing | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Crime/LF_SignalLamp*.c` | E1 | test LF_Test_SignalLamp | none |
| E3 | K4 | Crates wash up: spawn, carry, despawn at dawn | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Crime/LF_LandingDrop*.c` | E2 | test LF_Test_LandingDrop | inventory |
| E4 | K4 | Offshore boat sound and flare cue | visual | T1 | sonnet | session | `pack/Sounds/Crime/K4/*`, `pack/Prefabs/Systems/LF_OffshoreCue.et` | E2 | proof look grid:slice-E4 | none |
| E5 | K4 | Lucien's note item and lines | data | T1 | sonnet | test | `pack/Configs/Dialogue/LF_Dialogue_Boatman.conf`, `pack/Prefabs/Items/LF_LandingNote.et` | E1 | test LF_Test_EveronBoatmanNote | none |
| E6 | K4 | Hashish brick and heroin bundle items; cut chain rows | data | T1 | sonnet | test | `pack/Configs/Resources/ResourceChains_Landing.conf`, `pack/Prefabs/Items/Landing/*` | D7 | test LF_Test_EveronLandingChain | none |
| E7 | K4 | Town: Goat Bay landing, Andre's Beacon, the cutting barn | town | T1 | sonnet | full-profile | `pack/Worlds/**/Place_goatbay-landing.layer` | E2, E3, E6 | proof look goatbay; audit placement --strict | none |
| F1 | K5 | Narcotics amount class on goods | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Chains/LF_GoodsClass.c` | none | test LF_Test_GoodsClass | none |
| F2 | K5 | The buyer as informant: hidden chance, area lead | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Laundering/LF_BuyerTip*.c` | none | test LF_Test_BuyerTip | none |
| F3 | K5 | Anonymous tip line at a payphone, paid on seizure | system | T2 | opus | test | `core/Scripts/Game/Feature/Police/LF_TipLine*.c` | F1 | test LF_Test_TipLinePayOnSeizure | money |
| F4 | K5 | Underworld standing loss when a tipster is named | data | T1 | sonnet | test | `core/Configs/Reputation/LF_Reputation_Tips.conf` | F3 | test LF_Test_TipsterStandingLoss | none |
| G1 | K6 | Rota papers item and the start gate | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Crime/LF_HeistIntel*.c` | A6 | test LF_Test_HeistIntelGate | none |
| G2 | K6 | Power-cut stage: breaker cabinet, lights and alarm line down | system | T2 | opus | test | `core/Scripts/Game/Feature/Crime/LF_PowerCut*.c` | none | test LF_Test_PowerCutStage | replication |
| G3 | K6 | Two-layer vault stages on the heist run | system | T2 | sonnet | test | `core/Configs/Crime/LF_HeistReserve.conf`, `core/Scripts/Game/Feature/Crime/LF_HeistReserve*.c` | G1 | test LF_Test_HeistReserveStages | none |
| G4 | K6 | Armoured cash box: two-hand carry, cracked at a hideout | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Crime/LF_CashBox*.c` | none | test LF_Test_CashBoxCarry | money |
| G5 | K6 | Cash box mesh | visual | T1 | sonnet | none | `pack/Assets/Props/CashBox/*` | none | proof look item cash box | none |
| G6 | K6 | Unarmed watchman and the rota book | data | T1 | sonnet | test | `pack/Configs/Dialogue/LF_Dialogue_Watchman.conf`, `pack/Prefabs/Characters/LF_DepotWatchman.et` | G1 | test LF_Test_EveronWatchman | none |
| G7 | K6 | Armory opens with the depot alarm | system | T2 | sonnet | test | `core/Scripts/Game/Feature/Police/LF_ArmoryAlarm*.c` | G3 | test LF_Test_ArmoryOpensOnAlarm | none |
| G8 | K6 | Reserve run proof, 4 robbers vs 5 police | docs | T0 | opus | session-2p | `agent-box/proofs/reserve-run.mjs` | G2, G3, G4, G7 | proof reserve-run | none |
| G9 | K6 | Town: the depot and the substation | town | T1 | sonnet | full-profile | `pack/Worlds/**/Place_reserve-depot.layer` | G2, G3, G6 | proof look reserve-depot; audit placement --strict | none |

## 9. Risks and open questions

**Risks**

- **Workshop content policy on named drugs.** Cannabis, hashish, heroin and amphetamine may break the Bohemia
  Workshop rules. The plan keeps no chemistry steps ("press", "cut", "bag"), but the policy check (#4152 D2)
  comes before C1, D6 and E6. If a name fails, the chain keeps its verbs and gets a period euphemism.
- **Crime starves the police.** Marked money, the wash and seizures keep crime near duty pay. Watch the ledger
  for a week after each chapter; if crime runs above 2.5x civilian, raise the wash cut or lower buyer caps.
- **Low population.** Presence rules lock rungs 3 to 6 when few police are on. That is intended, but on a
  quiet night the chains (which need no police) carry the crime area.
- **Night landings on a 2 h day.** A two-in-the-morning landing comes round every two real hours. Announce
  landings for a real-time window instead, so players can plan.
- **Hidden crops collide with farming.** The plot needs #809 to place crops off a farm plot. If that slips,
  K2 waits.
- **Informants poison trust.** A tip line can turn into griefing. Tips name an area, never a player, and pay
  only on a seizure.
- **AI guards.** Armed NPC guards bring combat AI the framework does not have. The watchman is unarmed on
  purpose.

**Open questions for the maintainer** (each has a recommendation)

1. **Named drugs.** Use the 2026-11-22 list (cannabis, hashish or heroin, amphetamine) after the policy check?
   **Recommend yes**, with a euphemism fallback per chain.
2. **Crime pay band.** Expected clean pay equal to the duty band (1.5 to 2.5x civilian), with high variance?
   **Recommend yes.**
3. **No crime skill line.** Progress by record, standing and equipment only, not a CRIMINAL level?
   **Recommend yes**, to match M6 decision 7.
4. **Landings.** One a day server-wide, announced 12 to 24 hours ahead to whoever buys the note?
   **Recommend yes.**
5. **The pharmacist bribe.** A slow, quiet alternative to robbing the clinic? **Recommend yes**: it gives
   non-violent players a way into the speed chain.
6. **Tip line.** Let a player sell an anonymous tip to the police? **Recommend yes, in K5**, paid only on a
   seizure.
7. **Heist masks.** Spend the Meshy mask budget on the heist mask sold by Vince? **Recommend yes.**
8. **Order.** K0 before anything else, then K1 to K6 as listed? **Recommend yes**: K1 builds the buyer
   demand and stop search that every later chain uses.

## Suggested additions (proposals for the maintainer)

These go beyond the wishlist. Each is a proposal, not a plan item, until the maintainer accepts it.

| Proposal | What | Why it helps the whole | Size | Touches |
|---|---|---|---|---|
| **Front businesses** | A player who owns a shop or café can run marked notes through its till: a lower cut than the barn (20 %), a small daily cap, and an audit risk if the till takes more than its trade | Links ownership and crime; gives legal owners a reason to deal with criminals; a quieter wash for long-term players | M | Ownership, economy, police (audits) |
| **Pickpocketing** | A held action behind an unaware player or NPC on the square takes a few notes; a witness or the victim's turn ends it | A tiny crime for hour 1 that needs no tools; police on foot have something to watch | S | Police, residents |
| **Fuel siphoning** | A hose from the fence siphons a parked car's tank into a jerrycan; sold to the fuel buyer at a discount | Links the fuel chain and car owners; cheap, low-risk night work | S | Work (fuel), vehicles |
| **Car to order** | Vince's chalk board names a model this week; deliver it unpainted for double the chop value | Gives the chop shop a goal and a reason to hunt one car across the island (M6 C4 already suggests it) | S | Vehicles, police (plates) |
| **Re-plating** | At the chop yard, swap the plate of a stolen car for a cold one; the stolen flag stays but the board loses it | A second use of the yard and a reason for police to check the VIN at a stop | M | Vehicles, police |
| **Protection money** | A gang collects a weekly envelope from an NPC shopkeeper; if nobody collects, a rival can. The shopkeeper calls the police if threatened twice | Turf with a reason to patrol it; links gangs to the towns | M | Gangs, residents, police |
| **Poaching** | Hunting out of season or without a licence in the forest; meat sold to a bar's back door | Links hunting and the counter outlets; a rural crime with no robbery | S | Work (hunting), legal system |
| **Burglary of player homes** | Lockpick a player home's door while the owner is offline, take one item from an unlocked container, and leave a sign of entry the owner sees | Makes locks, safes and home storage matter; gives police a case to investigate | M | Ownership (homes), police |
| **Evidence locker theft** | Steal back seized goods from the police station's locker during a shift change (links #773, the evidence convoy) | A crime that targets the police, with high stakes | M | Police |
| **Bribing an officer** | An officer can accept a cash bribe instead of a fine; it goes on a hidden record that internal affairs (a higher rank) can read | Moral choice for police players; drama | M | Police, legal system |
| **Smuggled radios and jeans** | Western goods (cassette players, jeans, records) as a tier-1 smuggling good beside cigarettes, sold to the clothing store's back room | Period-true 1989 Iron Curtain black market; links the clothing store | S | Shops |
| **The newspaper crime column** | A notice board in the café prints last day's robberies and seizures, with no names | Gives the island a memory; players see their story told | S | Atmosphere, notice boards |
| **Getaway driver role** | A crew role for the heist run (#780): the driver waits in a car, gets a share, and their car is the only one that can start the heist clock's last stage | A role for players who like driving more than shooting | S | Gangs, vehicles |
| **Prison contraband** | Cigarettes as prison currency; a visitor can pass a carton through the yard fence (links Behind bars #1027) | Gives prison time something to do and ties it to the smuggling chain | S | Police (prison) |
