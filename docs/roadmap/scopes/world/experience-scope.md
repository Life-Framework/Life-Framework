# World, people and immersion: the experience scope

Milestone: cross-cutting (M6, M7, M8). This scope is the asset bible the other four scopes draw on:
[work](../work/experience-scope.md), [crime](../crime/experience-scope.md),
[gangs](../gangs/experience-scope.md), [police](../police/experience-scope.md).
Extends: residents (`Feature/Residents/`, `docs/features/residents.md`), atmosphere
(`docs/architecture/atmosphere.md`), the sign system (`docs/architecture/sign-system.md`, epic #3286),
visual boards (`docs/architecture/visual-boards.md`), NPC dialogue scenes and the ElevenLabs pipeline
(`tools/assets/pipeline/elevenlabs.mjs`, `dialogue.mjs`, `ambience.mjs`), NPC outfit presets
(`Configs/NpcOutfits/`), the clothing pipeline (`docs/asset-pipeline/clothing.md`), the town pipeline
and its kits (`tools/town/kits/`, `town street-life`, `town nightlights`, `parked-cars`), the map's
discovery (`LF_MapKnown`), and the livery epic #4155.

This is a plan only. No issues are cut and nothing is dispatched until the maintainer approves it and
the integrator folds it into the week plan.

**The rule this scope enforces everywhere.** Each system the other scopes add shows up in the world
four ways: a **place** you walk to, a **person** who works there in clothes that fit the job, a
**sound** you hear before you see it, and a **printed thing** (sign, board, slip or paper) you can
read. If a feature has only a menu, it is not finished.

## 1. Player fantasy and core loop

**Hour 1.** You step out of the hostel into Saint-Philippe at dawn. Pigeons lift off the square, the
fountain runs, the café radio plays and Odilon is wiping tables on the terrace. Every sign you pass is
readable: "TOWN HALL", "BAKERY - OPEN 6 AM", the chalk board outside the café with today's soup. The
clerk talks to you in a voice. You drive the starter car past a parked postal van lettered "POSTAL
SERVICE" and a blue-and-white gendarmerie UAZ outside the station. At the orchard a farmer in a flat
cap and raincoat nods and says the press is in the barn. You sell juice at the market, and the trader
hands you a printed till slip. You never opened a window to find any of this. The world told you.

**Hour 10.** You know people now and they know you. The baker calls you by your face ("The usual?")
and takes 10 % off; the bartender won't serve you since Gaspard saw you lift a wallet. You read the
weekly *Everon Courier* on the newsstand and find a short item about the bank robbery you heard
sirens for on Tuesday, with the reward the gendarmerie posted. Old women on the bench by the church
tell you about a cove that "pays at dusk", and it appears on your map as "heard of". You know the
island's rhythm: the factory siren at 6, market day on Saturday with the stalls up and an accordion
at the pub, the night shift walking to the docks. Your truck carries a company livery you earned on
the lumber track.

**Hour 50.** The island reacts to your history. Your name has been in the *Courier* three times (once
as "a local hauler of good standing", once on the wanted list). You picked a livery and plate for your
fleet; your gang's flag hangs on a barn in Levie and the locals there drop their voices when you walk
in. You can tell from the sound alone whether Montignac's factory is on shift, whether the harbor is
busy, or whether a siren is a fire drill or a robbery. New players ask you the way, and you answer with
landmarks: "past the yellow chandler sign, left at the payphone with the torn poster".

**Why it holds people.**

- **Recognition.** NPC memory (`LF_ResidentMemory`) and the newspaper make your actions visible to
  the island. Players come back to see what changed and what people say about them.
- **Discovery.** Rumors told by people fill the map (`LF_MapKnown.Hear`). Every conversation can turn
  up a place, a price or a tip, so talking to people pays.
- **Rhythm.** A weekly calendar of market day, payday, the Friday dance and the Sunday bell gives
  players fixed times to meet. It lines up with the Saturday community session.
- **Show-off.** Liveries, clothes, titles in the paper and a gang flag are rewards other players
  can see. The work scope's mastery rewards pay out here.
- **Variety without menus.** Each workplace has its own sounds, people and paper, so a fishing hour
  does not feel like a mining hour with different numbers.

## 2. Systems: how this fits the other areas

This scope builds few systems of its own. It mostly supplies people, places, sound and paper to the
systems the others build.

| Shared system | What this scope adds | Owner |
|---|---|---|
| XP and skills (#4140) | Level-up moment sound and voice bark ("You're getting the hang of it"); mastery rewards as liveries, clothing sets and a *Courier* item; foremen at each work site who say the next unlock in words | work |
| Reputation and standing (`LF_Reputation`, `LF_ResidentMemory`) | Clerk greetings and prices that follow the memory word; gossip lines; the tier word on the ID card printed, not shown as a number | world (barks), work and crime (awards) |
| Wanted level and the legal system (#4151) | Wanted posters with portraits on boards and in the *Courier*; the printed charge sheet, fine ticket and sentence slip as items; the gendarmerie's enamel signs and station sounds | police |
| Gangs (#4146 to #4150) | Flag cloth on poles and walls; hideout dressing kits per tier; a gang radio handset with squelch sounds; villagers who react to the flag; rebel outpost traders in outfits | gangs |
| Vehicles and cargo (#4141 to #4144) | The livery pipeline (Police, Ambulance, Civilian, Rebel) for cars, trucks and helis; company liveries; visible cargo; dealer lots with price cards on the windshields | world (livery), work (dealers) |
| Economy and market | Price boards and chalk boards that print live prices (sign S2 done); till slips; market-day stalls; sinks such as respray, barber, café meals and newspapers | work |
| Map and run chains (#2223) | Rumors that mark places "heard of"; map POIs for every dressed site; a printed road map sold at the newsagent | world |
| HUD | Nothing new on the HUD from this scope. Information lives on objects. HUD stays for the wanted state, the jail clock and timers the other scopes need | all |
| F2 player menu | The "What to do next" tab can point at a person ("Ask Gaspard about fishing"), not only a place | work |
| Day and night (#129), events (#757) | The island calendar (see section 3) runs event scenes at fixed times; night lights, shutters and night sounds | world |

## 3. Places

Each place below has a look, its people, its sound and its paper. Place ids follow
`docs/everon/places/` and the world plan's map states. Places marked **dossier** already have a scout
dossier and mapping; the rest need `tools\cli scout` first.

| Place | Look in 1989 | People | Sound | Paper |
|---|---|---|---|---|
| **Saint-Philippe square** (state 0, live) | Stone townhouses, café terrace with umbrellas (`MarketStand_01_Umbrella`), the fountain, the town hall flag (`Flag_1_2_Everon_01`), a newsstand (`NewspaperStand_01`) | Clerk, café owner, Gaspard on the bench, store clerk, teller, two gossips, a gendarme on foot at the bank in the evenings | Fountain, pigeons (`BirdFlock_DomesticPigeon`), a far murmur, the café radio, the church bell on the hour | Town board, the café chalk board, the *Courier* on the stand, enamel plates on civic doors |
| **Saint-Philippe dock and districts** (2a, dossier) | Warehouses, pallets (`Pallet_01_Pile`), fishing nets (`FishingNet_01..03`), boat trailers, the harbor master's hut, sodium lamps at night | Harbor master, two dockers in hard hats (`Hat_Hard_01`), a fishmonger, the night watchman | Gulls (`BirdFlock_Seagull`), rigging and water slap, a forklift beep, a ship's horn at 7 and 19 | Crate stencils, the tide and work board, weigh slips |
| **Meaux** (2a, dossier: 61 buildings, 1 shop, 1 kiosk) | A coastal village of sheds, garages and small farms around a harbor 220 m west | Postmaster (the postal run's drop), a village shopkeeper, an old fisherman mending nets | Waves, a dog barking, a moped passing | A painted wooden village sign, the post office's enamel plate |
| **Montignac** (2c, dossier: 164 buildings, 3 industrial, 2 churches) | The work town in the hills: the state factory with a floodlit yard, a canteen, a church, many ruins | Factory director (story), foreman, line workers in work clothes, a canteen cook, a priest | Shift siren (`Firestation_siren_01` or `Amplion_Siren` with our sound) at 6, 14 and 22, machinery, trucks reversing | Shift roster board, payslip envelopes, union notices, safety posters |
| **Coalman's Brow** (mine, dossier) | Spoil heaps, the furnace and the iron buyer 10 to 20 m apart | Mine foreman, the ore buyer | Picks, falling stone, the furnace roar (exists) | Mine safety board, ore receipts |
| **Everon sawmill and the lumber yard** (dossier) | Log stacks, the saw shed, a weighbridge | Sawyer, yard clerk | The saw (exists), a chainsaw in the woods | Timber price board, delivery note |
| **Farm compound, road farm, Saint-Philippe farm west** (dossiers) | Collective-farm sheds (`CowShed_E_01`), tractors, laundry lines from `town street-life` | Farmer, farmhand, the farm manager's wife selling eggs | Hens (exists), a tractor, wind in the wheat | Farm gate sign in wood, the seed-and-feed price card |
| **Black Lake and Fisherman's Bay** (2b, dossiers) | A shingle shore, an upturned boat (`BoatWreck_01`), a boat shed (`BoatShed_01`) | Gaspard at dusk, a weekend angler | Water, loons at night, wind | A painted "NO FISHING WITHOUT LICENSE" plate |
| **Scrapyard and chop yard** (2d, dossiers) | Car wrecks stacked, a crane, a guard dog's kennel (`DogHouse_01`) | Scrapyard owner, Vince at the chop yard | Grinding, a compressor (exists at the chop bench), a dog | Hand-painted "WE BUY SCRAP" boards |
| **Levie and Chotain** (villages) | Village shop and bar, moonshine shed at the edge | Bartender, village gossip | Bar music at night, crickets | Bar chalk board, a hand-written "rooms" card |
| **Morton** | South-coast harbor village, the cement works for the stone chain | Works clerk, harbor hands | Cement mixer drum, gulls | Cement sack stencils, works gate sign |
| **The hospital and the gendarmerie** (state 0) | White and red hospital signage, the blue enamel "GENDARMERIE" plate | Receptionist NPC at the hospital, desk sergeant NPC when no officer is on duty | Ward hum (exists), radio static (exists), typewriter (`Typewriter_01_grey` with a sound) | Hospital bill, fine ticket, sentence slip (assets exist) |
| **Rebel outposts north and south** (#4145) | Fenced compounds, a generator (`GeneratorFloodlight_USSR_01`), camo nets, the rebel flag | Two traders, two guards (outfits below) | Generator, a radio playing foreign stations, a guard dog | Hand-painted stock board in chalk, no prices for strangers |
| **Calvary Hill and the cemetery** (M8) | Crosses, a bench, flowers | A widow on Sundays | Wind, the bell from town | The anniversary notice |

**The island calendar.** One config row per recurring scene, run by the event shell. Game days are 2 h
real time (M4 decision), so an in-game week is 14 real hours.

| When (in game) | Scene | Where |
|---|---|---|
| Every hour, 07:00 to 21:00 | Church bell strikes the hour | Saint-Philippe, Montignac |
| 06:00, 14:00, 22:00 | Factory siren, workers walk to and from shift | Montignac |
| 07:00, 19:00 | Ship's horn, dockers change shift | Saint-Philippe dock |
| Day 6 (Saturday) | Market day: stalls up, a crowd murmur bed, an accordion player NPC | Saint-Philippe square |
| Day 5 (Friday) night | The dance: the pub's music louder, more residents at tables | The pub |
| Day 7 (Sunday) morning | Church service: residents walk to church, the square is quiet | Saint-Philippe |
| Payday (day 5, 16:00) | The cash van stops at the factory gate; workers queue | Montignac |
| Weekly | The *Courier* prints a new issue | Every newsstand and board |

## 4. People

**The cast rule.** Every workplace an area scope opens gets one named NPC who runs it and, where it
fits, one or two generic workers. Named NPCs are residents (`LF_ResidentDef` rows with a timetable,
home and work place kinds, memory and Talk). Generic workers are role NPCs with barks and no memory.
All clothing comes from the vanilla families checked with `asset_search` on 2026-11-22: `Jacket_Suit_01`,
`Jacket_Denim_01`, `Jacket_Raincoat_01`, `Shirt_CottonShirt_01`, `Shirt_Turtleneck_01`,
`Pants_Trousers_01`, `Pants_Suit_01`, `Pants_Fisherman_01`, `Hat_FlatCap_01`, `Hat_Knitted_01`,
`Hat_TruckerCap_01`, `Hat_Panama_Soviet`, `Hat_Ushanka_01`, `Hat_RadiovkaBeret_01`, `Hat_Hard_01`,
`Gloves_Leather_01`, `Gloves_Wool_01`, `Gloves_Rally_01`, `Jacket_Pilot_USSR_01`, `Pants_Pilot_USSR_01`,
`Jacket_Tanker_USSR_01`, `CombatBoots_Soviet_01`, `JungleBoots_01`, and the `GenericCivilians` set. The
base game has no apron, overall, sweater or coverall. Those come from the clothing pipeline's tint
route (`clothing-parity-tint`) or, for the doctor coat, the existing `doctor-coat` asset.

**Voices.** ElevenLabs through `tools/assets/pipeline/dialogue.mjs`. Lines are US English. A light
French accent is allowed for islanders; the relocated speak plainly. A named NPC has about 12 lines:
greetings for the five memory words, two job lines, two rumors, a refusal, goodbye. A generic role
has 6 barks. Lines ship as captions first if the pipeline is busy (M6 rule).

| NPC | Role and place | Outfit (vanilla) | Behavior | Lines |
|---|---|---|---|---|
| Harbor master (named) | Runs the dock contracts, Saint-Philippe dock | `Jacket_Raincoat_01_green`, `Hat_Knitted_01_black`, `Pants_Fisherman_01_green` | Stands at the hut 06:00 to 20:00, walks the quay at 07:00 and 19:00 | 14 |
| Docker (generic, 2) | Moves crates on the quay | `Shirt_CottonShirt_01`, `Hat_Hard_01`, `Pants_Trousers_01`, `Gloves_Leather_01` | Carry pose loop near pallets | 6 |
| Fishmonger (named) | The fish counter on the quay | Turtleneck, tinted apron (pipeline) | Serves at the counter, smokes at the door at 15:00 | 12 |
| Chandler (named) | Rod, bait and licenses help, dock | `Jacket_Denim_01`, `Hat_FlatCap_01_blue` | Behind the counter | 12 |
| Factory director (named, story) | Montignac factory office | `Jacket_Suit_01_black_V1`, `Pants_Suit_01_black_V1` | Office by day, walks the yard at shift change | 16 |
| Foreman (named, one per work site: factory, mine, sawmill) | Hands out shifts and tells the next unlock in words | `Hat_Hard_01`, `Jacket_Denim_01`, `Pants_Trousers_01_DarkBrown` | Stands at the site board; "You've earned the hardwood saws" | 12 each |
| Line worker (generic, 4) | Factory | Tinted work jacket, `Hat_FlatCap_01_darkgrey` | Walks in at the siren, out at the next | 6 |
| Canteen cook (generic) | Montignac canteen | Tinted apron | Serves food (shop) | 6 |
| Baker (named) | Saint-Philippe bakery (kit exists) | `Shirt_CottonShirt_01` white, tinted apron | Opens at 06:00, sells out of bread by 14:00 | 12 |
| Butcher, pharmacist, barber (named) | Kits exist: `butcher`, `pharmacy`, `barber` | Turtleneck and apron; suit and doctor coat; shirt and waistcoat-tint | Counter service, door loiter | 10 each |
| Newsagent (named) | The newspaper booth (`NewspaperBooth_E_01_redwhite`, kit exists) | `Jacket_Denim_01`, `Hat_TruckerCap_01` | Sells the *Courier*, the road map and cigarettes; the best gossip on the island | 14 |
| Two gossips (named, older women) | The square bench, church on Sunday | `Jacket_Raincoat_01`, `Hat_Knitted_01`, `GenericCivilians` | Sit at the bench anchor; each holds two rumors a day from the rumor pool | 14 each |
| Postmaster (named) | Meaux post office, postal run drop | `Jacket_Suit_01_Brown_V1`, `Hat_PeakedCap_USSR_01` with the insignia painted out (uniform-paint) | Behind the counter | 10 |
| Bartender (named, one per bar) | Saint-Philippe pub, Levie bar | `Shirt_Turtleneck_01_DarkBlue` | Serves; refuses drunk or wanted players | 12 |
| Accordion player (generic) | Market day and the Friday dance | `Hat_Panama_Soviet`, suit | Sits with a placed accordion (Blender build) and the music emitter | 3 |
| Priest (named) | Church, Saint-Philippe | Black suit, tinted collar | Sunday service scene, rings the bell | 8 |
| Mechanic (named) | The garage: respray and livery shop | Outfit preset `LF_Outfit_Mechanic` | Under a car or at the counter | 12 |
| Hospital receptionist, desk sergeant (generic) | Front desks when no player is on duty | `LF_Outfit_Doctor`, `LF_Outfit_Police` | "No officer on duty right now. Use the payphone and they'll be paged." | 8 each |
| Prison yard staff (generic) | The prison (police scope) | Police outfit, `Hat_PeakedCap` | Says the time left on a sentence | 6 |
| Rebel trader and guard (generic, 2 each per outpost) | Rebel outposts (gangs scope) | `Jacket_KZS`, `Pants_KZS`, `Hat_Boonie_US_01`, `Gloves_Rally_01`, balaclava (asset exists) | Trader refuses strangers until outpost standing; guards loiter armed (no AI combat) | 10 trader, 4 guard |
| Smuggler contact (named, night only) | A cove (crime scope) | Dark raincoat, `Hat_Knitted_01_black` | Appears 22:00 to 04:00 only | 10 |
| Informant (named) | Pub corner (police scope) | `Jacket_Denim_01`, cap | Sells a tip to officers for cash; to gangs it is a rumor | 10 |

**Behavior.** All of these use the residents system that exists: timetables, anchors, `serve`,
`sit`, `loiter`, `idle`, walks within 120 m, the 24-resident live cap per server with the director
filling the place a player stands in. Two additions are needed: a `work` activity loop for generic
workers (carry, hammer or sweep from vanilla gestures), and the rumor line in Talk (section 6).

## 5. Assets

Source order: vanilla first, then CC0 or CC-BY, then our Blender build scripts. Meshy only for the fish
and one or two masks. Sound is always ElevenLabs.

### 3D assets and props

| Asset | Source | Prefab or plan | Why |
|---|---|---|---|
| Newsstand and booth | Vanilla | `NewspaperStand_01`, `NewspaperBooth_E_01_redwhite` | Exists in base game |
| The *Courier* (held item and a stack on the stand) | Blender | Folded paper mesh, face drawn on the sign carrier quad (`paper-print/paperlib.py`) | No vanilla newspaper item |
| Fishing nets, boat shed, boat trailer, wreck | Vanilla | `FishingNet_01..03`, `BoatShed_01`, `BoatTrailer_01`, `BoatWreck_01` | Exists |
| Pallets, crates, toolboxes | Vanilla | `Pallet_01_Pile`, `Crate_01_*`, `CrateWooden_01_plain`, `ToolBox_01` | Exists |
| Market umbrellas and stalls | Vanilla | `MarketStand_01_Umbrella` and the market kit | Exists |
| Typewriter, switchboard phone, loudspeaker | Vanilla | `Typewriter_01_grey`, `SwitchboardPhone_01`, `LoudspeakerMicrophone_01` | Station and office dressing |
| Factory siren, loudspeaker siren | Vanilla | `Firestation_siren_01`, `Amplion_Siren` | Carriers for the shift siren sound |
| Flags and poles | Vanilla | `Flag_1_2_*`, `FlagPole_01_empty` | Town hall flag; gang flags reuse the cloth with our textures |
| Gang flag textures (about 20) | Our own art (generated in script) | A texture set on the `Flag_1_2` UVs; the flag mesh stays vanilla | Our own pattern art, no Bohemia texture derived |
| Birds | Vanilla | `BirdFlock_Seagull`, `BirdFlock_DomesticPigeon`, `BirdFlock_WoodPigeon` | Ambient life at no art cost |
| Dog kennel | Vanilla | `DogHouse_01` | The base game has no animals; a barking emitter at a kennel stands in for a dog |
| Suitcase | Vanilla | `Suitcase_01` | Arrival scene, the relocated |
| Accordion | Blender | Simple box and bellows mesh, static | No vanilla instrument |
| Shop sale tags, price cards on car windshields | Sign carriers | `quad4x3` carrier, `card` style | Sign system |
| Wanted poster, notices | Sign carriers | `poster` style (sign S6) | Sign system |
| Road map (paper, sold) | Blender plus sign face | Folded map item | Map discovery shortcut |
| Gang radio handset | Vanilla | `Radio_ANPRC68` or `Radio_R107M` reskinned in config only | Exists, era fits |
| Pager | Existing | `tools/assets/pager` | Exists |
| Bench, bins, flower boxes, washing lines | Vanilla via `town street-life` | Existing tool | Exists |
| Fish (5 species) | Meshy (approved) | `tools/assets/fish-*` | Approved Meshy use |
| Period masks (1 or 2) | Meshy (approved) | `doll-mask`, `hockey-mask` | Approved Meshy use |

### Clothing

| Need | Source | How |
|---|---|---|
| Every outfit in section 4 | Vanilla | New `LF_NpcOutfitPreset` rows in `LF_Everon1989/Configs/NpcOutfits/` |
| Aprons, work jackets, a priest collar | Clothing pipeline, tint route | Tinted vanilla garments; the textures fall under Bohemia's terms (`docs/asset-pipeline/clothing.md` "Licensing"), so each needs the maintainer's yes and a `LICENSES.md` row |
| Postmaster and prison staff caps | `uniform-paint` | Insignia painted out over the vanilla cap, as for police |
| Mastery clothing (work scope rewards) | Vanilla combos first | A named set ("Harbor jacket": raincoat, knitted hat, fisherman trousers) given as a reward, no new mesh |

### Vehicles and liveries (epic #4155)

The postal van (#4088) proved the approach: vanilla sign plates fixed to the van's body carry our
lettering through the sign system's render target. No vanilla texture is copied. That approach
becomes the pipeline:

1. **Color from vanilla variants.** Most cars and trucks come in vanilla colors (`EL_S105_beige`,
   `EL_UAZ469_covered_CIV_blue`, `EL_Z7211_red_CIV`, and so on). Pick the variant whose paint fits.
2. **Lettering and stripes on overlay quads.** One `LF_LiveryDef` row per variant lists carrier
   quads (sign sizes) with a position and rotation in the vehicle's frame, a sign style
   (`livery-block`, `livery-stripe`) and the text keys. A CLI step (`tools\cli livery build <def>`)
   writes the variant prefab beside the base vehicle.
3. **Light bars and sirens** from `Prefabs/Vehicles/SirenLights/` (exists).
4. **A full repaint** (a material override with our own texture on the vehicle's UVs) only where
   overlays cannot work, for example a two-tone police body. It is the maintainer's licensing call
   (question Q1).

| Livery | Vehicles | Looks like | Source |
|---|---|---|---|
| Police (Gendarmerie) | `LF_UAZ469_Police`, S1203 van, UH-1H (later) | Vanilla blue or white body, "POLICE" on the doors and hood in white block capitals, a red and white stripe, the light bar | Overlay plus vanilla color |
| Ambulance | `LF_M997_Ambulance`, S1203 van, Mi-8MT medevac (later) | White body, red cross panels, "AMBULANCE" (mirrored on the hood) | Overlay; a white body may need a repaint (Q1) |
| Civilian companies | Postal van (done), taxi (V2101 with a roof sign), "BAKERY", "HARDWARE", "EVERON TIMBER", "STATE FACTORY" trucks (Ural4320, M923A1 transport) | Company name and phone number on the doors | Overlay |
| Civilian player | Any dealer car | A plain vanilla color; respray at the mechanic (vanilla variants only) | Vanilla |
| Rebel | UAZ469, Ural4320 | No lettering: mud, a hand-painted number, camo net on the cargo bed, the gang flag on a pole | Overlay quads in a `wood` or `stencil` style; mud as a decal quad |
| Helicopters | `Prefabs/Vehicles/Helicopters/UH1H/UH1H.et`, `Mi8MT` | Civil registration "EV-xxxx" on the tail boom and a company name; police and medevac variants | Overlay on the flat boom panels first (spike proves it, #4155) |

### Sounds (ElevenLabs SFX and ambience)

| Sound | Where | Notes |
|---|---|---|
| Church bell (hour strike, Sunday peal) | Church towers | Long tail, rolls off at 400 m |
| Factory shift siren | Montignac | One rise-and-fall, 8 s |
| Ship's horn | Saint-Philippe dock | Two blasts |
| Harbor bed: gulls, rigging, water | Dock, Meaux harbor | Loop, 06:00 to 21:00; quieter at night with water only |
| Village night bed: crickets, a far dog | Villages, farms | 21:00 to 05:00 |
| Market-day crowd and accordion | Square | Day 6 only |
| Forklift reverse beep, crate thud | Dock | Pulsed by a docker's work loop |
| Machinery hum | Factory hall | On shift only |
| Typewriter and phone ring | Town hall, gendarmerie, newsagent | Pulsed |
| Paper rustle | Reading the *Courier*, taking a slip | UI-free feedback |
| Level-up chime (soft brass) | Player, on a skill level | Shared with the work scope |
| Generator and foreign radio | Rebel outposts | Loop |
| Gang radio squelch and static | The handset | Gangs scope |
| Dog bark | Kennels at the scrapyard and farms | Random, close range |
| Two more period radio tracks, one cassette track | Café, shops, hideouts | `atmosphere.md` "Next" item 3 |

### In-world UI elements

| Element | Carrier | Shows |
|---|---|---|
| The *Everon Courier* | Newsstand stack, boards, a held item with a Read action | Weekly front page from the server's own records: robberies, arrests and rewards, top earners by trade (words, not numbers), market prices, new businesses, the gang flags seen |
| Shift and work boards | `chalk` style on the site's board | Today's shift, open contracts, the foreman's note |
| Price boards | `chalk` (done for the trader, S2) | Live prices |
| Opening hours plates | `enamel` small | "OPEN 6 AM - 2 PM" on each shop door, from the resident's timetable |
| Till slip, weigh slip, payslip, delivery note | Paper item (`receipt-paper` exists) | What was sold or paid, with the date |
| Fine ticket, charge sheet copy, sentence slip | Paper item (assets exist) | Police scope |
| Wanted posters | `poster` style on boards | Portraits (visual boards) |
| Car price cards | `card` style quad on the windshield | Dealer lots |
| Road signs and town entry signs | `road` style (sign S10) | US English place names and distances |
| Taxi roof sign | `quad4x1` carrier on the roof | "TAXI", lit at night |

## 6. Interactions

Every interaction is a scroll action on an object or person. Full screens stay limited to paperwork,
the map, F2 and F3.

| Object or person | Actions |
|---|---|
| Newsstand or newsagent | "Buy the Courier: $1", "Buy a road map: $15", "Read the front page" (on the stand's top copy) |
| The *Courier* in hand | "Read" (paperwork full screen, allowed: it is a paper), "Pin to board" |
| Any named resident | "Talk" (existing number-key dialogue), with a new reply "Heard anything?" that gives one rumor a day per resident and marks its place "heard of" |
| Gossips on the bench | "Sit and listen" on the bench next to them: after 20 s they trade a rumor without a dialogue |
| Shop door | "Read the hours" (or just read the plate) |
| Foreman at a work board | "Ask about work", "Ask what's next" (the next unlock in words) |
| Mechanic | Per vehicle in the bay: "Respray: beige $150", "Respray: blue $150", "Apply livery: Everon Timber" (when earned), "Remove livery" |
| Dealer car | Price card on the windshield; "Buy: $560" on the car itself (existing) |
| Church bell rope | "Ring the bell" (priest scene, or a player at night: a public nuisance charge for the police scope) |
| Pub radio, café radio | "Switch on", "Switch off", "Next station" |
| Town board | "Post a notice: $5" (a player notice on paper, moderated by the existing moderation tools) |
| Flag pole at a hideout | "Raise the flag", "Lower the flag" (gangs scope) |
| Accordion player | "Tip: $2" (a friendly memory award, cap once a day) |

## 7. Balance

`tools\cli economy` (2026-11-22, pack mission): civilian work pays about $400 to $700 an hour (parcel
$400, postal $480, fuel $563, taxi $600, mining $460 to $515, lumber $365 to $478); police duty is $900
to $1260 an hour and medic $792 to $1224, inside the 1.5 to 2.5x band. The cheapest car is $560.

This scope adds **no income** except the small gossip and newspaper value of information. It adds
**sinks** and **soft rewards**:

| Item | Price | Rule |
|---|---|---|
| The *Courier* | $1 | One issue a week; reading it marks two places "heard of" |
| Road map | $15 | Marks every live town "heard of" (not jobs or coves) |
| Respray (vanilla color) | $150 | Any owned vehicle, at the mechanic |
| Company livery | $250 to $400 | Unlocked by a profession level (work scope); 30 to 60 min of income |
| Rebel livery | $300 | Gang standing at a rebel outpost; the plate is still on record |
| Barber | $15 | Hair and beard from the vanilla head options, if the character system allows a change |
| Café meal, pub drink | $4 to $12 (existing prices) | Survival buffs |
| Player notice on the town board | $5 | One live notice per player |
| Tip the accordion player | $2 | Once a day counts for memory |

**Rumors are rationed.** One rumor per resident per player per game day, from a pool tied to live
places and live state (today's best buyer, a cove at dusk, a car parked unlocked). Crime rumors only
come from bar staff and the informant, and only to players whose memory word is `friendly` or better.
That keeps rumors a reward for being known, not a free map.

**Risk vs reward.** Being recognized cuts both ways: `regular` saves 10 % in shops and opens rumors;
a crime seen by a resident spreads one hop and costs both. That is the world's main pressure on
crime, and it costs nothing to the economy.

## 8. Slices

Chapters run in this order. Each ends with something a player notices on the island. Cards follow
`docs/process/slice-card.md`: at most 45 min and 60 calls, file-disjoint, acceptance first. World props
go into grid cells; a `town` card per chapter moves them onto the island. Keys are local to this
plan. "R" is the residents feature, "A" atmosphere, "S" signs, "L" liveries, "P" printed matter.

**W1. The square comes alive** (hours 0 to 2 polish; ships first because it helps onboarding)
**W2. Read the island** (printed matter: the *Courier*, slips, hours plates)
**W3. Liveries** (epic #4155: spike, then rollout)
**W4. People at work** (foremen, workers and their barks at every work site)
**W5. Word of mouth** (rumors and discovery)
**W6. The island calendar** (bells, sirens, market day, the dance)
**W7. One 1989 look** (style audit and place-by-place passes)

| Key | Chapter | Card | Type | Tier | Model | Owns | Depends | Acceptance |
|---|---|---|---|---|---|---|---|---|
| W1.1 | W1 | Harbor, village and night ambience beds as data rows with hours | data | T1 | sonnet | `addons/LF_Everon1989/Sounds/Ambience/*`, `addons/LF_Everon1989/Configs/Atmosphere/LF_AmbientBeds_Everon1989.conf` | | test LF_Test_EveronAmbientBeds |
| W1.2 | W1 | Pigeons on the square, gulls on the dock (vanilla bird flocks in a kit) | visual | T1 | sonnet | `tools/town/kits/birds.json` | | proof look grid:slice-W1.2 |
| W1.3 | W1 | Square cast: two gossips and a gendarme on the evening bank beat, outfits and timetables | data | T1 | sonnet | `addons/LF_Everon1989/Configs/Residents/LF_Residents_Square*.conf`, `Configs/NpcOutfits/LF_Outfit_Gossip*.conf` | | test LF_Test_EveronResidentsSquare |
| W1.4 | W1 | Voice lines for the square cast (12 lines each) | data | T1 | sonnet | `addons/LF_Everon1989/Sounds/Voice/Square/*`, `tools/assets/dialogue/square.json` | W1.3 | test LF_Test_EveronDialogueSquare |
| W1.5 | W1 | US English sweep: a validator that fails French words on sign rows and texture-text specs | tooling | T0 | sonnet | `tools/validation/validate-us-english-signs.mjs` | | fixture tools/validation/fixtures/us-english/ |
| W1.6 | W1 | Town placement for W1 | town | T1 | sonnet | `MainWorld_Layers/Place_saint-philippe_square-life.layer` | W1.1, W1.2, W1.3 | proof look saint-philippe before and after |
| W2.1 | W2 | The newspaper item: mesh, carrier face, Read action | system | T2 | sonnet | `addons/LifeFramework/Scripts/Game/Feature/Signs/LF_Newspaper*.c`, `addons/LifeFramework/Prefabs/Items/Paper/LF_Newspaper.et` | | test LF_Test_NewspaperRead |
| W2.2 | W2 | The edition builder: weekly front page from crime reports, rewards, market prices and new businesses (server, words only) | system | T2 | opus | `Feature/Signs/LF_NewspaperEdition*.c`, `Configs/Signs/LF_NewspaperSections.conf` | W2.1 | test LF_Test_NewspaperEdition |
| W2.3 | W2 | Edition persistence as a save section | system | T2 | opus | `Feature/Signs/LF_NewspaperSavePayload.c` | W2.2 | test LF_Test_NewspaperPersist (PERSISTENCE), risk persistence |
| W2.4 | W2 | Newsagent sells the *Courier* and the road map; the road map marks towns heard of | capability | T2 | sonnet | `addons/LF_Everon1989/Configs/Shop/LF_ShopStock_Newsagent.conf`, `Feature/Map/LF_RoadMapItem.c` | W2.1 | test LF_Test_EveronNewsagent |
| W2.5 | W2 | Opening-hours plates generated from each shop's resident timetable | capability | T2 | sonnet | `Feature/Signs/LF_HoursPlate*.c` | | test LF_Test_HoursPlate |
| W2.6 | W2 | Weigh slip, payslip and delivery note paper items on the receipt asset | data | T1 | sonnet | `tools/assets/receipt-paper/*`, `addons/LifeFramework/Configs/Paper/LF_PaperKinds.conf` | | test LF_Test_PaperKinds |
| W2.7 | W2 | Town placement for W2 (stands, plates on Saint-Philippe doors) | town | T1 | sonnet | `MainWorld_Layers/Place_saint-philippe_print.layer` | W2.4, W2.5 | proof look saint-philippe |
| L1 | W3 | Spike: overlay quads on a UAZ and a UH-1H; findings doc (epic #4155) | tooling | T0 | opus | `docs/architecture/livery-spike.md`, `tmp/` | | proof look item LF_UAZ469_Police |
| L2 | W3 | `LF_LiveryDef` rows and the `livery build` CLI step that writes a variant prefab | tooling | T1 | sonnet | `tools/commands/livery.mjs`, `tools/livery/*` | L1 | fixture tools/livery/fixtures/ |
| L3 | W3 | Livery sign styles: block, stripe, stencil, mud | data | T1 | sonnet | `UI/Layouts/Signs/Styles/LF_SignStyle_Livery*.layout`, `Configs/Signs/LF_SignStyles_Livery.conf` | L1 | test LF_Test_SignStylesLivery |
| L4 | W3 | Police liveries: UAZ469 and S1203 | visual | T1 | sonnet | `addons/LF_Everon1989/Prefabs/Vehicles/Police/*` | L2, L3 | proof look item each |
| L5 | W3 | Ambulance liveries: M997 and S1203 | visual | T1 | sonnet | `addons/LF_Everon1989/Prefabs/Vehicles/Medic/*` | L2, L3 | proof look item each |
| L6 | W3 | Company liveries: taxi roof sign, bakery, hardware, timber, factory trucks | visual | T1 | sonnet | `addons/LF_Everon1989/Prefabs/Vehicles/Company/*` | L2, L3 | proof look item each |
| L7 | W3 | Rebel liveries: UAZ469 and Ural4320 | visual | T1 | sonnet | `addons/LF_Everon1989/Prefabs/Vehicles/Rebel/*` | L2, L3 | proof look item each |
| L8 | W3 | Heli liveries: civil, police, medevac on UH-1H and Mi-8MT | visual | T1 | sonnet | `addons/LF_Everon1989/Prefabs/Vehicles/Air/*` | L2, L3, #4142 | proof look item each |
| L9 | W3 | Mechanic respray and livery actions on the garage bay (money, owned vehicle) | capability | T2 | sonnet | `Feature/Vehicles/LF_LiveryShop*.c` | L2 | test LF_Test_LiveryShop, risk money |
| L10 | W3 | Applied livery survives a restart (vehicle record field) | system | T2 | opus | `Feature/Vehicles/LF_LiverySave*.c` | L9 | test LF_Test_LiveryPersist, risk persistence |
| W4.1 | W4 | `work` activity loop for generic workers (carry, hammer, sweep from vanilla gestures) | system | T2 | sonnet | `Feature/Residents/LF_ResidentWorkLoop.c` | | test LF_Test_ResidentWorkLoop |
| W4.2 | W4 | Dock cast: harbor master, two dockers, fishmonger, chandler | data | T1 | sonnet | `LF_Everon1989/Configs/Residents/LF_Residents_Dock.conf`, outfits | W4.1 | test LF_Test_EveronResidentsDock |
| W4.3 | W4 | Montignac cast: director, foreman, four line workers, cook | data | T1 | sonnet | `LF_Everon1989/Configs/Residents/LF_Residents_Montignac.conf`, outfits | W4.1 | test LF_Test_EveronResidentsMontignac |
| W4.4 | W4 | Mine and sawmill foremen with the "what's next" line from the job level | capability | T2 | sonnet | `Feature/Residents/LF_ResidentForeman.c`, `LF_Residents_Sites.conf` | | test LF_Test_ResidentForeman |
| W4.5 | W4 | Shop cast: baker, butcher, pharmacist, barber, newsagent, postmaster, bartenders | data | T1 | sonnet | `LF_Everon1989/Configs/Residents/LF_Residents_Shops.conf`, outfits | | test LF_Test_EveronResidentsShops |
| W4.6 | W4 | Tinted aprons and work jackets through the clothing pipeline (after Q2) | visual | T1 | sonnet | `addons/LF_Everon1989/Prefabs/Characters/Uniforms/LF/*` | | proof look item each |
| W4.7 | W4 | Voice lines for the W4 casts (batch per place) | data | T1 | sonnet | `LF_Everon1989/Sounds/Voice/Work/*` | W4.2, W4.3, W4.5 | test LF_Test_EveronDialogueWork |
| W4.8 | W4 | Front desk stand-ins: hospital receptionist and desk sergeant when nobody is on duty | capability | T2 | sonnet | `Feature/Residents/LF_ResidentDutyStandIn.c` | | test LF_Test_DutyStandIn |
| W4.9 | W4 | Town placement for W4 | town | T1 | sonnet | `Place_saint-philippe-dock_cast.layer`, `Place_montignac_cast.layer` | W4.2, W4.3, W4.5 | proof look each place |
| W5.1 | W5 | Rumor pool: rows tied to places, live state and memory word | system | T2 | sonnet | `Feature/Residents/LF_Rumor*.c`, `Configs/Residents/LF_Rumors.conf` | | test LF_Test_RumorPool |
| W5.2 | W5 | "Heard anything?" reply in resident Talk, one a day, marks the place heard of | capability | T2 | sonnet | `Feature/Residents/LF_ResidentTalkRumor.c` | W5.1 | test LF_Test_ResidentRumorTalk |
| W5.3 | W5 | "Sit and listen" on the gossip bench | capability | T2 | sonnet | `Feature/Residents/LF_GossipBench.c` | W5.1 | test LF_Test_GossipBench |
| W5.4 | W5 | 1989 pack rumor rows and their voice lines | data | T1 | sonnet | `LF_Everon1989/Configs/Residents/LF_Rumors_Everon1989.conf`, `Sounds/Voice/Rumors/*` | W5.1 | test LF_Test_EveronRumors |
| W6.1 | W6 | Island calendar: scene rows by game day and hour on the event shell | system | T2 | sonnet | `Feature/Events/LF_IslandCalendar*.c`, `Configs/Events/LF_IslandCalendar.conf` | | test LF_Test_IslandCalendar |
| W6.2 | W6 | Bells, siren and ship's horn: sounds and carriers | data | T1 | sonnet | `LF_Everon1989/Sounds/Calendar/*`, `LF_Everon1989/Prefabs/Atmosphere/LF_Bell*.et` | W6.1 | test LF_Test_EveronCalendarSounds |
| W6.3 | W6 | Market day: stalls up, crowd bed, the accordion player (Blender accordion) | visual | T1 | sonnet | `tools/assets/accordion/*`, `LF_Everon1989/Configs/Events/LF_MarketDay.conf` | W6.1 | proof look grid:slice-W6.3 |
| W6.4 | W6 | The Friday dance and the Sunday service (resident timetable overrides) | data | T1 | sonnet | `LF_Everon1989/Configs/Residents/LF_Residents_Calendar.conf` | W6.1 | test LF_Test_EveronCalendarResidents |
| W6.5 | W6 | Town placement for W6 | town | T1 | sonnet | `Place_saint-philippe_calendar.layer`, `Place_montignac_calendar.layer` | W6.2, W6.3 | proof look each place |
| W7.1 | W7 | Style bible: one palette and sign style per institution, checked by a validator | tooling | T0 | sonnet | `docs/everon/style-bible.md`, `tools/validation/validate-institution-styles.mjs` | | fixture tools/validation/fixtures/styles/ |
| W7.2 | W7 | Look pass per place, one card each: square, dock, Meaux, Montignac, the mine, the sawmill | town | T1 | sonnet | `Place_<place>_polish.layer` (one per card) | W7.1 | proof look <place> before and after; audit placement --strict |

That is 43 cards. A suggested parallel start, since nothing depends across these: W1.1, W1.2, W1.3,
W1.5, L1, W2.1, W2.5, W4.1, W4.4, W5.1, W6.1, W7.1.

## 9. Risks and open questions

**Risks.**

- **The live-resident cap.** 24 live residents per server, filled where players stand. Montignac at
  shift change plus the square on market day can exceed it. Mitigation: the director already
  prioritizes; generic workers count as half (they despawn first). Measure RAM on the 0grind box
  before W4 ships.
- **No AI walks on the island for most lines (#2996)** until each place kind has a slot. W4 casts
  depend on the town drafts for those places.
- **Render targets cost.** Every sign face is a render target. Liveries add 4 to 8 quads per vehicle.
  The visual-boards budget (`visual-boards-budget.md`) needs a livery row, and faces beyond
  60 m may need to stop updating.
- **Workbench never draws a render target**, so `look` proves placement, not lettering. Livery
  pictures need a full-profile session.
- **The newspaper can leak.** It reports server events, so it must never print what police have not
  made public (an open case, a hidden crime record) or a player's real name beyond their character.
- **Voice volume.** About 40 NPCs at 6 to 16 lines is about 450 lines. ElevenLabs credits are fine
  (use freely), but review time is the limit: listen in batches per place.
- **Copy-paste feel.** Generic workers repeating the same bark. Each bark row carries a cooldown
  per player and three variants.

**Open questions** (each with a recommendation).

- **Q1. Full repaints.** Overlays cover lettering and stripes. A white ambulance or a two-tone police
  car needs a texture on vanilla UVs, which counts as Bohemia's art under the clothing licensing rule.
  **Recommend overlays only for 0.3; decide repaints after the spike (L1).**
- **Q2. Tinted work clothes.** Aprons and work jackets come from the tint route, also Bohemia-derived.
  **Recommend yes, with a `LICENSES.md` row each**, because the base game has no work clothes.
- **Q3. The newspaper's reach.** Name players in print? **Recommend character names for public facts
  only** (arrests, rewards, business openings), never a hidden record or an open case.
- **Q4. Calendar length.** A 14-hour real week means market day comes once every 14 real hours, at a
  different clock time each week. **Recommend tying market day to the real Saturday session as well** (a server-time trigger),
  so the community night always has the market.
- **Q5. Gossip and crime tips.** Should rumors ever tip off crime targets (an unlocked car, a cash
  van's stop)? **Recommend yes, for `friendly` and better, from bar staff only**, as a reason for
  criminals to be known in town.
- **Q6. Dogs.** The base game has no animals. **Recommend sound-only dogs at kennels** rather than a
  mesh, which would need animation.

## Suggested additions (proposals for the maintainer)

These go beyond the wishlist. Each is a proposal, not a decision.

| What | Why it helps the whole | Rough size | Touches |
|---|---|---|---|
| **The *Everon Courier* as the server's memory** (W2): a weekly front page built from what players did | Makes every system's output visible and social; gives crime and police a public score without numbers on screen; a reason to visit the newsstand | 6 cards | crime, police, gangs, work |
| **Rumors as the discovery engine** (W5): residents and gossips mark places "heard of" | Turns talking to NPCs into a reward; ties the map epic to people instead of a list | 4 cards | work (job tips), crime (targets), police (informant) |
| **The island calendar** (W6): bells, shift siren, market day, the dance, Sunday | Gives a rhythm players learn and meet around; the world feels run, not static | 5 cards | work (payday, factory), crime (cash van at payday), police (patrols at shift change) |
| **The mechanic and the respray bay**: owned vehicles change color and wear earned liveries | A visible sink and a show-off reward; the payoff for profession mastery | 2 cards (L9, L10) | work (mastery), gangs (rebel livery) |
| **Front desk stand-ins** at the hospital and the station when no player is on duty | A new player is never met by an empty room; tells them how to get help | 1 card | police, medic |
| **Player notices on the town board** ($5, moderated) | Players advertise trades, taxi services and gang recruiting in the world, not in chat | 1 to 2 cards | gangs, work, moderation |
| **A local radio station**: two more period tracks, a pre-recorded weather and island news jingle every in-game hour | Cars and shops sound alive; the radio carries the calendar ("Market day tomorrow on the square") | 3 cards (sound, schedule, car radio hook) | work, events |
| **Weather reactions**: residents go indoors or open umbrellas in rain, the market packs up in storms | The island reacts to the base game's weather at almost no art cost | 2 cards | events |
| **Lost-and-found and found items**: a dropped wallet with an ID, handed to the gendarmerie for a small reward or kept (theft) | A moral choice in hour 1 that teaches the memory and police systems | 2 cards | police, crime |
| **Road map and town entry signs in US English** with distances | Players can navigate without the map screen; supports the no-pop-up rule | 2 cards (S10 rows plus placement) | work (routes), map |
| **The relocated memorial on Calvary Hill** (ahead of M8's anniversary): a bench, names, flowers players can lay | Story texture and a quiet place; anchors the premise | 1 card | story |
| **Tourism work** (the gap in the world bible): guiding a summer visitor NPC between two viewpoints for a tip | A gentle walking job for players without a car; uses the resident walk | 3 cards | work |

## Proof

- Each card's acceptance as listed, seen red first.
- `tools\cli look <place>` before and after each town card, with the polish checklist.
- A full-profile session for lettering (signs and liveries), voice and the calendar sounds, since
  Workbench draws no render target and plays no ambience.
- The first-two-hours review run again after W1 lands: a fresh character on the island, timing each
  step, noting any place where the player had to ask in chat.
