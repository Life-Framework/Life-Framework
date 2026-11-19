# World of Everon: internal notes

**Spoilers.** Story twists, secret locations and the cast not yet met. This file is not rendered on the
website. The repo is public, so "internal" means "not promoted", not secret: anyone can read it on GitHub
(open question 1 below).

The public part is [world-bible.md](world-bible.md). Everything here is a proposal until the maintainer
approves it. Nothing in this file is canon yet.

## The story arc

| Milestone | Story chapter | Public pitch | What it hides |
|---|---|---|---|
| M5 | The relocated (#1015) | Families from the Zone walk into town at dawn and start again | Form R-89 has a second copy. The clerk files it with someone other than the town hall |
| M6 | The state factory | Work arrives with the factory's new contract | The contract is a cover. The factory takes in sealed drums at night, and Rémi's night shift unloads them |
| M7 | The shipment | Smuggling comes to the coast | The drums are the shipment: material out of the Zone, sold through Everon. The gangs fight over the route without knowing what it is |
| M8 | The anniversary | The relocated mark three years since the evacuation | The town learns what the factory stored. The relocated choose: stay and build, or expose it and lose the island's goodwill |

**Why this arc.**

- It stays inside the existing premise: the Zone, three years ago, the relocated.
- It needs no new world assets beyond the story props the pipeline already makes.
- It joins the M6 factory to the M7 shipment, so the story leads straight into the turf war.

## Secret locations

| Place | Secret | Opens in |
|---|---|---|
| The second filing cabinet (behind the town hall, or in the police archive) | The R-89 copies, a list with every relocated name | M6 story beat |
| The factory's back yard | Where the sealed drums are stacked. Night only. Rémi's key opens it | M6 |
| A cove dock (Lancre, Le Bosc or West Cape, whichever the scout finds most hidden) | Where the drums leave by boat | M7 |
| Andre's Beacon | The lamp signals a landing. Players who learn the pattern know when a shipment runs | M7 |
| The airbase or military harbour | Where the shipment is bought | M7 heist or convoy end point |
| The Zone (parked) | If it returns, it is the source of the drums. The story gives it a reason to exist on the map | After the M5 playtests, maintainer's call |

## The cast: who they really are

| Person | Public | Hidden |
|---|---|---|
| The clerk | The kind face of the town hall | Files the second copy of R-89 under orders, and is uneasy about it. Becomes an informant in M7 if players earn the clerk's trust (a reputation gate) |
| Odilon Marchand | The café owner who helps newcomers | Runs a quiet support network for the relocated (warm coats, a spare bed). Clean, and the moral centre of the arc |
| Gaspard Lenoir | The retired fisherman | Knows the coves. Recognises the drums' markings from the evacuation. The first to talk in M7 |
| Rémi Duval | The night-shift worker | Unloads the drums in M6. Keeps his old house key because he means to go back. Can be turned either way in M7 |
| Vince | The fence | Buys salvage from the military sites and knows who sells the drums. Never says it outright |

## The proposed new cast

These are roles only. Names and voices follow the voice pipeline's licence rules
(`docs/architecture/npc-dialogue.md`).

| Role | Place | Milestone | Job in the arc |
|---|---|---|---|
| The factory director | The work town | M6 | Hires players; the face of the contract |
| The harbour master | The hub dock or Saint-Pierre | M6 | Gives dock and fishing work; sees the night boats |
| The landlord | The hub | M8 (M6 if reputation lands) | Rents rooms; the first user of reputation |
| A rival gang's go-between | A coast village bar | M7 | NPC voice of the gang that holds the coast road. Players can deal with them or fight them |
| The islander who resents the newcomers | A village bar | M5 or M6 | The social tension in one person. Turns out to have lost his job to the factory contract |

## Story beats by map state

| State | Beat | Place |
|---|---|---|
| 0 | Dawn arrival; the clerk; the hostel | First location |
| 1 | Rémi mentions the night shift; Gaspard watches the sea | First location by night |
| 2 | The factory hires; the drums are first seen | Work town |
| 3 | The lighthouse signal; the cove landing; the shipment heist | Coast and military sites |
| 4 | The anniversary on the square; the choice | The hub |

## Arc 3: The shipment (M7, #1036, story sheet #3438)

The public pitch is `story/the-shipment.md`. This section is the twist, the cast notes and every id the content
cards (#3439 to #3445) must agree on. It uses existing systems only: dialogue, quests, the event shell,
residents, standing and the buyer rotation. Rumours are residents' dialogue rows, never a new system.

**Who ships what, and why.** A mainland coaster ties up at the Saint-Philippe quay once a game week at dawn. The
manifest says tinned fish for the harbour co-op and Marcel signed it. One crate (the red one on its own pallet,
scene id `scene-crate`) carries a false bottom: under the tinned fish sits a **sample canister**, one small
sealed steel drum of the same material as the factory's night drums. The shipper sends a sample ahead so a buyer
can test it before the full lot moves. Marcel only knows the crate weighs 11 kg over the manifest.

**Who wants it.** Hugo Valmy, go-between for the gang that holds the coast road, wants the sample to reach his
buyer. The gendarmerie wants a seizure to show. The town wants the dock work to carry on. Nobody knows what
the material is (the gangs never do; that is arc 4's reveal).

**The player's choice, and what it moves.**

| Choice | Done by | Standing | Next |
|---|---|---|---|
| Report it | Hand the canister in at the police desk | TOWN up, POLICE up, UNDERWORLD down | A raid on the warehouse at the next dusk (beat 4) |
| Sell it | Sell it to a `LF_BuyerSpot` buyer (Hugo's) | UNDERWORLD up, TOWN down a little | No raid; the crate is empty when the foreman next looks |
| Walk away | Do nothing within the shipment's week | None | The crew collects it that night; no rumours, no raid; next week's ship runs as normal |

The red crate's contents are one item (`shipment_sample`, an existing carriable prefab the card picks; S3 records
its GUID here when chosen). Taking it out of the crate is "the crate is opened".

### Cast (new names are proposals, like the rest of this file)

| Id | Who | Public | Hidden | Where | Status |
|---|---|---|---|---|---|
| `harbour_master` | Marcel Roux | Gives dock work | Signed a manifest he suspects; wants someone he trusts to count. Does not know what the cargo is | Harbour cabin, dock b14 (placed, #3055) | Exists. Dialogue `harbour_master` (`LF_Dialogue_HarbourMaster.conf`) stays untouched |
| `shipment_foreman` | Thierry Vasseur | The dock foreman | Paid by Hugo to look away from the red crate | The quay and warehouse, as the scene's lead docker | Scene crew only (no dialogue, no resident row); appears in rumours and the quest text |
| `shipment_go_between` | Hugo Valmy | A regular in the village pub | Speaks for the coast-road gang; buys or fights, never explains | Village pub b03 (the `cafe` place) | NPC, dialogue `shipment_go_between` and resident row `shipment_go_between` (S8, #3804; voiced in #3809) |
| `shipment_officer` | Brigadier Colette Aubry | Gendarmerie | Wants the seizure for her record, not the culprit | Voice in the dispatch call text and the evidence slip; two AI gendarmes at the door | No NPC; text and AI crew only |
| `cafe_owner` | Odilon Marchand | The barman's role: the pub is his café | Hears everything, shares it with the relocated | Pub counter | Exists. Rumour rows only |
| `old_man` | Gaspard Lenoir | Retired fisherman | Saw the coaster ride too low | Pub or square bench | Exists. Rumour rows only |
| `night_shift` | Rémi Duval | Night worker | Knows the markings from the factory yard and says nothing | Pub | Exists. Rumour rows only |

### Beats, one event, quest or dialogue row each

| Beat | Card | Id(s) | What it is |
|---|---|---|---|
| 1 The ship docks | S2 #3439 | event `shipment_docks`; phases `dawn`, `unload`; scene id `shipment_docks`; scene config `LF_Scene_ShipmentDocks.conf`; precondition module `shipment_docks_dawn` (hour window, once per game week); scene operation module `shipment_docks_scene`; crate spot `scene-crate`; crate prefab chosen by S2 | Truck parks on the quay (#3074), two dockers walk crates to the warehouse, Marcel stands by, the red crate is left at `scene-crate`. Pack file `LF_Events_Everon1989.conf` (shared with S5: S5 rebases on S2) |
| 2 A crate is opened | S3 #3440 | quests `shipment_count` (giver `harbour_master`, gate `contract:harbour_crates`, the existing TOWN contract gate, no new gate row), `shipment_report` (giver: the police desk, S3 confirms the giver id), `shipment_sold`; item `shipment_sample`; a patch of the `harbour_master` dialogue (file `LF_Dialogue_ShipmentHarbourMaster.conf`, #3768 made it a patch: the greeting is full at four replies) with rows `shipment.ask`, `shipment.count`, `shipment.thanks_report`, `shipment.thanks_sell`; reuses the existing giver-table overlay (`LF_QuestGiverTable_Everon1989.conf`, no edit to Marcel's existing conf) | Marcel asks a trusted player to count the red crate. Collecting `shipment_sample` is the opening. `shipment_report` is turned in at the desk; `shipment_sold` is closed by the buyer paying for it. Both write the state the other cards read |
| 3 Rumours in the pub | S4 #3441 | rumour rows `rumour.shipment.reported` and `rumour.shipment.sold` for residents `cafe_owner`, `old_man`, `night_shift` (and the neighbours if S4 wants); file `LF_Rumours_Shipment.conf`; each row gated on the quest ids above and fading after 3 game days | Same rows, different line for a reporter and a seller. No new resident rows |
| 4 The raid | S5 #3443 | event `shipment_raid`; phases `dispatch`, `door`; scene id `shipment_raid`; scene config `LF_Scene_ShipmentRaid.conf`; precondition `shipment_raid_after_report` (reads `shipment_report` done; dusk); service call `police` at the dock warehouse; evidence row for `shipment_sample` | A police call for officers on duty, two AI gendarmes at the door, the crate seized into the evidence locker unless it already left (the sold branch has no raid) |
| Voices | S6 #3444 | folder `Sounds/Dialogue/Shipment/`; sources `tools/assets/dialogue/<npc>/dialogue.json` for `harbour_master` (shipment rows) and `shipment_go_between` | Lines for beat 2 and the go-between voiced through `asset dialogue`. The foreman and officer have no voiced lines |
| Warehouse | S7 #3445 | place `saint-philippe-dock.warehouse`, layer `Place_saint-philippe-dock_warehouse.layer`, claim `tools/town/claims/saint-philippe-dock-warehouse.json` | Already merged (the `warehouse` slot, dock survey b06, with the red crate at `scene-crate`). S2 and S5 read its markers |

### Places the arc uses

| Place | Id or layer | Used by |
|---|---|---|
| Dock warehouse | `saint-philippe-dock.warehouse`; marker kind `Warehouse` | Beat 1, crate spot `scene-crate`; beat 4 door |
| Harbour cabin | resident place `harbour_master`, dock b14 | Beat 2 giver |
| Village pub (the café) | the `cafe` place, `Place_saint-philippe_cafe.layer`, kind `CafeTable`, b03 | Beat 3 rumours, Hugo |
| Police desk | police villa b32 (`police` slot) | Report turn-in, evidence locker, raid call |
| The buyer | `LF_BuyerSpot` markers (three; one at dock b22, one at the districts back lane b21, one at the village-edge barn b64) rotated by `LF_BuyerRotation` | Sell branch |

### Cards still needed

- **S8 (new): Hugo Valmy, the go-between.** A pack NPC prefab, a dialogue conf `LF_Dialogue_ShipmentGoBetween.conf`
  (voice follows S6), a resident row and a pub anchor. No card owns this yet. S3 to S6 must not wait on it:
  beats 1 to 4 run without him. His lines add the sell branch's intro and one threat if the player reports.
- Standing: all awards come from existing `LF_ReputationSubscriber` rows; a new award row for `shipment_report`
  and `shipment_sold` lives in `LF_Reputation_Everon1989.conf`, which no card owns. S3 may add two rows there
  (about 10 lines) and says so in its PR.

### Joins to other arcs

- M6 (the factory): the sample's markings match the night drums; Rémi recognises them.
- M8 (the anniversary): which way the town leaned (the `TOWN` and `UNDERWORLD` standing at the anniversary) is
  the shipment's lasting effect.

## Arc 4: The anniversary (M8, #1039, story sheet #3462)

The public pitch is `story/the-anniversary.md`. This section is the twist, the cast notes and every id the content
cards must agree on. Card numbers are not cut yet; each row says what a card would own. It uses existing systems
only: dialogue, quests, the event shell, residents, standing and rumour rows (source ideas #214; residents #991
carry the town). No new system.

**What the day remembers.** Three years ago the Zone was evacuated in a night. The relocated count it from the
arrival at dawn; the town counts it from the day the first families walked in. The mayor, the priest and the widow
have each told it differently since.

**The letter.** Dominique Bouvier, the widow's husband, was on the clean-up crew that logged the Zone's drums
before the evacuation. He wrote to the mayor three weeks before he died, naming the plant's contractor and the
markings on the drums, and asking that the relocated be told what the island's new factory would store. The
mayor kept the letter and signed the factory contract anyway (the contract is a cover, arc 2). The widow found
the carbon copy in his coat after the evacuation and gave it to the priest for safekeeping. The priest brings it
to the memorial.

**Who knows what.**

| Id | Who | Public | Hidden | Where | Status |
|---|---|---|---|---|---|
| `mayor` | Bastien Girard | Runs the town hall; the day's host | Held Dominique's letter back and signed the factory contract. Wants it read as a thank-you, not a reckoning | Town hall steps, then the square | **New NPC** (proposal) |
| `priest` | Father Étienne Morel | Leads the walk and the blessing | Holds the carbon copy of the letter; will read it only if a player asks him to | Church b07, then the square | **New NPC** (proposal) |
| `widow` | Hélène Bouvier | Reads the names | Wants the letter read; does not know its contents, only that it exists | Square, by the candles | **New NPC** (proposal) |
| `clerk` | The clerk | The town hall's face | Filed the second copy of R-89; stands at the back and will not meet the widow's eye | Square crowd | Exists (`LF_Dialogue_ClerkEveron.conf`), rumour rows only |
| `cafe_owner` | Odilon Marchand | The café owner | Brings the coats and the soup; sides with the relocated | Square | Exists, rumour rows only |
| `old_man` | Gaspard Lenoir | The retired fisherman | Recognises Dominique's drum markings when the letter is read | Square | Exists, rumour rows only |
| `night_shift` | Rémi Duval | The night worker | Learns what he has been unloading, if the letter is read | Square, apart from the crowd | Exists, rumour rows only |

New names are proposals, like the rest of this file. Voices follow `docs/architecture/npc-dialogue.md`.

**The player's choice, and what it moves.**

| Choice | Done by | Standing | Next |
|---|---|---|---|
| Read the letter | Ask the priest to read it aloud (`anniversary_letter_read`) | TOWN down, POLICE up, UNDERWORLD up a little (the gangs hear first) | The town learns what the factory stored; the island's goodwill drops; the relocated keep their names clean |
| Hold the letter | Give it to the mayor, or let the walk end without it (`anniversary_letter_held`) | TOWN up, POLICE down | The town is told the day was a thank-you; the clerk's second copy stays hidden; arc 4 closes quietly |
| Burn it | Burn it in the candle tray at the widow's feet | TOWN up a little, UNDERWORLD down a little | The widow knows; the letter is gone; the priest's carbon copy is the only record (a later hook, not this card) |

The shipment's lasting effect decides the room. If the player's TOWN standing from the shipment is high, the
mayor listens; if UNDERWORLD is high, a gang runner is in the crowd and the letter's reading becomes an incident
(an event escalation, not a new system).

### Beats, one event, quest or dialogue row each

| Beat | Id(s) | What it is |
|---|---|---|
| 1 Preparations | event `anniversary` phase `prepare`; quests `anniversary_chairs` (giver `mayor`, carry chairs to the square), `anniversary_candles` (giver `priest`, buy candles at the general store), `anniversary_names` (giver `widow`, fetch the list from the town hall registry); dialogue ids `anniversary_mayor`, `anniversary_priest`, `anniversary_widow`; precondition module `anniversary_week` (a fixed game date, once) | The square fills up; three requests, any order. Files: `LF_Events_Everon1989/LF_Events_Anniversary.conf`, `LF_Dialogue_Anniversary*.conf` |
| 2 The memorial | phase `walk`; scene id `anniversary_walk` (walkers leave the church and cross to the square); scene config `LF_Scene_AnniversaryWalk.conf`; scene operation module `anniversary_walk_scene`; markers kind `Church` and the `square-evening` place | The town walks at dusk; the priest and the mayor speak; the widow lays the first candle. A player who carried a candle or a chair is named in the crowd's talk (state from beat 1) |
| 3 A letter from the past | phase `letter`; quests `anniversary_letter_read`, `anniversary_letter_held`; item `anniversary_letter` (an existing paper prefab the card picks); dialogue rows `anniversary.letter.ask`, `anniversary.letter.read`, `anniversary.letter.hold` | The priest holds the letter; the player decides (table above). Both quests write the state beat 4 reads |
| 4 The town gathers | phase `gather`; rumour rows `rumour.anniversary.read`, `rumour.anniversary.held` for residents `cafe_owner`, `old_man`, `night_shift` and the neighbours; file `LF_Rumours_Anniversary.conf`; each row gated on the quest ids above and fading after 5 game days | The same rows, a different line for each branch. One standing award row for each branch in `LF_Reputation_Everon1989.conf` (about 10 lines; the card says so in its PR) |

### Places the arc uses

| Place | Id or layer | Used by |
|---|---|---|
| Church | slot `church`, b07, `Place_saint-philippe_church.layer`, kind `Church` | Beat 2 start; the priest's resident place |
| The square | slot `square-evening`, `Place_saint-philippe_square-evening.layer` | Beats 1 to 4; the candle tray (a placed prop) |
| Town hall | the `clerk` place (registry desk) | Beat 1, `anniversary_names` |
| General store | the `store_clerk` place | Beat 1, `anniversary_candles` |
| Cemetery | Calvary Hill (world bible, "Calvary Hill, the cemetery") | Optional epilogue; not part of the four beats |

### Cards still needed

- **A new NPC card for the mayor, the priest and the widow.** Pack NPC prefabs, dialogue confs, resident rows and
  anchors. No card owns this yet. Beats 1 to 4 can run with the existing residents until it lands, using the
  clerk's voice for the mayor's lines.
- **Voices.** Folder `Sounds/Dialogue/Anniversary/`; sources `tools/assets/dialogue/<npc>/dialogue.json`.
- **The candle tray.** A placed prop in the square cell (or the town pass); `tools\cli place` only.

### Joins to other arcs

- Arc 1 (the relocated): the day marks the first arrival; the clerk's second copy of R-89 is the quiet proof that
  the town knew who the relocated were before they arrived.
- Arc 2 (the factory): the contract is a cover; the letter names the drums the factory takes in.
- Arc 3 (the shipment): the shipment's TOWN and UNDERWORLD standing decides who is in the crowd and how the
  mayor answers.

## Spoiler terms

The website check (`tools\cli validate`, `validate-public-bible`) fails when the public page, a place dossier's
picture caption or the generated "Live now" section contains one of these, case-insensitive. One bullet per term;
pick phrases that appear only here, and add a term when a new twist is written down above.

- sealed drums
- second copy
- second filing cabinet
- drums' markings
- the contract is a cover
- informant
- keeps his old house key
- night boats
- sample canister
- false bottom
- shipment_sample
- Hugo Valmy
- Thierry Vasseur
- Colette Aubry
- Dominique Bouvier
- Bastien Girard
- Étienne Morel
- Hélène Bouvier
- anniversary_letter
- held the letter back

## Open questions (internal)

1. **The repo is public.** Anyone can read this file on GitHub. Recommend keeping the arc's shape here and
   holding each chapter's final twist in the chapter issue's private notes (or the maintainer's head)
   until it ships.
2. **The arc itself.** Approve the drums arc, or ask for another. Recommend approving it: it reuses every
   existing character and leads into M7.
3. **The clerk's name.** Today the clerk has no name, only a voice. Recommend giving one in M6, when the
   clerk becomes a story character.
4. **The Zone.** If it stays parked, the drums still work as material brought over from the mainland.
   Recommend not making the arc depend on the Zone being on the map.
