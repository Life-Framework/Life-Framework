# Roadmap

The gameplay plan for the base mod. Two axes:

- A **phase** is when a slice is supposed to be playable.
- A **scope** is one bounded plan: the player loop, the assets it may use, the first shippable slice, and how to prove it.

The public site still hosts the November 2025 community checklist (website, Discord, brand). This folder is the plan for the playable mod.

## Rules for every scope

- **1989 Everon.** Jobs, places, and copy fit the period. A taxi rank, a fuel delivery, a parcel run. No phone apps.
- **Base-game assets.** Doors, vehicles, weapons, medical supplies, fuel cans, and buildings come from Arma Reforger. A scope configures those prefabs. It does not add a weapon pack, a vehicle pack, or aircraft.
- **Framework, then one example.** Ship the hook and one vanilla setup. Server owners retune items and places in config.
- **Money stays physical.** Cash is the MoneyStack. The ATM is the only cash-to-account boundary. Stolen money leaves as cash.
- **In the world.** The place is the interface: a bank vault, a jail door, a till, a hideout machine. See [design philosophy](../design-philosophy.md).
- **Reuse before writing.** [sources.md](sources.md) names the script or prefab to extend: this repo first, then Arma Reforger, then [LNGCLifeRP_Public](https://github.com/SouLGenetic/LNGCLifeRP_Public) (MIT).
- **Three gates.** [testing/README.md](testing/README.md) is the headless case, the DebugWorld entity, and the screenshot for each setup.

## Phases

| Phase | What it is | Doc |
| --- | --- | --- |
| Alpha | A small group plays the systems already in the repo | [phases/alpha.md](phases/alpha.md) |
| Release | The first public feature set | [phases/release.md](phases/release.md) |
| After release | Fix what play exposes, then the larger missing loops | [phases/after-release.md](phases/after-release.md) |

Widen the test only after the smaller group can finish a session on the basic loops. There is no release date in this plan.

## Release scopes

| Scope | Player loop | Doc |
| --- | --- | --- |
| Forced entry | A charge opens a door or container until someone repairs it | [scopes/crime/forced-entry.md](scopes/crime/forced-entry.md) |
| Bank robbery | Breach the vault, carry the cash out in bags | [scopes/crime/bank-robbery.md](scopes/crime/bank-robbery.md) |
| Jailbreak | The same breach opens the jail door | [scopes/crime/jailbreak.md](scopes/crime/jailbreak.md) |
| Hospital robbery | Take the till, breach the medicine cabinet | [scopes/crime/hospital-robbery.md](scopes/crime/hospital-robbery.md) |
| Gas station robbery | Rob the till | [scopes/crime/gas-station-robbery.md](scopes/crime/gas-station-robbery.md) |
| Jailing | A sentence at a placed jail | [scopes/police/jailing.md](scopes/police/jailing.md) |
| Police ranks | Rank gates duty kit drawn from vanilla gear | [scopes/police/ranks.md](scopes/police/ranks.md) |
| Organizations | Persistent groups, shared funds, bought upgrades | [scopes/ownership/organizations.md](scopes/ownership/organizations.md) |
| Hideouts | The building an organization owns | [scopes/ownership/hideouts.md](scopes/ownership/hideouts.md) |
| Contraband | Gather, process at the hideout, run a convoy | [scopes/crime/contraband.md](scopes/crime/contraband.md) |
| Resources | The legal gather → process → sell chains | [scopes/work/resources.md](scopes/work/resources.md) |
| Civilian jobs | Taxi, fuel delivery, parcel delivery, with levels | [scopes/work/civilian-jobs.md](scopes/work/civilian-jobs.md) |
| Vehicles | Own and buy base-game vehicles | [scopes/ownership/vehicles.md](scopes/ownership/vehicles.md) |

## Already in the tree

These are not re-planned as new systems. Scopes above extend them.

Character creation, civilian and police factions, persistence, cash and ATM, shops and traders, gathering and processing, jobs and skill-point licenses, cash and weapon robbery, wanted level, police duty, fines, arrest and confiscation, door locks and keys, vehicle locks, and a quest tracker.

Ground truth for behavior: [features.md](../features.md). Design intent beside the code: `addons/LifeFramework/Scripts/Game/Feature/*/Feature.md`.

## After release

Crafting as a general bench, task content, private housing purchase, reviving, police missions, and world events. Index: [phases/after-release.md](phases/after-release.md).

## How to add a scope

Copy the headings in [scopes/README.md](scopes/README.md). Put the file under `scopes/<area>/`. Name the phase. If it needs a new mesh, a new weapon, or a new vehicle, it does not belong in the base mod.
