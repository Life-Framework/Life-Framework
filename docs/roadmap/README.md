# Roadmap

The gameplay and release plan. Milestones are the one plan: each ends with a
public build and a filmed showcase, and a milestone's features do not start
before the previous milestone is done ([direction](../direction.md)).

A **scope** is one bounded plan: the player loop, the assets it may use, the
first slice, and how to prove it. Scope cards live in [scopes/](scopes/README.md)
and each names its milestone. The site's roadmap page (`website/`) renders these
docs with the milestones, their issues, and each epic's sub-issues from GitHub.
The November 2025 community checklist is in git history
(`website/content/archive/roadmap-2025.json`, removed in `4c96b4e`).

## Milestones

| # | Milestone | Status | Plan |
| --- | --- | --- | --- |
| M1 | Playtest Town | **Current** (issues labelled `m1`) | [m1-playtest-town.md](m1-playtest-town.md) |
| M2 | Living Town | Outline: moderation, medic, comms, investigation (epics #145-#148) | [m2-outline.md](m2-outline.md) |
| M3 | Framework Release | Outline: two addons, config layer, web generator, owner docs, example server, config harness | [m3-outline.md](m3-outline.md) |
| M4 | Flagship Live | Outline: hosting and ops, staff and community, showcase | [m4-outline.md](m4-outline.md) |
| M5 | Flagship Story | Ideas only: relocated from Chernobyl, life on Everon 1989 | epic #214 |

Each outline is planned in full only after the milestone before it closes.

## Rules for every scope

- **Framework, then flagship.** Ship the hook and one 1989 Everon setup. Server
  owners retune items and places in config. Core is era-agnostic; 1989 lives in
  the `LF_Everon1989` pack from M3.
- **1989 Everon.** Flagship jobs, places, and copy fit the period. A taxi rank, a
  fuel delivery, a parcel run. No phone apps.
- **Base-game assets.** Doors, vehicles, weapons, medical supplies, fuel cans,
  and buildings come from Arma Reforger. A scope configures those prefabs. It
  does not add a weapon pack, a vehicle pack, or aircraft.
- **Money stays physical.** Cash is the MoneyStack. The ATM is the only
  cash-to-account boundary. Stolen money leaves as cash.
- **In the world.** The place is the interface: a bank vault, a jail door, a
  till, a hideout machine. See [design](../design.md).
- **Reuse before writing.** [sources.md](sources.md) names the script or prefab
  to extend: this repo first, then Arma Reforger, then Overthrow and
  [LNGCLifeRP_Public](https://github.com/SouLGenetic/LNGCLifeRP_Public) (MIT).
- **Three gates.** [testing/README.md](testing/README.md) is the headless case,
  the DebugWorld entity, and the screenshot for each setup.
- **Proof classes.** In-session checks do not prove a restart. A host-only run
  does not prove a dedicated server.

## Scope cards

| Scope | Player loop | Milestone |
| --- | --- | --- |
| [Forced entry](scopes/crime/forced-entry.md) | A charge opens a door or container until someone repairs it | In the tree (M1) |
| [Bank robbery](scopes/crime/bank-robbery.md) | Breach the vault, carry the cash out in bags | In the tree (M1) |
| [Hospital robbery](scopes/crime/hospital-robbery.md) | Take the till, breach the medicine cabinet | In the tree (M1) |
| [Contraband](scopes/crime/contraband.md) | Gather, process at the hideout, run a convoy | In the tree (M1) |
| [Jailing](scopes/police/jailing.md) | A sentence at a placed jail | In the tree (M1) |
| [Police ranks](scopes/police/ranks.md) | Rank gates duty kit drawn from vanilla gear | In the tree (M1) |
| [Organizations](scopes/ownership/organizations.md) | Persistent groups, shared funds, bought upgrades | In the tree (M1) |
| [Hideouts](scopes/ownership/hideouts.md) | The building an organization owns | In the tree (M1) |
| [Vehicles](scopes/ownership/vehicles.md) | Own and buy base-game vehicles | In the tree (M1) |
| [Resources](scopes/work/resources.md) | The legal gather, process, sell chains | In the tree (M1) |
| [Civilian jobs](scopes/work/civilian-jobs.md) | Taxi, fuel delivery, parcel delivery, with levels | In the tree (M1) |
| [UI kit](scopes/later/ui-kit.md) | Shared 1989 menu parts | In the tree; menu migration not planned |
| [Jailbreak](scopes/crime/jailbreak.md) | The same breach opens the jail door | M2 |
| [Gas station robbery](scopes/crime/gas-station-robbery.md) | Rob the till | M2 |
| [Police missions](scopes/later/police-missions.md) | A mission board for on-duty police | M4 |
| [Private housing](scopes/later/housing.md) | Buy, rent, and furnish a home; the dacha | M5 |
| [Crafting](scopes/later/crafting.md) | A general bench; cooking and food | M5 |
| [Tasks](scopes/later/tasks.md) | Workplace tasks; the state factory | M5 |
| [Events](scopes/later/events.md) | World events | Parked |

"In the tree" means the system exists; [features.md](../features.md) is the
ground truth for its behaviour, and the card is kept as design intent. Revive
folded into the M2 medic theme (#146). Other placed issues: police application
#128 and jail work #127 (M2), day and night #129 (M4), bus stops #203 and trash
cans #205 (M5).

## How to add a scope

Copy the headings in [scopes/README.md](scopes/README.md). Put the file under
`scopes/<area>/`. Name the milestone. If it needs a new mesh, a new weapon, or a
new vehicle, it does not belong in the core or the flagship pack.
