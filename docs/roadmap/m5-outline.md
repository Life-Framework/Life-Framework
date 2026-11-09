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

## Moved out of M5

- **Property, renting, the dacha, and farming** go to a new milestone, **M6
  Homes and Land** (milestone 7 on GitHub), so M5 stays small and shippable.
- **The Zone** (the Chernobyl-like PvP area with gas masks) is parked in the
  idea backlog. Revisit after M5 playtests; it ties to the "one marked lawless
  area" decision and M2 events.

## Rules

- Flagship content only: everything lives in `LF_Everon1989`, and the core never
  references it (`validate-addon-boundary`).
- No new core system without a framework reason; arrival, food and factory use
  jobs, shops, dialogue and config that already exist or ship in M2/M3.
- Period copy and art follow the pack's branding brief (#375).

## Open questions

- How long the arrival arc is, and whether it replays for a new character.
- Which key NPCs get voices beyond the clerk.
- Which AI music tool, and whether its licence permits redistribution.
- Whether food seasons are real-time or tied to the day and night cycle (#129).
- How the factory wage scales so it does not outpay crime-and-trade play.
