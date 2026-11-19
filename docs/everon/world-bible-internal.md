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
| Odile Marchand | The café owner who helps newcomers | Runs a quiet support network for the relocated (warm coats, a spare bed). Clean, and the moral centre of the arc |
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
| `shipment_go_between` | Hugo Valmy | A regular in the village pub | Speaks for the coast-road gang; buys or fights, never explains | Village pub b03 (the `cafe` place) | **New NPC, needs card S8** (below) |
| `shipment_officer` | Brigadier Colette Aubry | Gendarmerie | Wants the seizure for her record, not the culprit | Voice in the dispatch call text and the evidence slip; two AI gendarmes at the door | No NPC; text and AI crew only |
| `cafe_owner` | Odile Marchand | The barman's role: the pub is her café | Hears everything, shares it with the relocated | Pub counter | Exists. Rumour rows only |
| `old_man` | Gaspard Lenoir | Retired fisherman | Saw the coaster ride too low | Pub or square bench | Exists. Rumour rows only |
| `night_shift` | Rémi Duval | Night worker | Knows the markings from the factory yard and says nothing | Pub | Exists. Rumour rows only |

### Beats, one event, quest or dialogue row each

| Beat | Card | Id(s) | What it is |
|---|---|---|---|
| 1 The ship docks | S2 #3439 | event `shipment_docks`; phases `dawn`, `unload`; scene id `shipment_docks`; scene config `LF_Scene_ShipmentDocks.conf`; precondition module `shipment_docks_dawn` (hour window, once per game week); scene operation module `shipment_docks_scene`; crate spot `scene-crate`; crate prefab chosen by S2 | Truck parks on the quay (#3074), two dockers walk crates to the warehouse, Marcel stands by, the red crate is left at `scene-crate`. Pack file `LF_Events_Everon1989.conf` (shared with S5: S5 rebases on S2) |
| 2 A crate is opened | S3 #3440 | quests `shipment_count` (giver `harbour_master`, gate `contract:harbour_crates`, the existing TOWN contract gate, no new gate row), `shipment_report` (giver: the police desk, S3 confirms the giver id), `shipment_sold`; item `shipment_sample`; dialogue id `shipment_harbour_master` (file `LF_Dialogue_ShipmentHarbourMaster.conf`) with rows `shipment.ask`, `shipment.count`, `shipment.thanks_report`, `shipment.thanks_sell`; reuses the existing giver-table overlay (`LF_QuestGiverTable_Everon1989.conf`, no edit to Marcel's existing conf) | Marcel asks a trusted player to count the red crate. Collecting `shipment_sample` is the opening. `shipment_report` is turned in at the desk; `shipment_sold` is closed by the buyer paying for it. Both write the state the other cards read |
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
