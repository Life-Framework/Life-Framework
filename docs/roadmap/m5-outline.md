# M5 Settling in (outline)

M5 is the first update after the 1989 Everon flagship goes live (M4). In its
first weeks every player is new, so M5 deepens the first ten hours: arriving,
getting kit, the evenings, policing the roads, and the first part of the
relocation story. The issue set was finalised on 2026-11-15. Read
it with the [roadmap](README.md), [direction](../direction.md) and the
[M4 outline](m4-outline.md).

**Theme.** You were relocated from the Chernobyl area and are starting life
again on Everon in 1989. Flagship content is built on framework systems, and
the core stays era-neutral.

## Chapters

Each chapter is one player story and one 60 to 120 second video; its beat
sheet is the acceptance test (rules in [README.md](README.md)). Chapters ship
one at a time to the live server as each passes its playtest, in this order:

| Order | Chapter | Framework (core) | Flagship (1989 pack) |
| --- | --- | --- | --- |
| 1 | Armed and dressed #1009 | Catalogue and outlets #988 (stock #1064, outlets #1065, armory #1066, search and seizure #1067, conceal flags #1068, licence desk #1069), store previews #583 | Weapon split #989, clothing and equipment #990, police career #797, vanilla period assets #981 |
| 2 | Evenings in the village #1011 | Emotes and sitting #982, drunk effect #636, race runner #790 | Chess, range and hall #802, drunk effect #637, dirt race #791 |
| 3 | On patrol #1014 | Spike strips, cones and barriers #646, placement preview #928, breathalyser #634, board answers #758, nearest named place #760 | Police kit #647, breathalyser #635, an alarm per robbery type #929 |
| 4 | Arrival #1007 | Onboarding chain #794, rejoin #640, handbook #642 | Arrival chain #795, rejoin #641, handbook #643 |
| 5 | Story: The relocated #1015 | Event shell #757, event scenes #1754 | Bus at dawn #1755, the clerk's welcome #1756, townspeople #1757, the hostel #1758, radio music #1759 |

Why this order: Armed and dressed is mostly built (the catalogue and clothing
are merged and in playtest). Evenings comes before On patrol because the
breathalyser needs the drunk effect. The story chapter needs the event shell,
so it comes last and carries the update's trailer.

## Foundation

Plumbing that ships with M5 but has no video of its own:

- Placement #927: the service, aimed ghost, probes, rules and compositions.
  #1032 to #1034 are children for M6 and M8.
- 3D asset pipeline #339: licence gate #664, then the Meshy, Blender and
  Reforger stages and the one-command harness (#665 to #669). It produces the
  arrival and factory props #670 and #671.
- Voice pipeline #343: already voices the Everon clerk; the story chapter uses
  it.
- ArchBuilder harness #995: buildings from the maintainer's ArchBuilder add-on.
  Harness only; its code is never copied.

The animation, weapon and vehicle pipelines (#340, #341, #342) moved to
Development on 2026-11-15, because no M5 chapter needs them.

## Decisions (maintainer)

- **Story scope (2026-11-15).** The relocated is voiced (the clerk and three
  townspeople) and has period radio music. Each music track gets a licence and
  credit record before it ships; if the tool's terms do not allow Workshop
  redistribution, stop and ask.
- **Firearm permit (2026-11-15).** A fee and a clean record (no active
  warrant, no recent conviction); issued after one in-game day; police can
  revoke it (#1069).
- **Car upkeep stays light (2026-11-09).** Fuel and simple repairs only, with
  no parts system and no random breakdowns (repair kit #648 is in M6).
- **Base-game assets first.** A new mesh only comes through the 3D harness,
  with a ledger row.

## Moved out of M5

- The bus (#203, the AI route driver #772, the Everon line #1101) is its own
  M6 chapter, Catch the bus #1793 (maintainer, 2026-11-15): AI driving is the
  riskiest piece, so M5 does not wait for it. The story's dawn bus is parked
  until then.
- The town paper is removed from the roadmap (maintainer, 2026-11-15; #1008,
  #792 and #793 closed).
- Food, the state factory and the market go to M6 Making a living
  (story chapter #1021, The market moves #1016).
- Property, the dacha and farming go to M8 Roots (#747, #1037).
- The Genetic Life review ideas are spread over M6 and M7 by theme (bank
  transfer, repair kit, tools, medic ranks, routes in M6; laundering, prison
  work, heist in M7).
- The Zone (the Chernobyl-like PvP area) is parked until after the M5
  playtests.
- The source idea epics #214 and #218 are closed; every item has a home
  (see the closing comment on #214).

## Rules

- Flagship content lives in `LF_Everon1989`, and the core never references it
  (`validate-addon-boundary`). A new core system has a Framework issue and a
  Flagship issue that uses it.
- Period copy and art follow the pack's branding brief (#375).
- Slices are cut per chapter with the roadmap pipeline
  (`docs/process/roadmap-pipeline.md`).

## Open questions

- Whether Meshy's plan terms allow Workshop redistribution (#664).
- Whether a new character's spawn can be a hostel bed without a core change
  (#1758).
