# Illicit chains

Milestone: M2 (the system and the easy chain), M5 (the other chains, as pack content)
Extends: [contraband](contraband.md), `Gathering`, `Processing`, [hideouts](../ownership/hideouts.md), investigation (witnesses and evidence)

## Player loop

A player runs an illegal production chain from raw goods to a buyer. Chains come in difficulty tiers. Every chain can be run solo, but the hard ones are very slow without other people. Each step is done by hand at a real place in the world: no loading bars and no menus.

## Decisions (maintainer, 2026-11-09)

- **Theme: period fitting.** 1989 black market, not named modern drugs.
- **Four difficulty levers, all used:** more steps, a skill or licence gate, capital and equipment, police heat.
- **Solo is possible; the hard tiers need many hands to be fast.** Slowness comes from physical work spread over several places, not from a timer.
- **Interactive and creative.** A step is a real action on a real object, in the world.
- **Police heat shows up as smell and smoke at sites and as searches at stops.** The other heat ideas (informants, patrol attention) are not in scope.
- **Split delivery.** The tier system, the hands-on stations and the easy chain are framework work in M2. The other chains are flagship pack content in M5, because the core never carries period content (see `docs/architecture/core-vs-pack.md`).

## The chains

| Tier | Chain | Where it lands |
| --- | --- | --- |
| Easy | Moonshine (samogon) | M2, reworked onto hands-on stations; the M1 contraband loop is its starting point |
| Medium | Tobacco smuggling | M5 pack |
| Hard | Pharmacy diversion: take medicine from the hospital or a pharmacy, repackage it, sell it on the street | M5 pack |
| Hard | Counterfeit: forged coupons, papers and cash | M5 pack |
| Hard | Poppy-straw "kompot": grow, harvest, cook, distribute | M5 pack; gated by a Bohemia Workshop policy check before any build |

## In scope

- **A chain is config.** A chain lists its steps, the station each step needs, the inputs and outputs, a skill or licence gate, an equipment cost, and its heat settings. A server owner can add a chain without script.
- **Hands-on stations.** A step happens at a physical station in the world. The first example is mixing at barrels: a site has several barrels, a person works one barrel at a time (a held action with an animation and sound), and several people can work different barrels at once. Progress belongs to the barrel, so a team finishes a batch much faster than one person. Other stations follow the same shape: a still to tend, a press to turn, a cooker to watch.
- **Tier gates.** A higher tier needs a skill level or a criminal licence earned on the lower tiers, plus bought equipment such as a machine or a bigger hideout.
- **Smell and smoke.** A running processing site gives off a trace (smell and smoke) that an officer or an NPC witness nearby can notice. The trace feeds the investigation system (witness statements and evidence).
- **Searches at stops.** Officers can search a player or vehicle for goods of a chain. A harder tier carries bulkier loads that are harder to hide.
- The server decides progress, gates and payouts. The client only asks.

## Out of scope

- Named modern drugs, and real-world chemistry steps.
- Informants and rising patrol attention.
- Turf wars, lab defence AI, and a second illegal job track beside organizations.
- Progress bars, menus, and timer-only steps.

## Assets

Stations are base-game props (barrels, stills, tables) with our interaction components. Animations and sounds come from the base game first, and the animation pipeline (#340) where a pose is missing. Smoke uses a base-game particle effect. Chain names, items and prices are pack config.

## First slice

The moonshine chain on one hideout site with three barrels. One player mixes a batch alone, slowly. Three players mix it in a third of the time. The site gives off smoke, an officer notices it from the road, and a search at a stop finds the goods.

## Depends on

- [Contraband](contraband.md), [organizations](../ownership/organizations.md), [hideouts](../ownership/hideouts.md)
- Investigation witnesses and evidence (M2, epic #148)
- `Gathering`, `Processing`, `Money`, `LF_ConfiscateAction`

## Proof

- Logic: progress is per station and adds across workers; a gate refuses a player without the licence; a search finds only chain goods.
- World: three players at three barrels on DebugWorld; smoke appears while the site runs.
- Persistence: a half-finished barrel survives a restart.
- Manual: the multiplayer run, and the officer noticing smoke.
