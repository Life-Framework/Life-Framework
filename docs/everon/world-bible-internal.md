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
