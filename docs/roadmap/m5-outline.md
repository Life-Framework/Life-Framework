# M5 Flagship Story (outline)

M5 gives the 1989 Everon flagship its own identity so it stands out and draws
new players. The maintainer set the shape on 2026-11-09 in an interview. This
file is an outline: no sub-issues exist yet, and each workstream gets scope
cards after M4 is live. The source ideas are in epic #214. Read it with
[direction](../direction.md), [decisions](../decisions.md), and the
[M4 outline](m4-outline.md).

**Theme.** You were relocated from the Chernobyl area and are starting life
again on Everon in 1989.

**What M5 is for.** Attracting new players (maintainer, 2026-11-09): a
distinct identity in the server browser, in trailers, and in a first session.
It is flagship content built on framework systems; the core stays era-neutral.

**The hook.** Player drama. Crime, police and the economy produce the stories
players tell; the arrival and the world frame them. M5 builds texture and a
strong first hour, not a quest campaign.

**Story shape.** A light guided arc: a short scripted arrival (papers, the
clerk, a starting room, the first week) that hands off to the open sandbox. No
ongoing quest lines and no quest-state tooling.

## First release

1. **Arrival and music.**
   - The arrival frames the player as a relocated citizen: relocation papers,
     the clerk, a starting room (#214 item 1).
   - Soviet-era style music from radios, cafes and the square. Source: AI
     generated in period style (maintainer's choice). Before building, record
     the tool's licence terms for redistribution in the APL Workshop build, and
     keep a per-track record; if the terms do not allow it, stop and ask
     (#214 item 2).
   - Voiced NPCs for the arrival clerk and a few key recurring characters only
     (#218); small line count, re-record only when copy changes.
2. **Food and the state factory.**
   - Real food items and shops: bread, potatoes, cabbage, beetroot, meat,
     coffee, with simple seasonal availability (#214 item 5).
   - The state factory as a steady wage for a real work loop at stations, not a
     timer, and no passive paycheck (#214 item 4; the decision stands).
3. **Getting around.**
   - Bus stops (#203): travel between known stations by fare.
   - Car upkeep stays light (maintainer, 2026-11-09): fuel and simple repairs
     only. It is not a focus, because upkeep is boring and adds friction. No
     parts system and no random breakdowns.
   - Trash cans (#205) ride along, since they are small and already scoped.

4. **Illicit chains (flagship content).** The M2 Underworld epic (#410) builds
   the tiered-chain system, hands-on stations, smoke and searches, and the easy
   moonshine chain. M5 adds the rest as pack config (maintainer, 2026-11-09):
   tobacco smuggling (medium), pharmacy diversion, counterfeit papers and cash
   (hard), and poppy-straw "kompot" (hard). Poppy kompot needs a Bohemia
   Workshop policy check before anything is built. Details:
   [illicit chains](scopes/crime/drug-chains.md).

## Two tracks (maintainer, 2026-11-10)

M5 is built in two tracks, and every roadmap item from M5 on is broken down the
same way:

- **Framework** (epic #632): the underlying systems in the core, era-neutral and
  configurable.
- **Flagship** (epic #633): the specific 1989 Everon content in
  `LF_Everon1989`, built on the Framework track.

The first-release items above sit on one side or the other: the arrival,
music, voiced NPCs, food, the factory wage and the chain content are Flagship;
the systems they need (stations, dialogue, route jobs) are Framework.

## 3D development (maintainer, 2026-11-10)

All 3D asset work is in M5 (epic #339, moved from Development). The harness
chains three tools into one nearly autonomous pipeline:

1. **Meshy** ([meshy.ai/mcp](https://www.meshy.ai/mcp)) generates a model from a
   brief.
2. **Blender** (MCP or CLI, headless) cleans, scales, retopologises, UVs, bakes
   and exports it.
3. **Reforger** import registers the files and builds the prefab.

Agents verify and re-verify both visually (turntable, then in-game shots held,
on the ground and in the inventory) and otherwise (budgets, scale, GUIDs, log).
One command (`toolscli asset new`) runs it all, with a cost cap, a retry
limit and an asset ledger. The licence gate comes first: Meshy's terms and
Bohemia's policy must allow Workshop redistribution, or we stop and ask.

| Framework | Flagship |
| --- | --- |
| #339 epic; licence gate #664, Meshy stage #665, Blender stage #666, Reforger stage #667, verification loop #668, one-command harness and ledger #669 | #670 first items (shovel, pickaxe, keys, previews), #671 arrival and factory props |

Base-game assets are still the first choice. From M5, a new mesh is allowed
only through this harness, with a ledger row.

## Ideas from the Genetic Life review (2026-11-10)

A list of another Reforger life mod's features was sorted into what fits. The
fits all land in M5, each as a Framework and a Flagship issue:

| Idea | Framework | Flagship |
| --- | --- | --- |
| Breathalyser and drug-test kit | #634 | #635 |
| Timed drunk effect | #636 | #637 |
| Bank transfer by account number | #638 | #639 |
| Rejoin where you logged out | #640 | #641 |
| In-game handbook | #642 | #643 |
| Money laundering station | #644 | #645 |
| Spike strips, cones, barriers | #646 | #647 |
| Repair kit and unflip | #648 | #649 |
| Pickaxe and shovel gate gathering | #650 | #651 |
| Medic ranks | #652 | #653 |
| Staff terminal | #654 | #655 |
| Prison work | #656 | #657 |
| Bus and garbage routes | #658 | #659 |
| Era clothing and fedora | none | #660 |
| Multi-stage heist (idea only) | #662 | #663 |

Several depend on M2 work (#419, #420, #416, #417, #246, #258). Not a fit:
custom weapons and vehicles, a custom map, a minimap, dirty-cash balances,
hard drugs, tasers, speed cameras, air and boat dealers.

## Moved out of M5

- **Property, renting, the dacha, and farming** go to a new milestone, **M6
  Homes and Land** (milestone 7 on GitHub), so M5 stays small and shippable.
- **The Zone** (the Chernobyl-like PvP area with gas masks) is parked in the
  idea backlog. Revisit after M5 playtests; it ties to the "one marked lawless
  area" decision and M2 events.

## Rules

- Flagship content lives in `LF_Everon1989`, and the core never references it
  (`validate-addon-boundary`). Framework items are core systems with a reason
  beyond one era.
- A new core system needs a Framework issue and a Flagship issue that uses it.
- Period copy and art follow the pack's branding brief (#375).

## Open questions

- Whether Meshy's plan terms allow Workshop redistribution (#664).
- How long the arrival arc is, and whether it replays for a new character.
- Which key NPCs get voices beyond the clerk.
- Which AI music tool, and whether its licence permits redistribution.
- Whether food seasons are real-time or tied to the day and night cycle (#129).
- How the factory wage scales so it does not outpay crime-and-trade play.
